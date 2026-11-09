"""Exports one trip's raw fixes to CSV (before the raw purge, for calibration).

    python tools/export_trip.py --db server/chatnav.db --list
    python tools/export_trip.py --db server/chatnav.db --trip 3 --out trip3.csv

The CSV holds real locations: keep it off the server's repo and out of git.
"""

import argparse
import csv
import sqlite3
import sys

COLUMNS = ["t_ms", "device_ms", "lat", "lon", "speed_ms", "course_deg", "alt_m", "sats",
           "quality", "hdop", "valid", "outlier"]
QUERY = (
    "SELECT t_ms, device_ms, lat_e7 / 1e7, lon_e7 / 1e7, speed_cmps / 100.0, course_deg10 / 10.0, "
    "alt_dm / 10.0, sats, quality, hdop10 / 10.0, valid, outlier FROM fixes "
    "WHERE device_id = ? AND t_ms BETWEEN ? AND ? ORDER BY t_ms"
)


def export(conn: sqlite3.Connection, trip_id: int, out) -> int:
    trip = conn.execute(
        "SELECT device_id, start_ms, last_fix_ms FROM trips WHERE id = ?", (trip_id,)
    ).fetchone()
    if trip is None:
        raise SystemExit(f"no trip {trip_id}")
    rows = conn.execute(QUERY, trip).fetchall()
    w = csv.writer(out)
    w.writerow(COLUMNS)
    w.writerows(rows)
    return len(rows)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("--db", required=True)
    p.add_argument("--trip", type=int)
    p.add_argument("--out", help="CSV path (default stdout)")
    p.add_argument("--list", action="store_true")
    args = p.parse_args()

    conn = sqlite3.connect(f"file:{args.db}?mode=ro", uri=True)
    if args.list or args.trip is None:
        for r in conn.execute(
            "SELECT id, device_id, datetime(start_ms / 1000, 'unixepoch'), status, "
            "ROUND(distance_m) FROM trips ORDER BY start_ms"
        ):
            print("trip %d device %s start %s UTC %s %s m" % r)
        return 0
    if args.out:
        with open(args.out, "w", newline="") as f:
            n = export(conn, args.trip, f)
    else:
        n = export(conn, args.trip, sys.stdout)
    print(f"[EXPORT] {n} fixes", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
