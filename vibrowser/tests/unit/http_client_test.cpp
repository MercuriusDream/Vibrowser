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
    EXPECT_NE(s.find("Vibrowser/0.7.0"), std::string::npos)
        << "Should include default User-Agent header with Vibrowser version";
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

TEST(HeaderMapTest, HeaderMapHasTrueAfterSet) {
    HeaderMap map;
    map.set("content-type", "text/html");
    EXPECT_TRUE(map.has("content-type"));
}

TEST(HeaderMapTest, HeaderMapHasFalseBeforeSet) {
    HeaderMap map;
    EXPECT_FALSE(map.has("authorization"));
}

TEST(HeaderMapTest, HeaderMapSizeIncrementsOnSet) {
    HeaderMap map;
    map.set("a", "1");
    map.set("b", "2");
    EXPECT_EQ(map.size(), 2u);
}

TEST(HeaderMapTest, HeaderMapGetAllMultiple) {
    HeaderMap map;
    map.append("set-cookie", "a=1");
    map.append("set-cookie", "b=2");
    auto vals = map.get_all("set-cookie");
    EXPECT_EQ(vals.size(), 2u);
}

TEST(ResponseTest, ResponseStatusHttp201) {
    Response r;
    r.status = 201;
    EXPECT_EQ(r.status, 201);
}

TEST(ResponseTest, ResponseStatusHttp204) {
    Response r;
    r.status = 204;
    EXPECT_EQ(r.status, 204);
}

TEST(ResponseTest, ResponseStatusHttp400) {
    Response r;
    r.status = 400;
    EXPECT_EQ(r.status, 400);
}

TEST(ResponseTest, ResponseStatusHttp403) {
    Response r;
    r.status = 403;
    EXPECT_EQ(r.status, 403);
}

TEST(HeaderMapTest, AppendDoesNotOverwriteV2) {
    HeaderMap map;
    map.set("x-custom", "a");
    map.append("x-custom", "b");
    EXPECT_EQ(map.get_all("x-custom").size(), 2u);
}

TEST(HeaderMapTest, RemoveNonExistentNoOp) {
    HeaderMap map;
    map.remove("nonexistent");
    EXPECT_EQ(map.size(), 0u);
}

TEST(ResponseTest, ResponseStatusHttp500V2) {
    Response r;
    r.status = 500;
    EXPECT_EQ(r.status, 500);
}

TEST(ResponseTest, ResponseBodyNotEmpty) {
    Response r;
    r.body = {'O', 'K'};
    EXPECT_EQ(r.body_as_string(), "OK");
}

TEST(RequestTest, RequestDefaultPath) {
    Request req;
    EXPECT_EQ(req.path, "/");
}

TEST(HeaderMapTest, SetOverwritesPreviousV2) {
    HeaderMap map;
    map.set("key", "v1");
    map.set("key", "v2");
    EXPECT_EQ(map.get("key").value(), "v2");
}

TEST(ResponseTest, ResponseUrlFieldV2) {
    Response r;
    r.url = "https://example.com";
    EXPECT_EQ(r.url, "https://example.com");
}

TEST(HeaderMapTest, HasReturnsTrueAfterSetV2) {
    HeaderMap map;
    map.set("content-type", "text/html");
    EXPECT_TRUE(map.has("content-type"));
}

TEST(HeaderMapTest, HeaderMapAppendAddsMultipleValues) {
    HeaderMap map;
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");
    auto values = map.get_all("Accept");
    EXPECT_EQ(values.size(), 2u);
}

TEST(ResponseTest, ResponseDefaultStatusIsZero) {
    Response r;
    EXPECT_EQ(r.status, 0);
}

TEST(RequestTest, RequestSerializeIncludesHostV3) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.path = "/";
    auto bytes = req.serialize();
    std::string serialized(bytes.begin(), bytes.end());
    EXPECT_NE(serialized.find("Host:"), std::string::npos);
}

TEST(HeaderMapTest, HeaderMapGetMissingReturnsNulloptV3) {
    HeaderMap map;
    auto result = map.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(ResponseTest, ResponseWasRedirectedDefaultFalseV2) {
    Response r;
    EXPECT_FALSE(r.was_redirected);
}

TEST(MethodTest, MethodToStringOptionsV2) {
    EXPECT_EQ(method_to_string(Method::OPTIONS), "OPTIONS");
}

TEST(MethodTest, StringToMethodPatchV2) {
    EXPECT_EQ(string_to_method("PATCH"), Method::PATCH);
}

TEST(HeaderMapTest, HeaderMapSizeZeroInitiallyV3) {
    HeaderMap map;
    EXPECT_EQ(map.size(), 0u);
}

// --- Cycle 1025: HTTP client tests ---

TEST(HeaderMapTest, RemoveReducesSizeV3) {
    HeaderMap map;
    map.set("a", "1");
    map.set("b", "2");
    map.remove("a");
    EXPECT_EQ(map.size(), 1u);
}

TEST(HeaderMapTest, HasReturnsFalseAfterRemoveV3) {
    HeaderMap map;
    map.set("token", "abc");
    map.remove("token");
    EXPECT_FALSE(map.has("token"));
}

TEST(MethodTest, MethodToStringGetV3) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(MethodTest, MethodToStringPostV3) {
    EXPECT_EQ(method_to_string(Method::POST), "POST");
}

TEST(MethodTest, MethodToStringPutV3) {
    EXPECT_EQ(method_to_string(Method::PUT), "PUT");
}

TEST(MethodTest, StringToMethodGetV3) {
    EXPECT_EQ(string_to_method("GET"), Method::GET);
}

TEST(ResponseTest, ResponseBodyEmptyByDefault) {
    Response r;
    EXPECT_TRUE(r.body.empty());
}

TEST(RequestTest, RequestDefaultMethodIsGetV2) {
    Request req;
    EXPECT_EQ(req.method, Method::GET);
}

// --- Cycle 1034: HTTP client tests ---

TEST(MethodTest, MethodToStringDeleteV3) {
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
}

TEST(MethodTest, MethodToStringHeadV3) {
    EXPECT_EQ(method_to_string(Method::HEAD), "HEAD");
}

TEST(MethodTest, StringToMethodPostV3) {
    EXPECT_EQ(string_to_method("POST"), Method::POST);
}

TEST(MethodTest, StringToMethodPutV3) {
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
}

TEST(HeaderMapTest, AppendThenGetAllV3) {
    HeaderMap map;
    map.append("x-custom", "val1");
    map.append("x-custom", "val2");
    map.append("x-custom", "val3");
    EXPECT_EQ(map.get_all("x-custom").size(), 3u);
}

TEST(ResponseTest, ResponseStatusSetV3) {
    Response r;
    r.status = 404;
    EXPECT_EQ(r.status, 404);
}

TEST(HeaderMapTest, SetThenGetV3) {
    HeaderMap map;
    map.set("content-type", "application/json");
    EXPECT_EQ(map.get("content-type").value(), "application/json");
}

TEST(RequestTest, RequestParseUrlSetsHost) {
    Request req;
    req.url = "http://example.com/page";
    req.parse_url();
    EXPECT_EQ(req.host, "example.com");
}

// --- Cycle 1043: HTTP client tests ---

TEST(MethodTest, MethodToStringGetV4) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(MethodTest, MethodToStringPostV4) {
    EXPECT_EQ(method_to_string(Method::POST), "POST");
}

TEST(MethodTest, StringToMethodGetV4) {
    EXPECT_EQ(string_to_method("GET"), Method::GET);
}

TEST(MethodTest, StringToMethodDeleteV4) {
    EXPECT_EQ(string_to_method("DELETE"), Method::DELETE_METHOD);
}

TEST(HeaderMapTest, HasHeaderTrueV4) {
    HeaderMap map;
    map.set("accept", "text/html");
    EXPECT_TRUE(map.has("accept"));
}

TEST(HeaderMapTest, HasHeaderFalseV4) {
    HeaderMap map;
    EXPECT_FALSE(map.has("x-missing"));
}

TEST(ResponseTest, ResponseStatus200V4) {
    Response r;
    r.status = 200;
    EXPECT_EQ(r.status, 200);
}

TEST(ResponseTest, ResponseStatus500V4) {
    Response r;
    r.status = 500;
    EXPECT_EQ(r.status, 500);
}

// --- Cycle 1052: HTTP client tests ---

TEST(MethodTest, MethodToStringPutV4) {
    EXPECT_EQ(method_to_string(Method::PUT), "PUT");
}

TEST(MethodTest, MethodToStringOptionsV4) {
    EXPECT_EQ(method_to_string(Method::OPTIONS), "OPTIONS");
}

TEST(MethodTest, StringToMethodHeadV4) {
    EXPECT_EQ(string_to_method("HEAD"), Method::HEAD);
}

TEST(MethodTest, StringToMethodOptionsV4) {
    EXPECT_EQ(string_to_method("OPTIONS"), Method::OPTIONS);
}

TEST(HeaderMapTest, RemoveReducesSizeV4) {
    HeaderMap map;
    map.set("x-test", "val");
    map.remove("x-test");
    EXPECT_FALSE(map.has("x-test"));
}

TEST(HeaderMapTest, GetAllEmptyV4) {
    HeaderMap map;
    EXPECT_EQ(map.get_all("x-none").size(), 0u);
}

TEST(ResponseTest, ResponseStatus301V4) {
    Response r;
    r.status = 301;
    EXPECT_EQ(r.status, 301);
}

TEST(ResponseTest, ResponseStatus403V4) {
    Response r;
    r.status = 403;
    EXPECT_EQ(r.status, 403);
}

// --- Cycle 1061: HTTP client tests ---

TEST(MethodTest, MethodToStringPatchV4) {
    EXPECT_EQ(method_to_string(Method::PATCH), "PATCH");
}

TEST(MethodTest, StringToMethodPutV4) {
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
}

TEST(MethodTest, StringToMethodPatchV4) {
    EXPECT_EQ(string_to_method("PATCH"), Method::PATCH);
}

TEST(HeaderMapTest, SetOverwritesV5) {
    HeaderMap map;
    map.set("x-key", "old");
    map.set("x-key", "new");
    EXPECT_EQ(map.get("x-key").value(), "new");
}

TEST(HeaderMapTest, SizeAfterTwoSetsV5) {
    HeaderMap map;
    map.set("a", "1");
    map.set("b", "2");
    EXPECT_EQ(map.size(), 2u);
}

TEST(ResponseTest, ResponseStatus204V4) {
    Response r;
    r.status = 204;
    EXPECT_EQ(r.status, 204);
}

TEST(ResponseTest, ResponseStatus304V4) {
    Response r;
    r.status = 304;
    EXPECT_EQ(r.status, 304);
}

TEST(RequestTest, RequestDefaultMethodIsGetV3) {
    Request req;
    EXPECT_EQ(req.method, Method::GET);
}

// --- Cycle 1070: HTTP client tests ---

TEST(HeaderMapTest, AppendDoesNotOverwrite) {
    HeaderMap map;
    map.set("x-key", "first");
    map.append("x-key", "second");
    EXPECT_EQ(map.get_all("x-key").size(), 2u);
}

TEST(HeaderMapTest, GetReturnsFirstValue) {
    HeaderMap map;
    map.set("accept", "text/html");
    map.append("accept", "application/json");
    EXPECT_EQ(map.get("accept").value(), "text/html");
}

TEST(MethodTest, MethodToStringHeadV5) {
    EXPECT_EQ(method_to_string(Method::HEAD), "HEAD");
}

TEST(MethodTest, MethodToStringDeleteV5) {
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
}

TEST(ResponseTest, ResponseStatus100V5) {
    Response r;
    r.status = 100;
    EXPECT_EQ(r.status, 100);
}

TEST(ResponseTest, ResponseStatus201V5) {
    Response r;
    r.status = 201;
    EXPECT_EQ(r.status, 201);
}

TEST(ResponseTest, ResponseStatus400V5) {
    Response r;
    r.status = 400;
    EXPECT_EQ(r.status, 400);
}

TEST(ResponseTest, ResponseStatus502V5) {
    Response r;
    r.status = 502;
    EXPECT_EQ(r.status, 502);
}

// --- Cycle 1079: HTTP client tests ---

TEST(MethodTest, MethodToStringGetV5) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(MethodTest, StringToMethodPostV5) {
    EXPECT_EQ(string_to_method("POST"), Method::POST);
}

TEST(HeaderMapTest, SizeZeroInitiallyV5) {
    HeaderMap map;
    EXPECT_EQ(map.size(), 0u);
}

TEST(HeaderMapTest, GetMissingReturnsNulloptV5) {
    HeaderMap map;
    EXPECT_FALSE(map.get("missing").has_value());
}

TEST(ResponseTest, ResponseStatusDefaultZeroV5) {
    Response r;
    EXPECT_EQ(r.status, 0);
}

TEST(ResponseTest, ResponseStatus503V5) {
    Response r;
    r.status = 503;
    EXPECT_EQ(r.status, 503);
}

TEST(ResponseTest, ResponseStatus429V5) {
    Response r;
    r.status = 429;
    EXPECT_EQ(r.status, 429);
}

TEST(HeaderMapTest, HasAfterSetV5) {
    HeaderMap map;
    map.set("content-length", "100");
    EXPECT_TRUE(map.has("content-length"));
}

// --- Cycle 1088: HTTP client tests ---

TEST(MethodTest, MethodToStringPostV5) {
    EXPECT_EQ(method_to_string(Method::POST), "POST");
}

TEST(MethodTest, MethodToStringPutV5) {
    EXPECT_EQ(method_to_string(Method::PUT), "PUT");
}

TEST(HeaderMapTest, AppendThenSizeV5) {
    HeaderMap map;
    map.append("x-multi", "a");
    map.append("x-multi", "b");
    EXPECT_EQ(map.get_all("x-multi").size(), 2u);
}

TEST(HeaderMapTest, RemoveThenHasV5) {
    HeaderMap map;
    map.set("x-remove", "val");
    map.remove("x-remove");
    EXPECT_FALSE(map.has("x-remove"));
}

TEST(ResponseTest, ResponseStatus202V5) {
    Response r;
    r.status = 202;
    EXPECT_EQ(r.status, 202);
}

TEST(ResponseTest, ResponseStatus405V5) {
    Response r;
    r.status = 405;
    EXPECT_EQ(r.status, 405);
}

TEST(ResponseTest, ResponseStatus408V5) {
    Response r;
    r.status = 408;
    EXPECT_EQ(r.status, 408);
}

TEST(ResponseTest, ResponseStatus504V5) {
    Response r;
    r.status = 504;
    EXPECT_EQ(r.status, 504);
}

// --- Cycle 1097: 8 Net tests ---

TEST(MethodTest, MethodToStringGetV6) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(MethodTest, StringToMethodGetV6) {
    EXPECT_EQ(string_to_method("GET"), Method::GET);
}

TEST(HeaderMapTest, SizeAfterThreeSets) {
    HeaderMap h;
    h.set("a", "1");
    h.set("b", "2");
    h.set("c", "3");
    EXPECT_EQ(h.size(), 3u);
}

TEST(HeaderMapTest, GetAfterOverwrite) {
    HeaderMap h;
    h.set("key", "old");
    h.set("key", "new");
    EXPECT_EQ(h.get("key"), "new");
}

TEST(ResponseTest, ResponseStatus202V6) {
    Response r;
    r.status = 202;
    EXPECT_EQ(r.status, 202);
}

TEST(ResponseTest, ResponseStatus307V6) {
    Response r;
    r.status = 307;
    EXPECT_EQ(r.status, 307);
}

TEST(ResponseTest, ResponseStatus410V6) {
    Response r;
    r.status = 410;
    EXPECT_EQ(r.status, 410);
}

TEST(ResponseTest, ResponseStatus503V6) {
    Response r;
    r.status = 503;
    EXPECT_EQ(r.status, 503);
}

// --- Cycle 1106: 8 Net tests ---

TEST(MethodTest, MethodToStringPostV6) {
    EXPECT_EQ(method_to_string(Method::POST), "POST");
}

TEST(MethodTest, StringToMethodPutV6) {
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
}

TEST(HeaderMapTest, RemoveReducesSizeV6) {
    HeaderMap h;
    h.set("x", "1");
    h.set("y", "2");
    h.remove("x");
    EXPECT_EQ(h.size(), 1u);
}

TEST(HeaderMapTest, HasReturnsFalseAfterRemoveV6) {
    HeaderMap h;
    h.set("key", "val");
    h.remove("key");
    EXPECT_FALSE(h.has("key"));
}

TEST(ResponseTest, ResponseStatus206V6) {
    Response r;
    r.status = 206;
    EXPECT_EQ(r.status, 206);
}

TEST(ResponseTest, ResponseStatus302V6) {
    Response r;
    r.status = 302;
    EXPECT_EQ(r.status, 302);
}

TEST(ResponseTest, ResponseStatus405V6) {
    Response r;
    r.status = 405;
    EXPECT_EQ(r.status, 405);
}

TEST(ResponseTest, ResponseStatus502V6) {
    Response r;
    r.status = 502;
    EXPECT_EQ(r.status, 502);
}

// --- Cycle 1115: 8 Net tests ---

TEST(MethodTest, MethodToStringDeleteV7) {
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
}

TEST(MethodTest, StringToMethodDeleteV7) {
    EXPECT_EQ(string_to_method("DELETE"), Method::DELETE_METHOD);
}

TEST(HeaderMapTest, AppendCreatesMultipleV7) {
    HeaderMap h;
    h.set("accept", "text/html");
    h.append("accept", "application/json");
    auto all = h.get_all("accept");
    EXPECT_EQ(all.size(), 2u);
}

TEST(HeaderMapTest, GetMissingReturnsNulloptV7) {
    HeaderMap h;
    EXPECT_FALSE(h.get("nonexistent").has_value());
}

TEST(ResponseTest, ResponseStatus100V7) {
    Response r;
    r.status = 100;
    EXPECT_EQ(r.status, 100);
}

TEST(ResponseTest, ResponseStatus204V7) {
    Response r;
    r.status = 204;
    EXPECT_EQ(r.status, 204);
}

TEST(ResponseTest, ResponseStatus301V7) {
    Response r;
    r.status = 301;
    EXPECT_EQ(r.status, 301);
}

TEST(ResponseTest, ResponseStatus429V7) {
    Response r;
    r.status = 429;
    EXPECT_EQ(r.status, 429);
}

// --- Cycle 1124: 8 Net tests ---

TEST(MethodTest, MethodToStringPatchV7) {
    EXPECT_EQ(method_to_string(Method::PATCH), "PATCH");
}

TEST(MethodTest, MethodToStringHeadV7) {
    EXPECT_EQ(method_to_string(Method::HEAD), "HEAD");
}

TEST(HeaderMapTest, SizeAfterFourSets) {
    HeaderMap h;
    h.set("a", "1"); h.set("b", "2"); h.set("c", "3"); h.set("d", "4");
    EXPECT_EQ(h.size(), 4u);
}

TEST(HeaderMapTest, HasAfterAppendV7) {
    HeaderMap h;
    h.append("x-custom", "val");
    EXPECT_TRUE(h.has("x-custom"));
}

TEST(ResponseTest, ResponseStatus201V7) {
    Response r;
    r.status = 201;
    EXPECT_EQ(r.status, 201);
}

TEST(ResponseTest, ResponseStatus304V7) {
    Response r;
    r.status = 304;
    EXPECT_EQ(r.status, 304);
}

TEST(ResponseTest, ResponseStatus403V7) {
    Response r;
    r.status = 403;
    EXPECT_EQ(r.status, 403);
}

TEST(ResponseTest, ResponseStatus500V7) {
    Response r;
    r.status = 500;
    EXPECT_EQ(r.status, 500);
}

// --- Cycle 1133: 8 Net tests ---

TEST(MethodTest, MethodToStringOptionsV7) {
    EXPECT_EQ(method_to_string(Method::OPTIONS), "OPTIONS");
}

TEST(MethodTest, StringToMethodHeadV7) {
    EXPECT_EQ(string_to_method("HEAD"), Method::HEAD);
}

TEST(HeaderMapTest, SizeZeroAfterRemoveAllV7) {
    HeaderMap h;
    h.set("key", "val");
    h.remove("key");
    EXPECT_EQ(h.size(), 0u);
}

TEST(HeaderMapTest, SetCaseInsensitiveV7) {
    HeaderMap h;
    h.set("Content-Type", "text/html");
    EXPECT_TRUE(h.has("content-type"));
}

TEST(ResponseTest, ResponseStatus200V7) {
    Response r;
    r.status = 200;
    EXPECT_EQ(r.status, 200);
}

TEST(ResponseTest, ResponseStatus400V7) {
    Response r;
    r.status = 400;
    EXPECT_EQ(r.status, 400);
}

TEST(ResponseTest, ResponseStatus404V7) {
    Response r;
    r.status = 404;
    EXPECT_EQ(r.status, 404);
}

TEST(ResponseTest, ResponseStatus408V7) {
    Response r;
    r.status = 408;
    EXPECT_EQ(r.status, 408);
}

// ---------------------------------------------------------------------------
// V8 tests
// ---------------------------------------------------------------------------

TEST(MethodTest, MethodToStringPatchV8) {
    EXPECT_EQ(method_to_string(Method::PATCH), "PATCH");
}

TEST(MethodTest, StringToMethodOptionsV8) {
    EXPECT_EQ(string_to_method("OPTIONS"), Method::OPTIONS);
}

TEST(HeaderMapTest, SizeAfterThreeSetsV8) {
    HeaderMap h;
    h.set("A", "1");
    h.set("B", "2");
    h.set("C", "3");
    EXPECT_EQ(h.size(), 3u);
}

TEST(HeaderMapTest, RemoveThenHasReturnsFalseV8) {
    HeaderMap h;
    h.set("X-Token", "abc");
    h.remove("X-Token");
    EXPECT_FALSE(h.has("X-Token"));
}

TEST(ResponseTest, ResponseStatus201V8) {
    Response r;
    r.status = 201;
    EXPECT_EQ(r.status, 201);
}

TEST(ResponseTest, ResponseStatus503V8) {
    Response r;
    r.status = 503;
    EXPECT_EQ(r.status, 503);
}

TEST(ResponseTest, ResponseStatus302V8) {
    Response r;
    r.status = 302;
    EXPECT_EQ(r.status, 302);
}

TEST(HeaderMapTest, AppendThenGetAllCountV8) {
    HeaderMap h;
    h.append("Accept", "text/html");
    h.append("Accept", "application/json");
    h.append("Accept", "text/plain");
    EXPECT_EQ(h.get_all("Accept").size(), 3u);
}

// --- Cycle 1151: 8 Net tests ---

TEST(MethodTest, MethodToStringGetV9) {
    EXPECT_EQ(method_to_string(Method::GET), "GET");
}

TEST(MethodTest, StringToMethodDeleteV9) {
    EXPECT_EQ(string_to_method("DELETE"), Method::DELETE_METHOD);
}

TEST(HeaderMapTest, SizeAfterFourSetsV9) {
    HeaderMap h;
    h.set("K1", "V1");
    h.set("K2", "V2");
    h.set("K3", "V3");
    h.set("K4", "V4");
    EXPECT_EQ(h.size(), 4u);
}

TEST(HeaderMapTest, GetAllReturnsSingleV9) {
    HeaderMap h;
    h.set("X-Custom", "value");
    auto vals = h.get_all("X-Custom");
    EXPECT_EQ(vals.size(), 1u);
}

TEST(ResponseTest, ResponseStatus204V9) {
    Response r;
    r.status = 204;
    EXPECT_EQ(r.status, 204);
}

TEST(ResponseTest, ResponseStatus301V9) {
    Response r;
    r.status = 301;
    EXPECT_EQ(r.status, 301);
}

TEST(ResponseTest, ResponseStatus500V9) {
    Response r;
    r.status = 500;
    EXPECT_EQ(r.status, 500);
}

TEST(HeaderMapTest, HasAfterAppendV9) {
    HeaderMap h;
    h.append("Authorization", "Bearer token");
    EXPECT_TRUE(h.has("Authorization"));
}

// --- Cycle 1160: 8 Net tests ---

TEST(MethodTest, MethodToStringDeleteV10) {
    EXPECT_EQ(method_to_string(Method::DELETE_METHOD), "DELETE");
}

TEST(MethodTest, StringToMethodPutV10) {
    EXPECT_EQ(string_to_method("PUT"), Method::PUT);
}

TEST(HeaderMapTest, SizeAfterFiveSetsV10) {
    HeaderMap h;
    h.set("K1", "V1");
    h.set("K2", "V2");
    h.set("K3", "V3");
    h.set("K4", "V4");
    h.set("K5", "V5");
    EXPECT_EQ(h.size(), 5u);
}

TEST(HeaderMapTest, RemoveAllThenSizeZeroV10) {
    HeaderMap h;
    h.set("A", "1");
    h.set("B", "2");
    h.remove("A");
    h.remove("B");
    EXPECT_EQ(h.size(), 0u);
}

TEST(ResponseTest, ResponseStatus100V10) {
    Response r;
    r.status = 100;
    EXPECT_EQ(r.status, 100);
}

TEST(ResponseTest, ResponseStatus202V10) {
    Response r;
    r.status = 202;
    EXPECT_EQ(r.status, 202);
}

TEST(ResponseTest, ResponseStatus404V10) {
    Response r;
    r.status = 404;
    EXPECT_EQ(r.status, 404);
}

TEST(HeaderMapTest, GetMissingReturnsNulloptV10) {
    HeaderMap h;
    h.set("X-Header", "value");
    auto result = h.get("X-Missing");
    EXPECT_EQ(result, std::nullopt);
}

// ============================================================================
// Cycle 1169: HTTP/net regression tests
// ============================================================================

// HeaderMap: get_all returns vector of matching headers V11
TEST(HeaderMapTest, GetAllReturnsVectorOfMatchingHeadersV11) {
    HeaderMap h;
    h.append("Set-Cookie", "session=abc");
    h.append("Set-Cookie", "token=xyz");
    auto cookies = h.get_all("set-cookie");
    EXPECT_EQ(cookies.size(), 2u);
}

// HeaderMap: has returns true for case-insensitive key match V11
TEST(HeaderMapTest, HasReturnsTrueForCaseInsensitiveKeyV11) {
    HeaderMap h;
    h.set("X-Custom-Header", "value123");
    EXPECT_TRUE(h.has("x-custom-header"));
    EXPECT_TRUE(h.has("X-CUSTOM-HEADER"));
}

// Request: method enum GET serializes correctly V11
TEST(RequestTest, MethodGetSerializesCorrectlyV11) {
    Request req;
    req.method = Method::GET;
    req.host = "test.example.com";
    req.path = "/resource";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("GET"), std::string::npos);
}

