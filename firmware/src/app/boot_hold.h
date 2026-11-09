#ifndef BOOT_HOLD_H
#define BOOT_HOLD_H

#include <stdbool.h>

/* F2: true if the encoder button was already held at power-on and stayed held for
 * SETUP_HOLD_S. Returns immediately (false) when the button is not held at boot. */
bool boot_hold_requested(void);

#endif
