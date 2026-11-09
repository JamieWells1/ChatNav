#include <string.h>

#include "net/http_resp.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const char OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nok";

static void test_whole_response(void) {
    http_resp_t r;
    http_resp_init(&r);
    const char *body;
    size_t n;
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, http_resp_feed(&r, OK, strlen(OK), &body, &n));
    TEST_ASSERT_EQUAL_INT(200, r.status);
    TEST_ASSERT_EQUAL_UINT(2, n);
    TEST_ASSERT_EQUAL_MEMORY("ok", body, 2);
    TEST_ASSERT_TRUE(r.keep_alive);
}

static void test_byte_by_byte(void) {
    http_resp_t r;
    http_resp_init(&r);
    http_resp_event_t ev = HTTP_RESP_NEED_MORE;
    char got[8] = {0};
    size_t total = 0;
    for (size_t i = 0; i < strlen(OK); i++) {
        const char *b;
        size_t n;
        ev = http_resp_feed(&r, OK + i, 1, &b, &n);
        if (n) {
            memcpy(got + total, b, n);
            total += n;
        }
    }
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, ev);
    TEST_ASSERT_EQUAL_STRING("ok", got);
}

static void test_401_and_close(void) {
    const char *msg =
        "HTTP/1.1 401 Unauthorized\r\ncontent-length: 0\r\nConnection: close\r\n\r\n";
    http_resp_t r;
    http_resp_init(&r);
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, http_resp_feed(&r, msg, strlen(msg), NULL, NULL));
    TEST_ASSERT_EQUAL_INT(401, r.status);
    TEST_ASSERT_FALSE(r.keep_alive);
}

static void test_extra_bytes_ignored(void) {
    const char *msg = "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nabcdef";
    http_resp_t r;
    http_resp_init(&r);
    const char *body;
    size_t n;
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, http_resp_feed(&r, msg, strlen(msg), &body, &n));
    TEST_ASSERT_EQUAL_UINT(1, n);
}

static void test_headers_event_then_body(void) {
    const char *head = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n";
    http_resp_t r;
    http_resp_init(&r);
    TEST_ASSERT_EQUAL(HTTP_RESP_HEADERS, http_resp_feed(&r, head, strlen(head), NULL, NULL));
    const char *b;
    size_t n;
    TEST_ASSERT_EQUAL(HTTP_RESP_NEED_MORE, http_resp_feed(&r, "ab", 2, &b, &n));
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, http_resp_feed(&r, "cd", 2, &b, &n));
}

static void test_errors(void) {
    http_resp_t r;
    http_resp_init(&r);
    const char *junk = "garbage\r\n\r\n";
    TEST_ASSERT_EQUAL(HTTP_RESP_ERROR, http_resp_feed(&r, junk, strlen(junk), NULL, NULL));

    http_resp_init(&r);
    const char *chunked = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    TEST_ASSERT_EQUAL(HTTP_RESP_ERROR, http_resp_feed(&r, chunked, strlen(chunked), NULL, NULL));

    http_resp_init(&r);
    char big[HTTP_RESP_HEADER_MAX + 10];
    memset(big, 'a', sizeof(big));
    TEST_ASSERT_EQUAL(HTTP_RESP_ERROR, http_resp_feed(&r, big, sizeof(big), NULL, NULL));
}

/* Feeds data in pieces of step bytes, collecting body bytes through feed_ex. */
static http_resp_event_t feed_all(http_resp_t *r, const char *data, size_t len, size_t step,
                                  char *out, size_t *out_len) {
    http_resp_event_t ev = HTTP_RESP_NEED_MORE;
    for (size_t off = 0; off < len; off += step) {
        size_t piece = len - off < step ? len - off : step;
        size_t used = 0;
        while (used < piece) {
            size_t consumed;
            const char *b;
            size_t n;
            ev = http_resp_feed_ex(r, data + off + used, piece - used, &consumed, &b, &n);
            if (ev == HTTP_RESP_ERROR) return ev;
            memcpy(out + *out_len, b, n);
            *out_len += n;
            used += consumed;
            if (ev == HTTP_RESP_DONE) return ev;
        }
    }
    return ev;
}

static const char CHUNKED[] =
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nX-Outcome: no_speech\r\n"
    "Content-Type: audio/L16\r\n\r\n"
    "5\r\nhello\r\n6;ext=1\r\n world\r\n0\r\n\r\n";

static void test_chunked_any_split(void) {
    for (size_t step = 1; step <= sizeof(CHUNKED) - 1; step += (step < 8 ? 1 : 17)) {
        http_resp_t r;
        http_resp_init_stream(&r);
        char out[64] = {0};
        size_t n = 0;
        TEST_ASSERT_EQUAL(HTTP_RESP_DONE, feed_all(&r, CHUNKED, sizeof(CHUNKED) - 1, step, out, &n));
        TEST_ASSERT_EQUAL_UINT(11, n);
        TEST_ASSERT_EQUAL_STRING("hello world", out);
        TEST_ASSERT_EQUAL_STRING("no_speech", r.outcome);
        TEST_ASSERT_TRUE(r.chunked);
    }
}

static void test_chunked_trailers_and_errors(void) {
    http_resp_t r;
    http_resp_init_stream(&r);
    const char *with_trailer = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                               "2\r\nab\r\n0\r\nX-T: 1\r\n\r\n";
    char out[8] = {0};
    size_t n = 0;
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, feed_all(&r, with_trailer, strlen(with_trailer), 5, out, &n));
    TEST_ASSERT_EQUAL_UINT(2, n);

    http_resp_init_stream(&r);
    const char *bad = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n";
    n = 0;
    TEST_ASSERT_EQUAL(HTTP_RESP_ERROR, feed_all(&r, bad, strlen(bad), 100, out, &n));

    http_resp_init_stream(&r);
    const char *no_crlf = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nabXY";
    n = 0;
    TEST_ASSERT_EQUAL(HTTP_RESP_ERROR, feed_all(&r, no_crlf, strlen(no_crlf), 100, out, &n));
}

static void test_outcome_and_length_stream(void) {
    http_resp_t r;
    http_resp_init_stream(&r);
    const char *msg = "HTTP/1.1 200 OK\r\nx-outcome: error\r\nContent-Length: 3\r\n\r\nabc";
    char out[8] = {0};
    size_t n = 0;
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, feed_all(&r, msg, strlen(msg), 7, out, &n));
    TEST_ASSERT_EQUAL_STRING("abc", out);
    TEST_ASSERT_EQUAL_STRING("error", r.outcome);
    TEST_ASSERT_FALSE(r.chunked);
}

static void test_status_404_chunked_empty(void) {
    http_resp_t r;
    http_resp_init_stream(&r);
    const char *msg = "HTTP/1.1 404 Not Found\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
    char out[4];
    size_t n = 0;
    TEST_ASSERT_EQUAL(HTTP_RESP_DONE, feed_all(&r, msg, strlen(msg), 3, out, &n));
    TEST_ASSERT_EQUAL_INT(404, r.status);
    TEST_ASSERT_EQUAL_UINT(0, n);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_chunked_any_split);
    RUN_TEST(test_chunked_trailers_and_errors);
    RUN_TEST(test_outcome_and_length_stream);
    RUN_TEST(test_status_404_chunked_empty);
    RUN_TEST(test_whole_response);
    RUN_TEST(test_byte_by_byte);
    RUN_TEST(test_401_and_close);
    RUN_TEST(test_extra_bytes_ignored);
    RUN_TEST(test_headers_event_then_body);
    RUN_TEST(test_errors);
    return UNITY_END();
}
