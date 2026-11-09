#include "status/led.h"

#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

static led_mode_t mode = LED_OFF;
static bool level;

bool led_init(void) {
    if (cyw43_arch_init() != 0) return false;
    led_set_mode(LED_OFF);
    return true;
}

void led_set(bool on) {
    mode = on ? LED_SOLID : LED_OFF;
    level = on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void led_set_mode(led_mode_t m) {
    if (m != mode) printf("[LED] mode %s\n", led_mode_name(m));
    mode = m;
    led_tick();
}

void led_tick(void) {
    bool want = led_pattern_level(mode, to_ms_since_boot(get_absolute_time()));
    if (want != level) {
        level = want;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, want);
    }
}
