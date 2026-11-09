#include "gps/gps.h"

#include <stdio.h>

#include "config.h"
#include "gps/gps_uart.h"
#include "gps/nmea.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

static uint8_t storage[DEVICE_FIX_BUFFER * FIX_RECORD_WIRE_SIZE];
static fix_buffer_t buffer;
static nmea_parser_t parser;
static fix_builder_t builder;
static uint32_t records;
static bool echo;

void gps_set_echo(bool on) { echo = on; }

void gps_init(void) {
    fix_buffer_init(&buffer, storage, DEVICE_FIX_BUFFER);
    nmea_parser_init(&parser);
    fix_builder_init(&builder);
    gps_uart_init();
}

bool gps_poll(fix_record_t *latest) {
    bool added = false;
    int c;
    while ((c = gps_uart_read()) >= 0) {
        if (echo) putchar_raw((int)c);
        nmea_msg_t msg;
        nmea_kind_t kind = nmea_feed(&parser, (uint8_t)c, &msg);
        if (kind == NMEA_BAD) {
            printf("[GPS] bad sentence (checksum or format), total bad %u\n", (unsigned)parser.bad);
        }
        fix_record_t rec;
        if ((kind == NMEA_RMC || kind == NMEA_GGA) &&
            fix_builder_add(&builder, &msg, time_us_32() / 1000, &rec)) {
            fix_buffer_push(&buffer, &rec);
            records++;
            if (latest != NULL) *latest = rec;
            added = true;
        }
    }
    return added;
}

fix_buffer_t *gps_buffer(void) { return &buffer; }

void gps_get_stats(gps_stats_t *s) {
    s->bytes = gps_uart_bytes();
    s->overruns = gps_uart_overruns();
    s->sentences_ok = parser.ok;
    s->sentences_bad = parser.bad;
    s->sentences_ignored = parser.ignored;
    s->records = records;
}
