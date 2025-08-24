#ifndef REDIS_CLIENT_HPP
#define REDIS_CLIENT_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <hiredis/hiredis.h>
#include <iostream>

class RedisClient {
private:
    redisContext* context_;
    std::string host_;
    int port_;
    bool connected_;

public:
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379);
    ~RedisClient();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }
    bool ping();

    // Basic operations
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    // Hash operations for file metadata
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    std::map<std::string, std::string> hgetall(const std::string& key);
    bool hdel(const std::string& key, const std::string& field);

    // List operations for chunk tracking
    bool lpush(const std::string& key, const std::string& value);
    bool rpush(const std::string& key, const std::string& value);
    std::string lpop(const std::string& key);
    std::string rpop(const std::string& key);
    std::vector<std::string> lrange(const std::string& key, int start, int stop);

    // Set operations for server tracking
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);
    bool sismember(const std::string& key, const std::string& member);

    // Expiration
    bool expire(const std::string& key, int seconds);

    // File metadata specific operations
    bool store_file_metadata(const std::string& filename, const std::map<std::string, std::string>& metadata);
    std::map<std::string, std::string> get_file_metadata(const std::string& filename);
    bool delete_file_metadata(const std::string& filename);
    
    // Chunk metadata operations
    bool store_chunk_metadata(const std::string& chunk_id, const std::vector<std::string>& servers);
    std::vector<std::string> get_chunk_servers(const std::string& chunk_id);
    bool add_chunk_to_server(const std::string& server_id, const std::string& chunk_id);
    bool remove_chunk_from_server(const std::string& server_id, const std::string& chunk_id);
    
    // Server health tracking
    bool update_server_health(const std::string& server_id, const std::map<std::string, std::string>& health_data);
    std::map<std::string, std::string> get_server_health(const std::string& server_id);
    std::vector<std::string> get_all_servers();

private:
    bool check_reply(redisReply* reply);
    void free_reply(redisReply* reply);
};

#endif // REDIS_CLIENT_HPP 