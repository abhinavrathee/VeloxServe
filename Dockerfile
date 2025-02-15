# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Production stage
FROM ubuntu:22.04

# Install runtime dependencies (only what's strictly needed)
RUN apt-get update && apt-get install -y \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/veloxserve

# Copy compiled binary from builder
COPY --from=builder /app/build/veloxserve /usr/local/bin/veloxserve

# Copy configurations and static assets
COPY veloxserve.conf .
COPY www/ ./www/
COPY logs/ ./logs/

# Ensure logs directory exists with correct permissions
RUN mkdir -p logs && chmod 777 logs

# Expose default port
EXPOSE 8080

# Run the server
CMD ["veloxserve", "veloxserve.conf"]
