"""Delete-all (U14): every tier for one device, plus the confirm guard for the two entry points."""

import logging
import sqlite3
import threading
import time

from app.config import settings
from app.store import gps_db
from app.store.retention import vacuum

log = logging.getLogger("chatnav.wipe")

# Every table that holds a device's data. The devices table (token hash) is kept: wiping data
# is not revoking the device.
DEVICE_TABLES = (
    "fixes", "fix_batches", "events", "minute_buckets", "trips", "query_log", "timings",
)

# Words after normalising (lowercase, letters only). Fixed rule, no LLM: must say "delete" and
# "history" or "data", and must not be negated or a how-to question.
_NEGATIONS = {"dont", "not", "never", "cancel", "stop", "how", "what", "why", "should"}


def is_delete_command(transcript: str) -> bool:
    words = "".join(c if c.isalpha() else " " for c in transcript.lower().replace("'", "")).split()
    ws = set(words)
    return "delete" in ws and bool(ws & {"history", "data"}) and not ws & _NEGATIONS


def counts(conn: sqlite3.Connection, device_id: str) -> dict[str, int]:
    return {
        t: conn.execute(f"SELECT COUNT(*) FROM {t} WHERE device_id = ?", (device_id,)).fetchone()[0]
        for t in DEVICE_TABLES
    }


def wipe_device(device_id: str, conn: sqlite3.Connection | None = None) -> dict[str, dict]:
    """Deletes the device's rows from every tier, then VACUUM. Returns before/after counts."""
    own = conn is None
    conn = conn or gps_db.connect()
    try:
        before = counts(conn, device_id)
        for t in DEVICE_TABLES:
            conn.execute(f"DELETE FROM {t} WHERE device_id = ?", (device_id,))
        conn.commit()
        vacuum(conn)
        after = counts(conn, device_id)
    finally:
        if own:
            conn.close()
    log.info("[WIPE] before=%s after=%s", before, after)  # row counts only, no ids or data
    return {"before": before, "after": after}


class DeleteGuard:
    """First request arms (per device and channel); a repeat within DELETE_CONFIRM_S fires."""

    def __init__(self) -> None:
        self._armed: dict[tuple[str, str], float] = {}
        self._lock = threading.Lock()

    def confirmed(self, device_id: str, channel: str) -> bool:
        """True when this request completes a pending one; otherwise arms and returns False."""
        key, now = (device_id, channel), time.monotonic()
        with self._lock:
            armed = self._armed.pop(key, None)
            if armed is not None and now - armed <= settings.delete_confirm_s:
                return True
            self._armed[key] = now
            return False


guard = DeleteGuard()
