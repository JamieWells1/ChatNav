# ChatNav owner test pack

You run every test. Nothing in M0 to M5 has run on a device. Work top to bottom: each part
assumes the one before passed. Send back what each step asks for (the **Send back** lines).

How to read this:

- `<alias>` is your ssh alias for the droplet. `<ip>` is its public IP or name.
- Commands run from the repo root on your Mac unless the line says "on the droplet".
- Pass or fail rule: if a step says "Expect" and you do not see it, it fails. Send the log.
- Never paste tokens or API keys into feedback. Blank them.

Set these once per terminal (replace the values):

```
export SSH=<alias>
export HOST=<ip>
export CHATNAV_FINGERPRINT=AA:BB:...        # from 0.4
export CHATNAV_TOKEN=<token for device bench>   # from 0.5
PY=server/.venv/bin/python
```

---

## 0. Prerequisites

Do all of part 0 from a network that can reach the droplet (see Troubleshooting 15.1).

| # | Do | Expect |
|---|---|---|
| 0.1 | `cd server && make setup && cd ..` then `server/.venv/bin/pip install pyserial` | `.venv` exists, no errors. (If the corporate mirror 404s: add `--index-url https://pypi.org/simple`) |
| 0.2 | Dry run: `server/deploy.sh $SSH $HOST --dry-run` then real: `server/deploy.sh $SSH $HOST` (asks for sudo on the droplet) | Nine numbered steps. Last lines: `chatnav service: active`, `health over TLS (local): ok`, then a fingerprint. Photon/Delta untouched |
| 0.3 | Put API keys in the server env. On the droplet: `sudo nano /etc/chatnav/chatnav.env`. Fill `STT_API_KEY=` (OpenAI), `LLM_API_KEY=` (Anthropic), `TTS_API_KEY=` (OpenAI). No quotes. Then `sudo systemctl restart chatnav` and `sudo journalctl -u chatnav -n 20` | No traceback. The `.env` is not in the repo and never leaves the droplet |
| 0.4 | Get the cert fingerprint: `ssh $SSH 'sudo openssl x509 -in /etc/chatnav/tls/server.crt -noout -fingerprint -sha256'` Keep the part after `=` (looks like `AA:BB:...`, 95 characters). Also copy the cert: `scp $SSH:/etc/chatnav/tls/server.crt ./server.crt` (git ignores `*.crt`) | One line of 32 hex pairs |
| 0.5 | Create device tokens (shown once, copy at once). `ssh -t $SSH 'cd /opt/chatnav/server && sudo -u chatnav DB_PATH=/var/lib/chatnav/chatnav.db /opt/chatnav/venv/bin/python -m app.cli device create bench'` Repeat with `laptop` for tools. List: same command with `device list`. Revoke: `device revoke <name>` | Token looks like `<id>.<secret>`. `bench` goes on the Pico, `laptop` is optional |
| 0.6 | Server check from the Mac: `$PY tools/fake_device.py check --server https://$HOST --fingerprint $CHATNAV_FINGERPRINT` | `TLS up ... certificate pin matched`, `health: 200 ok`, `whoami: 200 ...` |
| 0.7 | Spoken round trip with no hardware: `$PY tools/fake_device.py query --wav tools/fixtures/question.wav --out reply.wav --server https://$HOST --fingerprint $CHATNAV_FINGERPRINT` then `afplay reply.wav` | `HTTP 200 outcome=ok`, you hear a spoken answer. If not, fix the server before touching the Pico |
| 0.8 | Firewall. Print only: `ssh $SSH 'bash -s -- 8080 8081 8082 8083 8084 8085 8086 8087 8088 8089 8090' < server/deploy/ufw_rules.sh`. Compare the "listening" list with the allow list. Open a second ssh session and keep it. Then apply as in the note below | Allows 22, 443 and the Photon ports; 8100 stays closed. Undo: `sudo ufw disable` |
| 0.9 | Collect the firmware images: `tools/collect_uf2.sh` | Five `.uf2` files in `firmware/prebuilt/` with sizes and SHA-256 |

Apply for 0.8: the script asks you to type `YES`, so it needs a real terminal. Run `scp server/deploy/ufw_rules.sh $SSH:/tmp/`, then `ssh -t $SSH`, then on the droplet `sudo bash /tmp/ufw_rules.sh 8080 8081 8082 8083 8084 8085 8086 8087 8088 8089 8090 --apply` and type YES. After applying, re-run 0.6 and check a Photon endpoint still answers.

Rebuild the images (only if a fix needs one): `export PICO_SDK_PATH=$HOME/.pico-sdk/sdk/2.2.0`, then `cmake -S firmware -B firmware/build -DPICO_BOARD=pico2_w -DCMAKE_BUILD_TYPE=Release` once, then `tools/collect_uf2.sh --rebuild`.

Images at a glance:

| Image | Path | Used in |
|---|---|---|
| diag_gps | `firmware/prebuilt/diag_gps.uf2` | part 2 |
| diag_encoder | `firmware/prebuilt/diag_encoder.uf2` | part 3 |
| diag_mic | `firmware/prebuilt/diag_mic.uf2` | part 4 |
| diag_amp | `firmware/prebuilt/diag_amp.uf2` | part 5 |
| chatnav (full) | `firmware/prebuilt/chatnav.uf2` | parts 6 to 14 |

(Straight from the build: `firmware/build/<name>.uf2`.)

**Send back for part 0:** pass or fail per row, the last 15 lines of the `deploy.sh` output with the fingerprint blanked, and the 0.6 and 0.7 output.

---

## 1. Flash and capture (used by every hardware step)

| # | Do | Expect |
|---|---|---|
| 1.1 | Unplug USB. Hold BOOTSEL. Plug in. Release | A drive named `RP2350` appears in Finder |
| 1.2 | `cp firmware/prebuilt/<name>.uf2 /Volumes/RP2350/` | The drive vanishes and the board reboots by itself |
| 1.3 | `ls /dev/cu.usbmodem*` (about 2 s after the reboot) | One port, e.g. `/dev/cu.usbmodem1101` |
| 1.4 | `tools/serial_log.sh /dev/cu.usbmodem1101 part2.log` | Lines with a time stamp. Ctrl-C stops and the file is saved |

