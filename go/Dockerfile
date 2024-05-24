# Use the official Golang image
FROM golang:latest

# Install GMP library
RUN apt-get update && apt-get install -y libgmp-dev

# Set the Current Working Directory inside the container
WORKDIR /app

# Copy the source code into the container
COPY main.go .

# Initialize Go modules and download dependencies
RUN go mod init myapp \
    && go mod tidy

# Build the Go app
RUN go build -o app .

# Expose port 8080 to the outside world
EXPOSE 8080

# Command to run the executable
CMD ["./app"]
