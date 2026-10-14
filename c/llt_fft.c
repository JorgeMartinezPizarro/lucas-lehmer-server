/*
 * Lucas-Lehmer test with the irrational-base discrete weighted transform
 * (IBDWT) of Crandall and Fagin, on FFTW's real FFTs.
 *
 * s mod M_p (M_p = 2^p - 1) is kept as n balanced digits, digit j holding
 * w_j = ceil(p(j+1)/n) - ceil(pj/n) bits:
 *
 *     s = sum_j x_j 2^ceil(pj/n),    -2^(w_j-1) <= x_j < 2^(w_j-1)
 *
 * With the weights a_j = 2^(ceil(pj/n) - pj/n), the cyclic convolution of
 * a_j x_j is a_j times the digits of s^2 mod M_p before carrying, so the
 * reduction mod M_p comes for free. One LLT step is: FFT, square pointwise,
 * inverse FFT, unweight, round, subtract 2, carry and weight again; between
 * steps the digits are kept weighted. The carry out of the top digit wraps
 * around to digit 0 because 2^p = 1 (mod M_p).
 *
 * With r_j = (-pj) mod n, the weight is a_j = 2^(r_j / n), the width is
 * w_j = p / n + (r_j < p % n), and r_(j+1) = (r_j - p) mod n. So widths are
 * computed as the carry goes, and so are the weights of long FFTs, from two
 * tables of about sqrt(n) entries: long FFTs are limited by memory
 * bandwidth. Short ones fit in the cache, and reading their weights from
 * arrays of n elements is faster.
 *
 * Rounding is only exact while the round-off error stays below 0.5, so the
 * error is checked on every step and the test is redone with a longer FFT
 * if it gets close.
 *
 * On one thread a step uses FFTW's real FFTs. Long FFTs can also
 * run on several threads, with the four-step FFT of fourstep.c and a carry
 * done in blocks of digits whose carries then go into the next block. A
 * test starts on its caller's thread and, while it runs, borrows the
 * threads no other test is using (see llt_add_spare_threads); it gives them
 * back when it ends. Only the first extra threads need new plans.
 */

#include <fftw3.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fourstep.h"
#include "llt.h"

/* Round-off error above this means n is too short for p. */
#define MAX_ROUNDOFF 0.4

#define MIN_LENGTH 8

/* FFTW_MEASURE finds plans ~10% faster than FFTW_ESTIMATE but takes a good
   fraction of a second per length, which only pays off for long FFTs. */
#define MEASURE_MIN_LENGTH 8192

/* Shorter FFTs gain nothing from more threads (measured with test_llt scale
   on an i5-1235U: x1.08 at most for n = 65536, x1.57 for 131072 and x1.62
   for 393216); longer ones use at most one thread per DIGITS_PER_THREAD
   digits. -DTHREADS_MIN_LENGTH=4294967295u gives every test one thread. */
#ifndef THREADS_MIN_LENGTH
#define THREADS_MIN_LENGTH 131072
#endif
#ifndef DIGITS_PER_THREAD
#define DIGITS_PER_THREAD 8192
#endif

/* Steps between looks for spare threads */
#define GROW_INTERVAL 64

/* Carry blocks per thread: threads take blocks as they finish one */
#define CARRY_BLOCKS_PER_THREAD 4

/* Shorter FFTs read their weights from arrays (measured on an i5-1235U:
   computing them is x0.76 slower at n = 4096 and x0.99 at 393216 on one
   thread, but 14% faster at 393216 on 12 threads). */
#define WEIGHT_ARRAYS_MAX_LENGTH THREADS_MIN_LENGTH

/* Adding and subtracting 1.5 * 2^52 rounds a double to the nearest integer
   (for |v| < 2^51), without calling rint() on CPUs lacking SSE4.1. */
#define ROUND_CONST 6755399441055744.0

/* The FFTW planner is not thread-safe; executing plans is. */
static pthread_mutex_t planner_lock = PTHREAD_MUTEX_INITIALIZER;

/* Threads that no test is using */
static int spare_threads;

/* Widths and weights of the digits of p on n digits (see above) */
typedef struct {
    uint32_t p;
    size_t n, q, s;             /* q = p / n, s = p % n */
    int shift;                  /* r = (hi << shift) + lo */
    size_t mask;
    double *a_hi, *a_lo;        /* 2^(r/n) = a_hi[hi] * a_lo[lo] */
    double *u_hi, *u_lo;        /* 2^(-r/n) / n = u_hi[hi] * u_lo[lo] */
    double *a, *u;              /* the same per digit, for short FFTs (else NULL) */
} Digits;

