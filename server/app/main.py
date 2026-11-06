from fastapi import FastAPI
from fastapi.responses import PlainTextResponse

app = FastAPI(title="ChatNav")


@app.get("/v1/health", response_class=PlainTextResponse)
def health() -> str:
    return "ok"
