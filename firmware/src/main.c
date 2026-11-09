#include <stdio.h>

#include "app/app_state.h"
#include "app/boot_hold.h"
#include "app/voice.h"
#include "audio/speaker.h"
#include "common/error.h"
#include "config.h"
#include "gps/gps.h"
#include "hardware/watchdog.h"
#include "input/input.h"
#include "net/fix_upload.h"
#include "net/link.h"
#include "net/uplink.h"
#include "net/wifi.h"
#include "pico/stdlib.h"
#include "setup/portal.h"
#include "status/led.h"
#include "storage/flash_memory.h"

#define REBOOT_DELAY_MS 2000
#define GPS_LOG_INTERVAL_MS 10000

static app_state_t state = STATE_BOOT;
static device_config_t cfg;

/* Applies one event: logs it, runs the effects, switches state and plays the cue. */
static void transition(app_event_t ev) {
    app_transition_t t = app_step(state, ev);
    if (t.next == state && t.cue == CUE_NONE && t.actions == ACT_NONE) {
        printf("[STATE] %s ignores %s\n", app_state_name(state), app_event_name(ev));
        return;
    }
    if (t.next != state) {
        printf("[STATE] %s -> %s on %s\n", app_state_name(state), app_state_name(t.next),
               app_event_name(ev));
    }
    voice_do(t.actions, state);
    state = t.next;
    led_set_mode(app_state_led(state));
    if (t.cue != CUE_NONE) speaker_play_cue((cue_id_t)t.cue);
}

/* Decides between setup and normal boot from the flash config and the held button. */
static void boot(void) {
    Rotator rtr;
    input_rotator_init(&rtr);
    bool setup_requested = boot_hold_requested();

    ReturnCode rc = flash_memory_read(&cfg);
    bool valid = RC_SUCCESS(rc) && device_config_validate(&cfg) == CFG_OK;
    printf("[BOOT] config %s\n", valid ? "valid" : "missing or invalid");

    if (setup_requested) {
        transition(EV_SETUP_REQUESTED);
    } else {
        transition(valid ? EV_CONFIG_VALID : EV_CONFIG_MISSING);
    }
}

static void run_setup(void) {
    static bool started, saved_seen;
    static absolute_time_t reboot_at;

    if (!started) {
        started = true;
        if (RC_FAILED(portal_start())) printf("[SETUP] portal failed to start\n");
    }
    if (!saved_seen && portal_config_saved()) {
        saved_seen = true;
        reboot_at = make_timeout_time_ms(REBOOT_DELAY_MS);
        printf("[SETUP] saved, rebooting in %d ms\n", REBOOT_DELAY_MS);
    }
    if (saved_seen && absolute_time_diff_us(get_absolute_time(), reboot_at) <= 0) {
        watchdog_reboot(0, 0, 0);
    }
}

static void run_link(void) {
    static bool started;
    if (!started) {
        started = true;
        link_start(&cfg);
    }
    link_status_t s = link_poll();
    if (s == LINK_UP && state == STATE_CONNECTING) transition(EV_LINK_UP);
    if (s == LINK_LOST && state != STATE_CONNECTING) transition(EV_LINK_LOST);
}

static void log_gps(void) {
    static uint32_t last;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last < GPS_LOG_INTERVAL_MS) return;
    last = now;
    gps_stats_t g;
    gps_get_stats(&g);
    printf("[GPS] %u records, %u bytes, %u bad sentences, %u uart overruns, %u buffered\n",
           (unsigned)g.records, (unsigned)g.bytes, (unsigned)g.sentences_bad,
           (unsigned)g.overruns, (unsigned)fix_buffer_count(gps_buffer()));
}

/* Everything except SETUP: GPS, link watch, upload pump, button and encoder. */
static void run_active(void) {
    gps_poll(NULL);
    log_gps();
    run_link();
    fix_upload_poll(state != STATE_CONNECTING);
    uplink_poll();

    int ev = voice_poll_input(state);
    if (ev != VOICE_NO_EVENT) transition((app_event_t)ev);
    ev = voice_poll(state);
    if (ev != VOICE_NO_EVENT) transition((app_event_t)ev);
    voice_poll_volume_save(state);
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500); /* give the USB serial console time to attach */
    printf("[MAIN] chatnav boot\n");

    if (RC_FAILED(wifi_init())) return 1;
    led_init();
    boot();
    if (state != STATE_SETUP) {
        gps_init();
        voice_init(&cfg);
    }

    while (true) {
        switch (state) {
            case STATE_SETUP:
                run_setup();
                break;
            case STATE_BOOT:
                break;
            default:
                run_active();
                break;
        }
        led_tick();
        sleep_us(100); /* the encoder decoder needs frequent polls */
    }
}
