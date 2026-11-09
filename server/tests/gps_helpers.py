from datetime import UTC, datetime

import sim_trip

from app.gps import wire
from app.gps.ingest import ingest_batch

T0 = int(datetime(2026, 11, 9, 8, 0, 0, tzinfo=UTC).timestamp())
DAY = 86400
DEVICE = "dev1"


def record_time_ms(rec: tuple) -> int:
    return rec[1] * 1000 + rec[2]


def ingest(conn, records, per_batch=5, device=DEVICE, order=None, first_seq=1):
    """Sends records as device batches. now = time of the last fix in the batch."""
    bodies = list(sim_trip.batches(records, per_batch, first_seq=first_seq))
    if order == "reverse":
        bodies.reverse()
    now = 0
    for _, body in bodies:
        batch = wire.parse_batch(body)
        times = [f.utc_s * 1000 + f.utc_ms for f in batch.fixes if f.utc_s]
        now = max(times) if times else now + 5000  # time-less batches arrive after the last one
        ingest_batch(conn, device, batch, now)


def trip_rows(conn):
    return conn.execute("SELECT * FROM trips ORDER BY start_ms").fetchall()


def events(conn, kind=None):
    q = "SELECT * FROM events" + (" WHERE kind = ?" if kind else "") + " ORDER BY t_start_ms"
    return conn.execute(q, (kind,) if kind else ()).fetchall()


def sim(scenario, start=T0, **kw):
    return sim_trip.make_trip(scenario, start, **kw)


def end_ms(records) -> int:
    return record_time_ms(records[-1])


def signature(conn):
    return [
        (t["start_ms"], t["end_ms"], t["status"], round(t["distance_m"]), t["brakes"])
        for t in trip_rows(conn)
    ]
