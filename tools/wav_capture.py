"""Capture a WAV file from the diag_mic firmware over USB serial.

Needs pyserial (pip install pyserial). The device prints "[MIC] WAV_BEGIN <bytes>",
then that many raw bytes of a complete WAV file, then "[MIC] WAV_END". All other
"[MIC] ..." lines are echoed so the log can be pasted back.

    python3 tools/wav_capture.py --port /dev/cu.usbmodem1101 --out mic.wav
"""

import argparse
import sys
import time

BEGIN = "[MIC] WAV_BEGIN"


def read_line(stream) -> str | None:
    """Reads one line; returns None on timeout with nothing read."""
    buf = bytearray()
    while True:
        c = stream.read(1)
        if not c:
            return buf.decode(errors="replace") if buf else None
        if c == b"\n":
            return buf.decode(errors="replace").rstrip("\r")
        buf += c


def read_exact(stream, n: int, timeout_s: float) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout_s
    while len(data) < n:
        chunk = stream.read(n - len(data))
        if chunk:
            data += chunk
        elif time.monotonic() > deadline:
            raise TimeoutError(f"got {len(data)} of {n} WAV bytes")
    return bytes(data)


def capture(stream, log=print, wait_s: float = 60.0) -> bytes:
    """Reads lines until a WAV block arrives and returns its bytes."""
    deadline = time.monotonic() + wait_s
    while time.monotonic() < deadline:
        line = read_line(stream)
        if line is None:
            continue
        if line.startswith(BEGIN):
            size = int(line[len(BEGIN) :].strip())
            data = read_exact(stream, size, timeout_s=30.0)
            if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
                raise ValueError("captured bytes do not start with a WAV header")
            return data
        if line.startswith("[MIC]"):
            log(line)
    raise TimeoutError("no WAV_BEGIN seen; is diag_mic flashed and the port correct?")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True, help="serial device of the Pico")
    p.add_argument("--out", default="mic.wav")
    args = p.parse_args()

    try:
        import serial
    except ImportError:
        print("[WAV] pyserial missing: pip install pyserial", file=sys.stderr)
        return 2

    with serial.Serial(args.port, 115200, timeout=0.5) as port:
        print(f"[WAV] opened {args.port}, asking the device to record")
        port.write(b"r")
        try:
            data = capture(port)
        except (TimeoutError, ValueError) as e:
            print(f"[WAV] failed: {e}", file=sys.stderr)
            return 1
    with open(args.out, "wb") as f:
        f.write(data)
    print(f"[WAV] wrote {len(data)} bytes to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
