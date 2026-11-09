#include "app/app_state.h"

app_state_t app_state_next(app_state_t s, app_event_t ev) {
    switch (s) {
        case STATE_BOOT:
            if (ev == EV_SETUP_REQUESTED || ev == EV_CONFIG_MISSING) return STATE_SETUP;
            if (ev == EV_CONFIG_VALID) return STATE_CONNECTING;
            return s;
        case STATE_CONNECTING:
            return ev == EV_LINK_UP ? STATE_IDLE : s;
        case STATE_IDLE:
            return ev == EV_LINK_LOST ? STATE_CONNECTING : s;
        case STATE_SETUP:
            return s;
    }
    return s;
}

led_mode_t app_state_led(app_state_t s) {
    switch (s) {
        case STATE_SETUP: return LED_SLOW_BLINK;
        case STATE_CONNECTING: return LED_FAST_BLINK;
        default: return LED_OFF;
    }
}

const char *app_state_name(app_state_t s) {
    switch (s) {
        case STATE_BOOT: return "BOOT";
        case STATE_SETUP: return "SETUP";
        case STATE_CONNECTING: return "CONNECTING";
        case STATE_IDLE: return "IDLE";
    }
    return "?";
}
