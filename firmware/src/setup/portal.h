#ifndef PORTAL_H
#define PORTAL_H

#include <stdbool.h>

#include "common/error.h"

/* Starts the setup access point, DHCP, DNS and the config form (F1/F2). */
ReturnCode portal_start(void);

/* True once the form was submitted, validated and written to flash. */
bool portal_config_saved(void);

#endif
