from flask import Flask, request, jsonify
from multiprocessing import Pool
import math

app = Flask(__name__)

def computeLLT(p):
    print("Start Lucas Lehmer Test for p =", p)

    if p == 2:
        return {'p': 2, 'isPrime': True}  # 2^2 - 1 = 3 is prime

    if p <= 1 or p % 2 == 0:
        return {'p': p, 'isPrime': False}  # Mersenne number for p <= 1 or even p is not prime

    mersenne_number = (1 << p) - 1

    s = 4

    for i in range(3, p + 1):
        s = (s * s - 2) % mersenne_number

    isPrime = (s == 0)

    # Simulate some processing time

    return {'p': p, 'isPrime': isPrime}

def computeLLTP(numbers, num_processes):
    with Pool(num_processes) as pool:
        results = pool.map(computeLLT, numbers)
    return results

@app.route('/lltp', methods=['POST'])
def calculate():
    data = request.get_json()
    num_processes = data.get('num_processes', 1)
    numbers = data.get('numbers', [])

    if not isinstance(num_processes, int) or num_processes < 1:
        return jsonify({'error': 'Invalid num_processes value. It must be a positive integer.'}), 400

    if not isinstance(numbers, list):
        return jsonify({'error': 'Invalid numbers format. It must be a list of integers.'}), 400

    results = computeLLTP(numbers, num_processes)

    return jsonify({'results': results})

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)

