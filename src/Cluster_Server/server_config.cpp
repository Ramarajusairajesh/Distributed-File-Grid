#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

class ServerConfig {
private:
    std::map<std::string, std::string> config_;
    std::string config_file_;

public:
    ServerConfig(const std::string& config_file = "cluster_server_config.json") 
        : config_file_(config_file) {
        load_config();
    }

    void load_config() {
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(config_file_, pt);

            config_["server_id"] = pt.get<std::string>("server_id", "cluster_server_1");
            config_["storage_path"] = pt.get<std::string>("storage_path", "./storage");
            config_["port"] = pt.get<std::string>("port", "8080");
            config_["replication_factor"] = pt.get<std::string>("replication_factor", "3");
            config_["heartbeat_interval"] = pt.get<std::string>("heartbeat_interval", "30");
            config_["max_chunk_size"] = pt.get<std::string>("max_chunk_size", "67108864"); // 64MB
            config_["head_server_ip"] = pt.get<std::string>("head_server_ip", "127.0.0.1");
            config_["head_server_port"] = pt.get<std::string>("head_server_port", "9669");

            std::cout << "Configuration loaded from " << config_file_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            std::cout << "Using default configuration" << std::endl;
            set_default_config();
        }
    }

    void set_default_config() {
        config_["server_id"] = "cluster_server_1";
        config_["storage_path"] = "./storage";
        config_["port"] = "8080";
        config_["replication_factor"] = "3";
        config_["heartbeat_interval"] = "30";
        config_["max_chunk_size"] = "67108864";
        config_["head_server_ip"] = "127.0.0.1";
        config_["head_server_port"] = "9669";
    }

    void save_config() {
        try {
            boost::property_tree::ptree pt;
            
            for (const auto& pair : config_) {
                pt.put(pair.first, pair.second);
            }

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

    void print_config() const {
        std::cout << "Server Configuration:" << std::endl;
        std::cout << "====================" << std::endl;
        for (const auto& pair : config_) {
            std::cout << pair.first << ": " << pair.second << std::endl;
        }
        std::cout << "====================" << std::endl;
    }
};
