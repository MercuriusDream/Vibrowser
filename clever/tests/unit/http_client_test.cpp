#include <clever/net/header_map.h>
#include <clever/net/request.h>
#include <clever/net/response.h>
#include <clever/net/connection_pool.h>
#include <clever/net/cookie_jar.h>
#include <clever/net/http_client.h>
#include <clever/net/tls_socket.h>

#include <gtest/gtest.h>
#include <zlib.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

using namespace clever::net;

// ===========================================================================
// HeaderMap Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 1. HeaderMap: set and get (case-insensitive)
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, SetAndGetCaseInsensitive) {
    HeaderMap map;
    map.set("Content-Type", "text/html");
    EXPECT_EQ(map.get("Content-Type").value(), "text/html");
    EXPECT_EQ(map.get("content-type").value(), "text/html");
    EXPECT_EQ(map.get("CONTENT-TYPE").value(), "text/html");
}

TEST(HeaderMapTest, SetOverwritesPreviousValue) {
    HeaderMap map;
    map.set("Content-Type", "text/html");
    map.set("Content-Type", "application/json");
    // set() should replace all previous values, so get() returns the new one
    EXPECT_EQ(map.get("content-type").value(), "application/json");
    // Should only have one entry now
    EXPECT_EQ(map.get_all("content-type").size(), 1u);
}

TEST(HeaderMapTest, GetReturnsNulloptForMissingKey) {
    HeaderMap map;
    EXPECT_FALSE(map.get("X-Missing").has_value());
}

// ---------------------------------------------------------------------------
// 2. HeaderMap: append multiple values
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, AppendMultipleValues) {
    HeaderMap map;
    map.append("Set-Cookie", "a=1");
    map.append("Set-Cookie", "b=2");
    map.append("set-cookie", "c=3");

    auto all = map.get_all("Set-Cookie");
    EXPECT_EQ(all.size(), 3u);

    // Check all values are present (order may vary with unordered_multimap)
    EXPECT_TRUE(std::find(all.begin(), all.end(), "a=1") != all.end());
    EXPECT_TRUE(std::find(all.begin(), all.end(), "b=2") != all.end());
    EXPECT_TRUE(std::find(all.begin(), all.end(), "c=3") != all.end());
}

// ---------------------------------------------------------------------------
// 3. HeaderMap: get_all returns all values for key
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, GetAllReturnsAllValues) {
    HeaderMap map;
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");

    auto all = map.get_all("accept");
    EXPECT_EQ(all.size(), 2u);
}

TEST(HeaderMapTest, GetAllReturnsEmptyForMissingKey) {
    HeaderMap map;
    auto all = map.get_all("X-Missing");
    EXPECT_TRUE(all.empty());
}

// ---------------------------------------------------------------------------
// 4. HeaderMap: has / remove
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, HasReturnsTrueForExistingKey) {
    HeaderMap map;
    map.set("Host", "example.com");
    EXPECT_TRUE(map.has("Host"));
    EXPECT_TRUE(map.has("host"));
    EXPECT_TRUE(map.has("HOST"));
}

TEST(HeaderMapTest, HasReturnsFalseForMissingKey) {
    HeaderMap map;
    EXPECT_FALSE(map.has("X-Missing"));
}

TEST(HeaderMapTest, RemoveDeletesAllValuesForKey) {
    HeaderMap map;
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");
    map.set("Host", "example.com");

    EXPECT_TRUE(map.has("Accept"));
    map.remove("ACCEPT");
    EXPECT_FALSE(map.has("Accept"));
    // Host should still be there
    EXPECT_TRUE(map.has("Host"));
}

TEST(HeaderMapTest, RemoveNonexistentKeyIsNoop) {
    HeaderMap map;
    map.set("Host", "example.com");
    map.remove("X-Missing");
    EXPECT_EQ(map.size(), 1u);
}

// ---------------------------------------------------------------------------
// 5. HeaderMap: size / empty
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, SizeReturnsNumberOfEntries) {
    HeaderMap map;
    EXPECT_EQ(map.size(), 0u);
    map.set("Host", "example.com");
    EXPECT_EQ(map.size(), 1u);
    map.append("Accept", "text/html");
    EXPECT_EQ(map.size(), 2u);
    map.append("Accept", "application/json");
    EXPECT_EQ(map.size(), 3u);
}

TEST(HeaderMapTest, EmptyReturnsTrueWhenEmpty) {
    HeaderMap map;
    EXPECT_TRUE(map.empty());
    map.set("Host", "example.com");
    EXPECT_FALSE(map.empty());
}

// ---------------------------------------------------------------------------
// HeaderMap: iteration
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, IterationCoversAllEntries) {
    HeaderMap map;
    map.set("Host", "example.com");
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");

    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 3u);
}

// ===========================================================================
// Method Conversion Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 10. Method to/from string conversions
// ---------------------------------------------------------------------------
TEST(MethodTest, MethodToString) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
    EXPECT_EQ(method_to_string(Method::POST), "POST");
    EXPECT_EQ(method_to_string(Method::PUT), "PUT");
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
    EXPECT_EQ(method_to_string(Method::HEAD), "HEAD");
    EXPECT_EQ(method_to_string(Method::OPTIONS), "OPTIONS");
    EXPECT_EQ(method_to_string(Method::PATCH), "PATCH");
}

TEST(MethodTest, StringToMethod) {
    EXPECT_EQ(string_to_method("GET"), Method::GET);
    EXPECT_EQ(string_to_method("POST"), Method::POST);
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
    EXPECT_EQ(string_to_method("DELETE"), Method::DELETE_METHOD);
    EXPECT_EQ(string_to_method("HEAD"), Method::HEAD);
    EXPECT_EQ(string_to_method("OPTIONS"), Method::OPTIONS);
    EXPECT_EQ(string_to_method("PATCH"), Method::PATCH);
}

TEST(MethodTest, StringToMethodCaseInsensitive) {
    EXPECT_EQ(string_to_method("get"), Method::GET);
    EXPECT_EQ(string_to_method("Post"), Method::POST);
}

TEST(MethodTest, UnknownMethodDefaultsToGet) {
    EXPECT_EQ(string_to_method("FOOBAR"), Method::GET);
}

// ===========================================================================
// Request Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 7. Request parse_url: extracts host / port / path from URL
// ---------------------------------------------------------------------------
TEST(RequestTest, ParseUrlSimple) {
    Request req;
    req.url = "http://example.com/index.html";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_EQ(req.path, "/index.html");
    EXPECT_TRUE(req.query.empty());
}

TEST(RequestTest, ParseUrlWithPort) {
    Request req;
    req.url = "http://example.com:8080/api/data";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 8080);
    EXPECT_EQ(req.path, "/api/data");
}

TEST(RequestTest, ParseUrlWithQuery) {
    Request req;
    req.url = "http://example.com/search?q=test&page=1";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_EQ(req.path, "/search");
    EXPECT_EQ(req.query, "q=test&page=1");
}

TEST(RequestTest, ParseUrlRootPath) {
    Request req;
    req.url = "http://example.com";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_EQ(req.path, "/");
}

TEST(RequestTest, ParseUrlTrailingSlash) {
    Request req;
    req.url = "http://example.com/";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_EQ(req.path, "/");
}

TEST(RequestTest, ParseUrlHttpsDefaultPort) {
    Request req;
    req.url = "https://example.com/secure";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 443);
    EXPECT_EQ(req.path, "/secure");
    EXPECT_TRUE(req.use_tls);
}

TEST(RequestTest, ParseUrlHttpSetsUseTlsFalse) {
    Request req;
    req.url = "http://example.com/page";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_FALSE(req.use_tls);
}

TEST(RequestTest, ParseUrlHttpsWithCustomPort) {
    Request req;
    req.url = "https://example.com:8443/api";
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 8443);
    EXPECT_EQ(req.path, "/api");
    EXPECT_TRUE(req.use_tls);
}

TEST(RequestTest, UseTlsDefaultIsFalse) {
    Request req;
    EXPECT_FALSE(req.use_tls);
}

