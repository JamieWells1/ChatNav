import numpy as np
import pytest
from voice_helpers import RATE, silence_pcm, speech_pcm

from app.pipeline import audio
from app.pipeline.tts import StubTts, stream_pcm


def test_prepare_trims_removes_dc_and_normalises():
    raw = speech_pcm()
    out = audio.prepare(raw, RATE)
    x = audio.to_samples(out.pcm)
    assert len(x) < len(audio.to_samples(raw)) * 0.8  # silence trimmed
    assert abs(float(x.mean())) < 200
    assert 0.8 * 32767 < np.abs(x).max() <= 32767
    assert out.speech_ms >= 600


@pytest.mark.parametrize("pcm", [silence_pcm(), b"", b"\x01", speech_pcm(speech_s=0.1)])
def test_prepare_rejects_no_speech(pcm):
    with pytest.raises(audio.NoSpeech):
        audio.prepare(pcm, RATE)


def test_resample_length_and_chunk_independence():
    src = audio.to_pcm(8000 * np.sin(2 * np.pi * 440 * np.arange(24000) / 24000))
    whole = audio.resample(src, 24000, 16000)
    assert abs(len(whole) // 2 - 16000) <= 40
    rs = audio.StreamResampler(24000, 16000)
    odd = b"".join(rs.feed(src[i : i + 999]) for i in range(0, len(src), 999))  # odd chunk size
    assert odd == whole


def test_stub_tts_streams_at_output_rate():
    chunks = list(stream_pcm(StubTts(), "hello there driver", 16000))
    assert len(chunks) > 1
    assert 16000 * 0.4 < sum(len(c) for c in chunks) / 2 < 16000 * 1.2
