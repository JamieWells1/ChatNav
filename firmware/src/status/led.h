#ifndef LED_H
#define LED_H

#include <stdbool.h>

/* Returns false if the CYW43 chip fails to start. */
bool led_init(void);
void led_set(bool on);

#endif
