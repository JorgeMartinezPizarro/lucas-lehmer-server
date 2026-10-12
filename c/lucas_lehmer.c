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
/* Arithmetic mod M_p = 2^p - 1 on n = ceil(p / 64) limbs    */
/*                                                           */
/* Values are kept in [0, M_p]; both 0 and M_p mean zero.    */
/* --------------------------------------------------------- */

static inline mp_limb_t top_mask(int p)
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
                                   int p)
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
static inline void sub2_mersenne(mp_limb_t* s, size_t n, int p)
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
static inline bool is_zero_mersenne(const mp_limb_t* s, size_t n, int p)
{
    if (s[0] == 0 && high_limbs_zero(s, n))
        return true;

    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] != GMP_NUMB_MAX) return false;
    return s[n - 1] == top_mask(p);
}

/* --------------------------------------------------------- */

static bool lucas_lehmer(int p)
{
    if (p == 2) return true;
    if (!is_prime_u32((uint32_t)p)) return false;

    size_t n = (p + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS;

    mp_limb_t* s = calloc(n, sizeof(mp_limb_t));
    mp_limb_t* t = calloc(2 * n, sizeof(mp_limb_t));
    mp_limb_t* h = calloc(n + 1, sizeof(mp_limb_t));

    s[0] = 4;

    for (int i = 0; i < p - 2; i++) {
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