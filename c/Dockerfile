# --- Compilar el binario C con GMP ---
FROM debian:trixie-slim AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc libc6-dev libgmp-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY lucas_lehmer.c .

# Flags portables: el bucle caliente está dentro de libgmp, y -march=native
# no aportaba nada medible pero impedía usar la imagen en otra CPU.
RUN gcc -O3 -flto -Wall -Wextra \
	-o lucas_lehmer \
	lucas_lehmer.c \
	-lgmp -lpthread

# Comprobar el LLT antes de generar la imagen
RUN test "$(echo '[0,1,2,3,4,5,7,11,13,61,67,89,127,521,523,607]' | ./lucas_lehmer)" = \
	'[{"p":607,"is_prime":true},{"p":523,"is_prime":false},{"p":521,"is_prime":true},{"p":127,"is_prime":true},{"p":89,"is_prime":true},{"p":67,"is_prime":false},{"p":61,"is_prime":true},{"p":13,"is_prime":true},{"p":11,"is_prime":false},{"p":7,"is_prime":true},{"p":5,"is_prime":true},{"p":4,"is_prime":false},{"p":3,"is_prime":true},{"p":2,"is_prime":true},{"p":1,"is_prime":false},{"p":0,"is_prime":false}]'

# --- Imagen final: Flask + gunicorn + binario ---
FROM python:3.11-slim-trixie

RUN apt-get update && \
    apt-get install -y --no-install-recommends libgmp10 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .
COPY --from=build /src/lucas_lehmer .

EXPOSE 8000

CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--timeout", "2592000", "app:app"]
