#include <stdio.h>
#include <string.h>

#include "config.h"
#include "gps/fix_buffer.h"
#include "gps/fix_record.h"
#include "net/fix_batch.h"
#include "net/fix_sched.h"
#include "net/http_req.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const fix_record_t golden_fixes[3] = {
    {1000, 1794227719u, 250, 375000000, -1220000000, 1234, 2710, -35, 9, 1, 12, 0x0F},
    {2000, 1794227720u, 250, 375000450, -1219999000, 0, 0, 1234, 4, 2, 255, 0x0F},
    {3000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00},
};

static size_t read_golden(uint8_t *out, size_t cap) {
    FILE *f = fopen(FIXTURE_DIR "/fix_batch_golden.bin", "rb");
    TEST_ASSERT_NOT_NULL(f);
    size_t n = fread(out, 1, cap, f);
    fclose(f);
    return n;
}

static void test_batch_matches_server_bytes(void) {
    uint8_t want[256];
    size_t want_len = read_golden(want, sizeof(want));
    TEST_ASSERT_EQUAL_UINT(FIX_BATCH_HEADER_SIZE + 3 * FIX_RECORD_WIRE_SIZE, want_len);

    uint8_t recs[3 * FIX_RECORD_WIRE_SIZE];
    for (int i = 0; i < 3; i++) fix_record_pack(&golden_fixes[i], recs + i * FIX_RECORD_WIRE_SIZE);
    uint8_t got[256];
    size_t n = fix_batch_build(got, sizeof(got), 0x01020304u + 41, 4, 7, recs, 3);
    TEST_ASSERT_EQUAL_UINT(want_len, n);
    TEST_ASSERT_EQUAL_MEMORY(want, got, want_len);
}

static void test_batch_does_not_fit_and_empty(void) {
    uint8_t recs[FIX_RECORD_WIRE_SIZE] = {0};
    uint8_t out[64];
    TEST_ASSERT_EQUAL_UINT(0, fix_batch_build(out, FIX_BATCH_HEADER_SIZE + FIX_RECORD_WIRE_SIZE - 1,
                                              1, 4, 0, recs, 1));
    TEST_ASSERT_EQUAL_UINT(FIX_BATCH_HEADER_SIZE, fix_batch_build(out, sizeof(out), 1, 4, 0, NULL, 0));
}

static void test_drop_count_saturates(void) {
    uint8_t h[FIX_BATCH_HEADER_SIZE];
    fix_batch_header(h, 1, 4, 70000, 0);
    TEST_ASSERT_EQUAL_UINT8(0xFF, h[6]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, h[7]);
}

static void test_ack_parse(void) {
    uint32_t a = 0;
    TEST_ASSERT_TRUE(fix_ack_parse("{\"ack\":42}", 10, &a));
    TEST_ASSERT_EQUAL_UINT32(42, a);
    const char *spaced = "{\"ack\": 4000000000 }";
    TEST_ASSERT_TRUE(fix_ack_parse(spaced, strlen(spaced), &a));
    TEST_ASSERT_EQUAL_UINT32(4000000000u, a);
    TEST_ASSERT_FALSE(fix_ack_parse("{\"ack\":null}", 12, &a));
    TEST_ASSERT_FALSE(fix_ack_parse("{\"detail\":\"x\"}", 14, &a));
    TEST_ASSERT_FALSE(fix_ack_parse("{\"ack\":99999999999}", 19, &a));
    TEST_ASSERT_FALSE(fix_ack_parse("", 0, &a));
}

