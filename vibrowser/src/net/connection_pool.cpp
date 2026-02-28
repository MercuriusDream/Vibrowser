#include <clever/net/connection_pool.h>
#include <unistd.h>

namespace clever::net {

ConnectionPool::ConnectionPool(size_t max_per_host, size_t max_total,
                               std::chrono::seconds idle_timeout)
    : max_per_host_(max_per_host),
      max_total_(max_total),
      idle_timeout_(idle_timeout) {}

ConnectionPool::~ConnectionPool() {
    clear();
}

std::string ConnectionPool::make_key(const std::string& host, uint16_t port) const {
    return host + ":" + std::to_string(port);
}

int ConnectionPool::acquire(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    evict_idle_locked();

    auto key = make_key(host, port);
    auto it = pools_.find(key);
    if (it == pools_.end() || it->second.empty()) {
        return -1;
    }

    // LIFO: take from the back (most recently added)
    while (!it->second.empty()) {
        auto conn = it->second.back();
        it->second.pop_back();

        auto idle_for = std::chrono::steady_clock::now() - conn.returned_at;
        if (idle_for > idle_timeout_) {
            ::close(conn.fd);
            continue;
        }

        if (it->second.empty()) {
            pools_.erase(it);
        }
        return conn.fd;
    }

    pools_.erase(it);
    return -1;
}

void ConnectionPool::release(const std::string& host, uint16_t port, int fd) {
    if (fd < 0) {
        return;
    }
    if (max_per_host_ == 0 || max_total_ == 0) {
        ::close(fd);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    evict_idle_locked();

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

    while (total_count_locked() > max_total_) {
        auto oldest_pool_it = pools_.end();
        std::chrono::steady_clock::time_point oldest_returned_at =
            std::chrono::steady_clock::time_point::max();

        for (auto it = pools_.begin(); it != pools_.end(); ++it) {
            if (it->second.empty()) continue;
            const auto& candidate = it->second.front();
            if (candidate.returned_at < oldest_returned_at) {
                oldest_returned_at = candidate.returned_at;
                oldest_pool_it = it;
            }
        }

        if (oldest_pool_it == pools_.end() || oldest_pool_it->second.empty()) {
            break;
        }

        int old_fd = oldest_pool_it->second.front().fd;
        oldest_pool_it->second.pop_front();
        ::close(old_fd);
        if (oldest_pool_it->second.empty()) {
            pools_.erase(oldest_pool_it);
        }
    }
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

void ConnectionPool::evict_idle_locked() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = pools_.begin(); it != pools_.end();) {
        auto& pool = it->second;
        while (!pool.empty()) {
            const auto& conn = pool.front();
            auto idle_for = now - conn.returned_at;
            if (idle_for <= idle_timeout_) {
                break;
            }
            ::close(conn.fd);
            pool.pop_front();
        }

        if (pool.empty()) {
            it = pools_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t ConnectionPool::total_count_locked() const {
    size_t total = 0;
    for (const auto& [key, pool] : pools_) {
        (void)key;
        total += pool.size();
    }
    return total;
}

} // namespace clever::net