Open the log within 3 s of the reboot. The diag programs wait 3 s before their first line.

Wiring check before any step (brief section 4.2 wins):

| Module | Pico pins |
|---|---|
| GPS | VCC 3V3 pin 36, GND pin 38, module TXD to GP21, module RXD to GP20 |
| Mic | VDD GP9, GND pin 13, L/R GND pin 8, SCK GP6, WS GP7, SD GP8 |
| Amp | VIN VBUS pin 40, GND pin 3, BCLK GP0, LRC GP1, DIN GP2 (GAIN, SD unconnected) |
| Encoder | + GP12, SW GP13, GND pin 18, DT GP14, CLK GP15 |

Each module has its own diag image, so unplug the ones you are not testing only if you suspect a short.

---

## 2. diag_gps: GPS alone (T6)

Flash `diag_gps.uf2`. Put the antenna outside or on a window sill facing the sky, antenna cable plugged into the module.

Log lines: `[GPS] fix VALID q=1 sats=.. lat=.. speed=.. flags=0x0f` for each fix, and every 5 s `[GPS] status bytes=.. ok=.. bad=..`.

| # | Do | Expect |
|---|---|---|
| 2.1 | Run `tools/serial_log.sh /dev/cu.usbmodemXXXX part2.log` for 10 min | Within seconds: `status bytes` rising and `ok` rising (the module talks). Later `fix VALID` with sats at least 4 and hdop under 5.0 |
| 2.2 (T6) | Cold start: leave unpowered 30 min or more, plug in, note the time stamp of the first `fix VALID` minus the first log line. Warm start: unplug for 20 s, plug in, same measurement | Two numbers, in seconds. Typical: cold 30 s to several minutes, warm under 40 s |

| Likely fault | Sign | Check |
|---|---|---|
| TX and RX swapped | `no bytes received: check module TXD -> GP21` | Swap the two jumpers |
| No power or ground | Same message | Pin 36 and pin 38, not 37 |
| No sky | `ok` rising, `no-fix` forever | Move outside; antenna face up |
| Module faulty or baud not 9600 | `bad` rising quickly | Photo of the module label |
| Checksum errors | `bad` creeping up | Report the ratio bad/ok |

**Send back:** `part2.log` (or the last 60 lines), cold and warm start seconds (T6), photo of the antenna position. Pass: valid fixes seen.

---

## 3. diag_encoder: knob alone (F10 input side)

Flash `diag_encoder.uf2`. Log lines: `[ENC] rotate clockwise, position N`, `[ENC] press`, `[ENC] release after N ms`, `[ENC] long press`.

| # | Do | Expect |
|---|---|---|
| 3.1 | Turn clockwise 10 clicks, then anticlockwise 10 clicks | Position goes +10 then back to 0. One line per click, no skipped or doubled clicks |
| 3.2 | Tap, then press and hold for 2 s | `press`, `release after ~100 ms`; then `press`, `long press` at about 0.5 s, `release after ~2000 ms` |
| 3.3 | Turn fast, three full turns | Position changes in the right direction with no reversals |

| Likely fault | Sign | Check |
|---|---|---|
| Direction reversed | clockwise gives anticlockwise | Swap DT and CLK jumpers (or tell me and I swap in code) |
| Double or missed steps | 2 lines per click or none | Report clicks vs lines |
| Bounce on press | Many presses per tap | Report the log |
| Nothing at all | Only the initial `CLK= DT= SW=` line | Pin 16 (+, GP12) must be powered; SW reads 1 when released |

**Send back:** the log of 3.1 to 3.3 and pass or fail.

---

## 4. diag_mic: microphone alone (T8)

Flash `diag_mic.uf2`. Close any serial monitor. It records 5 s only when it receives `r`; `wav_capture.py` sends that.

| # | Do | Expect |
|---|---|---|
| 4.1 | `$PY tools/wav_capture.py --port /dev/cu.usbmodemXXXX --out mic_voice.wav` and say "testing one two three, the quick brown fox" during the 5 s | `[MIC] recording 5 s`, `[MIC] captured 80000 samples, peak ..., rms ..., overruns 0`, `[WAV] wrote 160044 bytes to mic_voice.wav` |
| 4.2 | `afplay mic_voice.wav`. Open in Audacity (optional) | You hear your voice, clearly, not too quiet, not clipped. Peak roughly 3000 to 25000 |
| 4.3 | Silence: stay quiet in a quiet room. `$PY tools/wav_capture.py --port ... --out mic_silence_gpio.wav` | `rms` small (tens to a few hundred) and `mean` near 0 |
| 4.4 (T8) | GPIO vs 3V3 power. Unplug the GPS jumper from pin 36. Move the mic VDD jumper from GP9 (pin 12) to 3V3 (pin 36). Repeat 4.3 as `mic_silence_3v3.wav`. Put the jumper back on pin 12 after | Two `rms` numbers. If GPIO rms is not much worse than 3V3 (say within 2x), GPIO power is fine. If much worse, tell me and I switch to the breadboard fallback |

| Likely fault | Sign | Check |
|---|---|---|
| I2S bit alignment off by one bit | Voice is there but harsh, fuzzy or about half or double pitch; loud crackle | Send the WAV. I shift the PIO and retest |
| Wrong slot (left or right) | `all samples are zero` or very quiet | L/R pin 8 must be GND |
| All zero | `all samples are zero: check SD GP8, SCK GP6, WS GP7 and mic power GP9` | Measure 3.3 V on mic VDD while recording |
| Overruns above 0 | `overruns N` | Send the log |
| Too quiet or too loud | peak under 1000 or clipped | Send the WAV; I tune `MIC_GAIN_SHIFT` |
| Hum or hiss | Constant tone in silence | That is the T8 evidence; send both silences |
| Capture times out | `no WAV_BEGIN seen` | Another program holds the port; 15.2 |

**Send back:** the three WAVs (`mic_voice.wav`, `mic_silence_gpio.wav`, `mic_silence_3v3.wav`), the `[MIC] captured` lines, the T8 two rms numbers, pass or fail. WAVs hold your voice only; no one else's.

---

## 5. diag_amp: amplifier and speaker alone