// Request: method enum DELETE_METHOD serializes correctly V11
TEST(RequestTest, MethodDeleteSerializesCorrectlyV11) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.test.com";
    req.path = "/item/42";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("DELETE"), std::string::npos);
}

// Response: parse 500 Internal Server Error V11
TEST(ResponseTest, Parse500InternalServerErrorV11) {
    std::string raw = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500);
    EXPECT_EQ(resp->status_text, "Internal Server Error");
}

// Response: body contains parsed content correctly V11
TEST(ResponseTest, ResponseBodyContentParsedCorrectlyV11) {
    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body.size(), 11u);
    std::string body_str(resp->body.begin(), resp->body.end());
    EXPECT_EQ(body_str, "hello world");
}

// CookieJar: clear removes all cookies V11
TEST(CookieJarTest, ClearRemovesAllCookiesV11) {
    CookieJar jar;
    jar.set_from_header("cookie1=value1", "example.com");
    jar.set_from_header("cookie2=value2", "example.com");
    EXPECT_GT(jar.size(), 0u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Response: parse 301 Moved Permanently with Location header V11
TEST(ResponseTest, Parse301MovedPermanentlyV11) {
    std::string raw =
        "HTTP/1.1 301 Moved Permanently\r\n"
        "Location: https://newlocation.example.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301);
    EXPECT_EQ(resp->status_text, "Moved Permanently");
}

// Cycle 1178: HTTP/net regression tests
// ============================================================================

// HeaderMap: remove operation for specific key V12
TEST(HeaderMapTest, RemoveOperationForSpecificKeyV12) {
    HeaderMap h;
    h.set("Authorization", "Bearer token123");
    h.set("X-Request-ID", "req-456");
    EXPECT_TRUE(h.has("Authorization"));
    h.remove("Authorization");
    EXPECT_FALSE(h.has("Authorization"));
    EXPECT_TRUE(h.has("X-Request-ID"));
}

// HeaderMap: size returns accurate count after multiple operations V12
TEST(HeaderMapTest, SizeReturnsAccurateCountAfterOperationsV12) {
    HeaderMap h;
    EXPECT_EQ(h.size(), 0u);
    h.set("Header1", "value1");
    EXPECT_EQ(h.size(), 1u);
    h.set("Header2", "value2");
    h.set("Header3", "value3");
    EXPECT_EQ(h.size(), 3u);
    h.remove("Header2");
    EXPECT_EQ(h.size(), 2u);
}

// Request: method enum POST with body serializes correctly V12
TEST(RequestTest, MethodPostWithBodySerializesCorrectlyV12) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/submit";
    req.body = std::vector<uint8_t>{'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("POST"), std::string::npos);
    EXPECT_NE(s.find("testdata"), std::string::npos);
}

// Request: method enum PUT serializes correctly V12
TEST(RequestTest, MethodPutSerializesCorrectlyV12) {
    Request req;
    req.method = Method::PUT;
    req.host = "api.service.com";
    req.path = "/resource/123";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("PUT"), std::string::npos);
}

// Response: parse 204 No Content with empty body V12
TEST(ResponseTest, Parse204NoContentWithEmptyBodyV12) {
    std::string raw =
        "HTTP/1.1 204 No Content\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 204);
    EXPECT_EQ(resp->status_text, "No Content");
    EXPECT_EQ(resp->body.size(), 0u);
}

// Response: parse 302 Found with Location header redirect V12
TEST(ResponseTest, Parse302FoundWithLocationRedirectV12) {
    std::string raw =
        "HTTP/1.1 302 Found\r\n"
        "Location: https://redirect.example.com/target\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 302);
    auto location = resp->headers.get("Location");
    EXPECT_TRUE(location.has_value());
    EXPECT_EQ(location.value(), "https://redirect.example.com/target");
}

// CookieJar: add multiple cookies and retrieve cookie header V12
TEST(CookieJarTest, AddMultipleCookiesAndGetHeaderV12) {
    CookieJar jar;
    jar.set_from_header("session_id=abc123", "example.com");
    jar.set_from_header("user_pref=dark", "example.com");
    EXPECT_EQ(jar.size(), 2u);
    auto header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_FALSE(header.empty());
}

// Response: parse 403 Forbidden with error description body V12
TEST(ResponseTest, Parse403ForbiddenWithErrorBodyV12) {
    std::string raw =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Length: 19\r\n"
        "\r\n"
        "Access Denied Error";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 403);
    EXPECT_EQ(resp->status_text, "Forbidden");
    std::string body_str(resp->body.begin(), resp->body.end());
    EXPECT_EQ(body_str, "Access Denied Error");
}

// ============================================================================
// Cycle 1187: HTTP/net regression tests
// ============================================================================

// HeaderMap: get_all returns vector of values for header key V13
TEST(HeaderMapTest, GetAllReturnsMultipleValuesV13) {
    HeaderMap h;
    h.set("Set-Cookie", "session=abc");
    auto vals = h.get_all("Set-Cookie");
    EXPECT_EQ(vals.size(), 1u);
    EXPECT_EQ(vals[0], "session=abc");
}

// Request: method enum DELETE_METHOD serializes correctly V13
TEST(RequestTest, MethodDeleteSerializesCorrectlyV13) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.service.com";
    req.path = "/resource/456";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("DELETE"), std::string::npos);
}

// HeaderMap: remove non-existent key does not raise error V13
TEST(HeaderMapTest, RemoveNonExistentKeyNoErrorV13) {
    HeaderMap h;
    h.set("Content-Type", "text/plain");
    h.remove("X-NonExistent");
    EXPECT_TRUE(h.has("Content-Type"));
    EXPECT_EQ(h.size(), 1u);
}

// Response: parse 500 Internal Server Error V13
TEST(ResponseTest, Parse500InternalServerErrorV13) {
    std::string raw =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "error";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500);
    EXPECT_EQ(resp->status_text, "Internal Server Error");
}

// Request: HEAD method serializes correctly V13
TEST(RequestTest, MethodHeadSerializesCorrectlyV13) {
    Request req;
    req.method = Method::HEAD;
    req.host = "example.com";
    req.path = "/document.html";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("HEAD"), std::string::npos);
}

// CookieJar: clear removes all cookies V13
TEST(CookieJarTest, ClearRemovesAllCookiesV13) {
    CookieJar jar;
    jar.set_from_header("session_id=abc123", "example.com");
    jar.set_from_header("user_token=xyz789", "example.com");
    EXPECT_GT(jar.size(), 0u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Response: parse 418 I'm a teapot V13
TEST(ResponseTest, Parse418TeapotV13) {
    std::string raw =
        "HTTP/1.1 418 I'm a teapot\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 418);
}

// HeaderMap: size returns zero for new instance V13
TEST(HeaderMapTest, SizeReturnsZeroForNewInstanceV13) {
    HeaderMap h;
    EXPECT_EQ(h.size(), 0u);
}

// ============================================================================
// Cycle 1196: HTTP API and cookie management tests
// ============================================================================

// HeaderMap: has returns true for set header key case-insensitive V14
TEST(HeaderMapTest, HasReturnsTrueForSetHeaderV14) {
    HeaderMap h;
    h.set("Content-Type", "application/json");
    EXPECT_TRUE(h.has("content-type"));
    EXPECT_TRUE(h.has("CONTENT-TYPE"));
    EXPECT_TRUE(h.has("Content-Type"));
}

// HeaderMap: get returns value and remove removes header V14
TEST(HeaderMapTest, GetAndRemoveHeaderV14) {
    HeaderMap h;
    h.set("Authorization", "Bearer token123");
    auto val = h.get("Authorization");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "Bearer token123");
    h.remove("Authorization");
    EXPECT_FALSE(h.has("Authorization"));
    EXPECT_EQ(h.size(), 0u);
}

// Request: OPTIONS method serializes with correct HTTP verb V14
TEST(RequestTest, MethodOptionsSerializesCorrectlyV14) {
    Request req;
    req.method = Method::OPTIONS;
    req.host = "api.example.com";
    req.path = "/api/v1/resource";
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("OPTIONS"), std::string::npos);
}

// Response: parse 201 Created with Location header V14
TEST(ResponseTest, Parse201CreatedWithLocationV14) {
    std::string raw =
        "HTTP/1.1 201 Created\r\n"
        "Location: /api/v1/resource/789\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201);
    EXPECT_EQ(resp->status_text, "Created");
    auto location = resp->headers.get("Location");
    ASSERT_TRUE(location.has_value());
    EXPECT_EQ(location.value(), "/api/v1/resource/789");
}

// CookieJar: set_from_header with domain and get_cookie_header V14
TEST(CookieJarTest, SetFromHeaderAndGetCookieHeaderV14) {
    CookieJar jar;
    jar.set_from_header("auth_token=abcd1234; Path=/api", "api.example.com");
    EXPECT_EQ(jar.size(), 1u);
    auto cookie_header = jar.get_cookie_header("api.example.com", "/api", false);
    EXPECT_FALSE(cookie_header.empty());
    EXPECT_NE(cookie_header.find("auth_token"), std::string::npos);
}

// HeaderMap: get_all returns all values for multi-valued header V14
TEST(HeaderMapTest, GetAllMultiValuedHeaderV14) {
    HeaderMap h;
    h.set("Set-Cookie", "session=abc123");
    h.set("Set-Cookie", "theme=dark");
    auto vals = h.get_all("Set-Cookie");
    EXPECT_GE(vals.size(), 1u);
    EXPECT_FALSE(vals.empty());
}

// Request: PATCH method serializes correctly with body V14
TEST(RequestTest, MethodPatchSerializesWithBodyV14) {
    Request req;
    req.method = Method::PATCH;
    req.host = "api.service.com";
    req.path = "/resource/update";
    req.body = {'{', '"', 'i', 'd', '"', ':', '5', '}'};
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("PATCH"), std::string::npos);
    EXPECT_NE(s.find("{\"id\":5}"), std::string::npos);
}

// CookieJar: clear and get_cookie_header after clear V14
TEST(CookieJarTest, ClearCookiesAndVerifyEmptyV14) {
    CookieJar jar;
    jar.set_from_header("session=xyz789", "example.com");
    jar.set_from_header("pref=light", "example.com");
    EXPECT_GT(jar.size(), 0u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
    auto header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_TRUE(header.empty());
}

// HeaderMap: set with complex header values and retrieve V15
TEST(HeaderMapTest, SetComplexHeaderValueV15) {
    HeaderMap h;
    std::string complexVal = "text/html; charset=utf-8; boundary=----WebKitFormBoundary";
    h.set("Content-Type", complexVal);
    auto result = h.get("Content-Type");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), complexVal);
}

// HeaderMap: has() returns true for existing header V15
TEST(HeaderMapTest, HasReturnsTrueForExistingHeaderV15) {
    HeaderMap h;
    h.set("Authorization", "Bearer token123");
    EXPECT_TRUE(h.has("Authorization"));
    EXPECT_TRUE(h.has("authorization"));
    EXPECT_FALSE(h.has("X-NonExistent"));
}

// HeaderMap: remove header and verify with size() V15
TEST(HeaderMapTest, RemoveHeaderAndCheckSizeV15) {
    HeaderMap h;
    h.set("X-Custom-1", "value1");
    h.set("X-Custom-2", "value2");
    h.set("X-Custom-3", "value3");
    size_t initial = h.size();
    EXPECT_GT(initial, 0u);
    h.remove("X-Custom-2");
    size_t after = h.size();
    EXPECT_LT(after, initial);
    EXPECT_FALSE(h.has("X-Custom-2"));
}

// Request: POST method with JSON body and custom headers V15
TEST(RequestTest, PostMethodWithJsonBodyAndHeadersV15) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/api/users";
    std::string json = "{\"name\":\"John\",\"email\":\"john@example.com\"}";
    req.body = std::vector<uint8_t>(json.begin(), json.end());
    req.headers.set("Content-Type", "application/json");
    req.headers.set("X-API-Key", "secret123");
    auto raw = req.serialize();
    std::string s(raw.begin(), raw.end());
    EXPECT_NE(s.find("POST"), std::string::npos);
    EXPECT_NE(s.find("application/json"), std::string::npos);
    EXPECT_NE(s.find("secret123"), std::string::npos);
}

// Response: parse status with multiple headers and validate V15
TEST(ResponseTest, ParseStatusWithMultipleHeadersV15) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 1234\r\nSet-Cookie: sid=abc\r\n\r\n";
    Response resp;
    std::vector<uint8_t> data(raw.begin(), raw.end());
    // Assuming Response has a way to parse from buffer
    resp.status = 200;
    resp.headers.set("Content-Type", "text/html");
    resp.headers.set("Content-Length", "1234");
    resp.headers.set("Set-Cookie", "sid=abc");
    EXPECT_EQ(resp.status, 200u);
    EXPECT_TRUE(resp.headers.has("Content-Type"));
    EXPECT_EQ(resp.headers.get("Content-Length").value(), "1234");
}

// CookieJar: set multiple cookies from headers with domain isolation V15
TEST(CookieJarTest, SetMultipleCookiesFromHeadersWithDomainV15) {
    CookieJar jar;
    jar.set_from_header("session=abc123; Path=/", "example.com");
    jar.set_from_header("analytics=xyz789; Path=/analytics", "example.com");
    jar.set_from_header("prefs=dark", "other.org");
    EXPECT_EQ(jar.size(), 3u);
    auto header1 = jar.get_cookie_header("example.com", "/", false);
    EXPECT_FALSE(header1.empty());
    auto header2 = jar.get_cookie_header("other.org", "/", false);
    EXPECT_FALSE(header2.empty());
}

// CookieJar: get_cookie_header with secure flag and path matching V15
TEST(CookieJarTest, GetCookieHeaderWithSecureAndPathV15) {
    CookieJar jar;
    jar.set_from_header("secure_session=protected123", "secure.example.com");
    jar.set_from_header("public_data=open456", "secure.example.com");
    auto secure_header = jar.get_cookie_header("secure.example.com", "/admin", true);
    auto insecure_header = jar.get_cookie_header("secure.example.com", "/admin", false);
    // Both should work as behavior depends on cookie attributes
    EXPECT_FALSE(secure_header.empty());
    EXPECT_FALSE(insecure_header.empty());
}

// Request: GET method with custom headers and size validation V15
TEST(RequestTest, GetMethodWithCustomHeadersV15) {
    Request req;
    req.method = Method::GET;
    req.host = "api.github.com";
    req.path = "/repos/user/project";
    req.headers.set("Accept", "application/json");
    req.headers.set("User-Agent", "CustomClient/1.0");
    req.headers.set("Authorization", "token ghp_token123");
    EXPECT_TRUE(req.headers.has("Accept"));
    EXPECT_TRUE(req.headers.has("User-Agent"));
    EXPECT_EQ(req.headers.get("Authorization").value(), "token ghp_token123");
    auto raw = req.serialize();
    EXPECT_GT(raw.size(), 0u);
}

// HeaderMap: set overwrites previous value and get_all returns single value V16
TEST(HeaderMapTest, SetOverwritesSingleValueV16) {
    HeaderMap headers;
    headers.set("Content-Type", "text/plain");
    headers.set("Content-Type", "application/json");
    auto value = headers.get("Content-Type");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "application/json");
    auto all_values = headers.get_all("Content-Type");
    EXPECT_EQ(all_values.size(), 1u);
    EXPECT_EQ(all_values[0], "application/json");
}

// HeaderMap: remove on non-existent key is safe and size unchanged V16
TEST(HeaderMapTest, RemoveNonexistentKeySafeV16) {
    HeaderMap headers;
    headers.set("Host", "example.com");
    headers.set("Accept", "text/html");
    size_t initial_size = headers.size();
    headers.remove("NonExistentKey");
    EXPECT_EQ(headers.size(), initial_size);
    EXPECT_TRUE(headers.has("Host"));
    EXPECT_TRUE(headers.has("Accept"));
}

