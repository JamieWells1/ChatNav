import re
from pathlib import Path

import pytest
import sim_trip
from gps_helpers import DAY, T0, ingest, record_time_ms, sim

from app.config import settings
from app.context.builder import Turn, build_context, estimate_tokens

PROMPT = Path(__file__).resolve().parents[1] / "app" / "context" / "system_prompt.txt"
COORD = re.compile(r"-?\d{1,3}\.\d{3,}")
COORD_WORDS = re.compile(r"\b(lat|lon|lng|latitude|longitude)\b|°", re.I)


def ctx_at(conn, records, cut=None, after_s=1, **kw):
    """Ingest records[:cut] and build context just after the last one."""
    part = records if cut is None else records[:cut]
    ingest(conn, part)
    now = record_time_ms(part[-1]) + after_s * 1000
    return build_context(conn, "dev1", now, **kw), now


def test_u1_live_speed_mph_and_gps_status(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("commute"), cut=100)  # mid cruise at 14 m/s
    assert "Speed 31 mph, moving" in ctx
    assert "heading north-east" in ctx
    assert "fix OK" in ctx and "9 satellites" in ctx and "last fix just now" in ctx
    assert "Now: 08:" in ctx  # UK local time
    assert "Speed over the last 2 min, every 5 s" in ctx


def test_u1_stale_fix_is_reported(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("commute"), cut=100, after_s=20)
    assert "fix is stale" in ctx and "Speed unknown" in ctx


def test_stopped_has_no_heading(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("commute"))
    assert "stopped" in ctx and "heading" not in ctx


