#include "storage/device_config.h"

#include <string.h>

#include "config.h"

uint32_t cfg_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void device_config_defaults(device_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->server_port = 443;
    cfg->volume_step = DEFAULT_VOLUME_STEP;
}

static bool is_host_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '-';
}

static bool is_token_char(char c) {
    return c > ' ' && c < 0x7F;
}

/* True if buf holds a NUL-terminated string whose length is within [min, max]. */
static bool length_ok(const char *buf, size_t cap, size_t min, size_t max) {
    size_t n = strnlen(buf, cap);
    return n < cap && n >= min && n <= max;
}

cfg_result_t device_config_validate(const device_config_t *cfg) {
    if (!length_ok(cfg->ssid, sizeof(cfg->ssid), 1, CFG_SSID_MAX)) return CFG_ERR_SSID;

    /* Empty password means an open network; otherwise WPA2 rules. */
    size_t pw = strnlen(cfg->password, sizeof(cfg->password));
    if (pw == sizeof(cfg->password)) return CFG_ERR_PASSWORD;
    if (pw != 0 && (pw < CFG_PASSWORD_MIN || pw > CFG_PASSWORD_MAX)) return CFG_ERR_PASSWORD;

    if (!length_ok(cfg->server_host, sizeof(cfg->server_host), 1, CFG_HOST_MAX)) {
        return CFG_ERR_HOST;
    }
    for (const char *p = cfg->server_host; *p; p++) {
        if (!is_host_char(*p)) return CFG_ERR_HOST;
    }
    if (cfg->server_host[0] == '.' || cfg->server_host[0] == '-') return CFG_ERR_HOST;

    if (cfg->server_port == 0) return CFG_ERR_PORT;

    if (!length_ok(cfg->token, sizeof(cfg->token), CFG_TOKEN_MIN, CFG_TOKEN_MAX)) {
        return CFG_ERR_TOKEN;
    }
    for (const char *p = cfg->token; *p; p++) {
        if (!is_token_char(*p)) return CFG_ERR_TOKEN;
    }

    uint8_t any = 0;
    for (size_t i = 0; i < CFG_FINGERPRINT_LEN; i++) any |= cfg->fingerprint[i];
    if (any == 0) return CFG_ERR_FINGERPRINT;
    return CFG_OK;
}

static uint32_t config_crc(const device_config_t *cfg) {
    return cfg_crc32((const uint8_t *)cfg, offsetof(device_config_t, crc32));
}

void device_config_seal(device_config_t *cfg) {
    cfg->magic = DEVICE_CONFIG_MAGIC;
    cfg->version = DEVICE_CONFIG_VERSION;
    cfg->crc32 = config_crc(cfg);
}

cfg_result_t device_config_check(const device_config_t *cfg) {
    if (cfg->magic != DEVICE_CONFIG_MAGIC) return CFG_ERR_MAGIC;
    if (cfg->version != DEVICE_CONFIG_VERSION) return CFG_ERR_VERSION;
    if (cfg->crc32 != config_crc(cfg)) return CFG_ERR_CRC;
    return CFG_OK;
}

const char *cfg_result_name(cfg_result_t r) {
    switch (r) {
        case CFG_OK: return "ok";
        case CFG_ERR_SSID: return "Wi-Fi name must be 1-32 characters";
        case CFG_ERR_PASSWORD: return "Wi-Fi password must be empty or 8-63 characters";
        case CFG_ERR_HOST: return "Server host must be a hostname or IPv4 address";
        case CFG_ERR_PORT: return "Server port must be 1-65535";
        case CFG_ERR_TOKEN: return "Device token must be 16-64 printable characters";
        case CFG_ERR_FINGERPRINT: return "Fingerprint must be 64 hex digits (SHA-256)";
        case CFG_ERR_MAGIC: return "no stored config";
        case CFG_ERR_VERSION: return "stored config has a different version";
        case CFG_ERR_CRC: return "stored config is corrupt";
    }
    return "unknown";
}
