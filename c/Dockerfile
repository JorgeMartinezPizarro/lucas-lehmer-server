# --- Compilar el binario C con GMP y FFTW ---
FROM debian:trixie-slim AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc libc6-dev libgmp-dev libfftw3-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY *.c *.h check.sh ./

# Flags portables: el trabajo pesado está en libgmp y FFTW, que eligen el
# código para la CPU en tiempo de ejecución; -march=native no aportaba nada
# medible pero impedía usar la imagen en otra CPU.
RUN gcc -O3 -flto -fopenmp -Wall -Wextra \
	-o lucas_lehmer \
	lucas_lehmer.c llt.c llt_mpn.c llt_fft.c fourstep.c \
	-lfftw3 -lgmp -lpthread -lm

# Comprobar el LLT antes de generar la imagen: FFT y GMP coinciden hasta
# p = 3000 y el binario encuentra los exponentes conocidos hasta 44497
RUN sh check.sh 3000 ./lucas_lehmer

# Herramienta de desarrollo, no va en la imagen final (make estimate / make scale)
RUN gcc -O3 -flto -fopenmp -Wall -Wextra \
	-o test_llt \
	test_llt.c llt.c llt_mpn.c llt_fft.c fourstep.c \
	-lfftw3 -lgmp -lpthread -lm

# --- Imagen final: Flask + gunicorn + binario ---
FROM python:3.11-slim-trixie

RUN apt-get update && \
    apt-get install -y --no-install-recommends libgmp10 libfftw3-double3 libgomp1 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .
COPY --from=build /src/lucas_lehmer .

EXPOSE 8000

CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--timeout", "2592000", "app:app"]
