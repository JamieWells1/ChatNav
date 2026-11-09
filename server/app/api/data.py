"""DELETE /v1/data: delete-all (U14). The first call arms, an identical one within DELETE_CONFIRM_S
wipes. Replies carry row counts only."""

from fastapi import APIRouter
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import JSONResponse

from app.auth import CurrentDevice
from app.config import settings
from app.pipeline.service import get_pipeline
from app.store import wipe

router = APIRouter(prefix="/v1")


def _wipe(device_id: str) -> dict:
    result = wipe.wipe_device(device_id)
    pipe = get_pipeline()
    pipe.memory.clear(device_id)
    pipe.cache.clear(device_id)
    return result


@router.delete("/data")
async def delete_data(device: CurrentDevice) -> JSONResponse:
    if not wipe.guard.confirmed(device.id, "http"):
        return JSONResponse(
            {"status": "confirm", "repeat_within_s": settings.delete_confirm_s}, status_code=202
        )
    result = await run_in_threadpool(_wipe, device.id)
    return JSONResponse({"status": "deleted", **result})
