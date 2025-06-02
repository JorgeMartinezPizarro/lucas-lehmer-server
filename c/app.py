from flask import Flask, request, jsonify
import subprocess
import json

app = Flask(__name__)

@app.route('/lucas-lehmer', methods=['POST'])
def lucas_lehmer():
    data = request.get_json()
    numbers = data.get("numbers", [])
    if not all(isinstance(n, int) and n > 0 for n in numbers):
        return jsonify({"error": "Input must be a list of positive integers"}), 400

    try:
        result = subprocess.run(
            ["./lucas_lehmer"],  # ejecutable compilado de C
            input=json.dumps(numbers),
            text=True,
            capture_output=True,
            check=True
        )
        return jsonify({"results": json.loads(result.stdout)})
    except subprocess.CalledProcessError as e:
        return jsonify({"error": e.stderr}), 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
