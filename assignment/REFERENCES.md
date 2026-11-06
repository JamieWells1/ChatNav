# References and Evidence for Design Decisions

Gathered 6 November 2026 with Claude Code (web search). **Check every entry yourself before citing it, ideally by opening it and adding it to Zotero.** The brief warns that AI-gathered references can be wrong. Source quality is rated in the tables as follows:

- ✅ **Primary:** official docs, datasheet, standard, regulator or peer-reviewed.
- 🟨 **Secondary:** vendor, blog or mirror. Fine for context, but swap in a primary source if one exists.
- ❌ **No source found:** an estimate. Measure it yourself or drop it.

---

## D-01 Firmware language: Pico SDK (C) vs MicroPython

| Claim | Source | Quality |
|---|---|---|
| MicroPython's own guide for speed: optimise the Python first, then the "native" and "viper" emitters. The native emitter produces code roughly twice the size of bytecode, and viper has extra restrictions | [MicroPython docs, *Maximising MicroPython speed*](https://docs.micropython.org/en/latest/reference/speed_python.html) | ✅ |
| MicroPython on the Pico 2 W runs a CPU benchmark in ~10–24 s. These are MicroPython-only figures, with no C comparison | [Grinberg, *Benchmarking MicroPython*](https://blog.miguelgrinberg.com/post/benchmarking-micropython); [Scruss, *MicroPython Benchmarks*](https://scruss.com/blog/2025/01/21/micropython-benchmarks/) | 🟨 |
| C extensions speed up heavy MicroPython work by roughly an order of magnitude | [PiCockpit (Italian)](https://picockpit.com/raspberry-pi/it/wp-json/wp/v2/posts/774) | 🟨 weak |
| "~50× slower than C" | none found | ❌ Measure it: time the mic sample conversion loop (32-bit → 16-bit, 16,000 samples) in both languages on your Pico. Your own result is better evidence than a citation |
| "~200 KB vs ~450 KB free memory" | none found | ❌ Measure it: `gc.mem_free()` in MicroPython; the linker map / heap stats in C |
| MicroPython has a built-in I2S driver for the rp2 port. It uses PIO + DMA, WS must be SCK+1, and the API is marked "technical preview" | [MicroPython rp2 quick reference](https://docs.micropython.org/en/v1.23.0/rp2/quickref.html); [machine.I2S class docs](https://docs.micropython.org/en/latest/library/machine.I2S.html); [rp2 `machine_i2s.c` source](https://github.com/micropython/micropython/blob/master/ports/rp2/machine_i2s.c) | ✅ |
| Pico SDK has TLS: link `pico_lwip_mbedtls` + `pico_mbedtls`, enable `LWIP_ALTCP_TLS` in `lwipopts.h`; the SDK supplies the hardware random number source | [pico-sdk commit "Add mbedtls to pico-sdk (#894)"](https://pigweed.googlesource.com/third_party/github/raspberrypi/pico-sdk/+/719d5e41a2f9c9347e43563b6bbcb2c46f780d65%5E%21); [i-programmer, *Pico WiFi: Simplest HTTPS Client*](https://www.i-programmer.info/programming/hardware/16268-master-the-pico-wifi-simplest-https-client.html?start=2); [pico_https_client (Pico 2 W example)](https://github.com/andrew-karppinen/pico_https_client) | 🟨 → use the pico-examples `tls_client` and the SDK docs as primary sources |
| Reusing your own Photon/Delta code | Your repo: [jamiewells1.github.io/photon](https://jamiewells1.github.io/photon/) | ✅ (own work) |

**Primary sources still to fetch:**
- Raspberry Pi, *Raspberry Pi Pico-series C/C++ SDK* (PDF, raspberrypi.com/documentation)
- Raspberry Pi, *RP2350 Datasheet* (PDF)
- Raspberry Pi, *Pico 2 W Datasheet* (PDF)

---

## D-02 Hardware limits (RP2350 / Pico 2 W)

| Claim | Source | Quality |
|---|---|---|
| RP2350: 520 KB SRAM, dual Cortex-M33 / Hazard3 cores up to 150 MHz, PIO state machines | [Tom's Hardware, *What's inside the RP2350*](https://www.tomshardware.com/raspberry-pi/raspberry-pi-pico/whats-inside-the-raspberry-pi-pico-2s-rp2350) | 🟨 → RP2350 Datasheet |
| GPIO output voltage is limited to 3.3 V (IOVDD 1.8–3.3 V) | Tom's Hardware (above) | 🟨 → RP2350 Datasheet, GPIO electrical section |
| GPIO drive strengths (2/4/8/12 mA) | not found in search | ❌ → RP2350 Datasheet pads section |

---

## D-03 Powering the mic and encoder from spare GPIO pins

| Claim | Source | Quality |
|---|---|---|
| INMP441 supply 1.62–3.63 V, current ~1.4–1.6 mA | [DigiKey INMP441 listing](https://www.digikey.ca/en/products/detail/tdk-invensense/INMP441ACEZ-R7/2606606); [TDK INMP441 datasheet (DigiKey HTML)](https://www.digikey.cn/htmldatasheets/production/1431884/0/0/1/inmp441-datasheet.html) | ✅ |
| INMP441 high power-supply rejection (−75 dBFS) | [INMP441 datasheet copy (Scribd)](https://www.scribd.com/document/664148019/INMP441) | 🟨 → TDK PDF |
| Put a 100 nF decoupling capacitor at the mic's VDD pin | [Core Electronics INMP441 guide](https://core-electronics.com.au/i2s-microphone-inmp441.html.md) | 🟨 |
| Mic outputs zero for the first ~2^18 clock cycles after power-up (85 ms at 3.072 MHz on the sister part, so ~256 ms at this project's 1.024 MHz) | [ADMP441 datasheet](https://dtsheet.com/model/727384); [INMP441 user manual, "Startup"](https://manuals.online/invensense/microphone/inmp441/user-manual/12y1p) | 🟨 sister part. Confirm in the INMP441 PDF |
| GPIO-powered INMP441 works in practice | none found | ❌ Your T8 noise test is the evidence |

---

## D-04 Speech services and LLM

| Claim | Source | Quality |
|---|---|---|
| `gpt-4o-mini-transcribe`: speech-to-text on `/v1/audio/transcriptions`, lower word error rate than the original Whisper | [OpenAI model page](https://developers.openai.com/api/docs/models/gpt-4o-mini-transcribe) | ✅ |
| OpenAI TTS `pcm` output = raw 24 kHz, mono, 16-bit signed little-endian | [openai-python SDK source](https://github.com/openai/openai-python/pull/1189/files); [Pipecat OpenAI TTS docs](https://docs.pipecat.ai/api-reference/services/tts/openai) | 🟨 → OpenAI API reference |
| Whisper paper: trained on 680,000 h of audio; robust to varied conditions | [Radford et al. (2023), ICML](https://proceedings.mlr.press/v202/radford23a.html) | ✅ |
| Whisper resamples input to 16 kHz | Not confirmed in search; check §2.2 of the paper or `whisper/audio.py` in [openai/whisper](https://github.com/openai/whisper) | ❌ to verify |
| Claude Haiku 5.5 model ID `claude-haiku-5-5`, $0.10 / $0.50 per M tokens | Anthropic models and pricing pages (from Claude Code's built-in API reference, cached October 2026) | ✅ → cite docs.anthropic.com / anthropic.com pricing |
| OpenAI small-model prices | Conflicting third-party listings ([CostBench](https://costbench.com/software/llm-api-providers/openai-api/), [Price Per Token](https://pricepertoken.com/pricing-page/model/openai-gpt-5.4-nano)) | 🟨 unreliable. Use OpenAI's pricing page |

---

## D-05 GPS rate, accuracy and harsh-braking detection

| Claim | Source | Quality |
|---|---|---|
| NEO-6: velocity accuracy 0.1 m/s, heading 0.5°, update rate up to 5 Hz (1 Hz default) | [u-blox NEO-6 datasheet copy](https://www.dessy.ru/include/images/ware/pdf/n/neo-6.pdf); [Waveshare NEO-6M wiki](https://www.waveshare.com/wiki/UART_GPS_NEO-6M) | 🟨 → u-blox official NEO-6 datasheet |
| No standard harsh-brake threshold; values range ~2.5–4.4 m/s², and up to 0.6 g in research | [SWOV / UDRIVE truck study](https://www.swov.nl/nl/node/17223); [arXiv 2608.16913 (connected-vehicle hotspots)](https://arxiv.org/pdf/2608.16913) | ✅ research |
| Product thresholds: −2.7 m/s² for ≥ 600 ms, only above 25 km/h (Telematics SDK); 4.4 m/s² default, 3.0 for testing (Digital Matter); −0.47 to −0.61 g (Geotab) | [Telematics SDK docs](https://docs.telematicssdk.com/docs/events-detection); [Digital Matter](https://digitalmatter.helpjuice.com/harsh-driving-event-setup); [Geotab](https://www.geotab.com/apac/blog/what-is-g-force/) | 🟨 industry practice. Useful for justifying the plan's starting value of −3.0 m/s² |

Design note: the Telematics SDK's minimum-speed condition, which ignores braking at walking pace, is worth copying. It is now in the plan as `HARSH_MIN_SPEED_KMH`.

---

## D-06 Driving rules used in answers

| Claim | Source | Quality |
|---|---|---|
| Highway Code Rule 91: a break of at least 15 minutes after every two hours of driving is recommended | [GOV.UK Highway Code, rules 89–102](https://www.gov.uk/highway-code/rules-for-drivers-and-motorcyclists-89-to-102) | ✅ |
| 70 mph is the national limit for cars on motorways and dual carriageways | Secondary only ([West Yorkshire Police](https://www.westyorkshire.police.uk/ask-the-police/question/Q594)) | 🟨 → GOV.UK *Speed limits* page / Highway Code rule 124 |

---

## D-07 Privacy and security frameworks

| Claim | Source | Quality |
|---|---|---|
| UK GDPR has seven principles, including data minimisation and storage limitation (Art. 5(1)(c), (e)) | [ICO, *A guide to the data protection principles*](https://ico.org.uk/for-organisations/uk-gdpr-guidance-and-resources/data-protection-principles/a-guide-to-the-data-protection-principles/) | ✅ |
| LINDDUN: seven privacy threat categories (Linkability, Identifiability, Non-repudiation, Detectability, Disclosure, Unawareness, Non-compliance), from KU Leuven | [Hovsepyan & Wuyts, LINDDUN slides (SecAppDev 2018)](https://handouts.secappdev.org/handouts/2018/Aram%20Hovsepyan%20&%20Kim%20Wuyts%20-%20LINDDUN-%20privacy%20engineering%20[SLIDES].pdf); [Danezis et al., *Privacy and Data Protection by Design* (ENISA, arXiv 1501.03726)](https://arxiv.org/pdf/1501.03726) | ✅ → also cite linddun.org |
| STRIDE: Spoofing, Tampering, Repudiation, Information disclosure, Denial of service, Elevation of privilege | Shostack, A. (2014) *Threat Modeling: Designing for Security*, Wiley, ch. 3; [Hernan et al., MSDN Magazine (2006)](https://msdn.microsoft.com/en-us/magazine/cc163519.aspx) | ✅ |
| OWASP API Security Top 10 (2023): API2 Broken Authentication, API4 Unrestricted Resource Consumption | Secondary ([APIsecurity.io](https://apisecurity.io/owasp-api-security-top-10/)) | 🟨 → owasp.org API Security project page |
| A DPIA is likely needed for a product that tracks location | not yet found | ❌ → ICO "When do we need to do a DPIA?" |

---

## Harvard list (draft: check each entry and complete it in Zotero)

- Hernan, S., Lambert, S., Ostwald, T. and Shostack, A. (2006) 'Uncover security design flaws using the STRIDE approach', *MSDN Magazine*, November. Available at: https://msdn.microsoft.com/en-us/magazine/cc163519.aspx (Accessed: 6 November 2026).
- Information Commissioner's Office (n.d.) *A guide to the data protection principles*. Available at: https://ico.org.uk/for-organisations/uk-gdpr-guidance-and-resources/data-protection-principles/a-guide-to-the-data-protection-principles/ (Accessed: 6 November 2026).
- MicroPython (n.d.) *Maximising MicroPython speed*. Available at: https://docs.micropython.org/en/latest/reference/speed_python.html (Accessed: 6 November 2026).
- OpenAI (n.d.) *GPT-4o mini Transcribe*. Available at: https://developers.openai.com/api/docs/models/gpt-4o-mini-transcribe (Accessed: 6 November 2026).
- Radford, A., Kim, J.W., Xu, T., Brockman, G., McLeavey, C. and Sutskever, I. (2023) 'Robust speech recognition via large-scale weak supervision', *Proceedings of the 40th International Conference on Machine Learning*, PMLR 202, pp. 28492–28518.
- Shostack, A. (2014) *Threat Modeling: Designing for Security*. Indianapolis: Wiley.
- TDK InvenSense (n.d.) *INMP441 Omnidirectional Microphone with Bottom Port and I²S Digital Output*, DS-INMP441-00 Rev 1.1.
- UK Government (n.d.) *The Highway Code: Rules for drivers and motorcyclists (89 to 102)*. Available at: https://www.gov.uk/highway-code/rules-for-drivers-and-motorcyclists-89-to-102 (Accessed: 6 November 2026).
