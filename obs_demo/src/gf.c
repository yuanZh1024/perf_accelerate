/*
 * gf.c - log/exp table implementation of GF(2^8).
 *
 * 256 element field, elements are bytes, addition is XOR.
 * Multiplication is done through a 255-entry log table and a doubled
 * exp table so that the sum of two logarithms can be used directly as
 * an index without a modulo operation.
 */
#include "gf.h"

#include <string.h>

#define GF_POLY 0x11d

static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int gf_ready = 0;

void gf_init(void)
{
    unsigned x = 1;
    int i;

    if (gf_ready)
        return;

    for (i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100)
            x ^= GF_POLY;
    }
    /* Duplicate the table: exp[i+255] == exp[i] */
    for (i = 255; i < 512; i++)
        gf_exp[i] = gf_exp[i - 255];

    gf_log[0] = 0; /* log(0) is undefined, never used */
    gf_ready = 1;
}

uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0)
        return 0;
    return gf_exp[(unsigned)gf_log[a] + (unsigned)gf_log[b]];
}

uint8_t gf_div(uint8_t a, uint8_t b)
{
    if (a == 0)
        return 0;
    /* log(a) - log(b) + 255 keeps the index inside [0, 509] */
    return gf_exp[(unsigned)gf_log[a] + 255u - (unsigned)gf_log[b]];
}

uint8_t gf_inv(uint8_t a)
{
    if (a == 0)
        return 0;
    return gf_exp[255u - (unsigned)gf_log[a]];
}

uint8_t gf_pow(uint8_t a, unsigned n)
{
    if (n == 0)
        return 1;
    if (a == 0)
        return 0;
    return gf_exp[((unsigned)gf_log[a] * (n % 255u)) % 255u];
}
