/*
 * store.h - the erasure-coded object store.
 *
 * Layout on disk (root = data directory):
 *
 *   <root>/data/d0/<key>/shard_0.ec      <- data shard 0
 *   <root>/data/d1/<key>/shard_1.ec      <- data shard 1
 *   ...
 *   <root>/data/d{k+m-1}/<key>/shard_X.ec<- parity shard
 *   <root>/meta/<key>.meta               <- metadata, written last
 *
 * Each d<i> directory plays the role of one "storage node"; in a real
 * deployment they would be separate disks or separate machines.
 */
#ifndef OBS_STORE_H
#define OBS_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "meta.h"
#include "rs.h"
#include "util.h"

typedef struct store store_t;

/* Return codes. */
#define STORE_OK 0
#define STORE_ERR_IO (-2)       /* disk error */
#define STORE_ERR_INVALID (-3)  /* bad key / bad geometry */
#define STORE_ERR_NOTFOUND (-4)
#define STORE_ERR_SHORT (-5)    /* fewer than k usable shards */
#define STORE_ERR_CORRUPT (-6)  /* metadata unreadable */
#define STORE_ERR_CLIENT (-7)   /* request body shorter than declared */

/* Source of object bytes (used by PUT). read() fills up to n bytes and
 * reports the count in *got; returns 0 on success, -1 on error. */
typedef struct {
    int (*read)(void *ctx, void *buf, size_t n, size_t *got);
    void *ctx;
} obj_src_t;

/* Destination for GET. on_meta() is called exactly once, before any data,
 * so the caller can emit response headers with the final size. */
typedef struct {
    int (*on_meta)(void *ctx, const obj_meta_t *m);
    int (*on_data)(void *ctx, const void *buf, size_t n);
    void *ctx;
} obj_sink_t;

typedef struct {
    uint64_t objects;
    uint64_t bytes;
    uint64_t shard_files;
    uint64_t shard_bytes;
    uint64_t shards[RS_MAX_N];
} store_stats_t;

int store_open(store_t **out, const char *root, int k, int m, uint32_t shard_size);
void store_close(store_t *st);

int store_put(store_t *st, const char *key, uint64_t size, obj_src_t *src, uint32_t *etag_out);
int store_meta_get(store_t *st, const char *key, obj_meta_t *m);
int store_get(store_t *st, const char *key, obj_sink_t *sink);
int store_delete(store_t *st, const char *key);
int store_list(store_t *st, const char *prefix, sbuf_t *json);
int store_stats(store_t *st, store_stats_t *out);

/* Report the configured EC geometry. */
void store_geometry(const store_t *st, int *k, int *m, uint32_t *shard_size);

const char *store_errstr(int rc);

#endif /* OBS_STORE_H */
