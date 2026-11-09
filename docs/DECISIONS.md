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
| 2026-11-09 | Fix record is 28 bytes, little-endian: device_ms u32, utc_s u32, utc_ms u16, lat/lon i32 x1e7, speed cm/s u16, course deg x10 u16, altitude dm i16, sats, quality, hdop x10 (u8, capped 255), flags (1 time valid, 2 RMC valid, 4 GGA, 8 position) | Other units | Fits the plan's ~28 bytes; server parser must match (M2) |
| 2026-11-09 | Fix ring stores packed 28-byte records; sequence starts at 1; ack(seq) releases all records with seq <= ack; batch seq = seq of last record | Store structs / packed | 1800 x 28 = 50 KB, upload is a plain copy; ack of dropped records is harmless |
| 2026-11-09 | One fix record per GPS epoch, built from RMC + GGA with the same UTC time in either order; an unpaired half is flushed on the next sentence | Emit on RMC only | NEO-6M sends RMC before GGA, other firmware differs |
| 2026-11-09 | NMEA parse is integer only; position accepts talker IDs GP/GN/etc.; RMC with status V still records (flag cleared) | Float parse | Exact, no FPU or libm in the parser |
| 2026-11-09 | Mic PIO runs 4 PIO cycles per bit, 32-bit slots, both slots captured, right slot discarded in `pcm_from_i2s32`; 16 KB DMA double buffer (2 x 64 ms) | Capture left only | Simple PIO program; 128 KB/s DMA is cheap. Bit alignment is unverified until diag_mic (T8) |
| 2026-11-09 | Amp PIO uses 16-bit slots (BCLK = 32 x rate), one 32-bit word per frame with the sample duplicated in both halves | 32-bit slots | Mono to stereo duplication is one word; MAX98357A accepts 32fs. Verify on the clone board |
| 2026-11-09 | Volume: step 0 mute, top step unity, 3 dB per step (15 steps = 45 dB range), default step 8 until flash config exists | Linear / other dB | Log curve per plan; owner can retune `VOLUME_DB_PER_STEP` |
| 2026-11-09 | Cues are tone sequences at fixed amplitude 16000 with 5 ms fades; volume applied at DMA fill | Pre-baked PCM | No flash cost, tweakable in one table |
| 2026-11-09 | LED patterns: slow blink 1 Hz, fast blink 5 Hz, flashing 100 ms on per 500 ms; pure function in `led_pattern.c` | Blocking blinks | Host-testable, non-blocking |
| 2026-11-09 | Photon `error.c` trimmed to common/WiFi/HTTP/flash plus new audio and GPS codes; Photon `input.c` kept as `Rotator` only (Button, `ws2812.pio.h` and logging inside predicates removed, init added) | Verbatim copy | Photon parser/server codes and LED PIO do not apply |
| 2026-11-09 | diag_mic records 5 s into RAM on serial `r`, then streams a whole WAV between `[MIC] WAV_BEGIN <bytes>` and `[MIC] WAV_END`; `tools/wav_capture.py` needs pyserial | Stream live | USB stalls cannot drop samples; framing is trivial to parse |
| 2026-11-09 | `.gitignore`: `*.nmea`/`*.wav` allowed only under `firmware/tests/fixtures/` and `tools/fixtures/` | Unignore everywhere | Real recordings and GPS logs stay out of git |
| 2026-11-09 | Device token is `<id>.<secret>` (secret 256 random bits); DB stores per-device salt + SHA-256(salt+secret), never the token; revoke sets `revoked_at`; token shown once by `python -m app.cli device create` | bcrypt/argon2 / salted SHA-256 | Secret is high-entropy so a slow hash adds nothing; constant-time compare, no extra dependency |
| 2026-11-09 | TLS: nginx on 443 with a self-signed P-256 cert proxying to uvicorn on 127.0.0.1:8100; device pins the leaf cert SHA-256 (entered in the portal as AA:BB:..) | Let's Encrypt / pinned self-signed | No domain; avoids trusting a CA bundle on the Pico; ports 8080-8090 left to Photon/Delta |
