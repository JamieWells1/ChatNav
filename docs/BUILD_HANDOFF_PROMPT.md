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
| M1 | Hardware bring-up | GPS UART1 + NMEA parser (RMC/GGA, checksums) with host tests; encoder rotation + press (Photon `Rotator`); LED states; mic I2S-in PIO + DMA dumping a WAV over USB serial (`tools/wav_capture.py`); amp I2S-out PIO playing a tone and the cues; GPIO-powered mic/encoder (GP9/GP12 high); `MIC_WARMUP_MS` | Owner checkpoint A passes; T6 and T8 recorded | `m1-bringup` |
| M2 | Secure link + setup | Setup-mode captive portal (F1/F2) storing Wi-Fi creds, server host, device token and cert fingerprint in flash; mbedTLS client with pinned cert to `/v1/health`; server TLS via nginx with a self-signed cert; device-token auth (hashed at rest) | Device gets health check over TLS; wrong cert refused; no-token request → 401 (evidence saved) | `m2-link` |
| M3 | Voice loop | `/v1/query` with chunked PCM upload and streamed PCM reply; STT → LLM (system prompt plan §9.2, no GPS context yet) → TTS; error clips; F3–F9, U9, U11–U13; per-stage timing log (plan §7.2); `tools/latency_report.py` | General questions answered on device; T1/T2 baseline saved | `m3-voice` |
| M4 | GPS data | `/v1/fixes` binary batches + device ring buffer, retry and flush-on-press; quality filter, trips, kinematics, events, minute buckets, trip summaries, rollups; context builder (plan §9.1) with a test asserting no coordinates; `tools/sim_trip.py`, `tools/nmea_replay.py`, `tools/export_trip.py`; U1–U8, U10, F11–F14 | All GPS use cases pass in pytest on simulated and replayed trips | `m4-gps` |
| M5 | Privacy + hardening | Purge job and ordering rules (plan §8.5); delete-all (U14); rate limit; ufw on the droplet; E1–E3 evidence per plan §10.1 | E1–E3 evidence saved; purge tests green | `m5-privacy` |
| M6 | Evaluation | T1–T5 and T7 runs with results tables and charts in `assignment/evidence/m6/`; threshold tuning from calibration data | Owner checkpoints B and C done; results committed | `m6-eval` |

## Owner checkpoints (the only times you need the owner)

Batch requests and ask only at these points. Otherwise proceed on the plan's defaults and log choices in `docs/DECISIONS.md`.

| When | Ask the owner for |
|---|---|
| Start | 1) SSH alias/host for the droplet; 2) OpenAI and Anthropic API keys (go into server `.env` only); 3) the phone hotspot name (2.4 GHz). Then continue M0 offline while you wait |
| Checkpoint A (M1) | Pico plugged in by USB, antenna at a window: you flash with `picotool` and read USB serial yourself; the owner only speaks/claps on cue and listens for tones. The owner's global rule says they do manual app testing, and this handoff explicitly allows you to flash and read serial |
| Checkpoint B (M6) | One calibration drive logging GPS (passenger or parked-car stops only), with the phone running phyphox as the reference accelerometer; owner sends the phyphox CSV |
| Checkpoint C (M6) | Owner records the demo video |

## Rules

- **Droplet:** it also runs the owner's Photon/Delta proxy (systemd `delta-proxy@*`, nginx, ports 8080–8090). Don't stop, change or firewall-block those. Before enabling ufw, list listening ports and keep everything Photon uses open. Install ChatNav under its own user, directory, systemd unit and nginx site. Show the owner the ufw rules before enabling.
- **Secrets:** never commit keys, tokens, certs' private keys, Wi-Fi credentials or real GPS logs. Keep `.gitignore` current and grep the diff before every commit.
- **Scope:** if a milestone is blocked on hardware, move on to server and tool work that doesn't need it, and come back later.
- **Context:** use one sub-agent per milestone to keep context small; the main session coordinates, reviews and commits.
- **Reporting:** report to the owner only at milestone ends and checkpoints, using the Status/Problem/Fix/Impact format from their CLAUDE.md.
