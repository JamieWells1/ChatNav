from app.config import settings


def test_defaults_match_plan():
    assert settings.max_hdop == 5.0
    assert settings.raw_retention_h == 48