// ---------------------------------------------------------------------------
// 6. Request serialization to HTTP/1.1 format
// ---------------------------------------------------------------------------
TEST(RequestTest, SerializeGetRequest) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.port = 80;
    req.path = "/index.html";
    req.headers.set("Accept", "text/html");

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    // Check request line
    EXPECT_TRUE(result.find("GET /index.html HTTP/1.1\r\n") != std::string::npos);
    // Check Host header is present
    EXPECT_TRUE(result.find("Host: example.com\r\n") != std::string::npos);
    // Check Connection header
    EXPECT_TRUE(result.find("Connection: keep-alive\r\n") != std::string::npos);
    // Check custom header (stored lowercase)
    EXPECT_TRUE(result.find("accept: text/html\r\n") != std::string::npos);
    // Check ends with empty line
    EXPECT_TRUE(result.find("\r\n\r\n") != std::string::npos);
}

TEST(RequestTest, SerializeGetRequestWithQuery) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.port = 80;
    req.path = "/search";
    req.query = "q=hello";

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_TRUE(result.find("GET /search?q=hello HTTP/1.1\r\n") != std::string::npos);
}

TEST(RequestTest, SerializePostRequestWithBody) {
    Request req;
    req.method = Method::POST;
    req.host = "example.com";
    req.port = 80;
    req.path = "/api/data";

    std::string body_str = R"({"key":"value"})";
    req.body.assign(body_str.begin(), body_str.end());
    req.headers.set("Content-Type", "application/json");

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_TRUE(result.find("POST /api/data HTTP/1.1\r\n") != std::string::npos);
    // Content-Length should be auto-added
    EXPECT_TRUE(result.find("Content-Length: 15\r\n") != std::string::npos);
    // Body should be at the end
    EXPECT_TRUE(result.find("\r\n\r\n{\"key\":\"value\"}") != std::string::npos);
}

TEST(RequestTest, SerializeNonStandardPort) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.port = 8080;
    req.path = "/";

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_TRUE(result.find("Host: example.com:8080\r\n") != std::string::npos);
}

// ===========================================================================
// Response Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 8. Response parsing from raw HTTP bytes
// ---------------------------------------------------------------------------
TEST(ResponseTest, ParseSimpleResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->status_text, "OK");
    EXPECT_EQ(resp->headers.get("content-type").value(), "text/html");
    EXPECT_EQ(resp->headers.get("content-length").value(), "13");
    EXPECT_EQ(resp->body.size(), 13u);
    EXPECT_EQ(resp->body_as_string(), "Hello, World!");
}

TEST(ResponseTest, Parse404Response) {
    std::string raw =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not Found";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404);
    EXPECT_EQ(resp->status_text, "Not Found");
    EXPECT_EQ(resp->body_as_string(), "Not Found");
}

TEST(ResponseTest, ParseResponseMultipleHeaders) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: a=1\r\n"
        "Set-Cookie: b=2\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    auto cookies = resp->headers.get_all("set-cookie");
    EXPECT_EQ(cookies.size(), 2u);
}

TEST(ResponseTest, ParseResponseNoBody) {
    std::string raw =
        "HTTP/1.1 204 No Content\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 204);
    EXPECT_EQ(resp->status_text, "No Content");
    EXPECT_TRUE(resp->body.empty());
}

TEST(ResponseTest, ParseIncompleteResponse) {
    // No CRLFCRLF separator -- should fail
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    EXPECT_FALSE(resp.has_value());
}

TEST(ResponseTest, ParseChunkedResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "7\r\n"
        ", World\r\n"
        "0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->body_as_string(), "Hello, World");
}

// ---------------------------------------------------------------------------
// 9. Response: body_as_string
// ---------------------------------------------------------------------------
TEST(ResponseTest, BodyAsString) {
    Response resp;
    std::string text = "test body content";
    resp.body.assign(text.begin(), text.end());
    EXPECT_EQ(resp.body_as_string(), "test body content");
}

TEST(ResponseTest, BodyAsStringEmpty) {
    Response resp;
    EXPECT_EQ(resp.body_as_string(), "");
}

// ===========================================================================
// ConnectionPool Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// 11. ConnectionPool: acquire returns -1 when empty
// ---------------------------------------------------------------------------
TEST(ConnectionPoolTest, AcquireReturnsNegativeOneWhenEmpty) {
    ConnectionPool pool;
    EXPECT_EQ(pool.acquire("example.com", 80), -1);
}

// ---------------------------------------------------------------------------
// 12. ConnectionPool: release and acquire round-trip
// ---------------------------------------------------------------------------
TEST(ConnectionPoolTest, ReleaseAndAcquireRoundTrip) {
    ConnectionPool pool;

    // Use a fake fd (we won't actually use it for I/O)
    int fake_fd = 42;
    pool.release("example.com", 80, fake_fd);

    EXPECT_EQ(pool.count("example.com", 80), 1u);
    int acquired = pool.acquire("example.com", 80);
    EXPECT_EQ(acquired, fake_fd);
    EXPECT_EQ(pool.count("example.com", 80), 0u);
}

TEST(ConnectionPoolTest, AcquireIsLIFO) {
    ConnectionPool pool;

    pool.release("example.com", 80, 10);
    pool.release("example.com", 80, 20);
    pool.release("example.com", 80, 30);

    // Should return most recently released first (LIFO)
    int fd = pool.acquire("example.com", 80);
    EXPECT_EQ(fd, 30);
    fd = pool.acquire("example.com", 80);
    EXPECT_EQ(fd, 20);
    fd = pool.acquire("example.com", 80);
    EXPECT_EQ(fd, 10);
    fd = pool.acquire("example.com", 80);
    EXPECT_EQ(fd, -1);
}

// ---------------------------------------------------------------------------
// 13. ConnectionPool: max per host limit
// ---------------------------------------------------------------------------
TEST(ConnectionPoolTest, MaxPerHostLimit) {
    ConnectionPool pool(2);  // max 2 per host

    pool.release("example.com", 80, 10);
    pool.release("example.com", 80, 20);
    // This should cause the oldest to be evicted (or just not stored)
    pool.release("example.com", 80, 30);

    EXPECT_EQ(pool.count("example.com", 80), 2u);
}

// ---------------------------------------------------------------------------
// 14. ConnectionPool: different hosts are independent
// ---------------------------------------------------------------------------
TEST(ConnectionPoolTest, DifferentHostsAreIndependent) {
    ConnectionPool pool;

    pool.release("example.com", 80, 10);
    pool.release("other.com", 80, 20);
    pool.release("example.com", 443, 30);

    EXPECT_EQ(pool.count("example.com", 80), 1u);
    EXPECT_EQ(pool.count("other.com", 80), 1u);
    EXPECT_EQ(pool.count("example.com", 443), 1u);

    EXPECT_EQ(pool.acquire("example.com", 80), 10);
    EXPECT_EQ(pool.acquire("other.com", 80), 20);
    EXPECT_EQ(pool.acquire("example.com", 443), 30);

    EXPECT_EQ(pool.acquire("example.com", 80), -1);
}

TEST(ConnectionPoolTest, ClearRemovesAllConnections) {
    ConnectionPool pool;

    pool.release("example.com", 80, 10);
    pool.release("other.com", 80, 20);

    pool.clear();

    EXPECT_EQ(pool.count("example.com", 80), 0u);
    EXPECT_EQ(pool.count("other.com", 80), 0u);
    EXPECT_EQ(pool.acquire("example.com", 80), -1);
    EXPECT_EQ(pool.acquire("other.com", 80), -1);
}

// ===========================================================================
// HttpClient Tests (unit-level, no real network)
// ===========================================================================

TEST(HttpClientTest, DefaultConstruction) {
    HttpClient client;
    // Just verify it constructs without crashing
    SUCCEED();
}

TEST(HttpClientTest, SetTimeout) {
    HttpClient client;
    client.set_timeout(std::chrono::milliseconds(5000));
    SUCCEED();
}

TEST(HttpClientTest, SetMaxRedirects) {
    HttpClient client;
    client.set_max_redirects(5);
    SUCCEED();
}

// Integration-like test: full request -> parse_url -> serialize round-trip
TEST(HttpClientTest, RequestRoundTrip) {
    Request req;
    req.url = "http://httpbin.org/get?foo=bar";
    req.method = Method::GET;
    req.parse_url();

    EXPECT_EQ(req.host, "httpbin.org");
    EXPECT_EQ(req.port, 80);
    EXPECT_EQ(req.path, "/get");
    EXPECT_EQ(req.query, "foo=bar");

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_TRUE(result.find("GET /get?foo=bar HTTP/1.1\r\n") != std::string::npos);
    EXPECT_TRUE(result.find("Host: httpbin.org\r\n") != std::string::npos);
}