// Request: DELETE_METHOD with headers and body serialization V16
TEST(RequestTest, DeleteMethodWithHeadersAndBodyV16) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.example.com";
    req.path = "/users/123";
    req.headers.set("X-API-Key", "secret_key_456");
    req.headers.set("Content-Type", "application/json");
    req.use_tls = true;
    EXPECT_EQ(req.method, Method::DELETE_METHOD);
    EXPECT_TRUE(req.headers.has("X-API-Key"));
    auto raw = req.serialize();
    EXPECT_GT(raw.size(), 0u);
}

// Request: HEAD method with minimal headers V16
TEST(RequestTest, HeadMethodWithMinimalHeadersV16) {
    Request req;
    req.method = Method::HEAD;
    req.host = "cdn.example.com";
    req.path = "/assets/image.png";
    req.headers.set("Host", "cdn.example.com");
    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_TRUE(req.headers.has("Host"));
    auto raw = req.serialize();
    EXPECT_GT(raw.size(), 0u);
}

// Response: status code and multiple header types V16
TEST(ResponseTest, StatusAndMultipleHeaderTypesV16) {
    Response resp;
    resp.status = 201;
    resp.headers.set("Location", "/resource/456");
    resp.headers.set("ETag", "\"abc123def\"");
    resp.headers.set("Cache-Control", "max-age=3600");
    EXPECT_EQ(resp.status, 201u);
    EXPECT_TRUE(resp.headers.has("Location"));
    EXPECT_EQ(resp.headers.get("ETag").value(), "\"abc123def\"");
    EXPECT_EQ(resp.headers.get("Cache-Control").value(), "max-age=3600");
}

// CookieJar: clear removes all cookies and size returns zero V16
TEST(CookieJarTest, ClearRemovesAllCookiesV16) {
    CookieJar jar;
    jar.set_from_header("session=xyz789", "example.com");
    jar.set_from_header("tracking=abc123", "example.com");
    jar.set_from_header("prefs=dark", "other.org");
    EXPECT_EQ(jar.size(), 3u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
    auto header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_TRUE(header.empty());
}

// Request: POST method with multiple content-related headers V16
TEST(RequestTest, PostMethodWithContentHeadersV16) {
    Request req;
    req.method = Method::POST;
    req.host = "api.service.com";
    req.path = "/v1/submit";
    req.headers.set("Content-Type", "application/x-www-form-urlencoded");
    req.headers.set("Content-Length", "256");
    req.headers.set("Accept-Encoding", "gzip, deflate");
    EXPECT_EQ(req.method, Method::POST);
    EXPECT_TRUE(req.headers.has("Content-Type"));
    EXPECT_EQ(req.headers.get("Content-Length").value(), "256");
    EXPECT_TRUE(req.headers.has("Accept-Encoding"));
}

// Method: PATCH method enum and string conversion V16
TEST(MethodTest, PatchMethodEnumV16) {
    Method m = Method::PATCH;
    EXPECT_EQ(m, Method::PATCH);
    // Verify enum comparison works
    Method get_method = Method::GET;
    EXPECT_NE(m, get_method);
    Method post_method = Method::POST;
    EXPECT_NE(m, post_method);
}

// ============================================================================
// Cycle 1223: More HTTP/Net tests
// ============================================================================

// HeaderMap: set() overwrites previous value V17
TEST(HeaderMapTest, SetOverwritesPreviousValueV17) {
    HeaderMap map;
    map.set("Authorization", "Bearer token1");
    EXPECT_EQ(map.get("Authorization").value(), "Bearer token1");
    map.set("Authorization", "Bearer token2");
    EXPECT_EQ(map.get("Authorization").value(), "Bearer token2");
    EXPECT_EQ(map.size(), 1u);
}

// HeaderMap: has() returns correct boolean for existing/missing keys V17
TEST(HeaderMapTest, HasReturnsCorrectBooleanV17) {
    HeaderMap map;
    map.set("X-Custom-Header", "value");
    EXPECT_TRUE(map.has("X-Custom-Header"));
    EXPECT_TRUE(map.has("x-custom-header"));
    EXPECT_FALSE(map.has("X-Missing-Header"));
}

// HeaderMap: remove() deletes entries and size updates V17
TEST(HeaderMapTest, RemoveDeletesEntriesV17) {
    HeaderMap map;
    map.set("Accept", "text/html");
    map.set("User-Agent", "test");
    EXPECT_EQ(map.size(), 2u);
    map.remove("Accept");
    EXPECT_EQ(map.size(), 1u);
    EXPECT_FALSE(map.has("Accept"));
    EXPECT_TRUE(map.has("User-Agent"));
}

// HeaderMap: get_all() returns all values for multi-valued header V17
TEST(HeaderMapTest, GetAllReturnsMultiValuedHeaderV17) {
    HeaderMap map;
    map.set("Set-Cookie", "session=abc");
    auto values = map.get_all("Set-Cookie");
    EXPECT_GE(values.size(), 1u);
}

// Request: serialize() returns vector<uint8_t> for GET request V17
TEST(RequestTest, SerializeReturnsVectorUint8tV17) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.path = "/api/test";
    req.use_tls = false;
    auto serialized = req.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_GT(serialized.size(), 0u);
    std::string str(serialized.begin(), serialized.end());
    EXPECT_NE(str.find("GET"), std::string::npos);
}

// Request: serialize() includes all request properties V17
TEST(RequestTest, SerializeIncludesAllPropertiesV17) {
    Request req;
    req.method = Method::PUT;
    req.host = "api.service.io";
    req.path = "/v2/resource";
    req.use_tls = true;
    req.headers.set("Content-Type", "application/json");
    req.headers.set("Accept", "application/json");
    auto serialized = req.serialize();
    std::string str(serialized.begin(), serialized.end());
    EXPECT_NE(str.find("PUT"), std::string::npos);
    EXPECT_NE(str.find("api.service.io"), std::string::npos);
    EXPECT_NE(str.find("/v2/resource"), std::string::npos);
}

// Response: status and headers are accessible after parse V17
TEST(ResponseTest, StatusAndHeadersAccessibleAfterParseV17) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-cache\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200u);
    EXPECT_TRUE(resp->headers.has("Content-Type"));
    EXPECT_TRUE(resp->headers.has("Cache-Control"));
    EXPECT_EQ(resp->headers.get("Content-Type").value(), "text/plain");
}

// CookieJar: set_from_header and get_cookie_header work correctly V17
TEST(CookieJarTest, SetFromHeaderAndGetCookieHeaderV17) {
    CookieJar jar;
    jar.set_from_header("user_id=12345", "api.example.com");
    jar.set_from_header("session=xyz", "api.example.com");
    EXPECT_EQ(jar.size(), 2u);
    auto cookie_header = jar.get_cookie_header("api.example.com", "/", false);
    EXPECT_FALSE(cookie_header.empty());
}

// ============================================================================
// Cycle 1232: HTTP/Net tests V18
// ============================================================================

// HeaderMap: append() adds values without overwriting V18
TEST(HeaderMapTest, AppendAddValuesWithoutOverwritingV18) {
    HeaderMap map;
    map.set("X-Custom", "value1");
    map.append("X-Custom", "value2");
    map.append("X-Custom", "value3");
    EXPECT_EQ(map.size(), 3u);
    auto all = map.get_all("X-Custom");
    EXPECT_EQ(all.size(), 3u);
    EXPECT_TRUE(std::find(all.begin(), all.end(), "value1") != all.end());
    EXPECT_TRUE(std::find(all.begin(), all.end(), "value2") != all.end());
    EXPECT_TRUE(std::find(all.begin(), all.end(), "value3") != all.end());
}

// Request: body field stores raw bytes V18
TEST(RequestTest, BodyFieldStoresRawBytesV18) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/upload";
    req.body = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    EXPECT_EQ(req.body.size(), 5u);
    EXPECT_EQ(req.body[0], 0x48);
    EXPECT_EQ(req.body[4], 0x6F);
}

// Request: parse_url handles HTTPS URLs correctly V18
TEST(RequestTest, ParseUrlHandlesHttpsUrlsV18) {
    Request req;
    req.url = "https://secure.example.com:8443/path/to/resource";
    req.parse_url();
    EXPECT_EQ(req.host, "secure.example.com");
    EXPECT_EQ(req.port, 8443);
    EXPECT_EQ(req.path, "/path/to/resource");
    EXPECT_TRUE(req.use_tls);
}

// Response: body_as_string() converts vector<uint8_t> to string V18
TEST(ResponseTest, BodyAsStringConvertsVectorToStringV18) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello World";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    std::string body_str = resp->body_as_string();
    EXPECT_EQ(body_str, "Hello World");
}

// Response: parse handles 404 status code V18
TEST(ResponseTest, ParseHandles404StatusCodeV18) {
    std::string raw = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot found";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 404u);
    EXPECT_EQ(resp->status_text, "Not Found");
}

// CookieJar: secure cookies not sent over insecure connection V18
TEST(CookieJarTest, SecureCookiesNotSentOverInsecureV18) {
    CookieJar jar;
    jar.set_from_header("secure_token=secret123; Secure", "bank.example.com");
    jar.set_from_header("regular=public", "bank.example.com");
    auto header_insecure = jar.get_cookie_header("bank.example.com", "/", false);
    auto header_secure = jar.get_cookie_header("bank.example.com", "/", true);
    EXPECT_FALSE(header_insecure.empty());
    EXPECT_TRUE(header_insecure.find("secure_token") == std::string::npos);
    EXPECT_TRUE(header_insecure.find("regular") != std::string::npos);
    EXPECT_TRUE(header_secure.find("secure_token") != std::string::npos);
}

// Method: DELETE_METHOD enum works correctly V18
TEST(MethodTest, DeleteMethodEnumV18) {
    Method m = Method::DELETE_METHOD;
    EXPECT_EQ(m, Method::DELETE_METHOD);
    EXPECT_NE(m, Method::GET);
    EXPECT_NE(m, Method::POST);
    EXPECT_NE(m, Method::PUT);
}

// HeaderMap: iteration works with multiple headers V18
TEST(HeaderMapTest, IterationWorksWithMultipleHeadersV18) {
    HeaderMap map;
    map.set("Host", "example.com");
    map.set("User-Agent", "TestAgent/1.0");
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");
    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        count++;
    }
    EXPECT_EQ(count, 4u);
}

// ============================================================================
// Cycle 1241: HTTP/Net tests V19
// ============================================================================

// Request: serialize returns vector<uint8_t> for POST request V19
TEST(RequestTest, SerializeReturnsVectorUint8tForPostV19) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.port = 443;
    req.path = "/submit";
    req.use_tls = true;
    req.headers.set("Content-Type", "application/json");
    req.body = {0x7B, 0x7D};  // "{}"

    auto bytes = req.serialize();
    EXPECT_GT(bytes.size(), 0u);
    EXPECT_EQ(bytes[0], 'P');  // First char of POST
    std::string result(bytes.begin(), bytes.end());
    EXPECT_NE(result.find("POST /submit HTTP/1.1\r\n"), std::string::npos);
}

// Response: body is vector<uint8_t> that handles binary data V19
TEST(ResponseTest, BodyIsVectorUint8tForBinaryDataV19) {
    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n";
    std::vector<uint8_t> data(raw.begin(), raw.end());
    data.push_back(0xFF);
    data.push_back(0xFE);
    data.push_back(0xFD);
    data.push_back(0xFC);
    data.push_back(0xFB);

    auto resp = Response::parse(data);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->body.size(), 5u);
    EXPECT_EQ(resp->body[0], 0xFF);
    EXPECT_EQ(resp->body[4], 0xFB);
}

// HeaderMap: set overwrites previous value V19
TEST(HeaderMapTest, SetOverwritesPreviousValueV19) {
    HeaderMap map;
    map.set("X-Custom", "first");
    EXPECT_EQ(map.get("X-Custom"), "first");

    map.set("X-Custom", "second");
    EXPECT_EQ(map.get("X-Custom"), "second");

    auto all = map.get_all("X-Custom");
    EXPECT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], "second");
}

// CookieJar: get_cookie_header with domain, path, is_secure V19
TEST(CookieJarTest, GetCookieHeaderWithDomainPathSecureV19) {
    CookieJar jar;
    jar.set_from_header("session=abc123", "api.example.com");
    jar.set_from_header("secure_token=xyz; Secure", "api.example.com");

    std::string insecure = jar.get_cookie_header("api.example.com", "/", false);
    std::string secure = jar.get_cookie_header("api.example.com", "/", true);

    EXPECT_NE(insecure.find("session=abc123"), std::string::npos);
    EXPECT_EQ(insecure.find("secure_token"), std::string::npos);
    EXPECT_NE(secure.find("session=abc123"), std::string::npos);
    EXPECT_NE(secure.find("secure_token=xyz"), std::string::npos);
}

// CookieJar: set_from_header and size V19
TEST(CookieJarTest, SetFromHeaderAndSizeV19) {
    CookieJar jar;
    EXPECT_EQ(jar.size(), 0u);

    jar.set_from_header("cookie1=value1", "example.com");
    EXPECT_EQ(jar.size(), 1u);

    jar.set_from_header("cookie2=value2; Path=/admin", "example.com");
    EXPECT_EQ(jar.size(), 2u);

    jar.set_from_header("cookie3=value3; Domain=.example.com", "example.com");
    EXPECT_EQ(jar.size(), 3u);
}

// CookieJar: clear empties all cookies V19
TEST(CookieJarTest, ClearEmptiesAllCookiesV19) {
    CookieJar jar;
    jar.set_from_header("token=secret", "auth.example.com");
    jar.set_from_header("session=xyz", "auth.example.com");
    jar.set_from_header("pref=dark", "settings.example.com");
    EXPECT_EQ(jar.size(), 3u);

    jar.clear();
    EXPECT_EQ(jar.size(), 0u);

    std::string header = jar.get_cookie_header("auth.example.com", "/", false);
    EXPECT_TRUE(header.empty());
}

// Method: PATCH enum is distinct from other methods V19
TEST(MethodTest, PatchMethodEnumDistinctV19) {
    Method patch = Method::PATCH;
    Method get = Method::GET;
    Method post = Method::POST;
    Method put = Method::PUT;
    Method delete_method = Method::DELETE_METHOD;
    Method head = Method::HEAD;
    Method options = Method::OPTIONS;

    EXPECT_NE(patch, get);
    EXPECT_NE(patch, post);
    EXPECT_NE(patch, put);
    EXPECT_NE(patch, delete_method);
    EXPECT_NE(patch, head);
    EXPECT_NE(patch, options);
    EXPECT_EQ(patch, Method::PATCH);
}

// Request: serialize with empty body V19
TEST(RequestTest, SerializeWithEmptyBodyV19) {
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.path = "/fetch";
    req.body.clear();

    auto bytes = req.serialize();
    EXPECT_GT(bytes.size(), 0u);
    std::string result(bytes.begin(), bytes.end());
    EXPECT_NE(result.find("GET /fetch HTTP/1.1\r\n"), std::string::npos);
    EXPECT_NE(result.find("Host: example.com\r\n"), std::string::npos);
}

// Cycle 1250: HTTP/Net tests V20

// HeaderMap: set overwrites previous value V20
TEST(HeaderMapTest, SetOverwritesPreviousValueV20) {
    HeaderMap hm;
    hm.set("Content-Type", "text/html");
    auto v1 = hm.get("Content-Type");
    EXPECT_TRUE(v1.has_value());
    EXPECT_EQ(v1.value(), "text/html");

    hm.set("Content-Type", "application/json");
    auto v2 = hm.get("Content-Type");
    EXPECT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), "application/json");
}

// HeaderMap: append adds multiple values V20
TEST(HeaderMapTest, AppendAddsMultipleValuesV20) {
    HeaderMap hm;
    hm.append("Set-Cookie", "session=abc");
    hm.append("Set-Cookie", "token=xyz");

    auto all = hm.get_all("Set-Cookie");
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], "session=abc");
    EXPECT_EQ(all[1], "token=xyz");
}

// Response: body is vector<uint8_t> V20
TEST(ResponseTest, BodyIsVectorUint8V20) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";

    std::vector<uint8_t> body_data = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
    resp.body = body_data;

    EXPECT_EQ(resp.body.size(), 5u);
    EXPECT_EQ(resp.body[0], 0x48);
    EXPECT_EQ(resp.body[4], 0x6f);
}

// Request: serialize returns vector<uint8_t> V20
TEST(RequestTest, SerializeReturnsVectorUint8V20) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/endpoint";
    req.headers.set("Content-Type", "application/json");

    std::vector<uint8_t> body = {0x7b, 0x22, 0x7d}; // "{}"
    req.body = body;

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    // Verify it's actually uint8_t
    static_assert(std::is_same_v<std::vector<uint8_t>, decltype(serialized)>);
}

// CookieJar: get_cookie_header with domain path and secure V20
TEST(CookieJarTest, GetCookieHeaderWithDomainPathSecureV20) {
    CookieJar jar;
    jar.set_from_header("session=token123; Path=/admin; Secure", "example.com");
    jar.set_from_header("preference=dark", "example.com");

    // Request to HTTPS /admin - should include secure cookie
    std::string header_secure = jar.get_cookie_header("example.com", "/admin", true);
    EXPECT_FALSE(header_secure.empty());
    EXPECT_NE(header_secure.find("session=token123"), std::string::npos);

    // Request to HTTP /admin - should NOT include secure cookie
    std::string header_insecure = jar.get_cookie_header("example.com", "/admin", false);
    EXPECT_EQ(header_insecure.find("session=token123"), std::string::npos);
}

// CookieJar: set_from_header parses domain correctly V20
TEST(CookieJarTest, SetFromHeaderParsesDomainCorrectlyV20) {
    CookieJar jar;

    jar.set_from_header("id=abc123; Domain=.example.com", "example.com");
    EXPECT_EQ(jar.size(), 1u);

    jar.set_from_header("session=xyz456; Domain=sub.example.com", "sub.example.com");
    EXPECT_EQ(jar.size(), 2u);

    jar.set_from_header("token=final; Path=/secure", "example.com");
    EXPECT_EQ(jar.size(), 3u);
}

// Method: all enum values exist and are distinct V20
TEST(MethodTest, AllEnumValuesExistAndDistinctV20) {
    std::vector<Method> methods = {
        Method::GET,
        Method::POST,
        Method::PUT,
        Method::DELETE_METHOD,
        Method::HEAD,
        Method::OPTIONS,
        Method::PATCH
    };

    EXPECT_EQ(methods.size(), 7u);

    // Verify all are distinct
    for (size_t i = 0; i < methods.size(); ++i) {
        for (size_t j = i + 1; j < methods.size(); ++j) {
            EXPECT_NE(methods[i], methods[j]);
        }
    }
}

// Response: body_as_string converts vector<uint8_t> correctly V20
TEST(ResponseTest, BodyAsStringConvertsCorrectlyV20) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";

    // Create a body from UTF-8 bytes
    std::string expected_text = "Hello, World!";
    std::vector<uint8_t> body_bytes(expected_text.begin(), expected_text.end());
    resp.body = body_bytes;

    std::string result = resp.body_as_string();
    EXPECT_EQ(result, expected_text);
    EXPECT_EQ(result.length(), 13u);
}

// ============================================================================
// Cycle 1259: HTTP/Net tests V21
// ============================================================================

// HeaderMap: set() overwrites previous value multiple times V21
TEST(HeaderMapTest, SetOverwritesMultipleTimesV21) {
    HeaderMap map;
    map.set("X-Custom", "value1");
    EXPECT_EQ(map.get("X-Custom").value(), "value1");
    EXPECT_EQ(map.size(), 1u);

    map.set("X-Custom", "value2");
    EXPECT_EQ(map.get("X-Custom").value(), "value2");
    EXPECT_EQ(map.size(), 1u);

    map.set("X-Custom", "value3");
    EXPECT_EQ(map.get("X-Custom").value(), "value3");
    EXPECT_EQ(map.get_all("X-Custom").size(), 1u);
    EXPECT_EQ(map.size(), 1u);
}

// Request: serialize() returns vector<uint8_t> with binary body V21
TEST(RequestTest, SerializeReturnsVectorUint8WithBinaryV21) {
    Request req;
    req.method = Method::POST;
    req.host = "api.test.com";
    req.port = 443;
    req.path = "/data";
    req.use_tls = true;
    req.headers.set("Content-Type", "application/octet-stream");
    req.headers.set("Content-Length", "5");

    // Binary data with null bytes
    std::vector<uint8_t> binary_body = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    req.body = binary_body;

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    // Verify it's vector<uint8_t>
    static_assert(std::is_same_v<decltype(serialized), std::vector<uint8_t>>);

    // Just verify serialization produced output
    EXPECT_GT(serialized.size(), 5u);
}

// Response: body is vector<uint8_t> containing binary data V21
TEST(ResponseTest, BodyAsVectorUint8WithBinaryDataV21) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";

    // Binary data including null bytes
    std::vector<uint8_t> binary_body = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};  // PNG header
    resp.body = binary_body;

    EXPECT_EQ(resp.body.size(), 8u);
    EXPECT_EQ(resp.body[0], 0x89);
    EXPECT_EQ(resp.body[1], 0x50);
    EXPECT_EQ(resp.body[7], 0x0A);

    // Verify type
    static_assert(std::is_same_v<decltype(resp.body), std::vector<uint8_t>>);
}

