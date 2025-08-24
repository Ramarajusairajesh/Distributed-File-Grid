#ifndef MACHINE_DETAILS_HPP
#define MACHINE_DETAILS_HPP

#include <string>
#include <map>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <fstream>
#include <sstream>
#include <iostream>

struct MachineInfo {
    std::string hostname;
    std::string os_name;
    std::string os_version;
    std::string architecture;
    uint64_t total_memory;
    uint64_t free_memory;
    uint64_t total_disk;
    uint64_t free_disk;
    int cpu_count;
    double cpu_usage;
    double memory_usage;
    double disk_usage;
};

class MachineDetails {
public:
    static MachineInfo get_machine_info() {
        MachineInfo info;
        
        // Get system information
        struct utsname uts;
        if (uname(&uts) == 0) {
            info.hostname = uts.nodename;
            info.os_name = uts.sysname;
            info.os_version = uts.release;
            info.architecture = uts.machine;
        }
        
        // Get memory information
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            info.total_memory = si.totalram * si.mem_unit;
            info.free_memory = si.freeram * si.mem_unit;
            info.memory_usage = ((double)(info.total_memory - info.free_memory) / info.total_memory) * 100.0;
        }
        
        // Get disk information (for root filesystem)
        struct statvfs stat;
        if (statvfs("/", &stat) == 0) {
            info.total_disk = stat.f_blocks * stat.f_frsize;
            info.free_disk = stat.f_bavail * stat.f_frsize;
            info.disk_usage = ((double)(info.total_disk - info.free_disk) / info.total_disk) * 100.0;
        }
        
        // Get CPU information
        info.cpu_count = get_cpu_count();
        info.cpu_usage = get_cpu_usage();
        
        return info;
    }
    
    static int get_cpu_count() {
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
    
    static double get_cpu_usage() {
        // Simple CPU usage calculation using /proc/stat
        static long prev_idle = 0, prev_total = 0;
        
        std::ifstream file("/proc/stat");
        std::string line;
        
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string cpu;
            long user, nice, system, idle, iowait, irq, softirq, steal;
            
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            long total = user + nice + system + idle + iowait + irq + softirq + steal;
            long idle_time = idle + iowait;
            
            if (prev_total > 0) {
                long total_diff = total - prev_total;
                long idle_diff = idle_time - prev_idle;
                double usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
                
                prev_idle = idle_time;
                prev_total = total;
                
                return usage;
            }
            
            prev_idle = idle_time;
            prev_total = total;
        }
        
        return 0.0;
    }
    
    static std::map<std::string, std::string> get_environment_variables() {
        std::map<std::string, std::string> env_vars;
        
        for (char** env = environ; *env != nullptr; env++) {
            std::string env_str(*env);
            size_t pos = env_str.find('=');
            if (pos != std::string::npos) {
                std::string key = env_str.substr(0, pos);
                std::string value = env_str.substr(pos + 1);
                env_vars[key] = value;
            }
        }
        
        return env_vars;
    }
    
    static std::string get_machine_id() {
        std::ifstream file("/etc/machine-id");
        std::string machine_id;
        
        if (std::getline(file, machine_id)) {
            return machine_id;
        }
        
        // Fallback to hostname if machine-id is not available
        return get_machine_info().hostname;
    }
    
    static void print_machine_info() {
        MachineInfo info = get_machine_info();
        
        std::cout << "Machine Information:" << std::endl;
        std::cout << "===================" << std::endl;
        std::cout << "Hostname: " << info.hostname << std::endl;
        std::cout << "OS: " << info.os_name << " " << info.os_version << std::endl;
        std::cout << "Architecture: " << info.architecture << std::endl;
        std::cout << "CPU Count: " << info.cpu_count << std::endl;
        std::cout << "CPU Usage: " << info.cpu_usage << "%" << std::endl;
        std::cout << "Memory: " << (info.total_memory / (1024*1024*1024)) << " GB total, "
                  << (info.free_memory / (1024*1024*1024)) << " GB free (" 
                  << info.memory_usage << "% used)" << std::endl;
        std::cout << "Disk: " << (info.total_disk / (1024*1024*1024)) << " GB total, "
                  << (info.free_disk / (1024*1024*1024)) << " GB free (" 
                  << info.disk_usage << "% used)" << std::endl;
        std::cout << "===================" << std::endl;
    }
};

#endif // MACHINE_DETAILS_HPP
