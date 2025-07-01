#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linux/sockios.h>
#include <net/if.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

// Structure to hold CPU usage data
struct CpuUsage
{
        uint64_t user, nice, system, idle, iowait, irq, softirq;
};

// Structure to hold network stats
struct NetStats
{
        uint64_t rx_bytes;
        uint64_t tx_bytes;
};

// Function to read CPU usage from /proc/stat
CpuUsage read_cpu_usage()
{
        std::ifstream stat_file("/proc/stat");
        std::string line;
        CpuUsage usage = {0};

        if (std::getline(stat_file, line))
        {
                std::istringstream iss(line);
                std::string cpu;
                iss >> cpu >> usage.user >> usage.nice >> usage.system >> usage.idle >>
                    usage.iowait >> usage.irq >> usage.softirq;
        }
        return usage;
}

// Calculate CPU usage percentage
double calculate_cpu_usage()
{
        CpuUsage prev = read_cpu_usage();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        CpuUsage curr = read_cpu_usage();

        uint64_t prev_total =
            prev.user + prev.nice + prev.system + prev.idle + prev.iowait + prev.irq + prev.softirq;
        uint64_t curr_total =
            curr.user + curr.nice + curr.system + curr.idle + curr.iowait + curr.irq + curr.softirq;

        uint64_t prev_idle = prev.idle + prev.iowait;
        uint64_t curr_idle = curr.idle + curr.iowait;

        uint64_t total_diff = curr_total - prev_total;
        uint64_t idle_diff  = curr_idle - prev_idle;

        return total_diff > 0 ? (100.0 * (total_diff - idle_diff)) / total_diff : 0.0;
}

// Get disk usage percentage
double get_disk_usage()
{
        struct statvfs stat;
        if (statvfs("/", &stat) != 0)
        {
                return 0.0;
        }
        uint64_t total = stat.f_blocks * stat.f_frsize;
        uint64_t free  = stat.f_bfree * stat.f_frsize;
        return 100.0 * (total - free) / total;
}

// Get RAM usage percentage
double get_ram_usage()
{
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        uint64_t total = 0, free = 0, buffers = 0, cached = 0;

        while (std::getline(meminfo, line))
        {
                std::istringstream iss(line);
                std::string key;
                uint64_t value;
                iss >> key >> value;

                if (key == "MemTotal:")
                        total = value * 1024; // Convert KB to bytes
                else if (key == "MemFree:")
                        free = value * 1024;
                else if (key == "Buffers:")
                        buffers = value * 1024;
                else if (key == "Cached:")
                        cached = value * 1024;
        }

        if (total == 0)
                return 0.0;
        uint64_t used = total - (free + buffers + cached);
        return 100.0 * used / total;
}

// Convert bytes to human-readable format
std::string bytes_to_human_readable(uint64_t bytes)
{
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index      = 0;
        double size         = static_cast<double>(bytes);

        while (size >= 1024 && unit_index < 4)
        {
                size /= 1024;
                unit_index++;
        }

        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
        return ss.str();
}

// Get network bandwidth
NetStats get_network_stats(const std::string &interface = "enp4s0")
{
        NetStats stats = {0, 0};
        std::ifstream net_file("/proc/net/dev");
        std::string line;

        while (std::getline(net_file, line))
        {
                if (line.find(interface) != std::string::npos)
                {
                        std::istringstream iss(line);
                        std::string iface;
                        iss >> iface; // Skip interface name
                        iss >> stats.rx_bytes >> std::ws;
                        for (int i = 0; i < 7; ++i)
                                iss >> std::ws; // Skip other fields
                        iss >> stats.tx_bytes;
                        break;
                }
        }
        return stats;
}

int main()
{
        // Get CPU usage
        double cpu_usage = calculate_cpu_usage();
        std::cout << "CPU Usage: " << std::fixed << std::setprecision(2) << cpu_usage << "%"
                  << std::endl;

        // Get disk usage
        double disk_usage = get_disk_usage();
        std::cout << "Disk Usage: " << std::fixed << std::setprecision(2) << disk_usage << "%"
                  << std::endl;

        // Get RAM usage
        double ram_usage = get_ram_usage();
        std::cout << "RAM Usage: " << std::fixed << std::setprecision(2) << ram_usage << "%"
                  << std::endl;

        // Get network bandwidth
        NetStats prev_stats = get_network_stats();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        NetStats curr_stats = get_network_stats();

        uint64_t rx_diff = curr_stats.rx_bytes - prev_stats.rx_bytes;
        uint64_t tx_diff = curr_stats.tx_bytes - prev_stats.tx_bytes;

        std::cout << "Network Received: " << bytes_to_human_readable(rx_diff) << "/s" << std::endl;
        std::cout << "Network Transmitted: " << bytes_to_human_readable(tx_diff) << "/s"
                  << std::endl;

        return 0;
}
