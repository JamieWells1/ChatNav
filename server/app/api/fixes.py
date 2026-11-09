import time

from fastapi import APIRouter, HTTPException, Request

from app.auth import CurrentDevice
from app.gps import wire
from app.gps.ingest import ingest_batch
from app.store.gps_db import connect

router = APIRouter(prefix="/v1")


@router.post("/fixes")
async def post_fixes(request: Request, device: CurrentDevice) -> dict[str, int]:
    try:
        batch = wire.parse_batch(await request.body())
    except wire.BatchError as e:
        raise HTTPException(400, str(e)) from e
    conn = connect()
    try:
        ack = ingest_batch(conn, device.id, batch, int(time.time() * 1000))
    finally:
        conn.close()
    return {"ack": ack}
