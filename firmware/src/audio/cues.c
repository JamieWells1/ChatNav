#include "audio/cues.h"

#include <math.h>

#include "config.h"

#define CUE_AMPLITUDE 16000.0f
#define EDGE_MS 5 /* linear fade at each tone edge to avoid clicks */

/* freq_hz 0 is silence. */
typedef struct {
    uint16_t freq_hz;
    uint16_t ms;
} tone_t;

#define T(f, m) {f, m}
static const tone_t press[] = {T(880, 70), {0, 0}};
static const tone_t release_[] = {T(660, 70), {0, 0}};
static const tone_t cancel[] = {T(660, 80), T(440, 120), {0, 0}};
static const tone_t limit[] = {T(330, 90), T(0, 60), T(330, 90), {0, 0}};
static const tone_t no_network[] = {T(440, 100), T(0, 60), T(440, 100), T(0, 60), T(440, 100), {0, 0}};
static const tone_t server_error[] = {T(220, 400), {0, 0}};
static const tone_t volume_tick[] = {T(1000, 20), {0, 0}};
#undef T

static const struct {
    const tone_t *tones;
    const char *name;
} cues[CUE_COUNT] = {
    [CUE_PRESS] = {press, "press"},
    [CUE_RELEASE] = {release_, "release"},
    [CUE_CANCEL] = {cancel, "cancel"},
    [CUE_LIMIT] = {limit, "limit"},
    [CUE_NO_NETWORK] = {no_network, "no-network"},
    [CUE_SERVER_ERROR] = {server_error, "server-error"},
    [CUE_VOLUME_TICK] = {volume_tick, "volume-tick"},
};

static uint32_t ms_to_samples(uint32_t ms) { return ms * AUDIO_SAMPLE_RATE / 1000; }

uint32_t cue_length_samples(cue_id_t id) {
    uint32_t n = 0;
    for (const tone_t *t = cues[id].tones; t->ms != 0; t++) n += ms_to_samples(t->ms);
    return n;
}

const char *cue_name(cue_id_t id) { return id < CUE_COUNT ? cues[id].name : "unknown"; }

void cue_start(cue_gen_t *g, cue_id_t id) {
    g->id = id;
    g->pos = 0;
    g->total = id < CUE_COUNT ? cue_length_samples(id) : 0;
    g->active = g->total > 0;
}

static int16_t sample_at(cue_id_t id, uint32_t pos) {
    uint32_t start = 0;
    for (const tone_t *t = cues[id].tones; t->ms != 0; t++) {
        uint32_t len = ms_to_samples(t->ms);
        if (pos < start + len) {
            if (t->freq_hz == 0) return 0;
            uint32_t i = pos - start;
            uint32_t edge = ms_to_samples(EDGE_MS);
            float env = 1.0f;
            if (i < edge) env = (float)i / (float)edge;
            else if (len - i <= edge) env = (float)(len - i) / (float)edge;
            float phase = 2.0f * 3.14159265f * (float)t->freq_hz * (float)i / AUDIO_SAMPLE_RATE;
            return (int16_t)(sinf(phase) * CUE_AMPLITUDE * env);
        }
        start += len;
    }
    return 0;
}

size_t cue_next(cue_gen_t *g, int16_t *out, size_t max) {
    if (!g->active) return 0;
    size_t n = 0;
    while (n < max && g->pos < g->total) out[n++] = sample_at(g->id, g->pos++);
    if (g->pos >= g->total) g->active = false;
    return n;
}
