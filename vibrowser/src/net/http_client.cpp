#include <clever/net/http_client.h>
#include <clever/net/cookie_jar.h>
#include <clever/url/url.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace clever::net {

namespace {

constexpr int kTlsPoolMaxPerHost = 4;
const std::chrono::seconds kTlsIdleTimeout = std::chrono::seconds(30);

struct PooledTlsConn {
    std::unique_ptr<TlsSocket> tls;
    int fd;
    std::chrono::steady_clock::time_point returned_at;
};

std::map<std::string, std::deque<PooledTlsConn>> g_tls_pool;
std::mutex g_tls_pool_mutex;

bool request_cancelled(const Request& request) {
    return request.is_cancelled();
}

void set_request_active_fd(const Request& request, int fd) {
    if (request.cancellation_state) {
        request.cancellation_state->set_active_fd(fd);
    }
}

void clear_request_active_fd(const Request& request, int fd) {
    if (request.cancellation_state) {
        request.cancellation_state->clear_active_fd(fd);
    }
}

std::optional<size_t> find_crlf(const std::vector<uint8_t>& buffer, size_t start) {
    if (start >= buffer.size()) {
        return std::nullopt;
    }

    for (size_t i = start; i + 1 < buffer.size(); ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
            return i;
        }
    }
    return std::nullopt;
}

std::string_view trim_ascii_whitespace(std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool is_complete_chunked_body(const std::vector<uint8_t>& buffer, size_t body_start) {
    size_t pos = body_start;
    while (true) {
        const auto line_end = find_crlf(buffer, pos);
        if (!line_end.has_value()) {
            return false;
        }

        std::string_view size_line(reinterpret_cast<const char*>(buffer.data() + pos),
                                   *line_end - pos);
        size_line = trim_ascii_whitespace(size_line);
        const auto extension = size_line.find(';');
        const std::string_view size_text = trim_ascii_whitespace(
            extension == std::string_view::npos ? size_line : size_line.substr(0, extension));
        if (size_text.empty()) {
            return false;
        }

        size_t chunk_size = 0;
        const auto [ptr, ec] = std::from_chars(size_text.data(),
                                               size_text.data() + size_text.size(),
                                               chunk_size, 16);
        if (ec != std::errc{} || ptr != size_text.data() + size_text.size()) {
            return false;
        }

        pos = *line_end + 2;
        if (chunk_size == 0) {
            while (true) {
                const auto trailer_end = find_crlf(buffer, pos);
                if (!trailer_end.has_value()) {
                    return false;
                }
                if (*trailer_end == pos) {
                    return true;
                }
                pos = *trailer_end + 2;
            }
        }

        if (pos + chunk_size + 2 > buffer.size()) {
            return false;
        }
        if (buffer[pos + chunk_size] != '\r' || buffer[pos + chunk_size + 1] != '\n') {
            return false;
        }
        pos += chunk_size + 2;
    }
}

bool has_explicit_response_framing(const std::vector<uint8_t>& buffer) {
    const char* sep = "\r\n\r\n";
    const auto header_it = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
    if (header_it == buffer.end()) {
        return false;
    }

    std::string header_str(buffer.begin(),
                           buffer.begin() + static_cast<std::ptrdiff_t>(
                               (header_it - buffer.begin()) + 4));
    std::transform(header_str.begin(), header_str.end(), header_str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return header_str.find("content-length:") != std::string::npos ||
           header_str.find("transfer-encoding: chunked") != std::string::npos ||
           header_str.find("transfer-encoding:chunked") != std::string::npos;
}

std::string strip_fragment_from_url(const std::string& url) {
    const size_t fragment_pos = url.find('#');
    if (fragment_pos == std::string::npos) {
        return url;
    }
    return url.substr(0, fragment_pos);
}

std::string make_cache_key_fallback(std::string_view raw_url) {
    std::string normalized(strip_fragment_from_url(std::string(raw_url)));
    const size_t scheme_sep = normalized.find("://");
    if (scheme_sep == std::string::npos) {
        return normalized;
    }

    std::string scheme = normalized.substr(0, scheme_sep);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (scheme != "http" && scheme != "https") {
        return normalized;
    }

    const size_t authority_start = scheme_sep + 3;
    const size_t suffix_start = normalized.find_first_of("/?", authority_start);
    const std::string authority = normalized.substr(
        authority_start,
        suffix_start == std::string::npos ? std::string::npos : suffix_start - authority_start);
    if (authority.empty()) {
        return normalized;
    }

    const size_t userinfo_sep = authority.rfind('@');
    const std::string_view authority_host_port = userinfo_sep == std::string::npos
                                                     ? std::string_view(authority)
                                                     : std::string_view(authority).substr(userinfo_sep + 1);

    std::string host;
    std::optional<uint16_t> port;
    if (!authority_host_port.empty() && authority_host_port.front() == '[') {
        const size_t closing_bracket = authority_host_port.find(']');
        if (closing_bracket == std::string_view::npos) {
            return normalized;
        }

        host.assign(authority_host_port.substr(0, closing_bracket + 1));
        if (closing_bracket + 1 < authority_host_port.size()) {
            if (authority_host_port[closing_bracket + 1] != ':') {
                return normalized;
            }
            const std::string_view port_text = authority_host_port.substr(closing_bracket + 2);
            if (port_text.empty()) {
                return normalized;
            }
            for (const char ch : port_text) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    return normalized;
                }
            }
            const unsigned long parsed_port = std::stoul(std::string(port_text));
            if (parsed_port > 65535UL) {
                return normalized;
            }
            port = static_cast<uint16_t>(parsed_port);
        }
    } else {
        const size_t colon = authority_host_port.rfind(':');
        if (colon == std::string_view::npos) {
            host.assign(authority_host_port);
        } else {
            host.assign(authority_host_port.substr(0, colon));
            const std::string_view port_text = authority_host_port.substr(colon + 1);
            if (port_text.empty()) {
                return normalized;
            }
            for (const char ch : port_text) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    return normalized;
                }
            }
            const unsigned long parsed_port = std::stoul(std::string(port_text));
            if (parsed_port > 65535UL) {
                return normalized;
            }
            port = static_cast<uint16_t>(parsed_port);
        }
    }

    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if ((scheme == "http" && port == 80) || (scheme == "https" && port == 443)) {
        port.reset();
    }

    std::string suffix = suffix_start == std::string::npos ? "/" : normalized.substr(suffix_start);
    if (!suffix.empty() && suffix.front() == '?') {
        suffix.insert(suffix.begin(), '/');
    }

    std::string normalized_authority;
    if (userinfo_sep != std::string::npos) {
        normalized_authority = authority.substr(0, userinfo_sep + 1);
    }
    normalized_authority += host;
    if (port.has_value()) {
        normalized_authority += ':';
        normalized_authority += std::to_string(*port);
    }

    return scheme + "://" + normalized_authority + suffix;
}

