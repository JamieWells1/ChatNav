"""In-memory sliding-window rate limit per device (plan E2, OWASP API4). Resets on restart."""

import threading
import time
from collections import defaultdict, deque

from fastapi import HTTPException

from app.auth import CurrentDevice
from app.config import settings

WINDOW_S = 60.0


class SlidingWindow:
    def __init__(self) -> None:
        self._hits: dict[str, deque[float]] = defaultdict(deque)
        self._lock = threading.Lock()

    def allow(self, key: str, limit: int, now: float | None = None) -> float:
        """0.0 if allowed (and counted), else seconds until the oldest hit leaves the window."""
        now = time.monotonic() if now is None else now
        with self._lock:
            q = self._hits[key]
            while q and now - q[0] >= WINDOW_S:
                q.popleft()
            if len(q) >= limit:
                return WINDOW_S - (now - q[0])
            q.append(now)
            return 0.0

    def reset(self) -> None:
        with self._lock:
            self._hits.clear()


limiter = SlidingWindow()


def query_rate_limit(device: CurrentDevice):
    """Runs after auth, so bad tokens (401) never use up a real device's allowance."""
    wait = limiter.allow(device.id, settings.rate_limit_queries_per_min)
    if wait:
        raise HTTPException(429, "too many queries", {"Retry-After": str(int(wait) + 1)})
    return device
