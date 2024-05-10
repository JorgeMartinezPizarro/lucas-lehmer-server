# Use the official Golang image
FROM golang:latest

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

