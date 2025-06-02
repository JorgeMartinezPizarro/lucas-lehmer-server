#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_NUMBERS 100

typedef struct {
    int p;
    bool result;
} TestResult;

typedef struct {
    int p;
    int index;
} ThreadInput;

TestResult results[MAX_NUMBERS];

bool is_mersenne_prime(int p) {
    if (p == 2) return true;
    long long m = (1LL << p) - 1;
    long long s = 4;
    for (int i = 0; i < p - 2; ++i) {
        s = ((s * s) - 2) % m;
    }
    return s == 0;
}

void* thread_func(void* arg) {
    ThreadInput* input = (ThreadInput*)arg;
    results[input->index].p = input->p;
    results[input->index].result = is_mersenne_prime(input->p);
    free(arg);
    return NULL;
}

int main() {
    char buffer[4096];
    fread(buffer, 1, sizeof(buffer), stdin);

    // Esperamos un JSON como [3, 5, 7]
    int numbers[MAX_NUMBERS], count = 0;
    char* token = strtok(buffer, "[,]");
    while (token && count < MAX_NUMBERS) {
        numbers[count++] = atoi(token);
        token = strtok(NULL, "[,]");
    }

    pthread_t threads[MAX_NUMBERS];
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

    return 0;
}
