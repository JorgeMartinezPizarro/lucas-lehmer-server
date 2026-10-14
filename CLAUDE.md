# CLAUDE.md

HTTP servers that run the Lucas-Lehmer primality test (LLT) on Mersenne exponents `p` (tests whether `2^p - 1` is prime). There are two implementations, Go and C, which serve the same endpoint and must stay identical. math.ideniox.com calls it only from a debug endpoint, so if the API changes, tell the user that client needs updating too.

## Layout

- `go/main.go`: the Go server (`net/http` + `github.com/ncw/gmp` through cgo). `go/main_test.go` compares the LLT with a `math/big` reference and covers the endpoint.
- `c/lucas_lehmer.c`: the C server binary's `main`. It reads numbers from stdin, takes the total thread count as `argv[1]`, and prints a JSON array to stdout. It runs one worker per exponent (largest first). Threads beyond the exponent count, and workers with nothing left to start, go to the spare pool through `llt_add_spare_threads`.
- `c/llt.c`: picks the method, `llt_mpn` below `FFT_MIN_EXPONENT` (4000, the measured crossover) and `llt_ibdwt` from there on.
- `c/llt_mpn.c`: the LLT on raw GMP limbs (`mpn_sqr` + reduction mod `2^p - 1` with shifts).
- `c/llt_fft.c`: the IBDWT (Crandall-Fagin) on FFTW's real FFTs.
  - `max_bits()` gives bits per digit for each length. It was calibrated with `test_llt roundoff`, where the max round-off error is about `2^(2b - 51.8) * n^0.6`, which keeps it near 0.2.
  - If the error goes above 0.4, `llt_ibdwt` retries with a longer FFT.
  - Plans use `FFTW_ESTIMATE` below n = 8192 and `FFTW_MEASURE` from there on.
  - The digits stay weighted between steps: `carry()` unweights, rounds, carries and weights again, which saves a separate weighting pass. The carries passed between blocks unweight and reweight the few digits they touch (`add_to_digit`).
  - `Digits` handles widths and weights. With `r_j = (-pj) mod n`, the width is `w_j = p/n + (r_j < p%n)` (on the fly), the weight is `a_j = 2^(r_j/n)`, and `r_(j+1) = (r_j - p) mod n`.
    - For n < `WEIGHT_ARRAYS_MAX_LENGTH` (131072), the weights are read from per-digit arrays, because computing them was up to x0.75 slower when everything is in cache.
    - Longer FFTs compute them from two tables of about sqrt(n) entries, because they are memory-bound. An isolated single test is about 5% slower, but 12 concurrent tests at n = 131072 are about 27% faster each, and n = 393216 is 20% faster on 12 threads.
  - On one thread a step uses FFTW's out-of-place r2c/c2r (in-place measured up to 20% slower for short FFTs). On several threads it uses `fourstep.c`, never FFTW's threaded plans, which measured slower than one thread (x0.4-0.8 on an i5-1235U).
    - A test starts with 1 thread, borrows spare threads before its first plan, and checks every 64 steps (`grow`). Only the switch from r2c to four-step plans (trylock on the planner, saving and restoring x because FFTW_MEASURE overwrites it); more threads after that need no planning. Borrowed threads are returned at the end.
    - The carry runs in 4 blocks per thread with `schedule(dynamic)`, then each block's carry ripples into the next one.
    - `THREADS_MIN_LENGTH` (131072) and `DIGITS_PER_THREAD` cap the threads per length. They were measured with `test_llt scale` on an i5-1235U, where n = 65536 gains nothing, 131072 gains x1.57 and 393216 gains x1.62.
- `c/fourstep.c`: squaring on several threads with a four-step complex FFT of length n/2 = N1·N2 (column FFTs, twiddle, row FFTs).
  - It uses FFTW single-thread `plan_many_dft` plans, one set per chunk. A chunk has columns, or rows 0..N2/2 together with their mirror rows N2-r.
  - The real spectrum is squared in transposed order by pairing Z_k with Z_(N-k): `Z'_k = 2(Z_k^2 + (1+w^k) O_k^2)`, with `O_k = (Z_k - conj Z_(N-k))/2i`.
  - Powers of w come from two tables of about sqrt(N) entries.
  - Up to 32 chunks are handed out with `schedule(dynamic)`, so plans do not depend on the thread count. Three barriers per step.
- `c/test_llt.c`: a developer tool. It's built in the image's build stage but not shipped. Modes: `roundoff`, `bench`, `bench-fft`, `estimate`, `scale`, and `threads-check` (forced 2-8 threads vs GMP). Run it with `make estimate P=...` and `make scale P=... THREADS=...`.
- `c/check.sh [limit] [binary]`: checks that FFT and GMP give the same answer for every p up to the limit, using two forced builds, and runs known-answer checks. The Docker build runs it with a limit of 3000. Run it with 20000 or more after touching the FFT.
- `c/app.py`: a Flask app (served by gunicorn) that validates input and runs the C binary through `subprocess`.
- `scripts/smoke_test.py`: an end-to-end check of the running servers (`make test`).
- `docker-compose.yml`, `Makefile`: build, push and run the images `${DOCKER_USER}/lucas-lehmer-server:{go,c}`.

## API

Both servers expose only `POST /lucas-lehmer-test`, with the same behaviour (`scripts/smoke_test.py` checks that their responses are identical). The algorithms can differ: C uses the FFT, while Go still uses GMP.

- Request: `{"numbers": [3, "5", 7], "num_threads": 4}`. `p` can be an int or a digit string with `0 <= p <= 2^31-1`. `num_threads` is the total thread budget; it's optional and defaults to the CPU count.
- Response: `{"results": [{"p": 7, "is_prime": true}, ...]}`, sorted by `p` descending.
- Errors: `400 {"error": "..."}`.

Host ports: Go 5002 (container 8080), C 5004 (container 8000).

## Build and test

Everything builds inside multi-stage Docker images. `go test` and `c/check.sh` run during `docker compose build`, so a broken LLT fails the build. There's no local gcc, so develop the C code in a container with `gcc libc6-dev libgmp-dev libfftw3-dev` installed, and compile `lucas_lehmer.c llt.c llt_mpn.c llt_fft.c fourstep.c` with `-fopenmp` and `-lfftw3 -lgmp -lpthread -lm`. The project is GPL-3.0-or-later (`LICENSE`), because the C server links FFTW, which is GPL.

```
make build    # docker compose build
make start    # docker compose up -d
make test     # python3 scripts/smoke_test.py
make stop     # docker compose down
make push     # Docker Hub repo owner only
```

Keep compiler flags portable (no `-march=native`): the images are pushed to Docker Hub and run on other CPUs.

Reference LLT for checking results: `s = 4`, then `p - 2` times `s = (s*s - 2) mod (2^p - 1)`. `2^p - 1` is prime iff `s == 0`, with `p = 2` as a special case. Known exponents up to 5000: 2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423.