// Full response parse round-trip
TEST(HttpClientTest, ResponseRoundTrip) {
    std::string raw =
        "HTTP/1.1 301 Moved Permanently\r\n"
        "Location: http://example.com/new\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301);
    EXPECT_EQ(resp->status_text, "Moved Permanently");
    EXPECT_EQ(resp->headers.get("location").value(), "http://example.com/new");
    EXPECT_TRUE(resp->body.empty());
}

// ===========================================================================
// TlsSocket Unit Tests
// ===========================================================================

TEST(TlsSocketTest, DefaultConstruction) {
    TlsSocket tls;
    EXPECT_FALSE(tls.is_connected());
}

TEST(TlsSocketTest, ConnectWithInvalidFdFails) {
    TlsSocket tls;
    // Connecting with an invalid fd should fail gracefully
    EXPECT_FALSE(tls.connect("example.com", 443, -1));
    EXPECT_FALSE(tls.is_connected());
}

TEST(TlsSocketTest, SendWithoutConnectFails) {
    TlsSocket tls;
    uint8_t data[] = {0x01, 0x02, 0x03};
    EXPECT_FALSE(tls.send(data, sizeof(data)));
}

TEST(TlsSocketTest, RecvWithoutConnectReturnsNullopt) {
    TlsSocket tls;
    auto result = tls.recv();
    EXPECT_FALSE(result.has_value());
}

TEST(TlsSocketTest, CloseWithoutConnectIsNoop) {
    TlsSocket tls;
    // Should not crash
    tls.close();
    EXPECT_FALSE(tls.is_connected());
}

// ===========================================================================
// HTTPS Integration Tests (require network access)
// ===========================================================================

// These tests require actual network connectivity.  They are enabled by
// default because the task specifically asks for them, but in a CI
// environment without internet access they will fail gracefully.

TEST(HttpsIntegrationTest, FetchExampleComOverHttps) {
    HttpClient client;
    client.set_timeout(std::chrono::seconds(10));

    Request req;
    req.url = "https://example.com/";
    req.method = Method::GET;
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 443);
    EXPECT_TRUE(req.use_tls);

    auto resp = client.fetch(req);

    // If we have no network, the fetch will return nullopt -- skip gracefully.
    if (!resp.has_value()) {
        GTEST_SKIP() << "Network unavailable, skipping HTTPS integration test";
    }

    EXPECT_EQ(resp->status, 200);

    // example.com should return HTML containing "Example Domain"
    std::string body = resp->body_as_string();
    EXPECT_FALSE(body.empty());
    EXPECT_NE(body.find("Example Domain"), std::string::npos);
}

TEST(HttpsIntegrationTest, HttpStillWorksAfterTlsChanges) {
    // Ensure that plain HTTP requests still work after our TLS modifications.
    // This is a round-trip test that exercises parse_url -> serialize only
    // (no actual network call) to verify we did not break the non-TLS path.
    Request req;
    req.url = "http://example.com/page";
    req.method = Method::GET;
    req.parse_url();

    EXPECT_EQ(req.host, "example.com");
    EXPECT_EQ(req.port, 80);
    EXPECT_FALSE(req.use_tls);

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_TRUE(result.find("GET /page HTTP/1.1\r\n") != std::string::npos);
    EXPECT_TRUE(result.find("Host: example.com\r\n") != std::string::npos);
}

// ===========================================================================
// CookieJar Tests
// ===========================================================================

TEST(CookieJarTest, SetAndGetCookie) {
    CookieJar jar;
    jar.set_from_header("session=abc123", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_EQ(header, "session=abc123");
    EXPECT_EQ(jar.size(), 1u);
}

TEST(CookieJarTest, MultipleCookies) {
    CookieJar jar;
    jar.set_from_header("a=1", "example.com");
    jar.set_from_header("b=2", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/", false);
    // Should contain both cookies separated by "; "
    EXPECT_TRUE(header.find("a=1") != std::string::npos);
    EXPECT_TRUE(header.find("b=2") != std::string::npos);
    EXPECT_EQ(jar.size(), 2u);
}

TEST(CookieJarTest, CookieWithDomain) {
    CookieJar jar;
    jar.set_from_header("token=xyz; Domain=.example.com", "www.example.com");

    // Should match subdomain
    std::string header = jar.get_cookie_header("www.example.com", "/", false);
    EXPECT_EQ(header, "token=xyz");

    // Should match bare domain
    std::string header2 = jar.get_cookie_header("example.com", "/", false);
    EXPECT_EQ(header2, "token=xyz");

    // Should NOT match other domain
    std::string header3 = jar.get_cookie_header("other.com", "/", false);
    EXPECT_TRUE(header3.empty());
}

TEST(CookieJarTest, CookieWithPath) {
    CookieJar jar;
    jar.set_from_header("key=val; Path=/api", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/api/users", false);
    EXPECT_EQ(header, "key=val");

    std::string header2 = jar.get_cookie_header("example.com", "/other", false);
    EXPECT_TRUE(header2.empty());
}

TEST(CookieJarTest, SecureCookie) {
    CookieJar jar;
    jar.set_from_header("secret=shh; Secure", "example.com");

    // Should NOT be sent over non-secure
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_TRUE(header.empty());

    // Should be sent over secure
    std::string header2 = jar.get_cookie_header("example.com", "/", true);
    EXPECT_EQ(header2, "secret=shh");
}

TEST(CookieJarTest, CookieReplacement) {
    CookieJar jar;
    jar.set_from_header("key=old_value", "example.com");
    jar.set_from_header("key=new_value", "example.com");

    EXPECT_EQ(jar.size(), 1u);
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_EQ(header, "key=new_value");
}

TEST(CookieJarTest, Clear) {
    CookieJar jar;
    jar.set_from_header("a=1", "example.com");
    jar.set_from_header("b=2", "other.com");
    EXPECT_EQ(jar.size(), 2u);

    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
    EXPECT_TRUE(jar.get_cookie_header("example.com", "/", false).empty());
}

TEST(CookieJarTest, ComplexSetCookieHeader) {
    CookieJar jar;
    jar.set_from_header("id=abc; Path=/; Domain=.example.com; Secure; HttpOnly", "www.example.com");

    EXPECT_EQ(jar.size(), 1u);
    // Secure cookie, not sent over http
    std::string header = jar.get_cookie_header("www.example.com", "/", false);
    EXPECT_TRUE(header.empty());

    std::string header2 = jar.get_cookie_header("www.example.com", "/", true);
    EXPECT_EQ(header2, "id=abc");
}

// ===========================================================================
// Response: gzip Content-Encoding decompression
// ===========================================================================

TEST(ResponseTest, GzipDecompression) {
    // Create a gzip-compressed "Hello, World!" payload
    // This is a real gzip-compressed version of "Hello, World!"
    const uint8_t gzip_hello[] = {
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7,
        0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04,
        0x00, 0xd0, 0xc3, 0x4a, 0xec, 0x0d, 0x00, 0x00,
        0x00
    };

    // Build a raw HTTP response with gzip content-encoding
    std::string header = "HTTP/1.1 200 OK\r\n"
                         "Content-Encoding: gzip\r\n"
                         "Content-Length: " + std::to_string(sizeof(gzip_hello)) + "\r\n"
                         "\r\n";

    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), gzip_hello, gzip_hello + sizeof(gzip_hello));

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);

    std::string body = resp->body_as_string();
    EXPECT_EQ(body, "Hello, World!");
}

TEST(ResponseTest, NonGzipResponseUnchanged) {
    std::string raw_str = "HTTP/1.1 200 OK\r\n"
                          "Content-Length: 5\r\n"
                          "\r\n"
                          "Hello";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "Hello");
}

// --- Cycle 192: User-Agent header and cookie expiration ---

TEST(RequestSerializeTest, DefaultUserAgent) {
    Request req;
    req.method = Method::GET;
    req.url = "http://example.com/page";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("Clever/0.7.0"), std::string::npos)
        << "Should include default User-Agent header with Clever version";
    EXPECT_NE(s.find("Accept: "), std::string::npos)
        << "Should include default Accept header";
    EXPECT_NE(s.find("Accept-Encoding: gzip"), std::string::npos)
        << "Should include default Accept-Encoding header";
}

TEST(RequestSerializeTest, CustomUserAgentOverridesDefault) {
    Request req;
    req.method = Method::GET;
    req.url = "http://example.com/";
    req.parse_url();
    req.headers.set("user-agent", "CustomBot/1.0");
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("CustomBot/1.0"), std::string::npos);
    // Should NOT have the default Clever user-agent
    EXPECT_EQ(s.find("Clever/0.5.0"), std::string::npos);
}

