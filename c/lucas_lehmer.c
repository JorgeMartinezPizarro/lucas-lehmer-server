#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <gmp.h>
#include <string.h>
#include <stdbool.h>
#include <semaphore.h>

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
int count;

sem_t thread_limiter;  // Semáforo para limitar hilos simultáneos

void* thread_func(void* arg) {
    ThreadInput* input = (ThreadInput*)arg;
    int p = input->p;
    int index = input->index;

    mpz_t m, s, two;
    mpz_inits(m, s, two, NULL);

    // m = 2^p - 1
    mpz_ui_pow_ui(m, 2, p);
    mpz_sub_ui(m, m, 1);

    if (p == 2) {
        // 2^2 - 1 = 3, que es primo
        results[index].p = p;
        results[index].result = true;
        mpz_clears(m, s, two, NULL);
        free(arg);
        return NULL;
    }

    mpz_set_ui(s, 4);
    mpz_set_ui(two, 2);

    for (int i = 0; i < p - 2; ++i) {
        mpz_mul(s, s, s);
        mpz_sub(s, s, two);
        mpz_mod(s, s, m);
    }

    results[index].p = p;
    results[index].result = (mpz_cmp_ui(s, 0) == 0);

    mpz_clears(m, s, two, NULL);
    free(arg);
    return NULL;
}

int main() {
    char buffer[65536];  // Más grande por si la entrada es grande
    size_t bytes_read = fread(buffer, 1, sizeof(buffer)-1, stdin);
    buffer[bytes_read] = '\0';

    int max_threads = 8;  // Ajusta según cores y RAM, 8 es conservador
    sem_init(&thread_limiter, 0, max_threads);

    int* numbers = malloc(sizeof(int) * 100000);
    count = 0;

    char* token = strtok(buffer, "[, ]");
    while (token != NULL) {
        int val = atoi(token);
        if(val > 0) {  // Validar número positivo
            numbers[count++] = val;
        }
        token = strtok(NULL, "[, ]");
    }

    results = malloc(sizeof(TestResult) * count);
    threads = malloc(sizeof(pthread_t) * count);

    for (int i = 0; i < count; ++i) {
        sem_wait(&thread_limiter);  // Esperamos que haya slot para nuevo hilo

        ThreadInput* input = malloc(sizeof(ThreadInput));
        input->p = numbers[i];
        input->index = i;
        if (pthread_create(&threads[i], NULL, thread_func, input) != 0) {
            fprintf(stderr, "Error creando hilo %d\n", i);
            results[i].p = numbers[i];
            results[i].result = false;
            sem_post(&thread_limiter);
            free(input);
        }
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
    sem_destroy(&thread_limiter);

    return 0;
}
