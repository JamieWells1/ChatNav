"""T1/T2 latency tables (and an optional chart) from the server timing table.

    python tools/latency_report.py --db server/chatnav.db [--chart out.png] [--label TEXT]

T1 is server-side release to first audio (first_audio_ms). T2 is per-stage time.
Only outcome=ok rows count. Needs only the standard library; the chart needs matplotlib.
"""

import argparse
import sqlite3
import statistics
import sys

T1_COL = "first_audio_ms"
T2_COLS = [
    "stt_ms",
    "context_ms",
    "llm_first_ms",
    "llm_total_ms",
    "tts_first_ms",
    "tts_total_ms",
    "total_ms",
]
CHART_STAGES = ["stt_ms", "context_ms", "llm_total_ms", "tts_first_ms"]


def percentile(values: list[float], p: float) -> float:
    s = sorted(values)
    k = (len(s) - 1) * p / 100
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def stats(values: list[float]) -> dict[str, float]:
    return {
        "n": len(values),
        "p50": percentile(values, 50),
        "p95": percentile(values, 95),
        "min": min(values),
        "max": max(values),
        "mean": statistics.fmean(values),
    }


def load(db: str) -> list[sqlite3.Row]:
    conn = sqlite3.connect(db)
    conn.row_factory = sqlite3.Row
    try:
        return conn.execute("SELECT * FROM timings WHERE outcome = 'ok' ORDER BY id").fetchall()
    finally:
        conn.close()


def column(rows: list[sqlite3.Row], col: str) -> list[float]:
    return [r[col] for r in rows if r[col] is not None]


def table(title: str, named: dict[str, list[float]]) -> str:
    lines = [
        f"{title}",
        "| metric | n | P50 ms | P95 ms | min ms | max ms |",
        "|---|---|---|---|---|---|",
    ]
    for name, vals in named.items():
        if not vals:
            continue
        s = stats(vals)
        lines.append(
            f"| {name} | {s['n']} | {s['p50']:.0f} | {s['p95']:.0f} "
            f"| {s['min']:.0f} | {s['max']:.0f} |"
        )
    return "\n".join(lines)


def chart(rows: list[sqlite3.Row], path: str, label: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (a, b) = plt.subplots(1, 2, figsize=(10, 4))
    t1 = column(rows, T1_COL)
    a.hist(t1, bins=min(20, max(5, len(t1) // 2)), color="#2a6fb0")
    a.axvline(percentile(t1, 50), color="#d1495b", label="P50")
    a.set_xlabel("release to first audio (ms)")
    a.set_ylabel("queries")
    a.set_title("T1")
    a.legend()
    medians = [percentile(column(rows, c), 50) if column(rows, c) else 0 for c in CHART_STAGES]
    b.barh([c.removesuffix("_ms") for c in CHART_STAGES], medians, color="#2a6fb0")
    b.set_xlabel("median ms")
    b.set_title("T2 stages")
    fig.suptitle(label)
    fig.tight_layout()
    fig.savefig(path, dpi=130)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--db", default="server/chatnav.db")
    p.add_argument("--chart", help="write a PNG here (needs matplotlib)")
    p.add_argument("--label", default="", help="heading, e.g. 'stub-provider baseline'")
    args = p.parse_args()
    rows = load(args.db)
    if not rows:
        print("no outcome=ok timing rows found", file=sys.stderr)
        return 1
    if args.label:
        print(f"# {args.label}\n")
    print(table("T1: release to first audio (server side)", {T1_COL: column(rows, T1_COL)}))
    print()
    print(table("T2: per-stage breakdown", {c: column(rows, c) for c in T2_COLS}))
    cost = column(rows, "cost_usd")
    if cost:
        print(f"\nEstimated cost per query: mean ${statistics.fmean(cost):.5f} (assumed rates)")
    if args.chart:
        try:
            chart(rows, args.chart, args.label or "latency")
            print(f"chart: {args.chart}")
        except ImportError:
            print("matplotlib not installed; skipping chart", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
