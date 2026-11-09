/* Diagnostic: GPS only. Prints each fix and a status line every 5 s. */
#include <stdio.h>

#include "config.h"
#include "gps/gps.h"
#include "pico/stdlib.h"

#define STATUS_INTERVAL_MS 5000

int main(void) {
    stdio_init_all();
    sleep_ms(3000); /* time to open the serial monitor */
    printf("[GPS] diag_gps start\n");
    gps_init();
    gps_set_echo(false);

    uint32_t next_status = to_ms_since_boot(get_absolute_time()) + STATUS_INTERVAL_MS;
    while (true) {
        fix_record_t r;
        if (gps_poll(&r)) {
            printf("[GPS] fix %s q=%u sats=%u hdop=%u.%u lat=%ld lon=%ld speed=%ucm/s course=%u.%u "
                   "alt=%ddm utc=%lu.%03u flags=0x%02x\n",
                   (r.flags & FIX_FLAG_RMC_VALID) ? "VALID" : "no-fix", r.quality, r.sats,
                   r.hdop10 / 10, r.hdop10 % 10, (long)r.lat_e7, (long)r.lon_e7, r.speed_cmps,
                   r.course_deg10 / 10, r.course_deg10 % 10, r.alt_dm, (unsigned long)r.utc_s,
                   r.utc_ms, r.flags);
        }
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now >= next_status) {
            next_status = now + STATUS_INTERVAL_MS;
            gps_stats_t s;
            gps_get_stats(&s);
            printf("[GPS] status bytes=%lu overruns=%lu ok=%lu bad=%lu ignored=%lu records=%lu "
                   "buffered=%u drops=%lu\n",
                   (unsigned long)s.bytes, (unsigned long)s.overruns, (unsigned long)s.sentences_ok,
                   (unsigned long)s.sentences_bad, (unsigned long)s.sentences_ignored,
                   (unsigned long)s.records, (unsigned)fix_buffer_count(gps_buffer()),
                   (unsigned long)fix_buffer_drops(gps_buffer()));
            if (s.bytes == 0) {
                printf("[GPS] no bytes received: check module TXD -> GP%d, power and ground\n",
                       PIN_GPS_RX);
            } else if (s.sentences_ok == 0 && s.sentences_bad > 0) {
                printf("[GPS] only bad sentences: check baud rate (%d)\n", GPS_BAUD);
            }
        }
        sleep_ms(5);
    }
}
