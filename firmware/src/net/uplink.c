#include "net/uplink.h"

#include <stdio.h>
#include <string.h>

#include "audio/playback.h"
#include "net/chunkq.h"
#include "net/fix_upload.h"
#include "net/link.h"
#include "net/tls_client.h"
#include "pico/time.h"

#define HEAD_MAX 512

static const device_config_t *cfg;
static chunkq_t queue;
static bool armed;
static bool finish_pending; /* terminal chunk not yet queued (queue was full) */
static bool finished;       /* terminal chunk queued */
static bool in_flight;      /* some request bytes were written and the reply is not done */
static bool failed;
static bool bound; /* the TLS receiver is bound to the reply parser */

void uplink_init(const device_config_t *c) { cfg = c; }

static void reset_request(void) {
    chunkq_reset(&queue);
    armed = false;
    finish_pending = false;
    finished = false;
    in_flight = false;
    failed = false;
    bound = false;
    playback_reset();
    http_query_expect(playback_buffer());
}

void uplink_begin_query(void) {
    reset_request();
    char head[HEAD_MAX];
    size_t n = http_query_post_head(head, sizeof(head), cfg->server_host, cfg->token);
    if (n == 0 || !chunkq_push_raw(&queue, head, n)) {
        failed = true; /* the request head sits first in the queue, unsent until armed */
        printf("[UPLINK] could not build the request head\n");
    }
    printf("[UPLINK] new query, link %s\n", tls_client_state() == TLS_READY ? "ready" : "not ready");
}

void uplink_feed_pcm(const int16_t *samples, size_t n) {
    if (n == 0) return;
    if (!chunkq_push_chunk(&queue, (const uint8_t *)samples, n * sizeof(samples[0]))) {
        printf("[UPLINK] queue full, audio chunk dropped (%u so far)\n", (unsigned)queue.dropped);
    }
}

void uplink_arm(void) {
    if (armed || failed) return;
    armed = true;
    printf("[UPLINK] past minimum utterance, upload enabled\n");
}

void uplink_finish(void) { finish_pending = true; }

void uplink_discard(void) {
    chunkq_reset(&queue);
    armed = false;
    finish_pending = false;
    finished = false;
    failed = false;
    bound = false;
    printf("[UPLINK] short press, nothing uploaded\n");
}

void uplink_begin_replay(void) {
    reset_request();
    char head[HEAD_MAX];
    size_t n = http_query_last_head(head, sizeof(head), cfg->server_host, cfg->token);
    if (n == 0 || !chunkq_push_raw(&queue, head, n)) {
        failed = true;
        return;
    }
    armed = true;
    finished = true; /* a GET has no body */
    printf("[UPLINK] requesting the last reply\n");
}

void uplink_abort(void) {
    const query_reply_t *r = http_query_reply();
    bool dirty = in_flight && !r->done;
    chunkq_reset(&queue);
    armed = false;
    finish_pending = false;
    finished = false;
    in_flight = false;
    failed = false;
    bound = false;
    if (dirty && tls_client_state() == TLS_READY) {
        printf("[UPLINK] request abandoned mid-flight, reopening the connection\n");
        link_force_reconnect();
    }
}

void uplink_poll(void) {
    tls_client_pump();
    if (finish_pending && chunkq_push_end(&queue)) {
        finish_pending = false;
        finished = true;
    }
    if (!armed || failed || tls_client_state() != TLS_READY) return;

    /* A fix batch owns the connection until its reply is in; the request waits queued. */
    if (!bound) {
        if (fix_upload_busy()) return;
        http_query_bind();
        bound = true;
    }

    const uint8_t *p;
    size_t len;
    while ((len = chunkq_peek(&queue, &p)) > 0) {
        size_t room = tls_client_writable();
        if (room == 0) break;
        size_t n = len < room ? len : room;
        ReturnCode rc = tls_client_write(p, n);
        if (rc == RC_ERR_BUSY) break;
        if (RC_FAILED(rc)) {
            printf("[UPLINK] write failed: %s\n", rc_to_string(rc));
            failed = true;
            return;
        }
        in_flight = true;
        chunkq_consume(&queue, n);
    }
}

uplink_status_t uplink_status(void) {
    const query_reply_t *r = http_query_reply();
    uplink_status_t s = {
        .upload_done = finished && chunkq_count(&queue) == 0,
        .failed = failed || r->error,
        .headers = r->headers,
        .status = r->status,
        .audio_started = r->audio_bytes > 0,
        .reply_done = r->done,
        .last_audio_ms = r->last_audio_ms,
        .dropped_chunks = queue.dropped,
    };
    return s;
}
