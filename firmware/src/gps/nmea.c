#include "gps/nmea.h"

#include <string.h>

#define MAX_FIELDS 20

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool nmea_checksum_ok(const char *line) {
    if (line == NULL || line[0] != '$') return false;
    uint8_t sum = 0;
    const char *p = line + 1;
    while (*p != '\0' && *p != '*') sum ^= (uint8_t)*p++;
    if (*p != '*') return false;
    int hi = hex_val(p[1]);
    int lo = hi < 0 ? -1 : hex_val(p[2]);
    if (hi < 0 || lo < 0 || p[3] != '\0') return false;
    return sum == (uint8_t)((hi << 4) | lo);
}

/* Parse "[-]ddd.ddd" into value * 10^decimals (truncated). */
static bool parse_scaled(const char *s, int decimals, int64_t *out) {
    if (*s == '\0') return false;
    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    }
    if (*s < '0' || *s > '9') return false;
    int64_t v = 0;
    while (*s >= '0' && *s <= '9') {
        if (v > INT64_MAX / 20) return false;
        v = v * 10 + (*s++ - '0');
    }
    int got = 0;
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            if (got < decimals) {
                v = v * 10 + (*s - '0');
                got++;
            }
            s++;
        }
    }
    if (*s != '\0') return false;
    for (; got < decimals; got++) v *= 10;
    *out = neg ? -v : v;
    return true;
}

static bool parse_uint(const char *s, uint32_t max, uint32_t *out) {
    int64_t v;
    if (!parse_scaled(s, 0, &v) || v < 0 || v > (int64_t)max) return false;
    *out = (uint32_t)v;
    return true;
}

static int32_t parse_time_ms(const char *s) {
    if (strlen(s) < 6) return -1;
    for (int i = 0; i < 6; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
    }
    int hh = (s[0] - '0') * 10 + (s[1] - '0');
    int mm = (s[2] - '0') * 10 + (s[3] - '0');
    int ss = (s[4] - '0') * 10 + (s[5] - '0');
    if (hh > 23 || mm > 59 || ss > 60) return -1;
    int32_t ms = 0;
    if (s[6] != '\0') {
        if (s[6] != '.') return -1;
        int scale = 100;
        for (const char *p = s + 7; *p != '\0'; p++) {
            if (*p < '0' || *p > '9') return -1;
            ms += (*p - '0') * scale;
            scale /= 10;
        }
    }
    return ((hh * 60 + mm) * 60 + ss) * 1000 + ms;
}

static bool parse_date(const char *s, nmea_msg_t *m) {
    if (strlen(s) != 6) return false;
    uint32_t v;
    if (!parse_uint(s, 999999, &v)) return false;
    uint32_t day = v / 10000, month = (v / 100) % 100, yy = v % 100;
    if (day < 1 || day > 31 || month < 1 || month > 12) return false;
    m->day = (uint8_t)day;
    m->month = (uint8_t)month;
    m->year = (uint16_t)(2000 + yy);
    return true;
}

/* "ddmm.mmmm" + hemisphere -> degrees * 1e7. */
static bool parse_coord(const char *s, char hemi, int deg_digits, int32_t *out) {
    int64_t v; /* ddmm.mmmm * 1e7 */
    if (!parse_scaled(s, 7, &v) || v < 0) return false;
    const char *dot = strchr(s, '.');
    size_t int_len = dot ? (size_t)(dot - s) : strlen(s);
    if (int_len != (size_t)deg_digits + 2) return false;
    int64_t deg = v / (100LL * 10000000LL);
    int64_t min_e7 = v % (100LL * 10000000LL);
    if (min_e7 >= 60LL * 10000000LL) return false;
    int64_t e7 = deg * 10000000LL + (min_e7 + 30) / 60;
    if (deg > (deg_digits == 2 ? 90 : 180)) return false;
    if (hemi == 'S' || hemi == 'W') e7 = -e7;
    else if (hemi != 'N' && hemi != 'E') return false;
    *out = (int32_t)e7;
    return true;
}

