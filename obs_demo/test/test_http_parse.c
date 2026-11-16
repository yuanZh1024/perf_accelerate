/*
 * test_http_parse.c - request head parsing.
 */
#include "../src/http_parse.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                   \
    do {                                   \
        if (!(cond)) {                     \
            printf("FAIL %d: ", __LINE__); \
            printf(__VA_ARGS__);           \
            printf("\n");                  \
            failures++;                    \
        }                                  \
    } while (0)

static int parse(const char *raw, http_req_t *req, size_t *head_len)
{
    return http_parse_head(raw, strlen(raw), req, head_len);
}

int main(void)
{
    http_req_t req;
    size_t hl = 0;
    char val[64];

    printf("test_http_parse\n");

    /* simple GET */
    CHECK(parse("GET /v1/objects/a.txt HTTP/1.1\r\nHost: x\r\n\r\n", &req, &hl) == 1, "simple GET");
    CHECK(strcmp(req.method, "GET") == 0, "method: %s", req.method);
    CHECK(strcmp(req.path, "/v1/objects/a.txt") == 0, "path: %s", req.path);
    CHECK(req.http_minor == 1, "minor: %d", req.http_minor);
    CHECK(hl == strlen("GET /v1/objects/a.txt HTTP/1.1\r\nHost: x\r\n\r\n"), "head_len %zu", hl);

    /* percent decoding + query string */
    CHECK(parse("PUT /v1/objects/dir/a%20b%2Fc.bin?x=1&y=2 HTTP/1.1\r\n\r\n", &req, &hl) == 1,
          "encoded PUT");
    CHECK(strcmp(req.path, "/v1/objects/dir/a b/c.bin") == 0, "decoded path: %s", req.path);
    CHECK(strcmp(req.query, "x=1&y=2") == 0, "query: %s", req.query);
    CHECK(http_query_get(req.query, "y", val, sizeof(val)) == 0 && strcmp(val, "2") == 0,
          "query_get y=%s", val);
    CHECK(http_query_get(req.query, "z", val, sizeof(val)) == -1, "query_get should miss z");

    /* content-length + expect */
    CHECK(parse("PUT /v1/objects/k HTTP/1.1\r\ncontent-length: 1234\r\n"
                "Expect: 100-continue\r\nContent-Type: text/plain\r\n\r\n",
                &req, &hl) == 1,
          "PUT with body");
    CHECK(req.has_content_length == 1 && req.content_length == 1234, "content-length %llu",
          req.content_length);
    CHECK(req.expect_continue == 1, "expect_continue");
    CHECK(strcmp(req.content_type, "text/plain") == 0, "content-type %s", req.content_type);

    /* incomplete head needs more data */
    CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\n", &req, &hl) == 0, "incomplete should return 0");
    CHECK(parse("", &req, &hl) == 0, "empty should return 0");

    /* malformed */
    CHECK(parse("garbage\r\n\r\n", &req, &hl) == -1, "garbage should fail");
    CHECK(parse("GET / HTTP/9.9\r\n\r\n", &req, &hl) == -1, "bad version should fail");
    CHECK(parse("GET / HTTP/1.1\r\nContent-Length: abc\r\n\r\n", &req, &hl) == -1,
          "bad content-length should fail");

    /* LF only (some clients/tools do this) */
    CHECK(parse("GET /v1/health HTTP/1.0\nHost: x\n\n", &req, &hl) == 1, "LF only head");
    CHECK(strcmp(req.path, "/v1/health") == 0, "LF path: %s", req.path);
    CHECK(req.http_minor == 0, "LF minor: %d", req.http_minor);

    if (failures) {
        printf("test_http_parse: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_http_parse: ok\n");
    return 0;
}
