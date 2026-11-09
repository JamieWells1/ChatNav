#include "net/fix_upload.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "gps/gps.h"
#include "net/fix_batch.h"
#include "net/fix_sched.h"
#include "net/http_req.h"
#include "net/http_resp.h"
#include "net/link.h"
#include "net/tls_client.h"
#include "pico/time.h"

#define HEAD_RESERVE 256 /* room in front of the body for the request head */
#define BODY_MAX (FIX_BATCH_HEADER_SIZE + FIX_BATCH_MAX * FIX_RECORD_WIRE_SIZE)
#define REPLY_BODY_MAX 31

typedef enum { F_IDLE, F_SENDING, F_WAITING } fstate_t;

static const device_config_t *cfg;
static fix_sched_t sched;
static fstate_t fstate;
static bool last_link_up, last_conn_free;

static uint8_t stage[HEAD_RESERVE + BODY_MAX]; /* head right-aligned against the body */
static size_t stage_off, stage_len, stage_sent;
static uint32_t sent_seq;
static uint16_t sent_count;
static uint32_t started_ms;

static struct {
    http_resp_t parser;
    char body[REPLY_BODY_MAX + 1];
    size_t body_len;
    volatile bool done;
    volatile bool error;
} rx;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

void fix_upload_init(const device_config_t *c) {
    cfg = c;
    fix_sched_init(&sched);
    fstate = F_IDLE;
}

bool fix_upload_busy(void) { return fstate != F_IDLE; }

/* Runs in the lwIP context: collects the reply. Always takes every byte. */
static size_t on_data(const uint8_t *data, size_t len, void *ctx) {
    (void)ctx;
    size_t used = 0;
    while (used < len && !rx.done && !rx.error) {
        size_t consumed;
        const char *body;
        size_t n;
        http_resp_event_t ev =
            http_resp_feed_ex(&rx.parser, (const char *)data + used, len - used, &consumed, &body, &n);
        used += consumed;
        if (n > 0 && rx.body_len < REPLY_BODY_MAX) {
            size_t room = REPLY_BODY_MAX - rx.body_len;
            if (n > room) n = room;
            memcpy(rx.body + rx.body_len, body, n);
            rx.body_len += n;
        }
        if (ev == HTTP_RESP_ERROR) rx.error = true;
        if (ev == HTTP_RESP_DONE) rx.done = true;
        if (consumed == 0 && ev == HTTP_RESP_NEED_MORE) break;
    }
    return len;
}

/* desync: the reply stream can no longer be trusted, so the connection is reopened. */
static void finish(bool ok, bool desync, const char *why) {
    tls_client_set_receiver(NULL, NULL);
    fstate = F_IDLE;
    fix_sched_result(&sched, ok);
    if (!ok) printf("[FIX] batch seq %u failed: %s, %u records kept\n", (unsigned)sent_seq, why,
                    (unsigned)fix_buffer_count(gps_buffer()));
    if (desync) link_force_reconnect();
}

static void start(uint32_t now) {
    fix_buffer_t *fb = gps_buffer();
    uint8_t *body = stage + HEAD_RESERVE;
    uint32_t first;
    size_t n = fix_buffer_peek(fb, body + FIX_BATCH_HEADER_SIZE, FIX_BATCH_MAX, &first);
    if (n == 0) return;
    sent_count = (uint16_t)n;
    sent_seq = first + (uint32_t)n - 1; /* the server acks the last record's sequence */
    fix_batch_header(body, sent_seq, FIRMWARE_VERSION, fix_buffer_drops(fb), sent_count);
    size_t body_len = FIX_BATCH_HEADER_SIZE + n * FIX_RECORD_WIRE_SIZE;

    char head[HEAD_RESERVE];
    http_req_t spec = {.method = "POST", .path = "/v1/fixes", .host = cfg->server_host,
                       .token = cfg->token, .content_type = "application/octet-stream",
                       .content_length = body_len};
    size_t hl = http_req_build(head, sizeof(head), &spec);
    if (hl == 0) {
        printf("[FIX] request head does not fit\n");
        fix_sched_started(&sched, now);
        fix_sched_result(&sched, false);
        return;
    }
    memcpy(stage + HEAD_RESERVE - hl, head, hl);
    stage_off = HEAD_RESERVE - hl;
    stage_len = hl + body_len;
    stage_sent = 0;

    http_resp_init_stream(&rx.parser);
    rx.body_len = 0;
    rx.body[0] = '\0';
    rx.done = false;
    rx.error = false;
    tls_client_set_receiver(on_data, NULL);
    fix_sched_started(&sched, now);
    started_ms = now;
    fstate = F_SENDING;
}

static void pump_write(void) {
    while (stage_sent < stage_len) {
        size_t room = tls_client_writable();
        if (room == 0) return;
        size_t n = stage_len - stage_sent;
        if (n > room) n = room;
        ReturnCode rc = tls_client_write(stage + stage_off + stage_sent, n);
        if (rc == RC_ERR_BUSY) return;
        if (RC_FAILED(rc)) {
            finish(false, true, rc_to_string(rc));
            return;
        }
        stage_sent += n;
    }
    fstate = F_WAITING;
}

static void check_reply(void) {
    uint32_t ack = 0;
    if (rx.parser.status != 200) {
        char why[24];
        snprintf(why, sizeof(why), "server answered %d", rx.parser.status);
        finish(false, false, why);
    } else if (!fix_ack_parse(rx.body, rx.body_len, &ack) || ack != sent_seq) {
        finish(false, false, "bad ack");
    } else {
        fix_buffer_ack(gps_buffer(), ack);
        printf("[FIX] acked seq %u, %u records, %u dropped so far, %u still buffered\n",
               (unsigned)ack, (unsigned)sent_count, (unsigned)fix_buffer_drops(gps_buffer()),
               (unsigned)fix_buffer_count(gps_buffer()));
        finish(true, false, "");
    }
}

static void step(uint32_t now, bool link_up, bool conn_free) {
    if (fstate != F_IDLE) {
        if (tls_client_state() != TLS_READY) {
            finish(false, false, "link lost");
        } else if (rx.error) {
            finish(false, true, "malformed reply");
        } else if (now - started_ms >= FIX_UPLOAD_TIMEOUT_S * 1000u) {
            finish(false, true, "timeout");
        } else if (fstate == F_SENDING) {
            pump_write();
        } else if (rx.done) {
            check_reply();
        }
        return;
    }
    if (cfg && fix_sched_due(&sched, now, fix_buffer_count(gps_buffer()), link_up && tls_client_state() == TLS_READY,
                             conn_free)) {
        start(now);
        if (fstate == F_SENDING) pump_write();
    }
}

void fix_upload_poll(bool link_up, bool conn_free) {
    last_link_up = link_up;
    last_conn_free = conn_free;
    step(now_ms(), link_up, conn_free);
}

void fix_upload_flush(void) {
    fix_sched_flush(&sched);
    step(now_ms(), last_link_up, last_conn_free);
}
