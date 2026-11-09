#ifndef VOLUME_H
#define VOLUME_H

#include <stddef.h>
#include <stdint.h>

#define VOLUME_UNITY_Q15 32768 /* gain of 1.0 */

/* Q15 gain for a step in [0, VOLUME_STEPS-1]. Step 0 is mute, the top step is unity,
 * and each step below the top is 3 dB quieter. Out-of-range steps are clamped. */
uint32_t volume_gain_q15(int step);

/* Scales samples in place by the gain for step. */
void volume_apply(int16_t *samples, size_t n, int step);

int volume_clamp_step(int step);

#endif
