/*
 * meta.h - object metadata (the "commit record" of an object).
 *
 * The metadata file is the single source of truth for an object: once it
 * is in place the object is readable, and it carries everything needed to
 * validate and repair the shards (size, geometry, etag and one CRC32 per
 * stripe per shard).
 */
#ifndef OBS_META_H
#define OBS_META_H

#include <stddef.h>
#include <stdint.h>

#define META_VERSION 1u
#define META_HEADER_SIZE 56

typedef struct {
    int k;
    int m;
    int nshards;
    uint64_t size;        /* original object size in bytes */
    uint32_t shard_size;  /* bytes per data shard, per stripe */
    uint32_t num_stripes;
    uint32_t etag;        /* crc32 of the whole object */
    uint64_t created_at;
    uint32_t *crc;        /* num_stripes * nshards entries, stripe major */
} obj_meta_t;

void meta_init(obj_meta_t *m);

/* Serialise into a freshly malloc'ed buffer. Returns 0 on success. */
int meta_encode(const obj_meta_t *m, uint8_t **buf, size_t *len);

/* Parse and validate a buffer produced by meta_encode. Returns 0 on success.
 * The returned struct owns a malloc'ed crc array; free with meta_free(). */
int meta_decode(const uint8_t *buf, size_t len, obj_meta_t *m);

void meta_free(obj_meta_t *m);

/* Number of bytes of the object carried by stripe `s`. */
uint64_t meta_stripe_bytes(const obj_meta_t *m, uint32_t s);

#endif /* OBS_META_H */
