/*
 * store.c - erasure coded object storage (4+2 by default).
 *
 * Write path:
 *   body --> stripe (k*shard_size) --> k data shards --> m parity shards
 *        --> one file per shard --> metadata file (commit point)
 *
 * Read path:
 *   metadata --> all shards of a stripe --> per shard CRC32 check
 *            --> rebuild missing/broken shards from any k good ones
 *            --> emit the first k shards as object bytes
 *
 * Every stripe is independent, so memory usage is bounded by one stripe
 * no matter how large the object is.
 */
#include "store.h"

#include "os.h"
#include "rs.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STORE_LOCKS 64
#define SHARD_HDR_SIZE 512
#define PATH_CAP 2048
#define ROOT_CAP 1024
#define MAX_META_BYTES (256ull * 1024 * 1024)
#define MAX_WALK_DEPTH 40

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

static const char SHARD_MAGIC[8] = {'O', 'B', 'S', 'E', 'C', '0', '0', '1'};

struct store {
    char root[ROOT_CAP];
    int k, m, n;
    uint32_t shard_size;
    uint64_t stripe_size;
    rs_t rs;
    pthread_rwlock_t locks[STORE_LOCKS];
};

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static unsigned key_hash(const char *key)
{
    unsigned h = 2166136261u;
    while (*key) {
        h ^= (unsigned char)*key++;
        h *= 16777619u;
    }
    return h;
}

static int shard_path(const store_t *st, const char *key, int idx, char *dst, size_t cap)
{
    return fmt_path(dst, cap, "%s/data/d%d/%s/shard_%d.ec", st->root, idx, key, idx);
}

static int meta_path(const store_t *st, const char *key, char *dst, size_t cap)
{
    return fmt_path(dst, cap, "%s/meta/%s.meta", st->root, key);
}

static int read_file(const char *path, uint8_t **out, size_t *out_len)
{
    uint64_t size = 0;
    int fd;
    uint8_t *buf;

    if (os_file_size(path, &size) != 0)
        return -1;
    if (size > MAX_META_BYTES)
        return -1;

    fd = os_open_read(path);
    if (fd == OS_FD_BAD)
        return -1;

    buf = (uint8_t *)malloc((size_t)size + 1);
    if (buf == NULL) {
        os_close(fd);
        return -1;
    }
    if (size > 0 && os_pread(fd, buf, (size_t)size, 0) != (long long)size) {
        free(buf);
        os_close(fd);
        return -1;
    }
    os_close(fd);
    *out = buf;
    *out_len = (size_t)size;
    return 0;
}

/* ------------------------------------------------------------------ */
/* shard file header                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t version;
    uint32_t shard_idx;
    uint32_t k;
    uint32_t m;
    uint32_t shard_size;
    uint32_t num_stripes;
    uint64_t payload_len;
    uint32_t payload_crc;
    uint64_t object_size;
} shard_hdr_t;

static void hdr_pack(const shard_hdr_t *h, uint8_t *buf)
{
    memset(buf, 0, SHARD_HDR_SIZE);
    memcpy(buf, SHARD_MAGIC, 8);
    le_put32(buf + 8, 1);
    le_put32(buf + 12, h->shard_idx);
    le_put32(buf + 16, h->k);
    le_put32(buf + 20, h->m);
    le_put32(buf + 24, h->shard_size);
    le_put32(buf + 28, h->num_stripes);
    le_put64(buf + 32, h->payload_len);
    le_put32(buf + 40, h->payload_crc);
    le_put64(buf + 44, h->object_size);
    le_put32(buf + 52, crc32_buf(buf, 52));
}

static int hdr_unpack(const uint8_t *buf, shard_hdr_t *h)
{
    if (memcmp(buf, SHARD_MAGIC, 8) != 0)
        return -1;
    if (crc32_buf(buf, 52) != le_get32(buf + 52))
        return -1;

    h->version = le_get32(buf + 8);
    h->shard_idx = le_get32(buf + 12);
    h->k = le_get32(buf + 16);
    h->m = le_get32(buf + 20);
    h->shard_size = le_get32(buf + 24);
    h->num_stripes = le_get32(buf + 28);
    h->payload_len = le_get64(buf + 32);
    h->payload_crc = le_get32(buf + 40);
    h->object_size = le_get64(buf + 44);

    if (h->version != 1)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* open / close                                                        */
