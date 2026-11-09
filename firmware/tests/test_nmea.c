#include <stdio.h>
#include <string.h>

#include "gps/fix_record.h"
#include "gps/nmea.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define GGA "$GPGGA,123519.00,5130.1234,N,00007.4321,W,1,08,0.9,45.3,M,47.0,M,,*43"
#define RMC "$GPRMC,123519.00,A,5130.1234,N,00007.4321,W,022.4,084.4,091126,,,A*40"
#define RMC_VOID "$GPRMC,123521.00,V,,,,,,,091126,,,N*76"
#define GGA_VOID "$GPGGA,123521.00,,,,,0,00,99.99,,,,,,*60"
#define GSA "$GPGSA,A,3,04,05,09,12,24,,,,,,,,2.5,1.3,2.1*39"

static void test_checksum(void) {
    TEST_ASSERT_TRUE(nmea_checksum_ok(RMC));
    TEST_ASSERT_TRUE(nmea_checksum_ok(GSA));
    TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,123519.00,A*00"));
    TEST_ASSERT_FALSE(nmea_checksum_ok("GPRMC,1*00"));
    TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,123519.00,A"));
    TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,1*4"));
    TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,1*4G"));
}

static void test_rmc_fields(void) {
    nmea_msg_t m;
    TEST_ASSERT_EQUAL_INT(NMEA_RMC, nmea_parse_sentence(RMC, &m));
    TEST_ASSERT_TRUE(m.status_valid);
    TEST_ASSERT_TRUE(m.pos_valid);
    TEST_ASSERT_EQUAL_INT32((12 * 3600 + 35 * 60 + 19) * 1000, m.time_ms);
    TEST_ASSERT_EQUAL_INT32(515020567, m.lat_e7);
    TEST_ASSERT_EQUAL_INT32(-1238683, m.lon_e7);
    TEST_ASSERT_TRUE(m.speed_valid);
    TEST_ASSERT_EQUAL_UINT32(1152, m.speed_cmps);
    TEST_ASSERT_TRUE(m.course_valid);
    TEST_ASSERT_EQUAL_UINT16(844, m.course_deg10);
    TEST_ASSERT_TRUE(m.date_valid);
    TEST_ASSERT_EQUAL_UINT16(2026, m.year);
    TEST_ASSERT_EQUAL_UINT8(11, m.month);
    TEST_ASSERT_EQUAL_UINT8(9, m.day);
}

static void test_gga_fields(void) {
    nmea_msg_t m;
    TEST_ASSERT_EQUAL_INT(NMEA_GGA, nmea_parse_sentence(GGA, &m));
    TEST_ASSERT_EQUAL_UINT8(1, m.quality);
    TEST_ASSERT_EQUAL_UINT8(8, m.sats);
    TEST_ASSERT_TRUE(m.hdop_valid);
    TEST_ASSERT_EQUAL_UINT16(9, m.hdop10);
    TEST_ASSERT_TRUE(m.alt_valid);
    TEST_ASSERT_EQUAL_INT16(453, m.alt_dm);
}

static void test_no_fix_sentences(void) {
    nmea_msg_t m;
    TEST_ASSERT_EQUAL_INT(NMEA_RMC, nmea_parse_sentence(RMC_VOID, &m));
    TEST_ASSERT_FALSE(m.status_valid);
    TEST_ASSERT_FALSE(m.pos_valid);
    TEST_ASSERT_FALSE(m.speed_valid);
    TEST_ASSERT_TRUE(m.date_valid);
    TEST_ASSERT_EQUAL_INT(NMEA_GGA, nmea_parse_sentence(GGA_VOID, &m));
    TEST_ASSERT_EQUAL_UINT8(0, m.quality);
    TEST_ASSERT_EQUAL_UINT8(0, m.sats);
    TEST_ASSERT_FALSE(m.pos_valid);
    TEST_ASSERT_EQUAL_UINT16(999, m.hdop10);
}

