#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace clever::net {

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path = "/";
    bool secure = false;
    bool http_only = false;
    std::string same_site;  // "Strict", "Lax", "None"
    // 0 = session cookie (no expiry); otherwise epoch seconds
    int64_t expires_at = 0;
};

class CookieJar {
public:
    // Parse and store cookies from a Set-Cookie header value
    void set_from_header(const std::string& header_value, const std::string& request_domain);

    // Get a Cookie header value for a given URL
    // is_same_site: true if the request is to the same site as the page origin
    // is_top_level_nav: true if this is a top-level navigation (GET for a page)
    std::string get_cookie_header(const std::string& domain, const std::string& path,
                                  bool is_secure, bool is_same_site = true,
                                  bool is_top_level_nav = true) const;

    // Clear all cookies
    void clear();

    // Number of stored cookies
    size_t size() const;

    // Singleton access for shared cookie jar
    static CookieJar& shared();

private:
    mutable std::mutex mutex_;
    // domain -> list of cookies
    std::unordered_map<std::string, std::vector<Cookie>> cookies_;

    bool domain_matches(const std::string& cookie_domain, const std::string& request_domain) const;
    bool path_matches(const std::string& cookie_path, const std::string& request_path) const;
};

} // namespace clever::net
