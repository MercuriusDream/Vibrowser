#include <clever/net/connection_pool.h>
#include <unistd.h>

namespace clever::net {

ConnectionPool::ConnectionPool(size_t max_per_host)
    : max_per_host_(max_per_host) {}

ConnectionPool::~ConnectionPool() {
    clear();
}

std::string ConnectionPool::make_key(const std::string& host, uint16_t port) const {
    return host + ":" + std::to_string(port);
}

int ConnectionPool::acquire(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(host, port);
    auto it = pools_.find(key);
    if (it == pools_.end() || it->second.empty()) {
        return -1;
    }

    // LIFO: take from the back (most recently added)
    auto conn = it->second.back();
    it->second.pop_back();
    return conn.fd;
}

void ConnectionPool::release(const std::string& host, uint16_t port, int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(host, port);
    auto& pool = pools_[key];

    // If at capacity, remove the oldest (front) and close it
    if (pool.size() >= max_per_host_) {
        int old_fd = pool.front().fd;
        pool.pop_front();
        // Close the evicted connection's fd.
        // Note: In tests we use fake fds, so close may fail -- that is fine.
        ::close(old_fd);
    }

    pool.push_back(PooledConnection{fd, std::chrono::steady_clock::now()});
}

void ConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, pool] : pools_) {
        for (auto& conn : pool) {
            ::close(conn.fd);
        }
    }
    pools_.clear();
}

size_t ConnectionPool::count(const std::string& host, uint16_t port) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(host, port);
    auto it = pools_.find(key);
    if (it == pools_.end()) {
        return 0;
    }
    return it->second.size();
}

} // namespace clever::net
