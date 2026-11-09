#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>

#include "gps/fix_buffer.h"
#include "gps/fix_record.h"

typedef struct {
    uint32_t bytes;
    uint32_t overruns;
    uint32_t sentences_ok;
    uint32_t sentences_bad;
    uint32_t sentences_ignored;
    uint32_t records;
} gps_stats_t;

void gps_init(void);

/* Drains received bytes, parses, and appends completed fixes to the buffer.
 * Returns true if at least one record was added; the newest is in *latest. */
bool gps_poll(fix_record_t *latest);

/* Diagnostics: copy every received byte to USB serial. */
void gps_set_echo(bool on);

fix_buffer_t *gps_buffer(void);
void gps_get_stats(gps_stats_t *s);

#endif
