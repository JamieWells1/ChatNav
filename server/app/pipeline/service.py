"""The voice pipeline: audio prep, STT, context, LLM, TTS, with failures turned into clips."""

import logging
import threading
import time
from collections.abc import Callable, Iterator
from dataclasses import dataclass, field
from pathlib import Path

from app.config import settings
from app.pipeline import audio
from app.pipeline.clips import Clips, render_clips
from app.pipeline.llm import LlmProvider, make_llm
from app.pipeline.memory import ConversationMemory, ReplyCache
from app.pipeline.stt import SttProvider, make_stt
from app.pipeline.tts import TtsProvider, make_tts, stream_pcm
from app.store import voice_db, wipe

log = logging.getLogger("chatnav.pipeline")

PROMPT_PATH = Path(__file__).parent.parent / "context" / "system_prompt.txt"
NO_CONTEXT = "No driving data is available."
CHARS_PER_SPOKEN_MIN = 900  # about 15 characters per second of speech

# context_fn(device_id, convo_turns) -> text for the DRIVING CONTEXT block
ContextFn = Callable[[str, list], str]


def default_context(device_id: str, turns: list) -> str:
    """Real builder from the M4 context package, or a stub line if it is not usable."""
    try:
        from app.context.builder import build_context
        from app.store import gps_db
    except ImportError:
        return NO_CONTEXT
    conn = gps_db.connect()
    try:
        return build_context(conn, device_id, int(time.time() * 1000), turns)
    finally:
        conn.close()


def _ms(t0: float) -> int:
    return int((time.monotonic() - t0) * 1000)


@dataclass
class Pipeline:
    stt: SttProvider
    llm: LlmProvider
    tts: TtsProvider
    context_fn: ContextFn = default_context
    memory: ConversationMemory = field(default_factory=ConversationMemory)
    cache: ReplyCache = field(default_factory=ReplyCache)
    _clips: Clips | None = None
    _lock: threading.Lock = field(default_factory=threading.Lock)

    @property
    def clips(self) -> Clips:
        with self._lock:
            if self._clips is None:
                self._clips = render_clips(self.tts, settings.audio_sample_rate)
            return self._clips

    def system_prompt(self, context: str) -> str:
        return f"{PROMPT_PATH.read_text()}\nDRIVING CONTEXT\n{context}\n"


class _Broken:
    """Stands in for a provider that could not be built (bad config); fails per call -> clip."""

    native_rate = 24000

    def __init__(self, name: str):
        self.name = name

    def _fail(self, *_):
        raise RuntimeError(f"{self.name} provider unavailable")

    transcribe = complete = synthesize = _fail


def _safe(name: str, factory):
    try:
        return factory()
    except Exception as e:
        log.error("[%s] provider not built: %s", name.upper(), type(e).__name__)
        return _Broken(name)


def build_pipeline() -> Pipeline:
    return Pipeline(_safe("stt", make_stt), _safe("llm", make_llm), _safe("tts", make_tts))


_pipeline: Pipeline | None = None


def get_pipeline() -> Pipeline:
    global _pipeline
    if _pipeline is None:
        _pipeline = build_pipeline()
    return _pipeline


