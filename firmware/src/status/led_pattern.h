#ifndef LED_PATTERN_H
#define LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

/* LED states from plan section 5.1. */
typedef enum {
    LED_OFF,         /* IDLE, PLAYING */
    LED_SLOW_BLINK,  /* SETUP: 1 Hz */
    LED_FAST_BLINK,  /* CONNECTING: 5 Hz */
    LED_SOLID,       /* RECORDING */
    LED_FLASHING,    /* WAITING: short flash twice a second */
} led_mode_t;

/* Pure function of mode and time so it can be tested on the host. */
bool led_pattern_level(led_mode_t mode, uint32_t now_ms);

const char *led_mode_name(led_mode_t mode);

#endif
