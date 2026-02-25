#include <clever/net/header_map.h>
#include <algorithm>
#include <cctype>

namespace clever::net {

std::string HeaderMap::normalize_name(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

void HeaderMap::set(const std::string& name, const std::string& value) {
    auto key = normalize_name(name);
    // Remove all existing entries with this key
    headers_.erase(key);
    // Insert the new value
    headers_.emplace(key, value);
}

void HeaderMap::append(const std::string& name, const std::string& value) {
    auto key = normalize_name(name);
    headers_.emplace(key, value);
}

std::optional<std::string> HeaderMap::get(const std::string& name) const {
    auto key = normalize_name(name);
    auto it = headers_.find(key);
    if (it != headers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> HeaderMap::get_all(const std::string& name) const {
    auto key = normalize_name(name);
    std::vector<std::string> result;
    auto range = headers_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    return result;
}

bool HeaderMap::has(const std::string& name) const {
    auto key = normalize_name(name);
    return headers_.find(key) != headers_.end();
}

void HeaderMap::remove(const std::string& name) {
    auto key = normalize_name(name);
    headers_.erase(key);
}

size_t HeaderMap::size() const {
    return headers_.size();
}

bool HeaderMap::empty() const {
    return headers_.empty();
}

HeaderMap::iterator HeaderMap::begin() const {
    return headers_.begin();
}

HeaderMap::iterator HeaderMap::end() const {
    return headers_.end();
}

} // namespace clever::net
