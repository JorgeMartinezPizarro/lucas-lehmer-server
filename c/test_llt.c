/*
 * Developer tool for the FFT LLT, not part of the server.
 *
 *   test_llt roundoff [max_n] [iterations] [threads]
 *       For every FFT length up to max_n, runs `iterations` LLT steps at the
 *       largest prime exponent that length is used for, and prints the
 *       largest round-off error seen. It must stay well below 0.4. With
 *       threads > 1 it checks the four-step FFT instead of FFTW's r2c.
 *
 *   test_llt bench p [p ...]
 *       Times llt_mpn and llt_ibdwt on each exponent.
 *
 *   test_llt bench-fft p [p ...]
 *       Times only llt_ibdwt, for exponents too large for GMP.
 *
 *   test_llt estimate p [p ...]
 *       Times a few hundred FFT steps for each exponent and projects how long
 *       the full test takes on one core.
 *
 *   test_llt scale p threads [threads ...]
 *       Like estimate, for one exponent on each number of threads.
 *
 *   test_llt threads-check from to
 *       Runs the FFT test of every prime in [from, to) on 2 to 8 threads and
 *       compares it with GMP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "llt.h"

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* Largest exponent that uses an FFT of length n or shorter */
static uint32_t max_exponent(size_t n)
{
    uint32_t lo = 64, hi = 0x7fffffff;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo + 1) / 2;
        if (fft_length(mid) <= n) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

/* Seconds per LLT step for p on the given threads */
static double time_step(uint32_t p, int threads)
{
    size_t n = fft_length(p);
    double err, elapsed;

    llt_fft(p, n, 2, threads, &err); /* plans the FFT, later runs reuse FFTW's wisdom */

    uint32_t steps = 25;
    do {
        steps *= 2;
        double t = now();
        llt_fft(p, n, steps, threads, &err);
        elapsed = now() - t;
    } while (elapsed < 1.0 && steps < p);

    return elapsed / steps;
}

static void roundoff(size_t max_n, uint32_t iterations, int threads)
{
    printf("%9s %11s %6s %8s\n", "n", "p", "bits", "max_err");
    for (size_t n = 8; n <= max_n; n = fft_next_length(n)) {
        uint32_t p = max_exponent(n);
        while (!is_prime_u32(p)) p--;

        double err;
        uint32_t steps = iterations < p - 2 ? iterations : p - 2;
        int r = llt_fft(p, n, steps, threads, &err);
        printf("%9zu %11u %6.2f %8.4f%s\n", n, p, (double)p / n, err, r < 0 ? "  TOO LARGE" : "");
        fflush(stdout);
    }
}

static void bench(int count, char** args, bool with_gmp)
{
    for (int i = 0; i < count; i++) {
        uint32_t p = (uint32_t)strtoul(args[i], NULL, 10);
        printf("p=%-9u n=%-8zu", p, fft_length(p));
        fflush(stdout);

        double gmp_time = 0;
        if (with_gmp) {
            double t = now();
            bool prime = llt_mpn(p);
            gmp_time = now() - t;
            printf("  gmp: %-5s %9.3f s", prime ? "true" : "false", gmp_time);
            fflush(stdout);
        }

        double t = now();
        bool prime = llt_ibdwt(p);
        double fft_time = now() - t;
        printf("  fft: %-5s %9.3f s", prime ? "true" : "false", fft_time);
        if (with_gmp) printf("  x%.1f", gmp_time / fft_time);
        printf("\n");
    }
}

static void estimate(int count, char** args)
{
    printf("%11s %9s %10s %12s\n", "p", "n", "ms/step", "full test");
    for (int i = 0; i < count; i++) {
        uint32_t p = (uint32_t)strtoul(args[i], NULL, 10);
        double step = time_step(p, 1);
        printf("%11u %9zu %10.3f %9.1f h\n", p, fft_length(p), step * 1000, step * (p - 2) / 3600);
        fflush(stdout);
    }
}

static void scale(uint32_t p, int count, char** args)
{
    printf("p=%u n=%zu\n%8s %10s %8s %12s\n", p, fft_length(p), "threads", "ms/step", "speedup", "full test");
    double first = 0;
    for (int i = 0; i < count; i++) {
        int threads = atoi(args[i]);
        double step = time_step(p, threads);
        if (i == 0) first = step;
        printf("%8d %10.3f %8.2f %9.1f h\n", threads, step * 1000, first / step, step * (p - 2) / 3600);
        fflush(stdout);
    }
}

static int threads_check(uint32_t from, uint32_t to)
{
    int tested = 0, failed = 0;
    for (uint32_t p = from; p < to; p++) {
        if (!is_prime_u32(p)) continue;

        int threads = 2 + p % 7;
        double err;
        int fft = llt_fft(p, fft_length(p), p - 2, threads, &err);
        int gmp = llt_mpn(p);
        if (fft != gmp) {
            printf("p=%u on %d threads: fft %d, gmp %d (max_err %.3f)\n", p, threads, fft, gmp, err);
            failed++;
        }
        tested++;
    }
    printf("%d primes in [%u, %u) on 2-8 threads: %d mismatches with GMP\n", tested, from, to, failed);
    return failed > 0;
}

int main(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "roundoff") == 0) {
        size_t max_n = argc > 2 ? strtoul(argv[2], NULL, 10) : 65536;
        uint32_t iterations = argc > 3 ? strtoul(argv[3], NULL, 10) : 2000;
        int threads = argc > 4 ? atoi(argv[4]) : 1;
        roundoff(max_n, iterations, threads);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "bench") == 0) {
        bench(argc - 2, argv + 2, true);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "bench-fft") == 0) {
        bench(argc - 2, argv + 2, false);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "estimate") == 0) {
        estimate(argc - 2, argv + 2);
        return 0;
    }
    if (argc >= 4 && strcmp(argv[1], "scale") == 0) {
        scale((uint32_t)strtoul(argv[2], NULL, 10), argc - 3, argv + 3);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "threads-check") == 0)
        return threads_check((uint32_t)strtoul(argv[2], NULL, 10), (uint32_t)strtoul(argv[3], NULL, 10));

    fprintf(stderr, "usage: test_llt roundoff [max_n] [iterations] [threads] | bench p... | bench-fft p... | estimate p...\n"
                    "              | scale p threads... | threads-check from to\n");
    return 1;
}
