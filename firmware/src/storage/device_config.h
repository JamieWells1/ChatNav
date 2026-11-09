#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEVICE_CONFIG_MAGIC 0x434E4156u /* "CNAV" */
#define DEVICE_CONFIG_VERSION 1

#define CFG_SSID_MAX 32
#define CFG_PASSWORD_MAX 63
#define CFG_PASSWORD_MIN 8
#define CFG_HOST_MAX 63
#define CFG_TOKEN_MIN 16
#define CFG_TOKEN_MAX 64
#define CFG_FINGERPRINT_LEN 32

/* Stored in the last flash sector. Plain C struct: fixed-width fields, no pointers. */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t server_port;
    char ssid[CFG_SSID_MAX + 1];
    char password[CFG_PASSWORD_MAX + 1];
    char server_host[CFG_HOST_MAX + 1];
    char token[CFG_TOKEN_MAX + 1];
    uint8_t fingerprint[CFG_FINGERPRINT_LEN]; /* SHA-256 of the server leaf cert DER */
    uint8_t volume_step;
    uint8_t reserved[3];
    uint32_t crc32;
} device_config_t;

typedef enum {
    CFG_OK = 0,
    CFG_ERR_SSID,
    CFG_ERR_PASSWORD,
    CFG_ERR_HOST,
    CFG_ERR_PORT,
    CFG_ERR_TOKEN,
    CFG_ERR_FINGERPRINT,
    CFG_ERR_MAGIC,
    CFG_ERR_VERSION,
    CFG_ERR_CRC,
} cfg_result_t;

uint32_t cfg_crc32(const uint8_t *data, size_t len);

/* Zeroes the struct and sets defaults (port 443, default volume step). */
void device_config_defaults(device_config_t *cfg);

/* Checks every user-entered field. Does not look at magic or CRC. */
cfg_result_t device_config_validate(const device_config_t *cfg);

/* Sets magic, version and CRC so the struct is ready to store. */
void device_config_seal(device_config_t *cfg);

/* Checks magic, version and CRC of a struct read back from storage. */
cfg_result_t device_config_check(const device_config_t *cfg);

const char *cfg_result_name(cfg_result_t r);

#endif
