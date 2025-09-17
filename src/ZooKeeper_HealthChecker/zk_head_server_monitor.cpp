#include "../include/version.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <fstream>

// ZooKeeper simulation (in production, use libzookeeper)
namespace zk_sim {
    struct ZNode {
        std::string data;
        std::chrono::steady_clock::time_point last_update;
        bool ephemeral = false;
    };
    
    class ZooKeeperClient {
    private:
        std::map<std::string, ZNode> nodes;
        std::mutex nodes_mutex;
        std::string session_id;
        
    public:
        ZooKeeperClient(const std::string& connection_string) {
            session_id = "session_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            std::cout << "ZooKeeper client connected to: " << connection_string << std::endl;
            std::cout << "Session ID: " << session_id << std::endl;
        }
        
        bool create_node(const std::string& path, const std::string& data, bool ephemeral = false) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            if (nodes.find(path) != nodes.end()) {
                return false; // Node already exists
            }
            
            ZNode node;
            node.data = data;
            node.last_update = std::chrono::steady_clock::now();
            node.ephemeral = ephemeral;
            nodes[path] = node;
            
            std::cout << "Created ZNode: " << path << " (ephemeral: " << ephemeral << ")" << std::endl;
            return true;
        }
        
        bool update_node(const std::string& path, const std::string& data) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            auto it = nodes.find(path);
            if (it == nodes.end()) {
                return false;
            }
            
            it->second.data = data;
            it->second.last_update = std::chrono::steady_clock::now();
            return true;
        }
        
        std::string get_node_data(const std::string& path) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            auto it = nodes.find(path);
            if (it == nodes.end()) {
                return "";
            }
            return it->second.data;
        }
        
        bool node_exists(const std::string& path) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            return nodes.find(path) != nodes.end();
        }
        
        bool delete_node(const std::string& path) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            auto it = nodes.find(path);
            if (it == nodes.end()) {
                return false;
            }
            nodes.erase(it);
            std::cout << "Deleted ZNode: " << path << std::endl;
            return true;
        }
        
        std::vector<std::string> list_children(const std::string& parent_path) {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            std::vector<std::string> children;
            
            for (const auto& [path, node] : nodes) {
                if (path.find(parent_path + "/") == 0 && 
                    path.substr(parent_path.length() + 1).find('/') == std::string::npos) {
                    children.push_back(path.substr(parent_path.length() + 1));
                }
            }
            return children;
        }
        
        void cleanup_ephemeral_nodes() {
            std::lock_guard<std::mutex> lock(nodes_mutex);
            auto now = std::chrono::steady_clock::now();
            
            for (auto it = nodes.begin(); it != nodes.end();) {
                if (it->second.ephemeral && 
                    std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_update).count() > 60) {
                    std::cout << "Cleaning up stale ephemeral node: " << it->first << std::endl;
                    it = nodes.erase(it);
                } else {
                    ++it;
                }
            }
        }
    };
}

struct HeadServerInfo {
    std::string server_id;
    std::string ip_address;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
    bool is_leader = false;
    std::string status = "unknown";
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    int active_connections = 0;
};

class ZooKeeperHeadServerMonitor {
private:
    std::unique_ptr<zk_sim::ZooKeeperClient> zk_client;
    std::map<std::string, HeadServerInfo> head_servers;
    std::mutex servers_mutex;
    std::atomic<bool> running{false};
    std::string monitor_id;
    std::string leader_server_id;
    
    const std::string ZK_ROOT_PATH = "/distributed_file_grid";
    const std::string HEAD_SERVERS_PATH = ZK_ROOT_PATH + "/head_servers";
    const std::string MONITORS_PATH = ZK_ROOT_PATH + "/monitors";
    const std::chrono::seconds HEARTBEAT_TIMEOUT{30};
    const std::chrono::seconds MONITOR_INTERVAL{10};
    
    bool check_head_server_health(const HeadServerInfo& server) {
        // Simple TCP connection test
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return false;
        }
        
        // Set socket to non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server.port);
        inet_pton(AF_INET, server.ip_address.c_str(), &addr.sin_addr);
        
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        close(sock);
        
