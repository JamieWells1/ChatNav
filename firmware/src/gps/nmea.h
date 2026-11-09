#ifndef NMEA_H
#define NMEA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NMEA_MAX_LINE 120

typedef enum {
    NMEA_INCOMPLETE, /* still assembling a line */
    NMEA_IGNORED,    /* valid checksum, sentence type we do not use */
    NMEA_BAD,        /* checksum or format failure */
    NMEA_RMC,
    NMEA_GGA,
} nmea_kind_t;

typedef struct {
    nmea_kind_t kind;
    int32_t time_ms; /* ms since UTC midnight, -1 if absent */
    bool date_valid;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    bool status_valid; /* RMC status A */
    bool pos_valid;
    int32_t lat_e7;
    int32_t lon_e7;
    bool speed_valid;
    uint32_t speed_cmps;
    bool course_valid;
    uint16_t course_deg10;
    uint8_t quality; /* GGA fix quality, 0 = none */
    uint8_t sats;
    bool hdop_valid;
    uint16_t hdop10;
    bool alt_valid;
    int16_t alt_dm; /* altitude in decimetres */
} nmea_msg_t;

typedef struct {
    char buf[NMEA_MAX_LINE + 1];
    size_t len;
    bool active;
    uint32_t ok;
    uint32_t bad;
    uint32_t ignored;
} nmea_parser_t;

void nmea_parser_init(nmea_parser_t *p);

/* Feed one byte. Returns NMEA_INCOMPLETE until a line ends; msg is set for RMC/GGA. */
nmea_kind_t nmea_feed(nmea_parser_t *p, uint8_t byte, nmea_msg_t *msg);

/* Parse one full sentence ("$...*hh", no line ending). */
nmea_kind_t nmea_parse_sentence(const char *line, nmea_msg_t *msg);

bool nmea_checksum_ok(const char *line);

#endif
