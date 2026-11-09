"""Synthetic speech-like PCM for the voice tests (no real recordings in the repo)."""

import numpy as np

RATE = 16000


def speech_pcm(
    rate: int = RATE,
    lead_s: float = 0.4,
    speech_s: float = 1.2,
    tail_s: float = 0.4,
    noise: float = 20.0,
    dc: int = 300,
) -> bytes:
    """Silence, a syllable-modulated harmonic burst, silence; with DC offset and faint noise."""
    rng = np.random.default_rng(1)
    n = lambda s: int(rate * s)  # noqa: E731
    t = np.arange(n(speech_s)) / rate
    voice = sum(np.sin(2 * np.pi * 140 * h * t) / h for h in range(1, 6))
    voice = voice * (0.5 + 0.5 * np.sin(2 * np.pi * 3.5 * t) ** 2) * 6000
    x = np.concatenate([np.zeros(n(lead_s)), voice, np.zeros(n(tail_s))])
    x = x + rng.normal(0, noise, len(x)) + dc
    return np.clip(x, -32768, 32767).astype("<i2").tobytes()


def silence_pcm(rate: int = RATE, secs: float = 1.5) -> bytes:
    return np.random.default_rng(2).normal(0, 15, int(rate * secs)).astype("<i2").tobytes()
