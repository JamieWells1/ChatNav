from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    # Secrets and paths (from .env)
    device_token: str = ""
    stt_api_key: str = ""
    llm_api_key: str = ""
    tts_api_key: str = ""
    db_path: str = "chatnav.db"

    # Tunables (plan section 14)
    audio_sample_rate: int = 16000
    min_sats: int = 4
    max_hdop: float = 5.0
    max_plausible_accel_ms2: float = 12.0
    stationary_speed_ms: float = 0.5
    trip_start_speed_ms: float = 2.5
    trip_start_hold_s: int = 10
    trip_end_idle_s: int = 300
    trip_end_gap_s: int = 600
    min_trip_m: int = 200
    max_delta_s: int = 3
    accel_smooth_n: int = 3
    harsh_brake_ms2: float = -3.0
    harsh_accel_ms2: float = 3.0
    harsh_min_duration_s: int = 1
    harsh_min_speed_kmh: int = 25
    over_limit_mph: int = 70
    over_limit_min_s: int = 5
    live_window_s: int = 300
    live_trace_window_s: int = 120
    live_trace_step_s: int = 5
    recent_peak_window_s: int = 30
    raw_retention_h: int = 48
    raw_hard_limit_h: int = 168
    event_location_dp: int = 3
    event_location_retention_d: int = 30
    event_retention_d: int = 180
    minute_retention_d: int = 30
    trip_retention_d: int = 365
    query_text_retention_d: int = 7
    timing_retention_d: int = 90
    purge_interval_min: int = 60
    delete_confirm_s: int = 30
    context_max_events: int = 5
    context_max_tokens: int = 1500
    convo_turns: int = 3
    convo_ttl_s: int = 120
    reply_max_sentences: int = 2
    llm_max_output_tokens: int = 150
    min_speech_ms: int = 300
    max_utterance_s: int = 10
    reply_cache_ttl_s: int = 300
    rate_limit_queries_per_min: int = 10
    latency_target_p50_s: int = 5
    latency_runs: int = 20


settings = Settings()
