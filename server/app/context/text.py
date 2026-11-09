"""Plain-speech formatting for the context bundle: mph, miles, relative times, words."""

from app.gps.units import M_PER_MILE, MS_TO_MPH

COMPASS = ["north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"]


def mph(ms: float) -> int:
    return round(ms * MS_TO_MPH)


def miles(m: float) -> str:
    return f"{m / M_PER_MILE:.1f} miles"


def duration(seconds: float) -> str:
    mins = round(seconds / 60)
    if mins < 1:
        return f"{round(seconds)} s"
    if mins < 60:
        return f"{mins} min"
    return f"{mins // 60} h {mins % 60} min"


def ago(delta_ms: float) -> str:
    s = max(0, delta_ms / 1000)
    if s < 5:
        return "just now"
    if s < 90:
        return f"{round(s)} s ago"
    return f"{duration(s)} ago"


def compass(course_deg10: int) -> str:
    return COMPASS[round(course_deg10 / 10 / 45) % 8]


def strength(accel_ms2: float) -> str:
    a = abs(accel_ms2)
    if a < 1.5:
        return "gentle"
    if a < 3.0:
        return "moderate"
    if a < 4.5:
        return "firm"
    return "hard"


def count(n: int, noun: str) -> str:
    return f"{n} {noun}" if n == 1 else f"{n} {noun}s"
