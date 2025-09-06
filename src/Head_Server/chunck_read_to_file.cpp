#include "../include/heart_beat_signal.hpp"
#include "./redis_handler.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

struct ChunkLocation {
    int chunk_id;
    std::string server_ip;
    std::string file_path;
};

class FileReconstructor {
private:
    std::vector<ChunkLocation> get_chunk_locations_from_redis(const std::string& filename) {
        std::vector<ChunkLocation> locations;
        
        try {
            // Use the read_entry function to get chunk information
            std::stringstream request;
            request << filename;
            
            // Capture output from read_entry
            std::streambuf* orig = std::cout.rdbuf();
            std::ostringstream captured;
            std::cout.rdbuf(captured.rdbuf());
            
            read_entry(request.str());
            
            std::cout.rdbuf(orig);
            std::string output = captured.str();
            
            // Parse the output to extract chunk locations
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("chunk:") != std::string::npos) {
                    // Parse line format: "chunk:X server=Y path=Z"
                    size_t chunk_pos = line.find("chunk:");
                    size_t server_pos = line.find("server=");
                    size_t path_pos = line.find("path=");
                    
                    if (chunk_pos != std::string::npos && server_pos != std::string::npos && path_pos != std::string::npos) {
                        ChunkLocation loc;
                        
                        // Extract chunk ID
                        std::string chunk_str = line.substr(chunk_pos + 6);
                        size_t space_pos = chunk_str.find(' ');
                        if (space_pos != std::string::npos) {
                            loc.chunk_id = std::stoi(chunk_str.substr(0, space_pos));
                        }
                        
                        // Extract server IP
                        std::string server_str = line.substr(server_pos + 7);
                        space_pos = server_str.find(' ');
                        if (space_pos != std::string::npos) {
                            loc.server_ip = server_str.substr(0, space_pos);
                        }
                        
                        // Extract file path
                        loc.file_path = line.substr(path_pos + 5);
                        
                        locations.push_back(loc);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting chunk locations: " << e.what() << std::endl;
        }
        
        return locations;
    }
    
    std::vector<char> read_chunk_from_server(const ChunkLocation& location) {
        std::vector<char> chunk_data;
        
        try {
            std::ifstream file(location.file_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to read chunk from: " << location.file_path << std::endl;
                return chunk_data;
            }
            
            // Get file size
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            chunk_data.resize(file_size);
            file.read(chunk_data.data(), file_size);
            file.close();
            
            std::cout << "Read chunk " << location.chunk_id << " (" << file_size << " bytes) from " << location.server_ip << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error reading chunk: " << e.what() << std::endl;
        }
        
        return chunk_data;
    }

public:
    bool reconstruct_file(const std::string& filename, const std::string& output_path) {
        std::cout << "Reconstructing file: " << filename << std::endl;
        
        // Get chunk locations from Redis
        auto chunk_locations = get_chunk_locations_from_redis(filename);
        if (chunk_locations.empty()) {
            std::cerr << "No chunks found for file: " << filename << std::endl;
            return false;
        }
        
        // Group chunks by chunk_id and select one location per chunk (for redundancy)
        std::map<int, ChunkLocation> unique_chunks;
        for (const auto& location : chunk_locations) {
            if (unique_chunks.find(location.chunk_id) == unique_chunks.end()) {
                unique_chunks[location.chunk_id] = location;
            }
        }
        
        std::cout << "Found " << unique_chunks.size() << " unique chunks to reconstruct" << std::endl;
        
        // Create output file
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file) {
            std::cerr << "Failed to create output file: " << output_path << std::endl;
            return false;
        }
        
        // Read and write chunks in order
        for (const auto& [chunk_id, location] : unique_chunks) {
            auto chunk_data = read_chunk_from_server(location);
            if (chunk_data.empty()) {
                std::cerr << "Failed to read chunk " << chunk_id << std::endl;
                output_file.close();
                fs::remove(output_path);
                return false;
            }
            
            output_file.write(chunk_data.data(), chunk_data.size());
        }
        
        output_file.close();
        std::cout << "File reconstructed successfully: " << output_path << std::endl;
        return true;
    }
    
    bool file_exists(const std::string& filename) {
        auto chunk_locations = get_chunk_locations_from_redis(filename);
        return !chunk_locations.empty();
    }
};

// Global file reconstructor instance
static FileReconstructor g_file_reconstructor;

extern "C" {
    int process_file_download(const char* filename, const char* output_path) {
        try {
            if (g_file_reconstructor.reconstruct_file(filename, output_path)) {
                return 0;
            }
            return -1;
        } catch (const std::exception& e) {
            std::cerr << "Error processing file download: " << e.what() << std::endl;
            return -1;
        }
    }
    
    int check_file_exists(const char* filename) {
        try {
            return g_file_reconstructor.file_exists(filename) ? 1 : 0;
        } catch (const std::exception& e) {
            std::cerr << "Error checking file existence: " << e.what() << std::endl;
            return -1;
        }
    }
}