def test_f11_no_fix_context_says_how_long(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    tr.change_speed(14, 1.5)
    tr.cruise(30)
    tr.no_fix(240)
    ctx, _ = ctx_at(gps_conn, tr.records)
    assert "GPS: no fix for 4 min" in ctx and "Speed unknown" in ctx


def test_no_data_from_device_at_all(gps_conn):
    ctx = build_context(gps_conn, "dev1", T0 * 1000)
    assert "no data from the device" in ctx


def test_u2_recent_harsh_brake_with_time_ago_and_strength(gps_conn):
    records = sim("harsh")
    brake_end = next(i for i, r in enumerate(records) if i > 80 and r[5] <= 400)
    ctx, _ = ctx_at(gps_conn, records, cut=brake_end + 10)
    assert "harsh brake" in ctx and "ago" in ctx
    assert re.search(r"harsh brake, [^,]+ ago, \d\.\d m/s2 \(hard\), \d+ mph to \d+ mph", ctx)
    assert "Last 30 s: strongest braking" in ctx
    assert "Recent events this trip, newest first:" in ctx


def test_u2_near_miss_peak_below_threshold_is_visible(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    tr.change_speed(20, 1.5)
    tr.cruise(20)
    tr.change_speed(10, 2.4)
    tr.cruise(5)
    ctx, _ = ctx_at(gps_conn, tr.records)
    assert "strongest braking 2.4 m/s2 (moderate)" in ctx
    assert "Recent events this trip: none." in ctx


def test_u3_current_trip_summary(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("commute"), cut=250)
    line = next(x for x in ctx.splitlines() if x.startswith("This trip:"))
    assert "driving for" in line and "miles" in line and "average moving speed" in line
    assert "top speed" in line and "since the last stop" in line


def test_u3_not_on_a_trip(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("parked"))
    assert "This trip: not on a trip right now." in ctx


def test_u4_today_summary_with_events(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("harsh"))
    today = next(x for x in ctx.splitlines() if x.startswith("Today:"))
    assert "1 trip" in today and "1 harsh brake" in today and "1 harsh acceleration" in today
    assert "miles" in today


def test_u5_weekly_trend_uses_rates_per_100_miles(gps_conn):
    ingest(gps_conn, sim("harsh", start=T0 - 8 * DAY))
    ingest(gps_conn, sim("harsh", start=T0 - 3 * DAY))
    ctx, _ = ctx_at(gps_conn, sim("commute"))
    for label in ("This week (last 7 days):", "Last week:", "4-week average before this week:"):
        assert label in ctx
    rates = r"\d+\.\d harsh brakes and \d+\.\d harsh accelerations per 100 miles"
    assert re.search(rf"Last week: .*{rates}", ctx)


def test_u6_long_trip_duration_and_time_since_last_stop(gps_conn):
    tr = sim_trip.Track(T0)
    tr.hold(5)
    tr.change_speed(14, 1.5)
    tr.cruise(130 * 60)
    ingest(gps_conn, tr.records, per_batch=900)
    now = record_time_ms(tr.records[-1]) + 1000
    ctx = build_context(gps_conn, "dev1", now)
    assert "driving for 2 h 10 min" in ctx
    assert "2 h 10 min since the last stop" in ctx
    assert "Highway Code" in PROMPT.read_text() and "15 minutes" in PROMPT.read_text()


def test_u7_top_speed_and_over_70(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("motorway"))
    assert "top speed 71 mph" in ctx
    assert "- over 70 mph," in ctx and "peak 71 mph" in ctx
    assert re.search(r"4\d s over 70 mph", ctx)
    assert "70 miles an hour" in PROMPT.read_text()


def test_u8_no_location_data_and_prompt_declines_places(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("harsh"))
    assert not COORD.search(ctx) and not COORD_WORDS.search(ctx)
    prompt = PROMPT.read_text()
    assert "do not have location or road names" in prompt and "place names" in prompt


def test_u10_follow_up_uses_recent_turns_only(gps_conn):
    ingest(gps_conn, sim("commute", start=T0 - DAY))
    records = sim("commute")
    ingest(gps_conn, records)
    now = record_time_ms(records[-1]) + 1000
    turns = [
        Turn(now - 300_000, "Old question?", "Old answer."),
        Turn(now - 60_000, "How was today?", "Fine, one trip."),
    ]
    ctx = build_context(gps_conn, "dev1", now, turns)
    assert "Driver: How was today?" in ctx and "You: Fine, one trip." in ctx
    assert "Old question" not in ctx  # past CONVO_TTL_S
    assert "Yesterday: 1 trip" in ctx  # "and yesterday?" has its data


def test_conversation_limited_to_convo_turns(gps_conn):
    turns = [Turn(T0 * 1000 - i * 1000, f"q{i}", f"a{i}") for i in range(10)][::-1]
    ctx = build_context(gps_conn, "dev1", T0 * 1000, turns)
    assert ctx.count("Driver:") == settings.convo_turns


def test_token_cap_drops_oldest_daily_lines_first(gps_conn, monkeypatch):
    for d in range(1, 8):
        ingest(gps_conn, sim("commute", start=T0 - d * DAY))
    records = sim("commute")
    ingest(gps_conn, records)
    now = record_time_ms(records[-1]) + 1000
    full = build_context(gps_conn, "dev1", now)
    assert "Yesterday:" in full and estimate_tokens(full) <= settings.context_max_tokens
    cap = estimate_tokens(full) - 60
    monkeypatch.setattr(settings, "context_max_tokens", cap)
    trimmed = build_context(gps_conn, "dev1", now)
    assert estimate_tokens(trimmed) <= cap
    assert "Yesterday:" in trimmed
    assert trimmed.count(" 1 trip,") < full.count(" 1 trip,")
    assert "Today:" in trimmed and "This week" in trimmed


@pytest.mark.parametrize("scenario", ["commute", "harsh", "motorway", "parked"])
def test_context_never_contains_coordinates(gps_conn, scenario):
    lat0, lon0 = 51.5, -0.12
    ingest(gps_conn, sim("harsh", start=T0 - 2 * DAY, lat=lat0, lon=lon0))
    records = sim(scenario, lat=lat0, lon=lon0)
    for cut in (60, len(records)):
        ctx, _ = ctx_at(gps_conn, records, cut=min(cut, len(records)))
        assert not COORD.search(ctx), ctx
        assert not COORD_WORDS.search(ctx)
        for n in re.findall(r"-?\d+\.\d+", ctx):
            assert len(n.split(".")[1]) <= 2


def test_context_is_plain_text_mph_miles(gps_conn):
    ctx, _ = ctx_at(gps_conn, sim("motorway"))
    assert "km/h" not in ctx and "kph" not in ctx and "{" not in ctx
    assert "mph" in ctx and "miles" in ctx


def test_system_prompt_rules_present():
    p = PROMPT.read_text()
    for needle in ("at most 2 sentences", "no GPS fix", "estimates", "per 100 miles",
                   "Never suggest looking at a screen", "ask again when parked", "untrusted"):
        assert needle in p
