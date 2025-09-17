# Distributed File Grid - Docker Deployment Guide

## 🐳 Docker Setup Overview

This document provides comprehensive instructions for deploying and testing the Distributed File Grid system using Docker and Docker Compose.

## 🏗️ Architecture

The Docker deployment includes:

### Core Services
- **Head Server 1 & 2**: Primary and backup metadata servers with leader election
- **Cluster Servers 1-3**: Distributed chunk storage with 3x replication
- **Health Checker**: Original heartbeat-based monitoring
- **ZooKeeper Head Monitor**: New ZooKeeper-based head server monitoring with leader election

### Infrastructure Services
- **ZooKeeper**: Coordination service for leader election and service discovery
- **Redis**: Metadata storage (optional, can be disabled)
- **Prometheus**: Metrics collection and monitoring
- **Grafana**: Visualization and dashboards

### Monitoring & Testing
- **Test Client**: Container for running system tests
- **Health Checks**: Automated service health monitoring

## 🚀 Quick Start

### Prerequisites
```bash
# Install Docker and Docker Compose
sudo apt-get update
sudo apt-get install docker.io docker-compose

# Or on Arch Linux
sudo pacman -S docker docker-compose

# Start Docker service
sudo systemctl start docker
sudo systemctl enable docker

# Add user to docker group (logout/login required)
sudo usermod -aG docker $USER
```

### 1. Build and Start All Services
```bash
# Make test script executable
chmod +x docker-test.sh

# Run complete test suite (builds, starts, and tests everything)
./docker-test.sh

# Or manually start services
docker-compose up -d
```

### 2. Monitor Service Status
```bash
# Check service status
docker-compose ps

# View logs for all services
docker-compose logs -f

# View logs for specific service
docker-compose logs -f head-server-1
```

### 3. Access Web Interfaces
- **Prometheus**: http://localhost:9090
- **Grafana**: http://localhost:3000 (admin/admin)
- **Head Server 1**: http://localhost:9669
- **Head Server 2**: http://localhost:9670

## 🔧 Service Configuration

### Environment Variables

Each service can be configured via environment variables:

```yaml
# Head Server Configuration
SERVER_ID: head_server_1
SERVER_PORT: 9669
REDIS_HOST: redis
REDIS_PORT: 6379
ZK_HOSTS: zookeeper:2181
SERVER_ROLE: primary

# Cluster Server Configuration  
SERVER_ID: 1
SERVER_PORT: 8080
METRICS_PORT: 9091
HEAD_SERVER_HOST: head-server-1
HEAD_SERVER_PORT: 9669
```

### Volume Mounts
- **Logs**: `./logs:/app/logs` - Service logs
- **Data**: `./data:/app/data` - Persistent data
- **Cluster Storage**: Named volumes for chunk storage

## 🧪 Testing

### Automated Test Suite
```bash
# Run full test suite
./docker-test.sh

# Available commands
./docker-test.sh cleanup    # Clean up containers
./docker-test.sh logs       # Show all logs
./docker-test.sh status     # Show service status
./docker-test.sh stop       # Stop all services
```

### Manual Testing

#### 1. Test ZooKeeper Integration
```bash
# Connect to ZooKeeper monitor
docker-compose exec zk-head-monitor /usr/local/bin/zk_head_server_monitor --zk-hosts zookeeper:2181 -i

# Commands in interactive mode:
# register head_server_3 127.0.0.1 9671  # Register new head server
# status                                   # Show health status
# leader                                   # Show current leader
# quit                                     # Exit
```

#### 2. Test File Operations
```bash
# Access test client
docker-compose exec test-client bash

# Create test file
echo "Test data" > /app/test_data/sample.txt

# Simulate file operations (when implemented)
# curl -X POST http://head-server-1:9669/upload -F "file=@/app/test_data/sample.txt"
```

#### 3. Test Health Monitoring
```bash
# Check health endpoints
curl http://localhost:9669/health  # Head server (if implemented)
curl http://localhost:9091/metrics # Cluster server metrics

# Check service health via Docker
docker-compose exec head-server-1 /app/healthcheck.sh
```

#### 4. Test Leader Election
```bash
# Stop primary head server to trigger election
docker-compose stop head-server-1

# Check ZooKeeper monitor logs for leader election
docker-compose logs zk-head-monitor

# Restart primary server
docker-compose start head-server-1
```

## 📊 Monitoring

### Prometheus Metrics
Access Prometheus at http://localhost:9090

