"""Audio prep and resampling. All in memory; nothing here touches disk. 16-bit mono PCM."""

import io
import wave
from dataclasses import dataclass

import numpy as np

from app.config import settings

FRAME_MS = 20
PAD_BEFORE_MS = 100
PAD_AFTER_MS = 200
MIN_ACTIVE_RMS = 150.0  # about -47 dBFS; below this a frame is never speech
MIN_PEAK_RMS = 300.0  # a clip whose loudest frame is quieter than this is treated as silence
TARGET_PEAK = 0.9
MAX_GAIN = 30.0
FIR_TAPS = 31


class NoSpeech(Exception):
    """The upload held less than MIN_SPEECH_MS of speech (F5)."""


@dataclass(frozen=True)
class Prepared:
    pcm: bytes
    speech_ms: int


def to_samples(pcm: bytes) -> np.ndarray:
    return np.frombuffer(pcm[: len(pcm) // 2 * 2], dtype="<i2").astype(np.float32)


def to_pcm(x: np.ndarray) -> bytes:
    return np.clip(np.rint(x), -32768, 32767).astype("<i2").tobytes()


def prepare(pcm: bytes, rate: int) -> Prepared:
    """DC removal, silence trim, normalise. Raises NoSpeech when too little speech remains."""
    x = to_samples(pcm)
    frame = rate * FRAME_MS // 1000
    if len(x) < frame:
        raise NoSpeech
    x = x - x.mean()
    n = len(x) // frame
    rms = np.sqrt((x[: n * frame].reshape(n, frame) ** 2).mean(axis=1))
    if rms.max() < MIN_PEAK_RMS:
        raise NoSpeech
    noise = float(np.percentile(rms, 10))
    active = rms > max(MIN_ACTIVE_RMS, noise * 3, float(rms.max()) * 0.08)
    speech_ms = int(active.sum()) * FRAME_MS
    if speech_ms < settings.min_speech_ms:
        raise NoSpeech
    idx = np.flatnonzero(active)
    start = max(0, (idx[0] * FRAME_MS - PAD_BEFORE_MS) * rate // 1000)
    end = min(len(x), ((idx[-1] + 1) * FRAME_MS + PAD_AFTER_MS) * rate // 1000)
    x = x[start:end]
    peak = float(np.abs(x).max())
    if peak > 0:
        x = x * min(MAX_GAIN, TARGET_PEAK * 32767 / peak)
    return Prepared(to_pcm(x), speech_ms)


def wav_bytes(pcm: bytes, rate: int) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    return buf.getvalue()


class StreamResampler:
    """Chunk-by-chunk linear resampler with a low-pass when downsampling (24 kHz to 16 kHz)."""

    def __init__(self, in_rate: int, out_rate: int):
        self.same = in_rate == out_rate
        self.step = in_rate / out_rate
        self._odd = b""
        self._buf = np.zeros(0, dtype=np.float32)
        self._pos = 0.0
        self._fir = None
        if out_rate < in_rate:
            cut = 0.45 * out_rate / in_rate  # cycles per input sample
            k = np.arange(FIR_TAPS) - (FIR_TAPS - 1) / 2
            fir = np.sinc(2 * cut * k) * np.hamming(FIR_TAPS)
            self._fir = (fir / fir.sum()).astype(np.float32)
            self._hist = np.zeros(FIR_TAPS - 1, dtype=np.float32)

    def feed(self, data: bytes) -> bytes:
        data = self._odd + data
        self._odd = data[len(data) // 2 * 2 :]
        if self.same:
            return data[: len(data) // 2 * 2]
        x = to_samples(data)
        if self._fir is not None:
            joined = np.concatenate([self._hist, x])
            self._hist = joined[-(FIR_TAPS - 1) :]
            x = np.convolve(joined, self._fir, mode="valid")
        buf = np.concatenate([self._buf, x])
        if len(buf) < 2 or self._pos > len(buf) - 1:
            self._buf = buf
            return b""
        count = int((len(buf) - 1 - self._pos) / self.step) + 1
        at = self._pos + self.step * np.arange(count)
        out = np.interp(at, np.arange(len(buf)), buf)
        nxt = self._pos + self.step * count
        drop = int(nxt)
        self._buf, self._pos = buf[drop:], nxt - drop
        return to_pcm(out)


def resample(pcm: bytes, in_rate: int, out_rate: int) -> bytes:
    return StreamResampler(in_rate, out_rate).feed(pcm)
