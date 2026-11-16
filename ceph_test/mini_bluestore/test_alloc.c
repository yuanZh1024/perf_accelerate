#include "alloc.h"
#include <stdio.h>

int main(void) {
    Allocator a;
    alloc_init(&a, 524288);          // 2GB / 4KB

    alloc_mark_used(&a, 0, 1);       // 块 0 预留（以后放超级块）

    int64_t x = alloc_alloc(&a, 4);  printf("alloc 4 -> %ld\n", x);   // 期望 1
    int64_t y = alloc_alloc(&a, 8);  printf("alloc 8 -> %ld\n", y);   // 期望 5
    alloc_free(&a, x, 4);            // 释放 1..4
    int64_t z = alloc_alloc(&a, 2);  printf("alloc 2 -> %ld\n", z);   // 期望 1（复用空洞）
    int64_t w = alloc_alloc(&a, 4);  printf("alloc 4 -> %ld\n", w);   // 期望 13（空洞只剩 2 块，放不下）

    printf("free blocks: %lu\n", a.free_cnt);
    alloc_destroy(&a);
    return 0;
}
