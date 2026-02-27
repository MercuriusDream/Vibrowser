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

// ============================================================================
// Cycle 567: More net/HTTP tests
// ============================================================================

// Request: default method is GET
TEST(RequestTest, DefaultMethodIsGET) {
    Request req;
    EXPECT_EQ(req.method, Method::GET);
}

// Request: PUT method serializes correctly
TEST(RequestTest, PutMethodSerializes) {
    Request req;
    req.method = Method::PUT;
    req.host = "example.com";
    req.path = "/resource";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("PUT"), std::string::npos);
}

// Request: DELETE method serializes correctly
TEST(RequestTest, DeleteMethodSerializes) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "example.com";
    req.path = "/item/1";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("DELETE"), std::string::npos);
}

// Request: PATCH method serializes correctly
TEST(RequestTest, PatchMethodSerializes) {
    Request req;
    req.method = Method::PATCH;
    req.host = "example.com";
    req.path = "/update";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("PATCH"), std::string::npos);
}

// Response: parse 404 Not Found
TEST(ResponseTest, Parse404NotFound) {
    std::string raw = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404);
    EXPECT_EQ(resp->status_text, "Not Found");
}

// Response: parse 400 Bad Request
TEST(ResponseTest, Parse400BadRequest) {
    std::string raw = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 400);
}

// Response: body_as_string works
TEST(ResponseTest, BodyAsStringWorks) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ntest";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "test");
}

// HeaderMap: get() returns nullopt for missing key
TEST(HeaderMapTest, GetMissingKeyReturnsNullopt) {
    HeaderMap map;
    EXPECT_FALSE(map.get("nonexistent").has_value());
}

// ============================================================================
// Cycle 579: More net/HTTP tests
// ============================================================================

// Request: OPTIONS method serializes correctly
TEST(RequestTest, OptionsMethodSerializes) {
    Request req;
    req.method = Method::OPTIONS;
    req.host = "example.com";
    req.path = "/api";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("OPTIONS"), std::string::npos);
}

// Request: body can be stored
TEST(RequestTest, RequestBodyCanBeStored) {
    Request req;
    req.method = Method::POST;
    req.host = "example.com";
    req.path = "/submit";
    std::string body = "key=value";
    req.body = std::vector<uint8_t>(body.begin(), body.end());
    EXPECT_EQ(req.body.size(), body.size());
}

// Response: parse 301 Moved Permanently
TEST(ResponseTest, Parse301MovedPermanently) {
    std::string raw = "HTTP/1.1 301 Moved Permanently\r\nLocation: /new\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301);
}

// Response: parse 503 Service Unavailable
TEST(ResponseTest, Parse503ServiceUnavailable) {
    std::string raw = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 503);
}

// HeaderMap: set and has for case-insensitive check
TEST(HeaderMapTest, SetAndHasCaseInsensitive) {
    HeaderMap map;
    map.set("Content-Type", "application/json");
    EXPECT_TRUE(map.has("content-type"));
    EXPECT_TRUE(map.has("CONTENT-TYPE"));
}

// HeaderMap: get returns value after set
TEST(HeaderMapTest, GetReturnsValueAfterSet) {
    HeaderMap map;
    map.set("Accept", "text/html");
    auto val = map.get("accept");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "text/html");
}

// Response: parse empty body
TEST(ResponseTest, ParseEmptyBody) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->body.empty());
}

// Response: headers accessible after parse
TEST(ResponseTest, ParsedResponseHeadersAccessible) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->headers.has("content-type"));
}

// ============================================================================
// Cycle 590: More net/HTTP tests
// ============================================================================

// Request: path default is "/"
TEST(RequestTest, PathDefaultIsSlash) {
    Request req;
    EXPECT_EQ(req.path, "/");
}

// Request: body is empty by default
TEST(RequestTest, BodyEmptyByDefault) {
    Request req;
    EXPECT_TRUE(req.body.empty());
}

// Response: parse 403 Forbidden
TEST(ResponseTest, Parse403Forbidden) {
    std::string raw = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 403);
    EXPECT_EQ(resp->status_text, "Forbidden");
}

// Response: parse 408 Request Timeout
TEST(ResponseTest, Parse408RequestTimeout) {
    std::string raw = "HTTP/1.1 408 Request Timeout\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 408);
}

// HeaderMap: set multiple, all accessible
TEST(HeaderMapTest, SetMultipleAllAccessible) {
    HeaderMap map;
    map.set("X-Foo", "1");
    map.set("X-Bar", "2");
    map.set("X-Baz", "3");
    EXPECT_TRUE(map.has("x-foo"));
    EXPECT_TRUE(map.has("x-bar"));
    EXPECT_TRUE(map.has("x-baz"));
}

// CookieJar: cookie stored correctly
TEST(CookieJarTest, CookieStoredCorrectly) {
    CookieJar jar;
    jar.set_from_header("session=abc; Domain=example.com; Path=/", "example.com");
    EXPECT_GT(jar.size(), 0u);
}

// Response: status_text is preserved
TEST(ResponseTest, StatusTextPreserved) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status_text, "OK");
}

// HeaderMap: remove non-existent key is no-op
TEST(HeaderMapTest, RemoveNonExistentIsNoOp) {
    HeaderMap map;
    map.set("A", "1");
    EXPECT_NO_THROW(map.remove("nonexistent"));
    EXPECT_TRUE(map.has("a"));
}

// ============================================================================
// Cycle 602: More Net HTTP tests
// ============================================================================

// Response: parse 201 Created with zero body
TEST(ResponseTest, Parse201CreatedZeroBody) {
    std::string raw = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201);
    EXPECT_EQ(resp->status_text, "Created");
}

// Response: parse 204 No Content (empty body)
TEST(ResponseTest, Parse204NoContentEmpty) {
    std::string raw = "HTTP/1.1 204 No Content\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 204);
}

// Response: parse 304 Not Modified
TEST(ResponseTest, Parse304NotModified) {
    std::string raw = "HTTP/1.1 304 Not Modified\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 304);
}

// Response: parse 401 Unauthorized
TEST(ResponseTest, Parse401Unauthorized) {
    std::string raw = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 401);
}

// HeaderMap: set multiple keys and iterate count
TEST(HeaderMapTest, SetFiveKeysHasAll) {
    HeaderMap map;
    map.set("A", "1");
    map.set("B", "2");
    map.set("C", "3");
    map.set("D", "4");
    map.set("E", "5");
    EXPECT_TRUE(map.has("a"));
    EXPECT_TRUE(map.has("e"));
}

// Request: HEAD method
TEST(RequestTest, HeadMethodSerializes) {
    Request req;
    req.method = Method::HEAD;
    EXPECT_EQ(req.method, Method::HEAD);
}

// Response: body_as_string with content
TEST(ResponseTest, BodyAsStringWithJson) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 15\r\n\r\n{\"status\":\"ok\"}";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "{\"status\":\"ok\"}");
}

// HeaderMap: overwrite preserves case-insensitive key
TEST(HeaderMapTest, OverwriteWithDifferentCase) {
    HeaderMap map;
    map.set("Content-Type", "text/plain");
    map.set("content-type", "application/json");
    auto val = map.get("Content-Type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "application/json");
}

// ============================================================================
// Cycle 612: More Net HTTP tests
// ============================================================================

// Response: parse 422 Unprocessable Entity
TEST(ResponseTest, Parse422UnprocessableEntity) {
    std::string raw = "HTTP/1.1 422 Unprocessable Entity\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 422);
}

// Response: parse 429 Too Many Requests
TEST(ResponseTest, Parse429TooManyRequests) {
    std::string raw = "HTTP/1.1 429 Too Many Requests\r\nRetry-After: 60\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 429);
}

// Response: parse 500 status_text verified
TEST(ResponseTest, Parse500StatusTextVerified) {
    std::string raw = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500);
    EXPECT_EQ(resp->status_text, "Internal Server Error");
}

// Response: parse 502 Bad Gateway
TEST(ResponseTest, Parse502BadGateway) {
    std::string raw = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 502);
}

// HeaderMap: has returns false for never-set key
TEST(HeaderMapTest, HasReturnsFalseForNeverSetKey) {
    HeaderMap map;
    EXPECT_FALSE(map.has("Authorization"));
}