        // For simulation, we'll consider the server healthy if it's been updated recently
        auto now = std::chrono::steady_clock::now();
        auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - server.last_heartbeat);
        
        return time_since_heartbeat < HEARTBEAT_TIMEOUT;
    }
    
    void register_monitor() {
        std::string monitor_path = MONITORS_PATH + "/" + monitor_id;
        std::string monitor_data = "monitor_id=" + monitor_id + ",start_time=" + 
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        
        zk_client->create_node(monitor_path, monitor_data, true); // ephemeral node
        std::cout << "Registered monitor: " << monitor_id << std::endl;
    }
    
    void discover_head_servers() {
        auto server_children = zk_client->list_children(HEAD_SERVERS_PATH);
        
        std::lock_guard<std::mutex> lock(servers_mutex);
        
        // Clear existing servers and rediscover
        head_servers.clear();
        
        for (const auto& server_name : server_children) {
            std::string server_path = HEAD_SERVERS_PATH + "/" + server_name;
            std::string server_data = zk_client->get_node_data(server_path);
            
            if (!server_data.empty()) {
                HeadServerInfo info = parse_server_data(server_data);
                info.server_id = server_name;
                head_servers[server_name] = info;
                
                std::cout << "Discovered head server: " << server_name 
                         << " at " << info.ip_address << ":" << info.port << std::endl;
            }
        }
    }
    
    HeadServerInfo parse_server_data(const std::string& data) {
        HeadServerInfo info;
        std::istringstream iss(data);
        std::string token;
        
        while (std::getline(iss, token, ',')) {
            size_t eq_pos = token.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = token.substr(0, eq_pos);
                std::string value = token.substr(eq_pos + 1);
                
                if (key == "ip") {
                    info.ip_address = value;
                } else if (key == "port") {
                    info.port = std::stoi(value);
                } else if (key == "status") {
                    info.status = value;
                } else if (key == "cpu_usage") {
                    info.cpu_usage = std::stod(value);
                } else if (key == "memory_usage") {
                    info.memory_usage = std::stod(value);
                } else if (key == "active_connections") {
                    info.active_connections = std::stoi(value);
                } else if (key == "last_update") {
                    // Parse timestamp
                    auto timestamp = std::chrono::steady_clock::time_point(
                        std::chrono::nanoseconds(std::stoll(value)));
                    info.last_heartbeat = timestamp;
                }
            }
        }
        
        return info;
    }
    
    void perform_leader_election() {
        std::lock_guard<std::mutex> lock(servers_mutex);
        
        std::string current_leader;
        HeadServerInfo* best_candidate = nullptr;
        
        // Find current leader
        for (auto& [server_id, info] : head_servers) {
            if (info.is_leader) {
                current_leader = server_id;
                break;
            }
        }
        
        // Check if current leader is healthy
        if (!current_leader.empty()) {
            auto& leader_info = head_servers[current_leader];
            if (check_head_server_health(leader_info)) {
                std::cout << "Current leader " << current_leader << " is healthy" << std::endl;
                return; // Leader is healthy, no election needed
            } else {
                std::cout << "Current leader " << current_leader << " is unhealthy, starting election" << std::endl;
                leader_info.is_leader = false;
            }
        }
        
        // Find best candidate (lowest server_id among healthy servers)
        for (auto& [server_id, info] : head_servers) {
            if (check_head_server_health(info)) {
                if (!best_candidate || server_id < best_candidate->server_id) {
                    best_candidate = &info;
                    best_candidate->server_id = server_id;
                }
            }
        }
        
        if (best_candidate) {
            best_candidate->is_leader = true;
            leader_server_id = best_candidate->server_id;
            
            // Update ZooKeeper with new leader info
            std::string leader_path = ZK_ROOT_PATH + "/leader";
            std::string leader_data = "server_id=" + leader_server_id + 
                                    ",elected_at=" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            
            if (zk_client->node_exists(leader_path)) {
                zk_client->update_node(leader_path, leader_data);
            } else {
                zk_client->create_node(leader_path, leader_data);
            }
            
            std::cout << "New leader elected: " << leader_server_id << std::endl;
        } else {
            std::cout << "No healthy head servers found for leader election" << std::endl;
        }
    }
    
    void monitor_loop() {
        while (running) {
            try {
                // Discover head servers
                discover_head_servers();
                
                // Perform health checks
                {
                    std::lock_guard<std::mutex> lock(servers_mutex);
                    for (auto& [server_id, info] : head_servers) {
                        bool healthy = check_head_server_health(info);
                        std::string old_status = info.status;
                        info.status = healthy ? "healthy" : "unhealthy";
                        
                        if (old_status != info.status) {
                            std::cout << "Head server " << server_id << " status changed: " 
                                     << old_status << " -> " << info.status << std::endl;
                        }
                    }
                }
                
                // Perform leader election if needed
                perform_leader_election();
                
                // Clean up stale ephemeral nodes
                zk_client->cleanup_ephemeral_nodes();
                
                // Generate health report
                generate_health_report();
                
            } catch (const std::exception& e) {
                std::cerr << "Error in monitor loop: " << e.what() << std::endl;
            }
            
            std::this_thread::sleep_for(MONITOR_INTERVAL);
        }
    }
    
    void generate_health_report() {
        std::lock_guard<std::mutex> lock(servers_mutex);
        
        std::ostringstream report;
        report << "=== Head Server Health Report ===" << std::endl;
        report << "Monitor ID: " << monitor_id << std::endl;
        report << "Current Leader: " << (leader_server_id.empty() ? "None" : leader_server_id) << std::endl;
        report << "Total Head Servers: " << head_servers.size() << std::endl;
        
        int healthy_count = 0;
        for (const auto& [server_id, info] : head_servers) {
            if (info.status == "healthy") healthy_count++;
            
            report << "  Server: " << server_id 
                   << " | Status: " << info.status
                   << " | Address: " << info.ip_address << ":" << info.port
                   << " | Leader: " << (info.is_leader ? "Yes" : "No")
                   << " | CPU: " << info.cpu_usage << "%"
                   << " | Memory: " << info.memory_usage << "%"
                   << " | Connections: " << info.active_connections << std::endl;
        }
        
        report << "Healthy Servers: " << healthy_count << "/" << head_servers.size() << std::endl;
        
        // Store report in ZooKeeper
        std::string report_path = ZK_ROOT_PATH + "/health_reports/" + monitor_id;
        zk_client->create_node(ZK_ROOT_PATH + "/health_reports", "", false);
        
        if (zk_client->node_exists(report_path)) {
            zk_client->update_node(report_path, report.str());
        } else {
            zk_client->create_node(report_path, report.str());
        }
        
        std::cout << report.str() << std::endl;
    }

