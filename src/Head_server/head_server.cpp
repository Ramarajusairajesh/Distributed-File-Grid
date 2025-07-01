#include "../include/heart_beat_signal.hpp"
#include "hiredis/hiredis.h"
#include <chrono>
#include <ctime>
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#define log_path "/var/log/head_server/server.logs"
pid_t redis_pid = -1;

int restore() { return 0; }
int create_log_file(std::string &file_name)
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

int runRedisServerInBackground()
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

int server_initialization()
{

        // Creating the redis daemon
        int redis_status = runRedisServerInBackground();
        if (redis_status == 1)
        {
                perror("Error while running redis");
                return 1;
        }
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

        return 0;
}

int main()
{
        int status = server_initialization();
        if (status == false)
        {
                std::cerr << "Error while initialization the head server" << std::endl;
        }
        return 0;
}