Key metrics to monitor:
- `heartbeat_active_connections` - Active connections per cluster server
- `heartbeat_messages_received_total` - Total messages received
- `heartbeat_processing_time_seconds` - Message processing latency
- `heartbeat_errors_total` - Error counts by type

### Grafana Dashboards
Access Grafana at http://localhost:3000 (admin/admin)

Pre-configured dashboards show:
- System resource usage (CPU, memory, disk)
- Service health status
- Network traffic and latency
- Error rates and patterns

### Log Analysis
```bash
# View aggregated logs
docker-compose logs -f

# Filter logs by service
docker-compose logs -f head-server-1 cluster-server-1

# Search for errors
docker-compose logs | grep -i error

# Monitor real-time logs
docker-compose logs -f --tail=100
```

## 🔧 Troubleshooting

### Common Issues

#### 1. Services Won't Start
```bash
# Check Docker daemon
sudo systemctl status docker

# Check available resources
docker system df
docker system prune -f  # Clean up if needed

# Check port conflicts
netstat -tulpn | grep -E ':(2181|6379|9669|8080|9090|3000)'
```

#### 2. Service Health Check Failures
```bash
# Check individual service health
docker-compose exec head-server-1 /app/healthcheck.sh

# Check service logs for errors
docker-compose logs head-server-1 | tail -50

# Restart unhealthy service
docker-compose restart head-server-1
```

#### 3. ZooKeeper Connection Issues
```bash
# Test ZooKeeper connectivity
docker-compose exec zookeeper zkCli.sh ls /

# Check ZooKeeper logs
docker-compose logs zookeeper

# Restart ZooKeeper
docker-compose restart zookeeper
```

#### 4. Redis Connection Issues
```bash
# Test Redis connectivity
docker-compose exec redis redis-cli ping

# Check Redis logs
docker-compose logs redis

# Connect to Redis CLI
docker-compose exec redis redis-cli
```

### Performance Tuning

#### 1. Resource Limits
```yaml
# Add to docker-compose.yml services
deploy:
  resources:
    limits:
      cpus: '2.0'
      memory: 4G
    reservations:
      cpus: '1.0'
      memory: 2G
```

#### 2. Volume Performance
```bash
# Use local volumes for better performance
volumes:
  cluster1_data:
    driver: local
    driver_opts:
      type: none
      o: bind
      device: /fast/storage/path
```

#### 3. Network Optimization
```yaml
# Use host networking for better performance (less secure)
network_mode: host
```

## 🔒 Security Considerations

### Production Deployment
1. **Change default passwords**: Update Grafana admin password
2. **Use secrets management**: Store sensitive data in Docker secrets
3. **Enable TLS**: Configure HTTPS for web interfaces
4. **Network isolation**: Use custom networks with firewall rules
5. **Resource limits**: Set appropriate CPU/memory limits
6. **Log rotation**: Configure log rotation to prevent disk filling

### Example Production Configuration
```yaml
# docker-compose.prod.yml
services:
  head-server-1:
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 4G
      restart_policy:
        condition: on-failure
        max_attempts: 3
    secrets:
      - redis_password
      - zk_auth
    environment:
      - REDIS_PASSWORD_FILE=/run/secrets/redis_password
```

## 📈 Scaling

### Horizontal Scaling
```bash
# Scale cluster servers
docker-compose up -d --scale cluster-server=5

# Add new head server
# Edit docker-compose.yml to add head-server-3
docker-compose up -d head-server-3
```

### Load Balancing
```yaml
# Add nginx load balancer
nginx:
  image: nginx:alpine
  ports:
    - "80:80"
  volumes:
    - ./nginx.conf:/etc/nginx/nginx.conf
  depends_on:
    - head-server-1
    - head-server-2
```

## 🎯 Next Steps

1. **Implement HTTP API**: Add REST endpoints to head servers
2. **Add authentication**: Implement user authentication and authorization
3. **Enhance monitoring**: Add custom metrics and alerts
4. **Performance testing**: Use tools like Apache Bench or JMeter
5. **Backup strategy**: Implement automated backup and recovery
6. **CI/CD pipeline**: Set up automated testing and deployment

## 📚 Additional Resources

- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [Prometheus Configuration](https://prometheus.io/docs/prometheus/latest/configuration/configuration/)
- [Grafana Dashboard Creation](https://grafana.com/docs/grafana/latest/dashboards/)
- [ZooKeeper Administration](https://zookeeper.apache.org/doc/current/zookeeperAdmin.html)

---

**Last Updated**: September 17, 2025  
**Docker Compose Version**: 3.8  
**Tested On**: Ubuntu 22.04, Arch Linux