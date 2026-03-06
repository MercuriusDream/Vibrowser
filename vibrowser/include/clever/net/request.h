#pragma once
#include <atomic>
#include <clever/net/header_map.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace clever::net {

enum class Method {
    GET, POST, PUT, DELETE_METHOD, HEAD, OPTIONS, PATCH
};

std::string method_to_string(Method method);
Method string_to_method(const std::string& str);

struct RequestCancellationState {
    std::atomic<bool> cancelled {false};
    std::mutex active_fd_mutex;
    int active_fd = -1;

    void cancel();
    bool is_cancelled() const;
    void set_active_fd(int fd);
    void clear_active_fd(int fd);
};

struct Request {
    std::string url;
    Method method = Method::GET;
    HeaderMap headers;
    std::vector<uint8_t> body;
    std::string host;
    uint16_t port = 80;
    std::string path = "/";
    std::string query;
    bool use_tls = false;
    std::shared_ptr<RequestCancellationState> cancellation_state;

    // Serialize to HTTP/1.1 request bytes
    std::vector<uint8_t> serialize() const;

    // Parse URL into host/port/path/query
    void parse_url();

    bool is_cancelled() const;
    void cancel() const;
};

} // namespace clever::net