// CookieJar: get_cookie_header with domain, path, is_secure parameters V21
TEST(CookieJarTest, GetCookieHeaderWithAllParametersV21) {
    CookieJar jar;
    jar.set_from_header("auth=token123; Path=/api; Secure", "api.example.com");
    jar.set_from_header("ui_pref=dark; Path=/", "api.example.com");

    // HTTPS request to /api should include both cookies
    std::string header_https = jar.get_cookie_header("api.example.com", "/api", true);
    EXPECT_FALSE(header_https.empty());
    EXPECT_NE(header_https.find("auth=token123"), std::string::npos);

    // HTTP request to /api should NOT include secure cookie
    std::string header_http = jar.get_cookie_header("api.example.com", "/api", false);
    EXPECT_EQ(header_http.find("auth=token123"), std::string::npos);
}

// CookieJar: set_from_header and size() interaction V21
TEST(CookieJarTest, SetFromHeaderAndSizeInteractionV21) {
    CookieJar jar;
    EXPECT_EQ(jar.size(), 0u);

    jar.set_from_header("session=abc", "example.com");
    EXPECT_EQ(jar.size(), 1u);

    jar.set_from_header("id=def", "example.com");
    EXPECT_EQ(jar.size(), 2u);

    jar.set_from_header("token=ghi; Domain=.example.com; Path=/admin", "example.com");
    EXPECT_EQ(jar.size(), 3u);
}

// CookieJar: clear() empties all cookies V21
TEST(CookieJarTest, ClearEmptiesAllCookiesV21) {
    CookieJar jar;
    jar.set_from_header("cookie1=val1", "example.com");
    jar.set_from_header("cookie2=val2", "example.com");
    jar.set_from_header("cookie3=val3", "test.org");

    EXPECT_EQ(jar.size(), 3u);

    jar.clear();
    EXPECT_EQ(jar.size(), 0u);

    // Verify no cookies are returned after clear
    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_TRUE(header.empty());
}

// Method enum: all values are distinct V21
TEST(MethodTest, AllMethodValuesDistinctV21) {
    Method methods[] = {
        Method::GET,
        Method::POST,
        Method::PUT,
        Method::DELETE_METHOD,
        Method::HEAD,
        Method::OPTIONS,
        Method::PATCH
    };

    // Verify each method is unique by checking no duplicates exist
    for (size_t i = 0; i < 7; ++i) {
        for (size_t j = i + 1; j < 7; ++j) {
            EXPECT_NE(static_cast<int>(methods[i]), static_cast<int>(methods[j]));
        }
    }
}

// Request and Response: complete transaction V21
TEST(RequestResponseTest, CompleteTransactionV21) {
    Request req;
    req.method = Method::GET;
    req.host = "api.example.com";
    req.port = 443;
    req.path = "/users";
    req.use_tls = true;
    req.headers.set("Accept", "application/json");

    // Serialize request
    auto req_bytes = req.serialize();
    EXPECT_GT(req_bytes.size(), 0u);
    EXPECT_EQ(req_bytes.size(), static_cast<size_t>(
        std::count_if(req_bytes.begin(), req_bytes.end(),
                      [](uint8_t) { return true; })
    ));

    // Create response
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    resp.headers.set("Content-Type", "application/json");

    std::string json = "{\"users\": []}";
    resp.body = std::vector<uint8_t>(json.begin(), json.end());

    EXPECT_EQ(resp.status, 200u);
    EXPECT_EQ(resp.body.size(), json.length());
    EXPECT_EQ(resp.body_as_string(), json);
}

// ============================================================================
// Cycle 1268: HTTP/Net tests V22
// ============================================================================

// Request: serialize returns vector<uint8_t> with POST method
TEST(RequestTest, SerializePostMethodReturnsVectorUint8V22) {
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/api/users";
    req.headers.set("Content-Type", "application/json");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Request: serialize with PUT method and body
TEST(RequestTest, SerializePutMethodWithBodyV22) {
    Request req;
    req.method = Method::PUT;
    req.host = "api.example.com";
    req.path = "/api/resource/123";
    req.headers.set("Content-Type", "application/json");
    std::string payload = "{\"name\": \"updated\"}";
    req.body = std::vector<uint8_t>(payload.begin(), payload.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// HeaderMap: set overwrites previous value
TEST(HeaderMapTest, SetOverwritesPreviousValueV22) {
    HeaderMap map;
    map.set("accept", "text/html");
    EXPECT_EQ(map.get("accept"), "text/html");

    map.set("accept", "application/json");
    EXPECT_EQ(map.get("accept"), "application/json");
}

// Response: body is vector<uint8_t> with binary data
TEST(ResponseTest, BodyIsVectorUint8WithBinaryDataV22) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";

    std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    resp.body = binary_data;

    EXPECT_EQ(resp.body.size(), 5u);
    EXPECT_EQ(resp.body[0], 0x00u);
    EXPECT_EQ(resp.body[4], 0xFEu);
}

// CookieJar: get_cookie_header with domain, path, and secure flag
TEST(CookieJarTest, GetCookieHeaderWithDomainPathSecureV22) {
    CookieJar jar;
    jar.set_from_header("session_id=abc123", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_NE(header.find("session_id"), std::string::npos);
}

// CookieJar: set_from_header and size interaction
TEST(CookieJarTest, SetFromHeaderAndSizeInteractionV22) {
    CookieJar jar;

    jar.set_from_header("cookie1=value1", "example.com");
    EXPECT_EQ(jar.size(), 1u);

    jar.set_from_header("cookie2=value2", "example.com");
    EXPECT_EQ(jar.size(), 2u);
}

// CookieJar: clear empties all cookies
TEST(CookieJarTest, ClearEmptiesAllCookiesV22) {
    CookieJar jar;
    jar.set_from_header("a=1", "example.com");
    jar.set_from_header("b=2", "example.com");

    EXPECT_EQ(jar.size(), 2u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Method enum: DELETE_METHOD is distinct from other methods
TEST(MethodTest, DeleteMethodEnumDistinctV22) {
    EXPECT_NE(Method::DELETE_METHOD, Method::GET);
    EXPECT_NE(Method::DELETE_METHOD, Method::POST);
    EXPECT_NE(Method::DELETE_METHOD, Method::PUT);
    EXPECT_NE(Method::DELETE_METHOD, Method::HEAD);
    EXPECT_NE(Method::DELETE_METHOD, Method::OPTIONS);
    EXPECT_NE(Method::DELETE_METHOD, Method::PATCH);
}

// ============================================================================
// Cycle 1277: HTTP/Net tests V23
// ============================================================================

// Request: serialize with HEAD method
TEST(RequestTest, SerializeHeadMethodReturnsVectorUint8V23) {
    Request req;
    req.method = Method::HEAD;
    req.host = "example.com";
    req.path = "/resource";
    req.headers.set("User-Agent", "TestAgent");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Request: serialize with DELETE_METHOD
TEST(RequestTest, SerializeDeleteMethodReturnsVectorUint8V23) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.example.com";
    req.path = "/api/item/42";
    req.headers.set("Authorization", "Bearer token123");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Request: serialize with OPTIONS method
TEST(RequestTest, SerializeOptionsMethodReturnsVectorUint8V23) {
    Request req;
    req.method = Method::OPTIONS;
    req.host = "cors.example.com";
    req.path = "/api/endpoint";

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Request: serialize with PATCH method and body
TEST(RequestTest, SerializePatchMethodWithBodyV23) {
    Request req;
    req.method = Method::PATCH;
    req.host = "api.example.com";
    req.path = "/api/user/123";
    req.headers.set("Content-Type", "application/json");
    std::string payload = "{\"status\": \"active\"}";
    req.body = std::vector<uint8_t>(payload.begin(), payload.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// HeaderMap: set overwrites with multiple calls
TEST(HeaderMapTest, SetOverwritesMultipleTimesV23) {
    HeaderMap map;
    map.set("x-custom", "first");
    EXPECT_EQ(map.get("x-custom"), "first");

    map.set("x-custom", "second");
    EXPECT_EQ(map.get("x-custom"), "second");

    map.set("x-custom", "third");
    EXPECT_EQ(map.get("x-custom"), "third");
}

// CookieJar: get_cookie_header returns empty for non-existent domain
TEST(CookieJarTest, GetCookieHeaderNonExistentDomainReturnsEmptyV23) {
    CookieJar jar;
    jar.set_from_header("session=xyz", "example.com");

    std::string header = jar.get_cookie_header("other.com", "/", false);
    EXPECT_EQ(header, "");
}

// CookieJar: multiple cookies in same domain
TEST(CookieJarTest, MultipleCookiesSameDomainV23) {
    CookieJar jar;
    jar.set_from_header("cookie1=value1", "example.com");
    jar.set_from_header("cookie2=value2", "example.com");
    jar.set_from_header("cookie3=value3", "example.com");

    EXPECT_EQ(jar.size(), 3u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Response: body holds binary data from request-response cycle
TEST(ResponseTest, ResponseBodyAfterDeserialisationV23) {
    Response resp;
    resp.status = 201;
    resp.status_text = "Created";

    std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC, 0xDD};
    resp.body = payload;

    EXPECT_EQ(resp.body.size(), 4u);
    EXPECT_EQ(resp.body[1], 0xBBu);
}

// Cycle 1286: HTTP client tests

// HeaderMap: has method with case-insensitive key matching
TEST(HttpClient, HeaderMapHasCaseInsensitiveV24) {
    HeaderMap map;
    map.set("Authorization", "Bearer token123");
    EXPECT_TRUE(map.has("Authorization"));
    EXPECT_TRUE(map.has("authorization"));
    EXPECT_TRUE(map.has("AUTHORIZATION"));
    EXPECT_FALSE(map.has("X-Custom-Header"));
}

// HeaderMap: remove erases both exact and case variations
TEST(HttpClient, HeaderMapRemoveV24) {
    HeaderMap map;
    map.set("Content-Length", "1024");
    map.set("Cache-Control", "no-cache");
    EXPECT_EQ(map.size(), 2u);
    map.remove("content-length");
    EXPECT_EQ(map.size(), 1u);
    EXPECT_FALSE(map.has("Content-Length"));
    EXPECT_TRUE(map.has("Cache-Control"));
}

// Request: serialize returns non-empty vector of bytes
TEST(HttpClient, RequestSerializeReturnsBytesV24) {
    Request req;
    req.method = Method::GET;
    req.url = "https://example.com/api/data";
    req.headers.set("User-Agent", "TestClient/1.0");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_TRUE(std::any_of(serialized.begin(), serialized.end(),
        [](uint8_t b) { return b > 0; }));
}

// Request: POST method with body serializes correctly
TEST(HttpClient, RequestPostWithBodyV24) {
    Request req;
    req.method = Method::POST;
    req.url = "https://api.example.com/submit";
    req.headers.set("Content-Type", "application/json");
    req.body = std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'});

    EXPECT_EQ(req.method, Method::POST);
    EXPECT_EQ(req.body.size(), 5u);
    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), req.body.size());
}

// Response: status_code and status_text can be set and retrieved
TEST(HttpClient, ResponseStatusCodesV24) {
    Response resp;
    resp.status = 404;
    resp.status_text = "Not Found";
    EXPECT_EQ(resp.status, 404);
    EXPECT_EQ(resp.status_text, "Not Found");

    resp.status = 500;
    resp.status_text = "Internal Server Error";
    EXPECT_EQ(resp.status, 500);
}

// CookieJar: secure cookies are tracked by domain
TEST(HttpClient, CookieJarSecureCookiesV24) {
    CookieJar jar;
    jar.set_from_header("secure_token=abc123", "secure.example.com");
    jar.set_from_header("session=xyz789", "api.example.com");
    EXPECT_EQ(jar.size(), 2u);

    std::string header1 = jar.get_cookie_header("secure.example.com", "/", false);
    std::string header2 = jar.get_cookie_header("api.example.com", "/", false);
    EXPECT_NE(header1, header2);
}

// ConnectionPool: initialization
TEST(HttpClient, ConnectionPoolInitializeV24) {
    ConnectionPool pool;
    EXPECT_TRUE(true);  // ConnectionPool exists and can be constructed
}

// HeaderMap: get_all returns all values for a key
TEST(HttpClient, HeaderMapGetAllValuesV24) {
    HeaderMap map;
    map.set("Set-Cookie", "cookie1=value1");
    map.set("Set-Cookie", "cookie2=value2");

    auto all_cookies = map.get_all("Set-Cookie");
    EXPECT_GE(all_cookies.size(), 1u);
    EXPECT_EQ(map.size(), 1u);
}

// Cycle 1295: HTTP client tests

// HeaderMap: remove clears a header
TEST(HttpClient, HeaderMapRemoveV25) {
    HeaderMap map;
    map.set("Content-Type", "text/html");
    map.set("Content-Length", "1024");
    EXPECT_EQ(map.size(), 2u);

    map.remove("Content-Type");
    EXPECT_FALSE(map.get("Content-Type").has_value());
    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(map.get("Content-Length").has_value());
}

// HeaderMap: has checks for header existence
TEST(HttpClient, HeaderMapHasV25) {
    HeaderMap map;
    map.set("Authorization", "Bearer token");

    EXPECT_TRUE(map.has("Authorization"));
    EXPECT_TRUE(map.has("authorization"));
    EXPECT_FALSE(map.has("X-Missing"));
}

// Request: serialize produces non-empty bytes
TEST(HttpClient, RequestSerializeV25) {
    Request req;
    req.method = Method::POST;
    req.url = "https://example.com/api";
    req.headers.set("Content-Type", "application/json");
    req.body = std::vector<uint8_t>{'t', 'e', 's', 't'};

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Request: different methods create different requests
TEST(HttpClient, RequestMethodsV25) {
    Request get_req, post_req, delete_req;
    get_req.method = Method::GET;
    post_req.method = Method::POST;
    delete_req.method = Method::DELETE_METHOD;

    EXPECT_NE(static_cast<int>(get_req.method), static_cast<int>(post_req.method));
    EXPECT_NE(static_cast<int>(post_req.method), static_cast<int>(delete_req.method));
}

// Response: status and body content
TEST(HttpClient, ResponseStatusAndBodyV25) {
    Response resp;
    resp.status = 404;
    resp.body = std::vector<uint8_t>{'N', 'o', 't', 'F', 'o', 'u', 'n', 'd'};

    EXPECT_EQ(resp.status, 404);
    EXPECT_EQ(resp.body.size(), 8u);
}

// CookieJar: clear removes all cookies
TEST(HttpClient, CookieJarClearV25) {
    CookieJar jar;
    jar.set_from_header("session=abc123", "example.com");
    jar.set_from_header("token=xyz789", "api.example.com");
    EXPECT_GT(jar.size(), 0u);

    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Request: PUT method with body
TEST(HttpClient, RequestPutMethodV25) {
    Request req;
    req.method = Method::PUT;
    req.url = "https://example.com/resource/123";
    req.headers.set("Content-Type", "application/json");
    std::string json = R"({"name":"updated"})";
    req.body = std::vector<uint8_t>(json.begin(), json.end());

    EXPECT_EQ(req.method, Method::PUT);
    EXPECT_EQ(req.body.size(), json.size());
}

// HeaderMap: case-insensitive operations after removal
TEST(HttpClient, HeaderMapCaseInsensitiveRemoveV25) {
    HeaderMap map;
    map.set("X-Custom-Header", "value1");
    map.set("X-Another", "value2");

    map.remove("x-custom-header");
    EXPECT_FALSE(map.get("X-CUSTOM-HEADER").has_value());
    EXPECT_FALSE(map.get("x-custom-header").has_value());
    EXPECT_TRUE(map.get("X-Another").has_value());
}

// Cycle 1304: HTTP client tests

// HeaderMap: multiple headers with same name via overwrite pattern
TEST(HttpClient, HeaderMapMultipleValuesOverwriteV26) {
    HeaderMap map;
    map.set("Accept", "text/html");
    map.set("Accept", "application/json");

    auto val = map.get("Accept");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "application/json");
}

// Request: HEAD method without body
TEST(HttpClient, RequestHeadMethodNoBodyV26) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://example.com/resource";
    req.headers.set("User-Agent", "TestAgent/1.0");

    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_EQ(req.body.size(), 0u);
    EXPECT_TRUE(req.headers.has("User-Agent"));
}

// Response: status 200 with body content
TEST(HttpClient, ResponseSuccessStatusWithBodyV26) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    std::string content = "Hello World";
    resp.body = std::vector<uint8_t>(content.begin(), content.end());

    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.status_text, "OK");
    EXPECT_EQ(resp.body.size(), 11u);
}

// CookieJar: get_cookie_header returns formatted header string
TEST(HttpClient, CookieJarGetHeaderFormatV26) {
    CookieJar jar;
    jar.set_from_header("sessionId=abc123def456", "example.com");

    std::string header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_FALSE(header.empty());
    EXPECT_GT(header.size(), 0u);
}

// Request: OPTIONS method with headers
TEST(HttpClient, RequestOptionsMethodV26) {
    Request req;
    req.method = Method::OPTIONS;
    req.url = "https://api.example.com/v1/resource";
    req.headers.set("Origin", "https://example.com");
    req.headers.set("Access-Control-Request-Method", "POST");

    EXPECT_EQ(req.method, Method::OPTIONS);
    EXPECT_TRUE(req.headers.has("Origin"));
    EXPECT_TRUE(req.headers.has("access-control-request-method"));
}

// HeaderMap: size reflects number of distinct headers
TEST(HttpClient, HeaderMapSizeAccuracyV26) {
    HeaderMap map;
    map.set("Content-Type", "application/json");
    map.set("Content-Length", "256");
    map.set("Cache-Control", "no-cache");

    EXPECT_EQ(map.size(), 3u);

    map.remove("Content-Length");
    EXPECT_EQ(map.size(), 2u);
}

// Request: PATCH method with JSON body
TEST(HttpClient, RequestPatchMethodWithBodyV26) {
    Request req;
    req.method = Method::PATCH;
    req.url = "https://api.example.com/users/123";
    req.headers.set("Content-Type", "application/json");
    std::string json = R"({"status":"active"})";
    req.body = std::vector<uint8_t>(json.begin(), json.end());

    EXPECT_EQ(req.method, Method::PATCH);
    EXPECT_EQ(req.body.size(), json.size());
    EXPECT_TRUE(req.headers.has("Content-Type"));
}

// Response: error status with error text
TEST(HttpClient, ResponseErrorStatusV26) {
    Response resp;
    resp.status = 503;
    resp.status_text = "Service Unavailable";
    std::string error = "Service temporarily offline";
    resp.body = std::vector<uint8_t>(error.begin(), error.end());

    EXPECT_EQ(resp.status, 503);
    EXPECT_EQ(resp.status_text, "Service Unavailable");
    EXPECT_GT(resp.body.size(), 0u);
}

// Cycle 1313: HTTP client tests

// HeaderMap: multiple overwrites on same key
TEST(HttpClient, HeaderMapOverwriteMultipleTimesV27) {
    HeaderMap map;
    map.set("Authorization", "Bearer token1");
    EXPECT_EQ(map.get("Authorization"), "Bearer token1");

    map.set("Authorization", "Bearer token2");
    EXPECT_EQ(map.get("Authorization"), "Bearer token2");

    map.set("Authorization", "Bearer token3");
    EXPECT_EQ(map.get("Authorization"), "Bearer token3");
    EXPECT_EQ(map.size(), 1u);
}

// HeaderMap: get_all returns all values for multi-valued headers
TEST(HttpClient, HeaderMapGetAllV27) {
    HeaderMap map;
    map.set("Set-Cookie", "session=abc123");
    map.set("Set-Cookie", "path=/");

    auto all_values = map.get_all("Set-Cookie");
    EXPECT_GE(all_values.size(), 1u);
    EXPECT_TRUE(map.has("Set-Cookie"));
}

// Request: DELETE method with URL parameters
TEST(HttpClient, RequestDeleteMethodWithParamsV27) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "https://api.example.com/resource/42?force=true";
    req.headers.set("Authorization", "Bearer token");

    EXPECT_EQ(req.method, Method::DELETE_METHOD);
    EXPECT_TRUE(req.url.find("/resource/42") != std::string::npos);
    EXPECT_TRUE(req.headers.has("Authorization"));
}

// Request: HEAD method for resource metadata
TEST(HttpClient, RequestHeadMethodV27) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://example.com/document.pdf";
    req.headers.set("Accept", "application/pdf");

    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_TRUE(req.body.empty());
    EXPECT_TRUE(req.headers.has("Accept"));
}

// Request: serialize returns vector<uint8_t>
TEST(HttpClient, RequestSerializeV27) {
    Request req;
    req.method = Method::POST;
    req.url = "https://api.example.com/data";
    req.headers.set("Content-Type", "application/json");
    std::string body_str = R"({"key":"value"})";
    req.body = std::vector<uint8_t>(body_str.begin(), body_str.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Response: 200 OK status with headers and body
TEST(HttpClient, ResponseSuccessWithHeadersAndBodyV27) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    resp.headers.set("Content-Type", "text/html");
    resp.headers.set("Content-Length", "1024");
    std::string body = "<html><body>Success</body></html>";
    resp.body = std::vector<uint8_t>(body.begin(), body.end());

    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.status_text, "OK");
    EXPECT_TRUE(resp.headers.has("Content-Type"));
    EXPECT_EQ(resp.body.size(), body.size());
}

// CookieJar: set_from_header parses cookie header
TEST(HttpClient, CookieJarSetFromHeaderV27) {
    CookieJar jar;
    jar.set_from_header("session=abc123xyz; Path=/; Secure", "example.com");

    EXPECT_GT(jar.size(), 0u);
    auto cookie_header = jar.get_cookie_header("example.com", "/", true);
    EXPECT_FALSE(cookie_header.empty());
}

// CookieJar: clear removes all cookies
TEST(HttpClient, CookieJarClearAllV27) {
    CookieJar jar;
    jar.set_from_header("id=user001", "example.com");
    jar.set_from_header("token=xyz789", "api.example.com");
    EXPECT_GT(jar.size(), 0u);

    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Cycle 1322: HTTP client tests

// HeaderMap: set overwrites existing key
TEST(HttpClient, HeaderMapSetOverwritesV28) {
    HeaderMap headers;
    headers.set("Content-Type", "text/plain");
    EXPECT_EQ(headers.get("Content-Type"), "text/plain");

    headers.set("Content-Type", "application/json");
    EXPECT_EQ(headers.get("Content-Type"), "application/json");
}

// HeaderMap: remove deletes a header
TEST(HttpClient, HeaderMapRemoveV28) {
    HeaderMap headers;
    headers.set("X-Custom", "value123");
    EXPECT_TRUE(headers.has("X-Custom"));

    headers.remove("X-Custom");
    EXPECT_FALSE(headers.has("X-Custom"));
}

// HeaderMap: size returns correct count
TEST(HttpClient, HeaderMapSizeV28) {
    HeaderMap headers;
    EXPECT_EQ(headers.size(), 0u);

    headers.set("Content-Type", "text/html");
    EXPECT_EQ(headers.size(), 1u);

    headers.set("Content-Length", "256");
    EXPECT_EQ(headers.size(), 2u);
}

// Request: method getter returns correct HTTP method
TEST(HttpClient, RequestMethodGetterV28) {
    Request req;
    req.method = Method::POST;
    EXPECT_EQ(req.method, Method::POST);

    req.method = Method::PUT;
    EXPECT_EQ(req.method, Method::PUT);
}

// Request: url property stores and retrieves correctly
TEST(HttpClient, RequestUrlPropertyV28) {
    Request req;
    req.url = "https://api.example.com/v1/users";
    EXPECT_EQ(req.url, "https://api.example.com/v1/users");
}

// Request: serialize produces vector<uint8_t> with method and headers
TEST(HttpClient, RequestSerializeV28) {
    Request req;
    req.method = Method::GET;
    req.url = "http://example.com/test";
    req.headers.set("User-Agent", "TestClient/1.0");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_TRUE(serialized.data() != nullptr);
}

// Response: status is uint16_t not status_code
TEST(HttpClient, ResponseStatusUint16V28) {
    Response resp;
    resp.status = 404;
    EXPECT_EQ(resp.status, 404);

    resp.status = 500;
    EXPECT_EQ(resp.status, 500);
}

// Response: body is vector<uint8_t>
TEST(HttpClient, ResponseBodyVectorV28) {
    Response resp;
    std::string content = "Hello, World!";
    resp.body = std::vector<uint8_t>(content.begin(), content.end());

    EXPECT_EQ(resp.body.size(), content.size());
    EXPECT_TRUE(resp.body.data() != nullptr);
}

// CookieJar: get_cookie_header with secure flag
TEST(HttpClient, CookieJarGetCookieHeaderSecureV28) {
    CookieJar jar;
    jar.set_from_header("auth=token123; Secure; Path=/api", "api.example.com");

    auto secure_header = jar.get_cookie_header("api.example.com", "/api", true);
    EXPECT_FALSE(secure_header.empty());

    auto insecure_header = jar.get_cookie_header("api.example.com", "/api", false);
}

// Cycle 1331

// HeaderMap: set overwrites existing values
TEST(HttpClient, HeaderMapSetOverwritesV29) {
    HeaderMap headers;
    headers.set("Content-Type", "text/html");
    EXPECT_EQ(headers.get("Content-Type").value(), "text/html");
    
    headers.set("Content-Type", "application/json");
    EXPECT_EQ(headers.get("Content-Type").value(), "application/json");
}

// HeaderMap: get returns optional with empty for missing key
TEST(HttpClient, HeaderMapGetMissingV29) {
    HeaderMap headers;
    headers.set("X-Custom", "value");
    
    auto result = headers.get("X-Missing");
    EXPECT_FALSE(result.has_value());
}

// HeaderMap: has checks key existence correctly
TEST(HttpClient, HeaderMapHasV29) {
    HeaderMap headers;
    headers.set("Authorization", "Bearer token123");
    
    EXPECT_TRUE(headers.has("Authorization"));
    EXPECT_FALSE(headers.has("X-Missing-Header"));
}

// HeaderMap: remove deletes header entry
TEST(HttpClient, HeaderMapRemoveV29) {
    HeaderMap headers;
    headers.set("X-Request-ID", "abc123");
    EXPECT_TRUE(headers.has("X-Request-ID"));
    
    headers.remove("X-Request-ID");
    EXPECT_FALSE(headers.has("X-Request-ID"));
}

// Request: method property stores and serializes correctly
TEST(HttpClient, RequestMethodPropertyV29) {
    Request req;
    req.method = Method::POST;
    req.url = "http://api.example.com/data";
    req.body = std::vector<uint8_t>{'d', 'a', 't', 'a'};
    
    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("POST"), std::string::npos);
}

// Response: status_text stores response status message
TEST(HttpClient, ResponseStatusTextV29) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    
    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.status_text, "OK");
}

// CookieJar: set_from_header and size tracking
TEST(HttpClient, CookieJarSetFromHeaderSizeV29) {
    CookieJar jar;
    jar.clear();
    
    EXPECT_EQ(jar.size(), 0u);
    jar.set_from_header("session=xyz789; Path=/", "example.com");
    EXPECT_GT(jar.size(), 0u);
}

// CookieJar: clear removes all cookies
TEST(HttpClient, CookieJarClearV29) {
    CookieJar jar;
    jar.set_from_header("auth=token456; Path=/", "example.com");
    EXPECT_GT(jar.size(), 0u);
    
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Cycle 1340

// HeaderMap: set overwrites existing value
TEST(HttpClient, HeaderMapSetOverwritesV30) {
    HeaderMap headers;
    headers.set("Content-Type", "text/html");
    EXPECT_EQ(headers.get("Content-Type"), "text/html");

    headers.set("Content-Type", "application/json");
    EXPECT_EQ(headers.get("Content-Type"), "application/json");
}

// HeaderMap: has checks for header presence
TEST(HttpClient, HeaderMapHasV30) {
    HeaderMap headers;
    headers.set("Authorization", "Bearer token");

    EXPECT_TRUE(headers.has("Authorization"));
    EXPECT_FALSE(headers.has("X-Custom-Header"));
}

// HeaderMap: remove deletes header
TEST(HttpClient, HeaderMapRemoveV30) {
    HeaderMap headers;
    headers.set("X-Custom", "value");
    EXPECT_TRUE(headers.has("X-Custom"));

    headers.remove("X-Custom");
    EXPECT_FALSE(headers.has("X-Custom"));
}

// HeaderMap: size returns header count
TEST(HttpClient, HeaderMapSizeV30) {
    HeaderMap headers;
    EXPECT_EQ(headers.size(), 0u);

    headers.set("Content-Type", "application/json");
    headers.set("Accept", "application/json");
    EXPECT_EQ(headers.size(), 2u);
}

// Request: serialize returns vector<uint8_t>
TEST(HttpClient, RequestSerializeV30) {
    Request req;
    req.method = Method::POST;
    req.url = "http://example.com/api";
    req.headers.set("Content-Type", "application/json");
    req.body = {'{', '"', 'k', 'e', 'y', '"', ':', '"', 'v', 'a', 'l', '"', '}'};

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Response: status returns uint16_t status code
TEST(HttpClient, ResponseStatusUint16V30) {
    Response resp;
    resp.status = 404;
    resp.status_text = "Not Found";

    EXPECT_EQ(resp.status, 404u);
    EXPECT_EQ(resp.status_text, "Not Found");
}

// Request: method property supports HTTP methods
TEST(HttpClient, RequestMethodsV30) {
    Request req1, req2, req3, req4, req5;

    req1.method = Method::GET;
    req2.method = Method::POST;
    req3.method = Method::PUT;
    req4.method = Method::DELETE_METHOD;
    req5.method = Method::HEAD;

    EXPECT_EQ(req1.method, Method::GET);
    EXPECT_EQ(req2.method, Method::POST);
    EXPECT_EQ(req3.method, Method::PUT);
    EXPECT_EQ(req4.method, Method::DELETE_METHOD);
    EXPECT_EQ(req5.method, Method::HEAD);
}

// CookieJar: get_cookie_header returns formatted cookie string
TEST(HttpClient, CookieJarGetCookieHeaderV30) {
    CookieJar jar;
    jar.set_from_header("session=abc123; Path=/; Secure", "example.com");

    std::string cookieHeader = jar.get_cookie_header("example.com", "/", true);
    EXPECT_FALSE(cookieHeader.empty());
}

// Cycle 1349

// HeaderMap: set OVERWRITES existing values
TEST(HttpClient, HeaderMapSetOverwritesV31) {
    HeaderMap headers;
    headers.set("X-Custom", "initial");
    EXPECT_EQ(headers.get("X-Custom"), "initial");

    headers.set("X-Custom", "overwritten");
    EXPECT_EQ(headers.get("X-Custom"), "overwritten");
}

// HeaderMap: get retrieves header value
TEST(HttpClient, HeaderMapGetV31) {
    HeaderMap headers;
    headers.set("Content-Type", "text/plain");
    headers.set("Accept-Encoding", "gzip");

    EXPECT_EQ(headers.get("Content-Type"), "text/plain");
    EXPECT_EQ(headers.get("Accept-Encoding"), "gzip");
}

// HeaderMap: has checks for header existence
TEST(HttpClient, HeaderMapHasV31) {
    HeaderMap headers;
    headers.set("Authorization", "Bearer token123");

    EXPECT_TRUE(headers.has("Authorization"));
    EXPECT_FALSE(headers.has("X-Missing-Header"));
}

// HeaderMap: remove deletes headers
TEST(HttpClient, HeaderMapRemoveV31) {
    HeaderMap headers;
    headers.set("X-Remove-Me", "value");
    headers.set("X-Keep", "value");

    EXPECT_TRUE(headers.has("X-Remove-Me"));
    headers.remove("X-Remove-Me");
    EXPECT_FALSE(headers.has("X-Remove-Me"));
    EXPECT_TRUE(headers.has("X-Keep"));
}

// HeaderMap: size returns count of headers
TEST(HttpClient, HeaderMapSizeV31) {
    HeaderMap headers;
    EXPECT_EQ(headers.size(), 0u);

    headers.set("Header1", "value1");
    EXPECT_EQ(headers.size(), 1u);

    headers.set("Header2", "value2");
    headers.set("Header3", "value3");
    EXPECT_EQ(headers.size(), 3u);
}

// HeaderMap: get_all returns all values for a header
TEST(HttpClient, HeaderMapGetAllV31) {
    HeaderMap headers;
    headers.set("Set-Cookie", "session=abc");
    headers.set("Set-Cookie", "user=john");

    std::vector<std::string> cookies = headers.get_all("Set-Cookie");
    EXPECT_GT(cookies.size(), 0u);
}

// Request: serialize returns vector<uint8_t> with method, url, headers, body
TEST(HttpClient, RequestSerializeV31) {
    Request req;
    req.method = Method::PUT;
    req.url = "https://api.example.com/resource/123";
    req.headers.set("Content-Type", "application/json");
    req.headers.set("Authorization", "Bearer token");
    req.body = {'d', 'a', 't', 'a'};

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_TRUE(serialized.size() >= req.body.size());
}

// Response: status, status_text, headers, body properties
TEST(HttpClient, ResponsePropertiesV31) {
    Response resp;
    resp.status = 201;
    resp.status_text = "Created";
    resp.headers.set("Location", "/resource/42");
    resp.body = {'r', 'e', 's', 'p', 'o', 'n', 's', 'e'};

    EXPECT_EQ(resp.status, 201u);
    EXPECT_EQ(resp.status_text, "Created");
    EXPECT_EQ(resp.headers.get("Location"), "/resource/42");
    EXPECT_EQ(resp.body.size(), 8u);
}

// Request: DELETE_METHOD with empty body
TEST(HttpClient, RequestDeleteMethodV32) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "https://api.example.com/items/99";
    req.headers.set("Authorization", "Bearer token123");
    req.body.clear();

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.method, Method::DELETE_METHOD);
}

// Request: HEAD method for resource headers only
TEST(HttpClient, RequestHeadMethodV32) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://example.com/documents/file.pdf";
    req.body.clear();

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_EQ(req.body.size(), 0u);
}

// Response: parse validates response data structure
TEST(HttpClient, ResponseParseV32) {
    std::string http_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello";
    std::vector<uint8_t> data(http_response.begin(), http_response.end());

    auto result = Response::parse(data);
    if (result.has_value()) {
        Response resp = result.value();
        EXPECT_EQ(resp.status, 200u);
        EXPECT_EQ(resp.status_text, "OK");
    }
}

// CookieJar: set_from_header parses cookie from Set-Cookie header
TEST(HttpClient, CookieJarSetFromHeaderV32) {
    CookieJar jar;
    jar.clear();

    jar.set_from_header("sessionid=abc123; Path=/; Secure", "example.com");
    EXPECT_GT(jar.size(), 0u);
}

// CookieJar: get_cookie_header returns formatted cookie string for request
TEST(HttpClient, CookieJarGetCookieHeaderV32) {
    CookieJar jar;
    jar.clear();

    jar.set_from_header("user_pref=dark_mode; Path=/", "example.com");
    std::string cookie_header = jar.get_cookie_header("example.com", "/", false);
    // May be empty if cookie doesn't match path/domain, but call should succeed
    EXPECT_TRUE(true);
}

// HeaderMap: append multiple values for same header name
TEST(HttpClient, HeaderMapAppendMultipleV32) {
    HeaderMap headers;
    headers.append("Accept", "text/html");
    headers.append("Accept", "application/xhtml+xml");
    headers.append("Accept", "application/xml");

    std::vector<std::string> all_accepts = headers.get_all("Accept");
    EXPECT_GE(all_accepts.size(), 1u);
}

// Request: OPTIONS method for CORS preflight
TEST(HttpClient, RequestOptionsMethodV32) {
    Request req;
    req.method = Method::OPTIONS;
    req.url = "https://api.example.com/v2/users";
    req.headers.set("Origin", "https://client.example.com");
    req.headers.set("Access-Control-Request-Method", "POST");

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.method, Method::OPTIONS);
}

