"""GPS tables on top of the shared connection (devices table lives in store/db.py)."""

import sqlite3

from app.store import db
from app.store.gps_schema import GPS_SCHEMA


def connect() -> sqlite3.Connection:
    conn = db.connect()
    conn.executescript(GPS_SCHEMA)
    return conn


def delete_trips_from(conn: sqlite3.Connection, device_id: str, from_ms: int) -> None:
    ids = "SELECT id FROM trips WHERE device_id = ? AND start_ms >= ?"
    args = (device_id, from_ms)
    conn.execute(f"DELETE FROM events WHERE trip_id IN ({ids})", args)
    conn.execute(f"DELETE FROM minute_buckets WHERE trip_id IN ({ids})", args)
    conn.execute("DELETE FROM trips WHERE device_id = ? AND start_ms >= ?", args)
