"""Speech to text behind a small interface. The real provider is OpenAI; the stub needs no key."""

from typing import Protocol

from app.config import settings

STT_LANGUAGE = "en"  # the API wants ISO-639-1; the plan's en-GB is not accepted


class SttProvider(Protocol):
    def transcribe(self, wav: bytes) -> str: ...


class StubStt:
    def __init__(self, text: str = "how fast am I going", fail: bool = False):
        self.text, self.fail = text, fail

    def transcribe(self, wav: bytes) -> str:
        if self.fail:
            raise RuntimeError("stub stt failure")
        return self.text


class OpenAiStt:
    def __init__(self, api_key: str, model: str, timeout_s: float):
        from openai import OpenAI

        self._client = OpenAI(api_key=api_key, timeout=timeout_s, max_retries=0)
        self._model = model

    def transcribe(self, wav: bytes) -> str:
        r = self._client.audio.transcriptions.create(
            model=self._model,
            file=("speech.wav", wav, "audio/wav"),  # in-memory upload
            language=STT_LANGUAGE,
            response_format="json",
        )
        return r.text.strip()


def make_stt() -> SttProvider:
    if settings.stt_provider == "stub":
        return StubStt()
    return OpenAiStt(settings.stt_api_key, settings.stt_model, settings.provider_timeout_s)
