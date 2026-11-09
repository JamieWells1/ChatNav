import sim_trip
from gps_helpers import (
    DAY,
    DEVICE,
    T0,
    end_ms,
    ingest,
    record_time_ms,
    signature,
    sim,
    trip_rows,
)

from app.config import settings
from app.gps.derive import close_stale


def track(start=T0):
    return sim_trip.Track(start)


def drive_then_stop(tr, stop_s):
    tr.hold(20)
    tr.change_speed(14, 1.5)
    tr.cruise(60)
    tr.change_speed(0, 2.0)
    tr.hold(stop_s)


def test_trip_starts_backdated_to_first_moving_fix_and_ends_idle(gps_conn):
    tr = track()
    drive_then_stop(tr, 400)
    ingest(gps_conn, tr.records)
    (trip,) = trip_rows(gps_conn)
    assert trip["start_ms"] == (T0 + 20) * 1000
    assert trip["status"] == "closed" and trip["close_reason"] == "idle"
    assert trip["distance_m"] > 900
    last_moving = max(record_time_ms(r) for r in tr.records if r[5] > 0)
    assert trip["end_ms"] == last_moving


def test_short_stop_stays_in_trip(gps_conn):
    tr = track()
    tr.hold(10)
    tr.change_speed(14, 1.5)
    tr.cruise(30)
    tr.change_speed(0, 2.0)
    tr.hold(settings.trip_end_idle_s - 30)  # light, well under the idle limit
    tr.change_speed(14, 1.5)
    tr.cruise(30)
    ingest(gps_conn, tr.records)
    trips = trip_rows(gps_conn)
    assert len(trips) == 1 and trips[0]["status"] == "open"


def test_long_stop_splits_into_two_trips(gps_conn):
    tr = track()
    drive_then_stop(tr, settings.trip_end_idle_s + 60)
    tr.change_speed(14, 1.5)
    tr.cruise(60)
    tr.change_speed(0, 2.0)
    tr.hold(10)
    ingest(gps_conn, tr.records)
    trips = trip_rows(gps_conn)
    assert [t["status"] for t in trips] == ["closed", "open"]
    assert trips[0]["close_reason"] == "idle"


def test_brief_motion_below_hold_time_is_not_a_trip(gps_conn):
    tr = track()
    tr.hold(10)
    tr.change_speed(5, 2.5)
    tr.cruise(4)
    tr.change_speed(0, 2.5)
    tr.hold(400)
    ingest(gps_conn, tr.records)
    assert trip_rows(gps_conn) == []


def test_trip_under_min_distance_is_discarded_on_close(gps_conn):
    tr = track()
    tr.hold(5)
    tr.change_speed(3.5, 3.5)
    tr.cruise(14)  # about 50 m
    tr.change_speed(0, 3.5)
    tr.hold(400)
    ingest(gps_conn, tr.records)
    assert trip_rows(gps_conn) == []


def test_gap_within_trip_continues_and_is_not_interpolated(gps_conn):
    tr = track()
    tr.hold(5)
    tr.change_speed(14, 1.5)
    tr.cruise(30)
    tr.gap(120)  # tunnel
    tr.cruise(30)
    ingest(gps_conn, tr.records)
    (trip,) = trip_rows(gps_conn)
    assert trip["status"] == "open"
    assert trip["distance_m"] < 14 * 70  # the 120 s of tunnel would add 1680 m


def test_f13_power_loss_closes_trip_by_gap_rule(gps_conn):
    tr = track()
    tr.hold(5)
    tr.change_speed(14, 1.5)
    tr.cruise(120)
    ingest(gps_conn, tr.records)
    last = end_ms(tr.records)
    assert not close_stale(gps_conn, DEVICE, last + (settings.trip_end_gap_s - 1) * 1000)
    assert trip_rows(gps_conn)[0]["status"] == "open"
    assert close_stale(gps_conn, DEVICE, last + settings.trip_end_gap_s * 1000)
    (trip,) = trip_rows(gps_conn)
    assert trip["status"] == "closed" and trip["close_reason"] == "gap"
    assert trip["end_ms"] == last


def test_gap_longer_than_rule_between_fixes_splits_trips(gps_conn):
    tr = track()
    tr.hold(5)
    tr.change_speed(14, 1.5)
    tr.cruise(60)
    tr.gap(settings.trip_end_gap_s + 60)
    tr.cruise(60)
    tr.change_speed(0, 2.0)
    tr.hold(5)
    ingest(gps_conn, tr.records)
    trips = trip_rows(gps_conn)
    assert [t["close_reason"] for t in trips] == ["gap", None]


def test_f11_no_fix_records_are_stored_but_never_make_a_trip(gps_conn):
    tr = track()
    tr.no_fix(120)
    tr.no_fix(30, time_valid=False)
    ingest(gps_conn, tr.records)
    assert gps_conn.execute("SELECT COUNT(*) FROM fixes WHERE valid = 0").fetchone()[0] == 150
    assert trip_rows(gps_conn) == []


def test_f12_late_batches_give_same_trips_as_live_upload(gps_conn, tmp_path, monkeypatch):
    records = sim("harsh")
    ingest(gps_conn, records)
    live = signature(gps_conn)

    from app.store.gps_db import connect

    monkeypatch.setattr(settings, "db_path", str(tmp_path / "late.db"))
    late = connect()
    ingest(late, records, order="reverse")
    assert signature(late) == live
    late.close()

    monkeypatch.setattr(settings, "db_path", str(tmp_path / "bulk.db"))
    bulk = connect()
    ingest(bulk, records, per_batch=len(records))  # whole outage in one upload
    assert signature(bulk) == live
    bulk.close()


def test_f12_wifi_outage_mid_trip_backfills(gps_conn):
    records = sim("commute")
    ingest(gps_conn, records[:100])
    ingest(gps_conn, records[200:], per_batch=50)  # first upload after reconnect
    ingest(gps_conn, records[100:200], per_batch=50)  # buffered middle arrives late
    assert signature(gps_conn) == _reference(records)


def _reference(records):
    import sqlite3

    # replay into a scratch in-memory db sharing the schema
    from app.store.gps_schema import GPS_SCHEMA

    scratch = sqlite3.connect(":memory:")
    scratch.row_factory = sqlite3.Row
    scratch.executescript(GPS_SCHEMA)
    ingest(scratch, records)
    return signature(scratch)


def test_f14_parked_with_drift_makes_no_phantom_trip(gps_conn):
    ingest(gps_conn, sim("parked"))
    assert trip_rows(gps_conn) == []
    assert gps_conn.execute("SELECT COUNT(*) FROM fixes").fetchone()[0] == 1800


def test_two_days_make_two_trips(gps_conn):
    ingest(gps_conn, sim("commute"))
    ingest(gps_conn, sim("commute", start=T0 + DAY))
    assert len(trip_rows(gps_conn)) == 2
