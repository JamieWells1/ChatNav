#include "status/led_pattern.h"

bool led_pattern_level(led_mode_t mode, uint32_t now_ms) {
    switch (mode) {
        case LED_SOLID: return true;
        case LED_SLOW_BLINK: return now_ms % 1000 < 500;
        case LED_FAST_BLINK: return now_ms % 200 < 100;
        case LED_FLASHING: return now_ms % 500 < 100;
        case LED_OFF:
        default: return false;
    }
}

const char *led_mode_name(led_mode_t mode) {
    switch (mode) {
        case LED_OFF: return "off";
        case LED_SLOW_BLINK: return "slow-blink";
        case LED_FAST_BLINK: return "fast-blink";
        case LED_SOLID: return "solid";
        case LED_FLASHING: return "flashing";
        default: return "unknown";
    }
}
