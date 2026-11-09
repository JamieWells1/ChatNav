#ifndef PORTAL_FORM_H
#define PORTAL_FORM_H

#include <stddef.h>

#include "storage/device_config.h"

#define FORM_MISSING (-1)
#define FORM_BAD (-2)

/* Decodes the value of key from an application/x-www-form-urlencoded body into out
 * (NUL-terminated, '+' becomes space, %XX decoded). Returns the decoded length, FORM_MISSING if
 * the key is absent, or FORM_BAD for a bad escape or a value that does not fit. */
int form_get(const char *body, const char *key, char *out, size_t cap);

/* Builds a config from the setup form fields ssid, password, host, port, token and
 * fingerprint, then validates it. Keeps the volume from base when given. */
cfg_result_t portal_form_to_config(const char *body, const device_config_t *base,
                                   device_config_t *out);

#endif