/* Records stay until acked: a failed batch is rebuilt identically; ack releases it. */
static void test_retry_keeps_records_until_ack(void) {
    uint8_t store[10 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 10);
    for (int i = 0; i < 5; i++) {
        fix_record_t r = {.device_ms = 100u * (unsigned)i};
        fix_buffer_push(&fb, &r);
    }
    uint8_t recs[FIX_BATCH_MAX * FIX_RECORD_WIRE_SIZE];
    uint32_t first;
    size_t n = fix_buffer_peek(&fb, recs, FIX_BATCH_MAX, &first);
    TEST_ASSERT_EQUAL_UINT(5, n);
    uint32_t seq1 = first + (uint32_t)n - 1;
    TEST_ASSERT_EQUAL_UINT32(5, seq1);
    uint8_t b1[512], b2[512];
    size_t l1 = fix_batch_build(b1, sizeof(b1), seq1, 4, 0, recs, (uint16_t)n);
    /* no ack: the retry carries the same bytes, plus new records after them */
    fix_record_t extra = {.device_ms = 999};
    fix_buffer_push(&fb, &extra);
    n = fix_buffer_peek(&fb, recs, FIX_BATCH_MAX, &first);
    TEST_ASSERT_EQUAL_UINT(6, n);
    size_t l2 = fix_batch_build(b2, sizeof(b2), first + (uint32_t)n - 1, 4, 0, recs, (uint16_t)n);
    TEST_ASSERT_EQUAL_UINT(l1 + FIX_RECORD_WIRE_SIZE, l2);
    TEST_ASSERT_EQUAL_MEMORY(b1 + FIX_BATCH_HEADER_SIZE, b2 + FIX_BATCH_HEADER_SIZE,
                             l1 - FIX_BATCH_HEADER_SIZE);
    TEST_ASSERT_TRUE(fix_buffer_ack(&fb, 6));
    TEST_ASSERT_EQUAL_UINT(0, fix_buffer_count(&fb));
}

static void test_full_buffer_drops_oldest_and_reports(void) {
    uint8_t store[4 * FIX_RECORD_WIRE_SIZE];
    fix_buffer_t fb;
    fix_buffer_init(&fb, store, 4);
    for (int i = 0; i < 7; i++) {
        fix_record_t r = {.device_ms = (uint32_t)i};
        fix_buffer_push(&fb, &r);
    }
    uint8_t recs[4 * FIX_RECORD_WIRE_SIZE];
    uint32_t first;
    size_t n = fix_buffer_peek(&fb, recs, 4, &first);
    TEST_ASSERT_EQUAL_UINT(4, n);
    TEST_ASSERT_EQUAL_UINT32(4, first);
    uint8_t out[256];
    fix_batch_build(out, sizeof(out), first + 3, 4, fix_buffer_drops(&fb), recs, 4);
    TEST_ASSERT_EQUAL_UINT8(3, out[6]); /* three dropped */
    fix_record_t u;
    fix_record_unpack(out + FIX_BATCH_HEADER_SIZE, &u);
    TEST_ASSERT_EQUAL_UINT32(3, u.device_ms); /* oldest surviving */
}

static void test_no_fix_record_is_device_ms_only_and_flagged(void) {
    nmea_msg_t rmc = {.kind = NMEA_RMC, .time_ms = -1, .status_valid = false};
    nmea_msg_t gga = {.kind = NMEA_GGA, .time_ms = -1, .quality = 0};
    fix_builder_t b;
    fix_builder_init(&b);
    fix_record_t r;
    TEST_ASSERT_FALSE(fix_builder_add(&b, &rmc, 5000, &r));
    TEST_ASSERT_TRUE(fix_builder_add(&b, &gga, 5010, &r));
    TEST_ASSERT_EQUAL_UINT32(5000, r.device_ms);
    TEST_ASSERT_EQUAL_UINT32(0, r.utc_s);
    TEST_ASSERT_FALSE(r.flags & FIX_FLAG_TIME_VALID);
    TEST_ASSERT_FALSE(r.flags & FIX_FLAG_POS);
    TEST_ASSERT_FALSE(r.flags & FIX_FLAG_RMC_VALID);
}

