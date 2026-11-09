from fastapi import FastAPI
from fastapi.responses import PlainTextResponse

from app.auth import CurrentDevice

app = FastAPI(title="ChatNav")


@app.get("/v1/health", response_class=PlainTextResponse)
def health() -> str:
    return "ok"


@app.get("/v1/whoami", response_class=PlainTextResponse)
def whoami(device: CurrentDevice) -> str:
    return device.name
