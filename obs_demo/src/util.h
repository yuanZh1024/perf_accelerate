/*
 * util.h - small helpers: crc32, logging, string builders, key validation.
 */
#ifndef OBS_UTIL_H
#define OBS_UTIL_H

#include <stddef.h>
#include <stdint.h>

#define KEY_MAX 512

/* ---- little endian serialisation ---- */
void le_put32(uint8_t *p, uint32_t v);
uint32_t le_get32(const uint8_t *p);
void le_put64(uint8_t *p, uint64_t v);
uint64_t le_get64(const uint8_t *p);

/* ---- crc32 (IEEE 802.3, poly 0xEDB88320) ---- */
#define CRC32_INIT 0xFFFFFFFFu
uint32_t crc32_update(uint32_t crc, const void *buf, size_t len);
uint32_t crc32_final(uint32_t crc); /* xor with 0xFFFFFFFF */
uint32_t crc32_buf(const void *buf, size_t len);

/* ---- logging ---- */
#define LOG_ERROR 0
#define LOG_WARN 1
#define LOG_INFO 2
#define LOG_DEBUG 3
void log_set_level(int level);
void log_msg(int level, const char *fmt, ...);
#define log_error(...) log_msg(LOG_ERROR, __VA_ARGS__)
#define log_warn(...) log_msg(LOG_WARN, __VA_ARGS__)
#define log_info(...) log_msg(LOG_INFO, __VA_ARGS__)
#define log_debug(...) log_msg(LOG_DEBUG, __VA_ARGS__)

/* ---- growable string ---- */
typedef struct {
    char *p;
    size_t len;
    size_t cap;
} sbuf_t;

void sbuf_init(sbuf_t *sb);
int sbuf_append(sbuf_t *sb, const char *s, size_t n);
int sbuf_puts(sbuf_t *sb, const char *s);
int sbuf_printf(sbuf_t *sb, const char *fmt, ...);
void sbuf_free(sbuf_t *sb);

/* ---- misc ---- */
/* Object key: relative path, 1..KEY_MAX chars, no empty/"."/".." parts,
 * only [A-Za-z0-9._/-]. */
int key_valid(const char *key);

/* "256K", "1m", "1048576" -> bytes. Returns 0 on success. */
int parse_size(const char *s, uint32_t *out);

long long now_secs(void);

/* snprintf with truncation detection. Returns 0 on success, -1 if truncated. */
int fmt_path(char *dst, size_t cap, const char *fmt, ...);

#endif /* OBS_UTIL_H */
