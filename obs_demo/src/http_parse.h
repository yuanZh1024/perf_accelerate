/*
 * http_parse.h - HTTP/1.x request head parser (no sockets involved).
 *
 * Kept separate from the server so that it can be unit tested on its own.
 */
#ifndef OBS_HTTP_PARSE_H
#define OBS_HTTP_PARSE_H

#include <stddef.h>

#define HTTP_PATH_MAX 1024
#define HTTP_QUERY_MAX 512

typedef struct {
    char method[16];
    char path[HTTP_PATH_MAX];   /* percent-decoded, without the query string */
    char query[HTTP_QUERY_MAX]; /* raw query string, may be empty */
    int http_minor;             /* 0 for HTTP/1.0, 1 for HTTP/1.1 */
    int has_content_length;
    unsigned long long content_length;
    int expect_continue;
    char content_type[128];
} http_req_t;

/*
 * Parse a request head.
 * Returns 1 when a complete head was found, 0 when more bytes are needed,
 * -1 when the request is malformed.
 * *head_len receives the number of bytes consumed.
 */
int http_parse_head(const char *buf, size_t len, http_req_t *req, size_t *head_len);

/* Copy the value of `name` from a query string ("a=1&b=2"). Returns 0 on
 * success, -1 when the parameter is absent. */
int http_query_get(const char *query, const char *name, char *out, size_t cap);

#endif /* OBS_HTTP_PARSE_H */
