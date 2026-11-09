#include <stdlib.h>

#include "audio/cues.h"
#include "audio/pcm.h"
#include "config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define MAX_SAMPLES 16000

static size_t render(cue_id_t id, int16_t *out) {
    cue_gen_t g;
    cue_start(&g, id);
    size_t total = 0, n;
    while ((n = cue_next(&g, out + total, 37)) > 0) total += n; /* odd chunk size on purpose */
    return total;
}

static void test_every_cue_terminates_with_expected_length(void) {
    static int16_t buf[MAX_SAMPLES];
    for (int id = 0; id < CUE_COUNT; id++) {
        size_t n = render((cue_id_t)id, buf);
        TEST_ASSERT_EQUAL_UINT32(cue_length_samples((cue_id_t)id), n);
        TEST_ASSERT_TRUE(n > 0 && n < MAX_SAMPLES);
        TEST_ASSERT_TRUE(pcm_peak(buf, n) > 1000);
        TEST_ASSERT_TRUE(pcm_peak(buf, n) < 32767);
        TEST_ASSERT_TRUE(abs(buf[0]) < 100);      /* fades in, no click */
        TEST_ASSERT_TRUE(abs(buf[n - 1]) < 4000); /* fades out */
    }
}

static void test_cue_names_are_set_and_unique(void) {
    for (int a = 0; a < CUE_COUNT; a++) {
        TEST_ASSERT_NOT_EQUAL(0, cue_name((cue_id_t)a)[0]);
        for (int b = a + 1; b < CUE_COUNT; b++) {
            TEST_ASSERT_TRUE(cue_name((cue_id_t)a) != cue_name((cue_id_t)b));
        }
    }
}

static void test_press_cue_frequency(void) {
    static int16_t buf[MAX_SAMPLES];
    size_t n = render(CUE_PRESS, buf);
    int crossings = 0;
    for (size_t i = 1; i < n; i++) {
        if ((buf[i - 1] < 0) != (buf[i] < 0)) crossings++;
    }
    float hz = (float)crossings / 2.0f / ((float)n / AUDIO_SAMPLE_RATE);
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 880.0f, hz);
}

static void test_limit_cue_has_a_gap(void) {
    static int16_t buf[MAX_SAMPLES];
    size_t n = render(CUE_LIMIT, buf);
    size_t mid = n / 2;
    TEST_ASSERT_EQUAL_INT16(0, buf[mid]);
    TEST_ASSERT_EQUAL_INT16(0, buf[mid + 5]);
}

static void test_finished_cue_returns_zero_and_unknown_id_is_silent(void) {
    int16_t buf[10];
    cue_gen_t g;
    cue_start(&g, CUE_VOLUME_TICK);
    while (cue_next(&g, buf, 10) > 0) {}
    TEST_ASSERT_EQUAL_size_t(0, cue_next(&g, buf, 10));
    cue_start(&g, (cue_id_t)99);
    TEST_ASSERT_EQUAL_size_t(0, cue_next(&g, buf, 10));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_cue_terminates_with_expected_length);
    RUN_TEST(test_cue_names_are_set_and_unique);
    RUN_TEST(test_press_cue_frequency);
    RUN_TEST(test_limit_cue_has_a_gap);
    RUN_TEST(test_finished_cue_returns_zero_and_unknown_id_is_silent);
    return UNITY_END();
}
