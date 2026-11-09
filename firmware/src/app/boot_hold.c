#include "app/boot_hold.h"

#include <stdio.h>

#include "config.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "status/led.h"

#define POLL_MS 20

static bool button_down(void) { return !gpio_get(PIN_ENC_SW); }

bool boot_hold_requested(void) {
    if (!button_down()) return false;
    printf("[BOOT] button held, keep holding %d s for setup mode\n", SETUP_HOLD_S);
    led_set_mode(LED_FAST_BLINK);
    for (int waited = 0; waited < SETUP_HOLD_S * 1000; waited += POLL_MS) {
        if (!button_down()) {
            printf("[BOOT] button released after %d ms, normal boot\n", waited);
            led_set_mode(LED_OFF);
            return false;
        }
        led_tick();
        sleep_ms(POLL_MS);
    }
    printf("[BOOT] setup mode requested\n");
    return true;
}
