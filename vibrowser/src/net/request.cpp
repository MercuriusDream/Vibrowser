#include <clever/net/request.h>
#include <clever/net/cookie_jar.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <clever/url/url.h>

namespace clever::net {

std::string method_to_string(Method method) {
    switch (method) {
        case Method::GET:           return "GET";
        case Method::POST:          return "POST";
        case Method::PUT:           return "PUT";
        case Method::DELETE_METHOD: return "DELETE";
        case Method::HEAD:          return "HEAD";
        case Method::OPTIONS:       return "OPTIONS";
        case Method::PATCH:         return "PATCH";
    }
    return "GET";
}

Method string_to_method(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upper == "GET")     return Method::GET;
    if (upper == "POST")    return Method::POST;
    if (upper == "PUT")     return Method::PUT;
    if (upper == "DELETE")  return Method::DELETE_METHOD;
    if (upper == "HEAD")    return Method::HEAD;
    if (upper == "OPTIONS") return Method::OPTIONS;
    if (upper == "PATCH")   return Method::PATCH;

    return Method::GET;  // default
}

void Request::parse_url() {
    if (auto parsed = clever::url::parse(url)) {
        host = parsed->host;
        use_tls = (parsed->scheme == "https");
        port = parsed->port.value_or(use_tls ? 443 : 80);
        path = parsed->path.empty() ? "/" : parsed->path;
        query = parsed->query;
        return;
    }

    // Fallback to legacy parsing logic for non-parseable URLs
    std::string url_copy = url;

    // Extract scheme
    bool is_https = false;
    std::string scheme_sep = "://";
    auto scheme_pos = url_copy.find(scheme_sep);
    if (scheme_pos != std::string::npos) {
        std::string scheme = url_copy.substr(0, scheme_pos);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        is_https = (scheme == "https");
        url_copy = url_copy.substr(scheme_pos + 3);
    }

    // Set default port and TLS flag based on scheme
    use_tls = is_https;
    port = is_https ? 443 : 80;

    // Split authority from path
    auto path_pos = url_copy.find('/');
    std::string authority;
    std::string rest;
    if (path_pos != std::string::npos) {
        authority = url_copy.substr(0, path_pos);
        rest = url_copy.substr(path_pos);
    } else {
        authority = url_copy;
        rest = "/";
    }

    // Parse authority for host:port
    auto colon_pos = authority.find(':');
    if (colon_pos != std::string::npos) {
        host = authority.substr(0, colon_pos);
        std::string port_str = authority.substr(colon_pos + 1);
        if (!port_str.empty()) {
            try {
                size_t consumed = 0;
                unsigned long parsed = std::stoul(port_str, &consumed, 10);
                if (consumed == port_str.size() && parsed <= 65535UL) {
                    port = static_cast<uint16_t>(parsed);
                }
            } catch (...) {
                // Keep default port for malformed explicit port.
            }
        }
    } else {
        host = authority;
    }

    // Split path from query
    auto query_pos = rest.find('?');
    if (query_pos != std::string::npos) {
        path = rest.substr(0, query_pos);
        query = rest.substr(query_pos + 1);
    } else {
        path = rest;
        query.clear();
    }

    // Ensure path is at least "/"
    if (path.empty()) {
        path = "/";
    }
}

std::vector<uint8_t> Request::serialize() const {
    std::ostringstream oss;

    // Request line
    oss << method_to_string(method) << " " << path;
    if (!query.empty()) {
        oss << "?" << query;
    }
    oss << " HTTP/1.1\r\n";

    // Host header (always first)
    oss << "Host: " << host;
    if ((port != 80) && (port != 443)) {
        oss << ":" << port;
    }
    oss << "\r\n";

    // Connection: keep-alive — reuse connections via ConnectionPool for plain HTTP.
    // TLS connections are still closed after each request (SecureTransport state).
    oss << "Connection: keep-alive\r\n";

    // Default User-Agent
    if (!headers.has("user-agent")) {
        oss << "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n";
    }

    // Default Accept
    if (!headers.has("accept")) {
        oss << "Accept: text/html,application/xhtml+xml,*/*;q=0.8\r\n";
    }

    // Default Accept-Encoding (we support gzip)
    if (!headers.has("accept-encoding")) {
        oss << "Accept-Encoding: gzip, deflate\r\n";
    }

    // Default Accept-Language — prevents sites from showing consent/locale pages
    if (!headers.has("accept-language")) {
        oss << "Accept-Language: en-US,en;q=0.9\r\n";
    }

    std::string cookies = CookieJar::shared().get_cookie_header(
        host, path, use_tls, true, method == Method::GET);
    if (!cookies.empty()) {
        oss << "Cookie: " << cookies << "\r\n";
    }

    // User-supplied headers
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        // Skip host and connection if user set them (we already wrote them)
        if (it->first == "host" || it->first == "connection") {
            continue;
        }
        oss << it->first << ": " << it->second << "\r\n";
    }

    // Content-Length for requests with a body
    if (!body.empty()) {
        // Only add if user hasn't set it
        if (!headers.has("content-length")) {
            oss << "Content-Length: " << body.size() << "\r\n";
        }
    }

    // End of headers
    oss << "\r\n";

    // Convert to bytes
    std::string header_str = oss.str();
    std::vector<uint8_t> result(header_str.begin(), header_str.end());

    // Append body if present
    if (!body.empty()) {
        result.insert(result.end(), body.begin(), body.end());
    }

    return result;
}

} // namespace clever::net