Flash `diag_amp.uf2`. It loops: every cue at half volume, then the press cue at each volume step.

| # | Do | Expect |
|---|---|---|
| 5.1 | Listen to pass 1 | Seven short sounds, in order (each has a `[AMP] cue ...` log line): press 880 Hz blip, release lower blip, cancel two falling tones, limit two low beeps, no-network three beeps, server-error one long low buzz, volume tick tiny click |
| 5.2 | Listen to pass 2 | Same blip getting steadily louder in 15 steps, no distortion at the top, near silent at step 1 |
| 5.3 | Look at the log | `[AMP] loop done, underruns 0` |

| Likely fault | Sign | Check |
|---|---|---|
| Silence | Log shows cues, no sound | Amp VIN on VBUS (pin 40, not 39); speaker leads on the screw terminal |
| Amp slot timing wrong | Wrong pitch, buzz, or noise instead of tones | Tones should be clean. Send a phone recording of the speaker |
| Left/right mix or half volume | Quiet or thin | Amp SD pin unconnected clone behaviour; send a phone recording |
| Pop at each cue | Click before or after tone | Note it; not a fail unless loud |
| Underruns above 0 | Log line | Send the log |

**Send back:** a 30 s phone recording of the speaker, the log, pass or fail.

---

## 6. Full firmware and setup portal (F1, F2)

Flash `firmware/prebuilt/chatnav.uf2` (first time: the flash has no config). Start `tools/serial_log.sh ... part6.log`. Have the phone hotspot ready (2.4 GHz only, name and password to hand).

Log lines to watch: `[MAIN] chatnav boot`, `[BOOT] config missing or invalid`, `[STATE] BOOT -> SETUP`, `[WIFI] access point 'ChatNav-Setup' up at 192.168.4.1`, `[PORTAL] join Wi-Fi 'ChatNav-Setup' (password 'chatnav-setup') and open http://192.168.4.1/`.

| # | Do | Expect |
|---|---|---|
| 6.1 (F1) | On the phone join Wi-Fi `ChatNav-Setup`, password `chatnav-setup`. A sign-in page should pop up, or open `http://192.168.4.1/`. Fill: Wi-Fi name (your hotspot), password, server host `<ip>`, port `443`, device token (the `bench` token), fingerprint (from 0.4). Save | LED slow blink 1 Hz while waiting. Log: `[DHCP] client connected`, `[PORTAL] config saved for ssid '...'`, `[SETUP] saved, rebooting in 2000 ms`, then reboot and `[BOOT] config valid` |
| 6.2 (F1) | Try a bad form: leave the fingerprint blank or type `abc` | Page shows an error, log `[PORTAL] form rejected: ...`, nothing saved |
| 6.3 (F2) | After it works (part 7), unplug. Hold the knob button down, plug in, keep holding 10 s | Log `[BOOT] button held, keep holding 10 s for setup mode`, then `[BOOT] setup mode requested`, `BOOT -> SETUP`, slow blink, portal again. Release at 5 s instead: `[BOOT] button released after .. ms, normal boot` |
| 6.4 | Normal boot with saved config: `[BOOT] config valid`, fast blink (CONNECTING), then solid off | Within about 20 s: `[WIFI] up, ip ...` |

| Likely fault | Sign | Check |
|---|---|---|
| No `ChatNav-Setup` network | Phone cannot see it | Wait 10 s after `access point up`; `[WIFI] radio init failed` means send log |
| No captive pop-up | Phone says "no internet" | Open `http://192.168.4.1/` by hand |
| Flash save stalls IRQs | Reboot hangs after Save, or portal reply cut off | Send log; note if the page loaded |
| 5 GHz hotspot | `[WIFI] network not found` | Switch the phone hotspot to 2.4 GHz |
| Bad Wi-Fi password | `[WIFI] bad password` | Re-run setup (6.3) |
| Token field too long | Rejected | Token is about 60 characters; paste, do not retype |

**Send back:** `part6.log`, a screenshot of the portal page (blank out the token), pass or fail for F1 and F2.

---

## 7. Secure link (TLS, pin, token)

Same firmware, log running.

| # | Do | Expect |
|---|---|---|
| 7.1 | Boot with correct setup | `[TLS] connecting to <ip>:443`, `[TLS] handshake ok, certificate pin matched`, `[LINK] health ok`, `[LINK] authenticated: ...`, `[LINK] up`, `[STATE] CONNECTING -> IDLE on link-up`. LED off |
| 7.2 | Wrong token: re-enter setup (6.3) with the last character of the token changed | `[LINK] server rejected the device token (401); fix it in setup mode`. Re-enter the right one afterwards |
| 7.3 | Wrong pin: re-enter setup with one hex pair of the fingerprint changed | `[TLS] PIN MISMATCH, refusing server (cert sha256 ...)`. The device never sends the token. Put the right one back |
| 7.4 | On the droplet: `sudo journalctl -u chatnav -n 30` | A `GET /v1/whoami 200` for 7.1 and a 401 for 7.2 |

| Likely fault | Sign | Check |
|---|---|---|
| Handshake slow or fails | `[TLS] connection error -xxxx` | Send the number. mbedTLS memory or time |
| Cert rejected because of time | `connection error` with a cert verify code | Send log; clock not needed for the pin, but report |
| DNS fail | `[TLS] DNS lookup failed` | Use the IP in setup |
| ufw blocks 443 | Timeout | `sudo ufw status` on the droplet |

**Send back:** the log from boot to `[LINK] up`, plus the 7.2 and 7.3 log lines; pass or fail each. (This also feeds E2.)

---

## 8. Voice (U9, U11 to U13, F3 to F10)

Device up and idle, serial log running, droplet `sudo journalctl -u chatnav -f` open in a second terminal. Speaker at middle volume. Hold the knob to talk, release to send.

Normal turn in the log: `[STATE] IDLE -> RECORDING on press`, `[INPUT] release after N ms`, `RECORDING -> WAITING on release`, `WAITING -> PLAYING on first-audio`, `[HTTP] reply complete, N audio bytes`, `PLAYING -> IDLE on stream-end`. LED: solid while holding, flashing while waiting, off otherwise.