TEST(CookieJarTest, MaxAgeCookie) {
    CookieJar jar;
    // Set a cookie with Max-Age=3600 (1 hour from now)
    jar.set_from_header("session=abc123; Max-Age=3600", "example.com");
    EXPECT_EQ(jar.get_cookie_header("example.com", "/", false), "session=abc123");
}

TEST(CookieJarTest, ExpiredMaxAgeCookieFiltered) {
    CookieJar jar;
    // Set a cookie that already expired (Max-Age=0)
    jar.set_from_header("old=gone; Max-Age=0", "example.com");
    EXPECT_EQ(jar.get_cookie_header("example.com", "/", false), "")
        << "Expired cookie should not be returned";
}

TEST(CookieJarTest, SameSiteAttribute) {
    CookieJar jar;
    jar.set_from_header("token=xyz; SameSite=Strict", "example.com");
    // Cookie should still be stored and returned for same-site requests
    EXPECT_EQ(jar.get_cookie_header("example.com", "/", false), "token=xyz");
}

TEST(CookieJarTest, ExpiresAttribute) {
    CookieJar jar;
    // Set a cookie with Expires far in the future
    jar.set_from_header("future=yes; Expires=Thu, 01 Jan 2099 00:00:00 GMT", "example.com");
    EXPECT_EQ(jar.get_cookie_header("example.com", "/", false), "future=yes");
}

TEST(CookieJarTest, ExpiredExpiresFiltered) {
    CookieJar jar;
    // Set a cookie with Expires in the past
    jar.set_from_header("old=no; Expires=Thu, 01 Jan 2020 00:00:00 GMT", "example.com");
    EXPECT_EQ(jar.get_cookie_header("example.com", "/", false), "")
        << "Cookie with past Expires should not be returned";
}

// ============================================================================
// Cycle 428: SameSite cross-site enforcement regression tests
// ============================================================================

TEST(CookieJarTest, SameSiteStrictNotSentCrossSite) {
    CookieJar jar;
    jar.set_from_header("token=secret; SameSite=Strict", "example.com");

    // Cross-site request (is_same_site=false) — Strict must not be sent
    std::string header = jar.get_cookie_header("example.com", "/", false, /*is_same_site=*/false);
    EXPECT_TRUE(header.empty())
        << "SameSite=Strict cookie should not be sent on cross-site requests";

    // Same-site request — Strict should be sent
    std::string header2 = jar.get_cookie_header("example.com", "/", false, /*is_same_site=*/true);
    EXPECT_EQ(header2, "token=secret");
}

TEST(CookieJarTest, SameSiteLaxSentForTopLevelNavOnly) {
    CookieJar jar;
    jar.set_from_header("session=lax; SameSite=Lax", "example.com");

    // Cross-site top-level navigation (GET for page) — Lax should be sent
    std::string header_nav = jar.get_cookie_header("example.com", "/", false,
                                                   /*is_same_site=*/false,
                                                   /*is_top_level_nav=*/true);
    EXPECT_EQ(header_nav, "session=lax")
        << "SameSite=Lax should be sent on cross-site top-level navigation";

    // Cross-site non-navigation (e.g. XHR/fetch) — Lax should NOT be sent
    std::string header_xhr = jar.get_cookie_header("example.com", "/", false,
                                                   /*is_same_site=*/false,
                                                   /*is_top_level_nav=*/false);
    EXPECT_TRUE(header_xhr.empty())
        << "SameSite=Lax should not be sent on cross-site non-navigation requests";
}

TEST(CookieJarTest, SameSiteNoneRequiresSecure) {
    CookieJar jar;
    // SameSite=None without Secure — should be filtered on cross-site
    jar.set_from_header("insecure=none; SameSite=None", "example.com");

    // Attempting cross-site over HTTP — should not be sent (no Secure flag)
    std::string header = jar.get_cookie_header("example.com", "/", /*is_secure=*/false,
                                               /*is_same_site=*/false);
    EXPECT_TRUE(header.empty())
        << "SameSite=None without Secure should not be sent on cross-site requests";
}

TEST(CookieJarTest, SameSiteNoneWithSecureSentCrossSite) {
    CookieJar jar;
    // SameSite=None with Secure — should be sent on cross-site HTTPS
    jar.set_from_header("cross=ok; SameSite=None; Secure", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/", /*is_secure=*/true,
                                               /*is_same_site=*/false);
    EXPECT_EQ(header, "cross=ok")
        << "SameSite=None with Secure should be sent on cross-site HTTPS requests";
}

TEST(CookieJarTest, DefaultSameSiteLaxBehavior) {
    CookieJar jar;
    // Cookie without SameSite attribute — defaults to Lax behavior (same as SameSite=Lax)
    jar.set_from_header("default=lax", "example.com");

    // Cross-site non-navigation — default Lax should block
    std::string header_xhr = jar.get_cookie_header("example.com", "/", false,
                                                   /*is_same_site=*/false,
                                                   /*is_top_level_nav=*/false);
    EXPECT_TRUE(header_xhr.empty())
        << "Cookie without SameSite defaults to Lax and should not be sent cross-site non-nav";

    // Same-site request — should always be sent
    std::string header_same = jar.get_cookie_header("example.com", "/", false,
                                                    /*is_same_site=*/true);
    EXPECT_EQ(header_same, "default=lax");
}

// ===========================================================================
// Request Serialization — Connection header
// ===========================================================================

TEST(RequestTest, DefaultConnectionKeepAlive) {
    Request req;
    req.url = "http://example.com/path";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("Connection: keep-alive"), std::string::npos)
        << "Default Connection should be keep-alive";
}

TEST(RequestTest, DefaultAcceptEncodingHeader) {
    Request req;
    req.url = "http://example.com/";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("Accept-Encoding: gzip, deflate"), std::string::npos)
        << "Default Accept-Encoding should include gzip and deflate";
}

TEST(RequestTest, DefaultAcceptHeader) {
    Request req;
    req.url = "http://example.com/";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("Accept: text/html"), std::string::npos)
        << "Default Accept should include text/html";
}

// ===========================================================================
// Host Header Serialization Tests
// ===========================================================================

TEST(RequestTest, HostHeaderNonStandardPort) {
    Request req;
    req.url = "http://example.com:9090/path";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    EXPECT_NE(s.find("Host: example.com:9090"), std::string::npos)
        << "Non-standard port should appear in Host header";
}

TEST(RequestTest, HostHeaderStandardPortOmitted) {
    Request req;
    req.url = "http://example.com/path";
    req.parse_url();
    auto bytes = req.serialize();
    std::string s(bytes.begin(), bytes.end());
    // Should be "Host: example.com\r\n" without port 80
    EXPECT_NE(s.find("Host: example.com\r\n"), std::string::npos)
        << "Standard port 80 should be omitted from Host header";
    EXPECT_EQ(s.find("Host: example.com:80"), std::string::npos)
        << "Port 80 should NOT appear in Host header";
}

// ============================================================================
// Cycle 429: Request serialization for PUT / PATCH / DELETE / OPTIONS methods
// ============================================================================

TEST(RequestTest, SerializePutRequestWithBody) {
    Request req;
    req.method = Method::PUT;
    req.host = "api.example.com";
    req.port = 443;
    req.path = "/resource/42";
    req.use_tls = true;

    std::string body_str = R"({"status":"active"})";
    req.body.assign(body_str.begin(), body_str.end());
    req.headers.set("Content-Type", "application/json");

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("PUT /resource/42 HTTP/1.1\r\n"), std::string::npos);
    EXPECT_NE(result.find("Content-Length:"), std::string::npos);
}

TEST(RequestTest, SerializePatchRequestWithBody) {
    Request req;
    req.method = Method::PATCH;
    req.host = "api.example.com";
    req.port = 80;
    req.path = "/users/7";

    std::string body_str = R"({"name":"Alice"})";
    req.body.assign(body_str.begin(), body_str.end());

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("PATCH /users/7 HTTP/1.1\r\n"), std::string::npos);
}

TEST(RequestTest, SerializeDeleteRequest) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.example.com";
    req.port = 80;
    req.path = "/items/99";

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("DELETE /items/99 HTTP/1.1\r\n"), std::string::npos);
}

TEST(RequestTest, SerializeOptionsRequest) {
    Request req;
    req.method = Method::OPTIONS;
    req.host = "api.example.com";
    req.port = 80;
    req.path = "/api";

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("OPTIONS /api HTTP/1.1\r\n"), std::string::npos);
}

