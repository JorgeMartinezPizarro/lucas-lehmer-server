# CLAUDE.md

HTTP servers that run the Lucas-Lehmer primality test (LLT) on Mersenne exponents `p` (tests whether `2^p - 1` is prime). There are two implementations, Go and C, which serve the same endpoint and must stay identical. math.ideniox.com calls it only from a debug endpoint, so if the API changes, tell the user that client needs updating too.

## Layout

- `go/main.go`: the Go server (`net/http` + `github.com/ncw/gmp` through cgo). `go/main_test.go` compares the LLT with a `math/big` reference and covers the endpoint.
- `c/lucas_lehmer.c`: the C LLT on raw GMP limbs (`mpn_sqr` + reduction mod `2^p - 1`), multithreaded with pthreads. It reads numbers from stdin, takes an optional thread count as `argv[1]`, and prints a JSON array to stdout.
- `c/app.py`: a Flask app (served by gunicorn) that validates input and runs the C binary through `subprocess`.
- `scripts/smoke_test.py`: an end-to-end check of the running servers (`make test`).
- `docker-compose.yml`, `Makefile`: build, push and run the images `${DOCKER_USER}/lucas-lehmer-server:{go,c}`.

## API

Both servers expose only `POST /lucas-lehmer-test`, with the same behaviour (`scripts/smoke_test.py` checks that their responses are identical):

- Request: `{"numbers": [3, "5", 7], "num_threads": 4}`. `p` can be an int or a digit string with `0 <= p <= 2^31-1`. `num_threads` is optional and defaults to the CPU count.
- Response: `{"results": [{"p": 7, "is_prime": true}, ...]}`, sorted by `p` descending.
- Errors: `400 {"error": "..."}`.

Host ports: Go 5002 (container 8080), C 5004 (container 8000).

## Build and test

Everything builds inside multi-stage Docker images. `go test` and a known-answer check of the C binary run during `docker compose build`, so a broken LLT fails the build.

```
make build    # docker compose build
make start    # docker compose up -d
make test     # python3 scripts/smoke_test.py
make stop     # docker compose down
make push     # Docker Hub repo owner only
```

Keep compiler flags portable (no `-march=native`): the images are pushed to Docker Hub and run on other CPUs.

Reference LLT for checking results: `s = 4`, then `p - 2` times `s = (s*s - 2) mod (2^p - 1)`. `2^p - 1` is prime iff `s == 0`, with `p = 2` as a special case. Known exponents up to 5000: 2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423.
