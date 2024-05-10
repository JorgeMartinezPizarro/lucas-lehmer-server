FROM hseeberger/scala-sbt:11.0.11_1.5.5_2.13.6 AS builder

WORKDIR /app

# Copy project files
COPY . .

# Compile the Scala application (using package)
RUN sbt assembly


RUN find . -name '*.jar'

# Stage 2: Create the final Docker image
FROM adoptopenjdk/openjdk11:alpine-slim

WORKDIR /app

# Copy the JAR file from the builder stage
COPY --from=builder /app/target/scala-2.13/LLTP-assembly-0.1.0.jar LLTP-0.1.0.jar

# Expose port
EXPOSE 8080

# List all files in the current directory to find the JAR filename
RUN ls -lah

# Command to run the application
CMD ["java", "-jar", "LLTP-0.1.0.jar"]  # Adjust filename if different
