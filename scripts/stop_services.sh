#!/bin/bash

# Distributed File Grid - Service Stop Script

echo "Stopping Distributed File Grid Services..."
echo "========================================"

# Stop Redis server
echo "Stopping Redis server..."
redis-cli shutdown || echo "Redis was not running or already stopped"

# Stop all cluster servers
echo "Stopping cluster servers..."
pkill -f "cluster_server" || echo "No cluster servers were running"

# Stop health checker
echo "Stopping health checker..."
pkill -f "health_checker" || echo "Health checker was not running"

# Stop head server
echo "Stopping head server..."
pkill -f "head_server" || echo "Head server was not running"

# Wait a moment for processes to terminate
sleep 2

# Force kill any remaining processes if needed
echo "Cleaning up any remaining processes..."
pkill -9 -f "cluster_server" 2>/dev/null || true
pkill -9 -f "health_checker" 2>/dev/null || true
pkill -9 -f "head_server" 2>/dev/null || true

echo ""
echo "All services stopped successfully!"
echo "================================="
echo "All distributed file grid services have been terminated."
echo "You can start them again using './scripts/start_services.sh'" 