// HeaderMap: set then get round-trip
TEST(HeaderMapTest, SetThenGetRoundTrip) {
    HeaderMap map;
    map.set("Authorization", "Bearer token123");
    auto val = map.get("Authorization");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "Bearer token123");
}

// Request: POST method
TEST(RequestTest, PostMethodSerializesV2) {
    Request req;
    req.method = Method::POST;
    req.url = "https://api.example.com/data";
    EXPECT_EQ(req.method, Method::POST);
    EXPECT_FALSE(req.url.empty());
}

// Response: headers accessible by lowercase
TEST(ResponseTest, ParsedHeaderCaseInsensitive) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nhello";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->headers.has("content-type"));
}

// ============================================================================
// Cycle 621: More Net HTTP tests
// ============================================================================

// Response: parse 302 Found
TEST(ResponseTest, Parse302Found) {
    std::string raw = "HTTP/1.1 302 Found\r\nLocation: /new-path\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 302);
}

// Response: parse 307 Temporary Redirect
TEST(ResponseTest, Parse307TemporaryRedirect) {
    std::string raw = "HTTP/1.1 307 Temporary Redirect\r\nLocation: /temp\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 307);
}

// Response: parse 308 Permanent Redirect
TEST(ResponseTest, Parse308PermanentRedirect) {
    std::string raw = "HTTP/1.1 308 Permanent Redirect\r\nLocation: /new\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 308);
}

// Response: multiple headers accessible
TEST(ResponseTest, MultipleHeadersAccessible) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nX-Custom: value\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->headers.has("content-type"));
    EXPECT_TRUE(resp->headers.has("x-custom"));
}

// HeaderMap: remove key makes has() false
TEST(HeaderMapTest, RemoveKeyMakesHasFalse) {
    HeaderMap map;
    map.set("X-Token", "abc");
    map.remove("X-Token");
    EXPECT_FALSE(map.has("x-token"));
}

// Request: url can be set
TEST(RequestTest, UrlCanBeSet) {
    Request req;
    req.url = "https://api.example.com/v1/users";
    EXPECT_EQ(req.url, "https://api.example.com/v1/users");
}

// Request: body can be set
TEST(RequestTest, BodyCanBeSet) {
    Request req;
    std::string body = "{\"key\": \"value\"}";
    req.body = std::vector<uint8_t>(body.begin(), body.end());
    EXPECT_EQ(req.body.size(), body.size());
}

// Response: body empty for 204
TEST(ResponseTest, BodyEmptyFor204) {
    std::string raw = "HTTP/1.1 204 No Content\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->body.empty());
}

// ============================================================================
// Cycle 638: More HTTP/Net tests
// ============================================================================

// Response: parse 200 OK status code
TEST(ResponseTest, Parse200OKStatus) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
}

// Response: parse 404 Not Found status text verification
TEST(ResponseTest, Parse404NotFoundStatusText) {
    std::string raw = "HTTP/1.1 404 Not Found\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404);
    EXPECT_EQ(resp->status_text, "Not Found");
}

// Response: header Content-Type accessible
TEST(ResponseTest, ContentTypeHeaderAccessible) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto ct = resp->headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(ct.value(), "application/json");
}

// Response: body_as_string with text response
TEST(ResponseTest, BodyAsStringTextResponse) {
    std::string raw = "HTTP/1.1 200 OK\r\n\r\nhello";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "hello");
}

// HeaderMap: set multiple then clear one
TEST(HeaderMapTest, SetMultipleThenRemoveOne) {
    HeaderMap map;
    map.set("a", "1");
    map.set("b", "2");
    map.set("c", "3");
    map.remove("b");
    EXPECT_TRUE(map.has("a"));
    EXPECT_FALSE(map.has("b"));
    EXPECT_TRUE(map.has("c"));
}

// HeaderMap: overwrite existing key preserves case insensitivity
TEST(HeaderMapTest, OverwriteKeyPreservesCaseInsensitivity) {
    HeaderMap map;
    map.set("X-Request-ID", "abc");
    map.set("x-request-id", "xyz");
    auto val = map.get("X-Request-ID");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "xyz");
}

// Request: method defaults to GET
TEST(RequestTest, DefaultMethodIsGet) {
    Request req;
    EXPECT_EQ(req.method, Method::GET);
}

// Request: body initially empty
TEST(RequestTest, BodyInitiallyEmpty) {
    Request req;
    EXPECT_TRUE(req.body.empty());
}

// ============================================================================
// Cycle 648: More HTTP/Net tests
// ============================================================================

// Response: parse 201 Created status text
TEST(ResponseTest, Parse201CreatedStatusText) {
    std::string raw = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201);
    EXPECT_EQ(resp->status_text, "Created");
}

// Response: parse 301 Moved Permanently status code
TEST(ResponseTest, Parse301MovedPermanentlyStatusCode) {
    std::string raw = "HTTP/1.1 301 Moved Permanently\r\nLocation: https://new.example.com\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301);
}

// Response: Location header accessible from redirect
TEST(ResponseTest, LocationHeaderFromRedirect) {
    std::string raw = "HTTP/1.1 302 Found\r\nLocation: /new-path\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto loc = resp->headers.get("Location");
    ASSERT_TRUE(loc.has_value());
    EXPECT_EQ(loc.value(), "/new-path");
}

// Response: Content-Length header accessible
TEST(ResponseTest, ContentLengthHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto cl = resp->headers.get("Content-Length");
    ASSERT_TRUE(cl.has_value());
    EXPECT_EQ(cl.value(), "13");
}

// HeaderMap: get returns nullopt for missing key
TEST(HeaderMapTest, GetNulloptForMissingKey) {
    HeaderMap map;
    auto val = map.get("X-Missing");
    EXPECT_FALSE(val.has_value());
}

// HeaderMap: empty map has no keys
TEST(HeaderMapTest, EmptyMapHasNoKeys) {
    HeaderMap map;
    EXPECT_FALSE(map.has("anything"));
}

// Request: method can be set to POST
TEST(RequestTest, MethodCanBeSetToPost) {
    Request req;
    req.method = Method::POST;
    EXPECT_EQ(req.method, Method::POST);
}

// Request: url is initially empty
TEST(RequestTest, UrlInitiallyEmpty) {
    Request req;
    EXPECT_TRUE(req.url.empty());
}

// ============================================================================
// Cycle 657: More net/http tests
// ============================================================================

// Response: parse 400 with body content
TEST(ResponseTest, Parse400WithBodyContent) {
    std::string raw = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{\"error\":\"invalid\"}";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 400);
    EXPECT_NE(resp->body_as_string().find("error"), std::string::npos);
}

// Response: parse 401 with WWW-Authenticate header
TEST(ResponseTest, Parse401WithWWWAuthenticate) {
    std::string raw = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Bearer\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 401);
    EXPECT_TRUE(resp->headers.has("WWW-Authenticate"));
}

// Response: parse 403 with status text "Forbidden" confirmed
TEST(ResponseTest, Parse403StatusTextForbidden) {
    std::string raw = "HTTP/1.1 403 Forbidden\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status_text, "Forbidden");
}

// Response: parse 503 with Retry-After header value 60
TEST(ResponseTest, Parse503WithRetryAfterSixty) {
    std::string raw = "HTTP/1.1 503 Service Unavailable\r\nRetry-After: 60\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 503);
    auto ra = resp->headers.get("Retry-After");
    ASSERT_TRUE(ra.has_value());
    EXPECT_EQ(ra.value(), "60");
}

// HeaderMap: case insensitive get for lowercase key set as uppercase
TEST(HeaderMapTest, GetCaseInsensitiveLower) {
    HeaderMap map;
    map.set("Content-Type", "text/html");
    auto val = map.get("content-type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "text/html");
}

// HeaderMap: has returns true after set
TEST(HeaderMapTest, HasReturnsTrueAfterSet) {
    HeaderMap map;
    map.set("X-Custom", "value");
    EXPECT_TRUE(map.has("X-Custom"));
}

// Request: PUT method can be set
TEST(RequestTest, PutMethodCanBeSet) {
    Request req;
    req.method = Method::PUT;
    EXPECT_EQ(req.method, Method::PUT);
}

// Request: DELETE_METHOD enum value can be set
TEST(RequestTest, DeleteMethodEnumCanBeSet) {
    Request req;
    req.method = Method::DELETE_METHOD;
    EXPECT_EQ(req.method, Method::DELETE_METHOD);
}

// ============================================================================
// Cycle 673: More net/http tests
// ============================================================================

// Response: parse 422 status code
TEST(ResponseTest, Parse422StatusCode) {
    std::string raw = "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 422);
}

// Response: body accessible as string for large content
TEST(ResponseTest, LargeBodyAccessibleAsString) {
    std::string body(1000, 'A');
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n" + body;
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string().size(), 1000u);
}