typedef struct {
    size_t n;
    int threads;            /* threads in use */
    int borrowed;           /* how many of them to give back to the spares */
    double* x;              /* the weighted digits */
    fftw_complex* f;        /* their spectrum, on one thread */
    fftw_plan forward, inverse;     /* one thread: FFTW's r2c and c2r, out of place
                                       (in place is up to 20% slower for short FFTs) */
    FourStep* fourstep;             /* several threads */
} Transform;

/* --------------------------------------------------------- */

/*
 * Bits per digit for length n. Measured with test_llt roundoff, the largest
 * round-off error is about 2^(2 * bits - 51.8) * n^0.6 for every length
 * from 8 to 2^21, so this keeps it near 0.23, well below MAX_ROUNDOFF.
 */
static double max_bits(size_t n)
{
    return 24.8 - 0.3 * log2((double)n);
}

/* FFT lengths are m * 2^k with m in {1, 3, 5, 7}: FFTW is fast on them and
   consecutive lengths are at most 25% apart. */
size_t fft_next_length(size_t n)
{
    for (size_t k = n + 1;; k++) {
        size_t m = k;
        while (m % 2 == 0) m /= 2;
        if (m == 1 || m == 3 || m == 5 || m == 7) return k;
    }
}

size_t fft_length(uint32_t p)
{
    size_t n = MIN_LENGTH;
    while (p > n * max_bits(n))
        n = fft_next_length(n);
    return n;
}

/* --------------------------------------------------------- */

static void digits_init(Digits* d, uint32_t p, size_t n)
{
    d->p = p;
    d->n = n;
    d->q = p / n;
    d->s = p % n;
    d->shift = 0;
    while (((size_t)1 << (2 * d->shift)) < n)
        d->shift++;
    d->mask = ((size_t)1 << d->shift) - 1;

    size_t his = (n >> d->shift) + 1, los = d->mask + 1;
    d->a_hi = malloc(his * sizeof(double));
    d->u_hi = malloc(his * sizeof(double));
    d->a_lo = malloc(los * sizeof(double));
    d->u_lo = malloc(los * sizeof(double));

    for (size_t h = 0; h < his; h++) {
        long double e = (long double)(h << d->shift) / n;
        d->a_hi[h] = (double)exp2l(e);
        d->u_hi[h] = (double)(exp2l(-e) / n);
    }
    for (size_t l = 0; l < los; l++) {
        long double e = (long double)l / n;
        d->a_lo[l] = (double)exp2l(e);
        d->u_lo[l] = (double)exp2l(-e);
    }

    d->a = d->u = NULL;
    if (n < WEIGHT_ARRAYS_MAX_LENGTH) {
        d->a = malloc(n * sizeof(double));
        d->u = malloc(n * sizeof(double));
        for (size_t j = 0, r = 0; j < n; j++, r = r >= d->s ? r - d->s : r + n - d->s) {
            long double e = (long double)r / n;
            d->a[j] = (double)exp2l(e);
            d->u[j] = (double)(exp2l(-e) / n);
        }
    }
}

static void digits_free(Digits* d)
{
    free(d->a_hi);
    free(d->u_hi);
    free(d->a_lo);
    free(d->u_lo);
    free(d->a);
    free(d->u);
}

/* r_j = (-pj) mod n */
static inline size_t residue(const Digits* d, size_t j)
{
    return (d->n - (size_t)((uint64_t)d->p * j % d->n)) % d->n;
}

/* r_(j+1) from r_j */
static inline size_t next(const Digits* d, size_t r)
{
    return r >= d->s ? r - d->s : r + d->n - d->s;
}

static inline int width(const Digits* d, size_t r)
{
    return (int)(d->q + (r < d->s));
}

static inline double weight(const Digits* d, size_t r)
{
    return d->a_hi[r >> d->shift] * d->a_lo[r & d->mask];
}

/* The weight of digit j, whose r is r */
static inline double weight_of(const Digits* d, size_t j, size_t r)
{
    return d->a ? d->a[j] : weight(d, r);
}

/* 1 / (weight * n): the inverse FFT also scales by n */
static inline double unweight(const Digits* d, size_t r)
{
    return d->u_hi[r >> d->shift] * d->u_lo[r & d->mask];
}

/* --------------------------------------------------------- */

void llt_add_spare_threads(int count)
{
    __atomic_add_fetch(&spare_threads, count, __ATOMIC_RELAXED);
}

