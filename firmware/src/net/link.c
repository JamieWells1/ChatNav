#include "net/link.h"

#include <stdio.h>

#include "config.h"
#include "net/http_stream.h"
#include "net/tls_client.h"
#include "net/wifi.h"
#include "pico/time.h"

#define RETRY_DELAY_MS 5000
#define REPLY_TIMEOUT_MS 5000
#define WATCH_INTERVAL_MS 250

typedef enum {
    STEP_JOIN,
    STEP_TLS,
    STEP_CHECK,
    STEP_UP,
    STEP_BACKOFF,
} step_t;

static const device_config_t *cfg;
static step_t step = STEP_BACKOFF;
static absolute_time_t retry_at;
static absolute_time_t tls_deadline;
static absolute_time_t next_watch;
static bool reconnecting; /* quick reopen of the server connection; skips the health checks */

static void back_off(const char *why) {
    reconnecting = false;
    printf("[LINK] %s, retrying in %d s\n", why, RETRY_DELAY_MS / 1000);
    tls_client_close();
    step = STEP_BACKOFF;
    retry_at = make_timeout_time_ms(RETRY_DELAY_MS);
}

void link_start(const device_config_t *c) {
    cfg = c;
    step = STEP_BACKOFF;
    retry_at = get_absolute_time();
}

void link_force_reconnect(void) {
    tls_client_close();
    if (!cfg || !wifi_link_up()) {
        printf("[LINK] reconnect requested but Wi-Fi is down\n");
        step = STEP_BACKOFF;
        retry_at = get_absolute_time();
        return;
    }
    printf("[LINK] reopening server connection\n");
    reconnecting = true;
    tls_client_open(cfg->server_host, cfg->server_port, cfg->fingerprint, NULL, NULL);
    tls_deadline = make_timeout_time_ms(SERVER_CONNECT_TIMEOUT_S * 1000);
    step = STEP_TLS;
}

/* Health proves TLS and routing; whoami proves the bearer token is accepted. */
static bool check_server(void) {
    http_reply_t reply;
    ReturnCode rc = http_stream_get(cfg->server_host, "/v1/health", NULL, REPLY_TIMEOUT_MS, &reply);
    if (RC_FAILED(rc) || reply.status != 200) {
        printf("[LINK] health check failed: %s (status %d)\n", rc_to_string(rc), reply.status);
        return false;
    }
    printf("[LINK] health ok\n");

    rc = http_stream_get(cfg->server_host, "/v1/whoami", cfg->token, REPLY_TIMEOUT_MS, &reply);
    if (RC_FAILED(rc)) {
        printf("[LINK] whoami failed: %s\n", rc_to_string(rc));
        return false;
    }
    if (reply.status == 401) {
        printf("[LINK] server rejected the device token (401); fix it in setup mode\n");
        return false;
    }
    if (reply.status != 200) {
        printf("[LINK] whoami unexpected status %d\n", reply.status);
        return false;
    }
    printf("[LINK] authenticated: %s\n", reply.body);
    return true;
}

link_status_t link_poll(void) {
    switch (step) {
        case STEP_BACKOFF:
            if (absolute_time_diff_us(get_absolute_time(), retry_at) > 0) break;
            if (RC_FAILED(wifi_start_join(cfg->ssid, cfg->password))) {
                back_off("could not start Wi-Fi join");
            } else {
                step = STEP_JOIN;
            }
            break;

        case STEP_JOIN: {
            wifi_status_t w = wifi_poll(WIFI_CONNECT_TIMEOUT_S);
            if (w == WIFI_UP) {
                tls_client_open(cfg->server_host, cfg->server_port, cfg->fingerprint, NULL, NULL);
                tls_deadline = make_timeout_time_ms(SERVER_CONNECT_TIMEOUT_S * 1000);
                step = STEP_TLS;
            } else if (w == WIFI_FAILED) {
                back_off(rc_to_string(wifi_last_error()));
            }
            break;
        }

        case STEP_TLS: {
            tls_state_t t = tls_client_state();
            bool was_reconnect = reconnecting;
            if (t == TLS_READY) {
                step = reconnecting ? STEP_UP : STEP_CHECK;
                if (reconnecting) printf("[LINK] reconnected\n");
                reconnecting = false;
            } else if (t == TLS_FAILED) {
                back_off(rc_to_string(tls_client_error()));
                if (was_reconnect) return LINK_LOST;
            } else if (absolute_time_diff_us(get_absolute_time(), tls_deadline) <= 0) {
                back_off("server connect timed out");
                if (was_reconnect) return LINK_LOST;
            }
            break;
        }

        case STEP_CHECK:
            if (check_server()) {
                step = STEP_UP;
                printf("[LINK] up\n");
            } else {
                back_off("server check failed");
            }
            break;

        case STEP_UP:
            if (absolute_time_diff_us(get_absolute_time(), next_watch) > 0) return LINK_UP;
            next_watch = make_timeout_time_ms(WATCH_INTERVAL_MS);
            if (wifi_poll(WIFI_CONNECT_TIMEOUT_S) != WIFI_UP || tls_client_state() != TLS_READY) {
                back_off("link lost");
                return LINK_LOST;
            }
            return LINK_UP;
    }
    return LINK_CONNECTING;
}