std::optional<clever::url::URL> request_url_for_redirect_resolution(const Request& request) {
    if (!request.url.empty()) {
        if (auto parsed = clever::url::parse(request.url)) {
            parsed->fragment.clear();
            if (parsed->is_special() && !parsed->host.empty() && parsed->path.empty()) {
                parsed->path = "/";
            }
            return parsed;
        }
        return std::nullopt;
    }

    clever::url::URL url;
    url.scheme = (request.use_tls || request.port == 443) ? "https" : "http";
    url.host = request.host;
    if (url.host.empty()) {
        return std::nullopt;
    }
    if ((url.scheme == "http" && request.port != 80) ||
        (url.scheme == "https" && request.port != 443)) {
        url.port = request.port;
    }
    url.path = request.path.empty() ? "/" : request.path;
    url.query = request.query;
    url.fragment.clear();
    return url;
}

std::optional<std::string> resolve_redirect_url(const Request& request,
                                                const std::string& location) {
    const auto base = request_url_for_redirect_resolution(request);
    if (!base.has_value()) {
        return std::nullopt;
    }

    const auto resolved = clever::url::parse(location, &*base);
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    return resolved->serialize();
}

std::unique_ptr<TlsSocket> acquire_tls_conn(const std::string& host, uint16_t port, int& fd) {
    fd = -1;
    const std::string key = host + ":" + std::to_string(port);

    std::lock_guard<std::mutex> lock(g_tls_pool_mutex);

    auto it = g_tls_pool.find(key);
    if (it == g_tls_pool.end()) return nullptr;

    auto now = std::chrono::steady_clock::now();
    auto& list = it->second;

    while (!list.empty()) {
        PooledTlsConn conn = std::move(list.front());
        list.pop_front();

        if (now - conn.returned_at > kTlsIdleTimeout) {
            if (conn.tls) {
                conn.tls->close();
            }
            if (conn.fd >= 0) {
                ::close(conn.fd);
            }
            continue;
        }

        fd = conn.fd;
        return std::move(conn.tls);
    }

    if (list.empty()) {
        g_tls_pool.erase(it);
    }

    return nullptr;
}

