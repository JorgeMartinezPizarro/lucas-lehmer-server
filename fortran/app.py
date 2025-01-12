from flask import Flask, request, jsonify
import subprocess

app = Flask(__name__)

@app.route('/sieve', methods=['GET'])
def sieve():
    try:
        # Obtener el límite superior del parámetro de la solicitud
        n = request.args.get('n', type=int)

        if not n or n <= 0:
            return jsonify({"error": "Invalid input. Provide a positive integer for 'n'."}), 400

        # Ejecutar el programa Fortran con el argumento n
        result = subprocess.run(
            ['./sieve', str(n)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        if result.returncode != 0:
            return jsonify({"error": result.stderr.strip()}), 500

        # Parsear la salida en una lista de números primos
        primes = [int(x) for x in result.stdout.split()]

        return jsonify({"primes": primes})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
