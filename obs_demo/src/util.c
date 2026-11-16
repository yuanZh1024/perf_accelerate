#include "util.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* little endian                                                       */
/* ------------------------------------------------------------------ */

void le_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

uint32_t le_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void le_put64(uint8_t *p, uint64_t v)
{
    le_put32(p, (uint32_t)(v & 0xffffffffu));
    le_put32(p + 4, (uint32_t)(v >> 32));
}

uint64_t le_get64(const uint8_t *p)
{
    return (uint64_t)le_get32(p) | ((uint64_t)le_get32(p + 4) << 32);
}

/* ------------------------------------------------------------------ */
/* crc32                                                               */
/* ------------------------------------------------------------------ */

static uint32_t crc_table[256];
static int crc_table_ready = 0;

static void crc32_build(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_table_ready = 1;
}

uint32_t crc32_update(uint32_t crc, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t i;

    if (!crc_table_ready)
        crc32_build();

    for (i = 0; i < len; i++)
        crc = crc_table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc;
}

uint32_t crc32_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32_buf(const void *buf, size_t len)
{
    return crc32_final(crc32_update(CRC32_INIT, buf, len));
}

/* ------------------------------------------------------------------ */
/* logging                                                             */
/* ------------------------------------------------------------------ */

static int log_level = LOG_INFO;

void log_set_level(int level)
{
    log_level = level;
}

void log_msg(int level, const char *fmt, ...)
{
    static const char *names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    char ts[32];
    time_t t = time(NULL);
    struct tm tmv;
    va_list ap;

    if (level > log_level)
        return;

#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(stderr, "%s [%s] ", ts, names[level]);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

/* ------------------------------------------------------------------ */
/* sbuf                                                                */
/* ------------------------------------------------------------------ */

void sbuf_init(sbuf_t *sb)
{
    sb->p = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static int sbuf_reserve(sbuf_t *sb, size_t extra)
{
    size_t need = sb->len + extra + 1;
    char *np;

    if (need <= sb->cap)
        return 0;

    size_t cap = sb->cap ? sb->cap : 256;
    while (cap < need)
        cap *= 2;

    np = (char *)realloc(sb->p, cap);
    if (np == NULL)
        return -1;
    sb->p = np;
    sb->cap = cap;
    return 0;
}

int sbuf_append(sbuf_t *sb, const char *s, size_t n)
{
    if (sbuf_reserve(sb, n) != 0)
        return -1;
    memcpy(sb->p + sb->len, s, n);
    sb->len += n;
    sb->p[sb->len] = '\0';
    return 0;
}

int sbuf_puts(sbuf_t *sb, const char *s)
{
    return sbuf_append(sb, s, strlen(s));
}

int sbuf_printf(sbuf_t *sb, const char *fmt, ...)
{
    va_list ap;
    int n;
    char tmp[512];

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n < 0)
        return -1;
    if ((size_t)n < sizeof(tmp))
        return sbuf_append(sb, tmp, (size_t)n);

    /* rare: long formatted output */
    if (sbuf_reserve(sb, (size_t)n) != 0)
        return -1;
    va_start(ap, fmt);
    vsnprintf(sb->p + sb->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t)n;
    return 0;
}

void sbuf_free(sbuf_t *sb)
{
    free(sb->p);
    sb->p = NULL;
    sb->len = sb->cap = 0;
}

/* ------------------------------------------------------------------ */
/* misc                                                                */
/* ------------------------------------------------------------------ */

int key_valid(const char *key)
{
    size_t len, i;
    const char *part;

    if (key == NULL)
        return 0;
    len = strlen(key);
    if (len == 0 || len > KEY_MAX)
        return 0;
    if (key[0] == '/' || key[len - 1] == '/')
        return 0;
    if (strstr(key, "//") != NULL)
        return 0;

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)key[i];
        if (c == '/')
            continue;
        if (!(isalnum(c) || c == '-' || c == '_' || c == '.'))
            return 0;
    }

    /* reject "." and ".." path components */
    part = key;
    while (part != NULL) {
        const char *slash = strchr(part, '/');
        size_t n = slash ? (size_t)(slash - part) : strlen(part);
        if (n == 0)
            return 0;
        if ((n == 1 && part[0] == '.') || (n == 2 && part[0] == '.' && part[1] == '.'))
            return 0;
        part = slash ? slash + 1 : NULL;
    }
    return 1;
}

int parse_size(const char *s, uint32_t *out)
{
    char *end = NULL;
    unsigned long long v;
    unsigned long long mult = 1;

    if (s == NULL || *s == '\0')
        return -1;

    v = strtoull(s, &end, 10);
    if (end == s)
        return -1;

    if (*end != '\0') {
        switch (tolower((unsigned char)*end)) {
        case 'k': mult = 1024ULL; break;
        case 'm': mult = 1024ULL * 1024; break;
        case 'g': mult = 1024ULL * 1024 * 1024; break;
        default: return -1;
        }
        end++;
        if (*end != '\0' && !(end[0] == 'b' && end[1] == '\0'))
            return -1;
    }

    v *= mult;
    if (v == 0 || v > 0xFFFFFFFFull)
        return -1;
    *out = (uint32_t)v;
    return 0;
}

long long now_secs(void)
{
    return (long long)time(NULL);
}

int fmt_path(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= cap)
        return -1;
    return 0;
}
