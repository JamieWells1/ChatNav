/*
 * Adapted from Photon src/middleware/wifi.c
 */

#include "net/wifi.h"

#include <stdio.h>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "setup/dhcpserver.h"

static bool initialised;
static wifi_status_t status = WIFI_IDLE;
static ReturnCode last_error = RC_OK;
static absolute_time_t join_started;
static dhcp_server_t dhcp;

ReturnCode wifi_init(void) {
    if (initialised) return RC_OK;
    if (cyw43_arch_init()) {
        printf("[WIFI] radio init failed\n");
        return RC_WIFI_INIT_FAILED;
    }
    initialised = true;
    printf("[WIFI] radio ready\n");
    return RC_OK;
}

ReturnCode wifi_start_join(const char *ssid, const char *password) {
    ReturnCode rc = wifi_init();
    if (RC_FAILED(rc)) return rc;

    cyw43_arch_enable_sta_mode();
    uint32_t auth = (password && password[0]) ? CYW43_AUTH_WPA2_MIXED_PSK : CYW43_AUTH_OPEN;
    int e = cyw43_arch_wifi_connect_async(ssid, password && password[0] ? password : NULL, auth);
    if (e != 0) {
        printf("[WIFI] join start failed (%d)\n", e);
        status = WIFI_FAILED;
        last_error = RC_WIFI_CONNECT_FAILED;
        return last_error;
    }
    printf("[WIFI] joining '%s'\n", ssid);
    join_started = get_absolute_time();
    status = WIFI_JOINING;
    last_error = RC_OK;
    return RC_OK;
}

static void log_addresses(void) {
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    printf("[WIFI] up, ip %s gw %s\n", ip4addr_ntoa(netif_ip4_addr(n)),
           ip4addr_ntoa(netif_ip4_gw(n)));
}

wifi_status_t wifi_poll(unsigned timeout_s) {
    if (status == WIFI_JOINING) {
        int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (link == CYW43_LINK_UP) {
            log_addresses();
            status = WIFI_UP;
        } else if (link == CYW43_LINK_BADAUTH) {
            printf("[WIFI] bad password\n");
            status = WIFI_FAILED;
            last_error = RC_WIFI_BAD_AUTH;
        } else if (link == CYW43_LINK_NONET) {
            printf("[WIFI] network not found\n");
            status = WIFI_FAILED;
            last_error = RC_WIFI_NO_NETWORK;
        } else if (link == CYW43_LINK_FAIL) {
            printf("[WIFI] join failed\n");
            status = WIFI_FAILED;
            last_error = RC_WIFI_CONNECT_FAILED;
        } else if (absolute_time_diff_us(join_started, get_absolute_time()) >
                   (int64_t)timeout_s * 1000000) {
            printf("[WIFI] join timed out after %us\n", timeout_s);
            status = WIFI_FAILED;
            last_error = RC_WIFI_TIMEOUT;
        }
    } else if (status == WIFI_UP && !wifi_link_up()) {
        printf("[WIFI] link lost\n");
        status = WIFI_FAILED;
        last_error = RC_WIFI_DISCONNECTED;
    }
    return status;
}

ReturnCode wifi_last_error(void) { return last_error; }

bool wifi_link_up(void) {
    return initialised &&
           cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

ReturnCode wifi_start_ap(const char *ssid, const char *password) {
    ReturnCode rc = wifi_init();
    if (RC_FAILED(rc)) return rc;

    cyw43_arch_enable_ap_mode(ssid, password, CYW43_AUTH_WPA2_AES_PSK);

    ip_addr_t gw, mask;
    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
    dhcp_server_init(&dhcp, &gw, &mask);
    printf("[WIFI] access point '%s' up at 192.168.4.1\n", ssid);
    return RC_OK;
}
