#include "audio/volume.h"

#include <math.h>

#include "config.h"

#define VOLUME_DB_PER_STEP 3.0f

int volume_clamp_step(int step) {
    if (step < 0) return 0;
    if (step > VOLUME_STEPS - 1) return VOLUME_STEPS - 1;
    return step;
}

uint32_t volume_gain_q15(int step) {
    step = volume_clamp_step(step);
    if (step == 0) return 0;
    float db = -VOLUME_DB_PER_STEP * (float)(VOLUME_STEPS - 1 - step);
    return (uint32_t)(powf(10.0f, db / 20.0f) * VOLUME_UNITY_Q15 + 0.5f);
}

void volume_apply(int16_t *samples, size_t n, int step) {
    int32_t gain = (int32_t)volume_gain_q15(step);
    for (size_t i = 0; i < n; i++) {
        int32_t v = ((int32_t)samples[i] * gain) >> 15;
        samples[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
}
