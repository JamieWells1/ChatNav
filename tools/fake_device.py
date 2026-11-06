"""STUB (M0): fake ChatNav device. Only checks /v1/health for now.

Later milestones send a WAV and GPS fixes exactly like the Pico.
"""

import argparse
import sys
import urllib.request


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--server", default="http://127.0.0.1:8000")
    args = p.parse_args()
    with urllib.request.urlopen(f"{args.server}/v1/health", timeout=5) as r:
        body = r.read().decode()
    print(f"[FAKE] health: {body}")
    return 0 if body == "ok" else 1


if __name__ == "__main__":
    sys.exit(main())
