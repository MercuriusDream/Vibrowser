#pragma once
#include <clever/net/header_map.h>
#include <cstdint>
#include <string>
#include <vector>

namespace clever::net {

enum class Method {
    GET, POST, PUT, DELETE_METHOD, HEAD, OPTIONS, PATCH
};

std::string method_to_string(Method method);
Method string_to_method(const std::string& str);

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

    // Serialize to HTTP/1.1 request bytes
    std::vector<uint8_t> serialize() const;

    // Parse URL into host/port/path/query
    void parse_url();
};

} // namespace clever::net
