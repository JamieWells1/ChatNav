#ifndef TLS_CLIENT_H
#define TLS_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "common/error.h"
#include "net/fingerprint.h"

/* One kept-open TLS connection to the ChatNav server. The server certificate is not
 * validated against a CA: its leaf certificate DER is hashed with SHA-256 and compared with
 * the pin stored in flash, before any request (and so the bearer token) is sent.
 *
 * Calls are safe from the main loop; callbacks run in the lwIP context. */

typedef enum {
    TLS_IDLE,
    TLS_RESOLVING,
    TLS_CONNECTING, /* TCP connect and handshake in progress */
    TLS_READY,      /* handshake done, pin matched */
    TLS_FAILED,     /* see tls_client_error */
} tls_state_t;

typedef void (*tls_recv_fn)(const uint8_t *data, size_t len, void *ctx);

ReturnCode tls_client_open(const char *host, uint16_t port, const uint8_t pin[FP_LEN],
                           tls_recv_fn on_recv, void *ctx);
tls_state_t tls_client_state(void);
ReturnCode tls_client_error(void);

/* Replaces the receive callback of the open connection. */
void tls_client_set_receiver(tls_recv_fn on_recv, void *ctx);

/* Queues plaintext for sending; returns RC_ERR_BUSY if the send buffer is full. */
ReturnCode tls_client_write(const void *data, size_t len);

void tls_client_close(void);

#endif
