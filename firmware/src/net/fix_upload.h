#ifndef FIX_UPLOAD_H
#define FIX_UPLOAD_H

#include <stdbool.h>

#include "storage/device_config.h"

/* POST /v1/fixes over the kept-open connection (F11-F14). Records stay in the GPS ring
 * buffer until the server acknowledges the batch sequence; a failed batch is retried.
 * The main loop calls fix_upload_poll every pass in every state except SETUP. A batch only
 * starts while the single connection is free (conn_free), so it never delays or interrupts
 * a voice query; a batch already in flight when a query starts is finished first, and
 * uplink_poll holds the query head back until fix_upload_busy is false. */

/* cfg must outlive the uploader. */
void fix_upload_init(const device_config_t *cfg);

void fix_upload_poll(bool link_up, bool conn_free);

/* Button press: send what is buffered now (U2) if the connection is free, otherwise as soon
 * as it is. Never blocks. */
void fix_upload_flush(void);

/* A batch is being written or awaiting its reply, so the connection is in use. */
bool fix_upload_busy(void);

#endif
