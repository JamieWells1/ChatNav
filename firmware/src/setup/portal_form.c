#include "setup/portal_form.h"

#include <stdlib.h>
#include <string.h>

#include "net/fingerprint.h"

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decodes the field value starting at src (ends at '&' or NUL) into out. */
static int decode_value(const char *src, char *out, size_t cap) {
    size_t n = 0;
    while (*src && *src != '&') {
        char c = *src++;
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            int hi = hex_val(src[0]);
            int lo = hi < 0 ? -1 : hex_val(src[1]);
            if (hi < 0 || lo < 0) return FORM_BAD;
            c = (char)((hi << 4) | lo);
            src += 2;
        }
        if (n + 1 >= cap) return FORM_BAD;
        out[n++] = c;
    }
    out[n] = '\0';
    return (int)n;
}

int form_get(const char *body, const char *key, char *out, size_t cap) {
    size_t klen = strlen(key);
    const char *p = body;
    while (*p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            return decode_value(p + klen + 1, out, cap);
        }
        const char *amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return FORM_MISSING;
}

/* Copies a form field into a fixed char array; missing fields give an empty string. */
static bool field(const char *body, const char *key, char *dst, size_t cap) {
    int n = form_get(body, key, dst, cap);
    if (n == FORM_BAD) return false;
    if (n == FORM_MISSING) dst[0] = '\0';
    return true;
}

cfg_result_t portal_form_to_config(const char *body, const device_config_t *base,
                                   device_config_t *out) {
    device_config_t cfg;
    device_config_defaults(&cfg);
    if (base) cfg.volume_step = base->volume_step;

    if (!field(body, "ssid", cfg.ssid, sizeof(cfg.ssid))) return CFG_ERR_SSID;
    if (!field(body, "password", cfg.password, sizeof(cfg.password))) return CFG_ERR_PASSWORD;
    if (!field(body, "host", cfg.server_host, sizeof(cfg.server_host))) return CFG_ERR_HOST;
    if (!field(body, "token", cfg.token, sizeof(cfg.token))) return CFG_ERR_TOKEN;

    char port[8];
    int pn = form_get(body, "port", port, sizeof(port));
    if (pn == FORM_BAD) return CFG_ERR_PORT;
    if (pn > 0) {
        char *end;
        long v = strtol(port, &end, 10);
        if (*end != '\0' || v < 1 || v > 65535) return CFG_ERR_PORT;
        cfg.server_port = (uint16_t)v;
    }

    char fp[128];
    int fn = form_get(body, "fingerprint", fp, sizeof(fp));
    if (fn == FORM_BAD || (fn >= 0 && !fp_parse(fp, cfg.fingerprint))) return CFG_ERR_FINGERPRINT;

    cfg_result_t r = device_config_validate(&cfg);
    if (r == CFG_OK) {
        device_config_seal(&cfg);
        *out = cfg;
    }
    return r;
}
