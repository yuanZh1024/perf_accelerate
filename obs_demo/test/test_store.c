/*
 * test_store.c - end to end tests of the EC object store, including
 * simulated disk/node failures.
 */
#include "../src/os.h"
#include "../src/store.h"
#include "../src/util.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#define ROOT "test/tmp/store"
#define K 4
#define M 2
#define SHARD_SIZE 4096u

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

static unsigned rng_state = 0xdeadbeefu;

static uint8_t rnd8(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (uint8_t)(rng_state & 0xff);
}

/* ------------------------------------------------------------------ */

static void rm_rf(const char *path)
{
    DIR *d = opendir(path);

    if (d != NULL) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            char child[2048];
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            if (os_is_dir(child))
                rm_rf(child);
            else
                os_unlink(child);
        }
        closedir(d);
        os_rmdir_if_empty(path);
    } else {
        os_unlink(path);
    }
}

static void shard_path(const char *key, int idx, char *out, size_t cap)
{
    snprintf(out, cap, ROOT "/data/d%d/%s/shard_%d.ec", idx, key, idx);
}

static void meta_file(const char *key, char *out, size_t cap)
{
    snprintf(out, cap, ROOT "/meta/%s.meta", key);
}

/* ------------------------------------------------------------------ */

typedef struct {
    const uint8_t *p;
    size_t len;
    size_t off;
} mem_src_t;

static int mem_read(void *ctx, void *buf, size_t n, size_t *got)
{
    mem_src_t *m = (mem_src_t *)ctx;
    size_t left = m->len - m->off;
    size_t take = n < left ? n : left;

    memcpy(buf, m->p + m->off, take);
    m->off += take;
    *got = take;
    return 0;
}

typedef struct {
    uint8_t *p;
    size_t len;
    size_t cap;
    uint64_t expect;
    int meta_calls;
} mem_sink_t;

static int sink_meta(void *ctx, const obj_meta_t *m)
{
    mem_sink_t *s = (mem_sink_t *)ctx;
    s->meta_calls++;
    s->expect = m->size;
    return 0;
}

static int sink_data(void *ctx, const void *buf, size_t n)
{
    mem_sink_t *s = (mem_sink_t *)ctx;

    if (s->len + n > s->cap) {
        size_t cap = s->cap ? s->cap : 1024;
        uint8_t *np;
        while (cap < s->len + n)
            cap *= 2;
        np = (uint8_t *)realloc(s->p, cap);
        if (np == NULL)
            return -1;
        s->p = np;
        s->cap = cap;
    }
    memcpy(s->p + s->len, buf, n);
    s->len += n;
    return 0;
}

static int put_obj(store_t *st, const char *key, const uint8_t *data, size_t len)
{
    mem_src_t src;
    src.p = data;
    src.len = len;
    src.off = 0;
    obj_src_t osrc;
    uint32_t etag = 0;
    int rc;

    osrc.read = mem_read;
    osrc.ctx = &src;
    rc = store_put(st, key, len, &osrc, &etag);
    if (rc == STORE_OK)
        CHECK(src.off == len, "source not fully consumed for %s", key);
    return rc;
}

static int get_obj(store_t *st, const char *key, uint8_t **out, size_t *out_len)
{
    mem_sink_t sink;
    obj_sink_t osink;
    int rc;

    memset(&sink, 0, sizeof(sink));
    osink.on_meta = sink_meta;
    osink.on_data = sink_data;
    osink.ctx = &sink;

    rc = store_get(st, key, &osink);
    *out = sink.p;
    *out_len = sink.len;

    if (rc == STORE_OK) {
        CHECK(sink.meta_calls == 1, "on_meta called %d times", sink.meta_calls);
        CHECK((uint64_t)sink.len == sink.expect, "size mismatch: %zu vs %llu", sink.len,
              (unsigned long long)sink.expect);
    }
    return rc;
}

/* ------------------------------------------------------------------ */

static void test_sizes(store_t *st)
{
    static const size_t sizes[] = {0, 1, 100, 4095, 4096, 4097, 16383, 16384, 16385, 100000};
    size_t i;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t len = sizes[i];
        uint8_t *data = (uint8_t *)malloc(len ? len : 1);
        uint8_t *back = NULL;
        size_t back_len = 0;
        char key[64];
        size_t j;
        int rc;

        for (j = 0; j < len; j++)
            data[j] = rnd8();

        snprintf(key, sizeof(key), "sizes/obj-%zu.bin", len);
        rc = put_obj(st, key, data, len);
        CHECK(rc == STORE_OK, "put %s failed rc=%d", key, rc);

        rc = get_obj(st, key, &back, &back_len);
        CHECK(rc == STORE_OK, "get %s failed rc=%d", key, rc);
        CHECK(back_len == len, "length mismatch for %s: %zu != %zu", key, back_len, len);
        CHECK(len == 0 || memcmp(data, back, len) == 0, "content mismatch for %s", key);

        free(data);
        free(back);
    }
}

