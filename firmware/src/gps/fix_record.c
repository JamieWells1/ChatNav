#include "gps/fix_record.h"

#include <string.h>

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
    put16(p, (uint16_t)v);
    put16(p + 2, (uint16_t)(v >> 16));
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint32_t get32(const uint8_t *p) { return get16(p) | ((uint32_t)get16(p + 2) << 16); }

void fix_record_pack(const fix_record_t *r, uint8_t out[FIX_RECORD_WIRE_SIZE]) {
    put32(out + 0, r->device_ms);
    put32(out + 4, r->utc_s);
    put16(out + 8, r->utc_ms);
    put32(out + 10, (uint32_t)r->lat_e7);
    put32(out + 14, (uint32_t)r->lon_e7);
    put16(out + 18, r->speed_cmps);
    put16(out + 20, r->course_deg10);
    put16(out + 22, (uint16_t)r->alt_dm);
    out[24] = r->sats;
    out[25] = r->quality;
    out[26] = r->hdop10;
    out[27] = r->flags;
}

void fix_record_unpack(const uint8_t in[FIX_RECORD_WIRE_SIZE], fix_record_t *r) {
    r->device_ms = get32(in + 0);
    r->utc_s = get32(in + 4);
    r->utc_ms = get16(in + 8);
    r->lat_e7 = (int32_t)get32(in + 10);
    r->lon_e7 = (int32_t)get32(in + 14);
    r->speed_cmps = get16(in + 18);
    r->course_deg10 = get16(in + 20);
    r->alt_dm = (int16_t)get16(in + 22);
    r->sats = in[24];
    r->quality = in[25];
    r->hdop10 = in[26];
    r->flags = in[27];
}

uint32_t fix_utc_epoch(uint16_t year, uint8_t month, uint8_t day, int32_t time_ms) {
    int32_t y = year - (month <= 2);
    int32_t era = y / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int32_t days = era * 146097 + doe - 719468;
    return (uint32_t)days * 86400u + (uint32_t)(time_ms / 1000);
}

void fix_builder_init(fix_builder_t *b) { memset(b, 0, sizeof(*b)); }

static void build_record(const fix_builder_t *b, fix_record_t *out) {
    memset(out, 0, sizeof(*out));
    out->device_ms = b->have_rmc ? b->rmc_ms : b->gga_ms;

    if (b->have_rmc) {
        const nmea_msg_t *r = &b->rmc;
        if (r->status_valid) out->flags |= FIX_FLAG_RMC_VALID;
        if (r->time_ms >= 0 && r->date_valid) {
            out->flags |= FIX_FLAG_TIME_VALID;
            out->utc_s = fix_utc_epoch(r->year, r->month, r->day, r->time_ms);
            out->utc_ms = (uint16_t)(r->time_ms % 1000);
        }
        if (r->pos_valid) {
            out->flags |= FIX_FLAG_POS;
            out->lat_e7 = r->lat_e7;
            out->lon_e7 = r->lon_e7;
        }
        out->speed_cmps = r->speed_cmps;
        out->course_deg10 = r->course_deg10;
    }
    if (b->have_gga) {
        const nmea_msg_t *g = &b->gga;
        out->flags |= FIX_FLAG_GGA;
        out->quality = g->quality;
        out->sats = g->sats;
        out->hdop10 = g->hdop10 > 255 ? 255 : (uint8_t)g->hdop10;
        out->alt_dm = g->alt_dm;
        if (!(out->flags & FIX_FLAG_POS) && g->pos_valid && g->quality > 0) {
            out->flags |= FIX_FLAG_POS;
            out->lat_e7 = g->lat_e7;
            out->lon_e7 = g->lon_e7;
        }
    }
}

bool fix_builder_add(fix_builder_t *b, const nmea_msg_t *m, uint32_t device_ms, fix_record_t *out) {
    if (m->kind != NMEA_RMC && m->kind != NMEA_GGA) return false;
    bool is_rmc = m->kind == NMEA_RMC;
    bool have_other = is_rmc ? b->have_gga : b->have_rmc;
    int32_t other_time = is_rmc ? b->gga.time_ms : b->rmc.time_ms;

    if (have_other && other_time == m->time_ms) {
        if (is_rmc) {
            b->rmc = *m;
            b->rmc_ms = device_ms;
            b->have_rmc = true;
        } else {
            b->gga = *m;
            b->gga_ms = device_ms;
            b->have_gga = true;
        }
        build_record(b, out);
        fix_builder_init(b);
        return true;
    }

    bool flushed = false;
    if (have_other) {
        build_record(b, out);
        flushed = true;
        fix_builder_init(b);
    } else if (is_rmc ? b->have_rmc : b->have_gga) {
        /* Same sentence type again without a partner: flush the older one. */
        build_record(b, out);
        flushed = true;
        fix_builder_init(b);
    }

    if (is_rmc) {
        b->rmc = *m;
        b->rmc_ms = device_ms;
        b->have_rmc = true;
    } else {
        b->gga = *m;
        b->gga_ms = device_ms;
        b->have_gga = true;
    }
    return flushed;
}
