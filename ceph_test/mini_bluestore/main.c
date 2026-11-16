#include "disk.h"
#include <stdio.h>
#include <string.h>
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
