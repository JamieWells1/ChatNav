/*
 * Copyright (c) 2026 Jamie Wells. All Rights Reserved.
 * Proprietary and Confidential - Unauthorized copying or distribution prohibited.
 * Adapted from Photon src/common/error.*
 */

#include "common/error.h"

const char *rc_to_string(ReturnCode rc) {
    switch (rc) {
        case RC_OK: return "OK";
        case RC_ERR_INVALID_PARAM: return "Invalid parameter";
        case RC_ERR_NULL_POINTER: return "Null pointer";
        case RC_ERR_OUT_OF_MEMORY: return "Out of memory";
        case RC_ERR_TIMEOUT: return "Timeout";
        case RC_ERR_BUSY: return "Resource busy";
        case RC_ERR_NOT_INITIALIZED: return "Not initialized";
        case RC_ERR_ALREADY_INIT: return "Already initialized";
        case RC_ERR_UNKNOWN: return "Unknown error";

        case RC_WIFI_INIT_FAILED: return "WiFi: Init failed";
        case RC_WIFI_CONNECT_FAILED: return "WiFi: Connection failed";
        case RC_WIFI_BAD_AUTH: return "WiFi: Bad credentials";
        case RC_WIFI_NO_NETWORK: return "WiFi: Network not found";
        case RC_WIFI_TIMEOUT: return "WiFi: Timeout";
        case RC_WIFI_ALREADY_CONNECTED: return "WiFi: Already connected";
        case RC_WIFI_DISCONNECTED: return "WiFi: Disconnected";
        case RC_WIFI_AP_FAILED: return "WiFi: AP mode failed";

        case RC_HTTP_INVALID_PARAMS: return "HTTP: Invalid params";
        case RC_HTTP_ALREADY_ACTIVE: return "HTTP: Already active";
        case RC_HTTP_ALLOC_FAILED: return "HTTP: Alloc failed";
        case RC_HTTP_DNS_FAILED: return "HTTP: DNS failed";
        case RC_HTTP_CONNECT_FAILED: return "HTTP: Connect failed";
        case RC_HTTP_WRITE_FAILED: return "HTTP: Write failed";
        case RC_HTTP_OUTPUT_FAILED: return "HTTP: Output failed";
        case RC_HTTP_TIMEOUT: return "HTTP: Timeout";
        case RC_HTTP_ERROR: return "HTTP: Protocol error";
        case RC_HTTP_BUFFER_FULL: return "HTTP: Buffer full";

        case RC_FLASH_MAGIC_INVALID: return "Flash: Invalid magic";
        case RC_FLASH_CRC_FAILED: return "Flash: CRC failed";
        case RC_FLASH_WRITE_FAILED: return "Flash: Write failed";
        case RC_FLASH_ERASE_FAILED: return "Flash: Erase failed";
        case RC_FLASH_NOT_FOUND: return "Flash: Not found";

        case RC_AUDIO_PIO_FAILED: return "Audio: PIO setup failed";
        case RC_AUDIO_DMA_FAILED: return "Audio: DMA setup failed";
        case RC_AUDIO_OVERRUN: return "Audio: Mic overrun";
        case RC_AUDIO_UNDERRUN: return "Audio: Speaker underrun";

        case RC_GPS_NO_DATA: return "GPS: No data";
        case RC_GPS_BAD_CHECKSUM: return "GPS: Bad checksum";
        case RC_GPS_NO_FIX: return "GPS: No fix";

        case RC_TLS_CONFIG_FAILED: return "TLS: Config failed";
        case RC_TLS_DNS_FAILED: return "TLS: DNS failed";
        case RC_TLS_CONNECT_FAILED: return "TLS: Connect failed";
        case RC_TLS_PIN_MISMATCH: return "TLS: Certificate pin mismatch";
        case RC_TLS_NO_CERT: return "TLS: No peer certificate";
        case RC_TLS_WRITE_FAILED: return "TLS: Write failed";
        case RC_TLS_CLOSED: return "TLS: Connection closed";

        default: return "Unknown return code";
    }
}
