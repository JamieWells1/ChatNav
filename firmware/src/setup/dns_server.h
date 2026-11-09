#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdbool.h>

/* Answers every A query with the portal address so phones show the captive portal.
 * Adapted from Photon src/server/dns_server.c. */
bool dns_server_start(void);
void dns_server_stop(void);

#endif
