# Use the official Rust image as a base with the latest stable version
FROM rust:latest as builder

# Set the working directory inside the container
WORKDIR /usr/src/llt_server

# Copy the Cargo.toml and Cargo.lock files
COPY Cargo.toml Cargo.lock ./

# Create a new empty Rust project to cache dependencies
RUN mkdir src
RUN echo "fn main() {}" > src/main.rs

# Build dependencies
RUN cargo build --release
RUN rm -f target/release/deps/llt_server*

# Copy the actual source code
COPY ./src ./src

# Build the actual project
RUN cargo build --release

# Use a more recent base image for the final stage
FROM debian:bookworm-slim

# Install necessary dependencies (if any)
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*

# Copy the build artifact from the builder stage
COPY --from=builder /usr/src/llt_server/target/release/llt_server /usr/local/bin/llt_server

# Set the command to run the server
CMD ["llt_server"]

# Expose the port the server runs on
EXPOSE 3030


