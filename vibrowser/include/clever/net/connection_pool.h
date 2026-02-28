#pragma once
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace clever::net {

class ConnectionPool {
public:
    explicit ConnectionPool(size_t max_per_host = 6);
    ~ConnectionPool();

    // Get an existing connection to host:port, or -1 if none available
    int acquire(const std::string& host, uint16_t port);

    // Return a connection to the pool
    void release(const std::string& host, uint16_t port, int fd);

    // Close all pooled connections
    void clear();

    // Get count of pooled connections for a host
    size_t count(const std::string& host, uint16_t port) const;

private:
    struct PooledConnection {
        int fd;
        std::chrono::steady_clock::time_point created_at;
    };

    std::string make_key(const std::string& host, uint16_t port) const;

    size_t max_per_host_;
    std::unordered_map<std::string, std::deque<PooledConnection>> pools_;
    mutable std::mutex mutex_;
};

} // namespace clever::net
