"""E1 evidence (plan 10.1): purge and delete-all row counts, and the logged LLM request.

    server/.venv/bin/python tools/privacy_evidence.py > assignment/evidence/m5-privacy/e1_privacy.txt

Throwaway database in a temp dir, synthetic trip (made-up route), stub providers. No real data.
"""

import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "server"))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "server" / "tests"))

from fastapi.testclient import TestClient  # noqa: E402
from gps_helpers import end_ms, ingest, sim  # noqa: E402
from voice_helpers import speech_pcm  # noqa: E402

from app.auth import create_device  # noqa: E402
from app.config import settings  # noqa: E402
from app.context.builder import build_context  # noqa: E402
from app.main import app  # noqa: E402
from app.pipeline import service  # noqa: E402
from app.pipeline.llm import StubLlm  # noqa: E402
from app.pipeline.service import Pipeline  # noqa: E402
from app.pipeline.stt import StubStt  # noqa: E402
from app.pipeline.tts import StubTts  # noqa: E402
from app.store import gps_db, retention, wipe  # noqa: E402
from app.store.db import connect  # noqa: E402

TABLES = ("fixes", "fix_batches", "trips", "events", "minute_buckets", "query_log", "timings")
H, D = retention.H_MS, retention.D_MS


def table(conn, title, rows=None):
    print(f"{'table':<18}{'rows':>8}   {title}")
    for t in TABLES:
        print(f"{t:<18}{conn.execute(f'SELECT COUNT(*) FROM {t}').fetchone()[0]:>8}")
    ev = conn.execute("SELECT COUNT(*) FROM events WHERE lat IS NOT NULL").fetchone()[0]
    qt = conn.execute("SELECT COUNT(*) FROM query_log WHERE transcript IS NOT NULL").fetchone()[0]
    print(f"{'events w/ location':<18}{ev:>8}\n{'queries w/ text':<18}{qt:>8}\n")


def main() -> None:
    tmp = tempfile.mkdtemp()
    settings.db_path = str(Path(tmp) / "e1.db")
    conn = gps_db.connect()
    token = create_device(conn, "e1-throwaway")
    dev = token.split(".")[0]
    rec = sim("harsh", lat=52.2053, lon=0.1218)
    ingest(conn, rec, device=dev)
    end = end_ms(rec)
    conn.execute("INSERT INTO query_log (device_id, t_ms, transcript, reply, outcome) "
                 "VALUES (?, ?, 'how fast am I going', 'thirty miles an hour', 'ok')", (dev, end))
    conn.execute("INSERT INTO timings (device_id, t_ms, outcome) VALUES (?, ?, 'ok')", (dev, end))
    conn.commit()

    print("# E1a. Purge: row counts before and after each simulated run (retention from plan 14)")
    print("# raw 48 h, event location 30 d, query text 7 d, minute buckets 30 d, events 180 d,")
    print("# timings 90 d, trips 365 d\n")
    table(conn, "BEFORE (t = trip end)")
    for label, ms in (("t + 49 h", 49 * H), ("t + 8 d", 8 * D), ("t + 31 d", 31 * D),
                      ("t + 91 d", 91 * D), ("t + 181 d", 181 * D), ("t + 366 d", 366 * D)):
        out = retention.purge(conn, end + ms)
        print(f"-- purge run at {label}: deleted per tier")
        print("   " + ", ".join(f"{k}={v}" for k, v in out.items() if k != "vacuumed"))
        table(conn, f"AFTER {label}")

    print("# E1b. Delete-all (U14): voice command twice, then rows for the device\n")
    conn.close()
    conn = gps_db.connect()
    ingest(conn, rec, device=dev)
    conn.execute("INSERT INTO query_log (device_id, t_ms, transcript, reply, outcome) "
                 "VALUES (?, ?, 'how fast am I going', 'thirty miles an hour', 'ok')", (dev, end))
    conn.commit()
    table(conn, "BEFORE delete-all")
    stt, llm = StubStt("delete my driving history"), StubLlm()
    pipe = Pipeline(stt, llm, StubTts(), context_fn=lambda d, t: "")
    service._pipeline = pipe
    client = TestClient(app)
    hdr = {"Authorization": f"Bearer {token}"}
    r1 = client.post("/v1/query", content=speech_pcm(), headers=hdr)
    print(f"request 1 (arms): confirm clip={r1.content == pipe.clips.delete_confirm}")
    table(conn, "AFTER request 1 (nothing deleted)")
    r2 = client.post("/v1/query", content=speech_pcm(), headers=hdr)
    print(f"request 2 (confirms): data-deleted clip={r2.content == pipe.clips.data_deleted}, "
          f"LLM called={bool(llm.last_user)}")
    table(conn, "AFTER request 2")
    print("counts per device:", wipe.counts(conn, dev), "\n")

    print("# E1c. The exact LLM request (system prompt + user message) for one query\n")
    ingest(conn, rec, device=dev)
    now = end + 1000
    stt.text = "was that a harsh brake just now"

    def context(device_id, turns):  # own connection: the request runs in another thread
        c = gps_db.connect()
        try:
            return build_context(c, device_id, now, turns)
        finally:
            c.close()

    pipe.context_fn = context
    client.post("/v1/query", content=speech_pcm(), headers=hdr)
    request = f"[system]\n{llm.last_system}\n[user]\n{llm.last_user}"
    print(request)
    coords = re.findall(r"-?\d{1,3}\.\d{3,}", request)
    print(f"\n# decimal numbers with 3+ places in the request: {coords or 'none'}")
    print("# synthetic fix coordinates were 52.2053, 0.1218: "
          f"present={'52.2' in request or '0.1218' in request}")


if __name__ == "__main__":
    main()
