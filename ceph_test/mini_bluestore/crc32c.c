// crc32c.c  —— 纯软件查表实现，不依赖外部库
#include "crc32c.h"

static uint32_t table[256];
static int inited;

static void init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : (c >> 1);
        table[i] = c;
    }
    inited = 1;
}

uint32_t crc32c(const void *buf, size_t len) {
    if (!inited) init_table();
    const uint8_t *p = buf;
    uint32_t c = ~0u;
    for (size_t i = 0; i < len; i++)
        c = table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return ~c;
}
