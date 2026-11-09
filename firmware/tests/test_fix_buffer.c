#include <string.h>

#include "gps/fix_buffer.h"
#include "gps/fix_record.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_pack_roundtrip(void) {
    fix_record_t r = {.device_ms = 0xA1B2C3D4,
                      .utc_s = 1794227719,
                      .utc_ms = 450,
                      .lat_e7 = -338500000,
                      .lon_e7 = 1512000000,
                      .speed_cmps = 1152,
                      .course_deg10 = 3599,
                      .alt_dm = -12,
                      .sats = 9,
                      .quality = 2,
                      .hdop10 = 17,
                      .flags = 0x0F};
    uint8_t w[FIX_RECORD_WIRE_SIZE];
    fix_record_pack(&r, w);
    fix_record_t u;
    fix_record_unpack(w, &u);
    TEST_ASSERT_EQUAL_UINT32(r.device_ms, u.device_ms);
    TEST_ASSERT_EQUAL_UINT32(r.utc_s, u.utc_s);
    TEST_ASSERT_EQUAL_UINT16(r.utc_ms, u.utc_ms);
    TEST_ASSERT_EQUAL_INT32(r.lat_e7, u.lat_e7);
    TEST_ASSERT_EQUAL_INT32(r.lon_e7, u.lon_e7);
    TEST_ASSERT_EQUAL_UINT16(r.speed_cmps, u.speed_cmps);
    TEST_ASSERT_EQUAL_UINT16(r.course_deg10, u.course_deg10);
    TEST_ASSERT_EQUAL_INT16(r.alt_dm, u.alt_dm);
    TEST_ASSERT_EQUAL_UINT8(r.sats, u.sats);
    TEST_ASSERT_EQUAL_UINT8(r.quality, u.quality);
    TEST_ASSERT_EQUAL_UINT8(r.hdop10, u.hdop10);
    TEST_ASSERT_EQUAL_UINT8(r.flags, u.flags);
}

static void test_pack_is_little_endian_and_28_bytes(void) {
    fix_record_t r = {.device_ms = 0x04030201, .flags = 0x7E, .sats = 5};
    uint8_t w[FIX_RECORD_WIRE_SIZE];
    fix_record_pack(&r, w);
    TEST_ASSERT_EQUAL_INT(28, FIX_RECORD_WIRE_SIZE);
    TEST_ASSERT_EQUAL_UINT8(0x01, w[0]);
    TEST_ASSERT_EQUAL_UINT8(0x04, w[3]);
    TEST_ASSERT_EQUAL_UINT8(5, w[24]);
    TEST_ASSERT_EQUAL_UINT8(0x7E, w[27]);
}

static void test_utc_epoch(void) {
    TEST_ASSERT_EQUAL_UINT32(1794227719u, fix_utc_epoch(2026, 11, 9, (12 * 3600 + 35 * 60 + 19) * 1000));
    TEST_ASSERT_EQUAL_UINT32(946684800u, fix_utc_epoch(2000, 1, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(1709251199u, fix_utc_epoch(2024, 2, 29, 86399000));
}

static void test_builder_pairs_gga_then_rmc_and_rmc_then_gga(void) {
    nmea_msg_t rmc = {.kind = NMEA_RMC, .time_ms = 5000, .date_valid = true, .year = 2026, .month = 1,
                      .day = 2, .status_valid = true, .pos_valid = true, .lat_e7 = 10, .lon_e7 = 20};
    nmea_msg_t gga = {.kind = NMEA_GGA, .time_ms = 5000, .quality = 1, .sats = 7, .hdop10 = 12};
    fix_builder_t b;
    fix_record_t r;
    fix_builder_init(&b);
    TEST_ASSERT_FALSE(fix_builder_add(&b, &gga, 1, &r));
    TEST_ASSERT_TRUE(fix_builder_add(&b, &rmc, 2, &r));
    TEST_ASSERT_EQUAL_UINT8(7, r.sats);
    TEST_ASSERT_EQUAL_INT32(10, r.lat_e7);
    TEST_ASSERT_TRUE(r.flags & FIX_FLAG_TIME_VALID);
    TEST_ASSERT_TRUE(r.flags & FIX_FLAG_GGA);
    TEST_ASSERT_TRUE(r.flags & FIX_FLAG_RMC_VALID);

    TEST_ASSERT_FALSE(fix_builder_add(&b, &rmc, 3, &r));
    TEST_ASSERT_TRUE(fix_builder_add(&b, &gga, 4, &r));
    TEST_ASSERT_EQUAL_UINT8(1, r.quality);
    TEST_ASSERT_EQUAL_UINT32(3, r.device_ms);
}

static void test_builder_flushes_unpaired_half(void) {
    nmea_msg_t rmc1 = {.kind = NMEA_RMC, .time_ms = 1000, .date_valid = true, .year = 2026, .month = 1,
                       .day = 2};
    nmea_msg_t rmc2 = rmc1;
    rmc2.time_ms = 2000;
    fix_builder_t b;
    fix_record_t r;
    fix_builder_init(&b);
    TEST_ASSERT_FALSE(fix_builder_add(&b, &rmc1, 1, &r));
    TEST_ASSERT_TRUE(fix_builder_add(&b, &rmc2, 2, &r));
    TEST_ASSERT_FALSE(r.flags & FIX_FLAG_GGA);
    TEST_ASSERT_FALSE(r.flags & FIX_FLAG_RMC_VALID);
    TEST_ASSERT_EQUAL_UINT8(0, r.quality);
}

static void test_buffer_fifo_and_peek(void) {
    uint8_t store[4 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 4);
    TEST_ASSERT_EQUAL_UINT32(1, fix_buffer_push(&fb, &(fix_record_t){.device_ms = 10}));
    TEST_ASSERT_EQUAL_UINT32(2, fix_buffer_push(&fb, &(fix_record_t){.device_ms = 20}));
    uint8_t out[4 * FIX_RECORD_WIRE_SIZE];
    uint32_t first = 0;
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_peek(&fb, out, 4, &first));
    TEST_ASSERT_EQUAL_UINT32(1, first);
    fix_record_t u;
    fix_record_unpack(out + FIX_RECORD_WIRE_SIZE, &u);
    TEST_ASSERT_EQUAL_UINT32(20, u.device_ms);
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_count(&fb)); /* peek does not release */
    TEST_ASSERT_EQUAL_size_t(1, fix_buffer_peek(&fb, out, 1, &first));
}