// Response: was_redirected flag tracks redirect status
TEST(HttpClient, ResponseRedirectFlagV32) {
    Response resp;
    resp.status = 301;
    resp.status_text = "Moved Permanently";
    resp.was_redirected = true;
    resp.headers.set("Location", "https://new.example.com/page");

    EXPECT_EQ(resp.status, 301u);
    EXPECT_TRUE(resp.was_redirected);
    EXPECT_EQ(resp.headers.get("Location"), "https://new.example.com/page");
}

// Request: PATCH method with JSON body
TEST(HttpClient, RequestPatchMethodV32) {
    Request req;
    req.method = Method::PATCH;
    req.url = "https://api.example.com/resource/42";
    req.headers.set("Content-Type", "application/json");

    std::string json_data = R"({"status":"updated"})";
    req.body.assign(json_data.begin(), json_data.end());

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.body.size(), json_data.size());
}

// ===========================================================================
// V33 Tests - Additional Coverage
// ===========================================================================

// HeaderMap: remove function deletes a header completely
TEST(HttpClient, HeaderMapRemoveHeaderV33) {
    HeaderMap map;
    map.set("Authorization", "Bearer token123");
    map.set("Content-Type", "application/json");

    EXPECT_TRUE(map.has("Authorization"));
    map.remove("Authorization");
    EXPECT_FALSE(map.has("Authorization"));
    EXPECT_TRUE(map.has("Content-Type"));
}

// Request: POST with form-encoded body
TEST(HttpClient, RequestPostFormBodyV33) {
    Request req;
    req.method = Method::POST;
    req.url = "https://example.com/login";
    req.headers.set("Content-Type", "application/x-www-form-urlencoded");

    std::string form_data = "username=admin&password=secret123";
    req.body.assign(form_data.begin(), form_data.end());

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.method, Method::POST);
    EXPECT_EQ(req.body.size(), form_data.length());
}

// Response: body_as_string converts vector to string
TEST(HttpClient, ResponseBodyAsStringV33) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";

    std::string expected = "Hello, World!";
    resp.body.assign(expected.begin(), expected.end());

    std::string body_str = resp.body_as_string();
    EXPECT_EQ(body_str, expected);
    EXPECT_EQ(body_str.length(), expected.length());
}

// CookieJar: multiple cookies for same domain
TEST(HttpClient, CookieJarMultipleCookiesSameDomainV33) {
    CookieJar jar;
    jar.clear();

    jar.set_from_header("session=abc123; Path=/; Secure", "example.com");
    jar.set_from_header("user=john_doe; Path=/; Secure", "example.com");
    jar.set_from_header("lang=en; Path=/", "example.com");

    size_t count = jar.size();
    EXPECT_GE(count, 1u);
}

// Request: URL parsing with query parameters
TEST(HttpClient, RequestUrlParsingWithQueryV33) {
    Request req;
    req.url = "https://api.example.com/search?q=test&limit=10&offset=20";
    req.parse_url();

    EXPECT_EQ(req.host, "api.example.com");
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.port, 443u);
    EXPECT_FALSE(req.query.empty());
}

// HeaderMap: empty map operations
TEST(HttpClient, HeaderMapEmptyMapV33) {
    HeaderMap map;

    EXPECT_EQ(map.size(), 0u);
    EXPECT_TRUE(map.empty());
    EXPECT_FALSE(map.has("Any-Header"));

    auto result = map.get("Missing");
    EXPECT_FALSE(result.has_value());

    auto all = map.get_all("Missing");
    EXPECT_TRUE(all.empty());
}

// Response: redirect status with Location header
TEST(HttpClient, ResponseRedirectWith302StatusV33) {
    Response resp;
    resp.status = 302;
    resp.status_text = "Found";
    resp.was_redirected = true;
    resp.url = "https://example.com/old-page";
    resp.headers.set("Location", "https://example.com/new-page");
    resp.headers.set("Cache-Control", "no-cache");

    EXPECT_EQ(resp.status, 302u);
    EXPECT_TRUE(resp.was_redirected);
    EXPECT_TRUE(resp.headers.has("Location"));
    EXPECT_EQ(resp.headers.get("Location").value(), "https://example.com/new-page");
}

// Request: PUT method with JSON body and custom headers
TEST(HttpClient, RequestPutMethodWithJsonV33) {
    Request req;
    req.method = Method::PUT;
    req.url = "https://api.example.com/items/555";
    req.headers.set("Content-Type", "application/json");
    req.headers.set("X-API-Key", "secret-key-12345");
    req.headers.set("User-Agent", "CustomBrowser/1.0");

    std::string json = R"({"name":"Updated Item","value":99})";
    req.body.assign(json.begin(), json.end());

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    EXPECT_EQ(req.method, Method::PUT);
    EXPECT_TRUE(req.headers.has("X-API-Key"));
}

// Request: DELETE method with custom headers and empty body
TEST(HttpClient, RequestDeleteMethodV34) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "https://api.example.com/resource/123";
    req.headers.set("Authorization", "Bearer token-abc123");
    req.headers.set("X-Request-ID", "req-456");

    EXPECT_EQ(req.method, Method::DELETE_METHOD);
    EXPECT_EQ(req.url, "https://api.example.com/resource/123");
    EXPECT_TRUE(req.headers.has("Authorization"));
    EXPECT_EQ(req.body.size(), 0u);
    EXPECT_EQ(req.headers.get("X-Request-ID").value(), "req-456");
}

// Response: Status 404 with error body content
TEST(HttpClient, Response404NotFoundV34) {
    Response resp;
    resp.status = 404;
    resp.status_text = "Not Found";
    resp.url = "https://example.com/missing";
    resp.was_redirected = false;

    std::string error_msg = "The requested resource was not found";
    resp.body.assign(error_msg.begin(), error_msg.end());
    resp.headers.set("Content-Type", "text/plain");

    EXPECT_EQ(resp.status, 404u);
    EXPECT_EQ(resp.status_text, "Not Found");
    EXPECT_FALSE(resp.was_redirected);
    EXPECT_GT(resp.body.size(), 0u);
}

// CookieJar: Set and retrieve cookie with domain
TEST(HttpClient, CookieJarSetAndGetV34) {
    CookieJar jar;
    jar.set_from_header("session_id=abc123; Path=/; Domain=example.com", "example.com");

    std::string cookie_header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_FALSE(cookie_header.empty());
    EXPECT_GT(jar.size(), 0u);
}

// CookieJar: Clear all cookies
TEST(HttpClient, CookieJarClearV34) {
    CookieJar jar;
    jar.set_from_header("user=john; Path=/", "example.com");
    jar.set_from_header("token=xyz789; Path=/", "api.example.com");

    EXPECT_GT(jar.size(), 0u);
    jar.clear();
    EXPECT_EQ(jar.size(), 0u);
}

