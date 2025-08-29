#pragma once
#include "../include/redis_client.hpp"
#include <chrono>
#include <ctime>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#define log_path "/var/log/head_server/server.logs"

// Types of requst:
//
//	 Get file
//	 Request File Deletion
//	 Health check
//	 Requst replication
//	 Adding a new cluster node / server
//	 Replace the zookeeper cluster with this server
//

enum request_type
{
        Get_file,
        Request_file_deletion,
        Health_check,
        Request_replication,
        New_cluster_server,
        Replace_zookeper,
        // TODO: Add more reuqest types in future

};

struct request
{
        request_type type;
        std::string user;
        time_t current_time;

        // TODO: Create a request
};

inline int runRedisServerInBackground()
{ // started the redis background server
        pid_t pid = fork();

        if (pid < 0)
        {
                perror("fork failed in runRedisServerInBackground");
                return -1;
        }
        else if (pid == 0)
        {
                if (setsid() < 0)
                {
                        perror("setsid failed in child");
                        _exit(1);
                }
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
                execlp("redis-server", "redis-server", NULL);
                perror("execlp for redis-server failed in child");
                _exit(1);
        }
        else
        {
                std::cout << "Parent process: Redis server child initiated with PID " << pid
                          << " in background." << std::endl;
                return 0;
        }
}

inline int create_log_file(std::string &file_name)
{
        char buffer[80];
        // Add the data and time for the logs file name
        auto now                 = std::chrono::system_clock::now();
        std::time_t current_time = std::chrono::system_clock::to_time_t(now);
        std::tm *time_stamp      = std::localtime(&current_time);
        if (time_stamp == nullptr)
        {
                std::cerr << "Failed to get local time" << std::endl;
        }
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", time_stamp);

        file_name = log_path + (std::string)buffer;
        std::ofstream outputFile(file_name);
        std::cout << "Log file path" << file_name << std::endl;

        return 0;
}

void write_logs(std::string &request, std::ofstream &outFile) {}
int clone_redis_server(std::string phs_ip_address, int port)
{
        // use redis replication consider the primary head server as master and the others as slave

        return 0;
}
