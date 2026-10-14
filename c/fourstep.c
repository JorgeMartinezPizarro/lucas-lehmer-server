/*
 * Squaring of n reals modulo x^n - 1 (their cyclic autoconvolution) on
 * several threads, with a four-step FFT built from FFTW's single-thread
 * batched plans. FFTW's own threaded plans are slower than one thread at
 * these lengths (x0.4 to x0.8 up to n = 131072, measured on an i5-1235U).
 *
 * The n reals are read as N = n/2 complex numbers z_c = x_2c + i x_2c+1,
 * which is the same memory. With c = j1 + N1 j2 and k = k2 + N2 k1 for
 * N = N1 N2, the FFT Z_k = sum_c z_c w^(ck), w = e^(-2 pi i / N), is:
 *
 *   1. N1 FFTs of length N2 down the columns j1 (stride N1),
 *   2. multiply position j1 + N1 k2 by w^(j1 k2),
 *   3. N2 FFTs of length N1 along the rows k2,
 *
 * which leaves Z_k at position k1 + N1 k2. The squaring needs no natural
 * order: each Z_k and its mirror Z_(N-k) give the spectrum of the reals,
 * which is squared and folded back into a complex spectrum,
 *
 *   Z'_k = 2 (Z_k^2 + (1 + w^k) O_k^2),    O_k = (Z_k - conj Z_(N-k)) / 2i,
 *
 * whose inverse FFT (the steps in reverse order) is n times the cyclic
 * convolution of the reals, in natural order. The mirrors of row k2 are in
 * row N2 - k2, so the work is cut into chunks of columns and chunks of the
 * rows 0..N2/2 with their mirror rows, and a squaring takes three barriers.
 * Threads take chunks as they finish the previous one, so on hybrid CPUs
 * the fast cores do more of them, and the plans (one set per chunk) do not
 * depend on the number of threads.
 *
 * The powers of w come from two tables of about sqrt(N) entries,
 * w^m = w^(hi << shift) * w^lo, instead of tables of N entries: long FFTs
 * are limited by memory bandwidth.
 */

#include <fftw3.h>
#include <math.h>
#include <omp.h>
#include <stdlib.h>

#include "fourstep.h"

/* Chunks per pass: at most this many, of at least 4 columns or 2 rows */
#define MAX_CHUNKS 32

struct FourStep {
    size_t N, N1, N2;
    int chunks;
    fftw_complex* z;            /* the reals, read as N complex numbers */
    int shift;                  /* m = (hi << shift) + lo */
    size_t mask;
    fftw_complex* roots_hi;     /* w^(hi << shift) */
    fftw_complex* roots_lo;     /* w^lo */
    size_t* cols;               /* chunk c has columns cols[c] to cols[c+1]-1, */
    size_t* rows;               /* and rows rows[c] to rows[c+1]-1 of 0..N2/2 with their mirrors */
    fftw_plan *cols_fwd, *cols_inv, *rows_fwd, *rows_inv, *mirrors_fwd, *mirrors_inv;
};

/* N1: the largest power of two up to sqrt(N) that leaves N2 = N / N1 even */
static size_t split(size_t N)
{
    size_t N1 = 1;
    while (4 * N1 * N1 <= N && N % (4 * N1) == 0)
        N1 *= 2;
    return N1;
}

static int chunk_count(size_t N1, size_t N2)
{
    size_t chunks = MAX_CHUNKS;
    if (chunks > N1 / 4) chunks = N1 / 4;
    if (chunks > (N2 / 2 + 1) / 2) chunks = (N2 / 2 + 1) / 2;
    return chunks < 1 ? 1 : (int)chunks;
}

int fourstep_max_threads(size_t n)
{
    size_t N = n / 2, N1 = split(N);
    return chunk_count(N1, N / N1);
}

/* w^m, exact at the quarter turns */
static void root(fftw_complex out, size_t m, size_t N)
{
    if (4 * m % N == 0) {
        static const double quarter[4][2] = { { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1 } };
        out[0] = quarter[4 * m / N % 4][0];
        out[1] = quarter[4 * m / N % 4][1];
        return;
    }
    long double a = -2 * 3.14159265358979323846264338327950288L * (long double)m / N;
    out[0] = (double)cosl(a);
    out[1] = (double)sinl(a);
}

/* w^m for m < N, into re and im */
static inline void power(const FourStep* fs, size_t m, double* re, double* im)
{
    const double* h = fs->roots_hi[m >> fs->shift];
    const double* l = fs->roots_lo[m & fs->mask];
    *re = h[0] * l[0] - h[1] * l[1];
    *im = h[0] * l[1] + h[1] * l[0];
}

