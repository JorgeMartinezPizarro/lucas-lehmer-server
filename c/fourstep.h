#ifndef FOURSTEP_H
#define FOURSTEP_H

#include <stddef.h>

typedef struct FourStep FourStep;

/* Most threads a four-step squaring of length n can use (1 if it is too short). */
int fourstep_max_threads(size_t n);

/*
 * Plans the squaring of the n reals in x. The plans do not depend on the
 * number of threads. With FFTW_MEASURE in flags, planning overwrites x.
 * Call with the FFTW planner lock held, as for fourstep_destroy.
 */
FourStep* fourstep_create(double* x, size_t n, unsigned flags);
void fourstep_destroy(FourStep* fs);

/*
 * x = n * (x * x) on `threads` threads, with * the cyclic convolution:
 * the same as FFTW's r2c, pointwise square and c2r.
 */
void fourstep_square(FourStep* fs, int threads);

#endif
