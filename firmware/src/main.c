#include <stdio.h>

#include "app/app_state.h"
#include "app/boot_hold.h"
#include "common/error.h"
#include "config.h"
#include "hardware/watchdog.h"
#include "input/input.h"
#include "net/link.h"
#include "net/wifi.h"
#include "pico/stdlib.h"
#include "setup/portal.h"
#include "status/led.h"
#include "storage/flash_memory.h"

#define REBOOT_DELAY_MS 2000

static app_state_t state = STATE_BOOT;
static device_config_t cfg;

static void transition(app_event_t ev) {
    app_state_t next = app_state_next(state, ev);
    if (next == state) return;
    printf("[STATE] %s -> %s\n", app_state_name(state), app_state_name(next));
    state = next;
    led_set_mode(app_state_led(state));
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
    if (s == LINK_UP) transition(EV_LINK_UP);
    if (s == LINK_LOST) transition(EV_LINK_LOST);
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500); /* give the USB serial console time to attach */
    printf("[MAIN] chatnav boot\n");

    if (RC_FAILED(wifi_init())) return 1;
    led_init();
    boot();

    while (true) {
        switch (state) {
            case STATE_SETUP:
                run_setup();
                break;
            case STATE_CONNECTING:
            case STATE_IDLE:
                run_link();
                break;
            case STATE_BOOT:
                break;
        }
        led_tick();
        sleep_ms(5);
    }
}
