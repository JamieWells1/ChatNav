#ifndef CUES_H
#define CUES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CUE_PRESS,
    CUE_RELEASE,
    CUE_CANCEL,
    CUE_LIMIT,
    CUE_NO_NETWORK,
    CUE_SERVER_ERROR,
    CUE_VOLUME_TICK,
    CUE_COUNT,
} cue_id_t;

/* Streaming generator: call cue_next until it returns 0. Output is full-scale-relative
 * mono at AUDIO_SAMPLE_RATE; volume is applied later by the speaker. */
typedef struct {
    cue_id_t id;
    uint32_t pos;    /* samples already produced */
    uint32_t total;  /* samples in the whole cue */
    bool active;
} cue_gen_t;

void cue_start(cue_gen_t *g, cue_id_t id);

/* Writes up to max samples, returns the count (0 when the cue is finished). */
size_t cue_next(cue_gen_t *g, int16_t *out, size_t max);

uint32_t cue_length_samples(cue_id_t id);
const char *cue_name(cue_id_t id);

#endif
