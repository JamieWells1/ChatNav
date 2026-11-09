#ifndef HTTP_RESP_H
#define HTTP_RESP_H

#include <stdbool.h>
#include <stddef.h>

#define HTTP_RESP_HEADER_MAX 768
#define HTTP_RESP_OUTCOME_MAX 16

typedef enum {
    HTTP_RESP_NEED_MORE, /* keep feeding */
    HTTP_RESP_HEADERS,   /* status and length known; body bytes may follow */
    HTTP_RESP_DONE,      /* the whole body has been seen */
    HTTP_RESP_ERROR,     /* malformed, header too large, or unsupported framing */
} http_resp_event_t;

typedef enum {
    CHUNK_SIZE,      /* reading hex digits */
    CHUNK_EXT,       /* skipping a chunk extension up to LF */
    CHUNK_DATA,
    CHUNK_DATA_CR,
    CHUNK_DATA_LF,
    CHUNK_TRAILER,   /* after the 0 chunk: skip trailer lines to the blank line */
    CHUNK_END,
} chunk_state_t;

/* Incremental parser for one HTTP/1.1 response, with a Content-Length body or (when
 * allow_chunked is set) a chunked body. Body bytes are not stored; the caller gets them
 * through the body pointer from feed. */
typedef struct {
    char head[HTTP_RESP_HEADER_MAX];
    size_t head_len;
    bool headers_done;
    bool allow_chunked;
    bool chunked;
    int status;
    long content_length; /* -1 when absent (treated as no body) */
    long body_seen;
    bool keep_alive;
    char outcome[HTTP_RESP_OUTCOME_MAX]; /* X-Outcome header value, empty when absent */
    chunk_state_t chunk_state;
    unsigned long chunk_left;
    int chunk_digits;
    int trailer_len;
} http_resp_t;

void http_resp_init(http_resp_t *r);

/* As http_resp_init but accepts Transfer-Encoding: chunked (streamed replies). */
void http_resp_init_stream(http_resp_t *r);

/* Feeds bytes. On return, *body and *body_len (if non-NULL) describe the body bytes in
 * this chunk. Bytes past Content-Length are ignored. Only valid for Content-Length
 * bodies in one call; use http_resp_feed_ex for chunked replies. */
http_resp_event_t http_resp_feed(http_resp_t *r, const char *data, size_t len,
                                 const char **body, size_t *body_len);

/* Consumes bytes up to the end of the next body span (or all of len). Call again with
 * the rest while *consumed < len. A call may return no span with bytes left over only
 * if DONE or ERROR was reached. */
http_resp_event_t http_resp_feed_ex(http_resp_t *r, const char *data, size_t len,
                                    size_t *consumed, const char **body, size_t *body_len);

#endif
