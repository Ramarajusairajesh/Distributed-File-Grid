#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "heart_beat.hpp"
#include "machine_details.hpp"

using boost::asio::ip::tcp;

class HealthChecker {
private:
    boost::asio::io_context io_context_;
    std::map<std::string, heartbeat::Heartbeat> server_status_;
    std::vector<std::string> head_servers_;
    std::vector<std::string> cluster_servers_;
    std::string current_leader_;
    bool is_leader_;
    std::thread monitoring_thread_;
    std::thread leader_election_thread_;
    bool running_;
    
    // Configuration
    int heartbeat_timeout_;
    int leader_election_timeout_;
    std::string checker_id_;

public:
    HealthChecker(const std::string& config_file = "health_checker_config.json")
        : is_leader_(false), running_(false), heartbeat_timeout_(60), leader_election_timeout_(30) {
        load_config(config_file);
        checker_id_ = MachineDetails::get_machine_id();
    }

    ~HealthChecker() {
        stop();
    }

    void start() {
        running_ = true;
        
        // Start monitoring thread
        monitoring_thread_ = std::thread(&HealthChecker::monitoring_loop, this);
        
        // Start leader election thread
        leader_election_thread_ = std::thread(&HealthChecker::leader_election_loop, this);
        
        std::cout << "Health checker started with ID: " << checker_id_ << std::endl;
        
        // Run the io_context
        io_context_.run();
    }

