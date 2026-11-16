// store.c
#include "store.h"
#include "crc32c.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/**
 * 打开存储栈：算磁盘总块数、初始化位图、预留块 0、开账本并重建位图。
 * @param s          输出，Store 句柄。
 * @param dev        块设备路径。
 * @param meta_path  RocksDB 目录。
 * @return           0 成功；-1 任一步失败。
 */
int store_open(Store *s, const char *dev, const char *meta_path) {
    if (disk_open(&s->disk, dev) != 0) return -1;
    uint64_t total = lseek(s->disk.fd, 0, SEEK_END) / BLOCK_SIZE;
    if (alloc_init(&s->alloc, total) != 0) return -1;
    alloc_mark_used(&s->alloc, 0, 1);
    if (meta_open(&s->meta, meta_path) != 0) return -1;
    return meta_rebuild_alloc(&s->meta, &s->alloc);
}

/**
 * 写对象（CoW）：分配 -> 补零算crc写盘 -> 提交onode -> 释放旧块。
 * @param s     存储句柄。
 * @param name  对象名。
 * @param data  源数据。
 * @param len   字节数，决定占用块数。
 * @return      0 成功；-1 失败（失败时回滚新分配，旧对象不变）。
 */
int store_write(Store *s, const char *name, const void *data, size_t len) {
    if (len == 0) return -1;
    uint64_t nblocks = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;

    Onode old = {0};
    int had_old = (meta_get(&s->meta, name, &old) == 0);

    int64_t start = alloc_alloc(&s->alloc, nblocks);
    if (start < 0) { if (had_old) onode_free(&old); return -1; }

    uint32_t *crcs = malloc(nblocks * sizeof(uint32_t));
    char blk[BLOCK_SIZE];
    const char *p = data;
    int ret = -1;

    // 2. 补零成整块 -> 算 crc -> 写到新位置
    for (uint64_t i = 0; i < nblocks; i++) {
        size_t chunk = len - i * BLOCK_SIZE;
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        memset(blk, 0, BLOCK_SIZE);
        memcpy(blk, p + i * BLOCK_SIZE, chunk);
        crcs[i] = crc32c(blk, BLOCK_SIZE);
        if (disk_write_block(&s->disk, start + i, blk, BLOCK_SIZE) != 0) goto out;
    }

    // 3. 提交元数据（含所有块的 crc）—— 提交点
    Onode n = { .start = start, .nblocks = nblocks, .size = len, .crcs = crcs };
    if (meta_put(&s->meta, name, &n) != 0) goto out;

    // 4. 提交之后才释放旧空间
    if (had_old) alloc_free(&s->alloc, old.start, old.nblocks);
    ret = 0;
out:
    if (ret != 0) alloc_free(&s->alloc, start, nblocks);
    free(crcs);
    if (had_old) onode_free(&old);
    return ret;
}

// 返回：0 成功, 1 不存在, -2 缓冲区太小, -3 校验失败, -1 其他错误
/**
 * 读对象并逐块校验 crc32c；失败时返回错误码而不返回坏数据。
 * @param s       存储句柄。
 * @param name    对象名。
 * @param out     输出缓冲区。
 * @param cap     缓冲区容量。
 * @param outlen  输出，实际字节数。
 * @return        0 成功；1 不存在；-2 缓冲区太小；-3 校验失败；-1 其他错误。
 */
int store_read(Store *s, const char *name, void *out, size_t cap, size_t *outlen) {
    Onode o;
    int rc = meta_get(&s->meta, name, &o);
    if (rc != 0) return rc;
    if (o.size > cap) { onode_free(&o); return -2; }

    char blk[BLOCK_SIZE];
    size_t remain = o.size;
    for (uint64_t i = 0; i < o.nblocks; i++) {
        if (disk_read_block(&s->disk, o.start + i, blk) != 0) { onode_free(&o); return -1; }
        if (crc32c(blk, BLOCK_SIZE) != o.crcs[i]) {          // 校验
            fprintf(stderr, "checksum mismatch: %s block #%lu (disk block %lu)\n",
                    name, i, o.start + i);
            onode_free(&o);
            return -3;
        }
        size_t n = remain < BLOCK_SIZE ? remain : BLOCK_SIZE;
        memcpy((char *)out + i * BLOCK_SIZE, blk, n);
        remain -= n;
    }
    *outlen = o.size;
    onode_free(&o);
    return 0;
}

/**
 * 删除对象：先提交删除，再释放块。
 * @param s     存储句柄。
 * @param name  对象名。
 * @return      0 成功；-1 对象不存在或删除失败。
 */
int store_delete(Store *s, const char *name) {
    Onode o;
    if (meta_get(&s->meta, name, &o) != 0) return -1;
    int rc = meta_delete(&s->meta, name);
    if (rc == 0) alloc_free(&s->alloc, o.start, o.nblocks);
    onode_free(&o);
    return rc;
}

/** 逆序释放 meta / alloc / disk。@param s 存储句柄。 */
void store_close(Store *s) {
    meta_close(&s->meta);
    alloc_destroy(&s->alloc);
    disk_close(&s->disk);
}