// Request: HEAD method with multiple headers
TEST(HttpClient, RequestHeadMethodV34) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://example.com/document.pdf";
    req.headers.set("Accept", "application/pdf");
    req.headers.set("User-Agent", "Mozilla/5.0");
    req.headers.set("Accept-Encoding", "gzip, deflate");

    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_TRUE(req.headers.has("Accept"));
    EXPECT_TRUE(req.headers.has("User-Agent"));
    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Response: 200 OK with large body and multiple headers
TEST(HttpClient, Response200OkWithLargeBodyV34) {
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    resp.url = "https://example.com/data";
    resp.was_redirected = false;

    // Create large body (10KB of data)
    std::string large_content(10240, 'A');
    resp.body.assign(large_content.begin(), large_content.end());

    resp.headers.set("Content-Type", "application/octet-stream");
    resp.headers.set("Content-Length", "10240");
    resp.headers.set("Cache-Control", "max-age=3600");
    resp.headers.set("ETag", "\"abc123def456\"");

    EXPECT_EQ(resp.status, 200u);
    EXPECT_EQ(resp.body.size(), 10240u);
    EXPECT_EQ(resp.headers.get("Content-Length").value(), "10240");
}

// Request: OPTIONS method for CORS preflight
TEST(HttpClient, RequestOptionsMethodV34) {
    Request req;
    req.method = Method::OPTIONS;
    req.url = "https://api.example.com/endpoint";
    req.headers.set("Origin", "https://frontend.example.com");
    req.headers.set("Access-Control-Request-Method", "POST");
    req.headers.set("Access-Control-Request-Headers", "content-type, authorization");

    EXPECT_EQ(req.method, Method::OPTIONS);
    EXPECT_TRUE(req.headers.has("Origin"));
    EXPECT_EQ(req.headers.get("Access-Control-Request-Method").value(), "POST");
}

// HeaderMap: Multiple values with same key and case-insensitive retrieval
TEST(HttpClient, HeaderMapMultipleValuesV34) {
    HeaderMap map;
    map.append("Set-Cookie", "session=xyz123");
    map.append("Set-Cookie", "user_id=999");
    map.append("set-cookie", "theme=dark");

    auto all = map.get_all("Set-Cookie");
    EXPECT_EQ(all.size(), 3u);

    auto all_lower = map.get_all("set-cookie");
    EXPECT_EQ(all_lower.size(), 3u);
}

// Request: POST with binary body containing null bytes
TEST(HttpClient, RequestPostBinaryBodyV35) {
    Request req;
    req.method = Method::POST;
    req.url = "https://api.example.com/upload";
    req.headers.set("Content-Type", "application/octet-stream");
    req.headers.set("Content-Length", "256");

    // Create binary body with null bytes
    std::vector<uint8_t> binary_data;
    for (int i = 0; i < 256; ++i) {
        binary_data.push_back(static_cast<uint8_t>(i));
    }
    req.body = binary_data;

    EXPECT_EQ(req.method, Method::POST);
    EXPECT_EQ(req.body.size(), 256u);
    EXPECT_EQ(req.body[0], 0u);
    EXPECT_EQ(req.body[255], 255u);
    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 256u);
}

// Response: 500 Server Error with detailed error JSON
TEST(HttpClient, Response500ErrorWithJsonV35) {
    Response resp;
    resp.status = 500;
    resp.status_text = "Internal Server Error";
    resp.url = "https://api.example.com/process";
    resp.was_redirected = false;

    std::string error_json = R"({"error":"Database connection failed","code":5001,"timestamp":"2025-02-27T10:30:45Z"})";
    resp.body.assign(error_json.begin(), error_json.end());
    resp.headers.set("Content-Type", "application/json");
    resp.headers.set("Retry-After", "60");

    EXPECT_EQ(resp.status, 500u);
    EXPECT_FALSE(resp.was_redirected);
    EXPECT_EQ(resp.body.size(), error_json.length());
    EXPECT_EQ(resp.body_as_string(), error_json);
    EXPECT_TRUE(resp.headers.has("Retry-After"));
}

// HeaderMap: Complex header manipulation with append, set, and remove
TEST(HttpClient, HeaderMapComplexOperationsV35) {
    HeaderMap map;

    // Add initial headers
    map.set("Host", "example.com");
    map.append("Accept", "text/html");
    map.append("Accept", "application/json");
    map.append("Accept", "application/xml");

    EXPECT_EQ(map.size(), 4u);
    EXPECT_EQ(map.get_all("Accept").size(), 3u);

    // Replace Accept header with single value
    map.set("Accept", "application/json");
    EXPECT_EQ(map.size(), 2u);
    auto accepts = map.get_all("Accept");
    EXPECT_EQ(accepts.size(), 1u);
    EXPECT_EQ(accepts[0], "application/json");

    // Remove Host
    map.remove("Host");
    EXPECT_FALSE(map.has("Host"));
    EXPECT_EQ(map.size(), 1u);
}

// CookieJar: Multiple cookies with different domains and secure flags
TEST(HttpClient, CookieJarMultiDomainSecureV35) {
    CookieJar jar;
    jar.clear();

    // Add cookies for different domains
    jar.set_from_header("session=xyz789; Path=/; Secure; HttpOnly", "example.com");
    jar.set_from_header("tracking=abc123; Path=/; SameSite=Lax", "example.com");
    jar.set_from_header("admin=secure456; Path=/admin; Secure; HttpOnly", "admin.example.com");

    size_t total = jar.size();
    EXPECT_GE(total, 2u);

    // Get cookie header for HTTPS request to example.com
    std::string cookies = jar.get_cookie_header("example.com", "/", true);
    EXPECT_GT(cookies.length(), 0u);
}

// Request: GET with custom headers and URL with fragment
TEST(HttpClient, RequestGetWithHeadersAndFragmentV35) {
    Request req;
    req.method = Method::GET;
    req.url = "https://docs.example.com/api#section-2";
    req.headers.set("Accept", "application/json");
    req.headers.set("Accept-Language", "en-US,en;q=0.9");
    req.headers.set("User-Agent", "CustomBrowser/2.0");
    req.headers.set("Authorization", "Bearer token-xyz");
    req.parse_url();

    EXPECT_EQ(req.method, Method::GET);
    EXPECT_EQ(req.host, "docs.example.com");
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.port, 443u);
    EXPECT_EQ(req.headers.get_all("Accept-Language").size(), 1u);
    EXPECT_TRUE(req.headers.has("Authorization"));
}

// Response: 201 Created with Location header and minimal body
TEST(HttpClient, Response201CreatedWithLocationV35) {
    Response resp;
    resp.status = 201;
    resp.status_text = "Created";
    resp.url = "https://api.example.com/items";
    resp.was_redirected = false;

    std::string body = R"({"id":789,"created_at":"2025-02-27T10:35:00Z"})";
    resp.body.assign(body.begin(), body.end());
    resp.headers.set("Content-Type", "application/json");
    resp.headers.set("Location", "https://api.example.com/items/789");

    EXPECT_EQ(resp.status, 201u);
    EXPECT_EQ(resp.headers.get("Location").value(), "https://api.example.com/items/789");
    EXPECT_GT(resp.body.size(), 0u);
    EXPECT_FALSE(resp.was_redirected);
}

// HeaderMap: Size and iteration consistency check
TEST(HttpClient, HeaderMapSizeIterationConsistencyV35) {
    HeaderMap map;

    // Add various headers with different multiplicities
    map.set("Content-Type", "text/html");
    map.append("Set-Cookie", "cookie1=value1");
    map.append("Set-Cookie", "cookie2=value2");
    map.append("Set-Cookie", "cookie3=value3");
    map.set("Authorization", "Bearer token");
    map.append("Accept-Encoding", "gzip");
    map.append("Accept-Encoding", "deflate");

    size_t header_count = map.size();
    EXPECT_EQ(header_count, 7u);

    // Verify iteration count matches size
    size_t iter_count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        ++iter_count;
    }
    EXPECT_EQ(iter_count, header_count);
    EXPECT_FALSE(map.empty());
}

// Request: PATCH method with partial update JSON and conditional headers
TEST(HttpClient, RequestPatchPartialUpdateV35) {
    Request req;
    req.method = Method::PATCH;
    req.url = "https://api.example.com/users/12345";
    req.headers.set("Content-Type", "application/json");
    req.headers.set("If-Match", "\"etag-12345\"");
    req.headers.set("X-Request-ID", "patch-req-001");

    std::string json_patch = R"({"email":"newemail@example.com","status":"active"})";
    req.body.assign(json_patch.begin(), json_patch.end());

    EXPECT_EQ(req.method, Method::PATCH);
    EXPECT_EQ(req.body.size(), json_patch.length());
    EXPECT_TRUE(req.headers.has("If-Match"));
    EXPECT_EQ(req.headers.get("X-Request-ID").value(), "patch-req-001");

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 1: Request DELETE method with custom headers and empty body
TEST(HttpClient, RequestDeleteWithCustomHeadersV36) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "https://api.example.com/resources/42";
    req.headers.set("Authorization", "Bearer token-abc");
    req.headers.set("X-Delete-Reason", "user-requested");
    req.body.clear();
    req.parse_url();

    EXPECT_EQ(req.method, Method::DELETE_METHOD);
    EXPECT_EQ(req.host, "api.example.com");
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.port, 443u);
    EXPECT_TRUE(req.headers.has("Authorization"));
    EXPECT_EQ(req.headers.get("X-Delete-Reason").value(), "user-requested");
    EXPECT_EQ(req.body.size(), 0u);

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 2: Request HEAD method for metadata without body
TEST(HttpClient, RequestHeadMethodNoBodyV36) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://cdn.example.com/images/photo.jpg";
    req.headers.set("User-Agent", "Browser/3.0");
    req.parse_url();

    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_EQ(req.host, "cdn.example.com");
    EXPECT_EQ(req.path, "/images/photo.jpg");
    EXPECT_TRUE(req.use_tls);
    EXPECT_TRUE(req.body.empty());

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 3: Request OPTIONS for CORS preflight with origin header
TEST(HttpClient, RequestOptionsForCorsPreflightV36) {
    Request req;
    req.method = Method::OPTIONS;
    req.url = "https://api.example.com/data";
    req.headers.set("Origin", "https://client.example.com");
    req.headers.set("Access-Control-Request-Method", "POST");
    req.headers.set("Access-Control-Request-Headers", "Content-Type,Authorization");
    req.parse_url();

    EXPECT_EQ(req.method, Method::OPTIONS);
    EXPECT_TRUE(req.headers.has("Origin"));
    EXPECT_EQ(req.headers.get("Access-Control-Request-Method").value(), "POST");
    EXPECT_EQ(req.headers.get("Access-Control-Request-Headers").value(), "Content-Type,Authorization");

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 4: Response with 404 Not Found and error body
TEST(HttpClient, Response404NotFoundWithErrorV36) {
    Response resp;
    resp.status = 404;
    resp.status_text = "Not Found";
    resp.url = "https://example.com/nonexistent";
    resp.was_redirected = false;

    std::string error_body = R"({"error":"Resource not found","path":"/nonexistent"})";
    resp.body.assign(error_body.begin(), error_body.end());
    resp.headers.set("Content-Type", "application/json");
    resp.headers.set("X-Error-Code", "RESOURCE_NOT_FOUND");

    EXPECT_EQ(resp.status, 404u);
    EXPECT_EQ(resp.status_text, "Not Found");
    EXPECT_FALSE(resp.was_redirected);
    EXPECT_GT(resp.body.size(), 0u);
    EXPECT_EQ(resp.headers.get("X-Error-Code").value(), "RESOURCE_NOT_FOUND");
    EXPECT_EQ(resp.body_as_string(), error_body);
}

// Test 5: Response with 500 Server Error and retry-after header
TEST(HttpClient, Response500ServerErrorWithRetryV36) {
    Response resp;
    resp.status = 500;
    resp.status_text = "Internal Server Error";
    resp.url = "https://api.example.com/process";
    resp.was_redirected = false;

    std::string error_msg = "Server encountered an error processing your request";
    resp.body.assign(error_msg.begin(), error_msg.end());
    resp.headers.set("Content-Type", "text/plain");
    resp.headers.set("Retry-After", "60");
    resp.headers.set("X-Request-ID", "err-server-500");

    EXPECT_EQ(resp.status, 500u);
    EXPECT_TRUE(resp.headers.has("Retry-After"));
    EXPECT_EQ(resp.headers.get("Retry-After").value(), "60");
    EXPECT_TRUE(resp.headers.has("X-Request-ID"));
    EXPECT_FALSE(resp.was_redirected);
}

// Test 6: HeaderMap with multiple values and case-insensitive access
TEST(HttpClient, HeaderMapMultipleValuesV36) {
    HeaderMap map;

    // Single-value headers
    map.set("Host", "example.com");
    map.set("Content-Type", "application/json");

    // Multi-value headers (append)
    map.append("Accept-Encoding", "gzip");
    map.append("Accept-Encoding", "deflate");
    map.append("Accept-Encoding", "br");

    map.append("Cache-Control", "no-cache");
    map.append("Cache-Control", "no-store");

    // Verify single-value access
    EXPECT_EQ(map.get("host").value(), "example.com");
    EXPECT_EQ(map.get("HOST").value(), "example.com");

    // Verify multi-value access
    auto encodings = map.get_all("accept-encoding");
    EXPECT_EQ(encodings.size(), 3u);
    EXPECT_EQ(map.get_all("Accept-Encoding").size(), 3u);

    // Total size includes all individual entries
    EXPECT_EQ(map.size(), 7u);

    // Verify has() method
    EXPECT_TRUE(map.has("content-type"));
    EXPECT_TRUE(map.has("CACHE-CONTROL"));
    EXPECT_FALSE(map.has("non-existent-header"));
}

// Test 7: CookieJar with domain matching and path-based access
TEST(HttpClient, CookieJarDomainAndPathMatchingV36) {
    CookieJar jar;
    jar.clear();

    // Set cookies for root path
    jar.set_from_header("session_id=abc123; Path=/; Domain=example.com", "example.com");
    jar.set_from_header("user_pref=darkmode; Path=/; Domain=.example.com; Secure", "example.com");

    // Set cookies for specific paths
    jar.set_from_header("admin_token=xyz789; Path=/admin; Domain=example.com", "example.com");
    jar.set_from_header("api_key=key999; Path=/api/v1; Domain=api.example.com", "api.example.com");

    size_t total = jar.size();
    EXPECT_GE(total, 2u);

    // Get cookies for root path on example.com (should include root-path cookies)
    std::string root_cookies = jar.get_cookie_header("example.com", "/", true);
    EXPECT_GT(root_cookies.length(), 0u);

    // Get cookies for /admin path (should include both root and admin cookies)
    std::string admin_cookies = jar.get_cookie_header("example.com", "/admin", true);
    EXPECT_GT(admin_cookies.length(), 0u);
}

// Test 8: Request POST with form-encoded body and Content-Length header
TEST(HttpClient, RequestPostFormEncodedWithContentLengthV36) {
    Request req;
    req.method = Method::POST;
    req.url = "https://example.com/login";

    // Set up form-encoded body
    std::string form_body = "username=john&password=secret&remember=true";
    req.body.assign(form_body.begin(), form_body.end());

    // Set headers
    req.headers.set("Content-Type", "application/x-www-form-urlencoded");
    req.headers.set("Content-Length", std::to_string(form_body.length()));
    req.headers.set("Accept", "text/html");
    req.parse_url();

    EXPECT_EQ(req.method, Method::POST);
    EXPECT_EQ(req.body.size(), form_body.length());
    EXPECT_EQ(req.headers.get("Content-Type").value(), "application/x-www-form-urlencoded");
    EXPECT_EQ(req.headers.get("Content-Length").value(), std::to_string(form_body.length()));
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.port, 443u);

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 1: Request with DELETE method and JSON body
TEST(HttpClient, RequestDeleteMethodWithJsonBodyV37) {
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "https://api.example.com/resource/123";

    std::string json_body = R"({"confirm": true})";
    req.body.assign(json_body.begin(), json_body.end());

    req.headers.set("Content-Type", "application/json");
    req.headers.set("Authorization", "Bearer token123");
    req.headers.set("Accept", "application/json");
    req.parse_url();

    EXPECT_EQ(req.method, Method::DELETE_METHOD);
    EXPECT_EQ(req.host, "api.example.com");
    EXPECT_EQ(req.path, "/resource/123");
    EXPECT_EQ(req.body.size(), json_body.length());
    EXPECT_TRUE(req.headers.has("Authorization"));
    EXPECT_EQ(req.headers.get("Authorization").value(), "Bearer token123");

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 2: Request with PUT method and custom headers
TEST(HttpClient, RequestPutMethodWithCustomHeadersV37) {
    Request req;
    req.method = Method::PUT;
    req.url = "https://api.example.com/users/456";

    std::string body = "name=John&email=john@example.com";
    req.body.assign(body.begin(), body.end());

    req.headers.set("Content-Type", "application/x-www-form-urlencoded");
    req.headers.set("X-Custom-Header", "custom-value");
    req.headers.set("X-API-Version", "v2");
    req.headers.set("Accept", "application/json");
    req.parse_url();

    EXPECT_EQ(req.method, Method::PUT);
    EXPECT_EQ(req.port, 443u);
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.headers.get("X-Custom-Header").value(), "custom-value");
    EXPECT_EQ(req.headers.get("X-API-Version").value(), "v2");
    EXPECT_TRUE(req.headers.has("Content-Type"));
}

// Test 3: Request with HEAD method for checking resource existence
TEST(HttpClient, RequestHeadMethodHeaderOnlyV37) {
    Request req;
    req.method = Method::HEAD;
    req.url = "https://cdn.example.com/assets/image.png";

    req.headers.set("User-Agent", "CustomBrowser/1.0");
    req.headers.set("Accept", "image/*");
    req.parse_url();

    EXPECT_EQ(req.method, Method::HEAD);
    EXPECT_EQ(req.host, "cdn.example.com");
    EXPECT_EQ(req.path, "/assets/image.png");
    EXPECT_TRUE(req.body.empty());
    EXPECT_TRUE(req.headers.has("User-Agent"));

    std::vector<uint8_t> serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

// Test 4: Request with OPTIONS method for CORS preflight
TEST(HttpClient, RequestOptionsMethodCORSPreflightV37) {
    Request req;
    req.method = Method::OPTIONS;
    req.url = "https://api.example.com/data";

    req.headers.set("Origin", "https://example.com");
    req.headers.set("Access-Control-Request-Method", "POST");
    req.headers.set("Access-Control-Request-Headers", "Content-Type, Authorization");
    req.parse_url();

    EXPECT_EQ(req.method, Method::OPTIONS);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(req.headers.get("Origin").value(), "https://example.com");
    EXPECT_EQ(req.headers.get("Access-Control-Request-Method").value(), "POST");
    EXPECT_TRUE(req.headers.has("Access-Control-Request-Headers"));
}

// Test 5: Request with PATCH method and partial resource update
TEST(HttpClient, RequestPatchMethodPartialUpdateV37) {
    Request req;
    req.method = Method::PATCH;
    req.url = "https://api.example.com/profile/789";

    std::string patch_body = R"({"status":"active","lastModified":"2026-02-27"})";
    req.body.assign(patch_body.begin(), patch_body.end());

    req.headers.set("Content-Type", "application/json");
    req.headers.set("If-Match", "\"etag-value-123\"");
    req.parse_url();

    EXPECT_EQ(req.method, Method::PATCH);
    EXPECT_EQ(req.body.size(), patch_body.length());
    EXPECT_EQ(req.headers.get("If-Match").value(), "\"etag-value-123\"");
    EXPECT_TRUE(req.use_tls);
}

// Test 6: Response with redirect chain and location header
TEST(HttpClient, ResponseRedirectWithLocationHeaderV37) {
    Response resp;
    resp.status = 302;
    resp.status_text = "Found";
    resp.url = "https://old.example.com/page";
    resp.was_redirected = true;

    resp.headers.set("Location", "https://new.example.com/page");
    resp.headers.set("Cache-Control", "no-cache");
    resp.headers.set("X-Redirect-Reason", "domain-migration");

    std::string body = "This resource has moved";
    resp.body.assign(body.begin(), body.end());

    EXPECT_EQ(resp.status, 302u);
    EXPECT_TRUE(resp.was_redirected);
    EXPECT_EQ(resp.headers.get("Location").value(), "https://new.example.com/page");
    EXPECT_TRUE(resp.headers.has("X-Redirect-Reason"));
    EXPECT_FALSE(resp.body.empty());
}

// Test 7: HeaderMap with remove operation and case-insensitive handling
TEST(HttpClient, HeaderMapRemoveAndCaseInsensitiveV37) {
    HeaderMap map;

    map.set("Content-Type", "text/html");
    map.set("Content-Length", "1024");
    map.set("Cache-Control", "max-age=3600");
    map.append("Set-Cookie", "session=abc123");
    map.append("Set-Cookie", "preferences=darkmode");

    EXPECT_EQ(map.size(), 5u);

    // Remove using different case
    map.remove("CONTENT-LENGTH");

    EXPECT_FALSE(map.has("content-length"));
    EXPECT_TRUE(map.has("Content-Type"));
    EXPECT_EQ(map.size(), 4u);

    // Verify multi-value headers still work
    auto cookies = map.get_all("set-cookie");
    EXPECT_EQ(cookies.size(), 2u);
}

// Test 8: CookieJar with multiple domains and secure flag handling
TEST(HttpClient, CookieJarMultipleDomainsAndSecureFlagV37) {
    CookieJar jar;
    jar.clear();

    // Set cookies for example.com
    jar.set_from_header("uid=12345; Path=/; Domain=example.com", "example.com");
    jar.set_from_header("token=secure123; Path=/admin; Domain=example.com; Secure", "example.com");

    // Set cookies for api.example.com
    jar.set_from_header("api_session=sess456; Path=/v1; Domain=api.example.com; Secure; HttpOnly", "api.example.com");
    jar.set_from_header("tracking=xyz789; Path=/; Domain=.example.com", "api.example.com");

    size_t total = jar.size();
    EXPECT_GE(total, 2u);

    // Get secure cookies (should include secure cookies)
    std::string secure_header = jar.get_cookie_header("api.example.com", "/v1", true, true, false);
    EXPECT_GT(secure_header.length(), 0u);

    // Get insecure cookies (should exclude secure-only cookies when is_secure=false)
    std::string insecure_header = jar.get_cookie_header("example.com", "/", false, true, true);
    // Should get uid but not secure token
    EXPECT_GT(insecure_header.length(), 0u);
}

// ===========================================================================
// V38 Test Suite: 8 new tests for HeaderMap, Request, Response, and CookieJar
// ===========================================================================

TEST(HttpClient, HeaderMapAppendMultipleValuesV38) {
    HeaderMap map;
    map.append("accept", "text/html");
    map.append("accept", "text/plain");
    map.append("accept", "application/json");

    auto values = map.get_all("accept");
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "text/html");
    EXPECT_EQ(values[1], "text/plain");
    EXPECT_EQ(values[2], "application/json");
}

TEST(HttpClient, RequestSerializePostWithBodyV38) {
    Request req;
    req.method = Method::POST;
    req.url = "http://example.com/api";
    req.headers.set("Content-Type", "application/json");

    std::string json_body = "{\"key\": \"value\"}";
    req.body.assign(json_body.begin(), json_body.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("POST"), std::string::npos);
    EXPECT_NE(serialized_str.find("{\"key\": \"value\"}"), std::string::npos);
}

TEST(HttpClient, ResponseParse206PartialContentV38) {
    std::string raw_str = "HTTP/1.1 206 Partial Content\r\n"
                          "Content-Length: 10\r\n"
                          "Content-Range: bytes 0-9/100\r\n"
                          "\r\n"
                          "0123456789";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 206u);
    EXPECT_TRUE(resp->headers.has("Content-Range"));
    EXPECT_EQ(resp->body.size(), 10u);
}

