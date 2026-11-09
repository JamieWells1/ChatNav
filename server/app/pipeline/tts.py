"""Text to speech behind a small interface. Chunks are resampled and yielded as produced."""

import math
from collections.abc import Iterator
from typing import Protocol

import numpy as np

from app.config import settings
from app.pipeline.audio import StreamResampler, to_pcm

OPENAI_PCM_RATE = 24000  # OpenAI "pcm" output is 24 kHz, 16-bit, mono, little-endian
CHUNK_BYTES = 4800


class TtsProvider(Protocol):
    native_rate: int

    def synthesize(self, text: str) -> Iterator[bytes]: ...


class StubTts:
    """Synthetic tone whose pitch depends on the text and whose length on the word count."""

    native_rate = OPENAI_PCM_RATE

    def __init__(self, fail: bool = False):
        self.fail = fail

    def synthesize(self, text: str) -> Iterator[bytes]:
        if self.fail:
            raise RuntimeError("stub tts failure")
        secs = min(3.0, 0.2 + 0.12 * len(text.split()))
        freq = 300 + sum(text.encode()) % 500
        n = int(self.native_rate * secs)
        t = np.arange(n) / self.native_rate
        env = np.minimum(1.0, np.minimum(t, secs - t) / 0.02)
        pcm = to_pcm(8000 * env * np.sin(2 * math.pi * freq * t))
        for i in range(0, len(pcm), CHUNK_BYTES):
            yield pcm[i : i + CHUNK_BYTES]


class OpenAiTts:
    native_rate = OPENAI_PCM_RATE

    def __init__(self, api_key: str, model: str, voice: str, timeout_s: float):
        from openai import OpenAI

        self._client = OpenAI(api_key=api_key, timeout=timeout_s, max_retries=0)
        self._model, self._voice = model, voice

    def synthesize(self, text: str) -> Iterator[bytes]:
        with self._client.audio.speech.with_streaming_response.create(
            model=self._model, voice=self._voice, input=text, response_format="pcm"
        ) as r:
            yield from r.iter_bytes(CHUNK_BYTES)


def stream_pcm(provider: TtsProvider, text: str, out_rate: int) -> Iterator[bytes]:
    """Provider audio resampled to out_rate, chunk by chunk, empty chunks dropped."""
    rs = StreamResampler(provider.native_rate, out_rate)
    for chunk in provider.synthesize(text):
        out = rs.feed(chunk)
        if out:
            yield out


def make_tts() -> TtsProvider:
    if settings.tts_provider == "stub":
        return StubTts()
    return OpenAiTts(
        settings.tts_api_key, settings.tts_model, settings.tts_voice, settings.provider_timeout_s
    )
