#pragma once
#include <clever/net/header_map.h>
#include <cstdint>
#include <string>
#include <vector>

namespace clever::net {

struct Response {
    uint16_t status = 0;
    std::string status_text;
    HeaderMap headers;
    std::vector<uint8_t> body;
    std::string url;
    bool was_redirected = false;

    // Parse from raw HTTP/1.1 response bytes
    static std::optional<Response> parse(const std::vector<uint8_t>& data);

    // Convenience: body as string
    std::string body_as_string() const;
};

} // namespace clever::net
