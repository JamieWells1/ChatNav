from fastapi import FastAPI
from fastapi.responses import PlainTextResponse

from app.api.data import router as data_router
from app.api.fixes import router as fixes_router
from app.api.query import router as query_router
from app.api.query import voice_lifespan
from app.auth import CurrentDevice

app = FastAPI(title="ChatNav", lifespan=voice_lifespan)
app.include_router(fixes_router)
app.include_router(query_router)
app.include_router(data_router)


@app.get("/v1/health", response_class=PlainTextResponse)
def health() -> str:
    return "ok"


@app.get("/v1/whoami", response_class=PlainTextResponse)
def whoami(device: CurrentDevice) -> str:
    return device.name
