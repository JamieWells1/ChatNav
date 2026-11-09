#include "net/fingerprint.h"

#include <string.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fp_parse(const char *text, uint8_t out[FP_LEN]) {
    if (!text) return false;
    static const char prefix[] = "SHA256 Fingerprint=";
    while (*text == ' ') text++;
    if (strncmp(text, prefix, sizeof(prefix) - 1) == 0) text += sizeof(prefix) - 1;

    uint8_t tmp[FP_LEN];
    int digits = 0;
    for (; *text; text++) {
        char c = *text;
        if (c == ':' || c == ' ' || c == '-' || c == '\r' || c == '\n') continue;
        int v = hex_val(c);
        if (v < 0 || digits >= FP_LEN * 2) return false;
        if (digits % 2 == 0) {
            tmp[digits / 2] = (uint8_t)(v << 4);
        } else {
            tmp[digits / 2] |= (uint8_t)v;
        }
        digits++;
    }
    if (digits != FP_LEN * 2) return false;
    memcpy(out, tmp, FP_LEN);
    return true;
}

void fp_format(const uint8_t fp[FP_LEN], char out[FP_TEXT_LEN]) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (int i = 0; i < FP_LEN; i++) {
        if (i) out[o++] = ':';
        out[o++] = hex[fp[i] >> 4];
        out[o++] = hex[fp[i] & 15];
    }
    out[o] = '\0';
}

bool fp_equal(const uint8_t a[FP_LEN], const uint8_t b[FP_LEN]) {
    uint8_t diff = 0;
    for (int i = 0; i < FP_LEN; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}
