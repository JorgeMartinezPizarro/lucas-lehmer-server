#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <gmp.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    int p;
    bool result;
} TestResult;

typedef struct {
    int p;
    int index;
} ThreadInput;

TestResult* results;
pthread_t* threads;

void* thread_func(void* arg) {
    ThreadInput* input = (ThreadInput*)arg;
    int p = input->p;
    int index = input->index;

    mpz_t m, s, two;
    mpz_inits(m, s, two, NULL);

    // m = 2^p - 1
    mpz_ui_pow_ui(m, 2, p);
    mpz_sub_ui(m, m, 1);

    mpz_set_ui(s, 4);
    mpz_set_ui(two, 2);

    for (int i = 0; i < p - 2; ++i) {
        mpz_mul(s, s, s);
        mpz_sub(s, s, two);
        mpz_mod(s, s, m);
    }

    results[index].p = p;
    results[index].result = mpz_cmp_ui(s, 0) == 0;

    mpz_clears(m, s, two, NULL);
    free(arg);
    return NULL;
}

int main() {
    char buffer[8192];
    fread(buffer, 1, sizeof(buffer), stdin);

    int* numbers = malloc(sizeof(int) * 10000);
    int count = 0;
    char* token = strtok(buffer, "[,]");
    while (token) {
        numbers[count++] = atoi(token);
        token = strtok(NULL, "[,]");
    }

    results = malloc(sizeof(TestResult) * count);
    threads = malloc(sizeof(pthread_t) * count);

    for (int i = 0; i < count; ++i) {
        ThreadInput* input = malloc(sizeof(ThreadInput));
        input->p = numbers[i];
        input->index = i;
        pthread_create(&threads[i], NULL, thread_func, input);
    }

    for (int i = 0; i < count; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("[");
    for (int i = 0; i < count; ++i) {
        printf("{\"p\": %d, \"is_prime\": %s}%s", results[i].p, results[i].result ? "true" : "false", i < count - 1 ? "," : "");
    }
    printf("]\n");

    free(numbers);
    free(results);
    free(threads);
    return 0;
}
