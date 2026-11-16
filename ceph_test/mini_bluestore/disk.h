#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#include <stddef.h>   /* size_t */
#define BLOCK_SIZE 4096
typedef struct {
    int fd;
    void *buf;   // 对齐的缓冲区（O_DIRECT 要求内存地址也对齐）
} Disk;
/**
 * 打开块设备并初始化 Disk（O_DIRECT + O_SYNC，绕过页缓存、写完即落盘）。
 * @param d     输出参数，Disk 结构体；函数内填入设备描述符 fd 与 4096 对齐的缓冲区 buf。
 * @param path  块设备路径，例如 "/dev/sde"。
 * @return      0 成功；-1 打开设备失败或对齐缓冲区分配失败。
 */
int  disk_open(Disk *d, const char *path);
/**
 * 把一段数据写入第 n 个 4KB 块；不足一块的部分补零。
 * @param d     已通过 disk_open 打开的 Disk。
 * @param n     目标块号（从 0 开始），实际写偏移 = n * BLOCK_SIZE。
 * @param data  源数据内存地址。
 * @param len   源数据字节数，必须 <= BLOCK_SIZE(4096)。
 * @return      0 成功（整块写入完成）；-1 参数非法或写入失败。
 */
int  disk_write_block(Disk *d, uint64_t n, const void *data, size_t len);
/**
 * 读取第 n 个 4KB 块的内容到调用方缓冲区（绕过页缓存，读的是盘上真实字节）。
 * @param d     已通过 disk_open 打开的 Disk。
 * @param n     源块号（从 0 开始）。
 * @param out   输出缓冲区，容量必须至少为 BLOCK_SIZE(4096) 字节。
 * @return      0 成功；-1 读取失败。
 */
int  disk_read_block(Disk *d, uint64_t n, void *out);   // out 至少 BLOCK_SIZE
/**
 * 释放对齐缓冲区并关闭块设备描述符。
 * @param d  已通过 disk_open 打开的 Disk。
 */
void disk_close(Disk *d);
#endif
