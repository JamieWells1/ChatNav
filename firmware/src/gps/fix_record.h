#ifndef FIX_RECORD_H
#define FIX_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "gps/nmea.h"

#define FIX_RECORD_WIRE_SIZE 28

#define FIX_FLAG_TIME_VALID 0x01 /* utc_s/utc_ms hold a real UTC time */
#define FIX_FLAG_RMC_VALID 0x02  /* RMC status A */
#define FIX_FLAG_GGA 0x04        /* GGA data present */
#define FIX_FLAG_POS 0x08        /* lat/lon present */

typedef struct {
    uint32_t device_ms;
    uint32_t utc_s;
    uint16_t utc_ms;
    int32_t lat_e7;
    int32_t lon_e7;
    uint16_t speed_cmps;
    uint16_t course_deg10;
    int16_t alt_dm;
    uint8_t sats;
    uint8_t quality;
    uint8_t hdop10;
    uint8_t flags;
} fix_record_t;

/* Little-endian wire layout, FIX_RECORD_WIRE_SIZE bytes, field order as above. */
void fix_record_pack(const fix_record_t *r, uint8_t out[FIX_RECORD_WIRE_SIZE]);
void fix_record_unpack(const uint8_t in[FIX_RECORD_WIRE_SIZE], fix_record_t *r);

/* Seconds since 1970-01-01 for a civil UTC date and time of day. */
uint32_t fix_utc_epoch(uint16_t year, uint8_t month, uint8_t day, int32_t time_ms);

/* Joins the RMC and GGA of one epoch (same UTC time) into one record. */
typedef struct {
    bool have_rmc;
    bool have_gga;
    nmea_msg_t rmc;
    nmea_msg_t gga;
    uint32_t rmc_ms;
    uint32_t gga_ms;
} fix_builder_t;

void fix_builder_init(fix_builder_t *b);

/* Returns true when a record was completed (or a stale half flushed) into out. */
bool fix_builder_add(fix_builder_t *b, const nmea_msg_t *m, uint32_t device_ms, fix_record_t *out);

#endif
