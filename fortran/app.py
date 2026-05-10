from flask import Flask, request, jsonify
import subprocess

app = Flask(__name__)


@app.route("/isprime", methods=["POST"])
def is_prime():
    data = request.get_json()

    if not data or "p" not in data:
        return jsonify({"error": "Missing 'p'"}), 400

    p = data["p"]

    try:
        result = subprocess.run(
            ["./prime", str(p)],
            capture_output=True,
            text=True,
            timeout=5
        )

        output = result.stdout.strip().lower()

        return jsonify({
            "p": p,
            "is_prime": output == "true"
        })

    except subprocess.TimeoutExpired:
        return jsonify({"error": "Fortran timeout"}), 500

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)