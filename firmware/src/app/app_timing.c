#include "app/app_timing.h"

#include "config.h"

app_event_t app_press_event(bool have_tap, uint32_t tap_released_ms, uint32_t now_ms) {
    return have_tap && (uint32_t)(now_ms - tap_released_ms) <= DOUBLE_PRESS_MS ? EV_DOUBLE_PRESS
                                                                                : EV_PRESS;
}

app_event_t app_release_event(uint32_t held_ms) {
    return held_ms < MIN_UTTERANCE_MS ? EV_RELEASE_SHORT : EV_RELEASE;
}

bool app_upload_armed(uint32_t held_ms) { return held_ms >= MIN_UTTERANCE_MS; }

bool app_max_utterance_reached(uint32_t held_ms) { return held_ms >= MAX_UTTERANCE_S * 1000u; }

bool app_query_timed_out(uint32_t waited_ms) { return waited_ms >= QUERY_TIMEOUT_S * 1000u; }

bool app_volume_save_due(bool pending, uint32_t last_turn_ms, uint32_t now_ms) {
    return pending && (uint32_t)(now_ms - last_turn_ms) >= VOLUME_SAVE_DELAY_S * 1000u;
}
