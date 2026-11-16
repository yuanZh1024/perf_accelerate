// dump_meta.c —— 查看 RocksDB 账本里 onode 的原始二进制，并与盘上 crc 对照
#include <rocksdb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "disk.h"
#include "crc32c.h"

/**
 * 以 16 字节一行的格式打印二进制数据的 hex + ascii 对照。
 * @param buf  待显示数据的起始地址。
 * @param len  待显示的字节数。
 * @return     无；结果直接打印到 stdout，不足 16 字节的行用空格补齐。
 */
static void hexdump(const void *buf, size_t len) {
    const uint8_t *p = buf;
    for (size_t off = 0; off < len; off += 16) {
        printf("  %04zx: ", off);
        for (size_t i = 0; i < 16; i++) {
            if (off + i < len) printf("%02x ", p[off + i]);
            else               printf("   ");
            if (i == 7) printf(" ");
        }
        printf(" |");
        for (size_t i = 0; i < 16 && off + i < len; i++) {
            uint8_t ch = p[off + i];
            putchar(ch >= 32 && ch < 127 ? ch : '.');
        }
        printf("|\n");
    }
}

// 用法: sudo ./dump_meta [db目录] [对象名] [设备]
/**
 * 查看 RocksDB 账本中某个 onode 的原始 value，并逐块重算 crc 与盘上数据对照。
 * 以只读方式打开数据库，不会污染账本。
 * @param argc  命令行参数个数（含程序名本身）。
 * @param argv  参数数组：
 *              argv[1]  RocksDB 目录，默认 "meta_rocksdb"；
 *              argv[2]  对象名，默认 "trace"；
 *              argv[3]  块设备路径，默认 "/dev/sde"。
 * @return      0 成功；1 打开数据库、读取对象或打开块设备失败。
 */
int main(int argc, char **argv) {
    const char *dbpath = argc > 1 ? argv[1] : "meta_rocksdb";
    const char *name   = argc > 2 ? argv[2] : "trace";
    const char *dev    = argc > 3 ? argv[3] : "/dev/sde";

    // 只读打开，不污染账本
    char *err = NULL;
    rocksdb_options_t *opts = rocksdb_options_create();
    rocksdb_t *db = rocksdb_open_for_read_only(opts, dbpath, 0, &err);
    if (err) { printf("open error: %s\n", err); free(err); return 1; }
    rocksdb_readoptions_t *ro = rocksdb_readoptions_create();

    size_t len = 0;
    char *val = rocksdb_get(db, ro, name, strlen(name), &len, &err);
    if (err) { printf("get error: %s\n", err); free(err); return 1; }
    if (!val) { printf("对象 '%s' 不存在\n", name); return 1; }

    printf("========== 账本里的原始记录 ==========\n");
    printf("key   = \"%s\" (%zu 字节)\n", name, strlen(name));
    printf("value = %zu 字节  [24B 头 + %zuB crc]\n\n", len, len - 24);
    hexdump(val, len);

    // 解析 24 字节头: start, nblocks, size
    uint64_t h[3];
    memcpy(h, val, 24);
    printf("\n========== 解析后的 onode ==========\n");
    printf("start   = %lu   (数据起始块号)\n", h[0]);
    printf("nblocks = %lu   (占用块数)\n", h[1]);
    printf("size    = %lu   (对象实际字节数)\n", h[2]);

    if (len != 24 + h[1] * 4) {
        printf("value 长度不符，crc 数组解析跳过\n");
    } else {
        const uint32_t *crcs = (const uint32_t *)(val + 24);
        printf("\ncrc 数组（每个块一个，存于账本）:\n");
        for (uint64_t i = 0; i < h[1]; i++)
            printf("  crcs[%lu] = 0x%08X\n", i, (unsigned)crcs[i]);

        // 重算盘上每个块的 crc 做对照
        printf("\n========== 与盘上数据对照 ==========\n");
        Disk d;
        if (disk_open(&d, dev) != 0) { printf("无法打开 %s\n", dev); return 1; }
        char blk[BLOCK_SIZE];
        for (uint64_t i = 0; i < h[1]; i++) {
            if (disk_read_block(&d, h[0] + i, blk) != 0) {
                printf("  块 %lu 读取失败\n", h[0] + i);
                continue;
            }
            uint32_t calc = crc32c(blk, BLOCK_SIZE);
            printf("  块 %-3lu 盘上重算 0x%08X  账本 0x%08X  %s\n",
                   h[0] + i, (unsigned)calc, (unsigned)crcs[i],
                   calc == crcs[i] ? "MATCH" : "MISMATCH");
        }
        disk_close(&d);
    }

    free(val);
    rocksdb_readoptions_destroy(ro);
    rocksdb_close(db);
    rocksdb_options_destroy(opts);
    return 0;
}