    void stop() {
        running_ = false;
        
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
        
        if (leader_election_thread_.joinable()) {
            leader_election_thread_.join();
        }
        
        io_context_.stop();
        std::cout << "Health checker stopped" << std::endl;
    }

private:
    void load_config(const std::string& config_file) {
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(config_file, pt);

            // Load head servers
            head_servers_.clear();
            for (const auto& server : pt.get_child("head_servers", boost::property_tree::ptree())) {
                head_servers_.push_back(server.second.data());
            }

            // Load cluster servers
            cluster_servers_.clear();
            for (const auto& server : pt.get_child("cluster_servers", boost::property_tree::ptree())) {
                cluster_servers_.push_back(server.second.data());
            }

            heartbeat_timeout_ = pt.get<int>("heartbeat_timeout", 60);
            leader_election_timeout_ = pt.get<int>("leader_election_timeout", 30);

            std::cout << "Configuration loaded from " << config_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            std::cout << "Using default configuration" << std::endl;
            set_default_config();
        }
    }

    void set_default_config() {
        head_servers_ = {
            "127.0.0.1:9669",
            "127.0.0.1:9670",
            "127.0.0.1:9671"
        };

        cluster_servers_ = {
            "127.0.0.1:8080",
            "127.0.0.1:8081",
            "127.0.0.1:8082"
        };

        heartbeat_timeout_ = 60;
        leader_election_timeout_ = 30;
    }

    void monitoring_loop() {
        while (running_) {
            if (is_leader_) {
                monitor_servers();
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    void leader_election_loop() {
        while (running_) {
            perform_leader_election();
            std::this_thread::sleep_for(std::chrono::seconds(leader_election_timeout_));
        }
    }

    void perform_leader_election() {
        // Simple leader election based on machine ID (lexicographical order)
        std::string new_leader = checker_id_;
        
        // In a real implementation, you would communicate with other health checkers
        // to determine the leader. For now, we'll use a simple approach.
        
        if (new_leader != current_leader_) {
            if (is_leader_) {
                std::cout << "Stepping down as leader" << std::endl;
                is_leader_ = false;
            } else {
                std::cout << "Becoming leader" << std::endl;
                is_leader_ = true;
            }
            current_leader_ = new_leader;
        }
    }

    void monitor_servers() {
        std::cout << "Monitoring servers..." << std::endl;
        
        // Monitor head servers
        for (const auto& server : head_servers_) {
            check_server_health(server, "head_server");
        }
        
        // Monitor cluster servers
        for (const auto& server : cluster_servers_) {
            check_server_health(server, "chunk_server");
        }
        
        // Check for failed servers and trigger failover if needed
        handle_failures();
    }

    void check_server_health(const std::string& server_address, const std::string& server_type) {
        try {
            // Parse server address
            size_t colon_pos = server_address.find(':');
            if (colon_pos == std::string::npos) {
                std::cerr << "Invalid server address format: " << server_address << std::endl;
                return;
            }
            
            std::string host = server_address.substr(0, colon_pos);
            std::string port_str = server_address.substr(colon_pos + 1);
            int port = std::stoi(port_str);
            
            // Create heartbeat request
            heartbeat::HeartbeatRequest request;
            request.set_requester_id(checker_id_);
            request.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            // Send heartbeat request
            boost::asio::io_context io_context;
            tcp::resolver resolver(io_context);
            tcp::resolver::results_type endpoints = resolver.resolve(host, port_str);
            
            tcp::socket socket(io_context);
            boost::asio::connect(socket, endpoints);
            
            // Send request
            std::string request_data = request.SerializeAsString();
            boost::asio::write(socket, boost::asio::buffer(request_data));
            
            // Read response
            std::vector<char> response_data(1024);
            size_t bytes_read = socket.read_some(boost::asio::buffer(response_data));
            
            // Parse response
            heartbeat::HeartbeatResponse response;
            if (response.ParseFromArray(response_data.data(), bytes_read)) {
                if (response.success()) {
                    const auto& heartbeat = response.heartbeat();
                    server_status_[server_address] = heartbeat;
                    
                    std::cout << "Server " << server_address << " is healthy: "
                              << "CPU=" << heartbeat.cpu_usage() << "%, "
                              << "Memory=" << heartbeat.memory_usage() << "%, "
                              << "Disk=" << heartbeat.disk_usage() << "%" << std::endl;
                } else {
                    std::cerr << "Server " << server_address << " reported error: "
                              << response.error_message() << std::endl;
                    mark_server_failed(server_address);
                }
            } else {
                std::cerr << "Failed to parse response from " << server_address << std::endl;
                mark_server_failed(server_address);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error checking health of " << server_address << ": " << e.what() << std::endl;
            mark_server_failed(server_address);
        }
    }

    void mark_server_failed(const std::string& server_address) {
        auto it = server_status_.find(server_address);
        if (it != server_status_.end()) {
            it->second.set_status(heartbeat::Heartbeat_Status_DEAD);
            std::cout << "Marked server " << server_address << " as failed" << std::endl;
        }
    }

    void handle_failures() {
        std::vector<std::string> failed_servers;
        
        for (const auto& pair : server_status_) {
            if (pair.second.status() == heartbeat::Heartbeat_Status_DEAD) {
                failed_servers.push_back(pair.first);
            }
        }
        
        if (!failed_servers.empty()) {
            std::cout << "Detected " << failed_servers.size() << " failed servers:" << std::endl;
            for (const auto& server : failed_servers) {
                std::cout << "  - " << server << std::endl;
            }
            
            // Trigger failover procedures
            trigger_failover(failed_servers);
        }
    }

    void trigger_failover(const std::vector<std::string>& failed_servers) {
        std::cout << "Triggering failover for failed servers..." << std::endl;
        
        for (const auto& failed_server : failed_servers) {
            if (is_head_server(failed_server)) {
                handle_head_server_failure(failed_server);
            } else {
                handle_cluster_server_failure(failed_server);
            }
        }
    }

    bool is_head_server(const std::string& server_address) {
        return std::find(head_servers_.begin(), head_servers_.end(), server_address) != head_servers_.end();
    }

    void handle_head_server_failure(const std::string& failed_server) {
        std::cout << "Handling head server failure: " << failed_server << std::endl;
        
        // Find a healthy head server to promote
        std::string new_leader = find_healthy_head_server();
        if (!new_leader.empty()) {
            std::cout << "Promoting " << new_leader << " as new head server leader" << std::endl;
            // TODO: Implement actual promotion logic
        } else {
            std::cerr << "No healthy head servers available for failover!" << std::endl;
        }
    }

    void handle_cluster_server_failure(const std::string& failed_server) {
        std::cout << "Handling cluster server failure: " << failed_server << std::endl;
        
        // TODO: Implement chunk re-replication logic
        std::cout << "Triggering chunk re-replication for failed server" << std::endl;
    }

    std::string find_healthy_head_server() {
        for (const auto& server : head_servers_) {
            auto it = server_status_.find(server);
            if (it != server_status_.end() && it->second.status() == heartbeat::Heartbeat_Status_ALIVE) {
                return server;
            }
        }
        return "";
    }

    void print_status() {
        std::cout << "Health Checker Status:" << std::endl;
        std::cout << "=====================" << std::endl;
        std::cout << "Checker ID: " << checker_id_ << std::endl;
        std::cout << "Is Leader: " << (is_leader_ ? "Yes" : "No") << std::endl;
        std::cout << "Current Leader: " << current_leader_ << std::endl;
        std::cout << "Server Status:" << std::endl;
        
        for (const auto& pair : server_status_) {
            const auto& heartbeat = pair.second;
            std::string status_str;
            switch (heartbeat.status()) {
                case heartbeat::Heartbeat_Status_ALIVE:
                    status_str = "ALIVE";
                    break;
                case heartbeat::Heartbeat_Status_DEAD:
                    status_str = "DEAD";
                    break;
                case heartbeat::Heartbeat_Status_WARNING:
                    status_str = "WARNING";
                    break;
                default:
                    status_str = "UNKNOWN";
                    break;
            }
            
            std::cout << "  " << pair.first << ": " << status_str
                      << " (CPU: " << heartbeat.cpu_usage() << "%, "
                      << "Memory: " << heartbeat.memory_usage() << "%, "
                      << "Disk: " << heartbeat.disk_usage() << "%)" << std::endl;
        }
        std::cout << "=====================" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::string config_file = "health_checker_config.json";
    
    if (argc > 1) {
        config_file = argv[1];
    }

    try {
        HealthChecker checker(config_file);
        
        // Start a thread to print status periodically
        std::thread status_thread([&checker]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                checker.print_status();
            }
        });
        
        checker.start();
        
        if (status_thread.joinable()) {
            status_thread.join();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 