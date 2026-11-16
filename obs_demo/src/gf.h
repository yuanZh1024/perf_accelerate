/*
 * gf.h - arithmetic over GF(2^8), primitive polynomial 0x11d (x^8+x^4+x^3+x^2+1)
 *
 * Part of obs-ec-store: a tiny erasure-coded object store.
 */
#ifndef OBS_GF_H
#define OBS_GF_H

#include <stdint.h>

/* Must be called once before any other gf_* function (idempotent). */
void gf_init(void);

uint8_t gf_mul(uint8_t a, uint8_t b);
uint8_t gf_div(uint8_t a, uint8_t b); /* b must be non-zero */
uint8_t gf_inv(uint8_t a);            /* a must be non-zero */
uint8_t gf_pow(uint8_t a, unsigned n);

#endif /* OBS_GF_H */
