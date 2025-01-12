FROM alpine:latest

# Instalar dependencias necesarias para Fortran y Python
RUN apk add --no-cache gfortran build-base python3 py3-pip

WORKDIR /app

# Copiar el archivo fuente de Fortran y compilarlo
COPY sieve.f90 .
RUN gfortran -O3 -fopenmp -march=native -o sieve sieve.f90

# Copiar la aplicación Flask
COPY app.py .

# Instalar Gunicorn
RUN pip install --break-system-packages flask gunicorn

EXPOSE 5000

CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "app:app"]
