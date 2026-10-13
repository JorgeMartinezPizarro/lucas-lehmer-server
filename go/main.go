package main

import (
	"cmp"
	"context"
	"encoding/json"
	"fmt"
	"log"
	"math"
	"net/http"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"sync"

	"github.com/ncw/gmp"
)

// maxExponent matches the C server, which stores p in an int.
const maxExponent = math.MaxInt32

// isPrime reports whether n is prime. 2^p - 1 can only be prime if p is.
func isPrime(n uint32) bool {
	if n < 2 {
		return false
	}
	if n%2 == 0 {
		return n == 2
	}
	for i := uint64(3); i*i <= uint64(n); i += 2 {
		if uint64(n)%i == 0 {
			return false
		}
	}
	return true
}

// lucasLehmer reports whether the Mersenne number 2^p - 1 is prime.
func lucasLehmer(p uint32) bool {
	if p == 2 {
		return true
	}
	if !isPrime(p) {
		return false
	}

	m := new(gmp.Int).Lsh(gmp.NewInt(1), uint(p))
	m.SubUint32(m, 1)

	s := gmp.NewInt(4)
	hi := new(gmp.Int)

	// s stays in [0, M_p) after every iteration.
	for i := uint32(0); i < p-2; i++ {
		s.Mul(s, s)

		// 2^p = 1 (mod M_p), so s = hi * 2^p + lo = hi + lo < 2 * M_p
		hi.Rsh(s, uint(p))
		s.And(s, m)
		s.Add(s, hi)
		if s.Cmp(m) >= 0 {
			s.Sub(s, m)
		}

		if s.CmpUint32(2) < 0 {
			s.Add(s, m)
		}
		s.SubUint32(s, 2)
	}

	return s.Sign() == 0
}

type result struct {
	P       uint32 `json:"p"`
	IsPrime bool   `json:"is_prime"`
}

// testAll runs the LLT on every exponent with the given number of workers.
// Results are sorted by p in descending order, the same order they are
// scheduled in, so the slowest tests start first. Workers stop picking new
// exponents once ctx is done.
func testAll(ctx context.Context, ps []uint32, workers int) []result {
	sorted := slices.Clone(ps)
	slices.SortFunc(sorted, func(a, b uint32) int { return cmp.Compare(b, a) })

	results := make([]result, len(sorted))
	indexes := make(chan int)

	var wg sync.WaitGroup
	for w := 0; w < min(workers, len(sorted)); w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := range indexes {
				p := sorted[i]
				log.Printf("worker %d: testing p=%d", w, p)
				results[i] = result{P: p, IsPrime: lucasLehmer(p)}
			}
		}()
	}

	for i := range sorted {
		if ctx.Err() != nil {
			break
		}
		indexes <- i
	}
	close(indexes)
	wg.Wait()

	return results
}

// parseExponent accepts a JSON integer or a string of decimal digits.
func parseExponent(raw json.RawMessage) (uint32, error) {
	text := string(raw)
	if strings.HasPrefix(text, `"`) {
		if err := json.Unmarshal(raw, &text); err != nil {
			return 0, err
		}
		text = strings.TrimSpace(text)
	}

	p, err := strconv.ParseUint(text, 10, 32)
	if err != nil || p > maxExponent {
		return 0, fmt.Errorf("invalid exponent %s, expected an integer between 0 and %d", raw, maxExponent)
	}
	return uint32(p), nil
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}

func writeError(w http.ResponseWriter, msg string) {
	writeJSON(w, http.StatusBadRequest, map[string]string{"error": msg})
}

// lucasLehmerHandler serves the API shared with the C server:
//
//	POST /lucas-lehmer-test {"numbers": [3, "5", 7], "num_threads": 4}
//	-> {"results": [{"p": 7, "is_prime": true}, ...]}
//
// num_threads is optional and defaults to the number of CPUs.
func lucasLehmerHandler(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Numbers    []json.RawMessage `json:"numbers"`
		NumThreads *json.RawMessage  `json:"num_threads"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.Numbers == nil {
		writeError(w, "Missing or invalid 'numbers' list")
		return
	}

	ps := make([]uint32, len(req.Numbers))
	for i, raw := range req.Numbers {
		p, err := parseExponent(raw)
		if err != nil {
			writeError(w, err.Error())
			return
		}
		ps[i] = p
	}

	workers := runtime.NumCPU()
	if req.NumThreads != nil && string(*req.NumThreads) != "null" {
		n, err := strconv.Atoi(string(*req.NumThreads))
		if err != nil || n < 1 {
			writeError(w, "'num_threads' must be a positive integer")
			return
		}
		workers = n
	}

	writeJSON(w, http.StatusOK, map[string][]result{"results": testAll(r.Context(), ps, workers)})
}

func newMux() *http.ServeMux {
	mux := http.NewServeMux()
	mux.HandleFunc("POST /lucas-lehmer-test", lucasLehmerHandler)
	return mux
}

func main() {
	addr := ":8080"
	log.Println("Server running on port", addr)
	log.Fatal(http.ListenAndServe(addr, newMux()))
}
