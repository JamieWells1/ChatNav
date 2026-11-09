#include <string.h>

#include "net/chunkq.h"
#include "net/http_req.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_post_head(void) {
    http_req_t r = {.method = "POST", .path = "/v1/query", .host = "chat.example",
                    .token = "id.secret", .content_type = "audio/L16;rate=16000",
                    .chunked_body = true};
    char out[512];
    size_t n = http_req_build(out, sizeof(out), &r);
    TEST_ASSERT_EQUAL_STRING("POST /v1/query HTTP/1.1\r\nHost: chat.example\r\n"
                             "Authorization: Bearer id.secret\r\n"
                             "Content-Type: audio/L16;rate=16000\r\n"
                             "Transfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n",
                             out);
    TEST_ASSERT_EQUAL_UINT(strlen(out), n);
}

static void test_get_head_and_overflow(void) {
    http_req_t r = {.method = "GET", .path = "/v1/query/last", .host = "h", .token = ""};
    char out[256];
    size_t n = http_req_build(out, sizeof(out), &r);
    TEST_ASSERT_NOT_NULL(strstr(out, "GET /v1/query/last HTTP/1.1\r\n"));
    TEST_ASSERT_NULL(strstr(out, "Authorization"));
    TEST_ASSERT_NULL(strstr(out, "Transfer-Encoding"));
    TEST_ASSERT_TRUE(n > 0);
    char tiny[20];
    TEST_ASSERT_EQUAL_UINT(0, http_req_build(tiny, sizeof(tiny), &r));
    for (size_t cap = 1; cap < n; cap++) {
        char small[256];
        TEST_ASSERT_EQUAL_UINT(0, http_req_build(small, cap, &r));
    }
}

static void test_chunk_encode(void) {
    uint8_t pcm[2048];
    memset(pcm, 0xAB, sizeof(pcm));
    uint8_t out[2100];
    size_t n = http_chunk_encode(pcm, 2048, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(5 + 2048 + 2, n);
    TEST_ASSERT_EQUAL_MEMORY("800\r\n", out, 5);
    TEST_ASSERT_EQUAL_UINT8(0xAB, out[5]);
    TEST_ASSERT_EQUAL_MEMORY("\r\n", out + 5 + 2048, 2);

    n = http_chunk_encode(pcm, 10, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(3 + 10 + 2, n);
    TEST_ASSERT_EQUAL_MEMORY("a\r\n", out, 3);

    TEST_ASSERT_EQUAL_UINT(0, http_chunk_encode(pcm, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT(0, http_chunk_encode(pcm, 2049, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT(0, http_chunk_encode(pcm, 10, out, 14));

    TEST_ASSERT_EQUAL_UINT(5, http_chunk_end(out, sizeof(out)));
    TEST_ASSERT_EQUAL_MEMORY("0\r\n\r\n", out, 5);
}

static chunkq_t q;

static void test_queue_order_and_partial_send(void) {
    chunkq_reset(&q);
    uint8_t pcm[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    TEST_ASSERT_TRUE(chunkq_push_raw(&q, "HEAD", 4));
    TEST_ASSERT_TRUE(chunkq_push_chunk(&q, pcm, 8));
    TEST_ASSERT_TRUE(chunkq_push_end(&q));
    TEST_ASSERT_EQUAL_UINT(3, chunkq_count(&q));

    const uint8_t *p;
    TEST_ASSERT_EQUAL_UINT(4, chunkq_peek(&q, &p));
    TEST_ASSERT_EQUAL_MEMORY("HEAD", p, 4);
    chunkq_consume(&q, 1);
    TEST_ASSERT_EQUAL_UINT(3, chunkq_peek(&q, &p));
    TEST_ASSERT_EQUAL_MEMORY("EAD", p, 3);
    chunkq_consume(&q, 3);
    TEST_ASSERT_EQUAL_UINT(3 + 8 + 2, chunkq_peek(&q, &p)); /* "8\r\n" + data + CRLF */
    TEST_ASSERT_EQUAL_MEMORY("8\r\n", p, 3);
    chunkq_consume(&q, 13);
    TEST_ASSERT_EQUAL_UINT(5, chunkq_peek(&q, &p));
    chunkq_consume(&q, 5);
    TEST_ASSERT_EQUAL_UINT(0, chunkq_peek(&q, &p));
}

static void test_queue_full_and_wrap(void) {
    chunkq_reset(&q);
    uint8_t pcm[16] = {0};
    for (int i = 0; i < CHUNKQ_SLOTS; i++) TEST_ASSERT_TRUE(chunkq_push_chunk(&q, pcm, 16));
    TEST_ASSERT_FALSE(chunkq_push_chunk(&q, pcm, 16));
    TEST_ASSERT_EQUAL_UINT32(1, q.dropped);
    TEST_ASSERT_FALSE(chunkq_push_end(&q));
    const uint8_t *p;
    size_t len = chunkq_peek(&q, &p);
    chunkq_consume(&q, len);
    TEST_ASSERT_TRUE(chunkq_push_end(&q)); /* freed slot is reused at the tail */
    for (int i = 0; i < CHUNKQ_SLOTS - 1; i++) {
        len = chunkq_peek(&q, &p);
        chunkq_consume(&q, len);
    }
    TEST_ASSERT_EQUAL_UINT(5, chunkq_peek(&q, &p));
    TEST_ASSERT_EQUAL_MEMORY("0\r\n\r\n", p, 5);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_post_head);
    RUN_TEST(test_get_head_and_overflow);
    RUN_TEST(test_chunk_encode);
    RUN_TEST(test_queue_order_and_partial_send);
    RUN_TEST(test_queue_full_and_wrap);
    return UNITY_END();
}
