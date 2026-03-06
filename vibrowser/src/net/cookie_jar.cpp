#include <clever/net/cookie_jar.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>

namespace clever::net {

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool domain_matches_value(const std::string& cookie_domain, const std::string& request_domain) {
    if (cookie_domain == request_domain) return true;
    if (request_domain.size() <= cookie_domain.size()) return false;
    auto pos = request_domain.size() - cookie_domain.size();
    return request_domain[pos - 1] == '.' && request_domain.substr(pos) == cookie_domain;
}

std::string normalize_path(const std::string& path) {
    if (path.empty() || path[0] != '/') return "/";
    return path;
}

std::string default_path_for_request(const std::string& request_path) {
    const std::string normalized_request_path = normalize_path(request_path);
    if (normalized_request_path == "/") return "/";

    const size_t last_slash = normalized_request_path.rfind('/');
    if (last_slash == std::string::npos || last_slash == 0) return "/";
    return normalized_request_path.substr(0, last_slash);
}

} // anonymous namespace

CookieJar& CookieJar::shared() {
    static CookieJar instance;
    return instance;
}

void CookieJar::set_from_header(const std::string& header_value,
                                const std::string& request_domain,
                                const std::string& request_path) {
    // Parse: name=value; Path=/; Domain=.example.com; Secure; HttpOnly
    Cookie cookie;
    cookie.domain = to_lower(request_domain);
    cookie.path = default_path_for_request(request_path);

    // Split by semicolons
    std::istringstream iss(header_value);
    std::string part;
    bool first = true;

    while (std::getline(iss, part, ';')) {
        part = trim(part);
        if (part.empty()) continue;

        if (first) {
            // First part is name=value
            auto eq = part.find('=');
            if (eq == std::string::npos) return; // invalid
            cookie.name = trim(part.substr(0, eq));
            cookie.value = trim(part.substr(eq + 1));
            first = false;
            continue;
        }

        // Attribute
        auto eq = part.find('=');
        std::string attr_name, attr_value;
        if (eq != std::string::npos) {
            attr_name = to_lower(trim(part.substr(0, eq)));
            attr_value = trim(part.substr(eq + 1));
        } else {
            attr_name = to_lower(part);
        }

        if (attr_name == "domain") {
            std::string dom = to_lower(attr_value);
            // Remove leading dot
            if (!dom.empty() && dom[0] == '.') dom = dom.substr(1);
            if (dom.empty() || !domain_matches_value(dom, cookie.domain)) {
                return;
            }
            cookie.domain = dom;
            cookie.host_only = false;
        } else if (attr_name == "path") {
            if (!attr_value.empty() && attr_value[0] == '/') {
                cookie.path = attr_value;
            }
        } else if (attr_name == "secure") {
            cookie.secure = true;
        } else if (attr_name == "httponly") {
            cookie.http_only = true;
        } else if (attr_name == "max-age") {
            // Max-Age in seconds from now
            try {
                int max_age = std::stoi(attr_value);
                if (max_age <= 0) {
                    // Delete cookie (expired)
                    cookie.expires_at = 1; // epoch + 1 = expired
                } else {
                    cookie.expires_at = static_cast<int64_t>(std::time(nullptr)) + max_age;
                }
            } catch (...) {}
        } else if (attr_name == "expires") {
            // Parse HTTP date: e.g. "Thu, 01 Dec 2025 00:00:00 GMT"
            struct std::tm tm = {};
            if (strptime(attr_value.c_str(), "%a, %d %b %Y %H:%M:%S", &tm)) {
                cookie.expires_at = static_cast<int64_t>(timegm(&tm));
            }
        } else if (attr_name == "samesite") {
            cookie.same_site = attr_value;
        }
    }

    if (cookie.name.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Replace existing cookie with same name+domain+path
    auto& domain_cookies = cookies_[cookie.domain];
    domain_cookies.erase(
        std::remove_if(domain_cookies.begin(), domain_cookies.end(),
                       [&](const Cookie& existing) {
                           return existing.name == cookie.name && existing.path == cookie.path;
                       }),
        domain_cookies.end());

    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (cookie.expires_at > 0 && cookie.expires_at <= now) {
        return;
    }

    cookie.creation_index = next_cookie_creation_index_++;
    domain_cookies.push_back(std::move(cookie));
}

std::string CookieJar::get_cookie_header(const std::string& domain, const std::string& path,
                                          bool is_secure, bool is_same_site,
                                          bool is_top_level_nav) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string domain_lower = to_lower(domain);
    std::string request_path = normalize_path(path);
    struct CookieMatch {
        const Cookie* cookie = nullptr;
        size_t path_length = 0;
    };
    std::vector<CookieMatch> matches;

    for (auto& [cookie_domain, domain_cookies] : cookies_) {
        int64_t now = static_cast<int64_t>(std::time(nullptr));
        for (auto& cookie : domain_cookies) {
            if (!domain_matches(cookie, domain_lower)) continue;
            if (!path_matches(cookie.path, request_path)) continue;
            if (cookie.secure && !is_secure) continue;
            // Skip expired cookies
            if (cookie.expires_at > 0 && cookie.expires_at <= now) continue;

            // SameSite enforcement
            if (!is_same_site) {
                std::string ss = to_lower(cookie.same_site);
                if (ss == "strict") {
                    // Strict: never send on cross-site requests
                    continue;
                }
                if (ss.empty() || ss == "lax") {
                    // Lax (default): only send on top-level GET navigations
                    if (!is_top_level_nav) continue;
                }
                // "none" cookies require Secure flag (already checked above)
                if (ss == "none" && !cookie.secure) continue;
            }

            matches.push_back(CookieMatch{
                .cookie = &cookie,
                .path_length = cookie.path.size(),
            });
        }
    }

    std::sort(matches.begin(), matches.end(),
              [](const CookieMatch& lhs, const CookieMatch& rhs) {
                  if (lhs.path_length != rhs.path_length) {
                      return lhs.path_length > rhs.path_length;
                  }
                  return lhs.cookie->creation_index < rhs.cookie->creation_index;
              });

    std::string result;
    for (const CookieMatch& match : matches) {
        if (!result.empty()) result += "; ";
        result += match.cookie->name + "=" + match.cookie->value;
    }

    return result;
}

void CookieJar::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cookies_.clear();
}

size_t CookieJar::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto& [_, domain_cookies] : cookies_) {
        count += domain_cookies.size();
    }
    return count;
}

bool CookieJar::domain_matches(const Cookie& cookie,
                                const std::string& request_domain) const {
    if (cookie.host_only) {
        return cookie.domain == request_domain;
    }
    return domain_matches_value(cookie.domain, request_domain);
}

bool CookieJar::path_matches(const std::string& cookie_path,
                               const std::string& request_path) const {
    if (cookie_path == "/") return true;
    if (cookie_path == request_path) return true;
    if (request_path.rfind(cookie_path, 0) != 0) return false;
    if (cookie_path.back() == '/') return true;
    return request_path.size() > cookie_path.size() &&
           request_path[cookie_path.size()] == '/';
}

} // namespace clever::net
