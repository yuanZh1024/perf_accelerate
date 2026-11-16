#include "alloc.h"
#include <stdlib.h>
#include <string.h>

/* 读取位图第 i 位：@param a 分配器，@param i 块号，@return 0 空闲 / 1 已用。 */
static int  get_bit(Allocator *a, uint64_t i) { return (a->bits[i / 8] >> (i % 8)) & 1; }
/* 把位图第 i 位置 1（已用）：@param a 分配器，@param i 块号。 */
static void set_bit(Allocator *a, uint64_t i) { a->bits[i / 8] |=  (1 << (i % 8)); }
/* 把位图第 i 位清 0（空闲）：@param a 分配器，@param i 块号。 */
static void clr_bit(Allocator *a, uint64_t i) { a->bits[i / 8] &= ~(1 << (i % 8)); }

/**
 * 初始化位图分配器：全部块置为空闲。
 * @param a             输出参数，分配器。
 * @param total_blocks  总块数，决定位图大小 (total+7)/8 字节。
 * @return              0 成功；-1 calloc 失败。
 */
int alloc_init(Allocator *a, uint64_t total_blocks) {
    a->total = total_blocks;
    a->free_cnt = total_blocks;
    a->bits = calloc((total_blocks + 7) / 8, 1);   // 全 0 = 全空闲
    return a->bits ? 0 : -1;
}

/** 释放位图内存。@param a 已初始化的分配器。 */
void alloc_destroy(Allocator *a) { free(a->bits); }

/**
 * 把 [start, start+count) 标记为已用，同时递减 free_cnt。
 * @param a      分配器。
 * @param start  起始块号。
 * @param count  连续块数。
 */
void alloc_mark_used(Allocator *a, uint64_t start, uint64_t count) {
    for (uint64_t i = start; i < start + count; i++)
        if (!get_bit(a, i)) { set_bit(a, i); a->free_cnt--; }
}

/**
 * 把 [start, start+count) 标记为空闲，同时递增 free_cnt。
 * @param a      分配器。
 * @param start  起始块号。
 * @param count  连续块数。
 */
void alloc_free(Allocator *a, uint64_t start, uint64_t count) {
    for (uint64_t i = start; i < start + count; i++)
        if (get_bit(a, i)) { clr_bit(a, i); a->free_cnt++; }
}

// first-fit：从头扫，找到第一段足够长的连续空闲块
/**
 * first-fit 分配一段连续空闲块，并立即标记已用（只改内存位图，不碰盘和账本）。
 * @param a      分配器。
 * @param count  需要的连续块数。
 * @return       起始块号；找不到足够长的连续空间返回 -1。
 */
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
