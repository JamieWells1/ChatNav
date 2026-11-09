#ifndef HTTP_REQ_H
#define HTTP_REQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_REQ_CHUNK_MAX 2048 /* largest payload per chunk (one UPLOAD_CHUNK_MS block) */

typedef struct {
    const char *method;       /* "GET", "POST" */
    const char *path;
    const char *host;
    const char *token;        /* bearer token, NULL or empty for none */
    const char *content_type; /* NULL for no body headers */
    bool chunked_body;        /* adds Transfer-Encoding: chunked */
    size_t content_length;    /* adds Content-Length when > 0 and not chunked */
} http_req_t;

/* Writes the request line and headers, ending with the blank line. Returns the length,
 * or 0 if it does not fit. */
size_t http_req_build(char *out, size_t cap, const http_req_t *req);

/* Wraps len payload bytes as one HTTP chunk: hex size, CRLF, data, CRLF. Returns the
 * encoded length, or 0 if len is 0 or too big or the output does not fit. */
size_t http_chunk_encode(const uint8_t *data, size_t len, uint8_t *out, size_t cap);

/* The terminating zero-length chunk. */
size_t http_chunk_end(uint8_t *out, size_t cap);

#endif