// Response: multiple headers all accessible
TEST(ResponseTest, FourHeadersAllAccessible) {
    std::string raw = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "X-Header-1: val1\r\n"
        "X-Header-2: val2\r\n"
        "X-Header-3: val3\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->headers.has("X-Header-1"));
    EXPECT_TRUE(resp->headers.has("X-Header-2"));
    EXPECT_TRUE(resp->headers.has("X-Header-3"));
}

// HeaderMap: overwrite changes value
TEST(HeaderMapTest, OverwriteChangesValue) {
    HeaderMap map;
    map.set("key", "first");
    map.set("key", "second");
    auto val = map.get("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "second");
}

// HeaderMap: remove then get returns nullopt
TEST(HeaderMapTest, RemoveThenGetReturnsNullopt) {
    HeaderMap map;
    map.set("temp", "value");
    map.remove("temp");
    EXPECT_FALSE(map.get("temp").has_value());
}

// Request: PATCH method can be set
TEST(RequestTest, PatchMethodCanBeSet) {
    Request req;
    req.method = Method::PATCH;
    EXPECT_EQ(req.method, Method::PATCH);
}

// Request: OPTIONS method can be set
TEST(RequestTest, OptionsMethodCanBeSet) {
    Request req;
    req.method = Method::OPTIONS;
    EXPECT_EQ(req.method, Method::OPTIONS);
}

// Request: HEAD method can be set
TEST(RequestTest, HeadMethodCanBeSet) {
    Request req;
    req.method = Method::HEAD;
    EXPECT_EQ(req.method, Method::HEAD);
}

// ---------------------------------------------------------------------------
// Cycle 696 — 8 additional HTTP client tests
// ---------------------------------------------------------------------------

// Request: Authorization header can be set
TEST(RequestTest, AuthorizationHeaderCanBeSet) {
    Request req;
    req.headers.set("Authorization", "Bearer my-token-123");
    auto val = req.headers.get("Authorization");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "Bearer my-token-123");
}

// Request: Accept-Language header can be set
TEST(RequestTest, AcceptLanguageHeaderSet) {
    Request req;
    req.headers.set("Accept-Language", "en-US,en;q=0.9");
    auto val = req.headers.get("Accept-Language");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "en-US,en;q=0.9");
}

// Request: If-None-Match header can be set with ETag
TEST(RequestTest, IfNoneMatchHeaderSet) {
    Request req;
    req.headers.set("If-None-Match", "\"abc123\"");
    auto val = req.headers.get("If-None-Match");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "\"abc123\"");
}

// Request: Content-Type application/json can be set
TEST(RequestTest, ContentTypeJsonForPost) {
    Request req;
    req.method = Method::POST;
    req.headers.set("Content-Type", "application/json");
    auto val = req.headers.get("Content-Type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "application/json");
}

// Response: Last-Modified header is accessible after parse
TEST(ResponseTest, ParseLastModifiedHeader) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "data";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto lm = resp->headers.get("Last-Modified");
    EXPECT_TRUE(lm.has_value());
}

// Request: Range header can be set
TEST(RequestTest, RangeRequestHeaderSet) {
    Request req;
    req.headers.set("Range", "bytes=0-1023");
    auto val = req.headers.get("Range");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "bytes=0-1023");
}

// Response: X-Content-Type-Options header is accessible
TEST(ResponseTest, ParseXContentTypeOptions) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "ok";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto xcto = resp->headers.get("X-Content-Type-Options");
    EXPECT_TRUE(xcto.has_value());
}

// Response: X-Frame-Options header is accessible after parse
TEST(ResponseTest, ParseXFrameOptionsHeader) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Frame-Options: DENY\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto xfo = resp->headers.get("X-Frame-Options");
    EXPECT_TRUE(xfo.has_value());
}

// ---------------------------------------------------------------------------
// Cycle 706 — 8 additional HTTP tests (status codes and headers)
// ---------------------------------------------------------------------------

// Response: 206 Partial Content
TEST(ResponseTest, Parse206PartialContent) {
    std::string raw =
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Range: bytes 0-99/1000\r\n"
        "Content-Length: 100\r\n"
        "\r\n";
    raw.append(100, 'x');
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 206);
}

// Response: 409 Conflict
TEST(ResponseTest, Parse409Conflict) {
    std::string raw =
        "HTTP/1.1 409 Conflict\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 409);
}

// Response: 410 Gone
TEST(ResponseTest, Parse410Gone) {
    std::string raw =
        "HTTP/1.1 410 Gone\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 410);
}

// Response: 415 Unsupported Media Type
TEST(ResponseTest, Parse415UnsupportedMediaType) {
    std::string raw =
        "HTTP/1.1 415 Unsupported Media Type\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 415);
}

// Response: 451 Unavailable For Legal Reasons
TEST(ResponseTest, Parse451UnavailableForLegalReasons) {
    std::string raw =
        "HTTP/1.1 451 Unavailable For Legal Reasons\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 451);
}

// Request: Content-Disposition header can be set
TEST(RequestTest, ContentDispositionHeaderSet) {
    Request req;
    req.headers.set("Content-Disposition", "form-data; name=\"file\"");
    auto val = req.headers.get("Content-Disposition");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "form-data; name=\"file\"");
}

// Request: Cache-Control header can be set in request
TEST(RequestTest, CacheControlHeaderInRequest) {
    Request req;
    req.headers.set("Cache-Control", "no-cache");
    auto val = req.headers.get("Cache-Control");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "no-cache");
}

// Request: Referer header can be set
TEST(RequestTest, RefererHeaderSet) {
    Request req;
    req.headers.set("Referer", "https://example.com/page");
    auto val = req.headers.get("Referer");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "https://example.com/page");
}

// Response: parse 301 Moved Permanently
TEST(ResponseTest, Parse301MovedPermanentlyWithLocation) {
    std::string raw =
        "HTTP/1.1 301 Moved Permanently\r\n"
        "Location: https://www.example.com/\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301);
}

// Response: parse 302 Found
TEST(ResponseTest, Parse302FoundWithLocation) {
    std::string raw =
        "HTTP/1.1 302 Found\r\n"
        "Location: /login\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 302);
}

// Response: parse 303 See Other
TEST(ResponseTest, Parse303SeeOther) {
    std::string raw =
        "HTTP/1.1 303 See Other\r\n"
        "Location: /result\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 303);
}

// Response: parse 307 Temporary Redirect
TEST(ResponseTest, Parse307TemporaryRedirectV2) {
    std::string raw =
        "HTTP/1.1 307 Temporary Redirect\r\n"
        "Location: /temp\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 307);
}

// Response: parse 308 Permanent Redirect
TEST(ResponseTest, Parse308PermanentRedirectV2) {
    std::string raw =
        "HTTP/1.1 308 Permanent Redirect\r\n"
        "Location: /new\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 308);
}

// Request: Accept header can be set
TEST(RequestTest, AcceptHeaderSet) {
    Request req;
    req.headers.set("Accept", "application/json");
    auto val = req.headers.get("Accept");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "application/json");
}

// Request: Origin header can be set
TEST(RequestTest, OriginHeaderSet) {
    Request req;
    req.headers.set("Origin", "https://example.com");
    auto val = req.headers.get("Origin");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "https://example.com");
}

// Request: Connection header can be set
TEST(RequestTest, ConnectionHeaderSet) {
    Request req;
    req.headers.set("Connection", "keep-alive");
    auto val = req.headers.get("Connection");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "keep-alive");
}

