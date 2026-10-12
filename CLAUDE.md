# CLAUDE.md

HTTP servers that run the Lucas-Lehmer primality test (LLT) on Mersenne exponents `p` (tests whether `2^p - 1` is prime). Originally a cross-language benchmark; only the Go and C servers remain.

## Layout

- `go/main.go` — Go server (gorilla/mux + `github.com/ncw/gmp`). There is no committed `go.mod`; the Dockerfile runs `go mod init` + `go mod tidy` at build time.
- `c/lucas_lehmer.c` — C LLT on raw GMP limbs (`mpn_sqr` + a custom Mersenne reduction), multithreaded with pthreads. Reads numbers from stdin, prints a JSON array to stdout.
- `c/app.py` — Flask app served by gunicorn that wraps the C binary through `subprocess`.
- `docker-compose.yml`, `Makefile` — build, push and run the images `jorgemartinezpizarro/lucas-lehmer-server:{go,c}`.

## Endpoints

| Server | Host port | Route | Request body | Response |
|---|---|---|---|---|
| Go | 5002 (container 8080) | `POST /lltp` | `{"numbers": "3,5,7", "num_threads": 4}` (comma-separated string) | `[{"p": "7", "isPrime": true}, ...]` (order not guaranteed) |
| C | 5004 (container 8000) | `POST /lucas-lehmer` | `{"numbers": [3, 5, 7]}` (JSON array) | `{"results": [{"p": 7, "is_prime": true}, ...]}` (sorted by `p` descending) |

The two APIs do not share request or response formats.

## Build and run

Everything builds inside Docker:

```
make build    # build both images
make start    # docker compose up -d
make stop     # docker compose down
make push     # Docker Hub repo owner only
```

The C image compiles with `-march=native`, so a pushed image may not run on a different CPU.

Reference LLT for checking results: `s = 4; repeat p-2 times: s = (s*s - 2) mod (2^p - 1)`. `p` is prime-Mersenne iff `s == 0` (with `p = 2` as a special case). Known exponents below 200: 2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127.
