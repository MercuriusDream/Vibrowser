#pragma once
#include <clever/net/request.h>
#include <clever/net/response.h>
#include <clever/net/tls_socket.h>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace clever::net {

// ---------------------------------------------------------------------------
// HTTP Cache Entry
// ---------------------------------------------------------------------------
struct CacheEntry {
    std::string url;
    std::string etag;            // ETag header value
    std::string last_modified;   // Last-Modified header value
    std::string body;            // cached response body (as string)
    std::map<std::string, std::string> headers; // cached response headers
    int status = 200;
    std::chrono::steady_clock::time_point stored_at;
    int max_age_seconds = 0;     // from Cache-Control: max-age=N
    bool no_cache = false;       // Cache-Control: no-cache (must revalidate)
    bool no_store = false;       // Cache-Control: no-store
    bool must_revalidate = false;// Cache-Control: must-revalidate
    bool is_private = false;     // Cache-Control: private

    // Approximate size in bytes (for LRU eviction budget)
    size_t approx_size() const;

    // Is this entry fresh (within max-age)?
    bool is_fresh() const;
};

// ---------------------------------------------------------------------------
// Cache-Control directive container (parsed from header value)
// ---------------------------------------------------------------------------
struct CacheControl {
    int max_age = -1;            // -1 means not present
    bool no_cache = false;
    bool no_store = false;
    bool must_revalidate = false;
    bool is_public = false;
    bool is_private = false;
};

// Parse a Cache-Control header value string into directives.
CacheControl parse_cache_control(const std::string& value);

// Determine whether the response should be stored in the shared HTTP cache.
bool should_cache_response(const Response& response, const CacheControl& cc);

// ---------------------------------------------------------------------------
// HTTP Cache (process-static, thread-safe via mutex)
// ---------------------------------------------------------------------------
class HttpCache {
public:
    static constexpr size_t kDefaultMaxBytes = 50 * 1024 * 1024;  // 50 MB
    static constexpr size_t kMaxEntryBytes   = 10 * 1024 * 1024;  // 10 MB per entry

    // Get the process-wide singleton cache.
    static HttpCache& instance();

    // Look up a cached response for the given URL. Returns nullopt if miss.
    std::optional<CacheEntry> lookup(const std::string& url);

    // Store (or update) a cache entry.
    void store(const CacheEntry& entry);

    // Remove a cache entry.
    void remove(const std::string& url);

    // Clear the entire cache.
    void clear();

    // Current total approximate size in bytes.
    size_t total_size() const;

    // Number of entries.
    size_t entry_count() const;

    // Set the max cache budget (for testing).
    void set_max_bytes(size_t bytes);

private:
    HttpCache() = default;

    void evict_if_needed();

    mutable std::mutex mu_;
    size_t max_bytes_ = kDefaultMaxBytes;
    size_t current_bytes_ = 0;

    // LRU list: front = most recently used, back = least recently used
    using LruList = std::list<std::string>;   // list of URLs
    LruList lru_;

    struct InternalEntry {
        CacheEntry data;
        LruList::iterator lru_it;
    };
    std::unordered_map<std::string, InternalEntry> entries_;
};

// ---------------------------------------------------------------------------
// HttpClient
// ---------------------------------------------------------------------------
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Synchronous fetch (blocking). Uses HTTP cache when appropriate.
    std::optional<Response> fetch(const Request& request);

    // Set connection timeout
    void set_timeout(std::chrono::milliseconds timeout);

    // Set max redirect count
    void set_max_redirects(int max);

private:
    std::chrono::milliseconds timeout_{std::chrono::seconds(30)};
    int max_redirects_ = 20;

    // Low-level: connect, send request, read response
    std::optional<Response> do_request(const Request& request);
    int connect_to(const std::string& host, uint16_t port);
    bool send_all(int fd, const uint8_t* data, size_t len);
    std::optional<std::vector<uint8_t>> recv_response(int fd);

    // Build a Response from a CacheEntry
    static Response response_from_cache(const CacheEntry& entry);
};

} // namespace clever::net
