#include "audio/pcm.h"

#include <math.h>

#include "audio/volume.h"

static int16_t saturate16(int64_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

size_t pcm_from_i2s32(const int32_t *words_in, size_t frames, int16_t *out, int gain_shift) {
    if (gain_shift < 0) gain_shift = 0;
    if (gain_shift > 15) gain_shift = 15;
    for (size_t i = 0; i < frames; i++) {
        int32_t top16 = words_in[2 * i] >> 16; /* arithmetic shift keeps the sign */
        out[i] = saturate16((int64_t)top16 * (1 << gain_shift));
    }
    return frames;
}

void pcm_mono_to_stereo(const int16_t *mono, size_t n, uint32_t *out, int volume_step) {
    int32_t gain = (int32_t)volume_gain_q15(volume_step);
    for (size_t i = 0; i < n; i++) {
        uint16_t s = (uint16_t)saturate16(((int32_t)mono[i] * gain) >> 15);
        out[i] = ((uint32_t)s << 16) | s;
    }
}

int16_t pcm_peak(const int16_t *s, size_t n) {
    int32_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t a = s[i] < 0 ? -(int32_t)s[i] : s[i];
        if (a > peak) peak = a;
    }
    return (int16_t)(peak > 32767 ? 32767 : peak);
}

uint32_t pcm_rms(const int16_t *s, size_t n) {
    if (n == 0) return 0;
    double acc = 0;
    for (size_t i = 0; i < n; i++) acc += (double)s[i] * s[i];
    return (uint32_t)sqrt(acc / (double)n);
}
