# Decision log

| Date | Decision | Options | Reason |
|---|---|---|---|
| 2026-11-06 | Build for `pico2_w` (RP2350) | `pico_w` (Photon's setting) / `pico2_w` | Hardware is a Pico 2 W; Photon's CMake must not be copied verbatim |
| 2026-11-06 | Photon source is at `~/Projects/photon` | `~/Code/Projects` (earlier docs) | Path in earlier notes was wrong; this is where the repo lives |
| 2026-11-06 | Firmware host tests: Unity v2.6.0 vendored in `firmware/tests/vendor`, run via CMake + ctest | FetchContent / vendor | Works offline and in CI with no download step |
| 2026-11-06 | Server: FastAPI + pydantic-settings, `pyproject.toml`, venv via Makefile | requirements.txt / pyproject | One file for deps, pytest and ruff config |
| 2026-11-06 | `/v1/health` returns plain text `ok` | JSON / plain text | Matches the brief |
| 2026-11-06 | `*.uf2` stays gitignored | Commit builds / ignore | Build artefacts; revisit if a release binary is needed |
| 2026-11-06 | Copied Photon `lwipopts.h` and `pico_sdk_import.cmake` into `firmware/` | Write own / copy | The CYW43 lwIP build needs them; Photon is the owner's own code |
| 2026-11-06 | Local pip needs `--index-url https://pypi.org/simple` if the corporate mirror 404s | n/a | Mirror lacked a wheel during setup; CI uses default PyPI |
