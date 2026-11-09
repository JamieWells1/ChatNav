#include "net/http_resp.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void http_resp_init(http_resp_t *r) {
    memset(r, 0, sizeof(*r));
    r->content_length = -1;
    r->keep_alive = true;
}

static bool starts_with_nocase(const char *s, const char *prefix) {
    for (; *prefix; s++, prefix++) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return false;
    }
    return true;
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
            return false; /* chunked replies are not used by the server for these calls */
        } else if (starts_with_nocase(h, "connection:")) {
            const char *v = h + 11;
            while (*v == ' ') v++;
            if (starts_with_nocase(v, "close")) r->keep_alive = false;
        }
    }
    return true;
}

http_resp_event_t http_resp_feed(http_resp_t *r, const char *data, size_t len,
                                 const char **body, size_t *body_len) {
    if (body) *body = NULL;
    if (body_len) *body_len = 0;

    size_t used = 0;
    bool just_finished_headers = false;
    if (!r->headers_done) {
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
        if (!r->headers_done) return HTTP_RESP_NEED_MORE;
    }

    size_t avail = len - used;
    long remaining = r->content_length < 0 ? 0 : r->content_length - r->body_seen;
    size_t take = avail < (size_t)remaining ? avail : (size_t)remaining;
    if (body) *body = data + used;
    if (body_len) *body_len = take;
    r->body_seen += (long)take;

    if (r->body_seen >= (r->content_length < 0 ? 0 : r->content_length)) return HTTP_RESP_DONE;
    return just_finished_headers ? HTTP_RESP_HEADERS : HTTP_RESP_NEED_MORE;
}
