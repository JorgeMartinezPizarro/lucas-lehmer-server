#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <gmp.h>
#include <ctype.h>
#include <unistd.h>

/* --------------------------------------------------------- */
/* Result structure                                          */
/* --------------------------------------------------------- */

typedef struct {
    int p;
    bool is_prime;
} TestResult;

/* --------------------------------------------------------- */
/* Lock-free work queue                                      */
/* --------------------------------------------------------- */

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
/* Small primality test                                      */
/* --------------------------------------------------------- */

static bool is_prime_u32(uint32_t n) {

    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if ((n & 1) == 0) return false;

    for (uint32_t i = 3; (uint64_t)i * i <= n; i += 2) {

        if (n % i == 0) {
            return false;
        }
    }

    return true;
}

/* --------------------------------------------------------- */
/* Fast reduction mod (2^p - 1)                              */
/* --------------------------------------------------------- */

static inline void mersenne_reduce(
    mpz_t x,
    const int p,
    const mpz_t mp,
    mpz_t high
) {

    /*
        Since:

            x < (2^p - 1)^2 < 2^(2p)

        after squaring, we need at most
        two folds.
    */

    /* first fold */

    mpz_fdiv_q_2exp(high, x, p);
    mpz_fdiv_r_2exp(x, x, p);

    mpz_add(x, x, high);

    /* second fold (only if needed) */

    mpz_fdiv_q_2exp(high, x, p);

    if (mpz_cmp_ui(high, 0) != 0) {

        mpz_fdiv_r_2exp(x, x, p);

        mpz_add(x, x, high);
    }

    /* final correction */

    if (mpz_cmp(x, mp) >= 0) {
        mpz_sub(x, x, mp);
    }
}

/* --------------------------------------------------------- */
/* Lucas-Lehmer test                                         */
/* --------------------------------------------------------- */

static bool lucas_lehmer(int p) {

    if (p == 2) {
        return true;
    }

    /*
        LLT only valid for prime exponents
    */

    if (!is_prime_u32((uint32_t)p)) {
        return false;
    }

    mpz_t s;
    mpz_t mp;
    mpz_t high;

    mpz_init_set_ui(s, 4);

    /*
        mp = 2^p - 1
    */

    mpz_init(mp);

    mpz_setbit(mp, p);
    mpz_sub_ui(mp, mp, 1);

    mpz_init(high);

    for (int i = 0; i < p - 2; ++i) {

        /*
            s = s²
        */

        mpz_mul(s, s, s);

        /*
            fast reduction mod M_p
        */

        mersenne_reduce(s, p, mp, high);

        /*
            s = s - 2 mod M_p
        */

        if (mpz_cmp_ui(s, 2) < 0) {
            mpz_add(s, s, mp);
        }

        mpz_sub_ui(s, s, 2);
    }

    bool result = (mpz_cmp_ui(s, 0) == 0);

    mpz_clear(s);
    mpz_clear(mp);
    mpz_clear(high);

    return result;
}

/* --------------------------------------------------------- */
/* Worker thread                                             */
/* --------------------------------------------------------- */

static void* worker_thread(void* arg) {

    WorkerContext* ctx = (WorkerContext*)arg;

    while (true) {

        int index =
            __atomic_fetch_add(
                &ctx->queue->next_index,
                1,
                __ATOMIC_RELAXED
            );

        if (index >= ctx->queue->count) {
            break;
        }

        int p = ctx->queue->numbers[index];

        bool result = lucas_lehmer(p);

        ctx->results[index].p = p;
        ctx->results[index].is_prime = result;
    }

    return NULL;
}

/* --------------------------------------------------------- */
/* Parse integer array from stdin                            */
/* --------------------------------------------------------- */

static int parse_input(int** out_numbers) {

    size_t cap = 1024;
    size_t count = 0;

    int* numbers = malloc(cap * sizeof(int));

    if (!numbers) {
        return -1;
    }

    int value = 0;
    bool in_number = false;

    int c;

    while ((c = getchar()) != EOF) {

        if (isdigit(c)) {

            value = value * 10 + (c - '0');

            in_number = true;

        } else if (in_number) {

            if (count >= cap) {

                cap *= 2;

                int* tmp =
                    realloc(numbers, cap * sizeof(int));

                if (!tmp) {
                    free(numbers);
                    return -1;
                }

                numbers = tmp;
            }

            numbers[count++] = value;

            value = 0;
            in_number = false;
        }
    }

    if (in_number) {
        numbers[count++] = value;
    }

    *out_numbers = numbers;

    return (int)count;
}

/* --------------------------------------------------------- */
/* Sort descending                                           */
/* --------------------------------------------------------- */

static int cmp_desc(const void* a, const void* b) {

    const int ia = *(const int*)a;
    const int ib = *(const int*)b;

    return ib - ia;
}

/* --------------------------------------------------------- */
/* Main                                                      */
/* --------------------------------------------------------- */

int main(void) {

    int* numbers = NULL;

    int count = parse_input(&numbers);

    if (count <= 0) {

        fprintf(stderr, "Invalid input\n");

        return 1;
    }

    /*
        Large exponents first:
        much better load balancing
    */

    qsort(numbers, count, sizeof(int), cmp_desc);

    TestResult* results =
        malloc(sizeof(TestResult) * count);

    if (!results) {

        free(numbers);

        return 1;
    }

    WorkQueue queue = {
        .numbers = numbers,
        .count = count,
        .next_index = 0
    };

    long cpu_count =
        sysconf(_SC_NPROCESSORS_ONLN);

    if (cpu_count < 1) {
        cpu_count = 4;
    }

    pthread_t* threads =
        malloc(sizeof(pthread_t) * cpu_count);

    WorkerContext ctx = {
        .queue = &queue,
        .results = results
    };

    for (long i = 0; i < cpu_count; ++i) {

        pthread_create(
            &threads[i],
            NULL,
            worker_thread,
            &ctx
        );
    }

    for (long i = 0; i < cpu_count; ++i) {

        pthread_join(threads[i], NULL);
    }

    printf("[");

    for (int i = 0; i < count; ++i) {

        printf(
            "{\"p\":%d,\"is_prime\":%s}%s",
            results[i].p,
            results[i].is_prime ? "true" : "false",
            (i + 1 < count) ? "," : ""
        );
    }

    printf("]\n");

    free(numbers);
    free(results);
    free(threads);

    return 0;
}