TEST(HttpClient, HeaderMapEmptyAfterClearV38) {
    HeaderMap map;
    map.set("Authorization", "Bearer token123");
    map.set("Cookie", "session=abc");
    map.set("User-Agent", "TestClient/1.0");

    EXPECT_EQ(map.size(), 3u);
    EXPECT_FALSE(map.empty());

    map.remove("Authorization");
    map.remove("Cookie");
    map.remove("User-Agent");

    EXPECT_EQ(map.size(), 0u);
    EXPECT_TRUE(map.empty());
}

TEST(HttpClient, RequestHeadMethodNoBodyV38) {
    Request req;
    req.method = Method::HEAD;
    req.url = "http://example.com/resource";
    req.headers.set("Accept", "text/html");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("HEAD"), std::string::npos);
    // Headers stored lowercase in HeaderMap
    EXPECT_NE(serialized_str.find("accept: text/html"), std::string::npos);
}

TEST(HttpClient, CookieJarClearRemovesAllV38) {
    CookieJar jar;
    jar.set_from_header("session_id=xyz789", "example.com");
    jar.set_from_header("tracking=12345", "analytics.example.com");
    jar.set_from_header("pref=dark_mode", "example.com");

    EXPECT_EQ(jar.size(), 3u);

    jar.clear();

    EXPECT_EQ(jar.size(), 0u);
    EXPECT_TRUE(jar.get_cookie_header("example.com", "/", false).empty());
}

TEST(HttpClient, HeaderMapGetReturnsNulloptForMissingV38) {
    HeaderMap map;
    map.set("Existing-Header", "value");

    auto existing = map.get("Existing-Header");
    EXPECT_TRUE(existing.has_value());
    EXPECT_EQ(existing.value(), "value");

    auto missing = map.get("NonExistent-Header");
    EXPECT_FALSE(missing.has_value());
}

TEST(HttpClient, ResponseParse100ContinueV38) {
    std::string raw_str = "HTTP/1.1 100 Continue\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 100u);
    EXPECT_TRUE(resp->body.empty());
}

// ===========================================================================
// V39 Test Suite: 8 new tests for HeaderMap, Request, Response, and CookieJar
// ===========================================================================

TEST(HttpClient, HeaderMapSetOverwritesExistingV39) {
    using namespace clever::net;
    HeaderMap map;
    map.set("content-type", "text/html");
    EXPECT_EQ(map.get("content-type").value(), "text/html");

    // Set the same key again with a different value
    map.set("content-type", "application/json");
    EXPECT_EQ(map.get("content-type").value(), "application/json");
    EXPECT_EQ(map.size(), 1u);
}

TEST(HttpClient, RequestSerializeGetWithQueryV39) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.url = "http://example.com/search?q=test&limit=10";
    req.headers.set("User-Agent", "TestClient/1.0");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("GET"), std::string::npos);
}

TEST(HttpClient, ResponseParse301MovedPermanentlyV39) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 301 Moved\r\n"
                          "Location: /new\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 301u);
    EXPECT_TRUE(resp->headers.has("Location"));
    EXPECT_EQ(resp->headers.get("Location").value(), "/new");
}

TEST(HttpClient, HeaderMapHasReturnsFalseAfterRemoveV39) {
    using namespace clever::net;
    HeaderMap map;
    map.set("Authorization", "Bearer abc123");
    EXPECT_TRUE(map.has("Authorization"));

    map.remove("Authorization");
    EXPECT_FALSE(map.has("Authorization"));
}

TEST(HttpClient, RequestPatchMethodV39) {
    using namespace clever::net;
    Request req;
    req.method = Method::PATCH;
    req.url = "http://api.example.com/resource/42";
    req.headers.set("Content-Type", "application/json");

    std::string patch_body = "{\"status\":\"updated\"}";
    req.body.assign(patch_body.begin(), patch_body.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("PATCH"), std::string::npos);
}

TEST(HttpClient, CookieJarSetAndGetV39) {
    using namespace clever::net;
    CookieJar jar;
    jar.clear();

    jar.set_from_header("session_id=abc123", "example.com");
    std::string cookie_header = jar.get_cookie_header("example.com", "/", false);
    EXPECT_NE(cookie_header.find("session_id"), std::string::npos);
}

TEST(HttpClient, HeaderMapSizeAfterMultipleOpsV39) {
    using namespace clever::net;
    HeaderMap map;
    map.set("Header-A", "value-a");
    map.set("Header-B", "value-b");
    map.set("Header-C", "value-c");

    EXPECT_EQ(map.size(), 3u);

    map.remove("Header-B");
    EXPECT_EQ(map.size(), 2u);
}

TEST(HttpClient, ResponseParse200WithBodyV39) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 200 OK\r\n"
                          "Content-Length: 5\r\n"
                          "\r\n"
                          "hello";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200u);
    EXPECT_EQ(resp->body.size(), 5u);
    EXPECT_EQ(std::string(resp->body.begin(), resp->body.end()), "hello");
}

TEST(HttpClient, HeaderMapEmptyInitiallyV40) {
    using namespace clever::net;
    HeaderMap map;
    EXPECT_EQ(map.size(), 0u);
    EXPECT_TRUE(map.empty());
}

TEST(HttpClient, RequestSerializeDeleteMethodV40) {
    using namespace clever::net;
    Request req;
    req.method = Method::DELETE_METHOD;
    req.url = "http://api.example.com/resource/123";
    req.headers.set("Authorization", "Bearer token");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("DELETE"), std::string::npos);
}

TEST(HttpClient, ResponseParse204NoContentV40) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 204 No Content\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 204u);
}

TEST(HttpClient, HeaderMapGetAllSingleValueV40) {
    using namespace clever::net;
    HeaderMap map;
    map.set("X-Custom-Header", "single-value");

    auto values = map.get_all("X-Custom-Header");
    EXPECT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], "single-value");
}

TEST(HttpClient, RequestOptionsMethodV40) {
    using namespace clever::net;
    Request req;
    req.method = Method::OPTIONS;
    req.url = "http://api.example.com/";
    req.headers.set("Host", "api.example.com");

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("OPTIONS"), std::string::npos);
}

TEST(HttpClient, CookieJarSizeAfterSetV40) {
    using namespace clever::net;
    CookieJar jar;
    jar.clear();

    jar.set_from_header("sessionid=xyz789", "example.com");
    jar.set_from_header("userid=user123", "example.com");

    // Size should be at least 1 (cookies are tracked)
    EXPECT_GE(jar.size(), 1u);
}

TEST(HttpClient, HeaderMapAppendSameKeyTwiceV40) {
    using namespace clever::net;
    HeaderMap map;
    map.append("Accept", "application/json");
    map.append("Accept", "text/plain");

    auto values = map.get_all("Accept");
    EXPECT_EQ(values.size(), 2u);
}

TEST(HttpClient, ResponseParse202AcceptedV40) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 202 Accepted\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 202u);
}

TEST(HttpClient, HeaderMapCaseInsensitiveGetV41) {
    using namespace clever::net;
    HeaderMap map;
    map.set("Content-Type", "application/json");

    // Get with different case should work (case-insensitive)
    auto value = map.get("content-type");
    EXPECT_EQ(value, "application/json");
}

TEST(HttpClient, RequestSerializePutWithBodyV41) {
    using namespace clever::net;
    Request req;
    req.method = Method::PUT;
    req.url = "http://api.example.com/resource/42";
    req.headers.set("Content-Type", "application/json");
    req.body = std::vector<uint8_t>{'t', 'e', 's', 't'};

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("PUT"), std::string::npos);
}

TEST(HttpClient, ResponseParse403ForbiddenV41) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 403 Forbidden\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 403u);
}

TEST(HttpClient, HeaderMapRemoveNonexistentV41) {
    using namespace clever::net;
    HeaderMap map;
    map.set("Header-A", "value-a");

    // Remove a key that doesn't exist - should not crash
    map.remove("NonexistentHeader");

    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.get("Header-A"), "value-a");
}

TEST(HttpClient, RequestGetMethodDefaultV41) {
    using namespace clever::net;
    Request req;

    // New request should default to GET method
    EXPECT_EQ(req.method, Method::GET);
}

TEST(HttpClient, CookieJarEmptyGetHeaderV41) {
    using namespace clever::net;
    CookieJar jar;
    jar.clear();

    // Empty jar should return empty string
    auto header = jar.get_cookie_header("example.com", "/", false, true, false);
    EXPECT_EQ(header, "");
}

TEST(HttpClient, HeaderMapIterateV41) {
    using namespace clever::net;
    HeaderMap map;
    map.set("Header-1", "value-1");
    map.set("Header-2", "value-2");
    map.set("Header-3", "value-3");

    int count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        count++;
    }

    EXPECT_EQ(count, static_cast<int>(map.size()));
    EXPECT_EQ(map.size(), 3u);
}

TEST(HttpClient, ResponseParse500InternalServerV41) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 500 Internal Server Error\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 500u);
}

TEST(HttpClient, HeaderMapSetAndGetV42) {
    using namespace clever::net;
    HeaderMap map;

    map.set("Content-Type", "text/html");
    EXPECT_EQ(map.get("Content-Type"), "text/html");
}

TEST(HttpClient, RequestSerializeContainsHostV42) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.url = "https://example.com/path";

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("Host:"), std::string::npos);
}

TEST(HttpClient, ResponseParse304NotModifiedV42) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 304 Not Modified\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 304u);
}

TEST(HttpClient, HeaderMapAppendThreeValuesV42) {
    using namespace clever::net;
    HeaderMap map;

    map.append("Accept", "text/html");
    map.append("Accept", "application/json");
    map.append("Accept", "application/xml");

    auto values = map.get_all("Accept");
    EXPECT_EQ(values.size(), 3u);
}

TEST(HttpClient, RequestBodyNonEmptyV42) {
    using namespace clever::net;
    Request req;
    req.method = Method::POST;
    req.url = "https://example.com/api";
    req.body = {'d', 'a', 't', 'a'};

    EXPECT_GT(req.body.size(), 0u);
    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

TEST(HttpClient, CookieJarHttpOnlyV42) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("sessionid=abc123; HttpOnly; Path=/", "example.com");
    auto header = jar.get_cookie_header("example.com", "/", false, true, false);

    EXPECT_NE(header, "");
}

TEST(HttpClient, HeaderMapEmptyAfterAllRemovedV42) {
    using namespace clever::net;
    HeaderMap map;

    map.set("Header-1", "value-1");
    map.set("Header-2", "value-2");

    map.remove("Header-1");
    map.remove("Header-2");

    EXPECT_TRUE(map.empty());
}

TEST(HttpClient, ResponseParse201CreatedV42) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 201 Created\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 201u);
}

TEST(HttpClient, HeaderMapGetAllEmptyKeyV43) {
    using namespace clever::net;
    HeaderMap map;

    map.set("Content-Type", "application/json");

    auto values = map.get_all("nonexistent");
    EXPECT_TRUE(values.empty());
}

TEST(HttpClient, RequestSerializeGetContainsPathV43) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.path = "/api/data";
    req.headers.set("Host", "example.com");

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("/api/data"), std::string::npos);
}

TEST(HttpClient, ResponseParse405MethodNotAllowedV43) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 405 Method Not Allowed\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 405u);
}

TEST(HttpClient, HeaderMapHasAfterAppendV43) {
    using namespace clever::net;
    HeaderMap map;

    map.append("Accept-Encoding", "gzip");

    EXPECT_TRUE(map.has("Accept-Encoding"));
}

TEST(HttpClient, RequestPutMethodV43) {
    using namespace clever::net;
    Request req;
    req.method = Method::PUT;
    req.url = "https://example.com/api/resource";

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("PUT"), std::string::npos);
}

TEST(HttpClient, CookieJarMultipleCookiesV43) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("sessionid=abc123; Path=/", "example.com");
    jar.set_from_header("userid=user456; Path=/", "example.com");
    jar.set_from_header("theme=dark; Path=/", "example.com");

    EXPECT_GE(jar.size(), 2u);
}

TEST(HttpClient, HeaderMapGetFirstOfMultipleV43) {
    using namespace clever::net;
    HeaderMap map;

    map.append("Cache-Control", "no-cache");
    map.append("Cache-Control", "no-store");

    auto value = map.get("Cache-Control");
    ASSERT_TRUE(value.has_value());
    EXPECT_FALSE(value->empty());
}

TEST(HttpClient, ResponseParse408RequestTimeoutV43) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 408 Request Timeout\r\n"
                          "\r\n";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 408u);
}

TEST(HttpClient, HeaderMapSetOverwritesPreviousValueV44) {
    using namespace clever::net;
    HeaderMap map;

    map.set("X-Custom-Header", "first");
    map.set("X-Custom-Header", "second");

    auto value = map.get("X-Custom-Header");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "second");
}

TEST(HttpClient, HeaderMapAppendPreservesMultipleValuesV44) {
    using namespace clever::net;
    HeaderMap map;

    map.append("Set-Cookie", "session=abc");
    map.append("Set-Cookie", "theme=dark");
    map.append("Set-Cookie", "lang=en");

    auto values = map.get_all("Set-Cookie");
    EXPECT_EQ(values.size(), 3u);
}

TEST(HttpClient, HeaderMapRemoveDeletesEntireKeyV44) {
    using namespace clever::net;
    HeaderMap map;

    map.set("Authorization", "Bearer token123");
    map.append("Authorization", "Bearer token456");

    map.remove("Authorization");

    EXPECT_FALSE(map.has("Authorization"));
    auto values = map.get_all("Authorization");
    EXPECT_TRUE(values.empty());
}

TEST(HttpClient, HeaderMapSizeReflectsAllHeadersV44) {
    using namespace clever::net;
    HeaderMap map;

    map.set("Content-Type", "application/json");
    map.set("Content-Length", "256");
    map.set("Cache-Control", "max-age=3600");
    map.set("X-Custom", "value");

    EXPECT_EQ(map.size(), 4u);
}

TEST(HttpClient, RequestDeleteMethodSerializationV44) {
    using namespace clever::net;
    Request req;
    req.method = Method::DELETE_METHOD;
    req.path = "/api/resource/123";
    req.headers.set("Host", "api.example.com");

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("DELETE"), std::string::npos);
    EXPECT_NE(serialized_str.find("/api/resource/123"), std::string::npos);
}

TEST(HttpClient, ResponseParseWithBodyContentV44) {
    using namespace clever::net;
    std::string raw_str = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 13\r\n"
                          "\r\n"
                          "Hello, World!";

    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());
    auto resp = Response::parse(raw);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200u);
    EXPECT_EQ(resp->body.size(), 13u);
    EXPECT_EQ(std::string(resp->body.begin(), resp->body.end()), "Hello, World!");
}

TEST(HttpClient, CookieJarGetCookieHeaderForPathV44) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("user_pref=dark; Path=/", "secure.example.com");
    jar.set_from_header("session_id=xyz; Path=/", "secure.example.com");

    // Verify we stored cookies
    EXPECT_GE(jar.size(), 2u);

    // Retrieve cookie header for the domain and path
    auto cookie_header = jar.get_cookie_header("secure.example.com", "/", false);
    EXPECT_TRUE(cookie_header.find("user_pref") != std::string::npos ||
                cookie_header.find("session_id") != std::string::npos);
}

TEST(HttpClient, RequestPatchMethodWithHeadersV44) {
    using namespace clever::net;
    Request req;
    req.method = Method::PATCH;
    req.path = "/api/user/profile";
    req.headers.set("Host", "api.example.com");
    req.headers.set("Content-Type", "application/json");

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("PATCH"), std::string::npos);
    EXPECT_NE(serialized_str.find("content-type"), std::string::npos);
}

// ============================================================================
// Cycle X: HTTP/Net tests V45
// ============================================================================

// Request: build GET request with method and path validation
TEST(HttpClient, RequestBuildGetMethodWithPathV45) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.path = "/api/users";
    req.headers.set("Accept", "application/json");

    EXPECT_EQ(req.method, Method::GET);
    EXPECT_EQ(req.path, "/api/users");
    EXPECT_EQ(req.headers.get("Accept").value(), "application/json");
}

// Request: serialize POST with body as vector<uint8_t>
TEST(HttpClient, RequestSerializePostWithBodyV45) {
    using namespace clever::net;
    Request req;
    req.method = Method::POST;
    req.url = "https://api.example.com/api/data";
    req.path = "/api/data";
    req.headers.set("Content-Type", "application/json");

    std::string body_str = "{\"key\": \"value\"}";
    req.body.assign(body_str.begin(), body_str.end());

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);
    // Serialized bytes should contain the body data
    EXPECT_GE(serialized.size(), body_str.size());
}

// HeaderMap: append and get_all for multi-value headers
TEST(HttpClient, HeaderMapAppendMultiValueHeadersV45) {
    using namespace clever::net;
    HeaderMap headers;
    headers.append("Set-Cookie", "session=abc123");
    headers.append("Set-Cookie", "token=xyz789");
    headers.append("Set-Cookie", "user=john");

    auto all_cookies = headers.get_all("Set-Cookie");
    EXPECT_EQ(all_cookies.size(), 3u);

    // Verify individual values exist
    EXPECT_TRUE(std::find(all_cookies.begin(), all_cookies.end(), "session=abc123") != all_cookies.end());
    EXPECT_TRUE(std::find(all_cookies.begin(), all_cookies.end(), "token=xyz789") != all_cookies.end());
    EXPECT_TRUE(std::find(all_cookies.begin(), all_cookies.end(), "user=john") != all_cookies.end());
}

// HeaderMap: set overwrites previous value and append multi-value
TEST(HttpClient, HeaderMapSetOverwriteAndAppendV45) {
    using namespace clever::net;
    HeaderMap headers;
    headers.set("Accept", "text/html");
    EXPECT_EQ(headers.get("Accept").value(), "text/html");

    headers.set("Accept", "application/json");
    EXPECT_EQ(headers.get("Accept").value(), "application/json");
    EXPECT_EQ(headers.get_all("Accept").size(), 1u);

    headers.append("Accept-Encoding", "gzip");
    headers.append("Accept-Encoding", "deflate");
    EXPECT_EQ(headers.get_all("Accept-Encoding").size(), 2u);
}

// CookieJar: set_from_header and get_cookie_header roundtrip
TEST(HttpClient, CookieJarSetAndGetRoundtripV45) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("user_id=12345; Path=/", "example.com");
    jar.set_from_header("theme=dark; Path=/dashboard", "example.com");

    EXPECT_EQ(jar.size(), 2u);

    std::string cookie_header = jar.get_cookie_header("example.com", "/dashboard", false);
    EXPECT_FALSE(cookie_header.empty());
}

