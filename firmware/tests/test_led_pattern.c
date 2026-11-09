#include "status/led_pattern.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static int count_on(led_mode_t m, uint32_t period_ms) {
    int on = 0;
    for (uint32_t t = 0; t < period_ms; t++) on += led_pattern_level(m, t);
    return on;
}

static void test_off_and_solid(void) {
    TEST_ASSERT_EQUAL_INT(0, count_on(LED_OFF, 2000));
    TEST_ASSERT_EQUAL_INT(2000, count_on(LED_SOLID, 2000));
}

static void test_blink_rates(void) {
    TEST_ASSERT_EQUAL_INT(500, count_on(LED_SLOW_BLINK, 1000));
    TEST_ASSERT_TRUE(led_pattern_level(LED_SLOW_BLINK, 0));
    TEST_ASSERT_FALSE(led_pattern_level(LED_SLOW_BLINK, 500));
    TEST_ASSERT_EQUAL_INT(100, count_on(LED_FAST_BLINK, 200));
    TEST_ASSERT_EQUAL_INT(100, count_on(LED_FLASHING, 500));
}

static void test_patterns_are_distinct(void) {
    int diff = 0;
    for (uint32_t t = 0; t < 1000; t++) {
        diff += led_pattern_level(LED_FAST_BLINK, t) != led_pattern_level(LED_FLASHING, t);
    }
    TEST_ASSERT_TRUE(diff > 100);
}

static void test_names(void) {
    TEST_ASSERT_EQUAL_STRING("flashing", led_mode_name(LED_FLASHING));
    TEST_ASSERT_EQUAL_STRING("unknown", led_mode_name((led_mode_t)42));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_off_and_solid);
    RUN_TEST(test_blink_rates);
    RUN_TEST(test_patterns_are_distinct);
    RUN_TEST(test_names);
    return UNITY_END();
}
