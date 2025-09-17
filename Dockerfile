# Multi-stage Dockerfile for Distributed File Grid
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    wget \
    curl \
    unzip \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libfmt-dev \
    libasio-dev \
    zlib1g-dev \
    libabsl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Prometheus-CPP from source
WORKDIR /tmp
RUN git clone --recursive https://github.com/jupp0r/prometheus-cpp.git && \
    cd prometheus-cpp && \
    mkdir build && cd build && \
    cmake -DBUILD_SHARED_LIBS=ON -DENABLE_PULL=ON -DENABLE_PUSH=ON .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Copy source code
WORKDIR /app
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake -DBUILD_TESTS=OFF -DWITH_REDIS=OFF -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libprotobuf32 \
    libssl3 \
    zlib1g \
    libabsl20210324 \
    redis-server \
    openjdk-11-jre-headless \
    wget \
    curl \
    netcat \
    && rm -rf /var/lib/apt/lists/*

# Install ZooKeeper
WORKDIR /opt
RUN wget https://archive.apache.org/dist/zookeeper/zookeeper-3.8.0/apache-zookeeper-3.8.0-bin.tar.gz && \
    tar -xzf apache-zookeeper-3.8.0-bin.tar.gz && \
    mv apache-zookeeper-3.8.0-bin zookeeper && \
    rm apache-zookeeper-3.8.0-bin.tar.gz

# Copy built executables
COPY --from=builder /app/build/head_server /usr/local/bin/
COPY --from=builder /app/build/cluster_server /usr/local/bin/
COPY --from=builder /app/build/health_checker /usr/local/bin/

# Copy ZooKeeper monitor (we'll build it separately)
COPY --from=builder /app/src/ZooKeeper_HealthChecker/zk_head_server_monitor.cpp /app/
RUN cd /app && g++ -std=c++20 -O2 -o /usr/local/bin/zk_head_server_monitor zk_head_server_monitor.cpp -pthread

# Copy configuration files
COPY config/ /app/config/
COPY scripts/ /app/scripts/

# Create necessary directories
RUN mkdir -p /app/logs /app/data /tmp/chunks /tmp/cluster_storage

# Copy ZooKeeper configuration
COPY docker/zoo.cfg /opt/zookeeper/conf/

# Copy startup scripts
COPY docker/start-services.sh /app/
COPY docker/healthcheck.sh /app/
RUN chmod +x /app/start-services.sh /app/healthcheck.sh

# Expose ports
EXPOSE 9669 8080 8081 8082 9000 6379 2181 9091

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD /app/healthcheck.sh

# Default command
CMD ["/app/start-services.sh"]