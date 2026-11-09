"""Pure maths over valid fixes: drift clamp, outliers, smoothed acceleration, events."""

from collections import deque
from dataclasses import dataclass

from app.config import settings
from app.gps.units import KMH_TO_MS, MS_TO_MPH

BRAKE = "harsh_brake"
ACCEL = "harsh_accel"
OVER = "over_limit"


@dataclass(slots=True)
class P:
    t: int  # UTC ms
    v: float  # m/s, drift-clamped
    lat: int  # degrees x 1e7
    lon: int
    course: int  # degrees x 10
    sats: int


@dataclass(slots=True)
class Delta:
    t0: int
    t1: int
    v0: float
    v1: float
    a: float  # smoothed m/s2


@dataclass(slots=True)
class Event:
    kind: str
    t0: int
    t1: int
    peak: float  # m/s2 for brake/accel, m/s for over_limit
    v_from: float
    v_to: float
    lat: int
    lon: int


def clamp_speed(cmps: int) -> float:
    v = cmps / 100
    return 0.0 if v < settings.stationary_speed_ms else v


def flag_outliers(pts: list[P]) -> list[bool]:
    """True where implied acceleration against the last accepted fix is implausible."""
    out = []
    prev = None
    max_dt = settings.max_delta_s * 1000
    for p in pts:
        bad = False
        if prev is not None and 0 < p.t - prev.t <= max_dt:
            bad = abs(p.v - prev.v) / ((p.t - prev.t) / 1000) > settings.max_plausible_accel_ms2
        out.append(bad)
        if not bad:
            prev = p
    return out


def deltas(pts: list[P]) -> list[Delta]:
    """Per-interval acceleration, smoothed over the last ACCEL_SMOOTH_N intervals.

    Intervals longer than MAX_DELTA_S are skipped and reset the smoothing window.
    """
    out = []
    window: deque[float] = deque(maxlen=settings.accel_smooth_n)
    max_dt = settings.max_delta_s * 1000
    for a, b in zip(pts, pts[1:], strict=False):
        dt = b.t - a.t
        if dt <= 0 or dt > max_dt:
            window.clear()
            continue
        window.append((b.v - a.v) / (dt / 1000))
        out.append(Delta(a.t, b.t, a.v, b.v, sum(window) / len(window)))
    return out


def _runs(ds: list[Delta], hit) -> list[list[Delta]]:
    runs: list[list[Delta]] = []
    cur: list[Delta] = []
    for d in ds:
        if hit(d) and (not cur or cur[-1].t1 == d.t0):
            cur.append(d)
            continue
        if cur:
            runs.append(cur)
        cur = [d] if hit(d) else []
    if cur:
        runs.append(cur)
    return runs


def _point_at(pts: list[P], t: int) -> P:
    return next(p for p in pts if p.t >= t)


def _onset(ds: list[Delta], run: list[Delta]) -> Delta:
    """First interval of the real manoeuvre: smoothing delays the run by up to N-1 intervals."""
    i = next(j for j, d in enumerate(ds) if d is run[0])
    first = run[0]
    for d in reversed(ds[max(0, i - settings.accel_smooth_n + 1) : i]):
        if d.t1 != first.t0:
            break
        first = d
    return first


def find_events(pts: list[P], ds: list[Delta]) -> list[Event]:
    events = []
    min_ms = settings.harsh_min_duration_s * 1000
    min_v = settings.harsh_min_speed_kmh * KMH_TO_MS
    for run in _runs(ds, lambda d: d.a <= settings.harsh_brake_ms2):
        if run[-1].t1 - run[0].t0 >= min_ms:
            on = _onset(ds, run)
            if on.v0 < min_v:
                continue
            p = _point_at(pts, on.t0)
            events.append(
                Event(BRAKE, on.t0, run[-1].t1, min(d.a for d in run),
                      on.v0, run[-1].v1, p.lat, p.lon)
            )
    for run in _runs(ds, lambda d: d.a >= settings.harsh_accel_ms2):
        if run[-1].t1 - run[0].t0 >= min_ms:
            on = _onset(ds, run)
            p = _point_at(pts, on.t0)
            events.append(
                Event(ACCEL, on.t0, run[-1].t1, max(d.a for d in run),
                      on.v0, run[-1].v1, p.lat, p.lon)
            )
    events += _over_limit(pts)
    return sorted(events, key=lambda e: e.t0)


def _over_limit(pts: list[P]) -> list[Event]:
    limit = settings.over_limit_mph / MS_TO_MPH
    max_dt = settings.max_delta_s * 1000
    events = []
    spell: list[P] = []

    def close():
        if spell and spell[-1].t - spell[0].t >= settings.over_limit_min_s * 1000:
            events.append(
                Event(OVER, spell[0].t, spell[-1].t, max(p.v for p in spell),
                      spell[0].v, spell[-1].v, spell[0].lat, spell[0].lon)
            )

    for p in pts:
        if p.v > limit and (not spell or p.t - spell[-1].t <= max_dt):
            spell.append(p)
            continue
        close()
        spell = [p] if p.v > limit else []
    close()
    return events


def peak_accels(ds: list[Delta], since_ms: int) -> tuple[float, float] | None:
    """(strongest braking as negative, strongest acceleration) for intervals ending after since."""
    recent = [d.a for d in ds if d.t1 >= since_ms]
    if not recent:
        return None
    return min(0.0, min(recent)), max(0.0, max(recent))
