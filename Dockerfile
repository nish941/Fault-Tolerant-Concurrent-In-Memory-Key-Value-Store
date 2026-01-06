FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    git \
    libasio-dev \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /kv-store

# Copy source code
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binaries from builder
COPY --from=builder /kv-store/build/kv_server .
COPY --from=builder /kv-store/build/kv_client .

# Create data directory for WAL files
RUN mkdir /data

# Expose port
EXPOSE 6379

# Run server
CMD ["./kv_server"]
