/* Diagnostic: microphone only. Send 'r' over USB serial (tools/wav_capture.py does this)
 * to record DIAG_SECONDS of audio, which is then streamed back as a WAV file between
 * "[MIC] WAV_BEGIN <bytes>" and "[MIC] WAV_END" lines. */
#include <stdio.h>
#include <string.h>

#include "audio/mic.h"
#include "audio/pcm.h"
#include "audio/wav.h"
#include "config.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"

#define DIAG_SECONDS 5
#define CLIP_SAMPLES (AUDIO_SAMPLE_RATE * DIAG_SECONDS)
#define SEND_CHUNK 256

static int16_t clip[CLIP_SAMPLES];

static void send_raw(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i += SEND_CHUNK) {
        size_t n = len - i < SEND_CHUNK ? len - i : SEND_CHUNK;
        stdio_put_string((const char *)data + i, (int)n, false, false);
    }
}

static void record_and_send(void) {
    printf("[MIC] recording %d s\n", DIAG_SECONDS);
    size_t have = 0;
    int16_t block[MIC_BLOCK_FRAMES];
    mic_start();
    while (have < CLIP_SAMPLES) {
        size_t n = mic_poll(block, MIC_BLOCK_FRAMES);
        if (n == 0) continue;
        if (n > CLIP_SAMPLES - have) n = CLIP_SAMPLES - have;
        memcpy(clip + have, block, n * sizeof(block[0]));
        have += n;
    }
    mic_stop();

    int64_t sum = 0;
    for (size_t i = 0; i < have; i++) sum += clip[i];
    printf("[MIC] captured %u samples, peak %d, rms %lu, mean %ld, overruns %lu\n", (unsigned)have,
           pcm_peak(clip, have), (unsigned long)pcm_rms(clip, have), (long)(sum / (int64_t)have),
           (unsigned long)mic_overruns());
    if (pcm_peak(clip, have) == 0) {
        printf("[MIC] all samples are zero: check SD GP%d, SCK GP%d, WS GP%d and mic power GP%d\n",
               PIN_MIC_SD, PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_VDD);
    }

    uint32_t data_bytes = (uint32_t)(have * sizeof(clip[0]));
    uint8_t header[WAV_HEADER_SIZE];
    wav_header(header, AUDIO_SAMPLE_RATE, data_bytes);
    printf("[MIC] WAV_BEGIN %lu\n", (unsigned long)(WAV_HEADER_SIZE + data_bytes));
    stdio_flush();
    send_raw(header, sizeof(header));
    send_raw((const uint8_t *)clip, data_bytes);
    stdio_flush();
    printf("\n[MIC] WAV_END\n");
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("[MIC] diag_mic start\n");
    if (mic_init() != RC_OK) {
        printf("[MIC] init failed\n");
        while (true) sleep_ms(1000);
    }

    uint32_t last_prompt = 0;
    while (true) {
        int c = getchar_timeout_us(100000);
        if (c == 'r' || c == 'R') {
            record_and_send();
            continue;
        }
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_prompt >= 5000) {
            last_prompt = now;
            printf("[MIC] waiting: send 'r' over serial to record (tools/wav_capture.py)\n");
        }
    }
}
