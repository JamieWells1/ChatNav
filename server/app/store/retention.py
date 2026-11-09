"""Purge job (plan 8.5): derive -> verify -> delete raw, then per-tier retention deletes.

Everything takes `now_ms` so tests can move the clock. Returns rows deleted per tier and logs
the same as one structured line (evidence for plan section 10, E1).
"""

import asyncio
import json
import logging
import sqlite3
import time

from fastapi.concurrency import run_in_threadpool

from app.config import settings
from app.gps.derive import close_stale, rebuild_from
from app.store import gps_db

log = logging.getLogger("chatnav.retention")

H_MS = 3_600_000
D_MS = 24 * H_MS
VACUUM_EVERY_MS = D_MS


def _derive(conn: sqlite3.Connection, now_ms: int) -> int:
    """Makes sure raw fixes about to be purged have derived rows. Returns devices re-derived."""
    raw_cut = now_ms - settings.raw_retention_h * H_MS
    n = 0
    for (dev,) in conn.execute("SELECT DISTINCT device_id FROM fixes").fetchall():
        # Trips still open but silent past the gap are closed (and so derived) first.
        close_stale(conn, dev, now_ms)
        # Moving raw fixes after the newest derived trip were never derived (ingest failed or
        # the trip rows were lost): derive from the first of them. Parked tails do not count.
        first = conn.execute(
            "SELECT MIN(t_ms) FROM fixes WHERE device_id = ? AND valid = 1 AND t_ms < ? "
            "AND speed_cmps >= ? "
            "AND t_ms > COALESCE((SELECT MAX(last_fix_ms) FROM trips WHERE device_id = ?), 0)",
            (dev, raw_cut, int(settings.stationary_speed_ms * 100), dev),
        ).fetchone()[0]
        if first is not None:
            rebuild_from(conn, dev, first, now_ms)
            conn.commit()
            n += 1
    return n


# A fix is held back while its trip is open or has no minute buckets (not yet derived), until
# RAW_HARD_LIMIT_H. The EXISTS check is the "verify derived rows exist" step.
_RAW_DELETE = """
DELETE FROM fixes WHERE t_ms < :raw AND (t_ms < :hard OR NOT EXISTS (
    SELECT 1 FROM trips tr WHERE tr.device_id = fixes.device_id
      AND fixes.t_ms BETWEEN tr.start_ms AND tr.last_fix_ms
      AND (tr.status = 'open'
           OR NOT EXISTS (SELECT 1 FROM minute_buckets mb WHERE mb.trip_id = tr.id))))
"""


def purge(conn: sqlite3.Connection, now_ms: int | None = None) -> dict[str, int]:
    now_ms = int(time.time() * 1000) if now_ms is None else now_ms
    s = settings
    out: dict[str, int] = {"derived_devices": _derive(conn, now_ms)}

    def run(tier: str, sql: str, args=()) -> None:
        out[tier] = conn.execute(sql, args).rowcount

    raw_cut = now_ms - s.raw_retention_h * H_MS
    hard_cut = now_ms - s.raw_hard_limit_h * H_MS
    run("raw_fixes", _RAW_DELETE, {"raw": raw_cut, "hard": hard_cut})
    run("fix_batches", "DELETE FROM fix_batches WHERE recv_ms < ?", (raw_cut,))
    run("events", "DELETE FROM events WHERE t_start_ms < ?", (now_ms - s.event_retention_d * D_MS,))
    run("minute_buckets", "DELETE FROM minute_buckets WHERE minute_ms < ?",
        (now_ms - s.minute_retention_d * D_MS,))
    old_trips = "SELECT id FROM trips WHERE status = 'closed' AND end_ms < ?"
    trip_cut = (now_ms - s.trip_retention_d * D_MS,)
    conn.execute(f"DELETE FROM events WHERE trip_id IN ({old_trips})", trip_cut)
    conn.execute(f"DELETE FROM minute_buckets WHERE trip_id IN ({old_trips})", trip_cut)
    run("trips", "DELETE FROM trips WHERE status = 'closed' AND end_ms < ?", trip_cut)
    run("event_locations_nulled",
        "UPDATE events SET lat = NULL, lon = NULL WHERE t_start_ms < ? AND lat IS NOT NULL",
        (now_ms - s.event_location_retention_d * D_MS,))
    run("query_text_nulled",
        "UPDATE query_log SET transcript = NULL, reply = NULL WHERE t_ms < ? "
        "AND (transcript IS NOT NULL OR reply IS NOT NULL)",
        (now_ms - s.query_text_retention_d * D_MS,))
    timing_cut = (now_ms - s.timing_retention_d * D_MS,)
    run("timings", "DELETE FROM timings WHERE t_ms < ?", timing_cut)
    run("query_log", "DELETE FROM query_log WHERE t_ms < ?", timing_cut)
    conn.commit()
    out["vacuumed"] = int(vacuum_if_due(conn, now_ms))
    log.info("[PURGE] %s", json.dumps(out, sort_keys=True))
    return out


def vacuum(conn: sqlite3.Connection) -> None:
    """VACUUM so deleted rows leave the file. Needs no open transaction."""
    conn.commit()
    conn.execute("VACUUM")


def vacuum_if_due(conn: sqlite3.Connection, now_ms: int) -> bool:
    row = conn.execute("SELECT value FROM meta WHERE key = 'last_vacuum_ms'").fetchone()
    if row is not None and now_ms - int(row[0]) < VACUUM_EVERY_MS:
        return False
    vacuum(conn)
    conn.execute("INSERT OR REPLACE INTO meta VALUES ('last_vacuum_ms', ?)", (str(now_ms),))
    conn.commit()
    return True


def purge_once(now_ms: int | None = None) -> dict[str, int]:
    conn = gps_db.connect()
    try:
        return purge(conn, now_ms)
    finally:
        conn.close()


async def purge_loop() -> None:
    """Once at startup, then every PURGE_INTERVAL_MIN. A failed run is logged, never fatal."""
    while True:
        try:
            await run_in_threadpool(purge_once)
        except Exception as e:
            log.error("[PURGE] run failed: %s", type(e).__name__)
        await asyncio.sleep(settings.purge_interval_min * 60)
