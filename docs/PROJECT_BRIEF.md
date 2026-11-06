# Driving Companion: Project Brief for Implementation Planning

Written 6 November 2026. This document is a full handoff of everything known about the project so far. **Nothing here is final.** Items are tagged so you can see how firm they are:

- **[DECIDED]** the owner has committed to it (parts bought, wiring chosen).
- **[PROVISIONAL]** the current plan, but open to change if there's a good reason.
- **[OPEN]** not yet decided; needs investigation or a choice.

---

## 1. The idea in one paragraph

An in-car voice companion built on a Raspberry Pi Pico 2 W. The driver holds a push-to-talk control, asks a question out loud, and hears a spoken answer. The device also reads GPS, so the answer can use real driving data (for example "how fast was I going?" or "was that a harsh brake?"), and it can answer general questions too. The Pico is only a thin I/O device. Speech-to-text, the LLM call and text-to-speech all happen on a DigitalOcean server the owner already has. Tagline: **"Your all in one driving companion."** The owner suspects products like this already exist.

This is both a university assessment and a personal project. The owner plans to use the assessment deadlines as motivation and to write the project up.

---

## 2. Academic context (this shapes scope)

**Module:** IOT6000U Digital Innovation and Product Development, Queen Mary University of London. It's a final-year degree apprenticeship module (apprenticeship standard v1.2, assessed against KSBs). The owner's technical specialism isn't stated.

| | 001 Presentation | 002 Individual Coursework |
|---|---|---|
| Weight | 20% | 80% |
| Format | 10-minute video, owner must appear on camera | 3000-word report (+/-10%), .docx or .pdf |
| Deadline | Friday 5 November 2026, 23:59 | Tuesday 12 January 2027, 23:59 |
| GenAI policy | AMBER: permitted, with cautions | GREEN: AI use is **required** |

> **Date check:** the 001 brief says "Friday 5th November 2026", but 5 November 2026 is a Thursday. The owner should confirm the real deadline and whether any late penalty applies (5% of marks per 24 hours, up to 7 days, then zero).

### 001 presentation (suggested structure and marking)

Five equally weighted criteria (20% each): introduction and area of investigation, proposed AI solution, potential obstacles, business case outline, communication skills.

- **Proposed AI solution** must include "some form of AI", contrast with at least one alternative solution, justify the preferred approach (competitive advantage), and show technical viability evidence (prototype visuals, sample code, feasibility testing).
- **Obstacles** should use a proven framework (PESTEL is suggested for external factors) covering internal and external obstacles. Top marks need justification of the chosen framework, with references.
- **Business case** must cover justified type of initiative (startup, social enterprise, internal initiative), benefits and value, and resources required (short and long term).
- Timing within +/-10% of 10 minutes for top bands. A "talking head" with no visuals can't reach the highest marks. Redact any personal information.
- Feedback from 001 must be used ("feedforward") in 002.

### 002 report (suggested structure, word counts and marking)

| Section | Words | Weight |
|---|---|---|
| Introduction and background | 500 | 10% |
| Proposed solution (design docs, iterated) | 500 | 20% |
| Proof of concept | 750 | 30% |
| Privacy and security evaluation | 750 | 20% |
| Conclusions and future plans | 500 | 10% |
| Written communication | n/a | 10% |

Key requirements that affect implementation:

- **Proof of concept (30%)** needs tangible outputs (code, config, UI, demo) with justified technical decisions, a working AI implementation, and evidence of at least two of: **version control, task tracking, generative AI use**. Top bands want sophisticated project management and at least one high-quality external deliverable (video demo, code repo or notebook, publicly accessible).
- **Privacy and security (20%)** needs **three specific, highly relevant examples**. Each needs risk identification, likelihood and impact evaluation, a mitigation, and **evidence of at least one working mitigation** (for example a screenshot of an attack failing). Top bands reference proven frameworks.
- **Design documentation** should show iteration (requirements, user stories, wireframes, architecture and data-flow diagrams, and so on).
- **GenAI disclosure:** coding support and agentic AI coding are allowed, but the brief says the student **must disclose and evidence which AI coding tools were used** and must be able to explain the code and decisions. **If an AI agent writes any implementation, the owner needs to record that.**

