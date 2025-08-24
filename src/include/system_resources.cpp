#include "system_resources.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <thread>
#include <chrono>

// Implementation of SystemResources class methods

long SystemResources::CpuStats::total() const { 
    return user + nice + system + idle + iowait + irq + softirq + steal; 
}

long SystemResources::CpuStats::idle_time() const { 
    return idle + iowait; 
}

SystemResources::SystemResources() : first_cpu_read_(true) {
    // Initialize CPU stats
    read_cpu_stats(prev_cpu_stats_);
}

double SystemResources::get_cpu_usage() {
    CpuStats current_stats;
    read_cpu_stats(current_stats);

    if (first_cpu_read_) {
        prev_cpu_stats_ = current_stats;
        first_cpu_read_ = false;
        return 0.0;
    }

    long total_diff = current_stats.total() - prev_cpu_stats_.total();
    long idle_diff = current_stats.idle_time() - prev_cpu_stats_.idle_time();

    if (total_diff == 0) return 0.0;

    double cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
    prev_cpu_stats_ = current_stats;

    return cpu_usage;
}

double SystemResources::get_memory_usage() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        std::cerr << "Failed to get system info" << std::endl;
        return 0.0;
    }

    uint64_t total_memory = si.totalram * si.mem_unit;
    uint64_t free_memory = si.freeram * si.mem_unit;
    uint64_t used_memory = total_memory - free_memory;

    return (double)used_memory / total_memory * 100.0;
}

double SystemResources::get_disk_usage(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        std::cerr << "Failed to get disk stats for " << path << std::endl;
        return 0.0;
    }

    uint64_t total_blocks = stat.f_blocks;
    uint64_t free_blocks = stat.f_bavail;
    uint64_t used_blocks = total_blocks - free_blocks;

    return (double)used_blocks / total_blocks * 100.0;
}

uint64_t SystemResources::get_available_disk_space(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        std::cerr << "Failed to get disk stats for " << path << std::endl;
        return 0;
    }

    return stat.f_bavail * stat.f_frsize;
}

uint64_t SystemResources::get_total_disk_space(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        std::cerr << "Failed to get disk stats for " << path << std::endl;
        return 0;
    }

    return stat.f_blocks * stat.f_frsize;
}

uint64_t SystemResources::get_total_memory() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        std::cerr << "Failed to get system info" << std::endl;
        return 0;
    }

    return si.totalram * si.mem_unit;
}

uint64_t SystemResources::get_free_memory() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        std::cerr << "Failed to get system info" << std::endl;
        return 0;
    }

    return si.freeram * si.mem_unit;
}

int SystemResources::get_cpu_count() {
    std::ifstream file("/proc/cpuinfo");
    std::string line;
    int count = 0;

    while (std::getline(file, line)) {
        if (line.substr(0, 9) == "processor") {
            count++;
        }
    }

    return count;
}

double SystemResources::get_load_average() {
    double loadavg[3];
    if (getloadavg(loadavg, 3) == -1) {
        std::cerr << "Failed to get load average" << std::endl;
        return 0.0;
    }

    return loadavg[0]; // Return 1-minute load average
}

std::string SystemResources::get_system_info() {
    struct utsname uts;
    if (uname(&uts) != 0) {
        return "Unknown";
    }

    std::ostringstream oss;
    oss << uts.sysname << " " << uts.release << " " << uts.machine;
    return oss.str();
}

std::string SystemResources::get_hostname() {
    struct utsname uts;
    if (uname(&uts) != 0) {
        return "Unknown";
    }

    return uts.nodename;
}

void SystemResources::print_system_status() {
    std::cout << "System Status:" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Hostname: " << get_hostname() << std::endl;
    std::cout << "System: " << get_system_info() << std::endl;
    std::cout << "CPU Usage: " << get_cpu_usage() << "%" << std::endl;
    std::cout << "Memory Usage: " << get_memory_usage() << "%" << std::endl;
    std::cout << "Disk Usage: " << get_disk_usage() << "%" << std::endl;
    std::cout << "Load Average: " << get_load_average() << std::endl;
    std::cout << "CPU Count: " << get_cpu_count() << std::endl;
    std::cout << "Total Memory: " << (get_total_memory() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "Free Memory: " << (get_free_memory() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "Total Disk: " << (get_total_disk_space() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "Available Disk: " << (get_available_disk_space() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "=============" << std::endl;
}

bool SystemResources::is_system_healthy() {
    double cpu_usage = get_cpu_usage();
    double memory_usage = get_memory_usage();
    double disk_usage = get_disk_usage();

    // System is healthy if:
    // - CPU usage < 90%
    // - Memory usage < 90%
    // - Disk usage < 95%
    return cpu_usage < 90.0 && memory_usage < 90.0 && disk_usage < 95.0;
}

std::string SystemResources::get_health_status() {
    if (is_system_healthy()) {
        return "HEALTHY";
    } else {
        return "WARNING";
    }
}

void SystemResources::read_cpu_stats(CpuStats& stats) {
    std::ifstream file("/proc/stat");
    std::string line;

    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu;
        iss >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle 
            >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
    }
} 