"""Day and week rollups, computed on demand from trip summaries (plan 8.3)."""

import sqlite3
from dataclasses import dataclass
from datetime import UTC, datetime, time, timedelta
from zoneinfo import ZoneInfo

from app.gps.units import M_PER_MILE

LOCAL_TZ = ZoneInfo("Europe/London")


@dataclass(frozen=True)
class Rollup:
    trips: int = 0
    duration_s: float = 0.0
    distance_m: float = 0.0
    moving_s: float = 0.0
    brakes: int = 0
    accels: int = 0
    overs: int = 0
    over_s: float = 0.0
    max_speed_ms: float = 0.0

    @property
    def miles(self) -> float:
        return self.distance_m / M_PER_MILE

    @property
    def avg_moving_ms(self) -> float:
        return self.distance_m / self.moving_s if self.moving_s else 0.0

    def per_100_miles(self, count: int) -> float:
        return count / self.miles * 100 if self.miles > 0 else 0.0


def rollup(conn: sqlite3.Connection, device_id: str, start_ms: int, end_ms: int) -> Rollup:
    r = conn.execute(
        "SELECT COUNT(*), COALESCE(SUM(end_ms - start_ms), 0) / 1000.0, SUM(distance_m), "
        "SUM(moving_s), SUM(brakes), SUM(accels), SUM(overs), SUM(over_s), MAX(max_speed_ms) "
        "FROM trips WHERE device_id = ? AND start_ms >= ? AND start_ms < ?",
        (device_id, start_ms, end_ms),
    ).fetchone()
    if r[0] == 0:
        return Rollup()
    return Rollup(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8])


def local_day_start_ms(now_ms: int, days_back: int = 0) -> int:
    local = datetime.fromtimestamp(now_ms / 1000, UTC).astimezone(LOCAL_TZ)
    day = local.date() - timedelta(days=days_back)
    return int(datetime.combine(day, time.min, LOCAL_TZ).timestamp() * 1000)


def day_rollup(conn: sqlite3.Connection, device_id: str, now_ms: int, days_back: int = 0) -> Rollup:
    start = local_day_start_ms(now_ms, days_back)
    return rollup(conn, device_id, start, local_day_start_ms(now_ms, days_back - 1))


def week_rollup(
    conn: sqlite3.Connection, device_id: str, now_ms: int, weeks_back: int = 0, span: int = 1
) -> Rollup:
    """Trailing 7-day weeks ending today; weeks_back=0 is the last 7 days including today."""
    start = local_day_start_ms(now_ms, 7 * (weeks_back + span - 1) + 6)
    end = local_day_start_ms(now_ms, 7 * weeks_back - 1)
    return rollup(conn, device_id, start, end)
