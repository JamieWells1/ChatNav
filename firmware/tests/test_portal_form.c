#include <string.h>

#include "setup/portal_form.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define FP "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF"
#define GOOD                                                                          \
    "ssid=My+Phone&password=p%40ssw0rd%21&host=chatnav.example.com&port=8443&token=" \
    "abcdefghijklmnop1234&fingerprint=" FP

static void test_form_get_decodes(void) {
    char out[32];
    TEST_ASSERT_EQUAL_INT(8, form_get(GOOD, "ssid", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("My Phone", out);
    TEST_ASSERT_EQUAL_INT(9, form_get(GOOD, "password", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("p@ssw0rd!", out);
}

static void test_form_get_missing_and_bad(void) {
    char out[8];
    TEST_ASSERT_EQUAL_INT(FORM_MISSING, form_get("a=1&b=2", "c", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(FORM_MISSING, form_get("ab=1", "a", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(FORM_BAD, form_get("a=%zz", "a", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(FORM_BAD, form_get("a=%4", "a", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(FORM_BAD, form_get("a=0123456789", "a", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(0, form_get("a=&b=2", "a", out, sizeof(out)));
}

static void test_good_form(void) {
    device_config_t base, cfg;
    device_config_defaults(&base);
    base.volume_step = 3;
    TEST_ASSERT_EQUAL(CFG_OK, portal_form_to_config(GOOD, &base, &cfg));
    TEST_ASSERT_EQUAL_STRING("My Phone", cfg.ssid);
    TEST_ASSERT_EQUAL_STRING("chatnav.example.com", cfg.server_host);
    TEST_ASSERT_EQUAL_UINT16(8443, cfg.server_port);
    TEST_ASSERT_EQUAL_UINT8(3, cfg.volume_step);
    TEST_ASSERT_EQUAL_UINT8(0x00, cfg.fingerprint[0]);
    TEST_ASSERT_EQUAL_UINT8(0x11, cfg.fingerprint[1]);
    TEST_ASSERT_EQUAL(CFG_OK, device_config_check(&cfg));
}

static void test_default_port_and_open_network(void) {
    device_config_t cfg;
    const char *body = "ssid=Open&password=&host=1.2.3.4&port=&token=abcdefghijklmnop1234"
                       "&fingerprint=" FP;
    TEST_ASSERT_EQUAL(CFG_OK, portal_form_to_config(body, NULL, &cfg));
    TEST_ASSERT_EQUAL_UINT16(443, cfg.server_port);
    TEST_ASSERT_EQUAL_STRING("", cfg.password);
}

static void test_errors(void) {
    device_config_t cfg;
    TEST_ASSERT_EQUAL(CFG_ERR_SSID, portal_form_to_config("host=a", NULL, &cfg));
    TEST_ASSERT_EQUAL(CFG_ERR_PORT,
                      portal_form_to_config("ssid=a&host=a.b&port=99999&token=abcdefghijklmnop1234"
                                            "&fingerprint=" FP, NULL, &cfg));
    TEST_ASSERT_EQUAL(CFG_ERR_PORT,
                      portal_form_to_config("ssid=a&host=a.b&port=44x&token=abcdefghijklmnop1234"
                                            "&fingerprint=" FP, NULL, &cfg));
    TEST_ASSERT_EQUAL(CFG_ERR_FINGERPRINT,
                      portal_form_to_config("ssid=a&host=a.b&token=abcdefghijklmnop1234"
                                            "&fingerprint=1234", NULL, &cfg));
    TEST_ASSERT_EQUAL(CFG_ERR_TOKEN,
                      portal_form_to_config("ssid=a&host=a.b&token=short&fingerprint=" FP, NULL,
                                            &cfg));
}

static void test_failure_leaves_output_untouched(void) {
    device_config_t cfg;
    memset(&cfg, 0x77, sizeof(cfg));
    portal_form_to_config("ssid=", NULL, &cfg);
    TEST_ASSERT_EQUAL_UINT8(0x77, ((uint8_t *)&cfg)[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_form_get_decodes);
    RUN_TEST(test_form_get_missing_and_bad);
    RUN_TEST(test_good_form);
    RUN_TEST(test_default_port_and_open_network);
    RUN_TEST(test_errors);
    RUN_TEST(test_failure_leaves_output_untouched);
    return UNITY_END();
}
