#ifndef METADATA_MANAGER_HPP
#define METADATA_MANAGER_HPP

#include "redis_client.hpp"
#include "file_deatils.pb.h"
#include "heat_beat.pb.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

class MetadataManager {
private:
    std::unique_ptr<RedisClient> redis_client_;
    
public:
    MetadataManager(const std::string& redis_host = "127.0.0.1", int redis_port = 6379);
    ~MetadataManager();
    
    // Initialize connection
    bool initialize();
    bool is_connected() const;
    
    // File metadata operations
    bool store_file(const std::string& filename, const file_details::FileDetails& file_details);
    file_details::FileDetails get_file(const std::string& filename);
    bool delete_file(const std::string& filename);
    std::vector<std::string> list_files();
    bool file_exists(const std::string& filename);
    
    // Chunk metadata operations
    bool store_chunk_location(const std::string& chunk_id, const std::vector<std::string>& server_locations);
    std::vector<std::string> get_chunk_locations(const std::string& chunk_id);
    bool remove_chunk(const std::string& chunk_id);
    bool add_chunk_to_server(const std::string& server_id, const std::string& chunk_id);
    bool remove_chunk_from_server(const std::string& server_id, const std::string& chunk_id);
    
    // Server health tracking
    bool update_server_status(const std::string& server_id, const heartbeat::Heartbeat& heartbeat);
    heartbeat::Heartbeat get_server_status(const std::string& server_id);
    std::vector<std::string> get_active_servers();
    std::vector<std::string> get_unhealthy_servers(int timeout_seconds = 120);
    
    // Chunk distribution and replication
    std::vector<std::string> find_best_servers_for_chunk(int replication_factor = 3);
    std::map<std::string, int> get_server_chunk_counts();
    std::vector<std::string> get_chunks_on_server(const std::string& server_id);
    
    // Statistics
    size_t get_total_files() const;
    size_t get_total_chunks() const;
    size_t get_total_servers() const;
    std::map<std::string, std::string> get_system_stats();
    
private:
    // Helper methods
    std::string serialize_file_details(const file_details::FileDetails& details);
    file_details::FileDetails deserialize_file_details(const std::string& data);
    std::string serialize_heartbeat(const heartbeat::Heartbeat& hb);
    heartbeat::Heartbeat deserialize_heartbeat(const std::string& data);
    
    // Key generation helpers
    std::string file_key(const std::string& filename) const;
    std::string chunk_key(const std::string& chunk_id) const;
    std::string server_key(const std::string& server_id) const;
    std::string server_chunks_key(const std::string& server_id) const;
};

#endif // METADATA_MANAGER_HPP 