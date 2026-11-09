#ifndef HTTP_RESP_H
#define HTTP_RESP_H

#include <stdbool.h>
#include <stddef.h>

#define HTTP_RESP_HEADER_MAX 512

typedef enum {
    HTTP_RESP_NEED_MORE, /* keep feeding */
    HTTP_RESP_HEADERS,   /* status and length known; body bytes may follow */
    HTTP_RESP_DONE,      /* Content-Length bytes of body have been seen */
    HTTP_RESP_ERROR,     /* malformed, header too large, or unsupported framing */
} http_resp_event_t;

/* Incremental parser for one HTTP/1.1 response with a Content-Length body. Body bytes
 * are not stored; the caller gets them through the body pointer from feed. */
typedef struct {
    char head[HTTP_RESP_HEADER_MAX];
    size_t head_len;
    bool headers_done;
    int status;
    long content_length; /* -1 when absent (treated as no body) */
    long body_seen;
    bool keep_alive;
} http_resp_t;

void http_resp_init(http_resp_t *r);

/* Feeds bytes. On return, *body and *body_len (if non-NULL) describe the body bytes in
 * this chunk. Bytes past Content-Length are ignored. */
http_resp_event_t http_resp_feed(http_resp_t *r, const char *data, size_t len,
                                 const char **body, size_t *body_len);

#endif
