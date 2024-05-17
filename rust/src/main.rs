use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};
use tokio::task;
use warp::Filter;
use num_bigint::BigUint;
use num_traits::{Zero, One};

#[derive(Deserialize)]
struct LLTRequest {
    numbers: Vec<String>, // Array of strings
    number_of_threads: usize,
}

#[derive(Serialize, Clone)]
struct LLTResult {
    p: String,
    is_prime: bool,
}

#[tokio::main]
async fn main() {
    let llt = warp::path("llt")
        .and(warp::post())
        .and(warp::body::json())
        .and_then(handle_llt);

    warp::serve(llt).run(([0, 0, 0, 0], 3030)).await;
}

async fn handle_llt(req: LLTRequest) -> Result<impl warp::Reply, warp::Rejection> {
    let results = Arc::new(Mutex::new(Vec::new()));
    let threads = req.number_of_threads;
    let numbers = req.numbers;

    let mut handles = vec![];

    for chunk in numbers.chunks((numbers.len() + threads - 1) / threads) {
        let chunk = chunk.to_vec();
        let results = Arc::clone(&results);

        let handle = task::spawn_blocking(move || {
            let mut chunk_results = vec![];

            for number_str in chunk {
                let number = BigUint::parse_bytes(number_str.as_bytes(), 10).unwrap();
                let p = number.to_u64_digits()[0];
                let modulus = (BigUint::one() << p) - BigUint::from(1u32); // 2^number - 1
                let mut x = BigUint::from(4u32);

                for _ in 0..(p - 2) {
                    x = (&x * &x - 2u32) % &modulus;
                }

                let is_prime = x.is_zero();
                chunk_results.push(LLTResult {
                    p: number_str,
                    is_prime,
                });
            }

            let mut res = results.lock().unwrap();
            res.extend(chunk_results);
        });

        handles.push(handle);
    }

    for handle in handles {
        handle.await.unwrap();
    }

    let results = results.lock().unwrap().clone();
    Ok(warp::reply::json(&results))
}


