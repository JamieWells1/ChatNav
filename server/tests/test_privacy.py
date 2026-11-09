"""M5: delete-all (U14), rate limit, no coordinates to the LLM or logs (E1), no audio table (E3)."""

import logging
import re

import pytest
from fastapi.testclient import TestClient
from gps_helpers import end_ms, ingest, sim
from voice_helpers import speech_pcm

from app import ratelimit
from app.auth import create_device
from app.cli import main as cli_main
from app.config import settings
from app.context.builder import build_context
from app.main import app
from app.pipeline import service
from app.pipeline.llm import StubLlm
from app.pipeline.service import Pipeline
from app.pipeline.stt import StubStt
from app.pipeline.tts import StubTts
from app.store import gps_db, wipe
from app.store.db import connect

COORD = re.compile(r"-?\d{1,3}\.\d{3,}")
LAT, LON = 52.2053, 0.1218


@pytest.fixture(autouse=True)
def fresh_state():
    ratelimit.limiter.reset()
    wipe.guard._armed.clear()


@pytest.fixture
def env(tmp_path, monkeypatch):
    monkeypatch.setattr(settings, "db_path", str(tmp_path / "t.db"))
    stt, llm = StubStt(), StubLlm()
    pipe = Pipeline(stt, llm, StubTts(), context_fn=lambda d, turns: "Speed 30 mph.")
    monkeypatch.setattr(service, "_pipeline", pipe)
    conn = connect()
    token = create_device(conn, "truck")
    device_id = token.split(".")[0]
    conn.close()
    return type("Env", (), dict(
        client=TestClient(app), stt=stt, llm=llm, pipe=pipe, device_id=device_id,
        auth={"Authorization": f"Bearer {token}"}, token=token,
    ))


def seed(env, device=None):
    conn = gps_db.connect()
    ingest(conn, sim("harsh", lat=LAT, lon=LON), device=device or env.device_id)
    conn.execute("INSERT INTO query_log (device_id, t_ms, transcript, reply, outcome) "
                 "VALUES (?, 1, 'q', 'a', 'ok')", (device or env.device_id,))
    conn.execute("INSERT INTO timings (device_id, t_ms, outcome) VALUES (?, 1, 'ok')",
                 (device or env.device_id,))
    conn.commit()
    return conn


def total(conn, device):
    return sum(wipe.counts(conn, device).values())


# --- delete-all ---------------------------------------------------------------------------

def test_http_delete_needs_auth(env):
    assert env.client.delete("/v1/data").status_code == 401


def test_http_delete_confirm_window(env):
    conn = seed(env)
    other = seed(env, device="other")
    assert total(conn, env.device_id) > 100
    r = env.client.delete("/v1/data", headers=env.auth)
    assert r.status_code == 202 and r.json()["status"] == "confirm"
    assert total(conn, env.device_id) > 100  # first call deletes nothing
    r = env.client.delete("/v1/data", headers=env.auth)
    assert r.status_code == 200
    body = r.json()
    assert body["before"]["fixes"] > 0 and set(body["after"].values()) == {0}
    assert total(conn, env.device_id) == 0
    assert total(other, "other") > 100  # only this device
    assert conn.execute("SELECT COUNT(*) FROM devices").fetchone()[0] == 1  # token kept


def test_http_delete_confirm_expires(env, monkeypatch):
    conn = seed(env)
    monkeypatch.setattr(settings, "delete_confirm_s", -1)  # window already over
    env.client.delete("/v1/data", headers=env.auth)
    r = env.client.delete("/v1/data", headers=env.auth)
    assert r.status_code == 202 and total(conn, env.device_id) > 100


@pytest.mark.parametrize("text,match", [
    ("delete my history", True),
    ("Please delete all my data.", True),
    ("delete my driving history", True),
    ("delete the timer", False),
    ("don't delete my history", False),
    ("how do I delete my data", False),
    ("what is my history", False),
])
def test_command_matcher(text, match):
    assert wipe.is_delete_command(text) is match


def test_voice_delete_needs_second_request(env):
    conn = seed(env)
    env.stt.text = "delete my driving history"
    env.pipe.memory.add(env.device_id, "q", "a")
    r = env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    assert r.status_code == 200 and r.content == env.pipe.clips.delete_confirm
    assert total(conn, env.device_id) > 100 and env.llm.last_user == ""
    r = env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    assert r.content == env.pipe.clips.data_deleted
    assert total(conn, env.device_id) == 0  # incl. this command: no query_log row afterwards
    assert env.pipe.memory.recent(env.device_id) == []
    assert env.llm.last_user == ""  # the LLM never saw the command


def test_voice_delete_unrelated_query_resets_nothing_but_does_not_fire(env):
    conn = seed(env)
    env.stt.text = "delete my history"
    env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    env.stt.text = "how fast am I going"
    env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    assert total(conn, env.device_id) > 100


