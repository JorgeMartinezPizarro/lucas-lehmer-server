package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"sync"

	"github.com/gorilla/mux"
	"github.com/ncw/gmp"
)

// lucasLehmerTest using gmp for high precision arithmetic
func lucasLehmerTest(p *gmp.Int) bool {
	if p.Cmp(gmp.NewInt(2)) == 0 {
		return true
	}

	s := gmp.NewInt(4)
	two := gmp.NewInt(2)
	mersenneNumber := new(gmp.Int).Sub(new(gmp.Int).Exp(gmp.NewInt(2), p, nil), gmp.NewInt(1))

	for i := int64(0); i < p.Int64()-2; i++ {
		s.Mul(s, s)
		s.Sub(s, two)
		s.Mod(s, mersenneNumber)
	}

	return s.Cmp(gmp.NewInt(0)) == 0
}

    
// Worker function for processing numbers
func worker(id int, numbers <-chan *gmp.Int, results chan<- struct {
	P       string `json:"p"`
	IsPrime bool   `json:"isPrime"`
}, wg *sync.WaitGroup) {
	defer wg.Done()
	for num := range numbers {
		fmt.Printf("Thread %d started processing number: %s\n", id, num.String()) // Log que indica el inicio
		isPrime := lucasLehmerTest(num)
		results <- struct {
			P       string `json:"p"`
			IsPrime bool   `json:"isPrime"`
		}{
			P:       num.String(),
			IsPrime: isPrime,
		}
	}
}

// LLTPHandler to handle incoming HTTP requests for LLT
func LLTPHandler(w http.ResponseWriter, r *http.Request) {
	var requestData struct {
		Numbers    string `json:"numbers"`
		NumThreads int    `json:"num_threads"`
	}

	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	numberStrs := strings.Split(requestData.Numbers, ",")
	var numbers []*gmp.Int
	for _, numStr := range numberStrs {
		num := new(gmp.Int)
		if _, ok := num.SetString(numStr, 10); !ok {
			http.Error(w, fmt.Sprintf("Invalid number: %s", numStr), http.StatusBadRequest)
			return
		}
		numbers = append(numbers, num)
	}

	numThreads := requestData.NumThreads
	if numThreads <= 0 {
		numThreads = 1
	}

	// Create channels for task distribution and results
	tasks := make(chan *gmp.Int, len(numbers))
	results := make(chan struct {
		P       string `json:"p"`
		IsPrime bool   `json:"isPrime"`
	}, len(numbers))

	// Start worker goroutines
	var wg sync.WaitGroup
	for i := 0; i < numThreads; i++ {
		wg.Add(1)
		go worker(i, tasks, results, &wg)
	}

	// Send tasks to workers
	go func() {
		for _, num := range numbers {
			tasks <- num
		}
		close(tasks)
	}()

	// Wait for workers to finish and close the results channel
	go func() {
		wg.Wait()
		close(results)
	}()

	// Collect results
	var primeResults []struct {
		P       string `json:"p"`
		IsPrime bool   `json:"isPrime"`
	}
	for result := range results {
		primeResults = append(primeResults, result)
	}

	json.NewEncoder(w).Encode(primeResults)
}

func main() {
	r := mux.NewRouter()
	r.HandleFunc("/lltp", LLTPHandler).Methods("POST")
	http.Handle("/", r)

	port := ":8080"
	fmt.Println("Server running on port", port)
	if err := http.ListenAndServe(port, nil); err != nil {
		fmt.Println("Error starting server:", err)
	}
}
