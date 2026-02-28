#include <clever/net/http_client.h>

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
#include <sstream>
#include <string>

namespace clever::net {

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

std::optional<CacheEntry> HttpCache::lookup(const std::string& url) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(url);
    if (it == entries_.end()) return std::nullopt;

    // Move to front of LRU (most recently used)
    lru_.erase(it->second.lru_it);
    lru_.push_front(url);
    it->second.lru_it = lru_.begin();

    return it->second.data;
}

void HttpCache::store(const CacheEntry& entry) {
    if (entry.body.size() > kMaxEntryBytes) return;  // Too large to cache
    if (entry.is_private) return;                     // Private responses stay out of shared cache

    std::lock_guard<std::mutex> lock(mu_);

    // If already present, remove old entry size
    auto it = entries_.find(entry.url);
    if (it != entries_.end()) {
        current_bytes_ -= it->second.data.approx_size();
        lru_.erase(it->second.lru_it);
        entries_.erase(it);
    }

    // Add to LRU front
    lru_.push_front(entry.url);

    InternalEntry ie;
    ie.data = entry;
    ie.lru_it = lru_.begin();
    current_bytes_ += entry.approx_size();
    entries_.insert_or_assign(entry.url, std::move(ie));

    evict_if_needed();
}

void HttpCache::remove(const std::string& url) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = entries_.find(url);
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

int HttpClient::connect_to(const std::string& host, uint16_t port) {
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
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

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
        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(result);
    return fd;
}

