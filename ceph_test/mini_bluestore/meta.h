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

/**
 * 打开（不存在则创建）RocksDB 账本，并启用同步写：put 返回即 WAL 落盘。
 * @param m     输出参数，Meta 句柄（db/options/writeoptions/readoptions）。
 * @param path  数据库目录路径，例如 "meta_rocksdb"。
 * @return      0 成功；-1 打开失败。
 */
int  meta_open(Meta *m, const char *path);
/**
 * 新增或覆盖一条 onode 记录；这是写入流程的提交点。
 * value 格式：24 字节头(start,nblocks,size) + nblocks*4 字节 crcs。
 * @param m     账本句柄。
 * @param name  对象名，作为 RocksDB 的 key。
 * @param o     要写入的 onode（其 crcs 数组须有 nblocks 个元素）。
 * @return      0 成功；-1 内存分配或写入失败。
 */
int  meta_put(Meta *m, const char *name, const Onode *o);
/**
 * 按对象名查询 onode；成功时 o->crcs 是 malloc 出来的数组，调用方必须调用 onode_free。
 * @param m     账本句柄。
 * @param name  对象名。
 * @param o     输出参数，查到的 onode。
 * @return      0 找到；1 不存在；-1 出错或 value 长度与 nblocks 不符。
 */
int  meta_get(Meta *m, const char *name, Onode *o);   // 0 找到, 1 不存在, -1 出错；成功后要 onode_free
/**
 * 释放 meta_get 填充的 crcs 数组，并把指针置 NULL。
 * @param o  由 meta_get 成功填充的 onode。
 */
void onode_free(Onode *o);
/**
 * 删除一条 onode 记录（同步写）。
 * @param m     账本句柄。
 * @param name  要删除的对象名。
 * @return      0 成功；-1 失败。
 */
int  meta_delete(Meta *m, const char *name);
/**
 * 遍历账本所有 onode，按其 extent 重建内存位图——证明"位图不是真相，账本才是"。
 * @param m  账本句柄。
 * @param a  输出参数，目标分配器（应已初始化，通常为空位图）。
 * @return    0 成功。
 */
int  meta_rebuild_alloc(Meta *m, Allocator *a);
/**
 * 关闭数据库并按逆序释放 readoptions / writeoptions / options。
 * @param m  账本句柄。
 */
void meta_close(Meta *m);
#endif
