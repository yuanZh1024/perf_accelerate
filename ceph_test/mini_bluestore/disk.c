#define _GNU_SOURCE
#include "disk.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/**
 * 打开块设备并初始化 Disk（O_DIRECT + O_SYNC）。
 * @param d     输出参数，填入 fd 和 posix_memalign 分配的 4096 字节对齐缓冲区。
 * @param path  块设备路径，例如 "/dev/sde"。
 * @return      0 成功；-1 open 失败或 posix_memalign 失败。
 */
int disk_open(Disk *d, const char *path) {
    // O_DIRECT: 绕过页缓存；O_SYNC: 写完才返回
    d->fd = open(path, O_RDWR | O_DIRECT | O_SYNC);
    if (d->fd < 0) return -1;
    // 缓冲区地址必须按块对齐
    if (posix_memalign(&d->buf, BLOCK_SIZE, BLOCK_SIZE) != 0) return -1;
    return 0;
}
/**
 * 写第 n 块：先用 0 填满内部缓冲区，再拷贝 len 字节，最后整块 pwrite。
 * @param d     已打开的 Disk。
 * @param n     块号，写偏移 = (off_t)n * BLOCK_SIZE。
 * @param data  源数据地址。
 * @param len   写入字节数，超过 BLOCK_SIZE 直接返回 -1；不足则补零。
 * @return      0 成功；-1 参数非法或实际写入字节数不足一块。
 */
int disk_write_block(Disk *d, uint64_t n, const void *data, size_t len) {
    if (len > BLOCK_SIZE) return -1;
    memset(d->buf, 0, BLOCK_SIZE);          // 不足一块的部分补零
    memcpy(d->buf, data, len);
    ssize_t r = pwrite(d->fd, d->buf, BLOCK_SIZE, (off_t)n * BLOCK_SIZE);
    return r == BLOCK_SIZE ? 0 : -1;
}
/**
 * 读第 n 块到 out（内部用对齐缓冲区 pread，再 memcpy 给调用方）。
 * @param d     已打开的 Disk。
 * @param n     块号，读偏移 = (off_t)n * BLOCK_SIZE。
 * @param out   输出缓冲区，至少 BLOCK_SIZE 字节。
 * @return      0 成功；-1 读取字节数不足一块。
 */
int disk_read_block(Disk *d, uint64_t n, void *out) {
    ssize_t r = pread(d->fd, d->buf, BLOCK_SIZE, (off_t)n * BLOCK_SIZE);
    if (r != BLOCK_SIZE) return -1;
    memcpy(out, d->buf, BLOCK_SIZE);
    return 0;
}
/**
 * 关闭设备并释放对齐缓冲区。
 * @param d  已打开的 Disk。
 */
void disk_close(Disk *d) {
    free(d->buf);
    close(d->fd);
}