public:
    ZooKeeperHeadServerMonitor(const std::string& zk_connection_string) {
        monitor_id = "monitor_" + std::to_string(getpid()) + "_" + 
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        
        zk_client = std::make_unique<zk_sim::ZooKeeperClient>(zk_connection_string);
        
        // Initialize ZooKeeper structure
        zk_client->create_node(ZK_ROOT_PATH, "Distributed File Grid Root");
        zk_client->create_node(HEAD_SERVERS_PATH, "Head Servers Registry");
        zk_client->create_node(MONITORS_PATH, "Monitors Registry");
        zk_client->create_node(ZK_ROOT_PATH + "/health_reports", "Health Reports");
    }
    
    void start() {
        running = true;
        std::cout << "Starting ZooKeeper Head Server Monitor..." << std::endl;
        
        register_monitor();
        
        // Start monitoring thread
        std::thread monitor_thread(&ZooKeeperHeadServerMonitor::monitor_loop, this);
        monitor_thread.detach();
        
        std::cout << "ZooKeeper Head Server Monitor started with ID: " << monitor_id << std::endl;
    }
    
    void stop() {
        running = false;
        std::cout << "Stopping ZooKeeper Head Server Monitor..." << std::endl;
    }
    
    void simulate_head_server_registration(const std::string& server_id, 
                                         const std::string& ip, int port) {
        std::string server_path = HEAD_SERVERS_PATH + "/" + server_id;
        std::string server_data = "ip=" + ip + ",port=" + std::to_string(port) + 
                                ",status=healthy,cpu_usage=25.5,memory_usage=60.2,active_connections=10" +
                                ",last_update=" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        
        zk_client->create_node(server_path, server_data, true); // ephemeral
        std::cout << "Simulated head server registration: " << server_id << std::endl;
    }
    
    void run_interactive_mode() {
        std::cout << "\n=== ZooKeeper Head Server Monitor Interactive Mode ===" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  register <server_id> <ip> <port> - Register a head server" << std::endl;
        std::cout << "  status - Show current status" << std::endl;
        std::cout << "  leader - Show current leader" << std::endl;
        std::cout << "  quit - Exit" << std::endl;
        
        std::string line;
        while (running && std::getline(std::cin, line)) {
            std::istringstream iss(line);
            std::string command;
            iss >> command;
            
            if (command == "register") {
                std::string server_id, ip;
                int port;
                if (iss >> server_id >> ip >> port) {
                    simulate_head_server_registration(server_id, ip, port);
                } else {
                    std::cout << "Usage: register <server_id> <ip> <port>" << std::endl;
                }
            } else if (command == "status") {
                generate_health_report();
            } else if (command == "leader") {
                std::cout << "Current leader: " << (leader_server_id.empty() ? "None" : leader_server_id) << std::endl;
            } else if (command == "quit") {
                break;
            } else {
                std::cout << "Unknown command: " << command << std::endl;
            }
        }
    }
};

// Global monitor instance
static std::unique_ptr<ZooKeeperHeadServerMonitor> g_monitor;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_monitor) {
        g_monitor->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: zk_head_server_monitor [OPTIONS]\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help     Show this help message and exit\n";
            std::cout << "  -v, --version  Show program's version number and exit\n";
            std::cout << "  -i, --interactive  Run in interactive mode\n";
            std::cout << "  --zk-hosts HOSTS   ZooKeeper connection string (default: localhost:2181)\n";
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "ZooKeeper Head Server Monitor version: " << APP_VERSION << std::endl;
            return 0;
        }
    }
    
    std::string zk_hosts = "localhost:2181";
    bool interactive = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--zk-hosts" && i + 1 < argc) {
            zk_hosts = argv[++i];
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        }
    }
    
    try {
        g_monitor = std::make_unique<ZooKeeperHeadServerMonitor>(zk_hosts);
        g_monitor->start();
        
        // Simulate some head servers for testing
        g_monitor->simulate_head_server_registration("head_server_1", "127.0.0.1", 9669);
        g_monitor->simulate_head_server_registration("head_server_2", "127.0.0.1", 9670);
        
        if (interactive) {
            g_monitor->run_interactive_mode();
        } else {
            // Keep running until signal
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}