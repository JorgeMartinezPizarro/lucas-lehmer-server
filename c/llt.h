#ifndef LLT_H
#define LLT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Lucas-Lehmer test: is 2^p - 1 prime? Picks the fastest method for p. */
bool lucas_lehmer(uint32_t p);

bool is_prime_u32(uint32_t n);

/* LLT on GMP limbs (llt_mpn.c). Exact, best for small p. */
bool llt_mpn(uint32_t p);

/* LLT with an IBDWT (llt_fft.c). Retries with longer FFTs if the
   round-off error gets too large, so the answer is always exact. */
bool llt_ibdwt(uint32_t p);

/*
 * Runs `iterations` LLT steps for p with an FFT of length n and stores the
 * largest round-off error seen in *max_err. Returns 1 if the final residue
 * is 0, 0 if it is not, and -1 if the round-off error got too large for n.
 * Starts on `threads` threads and, if n is long enough, borrows spare
 * threads while it runs; it gives them back before returning.
 */
int llt_fft(uint32_t p, size_t n, uint32_t iterations, int threads, double* max_err);

/* Adds threads that no test is using, for FFT tests to borrow. */
void llt_add_spare_threads(int count);

/* Shortest FFT length that is precise enough for p, and the next length. */
size_t fft_length(uint32_t p);
size_t fft_next_length(size_t n);

#endif