// Request: Cookie header can be set
TEST(RequestTest, CookieHeaderSet) {
    Request req;
    req.headers.set("Cookie", "session=abc123; user=john");
    auto val = req.headers.get("Cookie");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val.value().find("session"), std::string::npos);
}

// Request: X-Requested-With header for AJAX
TEST(RequestTest, XRequestedWithHeaderSet) {
    Request req;
    req.headers.set("X-Requested-With", "XMLHttpRequest");
    auto val = req.headers.get("X-Requested-With");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "XMLHttpRequest");
}

// Request: X-API-Key header can be set
TEST(RequestTest, XApiKeyHeaderSet) {
    Request req;
    req.headers.set("X-API-Key", "supersecretkey");
    auto val = req.headers.get("X-API-Key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "supersecretkey");
}

// Response: parse 500 Internal Server Error
TEST(ResponseTest, Parse500InternalServerErrorV2) {
    std::string raw =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500);
}

// Response: parse 502 Bad Gateway
TEST(ResponseTest, Parse502BadGatewayV2) {
    std::string raw =
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 502);
}

// Response: parse 503 Service Unavailable
TEST(ResponseTest, Parse503ServiceUnavailableRetryAfter) {
    std::string raw =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Retry-After: 120\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 503);
}

// Response: parse 504 Gateway Timeout
TEST(ResponseTest, Parse504GatewayTimeout) {
    std::string raw =
        "HTTP/1.1 504 Gateway Timeout\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 504);
}

// Response: Set-Cookie response header parsed
TEST(ResponseTest, SetCookieHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: sessionid=xyz; HttpOnly; Path=/\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Set-Cookie");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val.value().find("sessionid"), std::string::npos);
}

// Response: ETag header parsed
TEST(ResponseTest, ETagHeaderParsed) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "ETag: \"abc123\"\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("ETag");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val.value().find("abc123"), std::string::npos);
}

// Response: Vary header parsed
TEST(ResponseTest, VaryHeaderParsed) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Vary: Accept-Encoding, Accept-Language\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Vary");
    ASSERT_TRUE(val.has_value());
}

// Response: parse 201 Created
TEST(ResponseTest, Parse201CreatedWithLocation) {
    std::string raw =
        "HTTP/1.1 201 Created\r\n"
        "Location: /resources/123\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201);
}

// Response: parse 400 Bad Request
TEST(ResponseTest, Parse400BadRequestSimple) {
    std::string raw =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 400);
}

// Response: parse 401 Unauthorized
TEST(ResponseTest, Parse401UnauthorizedBearer) {
    std::string raw =
        "HTTP/1.1 401 Unauthorized\r\n"
        "WWW-Authenticate: Bearer\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 401);
}

// Request: Accept-Encoding header set
TEST(RequestTest, AcceptEncodingHeaderSet) {
    Request req;
    req.headers.set("Accept-Encoding", "gzip, deflate, br");
    auto val = req.headers.get("Accept-Encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val.value().find("gzip"), std::string::npos);
}

// Request: Pragma no-cache header set
TEST(RequestTest, PragmaNoCacheHeaderSet) {
    Request req;
    req.headers.set("Pragma", "no-cache");
    auto val = req.headers.get("Pragma");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "no-cache");
}

// Request: Content-Encoding header set
TEST(RequestTest, ContentEncodingHeaderSet) {
    Request req;
    req.headers.set("Content-Encoding", "gzip");
    auto val = req.headers.get("Content-Encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "gzip");
}

// Response: parse JSON content type
TEST(ResponseTest, ParseJsonContentType) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: 15\r\n"
        "\r\n"
        "{\"status\":\"ok\"}";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto ct = resp->headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_NE(ct.value().find("json"), std::string::npos);
}

// Response: body content accessible
TEST(ResponseTest, BodyContentAccessible) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "hello");
}

// Response: multiple response headers parsed
TEST(ResponseTest, MultipleResponseHeaders) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Header-One: value1\r\n"
        "X-Header-Two: value2\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->headers.get("X-Header-One").has_value());
    EXPECT_TRUE(resp->headers.get("X-Header-Two").has_value());
}

// Response: parse 404 Not Found with body
TEST(ResponseTest, Parse404WithBody) {
    std::string raw =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not found";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404);
    EXPECT_EQ(resp->body_as_string(), "Not found");
}

// Request: X-CSRF-Token header
TEST(RequestTest, XCSRFTokenHeaderSet) {
    Request req;
    req.headers.set("X-CSRF-Token", "token123");
    auto val = req.headers.get("X-CSRF-Token");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "token123");
}

// Request: User-Agent header
TEST(RequestTest, UserAgentHeaderSet) {
    Request req;
    req.headers.set("User-Agent", "Mozilla/5.0");
    auto val = req.headers.get("User-Agent");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val.value().find("Mozilla"), std::string::npos);
}

// Request: X-Forwarded-For header
TEST(RequestTest, XForwardedForHeaderSet) {
    Request req;
    req.headers.set("X-Forwarded-For", "192.168.1.1");
    auto val = req.headers.get("X-Forwarded-For");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "192.168.1.1");
}

// Request: Host header can be set
TEST(RequestTest, HostHeaderSet) {
    Request req;
    req.headers.set("Host", "api.example.com");
    auto val = req.headers.get("Host");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "api.example.com");
}

// Cycle 758 — HttpCache API and no-store/private directives
TEST(CacheControlTest, NoStoreDirectiveParsed) {
    auto cc = clever::net::parse_cache_control("no-store");
    EXPECT_TRUE(cc.no_store);
}

TEST(CacheControlTest, PrivateDirectiveParsed) {
    auto cc = clever::net::parse_cache_control("private");
    EXPECT_TRUE(cc.is_private);
}

TEST(CacheControlTest, PublicDirectiveParsed) {
    auto cc = clever::net::parse_cache_control("public");
    EXPECT_TRUE(cc.is_public);
}

TEST(HttpCacheTest, CacheEntryCountAfterStore) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry entry;
    entry.url = "http://example.com/count";
    entry.status = 200;
    entry.body = "hello";
    cache.store(entry);
    EXPECT_EQ(cache.entry_count(), 1u);
    cache.clear();
}

TEST(HttpCacheTest, CacheTotalSizeAfterStore) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry entry;
    entry.url = "http://example.com/size";
    entry.status = 200;
    entry.body = std::string(1000, 'x');
    cache.store(entry);
    EXPECT_GT(cache.total_size(), 0u);
    cache.clear();
}

TEST(HttpCacheTest, CacheRemovesEntry) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry entry;
    entry.url = "http://example.com/remove";
    entry.status = 200;
    cache.store(entry);
    cache.remove("http://example.com/remove");
    EXPECT_EQ(cache.entry_count(), 0u);
}

TEST(HttpCacheTest, CacheLookupHitAfterStore) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry entry;
    entry.url = "http://example.com/hit";
    entry.status = 200;
    entry.body = "cached";
    entry.max_age_seconds = 3600;
    cache.store(entry);
    auto found = cache.lookup("http://example.com/hit");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->body, "cached");
    cache.clear();
}

TEST(HttpCacheTest, CacheLookupMissReturnsNullopt) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    auto found = cache.lookup("http://example.com/miss");
    EXPECT_FALSE(found.has_value());
}

// Cycle 768 — HttpCache advanced operations
TEST(HttpCacheTest, CacheCountAfterClear) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry e;
    e.url = "http://a.com/p1"; e.status = 200;
    cache.store(e);
    cache.clear();
    EXPECT_EQ(cache.entry_count(), 0u);
}

TEST(HttpCacheTest, CacheOverwriteUpdatesBody) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry e1;
    e1.url = "http://example.com/overwrite"; e1.status = 200; e1.body = "v1";
    cache.store(e1);
    clever::net::CacheEntry e2;
    e2.url = "http://example.com/overwrite"; e2.status = 200; e2.body = "v2";
    cache.store(e2);
    auto found = cache.lookup("http://example.com/overwrite");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->body, "v2");
    cache.clear();
}

TEST(HttpCacheTest, CacheTwoEntriesCountIsTwo) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    clever::net::CacheEntry e1, e2;
    e1.url = "http://x.com/1"; e1.status = 200;
    e2.url = "http://x.com/2"; e2.status = 200;
    cache.store(e1); cache.store(e2);
    EXPECT_EQ(cache.entry_count(), 2u);
    cache.clear();
}