---

## 3. Scope decisions so far

| Decision | Status | Notes |
|---|---|---|
| Bench prototype, not installed in a car | [PROVISIONAL] | Lowest risk. In-car installation is "future work". |
| Push-to-talk, no wake word | [PROVISIONAL] | Avoids hard always-listening audio processing. Also a privacy positive. |
| WiFi via phone hotspot, no cellular | [PROVISIONAL] | Pico 2 W has WiFi only. Cellular is future work. |
| GPS only for driving data (no OBD-II, no IMU) | [PROVISIONAL] | OBD-II and a real accelerometer listed as future work. |
| Acceleration estimated from GPS speed changes | [PROVISIONAL] | Coarse (see section 6.5). |
| All AI on the server, none on the Pico | [DECIDED] | The RP2350 can't run STT/LLM/TTS. |
| Minimal hardware, no fancy extras | [DECIDED] | Owner wants it as simple as possible. |
| Owner already has the DigitalOcean server | [DECIDED] | Size, OS and region not recorded. |
| Firmware language | [OPEN] | MicroPython vs Arduino-Pico (C++). GPS/button test snippets so far are MicroPython. Mic and amp I2S test code hasn't been written. |

---

## 4. Hardware

### 4.1 Parts (all received as of 6 Nov 2026; total spend about £32 on parts bought for this project)

| Part | Detail | Notes |
|---|---|---|
| Microcontroller | Raspberry Pi Pico 2 WH (RP2350, WiFi, headers soldered) | Already owned. Micro-USB. |
| Microphone | INMP441 I2S MEMS breakout, pins soldered | 6 pins: SCK, WS, L/R, SD, VDD, GND |
| Amplifier | MAX98357A I2S Class-D breakout, pins and green screw terminal soldered | Pins: LRC, BCLK, DIN, GAIN, SD, GND, VIN |
| Speaker | CQRobot-style 8 ohm, 2-3 W, about 35 x 25 mm, lead wires | Leads end in female Dupont sockets (arrived this way), so they must be cut off to use the screw terminal |
| GPS | Gonnely "Mini NEO-6M", pins soldered, SMA and IPEX antenna connectors | Antenna **not** included with the module |
| GPS antenna | DollaTek active GPS antenna, SMA male, 28 dB, 3 m cable, magnetic base | Comes with a steel mounting plate and sticky pad. Peeling the plate off is harmless. |
| Controls | KY-040 rotary encoder (owned) | Push switch = talk. Rotation = volume (see 4.4). |
| Wiring | **20 female-to-female jumper wires** | Owned. All connectors are male pins, so both jumper ends must be female. |
| Power | Power bank + micro-USB cable | Phone provides hotspot. |

### 4.2 Pin plan: [PROVISIONAL, current wiring plan]