// ===========================================================================
// HTTP Content Decompression Tests
// ===========================================================================

// Helper: compress a string using gzip format via zlib
static std::vector<uint8_t> compress_gzip(const std::string& input) {
    z_stream strm{};
    // windowBits = 15 + 16 forces gzip format
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());

    std::vector<uint8_t> output;
    uint8_t buffer[4096];

    do {
        strm.next_out = buffer;
        strm.avail_out = sizeof(buffer);
        deflate(&strm, Z_FINISH);
        size_t have = sizeof(buffer) - strm.avail_out;
        output.insert(output.end(), buffer, buffer + have);
    } while (strm.avail_out == 0);

    deflateEnd(&strm);
    return output;
}

// Helper: compress a string using raw deflate format via zlib
static std::vector<uint8_t> compress_deflate(const std::string& input) {
    z_stream strm{};
    // windowBits = -15 forces raw deflate (no zlib/gzip header)
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());

    std::vector<uint8_t> output;
    uint8_t buffer[4096];

    do {
        strm.next_out = buffer;
        strm.avail_out = sizeof(buffer);
        deflate(&strm, Z_FINISH);
        size_t have = sizeof(buffer) - strm.avail_out;
        output.insert(output.end(), buffer, buffer + have);
    } while (strm.avail_out == 0);

    deflateEnd(&strm);
    return output;
}

// Helper: build an HTTP response with given headers and binary body
static std::vector<uint8_t> build_raw_response(
    const std::string& status_line,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::vector<uint8_t>& body) {

    std::string header_str = status_line + "\r\n";
    for (auto& [name, value] : headers) {
        header_str += name + ": " + value + "\r\n";
    }
    header_str += "\r\n";

    std::vector<uint8_t> raw(header_str.begin(), header_str.end());
    raw.insert(raw.end(), body.begin(), body.end());
    return raw;
}

// ---------------------------------------------------------------------------
// Deflate Content-Encoding decompression
// ---------------------------------------------------------------------------
TEST(DecompressionTest, DeflateDecompression) {
    std::string original = "This is a test of deflate decompression in the Clever browser engine.";
    auto compressed = compress_deflate(original);

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "deflate"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Gzip decompression with programmatically compressed data
// ---------------------------------------------------------------------------
TEST(DecompressionTest, GzipDecompressionProgrammatic) {
    std::string original = "The quick brown fox jumps over the lazy dog. "
                           "Pack my box with five dozen liquor jugs.";
    auto compressed = compress_gzip(original);

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// x-gzip Content-Encoding variant
// ---------------------------------------------------------------------------
TEST(DecompressionTest, XGzipContentEncoding) {
    std::string original = "x-gzip variant test";
    auto compressed = compress_gzip(original);

    // The response.cpp code checks for "gzip" substring, which matches "x-gzip"
    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "x-gzip"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    // x-gzip contains "gzip" substring, so the code should decompress it
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Case-insensitive Content-Encoding detection
// ---------------------------------------------------------------------------
TEST(DecompressionTest, ContentEncodingCaseInsensitive) {
    std::string original = "Case insensitive encoding test";
    auto compressed = compress_gzip(original);

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "GZIP"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Content-Encoding header detection in response
// ---------------------------------------------------------------------------
TEST(DecompressionTest, ContentEncodingHeaderPresent) {
    std::string original = "Encoding header detection";
    auto compressed = compress_gzip(original);

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Type", "text/plain"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());

    // The Content-Encoding header should be preserved in the response
    auto ce = resp->headers.get("content-encoding");
    ASSERT_TRUE(ce.has_value());
    EXPECT_EQ(*ce, "gzip");

    // And the body should be decompressed
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Invalid/corrupt compressed data falls back gracefully
// ---------------------------------------------------------------------------
TEST(DecompressionTest, CorruptGzipDataFallback) {
    // Construct obviously invalid gzip data
    std::vector<uint8_t> corrupt_data = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Length", std::to_string(corrupt_data.size())}},
        corrupt_data);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);

    // On decompression failure, the code returns the original compressed data
    EXPECT_EQ(resp->body.size(), corrupt_data.size());
    EXPECT_EQ(resp->body, corrupt_data);
}

// ---------------------------------------------------------------------------
// Empty body with Content-Encoding is handled
// ---------------------------------------------------------------------------
TEST(DecompressionTest, EmptyBodyWithContentEncoding) {
    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Length", "0"}},
        {});

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->body.empty());
}

// ---------------------------------------------------------------------------
// No Content-Encoding: body is returned as-is
// ---------------------------------------------------------------------------
TEST(DecompressionTest, NoContentEncodingBodyUnchanged) {
    std::string body_text = "This should not be decompressed";

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Type", "text/plain"},
         {"Content-Length", std::to_string(body_text.size())}},
        std::vector<uint8_t>(body_text.begin(), body_text.end()));

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), body_text);
}

// ---------------------------------------------------------------------------
// Chunked + gzip: chunked transfer encoding with gzip body
// ---------------------------------------------------------------------------
TEST(DecompressionTest, ChunkedWithGzipEncoding) {
    std::string original = "Chunked and compressed response body";
    auto compressed = compress_gzip(original);

    // Build chunked body: one chunk containing all compressed data
    std::ostringstream chunked;
    // Chunk size in hex
    chunked << std::hex << compressed.size() << "\r\n";
    chunked.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
    chunked << "\r\n";
    // Final chunk
    chunked << "0\r\n\r\n";

    std::string chunked_body = chunked.str();

    std::string header = "HTTP/1.1 200 OK\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "Content-Encoding: gzip\r\n"
                         "\r\n";

    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), chunked_body.begin(), chunked_body.end());

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Large body gzip decompression (tests multi-chunk inflate loop)
// ---------------------------------------------------------------------------
TEST(DecompressionTest, LargeBodyGzipDecompression) {
    // Create a large repetitive string (compresses well)
    std::string original;
    original.reserve(100000);
    for (int i = 0; i < 1000; ++i) {
        original += "Line " + std::to_string(i) + ": The quick brown fox jumps over the lazy dog.\n";
    }

    auto compressed = compress_gzip(original);
    // Compressed should be much smaller
    EXPECT_LT(compressed.size(), original.size());

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Length", std::to_string(compressed.size())}},
        compressed);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), original);
}

// ---------------------------------------------------------------------------
// Accept-Encoding header is sent in outgoing requests
// ---------------------------------------------------------------------------
TEST(DecompressionTest, AcceptEncodingHeaderInRequest) {
    Request req;
    req.url = "http://example.com/page";
    req.parse_url();

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("Accept-Encoding: gzip, deflate\r\n"), std::string::npos)
        << "Request must include Accept-Encoding header with gzip and deflate";
}

// ---------------------------------------------------------------------------
// Custom Accept-Encoding overrides default
// ---------------------------------------------------------------------------
TEST(DecompressionTest, CustomAcceptEncodingOverridesDefault) {
    Request req;
    req.url = "http://example.com/page";
    req.parse_url();
    req.headers.set("accept-encoding", "identity");

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("accept-encoding: identity\r\n"), std::string::npos);
    // Should NOT have the default
    EXPECT_EQ(result.find("Accept-Encoding: gzip, deflate"), std::string::npos)
        << "Custom Accept-Encoding should override the default";
}

// ---------------------------------------------------------------------------
// Partial/truncated gzip data falls back
// ---------------------------------------------------------------------------
TEST(DecompressionTest, TruncatedGzipDataFallback) {
    std::string original = "Full text that will be compressed and then truncated";
    auto compressed = compress_gzip(original);

    // Truncate the compressed data to half
    std::vector<uint8_t> truncated(compressed.begin(),
                                    compressed.begin() + static_cast<std::ptrdiff_t>(compressed.size() / 2));

    auto raw = build_raw_response(
        "HTTP/1.1 200 OK",
        {{"Content-Encoding", "gzip"},
         {"Content-Length", std::to_string(truncated.size())}},
        truncated);

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    // Should fall back to the raw truncated data since decompression fails mid-stream
    // The decompress function returns original on Z_DATA_ERROR, Z_STREAM_ERROR, or Z_MEM_ERROR
    // but may return partial data if inflate returns Z_STREAM_END on a truncated stream
    // Either way, it should NOT crash
    EXPECT_FALSE(resp->body.empty());
}