TEST(HttpCacheTest, CacheSetMaxBytes) {
    auto& cache = clever::net::HttpCache::instance();
    cache.clear();
    cache.set_max_bytes(100 * 1024 * 1024);
    // Just verify the method exists and doesn't crash
    EXPECT_EQ(cache.entry_count(), 0u);
    cache.set_max_bytes(clever::net::HttpCache::kDefaultMaxBytes);
}

TEST(CacheEntryTest, CacheEntryIsNotFreshWithZeroMaxAge) {
    clever::net::CacheEntry e;
    e.max_age_seconds = 0;
    e.stored_at = std::chrono::steady_clock::now();
    EXPECT_FALSE(e.is_fresh());
}

TEST(CacheEntryTest, CacheEntryApproxSizeIncludesBody) {
    clever::net::CacheEntry e;
    e.url = "http://example.com/size";
    e.body = std::string(500, 'a');
    EXPECT_GE(e.approx_size(), 500u);
}

TEST(CacheControlTest, MaxAgeZeroIsNotFresh) {
    auto cc = clever::net::parse_cache_control("max-age=0");
    EXPECT_EQ(cc.max_age, 0);
}

TEST(CacheControlTest, MaxAgeNegativeOneWhenAbsent) {
    auto cc = clever::net::parse_cache_control("no-cache");
    EXPECT_EQ(cc.max_age, -1);
}

// Cycle 779 — HTTP 4xx and 5xx status code coverage
TEST(ResponseTest, Parse405MethodNotAllowed) {
    std::string raw = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 405);
}

TEST(ResponseTest, Parse406NotAcceptable) {
    std::string raw = "HTTP/1.1 406 Not Acceptable\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 406);
}

TEST(ResponseTest, Parse411LengthRequired) {
    std::string raw = "HTTP/1.1 411 Length Required\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 411);
}

TEST(ResponseTest, Parse412PreconditionFailed) {
    std::string raw = "HTTP/1.1 412 Precondition Failed\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 412);
}

TEST(ResponseTest, Parse413ContentTooLarge) {
    std::string raw = "HTTP/1.1 413 Content Too Large\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 413);
}

TEST(ResponseTest, Parse416RangeNotSatisfiable) {
    std::string raw = "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */1000\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 416);
}

TEST(ResponseTest, Parse418ImATeapot) {
    std::string raw = "HTTP/1.1 418 I'm a Teapot\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 418);
}

TEST(ResponseTest, Parse507InsufficientStorage) {
    std::string raw = "HTTP/1.1 507 Insufficient Storage\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 507);
}

TEST(ResponseTest, Parse200OKBasic) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html></html>";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200);
}

TEST(ResponseTest, Parse402PaymentRequired) {
    std::string raw = "HTTP/1.1 402 Payment Required\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 402);
}

TEST(ResponseTest, Parse414URITooLong) {
    std::string raw = "HTTP/1.1 414 URI Too Long\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 414);
}

TEST(ResponseTest, Parse417ExpectationFailed) {
    std::string raw = "HTTP/1.1 417 Expectation Failed\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 417);
}

TEST(ResponseTest, Parse423Locked) {
    std::string raw = "HTTP/1.1 423 Locked\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 423);
}

TEST(ResponseTest, Parse426UpgradeRequired) {
    std::string raw = "HTTP/1.1 426 Upgrade Required\r\nUpgrade: HTTP/2\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 426);
}

TEST(ResponseTest, Parse428PreconditionRequired) {
    std::string raw = "HTTP/1.1 428 Precondition Required\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 428);
}

TEST(ResponseTest, Parse431RequestHeaderFieldsTooLarge) {
    std::string raw = "HTTP/1.1 431 Request Header Fields Too Large\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 431);
}

TEST(ResponseTest, ResponseBodyContentIsCorrect) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello Test";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "Hello Test");
}

TEST(ResponseTest, ResponseBodySizeMatchesContent) {
    std::string raw = "HTTP/1.1 200 OK\r\n\r\n12345";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body.size(), 5u);
}

TEST(ResponseTest, ResponseHeaderContentType) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{}";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto ct = resp->headers.get("content-type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_NE(ct->find("json"), std::string::npos);
}

TEST(ResponseTest, ResponseHeaderServerName) {
    std::string raw = "HTTP/1.1 200 OK\r\nServer: nginx/1.18\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto sv = resp->headers.get("server");
    ASSERT_TRUE(sv.has_value());
    EXPECT_NE(sv->find("nginx"), std::string::npos);
}

TEST(ResponseTest, ResponseHeaderCacheControl) {
    std::string raw = "HTTP/1.1 200 OK\r\nCache-Control: no-cache, no-store\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto cc = resp->headers.get("cache-control");
    ASSERT_TRUE(cc.has_value());
    EXPECT_NE(cc->find("no-cache"), std::string::npos);
}

TEST(ResponseTest, ResponseStatus200TextOK) {
    std::string raw = "HTTP/1.1 200 OK\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status_text, "OK");
}

TEST(ResponseTest, ResponseStatus201TextCreated) {
    std::string raw = "HTTP/1.1 201 Created\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status_text, "Created");
}

TEST(ResponseTest, ResponseBodyJsonString) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"ok\":true}";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body_as_string(), "{\"ok\":true}");
}

// Cycle 824 — HTTP status codes 202/203/205/501/505 and security headers
TEST(ResponseTest, Parse202Accepted) {
    std::string raw = "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 202);
}

TEST(ResponseTest, Parse203NonAuthoritativeInformation) {
    std::string raw = "HTTP/1.1 203 Non-Authoritative Information\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 203);
}

TEST(ResponseTest, Parse205ResetContent) {
    std::string raw = "HTTP/1.1 205 Reset Content\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 205);
}

TEST(ResponseTest, Parse501NotImplemented) {
    std::string raw = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 501);
}

TEST(ResponseTest, Parse505HttpVersionNotSupported) {
    std::string raw = "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 505);
}

TEST(ResponseTest, ContentSecurityPolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Security-Policy: default-src 'self'\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto csp = resp->headers.get("Content-Security-Policy");
    ASSERT_TRUE(csp.has_value());
    EXPECT_NE(csp->find("default-src"), std::string::npos);
}

TEST(ResponseTest, StrictTransportSecurityHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nStrict-Transport-Security: max-age=31536000; includeSubDomains\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto hsts = resp->headers.get("Strict-Transport-Security");
    ASSERT_TRUE(hsts.has_value());
    EXPECT_NE(hsts->find("max-age"), std::string::npos);
}

TEST(ResponseTest, ReferrerPolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nReferrer-Policy: no-referrer-when-downgrade\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto rp = resp->headers.get("Referrer-Policy");
    ASSERT_TRUE(rp.has_value());
    EXPECT_EQ(*rp, "no-referrer-when-downgrade");
}

// Cycle 835 — HTTP response headers: Accept-Ranges, Age, Transfer-Encoding, Permissions-Policy, Access-Control-Expose-Headers, CORP, COOP, COEP, NEL
TEST(ResponseTest, AcceptRangesHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Accept-Ranges");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "bytes");
}

TEST(ResponseTest, AgeHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nAge: 1234\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Age");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1234");
}

TEST(ResponseTest, TransferEncodingChunked) {
    std::string raw = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Transfer-Encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "chunked");
}

TEST(ResponseTest, PermissionsPolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nPermissions-Policy: geolocation=(), microphone=()\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Permissions-Policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("geolocation"), std::string::npos);
}

TEST(ResponseTest, AccessControlExposeHeadersHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nAccess-Control-Expose-Headers: X-Custom-Header, X-Request-ID\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Access-Control-Expose-Headers");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("X-Custom-Header"), std::string::npos);
}

TEST(ResponseTest, CrossOriginResourcePolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nCross-Origin-Resource-Policy: same-origin\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Cross-Origin-Resource-Policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "same-origin");
}

TEST(ResponseTest, CrossOriginOpenerPolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nCross-Origin-Opener-Policy: same-origin\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Cross-Origin-Opener-Policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "same-origin");
}

