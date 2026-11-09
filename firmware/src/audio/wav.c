#include "audio/wav.h"

#include <string.h>

static void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void wav_header(uint8_t out[WAV_HEADER_SIZE], uint32_t sample_rate, uint32_t data_bytes) {
    memcpy(out, "RIFF", 4);
    put32(out + 4, 36 + data_bytes);
    memcpy(out + 8, "WAVEfmt ", 8);
    put32(out + 16, 16);
    put16(out + 20, 1);  /* PCM */
    put16(out + 22, 1);  /* mono */
    put32(out + 24, sample_rate);
    put32(out + 28, sample_rate * 2);
    put16(out + 32, 2);  /* block align */
    put16(out + 34, 16); /* bits per sample */
    memcpy(out + 36, "data", 4);
    put32(out + 40, data_bytes);
}