void release_tls_conn(const std::string& host,
                     uint16_t port,
                     std::unique_ptr<TlsSocket> tls,
                     int fd) {
    if (!tls || fd < 0) {
        if (tls) {
            tls->close();
        }
        if (fd >= 0) {
            ::close(fd);
        }
        return;
    }

    const std::string key = host + ":" + std::to_string(port);
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(g_tls_pool_mutex);
    auto& list = g_tls_pool[key];

    while (list.size() >= static_cast<size_t>(kTlsPoolMaxPerHost)) {
        PooledTlsConn conn = std::move(list.front());
        list.pop_front();

        if (conn.tls) {
            conn.tls->close();
        }
        if (conn.fd >= 0) {
            ::close(conn.fd);
        }
    }

    list.push_back(PooledTlsConn{std::move(tls), fd, now});
}

}  // namespace

// ===========================================================================
// CacheEntry helpers
// ===========================================================================

size_t CacheEntry::approx_size() const {
    size_t s = url.size() + etag.size() + last_modified.size() + body.size();
    for (auto& [k, v] : headers) {
        s += k.size() + v.size();
    }
    return s + sizeof(CacheEntry);
}

bool CacheEntry::is_fresh() const {
    if (no_cache || must_revalidate) return false;
    if (max_age_seconds <= 0) return false;
    auto elapsed = std::chrono::steady_clock::now() - stored_at;
    auto elapsed_secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    return elapsed_secs < max_age_seconds;
}

// ===========================================================================
// Cache-Control parsing
// ===========================================================================

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

CacheControl parse_cache_control(const std::string& value) {
    CacheControl cc;
    std::istringstream stream(value);
    std::string directive;
    while (std::getline(stream, directive, ',')) {
        directive = trim(directive);
        std::string lower = to_lower(directive);

        if (lower == "no-cache") {
            cc.no_cache = true;
        } else if (lower == "no-store") {
            cc.no_store = true;
        } else if (lower == "must-revalidate") {
            cc.must_revalidate = true;
        } else if (lower == "public") {
            cc.is_public = true;
        } else if (lower == "private") {
            cc.is_private = true;
        } else if (lower.starts_with("max-age=")) {
            try {
                cc.max_age = std::stoi(lower.substr(8));
            } catch (...) {
                // ignore bad max-age
            }
        }
    }
    return cc;
}

bool should_cache_response(const Response& response, const CacheControl& cc) {
    if (response.status != 200) {
        return false;
    }
    if (cc.no_store || cc.is_private) {
        return false;
    }
    return true;
}

// ===========================================================================
// HttpCache implementation
// ===========================================================================

HttpCache& HttpCache::instance() {
    static HttpCache cache;
    return cache;
}

std::string HttpCache::make_cache_key(const std::string& url) {
    const auto parsed = clever::url::parse(url);
    if (!parsed.has_value()) {
        return make_cache_key_fallback(url);
    }

    clever::url::URL normalized = *parsed;
    std::transform(normalized.scheme.begin(), normalized.scheme.end(), normalized.scheme.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(normalized.host.begin(), normalized.host.end(), normalized.host.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    normalized.fragment.clear();
    if ((normalized.scheme == "http" || normalized.scheme == "https") && normalized.path.empty()) {
        normalized.path = "/";
    }
    if ((normalized.scheme == "http" && normalized.port == 80) ||
        (normalized.scheme == "https" && normalized.port == 443)) {
        normalized.port.reset();
    }
    return normalized.serialize();
}

std::optional<CacheEntry> HttpCache::lookup(const std::string& url) {
    const std::string cache_key = make_cache_key(url);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(cache_key);
    if (it == entries_.end()) return std::nullopt;

    // Move to front of LRU (most recently used)
    lru_.erase(it->second.lru_it);
    lru_.push_front(cache_key);
    it->second.lru_it = lru_.begin();

    return it->second.data;
}

void HttpCache::store(const CacheEntry& entry) {
    if (entry.body.size() > kMaxEntryBytes) return;  // Too large to cache
    if (entry.is_private) return;                     // Private responses stay out of shared cache

    CacheEntry normalized_entry = entry;
    normalized_entry.url = make_cache_key(entry.url);

    std::lock_guard<std::mutex> lock(mu_);

    // If already present, remove old entry size
    auto it = entries_.find(normalized_entry.url);
    if (it != entries_.end()) {
        current_bytes_ -= it->second.data.approx_size();
        lru_.erase(it->second.lru_it);
        entries_.erase(it);
    }

    // Add to LRU front
    lru_.push_front(normalized_entry.url);

    InternalEntry ie;
    ie.data = std::move(normalized_entry);
    ie.lru_it = lru_.begin();
    current_bytes_ += ie.data.approx_size();
    entries_.insert_or_assign(ie.data.url, std::move(ie));

    evict_if_needed();
}

void HttpCache::remove(const std::string& url) {
    const std::string cache_key = make_cache_key(url);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(cache_key);
    if (it != entries_.end()) {
        current_bytes_ -= it->second.data.approx_size();
        lru_.erase(it->second.lru_it);
        entries_.erase(it);
    }
}

void HttpCache::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    entries_.clear();
    lru_.clear();
    current_bytes_ = 0;
}

size_t HttpCache::total_size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return current_bytes_;
}

