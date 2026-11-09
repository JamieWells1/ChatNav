#include "net/fix_sched.h"

#include <string.h>

#include "config.h"

void fix_sched_init(fix_sched_t *s) { memset(s, 0, sizeof(*s)); }

void fix_sched_flush(fix_sched_t *s) { s->flush = true; }

bool fix_sched_due(const fix_sched_t *s, uint32_t now_ms, size_t pending, bool link_up,
                   bool conn_free) {
    if (!link_up || !conn_free || pending == 0) return false;
    uint32_t elapsed = now_ms - s->last_ms;
    if (s->retry_wait) return elapsed >= FIX_UPLOAD_RETRY_S * 1000u;
    if (s->flush || pending >= FIX_BATCH_MAX || !s->started_once) return true;
    return elapsed >= FIX_UPLOAD_INTERVAL_S * 1000u;
}

void fix_sched_started(fix_sched_t *s, uint32_t now_ms) {
    s->started_once = true;
    s->last_ms = now_ms;
}

void fix_sched_result(fix_sched_t *s, bool ok) {
    s->retry_wait = !ok;
    if (ok) s->flush = false;
}
