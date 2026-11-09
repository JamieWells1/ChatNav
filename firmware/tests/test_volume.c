#include "audio/volume.h"
#include "config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_endpoints(void) {
    TEST_ASSERT_EQUAL_UINT32(0, volume_gain_q15(0));
    TEST_ASSERT_EQUAL_UINT32(VOLUME_UNITY_Q15, volume_gain_q15(VOLUME_STEPS - 1));
}

static void test_monotonic_and_log_spaced(void) {
    for (int s = 1; s < VOLUME_STEPS - 1; s++) {
        uint32_t lo = volume_gain_q15(s), hi = volume_gain_q15(s + 1);
        TEST_ASSERT_TRUE(hi > lo);
        /* 3 dB step is a ratio of about 1.413 */
        float ratio = (float)hi / (float)lo;
        TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.413f, ratio);
    }
}

static void test_clamping(void) {
    TEST_ASSERT_EQUAL_UINT32(0, volume_gain_q15(-5));
    TEST_ASSERT_EQUAL_UINT32(VOLUME_UNITY_Q15, volume_gain_q15(VOLUME_STEPS + 10));
    TEST_ASSERT_EQUAL_INT(0, volume_clamp_step(-1));
    TEST_ASSERT_EQUAL_INT(VOLUME_STEPS - 1, volume_clamp_step(99));
    TEST_ASSERT_EQUAL_INT(7, volume_clamp_step(7));
}

static void test_apply(void) {
    int16_t a[3] = {10000, -10000, 32767};
    volume_apply(a, 3, VOLUME_STEPS - 1);
    TEST_ASSERT_EQUAL_INT16(10000, a[0]);
    TEST_ASSERT_EQUAL_INT16(-10000, a[1]);
    TEST_ASSERT_EQUAL_INT16(32767, a[2]);

    int16_t b[2] = {10000, -10000};
    volume_apply(b, 2, VOLUME_STEPS - 3); /* -6 dB */
    TEST_ASSERT_INT16_WITHIN(60, 5012, b[0]);
    TEST_ASSERT_INT16_WITHIN(60, -5012, b[1]);

    int16_t c[2] = {12345, -12345};
    volume_apply(c, 2, 0);
    TEST_ASSERT_EQUAL_INT16(0, c[0]);
    TEST_ASSERT_EQUAL_INT16(0, c[1]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_endpoints);
    RUN_TEST(test_monotonic_and_log_spaced);
    RUN_TEST(test_clamping);
    RUN_TEST(test_apply);
    return UNITY_END();
}
