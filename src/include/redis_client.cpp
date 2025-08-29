#include "redis_client.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

RedisClient::RedisClient(const std::string &host, int port)
    : context_(nullptr), host_(host), port_(port), connected_(false)
{
}

RedisClient::~RedisClient() { disconnect(); }

bool RedisClient::connect()
{
        if (connected_)
        {
                return true;
        }

        context_ = redisConnect(host_.c_str(), port_);
        if (context_ == nullptr || context_->err)
        {
                if (context_)
                {
                        std::cerr << "Redis connection error: " << context_->errstr << std::endl;
                        redisFree(context_);
                        context_ = nullptr;
                }
                else
                {
                        std::cerr << "Redis connection error: can't allocate redis context"
                                  << std::endl;
                }
                connected_ = false;
                return false;
        }

        connected_ = true;
        std::cout << "Connected to Redis server at " << host_ << ":" << port_ << std::endl;
        return true;
}

void RedisClient::disconnect()
{
        if (context_)
        {
                redisFree(context_);
                context_ = nullptr;
        }
        connected_ = false;
}

bool RedisClient::ping()
{
        if (!connected_)
                return false;

        redisReply *reply = (redisReply *)redisCommand(context_, "PING");
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "PONG");
        free_reply(reply);
        return success;
}

bool RedisClient::set(const std::string &key, const std::string &value)
{
        if (!connected_)
                return false;

        redisReply *reply =
            (redisReply *)redisCommand(context_, "SET %s %s", key.c_str(), value.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_STATUS);
        free_reply(reply);
        return success;
}

std::string RedisClient::get(const std::string &key)
{
        if (!connected_)
                return "";

        redisReply *reply = (redisReply *)redisCommand(context_, "GET %s", key.c_str());
        if (!check_reply(reply))
        {
                return "";
        }

        std::string result;
        if (reply->type == REDIS_REPLY_STRING)
        {
                result = std::string(reply->str, reply->len);
        }

        free_reply(reply);
        return result;
}

