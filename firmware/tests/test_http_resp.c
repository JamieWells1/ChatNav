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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_whole_response);
    RUN_TEST(test_byte_by_byte);
    RUN_TEST(test_401_and_close);
    RUN_TEST(test_extra_bytes_ignored);
    RUN_TEST(test_headers_event_then_body);
    RUN_TEST(test_errors);
    return UNITY_END();
}