static bool parse_position(const char *lat, const char *ns, const char *lon, const char *ew,
                           nmea_msg_t *m) {
    m->pos_valid = parse_coord(lat, ns[0], 2, &m->lat_e7) && parse_coord(lon, ew[0], 3, &m->lon_e7);
    return m->pos_valid;
}

static void fill_rmc(char **f, int n, nmea_msg_t *m) {
    if (n < 10) return;
    m->time_ms = parse_time_ms(f[1]);
    m->status_valid = f[2][0] == 'A';
    parse_position(f[3], f[4], f[5], f[6], m);
    int64_t v;
    if (parse_scaled(f[7], 3, &v) && v >= 0) { /* milli-knots -> cm/s */
        v = v * 514444LL / 10000000LL;
        m->speed_cmps = v > 65535 ? 65535 : (uint32_t)v;
        m->speed_valid = true;
    }
    if (parse_scaled(f[8], 1, &v) && v >= 0 && v < 3600) {
        m->course_deg10 = (uint16_t)v;
        m->course_valid = true;
    }
    m->date_valid = parse_date(f[9], m);
}

static void fill_gga(char **f, int n, nmea_msg_t *m) {
    if (n < 10) return;
    m->time_ms = parse_time_ms(f[1]);
    parse_position(f[2], f[3], f[4], f[5], m);
    uint32_t u;
    m->quality = parse_uint(f[6], 9, &u) ? (uint8_t)u : 0;
    m->sats = parse_uint(f[7], 99, &u) ? (uint8_t)u : 0;
    int64_t v;
    if (parse_scaled(f[8], 1, &v) && v >= 0) {
        m->hdop10 = v > 65535 ? 65535 : (uint16_t)v;
        m->hdop_valid = true;
    }
    if (parse_scaled(f[9], 1, &v) && v > -32768 && v < 32768) {
        m->alt_dm = (int16_t)v;
        m->alt_valid = true;
    }
}

nmea_kind_t nmea_parse_sentence(const char *line, nmea_msg_t *msg) {
    if (!nmea_checksum_ok(line)) return NMEA_BAD;

    char body[NMEA_MAX_LINE + 1];
    size_t len = (size_t)(strchr(line, '*') - (line + 1));
    if (len > NMEA_MAX_LINE) return NMEA_BAD;
    memcpy(body, line + 1, len);
    body[len] = '\0';

    char *fields[MAX_FIELDS];
    int n = 0;
    fields[n++] = body;
    for (char *p = body; *p != '\0' && n < MAX_FIELDS; p++) {
        if (*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }

    size_t id_len = strlen(fields[0]);
    if (id_len != 5) return NMEA_IGNORED;
    nmea_kind_t kind;
    if (strcmp(fields[0] + 2, "RMC") == 0) kind = NMEA_RMC;
    else if (strcmp(fields[0] + 2, "GGA") == 0) kind = NMEA_GGA;
    else return NMEA_IGNORED;

    memset(msg, 0, sizeof(*msg));
    msg->kind = kind;
    msg->time_ms = -1;
    if (kind == NMEA_RMC) fill_rmc(fields, n, msg);
    else fill_gga(fields, n, msg);
    return kind;
}

void nmea_parser_init(nmea_parser_t *p) { memset(p, 0, sizeof(*p)); }

nmea_kind_t nmea_feed(nmea_parser_t *p, uint8_t byte, nmea_msg_t *msg) {
    if (byte == '$') {
        p->len = 0;
        p->active = true;
    }
    if (!p->active) return NMEA_INCOMPLETE;

    if (byte == '\r' || byte == '\n') {
        p->active = false;
        if (p->len == 0) return NMEA_INCOMPLETE;
        p->buf[p->len] = '\0';
        nmea_kind_t k = nmea_parse_sentence(p->buf, msg);
        if (k == NMEA_BAD) p->bad++;
        else if (k == NMEA_IGNORED) p->ignored++;
        else p->ok++;
        return k;
    }

    if (p->len >= NMEA_MAX_LINE) {
        p->active = false;
        p->bad++;
        return NMEA_BAD;
    }
    p->buf[p->len++] = (char)byte;
    return NMEA_INCOMPLETE;
}