bool RedisClient::del(const std::string &key)
{
        if (!connected_)
                return false;

        redisReply *reply = (redisReply *)redisCommand(context_, "DEL %s", key.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        free_reply(reply);
        return success;
}

bool RedisClient::exists(const std::string &key)
{
        if (!connected_)
                return false;

        redisReply *reply = (redisReply *)redisCommand(context_, "EXISTS %s", key.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool exists = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        free_reply(reply);
        return exists;
}

bool RedisClient::hset(const std::string &key, const std::string &field, const std::string &value)
{
        if (!connected_)
                return false;

        redisReply *reply = (redisReply *)redisCommand(context_, "HSET %s %s %s", key.c_str(),
                                                       field.c_str(), value.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_INTEGER);
        free_reply(reply);
        return success;
}

std::string RedisClient::hget(const std::string &key, const std::string &field)
{
        if (!connected_)
                return "";

        redisReply *reply =
            (redisReply *)redisCommand(context_, "HGET %s %s", key.c_str(), field.c_str());
        if (!check_reply(reply))
        {
                return "";
        }

        std::string result;
        if (reply->type == REDIS_REPLY_STRING)
        {
                result = std::string(reply->str, reply->len);
        }

        free_reply(reply);
        return result;
}

std::map<std::string, std::string> RedisClient::hgetall(const std::string &key)
{
        std::map<std::string, std::string> result;
        if (!connected_)
                return result;

        redisReply *reply = (redisReply *)redisCommand(context_, "HGETALL %s", key.c_str());
        if (!check_reply(reply))
        {
                return result;
        }

        if (reply->type == REDIS_REPLY_ARRAY)
        {
                for (size_t i = 0; i < reply->elements; i += 2)
                {
                        if (i + 1 < reply->elements)
                        {
                                std::string field(reply->element[i]->str, reply->element[i]->len);
                                std::string value(reply->element[i + 1]->str,
                                                  reply->element[i + 1]->len);
                                result[field] = value;
                        }
                }
        }

        free_reply(reply);
        return result;
}

bool RedisClient::store_file_metadata(const std::string &filename,
                                      const std::map<std::string, std::string> &metadata)
{
        std::string key = "file:" + filename;

        for (const auto &pair : metadata)
        {
                if (!hset(key, pair.first, pair.second))
                {
                        return false;
                }
        }

        // Add to files set
        sadd("files", filename);
        return true;
}

std::map<std::string, std::string> RedisClient::get_file_metadata(const std::string &filename)
{
        std::string key = "file:" + filename;
        return hgetall(key);
}

bool RedisClient::delete_file_metadata(const std::string &filename)
{
        std::string key = "file:" + filename;
        bool success    = del(key);

        // Remove from files set
        srem("files", filename);
        return success;
}

bool RedisClient::store_chunk_metadata(const std::string &chunk_id,
                                       const std::vector<std::string> &servers)
{
        std::string key = "chunk:" + chunk_id;
        del(key); // Clear existing data

        for (const auto &server : servers)
        {
                if (!sadd(key, server))
                {
                        return false;
                }
        }

        return true;
}

std::vector<std::string> RedisClient::get_chunk_servers(const std::string &chunk_id)
{
        std::string key = "chunk:" + chunk_id;
        return smembers(key);
}

bool RedisClient::update_server_health(const std::string &server_id,
                                       const std::map<std::string, std::string> &health_data)
{
        std::string key = "server:" + server_id;

        for (const auto &pair : health_data)
        {
                if (!hset(key, pair.first, pair.second))
                {
                        return false;
                }
        }

        // Update timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        hset(key, "last_seen", std::to_string(timestamp));

        // Add to servers set
        sadd("servers", server_id);
        return true;
}

std::map<std::string, std::string> RedisClient::get_server_health(const std::string &server_id)
{
        std::string key = "server:" + server_id;
        return hgetall(key);
}

std::vector<std::string> RedisClient::get_all_servers() { return smembers("servers"); }

bool RedisClient::sadd(const std::string &key, const std::string &member)
{
        if (!connected_)
                return false;

        redisReply *reply =
            (redisReply *)redisCommand(context_, "SADD %s %s", key.c_str(), member.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_INTEGER);
        free_reply(reply);
        return success;
}

bool RedisClient::srem(const std::string &key, const std::string &member)
{
        if (!connected_)
                return false;

        redisReply *reply =
            (redisReply *)redisCommand(context_, "SREM %s %s", key.c_str(), member.c_str());
        if (!check_reply(reply))
        {
                return false;
        }

        bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        free_reply(reply);
        return success;
}

std::vector<std::string> RedisClient::smembers(const std::string &key)
{
        std::vector<std::string> result;
        if (!connected_)
                return result;

        redisReply *reply = (redisReply *)redisCommand(context_, "SMEMBERS %s", key.c_str());
        if (!check_reply(reply))
        {
                return result;
        }

        if (reply->type == REDIS_REPLY_ARRAY)
        {
                for (size_t i = 0; i < reply->elements; i++)
                {
                        result.push_back(
                            std::string(reply->element[i]->str, reply->element[i]->len));
                }
        }

        free_reply(reply);
        return result;
}

bool RedisClient::check_reply(redisReply *reply)
{
        if (!reply)
        {
                std::cerr << "Redis command failed: null reply" << std::endl;
                return false;
        }

        if (reply->type == REDIS_REPLY_ERROR)
        {
                std::cerr << "Redis error: " << reply->str << std::endl;
                free_reply(reply);
                return false;
        }

        return true;
}

void RedisClient::free_reply(redisReply *reply)
{
        if (reply)
        {
                freeReplyObject(reply);
        }
}

int RedisClient::replicatino(redisReply *reply) { return 0; }
