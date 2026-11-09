#!/usr/bin/env bash
# Proposed ufw firewall rules for the droplet. PRINTS ONLY by default; nothing is changed.
#
#   ufw_rules.sh [PORT ...]            list listening ports, print the proposed rules
#   ufw_rules.sh [PORT ...] --apply    run them (needs root; asks for the word YES first)
#
# PORT arguments are extra TCP ports to keep open, e.g. Photon/Delta's 8080 8081 ... 8090.
# 22 (ssh) and 443 (ChatNav via nginx) are always allowed. Nothing else (deploy.sh, systemd,
# CI) calls this script. The owner reviews the printed rules first, then runs --apply from a
# network that can reach the droplet over ssh, with a second ssh session already open, so a
# wrong rule can be undone with `ufw disable`.
set -uo pipefail

apply=0
keep=()
for a in "$@"; do
    case "$a" in
        --apply) apply=1 ;;
        -h|--help) sed -n 2,11p "$0"; exit 0 ;;
        *[!0-9]*) echo "not a port number: $a" >&2; exit 2 ;;
        *) keep+=("$a") ;;
    esac
done

echo "# Currently listening TCP/UDP ports (anything not allowed below becomes unreachable from outside)"
if command -v ss >/dev/null; then ss -tulnp 2>/dev/null; else netstat -tuln 2>/dev/null; fi
echo
echo "# Ports you asked to keep open: ${keep[*]:-none}"

rules=(
    "ufw default deny incoming"
    "ufw default allow outgoing"
    "ufw allow 22/tcp comment 'ssh'"
    "ufw allow 443/tcp comment 'chatnav via nginx'"
)
for p in "${keep[@]}"; do rules+=("ufw allow $p/tcp comment 'kept (not ChatNav)'"); done
rules+=("ufw --force enable")

echo
echo "# Proposed rules"
printf '%s\n' "${rules[@]}"
echo
echo "# Not allowed on purpose: 8100 (ChatNav's uvicorn, bound to 127.0.0.1, reached only via nginx)"

if [ "$apply" -ne 1 ]; then
    echo
    echo "# Dry run: nothing was changed. Re-run with --apply after reviewing."
    exit 0
fi

[ "$(id -u)" -eq 0 ] || { echo "--apply needs root" >&2; exit 1; }
command -v ufw >/dev/null || { echo "ufw is not installed" >&2; exit 1; }
printf 'Type YES to apply these rules: '
read -r answer
[ "$answer" = YES ] || { echo "Cancelled, nothing changed."; exit 1; }
for r in "${rules[@]}"; do
    echo "+ $r"
    eval "$r" || { echo "failed: $r" >&2; exit 1; }
done
ufw status verbose