**No breadboard** (the owner doesn't have one). Because a jumper can only plug into a pin once and the Pico has only one 3V3 output pin, power is distributed as follows:

- **GPS** gets the 3V3 pin (about 40+ mA, needs a real supply).
- **Amp** gets 5 V from VBUS (it can draw hundreds of mA; keep it off the 3.3 V regulator).
- **Mic VDD** is powered from a GPIO (GP9) driven high. The mic draws about 1.4 mA.
- **Encoder `+`** is powered from a GPIO (GP12) driven high. Its pull-ups draw about 1 mA at most.
- Each ground connection uses its own GND pin (the Pico has 8).

**Board orientation:** the pin names are printed on the **underside**. With the underside facing you and USB at the top: the **right column** top to bottom is GP0 to GP15 (pins 1-20); the **left column** bottom to top is GP16 ... up to 3V3, GND, VSYS, VBUS (pins 21-40). VBUS (pin 40) is the top-left corner. The board doesn't print pin numbers; numbers below are position counts.

| Module pin | Pico pin | Notes |
|---|---|---|
| **GPS** VCC | 3V3, pin 36 | **Not** 3V3_EN (pin 37, right next to it) |
| GPS GND | GND, pin 38 (between VSYS and 3V3_EN) | |
| GPS TXD | GP21, pin 27 | UART1 RX. Module TX goes to Pico RX. |
| GPS RXD | GP20, pin 26 | UART1 TX. Default 9600 baud. |
| **Mic** VDD | GP9, pin 12 | Powered from GPIO. Code must set it high. |
| Mic GND | GND, pin 13 (between GP9 and GP10) | |
| Mic L/R | GND, pin 8 (between GP5 and GP6) | Low selects the left channel. |
| Mic SCK | GP6, pin 9 | I2S bit clock |
| Mic WS | GP7, pin 10 | Word select. Must be SCK+1 for PIO I2S. |
| Mic SD | GP8, pin 11 | Data |
| **Amp** VIN | VBUS, pin 40 | **Not** VSYS (pin 39). Wire runs over the top of the board. |
| Amp GND | GND, pin 3 (between GP1 and GP2) | |
| Amp BCLK | GP0, pin 1 | I2S bit clock |
| Amp LRC | GP1, pin 2 | Must be BCLK+1 |
| Amp DIN | GP2, pin 4 | Data |
| Amp GAIN, SD | unconnected | Assumed defaults. Verify behaviour on this clone board. |
| **KY-040** `+` | GP12, pin 16 | Powered from GPIO. Code must set it high. |
| KY-040 SW | GP13, pin 17 | Push switch, active low |
| KY-040 GND | GND, pin 18 (between GP13 and GP14) | |
| KY-040 DT | GP14, pin 19 | |
| KY-040 CLK | GP15, pin 20 | |

**Free pins:** GP3, GP4, GP5, GP10, GP11, GP16 to GP19, GP22, GP26 to GP28 (GP4/GP5 are I2C0-capable, a possible future IMU). GND pins 23, 28 and 33 (AGND) are unused. **Reserved:** GP23, GP24, GP25 and GP29 are used by the WiFi chip.

**Voltage rules:** only the amp gets 5 V. The mic and encoder are 3.3 V parts. The Pico's own USB-powered VBUS is 5 V.

**Known concern: [OPEN]** GPIO-powered mic and encoder is unconventional. RP2040 documentation and the Pico SDK give selectable drive strengths of 2, 4, 8 and 12 mA with 4 mA the default; the RP2350 datasheet should be checked for its exact limits. Both loads are well under 4 mA, but noise on a GPIO-supplied mic rail is untested. **Fallback:** a small breadboard with a shared 3V3 rail.

### 4.3 Wiring documents already produced

- `driving-companion-wiring-diagram-clean.svg` / `.png`: current pin plan, no annotations (underside view).
- `driving-companion-wiring-diagram.svg`: annotated version of the same.
- `driving-companion-build-manual.md`: build and test steps. **Stale:** its encoder section still shows only SW and GND.
- `driving-companion-hardware-spec.md`: **obsolete.** It uses an earlier pin map.

This brief is the source of truth where they disagree.

### 4.4 Peripheral behaviour notes

- **Mic:** INMP441 outputs 24-bit samples in 32-bit I2S slots. With L/R tied low, data is on the **left** slot. Software must read the left slot and convert to 16-bit for transport.
- **Amp:** the MAX98357A has **no volume control**, only a fixed gain strap pin. **Volume must be done in software**, by scaling PCM samples on the Pico or on the server. The encoder rotation (CLK/DT) is the natural input. Output is an I2S stream; check how this clone handles mono and the SD pin.
- **GPS:** NMEA 0183 sentences over UART at about 1 Hz (lat/long, speed over ground, course, altitude, UTC time, satellites and fix quality). A cold start can take minutes. **No fix indoors.** Test near a window or outside. The antenna must face the sky.
- **Encoder:** the KY-040 has on-board pull-ups to `+`. SW is a switch to ground when pressed. CLK/DT are quadrature outputs.
- **Status LED:** the Pico's onboard LED, controlled through the WiFi chip (`Pin("LED")` in MicroPython). Planned states: off = idle, solid = recording, flashing = processing. It's small and hard to see in daylight. An external LED (GPIO + resistor) is a simple retrofit.
- **WiFi:** the Pico 2 W is **2.4 GHz only**. The phone hotspot must offer 2.4 GHz.

### 4.5 Enclosure: [OPEN]

The owner has a 3D printer. Rough footprints: Pico 51 x 21 mm, mic about 15 x 10, amp about 20 x 20, GPS about 20 x 15, KY-040 about 20 x 17 plus knob, speaker 35 x 25. Estimated case about 110 x 90 x 40 mm with standoffs, a speaker grille, a hole for the encoder knob, and cable exits. The GPS antenna can stay outside on its 3 m cable, and the speaker could be external. Measure the real parts before modelling.

---

## 5. System architecture: [PROVISIONAL]

```
Driver presses KY-040
        |
Pico 2 W: powers mic (GP9), records I2S audio
        |  (chunked over WiFi, phone hotspot)
        v
DigitalOcean server (Flask/FastAPI or similar)
   1. speech-to-text
   2. LLM call, with recent GPS data and derived stats in the prompt
   3. text-to-speech
        |  (audio back over WiFi)
        v
Pico 2 W: plays through MAX98357A + speaker
```

The Pico also reads GPS continuously and either forwards readings to the server or buffers them. The server keeps a rolling window of recent readings for context.

---

## 6. Implementation considerations for the next agent

### 6.1 Audio capture and transport: [OPEN]

- Whisper-style STT works best at **16 kHz mono**. 16-bit samples at 16 kHz is **32 KB/s**, so 10 seconds is about **320 KB**. The Pico 2 W has 520 KB SRAM in total, shared with WiFi and any TLS buffers, so a whole-clip buffer is risky. **Chunked streaming** (HTTP POST chunks or WebSocket) with a maximum recording length is likely needed.
- PIO-based I2S implementations typically need WS = SCK+1 (and LRCLK = BCLK+1). Check against the chosen library.
- The mic and amp must run as two separate I2S instances.

### 6.2 Playback: [OPEN]

- MP3 decoding on the Pico isn't realistic. Have the server return **raw PCM or WAV** at a rate the Pico can play (16-22 kHz mono is plenty for speech). Software volume scaling happens on the samples.
- Stream playback in chunks to avoid buffering the whole reply.

### 6.3 Server

- Droplet size is unknown. It determines whether **self-hosted STT** (for example a small Whisper model) is practical or whether a hosted STT API is better. [OPEN]
- STT, LLM and TTS providers are undecided. [OPEN] Cost per query is a business-case input. The LLM is called with the transcript plus GPS context, and the system prompt should force **short, plain spoken answers** (no markdown, no lists), keep replies brief to cut TTS latency and driver distraction, and avoid anything that encourages looking at a screen.
- Add per-stage timing logs (upload, STT, LLM, TTS, download) for latency evidence in the report.

### 6.4 Latency expectation

Estimated 4-10 s from releasing the button to hearing a reply, made up of upload (0.5-2 s), STT (1-3 s), LLM (1-4 s), TTS (1-3 s) and download/playback (0.5-1 s). This is slower than mainstream assistants and should be stated honestly. Cheap mitigations: an LED cue while processing and short replies. **Streaming** responses (start TTS before the LLM finishes) is a possible future improvement.

### 6.5 GPS-derived acceleration: [PROVISIONAL]

Acceleration = (speed now - speed previous) / time difference, computed **on the server** over a rolling window. Harsh braking is a threshold on strongly negative values. A starting point of about -3 m/s² was suggested, to be **calibrated with real test data** rather than trusted. Smooth over 2-3 deltas. It is noisy (GPS speed error of roughly 0.1-0.5 m/s, and 1 Hz sampling is coarse for a braking event), so the report should call it an **estimate**. A real IMU is the natural future step.

### 6.6 Noise robustness

Whisper-class models cope with some background noise and music but can fail, and may transcribe song lyrics as speech. Source separation isn't part of STT. Best levers: mic placement close to the driver, push-to-talk, and trimming silence and normalising volume server-side. Plan to **test with real recordings** (radio on, engine running) and keep the results as evidence. Noise cancellation is out of scope.

### 6.7 Testing plan (also report evidence)

Build and test one module at a time: GPS, then encoder, then mic, then amp and speaker, then everything together, then the server pipeline. Record what worked and what didn't as the iteration evidence the rubric rewards.

---

## 7. Privacy and security: candidate examples for the 002 evaluation

The report needs three specific risks with likelihood/impact evaluation, mitigations and **working evidence**. Candidates:

1. **Audio and location sent to third-party services (STT/LLM/TTS).** Mitigation ideas: minimise data sent (coarse or rounded location where possible), clear retention policy, no persistent audio storage, check provider data-use terms.
2. **Unauthorised access to the server endpoint.** Mitigation ideas: token authentication, firewall (only needed ports), rate limiting, TLS. Evidence: an unauthenticated request that fails.
3. **Unintended or covert recording (including passengers).** Mitigation: hardware push-to-talk, and the mic is **physically unpowered** (GP9 low) except while talking. This is demonstrable, and notes the device's consent angle.
4. **Credentials on the device** (WiFi password, API token in firmware). Mitigation ideas: keep API keys on the server only, use a revocable device token.
5. **Prompt injection through transcribed speech or LLM output.** Mitigation ideas: constrain the system prompt, limit what the LLM can do (no tool access).

**TLS on the Pico** is possible but memory-constrained and needs testing. If plain HTTP is used for the prototype, document it as a risk with compensating controls. [OPEN]

Frameworks that could earn the "proven framework" credit: STRIDE, LINDDUN (privacy), OWASP guidance, UK GDPR / ICO guidance (the owner is UK-based). Voice and location data may raise data-protection questions to research.

---

## 8. Competitive alternatives and business case: [OPEN, for 001/002]

**Alternatives to contrast:** Android Auto, Apple CarPlay, Siri, Google Assistant (these already do voice in the car); OBD-II dongles with phone apps; dashcams; a plain phone app. A purpose-built device can't beat Google at general voice assistance, so the argument must be **driving-data awareness, hands-free by design, and independence from a phone's assistant**. The business case type (startup, social enterprise, internal initiative), benefits, and resources are all undecided.

**Obstacles for a PESTEL analysis (to research and reference):**

- **Political/Legal:** road-safety and in-vehicle device rules (UK rules on hand-held device use and distraction), UK GDPR for voice and location data, product certification (UKCA/CE, EMC) for any real product. **Check official sources before stating specifics.**
- **Economic:** per-query STT, LLM and TTS costs; hardware cost.
- **Social:** trust, privacy concerns, driver distraction.
- **Technological:** latency, connectivity in a moving car, noise, GPS accuracy, microcontroller limits.
- **Environmental:** device e-waste, energy use of cloud AI.

Internal obstacles: limited time, a single builder, untested GPIO-powered peripherals, and unknown server capacity.

---

## 9. Process and evidence to keep

- **Version control** (for example a public GitHub repo) and a **task tracker**, as the PoC rubric asks.
- **GenAI use log:** which tools were used, for what, and how the output was checked. This is mandatory for coding assistance on 002.
- **A changelog of design decisions.** For example, the pin plan was revised several times during planning (breadboard dropped, power distribution reworked, encoder expanded from SW-only to all five pins). This is exactly the "iterative approach" evidence the report needs.
- An externally linked deliverable: video demo, code repo, or notebook.
- No personally identifiable information in submitted material.

---

## 10. Open questions for the implementation agent to help resolve

1. MicroPython or Arduino-Pico, and which I2S, GPS-parsing and WiFi libraries?
2. Transport design: HTTP chunk upload or WebSocket, and a maximum utterance length?
3. STT, LLM and TTS providers, and whether STT is self-hosted given the droplet's size?
4. Audio format and sample rate for replies, and where volume scaling happens?
5. Is GPIO-powering the mic and encoder reliable in practice, or is the breadboard fallback needed?
6. TLS on the Pico versus plain HTTP with compensating controls?
7. What GPS data is kept, for how long, and in what shape for the LLM prompt?
8. Harsh-braking threshold calibration method?
9. Whether to add a visible status LED or small sound cues.
10. Enclosure design and mounting.
11. What the demo looks like (bench with recorded or simulated trip data, or a live outdoor test)?
