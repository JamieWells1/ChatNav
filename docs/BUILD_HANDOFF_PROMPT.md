# Build Handoff Prompt

Paste everything below the line into a new Claude Code session opened in `~/Code/Projects/ChatNav`.

---

You are building ChatNav end to end: a push-to-talk, GPS-aware driving companion. A Raspberry Pi Pico 2 W records a spoken question. A server on a DigitalOcean droplet turns it into text, asks Claude for an answer using the driver's GPS data, and sends back speech that the Pico plays. The hardware is already wired. Your job is the firmware, the server, the tools and the tests, delivered milestone by milestone with as little input from the owner as possible.

## Read first (sources of truth)

| File | Owns |
|---|---|
| `docs/IMPLEMENTATION_PLAN.md` | All software design: use cases (U*/F* IDs), state machine, protocol, GPS data lifecycle and retention, context bundle, security, tests (T*), phases, tunables. **Follow it.** If you must deviate, record why in `docs/DECISIONS.md` |
| `docs/PROJECT_BRIEF.md` §4 | Wiring and pin map (wins over everything else for pins) |
| `~/.claude/CLAUDE.md` | Owner's working rules (reporting format, commits, comments, file size). Obey them |
| `assignment/REFERENCES.md` | Cited sources. Don't add to it unless asked |

## Reuse from Photon

`~/Code/Projects/photon` is the owner's earlier Pico 2 W project on pico-sdk 2.2 (VS Code Pico extension, SDK under `~/.pico-sdk`). Copy and adapt rather than rewrite. The reuse map is in plan §4. Key files:

- `src/middleware/wifi.c`
- `src/middleware/http.c` (raw lwIP; extend it for TLS + streaming)
- `src/middleware/flash_memory.c`
- `src/input/input.c` (KY-040 `Rotator`)
- `src/server/*` (setup captive portal; `http_server.c` is over 800 lines, so split it first)
- `src/common/error.c`
- `CMakeLists.txt` and `lwipopts.h` (structure and lwIP tuning)
- `proxy/photon-proxy/` (FastAPI + systemd + nginx deploy pattern)

Photon sets `PICO_BOARD pico_w`; ChatNav must use `pico2_w`. Never modify the Photon repo.

## Fixed decisions (don't reopen)

| Decision | Detail |
|---|---|
| Firmware language | C on pico-sdk. No MicroPython or Arduino |
| AI services | OpenAI `gpt-4o-mini-transcribe` (STT) and OpenAI TTS with `pcm` output (resample to `AUDIO_SAMPLE_RATE` on the server). LLM `claude-haiku-5-5` via the official `anthropic` Python SDK, each behind a small provider interface |
| Server | Python FastAPI + SQLite |
| Network security | TLS from the first network code: self-signed server cert with its fingerprint pinned on the device, one kept-open connection, bearer device token |
| GPS data | Tiered retention exactly as plan §8; coordinates never go to the LLM; audio never touches disk |
| Volume | Scaled on the device |

## Milestones

Work in order. Each milestone ends with: automated tests passing, docs updated, a commit (`git add -A && git commit`, one imperative line, **never push**), a git tag, and a row appended to `docs/GENAI_LOG.md` (date, "Claude Code", what you built, how you verified it). Save report evidence (logs, screenshots, CSVs, charts) under `assignment/evidence/<milestone>/`, with no personal data.

