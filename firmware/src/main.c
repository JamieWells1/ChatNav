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

    bool on = false;
    while (true) {
        on = !on;
        led_set(on);
        printf("[LED] %s\n", on ? "on" : "off");
        sleep_ms(500);
    }
}
