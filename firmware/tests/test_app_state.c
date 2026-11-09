#include "app/app_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    app_state_t from;
    app_event_t ev;
    app_state_t to;
    int cue;
    unsigned actions; /* exact action mask */
    const char *why;
} row_t;

#define A_STOP_MIC_FINISH (ACT_STOP_MIC | ACT_FINISH_UPLOAD)

static const row_t table[] = {
    /* boot and link */
    {STATE_BOOT, EV_CONFIG_VALID, STATE_CONNECTING, CUE_NONE, 0, "boot with config"},
    {STATE_BOOT, EV_CONFIG_MISSING, STATE_SETUP, CUE_NONE, 0, "F1"},
    {STATE_BOOT, EV_SETUP_REQUESTED, STATE_SETUP, CUE_NONE, 0, "F2"},
    {STATE_CONNECTING, EV_LINK_UP, STATE_IDLE, CUE_NONE, 0, "connected"},
    {STATE_IDLE, EV_LINK_LOST, STATE_CONNECTING, CUE_NONE, 0, "wifi lost"},

    /* press, F3, F4 */
    {STATE_IDLE, EV_PRESS, STATE_RECORDING, CUE_PRESS, ACT_START_RECORD, "press starts recording"},
    {STATE_RECORDING, EV_RELEASE_SHORT, STATE_IDLE, CUE_CANCEL, ACT_STOP_MIC | ACT_ABORT_UPLOAD,
     "F3 tap cancels, nothing uploaded"},
    {STATE_RECORDING, EV_RELEASE, STATE_WAITING, CUE_RELEASE, A_STOP_MIC_FINISH, "release"},
    {STATE_RECORDING, EV_MAX_UTTERANCE, STATE_WAITING, CUE_LIMIT, A_STOP_MIC_FINISH,
     "F4 limit cue then query proceeds"},

    /* F6 */
    {STATE_CONNECTING, EV_PRESS, STATE_CONNECTING, CUE_NO_NETWORK, 0, "F6 no wifi at press"},
    {STATE_CONNECTING, EV_DOUBLE_PRESS, STATE_CONNECTING, CUE_NO_NETWORK, 0, "F6 replay offline"},

    /* waiting and replies, F7 */
    {STATE_WAITING, EV_FIRST_AUDIO, STATE_PLAYING, CUE_NONE, ACT_START_PLAYBACK, "first audio"},
    {STATE_WAITING, EV_QUERY_ERROR, STATE_IDLE, CUE_SERVER_ERROR, ACT_ABORT_QUERY, "F7 non-200"},
    {STATE_WAITING, EV_QUERY_TIMEOUT, STATE_IDLE, CUE_SERVER_ERROR, ACT_ABORT_QUERY,
     "F7 QUERY_TIMEOUT_S"},
    {STATE_WAITING, EV_STREAM_END, STATE_IDLE, CUE_SERVER_ERROR, 0, "empty reply"},
    {STATE_WAITING, EV_LINK_LOST, STATE_CONNECTING, CUE_SERVER_ERROR, ACT_ABORT_QUERY,
     "F7 server unreachable"},
    {STATE_PLAYING, EV_STREAM_END, STATE_IDLE, CUE_NONE, ACT_STOP_PLAYBACK, "reply played"},
    {STATE_PLAYING, EV_QUERY_ERROR, STATE_IDLE, CUE_SERVER_ERROR,
     ACT_STOP_PLAYBACK | ACT_ABORT_QUERY, "stream broke"},
    {STATE_PLAYING, EV_QUERY_TIMEOUT, STATE_IDLE, CUE_SERVER_ERROR,
     ACT_STOP_PLAYBACK | ACT_ABORT_QUERY, "stream stalled"},
    {STATE_PLAYING, EV_LINK_LOST, STATE_CONNECTING, CUE_SERVER_ERROR, ACT_STOP_PLAYBACK,
     "link lost mid reply"},
    {STATE_RECORDING, EV_QUERY_ERROR, STATE_IDLE, CUE_SERVER_ERROR,
     ACT_STOP_MIC | ACT_ABORT_UPLOAD | ACT_ABORT_QUERY, "upload failed"},
    {STATE_RECORDING, EV_LINK_LOST, STATE_CONNECTING, CUE_SERVER_ERROR,
     ACT_STOP_MIC | ACT_ABORT_UPLOAD | ACT_ABORT_QUERY, "link lost while recording"},

    /* F9 barge-in */
    {STATE_PLAYING, EV_PRESS, STATE_RECORDING, CUE_PRESS,
     ACT_STOP_PLAYBACK | ACT_DROP_CONNECTION | ACT_START_RECORD, "F9 barge-in"},
    {STATE_PLAYING, EV_DOUBLE_PRESS, STATE_RECORDING, CUE_PRESS,
     ACT_STOP_PLAYBACK | ACT_DROP_CONNECTION | ACT_START_RECORD, "F9 barge-in after a tap"},

    /* U13 */
    {STATE_IDLE, EV_DOUBLE_PRESS, STATE_WAITING, CUE_NONE, ACT_START_REPLAY, "U13 replay"},

    /* F10 */
    {STATE_IDLE, EV_VOLUME_TURN, STATE_IDLE, CUE_VOLUME_TICK, ACT_SET_VOLUME, "tick in IDLE"},
    {STATE_CONNECTING, EV_VOLUME_TURN, STATE_CONNECTING, CUE_VOLUME_TICK, ACT_SET_VOLUME, "tick"},
    {STATE_WAITING, EV_VOLUME_TURN, STATE_WAITING, CUE_VOLUME_TICK, ACT_SET_VOLUME, "tick"},
    {STATE_PLAYING, EV_VOLUME_TURN, STATE_PLAYING, CUE_VOLUME_TICK, ACT_SET_VOLUME,
     "tick while playing"},
    {STATE_RECORDING, EV_VOLUME_TURN, STATE_RECORDING, CUE_NONE, ACT_SET_VOLUME,
     "silent so the mic does not hear it"},

    /* events that must do nothing */
    {STATE_WAITING, EV_PRESS, STATE_WAITING, CUE_NONE, 0, "press while waiting is ignored"},
    {STATE_WAITING, EV_RELEASE, STATE_WAITING, CUE_NONE, 0, "late release after the limit"},
    {STATE_IDLE, EV_RELEASE, STATE_IDLE, CUE_NONE, 0, "stray release"},
    {STATE_IDLE, EV_LINK_UP, STATE_IDLE, CUE_NONE, 0, "already up"},
    {STATE_RECORDING, EV_PRESS, STATE_RECORDING, CUE_NONE, 0, "press while recording"},
    {STATE_BOOT, EV_PRESS, STATE_BOOT, CUE_NONE, 0, "no input in BOOT"},
};

