"""RAM-only conversation memory and last-reply cache, per device. Lost on restart by design."""

import threading
import time
from collections import defaultdict, deque
from dataclasses import dataclass

from app.config import settings


@dataclass(frozen=True)
class ConvoTurn:
    """Same fields as context.builder.Turn, which only reads these attributes."""

    t_ms: int
    question: str
    answer: str


class ConversationMemory:
    def __init__(self) -> None:
        self._turns: dict[str, deque[ConvoTurn]] = defaultdict(deque)
        self._lock = threading.Lock()

    def add(self, device_id: str, question: str, answer: str) -> None:
        with self._lock:
            q = self._turns[device_id]
            q.append(ConvoTurn(int(time.time() * 1000), question, answer))
            while len(q) > settings.convo_turns:
                q.popleft()

    def recent(self, device_id: str) -> list[ConvoTurn]:
        cutoff = (time.time() - settings.convo_ttl_s) * 1000
        with self._lock:
            return [t for t in self._turns[device_id] if t.t_ms >= cutoff]


class ReplyCache:
    def __init__(self) -> None:
        self._items: dict[str, tuple[float, bytes]] = {}
        self._lock = threading.Lock()

    def put(self, device_id: str, pcm: bytes) -> None:
        with self._lock:
            self._items[device_id] = (time.monotonic(), pcm)

    def get(self, device_id: str) -> bytes | None:
        with self._lock:
            item = self._items.get(device_id)
        if item is None or time.monotonic() - item[0] > settings.reply_cache_ttl_s:
            return None
        return item[1]
