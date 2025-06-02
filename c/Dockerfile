FROM python:3.11-slim

# Instalar dependencias del sistema
RUN apt-get update && \
    apt-get install -y gcc libgmp-dev && \
    rm -rf /var/lib/apt/lists/*

# Crear directorio de trabajo
WORKDIR /app

# Copiar archivos
COPY app.py .
COPY lucas_lehmer.c .

# Compilar el código C con GMP
RUN gcc -O2 -o lucas_lehmer lucas_lehmer.c -lgmp -lpthread

# Instalar Flask y Gunicorn
RUN pip install flask gunicorn

EXPOSE 8000

CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--timeout", "2592000", "app:app"]