static void test_table(void) {
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        const row_t *r = &table[i];
        app_transition_t t = app_step(r->from, r->ev);
        TEST_ASSERT_EQUAL_MESSAGE(r->to, t.next, r->why);
        TEST_ASSERT_EQUAL_INT_MESSAGE(r->cue, t.cue, r->why);
        TEST_ASSERT_EQUAL_HEX_MESSAGE(r->actions, t.actions, r->why);
        TEST_ASSERT_EQUAL_MESSAGE(r->to, app_state_next(r->from, r->ev), r->why);
    }
}

static void test_setup_is_sticky(void) {
    for (int ev = 0; ev < EV_COUNT; ev++) {
        app_transition_t t = app_step(STATE_SETUP, (app_event_t)ev);
        TEST_ASSERT_EQUAL(STATE_SETUP, t.next);
        TEST_ASSERT_EQUAL_INT(CUE_NONE, t.cue);
        TEST_ASSERT_EQUAL_HEX(0, t.actions);
    }
}

static void test_mic_power_rules(void) {
    for (int s = 0; s <= STATE_PLAYING; s++) {
        for (int ev = 0; ev < EV_COUNT; ev++) {
            app_transition_t t = app_step((app_state_t)s, (app_event_t)ev);
            if (t.actions & ACT_START_RECORD) {
                TEST_ASSERT_TRUE(s == STATE_IDLE || s == STATE_PLAYING);
                TEST_ASSERT_EQUAL(STATE_RECORDING, t.next);
            }
            /* Leaving RECORDING always powers the mic down (privacy, plan 5.1). */
            if (s == STATE_RECORDING && t.next != STATE_RECORDING) {
                TEST_ASSERT_TRUE(t.actions & ACT_STOP_MIC);
            }
        }
    }
}

static void test_every_query_state_has_a_way_out(void) {
    const app_event_t exits[] = {EV_QUERY_ERROR, EV_LINK_LOST};
    for (int s = STATE_RECORDING; s <= STATE_PLAYING; s++) {
        for (unsigned i = 0; i < 2; i++) {
            TEST_ASSERT_NOT_EQUAL((int)s, (int)app_step((app_state_t)s, exits[i]).next);
        }
    }
}

static void test_led_per_state(void) {
    TEST_ASSERT_EQUAL(LED_SLOW_BLINK, app_state_led(STATE_SETUP));
    TEST_ASSERT_EQUAL(LED_FAST_BLINK, app_state_led(STATE_CONNECTING));
    TEST_ASSERT_EQUAL(LED_OFF, app_state_led(STATE_IDLE));
    TEST_ASSERT_EQUAL(LED_SOLID, app_state_led(STATE_RECORDING));
    TEST_ASSERT_EQUAL(LED_FLASHING, app_state_led(STATE_WAITING));
    TEST_ASSERT_EQUAL(LED_OFF, app_state_led(STATE_PLAYING));
}

static void test_names(void) {
    TEST_ASSERT_EQUAL_STRING("RECORDING", app_state_name(STATE_RECORDING));
    TEST_ASSERT_EQUAL_STRING("PLAYING", app_state_name(STATE_PLAYING));
    TEST_ASSERT_EQUAL_STRING("volume-turn", app_event_name(EV_VOLUME_TURN));
    TEST_ASSERT_EQUAL_STRING("?", app_event_name(EV_COUNT));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_table);
    RUN_TEST(test_setup_is_sticky);
    RUN_TEST(test_mic_power_rules);
    RUN_TEST(test_every_query_state_has_a_way_out);
    RUN_TEST(test_led_per_state);
    RUN_TEST(test_names);
    return UNITY_END();
}
