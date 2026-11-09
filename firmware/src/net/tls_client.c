#include "net/tls_client.h"

#include <stdio.h>
#include <string.h>

#include "altcp_tls_mbedtls_structs.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ssl.h"
#include "pico/cyw43_arch.h"

static struct altcp_tls_config *tls_config;
static struct altcp_pcb *pcb;
static tls_state_t state = TLS_IDLE;
static ReturnCode last_error = RC_OK;
static uint8_t pin[FP_LEN];
static uint16_t server_port;
static tls_recv_fn recv_fn;
static void *recv_ctx;
static char server_host[64];

/* Received data the receiver has not fully taken yet (back-pressure). */
static struct pbuf *held;
static size_t held_off;
static size_t held_total;
static bool close_pending;

static void fail(ReturnCode rc) {
    state = TLS_FAILED;
    last_error = rc;
}

static void drop_pcb(void) {
    if (!pcb) return;
    altcp_arg(pcb, NULL);
    altcp_recv(pcb, NULL);
    altcp_err(pcb, NULL);
    if (altcp_close(pcb) != ERR_OK) altcp_abort(pcb);
    pcb = NULL;
}

static void free_held(void) {
    if (held) pbuf_free(held);
    held = NULL;
    held_off = 0;
    held_total = 0;
    close_pending = false;
}

/* Offers the held data to the receiver; frees and acknowledges it once all is taken. */
static void deliver_held(void) {
    size_t skip = held_off;
    for (struct pbuf *q = held; q; q = q->next) {
        if (skip >= q->len) {
            skip -= q->len;
            continue;
        }
        size_t avail = q->len - skip;
        size_t used = recv_fn ? recv_fn((const uint8_t *)q->payload + skip, avail, recv_ctx) : avail;
        held_off += used;
        if (used < avail) return;
        skip = 0;
    }
    if (pcb) altcp_recved(pcb, (u16_t)held_total);
    pbuf_free(held);
    held = NULL;
    held_off = 0;
    held_total = 0;
}

static err_t on_recv(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p) { /* closed by the peer */
        printf("[TLS] server closed the connection\n");
        pcb = NULL;
        altcp_close(conn);
        if (held) {
            close_pending = true; /* report once the held data has been read */
        } else {
            fail(RC_TLS_CLOSED);
        }
        return ERR_OK;
    }
    if (err != ERR_OK || !recv_fn) {
        altcp_recved(conn, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }
    if (held) {
        held_total += p->tot_len;
        pbuf_cat(held, p);
    } else {
        held = p;
        held_total = p->tot_len;
        held_off = 0;
        deliver_held();
    }
    return ERR_OK;
}

void tls_client_pump(void) {
    if (!held && !close_pending) return;
    cyw43_arch_lwip_begin();
    if (held) deliver_held();
    if (!held && close_pending) {
        close_pending = false;
        fail(RC_TLS_CLOSED);
    }
    cyw43_arch_lwip_end();
}

size_t tls_client_writable(void) {
    if (state != TLS_READY || !pcb) return 0;
    cyw43_arch_lwip_begin();
    size_t n = altcp_sndbuf(pcb);
    cyw43_arch_lwip_end();
    return n;
}

static void on_err(void *arg, err_t err) {
    (void)arg;
    printf("[TLS] connection error %d\n", err);
    pcb = NULL; /* lwIP has already freed it */
    fail(state == TLS_READY ? RC_TLS_CLOSED : RC_TLS_CONNECT_FAILED);
}

/* Runs once the handshake has completed: pins the leaf certificate. */
static err_t on_connected(void *arg, struct altcp_pcb *conn, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        fail(RC_TLS_CONNECT_FAILED);
        return err;
    }
    mbedtls_ssl_context *ssl = altcp_tls_context(conn);
    const mbedtls_x509_crt *leaf = ssl ? mbedtls_ssl_get_peer_cert(ssl) : NULL;
    if (!leaf) {
        printf("[TLS] no peer certificate\n");
        fail(RC_TLS_NO_CERT);
        pcb = NULL;
        altcp_abort(conn);
        return ERR_ABRT;
    }

    uint8_t digest[FP_LEN];
    if (mbedtls_sha256(leaf->raw.p, leaf->raw.len, digest, 0) != 0 || !fp_equal(digest, pin)) {
        char text[FP_TEXT_LEN];
        fp_format(digest, text);
        printf("[TLS] PIN MISMATCH, refusing server (cert sha256 %s)\n", text);
        fail(RC_TLS_PIN_MISMATCH);
        pcb = NULL;
        altcp_abort(conn);
        return ERR_ABRT;
    }
    printf("[TLS] handshake ok, certificate pin matched\n");
    state = TLS_READY;
    return ERR_OK;
}

