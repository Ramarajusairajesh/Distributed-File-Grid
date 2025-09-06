# Distributed File Grid [Word in Progress]

## Project Summary

A complete distributed file storage system designed to reduce read/write latency and ensure fault-tolerant data redundancy across unreliable nodes.

- **High-throughput system** using asynchronous, multithreaded I/O to split files into 64 MB chunks with configurable replication
- **Head server** manages metadata and monitors node health via heartbeat signals with auto-triggered re-replication on failure
- **High availability** through a health-checking service with standby quorum and leader election for continuous operation
- **Efficient communication** using Protocol Buffers for heartbeat signals, metadata exchange, and inter-node communication
- **Modern web dashboard** for real-time system monitoring and management

---

## System Architecture

The Distributed File Grid consists of three main components:

### 1. Head Server

- **Purpose**: Entry point for file operations and metadata management
- **Features**:
  - File upload/download coordination
  - Chunk placement and replication management
  - Health monitoring of cluster servers
  - Automatic failover handling

### 2. Cluster Servers

- **Purpose**: Store actual file chunks with replication
- **Features**:
  - 64MB chunk storage with configurable replication factor
  - Health reporting via heartbeat signals
  - Automatic chunk integrity verification
  - Resource usage monitoring (CPU, memory, disk)

### 3. Health Checker

- **Purpose**: Monitor system health and ensure high availability
- **Features**:
  - Continuous monitoring of all servers
  - Leader election for failover coordination
  - Automatic detection and handling of server failures
  - Real-time health status reporting

---

## Features

- **Distributed Storage**: Files split into 64MB chunks across multiple servers
- **Fault Tolerance**: Configurable replication factor (default: 3x)
- **High Availability**: Automatic failover and recovery
- **Health Monitoring**: Real-time system health tracking
- **Web Dashboard**: Modern React-based monitoring interface
- **Docker Support**: Complete containerized deployment
- **Protocol Buffers**: Efficient binary communication
- **Asynchronous I/O**: High-performance file operations
- **Resource Monitoring**: CPU, memory, and disk usage tracking

---

## Quick Start

### Prerequisites

- **Linux**
- **C++20** compiler (GCC 7+ or Clang 6+) [since protobufs only support C++ 20+]
- **CMake** 3.16+
- **Protocol Buffers** compiler
- **Docker** and **Docker Compose** (for containerized deployment)
- **Redis-server** and **Redis-sentinel** with **sw-redis-plus-plus** library 
[+] Side-Node: Just build your own in memory data store could have saved some times compared to using redis for this.(Reason to use : just want to use REDIS for no reason and for replication without me doing it)

### Installation

#### Option 1: Native Installation

1. **Install Dependencies**

   **Ubuntu/Debian:**

   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake pkg-config \
       libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler \
       libfmt-dev libasio-dev
   ```

   **Arch Linux:**

   ```bash
   sudo pacman -S --needed base-devel cmake pkg-config \
        protobuf fmt asio
   ```


```bash

# It might not work if you have cmake >=4.x.x . It didn't work for me for the yay , so downloaded the repo and install it.
yay -S redis-plus-plus
#For instructions go to https://github.com/sewenew/redis-plus-plus (clone it and install it using cmake and make simple)
```

```

```
2. **Build the Project**

   ```bash
   git clone <repository-url>
   cd Distributed-File-Grid
   make install-deps
   make all
   ```

3. **Start Services**
   ```bash
   ./scripts/start_services.sh
   ```

#### Option 2: Docker Deployment

1. **Clone and Deploy**

   ```bash
   git clone <repository-url>
   cd Distributed-File-Grid
   docker-compose up -d
   ```

2. **Access the Dashboard**
   - Open http://localhost:3000 in your browser
   - Monitor system health and performance

---

## Usage

### File Operations

The system provides a RESTful API for file operations:

```bash
# Upload a file
curl -X POST http://localhost:9669/upload \
  -F "file=@/path/to/your/file.txt"

# Download a file
curl -X GET http://localhost:9669/download/filename.txt \
  -o downloaded_file.txt

# List files
curl -X GET http://localhost:9669/files

