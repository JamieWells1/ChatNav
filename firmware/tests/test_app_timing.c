#include "app/app_timing.h"
#include "config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_release_threshold(void) {
    TEST_ASSERT_EQUAL(EV_RELEASE_SHORT, app_release_event(0));
    TEST_ASSERT_EQUAL(EV_RELEASE_SHORT, app_release_event(MIN_UTTERANCE_MS - 1));
    TEST_ASSERT_EQUAL(EV_RELEASE, app_release_event(MIN_UTTERANCE_MS));
    TEST_ASSERT_FALSE(app_upload_armed(MIN_UTTERANCE_MS - 1));
    TEST_ASSERT_TRUE(app_upload_armed(MIN_UTTERANCE_MS));
}

static void test_max_utterance(void) {
    TEST_ASSERT_FALSE(app_max_utterance_reached(MAX_UTTERANCE_S * 1000 - 1));
    TEST_ASSERT_TRUE(app_max_utterance_reached(MAX_UTTERANCE_S * 1000));
}

static void test_query_timeout(void) {
    TEST_ASSERT_FALSE(app_query_timed_out(QUERY_TIMEOUT_S * 1000 - 1));
    TEST_ASSERT_TRUE(app_query_timed_out(QUERY_TIMEOUT_S * 1000));
}

static void test_double_press(void) {
    TEST_ASSERT_EQUAL(EV_PRESS, app_press_event(false, 0, 100));
    TEST_ASSERT_EQUAL(EV_DOUBLE_PRESS, app_press_event(true, 1000, 1000 + DOUBLE_PRESS_MS));
    TEST_ASSERT_EQUAL(EV_PRESS, app_press_event(true, 1000, 1001 + DOUBLE_PRESS_MS));
    /* millisecond counter wrap */
    TEST_ASSERT_EQUAL(EV_DOUBLE_PRESS, app_press_event(true, 0xFFFFFF00u, 0x00000050u));
}

static void test_volume_save_delay(void) {
    TEST_ASSERT_FALSE(app_volume_save_due(false, 0, 100000));
    TEST_ASSERT_FALSE(app_volume_save_due(true, 1000, 1000 + VOLUME_SAVE_DELAY_S * 1000 - 1));
    TEST_ASSERT_TRUE(app_volume_save_due(true, 1000, 1000 + VOLUME_SAVE_DELAY_S * 1000));
    TEST_ASSERT_TRUE(
        app_volume_save_due(true, 0xFFFFFFF0u, 0xFFFFFFF0u + VOLUME_SAVE_DELAY_S * 1000));
}

static void test_waiting_verdicts(void) {
    /* GET /v1/query/last with nothing cached: 404 -> error cue, never a hang */
    TEST_ASSERT_EQUAL(EV_QUERY_ERROR, app_waiting_event(false, true, 404, false, false));
    TEST_ASSERT_EQUAL(EV_QUERY_ERROR, app_waiting_event(false, true, 404, false, true));
    TEST_ASSERT_EQUAL(EV_QUERY_ERROR, app_waiting_event(false, true, 401, false, false));
    TEST_ASSERT_EQUAL(EV_QUERY_ERROR, app_waiting_event(true, false, 0, false, false));
    TEST_ASSERT_EQUAL(APP_NO_EVENT, app_waiting_event(false, false, 0, false, false));
    TEST_ASSERT_EQUAL(APP_NO_EVENT, app_waiting_event(false, true, 200, false, false));
    TEST_ASSERT_EQUAL(EV_FIRST_AUDIO, app_waiting_event(false, true, 200, true, false));
    TEST_ASSERT_EQUAL(EV_STREAM_END, app_waiting_event(false, true, 200, false, true));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_release_threshold);
    RUN_TEST(test_max_utterance);
    RUN_TEST(test_query_timeout);
    RUN_TEST(test_double_press);
    RUN_TEST(test_volume_save_delay);
    RUN_TEST(test_waiting_verdicts);
    return UNITY_END();
}
