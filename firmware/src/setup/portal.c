#include "setup/portal.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "net/wifi.h"
#include "setup/dns_server.h"
#include "setup/portal_form.h"
#include "setup/portal_html.h"
#include "setup/portal_http.h"
#include "storage/flash_memory.h"

#define PORTAL_URL "http://192.168.4.1/"

static bool saved;

static size_t handle_save(const portal_request_t *req, char *out, size_t cap) {
    device_config_t base, cfg;
    flash_memory_read(&base); /* keeps the stored volume; defaults if nothing stored */

    cfg_result_t v = portal_form_to_config(req->body, &base, &cfg);
    if (v != CFG_OK) {
        printf("[PORTAL] form rejected: %s\n", cfg_result_name(v));
        return html_form_page(out, cap, cfg_result_name(v));
    }
    if (RC_FAILED(flash_memory_write(&cfg))) {
        return html_form_page(out, cap, "Could not write settings to flash, try again");
    }
    printf("[PORTAL] config saved for ssid '%s', server %s:%u\n", cfg.ssid, cfg.server_host,
           cfg.server_port);
    saved = true;
    return html_saved_page(out, cap);
}

static size_t handle(const portal_request_t *req, char *out, size_t cap) {
    printf("[PORTAL] %s %s\n", req->is_post ? "POST" : "GET", req->path);
    if (req->is_post && strcmp(req->path, "/save") == 0) return handle_save(req, out, cap);
    if (strcmp(req->path, "/") == 0) return html_form_page(out, cap, NULL);
    /* OS connectivity probes and any other host land on the form. */
    return html_redirect(out, cap, PORTAL_URL);
}

ReturnCode portal_start(void) {
    saved = false;
    ReturnCode rc = wifi_start_ap(SETUP_AP_SSID, SETUP_AP_PASSWORD);
    if (RC_FAILED(rc)) return rc;
    if (!dns_server_start() || !portal_http_start(handle)) return RC_ERR_UNKNOWN;
    printf("[PORTAL] join Wi-Fi '%s' (password '%s') and open %s\n", SETUP_AP_SSID,
           SETUP_AP_PASSWORD, PORTAL_URL);
    return RC_OK;
}

bool portal_config_saved(void) { return saved; }
