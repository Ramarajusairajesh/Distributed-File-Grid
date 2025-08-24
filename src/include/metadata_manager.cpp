#include "metadata_manager.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>

MetadataManager::MetadataManager(const std::string& redis_host, int redis_port) {
    redis_client_ = std::make_unique<RedisClient>(redis_host, redis_port);
}

MetadataManager::~MetadataManager() = default;

bool MetadataManager::initialize() {
    bool connected = redis_client_->connect();
    if (connected) {
        std::cout << "MetadataManager: Connected to Redis successfully" << std::endl;
        
        // Test the connection
        if (redis_client_->ping()) {
            std::cout << "MetadataManager: Redis ping successful" << std::endl;
        } else {
            std::cerr << "MetadataManager: Redis ping failed" << std::endl;
            return false;
        }
    } else {
        std::cerr << "MetadataManager: Failed to connect to Redis" << std::endl;
    }
    return connected;
}

bool MetadataManager::is_connected() const {
    return redis_client_->is_connected();
}

bool MetadataManager::store_file(const std::string& filename, const file_details::FileDetails& file_details) {
    std::string serialized = serialize_file_details(file_details);
    if (serialized.empty()) {
        std::cerr << "Failed to serialize file details for " << filename << std::endl;
        return false;
    }
    
    std::string key = file_key(filename);
    bool success = redis_client_->set(key, serialized);
    
    if (success) {
        // Add to files set
        redis_client_->sadd("files", filename);
        
        // Store chunk mappings
        for (const auto& chunk : file_details.chunks()) {
            store_chunk_location(chunk.chunk_id(), 
                               std::vector<std::string>(chunk.replication_servers().begin(), 
                                                       chunk.replication_servers().end()));
        }
        
        std::cout << "Stored file metadata for: " << filename << std::endl;
    }
    
    return success;
}

file_details::FileDetails MetadataManager::get_file(const std::string& filename) {
    std::string key = file_key(filename);
    std::string serialized = redis_client_->get(key);
    
    if (serialized.empty()) {
        std::cerr << "File not found: " << filename << std::endl;
        return file_details::FileDetails();
    }
    
    return deserialize_file_details(serialized);
}

bool MetadataManager::delete_file(const std::string& filename) {
    // Get file details first to clean up chunk mappings
    file_details::FileDetails details = get_file(filename);
    
    // Remove chunk mappings
    for (const auto& chunk : details.chunks()) {
        remove_chunk(chunk.chunk_id());
    }
    
    // Remove file metadata
    std::string key = file_key(filename);
    bool success = redis_client_->del(key);
    
    if (success) {
        // Remove from files set
        redis_client_->srem("files", filename);
        std::cout << "Deleted file metadata for: " << filename << std::endl;
    }
    
    return success;
}

std::vector<std::string> MetadataManager::list_files() {
    return redis_client_->smembers("files");
}

bool MetadataManager::file_exists(const std::string& filename) {
    std::string key = file_key(filename);
    return redis_client_->exists(key);
}

bool MetadataManager::store_chunk_location(const std::string& chunk_id, 
                                          const std::vector<std::string>& server_locations) {
    return redis_client_->store_chunk_metadata(chunk_id, server_locations);
}

std::vector<std::string> MetadataManager::get_chunk_locations(const std::string& chunk_id) {
    return redis_client_->get_chunk_servers(chunk_id);
}

bool MetadataManager::remove_chunk(const std::string& chunk_id) {
    std::string key = chunk_key(chunk_id);
    return redis_client_->del(key);
}

bool MetadataManager::update_server_status(const std::string& server_id, 
                                          const heartbeat::Heartbeat& heartbeat) {
    std::map<std::string, std::string> health_data;
    health_data["server_id"] = heartbeat.server_id();
    health_data["server_type"] = heartbeat.server_type();
    health_data["cpu_usage"] = std::to_string(heartbeat.cpu_usage());
    health_data["memory_usage"] = std::to_string(heartbeat.memory_usage());
    health_data["disk_usage"] = std::to_string(heartbeat.disk_usage());
    health_data["active_connections"] = std::to_string(heartbeat.active_connections());
    health_data["status"] = std::to_string(heartbeat.status());
    health_data["ip_address"] = heartbeat.ip_address();
    health_data["port"] = std::to_string(heartbeat.port());
    
    // Serialize and store the full heartbeat
    std::string serialized_hb = serialize_heartbeat(heartbeat);
    health_data["heartbeat_data"] = serialized_hb;
    
    return redis_client_->update_server_health(server_id, health_data);
}

heartbeat::Heartbeat MetadataManager::get_server_status(const std::string& server_id) {
    auto health_data = redis_client_->get_server_health(server_id);
    
    if (health_data.find("heartbeat_data") != health_data.end()) {
        return deserialize_heartbeat(health_data["heartbeat_data"]);
    }
    
    return heartbeat::Heartbeat();
}

