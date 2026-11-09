#!/usr/bin/env bash
# Runs ON THE DROPLET as root (deploy.sh calls it over ssh; it can also be run by hand).
# Installs ChatNav under its own user, directories, systemd unit and nginx site.
# Idempotent. Never touches Photon/Delta services, ports 8080-8090, or the firewall.
#
#   remote_install.sh <public-host-or-ip> <staged-source-dir>
set -euo pipefail

PUBLIC_HOST="${1:?usage: remote_install.sh <public-host-or-ip> <staged-source-dir>}"
STAGE="${2:?usage: remote_install.sh <public-host-or-ip> <staged-source-dir>}"

APP_USER=chatnav
APP_DIR=/opt/chatnav
ETC_DIR=/etc/chatnav
DATA_DIR=/var/lib/chatnav
PORT=8100
step() { printf '\n==> %s\n' "$*"; }

step "1/9 Preflight (read only)"
echo "Listening TCP ports now (Photon/Delta stay on 8080-8090):"
ss -ltnH | awk '{print "  " $4}' | sort -u
if ss -ltnH "sport = :$PORT" | grep -q .; then
    if ! systemctl is-active --quiet chatnav; then
        echo "ERROR: port $PORT is used by something other than chatnav" >&2
        exit 1
    fi
fi
if ss -ltnpH 'sport = :443' | grep -q . && ! ss -ltnpH 'sport = :443' | grep -q nginx; then
    echo "ERROR: port 443 is held by a non-nginx process; refusing to continue" >&2
    exit 1
fi
command -v nginx >/dev/null || { echo "ERROR: nginx is not installed (apt install nginx)" >&2; exit 1; }
echo "ufw: not modified by this script"

step "2/9 System user and directories"
id "$APP_USER" >/dev/null 2>&1 || useradd --system --home "$APP_DIR" --shell /usr/sbin/nologin "$APP_USER"
install -d -o "$APP_USER" -g "$APP_USER" "$APP_DIR" "$DATA_DIR"
install -d -o root -g "$APP_USER" -m 750 "$ETC_DIR"
install -d -o root -g root -m 755 "$ETC_DIR/tls"

step "3/9 Server code to $APP_DIR/server"
install -d -o "$APP_USER" -g "$APP_USER" "$APP_DIR/server"
rsync -a --delete --exclude .venv --exclude __pycache__ --exclude '*.db' --exclude .env \
    "$STAGE/server/" "$APP_DIR/server/"
chown -R "$APP_USER:$APP_USER" "$APP_DIR/server"

step "4/9 Python virtualenv and dependencies"
[ -x "$APP_DIR/venv/bin/python" ] || sudo -u "$APP_USER" python3 -m venv "$APP_DIR/venv"
sudo -u "$APP_USER" "$APP_DIR/venv/bin/pip" install --quiet --upgrade pip
sudo -u "$APP_USER" "$APP_DIR/venv/bin/pip" install --quiet "$APP_DIR/server"

step "5/9 Environment file (kept if it exists)"
if [ ! -e "$ETC_DIR/chatnav.env" ]; then
    cat > "$ETC_DIR/chatnav.env" <<ENV
DB_PATH=$DATA_DIR/chatnav.db
STT_API_KEY=
LLM_API_KEY=
TTS_API_KEY=
ENV
    echo "created $ETC_DIR/chatnav.env (fill in provider keys in M3)"
else
    echo "kept existing $ETC_DIR/chatnav.env"
fi
chown root:"$APP_USER" "$ETC_DIR/chatnav.env"
chmod 640 "$ETC_DIR/chatnav.env"

step "6/9 Self-signed TLS certificate (kept if it exists)"
FINGERPRINT="$("$APP_DIR/server/deploy/make_cert.sh" "$ETC_DIR/tls" "$PUBLIC_HOST")"
chown root:"$APP_USER" "$ETC_DIR/tls/server.key"
chmod 640 "$ETC_DIR/tls/server.key"

step "7/9 systemd unit"
install -m 644 "$APP_DIR/server/deploy/chatnav.service" /etc/systemd/system/chatnav.service
systemctl daemon-reload
systemctl enable chatnav >/dev/null
systemctl restart chatnav

step "8/9 nginx site (tested before reload)"
SITE=/etc/nginx/sites-available/chatnav
[ -e "$SITE" ] && cp -a "$SITE" "$SITE.bak"
install -m 644 "$APP_DIR/server/deploy/nginx-chatnav.conf" "$SITE"
ln -sf "$SITE" /etc/nginx/sites-enabled/chatnav
if nginx -t; then
    systemctl reload nginx
else
    echo "ERROR: nginx config test failed; removing the chatnav site, nginx left as it was" >&2
    rm -f /etc/nginx/sites-enabled/chatnav
    [ -e "$SITE.bak" ] && mv "$SITE.bak" "$SITE"
    exit 1
fi

step "9/9 Checks"
sleep 2
systemctl is-active --quiet chatnav && echo "chatnav service: active" || { echo "chatnav service FAILED"; exit 1; }
echo "health over TLS (local): $(curl -sk --max-time 5 https://127.0.0.1/v1/health || echo FAILED)"

cat <<DONE

Certificate fingerprint (enter in the device setup portal):
  $FINGERPRINT

Create a device token (shown once):
  cd $APP_DIR/server && sudo -u $APP_USER DB_PATH=$DATA_DIR/chatnav.db \\
      $APP_DIR/venv/bin/python -m app.cli device create <name>

Firewall: unchanged. Review 'ss -ltn' and 'ufw status' before enabling ufw in M5.
DONE
