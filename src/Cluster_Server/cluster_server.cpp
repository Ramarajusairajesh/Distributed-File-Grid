#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <openssl/md5.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include "heart_beat.hpp"
#include "file_chunks.hpp"
#include "machine_details.hpp"

using boost::asio::ip::tcp;

class ClusterServer {
private:
    boost::asio::io_context io_context_;
    tcp::acceptor acceptor_;
    std::string server_id_;
    std::string storage_path_;
    std::map<std::string, std::string> chunk_locations_; // chunk_id -> file_path
    std::map<std::string, std::vector<std::string>> replication_groups_; // chunk_id -> replica_servers
    int replication_factor_;
    bool running_;
    std::thread heartbeat_thread_;
    
    // Health monitoring
    struct SystemResources {
        double cpu_usage;
        double memory_usage;
        double disk_usage;
        uint64_t available_storage;
        uint64_t total_storage;
    };

public:
    ClusterServer(const std::string& server_id, const std::string& storage_path, 
                  short port, int replication_factor = 3)
        : server_id_(server_id), storage_path_(storage_path), 
          acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
          replication_factor_(replication_factor), running_(false) {
        
        // Create storage directory if it doesn't exist
        std::filesystem::create_directories(storage_path_);
        
        std::cout << "Cluster server " << server_id_ << " initialized on port " << port << std::endl;
        std::cout << "Storage path: " << storage_path_ << std::endl;
    }

    ~ClusterServer() {
        stop();
    }

    void start() {
        running_ = true;
        
        // Start heartbeat thread
        heartbeat_thread_ = std::thread(&ClusterServer::heartbeat_loop, this);
        
        // Start accepting connections
        start_accept();
        
        std::cout << "Cluster server " << server_id_ << " started" << std::endl;
        
        // Run the io_context
        io_context_.run();
    }

    void stop() {
        running_ = false;
        
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
        
        io_context_.stop();
        std::cout << "Cluster server " << server_id_ << " stopped" << std::endl;
    }

private:
    void start_accept() {
        auto session = std::make_shared<Session>(io_context_, this);
        
        acceptor_.async_accept(session->socket(),
            [this, session](const boost::system::error_code& error) {
                if (!error) {
                    session->start();
                }
                start_accept();
            });
    }

