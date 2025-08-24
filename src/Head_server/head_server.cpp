#include "head_server.hpp"
#include "../include/heart_beat.hpp"
#include "../include/request_server.hpp"

// pid_t redis_pid = -1;

int server_initialization()
{

        // Creating redis daemon
        int redis_status = runRedisServerInBackground();
        if (redis_status == 1)
        {
                perror("Error while running redis");
                return 1;
        }
        // creating log file
        std::string file_name;
        create_log_file(file_name);
        std::ofstream log_file;
        log_file.open(file_name, std::ios::app);
        if (log_file.is_open() == false)
        {
                std::cerr << "Error while creating/ writing log file at " << file_name << std::endl;
                std::cerr << "Log are disable for this instance" << std::endl;
        }

        // start a multi threaded async for file recieving server
        int server_status = start_socket();
        if (server_status == 1)
        {
                std::cerr << "Error while creating the socket";
                return 1;
        }
        return 0;
}

int attach_cluster_servers(std::string cluster_ips[], int cluster_port, int machine_count)
{
        // Attach cluster server to head server
        // Check if the given server is a cluster server or not

        int cluster_fd;
        cluster_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (cluster_fd == -1)
        {
                perror("Socket creation failed");
                return 1;
        }
        struct sockaddr_in cluster_addr;
        memset(&cluster_addr, 0, sizeof(cluster_addr));
        cluster_addr.sin_family      = AF_INET;
        cluster_addr.sin_addr.s_addr = INADDR_ANY;
        cluster_addr.sin_port        = htons(cluster_port);
        for (int i = 0; i < machine_count; i++)
        {

                inet_pton(AF_INET, cluster_ips[i].c_str(), &cluster_addr.sin_addr);
                if (connect(cluster_fd, (struct sockaddr *)&cluster_addr, sizeof(cluster_addr)) < 0)
                {
                        std::cerr << "Error connecting to cluster server with IP :"
                                  << cluster_ips[i] << "on port:" << cluster_port << std::endl;
                        return 1;
                }
                // cluster server present and alive on port
                // TODO: Implement a custom message so that we can be sure that the machine is a
                // cluster server rather rather than random service running onn that machine port
                close(cluster_fd);
        }
        return 0;
}

int create_clone_primary(std::string &ip_addresses, int port)
{
        std::string primary_head_ip;
        std::cout << "Primary head server ip address " << std::endl;
        std::cin >> primary_head_ip;
        if (primary_head_ip.empty())
        {
                std::cout << "Please Enter primary head server ip address" << std::endl;
        }
        // create new replica for redis
        // TODO:  For redis use REPLICAOF to create live copy of one to another

        return 0;
}

int main(int argc, char **argv)
{
        char value;
        std::cout << "Is this the (P)rimary head server or (S)econdary head server" << std::endl;
        std::cin >> value;
        if (value == 'P' || value == 'p')
        {
                int status = server_initialization();
                if (status == 1)
                {
                        std::cerr << "Error while initialization the head server" << std::endl;
                }

                int client_cout;
        }

        else
        { // clone the primary server into this
          // create_clone_primary();
        }
        return 0;
}
