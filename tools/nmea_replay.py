"""Replays an NMEA log (RMC + GGA) into /v1/fixes, like the device would send it.

    CHATNAV_TOKEN=... python tools/nmea_replay.py tools/fixtures/drive.nmea \
        --server http://127.0.0.1:8000 --fast

Real logs contain location: keep them out of git (only tools/fixtures/ is allowed).
"""

import argparse
import calendar
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sim_trip import HEADER, RECORD

KNOT_MS = 0.514444


def checksum_ok(line: str) -> bool:
    if not line.startswith("$") or "*" not in line:
        return False
    body, _, cs = line[1:].partition("*")
    x = 0
    for c in body:
        x ^= ord(c)
    try:
        return x == int(cs.strip()[:2], 16)
    except ValueError:
        return False


def _coord(value: str, hemi: str) -> int:
    dot = value.index(".")
    deg = int(value[: dot - 2])
    minutes = float(value[dot - 2 :])
    e7 = round((deg + minutes / 60) * 1e7)
    return -e7 if hemi in "SW" else e7


def _utc(time_s: str, date_s: str) -> tuple[int, int]:
    hh, mm, ss = int(time_s[0:2]), int(time_s[2:4]), int(time_s[4:6])
    ms = round(float(time_s[6:] or 0) * 1000)
    day, mon, yr = int(date_s[0:2]), int(date_s[2:4]), 2000 + int(date_s[4:6])
    return calendar.timegm((yr, mon, day, hh, mm, ss)), ms


def parse_log(lines) -> list[tuple]:
    """Joins RMC and GGA of the same UTC time into fix records; skips bad checksums."""
    out, rmc, gga = [], {}, {}
    for line in lines:
        line = line.strip()
        if not checksum_ok(line):
            continue
        f = line[1 : line.index("*")].split(",")
        kind = f[0][2:]
        if kind == "RMC" and len(f) >= 10:
            rmc = {"time": f[1], "ok": f[2] == "A", "lat": f[3], "ns": f[4], "lon": f[5],
                   "ew": f[6], "kn": f[7], "course": f[8], "date": f[9]}
            gga = {}
        elif kind == "GGA" and len(f) >= 10 and rmc and f[1] == rmc["time"]:
            gga = {"quality": int(f[6] or 0), "sats": int(f[7] or 0), "hdop": float(f[8] or 99),
                   "alt": float(f[9] or 0)}
            out.append(_record(rmc, gga, len(out)))
            rmc, gga = {}, {}
    return out


def _record(rmc: dict, gga: dict, index: int) -> tuple:
    utc_s, utc_ms = _utc(rmc["time"], rmc["date"])
    has_pos = bool(rmc["lat"] and rmc["lon"])
    flags = 0x01 | 0x04 | (0x02 if rmc["ok"] else 0) | (0x08 if has_pos else 0)
    return (
        index * 1000, utc_s, utc_ms,
        _coord(rmc["lat"], rmc["ns"]) if has_pos else 0,
        _coord(rmc["lon"], rmc["ew"]) if has_pos else 0,
        round(float(rmc["kn"] or 0) * KNOT_MS * 100),
        round(float(rmc["course"] or 0) * 10),
        round(gga["alt"] * 10), gga["sats"], gga["quality"], min(255, round(gga["hdop"] * 10)),
        flags,
    )


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("log")
    p.add_argument("--server", required=True)
    p.add_argument("--token", default=os.environ.get("CHATNAV_TOKEN", ""))
    p.add_argument("--fingerprint", default=os.environ.get("CHATNAV_FINGERPRINT", ""))
    p.add_argument("--batch", type=int, default=5, help="records per batch")
    p.add_argument("--fast", action="store_true", help="do not wait between batches")
    args = p.parse_args()

    with open(args.log) as f:
        records = parse_log(f)
    print(f"[NMEA] {len(records)} fixes parsed from {args.log}")
    from fixes_client import FixPoster

    poster = FixPoster(args.server, args.token, args.fingerprint)
    for i in range(0, len(records), args.batch):
        chunk = records[i : i + args.batch]
        seq = i + len(chunk)
        body = HEADER.pack(seq, 1, 0, len(chunk)) + b"".join(RECORD.pack(*r) for r in chunk)
        print(f"[NMEA] batch seq {seq} acked {poster.post(body)}")
        if not args.fast:
            time.sleep(args.batch)
    return 0


if __name__ == "__main__":
    sys.exit(main())
