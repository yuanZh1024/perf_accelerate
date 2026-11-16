// test_csum.c
#include "store.h"
#include "crc32c.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("crc32c(\"123456789\") = 0x%08X (期望 0xE3069283)\n", crc32c("123456789", 9));

    Store s;
    if (store_open(&s, "/dev/sde", "meta_rocksdb") != 0) { puts("open failed"); return 1; }

    char a[10000]; memset(a, 'A', sizeof a);              // 3 个块
    store_write(&s, "obj2", a, sizeof a);

    char out[20000]; size_t n;
    printf("正常读: rc=%d (期望 0)\n", store_read(&s, "obj2", out, sizeof out, &n));

    // 绕过 store 和元数据，直接改盘上第 2 块，模拟静默损坏
    Onode o;
    meta_get(&s.meta, "obj2", &o);
    char junk[BLOCK_SIZE]; memset(junk, 'X', sizeof junk);
    disk_write_block(&s.disk, o.start + 1, junk, BLOCK_SIZE);
    onode_free(&o);

    printf("损坏后读: rc=%d (期望 -3)\n", store_read(&s, "obj2", out, sizeof out, &n));

    store_delete(&s, "obj2");
    store_close(&s);
    return 0;
}
