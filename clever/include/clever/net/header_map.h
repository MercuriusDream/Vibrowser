#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace clever::net {

class HeaderMap {
public:
    void set(const std::string& name, const std::string& value);
    void append(const std::string& name, const std::string& value);
    std::optional<std::string> get(const std::string& name) const;
    std::vector<std::string> get_all(const std::string& name) const;
    bool has(const std::string& name) const;
    void remove(const std::string& name);
    size_t size() const;
    bool empty() const;

    // Iteration
    using iterator = std::unordered_multimap<std::string, std::string>::const_iterator;
    iterator begin() const;
    iterator end() const;

private:
    // Case-insensitive storage: keys stored lowercase
    std::unordered_multimap<std::string, std::string> headers_;
    static std::string normalize_name(const std::string& name);
};

} // namespace clever::net
