/*
 * Adapted from Photon src/server/http_server.c (accept, receive, send, close).
 */

#include "setup/portal_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/tcp.h"

#define REQ_MAX 3072
#define RESP_MAX 3072
#define PORTAL_PORT 80

typedef struct {
    char req[REQ_MAX];
    size_t received;
    char resp[RESP_MAX];
    size_t resp_len;
    size_t sent;
} conn_t;

static struct tcp_pcb *listen_pcb;
static portal_handler_t handler;

static void conn_close(struct tcp_pcb *pcb, conn_t *c) {
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_err(pcb, NULL);
    free(c);
    if (tcp_close(pcb) != ERR_OK) tcp_abort(pcb);
}

static void on_err(void *arg, err_t err) {
    (void)err;
    free(arg); /* pcb is already gone */
}

static err_t push(struct tcp_pcb *pcb, conn_t *c) {
    while (c->sent < c->resp_len) {
        size_t room = tcp_sndbuf(pcb);
        if (room == 0) break;
        size_t n = c->resp_len - c->sent;
        if (n > room) n = room;
        err_t e = tcp_write(pcb, c->resp + c->sent, (u16_t)n, TCP_WRITE_FLAG_COPY);
        if (e != ERR_OK) return e;
        c->sent += n;
    }
    tcp_output(pcb);
    return ERR_OK;
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)len;
    conn_t *c = arg;
    if (c->sent >= c->resp_len) {
        conn_close(pcb, c);
        return ERR_OK;
    }
    if (push(pcb, c) != ERR_OK) conn_close(pcb, c);
    return ERR_OK;
}

/* Returns true once headers and the full Content-Length body are in the buffer. */
static bool request_complete(const conn_t *c, size_t *body_off) {
    const char *end = strstr(c->req, "\r\n\r\n");
    if (!end) return false;
    *body_off = (size_t)(end - c->req) + 4;
    const char *cl = strstr(c->req, "Content-Length:");
    if (!cl) cl = strstr(c->req, "content-length:");
    size_t want = (cl && cl < end) ? (size_t)atoi(cl + 15) : 0;
    return c->received - *body_off >= want;
}

static void respond(struct tcp_pcb *pcb, conn_t *c, size_t body_off) {
    portal_request_t r = {.is_post = strncmp(c->req, "POST ", 5) == 0};
    char *path = strchr(c->req, ' ');
    if (!path) {
        conn_close(pcb, c);
        return;
    }
    path++;
    char *sp = strchr(path, ' ');
    if (sp) *sp = '\0';
    r.path = path;
    r.body = c->req + body_off;

    c->resp_len = handler(&r, c->resp, sizeof(c->resp));
    c->sent = 0;
    tcp_sent(pcb, on_sent);
    if (push(pcb, c) != ERR_OK) conn_close(pcb, c);
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    conn_t *c = arg;
    if (!p) { /* peer closed */
        conn_close(pcb, c);
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }
    size_t n = p->tot_len;
    if (c->received + n >= sizeof(c->req)) n = sizeof(c->req) - 1 - c->received;
    pbuf_copy_partial(p, c->req + c->received, (u16_t)n, 0);
    c->received += n;
    c->req[c->received] = '\0';
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    size_t body_off;
    if (request_complete(c, &body_off)) {
        tcp_recv(pcb, NULL);
        respond(pcb, c, body_off);
    } else if (c->received >= sizeof(c->req) - 1) {
        printf("[PORTAL] request too large, dropping\n");
        conn_close(pcb, c);
    }
    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !newpcb) return ERR_VAL;
    conn_t *c = calloc(1, sizeof(*c));
    if (!c) return ERR_MEM;
    tcp_arg(newpcb, c);
    tcp_recv(newpcb, on_recv);
    tcp_err(newpcb, on_err);
    return ERR_OK;
}

bool portal_http_start(portal_handler_t h) {
    if (listen_pcb) return true;
    handler = h;
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return false;
    if (tcp_bind(pcb, IP_ADDR_ANY, PORTAL_PORT) != ERR_OK) {
        tcp_close(pcb);
        printf("[PORTAL] cannot bind port %d\n", PORTAL_PORT);
        return false;
    }
    listen_pcb = tcp_listen(pcb);
    if (!listen_pcb) return false;
    tcp_accept(listen_pcb, on_accept);
    printf("[PORTAL] http server on port %d\n", PORTAL_PORT);
    return true;
}

void portal_http_stop(void) {
    if (listen_pcb) {
        tcp_close(listen_pcb);
        listen_pcb = NULL;
    }
}
