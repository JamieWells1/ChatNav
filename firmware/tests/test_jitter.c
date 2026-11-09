#include <string.h>

#include "audio/jitter.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define CAP 64
#define PRE 16

static int16_t store[CAP];
static jitter_t j;

static void write_samples(int first, int n) {
    uint8_t b[2 * CAP];
    for (int i = 0; i < n; i++) {
        int16_t v = (int16_t)(first + i);
        b[2 * i] = (uint8_t)(v & 0xff);
        b[2 * i + 1] = (uint8_t)((v >> 8) & 0xff);
    }
    TEST_ASSERT_EQUAL_UINT((size_t)(2 * n), jitter_write_bytes(&j, b, (size_t)(2 * n)));
}

static void test_holds_until_prebuffered(void) {
    jitter_init(&j, store, CAP, PRE);
    int16_t out[CAP];
    write_samples(0, PRE - 1);
    TEST_ASSERT_EQUAL_UINT(0, jitter_read(&j, out, CAP));
    write_samples(PRE - 1, 1);
    TEST_ASSERT_EQUAL_UINT(PRE, jitter_read(&j, out, CAP));
    TEST_ASSERT_EQUAL_INT16(0, out[0]);
    TEST_ASSERT_EQUAL_INT16(PRE - 1, out[PRE - 1]);
}

static void test_short_reply_plays_when_finished(void) {
    jitter_init(&j, store, CAP, PRE);
    write_samples(0, 4);
    int16_t out[CAP];
    TEST_ASSERT_EQUAL_UINT(0, jitter_read(&j, out, CAP));
    jitter_finish(&j);
    TEST_ASSERT_EQUAL_UINT(4, jitter_read(&j, out, CAP));
    TEST_ASSERT_TRUE(jitter_drained(&j));
    TEST_ASSERT_EQUAL_UINT(0, j.underruns);
}

static void test_underrun_counts_once_per_dry_spell(void) {
    jitter_init(&j, store, CAP, PRE);
    write_samples(0, PRE);
    int16_t out[CAP];
    TEST_ASSERT_EQUAL_UINT(PRE, jitter_read(&j, out, PRE));
    TEST_ASSERT_EQUAL_UINT(0, jitter_read(&j, out, 8));
    TEST_ASSERT_EQUAL_UINT(0, jitter_read(&j, out, 8));
    TEST_ASSERT_EQUAL_UINT(1, j.underruns);
    write_samples(100, 4);
    TEST_ASSERT_EQUAL_UINT(4, jitter_read(&j, out, 4)); /* resumes without re-prebuffering */
    TEST_ASSERT_EQUAL_UINT(0, jitter_read(&j, out, 4));
    TEST_ASSERT_EQUAL_UINT(2, j.underruns);
    TEST_ASSERT_FALSE(jitter_drained(&j));
}

static void test_no_underrun_after_finish(void) {
    jitter_init(&j, store, CAP, PRE);
    write_samples(0, PRE);
    jitter_finish(&j);
    int16_t out[CAP];
    TEST_ASSERT_EQUAL_UINT(PRE, jitter_read(&j, out, CAP));
    TEST_ASSERT_EQUAL_UINT(0, j.underruns);
    TEST_ASSERT_TRUE(jitter_drained(&j));
}

static void test_wraps_and_applies_back_pressure(void) {
    jitter_init(&j, store, CAP, PRE);
    int16_t out[CAP];
    write_samples(0, CAP);
    TEST_ASSERT_EQUAL_UINT(0, jitter_free(&j));
    uint8_t extra[4] = {1, 0, 2, 0};
    TEST_ASSERT_EQUAL_UINT(0, jitter_write_bytes(&j, extra, 4));
    TEST_ASSERT_EQUAL_UINT(40, jitter_read(&j, out, 40));
    write_samples(1000, 30); /* wraps the storage */
    TEST_ASSERT_EQUAL_UINT(CAP - 40 + 30, jitter_read(&j, out, CAP));
    TEST_ASSERT_EQUAL_INT16(40, out[0]);
    TEST_ASSERT_EQUAL_INT16(63, out[23]);
    TEST_ASSERT_EQUAL_INT16(1000, out[24]);
    TEST_ASSERT_EQUAL_INT16(1029, out[53]);
}

static void test_odd_byte_split_across_writes(void) {
    jitter_init(&j, store, CAP, 2);
    const uint8_t a[3] = {0x34, 0x12, 0x78}; /* 0x1234, then low byte of 0x5678 */
    const uint8_t b[3] = {0x56, 0x01, 0x80}; /* high byte, then 0x8001 */
    TEST_ASSERT_EQUAL_UINT(3, jitter_write_bytes(&j, a, 3));
    TEST_ASSERT_EQUAL_UINT(1, jitter_queued(&j));
    TEST_ASSERT_EQUAL_UINT(3, jitter_write_bytes(&j, b, 3));
    int16_t out[4];
    TEST_ASSERT_EQUAL_UINT(3, jitter_read(&j, out, 4));
    TEST_ASSERT_EQUAL_INT16(0x1234, out[0]);
    TEST_ASSERT_EQUAL_INT16(0x5678, out[1]);
    TEST_ASSERT_EQUAL_INT16((int16_t)0x8001, out[2]);
}

static void test_reset(void) {
    jitter_init(&j, store, CAP, PRE);
    write_samples(0, PRE);
    jitter_finish(&j);
    jitter_reset(&j);
    TEST_ASSERT_EQUAL_UINT(0, jitter_queued(&j));
    TEST_ASSERT_FALSE(j.finished);
    TEST_ASSERT_FALSE(j.started);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_holds_until_prebuffered);
    RUN_TEST(test_short_reply_plays_when_finished);
    RUN_TEST(test_underrun_counts_once_per_dry_spell);
    RUN_TEST(test_no_underrun_after_finish);
    RUN_TEST(test_wraps_and_applies_back_pressure);
    RUN_TEST(test_odd_byte_split_across_writes);
    RUN_TEST(test_reset);
    return UNITY_END();
}