    void heartbeat_loop() {
        while (running_) {
            send_heartbeat();
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

    void send_heartbeat() {
        try {
            // Create heartbeat message
            heartbeat::Heartbeat hb;
            hb.set_server_id(server_id_);
            hb.set_server_type("chunk_server");
            
            SystemResources resources = get_system_resources();
            hb.set_cpu_usage(static_cast<int32_t>(resources.cpu_usage));
            hb.set_memory_usage(static_cast<int32_t>(resources.memory_usage));
            hb.set_disk_usage(static_cast<int32_t>(resources.disk_usage));
            hb.set_active_connections(0); // TODO: track active connections
            hb.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            if (resources.cpu_usage > 80 || resources.memory_usage > 80 || resources.disk_usage > 90) {
                hb.set_status(heartbeat::Heartbeat_Status_WARNING);
            } else {
                hb.set_status(heartbeat::Heartbeat_Status_ALIVE);
            }
            
            // TODO: Send heartbeat to head server
            std::cout << "Heartbeat sent: CPU=" << resources.cpu_usage 
                      << "%, Memory=" << resources.memory_usage 
                      << "%, Disk=" << resources.disk_usage << "%" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error sending heartbeat: " << e.what() << std::endl;
        }
    }

    SystemResources get_system_resources() {
        SystemResources resources;
        
        // Get disk usage
        struct statvfs stat;
        if (statvfs(storage_path_.c_str(), &stat) == 0) {
            uint64_t total = stat.f_blocks * stat.f_frsize;
            uint64_t available = stat.f_bavail * stat.f_frsize;
            resources.total_storage = total;
            resources.available_storage = available;
            resources.disk_usage = ((double)(total - available) / total) * 100.0;
        }
        
        // Get memory usage
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            uint64_t total_mem = si.totalram * si.mem_unit;
            uint64_t free_mem = si.freeram * si.mem_unit;
            resources.memory_usage = ((double)(total_mem - free_mem) / total_mem) * 100.0;
        }
        
        // Simple CPU usage estimation (in a real implementation, you'd use /proc/stat)
        resources.cpu_usage = 0.0; // TODO: implement proper CPU monitoring
        
        return resources;
    }

    bool store_chunk(const std::string& chunk_id, const std::vector<uint8_t>& data) {
        try {
            std::string chunk_path = storage_path_ + "/" + chunk_id;
            std::ofstream file(chunk_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to create chunk file: " << chunk_path << std::endl;
                return false;
            }
            
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.close();
            
            chunk_locations_[chunk_id] = chunk_path;
            
            // Calculate and store hash
            std::string hash = calculate_md5_hash(data);
            
            std::cout << "Stored chunk " << chunk_id << " (" << data.size() 
                      << " bytes) with hash " << hash << std::endl;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error storing chunk " << chunk_id << ": " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<uint8_t> retrieve_chunk(const std::string& chunk_id) {
        std::vector<uint8_t> data;
        
        auto it = chunk_locations_.find(chunk_id);
        if (it == chunk_locations_.end()) {
            std::cerr << "Chunk " << chunk_id << " not found" << std::endl;
            return data;
        }
        
        std::ifstream file(it->second, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open chunk file: " << it->second << std::endl;
            return data;
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        std::cout << "Retrieved chunk " << chunk_id << " (" << size << " bytes)" << std::endl;
        
        return data;
    }

    bool delete_chunk(const std::string& chunk_id) {
        auto it = chunk_locations_.find(chunk_id);
        if (it == chunk_locations_.end()) {
            std::cerr << "Chunk " << chunk_id << " not found for deletion" << std::endl;
            return false;
        }
        
        if (std::remove(it->second.c_str()) == 0) {
            chunk_locations_.erase(it);
            std::cout << "Deleted chunk " << chunk_id << std::endl;
            return true;
        } else {
            std::cerr << "Failed to delete chunk file: " << it->second << std::endl;
            return false;
        }
    }

    std::string calculate_md5_hash(const std::vector<uint8_t>& data) {
        unsigned char hash[MD5_DIGEST_LENGTH];
        MD5_CTX md5;
        MD5_Init(&md5);
        MD5_Update(&md5, data.data(), data.size());
        MD5_Final(hash, &md5);
        
        std::stringstream ss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    // Session class for handling client connections
    class Session : public std::enable_shared_from_this<Session> {
    private:
        tcp::socket socket_;
        ClusterServer* server_;
        enum { max_length = 1024 * 1024 }; // 1MB buffer
        char data_[max_length];

    public:
        Session(boost::asio::io_context& io_context, ClusterServer* server)
            : socket_(io_context), server_(server) {}

        tcp::socket& socket() { return socket_; }

        void start() {
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                boost::bind(&Session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }

    private:
        void handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
            if (!error) {
                // Parse the received data and handle the request
                handle_request(std::string(data_, bytes_transferred));
            }
        }

        void handle_request(const std::string& request_data) {
            // TODO: Parse protobuf message and handle different request types
            // For now, just echo back
            boost::asio::async_write(socket_,
                boost::asio::buffer(request_data),
                boost::bind(&Session::handle_write, this,
                    boost::asio::placeholders::error));
        }

        void handle_write(const boost::system::error_code& error) {
            if (!error) {
                socket_.async_read_some(
                    boost::asio::buffer(data_, max_length),
                    boost::bind(&Session::handle_read, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
            }
        }
    };
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_id> <storage_path> <port>" << std::endl;
        return 1;
    }

    std::string server_id = argv[1];
    std::string storage_path = argv[2];
    short port = static_cast<short>(std::atoi(argv[3]));

    try {
        ClusterServer server(server_id, storage_path, port);
        server.start();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
