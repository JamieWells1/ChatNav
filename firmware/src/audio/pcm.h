#ifndef PCM_H
#define PCM_H

#include <stddef.h>
#include <stdint.h>

/* Mic words arrive as interleaved left/right 32-bit I2S slots. The INMP441 puts a
 * 24-bit sample in the top bits of the left slot. This keeps the left slot only,
 * shifts the top 16 bits down by (16 - gain_shift) bits, and saturates.
 * words_in must hold 2 * frames words. Returns frames written. */
size_t pcm_from_i2s32(const int32_t *words_in, size_t frames, int16_t *out, int gain_shift);

/* Duplicates mono samples into packed stereo words for the amp: left in the high
 * half, right in the low half, scaled by volume step. */
void pcm_mono_to_stereo(const int16_t *mono, size_t n, uint32_t *out, int volume_step);

int16_t pcm_peak(const int16_t *s, size_t n);
uint32_t pcm_rms(const int16_t *s, size_t n);

#endif
