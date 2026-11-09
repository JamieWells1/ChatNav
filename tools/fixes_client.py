"""Shared by sim_trip.py and nmea_replay.py: POST fix batches like the device does."""

import http.client
import json
import os
import sys
from urllib.parse import urlparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


class FixPoster:
    """One kept-open connection. With a fingerprint it pins the cert (as fake_device does)."""

    def __init__(self, server: str, token: str, fingerprint: str = ""):
        url = urlparse(server)
        self.token = token
        host = url.hostname or "127.0.0.1"
        if url.scheme == "https":
            import fake_device

            pin = fake_device.parse_fingerprint(fingerprint)
            self.conn = fake_device.open_pinned(host, url.port or 443, pin)
        else:
            self.conn = http.client.HTTPConnection(host, url.port or 80, timeout=10)

    def post(self, body: bytes) -> int:
        headers = {"Authorization": f"Bearer {self.token}", "Content-Type": "application/octet-stream"}
        self.conn.request("POST", "/v1/fixes", body=body, headers=headers)
        r = self.conn.getresponse()
        data = r.read()
        if r.status != 200:
            raise RuntimeError(f"/v1/fixes returned {r.status}: {data[:100]!r}")
        return json.loads(data)["ack"]
