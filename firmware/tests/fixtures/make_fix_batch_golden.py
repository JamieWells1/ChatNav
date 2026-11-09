"""Regenerates fix_batch_golden.bin with the server's own encoder.

Run from the repo root: python3 firmware/tests/fixtures/make_fix_batch_golden.py
"""

import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "server"))
from app.gps import wire  # noqa: E402

fixes = [
    wire.Fix(1000, 1794227719, 250, 375000000, -1220000000, 1234, 2710, -35, 9, 1, 12, 0x0F),
    wire.Fix(2000, 1794227720, 250, 375000450, -1219999000, 0, 0, 1234, 4, 2, 255, 0x0F),
    wire.Fix(3000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00),  # before a fix: device ms only
]
body = wire.pack_batch(0x01020304 + 41, 4, 7, fixes)
assert wire.parse_batch(body).fixes == fixes
(Path(__file__).parent / "fix_batch_golden.bin").write_bytes(body)
