"""Device-token auth. Token format: "<device id>.<secret>"; only a salted hash is stored."""

import hashlib
import hmac
import secrets
import sqlite3
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Annotated

from fastapi import Depends, HTTPException, Request
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

from app.store.db import connect

SALT_BYTES = 16
SECRET_BYTES = 32  # 256 bits: random enough that one salted SHA-256 is a sound at-rest hash

_bearer = HTTPBearer(auto_error=False)


@dataclass(frozen=True)
class Device:
    id: str
    name: str


def _now() -> str:
    return datetime.now(UTC).isoformat(timespec="seconds")


def hash_secret(salt: bytes, secret: str) -> bytes:
    return hashlib.sha256(salt + secret.encode()).digest()


def create_device(conn: sqlite3.Connection, name: str) -> str:
    """Registers a device and returns its token. The token is never stored."""
    device_id = secrets.token_hex(4)
    secret = secrets.token_urlsafe(SECRET_BYTES)
    salt = secrets.token_bytes(SALT_BYTES)
    conn.execute(
        "INSERT INTO devices (id, name, salt, token_hash, created_at) VALUES (?, ?, ?, ?, ?)",
        (device_id, name, salt, hash_secret(salt, secret), _now()),
    )
    conn.commit()
    return f"{device_id}.{secret}"


def revoke_device(conn: sqlite3.Connection, name: str) -> bool:
    cur = conn.execute(
        "UPDATE devices SET revoked_at = ? WHERE name = ? AND revoked_at IS NULL", (_now(), name)
    )
    conn.commit()
    return cur.rowcount > 0


def verify_token(conn: sqlite3.Connection, token: str) -> Device | None:
    device_id, sep, secret = token.partition(".")
    if not sep or not device_id or not secret:
        return None
    row = conn.execute("SELECT * FROM devices WHERE id = ?", (device_id,)).fetchone()
    if row is None:
        return None
    # Hash even for revoked rows so timing does not reveal which ids exist or are revoked.
    ok = hmac.compare_digest(hash_secret(row["salt"], secret), row["token_hash"])
    if not ok or row["revoked_at"] is not None:
        return None
    conn.execute("UPDATE devices SET last_seen_at = ? WHERE id = ?", (_now(), device_id))
    conn.commit()
    return Device(id=row["id"], name=row["name"])


def require_device(
    request: Request,
    creds: Annotated[HTTPAuthorizationCredentials | None, Depends(_bearer)],
) -> Device:
    device = None
    if creds is not None and creds.scheme.lower() == "bearer":
        conn = connect()
        try:
            device = verify_token(conn, creds.credentials)
        finally:
            conn.close()
    if device is None:
        raise HTTPException(401, "invalid or missing device token", {"WWW-Authenticate": "Bearer"})
    request.state.device = device
    return device


CurrentDevice = Annotated[Device, Depends(require_device)]
