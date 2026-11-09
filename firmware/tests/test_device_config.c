#include <string.h>

#include "storage/device_config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static device_config_t good(void) {
    device_config_t c;
    device_config_defaults(&c);
    strcpy(c.ssid, "PhoneHotspot");
    strcpy(c.password, "hunter2hunter2");
    strcpy(c.server_host, "chatnav.example.com");
    strcpy(c.token, "0123456789abcdefABCDEF");
    memset(c.fingerprint, 0xAB, sizeof(c.fingerprint));
    return c;
}

static void test_defaults(void) {
    device_config_t c;
    device_config_defaults(&c);
    TEST_ASSERT_EQUAL_UINT16(443, c.server_port);
    TEST_ASSERT_EQUAL_UINT8(8, c.volume_step);
    TEST_ASSERT_EQUAL(CFG_ERR_SSID, device_config_validate(&c));
}

static void test_valid(void) {
    device_config_t c = good();
    TEST_ASSERT_EQUAL(CFG_OK, device_config_validate(&c));
    c.password[0] = '\0'; /* open network */
    TEST_ASSERT_EQUAL(CFG_OK, device_config_validate(&c));
    strcpy(c.server_host, "203.0.113.7");
    TEST_ASSERT_EQUAL(CFG_OK, device_config_validate(&c));
}

static void test_field_errors(void) {
    device_config_t c = good();
    c.ssid[0] = '\0';
    TEST_ASSERT_EQUAL(CFG_ERR_SSID, device_config_validate(&c));

    c = good();
    strcpy(c.password, "short");
    TEST_ASSERT_EQUAL(CFG_ERR_PASSWORD, device_config_validate(&c));

    c = good();
    strcpy(c.server_host, "bad host");
    TEST_ASSERT_EQUAL(CFG_ERR_HOST, device_config_validate(&c));
    strcpy(c.server_host, "-lead.example");
    TEST_ASSERT_EQUAL(CFG_ERR_HOST, device_config_validate(&c));
    c.server_host[0] = '\0';
    TEST_ASSERT_EQUAL(CFG_ERR_HOST, device_config_validate(&c));

    c = good();
    c.server_port = 0;
    TEST_ASSERT_EQUAL(CFG_ERR_PORT, device_config_validate(&c));

    c = good();
    strcpy(c.token, "tooshort");
    TEST_ASSERT_EQUAL(CFG_ERR_TOKEN, device_config_validate(&c));
    strcpy(c.token, "has space in it 123456");
    TEST_ASSERT_EQUAL(CFG_ERR_TOKEN, device_config_validate(&c));

    c = good();
    memset(c.fingerprint, 0, sizeof(c.fingerprint));
    TEST_ASSERT_EQUAL(CFG_ERR_FINGERPRINT, device_config_validate(&c));
}

static void test_unterminated_strings_rejected(void) {
    device_config_t c = good();
    memset(c.ssid, 'A', sizeof(c.ssid));
    TEST_ASSERT_EQUAL(CFG_ERR_SSID, device_config_validate(&c));
}

static void test_seal_and_check(void) {
    device_config_t c = good();
    TEST_ASSERT_EQUAL(CFG_ERR_MAGIC, device_config_check(&c));
    device_config_seal(&c);
    TEST_ASSERT_EQUAL(CFG_OK, device_config_check(&c));

    device_config_t d = c;
    d.token[3] ^= 1;
    TEST_ASSERT_EQUAL(CFG_ERR_CRC, device_config_check(&d));

    d = c;
    d.version = 99;
    TEST_ASSERT_EQUAL(CFG_ERR_VERSION, device_config_check(&d));
}

static void test_serialise_round_trip(void) {
    device_config_t c = good();
    device_config_seal(&c);
    uint8_t flash[sizeof(c)];
    memcpy(flash, &c, sizeof(c));
    device_config_t back;
    memcpy(&back, flash, sizeof(back));
    TEST_ASSERT_EQUAL(CFG_OK, device_config_check(&back));
    TEST_ASSERT_EQUAL_STRING("PhoneHotspot", back.ssid);
    TEST_ASSERT_EQUAL_MEMORY(c.fingerprint, back.fingerprint, CFG_FINGERPRINT_LEN);
}

static void test_crc32_known_vector(void) {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, cfg_crc32((const uint8_t *)"123456789", 9));
}

static void test_fits_in_flash_sector(void) {
    TEST_ASSERT_TRUE(sizeof(device_config_t) < 4096);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_valid);
    RUN_TEST(test_field_errors);
    RUN_TEST(test_unterminated_strings_rejected);
    RUN_TEST(test_seal_and_check);
    RUN_TEST(test_serialise_round_trip);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_fits_in_flash_sector);
    return UNITY_END();
}