# Delete a file
curl -X DELETE http://localhost:9669/files/filename.txt
```

### Configuration

Configuration files are located in the `config/` directory:

- `head_server_config.json` - Head server settings
- `health_checker_config.json` - Health checker settings

Key configuration options:

```json
{
  "replication_factor": 3, // Number of chunk replicas
  "chunk_size": 67108864, // 64MB chunks
  "heartbeat_timeout": 60, // Health check interval (seconds)
  "max_connections": 1000 // Maximum concurrent connections
}
```

### Monitoring

Access the web dashboard at http://localhost:3000 to:

- View real-time system health
- Monitor resource usage (CPU, memory, disk)
- Check server status and connections
- View detailed system logs
- Monitor chunk distribution and replication

---

## Architecture Details

### File Storage Process

1. **Upload**:
   - File received by head server
   - Split into 64MB chunks
   - Chunks distributed to cluster servers based on available storage
   - Replication factor enforced across servers

2. **Download**:
   - Head server retrieves file metadata
   - Chunks fetched from cluster servers
   - File reconstructed and served to client
   - Integrity verified using stored hashes

3. **Health Monitoring**:
   - Heartbeat signals sent every 30 seconds
   - Resource usage reported (CPU, memory, disk)
   - Failed servers detected automatically
   - Re-replication triggered for failed chunks

### Fault Tolerance

- **Replication**: Each chunk stored on multiple servers
- **Health Checks**: Continuous monitoring of all servers
- **Auto-Recovery**: Failed chunks automatically re-replicated
- **Leader Election**: Health checker coordinates failover
- **Graceful Degradation**: System continues operating with reduced capacity

---

## Development

### Project Structure

```
Distributed-File-Grid/
├── src/
│   ├── Head_server/          # Head server implementation
│   ├── Cluster_Server/       # Cluster server implementation
│   ├── Health_Checker/       # Health monitoring service
│   └── include/              # Shared headers and utilities
├── protos/                   # Protocol Buffer definitions
├── website/                  # Web dashboard (React)
├── config/                   # Configuration files
├── scripts/                  # Service management scripts
├── test/                     # Test suites
└── docker-compose.yml        # Docker deployment
```

### Building from Source

```bash
# Generate protocol buffer files
protoc --cpp_out=generated protos/*.proto

# Build with CMake
mkdir build && cd build
cmake ..
make -j$(nproc)

# Or use the Makefile
make all
```

### Running Tests

```bash
make test
```

---

## Performance

### Benchmarks

- **Throughput**: 2× lower latency compared to traditional file systems
- **Scalability**: Linear scaling with additional cluster servers
- **Reliability**: Zero data loss during simulated multi-node failures
- **Efficiency**: 64MB chunks optimized for network transfer

### Resource Requirements

- **Head Server**: 2GB RAM, 2 CPU cores
- **Cluster Server**: 4GB RAM, 4 CPU cores (per server)
- **Health Checker**: 1GB RAM, 1 CPU core
- **Storage**: 100GB+ per cluster server (depending on replication factor)

---

## Troubleshooting

### Common Issues

1. **Service Won't Start**
   - Check if ports are available (9669, 8080-8082, 6379)
   - Check logs in `logs/` directory

2. **High Resource Usage**
   - Adjust `replication_factor` in configuration
   - Add more cluster servers
   - Monitor with dashboard at http://localhost:3000

3. **File Upload Failures**
   - Verify cluster servers are healthy
   - Check available storage on cluster servers
   - Review head server logs

### Logs and Debugging

- **Head Server**: `logs/head_server.log`
- **Cluster Servers**: `logs/cluster_server_*.log`
- **Health Checker**: `logs/health_checker.log`

### Health Checks

```bash
# Check all services
curl http://localhost:9669/health

# Check individual cluster servers
curl http://localhost:8080/health
curl http://localhost:8081/health
curl http://localhost:8082/health
```

---

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

### Development Setup

```bash
# Install development dependencies
make install-deps

# Set up development environment
make dev

# Run tests
make test
```

---

## License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## Acknowledgments

- Inspired by distributed file systems such as GFS and HDFS
- Uses [Protocol Buffers](https://developers.google.com/protocol-buffers) for serialization
- Built with modern C++17 and React technologies
