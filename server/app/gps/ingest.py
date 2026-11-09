import sqlite3

from app.gps import quality, wire
from app.gps.derive import rebuild_from


def ingest_batch(conn: sqlite3.Connection, device_id: str, batch: wire.Batch, now_ms: int) -> int:
    """Stores a batch (duplicates on (device, UTC time) are ignored) and re-derives. Returns ack."""
    conn.execute(
        "INSERT OR REPLACE INTO fix_batches VALUES (?,?,?,?,?,?)",
        (device_id, batch.seq, batch.fw, batch.drops, len(batch.fixes), now_ms),
    )
    last_dev_ms = batch.fixes[-1].device_ms if batch.fixes else 0
    new_times = []
    for f in batch.fixes:
        if quality.time_ok(f):
            t_ms = f.utc_s * 1000 + f.utc_ms
        else:  # no UTC yet: stamp from receive time, kept for the "no fix" status only
            t_ms = now_ms - max(0, last_dev_ms - f.device_ms)
        valid = quality.is_valid(f)
        cur = conn.execute(
            "INSERT OR IGNORE INTO fixes (device_id, t_ms, device_ms, recv_ms, seq, lat_e7, "
            "lon_e7, speed_cmps, course_deg10, alt_dm, sats, quality, hdop10, flags, valid) "
            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (device_id, t_ms, f.device_ms, now_ms, batch.seq, f.lat_e7, f.lon_e7, f.speed_cmps,
             f.course_deg10, f.alt_dm, f.sats, f.quality, f.hdop10, f.flags, int(valid)),
        )
        if cur.rowcount and valid:
            new_times.append(t_ms)
    if new_times:
        latest = conn.execute(
            "SELECT MAX(t_ms) FROM fixes WHERE device_id = ? AND valid = 1", (device_id,)
        ).fetchone()[0]
        rebuild_from(conn, device_id, min(new_times), latest)
    conn.commit()
    return batch.seq
