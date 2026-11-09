"""Pre-rendered spoken error clips, made with the TTS provider; synthetic tones as a fallback."""

import logging
import math
from dataclasses import dataclass

import numpy as np

from app.pipeline.audio import to_pcm
from app.pipeline.tts import TtsProvider, stream_pcm

log = logging.getLogger("chatnav.clips")

TEXTS = {
    "didnt_catch": "Sorry, I didn't catch that.",
    "cant_reach_brain": "Sorry, I can't reach my brain right now. Try again in a moment.",
    "delete_confirm": (
        "This will delete all your driving history. Ask me again within thirty seconds to confirm."
    ),
    "data_deleted": "Done. All your driving data has been deleted.",
}


@dataclass(frozen=True)
class Clips:
    didnt_catch: bytes
    cant_reach_brain: bytes
    data_deleted: bytes
    delete_confirm: bytes


def _tone(rate: int, freqs: list[float]) -> bytes:
    """Always-playable fallback: one short beep per frequency."""
    parts = []
    for f in freqs:
        t = np.arange(int(rate * 0.2)) / rate
        env = np.minimum(1.0, np.minimum(t, 0.2 - t) / 0.02)
        parts.append(to_pcm(8000 * env * np.sin(2 * math.pi * f * t)))
        parts.append(bytes(int(rate * 0.05) * 2))
    return b"".join(parts)


FALLBACK_TONES = {
    "didnt_catch": [440.0, 330.0],
    "cant_reach_brain": [330.0, 330.0, 220.0],
    "data_deleted": [440.0, 550.0],
    "delete_confirm": [440.0, 440.0],
}


def render_clips(tts: TtsProvider, rate: int) -> Clips:
    out = {}
    for name, text in TEXTS.items():
        try:
            pcm = b"".join(stream_pcm(tts, text, rate))
            if not pcm:
                raise RuntimeError("empty audio")
        except Exception as e:  # a failing provider must never stop the server starting
            log.warning("[CLIPS] %s: TTS failed (%s); using tones", name, type(e).__name__)
            pcm = _tone(rate, FALLBACK_TONES[name])
        out[name] = pcm
    return Clips(**out)
