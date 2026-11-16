/*
 * test_rs.c - exhaustive checks of the Reed-Solomon layer.
 *
 * The important property is that ANY k of the n shards rebuild the stripe,
 * so the test walks every k-subset for a couple of geometries.
 */
#include "../src/gf.h"
#include "../src/rs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                        \
    do {                                        \
        if (!(cond)) {                          \
            printf("FAIL %d: ", __LINE__);      \
            printf(__VA_ARGS__);                \
            printf("\n");                       \
            failures++;                         \
        }                                       \
    } while (0)

static unsigned rng_state = 0x12345678u;

static uint8_t rnd8(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (uint8_t)(rng_state & 0xff);
}

static void test_gf(void)
{
    int a, b;

    gf_init();
    for (a = 1; a < 256; a++) {
        CHECK(gf_mul((uint8_t)a, gf_inv((uint8_t)a)) == 1, "inv(%d) wrong", a);
        CHECK(gf_div((uint8_t)a, (uint8_t)a) == 1, "div(%d,%d) wrong", a, a);
    }
    for (a = 0; a < 256; a++) {
        for (b = 0; b < 256; b++) {
            if (gf_mul((uint8_t)a, (uint8_t)b) != gf_mul((uint8_t)b, (uint8_t)a)) {
                CHECK(0, "mul not commutative for %d,%d", a, b);
                return;
            }
        }
    }
    /* distributive law spot check */
    for (a = 0; a < 256; a += 7) {
        for (b = 0; b < 256; b += 11) {
            uint8_t c = (uint8_t)(b + 3);
            uint8_t lhs = gf_mul((uint8_t)a, (uint8_t)(b ^ c));
            uint8_t rhs = gf_mul((uint8_t)a, (uint8_t)b) ^ gf_mul((uint8_t)a, c);
            CHECK(lhs == rhs, "distributive law broken at %d", a);
        }
    }
}

/* Verify that every k-subset can rebuild the stripe. */
static void test_geometry(int k, int m)
{
    rs_t rs;
    const size_t len = 1000;
    uint8_t *orig[RS_MAX_N];
    uint8_t *work[RS_MAX_N];
    uint8_t present[RS_MAX_N];
    unsigned mask, full;
    int i, combos = 0;

    CHECK(rs_init(&rs, k, m) == 0, "rs_init(%d,%d) failed", k, m);
    if (rs.mat == NULL)
        return;

    /* systematic part must be the identity */
    for (i = 0; i < k; i++) {
        int j;
        for (j = 0; j < k; j++)
            CHECK(rs.mat[i * k + j] == (i == j ? 1 : 0), "row %d is not systematic", i);
    }

    for (i = 0; i < k + m; i++) {
        orig[i] = (uint8_t *)malloc(len);
        work[i] = (uint8_t *)malloc(len);
    }

    /* fill data shards with pseudo random bytes, then encode */
    for (i = 0; i < k; i++) {
        size_t j;
        for (j = 0; j < len; j++)
            orig[i][j] = rnd8();
    }
    rs_encode(&rs, orig, orig + k, len);

    full = (1u << (k + m)) - 1;
    for (mask = 0; mask <= full; mask++) {
        int bits = 0;
        for (i = 0; i < k + m; i++)
            if (mask & (1u << i))
                bits++;
        if (bits < k)
            continue;

        for (i = 0; i < k + m; i++) {
            present[i] = (mask & (1u << i)) ? 1 : 0;
            memset(work[i], 0xAA, len); /* poison so we can detect a missing rebuild */
            if (present[i])
                memcpy(work[i], orig[i], len);
        }

        CHECK(rs_reconstruct(&rs, work, present, len) == 0, "reconstruct failed mask=%u", mask);
        for (i = 0; i < k + m; i++)
            CHECK(memcmp(work[i], orig[i], len) == 0, "shard %d mismatch for mask=%u", i, mask);
        combos++;
    }
    printf("  %d+%d: %d recovery combinations verified\n", k, m, combos);

    for (i = 0; i < k + m; i++) {
        free(orig[i]);
        free(work[i]);
    }
    rs_free(&rs);
}

static void test_insufficient(void)
{
    rs_t rs;
    uint8_t *shards[RS_MAX_N];
    uint8_t present[RS_MAX_N];
    const size_t len = 64;
    int i;

    CHECK(rs_init(&rs, 4, 2) == 0, "rs_init failed");
    for (i = 0; i < 6; i++) {
        shards[i] = (uint8_t *)calloc(len, 1);
        present[i] = i < 3 ? 1 : 0; /* only 3 of 4 needed */
    }
    CHECK(rs_reconstruct(&rs, shards, present, len) == -1, "should refuse with 3/6 shards");
    for (i = 0; i < 6; i++)
        free(shards[i]);
    rs_free(&rs);
}

int main(void)
{
    printf("test_rs\n");
    test_gf();
    test_geometry(4, 2);
    test_geometry(6, 3);
    test_geometry(2, 2);
    test_insufficient();

    if (failures) {
        printf("test_rs: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_rs: ok\n");
    return 0;
}