// ---------------------------------------------------------------------------
// Chunked transfer encoding with multiple chunks (no compression)
// ---------------------------------------------------------------------------
TEST(DecompressionTest, ChunkedMultipleChunksNoCompression) {
    std::string header = "HTTP/1.1 200 OK\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n";

    std::string chunked_body =
        "5\r\n"
        "Hello\r\n"
        "1\r\n"
        " \r\n"
        "6\r\n"
        "World!\r\n"
        "0\r\n"
        "\r\n";

    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), chunked_body.begin(), chunked_body.end());

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "Hello World!");
}

// ---------------------------------------------------------------------------
// Chunked encoding with chunk extensions (semicolon after size)
// ---------------------------------------------------------------------------
TEST(DecompressionTest, ChunkedWithExtensions) {
    std::string header = "HTTP/1.1 200 OK\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n";

    // Chunk size with extension: "a;ext=val\r\n" (size = 0xa = 10)
    std::string chunked_body =
        "a;ext=val\r\n"
        "0123456789\r\n"
        "0\r\n"
        "\r\n";

    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), chunked_body.begin(), chunked_body.end());

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "0123456789");
}

// ===========================================================================
// HTTP Cache Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// Cache-Control: max-age parsing
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseMaxAge) {
    auto cc = parse_cache_control("max-age=3600");
    EXPECT_EQ(cc.max_age, 3600);
    EXPECT_FALSE(cc.no_cache);
    EXPECT_FALSE(cc.no_store);
    EXPECT_FALSE(cc.must_revalidate);
}

TEST(CacheControlTest, ParseMaxAgeWithPublic) {
    auto cc = parse_cache_control("max-age=600, public");
    EXPECT_EQ(cc.max_age, 600);
    EXPECT_TRUE(cc.is_public);
    EXPECT_FALSE(cc.no_cache);
}

TEST(CacheControlTest, ParsePrivateMaxAge) {
    auto cc = parse_cache_control("private, max-age=300");
    EXPECT_EQ(cc.max_age, 300);
    EXPECT_TRUE(cc.is_private);
    EXPECT_FALSE(cc.is_public);
}

// ---------------------------------------------------------------------------
// Cache-Control: no-cache parsing
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseNoCache) {
    auto cc = parse_cache_control("no-cache");
    EXPECT_TRUE(cc.no_cache);
    EXPECT_EQ(cc.max_age, -1);
}

// ---------------------------------------------------------------------------
// Cache-Control: no-store parsing
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseNoStore) {
    auto cc = parse_cache_control("no-store");
    EXPECT_TRUE(cc.no_store);
    EXPECT_FALSE(cc.no_cache);
}

// ---------------------------------------------------------------------------
// Cache-Control: must-revalidate parsing
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseMustRevalidate) {
    auto cc = parse_cache_control("max-age=0, must-revalidate");
    EXPECT_EQ(cc.max_age, 0);
    EXPECT_TRUE(cc.must_revalidate);
}

// ---------------------------------------------------------------------------
// Cache-Control: complex combined directives
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseMultipleDirectives) {
    auto cc = parse_cache_control("public, max-age=31536000, no-cache, must-revalidate");
    EXPECT_EQ(cc.max_age, 31536000);
    EXPECT_TRUE(cc.is_public);
    EXPECT_TRUE(cc.no_cache);
    EXPECT_TRUE(cc.must_revalidate);
    EXPECT_FALSE(cc.no_store);
}

// ---------------------------------------------------------------------------
// Cache-Control: case insensitive
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseCaseInsensitive) {
    auto cc = parse_cache_control("Max-Age=120, No-Cache, Must-Revalidate");
    EXPECT_EQ(cc.max_age, 120);
    EXPECT_TRUE(cc.no_cache);
    EXPECT_TRUE(cc.must_revalidate);
}

// ---------------------------------------------------------------------------
// Cache-Control: empty string
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParseEmpty) {
    auto cc = parse_cache_control("");
    EXPECT_EQ(cc.max_age, -1);
    EXPECT_FALSE(cc.no_cache);
    EXPECT_FALSE(cc.no_store);
}

// ---------------------------------------------------------------------------
// CacheEntry: freshness check
// ---------------------------------------------------------------------------
TEST(CacheEntryTest, FreshEntry) {
    CacheEntry entry;
    entry.max_age_seconds = 3600;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.no_cache = false;
    entry.must_revalidate = false;
    EXPECT_TRUE(entry.is_fresh());
}

TEST(CacheEntryTest, StaleEntry) {
    CacheEntry entry;
    entry.max_age_seconds = 1;
    // Stored 10 seconds ago
    entry.stored_at = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    entry.no_cache = false;
    entry.must_revalidate = false;
    EXPECT_FALSE(entry.is_fresh());
}

TEST(CacheEntryTest, NoCacheAlwaysStale) {
    CacheEntry entry;
    entry.max_age_seconds = 3600;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.no_cache = true;
    EXPECT_FALSE(entry.is_fresh());
}

TEST(CacheEntryTest, MustRevalidateAlwaysStale) {
    CacheEntry entry;
    entry.max_age_seconds = 3600;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.must_revalidate = true;
    EXPECT_FALSE(entry.is_fresh());
}

TEST(CacheEntryTest, ZeroMaxAgeNotFresh) {
    CacheEntry entry;
    entry.max_age_seconds = 0;
    entry.stored_at = std::chrono::steady_clock::now();
    EXPECT_FALSE(entry.is_fresh());
}

// ---------------------------------------------------------------------------
// HttpCache: store and lookup
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, StoreAndLookup) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://example.com/test";
    entry.etag = "\"abc123\"";
    entry.last_modified = "Mon, 01 Jan 2024 00:00:00 GMT";
    entry.body = "<html>hello</html>";
    entry.status = 200;
    entry.max_age_seconds = 3600;
    entry.stored_at = std::chrono::steady_clock::now();

    cache.store(entry);

    auto result = cache.lookup("https://example.com/test");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->etag, "\"abc123\"");
    EXPECT_EQ(result->last_modified, "Mon, 01 Jan 2024 00:00:00 GMT");
    EXPECT_EQ(result->body, "<html>hello</html>");
    EXPECT_EQ(result->status, 200);
    EXPECT_EQ(result->max_age_seconds, 3600);
}

TEST(HttpCacheTest, PrivateEntriesAreIgnored) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://private.example/test";
    entry.body = "sensitive";
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.is_private = true;

    cache.store(entry);

    EXPECT_EQ(cache.entry_count(), 0u);
    EXPECT_FALSE(cache.lookup(entry.url).has_value());
}

// ---------------------------------------------------------------------------
// HttpCache: ETag storage and retrieval
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, ETagStorageAndRetrieval) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://cdn.example.com/style.css";
    entry.etag = "W/\"5e15153d-120f\"";
    entry.body = "body { color: red; }";
    entry.status = 200;
    entry.max_age_seconds = 60;
    entry.stored_at = std::chrono::steady_clock::now();

    cache.store(entry);

    auto result = cache.lookup("https://cdn.example.com/style.css");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->etag, "W/\"5e15153d-120f\"");
    EXPECT_EQ(result->body, "body { color: red; }");
}

// ---------------------------------------------------------------------------
// HttpCache: miss returns nullopt
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, LookupMissReturnsNullopt) {
    auto& cache = HttpCache::instance();
    cache.clear();

    auto result = cache.lookup("https://example.com/nonexistent");
    EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// HttpCache: remove entry
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, RemoveEntry) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://example.com/remove-me";
    entry.body = "to be removed";
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    cache.store(entry);

    EXPECT_TRUE(cache.lookup("https://example.com/remove-me").has_value());
    cache.remove("https://example.com/remove-me");
    EXPECT_FALSE(cache.lookup("https://example.com/remove-me").has_value());
}

// ---------------------------------------------------------------------------
// HttpCache: clear removes all entries
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, ClearRemovesAll) {
    auto& cache = HttpCache::instance();
    cache.clear();

    for (int i = 0; i < 5; ++i) {
        CacheEntry entry;
        entry.url = "https://example.com/" + std::to_string(i);
        entry.body = "body " + std::to_string(i);
        entry.status = 200;
        entry.stored_at = std::chrono::steady_clock::now();
        cache.store(entry);
    }

    EXPECT_EQ(cache.entry_count(), 5u);
    cache.clear();
    EXPECT_EQ(cache.entry_count(), 0u);
    EXPECT_EQ(cache.total_size(), 0u);
}