| # | Milestone | Deliverables | Done when | Tag |
|---|---|---|---|---|
| M0 | Scaffold | Repo layout from plan §4; `firmware/` builds for `pico2_w` (blink + USB serial log); `server/` with `/v1/health`, config from `.env`, pytest + ruff; Unity host-test harness for firmware; `tools/fake_device.py` stub; `docs/DECISIONS.md` started; GitHub Actions running both test suites | `cmake --build` + both test suites green locally | `m0-scaffold` |
| M1 | Device drivers | GPS UART1 + NMEA parser (RMC/GGA, checksums); encoder rotation + press (Photon `Rotator`); LED states; mic I2S-in PIO + DMA; amp I2S-out PIO with tones and cues; GPIO-powered mic/encoder (GP9/GP12 high); `MIC_WARMUP_MS`. **Plus one diagnostic firmware per module** (`diag_gps`, `diag_encoder`, `diag_mic` → WAV over USB serial via `tools/wav_capture.py`, `diag_amp`) so the owner can test one part at a time | Builds clean; parser, PCM and volume host tests green | `m1-drivers` |
| M2 | Secure link + setup | Setup-mode captive portal (F1/F2) storing Wi-Fi creds, server host, device token and cert fingerprint in flash; mbedTLS client with pinned cert; server TLS via nginx with a self-signed cert; device-token auth (hashed at rest) | Server side proven with `curl`: TLS works, no/wrong token → 401 (evidence saved) | `m2-link` |
| M3 | Voice loop | `/v1/query` with chunked PCM upload and streamed PCM reply; STT → LLM (system prompt plan §9.2) → TTS; error clips; F3–F9, U9, U11–U13; per-stage timing log (plan §7.2); `tools/latency_report.py` | `tools/fake_device.py` sends a recorded WAV over TLS and gets a spoken answer; server-side T1/T2 baseline saved | `m3-voice` |
| M4 | GPS data | `/v1/fixes` binary batches + device ring buffer, retry and flush-on-press; quality filter, trips, kinematics, events, minute buckets, trip summaries, rollups; context builder (plan §9.1) with a test asserting no coordinates; `tools/sim_trip.py`, `tools/nmea_replay.py`, `tools/export_trip.py`; U1–U8, U10, F11–F14 | All GPS use cases pass in pytest on simulated trips | `m4-gps` |
| M5 | Privacy + hardening | Purge job and ordering rules (plan §8.5); delete-all (U14); rate limit; ufw on the droplet; server-side E1/E2 evidence per plan §10.1 | E1/E2 evidence saved; purge tests green | `m5-privacy` |
| M6 | Owner test pack | `docs/TESTING.md`: step-by-step owner tests in build order (diag firmwares one module at a time → full firmware → setup portal → voice → GPS → privacy E3 → T1–T8 → calibration drive → demo), each with exact commands, prebuilt `.uf2` path, what success looks/sounds like, a likely-fault table, and **what to send back** (serial log, WAV, screenshot, pass/fail). End with a feedback template | Every plan use case and test ID is covered by a step | `m6-testpack` |

After M6, stop and report. The owner runs `docs/TESTING.md` and sends feedback; you then fix issues in batches (one commit per fix) and re-issue only the affected test steps.

## Owner input

The only thing to ask for during the build, at the start:
1. SSH alias/host for the droplet.
2. OpenAI and Anthropic API keys. They go in the server `.env` only.

Carry on with M0 offline while waiting. Don't ask for anything else. Proceed on the plan's defaults and log choices in `docs/DECISIONS.md`.

**No hardware during the build.** Don't flash the Pico, read its serial output or ask the owner to touch the device. Verify firmware by compiling, running host unit tests, and exercising the server with `fake_device.py` / `sim_trip.py`. Hardware testing is the owner's job, using `docs/TESTING.md`.

## Rules

- **Droplet:** it also runs the owner's Photon/Delta proxy (systemd `delta-proxy@*`, nginx, ports 8080–8090). Don't stop, change or firewall-block those. Before enabling ufw, list listening ports and keep everything Photon uses open. Install ChatNav under its own user, directory, systemd unit and nginx site. Show the owner the ufw rules before enabling.
- **Secrets:** never commit keys, tokens, certs' private keys, Wi-Fi credentials or real GPS logs. Keep `.gitignore` current and grep the diff before every commit.
- **Hardware risk:** since nothing runs on the device until the owner tests, keep firmware modules independent and loudly logged (`[MODULE] message` over USB serial) so faults are easy to pin down from a pasted log.
- **Context:** use one sub-agent per milestone to keep context small; the main session coordinates, reviews and commits.
- **Reporting:** report to the owner only at milestone ends, using the Status/Problem/Fix/Impact format from their CLAUDE.md.
