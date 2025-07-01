#pragma once
#include <cstdint>
#include <iostream>
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
