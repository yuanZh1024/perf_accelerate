// meta.h（Onode 增加 crcs，其余声明不变）
#ifndef META_H
#define META_H
#include <stdint.h>
#include <rocksdb/c.h>
#include "alloc.h"

typedef struct {
    rocksdb_t *db;
    rocksdb_options_t *opts;
    rocksdb_writeoptions_t *wo;
    rocksdb_readoptions_t *ro;
} Meta;

typedef struct {
    uint64_t start, nblocks, size;
    uint32_t *crcs;                  // nblocks 个，每块一个 crc32c
} Onode;

int  meta_open(Meta *m, const char *path);
int  meta_put(Meta *m, const char *name, const Onode *o);
int  meta_get(Meta *m, const char *name, Onode *o);   // 0 找到, 1 不存在, -1 出错；成功后要 onode_free
void onode_free(Onode *o);
int  meta_delete(Meta *m, const char *name);
int  meta_rebuild_alloc(Meta *m, Allocator *a);
void meta_close(Meta *m);
#endif