static void test_layout(store_t *st)
{
    int i;
    (void)st;

    for (i = 0; i < K + M; i++) {
        char path[2048];
        shard_path("sizes/obj-16385.bin", i, path, sizeof(path));
        CHECK(os_exists(path), "missing shard file %s", path);
    }
    {
        char path[2048];
        meta_file("sizes/obj-16385.bin", path, sizeof(path));
        CHECK(os_exists(path), "missing metadata file %s", path);
    }
}

/* Remove `count` shard files of an object. */
static void kill_shards(const char *key, int count, const int *which)
{
    int i;
    for (i = 0; i < count; i++) {
        char path[2048];
        shard_path(key, which[i], path, sizeof(path));
        CHECK(os_unlink(path) == 0, "cannot remove %s", path);
    }
}

/* Flip one byte in the payload of a shard. */
static void corrupt_shard(const char *key, int idx)
{
    char path[2048];
    uint8_t b = 0;
    int fd;

    shard_path(key, idx, path, sizeof(path));
    fd = os_open_rw(path);
    CHECK(fd != OS_FD_BAD, "cannot open %s for corruption", path);
    if (fd == OS_FD_BAD)
        return;
    CHECK(os_pread(fd, &b, 1, 512 + 17) == 1, "cannot read %s", path);
    b ^= 0xff;
    CHECK(os_pwrite(fd, &b, 1, 512 + 17) == 1, "cannot write %s", path);
    os_close(fd);
}

static void test_failures(store_t *st)
{
    const size_t len = 50000;
    uint8_t *data = (uint8_t *)malloc(len);
    uint8_t *back = NULL;
    size_t back_len = 0;
    size_t i;
    int rc;
    int list[3];

    for (i = 0; i < len; i++)
        data[i] = rnd8();

    /* --- 1. healthy read --- */
    CHECK(put_obj(st, "fail/healthy.dat", data, len) == STORE_OK, "put failed");
    rc = get_obj(st, "fail/healthy.dat", &back, &back_len);
    CHECK(rc == STORE_OK && back_len == len && memcmp(back, data, len) == 0, "healthy read failed");
    free(back);
    back = NULL;

    /* --- 2. lose two data shards (d0 and d2): 4 of 6 left --- */
    CHECK(put_obj(st, "fail/two-missing.dat", data, len) == STORE_OK, "put failed");
    list[0] = 0;
    list[1] = 2;
    kill_shards("fail/two-missing.dat", 2, list);
    rc = get_obj(st, "fail/two-missing.dat", &back, &back_len);
    CHECK(rc == STORE_OK, "read with 2 shards missing failed rc=%d", rc);
    CHECK(back_len == len && memcmp(back, data, len) == 0, "rebuilt content mismatch (2 missing)");
    free(back);
    back = NULL;
    printf("  ok: d0,d2 removed -> rebuilt from d1,d3,d4,d5\n");

    /* --- 3. one shard missing and one silently corrupted: 4 of 6 usable --- */
    CHECK(put_obj(st, "fail/corrupt.dat", data, len) == STORE_OK, "put failed");
    list[0] = 0;
    kill_shards("fail/corrupt.dat", 1, list);
    corrupt_shard("fail/corrupt.dat", 5);
    rc = get_obj(st, "fail/corrupt.dat", &back, &back_len);
    CHECK(rc == STORE_OK, "read with corruption failed rc=%d", rc);
    CHECK(back_len == len && memcmp(back, data, len) == 0, "rebuilt content mismatch (corrupt d5)");
    free(back);
    back = NULL;
    printf("  ok: d0 removed + d5 corrupted -> CRC caught it, rebuilt from d1,d2,d3,d4\n");

    /* --- 4. three shards gone: only 3 of 6 remain -> must fail --- */
    CHECK(put_obj(st, "fail/three-missing.dat", data, len) == STORE_OK, "put failed");
    list[0] = 0;
    list[1] = 1;
    list[2] = 2;
    kill_shards("fail/three-missing.dat", 3, list);
    rc = get_obj(st, "fail/three-missing.dat", &back, &back_len);
    CHECK(rc == STORE_ERR_SHORT, "expected STORE_ERR_SHORT with 3/6 shards, got %d", rc);
    printf("  ok: d0,d1,d2 removed -> read refused (rc=%d)\n", rc);
    free(back);

    /* --- 5. metadata is the commit record --- */
    CHECK(put_obj(st, "fail/no-meta.dat", data, len) == STORE_OK, "put failed");
    {
        char path[2048];
        meta_file("fail/no-meta.dat", path, sizeof(path));
        os_unlink(path);
        back = NULL;
        rc = get_obj(st, "fail/no-meta.dat", &back, &back_len);
        CHECK(rc == STORE_ERR_NOTFOUND, "expected NOTFOUND after metadata loss, got %d", rc);
        free(back);
    }

    free(data);
}

