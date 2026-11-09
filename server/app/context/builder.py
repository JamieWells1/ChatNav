"""Per-query context bundle (plan 9.1): plain text, no coordinates, no place names."""

import sqlite3
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta

from app.config import settings
from app.context import text as tx
from app.gps import kinematics as k
from app.gps.derive import close_stale
from app.gps.rollups import LOCAL_TZ, Rollup, day_rollup, week_rollup

FIX_STALE_S = 10
RECENT_TRIP_MIN = 30
CHARS_PER_TOKEN = 4

EVENT_NAMES = {k.BRAKE: "harsh brake", k.ACCEL: "harsh acceleration"}


@dataclass(frozen=True)
class Turn:
    t_ms: int
    question: str
    answer: str


def estimate_tokens(text: str) -> int:
    return -(-len(text) // CHARS_PER_TOKEN)


def build_context(
    conn: sqlite3.Connection, device_id: str, now_ms: int, turns: tuple[Turn, ...] | list[Turn] = ()
) -> str:
    close_stale(conn, device_id, now_ms)
    live = _live_points(conn, device_id, now_ms)
    trip = conn.execute(
        "SELECT * FROM trips WHERE device_id = ? ORDER BY start_ms DESC LIMIT 1", (device_id,)
    ).fetchone()
    head = [_now_line(conn, device_id, now_ms, live)]
    head += _trace_lines(live, now_ms)
    head += _event_lines(conn, trip, now_ms)
    head.append(_trip_line(trip, now_ms))
    head.append(_today_line(day_rollup(conn, device_id, now_ms)))
    days = _day_lines(conn, device_id, now_ms)
    tail = _trend_lines(conn, device_id, now_ms) + _convo_lines(turns, now_ms)

    def render() -> str:
        return "\n".join(head + days + tail)

    while days and estimate_tokens(render()) > settings.context_max_tokens:
        days.pop()  # drop the oldest day first
    return render()


def _live_points(conn, device_id, now_ms) -> list[k.P]:
    rows = conn.execute(
        "SELECT t_ms, speed_cmps, lat_e7, lon_e7, course_deg10, sats FROM fixes "
        "WHERE device_id = ? AND valid = 1 AND outlier = 0 AND t_ms >= ? AND t_ms <= ? "
        "ORDER BY t_ms",
        (device_id, now_ms - settings.live_window_s * 1000, now_ms),
    ).fetchall()
    return [k.P(r[0], k.clamp_speed(r[1]), r[2], r[3], r[4], r[5]) for r in rows]


def _local_clock(now_ms: int) -> str:
    local = datetime.fromtimestamp(now_ms / 1000, UTC).astimezone(LOCAL_TZ)
    return local.strftime("%H:%M")


def _now_line(conn, device_id, now_ms, live) -> str:
    clock = f"Now: {_local_clock(now_ms)} UK time."
    last = conn.execute(
        "SELECT t_ms, valid, sats FROM fixes WHERE device_id = ? AND t_ms <= ? "
        "ORDER BY t_ms DESC LIMIT 1",
        (device_id, now_ms),
    ).fetchone()
    if last is None or now_ms - last["t_ms"] > 30_000:
        gap = f" for {tx.duration((now_ms - last['t_ms']) / 1000)}" if last else ""
        return f"{clock} GPS: no data from the device{gap}. Speed unknown."
    if not last["valid"]:
        good = conn.execute(
            "SELECT MAX(t_ms) FROM fixes WHERE device_id = ? AND valid = 1 AND t_ms <= ?",
            (device_id, now_ms),
        ).fetchone()[0]
        since = f"for {tx.duration((now_ms - good) / 1000)}" if good else "since it started"
        return f"{clock} GPS: no fix {since} ({last['sats']} satellites). Speed unknown."
    p = live[-1] if live else None
    if p is None or now_ms - p.t > FIX_STALE_S * 1000:
        age = tx.ago(now_ms - last["t_ms"])
        return f"{clock} GPS: fix is stale, last fix {age}. Speed unknown."
    state = "moving" if p.v > 0 else "stopped"
    heading = f", heading {tx.compass(p.course)}" if p.v > 0 else ""
    return (
        f"{clock} GPS: fix OK, {p.sats} satellites, last fix {tx.ago(now_ms - p.t)}. "
        f"Speed {tx.mph(p.v)} mph, {state}{heading}."
    )


def _trace_lines(live: list[k.P], now_ms: int) -> list[str]:
    win, step = settings.live_trace_window_s, settings.live_trace_step_s
    samples = []
    for i in range(win // step, -1, -1):
        t = now_ms - i * step * 1000
        near = [p for p in live if t - step * 1000 < p.t <= t]
        samples.append(str(tx.mph(near[-1].v)) if near else "-")
    trace = (
        f"Speed over the last {tx.duration(win)}, every {step} s, oldest first, mph "
        f"(- means no fix): {' '.join(samples)}."
    )
    peak = k.peak_accels(k.deltas(live), now_ms - settings.recent_peak_window_s * 1000)
    window = f"Last {settings.recent_peak_window_s} s"
    if peak is None:
        return [trace, f"{window}: no GPS data."]
    brake, accel = peak
    return [
        trace,
        f"{window}: strongest braking {abs(brake):.1f} m/s2 ({tx.strength(brake)}), "
        f"strongest acceleration {accel:.1f} m/s2 ({tx.strength(accel)}). "
        f"Harsh thresholds are {abs(settings.harsh_brake_ms2):.0f} m/s2.",
    ]


def _event_lines(conn, trip, now_ms) -> list[str]:
    recent = trip is not None and (
        trip["status"] == "open" or now_ms - trip["end_ms"] < RECENT_TRIP_MIN * 60_000
    )
    if not recent:
        return ["Recent events: none (no current trip)."]
    rows = conn.execute(
        "SELECT * FROM events WHERE trip_id = ? ORDER BY t_start_ms DESC LIMIT ?",
        (trip["id"], settings.context_max_events),
    ).fetchall()
    if not rows:
        return ["Recent events this trip: none."]
    lines = ["Recent events this trip, newest first:"]
    for e in rows:
        when = tx.ago(now_ms - e["t_end_ms"])
        if e["kind"] == k.OVER:
            lines.append(
                f"- over {settings.over_limit_mph} mph, {when}, for "
                f"{round((e['t_end_ms'] - e['t_start_ms']) / 1000)} s, peak {tx.mph(e['peak'])} mph"
            )
        else:
            lines.append(
                f"- {EVENT_NAMES[e['kind']]}, {when}, {abs(e['peak']):.1f} m/s2 "
                f"({tx.strength(e['peak'])}), {tx.mph(e['v_from'])} mph to {tx.mph(e['v_to'])} mph"
            )
    return lines


def _trip_line(trip, now_ms) -> str:
    if trip is None or trip["status"] != "open":
        return "This trip: not on a trip right now."
    since = trip["since_stop_ms"]
    stop = (
        f"{tx.duration((now_ms - since) / 1000)} since the last stop"
        if since is not None
        else "currently stopped"
    )
    avg = trip["distance_m"] / trip["moving_s"] if trip["moving_s"] else 0
    return (
        f"This trip: driving for {tx.duration((trip['last_fix_ms'] - trip['start_ms']) / 1000)}, "
        f"{tx.miles(trip['distance_m'])}, average moving speed {tx.mph(avg)} mph, "
        f"top speed {tx.mph(trip['max_speed_ms'])} mph, "
        f"{tx.count(trip['brakes'], 'harsh brake')}, "
        f"{tx.count(trip['accels'], 'harsh acceleration')}, "
        f"{round(trip['over_s'])} s over {settings.over_limit_mph} mph, {stop}."
    )


def _summary(r: Rollup) -> str:
    return (
        f"{tx.count(r.trips, 'trip')}, {tx.duration(r.duration_s)} driving, "
        f"{tx.miles(r.distance_m)}, "
        f"top speed {tx.mph(r.max_speed_ms)} mph, {tx.count(r.brakes, 'harsh brake')}, "
        f"{tx.count(r.accels, 'harsh acceleration')}, "
        f"{round(r.over_s)} s over {settings.over_limit_mph} mph"
    )


def _today_line(r: Rollup) -> str:
    return f"Today: {_summary(r)}." if r.trips else "Today: no driving yet."


def _day_lines(conn, device_id, now_ms) -> list[str]:
    """Newest first so the oldest can be popped from the end."""
    lines = []
    local = datetime.fromtimestamp(now_ms / 1000, UTC).astimezone(LOCAL_TZ)
    for back in range(1, 8):
        r = day_rollup(conn, device_id, now_ms, back)
        if r.trips:
            day = local - timedelta(days=back)
            name = "Yesterday" if back == 1 else day.strftime("%a %d %b")
            lines.append(f"{name}: {_summary(r)}.")
    return ["Last 7 days (days with driving):"] + lines if lines else ["Last 7 days: no driving."]


def _week_text(label: str, r: Rollup, weeks: int = 1) -> str:
    if r.miles < 1:
        return f"{label}: under 1 mile driven."
    return (
        f"{label}: {tx.miles(r.distance_m / weeks)} per week, average moving speed "
        f"{tx.mph(r.avg_moving_ms)} mph, {r.per_100_miles(r.brakes):.1f} harsh brakes and "
        f"{r.per_100_miles(r.accels):.1f} harsh accelerations per 100 miles."
    )


def _trend_lines(conn, device_id, now_ms) -> list[str]:
    return [
        _week_text("This week (last 7 days)", week_rollup(conn, device_id, now_ms, 0)),
        _week_text("Last week", week_rollup(conn, device_id, now_ms, 1)),
        _week_text(
            "4-week average before this week", week_rollup(conn, device_id, now_ms, 1, 4), 4
        ),
    ]


def _convo_lines(turns, now_ms) -> list[str]:
    live = [t for t in turns if now_ms - t.t_ms <= settings.convo_ttl_s * 1000]
    live = live[-settings.convo_turns :]
    lines = []
    for t in live:
        lines += [f"Driver: {t.question}", f"You: {t.answer}"]
    return ["Conversation so far:"] + lines if lines else []
