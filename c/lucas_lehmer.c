#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "llt.h"

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

static void* worker(void* arg)
{
    WorkerContext* ctx = (WorkerContext*)arg;
    WorkQueue* q = ctx->queue;

    while (1) {
        int i = __atomic_fetch_add(&q->next_index, 1, __ATOMIC_RELAXED);
        if (i >= q->count) break;

        int p = q->numbers[i];
        ctx->results[i].p = p;
        ctx->results[i].is_prime = lucas_lehmer((uint32_t)p);
    }

    /* Nothing left to start: lend this thread to the tests still running */
    llt_add_spare_threads(1);

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

/*
 * usage: lucas_lehmer [threads] < numbers (threads defaults to the CPU count)
 *
 * Each worker thread tests one exponent at a time, largest first. Threads
 * without an exponent to start are spare, and long FFT tests borrow them.
 */
int main(int argc, char** argv)
{
    int* nums;
    int count = parse_input(&nums);

    if (count <= 0) return 1;

    qsort(nums, count, sizeof(int), cmp_desc);

    TestResult* res = malloc(count * sizeof(TestResult));

    WorkQueue q = { nums, count, 0 };

    long threads = argc > 1 ? strtol(argv[1], NULL, 10)
                            : sysconf(_SC_NPROCESSORS_ONLN);
    if (threads < 1) threads = 4;

    long workers = threads < count ? threads : count;
    llt_add_spare_threads((int)(threads - workers));

    pthread_t* th = malloc(workers * sizeof(pthread_t));
    WorkerContext ctx = { &q, res };

    for (long i = 0; i < workers; i++)
        pthread_create(&th[i], NULL, worker, &ctx);

    for (long i = 0; i < workers; i++)
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