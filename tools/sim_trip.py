"""Synthetic GPS trips as device fix batches (format: firmware/src/gps/fix_record.h).

    python tools/sim_trip.py --scenario commute --out /tmp/batches
    CHATNAV_TOKEN=... python tools/sim_trip.py --scenario harsh \
        --server http://127.0.0.1:8000 --fast
"""

import argparse
import math
import os
import struct
import sys
import time
from datetime import UTC, datetime

HEADER = struct.Struct("<IHHH")
RECORD = struct.Struct("<IIHiiHHhBBBB")
FLAGS_FIX = 0x0F  # time, RMC valid, GGA, position
FLAGS_NO_FIX = 0x05  # time and GGA only
MPH = 0.44704
EARTH_M_PER_DEG = 111_320.0


class Track:
    """Builds a drive second by second from scripted segments. Speeds in m/s."""

    def __init__(self, start_utc: float, hz: int = 1, lat: float = 51.5, lon: float = -0.12,
                 heading: float = 45.0, seed: int = 1):
        self.t = start_utc
        self.dt = 1 / hz
        self.lat, self.lon, self.heading = lat, lon, heading
        self.v = 0.0
        self.device_ms = 5000
        self.records: list[tuple] = []
        self.rng = _Lcg(seed)

    def _emit(self, flags: int = FLAGS_FIX, speed: float | None = None, time_valid: bool = True):
        t_ms = round(self.t * 1000)
        speed = self.v if speed is None else speed
        fix = flags & 0x08
        self.records.append((
            self.device_ms,
            t_ms // 1000 if time_valid else 0,
            t_ms % 1000 if time_valid else 0,
            round(self.lat * 1e7) if fix else 0,
            round(self.lon * 1e7) if fix else 0,
            max(0, round(speed * 100)),
            round(self.heading * 10) % 3600,
            35,
            9 if fix else 0,
            1 if fix else 0,
            8 if fix else 99,
            flags if time_valid else flags & ~0x01,
        ))

    def _advance(self):
        d = self.v * self.dt
        h = math.radians(self.heading)
        self.lat += d * math.cos(h) / EARTH_M_PER_DEG
        self.lon += d * math.sin(h) / (EARTH_M_PER_DEG * math.cos(math.radians(self.lat)))
        self.t += self.dt
        self.device_ms += round(self.dt * 1000)

    def hold(self, seconds: float):
        for _ in range(round(seconds / self.dt)):
            self.v = 0.0
            self._emit()
            self._advance()

    def drift(self, seconds: float, spike_every: int = 0):
        """Parked, GPS wandering: small speeds, optional short spikes above the trip threshold."""
        for i in range(round(seconds / self.dt)):
            noise = self.rng.next() * 0.4
            spike = spike_every and i % spike_every in (0, 1)
            self._emit(speed=3.2 if spike else noise)
            self.lat += (self.rng.next() - 0.5) * 2e-6
            self.lon += (self.rng.next() - 0.5) * 2e-6
            self.t += self.dt
            self.device_ms += round(self.dt * 1000)

    def cruise(self, seconds: float):
        for _ in range(round(seconds / self.dt)):
            self._emit()
            self._advance()

    def change_speed(self, to: float, rate: float):
        """Constant acceleration (rate m/s2, sign ignored) until speed reaches `to`."""
        step = abs(rate) * self.dt
        while abs(self.v - to) > 1e-9:
            self.v = min(self.v + step, to) if to > self.v else max(self.v - step, to)
            self._emit()
            self._advance()

    def turn(self, degrees: float):
        self.heading = (self.heading + degrees) % 360

    def no_fix(self, seconds: float, time_valid: bool = True):
        for _ in range(round(seconds / self.dt)):
            self._emit(FLAGS_NO_FIX, speed=0, time_valid=time_valid)
            self.t += self.dt
            self.device_ms += round(self.dt * 1000)

    def gap(self, seconds: float):
        """Device off or offline: time passes, no records."""
        self.t += seconds
        self.device_ms += round(seconds * 1000)


