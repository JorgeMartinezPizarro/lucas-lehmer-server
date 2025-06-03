from flask import Flask, request, jsonify
import subprocess
import json

app = Flask(__name__)

@app.route("/lucas-lehmer", methods=["POST"])
def lucas_lehmer():
    data = request.get_json()
    
    if not data or "numbers" not in data or not isinstance(data["numbers"], list):
        return jsonify({"error": "Missing or invalid 'numbers' list"}), 400

    try:
        # Ejecutar el binario C pasándole la lista de números como entrada
        result = subprocess.run(
            ["/app/lucas_lehmer"],
            input=json.dumps(data["numbers"]),
            text=True,
            capture_output=True,
            timeout=60 * 60 * 24 * 7  # hasta 7 días
        )
    except subprocess.TimeoutExpired:
        return jsonify({"error": "C process timeout"}), 504
    except Exception as e:
        return jsonify({"error": "Failed to execute C code", "details": str(e)}), 500

    # Buscar la última línea que parezca JSON
    stdout_lines = result.stdout.strip().splitlines()
    json_candidate = stdout_lines[-1] if stdout_lines else ""

    try:
        parsed = json.loads(json_candidate)
        return jsonify({"results": parsed})
    except json.JSONDecodeError:
        return jsonify({
            "error": "C output is not valid JSON",
            "raw_stdout": result.stdout,
            "raw_stderr": result.stderr
        }), 500
