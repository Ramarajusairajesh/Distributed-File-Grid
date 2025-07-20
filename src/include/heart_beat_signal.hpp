#pragma once
#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../../include/heart_beat.pb.h"

struct system_resources
{
        uint8_t cpu;
        uint8_t disk;
};

struct server_detail
{
        char *server_ip;
        int node_number;
        time_t timestamp;

        int status_code;
};

struct cluster_detail
{
        server_detail details;
        system_resources resources;
};

class Head_server
{

      private:
        int H_send_signal(int cluster_fd) { return 0; }

      public:
        void H_receive_signal() { return; }
};

class Cluster_server
{

      private:
      public:
        void C_heartbeat_response() {}
};

std::unordered_map<std::string, std::vector<char>> chunk_storage;

void handle_heartbeat(int client_socket) {
    Heartbeat hb;
    hb.set_cpu(10);
    hb.set_disk(80);
    hb.set_count(1);
    hb.set_status(Heartbeat::ALIVE);

    std::string out;
    hb.SerializeToString(&out);
    uint32_t size = out.size();
    send(client_socket, &size, sizeof(size), 0);
    send(client_socket, out.data(), size, 0);
}

void handle_store_chunk(int client_socket) {
    uint32_t size;
    read(client_socket, &size, sizeof(size));
    std::string in(size, 0);
    read(client_socket, &in[0], size);

    FileChunk chunk;
    chunk.ParseFromString(in);
    chunk_storage[chunk.chunk_id()] = std::vector<char>(chunk.data().begin(), chunk.data().end());
    std::cout << "Stored chunk: " << chunk.chunk_id() << std::endl;
}

void handle_get_chunk(int client_socket) {
    uint32_t size;
    read(client_socket, &size, sizeof(size));
    std::string in(size, 0);
    read(client_socket, &in[0], size);
    FileChunk req;
    req.ParseFromString(in);
    auto it = chunk_storage.find(req.chunk_id());
    FileChunk resp;
    resp.set_chunk_id(req.chunk_id());
    if (it != chunk_storage.end()) {
        resp.set_data(it->second.data(), it->second.size());
    }
    std::string out;
    resp.SerializeToString(&out);
    uint32_t out_size = out.size();
    send(client_socket, &out_size, sizeof(out_size), 0);
    send(client_socket, out.data(), out_size, 0);
}

int main(int argc, char* argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int port = 9001;
    if (argc > 1) port = std::stoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "Chunk Server: Running on port " << port << "...\n";

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        char cmd;
        read(client_socket, &cmd, 1);
        if (cmd == 'H') handle_heartbeat(client_socket);
        else if (cmd == 'S') handle_store_chunk(client_socket);
        else if (cmd == 'G') handle_get_chunk(client_socket);
        close(client_socket);
    }
    close(server_fd);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