size_t HttpCache::entry_count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return entries_.size();
}

void HttpCache::set_max_bytes(size_t bytes) {
    std::lock_guard<std::mutex> lock(mu_);
    max_bytes_ = bytes;
    evict_if_needed();
}

void HttpCache::evict_if_needed() {
    // Called with mu_ held
    while (current_bytes_ > max_bytes_ && !lru_.empty()) {
        // Evict least recently used (back of list)
        const std::string& victim_url = lru_.back();
        auto it = entries_.find(victim_url);
        if (it != entries_.end()) {
            current_bytes_ -= it->second.data.approx_size();
            entries_.erase(it);
        }
        lru_.pop_back();
    }
}

// ===========================================================================
// HttpClient — response_from_cache helper
// ===========================================================================

Response HttpClient::response_from_cache(const CacheEntry& entry) {
    Response resp;
    resp.status = static_cast<uint16_t>(entry.status);
    resp.status_text = "OK";
    resp.body.assign(entry.body.begin(), entry.body.end());
    resp.url = entry.url;
    for (auto& [k, v] : entry.headers) {
        resp.headers.set(k, v);
    }
    return resp;
}

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() = default;

ConnectionPool& HttpClient::connection_pool() {
    static ConnectionPool pool(6, 30, std::chrono::seconds(60));
    return pool;
}

void HttpClient::set_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

void HttpClient::set_max_redirects(int max) {
    max_redirects_ = max;
}

int HttpClient::connect_to(const std::string& host, uint16_t port, const Request* request) {
    // Resolve hostname
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);

    struct addrinfo* result = nullptr;
    int rv = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (rv != 0 || result == nullptr) {
        return -1;
    }

    int fd = -1;
    for (auto* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (request && request_cancelled(*request)) {
            break;
        }
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (request) {
            set_request_active_fd(*request, fd);
        }

        // Set socket to non-blocking for connect timeout
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        rv = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rv == 0) {
            // Connected immediately -- restore blocking mode
            if (flags >= 0) {
                ::fcntl(fd, F_SETFL, flags);
            }
            break;
        }

        if (errno == EINPROGRESS) {
            // Wait for connection with timeout
            struct pollfd pfd {};
            pfd.fd = fd;
            pfd.events = POLLOUT;

            int poll_rv = ::poll(&pfd, 1, static_cast<int>(timeout_.count()));
            if (poll_rv > 0 && (pfd.revents & POLLOUT)) {
                // Check for socket error
                int sock_err = 0;
                socklen_t err_len = sizeof(sock_err);
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &sock_err, &err_len);
                if (sock_err == 0) {
                    // Connected successfully -- restore blocking mode
                    if (flags >= 0) {
                        ::fcntl(fd, F_SETFL, flags);
                    }
                    break;
                }
            }
        }

        // Failed, try next address
        if (request) {
            clear_request_active_fd(*request, fd);
        }
        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(result);
    return fd;
}

