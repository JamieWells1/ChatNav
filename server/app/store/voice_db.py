"""Writes timing and query-log rows. The tables live in store/gps_schema.py (shared with purge)."""

import time

from app.store import gps_db

# our timing key -> column in the timings table
TIMING_COLUMNS = {
    "upload_ms": "upload_ms",
    "stt_ms": "stt_ms",
    "context_ms": "context_ms",
    "llm_first_ms": "llm_first_ms",
    "llm_ms": "llm_total_ms",
    "tts_first_ms": "tts_first_ms",
    "tts_ms": "tts_total_ms",
    "first_audio_sent_ms": "first_audio_ms",
    "total_ms": "total_ms",
    "tokens_in": "tokens_in",
    "tokens_out": "tokens_out",
    "cost_usd": "cost_usd",
}


def record_query(
    device_id: str, outcome: str, timing: dict, transcript: str | None, reply: str | None
) -> None:
    """One timing row (numbers only) and one query-log row (text only)."""
    now_ms = int(time.time() * 1000)
    cols = ", ".join(TIMING_COLUMNS.values())
    marks = ", ".join("?" * len(TIMING_COLUMNS))
    conn = gps_db.connect()
    try:
        conn.execute(
            f"INSERT INTO timings (device_id, t_ms, outcome, {cols}) VALUES (?, ?, ?, {marks})",
            (device_id, now_ms, outcome, *(timing.get(k) for k in TIMING_COLUMNS)),
        )
        conn.execute(
            "INSERT INTO query_log (device_id, t_ms, transcript, reply, outcome) "
            "VALUES (?, ?, ?, ?, ?)",
            (device_id, now_ms, transcript, reply, outcome),
        )
        conn.commit()
    finally:
        conn.close()
