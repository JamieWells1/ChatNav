#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <stdbool.h>
#include <stdint.h>

#define FP_LEN 32
#define FP_TEXT_LEN 96 /* "AA:BB:..." (95 chars) plus NUL */

/* Parses 64 hex digits. Colons, spaces and dashes between digits are ignored, case is
 * free, and an openssl-style "SHA256 Fingerprint=" prefix is skipped. Returns false on
 * anything else (wrong digit count, stray characters); out is untouched on failure. */
bool fp_parse(const char *text, uint8_t out[FP_LEN]);

/* Writes upper-case colon-separated hex, the format the server script prints. */
void fp_format(const uint8_t fp[FP_LEN], char out[FP_TEXT_LEN]);

/* Compares in constant time. */
bool fp_equal(const uint8_t a[FP_LEN], const uint8_t b[FP_LEN]);

#endif
