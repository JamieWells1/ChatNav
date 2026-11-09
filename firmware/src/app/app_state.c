#include "app/app_state.h"

#define T(n, c, a) ((app_transition_t){(n), (c), (a)})

static app_transition_t stay(app_state_t s) { return T(s, CUE_NONE, ACT_NONE); }

/* The encoder works in every state that has a speaker; in RECORDING the tick would reach
 * the mic, so the step changes silently. */
static app_transition_t volume_turn(app_state_t s) {
    return T(s, s == STATE_RECORDING ? CUE_NONE : CUE_VOLUME_TICK, ACT_SET_VOLUME);
}

static app_transition_t boot_step(app_event_t ev) {
    if (ev == EV_SETUP_REQUESTED || ev == EV_CONFIG_MISSING) return T(STATE_SETUP, CUE_NONE, 0);
    if (ev == EV_CONFIG_VALID) return T(STATE_CONNECTING, CUE_NONE, 0);
    return stay(STATE_BOOT);
}

static app_transition_t connecting_step(app_event_t ev) {
    switch (ev) {
        case EV_LINK_UP: return T(STATE_IDLE, CUE_NONE, ACT_NONE);
        case EV_PRESS:
        case EV_DOUBLE_PRESS: return T(STATE_CONNECTING, CUE_NO_NETWORK, ACT_NONE); /* F6 */
        case EV_VOLUME_TURN: return volume_turn(STATE_CONNECTING);
        default: return stay(STATE_CONNECTING);
    }
}

static app_transition_t idle_step(app_event_t ev) {
    switch (ev) {
        case EV_LINK_LOST: return T(STATE_CONNECTING, CUE_NONE, ACT_NONE);
        case EV_PRESS: return T(STATE_RECORDING, CUE_PRESS, ACT_START_RECORD);
        case EV_DOUBLE_PRESS: return T(STATE_WAITING, CUE_NONE, ACT_START_REPLAY); /* U13 */
        case EV_VOLUME_TURN: return volume_turn(STATE_IDLE);
        default: return stay(STATE_IDLE);
    }
}

static app_transition_t recording_step(app_event_t ev) {
    switch (ev) {
        case EV_RELEASE_SHORT: /* F3 */
            return T(STATE_IDLE, CUE_CANCEL, ACT_STOP_MIC | ACT_ABORT_UPLOAD);
        case EV_RELEASE:
            return T(STATE_WAITING, CUE_RELEASE, ACT_STOP_MIC | ACT_FINISH_UPLOAD);
        case EV_MAX_UTTERANCE: /* F4 */
            return T(STATE_WAITING, CUE_LIMIT, ACT_STOP_MIC | ACT_FINISH_UPLOAD);
        case EV_QUERY_ERROR:
            return T(STATE_IDLE, CUE_SERVER_ERROR, ACT_STOP_MIC | ACT_ABORT_UPLOAD | ACT_ABORT_QUERY);
        case EV_LINK_LOST:
            return T(STATE_CONNECTING, CUE_SERVER_ERROR,
                     ACT_STOP_MIC | ACT_ABORT_UPLOAD | ACT_ABORT_QUERY);
        case EV_VOLUME_TURN: return volume_turn(STATE_RECORDING);
        default: return stay(STATE_RECORDING);
    }
}

static app_transition_t waiting_step(app_event_t ev) {
    switch (ev) {
        case EV_FIRST_AUDIO: return T(STATE_PLAYING, CUE_NONE, ACT_START_PLAYBACK);
        case EV_QUERY_ERROR: /* F7 */
        case EV_QUERY_TIMEOUT: return T(STATE_IDLE, CUE_SERVER_ERROR, ACT_ABORT_QUERY);
        case EV_STREAM_END: return T(STATE_IDLE, CUE_SERVER_ERROR, ACT_NONE); /* empty reply */
        case EV_LINK_LOST: return T(STATE_CONNECTING, CUE_SERVER_ERROR, ACT_ABORT_QUERY);
        case EV_VOLUME_TURN: return volume_turn(STATE_WAITING);
        default: return stay(STATE_WAITING);
    }
}

static app_transition_t playing_step(app_event_t ev) {
    switch (ev) {
        case EV_STREAM_END: return T(STATE_IDLE, CUE_NONE, ACT_STOP_PLAYBACK);
        case EV_PRESS: /* F9 barge-in */
        case EV_DOUBLE_PRESS:
            return T(STATE_RECORDING, CUE_PRESS,
                     ACT_STOP_PLAYBACK | ACT_DROP_CONNECTION | ACT_START_RECORD);
        case EV_QUERY_ERROR:
        case EV_QUERY_TIMEOUT:
            return T(STATE_IDLE, CUE_SERVER_ERROR, ACT_STOP_PLAYBACK | ACT_ABORT_QUERY);
        case EV_LINK_LOST: return T(STATE_CONNECTING, CUE_SERVER_ERROR, ACT_STOP_PLAYBACK);
        case EV_VOLUME_TURN: return volume_turn(STATE_PLAYING);
        default: return stay(STATE_PLAYING);
    }
}

app_transition_t app_step(app_state_t s, app_event_t ev) {
    switch (s) {
        case STATE_BOOT: return boot_step(ev);
        case STATE_CONNECTING: return connecting_step(ev);
        case STATE_IDLE: return idle_step(ev);
        case STATE_RECORDING: return recording_step(ev);
        case STATE_WAITING: return waiting_step(ev);
        case STATE_PLAYING: return playing_step(ev);
        case STATE_SETUP: break;
    }
    return stay(s);
}

app_state_t app_state_next(app_state_t s, app_event_t ev) { return app_step(s, ev).next; }

led_mode_t app_state_led(app_state_t s) {
    switch (s) {
        case STATE_SETUP: return LED_SLOW_BLINK;
        case STATE_CONNECTING: return LED_FAST_BLINK;
        case STATE_RECORDING: return LED_SOLID;
        case STATE_WAITING: return LED_FLASHING;
        default: return LED_OFF;
    }
}

const char *app_state_name(app_state_t s) {
    switch (s) {
        case STATE_BOOT: return "BOOT";
        case STATE_SETUP: return "SETUP";
        case STATE_CONNECTING: return "CONNECTING";
        case STATE_IDLE: return "IDLE";
        case STATE_RECORDING: return "RECORDING";
        case STATE_WAITING: return "WAITING";
        case STATE_PLAYING: return "PLAYING";
    }
    return "?";
}

const char *app_event_name(app_event_t ev) {
    static const char *const names[EV_COUNT] = {
        "config-valid", "config-missing", "setup-requested", "link-up", "link-lost", "press",
        "double-press", "release-short", "release", "max-utterance", "first-audio",
        "stream-end", "query-error", "query-timeout", "volume-turn",
    };
    return ev < EV_COUNT ? names[ev] : "?";
}