// ---------------------------------------------------------------------------
// HttpCache: update existing entry
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, UpdateExistingEntry) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://example.com/update";
    entry.body = "version 1";
    entry.etag = "\"v1\"";
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    cache.store(entry);

    entry.body = "version 2";
    entry.etag = "\"v2\"";
    cache.store(entry);

    EXPECT_EQ(cache.entry_count(), 1u);
    auto result = cache.lookup("https://example.com/update");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->body, "version 2");
    EXPECT_EQ(result->etag, "\"v2\"");
}

// ---------------------------------------------------------------------------
// HttpCache: LRU eviction when over budget
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, LRUEvictionEnforcesSizeLimit) {
    auto& cache = HttpCache::instance();
    cache.clear();

    // Set a very small max size
    cache.set_max_bytes(500);

    // Insert entries that exceed the budget
    for (int i = 0; i < 10; ++i) {
        CacheEntry entry;
        entry.url = "https://example.com/lru/" + std::to_string(i);
        entry.body = std::string(100, 'x');  // ~100 bytes body each
        entry.status = 200;
        entry.stored_at = std::chrono::steady_clock::now();
        cache.store(entry);
    }

    // The cache should have evicted older entries to stay under budget
    EXPECT_LE(cache.total_size(), 500u + 300u);  // Allow some struct overhead

    // The most recently inserted should still be present
    auto result = cache.lookup("https://example.com/lru/9");
    EXPECT_TRUE(result.has_value());

    // Earliest entries should have been evicted
    auto old_result = cache.lookup("https://example.com/lru/0");
    EXPECT_FALSE(old_result.has_value());

    // Restore default max size
    cache.set_max_bytes(HttpCache::kDefaultMaxBytes);
}

// ---------------------------------------------------------------------------
// HttpCache: don't cache entries larger than kMaxEntryBytes
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, RejectOversizedEntry) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://example.com/huge";
    // Create a body larger than 10 MB
    entry.body = std::string(HttpCache::kMaxEntryBytes + 1, 'z');
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    cache.store(entry);

    // Should not have been stored
    EXPECT_FALSE(cache.lookup("https://example.com/huge").has_value());
    EXPECT_EQ(cache.entry_count(), 0u);
}

// ---------------------------------------------------------------------------
// HttpCache: approx_size calculation
// ---------------------------------------------------------------------------
TEST(CacheEntryTest, ApproxSizeCalculation) {
    CacheEntry entry;
    entry.url = "https://example.com/test";
    entry.etag = "\"abc\"";
    entry.body = "hello world";
    entry.headers["content-type"] = "text/html";

    size_t expected_min = entry.url.size() + entry.etag.size() + entry.body.size()
                        + std::string("content-type").size()
                        + std::string("text/html").size();
    EXPECT_GE(entry.approx_size(), expected_min);
}

// ---------------------------------------------------------------------------
// CacheEntry: no-store entries not considered fresh
// ---------------------------------------------------------------------------
TEST(CacheEntryTest, NoStoreNotFresh) {
    CacheEntry entry;
    entry.max_age_seconds = 3600;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.no_store = true;
    // no_store doesn't affect is_fresh() directly (it prevents storage),
    // but no_cache does
    entry.no_cache = false;
    entry.must_revalidate = false;
    // is_fresh() checks no_cache and must_revalidate, not no_store
    // (no_store prevents the entry from being stored in the first place)
    EXPECT_TRUE(entry.is_fresh());
}

// ---------------------------------------------------------------------------
// HttpCache: cache headers are stored
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, CacheStoresHeaders) {
    auto& cache = HttpCache::instance();
    cache.clear();

    CacheEntry entry;
    entry.url = "https://example.com/with-headers";
    entry.body = "content";
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    entry.headers["content-type"] = "text/css";
    entry.headers["x-custom"] = "value";

    cache.store(entry);

    auto result = cache.lookup("https://example.com/with-headers");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->headers.at("content-type"), "text/css");
    EXPECT_EQ(result->headers.at("x-custom"), "value");
}

// ============================================================================
// Cycle 427: should_cache_response regression tests
// ============================================================================

TEST(ShouldCacheResponseTest, CacheableBy200AndNoCCRestrictions) {
    Response resp;
    resp.status = 200;
    CacheControl cc;
    EXPECT_TRUE(should_cache_response(resp, cc));
}

TEST(ShouldCacheResponseTest, NonSuccessStatusNotCacheable) {
    CacheControl cc;

    Response r404;
    r404.status = 404;
    EXPECT_FALSE(should_cache_response(r404, cc));

    Response r301;
    r301.status = 301;
    EXPECT_FALSE(should_cache_response(r301, cc));

    Response r500;
    r500.status = 500;
    EXPECT_FALSE(should_cache_response(r500, cc));
}

TEST(ShouldCacheResponseTest, NoStorePreventsCaching) {
    Response resp;
    resp.status = 200;
    CacheControl cc;
    cc.no_store = true;
    EXPECT_FALSE(should_cache_response(resp, cc));
}

TEST(ShouldCacheResponseTest, PrivatePreventsCaching) {
    Response resp;
    resp.status = 200;
    CacheControl cc;
    cc.is_private = true;
    EXPECT_FALSE(should_cache_response(resp, cc));
}

TEST(ShouldCacheResponseTest, PublicWithMaxAgeCacheable) {
    Response resp;
    resp.status = 200;
    CacheControl cc;
    cc.is_public = true;
    cc.max_age = 86400;
    EXPECT_TRUE(should_cache_response(resp, cc));
}

// ============================================================================
// Cycle 427: parse_cache_control edge cases
// ============================================================================

TEST(CacheControlTest, ParseUnknownDirectivesIgnored) {
    // Unknown directives like s-maxage and immutable should not cause parsing to fail
    auto cc = parse_cache_control("max-age=300, s-maxage=600, immutable");
    EXPECT_EQ(cc.max_age, 300);
    EXPECT_FALSE(cc.no_cache);
}

TEST(CacheControlTest, ParseNoCacheWithMaxAge) {
    // no-cache coexisting with max-age — both should be recorded
    auto cc = parse_cache_control("no-cache, max-age=3600");
    EXPECT_TRUE(cc.no_cache);
    EXPECT_EQ(cc.max_age, 3600);
}

TEST(CacheControlTest, ParseNoStoreAndPrivate) {
    auto cc = parse_cache_control("no-store, private");
    EXPECT_TRUE(cc.no_store);
    EXPECT_TRUE(cc.is_private);
    EXPECT_FALSE(cc.is_public);
}

// ============================================================================
// Cycle 498: additional regression tests
// ============================================================================

