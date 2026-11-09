"""Admin tool: python -m app.cli device create|revoke|list, data delete <name> [--yes], purge"""

import argparse
import sys

from app.auth import create_device, revoke_device
from app.store import retention, wipe
from app.store.db import connect


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m app.cli", description=__doc__)
    sub = p.add_subparsers(dest="group", required=True)
    dev = sub.add_parser("device").add_subparsers(dest="action", required=True)
    dev.add_parser("create").add_argument("name")
    dev.add_parser("revoke").add_argument("name")
    dev.add_parser("list")
    data = sub.add_parser("data").add_subparsers(dest="action", required=True)
    d = data.add_parser("delete", help="delete-all (U14) for one device")
    d.add_argument("name")
    d.add_argument("--yes", action="store_true", help="really delete; without it only counts rows")
    sub.add_parser("purge").set_defaults(action="purge")
    args = p.parse_args(argv)
    if args.group == "purge":
        for tier, n in retention.purge_once().items():
            print(f"{tier:<24} {n}")
        return 0

    conn = connect()
    try:
        if args.group == "data":
            return _delete_data(conn, args.name, args.yes)
        if args.action == "create":
            token = create_device(conn, args.name)
            print(f"Device '{args.name}' created. Token (shown once, store it now):")
            print(token)
        elif args.action == "revoke":
            if not revoke_device(conn, args.name):
                print(f"No active device named '{args.name}'", file=sys.stderr)
                return 1
            print(f"Device '{args.name}' revoked")
        else:
            for r in conn.execute("SELECT * FROM devices ORDER BY created_at"):
                state = "revoked" if r["revoked_at"] else "active"
                print(f"{r['id']}  {r['name']:<20} {state:<8} last seen {r['last_seen_at'] or '-'}")
    except Exception as e:  # sqlite3.IntegrityError on a duplicate name, mostly
        print(f"error: {e}", file=sys.stderr)
        return 1
    finally:
        conn.close()
    return 0


def _delete_data(conn, name: str, yes: bool) -> int:
    row = conn.execute("SELECT id FROM devices WHERE name = ?", (name,)).fetchone()
    if row is None:
        print(f"No device named '{name}'", file=sys.stderr)
        return 1
    if not yes:
        print(f"Rows held for '{name}' (nothing deleted; add --yes to delete all):")
        for t, n in wipe.counts(conn, row["id"]).items():
            print(f"  {t:<16} {n}")
        return 0
    result = wipe.wipe_device(row["id"])
    print(f"{'table':<16} {'before':>8} {'after':>8}")
    for t, n in result["before"].items():
        print(f"{t:<16} {n:>8} {result['after'][t]:>8}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
