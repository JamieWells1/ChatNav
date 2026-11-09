import sqlite3

from app.config import settings

SCHEMA = """
CREATE TABLE IF NOT EXISTS devices (
    id           TEXT PRIMARY KEY,
    name         TEXT NOT NULL UNIQUE,
    salt         BLOB NOT NULL,
    token_hash   BLOB NOT NULL,
    created_at   TEXT NOT NULL,
    revoked_at   TEXT,
    last_seen_at TEXT
);
"""


def connect() -> sqlite3.Connection:
    conn = sqlite3.connect(settings.db_path)
    conn.row_factory = sqlite3.Row
    conn.executescript(SCHEMA)
    return conn
