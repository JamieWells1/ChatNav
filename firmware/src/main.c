#include <stdio.h>

#include "config.h"
#include "pico/stdlib.h"
#include "status/led.h"

int main(void) {
    stdio_init_all();
    if (!led_init()) {
        printf("[MAIN] cyw43 init failed\n");
        return 1;
    }
    printf("[MAIN] chatnav boot\n");

    /* M1: cycle the LED through every state so each pattern can be seen. */
    const led_mode_t modes[] = {LED_OFF, LED_SLOW_BLINK, LED_FAST_BLINK, LED_SOLID, LED_FLASHING};
    unsigned i = 0;
    absolute_time_t next = get_absolute_time();
    while (true) {
        if (absolute_time_diff_us(get_absolute_time(), next) <= 0) {
            led_set_mode(modes[i++ % (sizeof(modes) / sizeof(modes[0]))]);
            next = make_timeout_time_ms(4000);
        }
        led_tick();
        sleep_ms(5);
    }
}
