#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace clever::url {

struct URL {
    std::string scheme;
    std::string username;
    std::string password;
    std::string host;
    std::optional<uint16_t> port;
    std::string path;
    std::string query;
    std::string fragment;

    std::string serialize() const;
    std::string origin() const;
    bool is_special() const;
};

std::optional<URL> parse(std::string_view input, const URL* base = nullptr);
bool urls_same_origin(const URL& a, const URL& b);

} // namespace clever::url
