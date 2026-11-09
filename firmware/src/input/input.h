/*
 * Copyright (c) 2026 Jamie Wells. All Rights Reserved.
 * Proprietary and Confidential - Unauthorized copying or distribution prohibited.
 * Adapted from Photon src/input/input.*
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t pin_clk;
    uint8_t pin_dt;
    uint8_t pin_sw;
    bool current_clk_state;
    bool current_dt_state;
    bool current_sw_state;
    bool last_clk_state;
    bool last_sw_state;
    uint8_t state;              /* quadrature state 0-3 */
    int8_t rotation_count;      /* positive = clockwise, negative = anticlockwise */
    uint32_t last_change_time;  /* debounce, microseconds */
    uint32_t press_start_time;  /* milliseconds */
    bool long_press_triggered;
} Rotator;

/* Powers the encoder from PIN_ENC_VCC, sets pull-ups and reads the starting state. */
void input_rotator_init(Rotator *rtr);

/* Call every few hundred microseconds or faster from the main loop. */
void input_rtr_update(Rotator *rtr);

/* Each of cw/anti_cw consumes one detent. */
bool input_rtr_cw(Rotator *rtr);
bool input_rtr_anti_cw(Rotator *rtr);
bool input_rtr_pressed(Rotator *rtr);
bool input_rtr_released(Rotator *rtr);
bool input_rtr_long_pressed(Rotator *rtr);

#endif
