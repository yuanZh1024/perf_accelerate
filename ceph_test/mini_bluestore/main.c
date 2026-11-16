#include "disk.h"
#include <stdio.h>
#include <string.h>
/**
 * 第 1 步演示：向 /dev/sde 第 10 块写入 "hello bluestore" 再读回打印，
 * 验证 O_DIRECT 裸块读通路（屏显应为 hello bluestore）。
 */
int main(void) {
    Disk d;
    if (disk_open(&d, "/dev/sde") != 0) { perror("open"); return 1; }
    const char *msg = "hello bluestore";
    disk_write_block(&d, 10, msg, strlen(msg));
    char out[BLOCK_SIZE];
    disk_read_block(&d, 10, out);
    printf("%.15s\n", out);
    disk_close(&d);
    return 0;
}