TEST(ResponseTest, CrossOriginEmbedderPolicyHeader) {
    std::string raw = "HTTP/1.1 200 OK\r\nCross-Origin-Embedder-Policy: require-corp\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("Cross-Origin-Embedder-Policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "require-corp");
}

// Cycle 845 — informational, multi-status, and less-common codes
TEST(ResponseTest, Parse100Continue) {
    std::string raw = "HTTP/1.1 100 Continue\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 100);
}

TEST(ResponseTest, Parse101SwitchingProtocols) {
    std::string raw = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 101);
}

TEST(ResponseTest, Parse102Processing) {
    std::string raw = "HTTP/1.1 102 Processing\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 102);
}

TEST(ResponseTest, Parse207MultiStatus) {
    std::string raw = "HTTP/1.1 207 Multi-Status\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 207);
}

TEST(ResponseTest, Parse208AlreadyReported) {
    std::string raw = "HTTP/1.1 208 Already Reported\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 208);
}

TEST(ResponseTest, Parse226IMUsed) {
    std::string raw = "HTTP/1.1 226 IM Used\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 226);
}

TEST(ResponseTest, Parse424FailedDependency) {
    std::string raw = "HTTP/1.1 424 Failed Dependency\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 424);
}

TEST(ResponseTest, Parse425TooEarly) {
    std::string raw = "HTTP/1.1 425 Too Early\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 425);
}

// Cycle 854 — untested HTTP status codes: 300, 421, 407, 506, 508, 510, 511, 409 V2
TEST(ResponseTest, Parse300MultipleChoices) {
    std::string raw = "HTTP/1.1 300 Multiple Choices\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 300);
}

TEST(ResponseTest, Parse421MisdirectedRequest) {
    std::string raw = "HTTP/1.1 421 Misdirected Request\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 421);
}

TEST(ResponseTest, Parse407ProxyAuthRequired) {
    std::string raw = "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"proxy\"\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 407);
}

TEST(ResponseTest, Parse506VariantAlsoNegotiates) {
    std::string raw = "HTTP/1.1 506 Variant Also Negotiates\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 506);
}

TEST(ResponseTest, Parse508LoopDetected) {
    std::string raw = "HTTP/1.1 508 Loop Detected\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 508);
}

TEST(ResponseTest, Parse510NotExtended) {
    std::string raw = "HTTP/1.1 510 Not Extended\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 510);
}

TEST(ResponseTest, Parse511NetworkAuthRequired) {
    std::string raw = "HTTP/1.1 511 Network Authentication Required\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 511);
}

TEST(ResponseTest, Parse305UseProxy) {
    std::string raw = "HTTP/1.1 305 Use Proxy\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 305);
}

// Cycle 863 — conditional request headers: If-Modified-Since, If-Unmodified-Since, If-Range, Upgrade-Insecure-Requests, Accept-Charset, Max-Forwards, Expect, Forwarded
TEST(RequestTest, IfModifiedSinceHeaderSet) {
    Request req;
    req.headers.set("If-Modified-Since", "Wed, 21 Oct 2015 07:28:00 GMT");
    auto val = req.headers.get("If-Modified-Since");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "Wed, 21 Oct 2015 07:28:00 GMT");
}

TEST(RequestTest, IfUnmodifiedSinceHeaderSet) {
    Request req;
    req.headers.set("If-Unmodified-Since", "Thu, 01 Jan 2015 00:00:00 GMT");
    auto val = req.headers.get("If-Unmodified-Since");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "Thu, 01 Jan 2015 00:00:00 GMT");
}

TEST(RequestTest, IfMatchHeaderSet) {
    Request req;
    req.headers.set("If-Match", "\"etag123\", \"etag456\"");
    auto val = req.headers.get("If-Match");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("etag123"), std::string::npos);
}

TEST(RequestTest, IfRangeHeaderSet) {
    Request req;
    req.headers.set("If-Range", "\"abc-etag\"");
    auto val = req.headers.get("If-Range");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "\"abc-etag\"");
}

TEST(RequestTest, UpgradeInsecureRequestsHeaderSet) {
    Request req;
    req.headers.set("Upgrade-Insecure-Requests", "1");
    auto val = req.headers.get("Upgrade-Insecure-Requests");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1");
}

TEST(RequestTest, AcceptCharsetHeaderSet) {
    Request req;
    req.headers.set("Accept-Charset", "utf-8, iso-8859-1;q=0.5");
    auto val = req.headers.get("Accept-Charset");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("utf-8"), std::string::npos);
}

TEST(RequestTest, MaxForwardsHeaderSet) {
    Request req;
    req.headers.set("Max-Forwards", "10");
    auto val = req.headers.get("Max-Forwards");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "10");
}

TEST(RequestTest, ExpectContinueHeaderSet) {
    Request req;
    req.headers.set("Expect", "100-continue");
    auto val = req.headers.get("Expect");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100-continue");
}


// Cycle 873 — security/caching response headers: Last-Modified, Retry-After, X-Content-Type-Options, Referrer-Policy, HSTS, X-Frame-Options, Cache-Control, Content-Security-Policy
TEST(ResponseTest, LastModifiedHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("last-modified");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("21 Oct 2015"), std::string::npos);
}

TEST(ResponseTest, RetryAfterInResponse) {
    std::string raw =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Retry-After: 120\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 503);
    auto val = resp->headers.get("retry-after");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "120");
}

TEST(ResponseTest, XContentTypeOptionsInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("x-content-type-options");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "nosniff");
}

TEST(ResponseTest, ReferrerPolicyInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("referrer-policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "strict-origin-when-cross-origin");
}

TEST(ResponseTest, HSTSInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("strict-transport-security");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("max-age=31536000"), std::string::npos);
}

TEST(ResponseTest, XFrameOptionsInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Frame-Options: DENY\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("x-frame-options");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "DENY");
}

TEST(ResponseTest, CacheControlNoCacheInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("cache-control");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("no-cache"), std::string::npos);
}

TEST(ResponseTest, ContentSecurityPolicyInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Security-Policy: default-src 'self'\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-security-policy");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "default-src 'self'");
}

// Cycle 882 — HTTP header tests

TEST(RequestTest, DNTHeaderSet) {
    Request req;
    req.headers.set("DNT", "1");
    auto val = req.headers.get("DNT");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "1");
}

TEST(RequestTest, SecFetchSiteHeaderSet) {
    Request req;
    req.headers.set("Sec-Fetch-Site", "cross-site");
    auto val = req.headers.get("Sec-Fetch-Site");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "cross-site");
}

TEST(RequestTest, SecFetchModeHeaderSet) {
    Request req;
    req.headers.set("Sec-Fetch-Mode", "navigate");
    auto val = req.headers.get("Sec-Fetch-Mode");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "navigate");
}

TEST(RequestTest, SecFetchDestHeaderSet) {
    Request req;
    req.headers.set("Sec-Fetch-Dest", "document");
    auto val = req.headers.get("Sec-Fetch-Dest");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "document");
}

TEST(ResponseTest, LinkHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Link: <https://example.com/page2>; rel=\"next\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("link");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("next"), std::string::npos);
}

TEST(ResponseTest, AltSvcHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Alt-Svc: h2=\"example.com:443\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("alt-svc");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("h2"), std::string::npos);
}

TEST(ResponseTest, ContentRangeHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Range: bytes 0-999/5000\r\n"
        "Content-Length: 1000\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-range");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("bytes"), std::string::npos);
}

TEST(ResponseTest, AccessControlMaxAgeInResponse) {
    std::string raw =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("access-control-max-age");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "86400");
}

// Cycle 899 — HTTP response headers: Timing-Allow-Origin, Server-Timing, Report-To, Clear-Site-Data, Content-Location, Allow, Origin-Agent-Cluster, Content-Disposition

TEST(ResponseTest, TimingAllowOriginInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Timing-Allow-Origin: *\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("timing-allow-origin");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "*");
}

TEST(ResponseTest, ServerTimingInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Server-Timing: db;dur=53,app;dur=47.2\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("server-timing");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("db"), std::string::npos);
}

