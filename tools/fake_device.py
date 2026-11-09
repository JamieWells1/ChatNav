"""Fake ChatNav device: talks to the server like the Pico does (M2: TLS + auth only).

Pins the server certificate by SHA-256 fingerprint, keeps one connection open, and sends
the bearer token. Later milestones add the WAV upload and GPS fixes on the same link.

    CHATNAV_TOKEN=<token> python tools/fake_device.py \
        --server https://127.0.0.1:8443 --fingerprint AA:BB:...

    # M3: stream a 16 kHz 16-bit mono WAV as a question, save the spoken reply as a WAV
    python tools/fake_device.py query --wav tools/fixtures/question.wav --out reply.wav ...
"""

import argparse
import hashlib
import http.client
import os
import ssl
import sys
import time
import wave
from collections.abc import Iterator
from urllib.parse import urlparse

CHUNK_MS = 100


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


def read_wav(path: str) -> tuple[bytes, int]:
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise ValueError("WAV must be 16-bit mono")
        return w.readframes(w.getnframes()), w.getframerate()


def write_wav(path: str, pcm: bytes, rate: int) -> None:
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)


def pcm_chunks(pcm: bytes, rate: int, realtime: bool) -> Iterator[bytes]:
    size = rate * 2 * CHUNK_MS // 1000
    for i in range(0, len(pcm), size):
        yield pcm[i : i + size]
        if realtime:
            time.sleep(CHUNK_MS / 1000)


def query(conn, token: str, wav: str, out: str | None, realtime: bool) -> int:
    """Chunked PCM upload, then read the streamed reply; prints release-to-first-audio."""
    pcm, rate = read_wav(wav)
    headers = {"Authorization": f"Bearer {token}", "Content-Type": "audio/L16"}
    conn.putrequest("POST", "/v1/query")
    for k, v in {**headers, "Transfer-Encoding": "chunked"}.items():
        conn.putheader(k, v)
    conn.endheaders()
    for chunk in pcm_chunks(pcm, rate, realtime):
        conn.send(b"%x\r\n%s\r\n" % (len(chunk), chunk))
    conn.send(b"0\r\n\r\n")
    t_release = time.monotonic()
    r = conn.getresponse()
    reply = bytearray()
    first_ms = None
    while chunk := r.read(4096):
        if first_ms is None:
            first_ms = int((time.monotonic() - t_release) * 1000)
        reply += chunk
    total_ms = int((time.monotonic() - t_release) * 1000)
    outcome = r.getheader("X-Outcome", "?")
    print(f"[FAKE] query: HTTP {r.status} outcome={outcome} reply={len(reply)} bytes")
    print(f"[FAKE] release to first audio {first_ms} ms, to last byte {total_ms} ms")
    if r.status != 200:
        return 1
    if out:
        write_wav(out, bytes(reply), rate)
        print(f"[FAKE] reply written to {out}")
    return 0 if outcome == "ok" else 4


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("command", nargs="?", choices=["check", "query"], default="check")
    p.add_argument("--wav", default="tools/fixtures/question.wav", help="query: input WAV")
    p.add_argument("--out", default="reply.wav", help="query: where to write the reply WAV")
    p.add_argument("--realtime", action="store_true", help="query: pace the upload like a mic")
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
    if status != 200 or args.command == "check":
        return 0 if status == 200 else 1
    return query(conn, args.token, args.wav, args.out, args.realtime)


if __name__ == "__main__":
    sys.exit(main())
