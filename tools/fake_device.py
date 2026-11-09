"""Fake ChatNav device: talks to the server like the Pico does (M2: TLS + auth only).

Pins the server certificate by SHA-256 fingerprint, keeps one connection open, and sends
the bearer token. Later milestones add the WAV upload and GPS fixes on the same link.

    CHATNAV_TOKEN=<token> python tools/fake_device.py \
        --server https://127.0.0.1:8443 --fingerprint AA:BB:...
"""

import argparse
import hashlib
import http.client
import os
import ssl
import sys
from urllib.parse import urlparse


class PinMismatch(Exception):
    pass


def parse_fingerprint(text: str) -> bytes:
    """Accepts AA:BB:..., plain hex, or openssl's 'sha256 Fingerprint=' line."""
    text = text.split("=")[-1]
    digits = "".join(c for c in text if c not in ": -\r\n")
    fp = bytes.fromhex(digits)
    if len(fp) != 32:
        raise ValueError("fingerprint must be 32 bytes (64 hex digits)")
    return fp


def open_pinned(host: str, port: int, pin: bytes) -> http.client.HTTPSConnection:
    """Connects and checks the leaf certificate before anything (the token) is sent."""
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE  # trust comes from the pin, not a CA
    conn = http.client.HTTPSConnection(host, port, context=ctx, timeout=10)
    conn.connect()
    der = conn.sock.getpeercert(binary_form=True)
    seen = hashlib.sha256(der).digest()
    if seen != pin:
        conn.close()
        raise PinMismatch(f"server cert sha256 {seen.hex(':').upper()} does not match the pin")
    return conn


def get(conn: http.client.HTTPSConnection, path: str, token: str | None) -> tuple[int, str]:
    headers = {"Authorization": f"Bearer {token}"} if token else {}
    conn.request("GET", path, headers=headers)
    r = conn.getresponse()
    return r.status, r.read().decode()


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("--server", default="https://127.0.0.1:8443")
    p.add_argument("--token", default=os.environ.get("CHATNAV_TOKEN", ""))
    p.add_argument("--fingerprint", default=os.environ.get("CHATNAV_FINGERPRINT", ""))
    args = p.parse_args()

    if not args.fingerprint:
        print("[FAKE] --fingerprint (or CHATNAV_FINGERPRINT) is required", file=sys.stderr)
        return 2
    url = urlparse(args.server)
    host, port = url.hostname or "127.0.0.1", url.port or 443
    try:
        conn = open_pinned(host, port, parse_fingerprint(args.fingerprint))
    except PinMismatch as e:
        print(f"[FAKE] PIN MISMATCH, refusing server: {e}")
        return 3
    print(f"[FAKE] TLS up to {host}:{port}, certificate pin matched")

    status, body = get(conn, "/v1/health", None)
    print(f"[FAKE] health: {status} {body}")
    if status != 200 or body != "ok":
        return 1

    status, body = get(conn, "/v1/whoami", args.token)  # same connection, kept open
    print(f"[FAKE] whoami: {status} {body}")
    return 0 if status == 200 else 1


if __name__ == "__main__":
    sys.exit(main())
