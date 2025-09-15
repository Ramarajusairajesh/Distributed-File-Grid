#ifndef SYSTEM_INFO_HPP
#define SYSTEM_INFO_HPP

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <tuple>
#include <unistd.h>
#include <utility>  // for std::pair
struct system_usage {
  float cpu_usage;
  float total_ram;
  float ram_usage;
  int memory_usage;
  float disk_usage;
  std::basic_string<char> network_in;
  std::basic_string<char> network_out;
};

struct CpuStats {
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

  unsigned long long getTotalIdle() const { return idle + iowait; }

  unsigned long long getTotalNonIdle() const {
    return user + nice + system + irq + softirq + steal;
  }

  unsigned long long getTotal() const {
    return getTotalIdle() + getTotalNonIdle();
  }
};

// Helper function to format bandwidth with appropriate units
inline std::string formatBandwidth(unsigned long long bytesPerSecond) {
  const char *units[] = {"bps", "Kbps", "Mbps", "Gbps"};
  int unitIndex = 0;
  double speed = bytesPerSecond * 8.0; // Convert bytes to bits

  while (speed >= 1000.0 && unitIndex < 3) {
    speed /= 1000.0;
    unitIndex++;
  }

  std::stringstream ss;
  if (speed < 10.0) {
    ss << std::fixed << std::setprecision(2) << speed << " "
       << units[unitIndex];
  } else if (speed < 100.0) {
    ss << std::fixed << std::setprecision(1) << speed << " "
       << units[unitIndex];
  } else {
    ss << std::fixed << std::setprecision(0) << speed << " "
       << units[unitIndex];
  }
  return ss.str();
}

// Function 1: Returns CPU usage percentage
inline float getCpuUsagePercent() {
  auto readCpuStats = []() -> CpuStats {
    std::ifstream file("/proc/stat");
    std::string line, cpu;
    CpuStats stats{};
    if (file.is_open()) {
      getline(file, line);
      std::stringstream ss(line);
      ss >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle >>
          stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
      file.close();
    }
    return stats;
  };

  auto prevCpu = readCpuStats();
  sleep(1);
  auto currCpu = readCpuStats();

  unsigned long long prevIdle = prevCpu.getTotalIdle();
  unsigned long long currIdle = currCpu.getTotalIdle();
  unsigned long long prevTotal = prevCpu.getTotal();
  unsigned long long currTotal = currCpu.getTotal();

  unsigned long long totalDiff = currTotal - prevTotal;
  unsigned long long idleDiff = currIdle - prevIdle;

  if (totalDiff == 0)
    return 0.0;

  return (float)(totalDiff - idleDiff) / totalDiff * 100.0;
}

// Function 2: Returns RAM usage as {used GB, percentage used}
inline std::pair<float, float> getRamUsageGBPercent() {
  long totalMemKB = 0, freeMemKB = 0;
  std::ifstream file("/proc/meminfo");
  std::string line;

  while (getline(file, line)) {
    if (line.find("MemTotal:") == 0) {
      sscanf(line.c_str(), "MemTotal: %ld kB", &totalMemKB);
    } else if (line.find("MemFree:") == 0) {
      sscanf(line.c_str(), "MemFree: %ld kB", &freeMemKB);
    }
  }
  file.close();

  long usedMemKB = totalMemKB - freeMemKB;
  float totalGB = totalMemKB / (1024.0 * 1024.0);
  float usedGB = usedMemKB / (1024.0 * 1024.0);
  float usedPercent = (totalGB > 0) ? (usedGB / totalGB) * 100.0 : 0.0;

  return {usedGB, usedPercent};
}

// Function 3: Returns disk usage as {total GB, used percentage}
inline std::pair<float, float> getDiskUsageGBPercent() {
  struct statvfs stat;
  if (statvfs("/", &stat) != 0) {
    return {0.0f, 0.0f};
  }

  unsigned long long totalBytes =
      (unsigned long long)stat.f_blocks * stat.f_frsize;
  unsigned long long freeBytes =
      (unsigned long long)stat.f_bfree * stat.f_frsize;
  unsigned long long usedBytes = totalBytes - freeBytes;

  float totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
  float usedPercent =
      (totalBytes > 0) ? ((float)usedBytes / totalBytes) * 100.0 : 0.0;

  return {totalGB, usedPercent};
}

// Function 4: Returns network bandwidth as formatted strings {in, out}
inline std::pair<std::string, std::string> getNetworkBandwidthFormatted() {
  struct NetworkStats {
    unsigned long long rxBytes = 0;
    unsigned long long txBytes = 0;
  };

  auto readNetworkStats = []() -> NetworkStats {
    NetworkStats stats;
    std::ifstream file("/proc/net/dev");
    std::string line;

    // Skip header lines
    getline(file, line);
    getline(file, line);

    while (getline(file, line)) {
      std::stringstream ss(line);
      std::string iface;
      ss >> iface;

      // Remove trailing ':' from interface name
      if (!iface.empty() && iface.back() == ':') {
        iface.pop_back();
      }

      unsigned long long rBytes, rPackets, rErrs, rDrop, rFifo, rFrame,
          rCompressed, rMulticast;
      unsigned long long tBytes, tPackets, tErrs, tDrop, tFifo, tColls,
          tCarrier, tCompressed;

      ss >> rBytes >> rPackets >> rErrs >> rDrop >> rFifo >> rFrame >>
          rCompressed >> rMulticast;
      ss >> tBytes >> tPackets >> tErrs >> tDrop >> tFifo >> tColls >>
          tCarrier >> tCompressed;

      // Skip loopback interface
      if (iface != "lo") {
        stats.rxBytes += rBytes;
        stats.txBytes += tBytes;
      }
    }
    file.close();
    return stats;
  };

  auto prevStats = readNetworkStats();
  sleep(1);
  auto currStats = readNetworkStats();

  // Calculate bandwidth in bytes per second
  unsigned long long inBytesPerSecond = (currStats.rxBytes - prevStats.rxBytes);
  unsigned long long outBytesPerSecond =
      (currStats.txBytes - prevStats.txBytes);

  // Format to appropriate units
  std::string inBandwidth = formatBandwidth(inBytesPerSecond);
  std::string outBandwidth = formatBandwidth(outBytesPerSecond);

  return {inBandwidth, outBandwidth};
}

// Example usage
inline system_usage system_monitor() {
  // CPU Usage
  struct system_usage s;
  s.cpu_usage = getCpuUsagePercent();

  // RAM Usage
  auto [ramUsedGB, ramPercent] = getRamUsageGBPercent();
  s.ram_usage = ramPercent;

  // Disk Usage
  auto [diskTotalGB, diskPercent] = getDiskUsageGBPercent();
  std::cout << "Disk: " << diskTotalGB << " GB total, " << diskPercent
            << "% used" << std::endl;
  s.disk_usage = diskPercent;

  // Network Bandwidth (now formatted with appropriate units)
  auto [netIn, netOut] = getNetworkBandwidthFormatted();
  std::cout << "Network: " << netIn << " in, " << netOut << " out" << std::endl;
  s.network_in = netIn;
  s.network_out = netOut;
  return s;
}

#endif // SYSTEM_INFO_HPP