/* ------------------------------------------------------------------ */

int store_open(store_t **out, const char *root, int k, int m, uint32_t shard_size)
{
    store_t *st;
    char path[PATH_CAP];
    int i;

    if (root == NULL || strlen(root) >= ROOT_CAP || shard_size == 0)
        return -1;
    if (k < 1 || m < 1 || k + m > RS_MAX_N)
        return -1;

    st = (store_t *)calloc(1, sizeof(*st));
    if (st == NULL)
        return -1;

    snprintf(st->root, sizeof(st->root), "%s", root);
    st->k = k;
    st->m = m;
    st->n = k + m;
    st->shard_size = shard_size;
    st->stripe_size = (uint64_t)shard_size * (uint64_t)k;

    if (rs_init(&st->rs, k, m) != 0) {
        free(st);
        return -1;
    }

    for (i = 0; i < STORE_LOCKS; i++)
        pthread_rwlock_init(&st->locks[i], NULL);

    if (os_mkdir_p(st->root) != 0)
        goto fail;
    if (fmt_path(path, sizeof(path), "%s/meta", st->root) != 0 || os_mkdir_p(path) != 0)
        goto fail;
    for (i = 0; i < st->n; i++) {
        if (fmt_path(path, sizeof(path), "%s/data/d%d", st->root, i) != 0)
            goto fail;
        if (os_mkdir_p(path) != 0)
            goto fail;
    }

    *out = st;
    return 0;

fail:
    rs_free(&st->rs);
    for (i = 0; i < STORE_LOCKS; i++)
        pthread_rwlock_destroy(&st->locks[i]);
    free(st);
    return -1;
}

void store_close(store_t *st)
{
    int i;
    if (st == NULL)
        return;
    rs_free(&st->rs);
    for (i = 0; i < STORE_LOCKS; i++)
        pthread_rwlock_destroy(&st->locks[i]);
    free(st);
}

const char *store_errstr(int rc)
{
    switch (rc) {
    case STORE_OK: return "ok";
    case STORE_ERR_IO: return "storage io error";
    case STORE_ERR_INVALID: return "invalid request";
    case STORE_ERR_NOTFOUND: return "object not found";
    case STORE_ERR_SHORT: return "not enough healthy shards";
    case STORE_ERR_CORRUPT: return "object metadata is corrupt";
    case STORE_ERR_CLIENT: return "request body shorter than content-length";
    default: return "unknown error";
    }
}

void store_geometry(const store_t *st, int *k, int *m, uint32_t *shard_size)
{
    if (k)
        *k = st->k;
    if (m)
        *m = st->m;
    if (shard_size)
        *shard_size = st->shard_size;
}

/* ------------------------------------------------------------------ */
/* metadata                                                            */
/* ------------------------------------------------------------------ */

static int load_meta(store_t *st, const char *key, obj_meta_t *m)
{
    char path[PATH_CAP];
    uint8_t *buf = NULL;
    size_t len = 0;
    int rc;

    if (meta_path(st, key, path, sizeof(path)) != 0)
        return STORE_ERR_INVALID;
    if (read_file(path, &buf, &len) != 0)
        return STORE_ERR_NOTFOUND;

    rc = meta_decode(buf, len, m) == 0 ? STORE_OK : STORE_ERR_CORRUPT;
    free(buf);
    return rc;
}

