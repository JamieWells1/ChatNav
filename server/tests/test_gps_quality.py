from gps_helpers import DEVICE, T0, sim

from app.gps import kinematics as k
from app.gps import quality, wire
from app.gps.ingest import ingest_batch

GOOD = wire.Fix(1000, T0, 0, 515000000, -1200000, 500, 0, 35, 9, 1, 8, 0x0F)


def test_good_fix_is_valid():
    assert quality.is_valid(GOOD)


def test_each_rule_invalidates():
    assert not quality.is_valid(GOOD._replace(flags=0x0D))  # RMC status V
    assert not quality.is_valid(GOOD._replace(quality=0))  # GGA quality 0
    assert not quality.is_valid(GOOD._replace(sats=3))
    assert not quality.is_valid(GOOD._replace(hdop10=51))
    assert quality.is_valid(GOOD._replace(hdop10=50))
    assert not quality.is_valid(GOOD._replace(flags=0x0E))  # no UTC time
    assert not quality.is_valid(GOOD._replace(utc_s=100))  # nonsense date
    assert not quality.is_valid(GOOD._replace(flags=0x07))  # no position


def test_invalid_fixes_are_stored_but_flagged(gps_conn):
    bad = GOOD._replace(sats=2)
    batch = wire.Batch(1, 1, 0, [GOOD, bad._replace(utc_s=T0 + 1)])
    ingest_batch(gps_conn, DEVICE, batch, T0 * 1000)
    rows = gps_conn.execute("SELECT valid FROM fixes ORDER BY t_ms").fetchall()
    assert [r[0] for r in rows] == [1, 0]


def test_no_time_fixes_are_stamped_from_receive_time(gps_conn):
    f = GOOD._replace(flags=0x04, utc_s=0, utc_ms=0, device_ms=9000)
    g = f._replace(device_ms=10000)
    ingest_batch(gps_conn, DEVICE, wire.Batch(2, 1, 0, [f, g]), 5_000_000)
    rows = gps_conn.execute("SELECT t_ms, valid FROM fixes ORDER BY t_ms").fetchall()
    assert [tuple(r) for r in rows] == [(4_999_000, 0), (5_000_000, 0)]


def test_drift_clamped_to_zero_and_raw_kept(gps_conn):
    assert k.clamp_speed(49) == 0.0
    assert k.clamp_speed(50) == 0.5
    f = GOOD._replace(speed_cmps=30)
    ingest_batch(gps_conn, DEVICE, wire.Batch(1, 1, 0, [f]), T0 * 1000)
    assert gps_conn.execute("SELECT speed_cmps FROM fixes").fetchone()[0] == 30


def test_speed_jump_flagged_as_outlier(gps_conn):
    records = sim("commute")
    i = 80
    records[i] = records[i][:5] + (4000,) + records[i][6:]  # 40 m/s blip in a 14 m/s cruise
    from gps_helpers import ingest

    ingest(gps_conn, records)
    flagged = gps_conn.execute("SELECT t_ms FROM fixes WHERE outlier = 1").fetchall()
    assert [r[0] for r in flagged] == [records[i][1] * 1000]