static void test_buffer_overwrites_oldest_and_counts_drops(void) {
    uint8_t store[3 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 3);
    for (uint32_t i = 1; i <= 5; i++) fix_buffer_push(&fb, &(fix_record_t){.device_ms = i});
    TEST_ASSERT_EQUAL_size_t(3, fix_buffer_count(&fb));
    TEST_ASSERT_EQUAL_UINT32(2, fix_buffer_drops(&fb));
    uint8_t out[3 * FIX_RECORD_WIRE_SIZE];
    uint32_t first;
    TEST_ASSERT_EQUAL_size_t(3, fix_buffer_peek(&fb, out, 3, &first));
    TEST_ASSERT_EQUAL_UINT32(3, first);
    for (uint32_t i = 0; i < 3; i++) {
        fix_record_t u;
        fix_record_unpack(out + i * FIX_RECORD_WIRE_SIZE, &u);
        TEST_ASSERT_EQUAL_UINT32(3 + i, u.device_ms);
    }
}

static void test_ack_by_sequence(void) {
    uint8_t store[4 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 4);
    for (uint32_t i = 1; i <= 4; i++) fix_buffer_push(&fb, &(fix_record_t){.device_ms = i});
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 2));
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_count(&fb));
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 2)); /* repeat is harmless */
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_count(&fb));
    TEST_ASSERT_FALSE(fix_buffer_ack(&fb, 99)); /* never issued */
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_count(&fb));

    uint8_t out[4 * FIX_RECORD_WIRE_SIZE];
    uint32_t first;
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_peek(&fb, out, 4, &first));
    TEST_ASSERT_EQUAL_UINT32(3, first);

    /* wrap-around after ack */
    fix_buffer_push(&fb, &(fix_record_t){.device_ms = 5});
    fix_buffer_push(&fb, &(fix_record_t){.device_ms = 6});
    TEST_ASSERT_EQUAL_UINT32(0, fix_buffer_drops(&fb));
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 6));
    TEST_ASSERT_EQUAL_size_t(0, fix_buffer_count(&fb));
}

static void test_ack_of_overwritten_records_is_ok(void) {
    uint8_t store[2 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 2);
    for (uint32_t i = 1; i <= 5; i++) fix_buffer_push(&fb, &(fix_record_t){.device_ms = i});
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 3)); /* 1..3 already gone or overwritten */
    TEST_ASSERT_EQUAL_size_t(2, fix_buffer_count(&fb));
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 4));
    TEST_ASSERT_EQUAL_size_t(1, fix_buffer_count(&fb));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pack_roundtrip);
    RUN_TEST(test_pack_is_little_endian_and_28_bytes);
    RUN_TEST(test_utc_epoch);
    RUN_TEST(test_builder_pairs_gga_then_rmc_and_rmc_then_gga);
    RUN_TEST(test_builder_flushes_unpaired_half);
    RUN_TEST(test_buffer_fifo_and_peek);
    RUN_TEST(test_buffer_overwrites_oldest_and_counts_drops);
    RUN_TEST(test_ack_by_sequence);
    RUN_TEST(test_ack_of_overwritten_records_is_ok);
    return UNITY_END();
}