static fftw_plan plan_cols(FourStep* fs, size_t c0, size_t c1, int sign, unsigned flags)
{
    if (c1 <= c0) return NULL;
    int len = (int)fs->N2, stride = (int)fs->N1;
    fftw_complex* p = fs->z + c0;
    return fftw_plan_many_dft(1, &len, (int)(c1 - c0), p, NULL, stride, 1, p, NULL, stride, 1, sign, flags);
}

static fftw_plan plan_rows(FourStep* fs, size_t r0, size_t r1, int sign, unsigned flags)
{
    if (r1 <= r0) return NULL;
    int len = (int)fs->N1, dist = (int)fs->N1;
    fftw_complex* p = fs->z + fs->N1 * r0;
    return fftw_plan_many_dft(1, &len, (int)(r1 - r0), p, NULL, 1, dist, p, NULL, 1, dist, sign, flags);
}

FourStep* fourstep_create(double* x, size_t n, unsigned flags)
{
    FourStep* fs = calloc(1, sizeof(FourStep));
    size_t N = n / 2, N1 = split(N), N2 = N / N1;
    int C = chunk_count(N1, N2);

    fs->N = N;
    fs->N1 = N1;
    fs->N2 = N2;
    fs->chunks = C;
    fs->z = (fftw_complex*)x;

    while (((size_t)1 << (2 * fs->shift)) < N)
        fs->shift++;
    fs->mask = ((size_t)1 << fs->shift) - 1;
    size_t his = (N >> fs->shift) + 1, los = fs->mask + 1;
    fs->roots_hi = fftw_alloc_complex(his);
    fs->roots_lo = fftw_alloc_complex(los);
    for (size_t h = 0; h < his; h++)
        root(fs->roots_hi[h], h << fs->shift, N);
    for (size_t l = 0; l < los; l++)
        root(fs->roots_lo[l], l, N);

    /* Column chunks start at even columns, so all of them are aligned alike */
    fs->cols = malloc((C + 1) * sizeof(size_t));
    fs->rows = malloc((C + 1) * sizeof(size_t));
    for (int c = 0; c <= C; c++) {
        fs->cols[c] = N1 * c / C / 2 * 2;
        fs->rows[c] = (N2 / 2 + 1) * c / C;
    }

    fs->cols_fwd = malloc(C * sizeof(fftw_plan));
    fs->cols_inv = malloc(C * sizeof(fftw_plan));
    fs->rows_fwd = malloc(C * sizeof(fftw_plan));
    fs->rows_inv = malloc(C * sizeof(fftw_plan));
    fs->mirrors_fwd = malloc(C * sizeof(fftw_plan));
    fs->mirrors_inv = malloc(C * sizeof(fftw_plan));

    for (int c = 0; c < C; c++) {
        size_t r0 = fs->rows[c], r1 = fs->rows[c + 1];
        /* mirrors of rows 1..N2/2-1 in [r0, r1): rows N2-r, a block of rows too */
        size_t m0 = r0 > 1 ? r0 : 1, m1 = r1 < N2 / 2 ? r1 : N2 / 2;
        size_t from = m1 > m0 ? N2 - m1 + 1 : 0, to = m1 > m0 ? N2 - m0 + 1 : 0;

        fs->cols_fwd[c] = plan_cols(fs, fs->cols[c], fs->cols[c + 1], FFTW_FORWARD, flags);
        fs->cols_inv[c] = plan_cols(fs, fs->cols[c], fs->cols[c + 1], FFTW_BACKWARD, flags);
        fs->rows_fwd[c] = plan_rows(fs, r0, r1, FFTW_FORWARD, flags);
        fs->rows_inv[c] = plan_rows(fs, r0, r1, FFTW_BACKWARD, flags);
        fs->mirrors_fwd[c] = plan_rows(fs, from, to, FFTW_FORWARD, flags);
        fs->mirrors_inv[c] = plan_rows(fs, from, to, FFTW_BACKWARD, flags);
    }

    return fs;
}

void fourstep_destroy(FourStep* fs)
{
    fftw_plan* plans[] = { fs->cols_fwd, fs->cols_inv, fs->rows_fwd, fs->rows_inv, fs->mirrors_fwd, fs->mirrors_inv };
    for (size_t i = 0; i < sizeof(plans) / sizeof(plans[0]); i++) {
        for (int c = 0; c < fs->chunks; c++)
            if (plans[i][c]) fftw_destroy_plan(plans[i][c]);
        free(plans[i]);
    }
    fftw_free(fs->roots_hi);
    fftw_free(fs->roots_lo);
    free(fs->cols);
    free(fs->rows);
    free(fs);
}

