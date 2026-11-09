"""Purge job (plan 8.5): per-tier deletes, open-trip protection, derive-then-purge, nulling."""

import pytest
from gps_helpers import DAY, DEVICE, T0, end_ms, ingest, sim

from app.config import settings
from app.store import retention
from app.store.retention import D_MS, H_MS


def count(conn, table, where="1"):
    return conn.execute(f"SELECT COUNT(*) FROM {table} WHERE {where}").fetchone()[0]


@pytest.fixture
def trip(gps_conn):
    rec = sim("harsh")
    ingest(gps_conn, rec)
    return gps_conn, end_ms(rec)


def add_query(conn, t_ms):
    conn.execute(
        "INSERT INTO query_log (device_id, t_ms, transcript, reply, outcome) "
        "VALUES (?, ?, 'how fast', 'thirty', 'ok')", (DEVICE, t_ms))
    conn.execute("INSERT INTO timings (device_id, t_ms, outcome) VALUES (?, ?, 'ok')",
                 (DEVICE, t_ms))
    conn.commit()


def test_event_locations_are_rounded_on_write(trip):
    conn, _ = trip
    rows = conn.execute("SELECT lat, lon FROM events").fetchall()
    assert rows
    for r in rows:
        assert r["lat"] == round(r["lat"], settings.event_location_dp)
        assert r["lon"] == round(r["lon"], settings.event_location_dp)


def test_nothing_is_purged_when_fresh(trip):
    conn, end = trip
    before = {t: count(conn, t) for t in ("fixes", "events", "minute_buckets", "trips")}
    out = retention.purge(conn, end + 60_000)
    assert out["raw_fixes"] == 0
    assert {t: count(conn, t) for t in before} == before


def test_raw_purged_after_retention_but_derived_rows_stay(trip):
    conn, end = trip
    n = count(conn, "fixes")
    out = retention.purge(conn, end + (settings.raw_retention_h + 1) * H_MS)
    assert out["raw_fixes"] == n and count(conn, "fixes") == 0
    assert count(conn, "trips") == 1 and count(conn, "events") > 0
    assert count(conn, "minute_buckets") > 0


def test_each_tier_goes_at_its_own_retention(trip):
    conn, end = trip
    s = settings
    add_query(conn, end)
    at = lambda days: retention.purge(conn, end + int(days * D_MS))  # noqa: E731

    out = at(s.query_text_retention_d + 1)
    assert out["query_text_nulled"] == 1
    row = conn.execute("SELECT * FROM query_log").fetchone()
    assert row["transcript"] is None and row["reply"] is None and row["outcome"] == "ok"
    assert count(conn, "timings") == 1

    n_ev = count(conn, "events")
    out = at(s.event_location_retention_d + 1)
    assert out["event_locations_nulled"] == n_ev
    assert count(conn, "events", "lat IS NULL AND lon IS NULL") == n_ev  # events themselves stay
    assert count(conn, "minute_buckets") > 0 or out["minute_buckets"] > 0

    n_mb = count(conn, "minute_buckets")
    out = at(s.minute_retention_d + 1)
    assert out["minute_buckets"] == n_mb and count(conn, "minute_buckets") == 0
    assert count(conn, "events") == n_ev and count(conn, "trips") == 1

    out = at(s.timing_retention_d + 1)
    assert out["timings"] == 1 and out["query_log"] == 1
    assert count(conn, "timings") == 0 and count(conn, "query_log") == 0

    out = at(s.event_retention_d + 1)
    assert out["events"] == n_ev and count(conn, "events") == 0
    assert count(conn, "trips") == 1

    out = at(s.trip_retention_d + 1)
    assert out["trips"] == 1 and count(conn, "trips") == 0


def test_open_trip_raw_is_never_purged(gps_conn, monkeypatch):
    rec = sim("commute")[:100]  # mid-cruise: trip is open
    ingest(gps_conn, rec)
    assert gps_conn.execute("SELECT status FROM trips").fetchone()[0] == "open"
    start = gps_conn.execute("SELECT start_ms FROM trips").fetchone()[0]
    n = count(gps_conn, "fixes", f"t_ms >= {start}")
    monkeypatch.setattr(settings, "raw_retention_h", 0)  # everything is "old", but it is open
    retention.purge(gps_conn, end_ms(rec) + 30_000)  # only the lead-in before the trip may go
    assert count(gps_conn, "fixes", f"t_ms >= {start}") == n > 0


def test_open_trip_raw_goes_after_hard_limit(gps_conn, monkeypatch):
    rec = sim("commute")[:100]
    ingest(gps_conn, rec)
    monkeypatch.setattr(settings, "raw_retention_h", 0)
    monkeypatch.setattr(settings, "raw_hard_limit_h", 0)  # nothing newer than now survives
    out = retention.purge(gps_conn, end_ms(rec) + 30_000)
    assert out["raw_fixes"] > 0 and count(gps_conn, "fixes") == 0


def test_underived_trip_past_hard_limit_is_derived_then_purged(gps_conn):
    rec = sim("commute")
    ingest(gps_conn, rec)
    for t in ("events", "minute_buckets", "trips"):  # simulate a trip that was never derived
        gps_conn.execute(f"DELETE FROM {t}")
    gps_conn.commit()
    out = retention.purge(gps_conn, end_ms(rec) + (settings.raw_hard_limit_h + 1) * H_MS)
    assert out["derived_devices"] == 1
    assert count(gps_conn, "fixes") == 0
    trips = gps_conn.execute("SELECT * FROM trips").fetchall()
    assert len(trips) == 1 and trips[0]["status"] == "closed"
    assert count(gps_conn, "minute_buckets") > 0


def test_raw_held_back_until_derived_rows_exist(gps_conn):
    rec = sim("commute")
    ingest(gps_conn, rec)
    retention.purge(gps_conn, end_ms(rec) + H_MS)  # closes the trip, deletes nothing
    assert gps_conn.execute("SELECT status FROM trips").fetchone()[0] == "closed"
    gps_conn.execute("DELETE FROM minute_buckets")  # trip row without its derived rows
    gps_conn.commit()
    n = count(gps_conn, "fixes")
    out = retention.purge(gps_conn, end_ms(rec) + (settings.raw_retention_h + 1) * H_MS)
    assert out["raw_fixes"] < n and count(gps_conn, "fixes") > 0  # trip's fixes kept
    out = retention.purge(gps_conn, end_ms(rec) + (settings.raw_hard_limit_h + 1) * H_MS)
    assert count(gps_conn, "fixes") == 0


def test_vacuum_runs_daily_only(trip):
    conn, end = trip
    assert retention.purge(conn, end)["vacuumed"] == 1
    assert retention.purge(conn, end + H_MS)["vacuumed"] == 0
    assert retention.purge(conn, end + 25 * H_MS)["vacuumed"] == 1


def test_other_devices_data_follows_same_rules(gps_conn):
    ingest(gps_conn, sim("commute"), device="a")
    ingest(gps_conn, sim("commute", start=T0 + DAY), device="b")
    out = retention.purge(gps_conn, T0 * 1000 + 2 * DAY * 1000 + 3 * H_MS)  # a old, b recent
    assert out["raw_fixes"] > 0
    left = {r[0] for r in gps_conn.execute("SELECT DISTINCT device_id FROM fixes")}
    assert left == {"b"}
