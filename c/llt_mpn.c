#include <stdlib.h>
#include <gmp.h>

#include "llt.h"

/* --------------------------------------------------------- */
/* Arithmetic mod M_p = 2^p - 1 on n = ceil(p / 64) limbs    */
/*                                                           */
/* Values are kept in [0, M_p]; both 0 and M_p mean zero.    */
/* --------------------------------------------------------- */

static inline mp_limb_t top_mask(uint32_t p)
{
    unsigned r = p % GMP_NUMB_BITS;
    return r ? ((mp_limb_t)1 << r) - 1 : GMP_NUMB_MAX;
}

static inline bool high_limbs_zero(const mp_limb_t* s, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (s[i]) return false;
    return true;
}

/*
 * s = t mod M_p, with t < 2^(2p) of 2n limbs.
 * Since 2^p = 1 (mod M_p): t = hi * 2^p + lo = hi + lo.
 * h is scratch space of n + 1 limbs. t is clobbered.
 */
static inline void reduce_mersenne(mp_limb_t* s,
                                   mp_limb_t* t,
                                   mp_limb_t* h,
                                   size_t n,
                                   uint32_t p)
{
    size_t q = p / GMP_NUMB_BITS;
    unsigned r = p % GMP_NUMB_BITS;
    mp_limb_t mask = top_mask(p);

    /* hi = t >> p, fits in n limbs */
    if (r)
        mpn_rshift(h, t + q, 2 * n - q, r);
    else
        mpn_copyi(h, t + q, n);

    /* lo = t mod 2^p */
    t[n - 1] &= mask;

    /* s = lo + hi <= 2^(p+1) - 2 */
    mp_limb_t carry = mpn_add_n(s, t, h, n);

    /* fold bit p back once more, leaving s <= M_p */
    if (r) {
        carry = s[n - 1] >> r;
        s[n - 1] &= mask;
    }
    mpn_add_1(s, s, n, carry);
}

/* s = s - 2 mod M_p, for s <= M_p */
static inline void sub2_mersenne(mp_limb_t* s, size_t n, uint32_t p)
{
    if (s[0] >= 2 || !high_limbs_zero(s, n)) {
        mpn_sub_1(s, s, n, 2);
        return;
    }

    /* s is 0 or 1: s - 2 = M_p - (2 - s) */
    mp_limb_t v = s[0];
    for (size_t i = 0; i < n; i++)
        s[i] = GMP_NUMB_MAX;
    s[n - 1] = top_mask(p);
    mpn_sub_1(s, s, n, 2 - v);
}

/* s == 0 (mod M_p), for s <= M_p */
static inline bool is_zero_mersenne(const mp_limb_t* s, size_t n, uint32_t p)
{
    if (s[0] == 0 && high_limbs_zero(s, n))
        return true;

    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] != GMP_NUMB_MAX) return false;
    return s[n - 1] == top_mask(p);
}

/* --------------------------------------------------------- */

/* p must be an odd prime */
bool llt_mpn(uint32_t p)
{
    size_t n = (p + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS;

    mp_limb_t* s = calloc(n, sizeof(mp_limb_t));
    mp_limb_t* t = calloc(2 * n, sizeof(mp_limb_t));
    mp_limb_t* h = calloc(n + 1, sizeof(mp_limb_t));

    s[0] = 4;

    for (uint32_t i = 0; i < p - 2; i++) {
        mpn_sqr(t, s, n);
        reduce_mersenne(s, t, h, n, p);
        sub2_mersenne(s, n, p);
    }

    bool ok = is_zero_mersenne(s, n, p);

    free(s);
    free(t);
    free(h);

    return ok;
}
