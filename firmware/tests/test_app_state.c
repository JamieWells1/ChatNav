#include "app/app_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_boot_paths(void) {
    TEST_ASSERT_EQUAL(STATE_CONNECTING, app_state_next(STATE_BOOT, EV_CONFIG_VALID));
    TEST_ASSERT_EQUAL(STATE_SETUP, app_state_next(STATE_BOOT, EV_CONFIG_MISSING));
    TEST_ASSERT_EQUAL(STATE_SETUP, app_state_next(STATE_BOOT, EV_SETUP_REQUESTED));
    TEST_ASSERT_EQUAL(STATE_BOOT, app_state_next(STATE_BOOT, EV_LINK_UP));
}

static void test_link_cycle(void) {
    TEST_ASSERT_EQUAL(STATE_IDLE, app_state_next(STATE_CONNECTING, EV_LINK_UP));
    TEST_ASSERT_EQUAL(STATE_CONNECTING, app_state_next(STATE_IDLE, EV_LINK_LOST));
    TEST_ASSERT_EQUAL(STATE_IDLE, app_state_next(STATE_IDLE, EV_LINK_UP));
}

static void test_setup_is_sticky(void) {
    for (int ev = EV_CONFIG_VALID; ev <= EV_LINK_LOST; ev++) {
        TEST_ASSERT_EQUAL(STATE_SETUP, app_state_next(STATE_SETUP, (app_event_t)ev));
    }
}

static void test_led_per_state(void) {
    TEST_ASSERT_EQUAL(LED_SLOW_BLINK, app_state_led(STATE_SETUP));
    TEST_ASSERT_EQUAL(LED_FAST_BLINK, app_state_led(STATE_CONNECTING));
    TEST_ASSERT_EQUAL(LED_OFF, app_state_led(STATE_IDLE));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_paths);
    RUN_TEST(test_link_cycle);
    RUN_TEST(test_setup_is_sticky);
    RUN_TEST(test_led_per_state);
    return UNITY_END();
}
