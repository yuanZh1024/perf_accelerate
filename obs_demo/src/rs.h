/*
 * rs.h - systematic Reed-Solomon erasure code over GF(2^8).
 *
 * The code is built from a Cauchy matrix, which is guaranteed to be MDS:
 * any k of the n = k+m shards are enough to rebuild the whole stripe.
 *
 *   row 0..k-1   : identity  -> shard i is data shard i (systematic)
 *   row k..k+m-1 : 1/(x_i + y_j) with x_i = k+i, y_j = j  -> parity
 *
 * All arithmetic happens on whole shards, so a "shard" here is just a
 * buffer of `len` bytes.
 */
#ifndef OBS_RS_H
#define OBS_RS_H

#include <stddef.h>
#include <stdint.h>

#define RS_MAX_N 16 /* upper bound for k+m */

typedef struct {
    int k;
    int m;
    int n;
    uint8_t *mat; /* n x k coefficients, row major */
} rs_t;

/* Returns 0 on success, -1 on invalid parameters / allocation failure. */
int rs_init(rs_t *rs, int k, int m);
void rs_free(rs_t *rs);

/*
 * Encode k data shards into m parity shards.
 * Every buffer must hold exactly `len` bytes.
 */
void rs_encode(const rs_t *rs, uint8_t *const *data, uint8_t *const *parity, size_t len);

/*
 * Rebuild every shard whose `present[i]` is 0.
 * Requires at least k shards present. Buffers are all `len` bytes.
 * Returns 0 on success, -1 if fewer than k shards are usable or the
 * matrix turned out to be singular.
 */
int rs_reconstruct(const rs_t *rs, uint8_t *const *shards, const uint8_t *present, size_t len);

#endif /* OBS_RS_H */
