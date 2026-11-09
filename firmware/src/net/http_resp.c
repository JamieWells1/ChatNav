#include "net/http_resp.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void http_resp_init(http_resp_t *r) {
    memset(r, 0, sizeof(*r));
    r->content_length = -1;
    r->keep_alive = true;
}

void http_resp_init_stream(http_resp_t *r) {
    http_resp_init(r);
    r->allow_chunked = true;
}

static bool starts_with_nocase(const char *s, const char *prefix) {
    for (; *prefix; s++, prefix++) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return false;
    }
    return true;
}

static const char *skip_spaces(const char *v) {
    while (*v == ' ' || *v == '\t') v++;
    return v;
}

/* Parses the status line and the headers we care about from a complete header block. */
static bool parse_head(http_resp_t *r) {
    if (strncmp(r->head, "HTTP/1.", 7) != 0 || r->head[8] != ' ') return false;
    r->status = atoi(r->head + 9);
    if (r->status < 100 || r->status > 599) return false;

    for (char *line = strstr(r->head, "\r\n"); line; line = strstr(line + 2, "\r\n")) {
        const char *h = line + 2;
        if (starts_with_nocase(h, "content-length:")) {
            r->content_length = atol(h + 15);
            if (r->content_length < 0) return false;
        } else if (starts_with_nocase(h, "transfer-encoding:")) {
            if (!r->allow_chunked || !starts_with_nocase(skip_spaces(h + 18), "chunked")) {
                return false;
            }
            r->chunked = true;
        } else if (starts_with_nocase(h, "connection:")) {
            if (starts_with_nocase(skip_spaces(h + 11), "close")) r->keep_alive = false;
        } else if (starts_with_nocase(h, "x-outcome:")) {
            const char *v = skip_spaces(h + 10);
            size_t n = 0;
            while (v[n] && v[n] != '\r' && n < HTTP_RESP_OUTCOME_MAX - 1) n++;
            memcpy(r->outcome, v, n);
            r->outcome[n] = '\0';
        }
    }
    return true;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* One byte of chunked framing. Returns false on a framing error. */
static bool chunk_byte(http_resp_t *r, char c) {
    switch (r->chunk_state) {
        case CHUNK_SIZE: {
            int v = hex_value(c);
            if (v >= 0) {
                if (++r->chunk_digits > 7) return false;
                r->chunk_left = r->chunk_left * 16 + (unsigned long)v;
            } else if (c == '\r' || c == ';') {
                if (r->chunk_digits == 0) return false;
                r->chunk_state = CHUNK_EXT;
            } else {
                return false;
            }
            return true;
        }
        case CHUNK_EXT:
            if (c == '\n') {
                r->chunk_digits = 0;
                r->trailer_len = 0;
                r->chunk_state = r->chunk_left ? CHUNK_DATA : CHUNK_TRAILER;
            }
            return true;
        case CHUNK_DATA_CR:
            if (c != '\r') return false;
            r->chunk_state = CHUNK_DATA_LF;
            return true;
        case CHUNK_DATA_LF:
            if (c != '\n') return false;
            r->chunk_state = CHUNK_SIZE;
            return true;
        case CHUNK_TRAILER:
            if (c == '\n') {
                if (r->trailer_len == 0) r->chunk_state = CHUNK_END;
                r->trailer_len = 0;
            } else if (c != '\r') {
                r->trailer_len++;
            }
            return true;
        default:
            return true;
    }
}

http_resp_event_t http_resp_feed_ex(http_resp_t *r, const char *data, size_t len,
                                    size_t *consumed, const char **body, size_t *body_len) {
    if (body) *body = NULL;
    if (body_len) *body_len = 0;
    size_t used = 0;
    *consumed = 0;
    bool just_finished_headers = false;

    while (used < len && !r->headers_done) {
        if (r->head_len + 1 >= sizeof(r->head)) return HTTP_RESP_ERROR;
        r->head[r->head_len++] = data[used++];
        r->head[r->head_len] = '\0';
        if (r->head_len >= 4 && memcmp(r->head + r->head_len - 4, "\r\n\r\n", 4) == 0) {
            if (!parse_head(r)) return HTTP_RESP_ERROR;
            r->headers_done = true;
            just_finished_headers = true;
        }
    }
    if (!r->headers_done) {
        *consumed = used;
        return HTTP_RESP_NEED_MORE;
    }

    if (!r->chunked) {
        size_t avail = len - used;
        long total = r->content_length < 0 ? 0 : r->content_length;
        size_t take = avail < (size_t)(total - r->body_seen) ? avail : (size_t)(total - r->body_seen);
        if (body) *body = data + used;
        if (body_len) *body_len = take;
        r->body_seen += (long)take;
        *consumed = len; /* bytes past Content-Length are ignored */
        if (r->body_seen >= total) return HTTP_RESP_DONE;
        return just_finished_headers ? HTTP_RESP_HEADERS : HTTP_RESP_NEED_MORE;
    }

    while (used < len) {
        if (r->chunk_state == CHUNK_END) {
            *consumed = len;
            return HTTP_RESP_DONE;
        }
        if (r->chunk_state == CHUNK_DATA) {
            size_t avail = len - used;
            size_t take = avail < r->chunk_left ? avail : r->chunk_left;
            if (body) *body = data + used;
            if (body_len) *body_len = take;
            r->chunk_left -= take;
            r->body_seen += (long)take;
            if (r->chunk_left == 0) r->chunk_state = CHUNK_DATA_CR;
            *consumed = used + take;
            return HTTP_RESP_NEED_MORE;
        }
        if (!chunk_byte(r, data[used++])) return HTTP_RESP_ERROR;
    }
    *consumed = used;
    if (r->chunk_state == CHUNK_END) return HTTP_RESP_DONE;
    return just_finished_headers ? HTTP_RESP_HEADERS : HTTP_RESP_NEED_MORE;
}

http_resp_event_t http_resp_feed(http_resp_t *r, const char *data, size_t len,
                                 const char **body, size_t *body_len) {
    size_t consumed;
    return http_resp_feed_ex(r, data, len, &consumed, body, body_len);
}
