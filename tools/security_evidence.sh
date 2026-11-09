#!/usr/bin/env bash
# E2 evidence (plan 10.1): unauthorised-use checks, printed as a transcript.
#
#   tools/security_evidence.sh --local                      throwaway local TLS server + tokens
#   tools/security_evidence.sh --host my.droplet.example [--port 443] --cacert server.crt
#                              (valid token in $CHATNAV_TOKEN; run from a network that is NOT
#                              the droplet, after server/deploy/ufw_rules.sh --apply)
#
# Checks: no token -> 401, wrong token -> 401, burst -> 429, TLS only (plain HTTP and old TLS
# refused), open ports (nmap if installed, else a plain TCP connect scan). The burst uses
# empty uploads, which end before any speech provider is called, so it costs nothing.
# Save the output:  tools/security_evidence.sh ... | tee assignment/evidence/m5-privacy/e2_<where>.txt
# Never prints a token. Local mode uses throwaway tokens only.
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
server="$here/../server"
mode="" host="" port=443 cacert="" burst=15
while [ $# -gt 0 ]; do
    case "$1" in
        --local) mode=local ;;
        --host) host="$2"; mode=remote; shift ;;
        --port) port="$2"; shift ;;
        --cacert) cacert="$2"; shift ;;
        --burst) burst="$2"; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ -n "$mode" ] || { sed -n 2,13p "$0" >&2; exit 2; }

PY="${PY:-$server/.venv/bin/python}"
tmp="$(mktemp -d)"
pid=""
trap '[ -n "$pid" ] && kill "$pid" 2>/dev/null; rm -rf "$tmp"' EXIT

if [ "$mode" = local ]; then
    host=localhost port="${PORT:-8444}"
    export DB_PATH="$tmp/e2.db" STT_PROVIDER=stub LLM_PROVIDER=stub TTS_PROVIDER=stub
    (cd "$server" && deploy/make_cert.sh "$tmp" localhost 2 >/dev/null 2>&1)
    cacert="$tmp/server.crt"
    (cd "$server" && exec "$PY" -m uvicorn app.main:app --host 127.0.0.1 --port "$port" \
        --ssl-keyfile "$tmp/server.key" --ssl-certfile "$cacert" --log-level error) &
    pid=$!
    for _ in $(seq 40); do curl -sk -o /dev/null "https://localhost:$port/v1/health" && break; sleep 0.25; done
    token="$(cd "$server" && "$PY" -m app.cli device create e2-throwaway | tail -1)"
else
    token="${CHATNAV_TOKEN:-}"
    [ -n "$token" ] || { echo "set CHATNAV_TOKEN for --host mode" >&2; exit 2; }
fi
url="https://$host:$port"
ca=(); [ -n "$cacert" ] && ca=(--cacert "$cacert")
bad="${token%%.*}.not-the-real-secret"
code() { curl -sS -o /dev/null -w "%{http_code}" --max-time 10 "${ca[@]}" "$@" 2>&1; }
verdict() { [ "$1" = "$2" ] && echo "PASS" || echo "FAIL (wanted $2)"; }

echo "# E2 evidence: $(date -u +%Y-%m-%dT%H:%M:%SZ), target $host:$port, mode $mode"
echo
echo "# 1. No token -> 401"
c="$(code "$url/v1/whoami")"; echo "GET /v1/whoami (no Authorization) -> $c  $(verdict "$c" 401)"
echo "POST /v1/fixes (no Authorization) -> $(code -X POST --data-binary x "$url/v1/fixes")"
echo "DELETE /v1/data (no Authorization) -> $(code -X DELETE "$url/v1/data")  (must be 401)"
echo
echo "# 2. Wrong token -> 401"
c="$(code -H "Authorization: Bearer $bad" "$url/v1/whoami")"
echo "GET /v1/whoami (right id, wrong secret) -> $c  $(verdict "$c" 401)"
c="$(code -H "Authorization: Bearer garbage" "$url/v1/whoami")"
echo "GET /v1/whoami (malformed token) -> $c  $(verdict "$c" 401)"
echo
echo "# 3. Valid token -> 200"
c="$(code -H "Authorization: Bearer $token" "$url/v1/whoami")"
echo "GET /v1/whoami (valid token) -> $c  $(verdict "$c" 200)"
echo
echo "# 4. Burst of $burst queries in one minute -> 429 after RATE_LIMIT_QUERIES_PER_MIN"
codes=""
for _ in $(seq "$burst"); do
    codes="$codes $(code -X POST -H "Authorization: Bearer $token" --data-binary '' "$url/v1/query")"
done
echo "status codes:$codes"
case "$codes" in *429*) echo "PASS (429 seen)";; *) echo "FAIL (no 429)";; esac
echo
echo "# 5. TLS only"
c="$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://$host:$port/v1/health" 2>&1)"
echo "plain http:// to the TLS port -> $c (000 or 400 means no plaintext service)"
for v in tls1 tls1_1; do
    if echo | openssl s_client -connect "$host:$port" "-$v" >/dev/null 2>&1; then r="ACCEPTED (FAIL)"; else r="refused"; fi
    echo "openssl s_client -$v -> $r"
done
echo | openssl s_client -connect "$host:$port" -servername "$host" 2>/dev/null | grep -E "^New, |Protocol" | head -2
echo
echo "# 6. Open ports (expect only 22 and 443 externally; Photon's ports only if kept on purpose)"
if command -v nmap >/dev/null; then
    [ "$mode" = local ] && target=127.0.0.1 || target="$host"
    nmap -Pn -p 1-10000 "$target" | grep -E "^(PORT|[0-9]+/)|Nmap scan report|Host is"
else
    echo "(nmap not installed; plain TCP connect scan of common ports instead)"
    [ "$mode" = local ] && target=127.0.0.1 || target="$host"
    for p in 21 22 25 53 80 443 3306 5432 6379 8000 8080 8081 8082 8083 8084 8085 8086 8087 8088 8089 8090 8100; do
        if (exec 3<>"/dev/tcp/$target/$p") 2>/dev/null; then echo "$p/tcp open"; fi
    done
fi
[ "$mode" = local ] && echo "(local run: the only listener started here is $port; a droplet run is the real evidence)"
exit 0
