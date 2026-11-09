#include <string.h>

#include "net/fingerprint.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define HEX64 "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF"
#define COLON                                                                       \
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:" \
    "AA:BB:CC:DD:EE:FF"

static void expected(uint8_t out[FP_LEN]) {
    for (int i = 0; i < FP_LEN; i++) out[i] = (uint8_t)(((i % 16) << 4) | (i % 16));
}

static void test_parse_plain_and_colon(void) {
    uint8_t want[FP_LEN], got[FP_LEN];
    expected(want);
    TEST_ASSERT_TRUE(fp_parse(HEX64, got));
    TEST_ASSERT_EQUAL_MEMORY(want, got, FP_LEN);
    memset(got, 0, FP_LEN);
    TEST_ASSERT_TRUE(fp_parse(COLON, got));
    TEST_ASSERT_EQUAL_MEMORY(want, got, FP_LEN);
}

static void test_parse_lowercase_and_prefix(void) {
    uint8_t want[FP_LEN], got[FP_LEN];
    expected(want);
    TEST_ASSERT_TRUE(fp_parse("sha256 fingerprint=ignored", got) == false);
    TEST_ASSERT_TRUE(fp_parse("SHA256 Fingerprint=" COLON "\n", got));
    TEST_ASSERT_EQUAL_MEMORY(want, got, FP_LEN);
    TEST_ASSERT_TRUE(fp_parse("  00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff ", got));
    TEST_ASSERT_EQUAL_MEMORY(want, got, FP_LEN);
}

static void test_parse_rejects_bad_input(void) {
    uint8_t got[FP_LEN];
    memset(got, 0x5A, FP_LEN);
    TEST_ASSERT_FALSE(fp_parse(NULL, got));
    TEST_ASSERT_FALSE(fp_parse("", got));
    TEST_ASSERT_FALSE(fp_parse("0011", got));
    TEST_ASSERT_FALSE(fp_parse(HEX64 "00", got)); /* too long */
    TEST_ASSERT_FALSE(fp_parse(HEX64 "0", got));  /* odd digit count */
    TEST_ASSERT_FALSE(fp_parse("ZZ" HEX64, got));
    TEST_ASSERT_FALSE(fp_parse("0x" HEX64, got));
    TEST_ASSERT_EQUAL_UINT8(0x5A, got[0]); /* untouched on failure */
}

static void test_format_round_trip(void) {
    uint8_t fp[FP_LEN], back[FP_LEN];
    char text[FP_TEXT_LEN];
    expected(fp);
    fp_format(fp, text);
    TEST_ASSERT_EQUAL_STRING(COLON, text);
    TEST_ASSERT_TRUE(fp_parse(text, back));
    TEST_ASSERT_EQUAL_MEMORY(fp, back, FP_LEN);
}

static void test_equal_detects_single_bit(void) {
    uint8_t a[FP_LEN], b[FP_LEN];
    expected(a);
    memcpy(b, a, FP_LEN);
    TEST_ASSERT_TRUE(fp_equal(a, b));
    for (int i = 0; i < FP_LEN; i++) {
        b[i] ^= 0x01;
        TEST_ASSERT_FALSE(fp_equal(a, b));
        b[i] ^= 0x01;
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_plain_and_colon);
    RUN_TEST(test_parse_lowercase_and_prefix);
    RUN_TEST(test_parse_rejects_bad_input);
    RUN_TEST(test_format_round_trip);
    RUN_TEST(test_equal_detects_single_bit);
    return UNITY_END();
}