static void test_api(store_t *st)
{
    const char *key = "api/doc.txt";
    const char *body = "hello erasure coded world";
    obj_meta_t m;
    sbuf_t sb;
    store_stats_t stats;
    uint8_t *back = NULL;
    size_t back_len = 0;
    int rc;

    CHECK(put_obj(st, key, (const uint8_t *)body, strlen(body)) == STORE_OK, "put failed");

    meta_init(&m);
    rc = store_meta_get(st, key, &m);
    CHECK(rc == STORE_OK, "meta_get failed rc=%d", rc);
    CHECK(m.size == strlen(body), "meta size wrong");
    CHECK(m.k == K && m.m == M, "meta geometry wrong");
    meta_free(&m);

    rc = store_meta_get(st, "api/missing", &m);
    CHECK(rc == STORE_ERR_NOTFOUND, "missing object should be NOTFOUND");

    sbuf_init(&sb);
    rc = store_list(st, "api/", &sb);
    CHECK(rc == STORE_OK, "list failed");
    CHECK(sb.p != NULL && strstr(sb.p, "\"key\":\"api/doc.txt\"") != NULL, "list output: %s",
          sb.p ? sb.p : "(null)");
    sbuf_free(&sb);

    sbuf_init(&sb);
    rc = store_list(st, "no-such-prefix", &sb);
    CHECK(rc == STORE_OK && strstr(sb.p, "\"count\":0") != NULL, "empty list output: %s", sb.p);
    sbuf_free(&sb);

    rc = store_stats(st, &stats);
    CHECK(rc == STORE_OK, "stats failed");
    CHECK(stats.objects > 0, "stats reports no objects");
    CHECK(stats.shard_files > 0, "stats reports no shard files");
    printf("  stats: %llu objects, %llu shard files, %llu shard bytes\n",
           (unsigned long long)stats.objects, (unsigned long long)stats.shard_files,
           (unsigned long long)stats.shard_bytes);

    rc = store_delete(st, key);
    CHECK(rc == STORE_OK, "delete failed");
    rc = store_delete(st, key);
    CHECK(rc == STORE_ERR_NOTFOUND, "double delete should be NOTFOUND");

    rc = get_obj(st, key, &back, &back_len);
    CHECK(rc == STORE_ERR_NOTFOUND, "get after delete should be NOTFOUND");
    free(back);
}

static void test_bad_keys(store_t *st)
{
    const uint8_t body[] = "x";
    obj_src_t src;
    mem_src_t ms;
    uint32_t etag;
    static const char *bad[] = {"", "/abs", "a//b", "a/../b", "../x", "a/", "bad key", "a\\b"};
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        ms.p = body;
        ms.len = 1;
        ms.off = 0;
        src.read = mem_read;
        src.ctx = &ms;
        CHECK(store_put(st, bad[i], 1, &src, &etag) == STORE_ERR_INVALID,
              "key '%s' should be rejected", bad[i]);
    }
    CHECK(key_valid("photos/2024/cat.jpg") == 1, "valid key rejected");
    CHECK(key_valid("a.b_c-d") == 1, "valid key rejected");
}

int main(void)
{
    store_t *st = NULL;

    printf("test_store\n");
    rm_rf("test/tmp");

    CHECK(store_open(&st, ROOT, K, M, SHARD_SIZE) == 0, "store_open failed");
    if (st == NULL) {
        printf("test_store: cannot continue\n");
        return 1;
    }

    test_bad_keys(st);
    test_sizes(st);
    test_layout(st);
    test_failures(st);
    test_api(st);

    store_close(st);

    if (failures) {
        printf("test_store: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_store: ok\n");
    return 0;
}
