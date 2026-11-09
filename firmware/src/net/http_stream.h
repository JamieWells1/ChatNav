#ifndef HTTP_STREAM_H
#define HTTP_STREAM_H

#include <stdbool.h>
#include <stddef.h>

#include "common/error.h"

/* HTTP/1.1 requests over the kept-open TLS link. Adapted from Photon src/middleware/http.c
 * (request building and response parsing); the socket is now tls_client. M2 only needs
 * GET; chunked POST and streamed bodies arrive with /v1/query in M3. */

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

#endif
