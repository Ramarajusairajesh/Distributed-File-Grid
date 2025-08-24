#!/bin/bash

# Distributed File Grid - Service Startup Script

set -e

echo "Starting Distributed File Grid Services..."
echo "========================================"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the project root directory"
    exit 1
fi

# Create necessary directories
echo "Creating necessary directories..."
mkdir -p logs
mkdir -p storage
mkdir -p metadata
mkdir -p temp

# Build the project if not already built
if [ ! -f "bin/head_server" ] || [ ! -f "bin/cluster_server" ] || [ ! -f "bin/health_checker" ]; then
    echo "Building project..."
    make clean
    make all
fi

# Start Redis server
echo "Starting Redis server..."
redis-server --daemonize yes --port 6379 --logfile logs/redis.log
echo "Redis started on port 6379"

# Start cluster servers
echo "Starting cluster servers..."
./bin/cluster_server cluster_server_1 ./storage/cluster1 8080 &
echo "Cluster server 1 started on port 8080"

./bin/cluster_server cluster_server_2 ./storage/cluster2 8081 &
echo "Cluster server 2 started on port 8081"

./bin/cluster_server cluster_server_3 ./storage/cluster3 8082 &
echo "Cluster server 3 started on port 8082"

# Wait a moment for cluster servers to start
sleep 2

# Start health checker
echo "Starting health checker..."
./bin/health_checker health_checker_config.json &
echo "Health checker started"

# Wait a moment for health checker to start
sleep 2

# Start head server
echo "Starting head server..."
./bin/head_server &
echo "Head server started on port 9669"

echo ""
echo "All services started successfully!"
echo "================================="
echo "Head Server: http://localhost:9669"
echo "Cluster Server 1: localhost:8080"
echo "Cluster Server 2: localhost:8081"
echo "Cluster Server 3: localhost:8082"
echo "Redis: localhost:6379"
echo "Dashboard: http://localhost:3000"
echo ""
echo "Logs are available in the 'logs' directory"
echo "Use './scripts/stop_services.sh' to stop all services"

# Keep the script running to maintain the background processes
wait 