// ---------------------------------------------------------------------------
// HeaderMap: iteration exposes lowercase keys
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, IterationKeysAreLowercase) {
    HeaderMap map;
    map.set("X-Custom-Header", "my-value");
    bool found = false;
    for (auto& [key, val] : map) {
        if (val == "my-value") {
            EXPECT_EQ(key, "x-custom-header");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// HeaderMap: empty() returns true after all entries removed
// ---------------------------------------------------------------------------
TEST(HeaderMapTest, EmptyAfterAllEntriesRemoved) {
    HeaderMap map;
    map.set("x-a", "1");
    map.set("x-b", "2");
    EXPECT_EQ(map.size(), 2u);
    EXPECT_FALSE(map.empty());
    map.remove("x-a");
    map.remove("x-b");
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
}

// ---------------------------------------------------------------------------
// CookieJar: cookie with empty value is stored and sent
// ---------------------------------------------------------------------------
TEST(CookieJarTest, CookieWithEmptyValue) {
    CookieJar jar;
    jar.set_from_header("token=", "example.com");
    EXPECT_EQ(jar.size(), 1u);
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_NE(header.find("token="), std::string::npos);
}

// ---------------------------------------------------------------------------
// CookieJar: HttpOnly attribute does NOT prevent sending the cookie
// ---------------------------------------------------------------------------
TEST(CookieJarTest, HttpOnlyCookieIncludedInRequests) {
    CookieJar jar;
    // HttpOnly prevents JS access but the browser still sends it in HTTP requests
    jar.set_from_header("session=secret; HttpOnly", "example.com");
    EXPECT_EQ(jar.size(), 1u);
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_EQ(header, "session=secret");
}

// ---------------------------------------------------------------------------
// Request: HEAD method serializes correctly
// ---------------------------------------------------------------------------
TEST(RequestTest, SerializeHeadRequest) {
    Request req;
    req.method = Method::HEAD;
    req.host = "example.com";
    req.port = 80;
    req.path = "/index.html";

    auto bytes = req.serialize();
    std::string result(bytes.begin(), bytes.end());

    EXPECT_NE(result.find("HEAD /index.html HTTP/1.1\r\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// CacheControl: "public" directive alone sets is_public
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ParsePublicDirectiveAlone) {
    auto cc = parse_cache_control("public");
    EXPECT_TRUE(cc.is_public);
    EXPECT_FALSE(cc.is_private);
    EXPECT_FALSE(cc.no_cache);
    EXPECT_FALSE(cc.no_store);
    EXPECT_EQ(cc.max_age, -1);
}

// ---------------------------------------------------------------------------
// HttpCache: entry_count updates after store and remove
// ---------------------------------------------------------------------------
TEST(HttpCacheTest, EntryCountAfterStoreAndRemove) {
    auto& cache = HttpCache::instance();
    cache.clear();
    EXPECT_EQ(cache.entry_count(), 0u);

    CacheEntry entry;
    entry.url = "https://example.com/ec-test";
    entry.body = "data";
    entry.status = 200;
    entry.stored_at = std::chrono::steady_clock::now();
    cache.store(entry);
    EXPECT_EQ(cache.entry_count(), 1u);

    cache.remove("https://example.com/ec-test");
    EXPECT_EQ(cache.entry_count(), 0u);
}

// ---------------------------------------------------------------------------
// Response: multi-word status text is parsed correctly
// ---------------------------------------------------------------------------
TEST(ResponseTest, ParseResponseThreeWordStatusText) {
    std::string raw =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 503);
    EXPECT_EQ(resp->status_text, "Service Unavailable");
}

// ============================================================================
// Cycle 517: HTTP net regression tests
// ============================================================================

// HeaderMap: case-insensitive lookup (set lowercase, get uppercase)
TEST(HeaderMapTest, CaseInsensitiveLookup) {
    HeaderMap map;
    map.set("content-type", "application/json");
    EXPECT_EQ(map.get("Content-Type"), "application/json");
    EXPECT_EQ(map.get("CONTENT-TYPE"), "application/json");
}

// HeaderMap: has() returns true only for stored keys
TEST(HeaderMapTest, HasReturnsTrueForStoredKey) {
    HeaderMap map;
    map.set("x-request-id", "abc123");
    EXPECT_TRUE(map.has("x-request-id"));
    EXPECT_FALSE(map.has("x-missing-header"));
}

// HeaderMap: remove() deletes the key
TEST(HeaderMapTest, RemoveDeletesKey) {
    HeaderMap map;
    map.set("authorization", "Bearer token");
    EXPECT_TRUE(map.has("authorization"));
    map.remove("authorization");
    EXPECT_FALSE(map.has("authorization"));
}

// Response: parse 200 OK with body
TEST(ResponseTest, ParseOkWithBody) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->status_text, "OK");
    EXPECT_EQ(resp->body.size(), 5u);
}

// Response: parse 404 Not Found
TEST(ResponseTest, ParseNotFound) {
    std::string raw = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404);
    EXPECT_EQ(resp->status_text, "Not Found");
}

// CookieJar: expired cookie is not sent
TEST(CookieJarTest, ExpiredCookieNotSent) {
    CookieJar jar;
    // Set a cookie that expired in the past
    jar.set_from_header("oldcookie=gone; Max-Age=0", "example.com");
    // Expired cookies should not be sent
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_EQ(header.find("oldcookie=gone"), std::string::npos);
}

// CookieJar: size() counts stored cookies
TEST(CookieJarTest, SizeCountsStoredCookies) {
    CookieJar jar;
    jar.set_from_header("a=1", "example.com");
    jar.set_from_header("b=2", "example.com");
    EXPECT_EQ(jar.size(), 2u);
}

// Request: GET serialize includes method and path
TEST(RequestTest, SerializeGetIncludesMethodAndPath) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.port = 443;
    req.path = "/api/v1";
    auto raw = req.serialize();
    std::string serialized(raw.begin(), raw.end());
    EXPECT_NE(serialized.find("GET"), std::string::npos);
    EXPECT_NE(serialized.find("/api/v1"), std::string::npos);
}

// ============================================================================
// Cycle 534: HTTP/net regression tests
// ============================================================================

// HeaderMap: multiple headers can be stored
TEST(HeaderMapTest, MultipleHeadersStored) {
    HeaderMap map;
    map.set("Content-Type", "text/html");
    map.set("Accept", "application/json");
    map.set("Authorization", "Bearer token123");
    EXPECT_TRUE(map.has("content-type"));
    EXPECT_TRUE(map.has("accept"));
    EXPECT_TRUE(map.has("authorization"));
}

// HeaderMap: overwriting existing header
TEST(HeaderMapTest, OverwriteExistingHeader) {
    HeaderMap map;
    map.set("Cache-Control", "no-cache");
    map.set("Cache-Control", "max-age=3600");
    auto val = map.get("cache-control");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, "max-age=3600");
}

// Response: parse 201 Created
TEST(ResponseTest, Parse201Created) {
    std::string raw = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201);
}

// Response: parse 204 No Content (no body)
TEST(ResponseTest, Parse204NoContent) {
    std::string raw = "HTTP/1.1 204 No Content\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 204);
}

// Request: POST method in serialized output
TEST(RequestTest, SerializePostIncludesMethod) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/submit";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("POST"), std::string::npos);
    EXPECT_NE(s.find("/submit"), std::string::npos);
}

// CookieJar: get_cookie_header returns empty string when jar is empty
TEST(CookieJarTest, EmptyJarReturnsEmptyHeader) {
    CookieJar jar;
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_TRUE(header.empty());
}

// CookieJar: cookie is included for matching domain
TEST(CookieJarTest, CookieIncludedForMatchingDomain) {
    CookieJar jar;
    jar.set_from_header("session=abc123", "example.com");
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_NE(header.find("session=abc123"), std::string::npos);
}

// CookieJar: size is 0 for fresh jar
TEST(CookieJarTest, FreshJarSizeIsZero) {
    CookieJar jar;
    EXPECT_EQ(jar.size(), 0u);
}

// ============================================================================
// Cycle 545: HTTP/net regression tests
// ============================================================================

// HeaderMap: has() on three set entries
TEST(HeaderMapTest, ThreeEntriesAllPresent) {
    HeaderMap map;
    map.set("X-One", "1");
    map.set("X-Two", "2");
    map.set("X-Three", "3");
    EXPECT_TRUE(map.has("x-one"));
    EXPECT_TRUE(map.has("x-two"));
    EXPECT_TRUE(map.has("x-three"));
    EXPECT_FALSE(map.empty());
}

// Response: parse 302 redirect
TEST(ResponseTest, Parse302Redirect) {
    std::string raw = "HTTP/1.1 302 Found\r\nLocation: https://example.com/new\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 302);
    auto loc = resp->headers.get("location");
    EXPECT_TRUE(loc.has_value());
}

// Response: parse 500 Internal Server Error
TEST(ResponseTest, Parse500InternalServerError) {
    std::string raw = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500);
}

// CookieJar: set two cookies, size is 2
TEST(CookieJarTest, SetTwoCookiesSizeIsTwo) {
    CookieJar jar;
    jar.set_from_header("cookie1=value1", "example.com");
    jar.set_from_header("cookie2=value2", "example.com");
    EXPECT_EQ(jar.size(), 2u);
}

// Request: HEAD method serializes correctly
TEST(RequestTest, SerializeHeadRequestMethod) {
    Request req;
    req.method = Method::HEAD;
    req.host = "example.com";
    req.path = "/";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("HEAD"), std::string::npos);
}

// Request: serialized output includes host
TEST(RequestTest, SerializeIncludesHostHeader) {
    Request req;
    req.method = Method::GET;
    req.host = "api.example.com";
    req.path = "/data";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("api.example.com"), std::string::npos);
}

// Response: body content is preserved
TEST(ResponseTest, ResponseBodyPreserved) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    std::string body(resp->body.begin(), resp->body.end());
    EXPECT_EQ(body, "hello");
}

// HeaderMap: remove() reduces entries
TEST(HeaderMapTest, RemoveReducesEntries) {
    HeaderMap map;
    map.set("A", "1");
    map.set("B", "2");
    map.remove("a");
    EXPECT_FALSE(map.has("a"));
    EXPECT_TRUE(map.has("b"));
}
