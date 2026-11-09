import struct

import pytest
import sim_trip
from fastapi.testclient import TestClient
from gps_helpers import DEVICE, sim, trip_rows

from app.auth import create_device
from app.gps import wire
from app.main import app
from app.store.gps_db import connect


def test_record_is_28_bytes_little_endian_in_firmware_order():
    fix = wire.Fix(1000, 1_700_000_000, 250, 515000000, -1200000, 1400, 450, 35, 9, 1, 8, 15)
    raw = wire.RECORD.pack(*fix)
    assert len(raw) == 28
    assert raw[:4] == struct.pack("<I", 1000)
    assert raw[8:10] == struct.pack("<H", 250)
    assert raw[10:14] == struct.pack("<i", 515000000)
    assert wire.RECORD.unpack(raw) == tuple(fix)


def test_batch_roundtrip_and_header():
    fixes = [wire.Fix(*r) for r in sim("commute")[:7]]
    body = wire.pack_batch(42, 3, 5, fixes)
    assert len(body) == 10 + 7 * 28
    b = wire.parse_batch(body)
    assert (b.seq, b.fw, b.drops) == (42, 3, 5)
    assert b.fixes == fixes


@pytest.mark.parametrize("mutate", [lambda b: b[:-1], lambda b: b + b"x", lambda b: b[:5]])
def test_malformed_batches_rejected(mutate):
    body = wire.pack_batch(1, 1, 0, [wire.Fix(*r) for r in sim("commute")[:3]])
    with pytest.raises(wire.BatchError):
        wire.parse_batch(mutate(body))


@pytest.fixture
def client_token(tmp_path, monkeypatch):
    from app.config import settings

    monkeypatch.setattr(settings, "db_path", str(tmp_path / "api.db"))
    conn = connect()
    token = create_device(conn, "truck")
    device_id = token.split(".")[0]
    conn.close()
    return TestClient(app), {"Authorization": f"Bearer {token}"}, device_id


def test_fixes_endpoint_acks_and_is_idempotent(client_token):
    client, headers, device_id = client_token
    records = sim("commute")
    bodies = list(sim_trip.batches(records, 5))
    for seq, body in bodies:
        r = client.post("/v1/fixes", content=body, headers=headers)
        assert r.status_code == 200 and r.json() == {"ack": seq}
    conn = connect()
    n_fixes = conn.execute("SELECT COUNT(*) FROM fixes").fetchone()[0]
    first = [tuple(t) for t in trip_rows(conn)]
    assert n_fixes == len(records) and len(first) == 1
    conn.close()

    for _, body in bodies:  # device retries everything: same acks, no new rows, same trips
        assert client.post("/v1/fixes", content=body, headers=headers).status_code == 200
    conn = connect()
    assert conn.execute("SELECT COUNT(*) FROM fixes").fetchone()[0] == n_fixes
    assert [tuple(t) for t in trip_rows(conn)][0][1:] == first[0][1:]
    assert conn.execute("SELECT device_id FROM fixes LIMIT 1").fetchone()[0] == device_id
    conn.close()


def test_fixes_endpoint_requires_token_and_valid_body(client_token):
    client, headers, _ = client_token
    assert client.post("/v1/fixes", content=b"\x00" * 10).status_code == 401
    assert client.post("/v1/fixes", content=b"junk", headers=headers).status_code == 400


def test_dedupe_is_per_device(gps_conn):
    from gps_helpers import ingest

    records = sim("commute")
    ingest(gps_conn, records, device="a")
    ingest(gps_conn, records, device="b")
    assert gps_conn.execute("SELECT COUNT(*) FROM fixes").fetchone()[0] == 2 * len(records)
    assert DEVICE not in {r[0] for r in gps_conn.execute("SELECT device_id FROM fixes")}
