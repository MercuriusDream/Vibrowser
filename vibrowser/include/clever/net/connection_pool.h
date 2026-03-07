#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace clever::net {

class ConnectionPool {
public:
    struct EvictionStats {
        size_t idle_evictions = 0;
        size_t per_host_capacity_evictions = 0;
        size_t total_capacity_evictions = 0;
    };

    explicit ConnectionPool(size_t max_per_host = 6,
                            size_t max_total = 30,
                            std::chrono::seconds idle_timeout = std::chrono::seconds(60));
    ~ConnectionPool();

    // Get an existing connection to host:port, or -1 if none available
    int acquire(const std::string& host, uint16_t port);

    // Return a connection to the pool
    void release(const std::string& host, uint16_t port, int fd);

    // Close all pooled connections
    void clear();

    // Get count of pooled connections for a host
    size_t count(const std::string& host, uint16_t port) const;
    size_t reused_socket_count() const;
    EvictionStats eviction_stats() const;

private:
    struct PooledConnection {
        int fd;
        std::chrono::steady_clock::time_point returned_at;
    };

    std::string make_key(const std::string& host, uint16_t port) const;
    void evict_idle_locked();
    size_t total_count_locked() const;

    size_t max_per_host_;
    size_t max_total_;
    std::chrono::seconds idle_timeout_;
    size_t reused_socket_count_ = 0;
    size_t idle_evictions_count_ = 0;
    size_t per_host_capacity_evictions_count_ = 0;
    size_t total_capacity_evictions_count_ = 0;
    std::unordered_map<std::string, std::deque<PooledConnection>> pools_;
    mutable std::mutex mutex_;
};

} // namespace clever::net
