#!/usr/bin/env bash
# Creates a self-signed P-256 certificate and prints its SHA-256 fingerprint in the format
# the device setup portal expects (AA:BB:CC:...).
#
#   make_cert.sh <dir> <host-or-ip> [days]
#
# Writes <dir>/server.crt and <dir>/server.key (mode 600). Never overwrites existing files:
# regenerating the cert changes the fingerprint and every device must be re-entered.
set -euo pipefail

dir="${1:?usage: make_cert.sh <dir> <host-or-ip> [days]}"
host="${2:?usage: make_cert.sh <dir> <host-or-ip> [days]}"
days="${3:-3650}"

mkdir -p "$dir"
if [[ -e "$dir/server.crt" || -e "$dir/server.key" ]]; then
    echo "[CERT] $dir/server.crt already exists, keeping it" >&2
else
    if [[ "$host" =~ ^[0-9.]+$ ]]; then san="IP:$host"; else san="DNS:$host"; fi
    umask 077
    openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes \
        -keyout "$dir/server.key" -out "$dir/server.crt" -days "$days" \
        -subj "/CN=$host" -addext "subjectAltName=$san" 2>/dev/null
    chmod 600 "$dir/server.key"
    chmod 644 "$dir/server.crt"
    echo "[CERT] created $dir/server.crt for $host ($days days)" >&2
fi

# openssl prints "sha256 Fingerprint=AA:BB:..."; keep only the hex part.
openssl x509 -in "$dir/server.crt" -noout -fingerprint -sha256 | cut -d= -f2
