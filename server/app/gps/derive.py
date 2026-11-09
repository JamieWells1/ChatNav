"""Rebuilds trips, events and minute buckets from raw fixes.

Derived rows are a pure function of the raw rows, so any batch (late, duplicate, out of order)
is handled by re-running the affected span.
"""

import sqlite3

from app.config import settings
from app.gps import kinematics as k
from app.gps.analysis import analyse, cells
from app.gps.trips import TripSeg, segment
from app.store.gps_db import delete_trips_from

LOOKBACK_MS = (settings.trip_start_hold_s + 30) * 1000


def _load(conn: sqlite3.Connection, device_id: str, from_ms: int):
    return conn.execute(
        "SELECT t_ms, speed_cmps, lat_e7, lon_e7, course_deg10, sats, outlier FROM fixes "
        "WHERE device_id = ? AND valid = 1 AND t_ms >= ? ORDER BY t_ms",
        (device_id, from_ms),
    ).fetchall()


def rebuild_from(conn: sqlite3.Connection, device_id: str, min_new_ms: int, now_ms: int) -> None:
    """Re-derives everything that a fix at min_new_ms could change."""
    row = conn.execute(
        "SELECT MIN(start_ms) FROM trips "
        "WHERE device_id = ? AND (status = 'open' OR last_fix_ms >= ?)",
        (device_id, min_new_ms - settings.trip_end_gap_s * 1000),
    ).fetchone()
    from_ms = min_new_ms - LOOKBACK_MS
    if row[0] is not None:
        from_ms = min(from_ms, row[0])
    rows = _load(conn, device_id, from_ms)
    pts = [k.P(r[0], k.clamp_speed(r[1]), r[2], r[3], r[4], r[5]) for r in rows]
    flags = k.flag_outliers(pts)
    changed = [(int(f), device_id, p.t) for f, p, r in zip(flags, pts, rows, strict=True)
               if int(f) != r[6]]
    conn.executemany("UPDATE fixes SET outlier = ? WHERE device_id = ? AND t_ms = ?", changed)
    good = [p for p, bad in zip(pts, flags, strict=True) if not bad]
    delete_trips_from(conn, device_id, from_ms)
    for seg in segment(good, now_ms):
        _store(conn, device_id, seg)


def close_stale(conn: sqlite3.Connection, device_id: str, now_ms: int) -> bool:
    """Closes an open trip whose last fix is older than TRIP_END_GAP_S (power loss, F13)."""
    row = conn.execute(
        "SELECT start_ms, last_fix_ms FROM trips WHERE device_id = ? AND status = 'open'",
        (device_id,),
    ).fetchone()
    if row is None or now_ms - row[1] < settings.trip_end_gap_s * 1000:
        return False
    rebuild_from(conn, device_id, row[0], now_ms)
    conn.commit()
    return True


def _store(conn: sqlite3.Connection, device_id: str, seg: TripSeg) -> None:
    a = analyse(seg.pts)
    if seg.closed and a.distance_m < settings.min_trip_m:
        return
    start_cell, end_cell = cells(seg.pts)
    cur = conn.execute(
        "INSERT INTO trips (device_id, start_ms, end_ms, last_fix_ms, status, close_reason, "
        "distance_m, moving_s, idle_s, max_speed_ms, brakes, accels, overs, over_s, "
        "since_stop_ms, start_cell, end_cell) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        (device_id, seg.pts[0].t, _last_moving_t(seg), seg.pts[-1].t,
         "closed" if seg.closed else "open", seg.reason, a.distance_m, a.moving_s, a.idle_s,
         a.max_speed, a.count(k.BRAKE), a.count(k.ACCEL), a.count(k.OVER), a.over_s,
         a.since_stop_ms, start_cell, end_cell),
    )
    trip_id = cur.lastrowid
    places = settings.event_location_dp
    conn.executemany(
        "INSERT INTO events (device_id, trip_id, kind, t_start_ms, t_end_ms, peak, v_from, v_to, "
        "lat, lon) VALUES (?,?,?,?,?,?,?,?,?,?)",
        [(device_id, trip_id, e.kind, e.t0, e.t1, e.peak, e.v_from, e.v_to,
          round(e.lat / 1e7, places), round(e.lon / 1e7, places)) for e in a.events],
    )
    conn.executemany(
        "INSERT INTO minute_buckets VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
        [(device_id, trip_id, b.minute_ms, b.distance_m, b.speed_sum / max(b.n, 1), b.max_speed,
          b.min_a, b.max_a, b.stopped_s, b.brakes, b.accels, b.overs) for b in a.buckets],
    )


def _last_moving_t(seg: TripSeg) -> int:
    return next(p.t for p in reversed(seg.pts) if p.v > 0)