int store_meta_get(store_t *st, const char *key, obj_meta_t *m)
{
    int rc;
    unsigned h;

    if (!key_valid(key))
        return STORE_ERR_INVALID;

    h = key_hash(key) % STORE_LOCKS;
    pthread_rwlock_rdlock(&st->locks[h]);
    rc = load_meta(st, key, m);
    pthread_rwlock_unlock(&st->locks[h]);
    return rc;
}

/* ------------------------------------------------------------------ */
/* put                                                                 */
/* ------------------------------------------------------------------ */

static int read_exact(obj_src_t *src, void *buf, size_t n)
{
    size_t done = 0;

    while (done < n) {
        size_t got = 0;
        if (src->read(src->ctx, (uint8_t *)buf + done, n - done, &got) != 0)
            return -1;
        if (got == 0)
            return -1; /* premature end of body */
        done += got;
    }
    return 0;
}

int store_put(store_t *st, const char *key, uint64_t size, obj_src_t *src, uint32_t *etag_out)
{
    char tmp[RS_MAX_N][PATH_CAP];
    char fin[RS_MAX_N][PATH_CAP];
    int fds[RS_MAX_N];
    uint8_t *pool = NULL;
    uint8_t *shards[RS_MAX_N];
    uint32_t pcrc[RS_MAX_N];
    obj_meta_t meta;
    uint64_t num_stripes64, done_bytes = 0;
    uint32_t num_stripes, s, etag;
    int i, rc = STORE_OK;
    unsigned lk;

    if (!key_valid(key))
        return STORE_ERR_INVALID;
    if (size > 0 && st->stripe_size == 0)
        return STORE_ERR_INVALID;

    num_stripes64 = size == 0 ? 0 : (size + st->stripe_size - 1) / st->stripe_size;
    if (num_stripes64 > 0xFFFFFFFFull)
        return STORE_ERR_INVALID;
    num_stripes = (uint32_t)num_stripes64;

    if ((uint64_t)num_stripes * (uint64_t)st->n * 4ull > MAX_META_BYTES)
        return STORE_ERR_INVALID;

    meta_init(&meta);
    meta.k = st->k;
    meta.m = st->m;
    meta.nshards = st->n;
    meta.size = size;
    meta.shard_size = st->shard_size;
    meta.num_stripes = num_stripes;
    meta.created_at = (uint64_t)now_secs();
    if (num_stripes > 0) {
        meta.crc = (uint32_t *)calloc((size_t)num_stripes * (size_t)st->n, sizeof(uint32_t));
        if (meta.crc == NULL)
            return STORE_ERR_IO;
    }

    pool = (uint8_t *)malloc((size_t)st->n * st->shard_size);
    if (pool == NULL) {
        rc = STORE_ERR_IO;
        goto out;
    }
    for (i = 0; i < st->n; i++) {
        shards[i] = pool + (size_t)i * st->shard_size;
        fds[i] = OS_FD_BAD;
        pcrc[i] = CRC32_INIT;
    }

    lk = key_hash(key) % STORE_LOCKS;
    pthread_rwlock_wrlock(&st->locks[lk]);

    /* 1. create the shard files and reserve room for their headers */
    for (i = 0; i < st->n; i++) {
        uint8_t zero[SHARD_HDR_SIZE];

        if (shard_path(st, key, i, fin[i], PATH_CAP) != 0 ||
            fmt_path(tmp[i], PATH_CAP, "%s.tmp", fin[i]) != 0) {
            rc = STORE_ERR_INVALID;
            goto unlock;
        }
        if (os_mkdir_parent(fin[i]) != 0) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
        os_unlink(tmp[i]);
        fds[i] = os_open_rw_trunc(tmp[i]);
        if (fds[i] == OS_FD_BAD) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
        memset(zero, 0, sizeof(zero));
        if (os_pwrite(fds[i], zero, sizeof(zero), 0) != (long long)sizeof(zero)) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
    }

    /* 2. stream the body: one stripe at a time, encode, write shards */
    etag = CRC32_INIT;
    for (s = 0; s < num_stripes; s++) {
        uint64_t want = size - done_bytes;
        if (want > st->stripe_size)
            want = st->stripe_size;

        if (want < st->stripe_size)
            memset(pool + want, 0, (size_t)(st->stripe_size - want));

        if (read_exact(src, pool, (size_t)want) != 0) {
            rc = STORE_ERR_CLIENT;
            goto unlock;
        }
        etag = crc32_update(etag, pool, (size_t)want);

        rs_encode(&st->rs, shards, shards + st->k, st->shard_size);

        for (i = 0; i < st->n; i++) {
            uint64_t off = SHARD_HDR_SIZE + (uint64_t)s * st->shard_size;
            uint32_t c = crc32_buf(shards[i], st->shard_size);

            if (os_pwrite(fds[i], shards[i], st->shard_size, off) != (long long)st->shard_size) {
                rc = STORE_ERR_IO;
                goto unlock;
            }
            meta.crc[(size_t)s * st->n + i] = c;
            pcrc[i] = crc32_update(pcrc[i], shards[i], st->shard_size);
        }
        done_bytes += want;
    }
    meta.etag = crc32_final(etag);

    /* 3. finalise shard headers, flush and atomically publish the shards */
    for (i = 0; i < st->n; i++) {
        shard_hdr_t h;
        uint8_t hbuf[SHARD_HDR_SIZE];

        h.version = 1;
        h.shard_idx = (uint32_t)i;
        h.k = (uint32_t)st->k;
        h.m = (uint32_t)st->m;
        h.shard_size = st->shard_size;
        h.num_stripes = num_stripes;
        h.payload_len = (uint64_t)num_stripes * st->shard_size;
        h.payload_crc = crc32_final(pcrc[i]);
        h.object_size = size;

        hdr_pack(&h, hbuf);
        if (os_pwrite(fds[i], hbuf, SHARD_HDR_SIZE, 0) != (long long)SHARD_HDR_SIZE) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
        os_fsync(fds[i]);
        os_close(fds[i]);
        fds[i] = OS_FD_BAD;
        if (os_rename(tmp[i], fin[i]) != 0) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
    }

    /* 4. metadata last: this is the commit point */
    {
        uint8_t *mbuf = NULL;
        size_t mlen = 0;
        char mpath[PATH_CAP], mtmp[PATH_CAP];
        int mfd;

        if (meta_encode(&meta, &mbuf, &mlen) != 0) {
            rc = STORE_ERR_IO;
            goto unlock;
        }
        if (meta_path(st, key, mpath, PATH_CAP) != 0 ||
            fmt_path(mtmp, PATH_CAP, "%s.tmp", mpath) != 0) {
            free(mbuf);
            rc = STORE_ERR_INVALID;
            goto unlock;
        }
        if (os_mkdir_parent(mpath) != 0) {
            free(mbuf);
            rc = STORE_ERR_IO;
            goto unlock;
        }
        os_unlink(mtmp);
        mfd = os_open_rw_trunc(mtmp);
        if (mfd == OS_FD_BAD) {
            free(mbuf);
            rc = STORE_ERR_IO;
            goto unlock;
        }
        if (os_pwrite(mfd, mbuf, mlen, 0) != (long long)mlen) {
            os_close(mfd);
            os_unlink(mtmp);
            free(mbuf);
            rc = STORE_ERR_IO;
            goto unlock;
        }
        os_fsync(mfd);
        os_close(mfd);
        if (os_rename(mtmp, mpath) != 0) {
            os_unlink(mtmp);
            free(mbuf);
            rc = STORE_ERR_IO;
            goto unlock;
        }
        free(mbuf);
    }

    if (etag_out)
        *etag_out = meta.etag;
    rc = STORE_OK;

unlock:
    pthread_rwlock_unlock(&st->locks[lk]);
out:
    for (i = 0; i < st->n; i++) {
        if (fds[i] != OS_FD_BAD) {
            os_close(fds[i]);
            os_unlink(tmp[i]);
        }
    }
    free(pool);
    meta_free(&meta);
    return rc;
}

