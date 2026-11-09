#ifndef FIX_BUFFER_H
#define FIX_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gps/fix_record.h"

/* Ring of packed fix records. Each pushed record gets the next sequence number
 * (starting at 1). When full, the oldest record is overwritten and counted. */
typedef struct {
    uint8_t *store; /* capacity * FIX_RECORD_WIRE_SIZE bytes */
    size_t capacity;
    size_t head;  /* index of the oldest record */
    size_t count; /* records held */
    uint32_t oldest_seq;
    uint32_t next_seq;
    uint32_t drops;
} fix_buffer_t;

void fix_buffer_init(fix_buffer_t *fb, uint8_t *store, size_t capacity);

/* Returns the sequence number assigned to the record. */
uint32_t fix_buffer_push(fix_buffer_t *fb, const fix_record_t *r);

/* Copies up to max oldest unacknowledged records (packed) into out. first_seq is the
 * sequence of the first one copied. Returns how many were copied. Records stay held. */
size_t fix_buffer_peek(const fix_buffer_t *fb, uint8_t *out, size_t max, uint32_t *first_seq);

/* Releases every record with sequence <= seq. Returns false if seq was never issued. */
bool fix_buffer_ack(fix_buffer_t *fb, uint32_t seq);

size_t fix_buffer_count(const fix_buffer_t *fb);
uint32_t fix_buffer_drops(const fix_buffer_t *fb);

#endif