class _Lcg:
    def __init__(self, seed: int):
        self.s = seed

    def next(self) -> float:
        self.s = (self.s * 1103515245 + 12345) & 0x7FFFFFFF
        return self.s / 0x7FFFFFFF


def commute(tr: Track) -> None:
    tr.hold(20)
    tr.change_speed(14, 1.5)
    tr.cruise(120)
    tr.change_speed(0, 2.0)
    tr.hold(40)
    tr.change_speed(15, 1.5)
    tr.cruise(180)
    tr.change_speed(0, 2.0)
    tr.hold(20)


def harsh(tr: Track) -> None:
    """Gentle drive with one scripted harsh brake (-5 m/s2) and one harsh pull-away (+4)."""
    tr.hold(10)
    tr.change_speed(20, 1.5)
    tr.cruise(60)
    tr.change_speed(4, 5.0)
    tr.cruise(5)
    tr.change_speed(18, 4.0)
    tr.cruise(60)
    tr.change_speed(0, 1.5)
    tr.hold(10)


def motorway(tr: Track) -> None:
    tr.hold(10)
    tr.change_speed(31.9, 1.5)  # about 71 mph
    tr.cruise(40)
    tr.change_speed(29, 1.0)
    tr.cruise(60)
    tr.change_speed(0, 1.5)
    tr.hold(10)


def parked(tr: Track) -> None:
    tr.drift(1800, spike_every=240)


SCENARIOS = {"commute": commute, "harsh": harsh, "motorway": motorway, "parked": parked}


def make_trip(scenario: str, start_utc: float, hz: int = 1, **kw) -> list[tuple]:
    tr = Track(start_utc, hz, **kw)
    SCENARIOS[scenario](tr)
    return tr.records


def batches(records: list[tuple], per_batch: int, fw: int = 1, first_seq: int = 1):
    """Yields (seq, body); seq of a batch is the seq of its last record."""
    for i in range(0, len(records), per_batch):
        chunk = records[i : i + per_batch]
        seq = first_seq + i + len(chunk) - 1
        body = HEADER.pack(seq, fw, 0, len(chunk)) + b"".join(RECORD.pack(*r) for r in chunk)
        yield seq, body


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("--scenario", choices=sorted(SCENARIOS), default="commute")
    p.add_argument("--start", help="UTC start, e.g. 2026-11-09T08:00:00Z (default: now)")
    p.add_argument("--hz", type=int, default=1)
    p.add_argument("--batch-s", type=int, default=5, help="seconds of fixes per batch")
    p.add_argument("--out", help="directory to write batch_<seq>.bin files into")
    p.add_argument("--server", help="POST to this server, e.g. https://host:8443")
    p.add_argument("--token", default=os.environ.get("CHATNAV_TOKEN", ""))
    p.add_argument("--fingerprint", default=os.environ.get("CHATNAV_FINGERPRINT", ""))
    p.add_argument("--fast", action="store_true", help="do not wait between batches")
    args = p.parse_args()

    if args.start:
        start = datetime.fromisoformat(args.start).timestamp()
    else:
        start = time.time()
    records = make_trip(args.scenario, start, args.hz)
    per_batch = args.batch_s * args.hz
    print(f"[SIM] {args.scenario}: {len(records)} fixes, from {datetime.fromtimestamp(start, UTC)}")

    poster = None
    if args.server:
        from fixes_client import FixPoster

        poster = FixPoster(args.server, args.token, args.fingerprint)
    if args.out:
        os.makedirs(args.out, exist_ok=True)
    for seq, body in batches(records, per_batch):
        if args.out:
            with open(os.path.join(args.out, f"batch_{seq:06d}.bin"), "wb") as f:
                f.write(body)
        if poster:
            ack = poster.post(body)
            print(f"[SIM] batch seq {seq} acked {ack}")
            if not args.fast:
                time.sleep(args.batch_s)
    return 0


if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    sys.exit(main())
