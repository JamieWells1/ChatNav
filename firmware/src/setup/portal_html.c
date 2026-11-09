#include "setup/portal_html.h"

#include <stdio.h>

#define PAGE_HEAD                                                                        \
    "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width\">" \
    "<title>ChatNav Setup</title><style>"                                                \
    "body{font-family:sans-serif;margin:0;padding:16px;background:#f2f4f7}"             \
    ".c{max-width:420px;margin:auto;background:#fff;border-radius:10px;padding:20px}"    \
    "h1{font-size:22px;margin:0 0 12px}label{display:block;font-size:14px;margin:12px 0 4px}" \
    "input{width:100%;padding:10px;font-size:16px;box-sizing:border-box}"                \
    "button{width:100%;margin-top:18px;padding:12px;font-size:16px}"                     \
    ".e{color:#b00020;margin:8px 0}small{color:#667}</style></head><body><div class=\"c\">"

#define PAGE_TAIL "</div></body></html>"

static size_t respond(char *out, size_t cap, const char *status, const char *body) {
    int n = snprintf(out, cap,
                     "HTTP/1.1 %s\r\nContent-Type: text/html\r\nCache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n%s",
                     status, body);
    return n < 0 ? 0 : ((size_t)n < cap ? (size_t)n : cap - 1);
}

size_t html_form_page(char *out, size_t cap, const char *error_message) {
    char err[160] = "";
    if (error_message) snprintf(err, sizeof(err), "<p class=\"e\">%s</p>", error_message);

    char body[2400];
    snprintf(body, sizeof(body),
             "%s<h1>ChatNav setup</h1>%s"
             "<form method=\"POST\" action=\"/save\">"
             "<label>Wi-Fi name (phone hotspot)</label><input name=\"ssid\" maxlength=\"32\" required>"
             "<label>Wi-Fi password</label><input name=\"password\" type=\"password\" maxlength=\"63\">"
             "<label>Server host</label><input name=\"host\" maxlength=\"63\" required>"
             "<label>Server port</label><input name=\"port\" value=\"443\" inputmode=\"numeric\">"
             "<label>Device token</label><input name=\"token\" type=\"password\" maxlength=\"64\" required>"
             "<label>Server certificate fingerprint (SHA-256)</label>"
             "<input name=\"fingerprint\" maxlength=\"110\" required>"
             "<small>Printed by the server cert script as AA:BB:CC:...</small>"
             "<button type=\"submit\">Save and restart</button></form>%s",
             PAGE_HEAD, err, PAGE_TAIL);
    return respond(out, cap, "200 OK", body);
}

size_t html_saved_page(char *out, size_t cap) {
    return respond(out, cap, "200 OK",
                   PAGE_HEAD "<h1>Saved</h1><p>ChatNav is restarting and will join your Wi-Fi."
                   "</p>" PAGE_TAIL);
}

size_t html_redirect(char *out, size_t cap, const char *location) {
    int n = snprintf(out, cap,
                     "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n"
                     "Connection: close\r\n\r\n",
                     location);
    return n < 0 ? 0 : ((size_t)n < cap ? (size_t)n : cap - 1);
}
