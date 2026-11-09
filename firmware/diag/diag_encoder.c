/* Diagnostic: rotary encoder only. Logs rotation, press, release and long press. */
#include <stdio.h>

#include "input/input.h"
#include "pico/stdlib.h"

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("[ENC] diag_encoder start\n");

    Rotator rtr;
    input_rotator_init(&rtr);
    printf("[ENC] initial CLK=%d DT=%d SW=%d (SW 1 = released)\n", rtr.current_clk_state,
           rtr.current_dt_state, rtr.current_sw_state);

    int position = 0;
    uint32_t pressed_at = 0;
    uint32_t last_heartbeat = to_ms_since_boot(get_absolute_time());
    while (true) {
        input_rtr_update(&rtr);
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (input_rtr_cw(&rtr)) {
            position++;
            printf("[ENC] rotate clockwise, position %d\n", position);
        }
        if (input_rtr_anti_cw(&rtr)) {
            position--;
            printf("[ENC] rotate anticlockwise, position %d\n", position);
        }
        if (input_rtr_pressed(&rtr)) {
            pressed_at = now;
            printf("[ENC] press\n");
        }
        if (input_rtr_released(&rtr)) {
            printf("[ENC] release after %lu ms\n", (unsigned long)(now - pressed_at));
        }
        if (input_rtr_long_pressed(&rtr)) {
            printf("[ENC] long press\n");
        }
        if (now - last_heartbeat >= 5000) {
            last_heartbeat = now;
            printf("[ENC] alive, CLK=%d DT=%d SW=%d\n", rtr.current_clk_state, rtr.current_dt_state,
                   rtr.current_sw_state);
        }
        sleep_us(100);
    }
}