std::vector<std::string> MetadataManager::get_active_servers() {
    return redis_client_->get_all_servers();
}

std::vector<std::string> MetadataManager::get_unhealthy_servers(int timeout_seconds) {
    std::vector<std::string> unhealthy_servers;
    auto all_servers = get_active_servers();
    
    auto now = std::chrono::system_clock::now();
    auto current_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    for (const auto& server_id : all_servers) {
        auto health_data = redis_client_->get_server_health(server_id);
        
        if (health_data.find("last_seen") != health_data.end()) {
            long last_seen = std::stol(health_data["last_seen"]);
            if (current_timestamp - last_seen > timeout_seconds) {
                unhealthy_servers.push_back(server_id);
            }
        } else {
            // No last_seen timestamp means server is unhealthy
            unhealthy_servers.push_back(server_id);
        }
    }
    
    return unhealthy_servers;
}

std::vector<std::string> MetadataManager::find_best_servers_for_chunk(int replication_factor) {
    auto server_chunk_counts = get_server_chunk_counts();
    auto active_servers = get_active_servers();
    
    // Remove unhealthy servers
    auto unhealthy = get_unhealthy_servers();
    for (const auto& unhealthy_server : unhealthy) {
        active_servers.erase(
            std::remove(active_servers.begin(), active_servers.end(), unhealthy_server),
            active_servers.end()
        );
    }
    
    // Sort servers by chunk count (ascending)
    std::sort(active_servers.begin(), active_servers.end(),
              [&server_chunk_counts](const std::string& a, const std::string& b) {
                  return server_chunk_counts[a] < server_chunk_counts[b];
              });
    
    // Return the servers with the least chunks
    std::vector<std::string> best_servers;
    int count = std::min(replication_factor, static_cast<int>(active_servers.size()));
    
    for (int i = 0; i < count; i++) {
        best_servers.push_back(active_servers[i]);
    }
    
    return best_servers;
}

std::map<std::string, int> MetadataManager::get_server_chunk_counts() {
    std::map<std::string, int> chunk_counts;
    auto servers = get_active_servers();
    
    for (const auto& server : servers) {
        auto chunks = get_chunks_on_server(server);
        chunk_counts[server] = chunks.size();
    }
    
    return chunk_counts;
}

std::vector<std::string> MetadataManager::get_chunks_on_server(const std::string& server_id) {
    std::string key = server_chunks_key(server_id);
    return redis_client_->smembers(key);
}

std::map<std::string, std::string> MetadataManager::get_system_stats() {
    std::map<std::string, std::string> stats;
    
    stats["total_files"] = std::to_string(get_total_files());
    stats["total_servers"] = std::to_string(get_total_servers());
    stats["active_servers"] = std::to_string(get_active_servers().size());
    stats["unhealthy_servers"] = std::to_string(get_unhealthy_servers().size());
    
    return stats;
}

size_t MetadataManager::get_total_files() const {
    auto files = redis_client_->smembers("files");
    return files.size();
}

size_t MetadataManager::get_total_servers() const {
    auto servers = redis_client_->smembers("servers");
    return servers.size();
}

// Private helper methods
std::string MetadataManager::serialize_file_details(const file_details::FileDetails& details) {
    std::string serialized;
    if (!details.SerializeToString(&serialized)) {
        std::cerr << "Failed to serialize FileDetails" << std::endl;
        return "";
    }
    return serialized;
}

file_details::FileDetails MetadataManager::deserialize_file_details(const std::string& data) {
    file_details::FileDetails details;
    if (!details.ParseFromString(data)) {
        std::cerr << "Failed to deserialize FileDetails" << std::endl;
    }
    return details;
}

std::string MetadataManager::serialize_heartbeat(const heartbeat::Heartbeat& hb) {
    std::string serialized;
    if (!hb.SerializeToString(&serialized)) {
        std::cerr << "Failed to serialize Heartbeat" << std::endl;
        return "";
    }
    return serialized;
}

heartbeat::Heartbeat MetadataManager::deserialize_heartbeat(const std::string& data) {
    heartbeat::Heartbeat hb;
    if (!hb.ParseFromString(data)) {
        std::cerr << "Failed to deserialize Heartbeat" << std::endl;
    }
    return hb;
}

std::string MetadataManager::file_key(const std::string& filename) const {
    return "file:" + filename;
}

std::string MetadataManager::chunk_key(const std::string& chunk_id) const {
    return "chunk:" + chunk_id;
}

std::string MetadataManager::server_key(const std::string& server_id) const {
    return "server:" + server_id;
}

std::string MetadataManager::server_chunks_key(const std::string& server_id) const {
    return "server_chunks:" + server_id;
} 