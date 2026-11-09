#!/usr/bin/env bash
# Local proof of TLS + device-token auth (plan E2). Throwaway cert, DB and tokens in a temp
# dir, deleted on exit. Run from server/:   deploy/local_tls_proof.sh
set -uo pipefail

cd "$(dirname "$0")/.."
PY="${PY:-.venv/bin/python}"
PORT="${PORT:-8443}"
tmp="$(mktemp -d)"
trap 'kill "$pid" 2>/dev/null; rm -rf "$tmp"' EXIT

export DB_PATH="$tmp/proof.db"
fp="$(deploy/make_cert.sh "$tmp" localhost 2 2>/dev/null)"
echo "# Throwaway certificate fingerprint (what the device portal expects):"
echo "$fp"

"$PY" -m uvicorn app.main:app --host 127.0.0.1 --port "$PORT" \
    --ssl-keyfile "$tmp/server.key" --ssl-certfile "$tmp/server.crt" --log-level error &
pid=$!
for _ in $(seq 40); do curl -sk -o /dev/null "https://127.0.0.1:$PORT/v1/health" && break; sleep 0.25; done

token="$("$PY" -m app.cli device create proof-device | tail -1)"
bad="${token%%.*}.not-the-real-secret"
url="https://localhost:$PORT"
c() { echo; echo "\$ $1"; shift; "$@"; echo "[exit $?]"; }

echo; echo "# 1. TLS: the server certificate's SHA-256 as the client sees it"
seen="$(echo | openssl s_client -connect "127.0.0.1:$PORT" 2>/dev/null \
    | openssl x509 -noout -fingerprint -sha256 | cut -d= -f2)"
echo "seen:     $seen"
echo "expected: $fp"
[ "$seen" = "$fp" ] && echo "PIN MATCH" || echo "PIN MISMATCH"

c "curl https://localhost (no trust anchor, self-signed cert must be refused)" \
    curl -sS -o /dev/null -w "http %{http_code}\n" --max-time 5 "$url/v1/health"
c "curl --cacert server.crt /v1/health (TLS ok, no auth needed)" \
    sh -c 'curl -sS -v -w "\nhttp %{http_code}\n" --cacert "$1" "$2" 2>&1 | grep -E "SSL connection using|^ok|^http"' _ \
    "$tmp/server.crt" "$url/v1/health"
c "curl /v1/whoami with NO token" \
    curl -sS -w "\nhttp %{http_code}\n" --cacert "$tmp/server.crt" "$url/v1/whoami"
c "curl /v1/whoami with a WRONG token" \
    curl -sS -w "\nhttp %{http_code}\n" --cacert "$tmp/server.crt" -H "Authorization: Bearer $bad" "$url/v1/whoami"
c "curl /v1/whoami with the VALID token" \
    curl -sS -w "\nhttp %{http_code}\n" --cacert "$tmp/server.crt" -H "Authorization: Bearer $token" "$url/v1/whoami"

echo; echo "# 2. Hashed at rest: neither the token nor its secret appears in the database"
secret="${token#*.}"
if grep -aq -e "$token" -e "$secret" "$DB_PATH"; then echo "FAIL: plaintext found"; else echo "ok: no plaintext in $(basename "$DB_PATH")"; fi

echo; echo "# 3. tools/fake_device.py (pinned TLS, one connection, bearer token)"
fake=../tools/fake_device.py
c "fake_device with the right pin and token" \
    "$PY" "$fake" --server "$url" --fingerprint "$fp" --token "$token"
c "fake_device with a WRONG pin (must refuse before sending the token)" \
    "$PY" "$fake" --server "$url" --token "$token" \
    --fingerprint "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF"
c "fake_device with the right pin and a WRONG token" \
    "$PY" "$fake" --server "$url" --fingerprint "$fp" --token "$bad"

c "revoke the device" "$PY" -m app.cli device revoke proof-device
c "curl /v1/whoami with the REVOKED token" \
    curl -sS -w "\nhttp %{http_code}\n" --cacert "$tmp/server.crt" -H "Authorization: Bearer $token" "$url/v1/whoami"
c "curl /v1/health still open after revoke" \
    curl -sS -w "\nhttp %{http_code}\n" --cacert "$tmp/server.crt" "$url/v1/health"
