# Distributed File Grid (Work in Progress)

## Project Summary

Built a distributed file storage system to reduce read/write latency and ensure fault-tolerant data redundancy across unreliable nodes.

- Designed a high-throughput system using asynchronous, multithreaded I/O to split files into 64 MB chunks with configurable replication.
- Implemented a head server to manage metadata and monitor node health via heartbeat signals; auto-triggered re-replication on failure.
- Enabled high availability by deploying a health-checking service on a separate cluster with standby quorum and leader election to ensure continuous operation.
- Used Protocol Buffers (Protobuf) for efficient data serialization in heartbeat signals, metadata exchange, inter-node communication, and cluster coordination.
- Achieved 2× lower latency and zero data loss during simulated multi-node failures in test environments.

---

## System Overview

Distributed File Grid is a distributed file storage service designed for high throughput, low latency, and robust fault tolerance. The system consists of two main server types:

- **Head Server:**  
  - Entry point for users to upload/download files.  
  - Stores file metadata and manages chunking (splitting files into 64 MB chunks).  
  - Monitors the health of chunk servers via heartbeat signals.  
  - Decides chunk placement based on available storage and server health.  
  - Handles client requests by reconstructing files from distributed chunks.

- **Chunk Servers:**  
  - Store actual file chunks, each chunk replicated `n` times across different servers (configurable replication factor).  
  - On every write or new file creation, verify chunk integrity using hash values.  
  - Periodically report health and resource usage (storage, CPU, bandwidth) to the head server.

- **Health-Checking Service:**  
  - Runs on a separate cluster.  
  - Monitors the head server's health using a standby quorum and leader election.  
  - If the head server is unresponsive, triggers failover checks and ensures continuous operation.

---

## Features
- Distributed, fault-tolerant file storage
- Asynchronous, multithreaded I/O for high throughput
- Files split into 64 MB chunks with configurable replication factor
- Head server for metadata management and node health monitoring
- Heartbeat protocol for health checking and auto re-replication
- Health-checking service with leader election for high availability
- Efficient inter-node communication using Protocol Buffers
- Server and rack awareness for intelligent data placement

---

## Architecture Overview

![image](https://github.com/user-attachments/assets/1497ce36-1209-423c-a113-e5aa68a40af6)

- **Head Server:** Manages metadata, coordinates chunk servers, and monitors health.
- **Chunk Servers:** Store file chunks, handle replication, and respond to health checks.
- **Health-Checking Service:** Runs on a separate cluster, uses leader election for continuous monitoring.
- **Protocol Buffers:** Used for all structured communication between nodes.

---

## How It Works

1. **File Upload:**
   - User uploads a file via the head server.
   - The head server splits the file into 64 MB chunks.
   - Each chunk is assigned a replication factor (e.g., 3) and distributed to chunk servers with the most available storage.
   - Metadata (chunk locations, hashes) is stored by the head server.

2. **File Download:**
   - User requests a file from the head server.
   - The head server retrieves all relevant chunks from chunk servers and reconstructs the file for the user.

3. **Health Monitoring:**
   - Head server sends heartbeat requests to chunk servers.
   - Chunk servers respond with health status and resource usage.
   - If a chunk server fails, the head server triggers re-replication of affected chunks to maintain redundancy.

4. **Head Server High Availability:**
   - A separate health-checking service monitors the head server.
   - If the head server fails, the service coordinates with standby servers to elect a new leader and maintain service continuity.

5. **Efficient Communication:**
   - All inter-server communication (heartbeat, metadata, chunk placement, health reports) uses Protocol Buffers (Protobuf) for efficient, versioned serialization.

---

## Workflow Example

- **Replication:**  
  If the replication factor is 3, each chunk is stored on three different chunk servers. If one server fails, the system automatically re-replicates the chunk to another healthy server.

- **Storage Awareness:**  
  The head server always places new chunks on servers with the most available storage, balancing load and maximizing efficiency.

- **Odd Number of Servers:**  
  Both head and chunk servers are deployed in odd numbers to avoid stalemate and ensure quorum-based decisions.

---

## Key Advantages

- **Low Latency:**  
  Asynchronous, multithreaded I/O and intelligent chunk placement reduce read/write times.

- **Fault Tolerance:**  
  Automatic re-replication and health monitoring ensure zero data loss during node failures.

- **Scalability:**  
  Easily add new chunk servers; the system auto-balances and synchronizes data.

- **Extensible:**  
  Modular design and Protobuf-based communication make it easy to extend and integrate.

---

## Requirements
- At least 3 machines (preferably an odd number of chunk servers to avoid stalemate)
- C++17 or later
- Python 3.8+ (for tooling and PyInstaller, if using the Python interface)
- [Protocol Buffers Compiler (protoc)](https://developers.google.com/protocol-buffers)

---

## Installation & Offline Usage

### 1. Clone the Repository
```bash
git clone https://github.com/Stability-AI/stable-fast-3d.git
cd stable-fast-3d
```

### 2. Build the Project
- Use the provided `Makefile` to build C++ components:
  ```bash
  make
  ```
- Ensure `protoc` is installed and run to generate C++ source from `.proto` files:
  ```bash
  protoc -I=protos --cpp_out=include protos/heat_beat.proto
  ```

### 3. (Optional) Build a Standalone Executable with PyInstaller
If you have a Python interface or tools:
```bash
pip install -r requirements.txt
pip install pyinstaller
pyinstaller --onefile --add-data "model_weights;model_weights" main.py
```
- The standalone executable will be in the `dist/` folder.

### 4. Run the System
- Start the head server, chunk servers, and health-checking service as described in the documentation or scripts.
- Add new nodes to the chain as needed; the system will auto-synchronize files and maintain redundancy.

---

## Usage Notes
- The system requires at least 3 chunk servers for proper operation.
- Replication factor is configurable (default: 3).
- Heartbeat protocol ensures failed nodes are detected and data is re-replicated automatically.
- All inter-node communication uses Protocol Buffers for efficiency and versioning.

---

## Troubleshooting
- **Missing Dependencies:** Ensure all system and Python dependencies are installed offline as needed.
- **Model Weights Not Found:** Place required model/data files in the correct directory or bundle them with PyInstaller.
- **Stalemate:** Use an odd number of chunk servers to avoid split-brain scenarios.

---

## Acknowledgments
- Inspired by distributed file systems such as GFS and HDFS.
- Uses [Protocol Buffers](https://developers.google.com/protocol-buffers) for serialization.

---

## License
This project is open-source and a work in progress. Contributions are welcome!