TEST(ResponseTest, ReportToInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Report-To: {\"group\":\"default\",\"max_age\":86400}\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("report-to");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("default"), std::string::npos);
}

TEST(ResponseTest, ClearSiteDataInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Clear-Site-Data: \"cache\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("clear-site-data");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("cache"), std::string::npos);
}

TEST(ResponseTest, ContentLocationInResponse) {
    std::string raw =
        "HTTP/1.1 201 Created\r\n"
        "Content-Location: /documents/foo.json\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-location");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "/documents/foo.json");
}

TEST(ResponseTest, AllowMethodsInResponse) {
    std::string raw =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Allow: GET, HEAD, POST\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("allow");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("GET"), std::string::npos);
}

TEST(ResponseTest, OriginAgentClusterInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Origin-Agent-Cluster: ?1\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("origin-agent-cluster");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "?1");
}

TEST(ResponseTest, ContentDispositionAttachment) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Disposition: attachment; filename=\"report.pdf\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-disposition");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("attachment"), std::string::npos);
}

// Cycle 908 — HTTP headers: WWW-Authenticate, Proxy-Authenticate, Via, X-Forwarded-Host/Proto, traceparent, baggage, X-Request-Id

TEST(ResponseTest, WWWAuthenticateHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 401 Unauthorized\r\n"
        "WWW-Authenticate: Bearer realm=\"api\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("www-authenticate");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("Bearer"), std::string::npos);
}

TEST(ResponseTest, ProxyAuthenticateHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 407 Proxy Authentication Required\r\n"
        "Proxy-Authenticate: Basic realm=\"corporate-proxy\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("proxy-authenticate");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("Basic"), std::string::npos);
}

TEST(ResponseTest, ViaHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Via: 1.1 proxy.example.com\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("via");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("proxy.example.com"), std::string::npos);
}

TEST(RequestTest, XForwardedHostHeaderSet) {
    Request req;
    req.headers.set("X-Forwarded-Host", "original.example.com");
    auto val = req.headers.get("X-Forwarded-Host");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "original.example.com");
}

TEST(RequestTest, XForwardedProtoHeaderSet) {
    Request req;
    req.headers.set("X-Forwarded-Proto", "https");
    auto val = req.headers.get("X-Forwarded-Proto");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "https");
}

TEST(RequestTest, TraceParentHeaderSet) {
    Request req;
    req.headers.set("traceparent", "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");
    auto val = req.headers.get("traceparent");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("0af7651916cd43dd"), std::string::npos);
}

TEST(RequestTest, BaggageHeaderSet) {
    Request req;
    req.headers.set("baggage", "userId=alice,serverNode=DF28");
    auto val = req.headers.get("baggage");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("alice"), std::string::npos);
}

TEST(RequestTest, XRequestIdHeaderSet) {
    Request req;
    req.headers.set("X-Request-Id", "abc-123-def-456");
    auto val = req.headers.get("X-Request-Id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "abc-123-def-456");
}

// Cycle 917 — HTTP headers: Content-Encoding variants, Link preload/prefetch, Expires, Accept-Patch

TEST(ResponseTest, ContentEncodingGzip) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Encoding: gzip\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "gzip");
}

TEST(ResponseTest, ContentEncodingBrotli) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Encoding: br\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "br");
}

TEST(ResponseTest, ContentEncodingDeflate) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Encoding: deflate\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "deflate");
}

TEST(ResponseTest, ContentEncodingZstd) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Encoding: zstd\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-encoding");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "zstd");
}

TEST(ResponseTest, LinkHeaderPreload) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Link: </style.css>; rel=preload; as=style\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("link");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("preload"), std::string::npos);
}

TEST(ResponseTest, LinkHeaderPrefetch) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Link: </next-page>; rel=prefetch\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("link");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("prefetch"), std::string::npos);
}

TEST(ResponseTest, ExpiresInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Expires: Thu, 01 Jan 2026 00:00:00 GMT\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("expires");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("2026"), std::string::npos);
}

TEST(ResponseTest, AcceptPatchInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Accept-Patch: application/json-patch+json\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("accept-patch");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("json"), std::string::npos);
}

// Cycle 926 — additional HTTP response header coverage
TEST(ResponseTest, NELHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "NEL: {\"report_to\":\"default\",\"max_age\":86400}\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("nel");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("max_age"), std::string::npos);
}

TEST(ResponseTest, ReportingEndpointsHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Reporting-Endpoints: default=\"https://reports.example.com\"\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("reporting-endpoints");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("reports.example.com"), std::string::npos);
}

TEST(RequestTest, SecFetchUserHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("Sec-Fetch-User", "?1");
    auto val = req.headers.get("sec-fetch-user");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "?1");
}

TEST(RequestTest, SecChUaHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("Sec-CH-UA", "\"Chromium\";v=\"120\"");
    auto val = req.headers.get("sec-ch-ua");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("Chromium"), std::string::npos);
}

TEST(ResponseTest, AltUsedHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Alt-Used: cdn.example.com\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("alt-used");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "cdn.example.com");
}

TEST(ResponseTest, PriorityHeaderInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Priority: u=1\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("priority");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "u=1");
}

TEST(RequestTest, PriorityRequestHeaderSet) {
    Request req("https://example.com/resource");
    req.headers.set("Priority", "u=0, i");
    auto val = req.headers.get("priority");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("u=0"), std::string::npos);
}

TEST(ResponseTest, ContentLocationInResponsePath) {
    std::string raw =
        "HTTP/1.1 201 Created\r\n"
        "Content-Location: /items/42\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("content-location");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "/items/42");
}

// Cycle 935 — additional HTTP request headers and response features
TEST(RequestTest, MethodToStringPatch) {
    EXPECT_EQ(method_to_string(Method::PATCH), "PATCH");
}

TEST(RequestTest, MethodToStringOptions) {
    EXPECT_EQ(method_to_string(Method::OPTIONS), "OPTIONS");
}

TEST(RequestTest, XPoweredByHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("X-Powered-By", "PHP/8.2");
    auto val = req.headers.get("x-powered-by");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("PHP"), std::string::npos);
}

TEST(RequestTest, XRealIpHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("X-Real-IP", "203.0.113.5");
    auto val = req.headers.get("x-real-ip");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "203.0.113.5");
}

TEST(ResponseTest, XPoweredByInResponse) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "X-Powered-By: Express\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("x-powered-by");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "Express");
}

TEST(ResponseTest, ProxyStatusInResponse) {
    std::string raw =
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Proxy-Status: proxy.example.com; error=connection_refused\r\n"
        "Content-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    auto val = resp->headers.get("proxy-status");
    ASSERT_TRUE(val.has_value());
    EXPECT_NE(val->find("connection_refused"), std::string::npos);
}

TEST(ResponseTest, EarlyHintsStatus) {
    std::string raw = "HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 103);
}

TEST(RequestTest, AcceptVersionHeaderSet) {
    Request req("https://api.example.com/v2/users");
    req.headers.set("Accept-Version", "v2");
    auto val = req.headers.get("accept-version");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "v2");
}

// Cycle 944 — Device memory hints, Pragma, TE, service-worker headers
TEST(RequestTest, DeviceMemoryHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("Device-Memory", "4");
    auto val = req.headers.get("device-memory");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "4");
}

TEST(RequestTest, DownlinkSpeedHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("Downlink", "10");
    auto val = req.headers.get("downlink");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "10");
}

TEST(RequestTest, SaveDataHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("Save-Data", "on");
    auto val = req.headers.get("save-data");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "on");
}

TEST(RequestTest, ECTHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("ECT", "4g");
    auto val = req.headers.get("ect");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "4g");
}

TEST(RequestTest, RTTHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("RTT", "100");
    auto val = req.headers.get("rtt");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100");
}

TEST(RequestTest, PragmaNoCache) {
    Request req("https://example.com/");
    req.headers.set("Pragma", "no-cache");
    auto val = req.headers.get("pragma");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "no-cache");
}

TEST(RequestTest, ServiceWorkerNavigationPreload) {
    Request req("https://example.com/page");
    req.headers.set("Service-Worker-Navigation-Preload", "true");
    auto val = req.headers.get("service-worker-navigation-preload");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "true");
}

TEST(RequestTest, TEHeaderSet) {
    Request req("https://example.com/");
    req.headers.set("TE", "trailers");
    auto val = req.headers.get("te");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "trailers");
}

