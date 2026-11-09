/*
 * Copyright (c) 2026 Jamie Wells. All Rights Reserved.
 * Proprietary and Confidential - Unauthorized copying or distribution prohibited.
 * Adapted from Photon src/common/error.*
 */

#ifndef ERROR_H
#define ERROR_H

/* Return codes: 0 = success, negative = error, grouped by module in ranges of 100.
 * -1xx common, -2xx WiFi, -3xx HTTP, -4xx flash, -5xx audio, -6xx GPS, -7xx input */

typedef enum {
    RC_OK = 0,

    RC_ERR_INVALID_PARAM = -100,
    RC_ERR_NULL_POINTER = -101,
    RC_ERR_OUT_OF_MEMORY = -102,
    RC_ERR_TIMEOUT = -103,
    RC_ERR_BUSY = -104,
    RC_ERR_NOT_INITIALIZED = -105,
    RC_ERR_ALREADY_INIT = -106,
    RC_ERR_UNKNOWN = -199,

    RC_WIFI_INIT_FAILED = -200,
    RC_WIFI_CONNECT_FAILED = -201,
    RC_WIFI_BAD_AUTH = -202,
    RC_WIFI_NO_NETWORK = -203,
    RC_WIFI_TIMEOUT = -204,
    RC_WIFI_ALREADY_CONNECTED = -205,
    RC_WIFI_DISCONNECTED = -206,
    RC_WIFI_AP_FAILED = -207,

    RC_HTTP_INVALID_PARAMS = -300,
    RC_HTTP_ALREADY_ACTIVE = -301,
    RC_HTTP_ALLOC_FAILED = -302,
    RC_HTTP_DNS_FAILED = -303,
    RC_HTTP_CONNECT_FAILED = -304,
    RC_HTTP_WRITE_FAILED = -305,
    RC_HTTP_OUTPUT_FAILED = -306,
    RC_HTTP_TIMEOUT = -307,
    RC_HTTP_ERROR = -308,
    RC_HTTP_BUFFER_FULL = -309,

    RC_FLASH_MAGIC_INVALID = -400,
    RC_FLASH_CRC_FAILED = -401,
    RC_FLASH_WRITE_FAILED = -402,
    RC_FLASH_ERASE_FAILED = -403,
    RC_FLASH_NOT_FOUND = -404,

    RC_AUDIO_PIO_FAILED = -500,   /* no free PIO state machine or program space */
    RC_AUDIO_DMA_FAILED = -501,   /* no free DMA channel */
    RC_AUDIO_OVERRUN = -502,      /* mic DMA buffers not drained in time */
    RC_AUDIO_UNDERRUN = -503,     /* speaker ran out of samples */

    RC_GPS_NO_DATA = -600,        /* no bytes from the module */
    RC_GPS_BAD_CHECKSUM = -601,
    RC_GPS_NO_FIX = -602,
} ReturnCode;

#define RC_SUCCESS(rc) ((rc) == RC_OK)
#define RC_FAILED(rc) ((rc) != RC_OK)

const char *rc_to_string(ReturnCode rc);

#endif
