#ifndef FIX_BATCH_H
#define FIX_BATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gps/fix_record.h"

/* Body of POST /v1/fixes, byte for byte as server/app/gps/wire.py: a 10-byte little-endian
 * header (seq u32, firmware version u16, drop count u16, record count u16) followed by
 * count packed 28-byte fix records. */
#define FIX_BATCH_HEADER_SIZE 10

/* Writes the header. The drop count saturates at 65535. */
void fix_batch_header(uint8_t out[FIX_BATCH_HEADER_SIZE], uint32_t seq, uint16_t fw,
                      uint32_t drops, uint16_t count);

/* Header plus count records copied from records (already packed). Returns the length, or 0
 * if it does not fit in cap. */
size_t fix_batch_build(uint8_t *out, size_t cap, uint32_t seq, uint16_t fw, uint32_t drops,
                       const uint8_t *records, uint16_t count);

/* Reads N from a reply body such as {"ack": 42}. Returns false if there is no number. */
bool fix_ack_parse(const char *body, size_t len, uint32_t *ack);

#endif
