#ifndef APP_STATE_H
#define APP_STATE_H

#include "audio/cues.h"
#include "status/led_pattern.h"

/* Plan section 5.1. */
typedef enum {
    STATE_BOOT,
    STATE_SETUP,
    STATE_CONNECTING,
    STATE_IDLE,
    STATE_RECORDING,
    STATE_WAITING,
    STATE_PLAYING,
} app_state_t;

typedef enum {
    EV_CONFIG_VALID,   /* flash holds a valid config and setup was not requested */
    EV_CONFIG_MISSING, /* no usable config */
    EV_SETUP_REQUESTED,/* button held at boot for SETUP_HOLD_S (F2) */
    EV_LINK_UP,        /* Wi-Fi joined and the pinned TLS link to the server is up */
    EV_LINK_LOST,      /* Wi-Fi or server connection dropped */
    EV_PRESS,          /* button down */
    EV_DOUBLE_PRESS,   /* button down soon after a cancelled tap (U13) */
    EV_RELEASE_SHORT,  /* released before MIN_UTTERANCE_MS (F3) */
    EV_RELEASE,        /* released after MIN_UTTERANCE_MS */
    EV_MAX_UTTERANCE,  /* held for MAX_UTTERANCE_S (F4) */
    EV_FIRST_AUDIO,    /* first reply bytes arrived */
    EV_STREAM_END,     /* reply fully received and played, or ended with no audio */
    EV_QUERY_ERROR,    /* non-200, bad framing, upload failure (F7) */
    EV_QUERY_TIMEOUT,  /* no reply within QUERY_TIMEOUT_S (F7) */
    EV_VOLUME_TURN,    /* encoder turned (F10) */
    EV_COUNT,
} app_event_t;

#define CUE_NONE (-1)

/* Side effects the caller carries out for a transition, as a bit mask. */
enum {
    ACT_NONE = 0,
    ACT_START_RECORD = 1 << 0,   /* power mic, start capture, begin buffering the upload */
    ACT_STOP_MIC = 1 << 1,
    ACT_FINISH_UPLOAD = 1 << 2,  /* flush the last chunk and end the request body */
    ACT_ABORT_UPLOAD = 1 << 3,   /* discard queued audio; nothing was sent */
    ACT_START_REPLAY = 1 << 4,   /* GET /v1/query/last */
    ACT_START_PLAYBACK = 1 << 5,
    ACT_STOP_PLAYBACK = 1 << 6,  /* silence the speaker and empty the jitter buffer */
    ACT_DROP_CONNECTION = 1 << 7,/* close and reopen the server connection */
    ACT_ABORT_QUERY = 1 << 8,    /* forget the query; reopen the connection if it was mid-reply */
    ACT_SET_VOLUME = 1 << 9,     /* apply the encoder step and schedule the flash save */
};

typedef struct {
    app_state_t next;
    int cue;           /* cue_id_t or CUE_NONE */
    unsigned actions;
} app_transition_t;

/* Full transition table. Unlisted combinations stay in the same state with no effects.
 * SETUP never leaves by event: saving the form reboots the device. */
app_transition_t app_step(app_state_t s, app_event_t ev);

/* Next state only. */
app_state_t app_state_next(app_state_t s, app_event_t ev);

led_mode_t app_state_led(app_state_t s);
const char *app_state_name(app_state_t s);
const char *app_event_name(app_event_t ev);

#endif
