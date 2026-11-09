# STUB-PROVIDER BASELINE: 20 fake_device queries over local TLS with stub STT/LLM/TTS. Not real latency.

T1: release to first audio (server side)
| metric | n | P50 ms | P95 ms | min ms | max ms |
|---|---|---|---|---|---|
| first_audio_ms | 20 | 1 | 1 | 1 | 9 |

T2: per-stage breakdown
| metric | n | P50 ms | P95 ms | min ms | max ms |
|---|---|---|---|---|---|
| stt_ms | 20 | 0 | 0 | 0 | 0 |
| context_ms | 20 | 0 | 0 | 0 | 5 |
| llm_first_ms | 20 | 1 | 1 | 1 | 1 |
| llm_total_ms | 20 | 2 | 2 | 2 | 2 |
| tts_first_ms | 20 | 0 | 0 | 0 | 0 |
| tts_total_ms | 20 | 2 | 3 | 2 | 3 |
| total_ms | 20 | 3 | 4 | 3 | 11 |

Estimated cost per query: mean $0.00176 (assumed rates)
chart: ../assignment/evidence/m3-voice/t1_t2_stub_baseline.png
