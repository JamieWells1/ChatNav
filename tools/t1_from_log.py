"""T1 on the device: button release to first audio, from a tools/serial_log.sh capture.

    python3 tools/t1_from_log.py serial_20261120_101500.log

Each log line starts HH:MM:SS.mmm. A run is "RECORDING -> WAITING on release" followed by
"WAITING -> PLAYING on first-audio". The second line marks reply bytes arriving; the speaker
starts PLAYBACK_PREBUFFER_MS later, so true release-to-sound is about that much longer.
"""

import re
import statistics
import sys

STAMP = re.compile(r"^(\d\d):(\d\d):(\d\d)\.(\d\d\d) ")
START = "RECORDING -> WAITING on release"
END = "WAITING -> PLAYING on first-audio"


def ms(line: str) -> int | None:
    m = STAMP.match(line)
    if not m:
        return None
    h, mi, s, f = (int(x) for x in m.groups())
    return ((h * 60 + mi) * 60 + s) * 1000 + f


def runs(lines) -> list[int]:
    out, t0 = [], None
    for line in lines:
        t = ms(line)
        if t is None:
            continue
        if START in line:
            t0 = t
        elif END in line and t0 is not None:
            out.append((t - t0) % 86_400_000)
            t0 = None
    return out


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    with open(sys.argv[1], errors="replace") as f:
        vals = runs(f)
    if not vals:
        print("no complete release -> first-audio pairs found", file=sys.stderr)
        return 1
    s = sorted(vals)
    p95 = s[min(len(s) - 1, round(0.95 * (len(s) - 1)))]
    print(f"runs {len(vals)}: {vals}")
    print(f"min {s[0]} ms  P50 {statistics.median(s):.0f} ms  P95 {p95} ms  max {s[-1]} ms")
    print("target: P50 <= 5000 ms, P95 <= 8000 ms (add PLAYBACK_PREBUFFER_MS 300 for sound)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
