#include "net/http_req.h"

#include <stdio.h>
#include <string.h>

size_t http_req_build(char *out, size_t cap, const http_req_t *req) {
    if (!req || !req->method || !req->path || !req->host) return 0;
    size_t n = 0;
    int w = snprintf(out, cap, "%s %s HTTP/1.1\r\nHost: %s\r\n", req->method, req->path, req->host);
    if (w < 0 || (size_t)w >= cap) return 0;
    n = (size_t)w;
    if (req->token && req->token[0]) {
        w = snprintf(out + n, cap - n, "Authorization: Bearer %s\r\n", req->token);
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    if (req->content_type) {
        w = snprintf(out + n, cap - n, "Content-Type: %s\r\n", req->content_type);
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    if (req->chunked_body) {
        w = snprintf(out + n, cap - n, "Transfer-Encoding: chunked\r\n");
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    if (req->content_length > 0 && !req->chunked_body) {
        w = snprintf(out + n, cap - n, "Content-Length: %zu\r\n", req->content_length);
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    w = snprintf(out + n, cap - n, "Connection: keep-alive\r\n\r\n");
    if (w < 0 || (size_t)w >= cap - n) return 0;
    return n + (size_t)w;
}

size_t http_chunk_encode(const uint8_t *data, size_t len, uint8_t *out, size_t cap) {
    if (len == 0 || len > HTTP_REQ_CHUNK_MAX) return 0;
    char head[16];
    int h = snprintf(head, sizeof(head), "%zx\r\n", len);
    size_t total = (size_t)h + len + 2;
    if (total > cap) return 0;
    memcpy(out, head, (size_t)h);
    memcpy(out + h, data, len);
    memcpy(out + h + len, "\r\n", 2);
    return total;
}

size_t http_chunk_end(uint8_t *out, size_t cap) {
    static const char end[] = "0\r\n\r\n";
    if (cap < sizeof(end) - 1) return 0;
    memcpy(out, end, sizeof(end) - 1);
    return sizeof(end) - 1;
}
