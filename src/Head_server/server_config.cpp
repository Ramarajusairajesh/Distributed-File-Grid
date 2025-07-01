#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "heart_beat.pb.h"
#include <map>
#include <set>

void send_heartbeat_request(const std::string& server_ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9001);
    inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    char cmd = 'H';
    send(sock, &cmd, 1, 0);

    uint32_t size;
    read(sock, &size, sizeof(size));
    std::string in(size, 0);
    read(sock, &in[0], size);

    Heartbeat hb;
    hb.ParseFromString(in);
    std::cout << "Head Server: Heartbeat from " << hb.server_id()
              << " (storage: " << hb.storage_used() << "/" << hb.storage_total() << ")\n";
    close(sock);
}

void send_chunk(const std::string& server_ip, const std::string& chunk_id, const std::vector<char>& data) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9001);
    inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    char cmd = 'S';
    send(sock, &cmd, 1, 0);

    FileChunk chunk;
    chunk.set_chunk_id(chunk_id);
    chunk.set_data(data.data(), data.size());
    std::string out;
    chunk.SerializeToString(&out);
    uint32_t size = out.size();
    send(sock, &size, sizeof(size), 0);
    send(sock, out.data(), size, 0);
    close(sock);
}

struct ChunkLocation {
    std::string chunk_id;
    std::vector<std::string> servers; // IPs
};

std::map<std::string, std::vector<ChunkLocation>> file_metadata; // filename -> list of chunks

std::vector<std::string> chunk_servers = {"127.0.0.1", "127.0.0.2", "127.0.0.3"}; // Example IPs
int replication_factor = 2;

void split_and_send_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<char> buffer(64 * 1024 * 1024); // 64MB
    int chunk_num = 0;
    std::vector<ChunkLocation> chunk_locs;
    while (file) {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;
        std::vector<char> chunk_data(buffer.begin(), buffer.begin() + bytes_read);
        std::string chunk_id = filename + "_chunk_" + std::to_string(chunk_num++);
        ChunkLocation loc;
        loc.chunk_id = chunk_id;
        std::set<int> used;
        for (int r = 0; r < replication_factor; ++r) {
            int idx;
            do { idx = rand() % chunk_servers.size(); } while (used.count(idx));
            used.insert(idx);
            send_chunk(chunk_servers[idx], chunk_id, chunk_data);
            loc.servers.push_back(chunk_servers[idx]);
        }
        chunk_locs.push_back(loc);
        std::cout << "Sent chunk: " << chunk_id << " to servers: ";
        for (auto& s : loc.servers) std::cout << s << " ";
        std::cout << std::endl;
    }
    file_metadata[filename] = chunk_locs;
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    // Example: send heartbeat and upload a file to one chunk server
    send_heartbeat_request("127.0.0.1");
    split_and_send_file("testfile.txt");
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
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