class QueryRun:
    """One query. begin() runs everything up to the first audio so X-Outcome is known."""

    def __init__(self, pipe: Pipeline, device_id: str, pcm: bytes, t0: float, t_up: float):
        self.pipe, self.device_id, self.pcm, self.t_up = pipe, device_id, pcm, t_up
        self.t0 = t0
        self.m: dict = {"upload_ms": int((t_up - t0) * 1000)}
        self.transcript: str | None = None
        self.reply: str | None = None
        self.skip_log = False

    def begin(self) -> tuple[str, Iterator[bytes]]:
        pipe, m = self.pipe, self.m
        try:
            prepared = audio.prepare(self.pcm, settings.audio_sample_rate)
        except audio.NoSpeech:
            return self._clip("no_speech", pipe.clips.didnt_catch)
        m["speech_ms"] = prepared.speech_ms
        self.pcm = b""  # raw upload no longer needed
        try:
            t = time.monotonic()
            wav = audio.wav_bytes(prepared.pcm, settings.audio_sample_rate)
            text = pipe.stt.transcribe(wav)
            m["stt_ms"] = _ms(t)
        except Exception as e:
            log.warning("[STT] failed: %s", type(e).__name__)
            return self._clip("error", pipe.clips.cant_reach_brain)
        if not text.strip():
            return self._clip("no_speech", pipe.clips.didnt_catch)
        self.transcript = text.strip()
        if wipe.is_delete_command(self.transcript):
            return self._delete_command()

        t = time.monotonic()
        try:
            context = pipe.context_fn(self.device_id, pipe.memory.recent(self.device_id))
        except Exception as e:  # missing GPS data must not stop general questions
            log.warning("[CONTEXT] failed: %s", type(e).__name__)
            context = NO_CONTEXT
        m["context_ms"] = _ms(t)

        try:
            r = pipe.llm.complete(pipe.system_prompt(context), self.transcript)
            if not r.text.strip():
                raise RuntimeError("empty reply")
        except Exception as e:
            log.warning("[LLM] failed: %s", type(e).__name__)
            return self._clip("error", pipe.clips.cant_reach_brain)
        self.reply = r.text.strip()
        m.update(llm_first_ms=r.first_token_ms, llm_ms=r.total_ms)
        m.update(tokens_in=r.tokens_in, tokens_out=r.tokens_out)

        t_tts = time.monotonic()
        chunks = stream_pcm(pipe.tts, self.reply, settings.audio_sample_rate)
        try:
            first = next(chunks)
        except Exception as e:  # includes StopIteration: no audio at all
            log.warning("[TTS] failed: %s", type(e).__name__)
            return self._clip("error", pipe.clips.cant_reach_brain)
        m["tts_first_ms"] = _ms(t_tts)
        return "ok", self._speak(first, chunks, t_tts)

    def _delete_command(self) -> tuple[str, Iterator[bytes]]:
        """U14, matched on the transcript (never the LLM). The wipe leaves no trace of itself."""
        pipe = self.pipe
        self.transcript = None  # the command itself is not logged
        if not wipe.guard.confirmed(self.device_id, "voice"):
            return self._clip("ok", pipe.clips.delete_confirm)
        wipe.wipe_device(self.device_id)
        pipe.memory.clear(self.device_id)
        pipe.cache.clear(self.device_id)
        self.skip_log = True
        return self._clip("ok", pipe.clips.data_deleted)

    def _clip(self, outcome: str, pcm: bytes) -> tuple[str, Iterator[bytes]]:
        return outcome, self._play(outcome, pcm)

    def _play(self, outcome: str, pcm: bytes) -> Iterator[bytes]:
        try:
            self.m["first_audio_sent_ms"] = _ms(self.t_up)
            yield pcm
        finally:
            self._finish(outcome)

    def _speak(self, first: bytes, rest: Iterator[bytes], t_tts: float) -> Iterator[bytes]:
        outcome, sent = "error", [first]
        try:
            self.m["first_audio_sent_ms"] = _ms(self.t_up)
            yield first
            for chunk in rest:
                sent.append(chunk)
                yield chunk
            outcome = "ok"
        except Exception as e:  # provider died mid-stream: audio so far is all we can send
            log.warning("[TTS] stream failed: %s", type(e).__name__)
        finally:
            self.m["tts_ms"] = _ms(t_tts)
            if outcome == "ok":
                self.pipe.cache.put(self.device_id, b"".join(sent))
                self.pipe.memory.add(self.device_id, self.transcript or "", self.reply or "")
            self._finish(outcome)

    def _finish(self, outcome: str) -> None:
        m = self.m
        m["total_ms"] = _ms(self.t0)
        mins = (m.get("speech_ms") or 0) / 60000
        m["cost_usd"] = round(
            mins * settings.cost_stt_per_min
            + (m.get("tokens_in") or 0) * settings.cost_llm_in_per_mtok / 1e6
            + (m.get("tokens_out") or 0) * settings.cost_llm_out_per_mtok / 1e6
            + len(self.reply or "") / CHARS_PER_SPOKEN_MIN * settings.cost_tts_per_min,
            6,
        )
        if self.skip_log:
            return
        try:
            voice_db.record_query(self.device_id, outcome, m, self.transcript, self.reply)
        except Exception as e:
            log.error("[LOG] could not record query: %s", type(e).__name__)
