#!/bin/bash

# Distributed File Grid - Service Shutdown Script
# This script stops all components of the distributed file storage system

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIDS_DIR="$PROJECT_ROOT/pids"

echo -e "${BLUE}=== Distributed File Grid Shutdown ===${NC}"
echo ""

# Function to stop a service
stop_service() {
    local service_name="$1"
    local pid_file="$PIDS_DIR/${service_name}.pid"
    
    if [ ! -f "$pid_file" ]; then
        echo -e "${YELLOW}$service_name: No PID file found${NC}"
        return 0
    fi
    
    local pid=$(cat "$pid_file")
    
    if ! kill -0 "$pid" 2>/dev/null; then
        echo -e "${YELLOW}$service_name: Process not running (PID: $pid)${NC}"
        rm -f "$pid_file"
        return 0
    fi
    
    echo -e "${YELLOW}Stopping $service_name (PID: $pid)...${NC}"
    
    # Try graceful shutdown first
    kill -TERM "$pid" 2>/dev/null || true
    
    # Wait up to 10 seconds for graceful shutdown
    for i in {1..10}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            echo -e "${GREEN}$service_name stopped gracefully${NC}"
            rm -f "$pid_file"
            return 0
        fi
        sleep 1
    done
    
    # Force kill if still running
    echo -e "${YELLOW}Force killing $service_name...${NC}"
    kill -KILL "$pid" 2>/dev/null || true
    sleep 1
    
    if ! kill -0 "$pid" 2>/dev/null; then
        echo -e "${GREEN}$service_name stopped (forced)${NC}"
        rm -f "$pid_file"
        return 0
    else
        echo -e "${RED}Failed to stop $service_name${NC}"
        return 1
    fi
}

# Stop services in reverse order
echo "Stopping cluster servers..."
stop_service "cluster_server_3"
stop_service "cluster_server_2"
stop_service "cluster_server_1"
echo ""

echo "Stopping head server..."
stop_service "head_server"
echo ""

echo "Stopping health checker..."
stop_service "health_checker"
echo ""

echo "Stopping Redis..."
stop_service "redis"
echo ""

# Clean up any remaining processes
echo -e "${YELLOW}Cleaning up any remaining processes...${NC}"
pkill -f "head_server" 2>/dev/null || true
pkill -f "cluster_server" 2>/dev/null || true
pkill -f "health_checker" 2>/dev/null || true

echo -e "${GREEN}=== All services stopped ===${NC}"
echo ""
echo "To start services again, run: ./scripts/start_services.sh"
