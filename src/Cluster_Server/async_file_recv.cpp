#include "../include/heart_beat_signal.hpp"
#include "../include/system_info.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace fs = std::filesystem;

class ChunkStorage {
private:
    std::string storage_path = "/tmp/cluster_storage/";
    std::unordered_map<std::string, std::string> chunk_registry;
    std::mutex registry_mutex;
    
    void ensure_storage_directory() {
        fs::create_directories(storage_path);
    }
    
    std::string generate_chunk_path(const std::string& chunk_id) {
        return storage_path + "chunk_" + chunk_id + ".dat";
    }

public:
    ChunkStorage() {
        ensure_storage_directory();
    }
    
    bool store_chunk(const std::string& chunk_id, const std::vector<char>& data) {
        try {
            std::string chunk_path = generate_chunk_path(chunk_id);
            
            std::ofstream file(chunk_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to create chunk file: " << chunk_path << std::endl;
                return false;
            }
            
            file.write(data.data(), data.size());
            file.close();
            
            // Register chunk in memory
            {
                std::lock_guard<std::mutex> lock(registry_mutex);
                chunk_registry[chunk_id] = chunk_path;
            }
            
            std::cout << "Stored chunk " << chunk_id << " (" << data.size() << " bytes)" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error storing chunk " << chunk_id << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<char> retrieve_chunk(const std::string& chunk_id) {
        std::vector<char> data;
        
        try {
            std::string chunk_path;
            {
                std::lock_guard<std::mutex> lock(registry_mutex);
                auto it = chunk_registry.find(chunk_id);
                if (it == chunk_registry.end()) {
                    std::cerr << "Chunk " << chunk_id << " not found in registry" << std::endl;
                    return data;
                }
                chunk_path = it->second;
            }
            
            std::ifstream file(chunk_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to open chunk file: " << chunk_path << std::endl;
                return data;
            }
            
            // Get file size
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            data.resize(file_size);
            file.read(data.data(), file_size);
            file.close();
            
            std::cout << "Retrieved chunk " << chunk_id << " (" << data.size() << " bytes)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error retrieving chunk " << chunk_id << ": " << e.what() << std::endl;
        }
        
        return data;
    }
    
    bool delete_chunk(const std::string& chunk_id) {
        try {
            std::string chunk_path;
            {
                std::lock_guard<std::mutex> lock(registry_mutex);
                auto it = chunk_registry.find(chunk_id);
                if (it == chunk_registry.end()) {
                    return false;
                }
                chunk_path = it->second;
                chunk_registry.erase(it);
            }
            
            fs::remove(chunk_path);
            std::cout << "Deleted chunk " << chunk_id << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error deleting chunk " << chunk_id << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> list_chunks() {
        std::vector<std::string> chunks;
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            for (const auto& [chunk_id, path] : chunk_registry) {
                chunks.push_back(chunk_id);
            }
        }
        return chunks;
    }
    
    size_t get_storage_usage() {
        size_t total_size = 0;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(storage_path)) {
                if (entry.is_regular_file()) {
                    total_size += entry.file_size();
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error calculating storage usage: " << e.what() << std::endl;
        }
        return total_size;
    }
};

class ClusterServerService {
private:
    ChunkStorage storage;
    int server_id;
    std::string server_ip;
    int port;
    bool running = false;
    
    async_hb::task heartbeat_sender(async_hb::Reactor& reactor) {
        while (running) {
            try {
                // Send heartbeat to head server
                async_hb::send_signal("127.0.0.1", server_id, 9000);
                co_await reactor.sleep_for(std::chrono::seconds(30));
            } catch (const std::exception& e) {
                std::cerr << "Heartbeat error: " << e.what() << std::endl;
                co_await reactor.sleep_for(std::chrono::seconds(5));
            }
        }
    }
    
    async_hb::task chunk_server(async_hb::Reactor& reactor) {
        // Simple chunk server implementation
        // In a real implementation, this would be an HTTP/gRPC server
        std::cout << "Chunk server started on port " << port << std::endl;
        
        while (running) {
            // Simulate chunk operations
            co_await reactor.sleep_for(std::chrono::seconds(1));
            
            // Report system status periodically
            static int counter = 0;
            if (++counter % 60 == 0) { // Every minute
                auto usage = system_monitor();
                std::cout << "Server " << server_id << " - CPU: " << usage.cpu_usage 
                         << "%, RAM: " << usage.ram_usage << "%, Disk: " << usage.disk_usage << "%" << std::endl;
                
                size_t storage_usage = storage.get_storage_usage();
                std::cout << "Storage usage: " << storage_usage / (1024*1024) << " MB" << std::endl;
            }
        }
    }

public:
    ClusterServerService(int id, const std::string& ip, int p) 
        : server_id(id), server_ip(ip), port(p) {}
    
    void start() {
        running = true;
        std::cout << "Starting Cluster Server " << server_id << " on " << server_ip << ":" << port << std::endl;
        
        async_hb::Reactor reactor;
        
        // Start heartbeat sender
        reactor.spawn(heartbeat_sender(reactor));
        
        // Start chunk server
        reactor.spawn(chunk_server(reactor));
        
        // Run the reactor
        reactor.run();
    }
    
    void stop() {
        running = false;
        std::cout << "Stopping Cluster Server " << server_id << std::endl;
    }
    
    // Chunk operations API
    bool store_chunk(const std::string& chunk_id, const std::vector<char>& data) {
        return storage.store_chunk(chunk_id, data);
    }
    
    std::vector<char> retrieve_chunk(const std::string& chunk_id) {
        return storage.retrieve_chunk(chunk_id);
    }
    
    bool delete_chunk(const std::string& chunk_id) {
        return storage.delete_chunk(chunk_id);
    }
    
    std::vector<std::string> list_chunks() {
        return storage.list_chunks();
    }
};

// Global cluster server instance
static std::unique_ptr<ClusterServerService> g_cluster_server;

extern "C" {
    int start_cluster_server(int server_id, const char* ip, int port) {
        try {
            g_cluster_server = std::make_unique<ClusterServerService>(server_id, ip, port);
            g_cluster_server->start();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error starting cluster server: " << e.what() << std::endl;
            return -1;
        }
    }
    
    void stop_cluster_server() {
        if (g_cluster_server) {
            g_cluster_server->stop();
            g_cluster_server.reset();
        }
    }
}