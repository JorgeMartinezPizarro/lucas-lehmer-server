#include <cuda_runtime.h>
#include <stdio.h>

__device__ bool lucasLehmerTest(int p) {
    if (p == 2) return true;

    long long s = 4;
    long long m = (1LL << p) - 1;

    for (int i = 0; i < p - 2; i++) {
        s = (s * s - 2) % m;
    }

    return s == 0;
}

__global__ void lucasLehmerKernel(int *exponents, int *results, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        results[idx] = lucasLehmerTest(exponents[idx]) ? 1 : 0;
    }
}

extern "C" void launchLucasLehmer(int *exponents, int *results, int size) {
    int *d_exponents, *d_results;

    cudaMalloc(&d_exponents, size * sizeof(int));
    cudaMalloc(&d_results, size * sizeof(int));

    cudaMemcpy(d_exponents, exponents, size * sizeof(int), cudaMemcpyHostToDevice);

    int threadsPerBlock = 256;
    int blocksPerGrid = (size + threadsPerBlock - 1) / threadsPerBlock;

    lucasLehmerKernel<<<blocksPerGrid, threadsPerBlock>>>(d_exponents, d_results, size);

    cudaMemcpy(results, d_results, size * sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_exponents);
    cudaFree(d_results);
}
