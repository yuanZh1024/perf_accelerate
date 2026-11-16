#include "meta.h"
#include <stdio.h>

int main(void) {
    Meta m;
    meta_open(&m, "meta_rocksdb");

    Onode o = { .start = 1, .nblocks = 4, .size = 15000 };
    meta_put(&m, "obj1", &o);

    Onode r;
    int rc = meta_get(&m, "obj1", &r);
    printf("get obj1: rc=%d start=%lu nblocks=%lu size=%lu\n", rc, r.start, r.nblocks, r.size);
    printf("get nope: rc=%d\n", meta_get(&m, "nope", &r));        // 期望 1

    // 模拟"重启"：新建一个空分配器，只靠元数据把它恢复
    Allocator a;
    alloc_init(&a, 524288);
    alloc_mark_used(&a, 0, 1);               // 块 0 预留
    meta_rebuild_alloc(&m, &a);
    printf("free blocks: %lu\n", a.free_cnt); // 期望 524288 - 1 - 4 = 524283

    alloc_destroy(&a);
    meta_close(&m);
    return 0;
}