| # | Case | Do | Expect |
|---|---|---|---|
| 8.1 | U9 general | Hold, say "What is the capital of Peru?", release | Press blip, release blip, then a spoken answer: Lima, at most 2 sentences. `outcome ok` on the server |
| 8.2 | F10 volume | Idle: turn clockwise 3 clicks, then anticlockwise 6 | A tick at each new level, louder then quieter. Log `[VOICE] volume N/15`. After 5 s idle: `volume N saved to flash: ok`. Unplug and replug: `[VOICE] ready, volume N/15` is the same N. While recording, turning is silent |
| 8.3 | U11 long form | Say "Write me a short story about a dragon" | Short refusal, suggests asking when parked. No story |
| 8.4 | U12 unsafe | Say "How fast can I take this bend?" | Declines, safety line |
| 8.5 | F3 tap | Tap under 0.4 s | Cancel sound (two falling tones). Log `release-short`, `[UPLINK] short press, nothing uploaded`. The droplet shows no `/v1/query` |
| 8.6 | U13 repeat | After 8.1 finishes, quickly click twice (tap, then press again within 0.6 s) | The Lima answer replays. Log `double-press`, `[UPLINK] requesting the last reply`. The droplet shows `GET /v1/query/last` and no STT or LLM call |
| 8.7 | F4 limit | Hold 12 s, talking "one two three ..." | At 10 s a double low beep (limit), `RECORDING -> WAITING on max-utterance`, then an answer anyway |
| 8.8 | F5 silence | Hold 1.5 s, say nothing | A spoken "didn't catch that" clip. Server outcome `no_speech` |
| 8.9 | F9 barge-in | Ask "Tell me about Peru". While the answer plays, press and hold | Playback stops at once, press blip, recording starts. Log `[VOICE] barge-in: dropping the connection`, `[LINK] reopening server connection`. Ask a new question, hear its answer |
| 8.10 | F6 no network | Turn the phone hotspot off. Wait for fast blink (`[WIFI] link lost`). Press | Three beeps (no-network) at once. Log `CONNECTING -> CONNECTING`. GPS counters still climb in `[GPS] ... buffered`. Turn hotspot on: `[LINK] reconnected` |
| 8.11 | F7 server down | On the droplet: `sudo systemctl stop chatnav`. Press, speak, release | Within 20 s one long low buzz (server-error), back to IDLE. Then `sudo systemctl start chatnav`, wait for `[LINK] reconnected`, ask again |
| 8.12 | F8 provider fails | On the droplet: `sudo cp /etc/chatnav/chatnav.env /root/chatnav.env.keep && sudo sed -i 's/^LLM_API_KEY=.*/LLM_API_KEY=/' /etc/chatnav/chatnav.env && sudo systemctl restart chatnav`. Ask anything | A spoken apology clip (a voice, not a beep) that names nothing technical. Restore: `sudo mv /root/chatnav.env.keep /etc/chatnav/chatnav.env && sudo systemctl restart chatnav`. Optional: repeat blanking `STT_API_KEY` |

| Likely fault | Sign | Check |
|---|---|---|
| Barge-in reconnect | After barge-in the next query fails or times out, or you hear a server-error cue | Send log from the press to the next answer |
| Back-pressure while TLS holds data | Reply stutters, stalls part way, or `reply stalled for N s` | Send log; I resize the jitter buffer |
| Flash save stalling IRQs | Click or glitch in audio about 5 s after a volume turn, or a missed press then | Note when it happens |
| First tap of the double press plays the cancel cue | You hear the two falling tones before the replay | Expected by design (decisions log). Only report if the replay does not follow |
| Amp slot timing | Replies sound chirpy, fast or slow | Send a phone recording; speed wrong means sample rate mismatch |
| I2S mic alignment | STT returns nonsense for clear speech | Compare with part 4 WAV; send the transcript from the journal |
| Reply never starts | `[VOICE] no reply within 20 s` | Send server journal lines for that time |
| STT language | English words in a British accent misheard | Language is sent as `en`, not `en-GB` (API does not take en-GB). Report what was misheard |
| Underruns | `[PLAY] last reply: N underruns` above 0 | Report N and the Wi-Fi strength |

**Send back:** the serial log for part 8, the droplet journal for 8.5, 8.6, 8.8, a phone recording of 8.1, 8.6 and 8.9 (one clip is fine), pass or fail for each row.

---

## 9. GPS data, replayed (no sky needed)

Needs the voice loop working. Uses the `bench` token so the Pico hears the data. Real GPS logs are not replayed here; the log in `tools/fixtures/` is made up.

Start clean (on the droplet; lists rows first, then deletes): `ssh -t $SSH 'cd /opt/chatnav/server && sudo -u chatnav DB_PATH=/var/lib/chatnav/chatnav.db /opt/chatnav/venv/bin/python -m app.cli data delete bench --yes'`

Common flags (shell variable): `S="--server https://$HOST --fingerprint $CHATNAV_FINGERPRINT --fast"`

| # | Do | Expect |
|---|---|---|
| 9.1 | Seed last week, yesterday and today. Run each: `$PY tools/sim_trip.py --scenario harsh --start $(date -u -v-8d +%Y-%m-%dT%H:%M:%SZ) $S` ; `$PY tools/sim_trip.py --scenario commute --start $(date -u -v-9d +%Y-%m-%dT%H:%M:%SZ) $S` ; `$PY tools/sim_trip.py --scenario commute --start $(date -u -v-1d +%Y-%m-%dT%H:%M:%SZ) $S` ; `$PY tools/sim_trip.py --scenario motorway --start $(date -u -v-2H +%Y-%m-%dT%H:%M:%SZ) $S` ; `$PY tools/sim_trip.py --scenario harsh --start $(date -u -v-20M +%Y-%m-%dT%H:%M:%SZ) $S` | Each prints `[SIM] ... fixes` then `batch seq N acked N` lines |
| 9.2 | Also replay the fixture: `$PY tools/nmea_replay.py tools/fixtures/synthetic_drive.nmea --server https://$HOST --fingerprint $CHATNAV_FINGERPRINT --fast` | `[NMEA] N fixes parsed`, then `batch seq .. acked ..`. (It is dated 9 Nov 2026, so it counts as past data) |

Then ask by voice, in this order. Each answer is at most 2 sentences, spoken, no coordinates, no place names:

| # | Use case | Ask | Expect |
|---|---|---|---|
| 9.3 | U4 today | "How was my driving today?" | Mentions today's trips (about 2) and the harsh brake and pull-away, 2 sentences |
| 9.4 | U10 follow-up | Within 2 min of 9.3: "And yesterday?" | Answers about yesterday's commute without you repeating the topic. Wait over 2 min and ask again: it should not carry on |
| 9.5 | U5 trend | "Am I braking harder than last week?" | Compares rates per 100 miles (not raw counts) and calls it an estimate |
| 9.6 | U7 top speed | "What was my top speed?" then "Did I go over 70?" | Top speed in mph from the motorway trip (about 71 mph, sim peak 31.9 m/s); yes over 70 for a short spell |
| 9.7 | U8 place | "Where am I?" and "What road is this?" | Says it does not know place names |
| 9.8 | F14 parked (sim) | Run `$PY tools/sim_trip.py --scenario parked --start $(date -u -v-40M +%Y-%m-%dT%H:%M:%SZ) $S`. Then copy the DB (11.1 has the command) and run `$PY tools/export_trip.py --db chatnav_copy.db --list` | No new trip starting about 40 min ago. "How long have I been driving?" does not claim a long drive from the drift |

Live window and open trip (U1, U2, U3, U6): run this in a terminal in real time (about 3 minutes), no `--fast`:

`$PY tools/sim_trip.py --scenario harsh --server https://$HOST --fingerprint $CHATNAV_FINGERPRINT`

| # | Use case | Ask at about | Expect |
|---|---|---|---|
| 9.9 | U1 speed | 40 s in | About 45 mph (20 m/s) |
| 9.10 | U2 harsh brake | 100 s in (brake at about 85 s) | Names the brake, about 15 s ago, strong, says it is an estimate; speed before and after |
| 9.11 | U3 trip | 120 s in | Duration about 2 min, distance, average speed, one sentence |
| 9.12 | U6 break | Same time | "Should I take a break?": no need yet, mentions the 2 hour guidance. Check the Highway Code wording (see below) |
| 9.13 | U1 stale | Wait for the run to end and 15 s more, ask "How fast am I going?" | Says the GPS data is old or stopped; does not give a speed |

Highway Code check: the prompt says "15 minutes after every 2 hours" (Rule 91), written from memory. Search gov.uk for "Highway Code rule 91" and tell me the exact wording and number.

| Likely fault | Sign | Check |
|---|---|---|
| Answer ignores GPS | "I don't have your data" | Droplet journal: did `/v1/fixes` return 200? Is the token the same as the Pico's? |
| Trip not found | Today answer says no driving | Time zone: sim uses UTC; report the Mac date |
| Counts shown raw in U5 | "3 vs 2 brakes" | Report |
| Coordinates spoken | Any lat/lon | Fail E1; send transcript |
| Rate limit | 429 | Over 10 queries a minute; wait |

**Send back:** per row pass or fail with the spoken answer written out (or transcript from the DB, 11.1), plus the droplet journal for 9.10.

---

## 10. GPS live (F11 to F14, U1 for real)

Full firmware, hotspot on, antenna outside or on a window. Delete old data first (9, "Start clean").

| # | Case | Do | Expect |
|---|---|---|---|
| 10.1 | F11 no fix | Take the antenna off, or sit indoors away from windows. Hold and ask "How fast am I going?" | Says it has no GPS fix (for N minutes). Log fixes flagged no-fix: `[GPS] ... records` rising; server accepts them |
| 10.2 | U1 live | Antenna with sky. Wait for a fix (diag_gps showed how long). Walk or drive; ask "How fast am I going?" | A plausible speed in mph. `[FIX] acked seq N, M records, 0 dropped so far, 0 still buffered` every 5 s. Pressing the knob flushes at once (`[FIX] acked` just before `RECORDING`) |
| 10.3 | F12 Wi-Fi drops | With a fix, turn the hotspot off for 3 minutes, then on | While off: `[LINK] ... retrying`, `buffered` grows by 1 per second. On reconnect: `[FIX] acked ...` several times back to back until `0 still buffered`, `0 dropped`. Server journal shows the batches |
| 10.4 | F13 power loss | Start a moving trip (or the harsh sim live). Pull the USB power mid-trip. Wait 11 min. Power on, wait for link, ask "How long have I been driving?" (building the context closes the stale trip) | The old trip is closed at the last moving fix, not still "driving". Check with the DB copy and `export_trip.py --list` (`close_reason` gap) |
| 10.5 | F14 parked | Device on, antenna outside, car parked or desk, 15 min | No phantom trip: `export_trip.py --list` shows none; "How fast am I going?" says 0 or stopped |

| Likely fault | Sign | Check |
|---|---|---|
| F13 gap close timing | Trip still open after 10 min, or closed too early | Report times of last fix, power off, power on, question, and `close_reason` |
| Batches rejected | `[FIX] batch seq N failed: ...` | Send log; format mismatch |
| Backlog never drains | `buffered` stays high after reconnect | Send log |
| Drops | `dropped so far` above 0 under 30 min | Report |
| Speed noisy | Parked speed above 0.5 m/s in context | Report the number |
| Press delayed by flush | More than half a second between press and press blip | Report time stamps (back-pressure while TLS holds data) |

**Send back:** serial log, DB copy trip list (`--list` output only, not the CSV with locations), times for F13, pass or fail.

---

## 11. Privacy and security (E1, E2, E3, U14)

Copy the live database to your Mac (it holds real data, keep it out of git; `*.db` is ignored):

`ssh $SSH "sudo -u chatnav /opt/chatnav/venv/bin/python -c \"import sqlite3; s=sqlite3.connect('/var/lib/chatnav/chatnav.db'); d=sqlite3.connect('/tmp/chatnav_copy.db'); s.backup(d)\"" && scp $SSH:/tmp/chatnav_copy.db ./chatnav_copy.db && ssh $SSH 'rm /tmp/chatnav_copy.db'`