/* Takes up to `wanted` spare threads and returns how many it got. */
static int borrow_threads(int wanted)
{
    int spare = __atomic_load_n(&spare_threads, __ATOMIC_RELAXED);
    while (spare > 0) {
        int take = spare < wanted ? spare : wanted;
        if (__atomic_compare_exchange_n(&spare_threads, &spare, spare - take, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return take;
    }
    return 0;
}

static int max_threads(size_t n)
{
    if (n < THREADS_MIN_LENGTH) return 1;
    int cap = (int)(n / DIGITS_PER_THREAD), limit = fourstep_max_threads(n);
    return cap < limit ? cap : limit;
}

/* Call with planner_lock held */
static void destroy_plans(Transform* t)
{
    if (t->forward) fftw_destroy_plan(t->forward);
    if (t->inverse) fftw_destroy_plan(t->inverse);
    if (t->fourstep) fourstep_destroy(t->fourstep);
    t->forward = t->inverse = NULL;
    t->fourstep = NULL;
}

/*
 * (Re)creates the plans for t->threads threads. FFTW_MEASURE overwrites
 * x while timing candidate plans, so the digits are saved and restored.
 * Call with planner_lock held.
 */
static void make_plans(Transform* t)
{
    unsigned flags = t->n >= MEASURE_MIN_LENGTH ? FFTW_MEASURE : FFTW_ESTIMATE;
    double* saved = malloc(t->n * sizeof(double));
    memcpy(saved, t->x, t->n * sizeof(double));

    destroy_plans(t);
    if (t->threads > 1) {
        t->fourstep = fourstep_create(t->x, t->n, flags);
    } else {
        t->forward = fftw_plan_dft_r2c_1d((int)t->n, t->x, t->f, flags);
        t->inverse = fftw_plan_dft_c2r_1d((int)t->n, t->f, t->x, flags);
    }

    memcpy(t->x, saved, t->n * sizeof(double));
    free(saved);
}

/*
 * Borrows spare threads, up to max_threads(n). Going from one thread to
 * several needs the four-step plans; planning a long FFT takes seconds, so
 * that waits while another test holds the planner.
 */
static void grow(Transform* t)
{
    int cap = max_threads(t->n);
    if (t->threads >= cap || __atomic_load_n(&spare_threads, __ATOMIC_RELAXED) == 0)
        return;

    if (t->fourstep) {
        int got = borrow_threads(cap - t->threads);
        t->threads += got;
        t->borrowed += got;
        return;
    }

    if (pthread_mutex_trylock(&planner_lock) != 0)
        return;
    int got = borrow_threads(cap - t->threads);
    if (got > 0) {
        t->threads += got;
        t->borrowed += got;
        make_plans(t);
    }
    pthread_mutex_unlock(&planner_lock);
}

/* --------------------------------------------------------- */

/* Splits t into a balanced digit of w bits, stored in *digit, and returns
   the carry. Relies on >> of a negative int64_t being an arithmetic shift. */
static inline int64_t balance(int64_t t, int w, int64_t* digit)
{
    int64_t c = (t + ((int64_t)1 << (w - 1))) >> w;
    *digit = t - c * ((int64_t)1 << w);
    return c;
}

static void square(fftw_complex* f, size_t count)
{
    for (size_t k = 0; k < count; k++) {
        double re = f[k][0], im = f[k][1];
        f[k][0] = re * re - im * im;
        f[k][1] = 2 * re * im;
    }
}

/* Adds c to the weighted digit x_j and returns the carry out of it */
static int64_t add_to_digit(const Digits* d, double* x, size_t j, int64_t c)
{
    size_t r = residue(d, j);
    double a = weight_of(d, j, r);
    int64_t digit;
    c = balance(llround(x[j] / a) + c, width(d, r), &digit);
    x[j] = (double)digit * a;
    return c;
}

/*
 * Unweights and rounds the inverse FFT output, subtracts 2, carries so
 * every digit is balanced again, and weights the digits for the next step.
 * The digits are carried in `blocks` blocks on `threads` threads; then the
 * carry out of each block goes into the next one, and the one out of the
 * top digit wraps around to digit 0 because 2^p = 1 (mod M_p). Returns the
 * largest round-off error.
 */
static double carry(const Digits* d, double* x, int blocks, int threads)
{
    size_t n = d->n;
    int64_t block_carry[blocks];
    double block_err[blocks];

    #pragma omp parallel for schedule(dynamic, 1) num_threads(threads) if (threads > 1)
    for (int b = 0; b < blocks; b++) {
        size_t start = n * b / blocks, end = n * (b + 1) / blocks;
        size_t r = residue(d, start);
        int64_t c = b == 0 ? -2 : 0;
        double err = 0;

        for (size_t j = start; j < end; j++) {
            double a = weight_of(d, j, r);
            double u = d->u ? d->u[j] : unweight(d, r);

            double v = x[j] * u;
            double rounded = (v + ROUND_CONST) - ROUND_CONST;
            double e = fabs(v - rounded);
            if (e > err) err = e;

            int64_t digit;
            c = balance((int64_t)rounded + c, width(d, r), &digit);
            x[j] = (double)digit * a;
            r = next(d, r);
        }

        block_carry[b] = c;
        block_err[b] = err;
    }

    double err = 0;
    for (int b = 0; b < blocks; b++) {
        if (block_err[b] > err) err = block_err[b];

        int64_t c = block_carry[b];
        for (size_t j = n * (b + 1) / blocks % n; c != 0; j = (j + 1) % n)
            c = add_to_digit(d, x, j, c);
    }

    return err;
}

/*
 * Is s = 0 (mod M_p)? Unweights the digits and carries them into digits in
 * [0, 2^w_j); s is then in [0, M_p], where M_p (all ones) also means 0.
 */
static bool residue_is_zero(const Digits* d, const double* x)
{
    size_t n = d->n;
    int64_t* digits = malloc(n * sizeof(int64_t));
    for (size_t j = 0, r = 0; j < n; j++, r = next(d, r))
        digits[j] = llround(x[j] / weight_of(d, j, r));

    int64_t c = 0;
    do {
        for (size_t j = 0, r = 0; j < n; j++, r = next(d, r)) {
            int w = width(d, r);
            int64_t t = digits[j] + c;
            c = t >> w;
            digits[j] = t - c * ((int64_t)1 << w);
        }
    } while (c != 0);

    bool zero = true, ones = true;
    for (size_t j = 0, r = 0; j < n; j++, r = next(d, r)) {
        zero = zero && digits[j] == 0;
        ones = ones && digits[j] == ((int64_t)1 << width(d, r)) - 1;
    }

    free(digits);
    return zero || ones;
}

/* --------------------------------------------------------- */

int llt_fft(uint32_t p, size_t n, uint32_t iterations, int threads, double* max_err)
{
    int limit = fourstep_max_threads(n);
    Transform t = { .n = n, .threads = threads < limit ? threads : limit };
    Digits d;
    digits_init(&d, p, n);

    /* s = 4, and digit 0 has weight 1 */
    t.x = fftw_alloc_real(n);
    memset(t.x, 0, n * sizeof(double));
    t.x[0] = 4;
    t.f = fftw_alloc_complex(n / 2 + 1);

    /* Spare threads are cheapest to take now, before the first plan */
    if (max_threads(n) > t.threads) {
        t.borrowed = borrow_threads(max_threads(n) - t.threads);
        t.threads += t.borrowed;
    }

    pthread_mutex_lock(&planner_lock);
    make_plans(&t);
    pthread_mutex_unlock(&planner_lock);

    int result = 0;
    *max_err = 0;

    for (uint32_t i = 0; i < iterations; i++) {
        if (i > 0 && i % GROW_INTERVAL == 0)
            grow(&t);

        if (t.fourstep) {
            fourstep_square(t.fourstep, t.threads);
        } else {
            fftw_execute(t.forward);
            square(t.f, n / 2 + 1);
            fftw_execute(t.inverse);
        }

        int blocks = t.threads > 1 ? t.threads * CARRY_BLOCKS_PER_THREAD : 1;
        double err = carry(&d, t.x, blocks, t.threads);
        if (err > *max_err) *max_err = err;
        if (err > MAX_ROUNDOFF) {
            result = -1;
            break;
        }
    }

    if (result == 0)
        result = residue_is_zero(&d, t.x);

    pthread_mutex_lock(&planner_lock);
    destroy_plans(&t);
    pthread_mutex_unlock(&planner_lock);

    llt_add_spare_threads(t.borrowed);

    fftw_free(t.x);
    fftw_free(t.f);
    digits_free(&d);

    return result;
}

/* p must be a prime >= 64, so every digit has a few bits */
bool llt_ibdwt(uint32_t p)
{
    for (size_t n = fft_length(p);; n = fft_next_length(n)) {
        double err;
        int r = llt_fft(p, n, p - 2, 1, &err);
        if (r >= 0) return r;
        fprintf(stderr, "p=%u: round-off error %.3f with FFT length %zu, retrying with a longer FFT\n",
                p, err, n);
    }
}
