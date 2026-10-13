# lucas-lehmer-server

HTTP servers that run the Lucas-Lehmer test (LLT), which decides whether the Mersenne number `2^p - 1` is prime. There are two implementations, Go and C, and they expose the same endpoint. They are used by [math.ideniox.com](https://math.ideniox.com) and for benchmarking.

| Server | Port | Implementation |
|---|---|---|
| Go | 5002 | `net/http` + GMP through cgo ([ncw/gmp](https://github.com/ncw/gmp)) |
| C | 5004 | Flask/gunicorn calling a pthreads + GMP (`mpn`) binary |

Both servers reduce modulo `2^p - 1` with shifts and masks (because `2^p ≡ 1`) instead of dividing, so almost all of the time goes into GMP's squaring. On a 12-core machine, both test `p = 86243` in about 8 s.

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
- `num_threads`: optional. Defaults to the number of CPUs.
- Results are sorted by `p` in descending order, which is also the order they run in, so the slowest tests start first.
- Invalid input returns `400` with `{"error": "..."}`.

## Usage

```
make build   # build both images (Go unit tests and a C sanity check run during the build)
make start   # docker compose up -d
make test    # smoke test against the running servers (python3, standard library only)
make stop    # docker compose down
make push    # push to Docker Hub (repository owner only)
```

To use your own Docker Hub repository, override the user, for example `make build push DOCKER_USER=you`.

Images: https://hub.docker.com/r/jorgemartinezpizarro/lucas-lehmer-server/tags