static void test_sched_interval_flush_and_gating(void) {
    fix_sched_t s;
    fix_sched_init(&s);
    uint32_t t = 100000;
    TEST_ASSERT_FALSE(fix_sched_due(&s, t, 0, true, true));  /* nothing to send */
    TEST_ASSERT_FALSE(fix_sched_due(&s, t, 3, false, true)); /* link down */
    TEST_ASSERT_FALSE(fix_sched_due(&s, t, 3, true, false)); /* connection busy */
    TEST_ASSERT_TRUE(fix_sched_due(&s, t, 3, true, true));   /* first batch goes at once */
    fix_sched_started(&s, t);
    fix_sched_result(&s, true);
    TEST_ASSERT_FALSE(fix_sched_due(&s, t + FIX_UPLOAD_INTERVAL_S * 1000 - 1, 3, true, true));
    TEST_ASSERT_TRUE(fix_sched_due(&s, t + FIX_UPLOAD_INTERVAL_S * 1000, 3, true, true));
    /* flush ignores the interval but still waits for a free connection */
    fix_sched_flush(&s);
    TEST_ASSERT_FALSE(fix_sched_due(&s, t + 10, 3, true, false));
    TEST_ASSERT_TRUE(fix_sched_due(&s, t + 10, 3, true, true));
    /* a full batch does not wait for the interval */
    fix_sched_init(&s);
    fix_sched_started(&s, t);
    fix_sched_result(&s, true);
    TEST_ASSERT_TRUE(fix_sched_due(&s, t + 1, FIX_BATCH_MAX, true, true));
}

static void test_sched_retry_after_failure(void) {
    fix_sched_t s;
    fix_sched_init(&s);
    uint32_t t = 50000;
    fix_sched_flush(&s);
    fix_sched_started(&s, t);
    fix_sched_result(&s, false);
    /* even a flush waits out the retry delay; the flush survives the failure */
    TEST_ASSERT_FALSE(fix_sched_due(&s, t + FIX_UPLOAD_RETRY_S * 1000 - 1, 40, true, true));
    TEST_ASSERT_TRUE(fix_sched_due(&s, t + FIX_UPLOAD_RETRY_S * 1000, 40, true, true));
    fix_sched_started(&s, t + FIX_UPLOAD_RETRY_S * 1000);
    fix_sched_result(&s, true);
    TEST_ASSERT_FALSE(s.flush);
    TEST_ASSERT_FALSE(s.retry_wait);
    /* millisecond counter wrap */
    fix_sched_init(&s);
    fix_sched_started(&s, 0xFFFFFF00u);
    fix_sched_result(&s, true);
    TEST_ASSERT_TRUE(fix_sched_due(&s, 0xFFFFFF00u + FIX_UPLOAD_INTERVAL_S * 1000, 1, true, true));
}

static void test_post_head_has_length_and_token(void) {
    char head[256];
    http_req_t spec = {.method = "POST", .path = "/v1/fixes", .host = "h", .token = "tok",
                       .content_type = "application/octet-stream", .content_length = 94};
    size_t n = http_req_build(head, sizeof(head), &spec);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(head, "POST /v1/fixes HTTP/1.1\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(head, "Authorization: Bearer tok\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(head, "Content-Length: 94\r\n"));
    TEST_ASSERT_NULL(strstr(head, "Transfer-Encoding"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_batch_matches_server_bytes);
    RUN_TEST(test_batch_does_not_fit_and_empty);
    RUN_TEST(test_drop_count_saturates);
    RUN_TEST(test_ack_parse);
    RUN_TEST(test_retry_keeps_records_until_ack);
    RUN_TEST(test_full_buffer_drops_oldest_and_reports);
    RUN_TEST(test_no_fix_record_is_device_ms_only_and_flagged);
    RUN_TEST(test_sched_interval_flush_and_gating);
    RUN_TEST(test_sched_retry_after_failure);
    RUN_TEST(test_post_head_has_length_and_token);
    return UNITY_END();
}
