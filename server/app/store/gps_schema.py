GPS_SCHEMA = """
CREATE TABLE IF NOT EXISTS fixes (
    device_id    TEXT NOT NULL,
    t_ms         INTEGER NOT NULL,
    device_ms    INTEGER NOT NULL,
    recv_ms      INTEGER NOT NULL,
    seq          INTEGER NOT NULL,
    lat_e7       INTEGER NOT NULL,
    lon_e7       INTEGER NOT NULL,
    speed_cmps   INTEGER NOT NULL,
    course_deg10 INTEGER NOT NULL,
    alt_dm       INTEGER NOT NULL,
    sats         INTEGER NOT NULL,
    quality      INTEGER NOT NULL,
    hdop10       INTEGER NOT NULL,
    flags        INTEGER NOT NULL,
    valid        INTEGER NOT NULL,
    outlier      INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (device_id, t_ms)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS fix_batches (
    device_id TEXT NOT NULL,
    seq       INTEGER NOT NULL,
    fw        INTEGER NOT NULL,
    drops     INTEGER NOT NULL,
    n         INTEGER NOT NULL,
    recv_ms   INTEGER NOT NULL,
    PRIMARY KEY (device_id, seq)
);

CREATE TABLE IF NOT EXISTS trips (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id      TEXT NOT NULL,
    start_ms       INTEGER NOT NULL,
    end_ms         INTEGER NOT NULL,
    last_fix_ms    INTEGER NOT NULL,
    status         TEXT NOT NULL,
    close_reason   TEXT,
    distance_m     REAL NOT NULL,
    moving_s       REAL NOT NULL,
    idle_s         REAL NOT NULL,
    max_speed_ms   REAL NOT NULL,
    brakes         INTEGER NOT NULL,
    accels         INTEGER NOT NULL,
    overs          INTEGER NOT NULL,
    over_s         REAL NOT NULL,
    since_stop_ms  INTEGER,
    start_cell     TEXT,
    end_cell       TEXT
);
CREATE INDEX IF NOT EXISTS trips_dev_start ON trips (device_id, start_ms);

CREATE TABLE IF NOT EXISTS events (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id  TEXT NOT NULL,
    trip_id    INTEGER NOT NULL,
    kind       TEXT NOT NULL,
    t_start_ms INTEGER NOT NULL,
    t_end_ms   INTEGER NOT NULL,
    peak       REAL NOT NULL,
    v_from     REAL NOT NULL,
    v_to       REAL NOT NULL,
    lat        REAL,
    lon        REAL
);
CREATE INDEX IF NOT EXISTS events_trip ON events (trip_id);
CREATE INDEX IF NOT EXISTS events_dev_t ON events (device_id, t_start_ms);

CREATE TABLE IF NOT EXISTS minute_buckets (
    device_id     TEXT NOT NULL,
    trip_id       INTEGER NOT NULL,
    minute_ms     INTEGER NOT NULL,
    distance_m    REAL NOT NULL,
    mean_speed_ms REAL NOT NULL,
    max_speed_ms  REAL NOT NULL,
    min_accel     REAL NOT NULL,
    max_accel     REAL NOT NULL,
    stopped_s     REAL NOT NULL,
    brakes        INTEGER NOT NULL,
    accels        INTEGER NOT NULL,
    overs         INTEGER NOT NULL,
    PRIMARY KEY (trip_id, minute_ms)
);

CREATE TABLE IF NOT EXISTS query_log (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id  TEXT NOT NULL,
    t_ms       INTEGER NOT NULL,
    transcript TEXT,
    reply      TEXT,
    outcome    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS timings (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id     TEXT NOT NULL,
    t_ms          INTEGER NOT NULL,
    upload_ms     INTEGER,
    stt_ms        INTEGER,
    context_ms    INTEGER,
    llm_first_ms  INTEGER,
    llm_total_ms  INTEGER,
    tts_first_ms  INTEGER,
    tts_total_ms  INTEGER,
    first_audio_ms INTEGER,
    total_ms      INTEGER,
    tokens_in     INTEGER,
    tokens_out    INTEGER,
    cost_usd      REAL,
    outcome       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
"""