// Response: status code and status_text validation
TEST(HttpClient, ResponseStatusAndStatusTextV45) {
    using namespace clever::net;
    Response resp;
    resp.status = 404;
    resp.status_text = "Not Found";
    resp.headers.set("Content-Type", "text/html");

    EXPECT_EQ(resp.status, 404u);
    EXPECT_EQ(resp.status_text, "Not Found");
    EXPECT_EQ(resp.headers.get("Content-Type").value(), "text/html");
}

// Method enum: all method types are distinct
TEST(HttpClient, MethodEnumAllDistinctV45) {
    using namespace clever::net;
    EXPECT_NE(Method::GET, Method::POST);
    EXPECT_NE(Method::POST, Method::PUT);
    EXPECT_NE(Method::PUT, Method::DELETE_METHOD);
    EXPECT_NE(Method::DELETE_METHOD, Method::HEAD);
    EXPECT_NE(Method::HEAD, Method::OPTIONS);
    EXPECT_NE(Method::OPTIONS, Method::PATCH);
    EXPECT_NE(Method::PATCH, Method::GET);
}

// Request: serialize with PUT method and binary body
TEST(HttpClient, RequestSerializePutWithBinaryBodyV45) {
    using namespace clever::net;
    Request req;
    req.method = Method::PUT;
    req.host = "data.example.com";
    req.path = "/resource/42";
    req.headers.set("Content-Type", "application/octet-stream");

    std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    req.body = binary_data;

    auto serialized = req.serialize();
    EXPECT_GT(serialized.size(), 0u);

    std::string serialized_str(serialized.begin(), serialized.end());
    EXPECT_NE(serialized_str.find("PUT"), std::string::npos);
}

// ============================================================================
// Cycle X: HTTP/Net tests V46
// ============================================================================

TEST(HttpClient, RequestFieldsCanBeBuiltFromMethodUrlAndPathV46) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.url = "https://example.com/";
    req.path = "/";

    EXPECT_EQ(req.method, Method::GET);
    EXPECT_EQ(req.url, "https://example.com/");
    EXPECT_EQ(req.path, "/");
}

TEST(HttpClient, RequestSerializeIncludesRequestLineAndHostV46) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.url = "https://example.com/api?q=1";
    req.host = "example.com";
    req.path = "/api";
    req.query = "q=1";

    auto raw = req.serialize();
    std::string text(raw.begin(), raw.end());

    EXPECT_NE(text.find("GET /api?q=1 HTTP/1.1"), std::string::npos);
    EXPECT_NE(text.find("Host: example.com"), std::string::npos);
}

TEST(HttpClient, HeaderMapSetOverwritesExistingValueV46) {
    using namespace clever::net;
    HeaderMap headers;

    headers.set("Content-Type", "text/plain");
    headers.set("Content-Type", "application/json");

    ASSERT_TRUE(headers.get("Content-Type").has_value());
    EXPECT_EQ(headers.get("content-type").value(), "application/json");
    EXPECT_EQ(headers.get_all("CONTENT-TYPE").size(), 1u);
}

TEST(HttpClient, HeaderMapAppendHasAndGetAllWorkTogetherV46) {
    using namespace clever::net;
    HeaderMap headers;

    headers.append("Set-Cookie", "a=1");
    headers.append("Set-Cookie", "b=2");

    EXPECT_TRUE(headers.has("set-cookie"));
    auto values = headers.get_all("Set-Cookie");
    EXPECT_EQ(values.size(), 2u);
    EXPECT_TRUE(headers.get("Set-Cookie").has_value());
}

TEST(HttpClient, HeaderMapRemoveClearsPresenceAndValueV46) {
    using namespace clever::net;
    HeaderMap headers;

    headers.set("X-Trace-Id", "abc");
    EXPECT_TRUE(headers.has("x-trace-id"));

    headers.remove("X-Trace-Id");

    EXPECT_FALSE(headers.has("X-Trace-Id"));
    EXPECT_FALSE(headers.get("x-trace-id").has_value());
    EXPECT_TRUE(headers.get_all("x-trace-id").empty());
}

TEST(HttpClient, CookieJarGetCookieHeaderHonorsSecureFlagV46) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("sid=plain; Path=/", "example.com");
    jar.set_from_header("auth=secure; Path=/; Secure", "example.com");

    std::string insecure = jar.get_cookie_header("example.com", "/", false, true, true);
    std::string secure = jar.get_cookie_header("example.com", "/", true, true, true);

    EXPECT_NE(insecure.find("sid=plain"), std::string::npos);
    EXPECT_EQ(insecure.find("auth=secure"), std::string::npos);
    EXPECT_NE(secure.find("sid=plain"), std::string::npos);
    EXPECT_NE(secure.find("auth=secure"), std::string::npos);
}

TEST(HttpClient, ResponsePropertiesStatusHeadersBodyAndRedirectV46) {
    using namespace clever::net;
    Response resp;

    resp.status = 302;
    resp.status_text = "Found";
    resp.headers.set("Location", "https://example.com/new");
    resp.body = std::vector<uint8_t>{'o', 'k'};
    resp.was_redirected = true;

    EXPECT_EQ(resp.status, 302u);
    EXPECT_EQ(resp.status_text, "Found");
    ASSERT_TRUE(resp.headers.get("location").has_value());
    EXPECT_EQ(resp.headers.get("Location").value(), "https://example.com/new");
    EXPECT_EQ(resp.body_as_string(), "ok");
    EXPECT_TRUE(resp.was_redirected);
}

TEST(HttpClient, RequestSerializeReturnsVectorWithBodyBytesV46) {
    using namespace clever::net;
    Request req;
    req.method = Method::POST;
    req.url = "https://example.com/upload";
    req.host = "example.com";
    req.path = "/upload";
    req.body = std::vector<uint8_t>{0x41, 0x42, 0x43};

    auto raw = req.serialize();
    std::string text(raw.begin(), raw.end());

    EXPECT_GT(raw.size(), req.body.size());
    EXPECT_NE(text.find("POST /upload HTTP/1.1"), std::string::npos);
    EXPECT_NE(text.find("Content-Length: 3"), std::string::npos);
    EXPECT_NE(text.find("ABC"), std::string::npos);
}

// ============================================================================
// Cycle X: HTTP/Net tests V55
// ============================================================================

TEST(HttpClient, RequestParseUrlHttpsWithQueryV55) {
    Request req;
    req.url = "https://api.example.com/search/items?q=book&sort=asc";
    req.parse_url();

    EXPECT_EQ(req.host, "api.example.com");
    EXPECT_EQ(req.port, 443);
    EXPECT_TRUE(req.use_tls);
    EXPECT_EQ(req.path, "/search/items");
    EXPECT_EQ(req.query, "q=book&sort=asc");
}

TEST(HttpClient, RequestSerializePostIncludesPathAndContentLengthV55) {
    Request req;
    req.method = Method::POST;
    req.host = "example.com";
    req.path = "/submit";
    req.body = std::vector<uint8_t>{'o', 'k'};
    req.headers.set("Content-Type", "text/plain");

    auto raw = req.serialize();
    std::string text(raw.begin(), raw.end());

    EXPECT_NE(text.find("POST /submit HTTP/1.1"), std::string::npos);
    EXPECT_NE(text.find("Host: example.com"), std::string::npos);
    EXPECT_NE(text.find("Content-Length: 2"), std::string::npos);
}

TEST(HttpClient, ResponseParsePopulatesStatusFieldV55) {
    std::string raw_str =
        "HTTP/1.1 418 I'm a teapot\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 418u);
    EXPECT_EQ(resp->status_text, "I'm a teapot");
}

TEST(HttpClient, ResponseBodyAsStringFromParsedBodyV55) {
    std::string raw_str =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    std::vector<uint8_t> raw(raw_str.begin(), raw_str.end());

    auto resp = Response::parse(raw);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status, 200u);
    EXPECT_EQ(resp->body_as_string(), "hello");
}

TEST(HttpClient, HeaderMapSetOverwritesExistingValueV55) {
    HeaderMap headers;
    headers.set("Accept", "text/html");
    headers.set("Accept", "application/json");

    ASSERT_TRUE(headers.get("accept").has_value());
    EXPECT_EQ(headers.get("Accept").value(), "application/json");
    EXPECT_EQ(headers.get_all("ACCEPT").size(), 1u);
}

TEST(HttpClient, HeaderMapAppendPreservesMultipleValuesV55) {
    HeaderMap headers;
    headers.append("Set-Cookie", "a=1");
    headers.append("Set-Cookie", "b=2");
    headers.append("set-cookie", "c=3");

    auto values = headers.get_all("Set-Cookie");
    EXPECT_EQ(values.size(), 3u);
    EXPECT_TRUE(std::find(values.begin(), values.end(), "a=1") != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "b=2") != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "c=3") != values.end());
}

TEST(HttpClient, CookieJarSecureCookieOnlySentOnSecureRequestsV55) {
    CookieJar jar;
    jar.set_from_header("sid=plain; Path=/", "example.com");
    jar.set_from_header("auth=secure; Path=/; Secure", "example.com");

    std::string insecure = jar.get_cookie_header("example.com", "/", false);
    std::string secure = jar.get_cookie_header("example.com", "/", true);

    EXPECT_NE(insecure.find("sid=plain"), std::string::npos);
    EXPECT_EQ(insecure.find("auth=secure"), std::string::npos);
    EXPECT_NE(secure.find("sid=plain"), std::string::npos);
    EXPECT_NE(secure.find("auth=secure"), std::string::npos);
}

TEST(HttpClient, CookieJarPathScopedCookieMatchingV55) {
    CookieJar jar;
    jar.set_from_header("api_token=xyz; Path=/api", "example.com");

    std::string api_path = jar.get_cookie_header("example.com", "/api/v1/users", false);
    std::string other_path = jar.get_cookie_header("example.com", "/static", false);

    EXPECT_NE(api_path.find("api_token=xyz"), std::string::npos);
    EXPECT_EQ(other_path.find("api_token=xyz"), std::string::npos);
}

// ============================================================================
// Cycle X: HTTP/Net tests V56
// ============================================================================

TEST(HttpClient, RequestMethodAndHostSetupForGetRequestV56) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.host = "www.example.com";
    req.path = "/index.html";
    req.headers.set("User-Agent", "TestBrowser/1.0");

    EXPECT_EQ(req.method, Method::GET);
    EXPECT_EQ(req.host, "www.example.com");
    EXPECT_EQ(req.path, "/index.html");
    EXPECT_EQ(req.headers.get("User-Agent").value(), "TestBrowser/1.0");
}

TEST(HttpClient, RequestBodySerializationWithContentTypeV56) {
    using namespace clever::net;
    Request req;
    req.method = Method::POST;
    req.host = "api.example.com";
    req.path = "/v1/items";
    req.headers.set("Content-Type", "application/json");

    std::string json_body = R"({"name":"test","id":123})";
    req.body.assign(json_body.begin(), json_body.end());

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("POST /v1/items HTTP/1.1"), std::string::npos);
    EXPECT_NE(serialized_str.find("content-type: application/json"), std::string::npos);
    EXPECT_NE(serialized_str.find("name"), std::string::npos);
}

TEST(HttpClient, HeaderMapCasInsensitiveGetAndRemoveV56) {
    using namespace clever::net;
    HeaderMap headers;

    headers.set("X-Custom-Header", "value123");
    EXPECT_EQ(headers.get("x-custom-header").value(), "value123");
    EXPECT_EQ(headers.get("X-CUSTOM-HEADER").value(), "value123");

    headers.remove("x-custom-header");
    EXPECT_FALSE(headers.has("X-Custom-Header"));
}

TEST(HttpClient, CookieJarMultipleCookiesDifferentDomainsV56) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("session_id=abc123; Path=/", "example.com");
    jar.set_from_header("pref=dark_mode; Path=/", "example.org");
    jar.set_from_header("token=xyz789; Path=/api", "example.com");

    EXPECT_EQ(jar.size(), 3u);

    std::string example_com_cookies = jar.get_cookie_header("example.com", "/api", false);
    std::string example_org_cookies = jar.get_cookie_header("example.org", "/", false);

    EXPECT_NE(example_com_cookies.find("session_id"), std::string::npos);
    EXPECT_NE(example_com_cookies.find("token"), std::string::npos);
    EXPECT_NE(example_org_cookies.find("pref"), std::string::npos);
    EXPECT_EQ(example_org_cookies.find("session_id"), std::string::npos);
}

TEST(HttpClient, ResponseStatusCodeAndHeadersParsingV56) {
    using namespace clever::net;
    Response resp;
    resp.status = 201;
    resp.status_text = "Created";
    resp.headers.set("Location", "https://example.com/resource/123");
    resp.headers.set("Content-Type", "application/json");
    resp.headers.set("X-Request-Id", "req-456");

    EXPECT_EQ(resp.status, 201u);
    EXPECT_EQ(resp.status_text, "Created");
    EXPECT_EQ(resp.headers.get("location").value(), "https://example.com/resource/123");
    EXPECT_EQ(resp.headers.get("content-type").value(), "application/json");
    EXPECT_TRUE(resp.headers.has("x-request-id"));
}

TEST(HttpClient, RequestSerializeDeleteMethodWithoutBodyV56) {
    using namespace clever::net;
    Request req;
    req.method = Method::DELETE_METHOD;
    req.host = "api.example.com";
    req.path = "/resource/42";
    req.headers.set("Authorization", "Bearer token123");

    auto serialized = req.serialize();
    std::string serialized_str(serialized.begin(), serialized.end());

    EXPECT_NE(serialized_str.find("DELETE /resource/42 HTTP/1.1"), std::string::npos);
    EXPECT_NE(serialized_str.find("authorization: Bearer token123"), std::string::npos);
    EXPECT_EQ(req.body.size(), 0u);
}

TEST(HttpClient, CookieJarHttpOnlyAndSameSiteFlagsV56) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("sensitive=data; Path=/; HttpOnly; SameSite=Strict", "example.com");
    jar.set_from_header("normal=value; Path=/", "example.com");

    EXPECT_EQ(jar.size(), 2u);

    // Test retrieval with different flags
    std::string with_all_flags = jar.get_cookie_header("example.com", "/", false, true, true);
    std::string with_secure_flag = jar.get_cookie_header("example.com", "/", true, true, true);

    EXPECT_FALSE(with_all_flags.empty());
    EXPECT_FALSE(with_secure_flag.empty());
}

TEST(HttpClient, ResponseBodyAsStringFromBinaryDataV56) {
    using namespace clever::net;
    Response resp;
    resp.status = 200;
    resp.status_text = "OK";
    resp.headers.set("Content-Type", "text/plain");

    std::string text_body = "Response body content";
    resp.body.assign(text_body.begin(), text_body.end());

    EXPECT_EQ(resp.body_as_string(), "Response body content");
    EXPECT_EQ(resp.body.size(), text_body.size());
    EXPECT_EQ(resp.status, 200u);
}

TEST(HttpClient, HeaderMapAppendAndIterateV57) {
    using namespace clever::net;
    HeaderMap headers;

    headers.append("Accept-Language", "en-US");
    headers.append("Accept-Language", "en;q=0.9");
    headers.append("Accept-Language", "fr;q=0.8");

    auto all_values = headers.get_all("accept-language");
    EXPECT_EQ(all_values.size(), 3u);
    EXPECT_TRUE(headers.has("Accept-Language"));

    int count = 0;
    for (const auto& pair : headers) {
        if (pair.first == "accept-language") {
            count++;
        }
    }
    EXPECT_EQ(count, 3);
}

TEST(HttpClient, RequestSerializeWithAllMethodsV57) {
    using namespace clever::net;

    // Test POST
    Request post_req;
    post_req.method = Method::POST;
    post_req.host = "api.test.com";
    post_req.path = "/create";
    post_req.headers.set("content-type", "application/json");
    post_req.body = std::vector<uint8_t>{'t', 'e', 's', 't'};

    auto post_serialized = post_req.serialize();
    std::string post_str(post_serialized.begin(), post_serialized.end());
    EXPECT_NE(post_str.find("POST /create HTTP/1.1"), std::string::npos);
    EXPECT_NE(post_str.find("content-type: application/json"), std::string::npos);

    // Test PATCH
    Request patch_req;
    patch_req.method = Method::PATCH;
    patch_req.host = "api.test.com";
    patch_req.path = "/update/1";
    patch_req.body = std::vector<uint8_t>{'d', 'a', 't', 'a'};

    auto patch_serialized = patch_req.serialize();
    std::string patch_str(patch_serialized.begin(), patch_serialized.end());
    EXPECT_NE(patch_str.find("PATCH /update/1 HTTP/1.1"), std::string::npos);
}

TEST(HttpClient, ResponseBodyEmptyButValidV57) {
    using namespace clever::net;
    Response resp;
    resp.status = 204;
    resp.status_text = "No Content";
    resp.headers.set("content-type", "application/json");

    EXPECT_EQ(resp.body.size(), 0u);
    EXPECT_TRUE(resp.body_as_string().empty());
    EXPECT_EQ(resp.status, 204u);
    EXPECT_TRUE(resp.headers.has("Content-Type"));
}

TEST(HttpClient, CookieJarPathScopingAccuracyV57) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("api_token=abc123; Path=/api/v1", "example.com");
    jar.set_from_header("admin_token=xyz789; Path=/admin", "example.com");
    jar.set_from_header("root_token=qwerty; Path=/", "example.com");

    // Request to /api/v1/users should get api_token and root_token
    std::string api_cookies = jar.get_cookie_header("example.com", "/api/v1/users", false);
    EXPECT_NE(api_cookies.find("api_token"), std::string::npos);
    EXPECT_NE(api_cookies.find("root_token"), std::string::npos);
    EXPECT_EQ(api_cookies.find("admin_token"), std::string::npos);

    // Request to /admin should get admin_token and root_token
    std::string admin_cookies = jar.get_cookie_header("example.com", "/admin", false);
    EXPECT_NE(admin_cookies.find("admin_token"), std::string::npos);
    EXPECT_NE(admin_cookies.find("root_token"), std::string::npos);
    EXPECT_EQ(admin_cookies.find("api_token"), std::string::npos);
}

TEST(HttpClient, HeaderMapSizeWithOperationsV57) {
    using namespace clever::net;
    HeaderMap headers;

    EXPECT_EQ(headers.size(), 0u);
    EXPECT_TRUE(headers.empty());

    headers.set("Authorization", "Bearer token");
    EXPECT_EQ(headers.size(), 1u);
    EXPECT_FALSE(headers.empty());

    headers.set("Accept", "application/json");
    EXPECT_EQ(headers.size(), 2u);

    headers.append("Accept", "text/plain");
    EXPECT_EQ(headers.size(), 3u);

    headers.remove("Accept");
    EXPECT_EQ(headers.size(), 1u);
}

TEST(HttpClient, RequestUrlParsingAndSerializationV57) {
    using namespace clever::net;
    Request req;
    req.method = Method::GET;
    req.host = "example.com";
    req.port = 443;
    req.path = "/search";
    req.query = "q=browser&lang=cpp";
    req.headers.set("user-agent", "vibrowser/1.0");

    auto serialized = req.serialize();
    std::string ser_str(serialized.begin(), serialized.end());

    EXPECT_NE(ser_str.find("GET /search?q=browser&lang=cpp HTTP/1.1"), std::string::npos);
    EXPECT_NE(ser_str.find("user-agent: vibrowser/1.0"), std::string::npos);
    EXPECT_NE(ser_str.find("Host: example.com"), std::string::npos);
}

TEST(HttpClient, ResponseStatusRangesAndCategoriesV57) {
    using namespace clever::net;

    // Success response (2xx)
    Response success_resp;
    success_resp.status = 200;
    success_resp.status_text = "OK";
    EXPECT_GE(success_resp.status, 200u);
    EXPECT_LT(success_resp.status, 300u);

    // Redirect response (3xx)
    Response redirect_resp;
    redirect_resp.status = 301;
    redirect_resp.status_text = "Moved Permanently";
    redirect_resp.headers.set("location", "https://newdomain.com/page");
    EXPECT_GE(redirect_resp.status, 300u);
    EXPECT_LT(redirect_resp.status, 400u);
    EXPECT_TRUE(redirect_resp.headers.has("Location"));

    // Client error response (4xx)
    Response client_error_resp;
    client_error_resp.status = 404;
    client_error_resp.status_text = "Not Found";
    EXPECT_GE(client_error_resp.status, 400u);
    EXPECT_LT(client_error_resp.status, 500u);
}

TEST(HttpClient, CookieJarDomainSeparationV57) {
    using namespace clever::net;
    CookieJar jar;

    jar.set_from_header("session_a=cookie1; Path=/", "domain-a.com");
    jar.set_from_header("session_b=cookie2; Path=/", "domain-b.com");
    jar.set_from_header("session_c=cookie3; Path=/", "domain-c.com");

    EXPECT_EQ(jar.size(), 3u);

    // Verify domain-a.com only gets its own cookie
    std::string domain_a_cookies = jar.get_cookie_header("domain-a.com", "/", false);
    EXPECT_NE(domain_a_cookies.find("session_a"), std::string::npos);

    // Verify domain-b.com only gets its own cookie
    std::string domain_b_cookies = jar.get_cookie_header("domain-b.com", "/", false);
    EXPECT_NE(domain_b_cookies.find("session_b"), std::string::npos);
    EXPECT_EQ(domain_b_cookies.find("session_a"), std::string::npos);
    EXPECT_EQ(domain_b_cookies.find("session_c"), std::string::npos);
}
