#ifndef CHUNKQ_H
#define CHUNKQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "net/http_req.h"

/* Outgoing byte stream held as encoded slots (one request head or one chunk each) until
 * the TLS send buffer has room. The queue lets recording carry on while the connection
 * is busy or being re-opened. A slot is never partly freed: the sender reports how many
 * bytes of the front slot it managed to write. */

#define CHUNKQ_SLOTS 16 /* ~1 s of audio at UPLOAD_CHUNK_MS 64 */
#define CHUNKQ_SLOT_BYTES 2112 /* chunk of HTTP_REQ_CHUNK_MAX + framing, or a request head */

typedef struct {
    uint8_t data[CHUNKQ_SLOTS][CHUNKQ_SLOT_BYTES];
    uint16_t len[CHUNKQ_SLOTS];
    uint8_t first;
    uint8_t count;
    uint16_t sent;      /* bytes of the front slot already written */
    uint32_t dropped;   /* chunks refused because the queue was full */
} chunkq_t;

void chunkq_reset(chunkq_t *q);

/* Each returns false (and counts a drop for audio chunks) when no slot is free. */
bool chunkq_push_raw(chunkq_t *q, const void *data, size_t len);
bool chunkq_push_chunk(chunkq_t *q, const uint8_t *pcm, size_t len);
bool chunkq_push_end(chunkq_t *q);

/* Unsent part of the front slot; returns its length (0 when empty). */
size_t chunkq_peek(const chunkq_t *q, const uint8_t **p);
void chunkq_consume(chunkq_t *q, size_t n);

size_t chunkq_count(const chunkq_t *q);

#endif
