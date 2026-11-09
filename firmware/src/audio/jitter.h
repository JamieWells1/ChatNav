#ifndef JITTER_H
#define JITTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Playback jitter buffer: single-producer (network receive) single-consumer (speaker DMA
 * fill) ring of mono 16-bit samples. Reading is held off until PLAYBACK_PREBUFFER_MS of
 * audio is queued, or the stream has ended. Running dry mid-stream returns fewer samples
 * than asked for (the caller pads with silence) and counts one underrun per dry spell.
 * Indices are only ever written by their own side, so no lock is needed. */

typedef struct {
    int16_t *buf;
    uint32_t cap;
    volatile uint32_t head; /* total samples written (producer) */
    volatile uint32_t tail; /* total samples read (consumer) */
    uint32_t prebuffer;     /* samples to queue before playing */
    volatile bool started;
    volatile bool finished; /* producer will write no more */
    volatile uint32_t underruns;
    bool dry;               /* consumer side: already counted this dry spell */
    bool have_odd;          /* producer side: low byte of a split sample is held */
    uint8_t odd;
} jitter_t;

void jitter_init(jitter_t *j, int16_t *storage, uint32_t capacity, uint32_t prebuffer_samples);

/* Empties the buffer and clears started/finished. Call only while the consumer is idle. */
void jitter_reset(jitter_t *j);

/* Producer: free room in samples. */
size_t jitter_free(const jitter_t *j);
size_t jitter_queued(const jitter_t *j);

/* Producer: little-endian PCM bytes (may split a sample across calls). Returns the bytes
 * accepted; fewer than len means the buffer is full and the caller should retry later. */
size_t jitter_write_bytes(jitter_t *j, const uint8_t *data, size_t len);

/* Producer: no more data will come. */
void jitter_finish(jitter_t *j);

/* Consumer: copies up to n samples. Returns 0 until prebuffered (or finished). */
size_t jitter_read(jitter_t *j, int16_t *out, size_t n);

/* True once finished and every sample has been read. */
bool jitter_drained(const jitter_t *j);

#endif
