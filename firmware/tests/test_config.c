#include "config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_i2s_pin_pairs_are_adjacent(void) {
    TEST_ASSERT_EQUAL_INT(PIN_MIC_SCK + 1, PIN_MIC_WS);
    TEST_ASSERT_EQUAL_INT(PIN_AMP_BCLK + 1, PIN_AMP_LRC);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_i2s_pin_pairs_are_adjacent);
    return UNITY_END();
}
