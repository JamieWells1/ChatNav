#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

#include "common/error.h"

/* Adapted from Photon src/middleware/wifi.c: non-blocking so the main loop keeps running. */

typedef enum {
    WIFI_IDLE,
    WIFI_JOINING,
    WIFI_UP,
    WIFI_FAILED, /* bad password, no such network or join failed; see wifi_last_error */
} wifi_status_t;

/* Initialises the radio once. */
ReturnCode wifi_init(void);

/* Open-ended join; poll wifi_poll. An empty password means an open network. */
ReturnCode wifi_start_join(const char *ssid, const char *password);

/* Updates and returns the join state. Reports WIFI_FAILED after timeout_s without a link. */
wifi_status_t wifi_poll(unsigned timeout_s);
ReturnCode wifi_last_error(void);
bool wifi_link_up(void);

/* WPA2 access point at 192.168.4.1 with a DHCP server (setup mode). */
ReturnCode wifi_start_ap(const char *ssid, const char *password);

#endif
