"""LLM behind a small interface. The real provider is Claude via the official anthropic SDK."""

import time
from dataclasses import dataclass
from typing import Protocol

from app.config import settings


@dataclass
class LlmResult:
    text: str
    first_token_ms: int
    total_ms: int
    tokens_in: int
    tokens_out: int


class LlmProvider(Protocol):
    def complete(self, system: str, user: str) -> LlmResult: ...


class StubLlm:
    def __init__(self, reply: str | None = None, fail: bool = False):
        self.reply, self.fail = reply, fail
        self.last_system = ""
        self.last_user = ""

    def complete(self, system: str, user: str) -> LlmResult:
        if self.fail:
            raise RuntimeError("stub llm failure")
        self.last_system, self.last_user = system, user
        text = self.reply or f"You asked: {user}. This is a stub answer."
        return LlmResult(text, 1, 2, len(system + user) // 4, len(text) // 4)


class AnthropicLlm:
    def __init__(self, api_key: str, model: str, max_tokens: int, timeout_s: float):
        import anthropic

        self._client = anthropic.Anthropic(api_key=api_key, timeout=timeout_s, max_retries=0)
        self._model, self._max_tokens = model, max_tokens

    def complete(self, system: str, user: str) -> LlmResult:
        t0 = time.monotonic()
        first = 0
        parts: list[str] = []
        with self._client.messages.stream(
            model=self._model,
            max_tokens=self._max_tokens,
            thinking={"type": "disabled"},
            system=system,
            messages=[{"role": "user", "content": user}],
        ) as stream:
            for piece in stream.text_stream:
                if not parts:
                    first = int((time.monotonic() - t0) * 1000)
                parts.append(piece)
            usage = stream.get_final_message().usage
        total = int((time.monotonic() - t0) * 1000)
        text = "".join(parts).strip()
        return LlmResult(text, first, total, usage.input_tokens, usage.output_tokens)


def make_llm() -> LlmProvider:
    if settings.llm_provider == "stub":
        return StubLlm()
    return AnthropicLlm(
        settings.llm_api_key,
        settings.llm_model,
        settings.llm_max_output_tokens,
        settings.provider_timeout_s,
    )
