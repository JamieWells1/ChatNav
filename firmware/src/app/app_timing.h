#ifndef APP_TIMING_H
#define APP_TIMING_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_state.h"

/* Time-based decisions for the main loop, pure so they can be tested on the host. All
 * times are uint32 milliseconds and wrap safely. */

/* Press or double-press: DOUBLE_PRESS_MS after a cancelled tap (have_tap false if none). */
app_event_t app_press_event(bool have_tap, uint32_t tap_released_ms, uint32_t now_ms);

/* F3: release before MIN_UTTERANCE_MS cancels, otherwise the query goes ahead. */
app_event_t app_release_event(uint32_t held_ms);

/* The upload only starts once the press is known not to be an accidental tap. */
bool app_upload_armed(uint32_t held_ms);

/* F4. */
bool app_max_utterance_reached(uint32_t held_ms);

/* F7: waited_ms since the request was fully sent. */
bool app_query_timed_out(uint32_t waited_ms);

/* F10: a volume change is saved once VOLUME_SAVE_DELAY_S passes with no further turn. */
bool app_volume_save_due(bool pending, uint32_t last_turn_ms, uint32_t now_ms);

/* No event pending. */
#define APP_NO_EVENT (-1)

/* WAITING-state verdict from the reply so far: any non-200 answer (including 404 from
 * GET /v1/query/last with nothing cached) or a failure is a query error, which plays the
 * server-error cue; first audio starts playback; a 200 that ends with no audio is a stream
 * end (also the error cue). Returns an app_event_t or APP_NO_EVENT (timeout is separate). */
int app_waiting_event(bool failed, bool headers, int status, bool audio_started, bool reply_done);

#endif
