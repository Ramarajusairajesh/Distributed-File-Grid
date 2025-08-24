#ifndef SYSTEM_RESOURCES_HPP
#define SYSTEM_RESOURCES_HPP

#include <string>

class SystemResources {
private:
    struct CpuStats {
        long user, nice, system, idle, iowait, irq, softirq, steal;
        long total() const;
        long idle_time() const;
    };

    CpuStats prev_cpu_stats_;
    bool first_cpu_read_;

public:
    SystemResources();
    
    double get_cpu_usage();
    double get_memory_usage();
    double get_disk_usage(const std::string& path = "/");
    uint64_t get_available_disk_space(const std::string& path = "/");
    uint64_t get_total_disk_space(const std::string& path = "/");
    uint64_t get_total_memory();
    uint64_t get_free_memory();
    int get_cpu_count();
    double get_load_average();
    std::string get_system_info();
    std::string get_hostname();
    void print_system_status();
    bool is_system_healthy();
    std::string get_health_status();

private:
    void read_cpu_stats(CpuStats& stats);
};

#endif // SYSTEM_RESOURCES_HPP 