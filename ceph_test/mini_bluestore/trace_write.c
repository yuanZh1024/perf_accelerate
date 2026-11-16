// trace_write.c —— 带插桩的写入流程演示
// 把 store_write 的 4 个阶段拆开，每步之后：dump 相关块的裸字节 + 查 RocksDB 账本。
#include "store.h"
#include "crc32c.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 打印某个块的前 32 字节（hex + ascii）
/**
 * 打印某个块前 32 字节的 hex 与 ascii 对照，用于观察裸盘字节。
 * @param d  已打开的 Disk。
 * @param n  要显示的块号。
 */
static void dump_block(Disk *d, uint64_t n) {
    char blk[BLOCK_SIZE];
    if (disk_read_block(d, n, blk) != 0) { printf("    <read error>\n"); return; }
    printf("    块 %-3lu ", n);
    for (int i = 0; i < 32; i++) printf("%02x", (unsigned char)blk[i]);
    printf("  |");
    for (int i = 0; i < 16; i++) putchar(blk[i] >= 32 && blk[i] < 127 ? blk[i] : '.');
    printf("|\n");
}

// 查账本里某个对象
/**
 * 查询并打印账本中某对象的 onode（存在/不存在两种状态）。
 * @param m     账本句柄。
 * @param name  对象名。
 */
static void show_meta(Meta *m, const char *name) {
    Onode o;
    int rc = meta_get(m, name, &o);
    if (rc == 1) {
        printf("    账本: 无 '%s' 记录  <-- 对象不存在\n", name);
    } else if (rc == 0) {
        printf("    账本: %s  start=%lu nblocks=%lu size=%lu  <-- 对象存在\n",
               name, o.start, o.nblocks, o.size);
        onode_free(&o);
    } else {
        printf("    账本: 读取出错 rc=%d\n", rc);
    }
}

/**
 * 写入流程插桩演示：拆开 store_write 的 4 个阶段，每步后 dump 裸块并查账本，
 * 直观展示"数据先落盘、元数据后提交"与写时复制（先写旧版本 trace，再覆盖新版本）。
 */
int main(void) {
    Store s;
    if (store_open(&s, "/dev/sde", "meta_rocksdb") != 0) { puts("open failed"); return 1; }
    uint64_t base = s.alloc.free_cnt;

    printf("========== 初始状态 ==========\n");
    printf("free=%lu（块0预留，账本为空）\n\n", base);

    // ---- 预备：先写一个旧版本，好让步骤4「释放旧空间」有东西可演示 ----
    char old_data[3000];
    memset(old_data, 'Z', sizeof old_data);
    store_write(&s, "trace", old_data, sizeof old_data);   // 3000B -> 1 块
    Onode old = {0};
    meta_get(&s.meta, "trace", &old);
    printf("========== 预备：旧版本 trace = 3000B 全'Z'，占块 %lu ==========\n", old.start);
    printf("  账本: start=%lu nblocks=%lu size=%lu\n", old.start, old.nblocks, old.size);
    printf("  free=%lu\n", s.alloc.free_cnt);
    printf("  盘上旧块 %lu:\n", old.start);
    dump_block(&s.disk, old.start);
    printf("\n");

    // ---- 新版本：10000B = 3 块，内容按块区分 A / B / C ----
    char data[10000];
    memset(data,      'A', 4096);
    memset(data+4096, 'B', 4096);
    memset(data+8192, 'C', 10000 - 8192);
    uint64_t nblocks = 3;

    printf("########## 开始写入新版本 trace = 10000B（3块，内容 A/B/C）##########\n\n");

    // 【步骤1】分配：只改内存位图，盘和账本都不动
    int64_t start = alloc_alloc(&s.alloc, nblocks);
    printf("【步骤1 分配】新位置 = 块 %ld~%ld，free=%lu\n",
           start, start + nblocks - 1, s.alloc.free_cnt);
    printf("  盘上这3块此刻的内容（旧残留，还没被本次写入碰过）：\n");
    for (uint64_t i = 0; i < nblocks; i++) dump_block(&s.disk, start + i);
    show_meta(&s.meta, "trace");
    printf("  ↑ 位图已在内存标记这3块占用，但盘上字节没变、账本还指向旧版本\n\n");

    // 【步骤2】写数据：补零成整块 -> 算crc -> O_SYNC 落到新位置
    uint32_t crcs[3];
    char blk[BLOCK_SIZE];
    const char *p = data;
    for (uint64_t i = 0; i < nblocks; i++) {
        size_t chunk = 10000 - i * BLOCK_SIZE;
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        memset(blk, 0, BLOCK_SIZE);
        memcpy(blk, p + i * BLOCK_SIZE, chunk);
        crcs[i] = crc32c(blk, BLOCK_SIZE);
        if (disk_write_block(&s.disk, start + i, blk, BLOCK_SIZE) != 0) {
            puts("write failed"); return 1;
        }
    }
    printf("【步骤2 写数据】新数据已 O_SYNC 落到块 %ld~%ld\n", start, start + nblocks - 1);
    printf("  盘上这3块现在的内容：\n");
    for (uint64_t i = 0; i < nblocks; i++) dump_block(&s.disk, start + i);
    show_meta(&s.meta, "trace");
    printf("  ↑ 关键点：数据已经在盘上了，但账本仍指向旧版本！\n");
    printf("    此时若断电 -> 新块无人认领(重启重建位图后算空闲)，旧版本完好 -> 对象=旧版本\n\n");

    // 【步骤3】提交元数据 —— 提交点
    Onode n = { .start = (uint64_t)start, .nblocks = nblocks, .size = 10000, .crcs = crcs };
    if (meta_put(&s.meta, "trace", &n) != 0) { puts("commit failed"); return 1; }
    printf("【步骤3 提交元数据】提交点！RocksDB 同步写 onode（含每块 crc）\n");
    show_meta(&s.meta, "trace");
    printf("  盘上内容不变（数据早在步骤2就写好了）：\n");
    for (uint64_t i = 0; i < nblocks; i++) dump_block(&s.disk, start + i);
    printf("  ↑ 从这一刻起，对象才『存在』。账本指向新位置，且原子替换了旧记录\n\n");

    // 【步骤4】提交后才释放旧空间
    alloc_free(&s.alloc, old.start, old.nblocks);
    printf("【步骤4 释放旧块】释放块 %lu，free=%lu\n", old.start, s.alloc.free_cnt);
    printf("  旧块 %lu 的内容还躺在盘上（没人擦），但账上已无人认领：\n", old.start);
    dump_block(&s.disk, old.start);
    printf("  ↑ 这就是 CoW：旧版本在提交前始终完好；提交后旧块变孤儿，下次分配会覆盖它\n\n");

    // ---- 验证 ----
    char out[20000]; size_t got;
    int rc = store_read(&s, "trace", out, sizeof out, &got);
    printf("========== 验证 store_read ==========\n");
    printf("rc=%d, len=%zu, match=%d\n", rc, got,
           (got == 10000 && memcmp(out, data, 10000) == 0));

    onode_free(&old);
    store_close(&s);
    return 0;
}
