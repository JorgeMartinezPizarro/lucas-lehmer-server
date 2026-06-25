#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <gmp.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

/* --------------------------------------------------------- */

typedef struct {
    int p;
    bool is_prime;
} TestResult;

typedef struct {
    int* numbers;
    int count;
    volatile int next_index;
} WorkQueue;

typedef struct {
    WorkQueue* queue;
    TestResult* results;
} WorkerContext;

/* --------------------------------------------------------- */

static bool is_prime_u32(uint32_t n)
{
    if (n < 2) return false;
    if (n < 4) return n > 1 && (n & 1);
    if ((n & 1) == 0) return false;

    for (uint32_t i = 3; (uint64_t)i * i <= n; i += 2)
        if (n % i == 0) return false;

    return true;
}

/* --------------------------------------------------------- */
/* Mersenne reduction (safe, simple)                        */
/* --------------------------------------------------------- */

static inline void reduce_mersenne(mp_limb_t* x,
                                   size_t* xn,
                                   int p)
{
    size_t limbs = (p + GMP_LIMB_BITS - 1) / GMP_LIMB_BITS;

    /* fold high limbs */
    for (size_t i = limbs; i < *xn; i++) {
        size_t j = i - limbs;
        x[j] += x[i];
    }

    *xn = limbs;

    /* final carry fix */
    for (size_t i = 0; i + 1 < *xn; i++) {
        x[i + 1] += x[i] >> GMP_LIMB_BITS;
        x[i] &= GMP_NUMB_MASK;
    }

    /* reduce overflow bits beyond p */
    size_t excess_bits = (*xn * GMP_LIMB_BITS) - p;
    if (excess_bits > 0 && *xn > 0) {
        x[0] &= ((mp_limb_t)1 << (GMP_LIMB_BITS - excess_bits)) - 1;
    }
}

/* --------------------------------------------------------- */

static bool lucas_lehmer(int p)
{
    if (p == 2) return true;
    if (!is_prime_u32((uint32_t)p)) return false;

    size_t max_limbs = (p / GMP_LIMB_BITS) + 3;

    mp_limb_t* s  = calloc(max_limbs, sizeof(mp_limb_t));
    mp_limb_t* t  = calloc(max_limbs * 2, sizeof(mp_limb_t));

    size_t sn = 1;
    s[0] = 4;

    for (int i = 0; i < p - 2; i++) {

        /* square: t = s*s */
        mpn_sqr(t, s, sn);

        sn *= 2;

        /* reduce mod Mersenne */
        reduce_mersenne(t, &sn, p);

        /* copy back */
        memcpy(s, t, sn * sizeof(mp_limb_t));

        /* s = s - 2 mod M_p */
        if (sn == 0) {
            s[0] = 0;
            sn = 1;
        }

        if (s[0] < 2) {
            s[0] += ((mp_limb_t)1 << GMP_LIMB_BITS) - 2;
            if (sn > 1) s[1]--;
        } else {
            s[0] -= 2;
        }
    }

    bool ok = (sn == 1 && s[0] == 0);

    free(s);
    free(t);

    return ok;
}

/* --------------------------------------------------------- */

static void* worker(void* arg)
{
    WorkerContext* ctx = (WorkerContext*)arg;
    WorkQueue* q = ctx->queue;

    while (1) {
        int i = __atomic_fetch_add(&q->next_index, 1, __ATOMIC_RELAXED);
        if (i >= q->count) break;

        int p = q->numbers[i];
        ctx->results[i].p = p;
        ctx->results[i].is_prime = lucas_lehmer(p);
    }

    return NULL;
}

/* --------------------------------------------------------- */

static int parse_input(int** out)
{
    size_t cap = 1024, n = 0;
    int* a = malloc(cap * sizeof(int));

    int v = 0, in = 0, c;

    while ((c = getchar()) != EOF) {
        if (isdigit(c)) {
            v = v * 10 + (c - '0');
            in = 1;
        } else if (in) {
            if (n == cap) {
                cap *= 2;
                a = realloc(a, cap * sizeof(int));
            }
            a[n++] = v;
            v = 0;
            in = 0;
        }
    }

    if (in) a[n++] = v;

    *out = a;
    return (int)n;
}

/* --------------------------------------------------------- */

static int cmp_desc(const void* a, const void* b)
{
    return (*(int*)b - *(int*)a);
}

/* --------------------------------------------------------- */

int main()
{
    int* nums;
    int count = parse_input(&nums);

    if (count <= 0) return 1;

    qsort(nums, count, sizeof(int), cmp_desc);

    TestResult* res = malloc(count * sizeof(TestResult));

    WorkQueue q = { nums, count, 0 };

    long threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (threads < 1) threads = 4;

    pthread_t* th = malloc(threads * sizeof(pthread_t));
    WorkerContext ctx = { &q, res };

    for (long i = 0; i < threads; i++)
        pthread_create(&th[i], NULL, worker, &ctx);

    for (long i = 0; i < threads; i++)
        pthread_join(th[i], NULL);

    printf("[");

    for (int i = 0; i < count; i++) {
        printf("{\"p\":%d,\"is_prime\":%s}%s",
            res[i].p,
            res[i].is_prime ? "true" : "false",
            (i + 1 < count ? "," : ""));
    }

    printf("]\n");

    free(nums);
    free(res);
    free(th);
}