#define _GNU_SOURCE
#include "disk.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int disk_open(Disk *d, const char *path) {
    // O_DIRECT: 绕过页缓存；O_SYNC: 写完才返回
    d->fd = open(path, O_RDWR | O_DIRECT | O_SYNC);
    if (d->fd < 0) return -1;
    // 缓冲区地址必须按块对齐
    if (posix_memalign(&d->buf, BLOCK_SIZE, BLOCK_SIZE) != 0) return -1;
    return 0;
}
int disk_write_block(Disk *d, uint64_t n, const void *data, size_t len) {
    if (len > BLOCK_SIZE) return -1;
    memset(d->buf, 0, BLOCK_SIZE);          // 不足一块的部分补零
    memcpy(d->buf, data, len);
    ssize_t r = pwrite(d->fd, d->buf, BLOCK_SIZE, (off_t)n * BLOCK_SIZE);
    return r == BLOCK_SIZE ? 0 : -1;
}
int disk_read_block(Disk *d, uint64_t n, void *out) {
    ssize_t r = pread(d->fd, d->buf, BLOCK_SIZE, (off_t)n * BLOCK_SIZE);
    if (r != BLOCK_SIZE) return -1;
    memcpy(out, d->buf, BLOCK_SIZE);
    return 0;
}
void disk_close(Disk *d) {
    free(d->buf);
    close(d->fd);
}
