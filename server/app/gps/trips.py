"""Trip segmentation (plan 8.2) over time-ordered valid, non-outlier points."""

from dataclasses import dataclass

from app.config import settings
from app.gps.kinematics import P


@dataclass
class TripSeg:
    pts: list[P]
    closed: bool
    reason: str | None  # idle | gap


def _trim_to_last_moving(cur: list[P]) -> list[P]:
    i = len(cur)
    while i > 1 and cur[i - 1].v == 0:
        i -= 1
    return cur[:i]


def segment(pts: list[P], now_ms: int) -> list[TripSeg]:
    """Splits points into trips. now_ms closes a trip whose last fix is older than the gap rule."""
    idle_ms = settings.trip_end_idle_s * 1000
    gap_ms = settings.trip_end_gap_s * 1000
    hold_ms = settings.trip_start_hold_s * 1000
    max_dt = settings.max_delta_s * 1000
    trips: list[TripSeg] = []
    cur: list[P] | None = None
    last_moving = 0
    moving: list[P] = []  # contiguous moving points before a trip opens
    fast_since: int | None = None

    for p in pts:
        if cur is not None:
            if p.t - cur[-1].t >= gap_ms:
                trips.append(TripSeg(_trim_to_last_moving(cur), True, "gap"))
                cur = None
            elif cur[-1].v == 0 and p.t - last_moving >= idle_ms:
                trips.append(TripSeg(_trim_to_last_moving(cur), True, "idle"))
                cur = None
        if cur is not None:
            cur.append(p)
            if p.v > 0:
                last_moving = p.t
            continue
        if moving and (p.v == 0 or p.t - moving[-1].t > max_dt):
            moving, fast_since = [], None
        if p.v == 0:
            continue
        moving.append(p)
        if p.v > settings.trip_start_speed_ms:
            fast_since = fast_since if fast_since is not None else p.t
            if p.t - fast_since >= hold_ms:
                cur, moving, fast_since = moving, [], None
                last_moving = p.t
        else:
            fast_since = None

    if cur is not None:
        if cur[-1].v == 0 and cur[-1].t - last_moving >= idle_ms:
            trips.append(TripSeg(_trim_to_last_moving(cur), True, "idle"))
        elif now_ms - cur[-1].t >= gap_ms:
            trips.append(TripSeg(_trim_to_last_moving(cur), True, "gap"))
        else:
            trips.append(TripSeg(cur, False, None))
    return trips
