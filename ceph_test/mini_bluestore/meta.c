// meta.c（完整替换）
#include "meta.h"
#include <stdlib.h>
#include <string.h>

#define HDR (3 * sizeof(uint64_t))

int meta_open(Meta *m, const char *path) {
    char *err = NULL;
    m->opts = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(m->opts, 1);
    m->db = rocksdb_open(m->opts, path, &err);
    if (err) { free(err); return -1; }
    m->wo = rocksdb_writeoptions_create();
    rocksdb_writeoptions_set_sync(m->wo, 1);
    m->ro = rocksdb_readoptions_create();
    return 0;
}

int meta_put(Meta *m, const char *name, const Onode *o) {
    size_t len = HDR + o->nblocks * sizeof(uint32_t);
    char *buf = calloc(1, len);
    if (!buf) return -1;
    uint64_t h[3] = { o->start, o->nblocks, o->size };
    memcpy(buf, h, HDR);
    if (o->crcs) memcpy(buf + HDR, o->crcs, o->nblocks * sizeof(uint32_t));

    char *err = NULL;
    rocksdb_put(m->db, m->wo, name, strlen(name), buf, len, &err);
    free(buf);
    if (err) { free(err); return -1; }
    return 0;
}

int meta_get(Meta *m, const char *name, Onode *o) {
    char *err = NULL;
    size_t len;
    char *val = rocksdb_get(m->db, m->ro, name, strlen(name), &len, &err);
    if (err) { free(err); return -1; }
    if (!val) return 1;
    if (len < HDR) { free(val); return -1; }

    uint64_t h[3];
    memcpy(h, val, HDR);
    if (len != HDR + h[1] * sizeof(uint32_t)) { free(val); return -1; }

    o->start = h[0]; o->nblocks = h[1]; o->size = h[2];
    o->crcs = malloc(h[1] * sizeof(uint32_t));
    memcpy(o->crcs, val + HDR, h[1] * sizeof(uint32_t));
    free(val);
    return 0;
}

void onode_free(Onode *o) { free(o->crcs); o->crcs = NULL; }

int meta_delete(Meta *m, const char *name) {
    char *err = NULL;
    rocksdb_delete(m->db, m->wo, name, strlen(name), &err);
    if (err) { free(err); return -1; }
    return 0;
}

int meta_rebuild_alloc(Meta *m, Allocator *a) {
    rocksdb_iterator_t *it = rocksdb_create_iterator(m->db, m->ro);
    for (rocksdb_iter_seek_to_first(it); rocksdb_iter_valid(it); rocksdb_iter_next(it)) {
        size_t len;
        const char *val = rocksdb_iter_value(it, &len);
        if (len < HDR) continue;
        uint64_t h[3];
        memcpy(h, val, HDR);
        alloc_mark_used(a, h[0], h[1]);
    }
    rocksdb_iter_destroy(it);
    return 0;
}

void meta_close(Meta *m) {
    rocksdb_readoptions_destroy(m->ro);
    rocksdb_writeoptions_destroy(m->wo);
    rocksdb_close(m->db);
    rocksdb_options_destroy(m->opts);
}
