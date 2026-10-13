from flask import Flask, request, jsonify
import subprocess
import json

app = Flask(__name__)

# El binario C guarda p en un int
MAX_EXPONENT = 2**31 - 1


def parse_exponent(value):
    """Acepta un entero JSON o un string de dígitos decimales. Devuelve None si no es válido."""
    if isinstance(value, str):
        value = value.strip()
        if not (value.isascii() and value.isdigit()):
            return None
        value = int(value)
    if isinstance(value, bool) or not isinstance(value, int):
        return None
    if not 0 <= value <= MAX_EXPONENT:
        return None
    return value


@app.route("/lucas-lehmer-test", methods=["POST"])
def lucas_lehmer():
    data = request.get_json(silent=True)

    if not isinstance(data, dict) or not isinstance(data.get("numbers"), list):
        return jsonify({"error": "Missing or invalid 'numbers' list"}), 400

    numbers = []
    for value in data["numbers"]:
        p = parse_exponent(value)
        if p is None:
            return jsonify({
                "error": f"invalid exponent {json.dumps(value)}, expected an integer between 0 and {MAX_EXPONENT}"
            }), 400
        numbers.append(p)

    num_threads = data.get("num_threads")
    if num_threads is not None and (isinstance(num_threads, bool) or not isinstance(num_threads, int) or num_threads < 1):
        return jsonify({"error": "'num_threads' must be a positive integer"}), 400

    if not numbers:
        return jsonify({"results": []})

    command = ["/app/lucas_lehmer"]
    if num_threads is not None:
        command.append(str(num_threads))

    try:
        # Ejecutar el binario C pasándole la lista de números como entrada
        result = subprocess.run(
            command,
            input=json.dumps(numbers),
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
