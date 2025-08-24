#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem> // Added for create_directories
#include <algorithm>  // Added for std::remove

class HeadServerConfig {
private:
    std::map<std::string, std::string> config_;
    std::vector<std::string> cluster_servers_;
    std::string config_file_;

public:
    HeadServerConfig(const std::string& config_file = "head_server_config.json") 
        : config_file_(config_file) {
        load_config();
    }

    void load_config() {
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(config_file_, pt);

            config_["server_id"] = pt.get<std::string>("server_id", "head_server_1");
            config_["port"] = pt.get<std::string>("port", "9669");
            config_["redis_port"] = pt.get<std::string>("redis_port", "6379");
            config_["chunk_size"] = pt.get<std::string>("chunk_size", "67108864"); // 64MB
            config_["replication_factor"] = pt.get<std::string>("replication_factor", "3");
            config_["heartbeat_timeout"] = pt.get<std::string>("heartbeat_timeout", "60");
            config_["max_connections"] = pt.get<std::string>("max_connections", "1000");
            config_["log_level"] = pt.get<std::string>("log_level", "INFO");
            config_["metadata_path"] = pt.get<std::string>("metadata_path", "./metadata");
            config_["temp_path"] = pt.get<std::string>("temp_path", "./temp");

            // Load cluster servers
            cluster_servers_.clear();
            try {
                auto cluster_servers_node = pt.get_child("cluster_servers");
                for (const auto& server : cluster_servers_node) {
                    cluster_servers_.push_back(server.second.data());
                }
            } catch (const boost::property_tree::ptree_bad_path& e) {
                // No cluster servers configured, will use defaults
            }

            std::cout << "Configuration loaded from " << config_file_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            std::cout << "Using default configuration" << std::endl;
            set_default_config();
        }
    }

    void set_default_config() {
        config_["server_id"] = "head_server_1";
        config_["port"] = "9669";
        config_["redis_port"] = "6379";
        config_["chunk_size"] = "67108864";
        config_["replication_factor"] = "3";
        config_["heartbeat_timeout"] = "60";
        config_["max_connections"] = "1000";
        config_["log_level"] = "INFO";
        config_["metadata_path"] = "./metadata";
        config_["temp_path"] = "./temp";

        // Default cluster servers
        cluster_servers_ = {
            "127.0.0.1:8080",
            "127.0.0.1:8081",
            "127.0.0.1:8082"
        };
    }

    void save_config() {
        try {
            boost::property_tree::ptree pt;
            
            for (const auto& pair : config_) {
                pt.put(pair.first, pair.second);
            }

            // Save cluster servers
            boost::property_tree::ptree servers_array;
            for (const auto& server : cluster_servers_) {
                boost::property_tree::ptree server_node;
                server_node.put("", server);
                servers_array.push_back(std::make_pair("", server_node));
            }
            pt.add_child("cluster_servers", servers_array);

            boost::property_tree::write_json(config_file_, pt);
            std::cout << "Configuration saved to " << config_file_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error saving configuration: " << e.what() << std::endl;
        }
    }

    std::string get(const std::string& key, const std::string& default_value = "") const {
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : default_value;
    }

    int get_int(const std::string& key, int default_value = 0) const {
        auto it = config_.find(key);
        if (it != config_.end()) {
            try {
                return std::stoi(it->second);
            } catch (const std::exception& e) {
                std::cerr << "Error converting " << key << " to int: " << e.what() << std::endl;
            }
        }
        return default_value;
    }

    void set(const std::string& key, const std::string& value) {
        config_[key] = value;
    }

    const std::vector<std::string>& get_cluster_servers() const {
        return cluster_servers_;
    }

    void add_cluster_server(const std::string& server) {
        cluster_servers_.push_back(server);
    }

    void remove_cluster_server(const std::string& server) {
        cluster_servers_.erase(
            std::remove(cluster_servers_.begin(), cluster_servers_.end(), server),
            cluster_servers_.end()
        );
    }

    void print_config() const {
        std::cout << "Head Server Configuration:" << std::endl;
        std::cout << "=========================" << std::endl;
        for (const auto& pair : config_) {
            std::cout << pair.first << ": " << pair.second << std::endl;
        }
        std::cout << "Cluster Servers:" << std::endl;
        for (const auto& server : cluster_servers_) {
            std::cout << "  - " << server << std::endl;
        }
        std::cout << "=========================" << std::endl;
    }

    // Create necessary directories
    void create_directories() {
        std::filesystem::create_directories(config_["metadata_path"]);
        std::filesystem::create_directories(config_["temp_path"]);
        std::filesystem::create_directories("/var/log/head_server");
    }
}; 