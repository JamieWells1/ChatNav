#include "net/chunkq.h"

#include <string.h>

void chunkq_reset(chunkq_t *q) {
    q->first = 0;
    q->count = 0;
    q->sent = 0;
    q->dropped = 0;
}

static uint8_t *next_slot(chunkq_t *q, uint8_t *idx) {
    if (q->count >= CHUNKQ_SLOTS) return NULL;
    *idx = (uint8_t)((q->first + q->count) % CHUNKQ_SLOTS);
    return q->data[*idx];
}

bool chunkq_push_raw(chunkq_t *q, const void *data, size_t len) {
    uint8_t i;
    uint8_t *slot = next_slot(q, &i);
    if (!slot || len == 0 || len > CHUNKQ_SLOT_BYTES) return false;
    memcpy(slot, data, len);
    q->len[i] = (uint16_t)len;
    q->count++;
    return true;
}

bool chunkq_push_chunk(chunkq_t *q, const uint8_t *pcm, size_t len) {
    uint8_t i;
    uint8_t *slot = next_slot(q, &i);
    size_t n = slot ? http_chunk_encode(pcm, len, slot, CHUNKQ_SLOT_BYTES) : 0;
    if (n == 0) {
        q->dropped++;
        return false;
    }
    q->len[i] = (uint16_t)n;
    q->count++;
    return true;
}

bool chunkq_push_end(chunkq_t *q) {
    uint8_t i;
    uint8_t *slot = next_slot(q, &i);
    size_t n = slot ? http_chunk_end(slot, CHUNKQ_SLOT_BYTES) : 0;
    if (n == 0) return false;
    q->len[i] = (uint16_t)n;
    q->count++;
    return true;
}

size_t chunkq_peek(const chunkq_t *q, const uint8_t **p) {
    if (q->count == 0) return 0;
    *p = q->data[q->first] + q->sent;
    return (size_t)q->len[q->first] - q->sent;
}

void chunkq_consume(chunkq_t *q, size_t n) {
    if (q->count == 0) return;
    q->sent = (uint16_t)(q->sent + n);
    if (q->sent >= q->len[q->first]) {
        q->first = (uint8_t)((q->first + 1) % CHUNKQ_SLOTS);
        q->count--;
        q->sent = 0;
    }
}

size_t chunkq_count(const chunkq_t *q) { return q->count; }