def test_cli_delete_dry_run_then_yes(env, capsys):
    conn = seed(env)
    conn.execute("UPDATE devices SET name = 'truck'")
    conn.commit()
    assert cli_main(["data", "delete", "truck"]) == 0
    assert total(conn, env.device_id) > 100 and "nothing deleted" in capsys.readouterr().out
    assert cli_main(["data", "delete", "truck", "--yes"]) == 0
    assert total(conn, env.device_id) == 0
    assert cli_main(["data", "delete", "nobody"]) == 1


# --- rate limit ---------------------------------------------------------------------------

def test_rate_limit_429(env, monkeypatch):
    monkeypatch.setattr(settings, "rate_limit_queries_per_min", 3)
    codes = [env.client.post("/v1/query", content=speech_pcm(), headers=env.auth).status_code
             for _ in range(5)]
    assert codes == [200, 200, 200, 429, 429]
    r = env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    assert int(r.headers["Retry-After"]) >= 1
    # other endpoints and bad tokens are not counted against the device
    assert env.client.get("/v1/whoami", headers=env.auth).status_code == 200
    bad = {"Authorization": "Bearer nope.nope"}
    assert env.client.post("/v1/query", content=b"", headers=bad).status_code == 401


def test_rate_limit_is_per_device_and_slides(env):
    lim = ratelimit.SlidingWindow()
    assert [lim.allow("a", 2, now=t) for t in (0, 1, 2)] == [0.0, 0.0, 58.0]
    assert lim.allow("b", 2, now=2) == 0.0
    assert lim.allow("a", 2, now=60.5) == 0.0  # first hit has left the window


# --- E1: no coordinates to the LLM or in logs -----------------------------------------------

def test_llm_request_and_logs_have_no_coordinates(env, tmp_path, caplog):
    conn = seed(env)
    rec = sim("harsh", lat=LAT, lon=LON)
    now = end_ms(rec) + 1000

    def context(device_id, turns):  # own connection: the request runs in another thread
        c = gps_db.connect()
        try:
            return build_context(c, device_id, now, turns)
        finally:
            c.close()

    env.pipe.context_fn = context
    assert conn.execute("SELECT COUNT(*) FROM events WHERE lat IS NOT NULL").fetchone()[0] > 0
    env.stt.text = "was that a harsh brake just now"
    with caplog.at_level(logging.DEBUG):
        r = env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    assert r.status_code == 200
    request = env.llm.last_system + "\n" + env.llm.last_user  # exactly what the provider gets
    assert "DRIVING CONTEXT" in request and "No driving data" not in request
    assert re.search(r"harsh (brak|accel)", request, re.I)  # the real context was included
    assert not COORD.search(request)
    assert "52.2" not in request and "0.121" not in request
    assert not re.search(r"\b(lat|lon|lng|latitude|longitude)\b", request, re.I)
    logged = conn.execute("SELECT transcript, reply FROM query_log WHERE t_ms > 1").fetchall()
    assert logged and not any(COORD.search(" ".join(map(str, row))) for row in logged)
    assert not COORD.search(caplog.text) and env.token not in caplog.text
    assert env.token.split(".")[1] not in caplog.text


def test_no_source_logs_tokens_or_coordinates():
    """Static check: no logging call in app/ names a token, secret, lat, lon or the auth header."""
    from pathlib import Path

    bad = re.compile(r"\b(token|secret|authorization|lat|lon|lat_e7|lon_e7|creds)\b", re.I)
    hits = []
    for f in Path(service.__file__).parents[1].rglob("*.py"):
        for n, line in enumerate(f.read_text().splitlines(), 1):
            call = re.search(r"\blog(ger)?\.(debug|info|warning|error|exception)\((.*)", line)
            if call and bad.search(call.group(3)):
                hits.append(f"{f.name}:{n}")
    assert not hits, hits


# --- E3: no audio table --------------------------------------------------------------------

def test_schema_has_no_audio_table_or_column(env):
    conn = gps_db.connect()
    names = []
    for (t,) in conn.execute("SELECT name FROM sqlite_master WHERE type = 'table'"):
        names.append(t)
        names += [c[1] for c in conn.execute(f"PRAGMA table_info({t})")]
    assert names
    assert not [n for n in names if re.search(r"audio|wav|pcm|speech|recording|blob", n, re.I)
                if n != "first_audio_ms"]  # a latency number
    types = {c[2] for (t,) in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")
             for c in conn.execute(f"PRAGMA table_info({t})")}
    assert types <= {"", "TEXT", "INTEGER", "REAL", "BLOB"}  # "" = sqlite_sequence
    blobs = [(t, c[1]) for (t,) in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")
             for c in conn.execute(f"PRAGMA table_info({t})") if c[2] == "BLOB"]
    assert set(blobs) == {("devices", "salt"), ("devices", "token_hash")}  # only token hashing


def test_query_stores_no_audio_on_disk(env, tmp_path):
    env.client.post("/v1/query", content=speech_pcm(), headers=env.auth)
    files = [p for p in tmp_path.rglob("*") if p.is_file()]
    assert all(p.suffix in ("", ".db", ".db-journal", ".db-wal", ".db-shm") for p in files)
