#include "gps/fix_buffer.h"

#include <string.h>

void fix_buffer_init(fix_buffer_t *fb, uint8_t *store, size_t capacity) {
    memset(fb, 0, sizeof(*fb));
    fb->store = store;
    fb->capacity = capacity;
    fb->oldest_seq = 1;
    fb->next_seq = 1;
}

uint32_t fix_buffer_push(fix_buffer_t *fb, const fix_record_t *r) {
    size_t slot;
    if (fb->count == fb->capacity) {
        slot = fb->head;
        fb->head = (fb->head + 1) % fb->capacity;
        fb->oldest_seq++;
        fb->drops++;
    } else {
        slot = (fb->head + fb->count) % fb->capacity;
        fb->count++;
    }
    fix_record_pack(r, fb->store + slot * FIX_RECORD_WIRE_SIZE);
    return fb->next_seq++;
}

size_t fix_buffer_peek(const fix_buffer_t *fb, uint8_t *out, size_t max, uint32_t *first_seq) {
    size_t n = fb->count < max ? fb->count : max;
    for (size_t i = 0; i < n; i++) {
        size_t slot = (fb->head + i) % fb->capacity;
        memcpy(out + i * FIX_RECORD_WIRE_SIZE, fb->store + slot * FIX_RECORD_WIRE_SIZE,
               FIX_RECORD_WIRE_SIZE);
    }
    if (first_seq != NULL) *first_seq = fb->oldest_seq;
    return n;
}

bool fix_buffer_ack(fix_buffer_t *fb, uint32_t seq) {
    if (seq >= fb->next_seq) return false;
    if (seq < fb->oldest_seq) return true; /* already released or overwritten */
    size_t release = (size_t)(seq - fb->oldest_seq) + 1;
    fb->head = (fb->head + release) % fb->capacity;
    fb->count -= release;
    fb->oldest_seq += (uint32_t)release;
    return true;
}

size_t fix_buffer_count(const fix_buffer_t *fb) { return fb->count; }

uint32_t fix_buffer_drops(const fix_buffer_t *fb) { return fb->drops; }
