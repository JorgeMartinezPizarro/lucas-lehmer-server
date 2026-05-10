FROM python:3.12-slim

RUN apt-get update && \
    apt-get install -y gfortran && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

RUN gfortran -O2 prime.f90 -o prime

EXPOSE 5000

CMD ["python", "app.py"]