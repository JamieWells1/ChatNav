#ifndef APP_STATE_H
#define APP_STATE_H

#include "status/led_pattern.h"

/* Plan section 5.1. RECORDING, WAITING and PLAYING arrive in M3. */
typedef enum {
    STATE_BOOT,
    STATE_SETUP,
    STATE_CONNECTING,
    STATE_IDLE,
} app_state_t;

typedef enum {
    EV_CONFIG_VALID,   /* flash holds a valid config and setup was not requested */
    EV_CONFIG_MISSING, /* no usable config */
    EV_SETUP_REQUESTED,/* button held at boot for SETUP_HOLD_S (F2) */
    EV_LINK_UP,        /* Wi-Fi joined and the pinned TLS link to the server is up */
    EV_LINK_LOST,      /* Wi-Fi or server connection dropped */
} app_event_t;

/* Returns the next state; unlisted combinations leave the state unchanged. SETUP never
 * leaves by event: saving the form reboots the device. */
app_state_t app_state_next(app_state_t s, app_event_t ev);

led_mode_t app_state_led(app_state_t s);
const char *app_state_name(app_state_t s);

#endif
