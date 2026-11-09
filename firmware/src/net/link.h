#ifndef LINK_H
#define LINK_H

#include "storage/device_config.h"

/* Wi-Fi join, pinned TLS connect and auth check, with retry. Drives the CONNECTING state
 * and watches the link while IDLE. */

typedef enum {
    LINK_CONNECTING,
    LINK_UP,
    LINK_LOST, /* was up, dropped; link_poll keeps retrying and returns CONNECTING */
} link_status_t;

/* cfg must stay valid for the life of the link. */
void link_start(const device_config_t *cfg);

/* Call often from the main loop. */
link_status_t link_poll(void);

#endif
