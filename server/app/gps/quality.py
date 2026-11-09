"""Ingest-time quality checks (plan 8.1). Outliers are flagged later, against neighbours."""

from app.config import settings
from app.gps import wire

MIN_PLAUSIBLE_UTC_S = 1_500_000_000


def time_ok(f: wire.Fix) -> bool:
    return bool(f.flags & wire.FLAG_TIME_VALID) and f.utc_s >= MIN_PLAUSIBLE_UTC_S


def is_valid(f: wire.Fix) -> bool:
    return (
        time_ok(f)
        and bool(f.flags & wire.FLAG_RMC_VALID)
        and bool(f.flags & wire.FLAG_POS)
        and f.quality >= 1
        and f.sats >= settings.min_sats
        and f.hdop10 / 10 <= settings.max_hdop
    )
