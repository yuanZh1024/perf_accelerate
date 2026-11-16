#include "store.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    Store s;
    if (store_open(&s, "/dev/sde", "meta_rocksdb") != 0) { puts("open failed"); return 1; }
    uint64_t base = s.alloc.free_cnt;
    printf("start        free=%lu\n", base);

    char a[10000]; memset(a, 'A', sizeof a);          // 10000 字节 -> 3 个块
    store_write(&s, "obj1", a, sizeof a);
    printf("write 10000B free=%lu (期望比起始少 3)\n", s.alloc.free_cnt);

    char out[20000]; size_t n;
    store_read(&s, "obj1", out, sizeof out, &n);
    printf("read         %zu bytes, match=%d\n", n, n == sizeof a && memcmp(out, a, n) == 0);

    char b[5000]; memset(b, 'B', sizeof b);           // 5000 字节 -> 2 个块
    store_write(&s, "obj1", b, sizeof b);             // 覆盖写
    printf("overwrite 5000B free=%lu (期望比起始少 2)\n", s.alloc.free_cnt);
    store_read(&s, "obj1", out, sizeof out, &n);
    printf("read         %zu bytes, match=%d\n", n, n == sizeof b && memcmp(out, b, n) == 0);

    store_delete(&s, "obj1");
    printf("delete       free=%lu (期望回到 %lu)\n", s.alloc.free_cnt, base);

    store_close(&s);
    return 0;
}