/* --------------------------------------------------------- */

static inline void execute(fftw_plan plan)
{
    if (plan) fftw_execute(plan);
}

/* z *= w^m, or by its conjugate if inverse */
static inline void twiddle(const FourStep* fs, fftw_complex z, size_t m, int inverse)
{
    double wr, wi, re = z[0], im = z[1];
    power(fs, m, &wr, &wi);
    if (inverse) wi = -wi;
    z[0] = re * wr - im * wi;
    z[1] = re * wi + im * wr;
}

/* Twiddles the columns c0..c1-1: position j1 + N1 k2 by w^(j1 k2) */
static void twiddle_cols(const FourStep* fs, size_t c0, size_t c1, int inverse)
{
    for (size_t k2 = 0; k2 < fs->N2; k2++)
        for (size_t j1 = c0; j1 < c1; j1++)
            twiddle(fs, fs->z[j1 + fs->N1 * k2], j1 * k2, inverse);
}

/*
 * Replaces Z_k (in a) and its mirror Z_(N-k) (in b) by Z'_k and Z'_(N-k).
 * f = 1 + w^k. Since O_(N-k) = conj O_k and 1 + w^(N-k) = conj f, the
 * mirror's term is the conjugate of a's. a and b may be the same element.
 */
static inline void square_pair(fftw_complex a, fftw_complex b, double f_re, double f_im)
{
    double ar = a[0], ai = a[1], br = b[0], bi = b[1];
    double o_re = (ai + bi) / 2, o_im = (br - ar) / 2;
    double s_re = o_re * o_re - o_im * o_im, s_im = 2 * o_re * o_im;
    double t_re = f_re * s_re - f_im * s_im, t_im = f_re * s_im + f_im * s_re;

    a[0] = 2 * (ar * ar - ai * ai + t_re);
    a[1] = 2 * (2 * ar * ai + t_im);
    b[0] = 2 * (br * br - bi * bi + t_re);
    b[1] = 2 * (2 * br * bi - t_im);
}

/*
 * Squares row r (0 <= r <= N2/2) together with its mirrors. Row r holds
 * k = r + N2 k1, whose mirror N - k is in row N2 - r at k1' = N1 - 1 - k1;
 * row 0 is its own mirror row with k1' = (N1 - k1) mod N1, and so is row
 * N2/2 with k1' = N1 - 1 - k1.
 */
static void square_row(const FourStep* fs, size_t r)
{
    size_t N1 = fs->N1, N2 = fs->N2;
    fftw_complex* a = fs->z + N1 * r;
    fftw_complex* b = fs->z + N1 * (N2 - r);
    int own = r == 0 || 2 * r == N2;
    size_t shift = r == 0 ? 0 : 1;

    for (size_t k1 = 0; k1 < N1; k1++) {
        size_t m = own ? (2 * N1 - shift - k1) % N1 : N1 - 1 - k1;
        if (own && m < k1)
            continue;   /* that pair was squared at k1 = m */

        double f_re, f_im;
        power(fs, r + N2 * k1, &f_re, &f_im);
        square_pair(a[k1], own ? a[m] : b[m], f_re + 1, f_im);
    }
}

void fourstep_square(FourStep* fs, int threads)
{
    #pragma omp parallel num_threads(threads)
    {
        /* Transform and twiddle the columns */
        #pragma omp for schedule(dynamic, 1)
        for (int c = 0; c < fs->chunks; c++) {
            execute(fs->cols_fwd[c]);
            twiddle_cols(fs, fs->cols[c], fs->cols[c + 1], 0);
        }

        /* Transform the rows with their mirrors, square, transform back */
        #pragma omp for schedule(dynamic, 1)
        for (int c = 0; c < fs->chunks; c++) {
            execute(fs->rows_fwd[c]);
            execute(fs->mirrors_fwd[c]);
            for (size_t r = fs->rows[c]; r < fs->rows[c + 1]; r++)
                square_row(fs, r);
            execute(fs->rows_inv[c]);
            execute(fs->mirrors_inv[c]);
        }

        /* Untwiddle and transform back the columns */
        #pragma omp for schedule(dynamic, 1)
        for (int c = 0; c < fs->chunks; c++) {
            twiddle_cols(fs, fs->cols[c], fs->cols[c + 1], 1);
            execute(fs->cols_inv[c]);
        }
    }
}
