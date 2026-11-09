"""Admin tool: python -m app.cli device create|revoke|list"""

import argparse
import sys

from app.auth import create_device, revoke_device
from app.store.db import connect


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m app.cli", description=__doc__)
    sub = p.add_subparsers(dest="group", required=True)
    dev = sub.add_parser("device").add_subparsers(dest="action", required=True)
    dev.add_parser("create").add_argument("name")
    dev.add_parser("revoke").add_argument("name")
    dev.add_parser("list")
    args = p.parse_args(argv)

    conn = connect()
    try:
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


if __name__ == "__main__":
    sys.exit(main())
