#ifndef REDIS_HANDLER_HPP
#define REDIS_HANDLER_HPP

#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#ifdef WITH_REDIS
#include <sw/redis++/redis++.h>
using namespace sw::redis;
#endif

// Utility: simple ID when file_name isn’t supplied (time-based hex).
inline std::string gen_file_id() {
  auto now =
      std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::ostringstream os;
  os << std::hex << now;
  return os.str();
}

inline std::string file_key(const std::string &id) { return "file:" + id; }

// Encode/decode "server|path" without JSON.
inline std::string encode_loc(const std::string &server,
                              const std::string &path) {
  // If server/path may contain '|', choose a different delimiter or escape it.
  return server + "|" + path;
}
inline std::pair<std::string, std::string> decode_loc(const std::string &v) {
  auto p = v.find('|');
  if (p == std::string::npos)
    return {v, ""};
  return {v.substr(0, p), v.substr(p + 1)};
}

#ifdef WITH_REDIS
inline void create_entry(const std::string& request) {
  try {
    Redis redis("tcp://127.0.0.1:6379"); // primary for writes [1]

    std::istringstream in(request);
    std::string file_name;
    std::getline(in, file_name);

    if (file_name.empty())
      file_name = gen_file_id();
    const std::string key = file_key(file_name);

    // Optional TTL line: "TTL=seconds"
    long long ttl = 0;
    std::streampos after_first_line = in.tellg();
    std::string maybe_ttl;
    if (std::getline(in, maybe_ttl)) {
      if (maybe_ttl.rfind("TTL=", 0) == 0) {
        ttl = std::stoll(maybe_ttl.substr(4));
      } else {
        // Not TTL; rewind to start parsing chunks from this line
        in.clear();
        in.seekg(after_first_line);
      }
    }

    // Batch fields for HSET key field value [5][1]
    std::vector<std::pair<std::string, std::string>> fields;
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty())
        continue;
      std::istringstream ls(line);
      long long chunk_id;
      std::string server, path;
      if (!(ls >> chunk_id >> server >> path)) {
        continue; // skip malformed
      }
      std::string field = "chunk:" + std::to_string(chunk_id);
      fields.emplace_back(field, encode_loc(server, path));
    }

    if (!fields.empty()) {
      redis.hset(key, fields.begin(), fields.end()); // bulk HSET [1][5]
    } else {
      // Optionally create a marker so the hash exists:
      // redis.hset(key, "meta", "created");
    }

    if (ttl > 0) {
      redis.expire(key, std::chrono::seconds{ttl}); // set TTL on hash key [1]
    }

    std::cout << "Created file entry: " << file_name << "\n";
  } catch (const std::exception &e) {
    std::cerr << "create_entry error: " << e.what() << "\n";
  }
}
#else
inline void create_entry(const std::string& request) {
  std::cout << "Redis disabled - create_entry not implemented\n";
}
#endif