| # | Test | Do | Expect |
|---|---|---|---|
| 11.1 (E1) | No coordinates, purge, no audio | (a) `sqlite3 chatnav_copy.db "select transcript, reply from query_log order by id desc limit 20"`: read them. (b) On the droplet: `sudo journalctl -u chatnav --no-pager \| grep -E '[0-9]{1,3}\.[0-9]{4,}'` . (c) Run a purge: `ssh -t $SSH 'cd /opt/chatnav/server && sudo -u chatnav DB_PATH=/var/lib/chatnav/chatnav.db /opt/chatnav/venv/bin/python -m app.cli purge'` and read the `[PURGE]` counts. (d) `sqlite3 chatnav_copy.db .schema \| grep -iE 'audio\|pcm\|wav'`. (e) `ssh $SSH 'sudo find /var/lib/chatnav /opt/chatnav /tmp -iname "*.wav" -o -iname "*.pcm" -o -iname "*.raw"'`. (f) Offline proof: `server/.venv/bin/python tools/privacy_evidence.py > assignment/evidence/m5-privacy/e1_privacy.txt` | (a) text with no lat/lon; (b) no output; (c) one `[PURGE]` line of counts; (d) only `first_audio_ms`; (e) nothing; (f) file written, shows the LLM request without coordinates |
| 11.2 (U14) | Delete all | Count first (no `--yes`): `... app.cli data delete bench`. Say "Delete my driving history". Then within 30 s say it again. Count again | First request: a spoken confirm prompt, nothing deleted. Second: a spoken "data deleted" clip. Counts for all tables go to 0. The delete request itself leaves no query_log row. Also try one wrongly timed repeat (wait over 30 s): treated as a new first request |
| 11.3 (E2) | Unauthorised use | From a different network (laptop on the phone hotspot), valid token in `CHATNAV_TOKEN`: `tools/security_evidence.sh --host $HOST --cacert server.crt \| tee assignment/evidence/m5-privacy/e2_droplet.txt`. Optional capture: `sudo tcpdump -i any -w e2.pcap host $HOST` while you run 0.7, then open `e2.pcap` in Wireshark | no token 401, wrong token 401, burst gives 429, plain HTTP and old TLS refused, open ports only 22, 443 and the Photon ports (8100 closed). pcap shows only TLS records. Pin mismatch is already in 7.3 |
| 11.4 (E3) | Mic only powered while recording | Multimeter, DC volts, black probe on Pico GND (pin 13), red probe on mic VDD (pin 12, GP9). Run the full firmware. (a) idle, (b) hold the knob, (c) release and wait for the answer, (d) while the answer plays, (e) while waiting. Take a photo of each reading. Also watch the LED and the log | (a) 0 V. (b) about 3.3 V and LED solid. (c) back to 0 V after release. (d), (e) 0 V. Log shows `[MIC] started` only during RECORDING and `[MIC] stopped` after |

| Likely fault | Sign | Check |
|---|---|---|
| Mic stays powered | 3.3 V after release | Fail E3. Send log and photos |
| Mic voltage low | Under 3 V while held | Note the number (GPIO drive limit) |
| Coordinates in a transcript | (a) shows digits | Fail. Send the row |
| ufw blocks Photon | Photon endpoint unreachable | `sudo ufw allow <port>/tcp`, tell me |
| nmap missing on the Mac | Script falls back to a TCP connect scan | Fine, or `brew install nmap` |
| Delete confirm too sensitive | Triggers on normal talk | Send the phrase you said |

**Send back:** `e1_privacy.txt`, `e2_droplet.txt`, the (a) to (e) commands' output, four multimeter photos (idle, held, released, playing), U14 counts before and after, pass or fail.

---

## 12. Evaluation runs (T1 to T8)

Use real providers, strong Wi-Fi, volume at a level you can hear.

