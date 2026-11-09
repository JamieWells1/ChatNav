#ifndef PORTAL_HTTP_H
#define PORTAL_HTTP_H

#include <stdbool.h>
#include <stddef.h>

/* Minimal one-request-per-connection HTTP/1.0-style server for the setup portal.
 * Split out of Photon src/server/http_server.c: only accept, buffer, send and close. */

typedef struct {
    bool is_post;
    const char *path;
    const char *body; /* NUL-terminated, empty for GET */
} portal_request_t;

/* Fills out with a full HTTP response (status line, headers, body). Returns its length. */
typedef size_t (*portal_handler_t)(const portal_request_t *req, char *out, size_t cap);

bool portal_http_start(portal_handler_t handler);
void portal_http_stop(void);

#endif
