# --- Build and test the Go server (cgo + GMP) ---
FROM golang:1.27-trixie AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends libgmp-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY go.mod go.sum ./
RUN go mod download

COPY *.go ./
RUN go test ./... && go build -o /out/server .

# --- Final image: just the binary and libgmp ---
FROM debian:trixie-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends libgmp10 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /out/server /usr/local/bin/server

EXPOSE 8080

CMD ["server"]
