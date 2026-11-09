#include "net/http_stream.h"

#include <stdio.h>
#include <string.h>

#include "net/http_resp.h"
#include "net/tls_client.h"
#include "pico/time.h"

typedef struct {
    http_resp_t parser;
    http_reply_t *reply;
    bool error;
} rx_t;

static rx_t rx;

static void on_data(const uint8_t *data, size_t len, void *ctx) {
    rx_t *r = ctx;
    if (r->error || r->reply->done) return;
    const char *body;
    size_t n;
    http_resp_event_t ev = http_resp_feed(&r->parser, (const char *)data, len, &body, &n);
    if (ev == HTTP_RESP_ERROR) {
        r->error = true;
        return;
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
    int n;
    if (token && token[0]) {
        n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
                     "Connection: keep-alive\r\n\r\n",
                     path, host, token);
    } else {
        n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", path, host);
    }
    if (n <= 0 || (size_t)n >= sizeof(req)) return RC_HTTP_BUFFER_FULL;
    ReturnCode rc = tls_client_write(req, (size_t)n);
    if (RC_FAILED(rc)) return rc;

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!reply->done && !rx.error && tls_client_state() == TLS_READY) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) return RC_HTTP_TIMEOUT;
        sleep_ms(5);
    }
    if (rx.error) return RC_HTTP_ERROR;
    return reply->done ? RC_OK : RC_TLS_CLOSED;
}
