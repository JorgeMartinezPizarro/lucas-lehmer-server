package main

import (
    "encoding/json"
    "fmt"
    "net/http"
    "sync"
    "github.com/gorilla/mux"
    "strings"
    "github.com/ncw/gmp"
)

// lucasLehmerTest using gmp for high precision arithmetic
func lucasLehmerTest(p *gmp.Int, wg *sync.WaitGroup, results chan<- bool) {
    defer wg.Done()

    if p.Cmp(gmp.NewInt(2)) == 0 {
        results <- true
        return
    }

    s := gmp.NewInt(4)
    two := gmp.NewInt(2)
    mersenneNumber := new(gmp.Int).Sub(new(gmp.Int).Exp(gmp.NewInt(2), p, nil), gmp.NewInt(1))

    for i := int64(0); i < p.Int64()-2; i++ {
        s.Mul(s, s)
        s.Sub(s, two)
        s.Mod(s, mersenneNumber)
    }

    isPrime := s.Cmp(gmp.NewInt(0)) == 0
    results <- isPrime
}

// LLTPHandler to handle incoming HTTP requests for LLT
func LLTPHandler(w http.ResponseWriter, r *http.Request) {
    var requestData struct {
        Numbers    string `json:"numbers"`
        NumThreads int    `json:"num_threads"`
    }

    err := json.NewDecoder(r.Body).Decode(&requestData)
    if err != nil {
        fmt.Println("Error decoding JSON:", err)
        http.Error(w, "Invalid JSON", http.StatusBadRequest)
        return
    }

    fmt.Println("Received request with numbers string:", requestData.Numbers, "and num_threads:", requestData.NumThreads)

    // Split the numbers string by comma and convert them to integers
    numberStrs := strings.Split(requestData.Numbers, ",")
    var numbers []*gmp.Int
    for _, numStr := range numberStrs {
        num := new(gmp.Int)
        if _, ok := num.SetString(numStr, 10); !ok {
            fmt.Println("Invalid number:", numStr)
            http.Error(w, fmt.Sprintf("Invalid number: %s", numStr), http.StatusBadRequest)
            return
        }
        numbers = append(numbers, num)
    }

    var wg sync.WaitGroup
    results := make(chan struct {
        P       string `json:"p"`
        IsPrime bool   `json:"isPrime"`
    }, len(numbers))

    for _, num := range numbers {
        wg.Add(1)
        go func(num *gmp.Int) {
            defer wg.Done()
            var wgLLT sync.WaitGroup
            var result bool
            ch := make(chan bool)
            wgLLT.Add(1)
            go lucasLehmerTest(num, &wgLLT, ch)
            go func() {
                wgLLT.Wait()
                close(ch)
            }()
            result = <-ch
            results <- struct {
                P       string `json:"p"`
                IsPrime bool   `json:"isPrime"`
            }{num.String(), result}
        }(num)
    }

    go func() {
        wg.Wait()
        close(results)
    }()

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
