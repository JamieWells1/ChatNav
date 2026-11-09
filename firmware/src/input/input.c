/*
 * Copyright (c) 2026 Jamie Wells. All Rights Reserved.
 * Proprietary and Confidential - Unauthorized copying or distribution prohibited.
 * Adapted from Photon src/input/input.*
 */

#include "input/input.h"

#include <stdio.h>

#include "config.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define DEBOUNCE_TIME_US 200
#define LONG_PRESS_TIME_MS 500
#define ROTATOR_HALF_STEP 0 /* 1 for encoders with two edges per detent */

void input_rotator_init(Rotator *rtr) {
    gpio_init(PIN_ENC_VCC);
    gpio_set_dir(PIN_ENC_VCC, GPIO_OUT);
    gpio_put(PIN_ENC_VCC, 1);

    const uint8_t pins[] = {PIN_ENC_CLK, PIN_ENC_DT, PIN_ENC_SW};
    for (unsigned i = 0; i < sizeof(pins); i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    sleep_ms(5); /* let the supply and pull-ups settle */

    *rtr = (Rotator){.pin_clk = PIN_ENC_CLK, .pin_dt = PIN_ENC_DT, .pin_sw = PIN_ENC_SW};
    rtr->current_clk_state = gpio_get(rtr->pin_clk);
    rtr->current_dt_state = gpio_get(rtr->pin_dt);
    rtr->current_sw_state = gpio_get(rtr->pin_sw);
    rtr->last_clk_state = rtr->current_clk_state;
    rtr->last_sw_state = rtr->current_sw_state;
    rtr->state = (rtr->current_clk_state << 1) | rtr->current_dt_state;
    printf("[ENC] powered on GP%d, CLK GP%d DT GP%d SW GP%d\n", PIN_ENC_VCC, PIN_ENC_CLK,
           PIN_ENC_DT, PIN_ENC_SW);
}

void input_rtr_update(Rotator *rtr) {
    rtr->last_clk_state = rtr->current_clk_state;
    rtr->last_sw_state = rtr->current_sw_state;

    rtr->current_clk_state = gpio_get(rtr->pin_clk);
    rtr->current_dt_state = gpio_get(rtr->pin_dt);
    rtr->current_sw_state = gpio_get(rtr->pin_sw);

    if (input_rtr_pressed(rtr)) {
        rtr->press_start_time = time_us_32() / 1000;
        rtr->long_press_triggered = false;
    }
    if (input_rtr_released(rtr)) {
        rtr->long_press_triggered = false;
    }

    uint8_t new_state = (rtr->current_clk_state << 1) | rtr->current_dt_state;
    uint32_t now = time_us_32();
    if (new_state != rtr->state && (now - rtr->last_change_time) > DEBOUNCE_TIME_US) {
        static const int8_t transition_table[4][4] = {
            {0, -1, 1, 0},  /* from 00 */
            {1, 0, 0, -1},  /* from 01 */
            {-1, 0, 0, 1},  /* from 10 */
            {0, 1, -1, 0},  /* from 11 */
        };
        int8_t dir = transition_table[rtr->state][new_state];
        bool at_detent = new_state == 3 || (ROTATOR_HALF_STEP && new_state == 0);
        if (dir != 0 && at_detent) {
            rtr->rotation_count += dir;
            rtr->last_change_time = now;
        }
        rtr->state = new_state;
    }
}

bool input_rtr_cw(Rotator *rtr) {
    if (rtr->rotation_count > 0) {
        rtr->rotation_count--;
        return true;
    }
    return false;
}

bool input_rtr_anti_cw(Rotator *rtr) {
    if (rtr->rotation_count < 0) {
        rtr->rotation_count++;
        return true;
    }
    return false;
}

bool input_rtr_pressed(Rotator *rtr) {
    return rtr->last_sw_state == true && rtr->current_sw_state == false;
}

bool input_rtr_released(Rotator *rtr) {
    return rtr->last_sw_state == false && rtr->current_sw_state == true;
}

bool input_rtr_long_pressed(Rotator *rtr) {
    if (rtr->current_sw_state == false && !rtr->long_press_triggered) {
        uint32_t held = time_us_32() / 1000 - rtr->press_start_time;
        if (held >= LONG_PRESS_TIME_MS) {
            rtr->long_press_triggered = true;
            return true;
        }
    }
    return false;
}
