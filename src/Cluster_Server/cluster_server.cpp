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
      std::cout << "  -p, --port     Set the port number\n";
      return 0;
    }
    if (arg == "-v" || arg == "-V" || arg == "--version") {
      std::cout << "Current version: " << APP_VERSION << std::endl;
      return 0;
    }
    async_hb::send_signal("127.0.0.1", 20);
  }
  return 0;
}