static void test_ignored_and_bad(void) {
    nmea_msg_t m;
    TEST_ASSERT_EQUAL_INT(NMEA_IGNORED, nmea_parse_sentence(GSA, &m));
    TEST_ASSERT_EQUAL_INT(NMEA_BAD, nmea_parse_sentence("$GPRMC,123519.00,A,5130.1234,N*00", &m));
    /* GNSS talker IDs are accepted too */
    char gn[] = "$GNGGA,123519.00,5130.1234,N,00007.4321,W,1,08,0.9,45.3,M,47.0,M,,*XX";
    unsigned sum = 0;
    for (const char *p = gn + 1; *p != '*'; p++) sum ^= (unsigned char)*p;
    snprintf(gn + strlen(gn) - 2, 3, "%02X", sum);
    TEST_ASSERT_EQUAL_INT(NMEA_GGA, nmea_parse_sentence(gn, &m));
}

static void test_southern_and_eastern_hemispheres(void) {
    nmea_msg_t m;
    char s[120] = "$GPGGA,000000.00,3351.0000,S,15112.0000,E,1,05,1.0,10.0,M,0,M,,*XX";
    unsigned sum = 0;
    for (const char *p = s + 1; *p != '*'; p++) sum ^= (unsigned char)*p;
    snprintf(s + strlen(s) - 2, 3, "%02X", sum);
    TEST_ASSERT_EQUAL_INT(NMEA_GGA, nmea_parse_sentence(s, &m));
    TEST_ASSERT_EQUAL_INT32(-338500000, m.lat_e7);
    TEST_ASSERT_EQUAL_INT32(1512000000, m.lon_e7);
}

static void test_feed_bytes_with_noise(void) {
    nmea_parser_t p;
    nmea_parser_init(&p);
    nmea_msg_t m;
    const char *stream = "garbage\xff\r\n" GGA "\r\n" "$GPRMC,truncated" RMC "\r\n";
    int rmc = 0, gga = 0;
    for (const char *c = stream; *c != '\0'; c++) {
        nmea_kind_t k = nmea_feed(&p, (uint8_t)*c, &m);
        if (k == NMEA_RMC) rmc++;
        if (k == NMEA_GGA) gga++;
    }
    TEST_ASSERT_EQUAL_INT(1, gga);
    TEST_ASSERT_EQUAL_INT(1, rmc); /* restart at the second '$' recovers */
}

static void test_overlong_line_is_rejected(void) {
    nmea_parser_t p;
    nmea_parser_init(&p);
    nmea_msg_t m;
    nmea_feed(&p, '$', &m);
    nmea_kind_t k = NMEA_INCOMPLETE;
    for (int i = 0; i < 300 && k == NMEA_INCOMPLETE; i++) k = nmea_feed(&p, 'A', &m);
    TEST_ASSERT_EQUAL_INT(NMEA_BAD, k);
}

static void test_fixture_file(void) {
    FILE *f = fopen(FIXTURE_DIR "/sample.nmea", "rb");
    TEST_ASSERT_NOT_NULL(f);
    nmea_parser_t p;
    nmea_parser_init(&p);
    fix_builder_t b;
    fix_builder_init(&b);
    int c, records = 0, valid = 0;
    uint32_t t = 0;
    fix_record_t last = {0};
    while ((c = fgetc(f)) != EOF) {
        nmea_msg_t m;
        nmea_kind_t k = nmea_feed(&p, (uint8_t)c, &m);
        fix_record_t r;
        if ((k == NMEA_RMC || k == NMEA_GGA) && fix_builder_add(&b, &m, t += 1000, &r)) {
            records++;
            if (r.flags & FIX_FLAG_RMC_VALID) valid++;
            last = r;
        }
    }
    fclose(f);
    TEST_ASSERT_EQUAL_UINT32(1, p.bad);
    TEST_ASSERT_EQUAL_UINT32(2, p.ignored); /* GSA and VTG */
    TEST_ASSERT_EQUAL_UINT32(6, p.ok);
    TEST_ASSERT_EQUAL_INT(3, records);
    TEST_ASSERT_EQUAL_INT(2, valid);
    TEST_ASSERT_EQUAL_UINT8(0, last.quality);
    TEST_ASSERT_FALSE(last.flags & FIX_FLAG_POS);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checksum);
    RUN_TEST(test_rmc_fields);
    RUN_TEST(test_gga_fields);
    RUN_TEST(test_no_fix_sentences);
    RUN_TEST(test_ignored_and_bad);
    RUN_TEST(test_southern_and_eastern_hemispheres);
    RUN_TEST(test_feed_bytes_with_noise);
    RUN_TEST(test_overlong_line_is_rejected);
    RUN_TEST(test_fixture_file);
    return UNITY_END();
}
