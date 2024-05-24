# Use an official GCC image as the base image
FROM gcc:latest

# Install dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libgmp-dev \
    libboost-all-dev \
    cmake \
    git \
    libasio-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install Crow
RUN git clone https://github.com/CrowCpp/Crow.git /Crow && \
    mkdir /Crow/build && \
    cd /Crow/build && \
    cmake .. && \
    make && \
    make install && \
    rm -rf /Crow

# Set the working directory in the container
WORKDIR /usr/src/app

# Copy the current directory contents into the container at /usr/src/app
COPY . .

# Run CMake to configure the project
RUN cmake .

# Build the project
RUN make

# Make port 8080 available to the world outside this container
EXPOSE 8080

# Run the main executable
CMD ["./main"]