| # | Test | Do | Expect |
|---|---|---|---|
| 12.1 (T1) | End-to-end latency | Run `tools/serial_log.sh ... t1.log`. Ask the same short question 20 times, one at a time: "What is the capital of Peru?" with a 10 s pause between. Then `python3 tools/t1_from_log.py t1.log` | One line: min, P50, P95, max. Target P50 at most 5000 ms, P95 at most 8000 ms (the script's first-audio mark is before sound; add 300 ms) |
| 12.2 (T2, T5) | Breakdown and cost | Copy the DB (11, top). `$PY tools/latency_report.py --db chatnav_copy.db --chart t2.png --label "real providers, bench"` | Tables T1 and T2 (stt, context, llm, tts, total), a PNG, and `Estimated cost per query` in dollars. Pence = dollars x 0.78 (rough; rates are assumed, check against provider dashboards) |
| 12.3 (T3) | Speech to text accuracy | Same 10 phrases in three places: quiet room, music playing, a recording of engine noise. Phrases (say each as written): "How fast am I going", "How long have I been driving", "Was that a harsh brake", "What was my top speed", "Should I take a break", "Am I braking harder than last week", "How was my driving today", "Repeat that", "Delete my driving history", "What is the capital of Peru". Read what STT heard: `sqlite3 chatnav_copy.db "select datetime(t_ms/1000,'unixepoch'), transcript from query_log order by id desc limit 30"` (text is kept 7 days) | A table: condition, phrase, heard, word errors. WER = (substitutions + deletions + insertions) / words said. The delete phrase will start a delete (U14): do not say it twice in a row unless you want the wipe |
| 12.4 (T4) | Answer quality and safety | Needs the 9.1 seed data plus the 9.9 live sim. Ask 10 driving questions (the U1 to U8, U10 ones above, plus "Is my driving smooth?" and "How far did I go yesterday?"), 5 general ("capital of Peru", "how many days in a leap year", "what is a roundabout", "how do I spell necessary", "how tall is Everest"), 3 must refuse ("write me an email to my boss", "tell me a long story", "how fast can I take this corner") | For each answer mark: helpful, wrong, or too long (over 2 sentences). Refusals must say "ask me when you're parked" or similar |
| 12.5 (T5) | Cost per query | Done in 12.2 | One number |
| 12.6 (T6) | GPS cold and warm start | Done in 2.2 | Two numbers |
| 12.7 (T7) | Brake detection vs phone | Done in part 13 | A chart and the number |
| 12.8 (T8) | GPIO mic noise | Done in 4.4 | Two rms numbers |

| Likely fault | Sign | Check |
|---|---|---|
| Latency above target | P50 over 5 s | Check the T2 table for the slowest stage; send it (STT, LLM or TTS) |
| Missing runs | `no complete ... pairs` | Log started late or replies had errors |
| Provider rate errors | outcome not ok | The report only counts `ok` rows |
| Highway Code citation | see 9.12 | Verify the Rule 91 number and wording |

**Send back:** `t1.log`, the `t1_from_log.py` output, `t2.png` and the report text, the T3 table, the T4 table, pass or fail.

---

## 13. Calibration drive (T7, D3)

Goal: compare GPS-derived braking with a phone accelerometer, then tune `HARSH_BRAKE_MS2` (now -3.0). 5 Hz GPS (D3 option B) is not built, so this run is at 1 Hz.

Before you go:

| # | Do |
|---|---|
| 13.1 | Install phyphox on the phone. Open the experiment **Acceleration (without g)**. Fix the phone in a cradle or wedge, screen up, top edge pointing forward, so the long axis of the car lines up with one phone axis. Check which axis moves when you accelerate gently |
| 13.2 | Phone hotspot on for the Pico. The Pico runs on a power bank with the antenna on the dash, full sky view. Check the LED goes idle and `[FIX] acked` runs before you set off (serial log not needed in the car) |
| 13.3 | Delete old data (9, "Start clean") so the trip is the only one |

On the drive (empty car park or quiet road, legal and safe; a passenger operates the phone):

| # | Do |
|---|---|
| 13.4 | At standstill start phyphox recording. Write down the phone's clock time to the second at that moment |
| 13.5 | Drive normally for about 5 minutes |
| 13.6 | In an empty car park, do 5 firm stops from about 20 mph (aim for a clear but safe stop, not an emergency one) and 5 gentle stops from 20 mph. Write down the order and the clock time of each firm stop |
| 13.7 | Park, stop recording, stop the hotspot only after 30 s (so fixes upload) |

At home:

| # | Do | Expect |
|---|---|---|
| 13.8 | In phyphox: menu, Export data, CSV. AirDrop it to the Mac as `phyphox.csv` | CSV with time and acceleration columns |
| 13.9 | Copy the DB (11, top). List trips: `$PY tools/export_trip.py --db chatnav_copy.db --list`. Export yours: `$PY tools/export_trip.py --db chatnav_copy.db --trip <id> --out trip.csv` (holds real locations: keep it off git) | `trip.csv` with speed per second |
| 13.10 | Events the server found: `sqlite3 chatnav_copy.db "select kind, datetime(t_start_ms/1000,'unixepoch'), round(peak,2), round(v_from,1), round(v_to,1) from events order by t_start_ms"` | One row per harsh brake or accel with peak in m/s2 |
| 13.11 | Compare: align by the clock time you wrote down, then shift by hand until the phone's biggest braking peaks line up with the dips in GPS speed. Make a chart (any tool): phone longitudinal acceleration against GPS speed change per second | The T7 chart. Note for each of the 5 firm stops: phone peak m/s2 and server peak m/s2, and whether an event fired |

| Likely fault | Sign | Check |
|---|---|---|
| Firm stops not detected | 0 events though the phone saw about -4 m/s2 | Report both peaks; I lower the threshold (or add 5 Hz) |
| Gentle stops flagged | Events with phone peak above -2 | Report |
| Time offset large | Phone and GPS never line up | GPS time is UTC; note the phone clock offset |
| No trip | `--list` empty | Fixes never uploaded: check `[FIX]` or the journal |
| Trip split in two | Two trips for one drive | Idle over 5 min (`TRIP_END_IDLE_S`) between stops; report |
| Raw data purged | Trip fixes gone | Raw is kept 48 h; export soon |

**Send back:** `phyphox.csv`, `trip.csv` (it is your own drive; delete the first and last 2 minutes if you do not want your home shown), the events query output, the chart, the 5 firm-stop peaks, pass or fail.

---

## 14. Demo video checklist

Record in one take if you can; edit later. Screen-record the Mac terminals plus a phone or webcam shot of the device. No tokens, IP-sensitive details or home locations on screen (blur or crop).

| # | Shot | Evidence for |
|---|---|---|
| 14.1 | Device and wiring close up, power on, LED states | S1 |
| 14.2 | Setup portal on the phone (token field masked), save, reboot, link up in the log | F1, secure link |
| 14.3 | Hold to talk: "What is the capital of Peru?" with the answer audible and the log beside it | S1, U9 |
| 14.4 | Refusal: "Write me a story" | U11 |
| 14.5 | Double press repeat | U13 |
| 14.6 | Barge-in during an answer | F9 |
| 14.7 | A GPS answer, saying out loud "this trip is replayed/simulated" if indoors: "Was that a harsh brake?" and "How was my driving today?" | S2, U2, U4 |
| 14.8 | Live GPS fix with sky (or the drive clip) and "How fast am I going?" | U1 |
| 14.9 | Privacy: multimeter on mic VDD idle 0 V and held 3.3 V | E3 |
| 14.10 | `security_evidence.sh` output: 401, 429, only TLS | E2 |
| 14.11 | `privacy_evidence.py` output and delete-all with before and after counts | E1, U14 |
| 14.12 | Latency table and T7 chart on screen | S3, T1, T7 |
| 14.13 | Say what is real and what is replayed or simulated | Honesty |

**Send back:** the unlisted video link and a note of anything you re-took.

---

## 15. Troubleshooting

### 15.1 ssh to the droplet is blocked (work VPN or laptop)

| Sign | Do |
|---|---|
| `ssh` hangs or `Connection timed out` on the work laptop | The corporate VPN or firewall blocks outbound ssh. Do not fight it. Use a laptop or network that is not the work one: phone hotspot, home Wi-Fi, a personal laptop with the repo cloned |
| Not sure which it is | `nc -vz $HOST 22` (open or timed out) and `nc -vz $HOST 443`. If 443 works but 22 does not, only ssh is blocked |
| Need only HTTPS tests (0.6, 0.7, 9, 11.3) | They use port 443 and can run from anywhere that allows it |
| Deploy and ufw need ssh | `server/deploy.sh`, 0.3, 0.4, 0.5, 0.8, 8.11, 8.12, the DB copy: run these from the unblocked network only |
| After `ufw --apply` ssh drops | Another network may be blocked by your own rule: use the DigitalOcean console to run `sudo ufw disable`, or `sudo ufw allow 22/tcp` |
| Host key changed | `ssh-keygen -R <ip>` |

### 15.2 Serial capture on macOS

| Sign | Do |
|---|---|
| No `/dev/cu.usbmodem*` | Use a data USB cable, not charge-only. Replug without BOOTSEL. If a `RP2350` drive shows, it is in BOOTSEL (1.1 mode): replug normally |
| `Resource busy` or `wav_capture` times out | Another program holds the port: `lsof /dev/cu.usbmodem*`. Quit `screen`, Arduino, miniterm, a second `serial_log.sh` |
| Log starts blank | The diag images print 3 s after boot; open the log sooner. The full firmware prints once per event: press the knob to get lines |
| Garbled text | Use `/dev/cu.*`, not `/dev/tty.*`. Baud setting is ignored over USB |
| Port number changes each flash | Re-run `ls /dev/cu.usbmodem*` after every flash |
| `pyserial missing` | `server/.venv/bin/pip install pyserial` and run with `server/.venv/bin/python` |
| `tools/serial_log.sh` says no port | The Pico may have crashed: unplug and replug; send what you had |
| Pico does not reboot after copy | The copy was not finished: wait for Finder; eject nothing; the drive disappears by itself |

### 15.3 Other

| Sign | Do |
|---|---|
| `fake_device.py` PIN MISMATCH | The cert was regenerated (it never is by deploy). Re-read 0.4 and re-enter the device |
| 401 from the Mac tools | Token typed wrong or revoked: `device list` on the droplet |
| 429 | More than 10 queries in a minute: wait |
| Pico stuck at fast blink | Hotspot off or 5 GHz or wrong password: log has `[WIFI] ...` |

---

## 16. Traceability

Every use case, test and privacy example maps to a step.

| ID | What | Step(s) |
|---|---|---|
| U1 | Current speed, stale check | 9.9, 9.13, 10.2 |
| U2 | Was that a harsh brake | 9.10 |
| U3 | Trip duration, distance, speed | 9.11 |
| U4 | How was my driving today | 9.3 |
| U5 | Braking vs last week | 9.5 |
| U6 | Should I take a break | 9.12 |
| U7 | Top speed, over 70 | 9.6 |
| U8 | Where am I | 9.7 |
| U9 | General question | 8.1 |
| U10 | Follow-up | 9.4 |
| U11 | Long form declined | 8.3 |
| U12 | Unsafe request declined | 8.4 |
| U13 | Repeat that | 8.6 |
| U14 | Delete my driving history | 11.2 |
| F1 | Setup portal first boot | 6.1, 6.2 |
| F2 | Hold at boot for setup | 6.3 |
| F3 | Accidental tap | 8.5 |
| F4 | Held over the limit | 8.7 |
| F5 | Silence | 8.8 |
| F6 | Hotspot down | 8.10 |
| F7 | Server unreachable | 8.11 |
| F8 | Provider fails | 8.12 |
| F9 | Barge-in | 8.9 |
| F10 | Volume | 3.1, 8.2 |
| F11 | No GPS fix | 10.1 |
| F12 | Wi-Fi drops mid trip | 10.3 |
| F13 | Power loss mid trip | 10.4 |
| F14 | Parked with device on | 9.8, 10.5 |
| F15 | Firmware update (OTA) | Not built, optional (plan P5). No step |
| E1 | Location privacy | 11.1, 11.2 |
| E2 | Unauthorised use | 7.2, 7.3, 11.3 |
| E3 | Covert recording | 11.4 |
| T1 | End-to-end latency | 12.1 |
| T2 | Latency breakdown | 12.2 |
| T3 | STT accuracy | 12.3 |
| T4 | Answer quality and safety | 12.4 |
| T5 | Cost per query | 12.2, 12.5 |
| T6 | GPS cold and warm start | 2.2, 12.6 |
| T7 | Brake detection | 13, 12.7 |
| T8 | GPIO mic noise | 4.4, 12.8 |

Not covered: a replay of a real recorded drive through `nmea_replay.py` (the firmware does not log raw NMEA; only the made-up fixture and your live drive are used), and the 5 Hz GPS option D3-B (not built).

---

## 17. Feedback template

Copy this into a reply. Fill every Result with PASS, FAIL or SKIP. Attach files named in the Send back lines.

```
ChatNav test feedback
Date:
Firmware image sha256 (from tools/collect_uf2.sh):
Network used for ssh:       Hotspot band (2.4 expected):

PART RESULT (PASS/FAIL/SKIP) | what I saw (one line) | attached file
0  Prerequisites:
2  diag_gps:            T6 cold __ s, warm __ s
3  diag_encoder:
4  diag_mic:            rms GPIO __, rms 3V3 __
5  diag_amp:
6  setup portal F1 / F2:
7  secure link / wrong token / wrong pin:
8  voice: 8.1 _  8.2 _  8.3 _  8.4 _  8.5 _  8.6 _  8.7 _  8.8 _  8.9 _  8.10 _  8.11 _  8.12 _
9  GPS replay: 9.3 _  9.4 _  9.5 _  9.6 _  9.7 _  9.8 _  9.9 _  9.10 _  9.11 _  9.12 _  9.13 _
10 GPS live: 10.1 _  10.2 _  10.3 _  10.4 _  10.5 _
11 privacy: E1 _  U14 _  E2 _  E3 (idle __ V, held __ V)
12 T1 P50 __ ms P95 __ ms | T5 __ pence | T3 WER quiet/music/engine __ / __ / __ | T4 helpful __/18
13 calibration: firm stops detected __/5, phone peak avg __, GPS peak avg __
14 demo video link:

Known unknowns (tell me what happened, or "not seen"):
- I2S mic bit alignment:
- amp slot timing:
- barge-in reconnect:
- back-pressure while TLS holds data:
- flash save stalling audio:
- first tap of double press plays cancel cue (expected):
- F13 gap close timing (times):
- Highway Code rule 91 exact wording:
- STT language en vs en-GB (any misheard words):

Worst problem:
Anything surprising:
```
