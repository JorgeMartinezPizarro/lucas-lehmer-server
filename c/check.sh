#!/bin/sh
# Checks the LLT, also run while building the Docker image:
#   1. the FFT and GMP methods agree for every p <= LIMIT (builds the server
#      twice, forcing each method, and runs both on all CPUs);
#   2. if BINARY is given, it finds the known Mersenne exponents up to 44497
#      and rejects the next prime after each of them that is not one.
#
# usage: ./check.sh [limit] [binary]   (from c/, needs gcc, libgmp-dev and libfftw3-dev)
set -e

LIMIT=${1:-3000}
BINARY=$2
OUT=${TMPDIR:-/tmp}
SRC="lucas_lehmer.c llt.c llt_mpn.c llt_fft.c fourstep.c"
LIBS="-lfftw3 -lgmp -lpthread -lm"

gcc -O3 -fopenmp -Wall -Wextra -DFFT_MIN_EXPONENT=4294967295u -o "$OUT/llt_gmp_only" $SRC $LIBS
gcc -O3 -fopenmp -Wall -Wextra -DFFT_MIN_EXPONENT=64 -o "$OUT/llt_fft_only" $SRC $LIBS

seq 0 "$LIMIT" | "$OUT/llt_gmp_only" > "$OUT/llt_gmp.json"
seq 0 "$LIMIT" | "$OUT/llt_fft_only" > "$OUT/llt_fft.json"

if cmp -s "$OUT/llt_gmp.json" "$OUT/llt_fft.json"; then
    echo "FFT and GMP agree for every p <= $LIMIT"
else
    echo "FFT and GMP DISAGREE for some p <= $LIMIT:"
    tr '{' '\n' < "$OUT/llt_gmp.json" > "$OUT/a"
    tr '{' '\n' < "$OUT/llt_fft.json" > "$OUT/b"
    diff "$OUT/a" "$OUT/b" | head -20
    exit 1
fi

[ -n "$BINARY" ] || exit 0

KNOWN="2 3 5 7 13 17 19 31 61 89 107 127 521 607 1279 2203 2281 3217 4253 4423 9689 9941 11213 19937 21701 23209 44497"
NEXT=$(echo $KNOWN | awk '
    function isprime(n,  i) { for (i = 2; i * i <= n; i++) if (n % i == 0) return 0; return n >= 2 }
    {
        for (k = 1; k <= NF; k++) known[$k] = 1
        for (k = 1; k <= NF; k++) { n = $k + 1; while (!isprime(n) || n in known) n++; printf "%d ", n }
    }')

primes() { "$BINARY" | grep -o '"is_prime":true' | wc -l; }
found=$(echo $KNOWN | primes)
wrong=$(echo $NEXT | primes)

if [ "$found" -eq 27 ] && [ "$wrong" -eq 0 ]; then
    echo "$BINARY finds the 27 Mersenne exponents up to 44497 and rejects their next primes"
else
    echo "$BINARY found $found of 27 known exponents and $wrong false positives in: $NEXT"
    exit 1
fi
