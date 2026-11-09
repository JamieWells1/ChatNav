import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from app.config import settings  # noqa: E402
from app.store.gps_db import connect  # noqa: E402


@pytest.fixture
def gps_conn(tmp_path, monkeypatch):
    monkeypatch.setattr(settings, "db_path", str(tmp_path / "gps.db"))
    conn = connect()
    yield conn
    conn.close()