bool HttpClient::send_all(int fd, const uint8_t* data, size_t len, const Request& request) {
    size_t sent = 0;
    while (sent < len) {
        if (request_cancelled(request)) {
            return false;
        }
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool HttpClient::has_complete_response(const std::vector<uint8_t>& buffer) {
    const char* sep = "\r\n\r\n";
    const auto header_it = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
    if (header_it == buffer.end()) {
        return false;
    }

    const size_t header_end = static_cast<size_t>(header_it - buffer.begin()) + 4;
    std::string header_str(buffer.begin(),
                           buffer.begin() + static_cast<std::ptrdiff_t>(header_end));
    std::string lower_header = header_str;
    std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const std::string cl_header = "content-length:";
    const auto cl_pos = lower_header.find(cl_header);
    if (cl_pos != std::string::npos) {
        const auto val_start = cl_pos + cl_header.size();
        const auto val_end = lower_header.find("\r\n", val_start);
        if (val_end != std::string::npos) {
            std::string_view value(header_str.data() + static_cast<std::ptrdiff_t>(val_start),
                                   val_end - val_start);
            value = trim_ascii_whitespace(value);
            try {
                const size_t content_length = std::stoull(std::string(value));
                return buffer.size() >= header_end + content_length;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    if (lower_header.find("transfer-encoding: chunked") != std::string::npos ||
        lower_header.find("transfer-encoding:chunked") != std::string::npos) {
        return is_complete_chunked_body(buffer, header_end);
    }

    return false;
}

std::optional<std::vector<uint8_t>> HttpClient::recv_response(int fd, const Request& request) {
    std::vector<uint8_t> buffer;
    constexpr size_t kChunkSize = 8192;

    // Set a receive timeout using poll
    auto deadline = std::chrono::steady_clock::now() + timeout_;

    while (true) {
        if (request_cancelled(request)) {
            return std::nullopt;
        }
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            // Timed out -- return what we have if anything
            break;
        }

        struct pollfd pfd {};
        pfd.fd = fd;
        pfd.events = POLLIN;

        int poll_rv = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
        if (poll_rv <= 0) {
            break;  // Timeout or error
        }
        if (request_cancelled(request)) {
            return std::nullopt;
        }

        uint8_t chunk[kChunkSize];
        ssize_t n = ::recv(fd, chunk, kChunkSize, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) {
            // Connection closed by peer
            break;
        }

        buffer.insert(buffer.end(), chunk, chunk + n);

        if (has_complete_response(buffer)) {
            return buffer;
        }
    }

    if (buffer.empty()) {
        return std::nullopt;
    }
    return buffer;
}

std::optional<Response> HttpClient::do_request(const Request& request) {
    if (request_cancelled(request)) {
        return std::nullopt;
    }
    // Serialize the HTTP request bytes
    auto data = request.serialize();

    if (request.use_tls) {
        std::unique_ptr<TlsSocket> tls;
        int fd = -1;

        for (int attempt = 0; attempt < 2; ++attempt) {
            bool is_reused = false;

            if (attempt == 0) {
                tls = acquire_tls_conn(request.host, request.port, fd);
                is_reused = (tls != nullptr);
                if (fd >= 0) {
                    set_request_active_fd(request, fd);
                }

                if (!tls) {
                    fd = connect_to(request.host, request.port, &request);
                    if (fd < 0) {
                        return std::nullopt;
                    }

                    tls = std::make_unique<TlsSocket>();
                    if (!tls->connect(request.host, request.port, fd)) {
                        clear_request_active_fd(request, fd);
                        ::close(fd);
                        return std::nullopt;
                    }
                } else if (!tls->is_connected()) {
                    clear_request_active_fd(request, fd);
                    tls->close();
                    ::close(fd);
                    fd = -1;
                    if (is_reused) {
                        continue;
                    }
                    return std::nullopt;
                }
            } else {
                is_reused = false;
                if (fd >= 0) {
                    clear_request_active_fd(request, fd);
                    tls->close();
                    ::close(fd);
                    tls.reset();
                }

                fd = connect_to(request.host, request.port, &request);
                if (fd < 0) {
                    return std::nullopt;
                }

                tls = std::make_unique<TlsSocket>();
                if (!tls->connect(request.host, request.port, fd)) {
                    clear_request_active_fd(request, fd);
                    ::close(fd);
                    return std::nullopt;
                }
            }

            // Send request through TLS
            if (request_cancelled(request) || !tls->send(data.data(), data.size())) {
                if (is_reused && attempt == 0) {
                    clear_request_active_fd(request, fd);
                    tls->close();
                    ::close(fd);
                    continue;
                }
                clear_request_active_fd(request, fd);
                tls->close();
                ::close(fd);
                return std::nullopt;
            }

            // Receive response through TLS
            std::vector<uint8_t> buffer;
            auto deadline = std::chrono::steady_clock::now() + timeout_;

            while (true) {
                if (request_cancelled(request)) {
                    buffer.clear();
                    break;
                }
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());
                if (remaining.count() <= 0) {
                    break;  // Timed out
                }

                // Use poll to wait for data on the underlying fd
                struct pollfd pfd {};
                pfd.fd = fd;
                pfd.events = POLLIN;

                int poll_rv = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
                if (poll_rv < 0) {
                    if (errno == EINTR) continue;
                    break;  // Error
                }
                if (request_cancelled(request)) {
                    buffer.clear();
                    break;
                }

                auto chunk = tls->recv();
                if (!chunk.has_value()) {
                    break;  // TLS error
                }
                if (chunk->empty()) {
                    if (poll_rv == 0) {
                        // poll timed out and no TLS data — check if we already
                        // have enough data to parse (Connection: close case)
                        if (has_complete_response(buffer)) break;
                        if (!buffer.empty() && !has_explicit_response_framing(buffer)) break;
                        continue;  // Keep waiting
                    }
                    // errSSLWouldBlock or graceful close — if we have a
                    // complete response in the buffer, return it
                    if (has_complete_response(buffer)) {
                        break;
                    }
                    if (!buffer.empty()) {
                        const char* sep = "\r\n\r\n";
                        auto it2 = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
                        if (it2 != buffer.end() && !has_explicit_response_framing(buffer)) {
                            break;  // Connection-close response
                        }
                    }
                    // No data yet, connection may have closed
                    break;
                }

                buffer.insert(buffer.end(), chunk->begin(), chunk->end());

                if (has_complete_response(buffer)) {
                    break;
                }
            }

            if (buffer.empty()) {
                if (is_reused && attempt == 0) {
                    clear_request_active_fd(request, fd);
                    tls->close();
                    ::close(fd);
                    continue;
                }
                clear_request_active_fd(request, fd);
                tls->close();
                ::close(fd);
                return std::nullopt;
            }

            auto resp = Response::parse(buffer);
            if (!resp.has_value()) {
                clear_request_active_fd(request, fd);
                tls->close();
                ::close(fd);
                return std::nullopt;
            }

            bool is_http11 = false;
            const char* crlf = "\r\n";
            auto status_line_end = std::search(buffer.begin(), buffer.end(), crlf, crlf + 2);
            if (status_line_end != buffer.end()) {
                std::string status_line(buffer.begin(), status_line_end);
                std::string lower_status_line = to_lower(status_line);
                is_http11 = lower_status_line.starts_with("http/1.1");
            }

            // Decide keep-alive behavior:
            // - Connection: close => close
            // - Connection: keep-alive => keep
            // - otherwise default keep for HTTP/1.1, close for older versions
            auto conn_header = resp->headers.get("connection");
            bool server_wants_close = false;
            bool server_explicit_keep_alive = false;
            if (conn_header.has_value()) {
                std::istringstream stream(*conn_header);
                std::string token;
                while (std::getline(stream, token, ',')) {
                    std::string lower_token = to_lower(trim(token));
                    if (lower_token == "close") {
                        server_wants_close = true;
                    } else if (lower_token == "keep-alive") {
                        server_explicit_keep_alive = true;
                    }
                }
            }

            bool should_keep_alive = false;
            if (!server_wants_close) {
                should_keep_alive = server_explicit_keep_alive || is_http11;
            }

            if (!should_keep_alive) {
                clear_request_active_fd(request, fd);
                tls->close();
                ::close(fd);
            } else {
                clear_request_active_fd(request, fd);
                release_tls_conn(request.host, request.port, std::move(tls), fd);
            }

            return resp;
        }

        return std::nullopt;

    } else {
        // ---- Plain HTTP path with keep-alive connection pooling ----
        auto& pool = connection_pool();

        // Try to reuse a pooled connection first
        int fd = pool.acquire(request.host, request.port);
        bool reused = (fd >= 0);
        if (fd >= 0) {
            set_request_active_fd(request, fd);
        }

        if (!reused) {
            fd = connect_to(request.host, request.port, &request);
            if (fd < 0) {
                return std::nullopt;
            }
        }

        if (!send_all(fd, data.data(), data.size(), request)) {
            clear_request_active_fd(request, fd);
            ::close(fd);
            // If we reused a stale connection, retry with a fresh one
            if (reused) {
                fd = connect_to(request.host, request.port, &request);
                if (fd < 0) return std::nullopt;
                if (!send_all(fd, data.data(), data.size(), request)) {
                    clear_request_active_fd(request, fd);
                    ::close(fd);
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }

        auto raw = recv_response(fd, request);

        if (!raw.has_value()) {
            clear_request_active_fd(request, fd);
            ::close(fd);
            // If we reused a stale connection, retry with a fresh one
            if (reused) {
                fd = connect_to(request.host, request.port, &request);
                if (fd < 0) return std::nullopt;
                if (!send_all(fd, data.data(), data.size(), request)) {
                    clear_request_active_fd(request, fd);
                    ::close(fd);
                    return std::nullopt;
                }
                raw = recv_response(fd, request);
                if (!raw.has_value()) {
                    clear_request_active_fd(request, fd);
                    ::close(fd);
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }

        auto resp = Response::parse(*raw);
        if (!resp.has_value()) {
            clear_request_active_fd(request, fd);
            ::close(fd);
            return std::nullopt;
        }

        bool is_http11 = false;
        const char* crlf = "\r\n";
        auto status_line_end = std::search(raw->begin(), raw->end(), crlf, crlf + 2);
        if (status_line_end != raw->end()) {
            std::string status_line(raw->begin(), status_line_end);
            std::string lower_status_line = to_lower(status_line);
            is_http11 = lower_status_line.starts_with("http/1.1");
        }

        // Decide keep-alive behavior:
        // - Connection: close => close
        // - Connection: keep-alive => keep
        // - otherwise default keep for HTTP/1.1, close for older versions
        auto conn_header = resp->headers.get("connection");
        bool server_wants_close = false;
        bool server_explicit_keep_alive = false;
        if (conn_header.has_value()) {
            std::istringstream stream(*conn_header);
            std::string token;
            while (std::getline(stream, token, ',')) {
                std::string lower_token = to_lower(trim(token));
                if (lower_token == "close") {
                    server_wants_close = true;
                } else if (lower_token == "keep-alive") {
                    server_explicit_keep_alive = true;
                }
            }
        }

        bool should_keep_alive = false;
        if (!server_wants_close) {
            should_keep_alive = server_explicit_keep_alive || is_http11;
        }

        if (!should_keep_alive) {
            clear_request_active_fd(request, fd);
            ::close(fd);
        } else {
            // Return connection to pool for reuse
            clear_request_active_fd(request, fd);
            pool.release(request.host, request.port, fd);
        }

        return resp;
    }
}

std::optional<Response> HttpClient::fetch(const Request& request) {
    Request current = request;

    // Ensure URL is parsed
    if (current.host.empty() && !current.url.empty()) {
        current.parse_url();
    }

    // --- Cache: only cache GET requests ---
    const bool is_cacheable_method = (current.method == Method::GET);
    auto& cache = HttpCache::instance();
    const std::string request_cache_key = is_cacheable_method ? HttpCache::make_cache_key(current.url)
                                                              : std::string{};

    if (is_cacheable_method) {
        auto cached = cache.lookup(request_cache_key);
        if (cached.has_value()) {
            // If no-store was set, we should not have stored it, but double check
            if (!cached->no_store) {
                if (cached->is_fresh()) {
                    // Fresh cache hit — return immediately without network
                    auto resp = response_from_cache(*cached);
                    resp.url = current.url;
                    resp.was_redirected = false;
                    return resp;
                }

                // Stale — send conditional request
                if (!cached->etag.empty()) {
                    current.headers.set("If-None-Match", cached->etag);
                }
                if (!cached->last_modified.empty()) {
                    current.headers.set("If-Modified-Since", cached->last_modified);
                }
            }
        }
    }

    int redirects = 0;
    bool was_redirected = false;

    while (true) {
        auto resp = do_request(current);
        if (!resp.has_value()) {
            return std::nullopt;
        }

        resp->url = current.url;
        resp->was_redirected = was_redirected;

        for (auto it = resp->headers.begin(); it != resp->headers.end(); ++it) {
            if (to_lower(it->first) != "set-cookie") continue;
            const std::string& raw_value = it->second;
            if (raw_value.empty()) continue;

            size_t segment_start = 0;
            for (size_t i = 0; i < raw_value.size(); ++i) {
                if (raw_value[i] != ',') continue;

                size_t lookahead = i + 1;
                while (lookahead < raw_value.size() &&
                       (raw_value[lookahead] == ' ' || raw_value[lookahead] == '\t')) {
                    ++lookahead;
                }

                bool is_cookie_boundary = false;
                if (lookahead < raw_value.size()) {
                    size_t eq = raw_value.find('=', lookahead);
                    size_t semi = raw_value.find(';', lookahead);
                    if (eq != std::string::npos && (semi == std::string::npos || eq < semi)) {
                        std::string token = raw_value.substr(lookahead, eq - lookahead);
                        if (!token.empty() && token.find_first_of(" \t") == std::string::npos) {
                            is_cookie_boundary = true;
                        }
                    }
                }

                if (!is_cookie_boundary) continue;

                std::string one_cookie = trim(raw_value.substr(segment_start, i - segment_start));
                if (!one_cookie.empty()) {
                    CookieJar::shared().set_from_header(one_cookie, current.host, current.path);
                }
                segment_start = i + 1;
            }

            std::string trailing_cookie = trim(raw_value.substr(segment_start));
            if (!trailing_cookie.empty()) {
                CookieJar::shared().set_from_header(trailing_cookie, current.host, current.path);
            }
        }

        // --- Handle 304 Not Modified ---
        if (resp->status == 304 && is_cacheable_method) {
            auto cached = cache.lookup(request_cache_key);
            if (cached.has_value()) {
                // Refresh the stored_at time (entry is still valid)
                CacheEntry refreshed = *cached;
                refreshed.stored_at = std::chrono::steady_clock::now();

                // Update cache-control from the 304 response if present
                auto cc_header = resp->headers.get("cache-control");
                if (cc_header.has_value()) {
                    auto cc = parse_cache_control(*cc_header);
                    if (cc.max_age >= 0) refreshed.max_age_seconds = cc.max_age;
                    refreshed.no_cache = cc.no_cache;
                    refreshed.must_revalidate = cc.must_revalidate;
                }

                cache.store(refreshed);
                auto result = response_from_cache(refreshed);
                result.was_redirected = was_redirected;
                return result;
            }
        }

        // Handle redirects (301, 302, 303, 307, 308)
        if ((resp->status == 301 || resp->status == 302 ||
             resp->status == 303 || resp->status == 307 ||
             resp->status == 308) && redirects < max_redirects_) {

            auto location = resp->headers.get("location");
            if (!location.has_value()) {
                return resp;  // No Location header, return as-is
            }

            const auto new_url = resolve_redirect_url(current, *location);
            if (!new_url.has_value()) {
                return resp;
            }

            const bool rewrite_post_redirect_to_get =
                current.method == Method::POST &&
                (resp->status == 301 || resp->status == 302 || resp->status == 303);
            const bool rewrite_see_other_to_get =
                resp->status == 303 && current.method != Method::HEAD;
            if (rewrite_post_redirect_to_get || rewrite_see_other_to_get) {
                current.method = Method::GET;
                current.body.clear();
                current.headers.remove("content-length");
                current.headers.remove("content-type");
                current.headers.remove("content-encoding");
                current.headers.remove("content-language");
                current.headers.remove("content-location");
                current.headers.remove("transfer-encoding");
            }

            current.url = *new_url;
            current.parse_url();
            ++redirects;
            was_redirected = true;
            continue;
        }

        // --- Cache storage for 200 OK responses ---
        if (resp->status == 200 && is_cacheable_method) {
            auto cc_header = resp->headers.get("cache-control");
            CacheControl cc;
            if (cc_header.has_value()) {
                cc = parse_cache_control(*cc_header);
            }

            if (should_cache_response(*resp, cc)) {
                auto etag = resp->headers.get("etag");
                auto last_mod = resp->headers.get("last-modified");

                // Only cache if there is at least one caching signal
                if (cc.max_age >= 0 || etag.has_value() || last_mod.has_value() ||
                    cc.no_cache || cc.must_revalidate) {
                    std::string body_str = resp->body_as_string();

                    // Don't cache bodies > 10 MB
                    if (body_str.size() <= HttpCache::kMaxEntryBytes) {
                        CacheEntry entry;
                        entry.url = request_cache_key;
                        entry.etag = etag.value_or("");
                        entry.last_modified = last_mod.value_or("");
                        entry.body = std::move(body_str);
                        entry.status = resp->status;
                        entry.stored_at = std::chrono::steady_clock::now();
                        entry.max_age_seconds = (cc.max_age >= 0) ? cc.max_age : 0;
                        entry.no_cache = cc.no_cache;
                        entry.no_store = cc.no_store;
                        entry.must_revalidate = cc.must_revalidate;
                        entry.is_private = cc.is_private;

                        // Store response headers
                        for (auto& [k, v] : resp->headers) {
                            entry.headers[k] = v;
                        }

                        cache.store(entry);
                    }
                }
            }
        }

        return resp;
    }
}

} // namespace clever::net
