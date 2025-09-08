#include "../include/heart_beat_signal.hpp"
#include "../include/system_info.hpp"
#include "../include/version.h"
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

struct ServerHealth {
    int server_id;
    std::string ip;
    std::chrono::steady_clock::time_point last_heartbeat;
    float cpu_usage;
    float total_storage_used;
    bool is_healthy;
    int missed_heartbeats;
};

class HealthChecker {
private:
    std::unordered_map<int, ServerHealth> servers;
    std::mutex servers_mutex;
    bool running = false;
    const int MAX_MISSED_HEARTBEATS = 3;
    const std::chrono::seconds HEARTBEAT_TIMEOUT{60};
    
    async_hb::task heartbeat_receiver(async_hb::Reactor& reactor) {
        std::cout << "Starting heartbeat receiver on port 9000" << std::endl;
        
        // Start heartbeat receiver
        co_await async_hb::recieve_signal(9000);
    }
    
    async_hb::task health_monitor(async_hb::Reactor& reactor) {
        while (running) {
            check_server_health();
            co_await reactor.sleep_for(std::chrono::seconds(30));
        }
    }
    
    void check_server_health() {
        std::lock_guard<std::mutex> lock(servers_mutex);
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [server_id, health] : servers) {
            auto time_since_last = now - health.last_heartbeat;
            
            if (time_since_last > HEARTBEAT_TIMEOUT) {
                health.missed_heartbeats++;
                
                if (health.missed_heartbeats >= MAX_MISSED_HEARTBEATS && health.is_healthy) {
                    health.is_healthy = false;
                    std::cout << "Server " << server_id << " marked as unhealthy (missed " 
                             << health.missed_heartbeats << " heartbeats)" << std::endl;
                    
                    // Trigger re-replication for failed server
                    trigger_replication(server_id);
                }
            } else {
                if (!health.is_healthy && health.missed_heartbeats > 0) {
                    health.is_healthy = true;
                    health.missed_heartbeats = 0;
                    std::cout << "Server " << server_id << " recovered and marked as healthy" << std::endl;
                }
            }
        }
    }
    
    void trigger_replication(int failed_server_id) {
        std::cout << "Triggering re-replication for failed server " << failed_server_id << std::endl;
        // In a real implementation, this would:
        // 1. Query Redis for chunks stored on the failed server
        // 2. Find healthy servers to store new replicas
        // 3. Coordinate chunk copying between servers
        // 4. Update metadata in Redis
    }
    
    void process_heartbeat(const heart_beat::v1::HeartBeat& hb) {
        std::lock_guard<std::mutex> lock(servers_mutex);
        
        int server_id = hb.server_id();
        auto now = std::chrono::steady_clock::now();
        
        auto& health = servers[server_id];
        health.server_id = server_id;
        health.ip = hb.ip();
        health.last_heartbeat = now;
        health.cpu_usage = hb.cpu_usage();
        health.total_storage_used = hb.total_storage_used();
        health.missed_heartbeats = 0;
        
        if (!health.is_healthy) {
            health.is_healthy = true;
            std::cout << "Server " << server_id << " is back online" << std::endl;
        }
        
        std::cout << "Heartbeat from server " << server_id << " (" << health.ip 
                 << ") - CPU: " << health.cpu_usage << "%, Storage: " 
                 << health.total_storage_used << "%" << std::endl;
    }

public:
    void start() {
        running = true;
        std::cout << "Starting Health Checker service..." << std::endl;
        
        async_hb::Reactor reactor;
        
        // Start heartbeat receiver
        reactor.spawn(heartbeat_receiver(reactor));
        
        // Start health monitor
        reactor.spawn(health_monitor(reactor));
        
        // Run the reactor
        reactor.run();
    }
    
    void stop() {
        running = false;
        std::cout << "Stopping Health Checker service" << std::endl;
    }
    
    std::vector<ServerHealth> get_server_status() {
        std::lock_guard<std::mutex> lock(servers_mutex);
        std::vector<ServerHealth> status;
        for (const auto& [id, health] : servers) {
            status.push_back(health);
        }
        return status;
    }
    
    std::vector<int> get_healthy_servers() {
        std::lock_guard<std::mutex> lock(servers_mutex);
        std::vector<int> healthy;
        for (const auto& [id, health] : servers) {
            if (health.is_healthy) {
                healthy.push_back(id);
            }
        }
        return healthy;
    }
    
    bool is_server_healthy(int server_id) {
        std::lock_guard<std::mutex> lock(servers_mutex);
        auto it = servers.find(server_id);
        return it != servers.end() && it->second.is_healthy;
    }
};

// Global health checker instance
static std::unique_ptr<HealthChecker> g_health_checker;

int main(int argc, char** argv) {
    if (argc > 1) {
        std::string arg = argv[1];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: health_checker [OPTIONS]\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help     Show this help message and exit\n";
            std::cout << "  -v, --version  Show program's version number and exit\n";
            return 0;
        }
        if (arg == "-v" || arg == "-V" || arg == "--version") {
            std::cout << "Health Checker version: " << APP_VERSION << std::endl;
            return 0;
        }
    }
    
    try {
        g_health_checker = std::make_unique<HealthChecker>();
        g_health_checker->start();
    } catch (const std::exception& e) {
        std::cerr << "Error starting health checker: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

extern "C" {
    int start_health_checker() {
        try {
            g_health_checker = std::make_unique<HealthChecker>();
            g_health_checker->start();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error starting health checker: " << e.what() << std::endl;
            return -1;
        }
    }
    
    void stop_health_checker() {
        if (g_health_checker) {
            g_health_checker->stop();
            g_health_checker.reset();
        }
    }
}
