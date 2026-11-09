import sqlite3

import pytest
from fastapi.testclient import TestClient

from app import cli
from app.auth import create_device, revoke_device
from app.config import settings
from app.main import app
from app.store.db import connect


@pytest.fixture
def db(tmp_path, monkeypatch):
    path = str(tmp_path / "test.db")
    monkeypatch.setattr(settings, "db_path", path)
    return path


@pytest.fixture
def client(db):
    return TestClient(app)


def _token(name="truck"):
    conn = connect()
    try:
        return create_device(conn, name)
    finally:
        conn.close()


def _get(client, token=None, scheme="Bearer"):
    headers = {"Authorization": f"{scheme} {token}"} if token is not None else {}
    return client.get("/v1/whoami", headers=headers)


def test_health_needs_no_token(client):
    assert client.get("/v1/health").status_code == 200


def test_no_token_is_401(client):
    r = _get(client)
    assert r.status_code == 401
    assert r.headers["www-authenticate"] == "Bearer"


def test_valid_token_is_200(client):
    r = _get(client, _token("truck"))
    assert r.status_code == 200
    assert r.text == "truck"


def test_wrong_secret_is_401(client):
    token = _token()
    device_id = token.split(".")[0]
    assert _get(client, f"{device_id}.wrongsecret").status_code == 401


def test_unknown_id_and_malformed_tokens_are_401(client):
    _token()
    for bad in ["ffffffff.abc", "nodot", ".", "abc.", ".abc", ""]:
        assert _get(client, bad).status_code == 401, bad


def test_wrong_scheme_is_401(client):
    assert _get(client, _token(), scheme="Basic").status_code == 401


def test_revoked_token_is_401(client):
    token = _token("old")
    assert _get(client, token).status_code == 200
    conn = connect()
    assert revoke_device(conn, "old")
    conn.close()
    assert _get(client, token).status_code == 401


def test_revoke_only_affects_that_device(client):
    a, b = _token("a"), _token("b")
    conn = connect()
    revoke_device(conn, "a")
    conn.close()
    assert _get(client, a).status_code == 401
    assert _get(client, b).status_code == 200


def test_plaintext_token_not_in_database(db):
    token = _token()
    secret = token.split(".", 1)[1]
    raw = sqlite3.connect(db)
    dump = "\n".join(raw.iterdump()).encode()
    raw_bytes = open(db, "rb").read()
    raw.close()
    for needle in (token.encode(), secret.encode()):
        assert needle not in dump
        assert needle not in raw_bytes


def test_each_token_has_its_own_salt(db):
    _token("a")
    _token("b")
    conn = connect()
    salts = {r["salt"] for r in conn.execute("SELECT salt FROM devices")}
    conn.close()
    assert len(salts) == 2


def test_last_seen_is_updated(client, db):
    token = _token()
    conn = connect()
    assert conn.execute("SELECT last_seen_at FROM devices").fetchone()[0] is None
    conn.close()
    _get(client, token)
    conn = connect()
    assert conn.execute("SELECT last_seen_at FROM devices").fetchone()[0] is not None
    conn.close()


def test_cli_create_revoke_list(db, capsys):
    assert cli.main(["device", "create", "dash"]) == 0
    out = capsys.readouterr().out
    token = out.strip().splitlines()[-1]
    conn = connect()
    assert conn.execute("SELECT count(*) FROM devices").fetchone()[0] == 1
    conn.close()
    assert cli.main(["device", "create", "dash"]) == 1  # duplicate name
    capsys.readouterr()
    assert cli.main(["device", "list"]) == 0
    listing = capsys.readouterr().out
    assert "dash" in listing and "active" in listing
    assert token not in listing
    assert cli.main(["device", "revoke", "dash"]) == 0
    assert cli.main(["device", "revoke", "dash"]) == 1
