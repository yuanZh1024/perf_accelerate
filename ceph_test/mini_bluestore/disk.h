#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#include <stddef.h>   /* size_t */
#define BLOCK_SIZE 4096
typedef struct {
    int fd;
    void *buf;   // 对齐的缓冲区（O_DIRECT 要求内存地址也对齐）
} Disk;
int  disk_open(Disk *d, const char *path);
int  disk_write_block(Disk *d, uint64_t n, const void *data, size_t len);
int  disk_read_block(Disk *d, uint64_t n, void *out);   // out 至少 BLOCK_SIZE
void disk_close(Disk *d);
#endif
