#include <gtest/gtest.h>
#include <clever/url/url.h>
#include <optional>
#include <string>

using namespace clever::url;

// =============================================================================
// Test 1: Basic HTTP URL parsing
// =============================================================================
TEST(URLParser, BasicHttpsURL) {
    auto result = parse("https://example.com/path?q=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->query, "q=1");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_TRUE(result->username.empty());
    EXPECT_TRUE(result->password.empty());
}

// =============================================================================
// Test 2: URL with non-default port
// =============================================================================
TEST(URLParser, URLWithPort) {
    auto result = parse("http://example.com:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test 3: URL with default port (should be omitted / set to nullopt)
// =============================================================================
TEST(URLParser, DefaultPortOmitted) {
    auto result = parse("http://example.com:80/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/");
}

TEST(URLParser, DefaultPortHttps) {
    auto result = parse("https://example.com:443/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParser, DefaultPortFtp) {
    auto result = parse("ftp://example.com:21/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
}

// =============================================================================
// Test 4: URL with userinfo
// =============================================================================
TEST(URLParser, URLWithUserinfo) {
    auto result = parse("http://user:pass@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

TEST(URLParser, URLWithUsernameOnly) {
    auto result = parse("http://user@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_TRUE(result->password.empty());
    EXPECT_EQ(result->host, "example.com");
}

// =============================================================================
// Test 5: Relative URL resolution
// =============================================================================
TEST(URLParser, RelativeURLResolution) {
    auto base = parse("https://example.com/dir/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("page.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/page.html");
}

TEST(URLParser, RelativeURLResolutionFromDirectoryBase) {
    auto base = parse("https://example.com/dir/");
    ASSERT_TRUE(base.has_value());

    auto result = parse("page.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/page.html");
}

// =============================================================================
// Test 6: File URL
// =============================================================================
TEST(URLParser, FileURL) {
    auto result = parse("file:///Users/test/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/Users/test/file.txt");
}

// =============================================================================
// Test 7: Data URL recognition
// =============================================================================
TEST(URLParser, DataURL) {
    auto result = parse("data:text/html,<h1>Hello</h1>");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_EQ(result->path, "text/html,<h1>Hello</h1>");
}

// =============================================================================
// Test 8: Blob URL recognition
// =============================================================================
TEST(URLParser, BlobURL) {
    auto result = parse("blob:https://example.com/550e8400-e29b-41d4-a716-446655440000");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_EQ(result->path, "https://example.com/550e8400-e29b-41d4-a716-446655440000");
}

// =============================================================================
// Test 9: Invalid URL returns nullopt
// =============================================================================
TEST(URLParser, InvalidURLReturnsNullopt) {
    auto result = parse("not a url with spaces");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Test 10: Empty input returns nullopt
// =============================================================================
TEST(URLParser, EmptyInputReturnsNullopt) {
    auto result = parse("");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Test 11: Percent-encoding in path
// =============================================================================
TEST(URLParser, PercentEncodingInPath) {
    auto result = parse("https://example.com/hello world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/hello%20world");
}

// =============================================================================
// Test 12: Query parameter encoding
// =============================================================================
TEST(URLParser, QueryParameterEncoding) {
    auto result = parse("https://example.com/path?key=hello world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "key=hello%20world");
}

// =============================================================================
// Test 13: Fragment handling
// =============================================================================
TEST(URLParser, FragmentHandling) {
    auto result = parse("https://example.com/path#section-1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section-1");
    EXPECT_TRUE(result->query.empty());
}

TEST(URLParser, FragmentWithSpecialChars) {
    auto result = parse("https://example.com/path#sec tion");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "sec%20tion");
}

// =============================================================================
// Test 14: Same-origin comparison
// =============================================================================
TEST(URLParser, SameOriginComparison) {
    auto a = parse("https://example.com/path1");
    auto b = parse("https://example.com/path2");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

TEST(URLParser, DifferentOriginScheme) {
    auto a = parse("http://example.com/path");
    auto b = parse("https://example.com/path");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, DifferentOriginHost) {
    auto a = parse("https://example.com/path");
    auto b = parse("https://other.com/path");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, DifferentOriginPort) {
    auto a = parse("http://example.com:8080/path");
    auto b = parse("http://example.com:9090/path");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// =============================================================================
// Test 15: Non-special scheme
// =============================================================================
TEST(URLParser, NonSpecialScheme) {
    auto result = parse("custom://host/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "custom");
    EXPECT_EQ(result->host, "host");
    EXPECT_EQ(result->path, "/path");
    EXPECT_FALSE(result->is_special());
}

// =============================================================================
// Test 16: URL with IPv6 host
// =============================================================================
TEST(URLParser, IPv6Host) {
    auto result = parse("http://[::1]:8080/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/");
}

TEST(URLParser, IPv6HostNoPort) {
    auto result = parse("http://[::1]/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[::1]");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParser, IPv6FullAddress) {
    auto result = parse("http://[2001:db8::1]/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[2001:db8::1]");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParser, IPv4MappedIPv6) {
    auto result = parse("http://[::ffff:192.0.2.1]/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[::ffff:192.0.2.1]");
}

TEST(URLParser, IPv6UnclosedBracketInvalid) {
    auto result = parse("http://[::1/path");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParser, IPv6WithPort) {
    auto result = parse("http://[2001:db8::1]:8080/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[2001:db8::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
}

// =============================================================================
// Test 17: Trailing slash normalization
// =============================================================================
TEST(URLParser, TrailingSlashNormalization) {
    auto result = parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    // Special schemes get a "/" path if none given
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test 18: Scheme-relative URL
// =============================================================================
TEST(URLParser, SchemeRelativeURL) {
    auto base = parse("https://base.com/dir/page");
    ASSERT_TRUE(base.has_value());

    auto result = parse("//example.com/path", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test 19: Path-absolute URL with base
// =============================================================================
TEST(URLParser, PathAbsoluteURLWithBase) {
    auto base = parse("https://example.com/dir/page");
    ASSERT_TRUE(base.has_value());

    auto result = parse("/absolute/path", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/absolute/path");
}

// =============================================================================
// Test 20: Dot segment resolution
// =============================================================================
TEST(URLParser, DotSegmentResolution) {
    auto result = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/c");
}

TEST(URLParser, DotSegmentResolutionSingleDot) {
    auto result = parse("https://example.com/a/./b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b");
}

TEST(URLParser, DotSegmentResolutionMultiple) {
    auto result = parse("https://example.com/a/b/c/../../d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/d");
}

TEST(URLParser, DotSegmentResolutionAtRoot) {
    auto result = parse("https://example.com/../a");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a");
}

// =============================================================================
// Additional serialize tests
// =============================================================================
TEST(URLParser, SerializeBasicURL) {
    auto result = parse("https://example.com/path?q=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "https://example.com/path?q=1#frag");
}

TEST(URLParser, SerializeWithPort) {
    auto result = parse("http://example.com:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "http://example.com:8080/path");
}

TEST(URLParser, SerializeWithUserinfo) {
    auto result = parse("http://user:pass@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "http://user:pass@example.com/");
}

TEST(URLParser, SerializeFileURL) {
    auto result = parse("file:///Users/test/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "file:///Users/test/file.txt");
}

// =============================================================================
// origin tests
// =============================================================================
TEST(URLParser, OriginBasicHTTPS) {
    auto result = parse("https://example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "https://example.com");
}

TEST(URLParser, OriginWithNonDefaultPort) {
    auto result = parse("http://example.com:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "http://example.com:8080");
}

// =============================================================================
// is_special tests
// =============================================================================
TEST(URLParser, IsSpecialHTTP) {
    auto result = parse("http://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsSpecialHTTPS) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsSpecialFTP) {
    auto result = parse("ftp://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsSpecialWS) {
    auto result = parse("ws://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsSpecialWSS) {
    auto result = parse("wss://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsSpecialFile) {
    auto result = parse("file:///tmp/test");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_special());
}

TEST(URLParser, IsNotSpecialCustom) {
    auto result = parse("custom://host/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_special());
}

// =============================================================================
// Scheme case-insensitivity
// =============================================================================
TEST(URLParser, SchemeIsCaseLowered) {
    auto result = parse("HTTP://EXAMPLE.COM/PATH");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
}

// =============================================================================
// Whitespace stripping
// =============================================================================
TEST(URLParser, LeadingTrailingWhitespaceStripped) {
    auto result = parse("  https://example.com/  ");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
}

// =============================================================================
// No base, no scheme => invalid
// =============================================================================
TEST(URLParser, NoBaseNoSchemeInvalid) {
    auto result = parse("example.com/path");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Relative with query
// =============================================================================
TEST(URLParser, RelativeWithQuery) {
    auto base = parse("https://example.com/dir/page");
    ASSERT_TRUE(base.has_value());

    auto result = parse("?newquery", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/page");
    EXPECT_EQ(result->query, "newquery");
}

// =============================================================================
// Relative with fragment only
// =============================================================================
TEST(URLParser, RelativeWithFragmentOnly) {
    auto base = parse("https://example.com/dir/page?q=1");
    ASSERT_TRUE(base.has_value());

    auto result = parse("#newfrag", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/page");
    EXPECT_EQ(result->query, "q=1");
    EXPECT_EQ(result->fragment, "newfrag");
}

// =============================================================================
// ws and wss default ports
// =============================================================================
TEST(URLParser, WSDefaultPort) {
    auto result = parse("ws://example.com:80/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParser, WSSDefaultPort) {
    auto result = parse("wss://example.com:443/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
}

// =============================================================================
// Port boundary: port 0, max valid (65535), out-of-range (65536), non-digit
// =============================================================================
TEST(URLParser, PortZeroIsValid) {
    auto result = parse("http://example.com:0/");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 0);
}

TEST(URLParser, Port65535IsValid) {
    auto result = parse("http://example.com:65535/");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 65535);
}

TEST(URLParser, Port65536IsInvalid) {
    auto result = parse("http://example.com:65536/");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParser, PortWithNonDigitIsInvalid) {
    auto result = parse("http://example.com:8080abc/");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParser, EmptyPortEquivalentToNoPort) {
    // Per WHATWG URL spec, an empty explicit port ("example.com:") is treated as no port
    auto result = parse("http://example.com:/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, std::nullopt);
}
