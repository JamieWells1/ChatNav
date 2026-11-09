#ifndef HTTP_STREAM_H
#define HTTP_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio/jitter.h"
#include "common/error.h"
#include "net/http_resp.h"

/* HTTP/1.1 over the kept-open TLS link. Adapted from Photon src/middleware/http.c
 * (request building and response parsing); the socket is now tls_client. */

typedef struct {
    int status;        /* 0 until headers arrive */
    bool done;         /* full body received */
    char body[128];    /* first bytes of the body, NUL-terminated */
    size_t body_len;
} http_reply_t;

/* Sends GET path with the bearer token (NULL for none) and waits up to timeout_ms for the
 * complete reply. Polls from the main loop, so call it only outside time-critical states. */
ReturnCode http_stream_get(const char *host, const char *path, const char *token,
                           unsigned timeout_ms, http_reply_t *reply);

/* Streamed reply of POST /v1/query or GET /v1/query/last. */
typedef struct {
    volatile bool headers;     /* status line and headers parsed */
    volatile int status;
    char outcome[HTTP_RESP_OUTCOME_MAX]; /* X-Outcome: ok, no_speech, error, or empty */
    volatile uint32_t audio_bytes;
    volatile uint32_t last_audio_ms; /* when the latest body bytes arrived */
    volatile bool done;        /* reply complete */
    volatile bool error;       /* malformed reply */
    bool keep_alive;
} query_reply_t;

/* Request heads for the two query endpoints. Return the length, 0 if it does not fit. */
size_t http_query_post_head(char *out, size_t cap, const char *host, const char *token);
size_t http_query_last_head(char *out, size_t cap, const char *host, const char *token);

/* Binds the TLS receiver to the reply parser; 200 bodies go into sink (back-pressured by
 * its free space), other bodies are discarded. Call before the request is sent. */
void http_query_expect(jitter_t *sink);

const query_reply_t *http_query_reply(void);

#endif
