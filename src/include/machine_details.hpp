#include <iostream>

struct Header_machine
{
        int signal;
        int frequency;
        std::string server_ip;
};

struct Cluster_machine
{
        int signal;
        std::string header_address;
};

struct chunk_structure
{
        std::string hash_value;
        std::FILE &file;
        ssize_t size;
};
