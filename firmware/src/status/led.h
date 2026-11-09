#ifndef LED_H
#define LED_H

#include <stdbool.h>

#include "status/led_pattern.h"

/* The LED is wired to the CYW43 chip: call wifi_init() first. */
void led_init(void);

/* Direct control; cancels any pattern. */
void led_set(bool on);

/* Selects a pattern; call led_tick often from the main loop to drive it. */
void led_set_mode(led_mode_t mode);
void led_tick(void);

#endif
