import sim_trip
from gps_helpers import DAY, DEVICE, T0, events, ingest, sim, trip_rows

from app.gps import kinematics as k
from app.gps.rollups import day_rollup, week_rollup


def run(conn, build, start=T0):
    tr = sim_trip.Track(start)
    tr.hold(10)
    tr.change_speed(20, 1.5)
    tr.cruise(30)
    build(tr)
    tr.cruise(30)
    tr.change_speed(0, 1.5)
    tr.hold(5)
    ingest(conn, tr.records)


def test_scripted_harsh_brake_and_accel_detected_once_each(gps_conn):
    ingest(gps_conn, sim("harsh"))
    (brake,) = events(gps_conn, k.BRAKE)
    (accel,) = events(gps_conn, k.ACCEL)
    assert -5.1 <= brake["peak"] <= -4.5
    assert brake["v_from"] > 19 and brake["v_to"] < 14
    assert 3.5 <= accel["peak"] <= 4.1
    assert brake["t_start_ms"] < accel["t_start_ms"]
    assert events(gps_conn, k.OVER) == []
    trip = trip_rows(gps_conn)[0]
    assert (trip["brakes"], trip["accels"]) == (1, 1)


def test_event_location_is_rounded_to_three_places(gps_conn):
    ingest(gps_conn, sim("harsh"))
    e = events(gps_conn, k.BRAKE)[0]
    assert round(e["lat"], 3) == e["lat"] and round(e["lon"], 3) == e["lon"]


def test_gentle_commute_has_no_events(gps_conn):
    ingest(gps_conn, sim("commute"))
    assert events(gps_conn) == []


def test_near_miss_below_threshold_is_not_an_event(gps_conn):
    run(gps_conn, lambda tr: tr.change_speed(10, 2.8))
    assert events(gps_conn) == []


def test_one_second_blip_is_not_an_event(gps_conn):
    run(gps_conn, lambda tr: (tr.change_speed(15, 5.0), tr.cruise(1)))
    # 20 -> 15 m/s at 5 m/s2 is exactly 1 s; smoothing over 3 intervals spreads it out
    assert events(gps_conn, k.BRAKE) == []


def test_slow_speed_braking_is_ignored(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    tr.change_speed(6, 1.5)  # 21.6 km/h, below the 25 km/h floor
    tr.cruise(20)
    tr.change_speed(0, 5.0)
    tr.hold(5)
    ingest(gps_conn, tr.records)
    assert events(gps_conn, k.BRAKE) == []


def test_over_70_is_one_event_per_spell_with_peak(gps_conn):
    ingest(gps_conn, sim("motorway"))
    (over,) = events(gps_conn, k.OVER)
    assert 31.8 <= over["peak"] <= 32.0
    assert over["t_end_ms"] - over["t_start_ms"] >= 5000
    trip = trip_rows(gps_conn)[0]
    assert trip["overs"] == 1 and trip["over_s"] >= 40


def test_short_spell_over_70_is_not_an_event(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    tr.change_speed(31.0, 1.5)
    tr.cruise(20)
    tr.change_speed(31.6, 1.0)
    tr.cruise(3)
    tr.change_speed(25, 1.5)
    tr.cruise(20)
    tr.change_speed(0, 1.5)
    tr.hold(5)
    ingest(gps_conn, tr.records)
    assert events(gps_conn, k.OVER) == []


def test_two_spells_are_two_events(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    for _ in range(2):
        tr.change_speed(32, 1.5)
        tr.cruise(15)
        tr.change_speed(25, 1.5)
        tr.cruise(15)
    tr.change_speed(0, 1.5)
    tr.hold(5)
    ingest(gps_conn, tr.records)
    assert len(events(gps_conn, k.OVER)) == 2


def test_minute_buckets_sum_to_trip(gps_conn):
    ingest(gps_conn, sim("harsh"))
    trip = trip_rows(gps_conn)[0]
    rows = gps_conn.execute(
        "SELECT SUM(distance_m), SUM(brakes), SUM(accels), MIN(min_accel), COUNT(*) "
        "FROM minute_buckets WHERE trip_id = ?",
        (trip["id"],),
    ).fetchone()
    assert abs(rows[0] - trip["distance_m"]) < 1
    assert (rows[1], rows[2]) == (1, 1)
    assert rows[3] < -3.0 and rows[4] >= 2


def test_events_not_duplicated_by_incremental_ingest(gps_conn):
    records = sim("harsh")
    ingest(gps_conn, records, per_batch=3)
    ingest(gps_conn, records, per_batch=7)
    assert len(events(gps_conn, k.BRAKE)) == 1


def test_trip_summary_fields(gps_conn):
    ingest(gps_conn, sim("harsh"))
    t = trip_rows(gps_conn)[0]
    assert t["moving_s"] > 100 and t["idle_s"] >= 5
    assert 19.5 < t["max_speed_ms"] <= 20.5
    assert t["start_cell"] and t["end_cell"]
    assert t["status"] == "open"  # stopped 5 s only


def test_day_and_week_rollups(gps_conn):
    now = T0 * 1000 + 3600_000
    ingest(gps_conn, sim("harsh"))  # today
    ingest(gps_conn, sim("commute", start=T0 - DAY))  # yesterday
    ingest(gps_conn, sim("harsh", start=T0 - 8 * DAY))  # last week
    ingest(gps_conn, sim("motorway", start=T0 - 40 * DAY))  # outside four-week span
    today = day_rollup(gps_conn, DEVICE, now)
    assert today.trips == 1 and today.brakes == 1 and today.accels == 1
    assert day_rollup(gps_conn, DEVICE, now, 1).trips == 1
    this_week = week_rollup(gps_conn, DEVICE, now, 0)
    last_week = week_rollup(gps_conn, DEVICE, now, 1)
    assert this_week.trips == 2 and last_week.trips == 1
    assert last_week.per_100_miles(last_week.brakes) > 0
    four = week_rollup(gps_conn, DEVICE, now, 1, 4)
    assert four.trips == 1
    assert week_rollup(gps_conn, DEVICE, now, 0, 7).trips == 4
