# GenAI Use Log

Disclosure record for IOT6000U 002 (coding support and agentic AI coding must be disclosed and evidenced).

| Date | Tool | Task | How output was checked |
|---|---|---|---|
| 2026-11-06 | Claude Code (Claude Opus 5.5) | Drafted `IMPLEMENTATION_PLAN.md` from the project brief, brainstorm notes and the Photon/Delta codebase | Owner review of decisions, use cases and retention tiers |
| 2026-11-06 | Claude Code (Claude Opus 5.5) + web search | Gathered 8 key references into `assignment/REFERENCES.md` | Owner to open and verify each source before citing; unsourced claims dropped |
| 2026-11-06 | Claude Code (Claude Sonnet 5.5) | M0 scaffold: firmware pico2_w blink + USB log, Unity host tests, FastAPI /v1/health, fake_device stub, CI | Firmware build produced .uf2; Unity, pytest and ruff passed locally |
| 2026-11-09 | Claude Code (Claude Sonnet 5.5) | M1 drivers: GPS/NMEA, encoder, LED, mic and amp I2S PIO, cues, four diag firmwares, wav_capture.py | All targets built with 0 warnings; 7 host tests passed. Not run on hardware |
| 2026-11-09 | Claude Code (Claude Sonnet 5.5) | M2 secure link: setup portal, flash config, pinned-fingerprint mbedTLS client, device-token auth, deploy files | Firmware 0 warnings, 12 host tests, 14 pytest, ruff clean; local TLS curl evidence saved. Not run on hardware or droplet |
