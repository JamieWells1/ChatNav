#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

#include <stddef.h>

/* Each builder writes a complete HTTP response into out and returns its length. */
size_t html_form_page(char *out, size_t cap, const char *error_message);
size_t html_saved_page(char *out, size_t cap);
size_t html_redirect(char *out, size_t cap, const char *location);

#endif
