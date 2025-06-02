FROM python:3.11-slim

# Instalar dependencias del sistema
RUN apt-get update && apt-get install -y gcc && rm -rf /var/lib/apt/lists/*

# Crear directorio de trabajo
WORKDIR /app

# Copiar código
COPY app.py .
COPY lucas_lehmer.c .

# Compilar el código C
RUN gcc -O2 -o lucas_lehmer lucas_lehmer.c -lpthread

# Instalar dependencias de Python
RUN pip install flask gunicorn

# Exponer puerto
EXPOSE 8000

# Ejecutar el servidor
CMD ["gunicorn", "--bind", "0.0.0.0:8000", "app:app"]
