#include "store.h"
#include <stdio.h>
#include <string.h>

/**
 * 自写测试：写入全 'L' 的 objzzy（10000B / 3 块），再尝试读 obj1 并比对；
 * 覆盖写部分已注释掉。
 */
int main(void) {
    Store s;
    if (store_open(&s, "/dev/sde", "meta_rocksdb") != 0) { puts("open failed"); return 1; }
    uint64_t base = s.alloc.free_cnt;
    printf("start        free=%lu\n", base);

    char a[10000]; memset(a, 'L', sizeof a);          // 10000 字节 -> 3 个块
    store_write(&s, "objzzy", a, sizeof a);
    printf("write 10000B free=%lu (期望比起始少 3)\n", s.alloc.free_cnt);

    char out[20000]; size_t n;
    store_read(&s, "obj1", out, sizeof out, &n);
    printf("read         %zu bytes, match=%d\n", n, n == sizeof a && memcmp(out, a, n) == 0);

 //   char b[5000]; memset(b, 'B', sizeof b);           // 5000 字节 -> 2 个块
   // store_write(&s, "obj1", b, sizeof b);             // 覆盖写

    store_close(&s);
    return 0;
}

