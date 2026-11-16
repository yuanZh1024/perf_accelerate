// store.h
#ifndef STORE_H
#define STORE_H
#include <stddef.h>
#include "disk.h"
#include "alloc.h"
#include "meta.h"

typedef struct { Disk disk; Allocator alloc; Meta meta; } Store;

/**
 * 打开整个存储栈：裸盘 + 分配器 + 账本，并用账本重建空闲位图。
 * @param s          输出参数，Store 结构体（包含 disk/alloc/meta 三层）。
 * @param dev        块设备路径，例如 "/dev/sde"。
 * @param meta_path  RocksDB 账本目录，例如 "meta_rocksdb"。
 * @return           0 成功；-1 任一层初始化失败。
 */
int  store_open(Store *s, const char *dev, const char *meta_path);
/**
 * 写入/覆盖对象，写时复制(CoW)：分配新块 -> 写数据+crc -> 提交 onode -> 释放旧块。
 * @param s     存储句柄。
 * @param name  对象名。
 * @param data  源数据地址。
 * @param len   数据长度，占用 ceil(len/4096) 个块；0 视为非法。
 * @return      0 成功；-1 失败（新块回滚释放，旧版本保持完好）。
 */
int  store_write(Store *s, const char *name, const void *data, size_t len);
/**
 * 读取对象，并逐块用 onode 中的 crc32c 校验，发现损坏立即报错。
 * @param s       存储句柄。
 * @param name    对象名。
 * @param out     输出缓冲区。
 * @param cap     输出缓冲区容量；小于对象 size 时返回 -2。
 * @param outlen  输出参数，成功时写入实际读到的字节数（=对象 size）。
 * @return        0 成功；1 不存在；-2 缓冲区太小；-3 校验失败；-1 其他错误。
 */
int  store_read(Store *s, const char *name, void *out, size_t cap, size_t *outlen);
/**
 * 删除对象：先从账本原子删除记录（提交点），成功后才释放数据块。
 * @param s     存储句柄。
 * @param name  对象名。
 * @return      0 成功；-1 对象不存在或删除失败。
 */
int  store_delete(Store *s, const char *name);
/**
 * 按逆序关闭 meta / alloc / disk 三层。
 * @param s  存储句柄。
 */
void store_close(Store *s);
#endif
