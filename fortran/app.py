from flask import Flask, request, jsonify
from concurrent.futures import ThreadPoolExecutor
import subprocess

app = Flask(__name__)


def check_prime(p):
    try:
        result = subprocess.run(
            ["./prime", str(p)],
            capture_output=True,
            text=True,
            timeout=5
        )

        output = result.stdout.strip().lower()

        return {
            "p": p,
            "is_prime": output == "true"
        }

    except subprocess.TimeoutExpired:
        return {
            "p": p,
            "error": "Fortran timeout"
        }

    except Exception as e:
        return {
            "p": p,
            "error": str(e)
        }


@app.route("/lltp", methods=["POST"])
def lltp():
    data = request.get_json()

    if not data or "numbers" not in data or not isinstance(data["numbers"], list):
        return jsonify({"error": "Missing or invalid 'numbers' list"}), 400

    numbers = data["numbers"]

    try:
        with ThreadPoolExecutor() as executor:
            results = list(executor.map(check_prime, numbers))

        return jsonify({
            "results": results
        })

    except Exception as e:
        return jsonify({
            "error": "Failed to execute Fortran code",
            "details": str(e)
        }), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)