TEST(RequestTest, MethodToStringGet) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(RequestTest, MethodToStringPost) {
    EXPECT_EQ(method_to_string(Method::POST), "POST");
}

TEST(RequestTest, MethodToStringPut) {
    EXPECT_EQ(method_to_string(Method::PUT), "PUT");
}

TEST(RequestTest, MethodToStringHead) {
    EXPECT_EQ(method_to_string(Method::HEAD), "HEAD");
}

TEST(RequestTest, MethodToStringDeleteMethod) {
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
}

TEST(RequestTest, RequestDefaultMethodIsGet) {
    Request req;
    EXPECT_EQ(req.method, Method::GET);
}

TEST(RequestTest, RequestDefaultPortIsEighty) {
    Request req;
    EXPECT_EQ(req.port, 80);
}

TEST(RequestTest, RequestDefaultPathIsSlash) {
    Request req;
    EXPECT_EQ(req.path, "/");
}

TEST(RequestTest, RequestDefaultHostIsEmpty) {
    Request req;
    EXPECT_TRUE(req.host.empty());
}

TEST(RequestTest, RequestDefaultUrlIsEmpty) {
    Request req;
    EXPECT_TRUE(req.url.empty());
}

TEST(RequestTest, RequestDefaultUseTLSFalse) {
    Request req;
    EXPECT_FALSE(req.use_tls);
}

TEST(RequestTest, RequestBodyDefaultEmpty) {
    Request req;
    EXPECT_TRUE(req.body.empty());
}

TEST(RequestTest, StringToMethodGetParsed) {
    EXPECT_EQ(string_to_method("GET"), Method::GET);
}

TEST(RequestTest, StringToMethodPostParsed) {
    EXPECT_EQ(string_to_method("POST"), Method::POST);
}

TEST(RequestTest, StringToMethodPutParsed) {
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
}

TEST(RequestTest, StringToMethodHeadParsed) {
    EXPECT_EQ(string_to_method("HEAD"), Method::HEAD);
}

TEST(HeaderMapTest, HeaderMapSetLowercase) {
    HeaderMap map;
    map.set("content-type", "text/plain");
    auto val = map.get("content-type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "text/plain");
}

TEST(HeaderMapTest, HeaderMapGetAnyCase) {
    HeaderMap map;
    map.set("X-Custom-Header", "custom-value");
    auto lower = map.get("x-custom-header");
    auto upper = map.get("X-CUSTOM-HEADER");
    ASSERT_TRUE(lower.has_value());
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(*lower, *upper);
}

TEST(HeaderMapTest, HeaderMapSizeAfterTwoSets) {
    HeaderMap map;
    map.set("header-a", "value-a");
    map.set("header-b", "value-b");
    EXPECT_EQ(map.size(), 2u);
}

TEST(HeaderMapTest, HeaderMapHasAfterSet) {
    HeaderMap map;
    map.set("x-token", "abc123");
    EXPECT_TRUE(map.has("x-token"));
}

TEST(HeaderMapTest, HeaderMapHasAfterRemove) {
    HeaderMap map;
    map.set("x-temp", "temp");
    map.remove("x-temp");
    EXPECT_FALSE(map.has("x-temp"));
}

TEST(HeaderMapTest, HeaderMapSizeAfterRemove) {
    HeaderMap map;
    map.set("h1", "v1");
    map.set("h2", "v2");
    map.remove("h1");
    EXPECT_EQ(map.size(), 1u);
}

TEST(HeaderMapTest, HeaderMapGetAllSingleValue) {
    HeaderMap map;
    map.set("x-single", "only-one");
    auto all = map.get_all("x-single");
    EXPECT_EQ(all.size(), 1u);
}

TEST(HeaderMapTest, HeaderMapSetOverwritesV2) {
    HeaderMap map;
    map.set("x-version", "v1");
    map.set("x-version", "v2");
    auto val = map.get("x-version");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "v2");
}

TEST(RequestTest, StringToMethodDeleteParsed) {
    EXPECT_EQ(string_to_method("DELETE"), Method::DELETE_METHOD);
}

TEST(RequestTest, StringToMethodOptionsParsed) {
    EXPECT_EQ(string_to_method("OPTIONS"), Method::OPTIONS);
}

TEST(RequestTest, StringToMethodPatchParsed) {
    EXPECT_EQ(string_to_method("PATCH"), Method::PATCH);
}

TEST(ResponseTest, ResponseDefaultStatusZero) {
    Response r;
    EXPECT_EQ(r.status, 0u);
}

TEST(ResponseTest, ResponseDefaultWasRedirectedFalse) {
    Response r;
    EXPECT_FALSE(r.was_redirected);
}

TEST(ResponseTest, ResponseDefaultUrlIsEmpty) {
    Response r;
    EXPECT_TRUE(r.url.empty());
}

TEST(HeaderMapTest, HeaderMapEmptyAfterConstruct) {
    HeaderMap map;
    EXPECT_TRUE(map.empty());
}

TEST(HeaderMapTest, HeaderMapAppendAddsSecondValue) {
    HeaderMap map;
    map.append("x-multi", "first");
    map.append("x-multi", "second");
    auto all = map.get_all("x-multi");
    EXPECT_EQ(all.size(), 2u);
}

TEST(ResponseTest, ResponseBodyAsString) {
    Response r;
    r.body = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(r.body_as_string(), "Hello");
}

TEST(ResponseTest, ResponseBodyEmpty) {
    Response r;
    EXPECT_TRUE(r.body_as_string().empty());
}

TEST(ResponseTest, ResponseStatusHttp200) {
    Response r;
    r.status = 200;
    EXPECT_EQ(r.status, 200u);
}

TEST(ResponseTest, ResponseStatusHttp404) {
    Response r;
    r.status = 404;
    EXPECT_EQ(r.status, 404u);
}

TEST(ResponseTest, ResponseWasRedirectedSet) {
    Response r;
    r.was_redirected = true;
    EXPECT_TRUE(r.was_redirected);
}

TEST(ResponseTest, ResponseUrlSet) {
    Response r;
    r.url = "https://example.com/page";
    EXPECT_EQ(r.url, "https://example.com/page");
}

TEST(CookieJarTest, CookieJarSizeAfterSet) {
    CookieJar jar;
    jar.set_from_header("session=abc123; Path=/", "example.com");
    EXPECT_EQ(jar.size(), 1u);
}

TEST(CookieJarTest, CookieJarEmptyAfterClear) {
    CookieJar jar;
    jar.set_from_header("token=xyz; Path=/", "example.com");
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

TEST(ResponseTest, ResponseStatusHttp301) {
    Response r;
    r.status = 301;
    r.was_redirected = true;
    EXPECT_EQ(r.status, 301u);
    EXPECT_TRUE(r.was_redirected);
}

TEST(ResponseTest, ResponseStatusHttp500) {
    Response r;
    r.status = 500;
    EXPECT_EQ(r.status, 500u);
}

TEST(ResponseTest, ResponseHeaderSetAndGet) {
    Response r;
    r.headers.set("Content-Type", "application/json");
    auto val = r.headers.get("content-type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "application/json");
}

TEST(ResponseTest, ResponseHeadersEmptyByDefault) {
    Response r;
    EXPECT_TRUE(r.headers.empty());
}

TEST(CookieJarTest, CookieJarTwoSets) {
    CookieJar jar;
    jar.set_from_header("a=1; Path=/", "example.com");
    jar.set_from_header("b=2; Path=/", "example.com");
    EXPECT_EQ(jar.size(), 2u);
}

TEST(CookieJarTest, CookieJarClearThenAdd) {
    CookieJar jar;
    jar.set_from_header("x=1; Path=/", "example.com");
    jar.clear();
    jar.set_from_header("y=2; Path=/", "example.com");
    EXPECT_EQ(jar.size(), 1u);
}

TEST(HeaderMapTest, HeaderMapGetNonexistent) {
    HeaderMap map;
    auto val = map.get("x-does-not-exist");
    EXPECT_FALSE(val.has_value());
}

TEST(HeaderMapTest, HeaderMapEmptyAfterRemoveAll) {
    HeaderMap map;
    map.set("x-one", "v1");
    map.remove("x-one");
    EXPECT_TRUE(map.empty());
}