/* ------------------------------------------------------------------ */
/* get                                                                 */
/* ------------------------------------------------------------------ */

int store_get(store_t *st, const char *key, obj_sink_t *sink)
{
    obj_meta_t meta;
    int fds[RS_MAX_N];
    uint8_t *pool = NULL;
    uint8_t *shards[RS_MAX_N];
    uint8_t present[RS_MAX_N];
    uint32_t s, i, etag;
    int rc, opened = 0, bad = 0;
    unsigned lk;

    if (!key_valid(key))
        return STORE_ERR_INVALID;
    meta_init(&meta);
    for (i = 0; i < (uint32_t)st->n; i++) {
        fds[i] = OS_FD_BAD;
        present[i] = 0;
    }

    lk = key_hash(key) % STORE_LOCKS;
    pthread_rwlock_rdlock(&st->locks[lk]);

    rc = load_meta(st, key, &meta);
    if (rc != STORE_OK)
        goto out;

    pool = (uint8_t *)malloc((size_t)st->n * st->shard_size);
    if (pool == NULL) {
        rc = STORE_ERR_IO;
        goto out;
    }
    for (i = 0; i < (uint32_t)st->n; i++)
        shards[i] = pool + (size_t)i * st->shard_size;

    /* open and validate every shard we can find */
    for (i = 0; i < (uint32_t)st->n; i++) {
        char path[PATH_CAP];
        uint8_t hbuf[SHARD_HDR_SIZE];
        shard_hdr_t h;

        if (shard_path(st, key, (int)i, path, sizeof(path)) != 0)
            continue;
        fds[i] = os_open_read(path);
        if (fds[i] == OS_FD_BAD)
            continue;
        opened++;
        if (os_pread(fds[i], hbuf, SHARD_HDR_SIZE, 0) != SHARD_HDR_SIZE ||
            hdr_unpack(hbuf, &h) != 0 || h.shard_idx != i || h.k != (uint32_t)st->k ||
            h.m != (uint32_t)st->m || h.shard_size != meta.shard_size ||
            h.num_stripes != meta.num_stripes || h.object_size != meta.size) {
            log_warn("shard %u of '%s' has a bad header, ignoring it", i, key);
            os_close(fds[i]);
            fds[i] = OS_FD_BAD;
            opened--;
            bad++;
        }
    }

    /* Fail before the caller commits to a 200 response if the object can
     * no longer be rebuilt. */
    if (opened < st->k) {
        log_error("object '%s': only %d/%d shard files usable", key, opened, st->k);
        rc = STORE_ERR_SHORT;
        goto out;
    }

    if (sink->on_meta != NULL && sink->on_meta(sink->ctx, &meta) != 0) {
        rc = STORE_ERR_IO;
        goto out;
    }

    /* stripe by stripe: verify, repair, emit */
    etag = CRC32_INIT;
    for (s = 0; s < meta.num_stripes; s++) {
        uint64_t base = SHARD_HDR_SIZE + (uint64_t)s * meta.shard_size;
        int good = 0;
        uint64_t left;

        for (i = 0; i < (uint32_t)st->n; i++) {
            present[i] = 0;
            if (fds[i] == OS_FD_BAD)
                continue;
            if (os_pread(fds[i], shards[i], meta.shard_size, base) != (long long)meta.shard_size)
                continue;
            if (crc32_buf(shards[i], meta.shard_size) != meta.crc[(size_t)s * st->n + i]) {
                bad++;
                continue;
            }
            present[i] = 1;
            good++;
        }

        if (good < st->k) {
            log_error("object '%s' stripe %u: only %d/%d shards healthy (%d corrupted)",
                      key, s, good, st->k, bad);
            rc = STORE_ERR_CORRUPT;
            goto out;
        }
        if (good < st->n) {
            log_info("object '%s' stripe %u: rebuilding %d shard(s)", key, s, st->n - good);
            if (rs_reconstruct(&st->rs, shards, present, meta.shard_size) != 0) {
                rc = STORE_ERR_CORRUPT;
                goto out;
            }
        }

        left = meta_stripe_bytes(&meta, s);
        for (i = 0; i < (uint32_t)st->k && left > 0; i++) {
            size_t take = left > meta.shard_size ? meta.shard_size : (size_t)left;
            if (sink->on_data != NULL && sink->on_data(sink->ctx, shards[i], take) != 0) {
                rc = STORE_ERR_IO;
                goto out;
            }
            etag = crc32_update(etag, shards[i], take);
            left -= take;
        }
    }

    if (crc32_final(etag) != meta.etag && meta.size > 0) {
        log_error("object '%s': etag mismatch after read", key);
        rc = STORE_ERR_CORRUPT;
        goto out;
    }
    rc = STORE_OK;

out:
    for (i = 0; i < (uint32_t)st->n; i++)
        if (fds[i] != OS_FD_BAD)
            os_close(fds[i]);
    free(pool);
    meta_free(&meta);
    pthread_rwlock_unlock(&st->locks[lk]);
    return rc;
}

