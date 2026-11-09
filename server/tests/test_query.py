import os

import pytest
from fastapi.testclient import TestClient
from voice_helpers import RATE, silence_pcm, speech_pcm

from app.auth import create_device
from app.config import settings
from app.main import app
from app.pipeline import service
from app.pipeline.llm import StubLlm
from app.pipeline.service import Pipeline
from app.pipeline.stt import StubStt
from app.pipeline.tts import StubTts
from app.store.db import connect


@pytest.fixture
def env(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    db_dir = tmp_path / "data"
    db_dir.mkdir()
    monkeypatch.setattr(settings, "db_path", str(db_dir / "t.db"))
    stt, llm, tts = StubStt(), StubLlm(), StubTts()
    pipe = Pipeline(stt, llm, tts, context_fn=lambda d, turns: "Speed 30 mph.")
    monkeypatch.setattr(service, "_pipeline", pipe)
    conn = connect()
    token = create_device(conn, "truck")
    conn.close()
    before = set(os.listdir(tmp_path)) | {"data/" + f for f in os.listdir(db_dir)}
    return type(
        "Env",
        (),
        dict(
            client=TestClient(app),
            token=token,
            pipe=pipe,
            stt=stt,
            llm=llm,
            tts=tts,
            auth={"Authorization": f"Bearer {token}"},
            root=tmp_path,
            db=str(db_dir / "t.db"),
            before=before,
        ),
    )


def post(env, pcm, headers=None):
    return env.client.post("/v1/query", content=pcm, headers=headers or env.auth)


def rows(env, table):
    conn = connect()
    try:
        return conn.execute(f"SELECT * FROM {table}").fetchall()
    finally:
        conn.close()


def test_ok_end_to_end(env):
    r = post(env, speech_pcm())
    assert r.status_code == 200
    assert r.headers["x-outcome"] == "ok"
    assert r.headers["content-type"].startswith("audio/L16")
    assert len(r.content) > RATE and len(r.content) % 2 == 0
    assert "DRIVING CONTEXT\nSpeed 30 mph." in env.llm.last_system
    assert env.llm.last_user == "how fast am I going"


def test_clip_rendered_through_tts_provider(env):
    assert len(env.pipe.clips.didnt_catch) > 1000


def test_no_speech_returns_clip(env):
    r = post(env, silence_pcm())
    assert r.headers["x-outcome"] == "no_speech"
    assert r.content == env.pipe.clips.didnt_catch


def test_empty_transcript_is_no_speech(env):
    env.stt.text = "  "
    r = post(env, speech_pcm())
    assert r.headers["x-outcome"] == "no_speech"
    assert r.content == env.pipe.clips.didnt_catch


@pytest.mark.parametrize("which", ["stt", "llm", "tts"])
def test_provider_failure_returns_spoken_clip(env, which):
    getattr(env, which).fail = True
    r = post(env, speech_pcm())
    assert r.status_code == 200
    assert r.headers["x-outcome"] == "error"
    assert r.content == env.pipe.clips.cant_reach_brain
    assert env.client.get("/v1/query/last", headers=env.auth).status_code == 404


def test_clips_fall_back_to_tones_when_tts_is_down(env):
    env.tts.fail = True
    assert len(env.pipe.clips.cant_reach_brain) > 1000
    env.stt.fail = True
    assert post(env, speech_pcm()).headers["x-outcome"] == "error"


def test_repeat_endpoint(env):
    assert env.client.get("/v1/query/last", headers=env.auth).status_code == 404
    first = post(env, speech_pcm())
    env.llm.fail = True  # a repeat must not call any provider
    again = env.client.get("/v1/query/last", headers=env.auth)
    assert again.status_code == 200 and again.content == first.content


def test_repeat_expires(env, monkeypatch):
    post(env, speech_pcm())
    monkeypatch.setattr(settings, "reply_cache_ttl_s", -1)
    assert env.client.get("/v1/query/last", headers=env.auth).status_code == 404


def test_conversation_memory_limits(env, monkeypatch):
    monkeypatch.setattr(settings, "convo_turns", 2)
    for _ in range(3):
        post(env, speech_pcm())
    assert len(env.pipe.memory.recent(next(iter(_device_ids())))) == 2
    monkeypatch.setattr(settings, "convo_ttl_s", -1)
    assert env.pipe.memory.recent(next(iter(_device_ids()))) == []


def _device_ids():
    conn = connect()
    try:
        return [r["id"] for r in conn.execute("SELECT id FROM devices")]
    finally:
        conn.close()


@pytest.mark.parametrize("method,path", [("post", "/v1/query"), ("get", "/v1/query/last")])
def test_auth_required(env, method, path):
    c = env.client
    assert getattr(c, method)(path).status_code == 401
    bad = {"Authorization": "Bearer nope.nope"}
    assert getattr(c, method)(path, headers=bad).status_code == 401


def test_chunked_upload(env):
    pcm = speech_pcm()
    chunks = (pcm[i : i + 3200] for i in range(0, len(pcm), 3200))
    r = env.client.post("/v1/query", content=chunks, headers=env.auth)
    assert r.headers["x-outcome"] == "ok"


def test_upload_is_capped(env):
    r = post(env, speech_pcm(speech_s=1.0, tail_s=(settings.max_utterance_s + 5)))
    assert r.headers["x-outcome"] == "ok"


def test_audio_never_written_to_disk(env):
    post(env, speech_pcm())
    post(env, silence_pcm())
    env.stt.fail = True
    post(env, speech_pcm())
    after = set(os.listdir(env.root)) | {"data/" + f for f in os.listdir(env.root / "data")}
    new = {f for f in after - env.before if not f.startswith("data/t.db")}
    assert new == set()
    for f in os.listdir(env.root / "data"):
        assert os.path.getsize(env.root / "data" / f) < 100_000  # no PCM-sized files


def test_timing_row_has_no_transcript_and_log_has_text(env):
    post(env, speech_pcm())
    (t,) = rows(env, "timings")
    assert "transcript" not in t.keys() and "reply" not in t.keys()
    assert t["outcome"] == "ok"
    for col in (
        "upload_ms",
        "stt_ms",
        "context_ms",
        "llm_total_ms",
        "tts_first_ms",
        "tts_total_ms",
        "first_audio_ms",
        "total_ms",
        "tokens_in",
        "tokens_out",
    ):
        assert t[col] is not None
    assert t["total_ms"] >= t["first_audio_ms"]
    assert not any(isinstance(v, str) and "how fast" in v for v in tuple(t))
    (q,) = rows(env, "query_log")
    assert q["transcript"] == "how fast am I going" and q["reply"]


def test_failed_query_still_logged(env):
    env.llm.fail = True
    post(env, speech_pcm())
    (t,) = rows(env, "timings")
    assert t["outcome"] == "error" and t["llm_total_ms"] is None


def test_lifespan_prerenders_clips(env):
    with TestClient(app) as c:
        assert env.pipe._clips is not None
        assert c.get("/v1/health").text == "ok"
