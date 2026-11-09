#include "audio/playback.h"

#include <stdio.h>

#include "audio/speaker.h"
#include "config.h"

#define JITTER_SAMPLES 12288 /* 768 ms, 24 KB */
#define PREBUFFER_SAMPLES ((uint32_t)AUDIO_SAMPLE_RATE * PLAYBACK_PREBUFFER_MS / 1000)

static int16_t storage[JITTER_SAMPLES];
static jitter_t jitter;

void playback_init(void) {
    jitter_init(&jitter, storage, JITTER_SAMPLES, PREBUFFER_SAMPLES);
    speaker_set_source(&jitter);
    printf("[PLAY] jitter buffer %d samples, prebuffer %u samples\n", JITTER_SAMPLES,
           (unsigned)PREBUFFER_SAMPLES);
}

jitter_t *playback_buffer(void) { return &jitter; }

void playback_reset(void) {
    speaker_set_source(NULL); /* the DMA fill must not read while the indices are cleared */
    if (jitter.underruns) printf("[PLAY] last reply: %u underruns\n", (unsigned)jitter.underruns);
    jitter_reset(&jitter);
    speaker_set_source(&jitter);
}

bool playback_finished(void) { return jitter_drained(&jitter) && speaker_queued() == 0; }
