"""Per-trip summary and minute buckets from a segmented trip's points."""

from collections import defaultdict
from dataclasses import dataclass, field

from app.config import settings
from app.gps import kinematics as k
from app.gps.units import MS_TO_MPH


@dataclass
class Bucket:
    minute_ms: int
    distance_m: float = 0.0
    speed_sum: float = 0.0
    n: int = 0
    max_speed: float = 0.0
    min_a: float = 0.0
    max_a: float = 0.0
    stopped_s: float = 0.0
    brakes: int = 0
    accels: int = 0
    overs: int = 0


@dataclass
class Analysis:
    distance_m: float = 0.0
    moving_s: float = 0.0
    idle_s: float = 0.0
    max_speed: float = 0.0
    over_s: float = 0.0
    since_stop_ms: int | None = None
    events: list[k.Event] = field(default_factory=list)
    buckets: list[Bucket] = field(default_factory=list)

    def count(self, kind: str) -> int:
        return sum(1 for e in self.events if e.kind == kind)


def _cell(lat_e7: int, lon_e7: int) -> str:
    return f"{round(lat_e7 / 1e7, 2)},{round(lon_e7 / 1e7, 2)}"


def cells(pts: list[k.P]) -> tuple[str, str]:
    return _cell(pts[0].lat, pts[0].lon), _cell(pts[-1].lat, pts[-1].lon)


def _since_stop(pts: list[k.P]) -> int | None:
    """Start of the current moving spell (trip start if never stopped); None while stopped."""
    since, stopped = pts[0].t, False
    for p in pts:
        if p.v == 0:
            stopped = True
        elif stopped:
            since, stopped = p.t, False
    return None if pts[-1].v == 0 else since


def analyse(pts: list[k.P]) -> Analysis:
    a = Analysis(max_speed=max(p.v for p in pts))
    ds = k.deltas(pts)
    a.events = k.find_events(pts, ds)
    limit = settings.over_limit_mph / MS_TO_MPH
    max_dt = settings.max_delta_s * 1000
    buckets: dict[int, Bucket] = {}

    def bucket(t: int) -> Bucket:
        m = t // 60000 * 60000
        return buckets.setdefault(m, Bucket(m))

    smoothed = {d.t1: d.a for d in ds}
    for p0, p1 in zip(pts, pts[1:], strict=False):
        dt = p1.t - p0.t
        if dt > max_dt:
            continue
        sec = dt / 1000
        dist = (p0.v + p1.v) / 2 * sec
        b = bucket(p1.t)
        b.distance_m += dist
        b.speed_sum += p1.v
        b.n += 1
        b.max_speed = max(b.max_speed, p1.v)
        acc = smoothed.get(p1.t, 0.0)
        b.min_a = min(b.min_a, acc)
        b.max_a = max(b.max_a, acc)
        a.distance_m += dist
        if p1.v == 0 and p0.v == 0:
            a.idle_s += sec
            b.stopped_s += sec
        else:
            a.moving_s += sec
        if p1.v > limit:
            a.over_s += sec
    a.since_stop_ms = _since_stop(pts)
    counts: dict[int, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    for e in a.events:
        counts[e.t0 // 60000 * 60000][e.kind] += 1
    for m, c in counts.items():
        b = bucket(m)
        b.brakes, b.accels, b.overs = c[k.BRAKE], c[k.ACCEL], c[k.OVER]
    a.buckets = [buckets[m] for m in sorted(buckets)]
    return a