static void start_connect(const ip_addr_t *addr) {
    if (!tls_config) tls_config = altcp_tls_create_config_client(NULL, 0);
    if (!tls_config) {
        fail(RC_TLS_CONFIG_FAILED);
        return;
    }
    pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
    if (!pcb) {
        fail(RC_ERR_OUT_OF_MEMORY);
        return;
    }
    mbedtls_ssl_context *ssl = altcp_tls_context(pcb);
    ip_addr_t probe;
    if (ssl && !ipaddr_aton(server_host, &probe)) mbedtls_ssl_set_hostname(ssl, server_host);

    altcp_recv(pcb, on_recv);
    altcp_err(pcb, on_err);
    state = TLS_CONNECTING;
    err_t e = altcp_connect(pcb, addr, server_port, on_connected);
    if (e != ERR_OK) {
        printf("[TLS] connect start failed (%d)\n", e);
        altcp_abort(pcb);
        pcb = NULL;
        fail(RC_TLS_CONNECT_FAILED);
        return;
    }
    printf("[TLS] connecting to %s:%u\n", server_host, server_port);
}

static void on_dns(const char *name, const ip_addr_t *addr, void *arg) {
    (void)name;
    (void)arg;
    if (!addr) {
        printf("[TLS] DNS lookup failed for %s\n", server_host);
        fail(RC_TLS_DNS_FAILED);
        return;
    }
    start_connect(addr);
}

ReturnCode tls_client_open(const char *host, uint16_t port, const uint8_t fp[FP_LEN],
                           tls_recv_fn cb, void *ctx) {
    if (!host || strlen(host) >= sizeof(server_host)) return RC_ERR_INVALID_PARAM;
    cyw43_arch_lwip_begin();
    drop_pcb();
    free_held();
    strcpy(server_host, host);
    server_port = port;
    memcpy(pin, fp, FP_LEN);
    recv_fn = cb;
    recv_ctx = ctx;
    last_error = RC_OK;
    state = TLS_RESOLVING;

    ip_addr_t addr;
    err_t e = dns_gethostbyname(host, &addr, on_dns, NULL);
    if (e == ERR_OK) {
        start_connect(&addr);
    } else if (e != ERR_INPROGRESS) {
        fail(RC_TLS_DNS_FAILED);
    }
    cyw43_arch_lwip_end();
    return last_error;
}

tls_state_t tls_client_state(void) { return state; }
ReturnCode tls_client_error(void) { return last_error; }

void tls_client_set_receiver(tls_recv_fn cb, void *ctx) {
    cyw43_arch_lwip_begin();
    recv_fn = cb;
    recv_ctx = ctx;
    cyw43_arch_lwip_end();
}

ReturnCode tls_client_write(const void *data, size_t len) {
    if (state != TLS_READY || !pcb) return RC_TLS_CLOSED;
    ReturnCode rc = RC_OK;
    cyw43_arch_lwip_begin();
    if (altcp_sndbuf(pcb) < len) {
        rc = RC_ERR_BUSY;
    } else if (altcp_write(pcb, data, (u16_t)len, TCP_WRITE_FLAG_COPY) != ERR_OK ||
               altcp_output(pcb) != ERR_OK) {
        rc = RC_TLS_WRITE_FAILED;
    }
    cyw43_arch_lwip_end();
    return rc;
}

void tls_client_close(void) {
    cyw43_arch_lwip_begin();
    drop_pcb();
    free_held();
    state = TLS_IDLE;
    cyw43_arch_lwip_end();
}
