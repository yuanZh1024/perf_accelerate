// meta.c（完整替换）
#include "meta.h"
#include <stdlib.h>
#include <string.h>

#define HDR (3 * sizeof(uint64_t))

/**
 * 打开或创建 RocksDB；writeoptions 设 sync=1，保证提交点语义。
 * @param m     输出，Meta 句柄。
 * @param path  数据库目录路径。
 * @return      0 成功；-1 打开出错。
 */
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

/**
 * 序列化 onode（头 + crcs）并同步写入；单条 put 自带事务，原子生效。
 * @param m     账本句柄。
 * @param name  对象名（key）。
 * @param o     要写入的 onode。
 * @return      0 成功；-1 分配或写入失败。
 */
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

/**
 * 读取并校验 value 长度，然后解包出 onode 与 crcs 数组。
 * @param m     账本句柄。
 * @param name  对象名。
 * @param o     输出，成功时含 malloc 的 crcs，需 onode_free。
 * @return      0 找到；1 不存在；-1 出错。
 */
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

/** 释放 onode 中的 crcs 数组并置空。@param o 由 meta_get 填充的 onode。 */
void onode_free(Onode *o) { free(o->crcs); o->crcs = NULL; }

/**
 * 同步删除对象记录。
 * @param m     账本句柄。
 * @param name  对象名。
 * @return      0 成功；-1 失败。
 */
int meta_delete(Meta *m, const char *name) {
    char *err = NULL;
    rocksdb_delete(m->db, m->wo, name, strlen(name), &err);
    if (err) { free(err); return -1; }
    return 0;
}

/**
 * 迭代所有 onode，用 h[0]=start、h[1]=nblocks 标记位图已用。
 * @param m  账本句柄。
 * @param a  输出，待重建的分配器。
 * @return    0 成功。
 */
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

/** 释放 RocksDB 资源。@param m 账本句柄。 */
void meta_close(Meta *m) {
    rocksdb_readoptions_destroy(m->ro);
    rocksdb_writeoptions_destroy(m->wo);
    rocksdb_close(m->db);
    rocksdb_options_destroy(m->opts);
}
