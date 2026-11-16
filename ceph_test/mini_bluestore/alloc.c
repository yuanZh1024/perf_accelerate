#include "alloc.h"
#include <stdlib.h>
#include <string.h>

static int  get_bit(Allocator *a, uint64_t i) { return (a->bits[i / 8] >> (i % 8)) & 1; }
static void set_bit(Allocator *a, uint64_t i) { a->bits[i / 8] |=  (1 << (i % 8)); }
static void clr_bit(Allocator *a, uint64_t i) { a->bits[i / 8] &= ~(1 << (i % 8)); }

int alloc_init(Allocator *a, uint64_t total_blocks) {
    a->total = total_blocks;
    a->free_cnt = total_blocks;
    a->bits = calloc((total_blocks + 7) / 8, 1);   // 全 0 = 全空闲
    return a->bits ? 0 : -1;
}

void alloc_destroy(Allocator *a) { free(a->bits); }

void alloc_mark_used(Allocator *a, uint64_t start, uint64_t count) {
    for (uint64_t i = start; i < start + count; i++)
        if (!get_bit(a, i)) { set_bit(a, i); a->free_cnt--; }
}

void alloc_free(Allocator *a, uint64_t start, uint64_t count) {
    for (uint64_t i = start; i < start + count; i++)
        if (get_bit(a, i)) { clr_bit(a, i); a->free_cnt++; }
}

// first-fit：从头扫，找到第一段足够长的连续空闲块
int64_t alloc_alloc(Allocator *a, uint64_t count) {
    uint64_t run = 0;
    for (uint64_t i = 0; i < a->total; i++) {
        run = get_bit(a, i) ? 0 : run + 1;
        if (run == count) {
            uint64_t start = i + 1 - count;
            alloc_mark_used(a, start, count);
            return (int64_t)start;
        }
    }
    return -1;   // 没有足够长的连续空间
}
