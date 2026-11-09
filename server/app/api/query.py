"""POST /v1/query (voice in, voice out) and GET /v1/query/last (repeat)."""

import time
from contextlib import asynccontextmanager
from typing import Annotated

from fastapi import APIRouter, Depends, FastAPI, Request, Response
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import StreamingResponse

from app.auth import CurrentDevice
from app.config import settings
from app.pipeline.service import Pipeline, QueryRun, get_pipeline

router = APIRouter()
Pipe = Annotated[Pipeline, Depends(get_pipeline)]


def _media_type() -> str:
    return f"audio/L16; rate={settings.audio_sample_rate}; channels=1"


@router.post("/v1/query")
async def query(request: Request, device: CurrentDevice, pipe: Pipe) -> StreamingResponse:
    t0 = time.monotonic()
    cap = (settings.max_utterance_s + 1) * settings.audio_sample_rate * 2
    buf = bytearray()
    async for chunk in request.stream():
        if len(buf) < cap:
            buf += chunk[: cap - len(buf)]
    run = QueryRun(pipe, device.id, bytes(buf), t0, time.monotonic())
    outcome, body = await run_in_threadpool(run.begin)
    return StreamingResponse(body, media_type=_media_type(), headers={"X-Outcome": outcome})


@router.get("/v1/query/last")
def last(device: CurrentDevice, pipe: Pipe) -> Response:
    pcm = pipe.cache.get(device.id)
    if pcm is None:
        return Response(status_code=404)
    return Response(pcm, media_type=_media_type())


@asynccontextmanager
async def voice_lifespan(app: FastAPI):
    """Renders the error clips at startup so the first failure is already playable."""
    await run_in_threadpool(lambda: get_pipeline().clips)
    yield
