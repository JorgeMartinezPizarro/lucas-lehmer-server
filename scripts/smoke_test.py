#!/usr/bin/env python3
"""Smoke test for the running servers (make start && make test).

Checks both servers against known Mersenne exponents, checks that they give
identical answers on the shared API, and checks input validation. Uses only
the standard library.

    python3 scripts/smoke_test.py [--go URL] [--c URL]
"""
import argparse
import json
import sys
import urllib.error
import urllib.request

KNOWN = [2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423]
NOT_MERSENNE = [0, 1, 4, 11, 23, 29, 37, 67, 97, 509, 523, 1277, 2207, 4447]  # composites and primes p with 2^p - 1 composite

failures = 0


def check(name, ok, detail=""):
    global failures
    print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  ({detail})" if not ok and detail else ""))
    failures += not ok


def post(base, path, body):
    data = body if isinstance(body, bytes) else json.dumps(body).encode()
    req = urllib.request.Request(base + path, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=600) as resp:
            return resp.status, resp.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


def expected_results(numbers):
    return [{"p": p, "is_prime": p in KNOWN} for p in sorted(numbers, reverse=True)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--go", default="http://localhost:5002")
    parser.add_argument("--c", default="http://localhost:5004")
    args = parser.parse_args()
    servers = {"go": args.go, "c": args.c}

    numbers = KNOWN + NOT_MERSENNE + [127]  # includes a duplicate
    request_numbers = numbers[:-1] + ["127"]  # digit strings are accepted too
    want = expected_results(numbers)

    responses = {}
    for name, base in servers.items():
        status, body = post(base, "/lucas-lehmer-test", {"numbers": request_numbers})
        ok = status == 200 and json.loads(body).get("results") == want
        check(f"{name}: known exponents up to 4423", ok, f"{status} {body[:200]}")
        responses[name] = body

        status, body = post(base, "/lucas-lehmer-test", {"numbers": [127, 61, 89, 67], "num_threads": 2})
        ok = status == 200 and json.loads(body).get("results") == expected_results([127, 61, 89, 67])
        check(f"{name}: num_threads=2", ok, f"{status} {body[:200]}")

        status, body = post(base, "/lucas-lehmer-test", {"numbers": []})
        check(f"{name}: empty list", status == 200 and json.loads(body) == {"results": []}, f"{status} {body}")

        for bad in [b"not json", {}, {"numbers": "3,5"}, {"numbers": [-7]}, {"numbers": [7.5]},
                    {"numbers": [True]}, {"numbers": [None]}, {"numbers": ["abc"]},
                    {"numbers": [2**31]}, {"numbers": [7], "num_threads": 0},
                    {"numbers": [7], "num_threads": "4"}]:
            status, body = post(base, "/lucas-lehmer-test", bad)
            ok = status == 400 and "error" in json.loads(body)
            check(f"{name}: rejects {bad!r}", ok, f"{status} {body}")

        for old in ["/lltp", "/lucas-lehmer"]:
            status, _ = post(base, old, {"numbers": [7]})
            check(f"{name}: old route {old} is gone", status == 404, str(status))

    same = json.loads(responses["go"]) == json.loads(responses["c"])
    check("go and c give identical results", same)

    print(f"\n{failures} failure(s)" if failures else "\nall checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
