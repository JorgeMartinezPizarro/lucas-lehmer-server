# lucas-lehmer-server

HTTP servers that run the Lucas-Lehmer test (LLT), which decides whether the Mersenne number `2^p - 1` is prime. There are two implementations, Go and C, and they expose the same endpoint. They are used by [math.ideniox.com](https://math.ideniox.com) and for benchmarking.

| Server | Port | Implementation |
|---|---|---|
| Go | 5002 | `net/http` + GMP through cgo ([ncw/gmp](https://github.com/ncw/gmp)) |
| C | 5004 | Flask/gunicorn calling a pthreads binary: GMP (`mpn`) below `p = 4000`, an FFT ([FFTW](https://www.fftw.org)) from there on |

## How the test is computed

Every step squares a `p`-bit number modulo `2^p - 1`.

- **GMP (Go, and C below `p = 4000`):** squares with GMP and reduces with shifts and masks instead of dividing, because `2^p ≡ 1`. The division used to take about 75% of the time.
- **IBDWT (C from `p = 4000`):** the irrational-base discrete weighted transform of Crandall and Fagin, the method used by Prime95 and gpuowl.
  - The number is split into `n` digits of about 18-24 bits.
  - Each digit is multiplied by a weight `2^(ceil(pj/n) - pj/n)`. With these weights, the FFT's cyclic convolution already computes modulo `2^p - 1`.
  - A step is: real FFT, square pointwise, inverse FFT, unweight, round, carry and weight again. The digits stay weighted between steps.
  - With `r_j = (−p·j) mod n`, the weight is `2^(r_j/n)` and the width is `⌊p/n⌋ + (r_j < p mod n)`, and `r_(j+1)` follows from `r_j` with a subtraction, so widths are computed on the fly.
  - Long FFTs (`n` ≥ 131072) are limited by memory bandwidth, so their weights also come from two tables of about `√n` entries, and so do the four-step FFT's twiddles. On the laptop this made the 38th exponent 20% faster on 12 threads. With 12 tests running at once (like a range scan), `p` = 2582093 got about 27% faster per test.
  - Short FFTs fit in the cache and read their weights from arrays, which is faster there.
  - Digit sizes are calibrated so the round-off error stays around 0.2. The error is checked on every step, and if it ever exceeds 0.4 the test is redone with a longer FFT, so results are always exact.

Single exponent on one core of an i5-11400F:

| p | GMP | FFT |
|---|---|---|
| 86243 | 8.5 s | 1.8 s |
| 216091 | 81 s | 16 s |
| 6972593 | | ~10 h (projected) |

### Threads

The C server tests one exponent per thread, largest first.

- Threads that have no exponent left to start become spare. This happens when the request has fewer exponents than threads, or near the end of a batch.
- FFT tests long enough to benefit (FFT length 131072 and up, so `p` above about 1.6 million) borrow spare threads while they run and give them back when they finish.
- A test with several threads uses a four-step FFT (`c/fourstep.c`) instead of FFTW's real FFT. FFTW's own threaded FFTs turned out slower than one thread at these sizes.
  - The `n` real digits are read as `n/2` complex numbers, and the complex FFT of length `N = N1·N2` is done as column FFTs, a twiddle and row FFTs, with FFTW single-thread plans.
  - The output stays in transposed order. The squaring pairs each frequency with its mirror `N − k` to square the real spectrum directly, then the steps run backwards.
  - The work is cut into about 32 chunks of columns and of rows (each row together with its mirror row), and a step needs only three barriers.
  - Threads take chunks as they become free, so on hybrid CPUs the fast cores do more of them. The plans don't depend on the number of threads, so a test can take more threads without planning again.
  - The carry is done in blocks, then each block's carry is passed to the next one.

So the largest exponent of a batch, which used to finish alone on one core, also uses the cores that the others free up.

Measured on an i5-1235U laptop (2 performance cores + 8 efficiency cores, 15 W), one exponent on several threads:

| p | FFT length | 1 thread | best | |
|---|---|---|---|---|
| 1310719 | 65536 | 0.33 ms/step | 0.31 ms/step | x1.08, so this length keeps one thread |
| 2582093 | 131072 | 0.97 ms/step | 0.68 ms/step | x1.43 |
| 6972593 | 393216 | 3.42 ms/step (6.6 h) | 1.94 ms/step (3.8 h) | x1.76 |

On that laptop, memory bandwidth and the power limit cap the gain. The whole laptop only does 3x the work of one core, even with independent tests. A desktop should scale better. On hybrid CPUs, more threads isn't always faster: on the laptop, 4 parallel tests did more work than 8 or 12. Check yours with `make scale`.

### How far your machine goes

```
make estimate P="6972593 13466917"             # projected time for a full test on one core
make scale P=6972593 THREADS="1 2 4 8 16 20"   # time per step with each number of threads
```

## API

### `POST /lucas-lehmer-test`

Both servers serve exactly the same endpoint; only the port changes.

```
curl -X POST localhost:5002/lucas-lehmer-test \
  -H 'Content-Type: application/json' \
  -d '{"numbers": [3, 5, "7", 11], "num_threads": 4}'
```

```json
{"results": [{"p": 11, "is_prime": false}, {"p": 7, "is_prime": true}, {"p": 5, "is_prime": true}, {"p": 3, "is_prime": true}]}
```

- `numbers`: the exponents `p`, given as JSON integers or decimal strings, with `0 <= p <= 2147483647`. Duplicates are kept.
- `num_threads`: optional. The number of threads to use in total. Defaults to the number of CPUs.
- Results are sorted by `p` in descending order, which is also the order they run in, so the slowest tests start first.
- Invalid input returns `400` with `{"error": "..."}`.

## Usage

```
make build   # build both images (Go unit tests and the C FFT-vs-GMP check run during the build)
make start   # docker compose up -d
make test    # smoke test against the running servers (python3, standard library only)
make stop    # docker compose down
make push    # push to Docker Hub (repository owner only)
```

To use your own Docker Hub repository, override the user, for example `make build push DOCKER_USER=you`.

Images: https://hub.docker.com/r/jorgemartinezpizarro/lucas-lehmer-server/tags

## License

[GNU GPL v3](LICENSE) or later. The C server links [FFTW](https://www.fftw.org), which is GPL v2 or later, and [GMP](https://gmplib.org), which is LGPL v3 or GPL v2.
