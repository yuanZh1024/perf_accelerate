#include "http_parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Percent-decoding; the target is a path so '+' is kept literally. */
static int pct_decode(const char *src, char *dst, size_t cap)
{
    size_t o = 0;

    while (*src) {
        int hi, lo;
        if (*src == '%') {
            hi = hexval((unsigned char)src[1]);
            lo = hi < 0 ? -1 : hexval((unsigned char)src[2]);
            if (hi < 0 || lo < 0)
                return -1;
            if (o + 1 >= cap)
                return -1;
            dst[o++] = (char)((hi << 4) | lo);
            src += 3;
        } else {
            if (o + 1 >= cap)
                return -1;
            dst[o++] = *src++;
        }
    }
    dst[o] = '\0';
    return 0;
}

int http_parse_head(const char *buf, size_t len, http_req_t *req, size_t *head_len)
{
    size_t i, head = 0;
    char *copy, *line, *save = NULL, *p, *target, *version;
    int found = 0;

    if (req == NULL)
        return -1;
    if (buf == NULL) {
        if (len != 0)
            return -1;
        memset(req, 0, sizeof(*req));
        return 0;
    }
    memset(req, 0, sizeof(*req));

    for (i = 0; i + 3 < len; i++) {
        if (memcmp(buf + i, "\r\n\r\n", 4) == 0) {
            head = i + 4;
            found = 1;
            break;
        }
    }
    if (!found) {
        for (i = 0; i + 1 < len; i++) {
            if (buf[i] == '\n' && buf[i + 1] == '\n') {
                head = i + 2;
                found = 1;
                break;
            }
        }
    }
    if (!found)
        return 0;

    copy = (char *)malloc(head + 1);
    if (copy == NULL)
        return -1;
    memcpy(copy, buf, head);
    copy[head] = '\0';

    line = strtok_r(copy, "\r\n", &save);
    if (line == NULL) {
        free(copy);
        return -1;
    }

    p = strchr(line, ' ');
    if (p == NULL) {
        free(copy);
        return -1;
    }
    *p = '\0';
    if (strlen(line) >= sizeof(req->method)) {
        free(copy);
        return -1;
    }
    memcpy(req->method, line, strlen(line) + 1);

    target = p + 1;
    version = strrchr(target, ' ');
    if (version == NULL) {
        free(copy);
        return -1;
    }
    *version++ = '\0';

    if (strncmp(version, "HTTP/1.", 7) != 0 || !isdigit((unsigned char)version[7])) {
        free(copy);
        return -1;
    }
    req->http_minor = version[7] - '0';

    p = strchr(target, '?');
    if (p != NULL) {
        *p = '\0';
        if (strlen(p + 1) >= sizeof(req->query)) {
            free(copy);
            return -1;
        }
        memcpy(req->query, p + 1, strlen(p + 1) + 1);
    }

    if (pct_decode(target, req->path, sizeof(req->path)) != 0) {
        free(copy);
        return -1;
    }

    while ((line = strtok_r(NULL, "\r\n", &save)) != NULL) {
        char *colon = strchr(line, ':');
        char *val;

        if (colon == NULL)
            continue;
        *colon = '\0';
        val = colon + 1;
        while (*val == ' ' || *val == '\t')
            val++;

        if (ieq(line, "Content-Length")) {
            char *end = NULL;
            unsigned long long v = strtoull(val, &end, 10);
            if (end == val) {
                free(copy);
                return -1;
            }
            req->has_content_length = 1;
            req->content_length = v;
        } else if (ieq(line, "Expect")) {
            if (strstr(val, "100-continue") != NULL)
                req->expect_continue = 1;
        } else if (ieq(line, "Content-Type")) {
            size_t n = strlen(val);
            if (n >= sizeof(req->content_type))
                n = sizeof(req->content_type) - 1;
            memcpy(req->content_type, val, n);
            req->content_type[n] = '\0';
        }
    }

    free(copy);
    *head_len = head;
    return 1;
}

int http_query_get(const char *query, const char *name, char *out, size_t cap)
{
    size_t name_len = strlen(name);
    const char *p = query;

    while (p != NULL && *p != '\0') {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);

        if ((size_t)(end - p) > name_len && strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
            const char *v = p + name_len + 1;
            size_t n = (size_t)(end - v);
            if (n >= cap)
                n = cap - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            return 0;
        }
        p = amp ? amp + 1 : NULL;
    }
    return -1;
}
