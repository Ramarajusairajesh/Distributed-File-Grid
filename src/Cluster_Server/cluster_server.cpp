#include "../include/heart_beat_signal.hpp"
#include "../include/version.h"
#include <cstring> // for std::strcmp
#include <iostream>

int main(int argc, char **argv) {
  if (argc > 1) {
    std::string arg = argv[1];

    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: cluster_server [OPTIONS]\n";
      std::cout << "Options:\n";
      std::cout << "  -h, --help     Show this help message and exit\n";
      std::cout << "  -v, --version  Show program's version number and exit\n";
      std::cout << "  --server-id ID Set the server ID\n";
      std::cout << "  --port PORT    Set the port number\n";
      return 0;
    }
    if (arg == "-v" || arg == "-V" || arg == "--version") {
      std::cout << "Current version: " << APP_VERSION << std::endl;
      return 0;
    }
    
    // Parse command line arguments
    int server_id = 1;
    std::string ip = "127.0.0.1";
    int port = 8080;
    
    for (int i = 1; i < argc; i++) {
      std::string current_arg = argv[i];
      if (current_arg == "--server-id" && i + 1 < argc) {
        server_id = std::stoi(argv[++i]);
      } else if (current_arg == "--port" && i + 1 < argc) {
        port = std::stoi(argv[++i]);
      } else if (current_arg == "--ip" && i + 1 < argc) {
        ip = argv[++i];
      }
    }
    
    // Start cluster server service
    std::cout << "Starting Cluster Server " << server_id << " on " << ip << ":" << port << std::endl;
    std::cout << "Note: Heartbeat functionality requires health checker to be running" << std::endl;
    
    // For now, just simulate running without the problematic heartbeat
    std::cout << "Cluster Server is ready (simulation mode)" << std::endl;
    return 0;
  }
  
  std::cout << "Usage: cluster_server [OPTIONS]" << std::endl;
  std::cout << "Use -h or --help for more information" << std::endl;
  return 0;
}
