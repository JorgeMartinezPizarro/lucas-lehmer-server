#include "llt.h"

/* From this exponent on the FFT is faster than GMP's squaring (measured on
   batches of primes: x0.9 at 3000-4000, x1.1 at 4000-5000, x2 at 10000).
   Can be overridden with -DFFT_MIN_EXPONENT=... */
#ifndef FFT_MIN_EXPONENT
#define FFT_MIN_EXPONENT 4000
#endif

#if FFT_MIN_EXPONENT < 64
#error "FFT_MIN_EXPONENT must be >= 64 so every FFT digit has a few bits"
#endif

bool is_prime_u32(uint32_t n)
{
    if (n < 4) return n >= 2;
    if ((n & 1) == 0) return false;

    for (uint32_t i = 3; (uint64_t)i * i <= n; i += 2)
        if (n % i == 0) return false;

    return true;
}

bool lucas_lehmer(uint32_t p)
{
    if (p == 2) return true;
    if (!is_prime_u32(p)) return false;

    return p < FFT_MIN_EXPONENT ? llt_mpn(p) : llt_ibdwt(p);
}
