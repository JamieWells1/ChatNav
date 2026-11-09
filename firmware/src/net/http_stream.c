#include "net/http_stream.h"

#include <stdio.h>
#include <string.h>

#include "net/http_req.h"
#include "net/tls_client.h"
#include "pico/time.h"

#define AUDIO_CONTENT_TYPE "audio/L16;rate=16000"

typedef struct {
    http_resp_t parser;
    http_reply_t *reply;
    bool error;
} rx_t;

static rx_t rx;

static size_t on_data(const uint8_t *data, size_t len, void *ctx) {
    rx_t *r = ctx;
    if (r->error || r->reply->done) return len;
    const char *body;
    size_t n;
    http_resp_event_t ev = http_resp_feed(&r->parser, (const char *)data, len, &body, &n);
    if (ev == HTTP_RESP_ERROR) {
        r->error = true;
        return len;
    }
    r->reply->status = r->parser.status;
    size_t room = sizeof(r->reply->body) - 1 - r->reply->body_len;
    if (n > room) n = room;
    if (n) {
        memcpy(r->reply->body + r->reply->body_len, body, n);
        r->reply->body_len += n;
        r->reply->body[r->reply->body_len] = '\0';
    }
    if (ev == HTTP_RESP_DONE) r->reply->done = true;
    return len;
}

/* Rebinds the receive callback of an already open link to our parser. */
static void begin_reply(http_reply_t *reply) {
    memset(reply, 0, sizeof(*reply));
    http_resp_init(&rx.parser);
    rx.reply = reply;
    rx.error = false;
}

ReturnCode http_stream_get(const char *host, const char *path, const char *token,
                           unsigned timeout_ms, http_reply_t *reply) {
    if (tls_client_state() != TLS_READY) return RC_TLS_CLOSED;
    begin_reply(reply);
    tls_client_set_receiver(on_data, &rx);

    char req[320];
    http_req_t spec = {.method = "GET", .path = path, .host = host, .token = token};
    size_t n = http_req_build(req, sizeof(req), &spec);
    if (n == 0) return RC_HTTP_BUFFER_FULL;
    ReturnCode rc = tls_client_write(req, n);
    if (RC_FAILED(rc)) return rc;

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!reply->done && !rx.error && tls_client_state() == TLS_READY) {
        tls_client_pump();
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) return RC_HTTP_TIMEOUT;
        sleep_ms(5);
    }
    if (rx.error) return RC_HTTP_ERROR;
    return reply->done ? RC_OK : RC_TLS_CLOSED;
}

/* ---- streamed query replies ---- */

static struct {
    http_resp_t parser;
    query_reply_t reply;
    jitter_t *sink;
} q;

static void headers_ready(void) {
    q.reply.headers = true;
    q.reply.status = q.parser.status;
    memcpy(q.reply.outcome, q.parser.outcome, sizeof(q.reply.outcome));
    q.reply.keep_alive = q.parser.keep_alive;
    printf("[HTTP] reply status %d, outcome '%s'%s\n", q.parser.status, q.parser.outcome,
           q.parser.chunked ? ", chunked" : "");
}

static size_t on_query_data(const uint8_t *data, size_t len, void *ctx) {
    (void)ctx;
    size_t used = 0;
    while (used < len && !q.reply.done && !q.reply.error) {
        size_t step = 1; /* headers: one byte at a time so the body is never over-read */
        bool audio = q.parser.headers_done && q.parser.status == 200;
        if (q.parser.headers_done) {
            step = len - used;
            if (audio) {
                size_t room = jitter_free(q.sink) * 2;
                if (room == 0) break; /* back-pressure: the rest is held and re-offered */
                if (step > room) step = room;
            }
        }
        size_t consumed;
        const char *body;
        size_t n;
        http_resp_event_t ev = http_resp_feed_ex(&q.parser, (const char *)data + used, step,
                                                 &consumed, &body, &n);
        used += consumed;
        if (ev == HTTP_RESP_ERROR) {
            printf("[HTTP] malformed reply\n");
            q.reply.error = true;
            break;
        }
        if (q.parser.headers_done && !q.reply.headers) headers_ready();
        if (n > 0 && q.parser.status == 200) {
            jitter_write_bytes(q.sink, (const uint8_t *)body, n);
            q.reply.audio_bytes += (uint32_t)n;
            q.reply.last_audio_ms = to_ms_since_boot(get_absolute_time());
        }
        if (ev == HTTP_RESP_DONE) {
            q.reply.done = true;
            jitter_finish(q.sink);
            printf("[HTTP] reply complete, %u audio bytes\n", (unsigned)q.reply.audio_bytes);
        }
    }
    if (q.reply.done || q.reply.error) return len; /* swallow anything after the reply */
    return used;
}

void http_query_expect(jitter_t *sink) {
    tls_client_set_receiver(NULL, NULL);
    memset(&q.reply, 0, sizeof(q.reply));
    http_resp_init_stream(&q.parser);
    q.sink = sink;
    q.reply.last_audio_ms = to_ms_since_boot(get_absolute_time());
    tls_client_set_receiver(on_query_data, NULL);
}

const query_reply_t *http_query_reply(void) { return &q.reply; }

size_t http_query_post_head(char *out, size_t cap, const char *host, const char *token) {
    http_req_t spec = {.method = "POST", .path = "/v1/query", .host = host, .token = token,
                       .content_type = AUDIO_CONTENT_TYPE, .chunked_body = true};
    return http_req_build(out, cap, &spec);
}

size_t http_query_last_head(char *out, size_t cap, const char *host, const char *token) {
    http_req_t spec = {.method = "GET", .path = "/v1/query/last", .host = host, .token = token};
    return http_req_build(out, cap, &spec);
}