#ifdef WITH_REDIS
inline void read_entry(const std::string& request) {
  try {
    std::istringstream in(request);
    std::string file_name;
    in >> file_name;
    if (file_name.empty()) {
      std::cerr << "read_entry: file_name required\n";
      return;
    }
    const std::string key = file_key(file_name);

    // If reading from a replica, point this connection to the replica host.
    // [20]
    Redis redis("tcp://127.0.0.1:6379"); // [1]

    if (in.good()) {
      // Specific chunk
      long long chunk_id;
      if (in >> chunk_id) {
        std::string field = "chunk:" + std::to_string(chunk_id);
        auto v = redis.hget(key, field); // optional<string> [1][15]
        if (v) {
          auto [server, path] = decode_loc(*v);
          std::cout << field << " server=" << server << " path=" << path
                    << "\n";
        } else {
          std::cout << "Chunk not found\n";
        }
        return;
      }
    }

    // All chunks
    std::unordered_map<std::string, std::string> all;
    redis.hgetall(key, std::inserter(all, all.end())); // [1][8]
    if (all.empty()) {
      std::cout << "No chunks or file not found\n";
      return;
    }
    for (const auto &kv : all) {
      if (kv.first.rfind("chunk:", 0) == 0) {
        auto [server, path] = decode_loc(kv.second);
        std::cout << kv.first << " server=" << server << " path=" << path
                  << "\n";
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "read_entry error: " << e.what() << "\n";
  }
}
#else
inline void read_entry(const std::string& request) {
  std::cout << "Redis disabled - read_entry not implemented\n";
}
#endif

#ifdef WITH_REDIS
inline void delete_entry(const std::string& file_name) {
  try {
    // If file_name looks like "name#chunk:3", delete that field; else delete
    // hash. [15]
    std::string base = file_name;
    std::string field;

    auto pos = file_name.find("#chunk:");
    if (pos != std::string::npos) {
      base = file_name.substr(0, pos);
      field = file_name.substr(pos + 1); // "chunk:3"
    }

    const std::string key = file_key(base);
    Redis redis("tcp://127.0.0.1:6379"); // [1]

    if (!field.empty()) {
      long long n = redis.hdel(key, field); // [1][15]
      std::cout << "Removed fields: " << n << "\n";
    } else {
      long long n = redis.del(key); // [1]
      std::cout << "Removed keys: " << n << "\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "delete_entry error: " << e.what() << "\n";
  }
}
#else
inline void delete_entry(const std::string& file_name) {
  std::cout << "Redis disabled - delete_entry not implemented\n";
}
#endif

#ifdef WITH_REDIS
// Configure the local Redis instance to replicate from the given "host:port".
// Issues REPLICAOF via the generic command interface; returns 0 on success.
// [19]
inline int create_replication(const std::string& ip_address) {
  std::cout << "Creating replication server\n";
  std::cout
      << "Current machine Slave , master server in the sentient protocol is "
      << ip_address << std::endl;
  try {
    auto pos = ip_address.find(':');
    std::string host = ip_address.substr(0, pos);
    int port = (pos == std::string::npos)
                   ? 6379
                   : std::stoi(ip_address.substr(pos + 1));

    Redis redis("tcp://127.0.0.1:6379"); // local node to become a replica [1]
    redis.command("REPLICAOF", host,
                  std::to_string(port)); // server-side replication [19]
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "create_replication error: " << e.what() << "\n";
    return -1;
  }
}
#else
inline int create_replication(const std::string& ip_address) {
  std::cout << "Redis disabled - create_replication not implemented\n";
  return -1;
}
#endif

#ifdef WITH_REDIS
inline int start_server() {
  // First fork
  pid_t pid = fork();
  if (pid < 0)
    return 1;
  if (pid > 0)
    return 0; // parent exits

  // New session
  if (setsid() < 0)
    _exit(1);

  // Second fork
  pid = fork();
  if (pid < 0)
    _exit(1);
  if (pid > 0)
    _exit(0); // intermediate exits

  // Detach
  // chdir("/");
  umask(027);

  // Detach stdio
  for (int fd = 0; fd < 3; ++fd)
    close(fd);
  open("/dev/null", O_RDONLY);                   // stdin
  open("../../logs/current_logs.txt", O_WRONLY); // stdout
  open("/dev/null", O_WRONLY);                   // stderr

  // IMPORTANT: Start Redis with NO arguments (no config file)
  // This uses Redis built-in defaults. Suitable for testing/dev.
  // For production, prefer a config file.
  execl("/usr/bin/redis-server", "redis-server", (char *)nullptr);

  // If execl fails
  _exit(1);
}
#else
inline int start_server() {
  std::cout << "Redis disabled - start_server not implemented\n";
  return -1;
}
#endif

inline void start_daemon() {
  std::cout << "Starting Head Server daemon..." << std::endl;
  
  // Start Redis server in background
  if (start_server() != 0) {
    std::cerr << "Failed to start Redis server" << std::endl;
    return;
  }
  
  // Give Redis time to start
  sleep(2);
}

#endif // REDIS_HANDLER_HPP
