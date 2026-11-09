/*
 * Adapted from Photon src/server/dns_server.c
 */

#include "setup/dns_server.h"

#include <stdio.h>
#include <string.h>

#include "lwip/udp.h"

#define DNS_HEADER_LEN 12
#define DNS_MAX_QUESTION 400
#define PORTAL_IP_A 192
#define PORTAL_IP_B 168
#define PORTAL_IP_C 4
#define PORTAL_IP_D 1

static struct udp_pcb *dns_pcb;

/* Answer record: pointer to the question name, A, IN, TTL 60, 4 bytes of address. */
static const uint8_t answer_tail[] = {0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                                      0x3C, 0x00, 0x04, PORTAL_IP_A, PORTAL_IP_B, PORTAL_IP_C,
                                      PORTAL_IP_D};

static void dns_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr,
                     u16_t port) {
    (void)arg;
    if (p->len < DNS_HEADER_LEN) {
        pbuf_free(p);
        return;
    }
    const uint8_t *q = p->payload;
    uint16_t flags = (uint16_t)((q[2] << 8) | q[3]);
    uint16_t questions = (uint16_t)((q[4] << 8) | q[5]);
    if ((flags & 0x8000) || questions == 0) { /* a response, or nothing asked */
        pbuf_free(p);
        return;
    }

    uint8_t resp[DNS_HEADER_LEN + DNS_MAX_QUESTION + sizeof(answer_tail)];
    size_t qlen = p->len - DNS_HEADER_LEN;
    if (qlen > DNS_MAX_QUESTION) qlen = DNS_MAX_QUESTION;

    memset(resp, 0, DNS_HEADER_LEN);
    resp[0] = q[0];
    resp[1] = q[1];
    resp[2] = 0x81; /* response, recursion desired */
    resp[3] = 0x80; /* recursion available */
    resp[4] = q[4];
    resp[5] = q[5];
    resp[7] = 0x01; /* one answer */
    memcpy(resp + DNS_HEADER_LEN, q + DNS_HEADER_LEN, qlen);
    memcpy(resp + DNS_HEADER_LEN + qlen, answer_tail, sizeof(answer_tail));
    size_t total = DNS_HEADER_LEN + qlen + sizeof(answer_tail);

    struct pbuf *out = pbuf_alloc(PBUF_TRANSPORT, (u16_t)total, PBUF_RAM);
    if (out) {
        memcpy(out->payload, resp, total);
        udp_sendto(pcb, out, addr, port);
        pbuf_free(out);
    }
    pbuf_free(p);
}

bool dns_server_start(void) {
    if (dns_pcb) return true;
    dns_pcb = udp_new();
    if (!dns_pcb) {
        printf("[DNS] cannot allocate pcb\n");
        return false;
    }
    if (udp_bind(dns_pcb, IP_ADDR_ANY, 53) != ERR_OK) {
        printf("[DNS] cannot bind port 53\n");
        udp_remove(dns_pcb);
        dns_pcb = NULL;
        return false;
    }
    udp_recv(dns_pcb, dns_recv, NULL);
    printf("[DNS] answering all names with %d.%d.%d.%d\n", PORTAL_IP_A, PORTAL_IP_B, PORTAL_IP_C,
           PORTAL_IP_D);
    return true;
}

void dns_server_stop(void) {
    if (dns_pcb) {
        udp_remove(dns_pcb);
        dns_pcb = NULL;
    }
}
