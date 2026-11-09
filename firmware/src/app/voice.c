#include "app/voice.h"

#include <stdio.h>

#include "app/app_timing.h"
#include "audio/mic.h"
#include "audio/playback.h"
#include "audio/speaker.h"
#include "audio/volume.h"
#include "config.h"
#include "input/input.h"
#include "net/fix_upload.h"
#include "net/link.h"
#include "net/uplink.h"
#include "pico/time.h"
#include "storage/flash_memory.h"

static device_config_t *cfg;
static Rotator rtr;
static int16_t mic_buf[MIC_BLOCK_FRAMES];

static uint32_t press_ms;      /* when the current recording began */
static uint32_t wait_ms;       /* when the request was fully handed over */
static uint32_t tap_release_ms;
static bool have_tap;          /* last press was a cancelled tap (for U13) */
static bool armed;             /* upload enabled for this recording */
static int turn_dir;           /* +1/-1 for the pending EV_VOLUME_TURN */
static bool volume_pending;
static uint32_t volume_turn_ms;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

void voice_init(device_config_t *c) {
    cfg = c;
    input_rotator_init(&rtr);
    if (RC_FAILED(mic_init())) printf("[VOICE] mic init failed, recording will not work\n");
    if (RC_FAILED(speaker_init())) printf("[VOICE] speaker init failed, no sound\n");
    speaker_set_volume(cfg->volume_step);
    playback_init();
    speaker_start();
    uplink_init(cfg);
    printf("[VOICE] ready, volume %d/%d\n", speaker_get_volume(), VOLUME_STEPS - 1);
}

int voice_poll_input(app_state_t s) {
    (void)s;
    input_rtr_update(&rtr);
    uint32_t now = now_ms();

    if (input_rtr_pressed(&rtr)) {
        app_event_t ev = app_press_event(have_tap, tap_release_ms, now);
        have_tap = false;
        printf("[INPUT] press (%s)\n", app_event_name(ev));
        fix_upload_flush();
        return ev;
    }
    if (input_rtr_released(&rtr) && s == STATE_RECORDING) {
        uint32_t held = now - press_ms;
        app_event_t ev = app_release_event(held);
        printf("[INPUT] release after %u ms (%s)\n", (unsigned)held, app_event_name(ev));
        if (ev == EV_RELEASE_SHORT) {
            have_tap = true;
            tap_release_ms = now;
        }
        return ev;
    }
    if (input_rtr_cw(&rtr)) {
        turn_dir = 1;
        return EV_VOLUME_TURN;
    }
    if (input_rtr_anti_cw(&rtr)) {
        turn_dir = -1;
        return EV_VOLUME_TURN;
    }
    return VOICE_NO_EVENT;
}

static void drain_mic(void) {
    for (int i = 0; i < 2; i++) {
        size_t n = mic_poll(mic_buf, MIC_BLOCK_FRAMES);
        if (n == 0) break;
        uplink_feed_pcm(mic_buf, n);
    }
}

static int poll_recording(void) {
    drain_mic();
    uint32_t held = now_ms() - press_ms;
    if (!armed && app_upload_armed(held)) {
        armed = true;
        uplink_arm();
    }
    return app_max_utterance_reached(held) ? EV_MAX_UTTERANCE : VOICE_NO_EVENT;
}

static int poll_waiting(void) {
    uplink_status_t st = uplink_status();
    int ev = app_waiting_event(st.failed, st.headers, st.status, st.audio_started, st.reply_done);
    if (ev != APP_NO_EVENT) {
        if (st.headers && st.status != 200) printf("[VOICE] server answered %d\n", st.status);
        return ev;
    }
    if (app_query_timed_out(now_ms() - wait_ms)) {
        printf("[VOICE] no reply within %d s\n", QUERY_TIMEOUT_S);
        return EV_QUERY_TIMEOUT;
    }
    return VOICE_NO_EVENT;
}

static int poll_playing(void) {
    uplink_status_t st = uplink_status();
    if (st.failed) return EV_QUERY_ERROR;
    if (playback_finished()) return EV_STREAM_END;
    /* A stalled stream: the server stopped sending but the reply never completed. */
    if (!st.reply_done && jitter_queued(playback_buffer()) == 0 &&
        app_query_timed_out(now_ms() - st.last_audio_ms)) {
        printf("[VOICE] reply stalled for %d s\n", QUERY_TIMEOUT_S);
        return EV_QUERY_TIMEOUT;
    }
    return VOICE_NO_EVENT;
}

int voice_poll(app_state_t s) {
    speaker_poll();
    switch (s) {
        case STATE_RECORDING: return poll_recording();
        case STATE_WAITING: return poll_waiting();
        case STATE_PLAYING: return poll_playing();
        default: return VOICE_NO_EVENT;
    }
}

static void apply_volume(void) {
    int step = volume_clamp_step(speaker_get_volume() + turn_dir);
    speaker_set_volume(step);
    volume_pending = true;
    volume_turn_ms = now_ms();
    printf("[VOICE] volume %d/%d\n", step, VOLUME_STEPS - 1);
}

void voice_do(unsigned actions, app_state_t from) {
    (void)from;
    if (actions & ACT_STOP_PLAYBACK) playback_reset();
    if (actions & ACT_DROP_CONNECTION) {
        printf("[VOICE] barge-in: dropping the connection\n");
        link_force_reconnect();
    }
    if (actions & ACT_STOP_MIC) {
        if (actions & ACT_FINISH_UPLOAD) {
            drain_mic();
            armed = true; /* past MIN_UTTERANCE_MS by definition of the event */
            uplink_arm();
        }
        mic_stop();
    }
    if (actions & ACT_ABORT_UPLOAD) uplink_discard();
    if (actions & ACT_FINISH_UPLOAD) {
        uplink_finish();
        wait_ms = now_ms();
    }
    if (actions & ACT_ABORT_QUERY) uplink_abort();
    if (actions & ACT_START_REPLAY) {
        uplink_begin_replay();
        wait_ms = now_ms();
    }
    if (actions & ACT_START_RECORD) {
        press_ms = now_ms();
        armed = false;
        uplink_begin_query();
        mic_start();
    }
    if (actions & ACT_SET_VOLUME) apply_volume();
}

void voice_poll_volume_save(app_state_t s) {
    if (s != STATE_IDLE || !app_volume_save_due(volume_pending, volume_turn_ms, now_ms())) return;
    volume_pending = false;
    cfg->volume_step = (uint8_t)speaker_get_volume();
    ReturnCode rc = flash_memory_write(cfg);
    printf("[VOICE] volume %u saved to flash: %s\n", (unsigned)cfg->volume_step, rc_to_string(rc));
}