bool HttpClient::send_all(int fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
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

std::optional<std::vector<uint8_t>> HttpClient::recv_response(int fd) {
    std::vector<uint8_t> buffer;
    constexpr size_t kChunkSize = 8192;

    // Set a receive timeout using poll
    auto deadline = std::chrono::steady_clock::now() + timeout_;

    while (true) {
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

        // Check if we have a complete response
        // First, find end of headers
        const char* sep = "\r\n\r\n";
        auto it = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
        if (it == buffer.end()) {
            continue;  // Headers not yet complete
        }

        size_t header_end = static_cast<size_t>(it - buffer.begin()) + 4;

        // Check for Content-Length
        std::string header_str(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(header_end));

        // Case-insensitive search for Content-Length
        std::string lower_header = header_str;
        std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::string cl_header = "content-length:";
        auto cl_pos = lower_header.find(cl_header);
        if (cl_pos != std::string::npos) {
            auto val_start = cl_pos + cl_header.size();
            auto val_end = lower_header.find("\r\n", val_start);
            if (val_end != std::string::npos) {
                std::string val = header_str.substr(val_start, val_end - val_start);
                // Trim whitespace
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                try {
                    size_t content_length = std::stoull(val);
                    if (buffer.size() >= header_end + content_length) {
                        return buffer;  // Complete response
                    }
                } catch (...) {
                    // Fall through
                }
            }
            continue;  // Need more data
        }

        // Check for Transfer-Encoding: chunked
        if (lower_header.find("transfer-encoding: chunked") != std::string::npos ||
            lower_header.find("transfer-encoding:chunked") != std::string::npos) {
            // Look for the final chunk marker: "\r\n0\r\n\r\n"
            // (the last chunk "0\r\n" followed by empty trailer "\r\n")
            const char* end7 = "\r\n0\r\n\r\n";
            auto end_it = std::search(buffer.begin() + static_cast<std::ptrdiff_t>(header_end),
                                       buffer.end(),
                                       end7, end7 + 7);
            if (end_it != buffer.end()) {
                return buffer;
            }
            // Also check "0\r\n\r\n" at the very start of body (first chunk is empty/zero)
            const char* end5 = "0\r\n\r\n";
            auto body_start = buffer.begin() + static_cast<std::ptrdiff_t>(header_end);
            if (std::distance(body_start, buffer.end()) >= 5 &&
                std::equal(end5, end5 + 5, body_start)) {
                return buffer;
            }
            continue;
        }

        // No Content-Length and not chunked: keep reading until connection closes
    }

    if (buffer.empty()) {
        return std::nullopt;
    }
    return buffer;
}

std::optional<Response> HttpClient::do_request(const Request& request) {
    // Serialize the HTTP request bytes
    auto data = request.serialize();

    if (request.use_tls) {
        // ---- TLS path (no connection pooling) ----
        int fd = connect_to(request.host, request.port);
        if (fd < 0) {
            return std::nullopt;
        }

        TlsSocket tls;
        if (!tls.connect(request.host, request.port, fd)) {
            ::close(fd);
            return std::nullopt;
        }

        // Send request through TLS
        if (!tls.send(data.data(), data.size())) {
            tls.close();
            ::close(fd);
            return std::nullopt;
        }

        // Receive response through TLS
        std::vector<uint8_t> buffer;
        auto deadline = std::chrono::steady_clock::now() + timeout_;

        while (true) {
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

            auto chunk = tls.recv();
            if (!chunk.has_value()) {
                break;  // TLS error
            }
            if (chunk->empty()) {
                if (poll_rv == 0) {
                    // poll timed out and no TLS data — check if we already
                    // have enough data to parse (Connection: close case)
                    if (!buffer.empty()) break;
                    continue;  // Keep waiting
                }
                // errSSLWouldBlock or graceful close — if we have a
                // complete response in the buffer, return it
                if (!buffer.empty()) {
                    const char* sep = "\r\n\r\n";
                    auto it2 = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
                    if (it2 != buffer.end()) break;  // Have headers, done
                }
                // No data yet, connection may have closed
                break;
            }

            buffer.insert(buffer.end(), chunk->begin(), chunk->end());

            // Check if we have a complete response
            const char* sep = "\r\n\r\n";
            auto it = std::search(buffer.begin(), buffer.end(), sep, sep + 4);
            if (it == buffer.end()) {
                continue;  // Headers not yet complete
            }

            size_t header_end = static_cast<size_t>(it - buffer.begin()) + 4;

            // Check for Content-Length
            std::string header_str(buffer.begin(),
                                   buffer.begin() + static_cast<std::ptrdiff_t>(header_end));
            std::string lower_header = header_str;
            std::transform(lower_header.begin(), lower_header.end(),
                           lower_header.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            std::string cl_header = "content-length:";
            auto cl_pos = lower_header.find(cl_header);
            if (cl_pos != std::string::npos) {
                auto val_start = cl_pos + cl_header.size();
                auto val_end = lower_header.find("\r\n", val_start);
                if (val_end != std::string::npos) {
                    std::string val = header_str.substr(val_start, val_end - val_start);
                    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                    try {
                        size_t content_length = std::stoull(val);
                        if (buffer.size() >= header_end + content_length) {
                            tls.close();
                            ::close(fd);
                            return Response::parse(buffer);
                        }
                    } catch (...) {
                        // Fall through
                    }
                }
                continue;
            }

            // Check for Transfer-Encoding: chunked
            if (lower_header.find("transfer-encoding: chunked") != std::string::npos ||
                lower_header.find("transfer-encoding:chunked") != std::string::npos) {
                const char* end7 = "\r\n0\r\n\r\n";
                auto end_it = std::search(
                    buffer.begin() + static_cast<std::ptrdiff_t>(header_end),
                    buffer.end(), end7, end7 + 7);
                if (end_it != buffer.end()) {
                    tls.close();
                    ::close(fd);
                    return Response::parse(buffer);
                }
                // Also check "0\r\n\r\n" at very start of body
                const char* end5 = "0\r\n\r\n";
                auto body_start = buffer.begin() + static_cast<std::ptrdiff_t>(header_end);
                if (std::distance(body_start, buffer.end()) >= 5 &&
                    std::equal(end5, end5 + 5, body_start)) {
                    tls.close();
                    ::close(fd);
                    return Response::parse(buffer);
                }
                continue;
            }

            // No Content-Length and not chunked — keep reading until connection close
        }

        tls.close();
        ::close(fd);

        if (buffer.empty()) {
            return std::nullopt;
        }
        return Response::parse(buffer);

    } else {
        // ---- Plain HTTP path with keep-alive connection pooling ----
        auto& pool = connection_pool();

        // Try to reuse a pooled connection first
        int fd = pool.acquire(request.host, request.port);
        bool reused = (fd >= 0);

        if (!reused) {
            fd = connect_to(request.host, request.port);
            if (fd < 0) {
                return std::nullopt;
            }
        }

        if (!send_all(fd, data.data(), data.size())) {
            ::close(fd);
            // If we reused a stale connection, retry with a fresh one
            if (reused) {
                fd = connect_to(request.host, request.port);
                if (fd < 0) return std::nullopt;
                if (!send_all(fd, data.data(), data.size())) {
                    ::close(fd);
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }

        auto raw = recv_response(fd);

        if (!raw.has_value()) {
            ::close(fd);
            // If we reused a stale connection, retry with a fresh one
            if (reused) {
                fd = connect_to(request.host, request.port);
                if (fd < 0) return std::nullopt;
                if (!send_all(fd, data.data(), data.size())) {
                    ::close(fd);
                    return std::nullopt;
                }
                raw = recv_response(fd);
                if (!raw.has_value()) {
                    ::close(fd);
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }

        auto resp = Response::parse(*raw);
        if (!resp.has_value()) {
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
            ::close(fd);
        } else {
            // Return connection to pool for reuse
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

    if (is_cacheable_method) {
        auto cached = cache.lookup(current.url);
        if (cached.has_value()) {
            // If no-store was set, we should not have stored it, but double check
            if (!cached->no_store) {
                if (cached->is_fresh()) {
                    // Fresh cache hit — return immediately without network
                    auto resp = response_from_cache(*cached);
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

        // --- Handle 304 Not Modified ---
        if (resp->status == 304 && is_cacheable_method) {
            auto cached = cache.lookup(current.url);
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

            std::string new_url = *location;

            // Handle relative URLs
            if (new_url.find("://") == std::string::npos) {
                // Relative URL: prepend scheme + host
                std::string scheme = (current.port == 443) ? "https://" : "http://";
                if (new_url.front() != '/') {
                    new_url = "/" + new_url;
                }
                new_url = scheme + current.host + new_url;
            }

            // For 303, change method to GET
            if (resp->status == 303) {
                current.method = Method::GET;
                current.body.clear();
            }

            current.url = new_url;
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
                        entry.url = current.url;
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