/* ------------------------------------------------------------------ */
/* delete                                                              */
/* ------------------------------------------------------------------ */

int store_delete(store_t *st, const char *key)
{
    char path[PATH_CAP];
    int i, rc = STORE_ERR_NOTFOUND;
    unsigned lk;

    if (!key_valid(key))
        return STORE_ERR_INVALID;

    lk = key_hash(key) % STORE_LOCKS;
    pthread_rwlock_wrlock(&st->locks[lk]);

    if (meta_path(st, key, path, PATH_CAP) == 0 && os_unlink(path) == 0)
        rc = STORE_OK;

    for (i = 0; i < st->n; i++) {
        if (shard_path(st, key, i, path, PATH_CAP) == 0)
            os_unlink(path);
    }

    pthread_rwlock_unlock(&st->locks[lk]);
    return rc;
}

/* ------------------------------------------------------------------ */
/* list / stats                                                        */
/* ------------------------------------------------------------------ */

typedef int (*walk_cb)(void *ctx, const char *full, const char *rel, const struct stat *st);

static int walk_dir(const char *base, const char *rel, walk_cb cb, void *ctx, int depth)
{
    char dirpath[PATH_CAP];
    DIR *d;
    struct dirent *de;
    int rc = 0;

    if (depth > MAX_WALK_DEPTH)
        return 0;

    if (rel[0] == '\0') {
        if (fmt_path(dirpath, sizeof(dirpath), "%s", base) != 0)
            return 0;
    } else {
        if (fmt_path(dirpath, sizeof(dirpath), "%s/%s", base, rel) != 0)
            return 0;
    }

    d = opendir(dirpath);
    if (d == NULL)
        return 0;

    while ((de = readdir(d)) != NULL) {
        char full[PATH_CAP], nrel[PATH_CAP];
        struct stat st;

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        if (rel[0] == '\0')
            snprintf(nrel, sizeof(nrel), "%s", de->d_name);
        else
            snprintf(nrel, sizeof(nrel), "%s/%s", rel, de->d_name);

        if (fmt_path(full, sizeof(full), "%s/%s", base, nrel) != 0)
            continue;
        if (stat(full, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            if (walk_dir(base, nrel, cb, ctx, depth + 1) != 0) {
                rc = -1;
                break;
            }
        } else {
            if (cb(ctx, full, nrel, &st) != 0) {
                rc = -1;
                break;
            }
        }
    }
    closedir(d);
    return rc;
}

typedef struct {
    store_t *st;
    const char *prefix;
    sbuf_t *sb;
    int count;
} list_ctx_t;

static int list_cb(void *vctx, const char *full, const char *rel, const struct stat *fst)
{
    list_ctx_t *c = (list_ctx_t *)vctx;
    size_t rel_len = strlen(rel);
    char key[PATH_CAP];
    obj_meta_t m;
    uint8_t *buf = NULL;
    size_t len = 0;

    if (rel_len <= 5 || strcmp(rel + rel_len - 5, ".meta") != 0)
        return 0;

    memcpy(key, rel, rel_len - 5);
    key[rel_len - 5] = '\0';

    if (c->prefix != NULL && *c->prefix != '\0' && strncmp(key, c->prefix, strlen(c->prefix)) != 0)
        return 0;

    meta_init(&m);
    if (read_file(full, &buf, &len) == 0 && meta_decode(buf, len, &m) == 0) {
        sbuf_printf(c->sb, "%s{\"key\":\"%s\",\"size\":%llu,\"etag\":\"%08x\",\"shards\":\"%d+%d\",\"mtime\":%lld}",
                    c->count ? "," : "", key, (unsigned long long)m.size, m.etag, m.k, m.m,
                    (long long)fst->st_mtime);
    } else {
        sbuf_printf(c->sb, "%s{\"key\":\"%s\",\"size\":%llu,\"etag\":null}",
                    c->count ? "," : "", key, (unsigned long long)fst->st_size);
    }
    free(buf);
    meta_free(&m);
    c->count++;
    return 0;
}

int store_list(store_t *st, const char *prefix, sbuf_t *json)
{
    char base[PATH_CAP];
    list_ctx_t ctx;
    int rc;

    if (fmt_path(base, sizeof(base), "%s/meta", st->root) != 0)
        return STORE_ERR_IO;

    ctx.st = st;
    ctx.prefix = prefix;
    ctx.sb = json;
    ctx.count = 0;

    sbuf_puts(json, "{\"objects\":[");
    rc = walk_dir(base, "", list_cb, &ctx, 0);
    sbuf_printf(json, "],\"count\":%d}\n", ctx.count);

    return rc == 0 ? STORE_OK : STORE_ERR_IO;
}

typedef struct {
    store_t *st;
    int shard_idx;
    uint64_t files;
    uint64_t bytes;
    uint64_t objects;
} stats_ctx_t;

static int stats_cb(void *vctx, const char *full, const char *rel, const struct stat *st)
{
    stats_ctx_t *c = (stats_ctx_t *)vctx;
    size_t rel_len = strlen(rel);

    (void)full;
    if (c->shard_idx >= 0) {
        if (rel_len > 3 && strcmp(rel + rel_len - 3, ".ec") == 0) {
            c->files++;
            c->bytes += (uint64_t)st->st_size;
        }
    } else {
        if (rel_len > 5 && strcmp(rel + rel_len - 5, ".meta") == 0)
            c->objects++;
    }
    return 0;
}

int store_stats(store_t *st, store_stats_t *out)
{
    char path[PATH_CAP];
    int i;

    memset(out, 0, sizeof(*out));

    for (i = 0; i < st->n; i++) {
        stats_ctx_t c;
        memset(&c, 0, sizeof(c));
        c.st = st;
        c.shard_idx = i;
        if (fmt_path(path, sizeof(path), "%s/data/d%d", st->root, i) != 0)
            return STORE_ERR_IO;
        if (walk_dir(path, "", stats_cb, &c, 0) != 0)
            return STORE_ERR_IO;
        out->shard_files += c.files;
        out->shard_bytes += c.bytes;
        out->shards[i] = c.files;
    }

    {
        stats_ctx_t c;
        memset(&c, 0, sizeof(c));
        c.st = st;
        c.shard_idx = -1;
        if (fmt_path(path, sizeof(path), "%s/meta", st->root) != 0)
            return STORE_ERR_IO;
        if (walk_dir(path, "", stats_cb, &c, 0) != 0)
            return STORE_ERR_IO;
        out->objects = c.objects;
    }
    return STORE_OK;
}
