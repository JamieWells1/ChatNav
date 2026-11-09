#include "net/fix_batch.h"

#include <string.h>

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
    put16(p, (uint16_t)v);
    put16(p + 2, (uint16_t)(v >> 16));
}

void fix_batch_header(uint8_t out[FIX_BATCH_HEADER_SIZE], uint32_t seq, uint16_t fw,
                      uint32_t drops, uint16_t count) {
    put32(out, seq);
    put16(out + 4, fw);
    put16(out + 6, drops > 0xFFFF ? 0xFFFF : (uint16_t)drops);
    put16(out + 8, count);
}

size_t fix_batch_build(uint8_t *out, size_t cap, uint32_t seq, uint16_t fw, uint32_t drops,
                       const uint8_t *records, uint16_t count) {
    size_t total = FIX_BATCH_HEADER_SIZE + (size_t)count * FIX_RECORD_WIRE_SIZE;
    if (total > cap) return 0;
    fix_batch_header(out, seq, fw, drops, count);
    if (count > 0) memcpy(out + FIX_BATCH_HEADER_SIZE, records, (size_t)count * FIX_RECORD_WIRE_SIZE);
    return total;
}

bool fix_ack_parse(const char *body, size_t len, uint32_t *ack) {
    static const char key[] = "\"ack\"";
    size_t klen = sizeof(key) - 1;
    for (size_t i = 0; i + klen <= len; i++) {
        if (memcmp(body + i, key, klen) != 0) continue;
        size_t j = i + klen;
        while (j < len && (body[j] == ' ' || body[j] == ':')) j++;
        if (j >= len || body[j] < '0' || body[j] > '9') return false;
        uint64_t v = 0;
        while (j < len && body[j] >= '0' && body[j] <= '9') {
            v = v * 10 + (uint64_t)(body[j] - '0');
            if (v > 0xFFFFFFFFu) return false;
            j++;
        }
        *ack = (uint32_t)v;
        return true;
    }
    return false;
}
