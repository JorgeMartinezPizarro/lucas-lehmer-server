#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <gmp.h>
#include <crow.h>

std::mutex primes_mutex;
std::vector<unsigned long> mersenne_primes;

bool lucas_lehmer_test(unsigned long p) {
    if (p == 2) return true;

    mpz_t s, M;
    mpz_init_set_ui(s, 4);
    mpz_init(M);
    mpz_ui_pow_ui(M, 2, p);
    mpz_sub_ui(M, M, 1);

    for (unsigned long i = 3; i <= p; ++i) {
        mpz_pow_ui(s, s, 2);
        mpz_sub_ui(s, s, 2);
        mpz_mod(s, s, M);
    }

    bool is_prime = (mpz_cmp_ui(s, 0) == 0);
    mpz_clear(s);
    mpz_clear(M);
    return is_prime;
}

void find_mersenne_primes(const std::vector<unsigned long>& numbers, unsigned int numberOfThreads) {
    std::vector<std::thread> threads;
    unsigned long range = numbers.size() / numberOfThreads;
    
    for (unsigned int i = 0; i < numberOfThreads; ++i) {
        threads.emplace_back([&, i] {
            unsigned long start = i * range;
            unsigned long end = (i == numberOfThreads - 1) ? numbers.size() : start + range;
            for (unsigned long j = start; j < end; ++j) {
                if (lucas_lehmer_test(numbers[j])) {
                    std::lock_guard<std::mutex> lock(primes_mutex);
                    mersenne_primes.push_back(numbers[j]);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

int main(int argc, char* argv[]) {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/lltp").methods("POST"_method)
    ([](const crow::request& req, crow::response& res){
        mersenne_primes.clear();
        auto json_data = crow::json::load(req.body);
        if (!json_data || !json_data.has("numbers") || !json_data.has("threads")) {
            res.code = 400;
            res.end("Invalid request");
            return;
        }

        std::vector<unsigned long> numbers;
        for (const auto& num : json_data["numbers"]) {
            numbers.push_back(num.i());
        }
        unsigned int numberOfThreads = json_data["threads"].i();

        find_mersenne_primes(numbers, numberOfThreads);

        crow::json::wvalue result;
        result["mersenne_primes"] = mersenne_primes;
        res.set_header("Content-Type", "application/json");
        res.write(result.dump());
        res.end();
    });

    app.port(8080).multithreaded().run();
}
