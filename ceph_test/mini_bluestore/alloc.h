#ifndef ALLOC_H
#define ALLOC_H
#include <stdint.h>

typedef struct {
    uint8_t *bits;       // 位图，1 = 已用
    uint64_t total;      // 总块数
    uint64_t free_cnt;   // 空闲块数
} Allocator;

int     alloc_init(Allocator *a, uint64_t total_blocks);
void    alloc_destroy(Allocator *a);
int64_t alloc_alloc(Allocator *a, uint64_t count);               // 成功返回起始块号，失败返回 -1
void    alloc_free(Allocator *a, uint64_t start, uint64_t count);
void    alloc_mark_used(Allocator *a, uint64_t start, uint64_t count);  // 预留/恢复时用
#endif
