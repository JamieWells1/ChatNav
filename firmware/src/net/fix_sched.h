#ifndef FIX_SCHED_H
#define FIX_SCHED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* When to start a fix batch (pure, host-tested). A batch starts when records are waiting,
 * the link is up and the single connection is free, and either FIX_UPLOAD_INTERVAL_S has
 * passed, a flush was asked for (button press), or a full batch is waiting. After a failure
 * nothing starts until FIX_UPLOAD_RETRY_S has passed, flush or not. */
typedef struct {
    bool flush;
    bool retry_wait;
    bool started_once;
    uint32_t last_ms; /* when the last batch was started */
} fix_sched_t;

void fix_sched_init(fix_sched_t *s);

/* Marks a flush as wanted; it stays wanted until a batch is acknowledged. */
void fix_sched_flush(fix_sched_t *s);

bool fix_sched_due(const fix_sched_t *s, uint32_t now_ms, size_t pending, bool link_up,
                   bool conn_free);

void fix_sched_started(fix_sched_t *s, uint32_t now_ms);

/* ok: the server acknowledged the batch. */
void fix_sched_result(fix_sched_t *s, bool ok);

#endif
