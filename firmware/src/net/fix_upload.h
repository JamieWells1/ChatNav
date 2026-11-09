#ifndef FIX_UPLOAD_H
#define FIX_UPLOAD_H

#include <stdbool.h>

/* Hook for M4 (POST /v1/fixes). The main loop calls fix_upload_poll every pass in every
 * state except SETUP, and fix_upload_flush when the button is pressed so the server sees
 * the last seconds of driving (U2). Both do nothing until M4. */

void fix_upload_poll(bool link_up);
void fix_upload_flush(void);

#endif
