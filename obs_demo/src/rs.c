#include "rs.h"
#include "gf.h"

#include <stdlib.h>
#include <string.h>

/* dst ^= coef * src, byte by byte, using a 256 byte multiplication table. */
static void mul_add(uint8_t coef, const uint8_t *src, uint8_t *dst, size_t len)
{
    uint8_t tab[256];
    size_t i;
    int v;

    if (coef == 0)
        return;

    if (coef == 1) {
        for (i = 0; i < len; i++)
            dst[i] ^= src[i];
        return;
    }

    for (v = 0; v < 256; v++)
        tab[v] = gf_mul(coef, (uint8_t)v);

    for (i = 0; i < len; i++)
        dst[i] ^= tab[src[i]];
}

/*
 * Gauss-Jordan inversion of an n x n matrix in GF(2^8).
 * `a` is consumed. `out` receives the inverse.
 * Returns 0 on success, -1 if the matrix is singular.
 */
static int mat_inv(uint8_t *a, int n, uint8_t *out)
{
    int i, j, r, c;

    memset(out, 0, (size_t)n * (size_t)n);
    for (i = 0; i < n; i++)
        out[i * n + i] = 1;

    for (c = 0; c < n; c++) {
        /* find a pivot in column c */
        r = -1;
        for (i = c; i < n; i++) {
            if (a[i * n + c] != 0) {
                r = i;
                break;
            }
        }
        if (r < 0)
            return -1;

        if (r != c) {
            for (j = 0; j < n; j++) {
                uint8_t t = a[r * n + j];
                a[r * n + j] = a[c * n + j];
                a[c * n + j] = t;
                t = out[r * n + j];
                out[r * n + j] = out[c * n + j];
                out[c * n + j] = t;
            }
        }

        if (a[c * n + c] != 1) {
            uint8_t inv = gf_inv(a[c * n + c]);
            for (j = 0; j < n; j++) {
                a[c * n + j] = gf_mul(a[c * n + j], inv);
                out[c * n + j] = gf_mul(out[c * n + j], inv);
            }
        }

        for (r = 0; r < n; r++) {
            uint8_t f;
            if (r == c)
                continue;
            f = a[r * n + c];
            if (f == 0)
                continue;
            for (j = 0; j < n; j++) {
                a[r * n + j] ^= gf_mul(f, a[c * n + j]);
                out[r * n + j] ^= gf_mul(f, out[c * n + j]);
            }
        }
    }
    return 0;
}

int rs_init(rs_t *rs, int k, int m)
{
    int i, j;

    if (rs == NULL)
        return -1;
    memset(rs, 0, sizeof(*rs));

    if (k < 1 || m < 1 || k + m > RS_MAX_N)
        return -1;

    gf_init();

    rs->k = k;
    rs->m = m;
    rs->n = k + m;
    rs->mat = (uint8_t *)calloc((size_t)rs->n * (size_t)k, 1);
    if (rs->mat == NULL)
        return -1;

    for (i = 0; i < k; i++)
        rs->mat[i * k + i] = 1; /* systematic part */

    /* Cauchy rows: guaranteed MDS because all x_i and y_j are distinct. */
    for (i = 0; i < m; i++) {
        for (j = 0; j < k; j++) {
            uint8_t x = (uint8_t)(k + i);
            uint8_t y = (uint8_t)j;
            rs->mat[(k + i) * k + j] = gf_inv((uint8_t)(x ^ y));
        }
    }
    return 0;
}

void rs_free(rs_t *rs)
{
    if (rs == NULL)
        return;
    free(rs->mat);
    rs->mat = NULL;
}

void rs_encode(const rs_t *rs, uint8_t *const *data, uint8_t *const *parity, size_t len)
{
    int i, j;

    for (i = 0; i < rs->m; i++) {
        uint8_t *dst = parity[i];
        memset(dst, 0, len);
        for (j = 0; j < rs->k; j++)
            mul_add(rs->mat[(rs->k + i) * rs->k + j], data[j], dst, len);
    }
}

int rs_reconstruct(const rs_t *rs, uint8_t *const *shards, const uint8_t *present, size_t len)
{
    int k = rs->k, n = rs->n;
    int idx[RS_MAX_N];
    int cnt = 0;
    int i, r, c, missing = 0;
    uint8_t A[RS_MAX_N * RS_MAX_N];
    uint8_t Ainv[RS_MAX_N * RS_MAX_N];
    uint8_t *data[RS_MAX_N];

    for (i = 0; i < n; i++) {
        if (present[i])
            idx[cnt++] = i;
        else
            missing++;
    }
    if (cnt < k)
        return -1;
    if (missing == 0)
        return 0;

    /* Only the first k usable shards are needed to solve the system. */
    for (r = 0; r < k; r++)
        for (c = 0; c < k; c++)
            A[r * k + c] = rs->mat[idx[r] * k + c];

    if (mat_inv(A, k, Ainv) != 0)
        return -1;

    for (r = 0; r < k; r++) {
        data[r] = (uint8_t *)calloc(len, 1);
        if (data[r] == NULL) {
            while (--r >= 0)
                free(data[r]);
            return -1;
        }
    }

    /* data = Ainv * usable_shards */
    for (r = 0; r < k; r++)
        for (c = 0; c < k; c++)
            mul_add(Ainv[r * k + c], shards[idx[c]], data[r], len);

    for (i = 0; i < n; i++) {
        if (present[i])
            continue;
        if (i < k) {
            memcpy(shards[i], data[i], len);
        } else {
            memset(shards[i], 0, len);
            for (c = 0; c < k; c++)
                mul_add(rs->mat[i * k + c], data[c], shards[i], len);
        }
    }

    for (r = 0; r < k; r++)
        free(data[r]);
    return 0;
}
