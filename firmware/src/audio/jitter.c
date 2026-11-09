#include "audio/jitter.h"

void jitter_init(jitter_t *j, int16_t *storage, uint32_t capacity, uint32_t prebuffer_samples) {
    j->buf = storage;
    j->cap = capacity;
    j->prebuffer = prebuffer_samples < capacity ? prebuffer_samples : capacity;
    jitter_reset(j);
}

void jitter_reset(jitter_t *j) {
    j->head = 0;
    j->tail = 0;
    j->started = false;
    j->finished = false;
    j->underruns = 0;
    j->dry = false;
    j->have_odd = false;
}

size_t jitter_queued(const jitter_t *j) { return j->head - j->tail; }

size_t jitter_free(const jitter_t *j) { return j->cap - (j->head - j->tail); }

size_t jitter_write_bytes(jitter_t *j, const uint8_t *data, size_t len) {
    size_t used = 0;
    uint32_t h = j->head;
    uint32_t room = (uint32_t)jitter_free(j);
    if (j->have_odd && used < len && room > 0) {
        j->buf[h % j->cap] = (int16_t)(uint16_t)(j->odd | (data[used++] << 8));
        h++;
        room--;
        j->have_odd = false;
    }
    while (len - used >= 2 && room > 0) {
        j->buf[h % j->cap] = (int16_t)(uint16_t)(data[used] | (data[used + 1] << 8));
        used += 2;
        h++;
        room--;
    }
    if (len - used == 1 && !j->have_odd && room > 0) {
        j->odd = data[used++];
        j->have_odd = true;
    }
    j->head = h;
    if (!j->started && jitter_queued(j) >= j->prebuffer) j->started = true;
    return used;
}

void jitter_finish(jitter_t *j) { j->finished = true; }

size_t jitter_read(jitter_t *j, int16_t *out, size_t n) {
    if (!j->started) {
        if (!j->finished) return 0;
        j->started = true; /* short reply: play what there is */
    }
    uint32_t t = j->tail;
    size_t avail = j->head - t;
    size_t take = n < avail ? n : avail;
    for (size_t i = 0; i < take; i++) out[i] = j->buf[t++ % j->cap];
    j->tail = t;
    if (take < n && !j->finished) {
        if (!j->dry) {
            j->dry = true;
            j->underruns++;
        }
    } else if (take > 0) {
        j->dry = false;
    }
    return take;
}

bool jitter_drained(const jitter_t *j) { return j->finished && j->head == j->tail; }
