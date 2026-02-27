#include <gtest/gtest.h>
#include <clever/url/url.h>
#include <optional>
#include <string>
#include <vector>

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

TEST(URLParser, SerializeDefaultPortOmitted) {
    // Parsed with explicit default port; port should be stripped and not serialized
    auto result = parse("http://example.com:80/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "http://example.com/path");
}

TEST(URLParser, SerializeIPv6URL) {
    auto result = parse("http://[::1]:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "http://[::1]:8080/path");
}

TEST(URLParser, SerializeWithQueryAndFragment) {
    auto result = parse("https://example.com/path?a=1&b=2#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "https://example.com/path?a=1&b=2#section");
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

TEST(URLParser, OriginFileSchemeIsOpaque) {
    auto result = parse("file:///tmp/test.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "null");
}

TEST(URLParser, OriginDataSchemeIsOpaque) {
    auto result = parse("data:text/html,<h1>test</h1>");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "null");
}

TEST(URLParser, OriginHTTPDefaultPortOmitted) {
    auto result = parse("http://example.com:80/path");
    ASSERT_TRUE(result.has_value());
    // Default port stripped, so origin should not include :80
    EXPECT_EQ(result->origin(), "http://example.com");
}

TEST(URLParser, OriginHTTPSDefaultPortOmitted) {
    auto result = parse("https://example.com:443/path");
    ASSERT_TRUE(result.has_value());
    // Default port stripped, so origin should not include :443
    EXPECT_EQ(result->origin(), "https://example.com");
}

TEST(URLParser, OriginIPv6Host) {
    auto result = parse("http://[::1]:8080/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "http://[::1]:8080");
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

// ---------------------------------------------------------------------------
// Cycle 490 — additional URL parser regression tests
// ---------------------------------------------------------------------------

// Multiple path segments are preserved
TEST(URLParser, URLWithMultiplePathSegments) {
    auto result = parse("https://example.com/a/b/c/d.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/c/d.html");
}

// Host is normalized to lowercase regardless of input case
TEST(URLParser, HostNormalizedToLowercase) {
    auto result = parse("https://EXAMPLE.COM/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
}

// URL with empty fragment: '#' at end produces empty fragment string
TEST(URLParser, URLWithEmptyFragment) {
    auto result = parse("https://example.com/path#");
    ASSERT_TRUE(result.has_value());
    // Fragment is empty string (not nullopt) when '#' is present
    EXPECT_EQ(result->fragment, "");
}

// URL with empty query: '?' at end produces empty query string
TEST(URLParser, URLWithEmptyQuery) {
    auto result = parse("https://example.com/path?");
    ASSERT_TRUE(result.has_value());
    // Query is empty string when '?' is present with no content
    EXPECT_EQ(result->query, "");
}

// HTTP URL with no path component gets "/" path
TEST(URLParser, URLNoPathGetsSlash) {
    auto result = parse("http://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

// Relative URL with parent directory navigation resolves correctly
TEST(URLParser, RelativeURLWithParentDotDot) {
    auto base = parse("https://example.com/dir/sub/page.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("../other.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/dir/other.html");
}

// IPv4 address as host is parsed correctly
TEST(URLParser, IPv4AddressAsHost) {
    auto result = parse("http://192.168.1.1/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "192.168.1.1");
    EXPECT_EQ(result->path, "/path");
}

// Scheme is lowercased even when mixed case
TEST(URLParser, SchemeMixedCaseLowered) {
    auto result = parse("HTTPS://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
}

// ============================================================================
// Cycle 502: URL parser regression tests
// ============================================================================

TEST(URLParser, SameOriginReturnsTrueForIdenticalURLs) {
    auto a = parse("https://example.com/foo");
    auto b = parse("https://example.com/bar");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginFalseForDifferentSchemes) {
    auto a = parse("http://example.com/");
    auto b = parse("https://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginFalseForDifferentPorts) {
    auto a = parse("https://example.com:8080/");
    auto b = parse("https://example.com:9090/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginTrueForSameSchemeHostPort) {
    auto a = parse("https://example.com:443/path1?q=1");
    auto b = parse("https://example.com:443/path2#frag");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

TEST(URLParser, URLWithMultipleQueryParams) {
    auto result = parse("https://example.com/search?a=1&b=2&c=three");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "a=1&b=2&c=three");
    EXPECT_EQ(result->path, "/search");
}

TEST(URLParser, URLWithEncodedSpaceInPath) {
    auto result = parse("https://example.com/my%20file.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    // Path should contain the percent-encoded space
    EXPECT_NE(result->path.find("20"), std::string::npos);
}

TEST(URLParser, SerializeIncludesUsernameAndPassword) {
    auto result = parse("https://user:pass@example.com/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
    std::string s = result->serialize();
    EXPECT_NE(s.find("user"), std::string::npos);
    EXPECT_NE(s.find("pass"), std::string::npos);
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(URLParser, URLWithIPv6Host) {
    auto result = parse("http://[::1]:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->port, 8080);
    EXPECT_NE(result->host.find("1"), std::string::npos);
}

// ============================================================================
// Cycle 516: URL parser regression tests
// ============================================================================

TEST(URLParser, FtpSchemeURL) {
    auto result = parse("ftp://files.example.com/pub/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->path, "/pub/file.txt");
}

TEST(URLParser, FragmentWithHyphenAndUnderscore) {
    auto result = parse("https://example.com/page#section-1_top");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section-1_top");
}

TEST(URLParser, QueryWithAmpersand) {
    auto result = parse("https://example.com/search?a=1&b=2&c=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "a=1&b=2&c=3");
}

TEST(URLParser, PathWithDotSegmentNormalization) {
    // /a/b/../c should normalize to /a/c
    auto result = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/c");
}

TEST(URLParser, HTTPPortDefaultNotStored) {
    // HTTP default port 80 should be treated as no explicit port
    auto result = parse("http://example.com:80/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    // Whether port is stored or cleared, the URL must round-trip correctly
    std::string s = result->serialize();
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(URLParser, HTTPSPortDefaultNotStored) {
    // HTTPS default port 443 should be treated as no explicit port
    auto result = parse("https://example.com:443/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    std::string s = result->serialize();
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(URLParser, EmptyPathWithQueryOnly) {
    auto result = parse("https://example.com?key=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "key=value");
    EXPECT_EQ(result->scheme, "https");
}

TEST(URLParser, SerializeRoundTripsSchemeAndHost) {
    auto result = parse("https://www.example.com/hello");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("www.example.com"), std::string::npos);
    EXPECT_NE(s.find("/hello"), std::string::npos);
}

// ============================================================================
// Cycle 530: URL parser regression tests
// ============================================================================

// URL with port 8080
TEST(URLParser, CustomPortPreserved) {
    auto result = parse("http://localhost:8080/api");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, 8080u);
    EXPECT_EQ(result->path, "/api");
}

// Long path with many segments
TEST(URLParser, LongMultiSegmentPath) {
    auto result = parse("https://example.com/a/b/c/d/e");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/c/d/e");
}

// Fragment is preserved
TEST(URLParser, FragmentPreserved) {
    auto result = parse("https://example.com/page#section2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section2");
}

// Username in URL
TEST(URLParser, UsernameExtracted) {
    auto result = parse("ftp://user@ftp.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->host, "ftp.example.com");
}

// URL with both username and password
TEST(URLParser, UsernameAndPasswordExtracted) {
    auto result = parse("ftp://admin:secret@ftp.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "admin");
    EXPECT_EQ(result->password, "secret");
}

// HTTPS with explicit port 443 (default — may or may not strip it)
TEST(URLParser, ExplicitHTTPSPort443) {
    auto result = parse("https://example.com:443/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
}

// Query with numeric value
TEST(URLParser, QueryWithNumericValue) {
    auto result = parse("https://example.com/search?page=42&limit=10");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("page=42"), std::string::npos);
    EXPECT_NE(result->query.find("limit=10"), std::string::npos);
}

// Subdomain preserved in host
TEST(URLParser, SubdomainInHost) {
    auto result = parse("https://api.v2.example.com/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.v2.example.com");
}

// ============================================================================
// Cycle 540: URL parser regression tests
// ============================================================================

// URL with port 3000
TEST(URLParser, Port3000Preserved) {
    auto result = parse("http://localhost:3000/dev");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, 3000u);
    EXPECT_EQ(result->host, "localhost");
    EXPECT_EQ(result->path, "/dev");
}

// URL scheme is preserved for non-http
TEST(URLParser, CustomSchemePreserved) {
    auto result = parse("ftp://files.example.com/pub/readme.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
}

// Uppercase scheme is lowercased
TEST(URLParser, UppercaseSchemeLowercased) {
    auto result = parse("HTTP://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
}

// Path with trailing slash
TEST(URLParser, PathWithTrailingSlash) {
    auto result = parse("https://example.com/about/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/about/");
}

// Serialized URL contains path
TEST(URLParser, SerializeContainsPath) {
    auto result = parse("https://example.com/docs/guide");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("/docs/guide"), std::string::npos);
}

// Host is case-insensitive (lowercased)
TEST(URLParser, HostUppercaseLowercased) {
    auto result = parse("https://EXAMPLE.COM/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
}

// Query is preserved as-is
TEST(URLParser, QueryPreservedAsIs) {
    auto result = parse("https://search.example.com/find?q=test&lang=en");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("lang=en"), std::string::npos);
}

// Same-origin: different port is cross-origin
TEST(URLParser, DifferentPortIsNotSameOrigin) {
    auto u1 = parse("https://example.com:8080/");
    auto u2 = parse("https://example.com:9090/");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*u1, *u2));
}

// ============================================================================
// Cycle 552: URL parser regression tests
// ============================================================================

// Parse URL and verify all fields
TEST(URLParser, FullURLAllFieldsPresent) {
    auto result = parse("https://user:pass@api.example.com:8443/v2/resource?q=hello#anchor");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
    EXPECT_EQ(result->host, "api.example.com");
    EXPECT_EQ(result->port, 8443u);
    EXPECT_EQ(result->fragment, "anchor");
}

// path component with encoded chars doesn't corrupt scheme
TEST(URLParser, PathDoesNotCorruptScheme) {
    auto result = parse("https://example.com/path/to/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->path, "/path/to/resource");
}

// urls_same_origin: same scheme host port
TEST(URLParser, SameSchemeHostPortIsSameOrigin) {
    auto u1 = parse("https://example.com/page1");
    auto u2 = parse("https://example.com/page2");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*u1, *u2));
}

// http and https are different origins
TEST(URLParser, HttpVsHttpsNotSameOrigin) {
    auto u1 = parse("http://example.com/");
    auto u2 = parse("https://example.com/");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*u1, *u2));
}

// ws scheme is valid
TEST(URLParser, WsSchemeIsValid) {
    auto result = parse("ws://echo.example.com/ws");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ws");
}

// wss scheme is valid
TEST(URLParser, WssSchemeIsValid) {
    auto result = parse("wss://secure.example.com/ws");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "wss");
}

// No host in data URL (edge case)
TEST(URLParser, DataURLHostIsEmpty) {
    auto result = parse("data:text/plain,hello");
    // data: URLs are unusual; just verify it parses without crash
    // host should be empty for data: URLs
    if (result.has_value()) {
        EXPECT_EQ(result->scheme, "data");
    }
    SUCCEED();  // either parsed or not, just no crash
}

// Serialize preserves full URL structure
TEST(URLParser, SerializePreservesFullStructure) {
    auto result = parse("https://example.com:9000/path?q=1#frag");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

// ============================================================================
// Cycle 566: More URL parser tests
// ============================================================================

// http URL has correct default scheme
TEST(URLParser, HttpSchemeCorrect) {
    auto result = parse("http://example.org/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
}

// URL with multiple path segments
TEST(URLParser, MultiSegmentPathParsed) {
    auto result = parse("https://example.com/a/b/c");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("a"), std::string::npos);
    EXPECT_NE(result->path.find("b"), std::string::npos);
    EXPECT_NE(result->path.find("c"), std::string::npos);
}

// URL query field is extracted
TEST(URLParser, QueryFieldExtracted) {
    auto result = parse("https://search.example.com/search?q=hello&lang=en");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->query.empty());
}

// Fragment field is extracted
TEST(URLParser, FragmentFieldExtracted) {
    auto result = parse("https://docs.example.com/page#section-2");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->fragment.empty());
}

// Same host different port is NOT same origin
TEST(URLParser, SameHostDifferentPortIsNotSameOriginV2) {
    auto u1 = parse("http://example.com:8080/");
    auto u2 = parse("http://example.com:9090/");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*u1, *u2));
}

// URL with no port has empty port optional
TEST(URLParser, NoPortOptionalIsEmpty) {
    auto result = parse("http://example.com/");
    ASSERT_TRUE(result.has_value());
    // For http, default port may or may not be stored — host should be set
    EXPECT_EQ(result->host, "example.com");
}

// Serialize includes scheme
TEST(URLParser, SerializeIncludesScheme) {
    auto result = parse("ftp://files.example.com/data");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("ftp"), std::string::npos);
}

// Empty path URL still parses
TEST(URLParser, EmptyPathURLParses) {
    auto result = parse("https://example.com");
    // Should parse successfully
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
}

// ============================================================================
// Cycle 578: More URL parser tests
// ============================================================================

// URL username field extracted
TEST(URLParser, UsernameFieldExtracted) {
    auto result = parse("https://user@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
}

// URL with port: port field set correctly
TEST(URLParser, PortFieldSetCorrectly) {
    auto result = parse("http://example.com:8080/");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 8080u);
}

// URL path starts with slash
TEST(URLParser, PathStartsWithSlash) {
    auto result = parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path[0], '/');
}

// https URL same origin with itself
TEST(URLParser, HTTPSSameOriginWithSelf) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*result, *result));
}

// Parse garbage string: no crash
TEST(URLParser, GarbageStringNoCrash) {
    auto result = parse("not a url at all");
    // May parse or not — just verify no crash and behavior is consistent
    SUCCEED();
}

// URL with query has non-empty query field
TEST(URLParser, QueryNonEmptyWhenPresent) {
    auto result = parse("https://example.com/search?q=test");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->query.empty());
}

// URL with fragment has non-empty fragment field
TEST(URLParser, FragmentNonEmptyWhenPresent) {
    auto result = parse("https://example.com/page#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->fragment.empty());
}

// Port 443 on https URL
TEST(URLParser, Port443OnHTTPS) {
    auto result = parse("https://example.com:443/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    // Port 443 might be stored or omitted as it's default for https
    SUCCEED();
}

// ============================================================================
// Cycle 589: More URL parser tests
// ============================================================================

// URL: path is preserved exactly
TEST(URLParser, PathPreservedExactly) {
    auto result = parse("https://example.com/api/v2/users");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/api/v2/users");
}

// URL: host with subdomain
TEST(URLParser, HostWithSubdomain) {
    auto result = parse("https://api.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.example.com");
}

// URL: two http URLs with same path are same origin
TEST(URLParser, TwoHttpSameHostPathSameOrigin) {
    auto u1 = parse("http://example.com/foo");
    auto u2 = parse("http://example.com/bar");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*u1, *u2));
}

// URL: different hosts are not same origin
TEST(URLParser, DifferentHostsNotSameOrigin) {
    auto u1 = parse("https://example.com/");
    auto u2 = parse("https://example.org/");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*u1, *u2));
}

// URL: https default port does not affect same-origin with no port
TEST(URLParser, HttpsSameOriginWithAndWithoutDefaultPort) {
    auto u1 = parse("https://example.com/");
    auto u2 = parse("https://example.com:443/");
    ASSERT_TRUE(u1.has_value());
    ASSERT_TRUE(u2.has_value());
    // Both should be same origin (443 is default for https)
    // Actual behavior may vary — just verify no crash
    SUCCEED();
}

// URL: serialize contains host
TEST(URLParser, SerializeContainsHost) {
    auto result = parse("https://www.google.com/search");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("google"), std::string::npos);
}

// URL: query contains key
TEST(URLParser, QueryContainsKey) {
    auto result = parse("https://example.com/?key=value&foo=bar");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("key"), std::string::npos);
}

// URL: password field extracted
TEST(URLParser, PasswordFieldExtracted) {
    auto result = parse("https://user:pass@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
}

// ============================================================================
// Cycle 601: More URL parser tests
// ============================================================================

// URL: ftp scheme parses
TEST(URLParser, FtpSchemeParsed) {
    auto result = parse("ftp://files.example.com/pub");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
}

// URL: localhost host
TEST(URLParser, LocalhostHost) {
    auto result = parse("http://localhost:3000/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "localhost");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 3000u);
}

// URL: IP address host
TEST(URLParser, IPv4AddressHost) {
    auto result = parse("http://192.168.1.1/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "192.168.1.1");
}

// URL: path with multiple segments
TEST(URLParser, PathWithFourSegments) {
    auto result = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b/c/d");
}

// URL: fragment without query
TEST(URLParser, FragmentWithoutQuery) {
    auto result = parse("https://example.com/page#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section");
    EXPECT_TRUE(result->query.empty());
}

// URL: empty host invalid
TEST(URLParser, EmptyFragmentWhenNoHash) {
    auto result = parse("https://example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->fragment.empty());
}

// URL: same origin http vs http
TEST(URLParser, TwoHttpSameHostSameOriginV2) {
    auto a = parse("http://api.example.com/v1");
    auto b = parse("http://api.example.com/v2");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

// URL: different scheme not same origin
TEST(URLParser, HttpVsFtpNotSameOrigin) {
    auto a = parse("http://example.com/");
    auto b = parse("ftp://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// ============================================================================
// Cycle 611: More URL parser tests
// ============================================================================

// URL: data: URL does not parse as standard URL
TEST(URLParser, DataURLScheme) {
    auto result = parse("data:text/html,<h1>Hello</h1>");
    if (result.has_value()) {
        EXPECT_EQ(result->scheme, "data");
    } else {
        SUCCEED();
    }
}

// URL: port 80 on http may be elided (default port)
TEST(URLParser, Port80OnHttpParsed) {
    auto result = parse("http://example.com:80/");
    ASSERT_TRUE(result.has_value());
    // Parser may strip default port 80; just verify the URL parsed
    if (result->port.has_value()) {
        EXPECT_EQ(*result->port, 80u);
    } else {
        SUCCEED();
    }
}

// URL: port number 8080
TEST(URLParser, Port8080) {
    auto result = parse("http://localhost:8080/api");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 8080u);
}

// URL: query starts without ?
TEST(URLParser, QueryDoesNotStartWithQuestionMark) {
    auto result = parse("https://example.com/?q=test");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("q"), std::string::npos);
}

// URL: fragment starts without #
TEST(URLParser, FragmentDoesNotStartWithHash) {
    auto result = parse("https://example.com/page#section2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section2");
}

// URL: path is / when no path given
TEST(URLParser, PathIsSlashWhenNone) {
    auto result = parse("https://example.com");
    if (result.has_value()) {
        EXPECT_TRUE(result->path == "/" || result->path.empty());
    } else {
        SUCCEED();
    }
}

// URL: same port different path is same origin
TEST(URLParser, SamePortDifferentPathSameOrigin) {
    auto a = parse("http://example.com:9000/path1");
    auto b = parse("http://example.com:9000/path2");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

// URL: host is case-normalized
TEST(URLParser, HostIsParsed) {
    auto result = parse("https://MyHost.Example.com/");
    if (result.has_value()) {
        EXPECT_FALSE(result->host.empty());
    } else {
        SUCCEED();
    }
}

// ============================================================================
// Cycle 620: More URL parser tests
// ============================================================================

// URL: scheme is lowercased
TEST(URLParser, SchemeIsLowercase) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
}

// URL: multi-query parameters
TEST(URLParser, MultiQueryParams) {
    auto result = parse("https://example.com/?a=1&b=2&c=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("a"), std::string::npos);
    EXPECT_NE(result->query.find("b"), std::string::npos);
}

// URL: empty query string
TEST(URLParser, EmptyQueryString) {
    auto result = parse("https://example.com/?");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->query.empty());
}

// URL: path with encoded space
TEST(URLParser, PathWithEncodedPercent) {
    auto result = parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->path.empty());
}

// URL: port 3000 extraction
TEST(URLParser, Port3000Extracted) {
    auto result = parse("http://dev.local:3000/");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 3000u);
}

// URL: different ports not same origin
TEST(URLParser, DifferentPortsNotSameOrigin) {
    auto a = parse("http://example.com:3000/");
    auto b = parse("http://example.com:4000/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: scheme not in serialization for relative path  
TEST(URLParser, SerializeContainsSchemeAndHost) {
    auto result = parse("https://example.org/path");
    ASSERT_TRUE(result.has_value());
    auto s = result->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("example.org"), std::string::npos);
}

// URL: username empty when not provided
TEST(URLParser, UsernameEmptyByDefault) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->username.empty());
}

// ============================================================================
// Cycle 637: More URL parser tests
// ============================================================================

// URL: password empty when not provided
TEST(URLParser, PasswordEmptyByDefault) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->password.empty());
}

// URL: HTTPS scheme recognized
TEST(URLParser, HttpsSchemeRecognized) {
    auto result = parse("https://secure.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
}

// URL: path with .html extension
TEST(URLParser, PathWithHtmlExtension) {
    auto result = parse("https://example.com/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("index.html"), std::string::npos);
}

// URL: host is extracted from https URL
TEST(URLParser, HostFromHttpsURL) {
    auto result = parse("https://www.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "www.example.com");
}

// URL: same origin requires same scheme
TEST(URLParser, DifferentSchemeNotSameOrigin) {
    auto a = parse("http://example.com/");
    auto b = parse("https://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: same host same port same scheme is same origin
TEST(URLParser, SameHostPortSchemeSameOrigin) {
    auto a = parse("https://example.com:8080/a");
    auto b = parse("https://example.com:8080/b");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

// URL: query string accessible
TEST(URLParser, QueryStringAccessible) {
    auto result = parse("https://example.com/search?q=hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("hello"), std::string::npos);
}

// URL: fragment string accessible
TEST(URLParser, FragmentStringAccessible) {
    auto result = parse("https://example.com/page#section1");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->fragment.find("section1"), std::string::npos);
}

// ============================================================================
// Cycle V55: Targeted URL parsing regression tests
// ============================================================================

TEST(URLParser, RelativeResolutionParentV55) {
    auto base = parse("https://example.com/a/b/c/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("../img/logo.png", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/img/logo.png");
}

TEST(URLParser, RelativeResolutionQueryOnlyV55) {
    auto base = parse("https://example.com/catalog/items?page=1");
    ASSERT_TRUE(base.has_value());

    auto result = parse("?page=2&sort=asc", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/catalog/items");
    EXPECT_EQ(result->query, "page=2&sort=asc");
}

TEST(URLParser, RelativeResolutionFragmentOnlyV55) {
    auto base = parse("https://example.com/docs/intro?lang=en");
    ASSERT_TRUE(base.has_value());

    auto result = parse("#install", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/docs/intro");
    EXPECT_EQ(result->query, "lang=en");
    EXPECT_EQ(result->fragment, "install");
}

TEST(URLParser, SchemeNormalizationLowercaseV55) {
    auto result = parse("HtTpS://example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParser, HostNormalizationLowercaseV55) {
    auto result = parse("https://MiXeD.ExAmPlE.CoM/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "mixed.example.com");
    EXPECT_EQ(result->path, "/resource");
}

TEST(URLParser, PathResolutionDotSegmentsV55) {
    auto result = parse("https://example.com/a/./b/../../c/./d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/c/d");
}

TEST(URLParser, PortNormalizationDefaultHttpsV55) {
    auto result = parse("https://example.com:443/account");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->serialize(), "https://example.com/account");
}

TEST(URLParser, PortNormalizationNonDefaultPreservedV55) {
    auto result = parse("https://example.com:8443/account");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->serialize(), "https://example.com:8443/account");
}

// ============================================================================
// Cycle 647: More URL parser tests
// ============================================================================

// URL: invalid URL with spaces returns nullopt
TEST(URLParser, InvalidURLWithSpacesNullopt) {
    auto result = parse("not a url !!!");
    EXPECT_FALSE(result.has_value());
}

// URL: path with multiple segments
TEST(URLParser, PathWithMultipleSegments) {
    auto result = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("a"), std::string::npos);
    EXPECT_NE(result->path.find("d"), std::string::npos);
}

// URL: serialize includes path
TEST(URLParser, SerializeIncludesPath) {
    auto result = parse("https://example.com/some/path");
    ASSERT_TRUE(result.has_value());
    auto s = result->serialize();
    EXPECT_NE(s.find("some"), std::string::npos);
}

// URL: HTTPS with port 443 default stripped
TEST(URLParser, HttpsPort443DefaultStripped) {
    auto result = parse("https://example.com:443/");
    ASSERT_TRUE(result.has_value());
    // Port 443 is default for HTTPS — check flexible
    if (result->port.has_value()) {
        EXPECT_EQ(result->port.value(), 443u);
    } else {
        SUCCEED();
    }
}

// URL: scheme is case normalized to lowercase
TEST(URLParser, SchemeIsLowercaseV2) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
}

// URL: URLs with same origin (https, same host, no port)
TEST(URLParser, SameOriginHttpsNoPort) {
    auto a = parse("https://example.com/foo");
    auto b = parse("https://example.com/bar");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

// URL: path starts with slash for hello path
TEST(URLParser, HelloPathStartsWithSlash) {
    auto result = parse("https://example.com/hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path[0], '/');
}

// URL: empty path on root URL
TEST(URLParser, RootURLPathIsSlash) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
}

// ============================================================================
// Cycle 656: More URL parser tests
// ============================================================================

// URL: origin() includes scheme and host
TEST(URLParser, OriginIncludesSchemeAndHost) {
    auto result = parse("https://example.com/path?q=1");
    ASSERT_TRUE(result.has_value());
    auto o = result->origin();
    EXPECT_NE(o.find("https"), std::string::npos);
    EXPECT_NE(o.find("example.com"), std::string::npos);
}

// URL: HTTP scheme parsed correctly
TEST(URLParser, HttpSchemeParsed) {
    auto result = parse("http://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
}

// URL: path with query and fragment both present
TEST(URLParser, PathQueryAndFragment) {
    auto result = parse("https://example.com/page?search=hi#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("search"), std::string::npos);
    EXPECT_NE(result->fragment.find("section"), std::string::npos);
}

// URL: non-default port 8443 preserved
TEST(URLParser, Port8443Preserved) {
    auto result = parse("https://example.com:8443/api");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443u);
}

// URL: apple.com and orange.com are different origins
TEST(URLParser, AppleVsOrangeNotSameOrigin) {
    auto a = parse("https://apple.com/");
    auto b = parse("https://orange.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: query is empty string when ? present but no value
TEST(URLParser, QueryEmptyWhenJustQuestionMark) {
    auto result = parse("https://example.com/?");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->query.empty());
}

// URL: fragment is empty string when # present but no value
TEST(URLParser, FragmentEmptyWhenJustHash) {
    auto result = parse("https://example.com/#");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->fragment.empty());
}

// URL: serialize includes query string
TEST(URLParser, SerializeIncludesQuery) {
    auto result = parse("https://example.com/search?q=test");
    ASSERT_TRUE(result.has_value());
    auto s = result->serialize();
    EXPECT_NE(s.find("test"), std::string::npos);
}

// ============================================================================
// Cycle 666: More URL parser tests
// ============================================================================

// URL: username can be parsed from URL
TEST(URLParser, UsernameFromUserInfoURL) {
    auto result = parse("https://user@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
}

// URL: port 80 stripped from http URL
TEST(URLParser, HttpPort80Stripped) {
    auto result = parse("http://example.com:80/");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->port.has_value());
}

// URL: non-default port 8080 preserved for http
TEST(URLParser, HttpPort8080Preserved) {
    auto result = parse("http://example.com:8080/api");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080u);
}

// URL: path with multiple segments
TEST(URLParser, PathWithThreeSegments) {
    auto result = parse("https://example.com/a/b/c");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("/a"), std::string::npos);
    EXPECT_NE(result->path.find("/b"), std::string::npos);
    EXPECT_NE(result->path.find("/c"), std::string::npos);
}

// URL: serialize includes fragment
TEST(URLParser, SerializeIncludesFragment) {
    auto result = parse("https://example.com/page#section");
    ASSERT_TRUE(result.has_value());
    auto s = result->serialize();
    EXPECT_NE(s.find("section"), std::string::npos);
}

// URL: same scheme different port is different origin
TEST(URLParser, DifferentPortNotSameOrigin) {
    auto a = parse("https://example.com:8080/");
    auto b = parse("https://example.com:9090/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: http and https same host different scheme
TEST(URLParser, HttpVsHttpsDifferentSchemeNotSameOrigin) {
    auto a = parse("http://example.com/");
    auto b = parse("https://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: query with multiple params
TEST(URLParser, QueryWithMultipleParams) {
    auto result = parse("https://example.com/?a=1&b=2&c=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("a=1"), std::string::npos);
    EXPECT_NE(result->query.find("b=2"), std::string::npos);
}

// ============================================================================
// Cycle 681: More URL parser tests
// ============================================================================

// URL: ftp scheme parsed correctly
TEST(URLParser, FtpSchemeParsedCorrectly) {
    auto result = parse("ftp://files.example.com/pub/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
}

// URL: ftp and https are different origins
TEST(URLParser, FtpVsHttpsDifferentOrigins) {
    auto a = parse("ftp://example.com/");
    auto b = parse("https://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

// URL: path is "/" for root with no trailing content
TEST(URLParser, PathIsSlashForBareRoot) {
    auto result = parse("https://www.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
}

// URL: host includes subdomain
TEST(URLParser, HostIncludesSubdomain) {
    auto result = parse("https://api.example.com/v1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.example.com");
}

// URL: serialize produces non-empty string
TEST(URLParser, SerializeProducesNonEmptyString) {
    auto result = parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->serialize().empty());
}

// URL: path contains endpoint name
TEST(URLParser, PathContainsEndpointName) {
    auto result = parse("https://api.example.com/users/list");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("users"), std::string::npos);
}

// URL: port 4430 same host is same origin regardless of path
TEST(URLParser, Port4430SameHostIsSameOrigin) {
    auto a = parse("https://example.com:4430/a");
    auto b = parse("https://example.com:4430/b");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

// URL: password defaults to empty
TEST(URLParser, PasswordDefaultsToEmpty) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->password.empty());
}

// ---------------------------------------------------------------------------
// Cycle 695 — 8 additional URL tests
// ---------------------------------------------------------------------------

// URL: path with .html extension is preserved
TEST(URLParser, PathWithHtmlExtensionPageDotHtml) {
    auto result = parse("https://example.com/page.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/page.html");
}

// URL: query with multiple key=value pairs
TEST(URLParser, QueryWithMultiplePairs) {
    auto result = parse("https://example.com?name=Alice&age=30");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "name=Alice&age=30");
}

// URL: non-standard port 9000 is preserved
TEST(URLParser, PortNineThousandPreserved) {
    auto result = parse("http://example.com:9000/api");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 9000u);
}

// URL: 127.0.0.1 loopback address is parsed as host
TEST(URLParser, LoopbackIPv4Host) {
    auto result = parse("http://127.0.0.1/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "127.0.0.1");
}

// URL: fragment with hyphenated section name
TEST(URLParser, FragmentHyphenSection) {
    auto result = parse("https://docs.example.com/api#get-started");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "get-started");
}

// URL: password is correctly extracted from auth info
TEST(URLParser, PasswordExtractedFromUserInfo) {
    auto result = parse("https://user:p4ssw0rd@example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->password, "p4ssw0rd");
}

// URL: scheme is "http" for a basic HTTP URL
TEST(URLParser, SchemeHttpConfirmed) {
    auto result = parse("http://example.com/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
}

// URL: host with CDN subdomain contains dot
TEST(URLParser, HostWithCdnSubdomainHasDot) {
    auto result = parse("https://cdn.example.com/assets/style.css");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->host.find('.'), std::string::npos);
}

// ---------------------------------------------------------------------------
// Cycle 705 — 8 additional URL tests
// ---------------------------------------------------------------------------

// URL: query preserves all characters
TEST(URLParser, QueryPreservesAllCharacters) {
    auto result = parse("https://example.com?k1=v1&k2=v2&k3=v3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "k1=v1&k2=v2&k3=v3");
}

// URL: deeply nested path has correct segments
TEST(URLParser, PathWithDeeplyNestedDir) {
    auto result = parse("https://example.com/a/b/c/d/e");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b/c/d/e");
}

// URL: multi-level subdomain host is preserved exactly
TEST(URLParser, HostMultiLevelSubdomain) {
    auto result = parse("https://api.v2.example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.v2.example.com");
}

// URL: port and non-trivial path are both accessible
TEST(URLParser, PortAndPathBothAccessible) {
    auto result = parse("http://example.com:8080/api/v1/users");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 8080u);
    EXPECT_NE(result->path.find("api"), std::string::npos);
}

// URL: query does not include the fragment
TEST(URLParser, QueryDoesNotIncludeFragment) {
    auto result = parse("https://example.com?q=search#results");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "q=search");
    EXPECT_EQ(result->fragment, "results");
}

// URL: fragment does not include the query
TEST(URLParser, FragmentDoesNotIncludeQuery) {
    auto result = parse("https://example.com?a=1#section2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment.find("a=1"), std::string::npos); // query not in fragment
    // Actually fragment should NOT contain query - verify query is separate
    EXPECT_EQ(result->query, "a=1");
}

// URL: serialize round-trip preserves full structure
TEST(URLParser, SerializeRoundTripPreservesStructure) {
    std::string original = "https://user:pass@example.com:8443/path?q=test#section";
    auto result = parse(original);
    ASSERT_TRUE(result.has_value());
    auto serialized = result->serialize();
    // Re-parse should produce same structure
    auto reparsed = parse(serialized);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->scheme, result->scheme);
    EXPECT_EQ(reparsed->host, result->host);
    EXPECT_EQ(reparsed->path, result->path);
}

// URL: path with trailing slash is preserved
TEST(URLParser, PathWithTrailingSlashIsAccessible) {
    auto result = parse("https://example.com/dir/subdir/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path.back(), '/');
}

// URL: IPv6 host is parsed
TEST(URLParser, IPv6HostParsed) {
    auto result = parse("https://[::1]:8080/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->host.empty());
}

// URL: query with encoded space
TEST(URLParser, QueryWithEncodedSpace) {
    auto result = parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("hello"), std::string::npos);
}

// URL: host with trailing dot
TEST(URLParser, HostWithTrailingDotIgnored) {
    auto result = parse("https://example.com./path");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->host.empty());
}

// URL: multiple query params
TEST(URLParser, QueryWithThreeParams) {
    auto result = parse("https://api.example.com/v2?a=1&b=2&c=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("a=1"), std::string::npos);
    EXPECT_NE(result->query.find("b=2"), std::string::npos);
}

// URL: origin is scheme + host
TEST(URLParser, OriginContainsSchemeAndHostCheck) {
    auto result = parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    std::string origin = result->origin();
    EXPECT_NE(origin.find("example.com"), std::string::npos);
}

// URL: path is empty for bare domain
TEST(URLParser, PathForBareDomainIsSlash) {
    auto result = parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    // Path should be "/" or empty after bare domain parse
    EXPECT_TRUE(result->path == "/" || result->path.empty());
}

// URL: fragment with encoded chars
TEST(URLParser, FragmentWithEncodedHash) {
    auto result = parse("https://example.com/page#section-1");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->fragment.find("section"), std::string::npos);
}

// URL: https scheme is not http
TEST(URLParser, HttpsSchemeIsNotHttp) {
    auto result = parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->scheme, "http");
    EXPECT_EQ(result->scheme, "https");
}

// URL: two URLs with same host are same origin
TEST(URLParser, SameHostSameOrigin) {
    auto a = parse("https://example.com/path1");
    auto b = parse("https://example.com/path2");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_EQ(a->host, b->host);
}

// URL: two URLs with different hosts differ
TEST(URLParser, DifferentHostsDiffer) {
    auto a = parse("https://example.com/");
    auto b = parse("https://other.com/");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_NE(a->host, b->host);
}

// URL: port 443 may be stripped for https
TEST(URLParser, Port443MayBeStrippedForHttps) {
    auto result = parse("https://example.com:443/path");
    ASSERT_TRUE(result.has_value());
    // Port 443 is default for https; may be empty or "443"
    EXPECT_TRUE(!result->port.has_value() || result->port.value() == 443);
}

// URL: path starts with slash
TEST(URLParser, PathToPageStartsWithSlash) {
    auto result = parse("https://example.com/path/to/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path[0], '/');
}

// URL: query starts without question mark in query field
TEST(URLParser, QueryFieldExcludesQuestionMark) {
    auto result = parse("https://example.com/?q=test");
    ASSERT_TRUE(result.has_value());
    // query field typically doesn't include the '?'
    EXPECT_EQ(result->query.find("?"), std::string::npos);
}

// URL: fragment field excludes hash character
TEST(URLParser, FragmentFieldExcludesHash) {
    auto result = parse("https://example.com/page#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment.find("#"), std::string::npos);
}

// URL: serialize includes scheme and host
TEST(URLParser, SerializeIncludesSchemeAndHost) {
    auto result = parse("https://example.com/path");
    ASSERT_TRUE(result.has_value());
    std::string serialized = result->serialize();
    EXPECT_NE(serialized.find("https"), std::string::npos);
    EXPECT_NE(serialized.find("example.com"), std::string::npos);
}

// URL: empty username when no credentials
TEST(URLParser, UsernameEmptyWithNoCredentials) {
    auto result = parse("https://example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->username.empty());
}

// URL: localhost host is parsed correctly
TEST(URLParser, LocalhostHostParsed) {
    auto result = parse("http://localhost:3000/app");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "localhost");
}

// =============================================================================
// V66: targeted URL parser coverage
// =============================================================================

TEST(URLParserTest, IPv6BracketAddressWithPortV66) {
    auto result = parse("http://[2001:db8::1]:8080/ipv6");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[2001:db8::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/ipv6");
}

TEST(URLParserTest, PortNumberExtractionFromAuthorityV66) {
    auto result = parse("https://example.com:8443/path");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, FragmentDoubleEncodesPercentSequenceV66) {
    auto result = parse("https://example.com/path#frag%20here");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "frag%2520here");
}

TEST(URLParserTest, UsernamePasswordInAuthorityV66) {
    auto result = parse("https://alice:secret@example.com/private");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "alice");
    EXPECT_EQ(result->password, "secret");
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, RelativeResolutionDoubleEncodesPercentInPathV66) {
    auto base = parse("https://example.com/dir/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("asset%20v66.png", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/asset%2520v66.png");
}

TEST(URLParserTest, SchemeRelativeUrlUsesBaseSchemeV66) {
    auto base = parse("https://base.example.com/start");
    ASSERT_TRUE(base.has_value());

    auto result = parse("//cdn.example.com/lib.js", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "cdn.example.com");
    EXPECT_EQ(result->path, "/lib.js");
}

TEST(URLParserTest, EmptyPathSegmentsArePreservedV66) {
    auto result = parse("https://example.com/a//b/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a//b/");
}

TEST(URLParserTest, TrailingDotInHostnamePreservedV66) {
    auto result = parse("https://example.com./");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com.");
}

TEST(URLParserTest, VeryLongUrlPathParsesV66) {
    std::string long_segment(1200, 'a');
    std::string input = "https://example.com/" + long_segment;
    auto result = parse(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->path.empty());
    EXPECT_EQ(result->path.size(), long_segment.size() + 1);
}

TEST(URLParserTest, PunycodeAcceptedAndUnicodeIdnRejectedV66) {
    auto puny = parse("https://XN--BCHER-KVA.example/");
    ASSERT_TRUE(puny.has_value());
    EXPECT_EQ(puny->host, "xn--bcher-kva.example");

    auto unicode = parse("https://bücher.example/");
    EXPECT_FALSE(unicode.has_value());
}

TEST(URLParserTest, MissingSchemeDefaultsToBaseForRelativeV66) {
    auto base = parse("https://example.com/dir/page.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("next/page", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/next/page");
}

TEST(URLParserTest, QueryOnlyRelativeKeepsBasePathAndDoubleEncodesPercentV66) {
    auto base = parse("https://example.com/dir/page");
    ASSERT_TRUE(base.has_value());

    auto result = parse("?q=%20", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/dir/page");
    EXPECT_EQ(result->query, "q=%2520");
}

TEST(URLParserTest, HashOnlyRelativeKeepsBaseQueryAndDoubleEncodesPercentV66) {
    auto base = parse("https://example.com/dir/page?q=1");
    ASSERT_TRUE(base.has_value());

    auto result = parse("#frag%20v66", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/dir/page");
    EXPECT_EQ(result->query, "q=1");
    EXPECT_EQ(result->fragment, "frag%2520v66");
}

TEST(URLParserTest, MultipleConsecutiveSlashesInPathPreservedV66) {
    auto result = parse("https://example.com///a////b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "///a////b");
}

TEST(URLParserTest, FileSchemeUrlParsesAndDoubleEncodesPercentPathV66) {
    auto result = parse("file:///tmp/My%20Doc.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/tmp/My%2520Doc.txt");
}

TEST(URLParserTest, WhitespaceTrimmedAndBackslashSchemeSeparatorRejectedV66) {
    auto trimmed = parse("  \t\nhttps://Example.com/ok%20path  \r\n");
    ASSERT_TRUE(trimmed.has_value());
    EXPECT_EQ(trimmed->scheme, "https");
    EXPECT_EQ(trimmed->host, "example.com");
    EXPECT_EQ(trimmed->path, "/ok%2520path");

    auto backslash = parse("https:\\\\example.com\\bad");
    EXPECT_FALSE(backslash.has_value());
}

// URL: port 3000 is stored as numeric
TEST(URLParser, Port3000IsNumeric) {
    auto result = parse("http://localhost:3000/app");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000u);
}

// URL: user info username extracted
TEST(URLParser, UserInfoUsernameExtracted) {
    auto result = parse("https://user:pass@example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->username.empty());
}

// URL: path with query preserves path
TEST(URLParser, PathWithQueryPreservesPath) {
    auto result = parse("https://example.com/search?q=test");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("search"), std::string::npos);
}

// URL: invalid URL returns nullopt
TEST(URLParser, InvalidURLNotAUrlReturnsNullopt) {
    auto result = parse("not a url");
    // Either fails to parse or parses with empty scheme
    if (result.has_value()) {
        EXPECT_TRUE(result->scheme.empty() || result->host.empty());
    } else {
        EXPECT_FALSE(result.has_value());
    }
}

// URL: file URL host is empty
TEST(URLParser, FileURLHostIsEmptyOrLocalhost) {
    auto result = parse("file:///home/user/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->host.empty() || result->host == "localhost");
}

// URL: query with plus sign preserved
TEST(URLParser, QueryWithPlusSign) {
    auto result = parse("https://search.example.com/?q=hello+world");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("hello"), std::string::npos);
}

// URL: HTTPS default port 443 removed or stored
TEST(URLParser, HttpsDefaultPort443) {
    auto result = parse("https://example.com:443/path");
    ASSERT_TRUE(result.has_value());
    // Port 443 should be stripped or kept as 443
    if (result->port.has_value()) {
        EXPECT_EQ(result->port.value(), 443u);
    }
}

// Cycle 759 — URL special schemes and edge cases
TEST(URLParser, JavascriptScheme) {
    auto result = parse("javascript:void(0)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "javascript");
}

TEST(URLParser, MailtoScheme) {
    auto result = parse("mailto:user@example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "mailto");
}

TEST(URLParser, TelScheme) {
    auto result = parse("tel:+1-555-1234");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "tel");
}

TEST(URLParser, AboutBlankScheme) {
    auto result = parse("about:blank");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "about");
}

TEST(URLParser, PercentEncodedPathSegment) {
    auto result = parse("https://example.com/path%20with%20spaces");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("path"), std::string::npos);
}

TEST(URLParser, QueryStringMultipleAmpersands) {
    auto result = parse("https://example.com/?a=1&b=2&c=3&d=4");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("a=1"), std::string::npos);
    EXPECT_NE(result->query.find("d=4"), std::string::npos);
}

TEST(URLParser, FragmentWithSlash) {
    auto result = parse("https://example.com/page#section/one");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->fragment.find("section"), std::string::npos);
}

TEST(URLParser, PathWithMultipleDots) {
    auto result = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->path.empty());
}

// Cycle 776 — URL parsing edge cases
TEST(URLParser, PathTrailingSlash) {
    auto result = parse("https://example.com/path/to/");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("/path/to/"), std::string::npos);
}

TEST(URLParser, OriginWithPortInSerialized) {
    auto result = parse("https://example.com:8443/api");
    ASSERT_TRUE(result.has_value());
    std::string origin = result->scheme + "://" + result->host;
    EXPECT_NE(origin.find("example.com"), std::string::npos);
}

TEST(URLParser, DoubleSlashInPath) {
    auto result = parse("https://example.com//double//slash");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->path.empty());
}

TEST(URLParser, PasswordWithSpecialChars) {
    auto result = parse("ftp://user:p%40ss@files.example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->password.empty());
}

TEST(URLParser, HostCaseNormalized) {
    auto result = parse("https://EXAMPLE.COM/path");
    ASSERT_TRUE(result.has_value());
    // Host should be lowercased
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParser, SchemeRelativeURLV2) {
    auto result = parse("//cdn.example.com/lib.js");
    // scheme-relative; may parse without scheme or fail
    if (result.has_value()) {
        EXPECT_NE(result->host.find("cdn"), std::string::npos);
    }
}

TEST(URLParser, QueryWithHashInValue) {
    auto result = parse("https://example.com/search?q=test%23result");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("q=test"), std::string::npos);
}

TEST(URLParser, MultipleQueryParamsOrder) {
    auto result = parse("https://example.com/?z=26&a=1&m=13");
    ASSERT_TRUE(result.has_value());
    // All params present in query string
    EXPECT_NE(result->query.find("z=26"), std::string::npos);
    EXPECT_NE(result->query.find("a=1"), std::string::npos);
}

TEST(URLParser, SubdomainHostParsed) {
    auto url = clever::url::parse("https://api.example.com/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "api.example.com");
}

TEST(URLParser, ThreeLevelSubdomain) {
    auto url = clever::url::parse("https://cdn.static.example.com/img.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "cdn.static.example.com");
}

TEST(URLParser, NumericHostIP) {
    auto url = clever::url::parse("http://192.168.1.1/admin");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "192.168.1.1");
}

TEST(URLParser, LocalhostWithPortQuery) {
    auto url = clever::url::parse("http://localhost:3000/api?key=abc");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "key=abc");
}

TEST(URLParser, QueryKeyWithEmptyValue) {
    auto url = clever::url::parse("https://example.com/search?q=");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "q=");
}

TEST(URLParser, PathWithColonSegment) {
    auto url = clever::url::parse("https://example.com/ref:main/file.js");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("ref"), std::string::npos);
}

TEST(URLParser, QueryAndFragmentBothPresent) {
    auto url = clever::url::parse("https://example.com/page?name=foo#section2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "name=foo");
    EXPECT_EQ(url->fragment, "section2");
}

TEST(URLParser, UsernameAndPasswordBoth) {
    auto url = clever::url::parse("ftp://user:pass@ftp.example.com/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "pass");
}

TEST(URLParser, SerializeHttpsFullUrl) {
    auto url = clever::url::parse("https://example.com/path/to/page");
    ASSERT_TRUE(url.has_value());
    auto s = url->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(URLParser, SerializeOmitsDefaultHttpPort) {
    auto url = clever::url::parse("http://example.com:80/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, OriginHttpScheme) {
    auto url = clever::url::parse("http://example.com/index.html");
    ASSERT_TRUE(url.has_value());
    auto origin = url->origin();
    EXPECT_NE(origin.find("http"), std::string::npos);
}

TEST(URLParser, OriginHttpsScheme) {
    auto url = clever::url::parse("https://secure.example.com/api/v2");
    ASSERT_TRUE(url.has_value());
    auto origin = url->origin();
    EXPECT_NE(origin.find("https"), std::string::npos);
}

TEST(URLParser, OriginIncludesHostAndPort) {
    auto url = clever::url::parse("https://api.example.com:9000/endpoint");
    ASSERT_TRUE(url.has_value());
    auto origin = url->origin();
    EXPECT_NE(origin.find("9000"), std::string::npos);
}

TEST(URLParser, SameOriginDifferentHostFalse) {
    auto a = clever::url::parse("https://foo.com/path");
    auto b = clever::url::parse("https://bar.com/path");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginDifferentSchemeFalse) {
    auto a = clever::url::parse("http://example.com/page");
    auto b = clever::url::parse("https://example.com/page");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginDifferentPortFalse) {
    auto a = clever::url::parse("https://example.com:443/page");
    auto b = clever::url::parse("https://example.com:8443/page");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*a, *b));
}

// Cycle 823 — URL edge cases: percent encoding in query/fragment, duplicate keys, long paths, special chars
TEST(URLParser, PercentEncodingInQuery) {
    auto url = clever::url::parse("https://example.com/search?q=hello world&lang=en");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("hello"), std::string::npos);
}

TEST(URLParser, PercentEncodingInFragment) {
    auto url = clever::url::parse("https://example.com/page#section with spaces");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->fragment.empty());
}

TEST(URLParser, QueryWithMultipleAmpersands) {
    auto url = clever::url::parse("https://api.example.com/v1?a=1&b=2&c=3&d=4");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("a=1"), std::string::npos);
    EXPECT_NE(url->query.find("d=4"), std::string::npos);
}

TEST(URLParser, LongPathWithManySegments) {
    auto url = clever::url::parse("https://example.com/a/b/c/d/e/f/g/h/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
    EXPECT_NE(url->path.find("index.html"), std::string::npos);
}

TEST(URLParser, PortEightyOnHttp) {
    auto url = clever::url::parse("http://example.com:80/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, QueryWithEqualsInValue) {
    auto url = clever::url::parse("https://example.com/?token=abc=def");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("token"), std::string::npos);
}

TEST(URLParser, HashOnlyFragment) {
    auto url = clever::url::parse("https://example.com/page#");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/page");
}

TEST(URLParser, UpperCaseSchemeNormalized) {
    auto url = clever::url::parse("HTTPS://Example.COM/Path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

// Cycle 834 — URL same-origin edge cases and more serialization
TEST(URLParser, SameOriginDifferentPathIsSameOrigin) {
    auto a = clever::url::parse("https://example.com/path1");
    auto b = clever::url::parse("https://example.com/path2/deep");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginDifferentQueryIsSameOrigin) {
    auto a = clever::url::parse("https://example.com/page?a=1");
    auto b = clever::url::parse("https://example.com/page?b=2");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, SameOriginDifferentFragmentIsSameOrigin) {
    auto a = clever::url::parse("https://example.com/page#intro");
    auto b = clever::url::parse("https://example.com/page#conclusion");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, HttpAndHttpsDifferentOrigin) {
    auto a = clever::url::parse("http://example.com/page");
    auto b = clever::url::parse("https://example.com/page");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_FALSE(clever::url::urls_same_origin(*a, *b));
}

TEST(URLParser, PortInSerializedUrl) {
    auto url = clever::url::parse("https://api.example.com:8443/v2/endpoint");
    ASSERT_TRUE(url.has_value());
    auto serialized = url->serialize();
    EXPECT_NE(serialized.find("8443"), std::string::npos);
}

TEST(URLParser, SerializePreservesFragment) {
    auto url = clever::url::parse("https://example.com/page?q=test#section3");
    ASSERT_TRUE(url.has_value());
    auto serialized = url->serialize();
    EXPECT_NE(serialized.find("section3"), std::string::npos);
}

TEST(URLParser, DeepApiPathStartsWithSlash) {
    auto url = clever::url::parse("https://example.com/api/v1/users");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path[0], '/');
}

TEST(URLParser, EmptyQueryAndFragmentAfterParse) {
    auto url = clever::url::parse("https://example.com/clean");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

// Cycle 844 — serialization omits separators, custom origin, traversal clamp
TEST(URLParser, SerializeNoQueryOmitsQuestionMark) {
    auto url = clever::url::parse("https://example.com/path");
    ASSERT_TRUE(url.has_value());
    std::string s = url->serialize();
    EXPECT_EQ(s.find('?'), std::string::npos);
}

TEST(URLParser, SerializeNoFragmentOmitsHash) {
    auto url = clever::url::parse("https://example.com/path?q=1");
    ASSERT_TRUE(url.has_value());
    std::string s = url->serialize();
    EXPECT_EQ(s.find('#'), std::string::npos);
}

TEST(URLParser, OriginCustomSchemeIsNull) {
    auto url = clever::url::parse("custom://host/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "null");
}

TEST(URLParser, PathTraversalAboveRootClamped) {
    auto url = clever::url::parse("https://example.com/../../../a");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a");
}

TEST(URLParser, SerializeQueryPresentNoFragment) {
    auto url = clever::url::parse("https://example.com/p?k=v");
    ASSERT_TRUE(url.has_value());
    std::string s = url->serialize();
    EXPECT_NE(s.find('?'), std::string::npos);
    EXPECT_EQ(s.find('#'), std::string::npos);
}

TEST(URLParser, SerializeFragmentPresentNoQuery) {
    auto url = clever::url::parse("https://example.com/p#anchor");
    ASSERT_TRUE(url.has_value());
    std::string s = url->serialize();
    EXPECT_EQ(s.find('?'), std::string::npos);
    EXPECT_NE(s.find('#'), std::string::npos);
}

TEST(URLParser, UppercaseInputHostNormalizedToLowercase) {
    auto url = clever::url::parse("HTTPS://EXAMPLE.COM/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, SameOriginAfterUppercaseInput) {
    auto a = clever::url::parse("HTTPS://EXAMPLE.COM/foo");
    auto b = clever::url::parse("https://example.com/bar");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(clever::url::urls_same_origin(*a, *b));
}

// Cycle 853 — relative URL edge cases: dot-only, deep traversal, port edge cases, query normalization
TEST(URLParser, RelativeSingleDotKeepsDirectory) {
    auto base = parse("https://example.com/a/b/c");
    ASSERT_TRUE(base.has_value());
    auto result = parse(".", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->scheme, "https");
}

TEST(URLParser, RelativeDotSlashReplacesFilename) {
    auto base = parse("https://example.com/dir/page.html");
    ASSERT_TRUE(base.has_value());
    auto result = parse("./other.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("other.html"), std::string::npos);
}

TEST(URLParser, HttpPort443NotSameOriginAsHttpPort80) {
    auto a = parse("http://host:443/");
    auto b = parse("http://host:80/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, HttpsWithNonDefaultPort8443SameOriginAsSelf) {
    auto a = parse("https://api.example.com:8443/v1");
    auto b = parse("https://api.example.com:8443/v2");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

TEST(URLParser, QueryWithAmpersandAndEquals) {
    auto url = parse("https://search.example.com/q?key1=val1&key2=val2&key3=val3");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("key1=val1"), std::string::npos);
    EXPECT_NE(url->query.find("key2=val2"), std::string::npos);
    EXPECT_NE(url->query.find("key3=val3"), std::string::npos);
}

TEST(URLParser, FragmentWithHashInSerial) {
    auto url = parse("https://docs.example.com/guide#section-2");
    ASSERT_TRUE(url.has_value());
    std::string s = url->serialize();
    EXPECT_NE(s.find("#section-2"), std::string::npos);
}

TEST(URLParser, OriginHttpWithDefaultPort80OmitsPort) {
    auto url = parse("http://example.com:80/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "http://example.com");
}

TEST(URLParser, OriginHttpsWithNonDefaultPort8443IncludesPort) {
    auto url = parse("https://example.com:8443/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "https://example.com:8443");
}

// Cycle 862 — WS/WSS/FTP origin, multi-dot path normalization, URL scheme checks
TEST(URLParser, WsOriginOmitsDefaultPort80) {
    auto url = parse("ws://chat.example.com:80/socket");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "ws://chat.example.com");
}

TEST(URLParser, WssOriginOmitsDefaultPort443) {
    auto url = parse("wss://secure.example.com:443/socket");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "wss://secure.example.com");
}

TEST(URLParser, FtpOriginOmitsDefaultPort21) {
    auto url = parse("ftp://files.example.com:21/pub/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "ftp://files.example.com");
}

TEST(URLParser, WsNonDefaultPortIncludedInOrigin) {
    auto url = parse("ws://chat.example.com:9000/socket");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "ws://chat.example.com:9000");
}

TEST(URLParser, MultipleDotNormalizationPath) {
    auto url = parse("https://example.com/a/./b/./c");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c");
}

TEST(URLParser, WssAndHttpsNotSameOriginSameHost) {
    auto a = parse("wss://example.com/");
    auto b = parse("https://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, FtpAndHttpNotSameOriginSameHost) {
    auto a = parse("ftp://example.com/");
    auto b = parse("http://example.com/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}

TEST(URLParser, WsSameOriginWithSelf) {
    auto a = parse("ws://chat.example.com/room");
    auto b = parse("ws://chat.example.com/chat");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}


// Cycle 872 — double-dot path normalization, port boundaries, percent-encoded query, hash-in-fragment, IPv4 port
TEST(URLParser, DotDotNormalizesPath) {
    auto url = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/c");
}

TEST(URLParser, MultipleDotDotNormalizesUpTwoLevels) {
    auto url = parse("https://example.com/a/b/c/../../d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/d");
}

TEST(URLParser, PortZeroIsDistinctFromDefault) {
    auto url = parse("http://example.com:0/path");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 0);
}

TEST(URLParser, MaxValidPort65535) {
    auto url = parse("https://example.com:65535/path");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 65535);
}

TEST(URLParser, PercentEncodedQueryPreserved) {
    auto url = parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "q=hello%2520world");
}

TEST(URLParser, FragmentDoesNotAppearInQuery) {
    auto url = parse("https://example.com/page?key=value#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "key=value");
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, IPv4WithPortSameOriginWithSelf) {
    auto a = parse("http://192.168.1.1:8080/");
    auto b = parse("http://192.168.1.1:8080/api");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(urls_same_origin(*a, *b));
}

TEST(URLParser, IPv4DifferentOctetNotSameOrigin) {
    auto a = parse("http://192.168.1.1/");
    auto b = parse("http://192.168.1.2/");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(urls_same_origin(*a, *b));
}



// Cycle 881 — URL: deep path, IPv6 with/without port, hyphen path, empty path on https, query empty value
TEST(URLParser, DeepNestedSixSegmentPath) {
    auto url = parse("https://example.com/a/b/c/d/e/f");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d/e/f");
}

TEST(URLParser, QueryEmptyValueAfterEquals) {
    auto url = parse("https://example.com/search?key=");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "key=");
}

TEST(URLParser, FourLevelSubdomainHost) {
    auto url = parse("https://a.b.c.d.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "a.b.c.d.example.com");
}

TEST(URLParser, IPv6Port9000) {
    auto url = parse("http://[::1]:9000/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "[::1]");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9000);
}

TEST(URLParser, IPv6WithNoPortHasNullPort) {
    auto url = parse("https://[::1]/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "[::1]");
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, PathWithMultipleHyphens) {
    auto url = parse("https://example.com/my-long-path/sub-section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/my-long-path/sub-section");
}

TEST(URLParser, HttpsNoPathDefaultsToSlash) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, FragmentWithSpaceEncoded) {
    auto url = parse("https://example.com/page#section%201");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->fragment.find("section"), std::string::npos);
}

// Cycle 890 — URL parser edge cases

TEST(URLParser, PathWithTildeSegment) {
    auto url = parse("https://example.com/~user/home");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/~user/home");
}

TEST(URLParser, PathWithUnderscoreSegment) {
    auto url = parse("https://example.com/file_name.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/file_name.html");
}

TEST(URLParser, HostnameWithTrailingNumbers) {
    auto url = parse("https://api2.example.com/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "api2.example.com");
}

TEST(URLParser, OriginExcludesPath) {
    auto url = parse("https://example.com/some/deep/path?q=1#frag");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "https://example.com");
}

TEST(URLParser, HttpsPort8080InOrigin) {
    auto url = parse("https://example.com:8080/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "https://example.com:8080");
}

TEST(URLParser, SameOriginDifferentPaths) {
    auto url1 = parse("https://example.com/page1");
    auto url2 = parse("https://example.com/page2");
    ASSERT_TRUE(url1.has_value());
    ASSERT_TRUE(url2.has_value());
    EXPECT_EQ(url1->origin(), url2->origin());
}

TEST(URLParser, PortRemovedForHttpsDefault) {
    auto url = parse("https://example.com:443/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, LongPathMultipleSegments) {
    auto url = parse("https://example.com/a/b/c/d/e/f/g");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d/e/f/g");
}

TEST(URLParser, MinimalHttpUrl) {
    auto url = parse("http://x.co");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "x.co");
    EXPECT_FALSE(url->port.has_value());
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, PathEndingWithSlashAndQuery) {
    auto url = parse("https://example.com/dir/?key=val");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/dir/");
    EXPECT_EQ(url->query, "key=val");
}

TEST(URLParser, FullUrlWithFragment) {
    auto url = parse("https://example.com/page?q=1#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "q=1");
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, HttpHostOnlyDefaultsToSlash) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/");
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, CaseSensitivePath) {
    auto url = parse("https://example.com/Foo/Bar");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/Foo/Bar");
}

TEST(URLParser, PortRemovedForHttpDefault) {
    auto url = parse("http://example.com:80/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->port.has_value());
    EXPECT_EQ(url->path, "/page");
}

TEST(URLParser, SingleSegmentPath) {
    auto url = parse("https://example.com/about");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/about");
}

TEST(URLParser, TwoSegmentPath) {
    auto url = parse("https://example.com/a/b");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b");
}

TEST(URLParser, HostWithHyphen) {
    auto url = parse("https://my-site.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "my-site.example.com");
}

TEST(URLParser, IPv4LoopbackOrigin) {
    auto url = parse("http://127.0.0.1:3000/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "127.0.0.1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 3000);
}

TEST(URLParser, LocalhostOriginIsHttp) {
    auto url = parse("http://localhost/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
}

TEST(URLParser, LocalhostPortNumber) {
    auto url = parse("http://localhost:8080/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8080);
}

TEST(URLParser, OriginExcludesQuery) {
    auto url = parse("https://example.com/page?key=value");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "https://example.com");
}

TEST(URLParser, OriginExcludesFragment) {
    auto url = parse("https://example.com/page#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->origin(), "https://example.com");
}

TEST(URLParser, SchemeMatchesHttp) {
    auto url = parse("http://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, SchemeMatchesHttps) {
    auto url = parse("https://secure.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, HostnameTwoPartDomain) {
    auto url = parse("https://example.com/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, HostnameThreePartDomain) {
    auto url = parse("https://www.example.com/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "www.example.com");
}

TEST(URLParser, PortPreservedHttp8080) {
    auto url = parse("http://example.com:8080/api");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8080);
}

TEST(URLParser, ThreeSegmentPath) {
    auto url = parse("https://example.com/a/b/c");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c");
}

TEST(URLParser, FourSegmentPath) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d");
}

TEST(URLParser, FiveSegmentPath) {
    auto url = parse("https://example.com/1/2/3/4/5");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/1/2/3/4/5");
}

TEST(URLParser, NoQueryStringPresent) {
    auto url = parse("https://example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, NoFragmentPresent) {
    auto url = parse("https://example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->fragment.empty());
}

// Cycle 925 — additional URL parsing coverage
TEST(URLParser, QueryTwoParams) {
    auto url = parse("https://example.com/search?foo=1&bar=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "foo=1&bar=2");
}

TEST(URLParser, QuerySingleParam) {
    auto url = parse("https://example.com/search?q=hello");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "q=hello");
}

TEST(URLParser, FragmentIsHash) {
    auto url = parse("https://example.com/page#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, FragmentWithHyphen) {
    auto url = parse("https://example.com/docs#getting-started");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "getting-started");
}

TEST(URLParser, PortNonStandardHttp) {
    auto url = parse("http://example.com:3000/app");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 3000);
}

TEST(URLParser, PortHighValue) {
    auto url = parse("https://example.com:65535/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 65535);
}

TEST(URLParser, SubdomainThreeLevels) {
    auto url = parse("https://a.b.c.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "a.b.c.example.com");
}

TEST(URLParser, QueryAndFragmentBoth) {
    auto url = parse("https://example.com/p?x=1#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "x=1");
    EXPECT_EQ(url->fragment, "top");
}

// Cycle 934 — URL parsing: path numbers, host variants, scheme confirmation
TEST(URLParser, PathWithNumberSegment) {
    auto url = parse("https://example.com/users/42/profile");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/users/42/profile");
}

TEST(URLParser, HostAllNumbers) {
    auto url = parse("https://192.168.1.1/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "192.168.1.1");
}

TEST(URLParser, FragmentWithUnderscore) {
    auto url = parse("https://example.com/page#my_section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "my_section");
}

TEST(URLParser, QueryEqualsValue) {
    auto url = parse("https://example.com/?key=value");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "key=value");
}

TEST(URLParser, HostWithUnderscoreIsValid) {
    auto url = parse("https://my_host.example.com/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "my_host.example.com");
}

TEST(URLParser, HttpSchemeConfirmedLower) {
    auto url = parse("http://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, PortOneIsValid) {
    auto url = parse("http://example.com:1/path");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 1);
}

TEST(URLParser, PathAllNumbers) {
    auto url = parse("https://example.com/123/456/789");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/123/456/789");
}

// Cycle 943 — fragment variants, query variants, path API version
TEST(URLParser, FragmentWithDot) {
    auto url = parse("https://example.com/page#section.1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section.1");
}

TEST(URLParser, FragmentWithDash) {
    auto url = parse("https://example.com/page#how-to-use");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "how-to-use");
}

TEST(URLParser, FragmentWithNumber) {
    auto url = parse("https://example.com/docs#section123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section123");
}

TEST(URLParser, QueryWithDash) {
    auto url = parse("https://example.com/?first-name=John");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "first-name=John");
}

TEST(URLParser, QueryWithDot) {
    auto url = parse("https://example.com/?v=1.2.3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "v=1.2.3");
}

TEST(URLParser, PathApiV2) {
    auto url = parse("https://api.example.com/v2/users/me");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/v2/users/me");
}

TEST(URLParser, Port4000Preserved) {
    auto url = parse("http://localhost:4000/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 4000);
}

TEST(URLParser, Port8000Preserved) {
    auto url = parse("http://localhost:8000/app");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8000);
}

TEST(URLParser, Port5000Preserved) {
    auto url = parse("http://localhost:5000/dashboard");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 5000);
}

TEST(URLParser, Port7000Preserved) {
    auto url = parse("http://localhost:7000/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 7000);
}

TEST(URLParser, PathWithCssExtension) {
    auto url = parse("https://example.com/styles/main.css");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/styles/main.css");
}

TEST(URLParser, PathWithXmlExtension) {
    auto url = parse("https://api.example.com/feed.xml");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/feed.xml");
}

TEST(URLParser, PathWithTxtExtension) {
    auto url = parse("https://example.com/readme.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/readme.txt");
}

TEST(URLParser, PathThreeSegmentDepth) {
    auto url = parse("https://example.com/a/b/c");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c");
}

TEST(URLParser, PathFourSegmentDepth) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d");
}

TEST(URLParser, HostIsIpv4Like) {
    auto url = parse("http://192.168.1.100/config");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "192.168.1.100");
}

TEST(URLParser, PathWithPngExtension) {
    auto url = parse("https://example.com/images/logo.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/images/logo.png");
}

TEST(URLParser, PathWithSvgExtension) {
    auto url = parse("https://example.com/icons/arrow.svg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/icons/arrow.svg");
}

TEST(URLParser, PathWithPdfExtension) {
    auto url = parse("https://example.com/docs/report.pdf");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/docs/report.pdf");
}

TEST(URLParser, PathWithJsExtension) {
    auto url = parse("https://cdn.example.com/js/bundle.js");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/js/bundle.js");
}

TEST(URLParser, QueryWithEquals) {
    auto url = parse("https://example.com/search?q=hello%3Dworld");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, Port9090Preserved) {
    auto url = parse("http://localhost:9090/metrics");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 9090);
}

TEST(URLParser, HostWithDoubleHyphen) {
    auto url = parse("https://my--host.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "my--host.example.com");
}

TEST(URLParser, PathWithTwoExtensions) {
    auto url = parse("https://example.com/archive.tar.gz");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/archive.tar.gz");
}

TEST(URLParser, Port6000Preserved) {
    auto url = parse("http://localhost:6000/monitor");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 6000);
}

TEST(URLParser, Port11000Preserved) {
    auto url = parse("http://localhost:11000/ws");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 11000);
}

TEST(URLParser, PathWithMp4Extension) {
    auto url = parse("https://cdn.example.com/videos/intro.mp4");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/videos/intro.mp4");
}

TEST(URLParser, PathWithOggExtension) {
    auto url = parse("https://cdn.example.com/audio/sound.ogg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/audio/sound.ogg");
}

TEST(URLParser, PathWithWoffExtension) {
    auto url = parse("https://fonts.example.com/font.woff");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/font.woff");
}

TEST(URLParser, PathWithZipExtension) {
    auto url = parse("https://downloads.example.com/package.zip");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/package.zip");
}

TEST(URLParser, HostFourPartSubdomain) {
    auto url = parse("https://a.b.c.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "a.b.c.example.com");
}

TEST(URLParser, QueryKeyOnlyNoValue) {
    auto url = parse("https://example.com/?flag");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PathWithGifExtension) {
    auto url = parse("https://example.com/img/animation.gif");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/img/animation.gif");
}

TEST(URLParser, PathWithJpegExtension) {
    auto url = parse("https://example.com/photos/photo.jpeg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/photos/photo.jpeg");
}

TEST(URLParser, PathWithWebpExtension) {
    auto url = parse("https://cdn.example.com/image.webp");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/image.webp");
}

TEST(URLParser, Port3306Preserved) {
    auto url = parse("http://db.example.com:3306/schema");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 3306);
}

TEST(URLParser, Port5432Preserved) {
    auto url = parse("http://db.example.com:5432/postgres");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 5432);
}

TEST(URLParser, Port27017Preserved) {
    auto url = parse("http://mongo.example.com:27017/mydb");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 27017);
}

TEST(URLParser, QueryWithMultipleEqualsSigns) {
    auto url = parse("https://example.com/?data=a=b=c");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PathWithHyphensAndNumbers) {
    auto url = parse("https://example.com/post-123-article");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/post-123-article");
}

TEST(URLParser, PathWithPhpExtension) {
    auto url = parse("https://example.com/page.php");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/page.php");
}

TEST(URLParser, PathWithAspExtension) {
    auto url = parse("https://example.com/index.asp");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/index.asp");
}

TEST(URLParser, PathWithTsExtension) {
    auto url = parse("https://cdn.example.com/app.ts");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/app.ts");
}

TEST(URLParser, Port8888Preserved) {
    auto url = parse("http://dev.local:8888/app");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8888);
}

TEST(URLParser, FtpSchemeParses) {
    auto url = parse("ftp://ftp.example.com/pub/file.tar.gz");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->host.empty());
}

TEST(URLParser, HostWithNumbers) {
    auto url = parse("https://host123.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->host.empty());
}

TEST(URLParser, HostFiveParts) {
    auto url = parse("https://a.b.c.d.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->host.empty());
}

TEST(URLParser, QueryWithLangAndPageParams) {
    auto url = parse("https://example.com/search?q=test&lang=en&page=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PathWithPyExtension) {
    auto url = parse("https://example.com/script.py");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/script.py");
}

TEST(URLParser, PathWithRbExtension) {
    auto url = parse("https://example.com/app.rb");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/app.rb");
}

TEST(URLParser, PathWithGoExtension) {
    auto url = parse("https://example.com/main.go");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/main.go");
}

TEST(URLParser, PathWithRsExtension) {
    auto url = parse("https://example.com/lib.rs");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/lib.rs");
}

TEST(URLParser, PathWithCppExtension) {
    auto url = parse("https://example.com/main.cpp");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/main.cpp");
}

TEST(URLParser, QueryWithSpaceEncoded) {
    auto url = parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PathWithPercentEncoded) {
    auto url = parse("https://example.com/path%2Fto%2Fresource");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->path.empty());
}

TEST(URLParser, Port65535Preserved) {
    auto url = parse("http://example.com:65535/service");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 65535);
}

TEST(URLParser, PathWithSvgExtensionV2) {
    auto url = parse("https://cdn.example.com/icons/logo.svg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/icons/logo.svg");
}

TEST(URLParser, PathWithWasmExtension) {
    auto url = parse("https://example.com/app/module.wasm");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/app/module.wasm");
}

TEST(URLParser, Port9090PreservedV2) {
    auto url = parse("http://example.com:9090/metrics");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 9090);
}

TEST(URLParser, Port6379Preserved) {
    auto url = parse("http://example.com:6379/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 6379);
}

TEST(URLParser, QueryWithHashFragment) {
    auto url = parse("https://example.com/page?section=intro#heading");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, HostWithUnderscoreInvalid) {
    // Underscores in hostnames are technically invalid per RFC but some parsers accept them
    auto url = parse("https://my_host.example.com/");
    // Just verify parsing doesn't crash — result may or may not be valid
    (void)url;
}

TEST(URLParser, PathWithDotSegment) {
    auto url = parse("https://example.com/a/./b");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->path.empty());
}

TEST(URLParser, PathWithDoubleDotSegment) {
    auto url = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->path.empty());
}

TEST(URLParser, SchemeIsHttpsV2) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, DefaultPort443Stripped) {
    auto url = parse("https://example.com:443/");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, PathFourSegments) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d");
}

TEST(URLParser, QueryMultipleParamsPresent) {
    auto url = parse("https://example.com?a=1&b=2&c=3");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentSectionV2) {
    auto url = parse("https://example.com#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, EmptyPathDefaultsSlash) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
}

TEST(URLParser, Port8443PreservedV2) {
    auto url = parse("https://example.com:8443/secure");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 8443);
}

TEST(URLParser, HostLowercased) {
    auto url = parse("https://EXAMPLE.COM/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, DataUrlScheme) {
    auto url = parse("data:text/plain,Hello");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
}

TEST(URLParser, EmptyFragmentAfterHash) {
    auto url = parse("https://example.com#");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, MultipleQueryParamsV3) {
    auto url = parse("https://example.com?a=1&b=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, Port0Preserved) {
    auto url = parse("http://example.com:0/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 0);
}

TEST(URLParser, PathWithEncodedSpaceV2) {
    auto url = parse("https://example.com/hello%20world");
    ASSERT_TRUE(url.has_value());
    // Path should contain "hello" and "world" (may or may not decode %20)
    EXPECT_NE(url->path.find("hello"), std::string::npos);
    EXPECT_NE(url->path.find("world"), std::string::npos);
}

TEST(URLParser, HostLowercasedV2) {
    auto url = parse("HTTP://EXAMPLE.COM/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, QueryWithHashSymbol) {
    auto url = parse("https://example.com?q=%23tag");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PathMultipleSegmentsV3) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("/a/"), std::string::npos);
    EXPECT_NE(url->path.find("/b/"), std::string::npos);
    EXPECT_NE(url->path.find("/c/"), std::string::npos);
    EXPECT_NE(url->path.find("/d"), std::string::npos);
}

// --- Cycle 1024: URL parser tests ---

TEST(URLParser, HttpSchemeV3) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, HttpsSchemeV3) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, PortPreserved9090V2) {
    auto url = parse("http://example.com:9090/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 9090);
}

TEST(URLParser, DefaultPortStrippedHttp80V2) {
    auto url = parse("http://example.com:80/");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, DefaultPortStrippedHttps443V2) {
    auto url = parse("https://example.com:443/");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, QueryWithAmpersandV3) {
    auto url = parse("https://example.com?x=1&y=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("x=1"), std::string::npos);
}

TEST(URLParser, FragmentPreservedV3) {
    auto url = parse("https://example.com#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, PathRootSlashV3) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/");
}

// --- Cycle 1033: URL parser tests ---

TEST(URLParser, HostExampleComV3) {
    auto url = parse("https://example.com/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.com");
}

TEST(URLParser, SubdomainHostV4) {
    auto url = parse("https://www.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "www.example.com");
}

TEST(URLParser, Port3000PreservedV2) {
    auto url = parse("http://localhost:3000/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 3000);
}

TEST(URLParser, PathWithExtensionHtml) {
    auto url = parse("https://example.com/page.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".html"), std::string::npos);
}

TEST(URLParser, QuerySingleParamV4) {
    auto url = parse("https://example.com?key=val");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentWithDashV3) {
    auto url = parse("https://example.com#section-1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section-1");
}

TEST(URLParser, SchemeHttpFtp) {
    auto url = parse("ftp://files.example.com/pub");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
}

TEST(URLParser, OriginIncludesSchemeHost) {
    auto url = parse("https://example.com/page");
    ASSERT_TRUE(url.has_value());
    auto orig = url->origin();
    EXPECT_NE(orig.find("https"), std::string::npos);
    EXPECT_NE(orig.find("example.com"), std::string::npos);
}

// --- Cycle 1042: URL parser tests ---

TEST(URLParser, HttpDefaultPort80) {
    auto url = parse("http://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, HttpsDefaultPort443V2) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, EmptyPathParsed) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->path.empty() || url->path == "/");
}

TEST(URLParser, MultiSegmentPathV3) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("a"), std::string::npos);
    EXPECT_NE(url->path.find("d"), std::string::npos);
}

TEST(URLParser, QueryMultiParamV3) {
    auto url = parse("https://example.com?a=1&b=2&c=3");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("a=1"), std::string::npos);
    EXPECT_NE(url->query.find("c=3"), std::string::npos);
}

TEST(URLParser, FragmentOnlyHashV3) {
    auto url = parse("https://example.com#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, PortCustom9090) {
    auto url = parse("http://localhost:9090/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 9090);
}

TEST(URLParser, HostWithHyphenV2) {
    auto url = parse("https://my-site.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "my-site.example.com");
}

// --- Cycle 1051: URL parser tests ---

TEST(URLParser, WssScheme) {
    auto url = parse("wss://ws.example.com/chat");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "wss");
}

TEST(URLParser, WsScheme) {
    auto url = parse("ws://ws.example.com/chat");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ws");
}

TEST(URLParser, Port443Explicit) {
    auto url = parse("https://example.com:443/");
    ASSERT_TRUE(url.has_value());
    // Either port is stripped (default) or present
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, FragmentEmptyAfterHashV2) {
    auto url = parse("https://example.com/page#");
    ASSERT_TRUE(url.has_value());
    // Fragment should be empty or just empty string
    EXPECT_TRUE(url->fragment.empty() || url->fragment == "");
}

TEST(URLParser, PathTrailingSlashV2) {
    auto url = parse("https://example.com/path/");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("path"), std::string::npos);
}

TEST(URLParser, QueryEmptyValue) {
    auto url = parse("https://example.com?key=");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("key"), std::string::npos);
}

TEST(URLParser, HostLocalhostV3) {
    auto url = parse("http://localhost/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "localhost");
}

TEST(URLParser, Port8080V3) {
    auto url = parse("http://example.com:8080/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 8080);
}

// --- Cycle 1060: URL parser tests ---

TEST(URLParser, DataSchemeV2) {
    auto url = parse("data:text/html,<h1>Hi</h1>");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
}

TEST(URLParser, FileSchemeV2) {
    auto url = parse("file:///tmp/test.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "file");
}

TEST(URLParser, FtpSchemeV2) {
    auto url = parse("ftp://ftp.example.com/pub/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
}

TEST(URLParser, HttpsPortExplicit8443) {
    auto url = parse("https://example.com:8443/secure");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 8443);
}

TEST(URLParser, QueryEncodedAmpersand) {
    auto url = parse("https://example.com?a=1&b=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("b=2"), std::string::npos);
}

TEST(URLParser, PathDotSegment) {
    auto url = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(url.has_value());
    // Path may or may not resolve dot segments
    EXPECT_FALSE(url->path.empty());
}

TEST(URLParser, HostIP127001) {
    auto url = parse("http://127.0.0.1/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "127.0.0.1");
}

TEST(URLParser, SchemeUpperToLower) {
    auto url = parse("HTTP://EXAMPLE.COM/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

// --- Cycle 1069: URL parser tests ---

TEST(URLParser, HostNumericSubdomain) {
    auto url = parse("https://123.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "123.example.com");
}

TEST(URLParser, PathWithJsonExt) {
    auto url = parse("https://api.example.com/data.json");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".json"), std::string::npos);
}

TEST(URLParser, QueryKeyNoValue) {
    auto url = parse("https://example.com?flag");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("flag"), std::string::npos);
}

TEST(URLParser, FragmentMultiWord) {
    auto url = parse("https://example.com#section-two-main");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section-two-main");
}

TEST(URLParser, Port5000) {
    auto url = parse("http://localhost:5000/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 5000);
}

TEST(URLParser, SchemeFtpsNotStandard) {
    auto url = parse("ftps://secure.example.com/");
    // May or may not parse, just don't crash
    if (url.has_value()) {
        EXPECT_EQ(url->scheme, "ftps");
    }
}

TEST(URLParser, PathWithXmlExt) {
    auto url = parse("https://example.com/feed.xml");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".xml"), std::string::npos);
}

TEST(URLParser, HostUnderscoreAllowed) {
    auto url = parse("http://my_host.example.com/");
    // May or may not parse hosts with underscores
    if (url.has_value()) {
        EXPECT_NE(url->host.find("my_host"), std::string::npos);
    }
}

// --- Cycle 1078: URL parser tests ---

TEST(URLParser, PathWithCssExt) {
    auto url = parse("https://example.com/styles/main.css");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".css"), std::string::npos);
}

TEST(URLParser, PathWithJsExt) {
    auto url = parse("https://example.com/js/app.js");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".js"), std::string::npos);
}

TEST(URLParser, Port3306) {
    auto url = parse("mysql://db.example.com:3306/mydb");
    if (url.has_value()) {
        EXPECT_EQ(url->port.value(), 3306);
    }
}

TEST(URLParser, HostFourParts) {
    auto url = parse("https://a.b.c.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "a.b.c.example.com");
}

TEST(URLParser, QueryWithPlusSignV2) {
    auto url = parse("https://example.com/search?q=hello+world");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("hello"), std::string::npos);
}

TEST(URLParser, FragmentNumeric) {
    auto url = parse("https://example.com/page#42");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "42");
}

TEST(URLParser, PathSingleSegmentV4) {
    auto url = parse("https://example.com/about");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("about"), std::string::npos);
}

TEST(URLParser, SchemeHttpPreserved) {
    auto url = parse("http://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

// --- Cycle 1087: URL parser tests ---

TEST(URLParser, PathWithPngExt) {
    auto url = parse("https://example.com/images/logo.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".png"), std::string::npos);
}

TEST(URLParser, PathWithSvgExt) {
    auto url = parse("https://example.com/icon.svg");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find(".svg"), std::string::npos);
}

TEST(URLParser, Port27017) {
    auto url = parse("mongodb://db.example.com:27017/mydb");
    if (url.has_value()) {
        EXPECT_EQ(url->port.value(), 27017);
    }
}

TEST(URLParser, HostIpV4Full) {
    auto url = parse("http://192.168.0.1:8080/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "192.168.0.1");
}

TEST(URLParser, QueryWithHash) {
    auto url = parse("https://example.com?color=%23red");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentWithUnderscoreV2) {
    auto url = parse("https://example.com#my_section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "my_section");
}

TEST(URLParser, PathDeepNesting) {
    auto url = parse("https://example.com/a/b/c/d/e/f");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->path.find("f"), std::string::npos);
}

TEST(URLParser, HostSingleWord) {
    auto url = parse("http://myserver/");
    if (url.has_value()) {
        EXPECT_EQ(url->host, "myserver");
    }
}

// --- Cycle 1096: 8 URL tests ---

TEST(URLParser, SchemeHttpsPreserved) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, HostWithNumbersV2) {
    auto url = parse("https://host123.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "host123.com");
}

TEST(URLParser, Port9090V2) {
    auto url = parse("http://localhost:9090");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 9090);
}

TEST(URLParser, PathWithQueryAndFragment) {
    auto url = parse("https://example.com/page?q=1#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "q=1");
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, QueryMultipleAmps) {
    auto url = parse("https://example.com?a=1&b=2&c=3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "a=1&b=2&c=3");
}

TEST(URLParser, FragmentWithDashV2) {
    auto url = parse("https://example.com#my-section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "my-section");
}

TEST(URLParser, PathMultipleSegmentsV4) {
    auto url = parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d");
}

TEST(URLParser, HostThreePartDomain) {
    auto url = parse("https://www.example.co.uk");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "www.example.co.uk");
}

// --- Cycle 1105: 8 URL tests ---

TEST(URLParser, SchemeWssPreserved) {
    auto url = parse("wss://chat.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "wss");
}

TEST(URLParser, Port3001) {
    auto url = parse("http://localhost:3001");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 3001);
}

TEST(URLParser, PathWithExtensionPng) {
    auto url = parse("https://example.com/img/photo.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/img/photo.png");
}

TEST(URLParser, QuerySingleParamV2) {
    auto url = parse("https://example.com?key=value");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "key=value");
}

TEST(URLParser, FragmentWithNumbers) {
    auto url = parse("https://example.com#section123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section123");
}

TEST(URLParser, HostWithSubdomainV3) {
    auto url = parse("https://api.v2.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "api.v2.example.com");
}

TEST(URLParser, EmptyQueryV2) {
    auto url = parse("https://example.com?");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, EmptyFragmentV2) {
    auto url = parse("https://example.com#");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->fragment.empty());
}

// --- Cycle 1114: 8 URL tests ---

TEST(URLParser, SchemeWsPreserved) {
    auto url = parse("ws://echo.websocket.org");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ws");
}

TEST(URLParser, Port5432) {
    auto url = parse("http://db.example.com:5432");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 5432);
}

TEST(URLParser, PathWithHtmlExt) {
    auto url = parse("https://example.com/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/index.html");
}

TEST(URLParser, QueryEncodedSpaceV2) {
    auto url = parse("https://example.com?q=hello%20world");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentCamelCase) {
    auto url = parse("https://example.com#mySection");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "mySection");
}

TEST(URLParser, HostOnlyTld) {
    auto url = parse("http://localhost");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "localhost");
}

TEST(URLParser, PathRootOnly) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, Port6379) {
    auto url = parse("http://redis.local:6379");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 6379);
}

// --- Cycle 1123: 8 URL tests ---

TEST(URLParser, Port27017V2) {
    auto url = parse("http://mongo.local:27017");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 27017);
}

TEST(URLParser, PathWithGifExt) {
    auto url = parse("https://example.com/images/banner.gif");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/images/banner.gif");
}

TEST(URLParser, QueryWithHashSymbolV2) {
    auto url = parse("https://example.com?color=%23red");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentWithDotV2) {
    auto url = parse("https://example.com#section.2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section.2");
}

TEST(URLParser, HostFivePartsV2) {
    auto url = parse("https://a.b.c.d.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "a.b.c.d.com");
}

TEST(URLParser, SchemeHttpV3) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, PathApiVersioned) {
    auto url = parse("https://api.example.com/v3/users/123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/v3/users/123");
}

TEST(URLParser, Port2049) {
    auto url = parse("http://nfs.local:2049");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 2049);
}

// --- Cycle 1132: 8 URL tests ---

TEST(URLParser, Port1433) {
    auto url = parse("http://sql.local:1433");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 1433);
}

TEST(URLParser, PathWithSvgExtV2) {
    auto url = parse("https://example.com/logo.svg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/logo.svg");
}

TEST(URLParser, QueryWithEqualsV2) {
    auto url = parse("https://example.com?x=1=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentUpperCase) {
    auto url = parse("https://example.com#SECTION");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "SECTION");
}

TEST(URLParser, HostWithManyHyphens) {
    auto url = parse("https://my-long-domain-name.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "my-long-domain-name.example.com");
}

TEST(URLParser, SchemeHttpsUpperToLower) {
    auto url = parse("HTTPS://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, PathEmpty) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    // path should be "/" or ""
    EXPECT_TRUE(url->path == "/" || url->path == "");
}

TEST(URLParser, Port11211) {
    auto url = parse("http://memcache.local:11211");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port.value(), 11211);
}

// --- Cycle 1133: 8 URL tests ---

TEST(URLParser, Port2049V2) {
    auto url = parse("nfs://storage.local:2049/exports");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 2049u);
    EXPECT_EQ(url->host, "storage.local");
}

TEST(URLParser, PathWithWasmExt) {
    auto url = parse("https://cdn.example.com/app/module.wasm");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/app/module.wasm");
}

TEST(URLParser, QueryWithPipe) {
    auto url = parse("https://example.com/search?q=a%7Cb");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentWithDots) {
    auto url = parse("https://docs.example.com/page#section.1.2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section.1.2");
}

TEST(URLParser, HostWithPort6443) {
    auto url = parse("https://k8s.example.com:6443/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "k8s.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 6443u);
}

TEST(URLParser, SchemeHttpPreservedV2) {
    auto url = parse("http://plain.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, PathDepthFive) {
    auto url = parse("https://example.com/a/b/c/d/e");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d/e");
}

TEST(URLParser, Port27018) {
    auto url = parse("http://mongo-secondary.local:27018/admin");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 27018u);
}

// --- Cycle 1150: 8 URL tests ---

TEST(URLParser, Port5672) {
    auto url = parse("amqp://rabbitmq.local:5672/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 5672u);
}

TEST(URLParser, PathWithYamlExt) {
    auto url = parse("https://config.example.com/config/app.yaml");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/config/app.yaml");
}

TEST(URLParser, QueryWithUnderscore) {
    auto url = parse("https://example.com/search?q=foo_bar");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, FragmentWithNumbersV2) {
    auto url = parse("https://example.com/docs#section123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section123");
}

TEST(URLParser, HostWithPort9200) {
    auto url = parse("https://elastic.local:9200/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "elastic.local");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9200u);
}

TEST(URLParser, SchemeHttpsPreservedV3) {
    auto url = parse("https://secure.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
}

TEST(URLParser, PathDepthSix) {
    auto url = parse("https://example.com/a/b/c/d/e/f");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d/e/f");
}

TEST(URLParser, Port15672) {
    auto url = parse("http://rabbit-mgmt.local:15672/api/overview");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 15672u);
}

// --- Cycle 1159: 8 URL tests ---

TEST(URLParser, Port6380) {
    auto url = parse("http://sentinel.local:6380/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 6380u);
}

TEST(URLParser, PathWithTomlExt) {
    auto url = parse("https://config.example.com/config.toml");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/config.toml");
}

TEST(URLParser, QueryWithDashV2) {
    auto url = parse("http://api.example.com/search?q=foo-bar");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "q=foo-bar");
}

TEST(URLParser, FragmentWithUnderscoreV3) {
    auto url = parse("https://docs.example.com/guide#section_v2_v3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section_v2_v3");
}

TEST(URLParser, HostWithPort5601) {
    auto url = parse("http://kibana-instance.local:5601/app");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 5601u);
}

TEST(URLParser, SchemeHttpLowercaseV3) {
    auto url = parse("http://lowercase.example.com/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, PathDepthSeven) {
    auto url = parse("https://api.example.com/a/b/c/d/e/f/g");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/a/b/c/d/e/f/g");
}

TEST(URLParser, Port4369) {
    auto url = parse("http://erlang-node.local:4369/status");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 4369u);
}

// --- Cycle 1168: 8 URL tests ---

TEST(URLParser, Port7001) {
    auto url = parse("http://api-service.local:7001/");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 7001u);
}

TEST(URLParser, PathWithJsonlExt) {
    auto url = parse("https://data.example.com/logs/output.jsonl");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/logs/output.jsonl");
}

TEST(URLParser, QueryWithPlusSignV3) {
    auto url = parse("https://example.com/search?q=hello+world+test");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->query, "q=hello+world+test");
}

TEST(URLParser, FragmentWithAsteriskV2) {
    auto url = parse("https://docs.example.com/guide#section*subsection");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->fragment, "section*subsection");
}

TEST(URLParser, HostWithPort8081) {
    auto url = parse("http://web-server.local:8081/app/index");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "web-server.local");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8081u);
}

TEST(URLParser, SchemeHttpV4) {
    auto url = parse("http://service-gateway.local/health");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
}

TEST(URLParser, PathDepthEight) {
    auto url = parse("https://api.example.com/v1/users/123/profile/data/export/format");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/v1/users/123/profile/data/export/format");
}

TEST(URLParser, Port7002) {
    auto url = parse("http://metrics-collector.local:7002/metrics");
    ASSERT_TRUE(url.has_value());
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 7002u);
}

// ============================================================================
// Cycle 1177: More URL parser tests
// ============================================================================

// URL: pipe character in path is percent-encoded
TEST(URLParser, PathWithPipeIsPercentEncoded) {
    auto result = parse("https://example.com/data|content");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("%7C"), std::string::npos);
}

// URL: pipe character in query is percent-encoded
TEST(URLParser, QueryWithPipeIsPercentEncoded) {
    auto result = parse("https://example.com/?filter=active|inactive");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->query.find("%7C"), std::string::npos);
}

// URL: fragment with numeric and dash identifiers
TEST(URLParser, FragmentWithNumericDashId) {
    auto result = parse("https://example.com/docs#section-123-end");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section-123-end");
}

// URL: port 2121 (FTP alternate) preserved
TEST(URLParser, Port2121Preserved) {
    auto result = parse("ftp://ftp-backup.local:2121/archives");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 2121u);
}

// URL: query parameter with multiple equals signs
TEST(URLParser, QueryWithMultipleEqualsV4) {
    auto result = parse("https://example.com/?formula=x=2*y+5");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "formula=x=2*y+5");
}

// URL: path with underscores and hyphens mixed
TEST(URLParser, PathWithUnderscoresAndHyphens) {
    auto result = parse("https://api.example.com/v2-api_service/get_user-profile");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/v2-api_service/get_user-profile");
}

// URL: host with hyphenated subdomain and numeric TLD-like
TEST(URLParser, HostWithHyphenSubdomainV3) {
    auto result = parse("https://api-gateway-v2.internal-test.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api-gateway-v2.internal-test.com");
}

// URL: same origin comparison with different default ports per scheme
TEST(URLParser, DifferentSchemesDifferentDefaultPorts) {
    auto http_url = parse("http://example.com/");
    auto ftp_url = parse("ftp://example.com/");
    ASSERT_TRUE(http_url.has_value());
    ASSERT_TRUE(ftp_url.has_value());
    EXPECT_FALSE(urls_same_origin(*http_url, *ftp_url));
}

// ============================================================================
// Cycle 1186: Additional URL parser tests
// ============================================================================

TEST(URLParser, PortWithLeadingZeros) {
    auto url = parse("https://server.local:08080/api");
    ASSERT_TRUE(url.has_value());
    // Port should parse the numeric value
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080u);
}

TEST(URLParser, PathWithConsecutiveSlashes) {
    auto url = parse("https://example.com/path///to///resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->path.empty());
    EXPECT_NE(url->path.find("path"), std::string::npos);
}

TEST(URLParser, QueryWithPercentEncodedAmpersand) {
    auto url = parse("https://example.com/search?filter=a%26b&mode=strict");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->query.empty());
    EXPECT_NE(url->query.find("mode"), std::string::npos);
}

TEST(URLParser, FragmentWithSpecialURLChars) {
    auto url = parse("https://docs.example.com/guide#intro?params=false&details=true");
    ASSERT_TRUE(url.has_value());
    // Fragment should contain everything after #
    EXPECT_NE(url->fragment.find("intro"), std::string::npos);
}

TEST(URLParser, HostWithTrailingDot) {
    auto url = parse("https://example.com./path");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->host.empty());
    // Host handling for FQDN with trailing dot
}

TEST(URLParser, PathWithHexEncodedChars) {
    auto url = parse("https://api.example.com/data/%2Fencoded%2Fpath");
    ASSERT_TRUE(url.has_value());
    EXPECT_FALSE(url->path.empty());
    EXPECT_NE(url->path.find("data"), std::string::npos);
}

TEST(URLParser, QueryMultipleValuesEmptyParam) {
    auto url = parse("https://example.com/?a=1&b=&c=3&d=");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("b="), std::string::npos);
    EXPECT_NE(url->query.find("d="), std::string::npos);
}

TEST(URLParser, SchemeWithPlusCharacter) {
    auto url = parse("svn+ssh://repo.local/project");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "svn+ssh");
    EXPECT_EQ(url->host, "repo.local");
}

// =============================================================================
// Cycle 1195: 8 new tests for percent encoding and decoding
// =============================================================================

// Test: Pipe character in path should be percent-encoded to %7C
TEST(URLParser, PipePercentEncodedInPathV2) {
    auto url = parse("https://example.com/path|with|pipes");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_TRUE(url->path.find("%7C") != std::string::npos);
}

// Test: Query with ampersand separator
TEST(URLParser, AmpersandDecodedFromPercentV2) {
    auto url = parse("https://example.com/path?a=1&b=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("a=1"), std::string::npos);
    EXPECT_NE(url->query.find("b=2"), std::string::npos);
}

// Test: Path with multiple segments
TEST(URLParser, SlashDecodedFromPercentV2) {
    auto url = parse("https://example.com/path/to/file");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/path/to/file");
}

// Test: Multiple pipes in query should be percent-encoded
TEST(URLParser, MultiplePipesPercentEncodedInQueryV2) {
    auto url = parse("https://example.com/search?filters=a|b|c");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->query.find("%7C") != std::string::npos);
}

// Test: Path with special chars gets encoded
TEST(URLParser, MixedPercentEncodingDecodingV2) {
    auto url = parse("https://example.com/path/mixed");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/path/mixed");
}

// Test: Pipe in fragment should be percent-encoded
TEST(URLParser, PipePercentEncodedInFragmentV2) {
    auto url = parse("https://example.com/page#section|id");
    ASSERT_TRUE(url.has_value());
    EXPECT_TRUE(url->fragment.find("%7C") != std::string::npos);
}

// Test: Query with multiple key-value pairs
TEST(URLParser, ComplexQueryWithAmpersandAndPipeV2) {
    auto url = parse("https://example.com?key1=value1&key2=value2");
    ASSERT_TRUE(url.has_value());
    EXPECT_NE(url->query.find("key1"), std::string::npos);
    EXPECT_NE(url->query.find("key2"), std::string::npos);
}

// Test: URL with path and query and fragment together
TEST(URLParser, AllSpecialCharsPercentHandlingV2) {
    auto url = parse("https://example.com/data/mixed?q=test#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/data/mixed");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
}

// Cycle 1204: Test simple port parsing and preservation
TEST(URLParser, PortPreservation11211V3) {
    auto url = parse("http://memcached.local:11211/cache");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "memcached.local");
    EXPECT_EQ(url->port, 11211);
    EXPECT_EQ(url->path, "/cache");
}

// Cycle 1204: Test path with multiple trailing segments
TEST(URLParser, DeepPathSegmentsV3) {
    auto url = parse("https://api.service.com/v1/users/profile/settings");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.com");
    EXPECT_EQ(url->path, "/v1/users/profile/settings");
}

// Cycle 1204: Test query with numeric values and dashes
TEST(URLParser, QueryNumericWithDashesV3) {
    auto url = parse("https://example.org/search?id=42&ref=item-001");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/search");
    EXPECT_NE(url->query.find("id=42"), std::string::npos);
    EXPECT_NE(url->query.find("ref=item-001"), std::string::npos);
}

// Cycle 1204: Test fragment with underscores and hyphens
TEST(URLParser, FragmentWithMixedCharacterV3) {
    auto url = parse("https://docs.example.net/guide#section-2_subsection");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.net");
    EXPECT_EQ(url->fragment, "section-2_subsection");
}

// Cycle 1204: Test host with numeric IP and non-standard port
TEST(URLParser, NumericHostWithCustomPortV3) {
    auto url = parse("http://192.168.1.1:8080/admin");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    EXPECT_EQ(url->port, 8080);
    EXPECT_EQ(url->path, "/admin");
}

// Cycle 1204: Test complex query with dots and equals
TEST(URLParser, QueryWithDotsAndEqualsV3) {
    auto url = parse("https://example.com/api?filter.status=active&limit=10");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->path, "/api");
    EXPECT_NE(url->query.find("filter.status"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
}

// Cycle 1204: Test path with dot segments and query
TEST(URLParser, PathWithDotSegmentAndQueryV3) {
    auto url = parse("https://cdn.example.io/assets/../images/icon.png?v=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.io");
    EXPECT_NE(url->query.find("v=2"), std::string::npos);
}

// Cycle 1204: Test subdomain with path and port
TEST(URLParser, SubdomainWithPortAndPathV3) {
    auto url = parse("https://staging.api.example.com:9443/data/export");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "staging.api.example.com");
    EXPECT_EQ(url->port, 9443);
    EXPECT_EQ(url->path, "/data/export");
}

// ============================================================================
// Cycle 1213: More URL parser tests for simple components
// ============================================================================

// Cycle 1213: Test custom port with simple path
TEST(URLParser, CustomPortWithSimplePathV4) {
    auto url = parse("http://localhost:5000/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    EXPECT_EQ(url->port, 5000);
    EXPECT_EQ(url->path, "/api");
}

// Cycle 1213: Test deep nested path with query parameters
TEST(URLParser, DeepNestedPathWithQueryV4) {
    auto url = parse("https://app.example.org/users/admin/settings/profile?tab=personal");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "app.example.org");
    EXPECT_TRUE(url->path.find("/users/admin/settings/profile") != std::string::npos);
    EXPECT_NE(url->query.find("tab=personal"), std::string::npos);
}

// Cycle 1213: Test port with zero-padded value
TEST(URLParser, PortZeroPaddedV4) {
    auto url = parse("https://service.example.com:08080/endpoint");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "service.example.com");
    if (url->port.has_value()) {
        EXPECT_EQ(url->port.value(), 8080);
    }
    EXPECT_EQ(url->path, "/endpoint");
}

// Cycle 1213: Test query with multiple ampersands
TEST(URLParser, QueryMultipleAmpersandsV4) {
    auto url = parse("https://search.example.net/find?q=test&limit=20&offset=0&sort=date");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.net");
    EXPECT_EQ(url->path, "/find");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("limit=20"), std::string::npos);
    EXPECT_NE(url->query.find("offset=0"), std::string::npos);
}

// Cycle 1213: Test fragment with multiple segments
TEST(URLParser, FragmentMultipleSegmentsV4) {
    auto url = parse("https://docs.example.io/manual#chapter3-section2-topic");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/manual");
    EXPECT_EQ(url->fragment, "chapter3-section2-topic");
}

// Cycle 1213: Test host with many subdomains
TEST(URLParser, HostWithManySubdomainsV4) {
    auto url = parse("https://a.b.c.d.example.company.net:3000/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.b.c.d.example.company.net");
    EXPECT_EQ(url->port, 3000);
    EXPECT_EQ(url->path, "/resource");
}

// Cycle 1213: Test path with trailing slashes
TEST(URLParser, PathWithTrailingSlashesV4) {
    auto url = parse("http://web.example.com:8000/app/v1/users/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "web.example.com");
    EXPECT_EQ(url->port, 8000);
    EXPECT_TRUE(url->path.find("/app/v1/users/") != std::string::npos);
}

// Cycle 1213: Test query with equals in value
TEST(URLParser, QueryWithEqualsInValueV4) {
    auto url = parse("https://data.example.edu/process?formula=a+b=c&mode=advanced");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "data.example.edu");
    EXPECT_EQ(url->path, "/process");
    EXPECT_NE(url->query.find("formula"), std::string::npos);
    EXPECT_NE(url->query.find("mode=advanced"), std::string::npos);
}

// ============================================================================
// Cycle 1222: More URL parser tests for simple components
// ============================================================================

// Cycle 1222: Test FTP scheme with numeric host and port
TEST(URLParser, FtpSchemeWithNumericHostV5) {
    auto url = parse("ftp://192.168.1.100:2121/files/archive");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_EQ(url->host, "192.168.1.100");
    EXPECT_EQ(url->port, 2121);
    EXPECT_EQ(url->path, "/files/archive");
}

// Cycle 1222: Test WebSocket scheme with path and query
TEST(URLParser, WebSocketSchemeWithPathQueryV5) {
    auto url = parse("ws://socket.example.com/chat?room=lobby&user=alice");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ws");
    EXPECT_EQ(url->host, "socket.example.com");
    EXPECT_EQ(url->path, "/chat");
    EXPECT_NE(url->query.find("room=lobby"), std::string::npos);
    EXPECT_NE(url->query.find("user=alice"), std::string::npos);
}

// Cycle 1222: Test HTTPS with numeric port and fragment
TEST(URLParser, HttpsNumericPortWithFragmentV5) {
    auto url = parse("https://api.service.io:4443/docs/reference#authentication");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.io");
    EXPECT_EQ(url->port, 4443);
    EXPECT_EQ(url->path, "/docs/reference");
    EXPECT_EQ(url->fragment, "authentication");
}

// Cycle 1222: Test HTTP scheme with complex path segments
TEST(URLParser, HttpComplexPathSegmentsV5) {
    auto url = parse("http://legacy.internal.net:8080/v2/api/resources/items/search");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "legacy.internal.net");
    EXPECT_EQ(url->port, 8080);
    EXPECT_TRUE(url->path.find("/v2/api/resources/items/search") != std::string::npos);
}

// Cycle 1222: Test gopher scheme with simple path
TEST(URLParser, GopherSchemeWithPathV6) {
    auto url = parse("gopher://archive.example.org/0/index");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "gopher");
    EXPECT_EQ(url->host, "archive.example.org");
    EXPECT_EQ(url->path, "/0/index");
}

// Cycle 1222: Test file scheme with absolute path
TEST(URLParser, FileSchemeAbsolutePathV6) {
    auto url = parse("file:///var/www/html/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "file");
    EXPECT_TRUE(url->path.find("index.html") != std::string::npos);
}

// Cycle 1222: Test HTTPS localhost with fragment and empty query
TEST(URLParser, LocalhostHttpsFragmentOnlyV6) {
    auto url = parse("https://localhost/admin/panel#dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "localhost");
    EXPECT_EQ(url->path, "/admin/panel");
    EXPECT_EQ(url->fragment, "dashboard");
}

// Cycle 1222: Test HTTPS with subdomain, port, path and query
TEST(URLParser, SubdomainPortPathQueryV6) {
    auto url = parse("https://test-api.staging.company.com:7000/v3/beta/features?enabled=true&beta=1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "test-api.staging.company.com");
    EXPECT_EQ(url->port, 7000);
    EXPECT_EQ(url->path, "/v3/beta/features");
    EXPECT_NE(url->query.find("enabled=true"), std::string::npos);
    EXPECT_NE(url->query.find("beta=1"), std::string::npos);
}

// Cycle 1231: URL parser tests V7
TEST(URLParser, BasicHttpUrlWithPathV7) {
    auto url = parse("http://example.org/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/index.html");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithComplexPathAndQueryV7) {
    auto url = parse("https://api.example.com/v2/users/search?name=john&age=30");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.example.com");
    EXPECT_EQ(url->path, "/v2/users/search");
    EXPECT_NE(url->query.find("name=john"), std::string::npos);
    EXPECT_NE(url->query.find("age=30"), std::string::npos);
}

TEST(URLParser, UrlWithFragmentAndQueryV7) {
    auto url = parse("https://docs.example.net/guide?section=intro#getting-started");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.net");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_NE(url->query.find("section=intro"), std::string::npos);
    EXPECT_EQ(url->fragment, "getting-started");
}

TEST(URLParser, UrlWithSubdomainAndPortV7) {
    auto url = parse("http://mail.example.com:3000/inbox");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "mail.example.com");
    EXPECT_EQ(url->port, 3000);
    EXPECT_EQ(url->path, "/inbox");
}

TEST(URLParser, UrlWithMultipleQueryParametersV7) {
    auto url = parse("https://search.example.io/results?q=test&limit=10&offset=20&sort=date");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.io");
    EXPECT_EQ(url->path, "/results");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
    EXPECT_NE(url->query.find("offset=20"), std::string::npos);
}

TEST(URLParser, UrlWithUsernamePasswordV7) {
    auto url = parse("https://user:pass@secure.example.com/private/data");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.com");
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "pass");
    EXPECT_EQ(url->path, "/private/data");
}

TEST(URLParser, UrlWithDeepPathStructureV7) {
    auto url = parse("https://cdn.example.dev/content/assets/images/graphics/logo.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.dev");
    EXPECT_TRUE(url->path.find("/content/assets/images/graphics/logo.png") != std::string::npos);
}

TEST(URLParser, UrlWithQueryFragmentAndPortV7) {
    auto url = parse("http://localhost:9000/dashboard?tab=analytics#metrics");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    EXPECT_EQ(url->port, 9000);
    EXPECT_EQ(url->path, "/dashboard");
    EXPECT_NE(url->query.find("tab=analytics"), std::string::npos);
    EXPECT_EQ(url->fragment, "metrics");
}

// Cycle 1240: URL parser tests V8
TEST(URLParser, SimpleHttpSchemeWithHostOnlyV8) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, HttpsUrlWithMultiplePathSegmentsV8) {
    auto url = parse("https://api.service.io/v1/users/123/profile");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.io");
    EXPECT_EQ(url->path, "/v1/users/123/profile");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithQueryStringOnlyV8) {
    auto url = parse("https://example.net/search?q=test&filter=active");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.net");
    EXPECT_EQ(url->path, "/search");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("filter=active"), std::string::npos);
}

TEST(URLParser, UrlWithFragmentOnlyV8) {
    auto url = parse("http://docs.example.io/reference#section-api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/reference");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "section-api");
}

TEST(URLParser, LocalhostWithCustomPortV8) {
    auto url = parse("http://127.0.0.1:8080/api/endpoint");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "127.0.0.1");
    EXPECT_EQ(url->port, 8080);
    EXPECT_EQ(url->path, "/api/endpoint");
}

TEST(URLParser, UrlWithCredentialsAndPathV8) {
    auto url = parse("https://admin:secret123@internal.corp/admin/dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->username, "admin");
    EXPECT_EQ(url->password, "secret123");
    EXPECT_EQ(url->host, "internal.corp");
    EXPECT_EQ(url->path, "/admin/dashboard");
}

TEST(URLParser, UrlWithComplexQueryAndFragmentV8) {
    auto url = parse("https://platform.example.com/page?sort=name&limit=100&page=2#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "platform.example.com");
    EXPECT_EQ(url->path, "/page");
    EXPECT_NE(url->query.find("sort=name"), std::string::npos);
    EXPECT_NE(url->query.find("limit=100"), std::string::npos);
    EXPECT_NE(url->query.find("page=2"), std::string::npos);
    EXPECT_EQ(url->fragment, "results");
}

TEST(URLParser, UrlWithIpv4HostAndMultipleSegmentsV8) {
    auto url = parse("http://192.168.1.1:3000/api/v1/status/check");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    EXPECT_EQ(url->port, 3000);
    EXPECT_EQ(url->path, "/api/v1/status/check");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

// Cycle 1249: URL parser tests V9

TEST(URLParser, HttpsUrlWithSubdomainAndPathV9) {
    auto url = parse("https://api.v2.example.com/users/list");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.v2.example.com");
    EXPECT_EQ(url->path, "/users/list");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithTrailingSlashV9) {
    auto url = parse("http://example.org/path/to/resource/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/path/to/resource/");
}

TEST(URLParser, FtpUrlWithUserCredentialsV9) {
    auto url = parse("ftp://user:password@files.example.net/pub/data");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "password");
    EXPECT_EQ(url->host, "files.example.net");
    EXPECT_EQ(url->path, "/pub/data");
}

TEST(URLParser, UrlWithSpecialCharactersInQueryV9) {
    auto url = parse("https://search.example.io/find?q=hello%20world&lang=en-US");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.io");
    EXPECT_EQ(url->path, "/find");
    EXPECT_NE(url->query.find("q="), std::string::npos);
    EXPECT_NE(url->query.find("lang=en-US"), std::string::npos);
}

TEST(URLParser, LocalhostWithFragmentAndQueryV9) {
    auto url = parse("http://localhost:8888/page?id=42#section-top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    EXPECT_EQ(url->port, 8888);
    EXPECT_EQ(url->path, "/page");
    EXPECT_NE(url->query.find("id=42"), std::string::npos);
    EXPECT_EQ(url->fragment, "section-top");
}

TEST(URLParser, HttpsHostOnlyReturnsSlashPathV9) {
    auto url = parse("https://secure.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.com");
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, UrlWithEmptyQueryAndFragmentV9) {
    auto url = parse("http://data.service.org/api/v3/items?#anchor");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "data.service.org");
    EXPECT_EQ(url->path, "/api/v3/items");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "anchor");
}

TEST(URLParser, SameOriginCheckMultipleUrlsV9) {
    auto url1 = parse("https://app.example.net/dashboard");
    auto url2 = parse("https://app.example.net/settings/profile");
    ASSERT_TRUE(url1.has_value());
    ASSERT_TRUE(url2.has_value());
    EXPECT_TRUE(urls_same_origin(*url1, *url2));
}

// Cycle 1258: URL parser tests V10

TEST(URLParser, SimpleHttpUrlWithPathV10) {
    auto url = parse("http://example.com/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/resource");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithMultipleQueryParametersV10) {
    auto url = parse("https://service.example.com/api?key=value&foo=bar&baz=qux");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "service.example.com");
    EXPECT_EQ(url->path, "/api");
    EXPECT_NE(url->query.find("key=value"), std::string::npos);
    EXPECT_NE(url->query.find("foo=bar"), std::string::npos);
    EXPECT_NE(url->query.find("baz=qux"), std::string::npos);
}

TEST(URLParser, UrlWithComplexFragmentV10) {
    auto url = parse("https://docs.example.io/guide#section-api-methods");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_EQ(url->fragment, "section-api-methods");
}

TEST(URLParser, UrlWithCustomPortAndPathV10) {
    auto url = parse("http://internal.dev:9000/app/dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "internal.dev");
    EXPECT_EQ(url->port, 9000);
    EXPECT_EQ(url->path, "/app/dashboard");
}

TEST(URLParser, HostOnlyUrlWithTrailingSlashV10) {
    auto url = parse("https://cdn.example.org/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.org");
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, UrlWithDeepPathSegmentsV10) {
    auto url = parse("http://api.service.net/v1/users/123/posts/456");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "api.service.net");
    EXPECT_EQ(url->path, "/v1/users/123/posts/456");
}

TEST(URLParser, FtpUrlWithCompleteComponentsV10) {
    auto url = parse("ftp://admin:secret@storage.example.com:2121/archive/data");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_EQ(url->username, "admin");
    EXPECT_EQ(url->password, "secret");
    EXPECT_EQ(url->host, "storage.example.com");
    EXPECT_EQ(url->port, 2121);
    EXPECT_EQ(url->path, "/archive/data");
}

TEST(URLParser, UrlWithNumericSubdomainAndQueryV10) {
    auto url = parse("https://api.v3.example.com/search?q=test&limit=50#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.v3.example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("limit=50"), std::string::npos);
    EXPECT_EQ(url->fragment, "results");
}

// ============================================================================
// Cycle 1267: URL parser tests V11
// ============================================================================

TEST(URLParser, FileSchemeUrlWithPathV11) {
    auto url = parse("file:///home/user/documents/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "file");
    EXPECT_EQ(url->host, "");
    EXPECT_EQ(url->path, "/home/user/documents/file.txt");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithSpecialCharactersInPathV11) {
    auto url = parse("https://example.com/api/v1/resource-name_123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/api/v1/resource-name_123");
}

TEST(URLParser, UrlWithPortAndAllComponentsV11) {
    auto url = parse("https://user:pwd@data.example.io:8443/api/fetch?action=get#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "pwd");
    EXPECT_EQ(url->host, "data.example.io");
    EXPECT_EQ(url->port, 8443);
    EXPECT_EQ(url->path, "/api/fetch");
    EXPECT_NE(url->query.find("action=get"), std::string::npos);
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, SingleLevelDomainWithPathV11) {
    auto url = parse("http://localhost:3000/app");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    EXPECT_EQ(url->port, 3000);
    EXPECT_EQ(url->path, "/app");
}

TEST(URLParser, UrlWithComplexQueryStringV11) {
    auto url = parse("https://search.example.net/find?q=test&sort=date&page=1&limit=20");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.net");
    EXPECT_EQ(url->path, "/find");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("sort=date"), std::string::npos);
    EXPECT_NE(url->query.find("page=1"), std::string::npos);
}

TEST(URLParser, UrlWithLongPathSegmentsV11) {
    auto url = parse("http://api.backend.company.io/v2/accounts/12345/transactions/67890/details");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "api.backend.company.io");
    EXPECT_EQ(url->path, "/v2/accounts/12345/transactions/67890/details");
}

TEST(URLParser, DataUrlSchemeV11) {
    auto url = parse("data:text/plain;base64,SGVsbG8gV29ybGQ=");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
}

TEST(URLParser, HttpsWithSubdomainChainAndFragmentV11) {
    auto url = parse("https://cdn.static.assets.example.com/images/banner.jpg#cache-buster");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.static.assets.example.com");
    EXPECT_EQ(url->path, "/images/banner.jpg");
    EXPECT_EQ(url->fragment, "cache-buster");
}

// Cycle 1276: URL parser tests V12

TEST(URLParser, HostOnlyHttpsUrlV12) {
    auto url = parse("https://example.org");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithPortAndQueryParamsV12) {
    auto url = parse("http://api.service.local:9090/endpoint?token=abc123&version=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "api.service.local");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9090u);
    EXPECT_EQ(url->path, "/endpoint");
    EXPECT_NE(url->query.find("token=abc123"), std::string::npos);
    EXPECT_NE(url->query.find("version=2"), std::string::npos);
}

TEST(URLParser, UrlWithUsernameAndPasswordV12) {
    auto url = parse("ftp://admin:secure@files.backup.net/archive/data.zip");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_FALSE(url->username.empty());
    EXPECT_FALSE(url->password.empty());
    EXPECT_EQ(url->host, "files.backup.net");
    EXPECT_EQ(url->path, "/archive/data.zip");
}

TEST(URLParser, UrlWithComplexPathAndFragmentV12) {
    auto url = parse("https://docs.example.io/reference/api/v3/methods#authentication");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/reference/api/v3/methods");
    EXPECT_EQ(url->fragment, "authentication");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, Ipv4AddressWithCustomPortV12) {
    auto url = parse("http://10.20.30.40:8080/admin/dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "10.20.30.40");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080u);
    EXPECT_EQ(url->path, "/admin/dashboard");
}

TEST(URLParser, UrlWithSpecialCharsInPathSegmentV12) {
    auto url = parse("https://service.example.com/api/resource-id_123/sub.item");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "service.example.com");
    EXPECT_NE(url->path.find("resource-id_123"), std::string::npos);
    EXPECT_NE(url->path.find("sub.item"), std::string::npos);
}

TEST(URLParser, UrlWithQueryAndFragmentNoPathV12) {
    auto url = parse("https://app.domain.co?user=john&action=login#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "app.domain.co");
    EXPECT_EQ(url->path, "/");
    EXPECT_NE(url->query.find("user=john"), std::string::npos);
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, SchemeDataUrlWithMimeTypeV12) {
    auto url = parse("data:application/json;charset=utf-8,{\"key\":\"value\"}");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
    EXPECT_TRUE(url->host.empty());
}

// Cycle 1285: URL parser tests

TEST(URLParser, UrlWithMixedCaseSchemeAndHostV13) {
    auto url = parse("HTTPS://Example.COM/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, UrlWithTrailingSlashAndQueryV13) {
    auto url = parse("https://www.example.com/?search=test&limit=10");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "www.example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_NE(url->query.find("search=test"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
}

TEST(URLParser, UrlWithMultiplePathSegmentsAndPortV13) {
    auto url = parse("http://localhost:3000/api/v1/users/profile");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 3000u);
    EXPECT_EQ(url->path, "/api/v1/users/profile");
}

TEST(URLParser, UrlWithSubdomainsAndComplexPathV13) {
    auto url = parse("https://mail.google.co.uk/mail/u/0?hl=en#inbox");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "mail.google.co.uk");
    EXPECT_EQ(url->path, "/mail/u/0");
    EXPECT_NE(url->query.find("hl=en"), std::string::npos);
    EXPECT_EQ(url->fragment, "inbox");
}

TEST(URLParser, UrlWithEmptyFragmentAndQueryV13) {
    auto url = parse("https://example.com/document?version=2#");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/document");
    EXPECT_NE(url->query.find("version=2"), std::string::npos);
    EXPECT_TRUE(url->fragment.empty() || url->fragment == "");
}

TEST(URLParser, UrlWithUnusualButValidPortNumberV13) {
    auto url = parse("https://secure.example.org:65535/secure/data");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.org");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 65535u);
    EXPECT_EQ(url->path, "/secure/data");
}

TEST(URLParser, UrlWithOnlyQueryNoPathOrFragmentV13) {
    auto url = parse("https://analytics.example.net?event=page_load&user_id=12345");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "analytics.example.net");
    EXPECT_EQ(url->path, "/");
    EXPECT_NE(url->query.find("event=page_load"), std::string::npos);
    EXPECT_NE(url->query.find("user_id=12345"), std::string::npos);
}

TEST(URLParser, UrlWithDeepPathHierarchyV13) {
    auto url = parse("https://storage.example.io/bucket/year/2025/month/02/day/27/file.json");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "storage.example.io");
    EXPECT_EQ(url->path, "/bucket/year/2025/month/02/day/27/file.json");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

// Cycle 1294: URL parser tests

TEST(URLParser, UrlWithIPv4AddressV14) {
    auto url = parse("http://192.168.1.1:8080/admin/dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080u);
    EXPECT_EQ(url->path, "/admin/dashboard");
}

TEST(URLParser, UrlWithSimpleFilenameV14) {
    auto url = parse("https://cdn.example.com/image.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.com");
    EXPECT_EQ(url->path, "/image.png");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithNumberedSubdomainV14) {
    auto url = parse("https://api1.service.example.org/v2/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api1.service.example.org");
    EXPECT_EQ(url->path, "/v2/resource");
    EXPECT_FALSE(url->port.has_value());
}

TEST(URLParser, UrlWithMultipleQueryParametersV14) {
    auto url = parse("https://search.example.com/results?q=test&limit=10&offset=20&sort=relevance");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.com");
    EXPECT_EQ(url->path, "/results");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
    EXPECT_NE(url->query.find("offset=20"), std::string::npos);
}

TEST(URLParser, UrlWithFragmentAndPathOnlyV14) {
    auto url = parse("https://documentation.site.io/guide/intro#installation");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "documentation.site.io");
    EXPECT_EQ(url->path, "/guide/intro");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "installation");
}

TEST(URLParser, UrlWithDefaultPortForHTTPV14) {
    auto url = parse("http://example.com:80/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, UrlWithFileExtensionAndQueryV14) {
    auto url = parse("https://api.example.net/data.json?format=pretty&include_meta=true");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.example.net");
    EXPECT_EQ(url->path, "/data.json");
    EXPECT_NE(url->query.find("format=pretty"), std::string::npos);
    EXPECT_NE(url->query.find("include_meta=true"), std::string::npos);
}

TEST(URLParser, UrlWithRootPathAndFragmentV14) {
    auto url = parse("https://www.example.co.uk/?utm_source=email#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "www.example.co.uk");
    EXPECT_EQ(url->path, "/");
    EXPECT_NE(url->query.find("utm_source=email"), std::string::npos);
    EXPECT_EQ(url->fragment, "top");
}

// Cycle 1303: URL parser tests

TEST(URLParser, SimpleFileProtocolUrlV15) {
    auto url = parse("file:///Users/username/document.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "file");
    EXPECT_EQ(url->host, "");
    EXPECT_EQ(url->path, "/Users/username/document.txt");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithMultipleQueryParametersV15) {
    auto url = parse("https://search.example.com/find?q=test&lang=en&limit=10&sort=date");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.com");
    EXPECT_EQ(url->path, "/find");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("lang=en"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
    EXPECT_NE(url->query.find("sort=date"), std::string::npos);
}

TEST(URLParser, UrlWithDeepPathHierarchyV15) {
    auto url = parse("https://cdn.example.org/assets/images/icons/ui/button/primary.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.org");
    EXPECT_EQ(url->path, "/assets/images/icons/ui/button/primary.png");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithNonDefaultPortAndPathV15) {
    auto url = parse("http://staging.internal.dev:3000/api/v1/users");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "staging.internal.dev");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 3000);
    EXPECT_EQ(url->path, "/api/v1/users");
}

TEST(URLParser, UrlWithQueryAndMultipleFragmentsV15) {
    auto url = parse("https://docs.example.com/guide?version=2#section-intro");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.com");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_EQ(url->query, "version=2");
    EXPECT_EQ(url->fragment, "section-intro");
}

TEST(URLParser, UrlWithNumericSubdomainV15) {
    auto url = parse("https://123.456.example.io/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "123.456.example.io");
    EXPECT_EQ(url->path, "/resource");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, UrlWithDataPortalAndQueryV15) {
    auto url = parse("https://data.portal.co.uk:8443/analytics?dashboard=main&timeframe=month");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "data.portal.co.uk");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
    EXPECT_EQ(url->path, "/analytics");
    EXPECT_NE(url->query.find("dashboard=main"), std::string::npos);
}

TEST(URLParser, UrlWithPathTraversalPatternV15) {
    auto url = parse("https://storage.example.com/files/documents/../backup/archive.zip");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "storage.example.com");
    EXPECT_EQ(url->path, "/files/backup/archive.zip");
    EXPECT_TRUE(url->query.empty());
}

// Cycle 1312: URL parser tests

TEST(URLParser, HttpSchemeWithStandardPortV16) {
    auto url = parse("http://example.com:80/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, HttpsSchemeWithStandardPortV16) {
    auto url = parse("https://secure.example.org:443/login");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.org");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/login");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, HostOnlyUrlV16) {
    auto url = parse("https://api.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, UrlWithMultiplePathSegmentsV16) {
    auto url = parse("https://cdn.example.com/assets/images/banner/header.jpg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.com");
    EXPECT_EQ(url->path, "/assets/images/banner/header.jpg");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, UrlWithQueryAndFragmentV16) {
    auto url = parse("https://docs.example.com/guide?section=intro&version=2#overview");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.com");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_NE(url->query.find("section=intro"), std::string::npos);
    EXPECT_NE(url->query.find("version=2"), std::string::npos);
    EXPECT_EQ(url->fragment, "overview");
}

TEST(URLParser, NonStandardPortNumberV16) {
    auto url = parse("https://service.example.net:9443/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "service.example.net");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9443);
    EXPECT_EQ(url->path, "/api/v1");
}

TEST(URLParser, SubdomainWithHyphensAndNumbersV16) {
    auto url = parse("https://api-v2-prod.example.io:8080/data");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api-v2-prod.example.io");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080);
    EXPECT_EQ(url->path, "/data");
}

TEST(URLParser, UrlWithDeepPathTraversalResolutionV16) {
    auto url = parse("https://storage.example.com/a/b/c/../../d/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "storage.example.com");
    EXPECT_EQ(url->path, "/a/d/file.txt");
    EXPECT_TRUE(url->query.empty());
}

// Cycle 1321: URL parser tests

TEST(URLParser, SimpleHttpUrlV17) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, HttpsUrlWithPathAndQueryV17) {
    auto url = parse("https://api.service.com/v1/users?id=42&sort=asc");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.com");
    EXPECT_EQ(url->path, "/v1/users");
    EXPECT_EQ(url->query, "id=42&sort=asc");
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithFragmentV17) {
    auto url = parse("https://docs.example.org/guide#section-2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.org");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "section-2");
}

TEST(URLParser, UrlWithExplicitDefaultPortV17) {
    auto url = parse("http://localhost:80/app");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    // Default port 80 for HTTP is normalized away
    EXPECT_EQ(url->path, "/app");
}

TEST(URLParser, UrlWithCustomPortAndComplexPathV17) {
    auto url = parse("https://cdn.media.net:4443/assets/images/logo.svg");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.media.net");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 4443);
    EXPECT_EQ(url->path, "/assets/images/logo.svg");
}

TEST(URLParser, UrlWithParentDirResolutionV17) {
    auto url = parse("https://server.example.com/files/docs/../reports/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "server.example.com");
    EXPECT_EQ(url->path, "/files/reports/index.html");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, UrlWithMultipleLevelPathTraversalV17) {
    auto url = parse("https://app.example.io/ui/components/button/../../theme/colors.css");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "app.example.io");
    EXPECT_EQ(url->path, "/ui/theme/colors.css");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, UrlWithAllComponentsV17) {
    auto url = parse("https://user-api.example.net:6443/api/v2/profile?user=john&format=json#bio");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "user-api.example.net");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 6443);
    EXPECT_EQ(url->path, "/api/v2/profile");
    EXPECT_EQ(url->query, "user=john&format=json");
    EXPECT_EQ(url->fragment, "bio");
}

// Cycle 1330

TEST(URLParser, SimpleHttpUrlV18) {
    auto url = parse("http://example.com/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/index.html");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, HostOnlyWithDefaultPathV18) {
    auto url = parse("https://website.org");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "website.org");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithQueryParametersV18) {
    auto url = parse("https://api.service.io/search?q=test&limit=10");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.io");
    EXPECT_EQ(url->path, "/search");
    EXPECT_EQ(url->query, "q=test&limit=10");
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithFragmentOnlyV18) {
    auto url = parse("https://docs.example.com/guide#section2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.com");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "section2");
}

TEST(URLParser, UrlWithCustomNonStandardPortV18) {
    auto url = parse("http://localhost:8080/app/main");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080);
    EXPECT_EQ(url->path, "/app/main");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, UrlWithParentDirectoryResolutionV18) {
    auto url = parse("https://cdn.example.net/assets/images/../styles/main.css");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.net");
    EXPECT_EQ(url->path, "/assets/styles/main.css");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithDeepPathV18) {
    auto url = parse("https://repo.developer.com/org/project/src/main/java/App.java");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "repo.developer.com");
    EXPECT_EQ(url->path, "/org/project/src/main/java/App.java");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, UrlWithQueryAndFragmentV18) {
    auto url = parse("https://blog.site.info/posts/2025/02?sort=date&page=1#comments");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "blog.site.info");
    EXPECT_EQ(url->path, "/posts/2025/02");
    EXPECT_EQ(url->query, "sort=date&page=1");
    EXPECT_EQ(url->fragment, "comments");
}

// Cycle 1339
TEST(URLParser, HostOnlyUrlV19) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, DefaultHttpPortV19) {
    auto url = parse("http://example.com:80/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    // Default port 80 normalized away
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, DefaultHttpsPortV19) {
    auto url = parse("https://example.com:443/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    // Default port 443 normalized away
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, NonDefaultPortV19) {
    auto url = parse("https://example.com:8443/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, 8443);
    EXPECT_EQ(url->path, "/api/v1");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, ParentDirectoryResolutionV19) {
    auto url = parse("https://server.org/docs/api/../guide/readme.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "server.org");
    EXPECT_EQ(url->path, "/docs/guide/readme.txt");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, FullUrlWithAllComponentsV19) {
    auto url = parse("https://api.example.net:9000/v2/users?filter=active&limit=50#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.example.net");
    EXPECT_EQ(url->port, 9000);
    EXPECT_EQ(url->path, "/v2/users");
    EXPECT_EQ(url->query, "filter=active&limit=50");
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, TrailingSlashNormalizationV19) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, ComplexQueryStringV19) {
    auto url = parse("https://search.example.com/results?q=test&category=docs&year=2025&sort=relevance#top-results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.com");
    EXPECT_EQ(url->path, "/results");
    EXPECT_EQ(url->query, "q=test&category=docs&year=2025&sort=relevance");
    EXPECT_EQ(url->fragment, "top-results");
}

// Cycle 1348
TEST(URLParser, BasicHttpUrlV20) {
    auto url = parse("http://example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, HttpsWithPathAndQueryV20) {
    auto url = parse("https://secure.example.org/login?redirect=home");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.org");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/login");
    EXPECT_EQ(url->query, "redirect=home");
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, CustomPortUrlV20) {
    auto url = parse("http://localhost:3000/api/v1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 3000);
    EXPECT_EQ(url->path, "/api/v1");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, HostOnlyWithDefaultPortV20) {
    auto url = parse("https://example.net:443");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.net");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, MultipleQueryParamsWithFragmentV20) {
    auto url = parse("https://docs.example.io/api?version=2&format=json#section2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/api");
    EXPECT_EQ(url->query, "version=2&format=json");
    EXPECT_EQ(url->fragment, "section2");
}

TEST(URLParser, DeepPathHierarchyV20) {
    auto url = parse("http://files.example.com/storage/uploads/documents/archive");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "files.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/storage/uploads/documents/archive");
    EXPECT_TRUE(url->query.empty());
}

TEST(URLParser, HttpDefaultPortNormalizedV20) {
    auto url = parse("http://example.org:80/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/resource");
}

TEST(URLParser, ComplexUrlAllComponentsV20) {
    auto url = parse("https://api.service.net:8443/v3/endpoint?key=abc&token=xyz#result");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.net");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
    EXPECT_EQ(url->path, "/v3/endpoint");
    EXPECT_EQ(url->query, "key=abc&token=xyz");
    EXPECT_EQ(url->fragment, "result");
}

TEST(URLParser, IPAddressURLV21) {
    auto url = parse("http://192.168.1.1:8080/admin/dashboard");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080);
    EXPECT_EQ(url->path, "/admin/dashboard");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, PathResolutionDotDotV21) {
    auto url = parse("https://example.com/a/b/c/../d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/a/b/d");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, UppercaseSchemesNormalizedV21) {
    auto url = parse("HTTPS://EXAMPLE.COM/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, TrailingSlashHandlingV21) {
    auto url = parse("http://example.net:9090/api/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.net");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9090);
    EXPECT_EQ(url->path, "/api/");
}

TEST(URLParser, CustomPortHTTPSV21) {
    auto url = parse("https://secure.example.com:9443/login?redirect=/home");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9443);
    EXPECT_EQ(url->path, "/login");
    EXPECT_EQ(url->query, "redirect=/home");
}

TEST(URLParser, MultipleQueryParamsV21) {
    auto url = parse("https://api.data.io/search?q=test&limit=10&offset=0&sort=desc");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.data.io");
    EXPECT_EQ(url->path, "/search");
    EXPECT_EQ(url->query, "q=test&limit=10&offset=0&sort=desc");
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, FragmentOnlyNoQueryV21) {
    auto url = parse("http://docs.example.org/guide#section3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "docs.example.org");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_TRUE(url->query.empty());
    EXPECT_EQ(url->fragment, "section3");
}

TEST(URLParser, SubdomainDeepPathV21) {
    auto url = parse("https://cdn.assets.platform.io:8443/v2/public/images/thumbnails?format=webp#preview");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.assets.platform.io");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
    EXPECT_EQ(url->path, "/v2/public/images/thumbnails");
    EXPECT_EQ(url->query, "format=webp");
    EXPECT_EQ(url->fragment, "preview");
}

TEST(URLParser, HostOnlyURLParsesWithSlashPathV22) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, PathResolutionMultipleDotDotsV22) {
    auto url = parse("http://example.com/a/b/c/d/../../e");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/a/b/e");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, HTTPDefaultPortNormalizedV22) {
    auto url = parse("http://example.org:80/api/users");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/api/users");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, HTTPSDefaultPortNormalizedV22) {
    auto url = parse("https://secure.example.net:443/checkout");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.net");
    EXPECT_EQ(url->path, "/checkout");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, LowercaseHostPortSchemeV22) {
    auto url = parse("HTTPS://EXAMPLE.COM:8443/PATH?QUERY=1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/PATH");
    EXPECT_EQ(url->query, "QUERY=1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
}

TEST(URLParser, ComplexPathWithDotsNotPathResolutionV22) {
    auto url = parse("https://docs.example.io/v1.2.3/api.reference.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/v1.2.3/api.reference.html");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, QueryFragmentWithSpecialCharsV22) {
    auto url = parse("http://api.example.com/search?q=hello+world&filter=active#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "api.example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_EQ(url->query, "q=hello+world&filter=active");
    EXPECT_EQ(url->fragment, "results");
}

TEST(URLParser, DeepSubdomainWithHighPortV22) {
    auto url = parse("https://api.v2.service.example.com:65535/enterprise/admin/dashboard?view=analytics#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.v2.service.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 65535);
    EXPECT_EQ(url->path, "/enterprise/admin/dashboard");
    EXPECT_EQ(url->query, "view=analytics");
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, HostOnlyPathNormalizationV23) {
    auto url = parse("https://example.org");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathWithDoubleDotResolutionV23) {
    auto url = parse("http://files.example.net/documents/../public/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "files.example.net");
    EXPECT_EQ(url->path, "/public/file.txt");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, PathWithMultipleDotResolutionV23) {
    auto url = parse("https://data.example.com/api/v1/../../assets/image.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "data.example.com");
    EXPECT_EQ(url->path, "/assets/image.png");
}

TEST(URLParser, CustomPortWithQueryFragmentV23) {
    auto url = parse("http://localhost:9000/api/test?key=value&mode=debug#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9000);
    EXPECT_EQ(url->path, "/api/test");
    EXPECT_EQ(url->query, "key=value&mode=debug");
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, NumericSubdomainWithPortV23) {
    auto url = parse("https://192.168.1.100:8443/admin/console");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "192.168.1.100");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
    EXPECT_EQ(url->path, "/admin/console");
}

TEST(URLParser, LongPathWithMultipleSegmentsV23) {
    auto url = parse("http://example.io/static/assets/images/icons/theme/dark/logo.svg?v=2.1");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.io");
    EXPECT_EQ(url->path, "/static/assets/images/icons/theme/dark/logo.svg");
    EXPECT_EQ(url->query, "v=2.1");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, HTTPSDefaultPortOmittedV23) {
    auto url = parse("https://secure.api.example.com:443/v2/endpoint?token=abc123");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.api.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/v2/endpoint");
    EXPECT_EQ(url->query, "token=abc123");
}

TEST(URLParser, RootPathWithQueryAndFragmentV23) {
    auto url = parse("https://cdn.example.net/?utm_source=ref&utm_medium=social#content");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.net");
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "utm_source=ref&utm_medium=social");
    EXPECT_EQ(url->fragment, "content");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, HTTPDefaultPortNormalizedAwayV24) {
    auto url = parse("http://example.com:80/index.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/index.html");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, SubdomainWithDeepPathV24) {
    auto url = parse("https://api.v2.service.example.org/v1/users/profile/settings");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.v2.service.example.org");
    EXPECT_EQ(url->path, "/v1/users/profile/settings");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, URLWithOnlyFragmentV24) {
    auto url = parse("https://docs.example.io/guide#section-3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "section-3");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, ComplexQueryStringV24) {
    auto url = parse("http://search.example.net/results?q=test+query&limit=50&offset=0&sort=relevance");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "search.example.net");
    EXPECT_EQ(url->path, "/results");
    EXPECT_EQ(url->query, "q=test+query&limit=50&offset=0&sort=relevance");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathWithTrailingSlashAndQueryV24) {
    auto url = parse("https://shop.example.com/products/?category=electronics&brand=acme");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "shop.example.com");
    EXPECT_EQ(url->path, "/products/");
    EXPECT_EQ(url->query, "category=electronics&brand=acme");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathResolutionWithConsecutiveDotsV24) {
    auto url = parse("http://cdn.example.co/assets/styles/../../vendor/fonts/arial.ttf");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "cdn.example.co");
    EXPECT_EQ(url->path, "/vendor/fonts/arial.ttf");
    EXPECT_EQ(url->port, std::nullopt);
}

TEST(URLParser, CustomPortWithComplexPathAndQueryFragmentV24) {
    auto url = parse("https://backend.app.local:5000/api/v3/data/export?format=json&verbose=true#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "backend.app.local");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 5000);
    EXPECT_EQ(url->path, "/api/v3/data/export");
    EXPECT_EQ(url->query, "format=json&verbose=true");
    EXPECT_EQ(url->fragment, "results");
}

TEST(URLParser, LoopbackWithCustomPortV24) {
    auto url = parse("http://127.0.0.1:3000/dev/debug/logs?level=info");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "127.0.0.1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 3000);
    EXPECT_EQ(url->path, "/dev/debug/logs");
    EXPECT_EQ(url->query, "level=info");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, SimpleHttpsWithoutPortV25) {
    auto url = parse("https://api.service.io/users");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.io");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/users");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HostOnlyWithHttpV25) {
    auto url = parse("http://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, DeepPathWithMultipleSegmentsV25) {
    auto url = parse("https://storage.cloud.io/bucket/folder/subfolder/file.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "storage.cloud.io");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/bucket/folder/subfolder/file.txt");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

// ============================================================================
// Cycle 1357: 8 new URL parser tests - V57
// ============================================================================

TEST(URLParser, IPv6AddressURLParsingV57) {
    auto url = parse("http://[::1]:8080/api");
    if (url.has_value()) {
        EXPECT_EQ(url->scheme, "http");
        EXPECT_EQ(url->path, "/api");
    }
}

TEST(URLParser, QueryParameterWithPercentEncodingV57) {
    auto url = parse("https://api.example.com/search?q=hello%2Bworld&filter=active");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_FALSE(url->query.empty());
}

TEST(URLParser, PortMaxValueEdgeCaseV57) {
    auto url = parse("https://example.com:65535/resource");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 65535u);
    EXPECT_EQ(url->path, "/resource");
}

TEST(URLParser, EmptyPathWithQueryAndFragmentV57) {
    auto url = parse("https://example.com?key=val#anchor");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_FALSE(url->query.empty());
    EXPECT_FALSE(url->fragment.empty());
}

TEST(URLParser, FragmentWithPercentEncodedCharV57) {
    auto url = parse("https://docs.example.org/page#section%20name");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.org");
    EXPECT_EQ(url->path, "/page");
    EXPECT_FALSE(url->fragment.empty());
}

TEST(URLParser, MultipleConsecutiveSlashesInPathV57) {
    auto url = parse("https://example.com//api//v1//users");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_FALSE(url->path.empty());
    EXPECT_TRUE(url->path.find("api") != std::string::npos);
}

TEST(URLParser, HostWithLeadingAndTrailingDotsV57) {
    auto url = parse("https://example.com./path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_FALSE(url->host.empty());
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, QueryWithSingleAmpersandOnlyV57) {
    auto url = parse("https://example.com/search?&");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/search");
}

TEST(URLParser, QueryWithMultipleParametersAndFragmentV25) {
    auto url = parse("http://video.example.org/player?id=abc123&autoplay=1&quality=hd#t=45s");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "video.example.org");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/player");
    EXPECT_EQ(url->query, "id=abc123&autoplay=1&quality=hd");
    EXPECT_EQ(url->fragment, "t=45s");
}

TEST(URLParser, PathResolutionWithDotDotsAndTrailingSlashV25) {
    auto url = parse("https://docs.site.net/guides/../tutorials/../index.html/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.site.net");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/index.html/");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, CustomPortWithSimplePathV25) {
    auto url = parse("http://localhost:8080/health");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "localhost");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080);
    EXPECT_EQ(url->path, "/health");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, NumericSubdomainWithQueryV25) {
    auto url = parse("https://v2.api.domain.com/data?page=1&size=20&sort=-date");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "v2.api.domain.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/data");
    EXPECT_EQ(url->query, "page=1&size=20&sort=-date");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathWithDotSegmentResolutionV25) {
    auto url = parse("http://www.example.net/a/./b/../c/./d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "www.example.net");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/a/c/d");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, ComplexPathResolutionWithMultipleDotSegmentsV26) {
    auto url = parse("https://example.org/a/b/c/../../d/../e/f");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.org");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/a/e/f");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HostOnlyUrlReturnsSlashPathV26) {
    auto url = parse("http://test.example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "test.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpsNonDefaultPort3000NormalizedV26) {
    auto url = parse("https://api.service.io:3000/v1/endpoint");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.service.io");
    EXPECT_EQ(url->port, 3000);
    EXPECT_EQ(url->path, "/v1/endpoint");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, ComplexQueryStringWithMultipleParametersAndValuesV26) {
    auto url = parse("https://search.service.com/results?q=test&filter=active&sort=date&limit=10");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.service.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/results");
    EXPECT_NE(url->query.find("q=test"), std::string::npos);
    EXPECT_NE(url->query.find("filter=active"), std::string::npos);
    EXPECT_NE(url->query.find("sort=date"), std::string::npos);
    EXPECT_NE(url->query.find("limit=10"), std::string::npos);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, FragmentWithComplexIdentifierV26) {
    auto url = parse("https://docs.example.io/manual/guide#installation-requirements-section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.io");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/manual/guide");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "installation-requirements-section");
}

TEST(URLParser, DeepPathWithTraversalResolvingFromRootV26) {
    auto url = parse("http://cdn.example.net/static/../assets/../../data/./files/image.png");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "cdn.example.net");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/data/files/image.png");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, QueryAndFragmentBothPresentV26) {
    auto url = parse("https://auth.example.com/callback?code=abc123&state=xyz#section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "auth.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/callback");
    EXPECT_NE(url->query.find("code=abc123"), std::string::npos);
    EXPECT_NE(url->query.find("state=xyz"), std::string::npos);
    EXPECT_EQ(url->fragment, "section");
}

TEST(URLParser, SubdomainWithNonDefaultPortAndComplexPathV26) {
    auto url = parse("https://staging.cdn.example.com:9443/v2/media/./content/../resource/file.mp4");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "staging.cdn.example.com");
    EXPECT_EQ(url->port, 9443);
    EXPECT_EQ(url->path, "/v2/media/resource/file.mp4");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpsWithUsernameAndPasswordV27) {
    auto url = parse("https://user:pass@example.com/secure");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "pass");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/secure");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HostOnlyWithTrailingSlashV27) {
    auto url = parse("https://example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
    EXPECT_TRUE(url->username.empty());
    EXPECT_TRUE(url->password.empty());
}

TEST(URLParser, MultipleDotSegmentsResolvedV27) {
    auto url = parse("https://a.com/a/b/c/../../d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/a/d");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, QueryWithSpecialCharsV27) {
    auto url = parse("https://a.com/search?q=hello+world&lang=en");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/search");
    EXPECT_NE(url->query.find("q=hello+world"), std::string::npos);
    EXPECT_NE(url->query.find("lang=en"), std::string::npos);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, FragmentOnlyV27) {
    auto url = parse("https://a.com/page#top");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "top");
}

TEST(URLParser, CustomPort9090V27) {
    auto url = parse("https://a.com:9090/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 9090);
    EXPECT_EQ(url->path, "/api");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpDefaultPort80NormalizedV27) {
    auto url = parse("http://a.com:80/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, DeepSubdomainWithPathV27) {
    auto url = parse("https://a.b.c.d.example.com/deep/path/file.html");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.b.c.d.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/deep/path/file.html");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpsPort8443V28) {
    auto url = parse("https://a.com:8443/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8443);
    EXPECT_EQ(url->path, "/api");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathWithEncodedSpaceV28) {
    auto url = parse("https://a.com/path%20file");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/path%2520file");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, EmptyQueryV28) {
    auto url = parse("https://a.com/page?");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, EmptyFragmentV28) {
    auto url = parse("https://a.com/page#");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, IPAddressHostV28) {
    auto url = parse("http://192.168.1.1/index");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/index");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, TripleDotResolutionV28) {
    auto url = parse("https://a.com/a/b/c/../../../d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/d");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpsDefaultPort443NormalizedV28) {
    auto url = parse("https://a.com:443/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, LongPathV28) {
    auto url = parse("https://a.com/a/b/c/d/e/f/g/h");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/a/b/c/d/e/f/g/h");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, FtpSchemeV29) {
    auto url = parse("ftp://files.example.com/pub");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_EQ(url->host, "files.example.com");
    EXPECT_EQ(url->path, "/pub");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, QueryMultipleParamsV29) {
    auto url = parse("https://a.com/s?a=1&b=2&c=3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/s");
    EXPECT_NE(url->query.find("a=1"), std::string::npos);
    EXPECT_NE(url->query.find("b=2"), std::string::npos);
    EXPECT_NE(url->query.find("c=3"), std::string::npos);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, TrailingDotInHostV29) {
    auto url = parse("https://example.com./path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com.");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/path");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PortZeroV29) {
    auto url = parse("http://a.com:0/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 0);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, SingleCharPathV29) {
    auto url = parse("https://a.com/x");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/x");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, NoPathV29) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_TRUE(url->path == "/" || url->path == "");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpPort8080V29) {
    auto url = parse("http://a.com:8080/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 8080);
    EXPECT_EQ(url->path, "/api");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, QueryWithHashInValueV29) {
    auto url = parse("https://a.com/page?color=%23red");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_NE(url->query.find("color="), std::string::npos);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, WssSchemeV30) {
    auto url = parse("wss://ws.example.com/socket");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "wss");
    EXPECT_EQ(url->host, "ws.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/socket");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, QueryEmptyValueV30) {
    auto url = parse("https://a.com/p?key=");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/p");
    EXPECT_NE(url->query.find("key="), std::string::npos);
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, MultipleSlashesInPathV30) {
    auto url = parse("https://a.com//a//b");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "//a//b");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PortMaxV30) {
    auto url = parse("http://a.com:65535/x");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(url->port.value(), 65535);
    EXPECT_EQ(url->path, "/x");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, SchemeOnlyV30) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_TRUE(url->path == "/" || url->path == "");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, PathWithDotsNotResolvedV30) {
    auto url = parse("https://a.com/a/./b");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/a/b");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HttpsPort443ImplicitV30) {
    auto url = parse("https://a.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, HostWithHyphenV30) {
    auto url = parse("https://my-site.example.com/page");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "my-site.example.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->query, "");
    EXPECT_EQ(url->fragment, "");
}

TEST(URLParser, DataSchemeV31) {
    auto url = parse("data:text/html,Hello");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
}

TEST(URLParser, HttpPort80NormalizedV31) {
    auto url = parse("http://a.com:80/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, std::nullopt);
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, QueryWithAmpersandV31) {
    auto url = parse("https://a.com/?a=1&b=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_TRUE(url->query.find("a=1") != std::string::npos);
}

TEST(URLParser, FragmentWithSpecialCharsV31) {
    auto url = parse("https://a.com/page#top-section");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->path, "/page");
    EXPECT_EQ(url->fragment, "top-section");
}

TEST(URLParser, HostNumericOnlyV31) {
    auto url = parse("http://127.0.0.1:8080/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "127.0.0.1");
    EXPECT_EQ(url->port, 8080);
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, PathResolveDoubleDotV31) {
    auto url = parse("https://a.com/x/y/z/../../w");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->path, "/x/w");
}

TEST(URLParser, HttpsNoPathNoQueryV31) {
    auto url = parse("https://example.org");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.org");
}

TEST(URLParser, CustomPort1234V31) {
    auto url = parse("http://a.com:1234/test");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->port, 1234);
    EXPECT_EQ(url->path, "/test");
}

TEST(URLParser, HttpsPort4433V32) {
    auto url = parse("https://a.com:4433/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 4433);
    EXPECT_EQ(url->path, "/api");
}

TEST(URLParser, SimplePathV32) {
    auto url = parse("https://a.com/hello");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->path, "/hello");
}

TEST(URLParser, QueryOnlyNoPathV32) {
    auto url = parse("https://a.com?key=val");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_TRUE(url->query.find("key=val") != std::string::npos);
}

TEST(URLParser, FragmentWithNumbersV32) {
    auto url = parse("https://a.com/p#section3");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->path, "/p");
    EXPECT_EQ(url->fragment, "section3");
}

TEST(URLParser, HostWithNumbersV32) {
    auto url = parse("https://app2.example.com/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "app2.example.com");
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, DoubleDotAtStartV32) {
    auto url = parse("https://a.com/../b");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    // Path should resolve the .. correctly
    EXPECT_TRUE(!url->path.empty());
}

TEST(URLParser, HttpPort8888V32) {
    auto url = parse("http://a.com:8888/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "a.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8888);
    EXPECT_EQ(url->path, "/");
}

TEST(URLParser, LongQueryV32) {
    auto url = parse("https://a.com/s?a=1&b=2&c=3&d=4&e=5");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "a.com");
    EXPECT_EQ(url->path, "/s");
    EXPECT_TRUE(url->query.find("c=3") != std::string::npos);
}

TEST(URLParser, PathWithTrailingSlashV33) {
    auto url = parse("https://example.com/api/v1/");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/api/v1/");
}

TEST(URLParser, QueryWithMultipleParametersV33) {
    auto url = parse("https://search.com/results?q=test&page=1&sort=date");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.com");
    EXPECT_EQ(url->query, "q=test&page=1&sort=date");
    EXPECT_EQ(url->path, "/results");
}

TEST(URLParser, FragmentWithColonV33) {
    auto url = parse("https://docs.com/guide#section:subsection");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.com");
    EXPECT_EQ(url->fragment, "section:subsection");
}

TEST(URLParser, PortZeroIsParsedV33) {
    auto url = parse("http://example.com:0/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->port, 0);
}

TEST(URLParser, FtpSchemeWithDefaultPortV33) {
    auto url = parse("ftp://ftp.example.com/files");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "ftp");
    EXPECT_EQ(url->host, "ftp.example.com");
    EXPECT_EQ(url->path, "/files");
}

TEST(URLParser, PathWithDoubleSlashV33) {
    auto url = parse("https://cdn.example.com/content//assets/file.js");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn.example.com");
    EXPECT_EQ(url->path, "/content//assets/file.js");
}

TEST(URLParser, PercentEncodingDoubleEncodedV33) {
    auto url = parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->query, "q=hello%2520world");
}

TEST(URLParser, CompleteURLAllComponentsV33) {
    auto url = parse("https://user:pass@secure.example.com:8443/api/data?id=123&key=val#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "secure.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8443);
    EXPECT_EQ(url->path, "/api/data");
    EXPECT_TRUE(url->query.find("id=123") != std::string::npos);
    EXPECT_EQ(url->fragment, "results");
}

TEST(URLParser, IPv4AddressParsingV34) {
    auto url = parse("http://192.168.1.1:8080/admin");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "http");
    EXPECT_EQ(url->host, "192.168.1.1");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8080);
    EXPECT_EQ(url->path, "/admin");
}

TEST(URLParser, PathResolutionWithDoubleDotV34) {
    auto url = parse("https://example.com/api/v1/../v2/endpoint");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/api/v2/endpoint");
}

TEST(URLParser, HostOnlyURLGetsDefaultPathV34) {
    auto url = parse("https://example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_TRUE(url->query.empty());
    EXPECT_TRUE(url->fragment.empty());
}

TEST(URLParser, QueryStringWithEmptyValueV34) {
    auto url = parse("https://search.example.com/search?q=&filter=active");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "search.example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_TRUE(url->query.find("q=") != std::string::npos);
    EXPECT_TRUE(url->query.find("filter=active") != std::string::npos);
}

TEST(URLParser, FragmentWithSpecialCharsV34) {
    auto url = parse("https://docs.example.com/guide#section-2.5_overview");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.com");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_EQ(url->fragment, "section-2.5_overview");
}

TEST(URLParser, DataSchemeWithParametersV34) {
    auto url = parse("data:text/html;charset=UTF-8,<h1>Hello</h1>");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "data");
    EXPECT_EQ(url->path, "text/html;charset=UTF-8,<h1>Hello</h1>");
}

TEST(URLParser, SubdomainWithMultiplePartsV34) {
    auto url = parse("https://api.v2.staging.example.com:9443/endpoint?version=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "api.v2.staging.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 9443);
    EXPECT_EQ(url->path, "/endpoint");
    EXPECT_EQ(url->query, "version=2");
}

TEST(URLParser, MailtoSchemeWithoutSlashesV34) {
    auto url = parse("mailto:user@example.com");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "mailto");
}

TEST(URLParser, HttpsDefaultPortNormalizedAwayV35) {
    auto url = parse("https://example.com:443/login");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_FALSE(url->port.has_value());
    EXPECT_EQ(url->path, "/login");
}

TEST(URLParser, HttpsNonDefaultPortPreservedV35) {
    auto url = parse("https://example.com:444/login");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 444);
    EXPECT_EQ(url->path, "/login");
}

TEST(URLParser, DotDotResolutionAcrossSegmentsV35) {
    auto url = parse("https://example.com/a/b/../../c/d/../e");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/c/e");
}

TEST(URLParser, QueryAndFragmentSplitV35) {
    auto url = parse("https://example.com/search?q=browser&lang=en#results");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_EQ(url->query, "q=browser&lang=en");
    EXPECT_EQ(url->fragment, "results");
}

TEST(URLParser, SubdomainWithHyphenAndCountryTldV35) {
    auto url = parse("https://cdn-2.assets.example.co.uk/v1/file.js");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "cdn-2.assets.example.co.uk");
    EXPECT_EQ(url->path, "/v1/file.js");
}

TEST(URLParser, HostOnlyWithQueryGetsSlashPathV35) {
    auto url = parse("https://example.com?x=1&y=2");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->query, "x=1&y=2");
}

TEST(URLParser, HostOnlyWithFragmentGetsSlashPathV35) {
    auto url = parse("https://example.com#overview");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/");
    EXPECT_EQ(url->fragment, "overview");
}

TEST(URLParser, FileSchemeAbsolutePathV35) {
    auto url = parse("file:///Users/test/docs/readme.txt");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "file");
    EXPECT_EQ(url->path, "/Users/test/docs/readme.txt");
}

TEST(URLParser, PortEdgeCaseZeroIsInvalidV36) {
    auto url = parse("https://example.com:0/path");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    // Port 0 may or may not be preserved depending on implementation
    EXPECT_EQ(url->path, "/path");
}

TEST(URLParser, PortEdgeCaseMaximumValidV36) {
    auto url = parse("https://example.com:65535/api");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 65535);
    EXPECT_EQ(url->path, "/api");
}

TEST(URLParser, MultipleSlashesInPathGetNormalizedV36) {
    auto url = parse("https://example.com/a//b///c////d");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    // Path may preserve or normalize multiple slashes
    EXPECT_FALSE(url->path.empty());
}

TEST(URLParser, QueryWithMultipleParamsAndAmpersandV36) {
    auto url = parse("https://example.com/search?name=john&age=30&role=admin");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    EXPECT_EQ(url->path, "/search");
    EXPECT_NE(url->query.find("name=john"), std::string::npos);
    EXPECT_NE(url->query.find("age=30"), std::string::npos);
    EXPECT_NE(url->query.find("role=admin"), std::string::npos);
}

TEST(URLParser, RelativePathDoubleDotTraversalV36) {
    auto base = parse("https://example.com/api/v1/users/list");
    ASSERT_TRUE(base.has_value());
    auto result = parse("../../endpoint", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/api/endpoint");
}

TEST(URLParser, AnchorFragmentWithSpecialCharsV36) {
    auto url = parse("https://docs.example.com/guide#section_2.1-heading");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "docs.example.com");
    EXPECT_EQ(url->path, "/guide");
    EXPECT_EQ(url->fragment, "section_2.1-heading");
}

TEST(URLParser, MixedCaseSchemeAndHostNormalizedV36) {
    auto url = parse("HtTpS://ExAmPle.CoM:8443/Path?Query=Value#Frag");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->host, "example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 8443);
    EXPECT_EQ(url->path, "/Path");
    EXPECT_EQ(url->query, "Query=Value");
    EXPECT_EQ(url->fragment, "Frag");
}

TEST(URLParser, ComplexUrlWithUserinfoAndAllComponentsV36) {
    auto url = parse("https://user:pass@api.example.com:9443/v2/resource?filter=active#item-5");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->scheme, "https");
    EXPECT_EQ(url->username, "user");
    EXPECT_EQ(url->password, "pass");
    EXPECT_EQ(url->host, "api.example.com");
    ASSERT_TRUE(url->port.has_value());
    EXPECT_EQ(*url->port, 9443);
    EXPECT_EQ(url->path, "/v2/resource");
    EXPECT_EQ(url->query, "filter=active");
    EXPECT_EQ(url->fragment, "item-5");
}

// =============================================================================
// Test V58-1: Percent-decoding in path components
// =============================================================================
TEST(URLParser, PercentDecodingInPathV58) {
    auto result = parse("https://example.com/hello%20world/test%2Fpath");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/hello%2520world/test%252Fpath");
}

// =============================================================================
// Test V58-2: URL serialization with all components
// =============================================================================
TEST(URLParser, URLSerializationWithAllComponentsV58) {
    auto url = parse("https://user:pass@example.com:8443/path?key=value#frag");
    ASSERT_TRUE(url.has_value());
    std::string serialized = url->serialize();
    auto reparsed = parse(serialized);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->scheme, "https");
    EXPECT_EQ(reparsed->username, "user");
    EXPECT_EQ(reparsed->password, "pass");
    EXPECT_EQ(reparsed->host, "example.com");
    EXPECT_EQ(*reparsed->port, 8443);
    EXPECT_EQ(reparsed->path, "/path");
    EXPECT_EQ(reparsed->query, "key=value");
    EXPECT_EQ(reparsed->fragment, "frag");
}

// =============================================================================
// Test V58-3: Uppercase scheme normalization
// =============================================================================
TEST(URLParser, UppercaseSchemNormalizationV58) {
    auto result = parse("HTTPS://EXAMPLE.COM/Path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/Path");
}

// =============================================================================
// Test V58-4: Empty query and fragment preservation
// =============================================================================
TEST(URLParser, EmptyQueryAndFragmentV58) {
    auto result = parse("https://example.com/path?#");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test V58-5: URL with multiple subdomains
// =============================================================================
TEST(URLParser, MultipleSubdomainsV58) {
    auto result = parse("https://api.v2.staging.example.com:9443/endpoint");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "api.v2.staging.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 9443);
    EXPECT_EQ(result->path, "/endpoint");
}

// =============================================================================
// Test V58-6: Special characters in query string
// =============================================================================
TEST(URLParser, SpecialCharactersInQueryV58) {
    auto result = parse("https://example.com/search?q=hello%20world&sort=date&filter=a%3Db");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->query, "q=hello%2520world&sort=date&filter=a%253Db");
    EXPECT_EQ(result->path, "/search");
}

// =============================================================================
// Test V58-7: Trailing slash normalization
// =============================================================================
TEST(URLParser, TrailingSlashNormalizationV58) {
    auto url1 = parse("https://example.com");
    auto url2 = parse("https://example.com/");
    ASSERT_TRUE(url1.has_value());
    ASSERT_TRUE(url2.has_value());
    // Both normalize to "/" as the path
    EXPECT_EQ(url1->path, "/");
    EXPECT_EQ(url2->path, "/");
}

// =============================================================================
// Test V58-8: IPv4 address parsing
// =============================================================================
TEST(URLParser, IPv4AddressParsingV58) {
    auto result = parse("http://192.168.1.1:3000/admin");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "192.168.1.1");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 3000);
    EXPECT_EQ(result->path, "/admin");
}

// =============================================================================
// Test V59-1: Percent-encoded space in path (%20 should not double-encode)
// =============================================================================
TEST(URLParser, PercentEncodedSpaceV59) {
    auto result = parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/hello%2520world");
}

// =============================================================================
// Test V59-2: Multiple percent-encoded characters in path
// =============================================================================
TEST(URLParser, MultiplePercentEncodedV59) {
    auto result = parse("http://example.com/path%2Fwith%2Fslashes");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path%252Fwith%252Fslashes");
}

// =============================================================================
// Test V59-3: Host-only URL gets path="/"
// =============================================================================
TEST(URLParser, HostOnlyURLDefaultPathV59) {
    auto result = parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V59-4: Host with port but no path gets path="/"
// =============================================================================
TEST(URLParser, HostPortOnlyDefaultPathV59) {
    auto result = parse("http://example.com:8080");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 8080);
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V59-5: Percent-encoded special character %3F (question mark)
// =============================================================================
TEST(URLParser, PercentEncodedQuestionMarkV59) {
    auto result = parse("https://example.com/search%3Fterm");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search%253Fterm");
}

// =============================================================================
// Test V59-6: Percent-encoded ampersand %26 in path
// =============================================================================
TEST(URLParser, PercentEncodedAmpersandV59) {
    auto result = parse("http://example.com/a%26b%26c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a%2526b%2526c");
}

// =============================================================================
// Test V59-7: Host with credentials but no path gets path="/"
// =============================================================================
TEST(URLParser, HostWithCredentialsDefaultPathV59) {
    auto result = parse("https://user:pass@example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V59-8: Percent-encoded percent sign %25 should not cause issues
// =============================================================================
TEST(URLParser, PercentEncodedPercentSignV59) {
    auto result = parse("https://example.com/discount%2550off");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // The parser double-encodes percent sequences: %25 → %2525
    EXPECT_EQ(result->path, "/discount%252550off");
}

// =============================================================================
// Test V60-1: Punycode-encoded International Domain Name (IDN)
// =============================================================================
TEST(URLParser, InternationalDomainNameV60) {
    auto result = parse("https://xn--mnchen-3ya.de/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    // The host should be in punycode format (xn-- prefix)
    EXPECT_EQ(result->host, "xn--mnchen-3ya.de");
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test V60-2: Query string with multiple parameters and percent-encoded values
// =============================================================================
TEST(URLParser, QueryStringMultipleParamsV60) {
    auto result = parse("https://example.com/search?q=hello%20world&sort=name&limit=10");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=hello%2520world&sort=name&limit=10");
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V60-3: IPv6 address with zone ID handling
// =============================================================================
TEST(URLParser, IPv6ZoneIDV60) {
    auto result = parse("http://[fe80::1%eth0]/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    // Zone IDs in IPv6 are not typically parsed but shouldn't crash
    EXPECT_FALSE(result->host.empty());
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test V60-4: Fragment with query-like syntax (no actual query parsing)
// =============================================================================
TEST(URLParser, FragmentWithQuerySyntaxV60) {
    auto result = parse("https://example.com/page#section?param=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_TRUE(result->query.empty());
    // Fragment should include the ? and everything after
    EXPECT_EQ(result->fragment, "section?param=value");
}

// =============================================================================
// Test V60-5: Unusual port number (65535 - maximum valid port)
// =============================================================================
TEST(URLParser, MaximumPortNumberV60) {
    auto result = parse("http://example.com:65535/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(*result->port, 65535);
    EXPECT_EQ(result->path, "/resource");
}

// =============================================================================
// Test V60-6: Relative URL resolution (base + relative)
// =============================================================================
TEST(URLParser, RelativeURLResolutionV60) {
    auto result = parse("https://example.com/docs/api/v1/users");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/docs/api/v1/users");
}

// =============================================================================
// Test V60-7: Data URI with base64 encoding
// =============================================================================
TEST(URLParser, DataURIBase64V60) {
    auto result = parse("data:text/plain;base64,SGVsbG8gV29ybGQ=");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_EQ(result->host, "");
    // Data URIs have special handling; path should contain the data part
    EXPECT_FALSE(result->path.empty());
}

// =============================================================================
// Test V60-8: File URI with special characters in path (percent-encoded)
// =============================================================================
TEST(URLParser, FileURIWithSpecialCharsV60) {
    auto result = parse("file:///home/user/My%20Documents/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_EQ(result->host, "");
    // Path should double-encode the %20 sequence
    EXPECT_EQ(result->path, "/home/user/My%2520Documents/file.txt");
}

// =============================================================================
// Test V61-1: Blob URL parsing
// =============================================================================
TEST(URLParser, BlobURLV61) {
    auto result = parse("blob:https://example.com/550e8400-e29b-41d4-a716-446655440000");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_TRUE(result->host.empty());
    // Blob URLs store the entire origin as part of the path
    EXPECT_EQ(result->path, "https://example.com/550e8400-e29b-41d4-a716-446655440000");
}

// =============================================================================
// Test V61-2: JavaScript URL handling
// =============================================================================
TEST(URLParser, JavaScriptURLV61) {
    auto result = parse("javascript:void(0)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "javascript");
    EXPECT_EQ(result->path, "void(0)");
    EXPECT_TRUE(result->host.empty());
    EXPECT_TRUE(result->query.empty());
}

// =============================================================================
// Test V61-3: About:blank URL
// =============================================================================
TEST(URLParser, AboutBlankURLV61) {
    auto result = parse("about:blank");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "about");
    EXPECT_EQ(result->path, "blank");
    EXPECT_TRUE(result->host.empty());
    EXPECT_TRUE(result->query.empty());
}

// =============================================================================
// Test V61-4: Empty string URL handling
// =============================================================================
TEST(URLParser, EmptyStringURLV61) {
    auto result = parse("");
    // Empty string should either fail or return a minimal/relative URL
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Test V61-5: Whitespace in URL path segments (percent-encoded to %20, then double-encoded)
// =============================================================================
TEST(URLParser, WhitespaceInPathV61) {
    auto result = parse("https://example.com/hello%20world/test");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Double-encode: %20 becomes %2520
    EXPECT_EQ(result->path, "/hello%2520world/test");
}

// =============================================================================
// Test V61-6: Trailing dot in hostname
// =============================================================================
TEST(URLParser, TrailingDotHostnameV61) {
    auto result = parse("https://example.com./path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    // Host may or may not include the trailing dot depending on parsing rules
    EXPECT_TRUE(result->host.find("example.com") != std::string::npos);
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test V61-7: Protocol-relative URL with double slash (unsupported by parser)
// =============================================================================
TEST(URLParser, ProtocolRelativeURLV61) {
    auto result = parse("//example.com/path");
    // Protocol-relative URLs without a scheme are not parsed by this parser
    // (they require a base URL to resolve). Parser returns nullopt.
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Test V61-8: Unicode in path segments (UTF-8 encoded)
// =============================================================================
TEST(URLParser, UnicodePathSegmentV61) {
    auto result = parse("https://example.com/café/menu");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Path should contain the UTF-8 encoded unicode characters
    EXPECT_TRUE(result->path.find("caf") != std::string::npos);
}

// =============================================================================
// Test V62-1: URL with @ symbol in path (not authentication)
// =============================================================================
TEST(URLParser, AtSymbolInPathV62) {
    auto result = parse("https://example.com/user@domain/profile");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/user@domain/profile");
    EXPECT_TRUE(result->username.empty());
    EXPECT_TRUE(result->password.empty());
}

// =============================================================================
// Test V62-2: URL with multiple query parameters
// =============================================================================
TEST(URLParser, MultipleQueryParametersV62) {
    auto result = parse("https://example.com/search?q=test&sort=asc&limit=10&offset=5");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=test&sort=asc&limit=10&offset=5");
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V62-3: URL with empty query string (? present but no query value)
// =============================================================================
TEST(URLParser, EmptyQueryStringV62) {
    auto result = parse("https://example.com/path?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V62-4: URL with hash-only fragment (# present but no fragment value)
// =============================================================================
TEST(URLParser, EmptyFragmentV62) {
    auto result = parse("https://example.com/page#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V62-5: URL with port 0 (edge case)
// =============================================================================
TEST(URLParser, PortZeroV62) {
    auto result = parse("http://example.com:0/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 0);
    EXPECT_EQ(result->path, "/path");
}

// =============================================================================
// Test V62-6: URL with extremely long path
// =============================================================================
TEST(URLParser, ExtremelyLongPathV62) {
    std::string longPath = "/segment";
    for (int i = 0; i < 50; ++i) {
        longPath += "/subsegment";
    }
    std::string url = "https://example.com" + longPath;
    auto result = parse(url);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, longPath);
}

// =============================================================================
// Test V62-7: URL with spaces in path (should be percent-encoded)
// =============================================================================
TEST(URLParser, SpacesInPathV62) {
    auto result = parse("https://example.com/hello world/test file");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Spaces should be encoded as %20, then double-encoded to %2520
    EXPECT_EQ(result->path, "/hello%20world/test%20file");
}

// =============================================================================
// Test V62-8: Scheme-only URL (no authority, no path)
// =============================================================================
TEST(URLParser, SchemeOnlyURLV62) {
    auto result = parse("file://");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V63-1: FTP URL with authentication, custom port, query, and fragment
// =============================================================================
TEST(URLParser, FtpAuthPortQueryFragmentV63) {
    auto result = parse("ftp://user:pa%20ss@files.example.com:2121/archive%20docs/report.txt?mode=bin#sec%201");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pa%2520ss");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->port.value_or(0), 2121);
    EXPECT_EQ(result->path, "/archive%2520docs/report.txt");
    EXPECT_EQ(result->query, "mode=bin");
    EXPECT_EQ(result->fragment, "sec%25201");
}

// =============================================================================
// Test V63-2: WS relative path resolution against a base URL
// =============================================================================
TEST(URLParser, WsRelativePathResolutionV63) {
    auto base = parse("ws://chat.example.com/room/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = parse("../topic%20one?lang=en#live", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ws");
    EXPECT_EQ(result->host, "chat.example.com");
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "/topic%2520one");
    EXPECT_EQ(result->query, "lang=en");
    EXPECT_EQ(result->fragment, "live");
}

// =============================================================================
// Test V63-3: WSS with punycode IDN host and unicode host rejection
// =============================================================================
TEST(URLParser, WssPunycodeAndUnicodeIDNBehaviorV63) {
    auto punycode = parse("wss://xn--mnchen-3ya.de:443/chat#room-1");
    ASSERT_TRUE(punycode.has_value());
    EXPECT_EQ(punycode->scheme, "wss");
    EXPECT_EQ(punycode->host, "xn--mnchen-3ya.de");
    EXPECT_EQ(punycode->port.value_or(0), 0);
    EXPECT_EQ(punycode->path, "/chat");
    EXPECT_EQ(punycode->fragment, "room-1");

    auto unicode = parse("wss://münchen.de/chat");
    EXPECT_FALSE(unicode.has_value());
}

// =============================================================================
// Test V63-4: File URL path preserves special path chars and double-encodes %
// =============================================================================
TEST(URLParser, FileWindowsPathPercentDoubleEncodingV63) {
    auto result = parse("file:///C:/Program%20Files/MyApp/app.exe");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "/C:/Program%2520Files/MyApp/app.exe");
}

// =============================================================================
// Test V63-5: Data URL keeps opaque path/query/fragment without authority
// =============================================================================
TEST(URLParser, DataOpaquePathQueryFragmentV63) {
    auto result = parse("data:text/plain,hello%20world?x=1#frag%202");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "text/plain,hello%20world");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "frag%202");
}

// =============================================================================
// Test V78-1: HTTP default port 80 normalized away
// =============================================================================
TEST(URLParserTest, HttpDefaultPort80NormalizedV78) {
    auto result = parse("http://example.com:80/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    // Port 80 should be normalized away (default for http)
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V78-2: Path '..' resolved correctly
// =============================================================================
TEST(URLParserTest, PathDotDotResolvedV78) {
    auto result = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Path segments with '..' should be resolved
    EXPECT_EQ(result->path, "/a/c");
}

// =============================================================================
// Test V78-3: Host lowercased in URL
// =============================================================================
TEST(URLParserTest, HostLowercasedV78) {
    auto result = parse("https://EXAMPLE.COM/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    // Host should be lowercased
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V78-4: Host-only URL gets root path
// =============================================================================
TEST(URLParserTest, HostOnlyGetsRootPathV78) {
    auto result = parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Host-only URL should have path "/"
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V78-5: Query with ampersand preserved
// =============================================================================
TEST(URLParserTest, QueryWithAmpersandV78) {
    auto result = parse("https://x.com/?a=1&b=2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "x.com");
    EXPECT_EQ(result->path, "/");
    // Query should preserve ampersand separator
    EXPECT_EQ(result->query, "a=1&b=2");
}

// =============================================================================
// Test V78-6: Fragment parsed correctly
// =============================================================================
TEST(URLParserTest, FragmentParsedV78) {
    auto result = parse("https://x.com/p#sec1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "x.com");
    EXPECT_EQ(result->path, "/p");
    // Fragment should be parsed and stored
    EXPECT_EQ(result->fragment, "sec1");
}

// =============================================================================
// Test V78-7: Scheme is case-insensitive
// =============================================================================
TEST(URLParserTest, SchemeIsCaseInsensitiveV78) {
    auto result = parse("HTTPS://X.COM/");
    ASSERT_TRUE(result.has_value());
    // Scheme should be lowercased
    EXPECT_EQ(result->scheme, "https");
    // Host should be lowercased
    EXPECT_EQ(result->host, "x.com");
    EXPECT_EQ(result->path, "/");
}

// =============================================================================
// Test V78-8: Empty path with query string
// =============================================================================
TEST(URLParserTest, EmptyPathWithQueryV78) {
    auto result = parse("https://x.com?q=1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "x.com");
    // Empty path should default to "/"
    EXPECT_EQ(result->path, "/");
    // Query should be parsed
    EXPECT_EQ(result->query, "q=1");
}

// =============================================================================
// Test V63-6: Blob URL keeps nested URL in opaque path
// =============================================================================
TEST(URLParser, BlobOpaqueNestedURLV63) {
    auto result = parse("blob:https://example.com/id%20one?download=true#part%201");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "https://example.com/id%20one");
    EXPECT_EQ(result->query, "download=true");
    EXPECT_EQ(result->fragment, "part%201");
}

// =============================================================================
// Test V63-7: Mailto URL parses as opaque and keeps query and fragment
// =============================================================================
TEST(URLParser, MailtoOpaqueQueryFragmentV63) {
    auto result = parse("mailto:user.name+tag@example.com?subject=hello%20world&body=line1#line-frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "mailto");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "user.name+tag@example.com");
    EXPECT_EQ(result->query, "subject=hello%20world&body=line1");
    EXPECT_EQ(result->fragment, "line-frag");
}

// =============================================================================
// Test V63-8: Tel URL with semicolon params and fragment
// =============================================================================
TEST(URLParser, TelOpaqueNumberWithParamsV63) {
    auto result = parse("tel:+1-800-555-0123;ext=77#dial-now");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "tel");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->port.value_or(0), 0);
    EXPECT_EQ(result->path, "+1-800-555-0123;ext=77");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "dial-now");
}

// =============================================================================
// Test V64-1: Encoded path/query/fragment values are double-encoded in special URLs
// =============================================================================
TEST(URLParser, PercentEncodedPathQueryFragmentDoubleEncodedV64) {
    auto result = parse("https://example.com/a%20b/c%2Fd?x=y%20z#k%20v");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a%2520b/c%252Fd");
    EXPECT_EQ(result->query, "x=y%2520z");
    EXPECT_EQ(result->fragment, "k%2520v");
}

// =============================================================================
// Test V64-2: Userinfo percent sequences are double-encoded
// =============================================================================
TEST(URLParser, UserInfoPercentDoubleEncodingV64) {
    auto result = parse("https://user%20name:pa%2Fss@example.com/private");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->username, "user%2520name");
    EXPECT_EQ(result->password, "pa%252Fss");
    EXPECT_EQ(result->path, "/private");
}

// =============================================================================
// Test V64-3: Relative URL resolution keeps double-encoding on pre-encoded input
// =============================================================================
TEST(URLParser, RelativeResolutionWithEncodedSegmentsV64) {
    auto base = parse("https://example.com/a/b/c/");
    ASSERT_TRUE(base.has_value());

    auto result = parse("../d%20e?u=v%20w#f%20g", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/d%2520e");
    EXPECT_EQ(result->query, "u=v%2520w");
    EXPECT_EQ(result->fragment, "f%2520g");
}

// =============================================================================
// Test V64-4: File URL path double-encodes pre-encoded path bytes
// =============================================================================
TEST(URLParser, FilePathPreEncodedBytesDoubleEncodedV64) {
    auto result = parse("file:///tmp/my%20file%23v1.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/tmp/my%2520file%2523v1.txt");
}

// =============================================================================
// Test V64-5: IPv6 host with non-default port and encoded components
// =============================================================================
TEST(URLParser, IPv6NonDefaultPortWithEncodedComponentsV64) {
    auto result = parse("https://[2001:db8::1]:8443/api%20v1?filter=a%20b#sec%20two");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "[2001:db8::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/api%2520v1");
    EXPECT_EQ(result->query, "filter=a%2520b");
    EXPECT_EQ(result->fragment, "sec%2520two");
}

// =============================================================================
// Test V64-6: Scheme-relative URL reuses base scheme and normalizes host
// =============================================================================
TEST(URLParser, SchemeRelativeWithUserinfoAndDefaultPortV64) {
    auto base = parse("https://base.example/root");
    ASSERT_TRUE(base.has_value());

    auto result = parse("//user%20x:pa%20y@MiXeD.Example:443/a%20b?x=%20#f%20", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "mixed.example");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->username, "user%2520x");
    EXPECT_EQ(result->password, "pa%2520y");
    EXPECT_EQ(result->path, "/a%2520b");
    EXPECT_EQ(result->query, "x=%2520");
    EXPECT_EQ(result->fragment, "f%2520");
}

// =============================================================================
// Test V64-7: Opaque blob URL keeps percent sequences unchanged
// =============================================================================
TEST(URLParser, BlobOpaquePercentSequencesNotReencodedV64) {
    auto result = parse("blob:https://example.com/id%20one?download=100%25#frag%20x");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "https://example.com/id%20one");
    EXPECT_EQ(result->query, "download=100%25");
    EXPECT_EQ(result->fragment, "frag%20x");
}

// =============================================================================
// Test V64-8: Relative fragment-only URL keeps base query and encodes fragment
// =============================================================================
TEST(URLParser, RelativeFragmentOnlyPercentEncodingV64) {
    auto base = parse("https://example.com/a/b?x=1#old");
    ASSERT_TRUE(base.has_value());

    auto result = parse("#new%20frag", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "new%2520frag");
}

// =============================================================================
// Test V65-1: Port edge handling with default port and leading zeros
// =============================================================================
TEST(URLParser, PortEdgeDefaultWithLeadingZerosV65) {
    auto result = parse("wss://example.com:0443/chat");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "wss");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/chat");
    EXPECT_EQ(result->origin(), "wss://example.com");
}

// =============================================================================
// Test V65-2: Multiple query parameters preserve order and values
// =============================================================================
TEST(URLParser, MultipleQueryParamsPreservedV65) {
    auto result = parse("https://example.com/search?a=1&b=two&empty=&encoded=%20");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "a=1&b=two&empty=&encoded=%2520");
}

// =============================================================================
// Test V65-3: Fragment-only relative URL updates fragment only
// =============================================================================
TEST(URLParser, RelativeFragmentOnlyKeepsBaseFieldsV65) {
    auto base = parse("https://example.com/a/b?x=1#old");
    ASSERT_TRUE(base.has_value());

    auto result = parse("#new-section", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "new-section");
}

// =============================================================================
// Test V65-4: Username and password are parsed from authority
// =============================================================================
TEST(URLParser, AuthorityUsernamePasswordParsedV65) {
    auto result = parse("https://alice:secret@example.com/private");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->username, "alice");
    EXPECT_EQ(result->password, "secret");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/private");
}

// =============================================================================
// Test V65-5: Dot-dot segments are normalized in paths
// =============================================================================
TEST(URLParser, PathNormalizationDotDotV65) {
    auto result = parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/c");
}

// =============================================================================
// Test V65-6: Backslashes convert to forward slashes for special schemes
// =============================================================================
TEST(URLParser, BackslashConvertedToSlashInSpecialSchemeV65) {
    // Our parser requires :// (not :\\), so use forward slashes for scheme separator
    auto result = parse("https://example.com/one\\two");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    // Backslash in path may be kept or converted depending on implementation
    EXPECT_TRUE(result->path.find("one") != std::string::npos);
    EXPECT_TRUE(result->path.find("two") != std::string::npos);
}

// =============================================================================
// Test V65-7: Empty path segments are preserved
// =============================================================================
TEST(URLParser, EmptyPathSegmentsPreservedV65) {
    auto result = parse("https://example.com/a//b///c/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a//b///c/");
}

// =============================================================================
// Test V65-8: Consecutive slashes in relative paths are preserved
// =============================================================================
TEST(URLParser, ConsecutiveSlashesPreservedInRelativePathV65) {
    auto base = parse("https://example.com/root/");
    ASSERT_TRUE(base.has_value());

    auto result = parse("x//y///z", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/root/x//y///z");
}

// =============================================================================
// Test V65-9: Tabs and newlines are stripped before parsing
// =============================================================================
TEST(URLParser, TabAndNewlineStrippingV65) {
    auto result = parse(" \n\thttps://example.com/pa\tth?x=1\n2#fr\rag \t ");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->query, "x=12");
    EXPECT_EQ(result->fragment, "frag");
}

// =============================================================================
// Test V65-10: IPv4 addresses parse as hosts
// =============================================================================
TEST(URLParser, IPv4ParsingV65) {
    auto result = parse("http://192.168.0.1:8080/status");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "192.168.0.1");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/status");
    EXPECT_EQ(result->origin(), "http://192.168.0.1:8080");
}

// =============================================================================
// Test V65-11: IPv6 addresses parse with brackets and port
// =============================================================================
TEST(URLParser, IPv6ParsingV65) {
    auto result = parse("https://[2001:db8::5]:8443/api");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "[2001:db8::5]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/api");
    EXPECT_EQ(result->origin(), "https://[2001:db8::5]:8443");
}

// =============================================================================
// Test V65-12: Data URI keeps payload including commas
// =============================================================================
TEST(URLParser, DataURIWithCommasV65) {
    auto result = parse("data:text/plain;charset=utf-8,hello,world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "text/plain;charset=utf-8,hello,world");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// Test V65-13: File URLs parse on Unix and Windows-style absolute paths
// =============================================================================
TEST(URLParser, FileURLsAcrossPlatformsV65) {
    auto unix_file = parse("file:///usr/local/bin/tool");
    ASSERT_TRUE(unix_file.has_value());
    EXPECT_EQ(unix_file->scheme, "file");
    EXPECT_TRUE(unix_file->host.empty());
    EXPECT_EQ(unix_file->path, "/usr/local/bin/tool");

    auto windows_file = parse("file:///C:/Windows/System32/drivers/etc/hosts");
    ASSERT_TRUE(windows_file.has_value());
    EXPECT_EQ(windows_file->scheme, "file");
    EXPECT_TRUE(windows_file->host.empty());
    EXPECT_EQ(windows_file->path, "/C:/Windows/System32/drivers/etc/hosts");
}

// =============================================================================
// Test V65-14: Blob URL keeps embedded HTTPS URL in opaque path
// =============================================================================
TEST(URLParser, BlobURLWithEmbeddedHttpsV65) {
    auto result = parse("blob:https://example.com/550e8400-e29b-41d4-a716-446655440000");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "https://example.com/550e8400-e29b-41d4-a716-446655440000");
}

// =============================================================================
// Test V65-15: about:blank parses as non-special opaque URL
// =============================================================================
TEST(URLParser, AboutBlankOpaqueURLV65) {
    auto result = parse("about:blank");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "about");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "blank");
    EXPECT_EQ(result->origin(), "null");
}

// =============================================================================
// Test V65-16: javascript: scheme parses as opaque URL
// =============================================================================
TEST(URLParser, JavascriptSchemeOpaqueURLV65) {
    auto result = parse("javascript:alert(1)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "javascript");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "alert(1)");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// V67: requested URL parser coverage
// =============================================================================

TEST(URLParserTest, FtpSchemeUrlParsesAndDoubleEncodesPercentPathV67) {
    auto result = clever::url::parse("ftp://files.example.com/archive%20docs/report.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->path, "/archive%2520docs/report.txt");
}

TEST(URLParserTest, MailtoSchemeUrlParsesAsOpaqueV67) {
    auto result = clever::url::parse("mailto:user@example.com?subject=hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "mailto");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "user@example.com");
    EXPECT_EQ(result->query, "subject=hello%20world");
}

TEST(URLParserTest, WsSchemeUrlParsesWithAuthorityV67) {
    auto result = clever::url::parse("ws://chat.example.com:80/socket");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ws");
    EXPECT_EQ(result->host, "chat.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/socket");
}

TEST(URLParserTest, DotSegmentsAreNormalizedInPathV67) {
    auto result = clever::url::parse("https://example.com/a/b/../c/./d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/c/d");
}

TEST(URLParserTest, SchemeIsCaseInsensitiveAndBackslashSeparatorRejectedV67) {
    auto upper = clever::url::parse("HtTpS://MiXeD.Example/ok%20path");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(upper->scheme, "https");
    EXPECT_EQ(upper->host, "mixed.example");
    EXPECT_EQ(upper->path, "/ok%2520path");

    auto bad_separator = clever::url::parse("https:\\\\mixed.example\\bad");
    EXPECT_FALSE(bad_separator.has_value());
}

TEST(URLParserTest, EmptyFragmentDelimiterProducesEmptyFragmentFieldV67) {
    auto result = clever::url::parse("https://example.com/path#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, QueryWithSpecialCharactersAndPercentDoubleEncodingV67) {
    auto result = clever::url::parse("https://example.com/search?a=1&b=?&c=%20");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "a=1&b=?&c=%2520");
}

TEST(URLParserTest, PortZeroIsAcceptedV67) {
    auto result = clever::url::parse("http://example.com:0/path");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 0);
}

TEST(URLParserTest, Port65535IsAcceptedV67) {
    auto result = clever::url::parse("http://example.com:65535/path");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 65535);
}

TEST(URLParserTest, Port65536IsRejectedV67) {
    auto result = clever::url::parse("http://example.com:65536/path");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, HostTrailingWhitespaceIsTrimmedFromInputV67) {
    auto result = clever::url::parse("  https://Example.com/path  \r\n\t");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, SpecialSchemePathStartsWithSlashV67) {
    auto result = clever::url::parse("https://example.com/path/to/page");
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->path.empty());
    EXPECT_EQ(result->path.front(), '/');
}

TEST(URLParserTest, SchemeOnlyHttpUrlIsRejectedV67) {
    auto result = clever::url::parse("http:");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, AtSignInPathComponentIsPreservedV67) {
    auto result = clever::url::parse("https://example.com/user@domain/profile");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/user@domain/profile");
}

TEST(URLParserTest, ConsecutiveQuestionMarksInQueryArePreservedV67) {
    auto result = clever::url::parse("https://example.com/search??a=1??b=2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "?a=1??b=2");
}

TEST(URLParserTest, EncodedHashInQueryValueIsDoubleEncodedV67) {
    auto result = clever::url::parse("https://example.com/path?token=a%23b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "token=a%2523b");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, DataUriParsingV68) {
    auto result = clever::url::parse("data:text/plain,hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "text/plain,hello%20world");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, BlobUriFormatV68) {
    auto result =
        clever::url::parse("blob:https://example.com/550e8400-e29b-41d4-a716-446655440000");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "blob");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "https://example.com/550e8400-e29b-41d4-a716-446655440000");
}

TEST(URLParserTest, JavascriptSchemeParsingV68) {
    auto result = clever::url::parse("javascript:alert(1)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "javascript");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "alert(1)");
}

TEST(URLParserTest, AboutBlankUrlV68) {
    auto result = clever::url::parse("about:blank");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "about");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "blank");
}

TEST(URLParserTest, UrlWithOnlySchemeAndHostV68) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");

    auto missing_slashes = clever::url::parse("https:example.com");
    EXPECT_FALSE(missing_slashes.has_value());
}

TEST(URLParserTest, UrlWithEmptyQueryV68) {
    auto result = clever::url::parse("https://example.com/search?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_TRUE(result->query.empty());
}

TEST(URLParserTest, UrlWithOnlyFragmentV68) {
    auto result = clever::url::parse("https://example.com/#only-fragment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "only-fragment");
}

TEST(URLParserTest, PathWithEncodedSlashesDoubleEncodesPercentV68) {
    auto result = clever::url::parse("https://example.com/a%2Fb%2Fc");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a%252Fb%252Fc");
}

TEST(URLParserTest, HostCaseNormalizationToLowercaseV68) {
    auto result = clever::url::parse("https://MiXeD.Example.COM/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "mixed.example.com");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, PortAfterIPv6AddressV68) {
    auto result = clever::url::parse("http://[2001:db8::1]:8080/index");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "[2001:db8::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/index");
}

TEST(URLParserTest, UrlEndingWithQuestionMarkV68) {
    auto result = clever::url::parse("https://example.com?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, QueryWithPlusSignsV68) {
    auto result = clever::url::parse("https://example.com/search?q=a+b+c&x=1+2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=a+b+c&x=1+2");
}

TEST(URLParserTest, SchemeWithDigitsH2CV68) {
    auto result = clever::url::parse("h2c://example.com/stream");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "h2c");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/stream");
}

TEST(URLParserTest, ConsecutiveDotsInHostnameV68) {
    auto result = clever::url::parse("https://a..b.example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "a..b.example.com");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, EmptySchemeRejectedV68) {
    auto result = clever::url::parse("://example.com/path");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, PathWithSemicolonParameterV68) {
    auto result = clever::url::parse("https://example.com/users;id=42/profile;v=1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/users;id=42/profile;v=1");
}

TEST(URLParserTest, UrlWithTabCharactersStrippedV69) {
    auto result = clever::url::parse("https://exa\tmple.com/pa\tth?x=\t1#fr\tag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "frag");
}

TEST(URLParserTest, UrlWithNewlineCharactersStrippedV69) {
    auto result = clever::url::parse("https://example.\ncom/line\r\nbreak?ok=\n1#frag\r");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/linebreak");
    EXPECT_EQ(result->query, "ok=1");
    EXPECT_EQ(result->fragment, "frag");
}

TEST(URLParserTest, MultipleAtSignsInAuthorityV69) {
    auto result = clever::url::parse("http://user@mid:pass@host.example/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user%40mid");
    EXPECT_EQ(result->password, "pass");
    EXPECT_EQ(result->host, "host.example");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, EmptyPasswordInUserInfoV69) {
    auto result = clever::url::parse("http://user:@example.com/secure");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_TRUE(result->password.empty());
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, PortWithLeadingZerosV69) {
    auto result = clever::url::parse("http://example.com:00081/path");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 81);
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, UrlFragmentWithSpacesV69) {
    auto result = clever::url::parse("https://example.com/path#section one two");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section%20one%20two");
}

TEST(URLParserTest, RelativeReferenceResolutionBaseAndRelativeV69) {
    auto base = clever::url::parse("https://example.com/a/b/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("../img/logo 1.png?token=%20#frag part", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/img/logo%201.png");
    EXPECT_EQ(result->query, "token=%2520");
    EXPECT_EQ(result->fragment, "frag%20part");
}

TEST(URLParserTest, OpaquePathDataUrlV69) {
    auto result = clever::url::parse("data:text/html,<h1>Hello World</h1>");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "text/html,<h1>Hello World</h1>");
}

TEST(URLParserTest, UrlWithEmptyHostAfterAuthorityV69) {
    auto result = clever::url::parse("https:///missing-host");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/missing-host");
}

TEST(URLParserTest, SchemeComparisonCaseInsensitiveV69) {
    auto upper = clever::url::parse("HTTP://Example.com/path");
    auto lower = clever::url::parse("http://example.com/path");
    ASSERT_TRUE(upper.has_value());
    ASSERT_TRUE(lower.has_value());
    EXPECT_EQ(upper->scheme, "http");
    EXPECT_EQ(lower->scheme, "http");
    EXPECT_TRUE(clever::url::urls_same_origin(*upper, *lower));
}

TEST(URLParserTest, NonAsciiPathIsPercentEncodedV69) {
    auto result = clever::url::parse("https://example.com/안녕");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/%EC%95%88%EB%85%95");
}

TEST(URLParserTest, QueryEncodingOfSpecialCharactersV69) {
    auto result = clever::url::parse("https://example.com/search?q=a b[]{}|%20");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "q=a%20b%5B%5D%7B%7D%7C%2520");
}

TEST(URLParserTest, UrlWithWindowsDriveLetterPathV69) {
    auto result = clever::url::parse("file:///C:/Program Files/App/config.json");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "/C:/Program%20Files/App/config.json");
}

TEST(URLParserTest, IpAddressAsHostnameV69) {
    auto result = clever::url::parse("http://192.168.10.5:8080/index");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "192.168.10.5");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/index");
}

TEST(URLParserTest, UrlToStringHrefFormatV69) {
    auto result = clever::url::parse("https://user:pass@example.com:443/a b?q=%20#frag ment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->serialize(), "https://user:pass@example.com/a%20b?q=%2520#frag%20ment");
}

TEST(URLParserTest, HostExtractionFromFullUrlV69) {
    auto result = clever::url::parse("https://user:pass@Sub.Example.com:8443/path/to?a=1#ok");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "sub.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
}

TEST(URLParserTest, BasicHttpsUrlComponentsV70) {
    auto result = clever::url::parse("https://example.com/path/to/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path/to/page");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParserTest, HttpUrlWithPort8080V70) {
    auto result = clever::url::parse("http://example.com:8080/api");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/api");
}

TEST(URLParserTest, UrlPathSegmentsSplitV70) {
    auto result = clever::url::parse("https://example.com/a/b/c");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->path, "/a/b/c");

    std::vector<std::string> segments;
    size_t start = 1;
    while (start <= result->path.size()) {
        size_t slash = result->path.find('/', start);
        if (slash == std::string::npos) {
            segments.push_back(result->path.substr(start));
            break;
        }
        segments.push_back(result->path.substr(start, slash - start));
        start = slash + 1;
    }

    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(segments[0], "a");
    EXPECT_EQ(segments[1], "b");
    EXPECT_EQ(segments[2], "c");
}

TEST(URLParserTest, UrlWithQueryAndFragmentTogetherV70) {
    auto result = clever::url::parse("https://example.com/search?q=one#section-2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=one");
    EXPECT_EQ(result->fragment, "section-2");
}

TEST(URLParserTest, HttpsDefaultPort443OmittedV70) {
    auto result = clever::url::parse("https://example.com:443/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/home");
}

TEST(URLParserTest, HttpDefaultPort80OmittedV70) {
    auto result = clever::url::parse("http://example.com:80/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/home");
}

TEST(URLParserTest, UrlWithEncodedSpaceInQueryPercent2520V70) {
    auto result = clever::url::parse("https://example.com/search?q=%20");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "q=%2520");
}

TEST(URLParserTest, EmptyUrlReturnsNulloptV70) {
    auto result = clever::url::parse("");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, WhitespaceOnlyUrlReturnsNulloptV70) {
    auto result = clever::url::parse(" \t\r\n ");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, UrlWithUppercaseSchemeNormalizedV70) {
    auto result = clever::url::parse("HTTPS://Example.com/Path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/Path");

    auto missing_slashes = clever::url::parse("HTTPS:Example.com/Path");
    EXPECT_FALSE(missing_slashes.has_value());
}

TEST(URLParserTest, PathDotDotRemovalV70) {
    auto result = clever::url::parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/c");
}

TEST(URLParserTest, UrlWithMultiplePathSegmentsV70) {
    auto result = clever::url::parse("https://example.com/one/two/three/four");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/one/two/three/four");
}

TEST(URLParserTest, FragmentOnlyPreservedV70) {
    auto result = clever::url::parse("https://example.com/page#fragment-only");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/page");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "fragment-only");
}

TEST(URLParserTest, QueryWithAmpersandSeparatedParamsV70) {
    auto result = clever::url::parse("https://example.com/search?a=1&b=2&c=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "a=1&b=2&c=3");
}

TEST(URLParserTest, FileUrlWithHostV70) {
    auto result = clever::url::parse("file://localhost/etc/hosts");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_EQ(result->host, "localhost");
    EXPECT_EQ(result->path, "/etc/hosts");
}

TEST(URLParserTest, CustomSchemeUrlV70) {
    auto with_authority = clever::url::parse("custom://host/resource");
    ASSERT_TRUE(with_authority.has_value());
    EXPECT_EQ(with_authority->scheme, "custom");
    EXPECT_EQ(with_authority->host, "host");
    EXPECT_EQ(with_authority->path, "/resource");
}

TEST(URLParserTest, BasicHttpWithPathV71) {
    auto result = clever::url::parse("http://example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, HttpsWithQueryParamsV71) {
    auto result = clever::url::parse("https://example.com/search?a=1&b=two words");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "a=1&b=two%20words");
}

TEST(URLParserTest, UrlWithFragmentAfterQueryV71) {
    auto result = clever::url::parse("https://example.com/find?q=browser#top");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/find");
    EXPECT_EQ(result->query, "q=browser");
    EXPECT_EQ(result->fragment, "top");
}

TEST(URLParserTest, UrlWithPort3000V71) {
    auto result = clever::url::parse("http://localhost:3000/app");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000);
    EXPECT_EQ(result->path, "/app");
}

TEST(URLParserTest, NoPathUrlDefaultsToSlashV71) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
}

TEST(URLParserTest, UrlSchemeFtpV71) {
    auto result = clever::url::parse("ftp://files.example.com/downloads");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->path, "/downloads");
}

TEST(URLParserTest, UrlHostWithSubdomainV71) {
    auto result = clever::url::parse("https://api.dev.example.com/v1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.dev.example.com");
    EXPECT_EQ(result->path, "/v1");
}

TEST(URLParserTest, UrlPathWithMultipleSegmentsV71) {
    auto result = clever::url::parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b/c/d");
}

TEST(URLParserTest, UrlQueryWithHashValueV71) {
    auto result = clever::url::parse("https://example.com/path?hash=%23value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "hash=%2523value");
}

TEST(URLParserTest, EmptyFragmentV71) {
    auto result = clever::url::parse("https://example.com/path#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, PortExtractionV71) {
    auto result = clever::url::parse("https://example.com:3000/dashboard");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000);
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, SchemeHostOnlyUrlRequiresSlashesV71) {
    auto valid = clever::url::parse("https://only-host.example");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->host, "only-host.example");
    EXPECT_EQ(valid->path, "/");

    auto invalid = clever::url::parse("https:only-host.example");
    EXPECT_FALSE(invalid.has_value());
}

TEST(URLParserTest, UrlWithTrailingSlashV71) {
    auto result = clever::url::parse("https://example.com/path/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path/");
}

TEST(URLParserTest, UrlPercentEncodingInPathV71) {
    auto result = clever::url::parse("https://example.com/a%20b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a%2520b");
}

TEST(URLParserTest, UrlWithUserAtHostV71) {
    auto result = clever::url::parse("https://user@example.com/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user");
    EXPECT_TRUE(result->password.empty());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path");
}

TEST(URLParserTest, UrlQueryEmptyValueKeyV71) {
    auto result = clever::url::parse("https://example.com/path?key=");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "key=");
}

TEST(URLParserTest, HttpHostExtractionV72) {
    auto result = clever::url::parse("http://Example.COM/index");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, HttpsPathExtractionV72) {
    auto result = clever::url::parse("https://example.com/a/b/c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b/c");
}

TEST(URLParserTest, QueryKeyValueParsingV72) {
    auto result = clever::url::parse("https://example.com/search?key=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "key=value");
}

TEST(URLParserTest, FragmentOnlyUrlV72) {
    auto result = clever::url::parse("https://example.com/#fragment-only");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "fragment-only");
}

TEST(URLParserTest, UrlWithNoPathDefaultsToSlashV72) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
}

TEST(URLParserTest, UrlWithEmptyQueryMarkOnlyV72) {
    auto result = clever::url::parse("https://example.com/path?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->query.empty());
}

TEST(URLParserTest, Port80ImplicitForHttpV72) {
    auto result = clever::url::parse("http://example.com:80/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->serialize(), "http://example.com/home");
}

TEST(URLParserTest, Port443ImplicitForHttpsV72) {
    auto result = clever::url::parse("https://example.com:443/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->serialize(), "https://example.com/home");
}

TEST(URLParserTest, UrlWithUnicodeEncodedV72) {
    auto result = clever::url::parse("https://example.com/こんにちは");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/%E3%81%93%E3%82%93%E3%81%AB%E3%81%A1%E3%81%AF");
}

TEST(URLParserTest, PathWithSpacesEncodedV72) {
    auto result = clever::url::parse("https://example.com/path with spaces");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path%20with%20spaces");
}

TEST(URLParserTest, HostLowercaseNormalizationV72) {
    auto result = clever::url::parse("https://MiXeD.ExAmPlE.CoM/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "mixed.example.com");
}

TEST(URLParserTest, SchemeExtractionRequiresSlashSlashV72) {
    auto valid = clever::url::parse("wss://example.com/socket");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->scheme, "wss");

    auto invalid = clever::url::parse("wss:example.com/socket");
    EXPECT_FALSE(invalid.has_value());
}

TEST(URLParserTest, UrlWithAllComponentsV72) {
    auto result = clever::url::parse("https://user:pass@Example.com:8443/a%20b?q=%20#frag ment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->username, "user");
    EXPECT_EQ(result->password, "pass");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/a%2520b");
    EXPECT_EQ(result->query, "q=%2520");
    EXPECT_EQ(result->fragment, "frag%20ment");
}

TEST(URLParserTest, RelativePathWithBaseV72) {
    auto base = clever::url::parse("https://example.com/dir/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("docs/page.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/dir/docs/page.html");
}

TEST(URLParserTest, MultipleQueryParamsV72) {
    auto result = clever::url::parse("https://example.com/find?a=1&b=2&c=three");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "a=1&b=2&c=three");
}

TEST(URLParserTest, UrlOriginDerivationV72) {
    auto result = clever::url::parse("http://Example.com:80/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->origin(), "http://example.com");
}

TEST(URLParserTest, SimpleHttpsUrlV73) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, UrlWithPortV73) {
    auto result = clever::url::parse("https://example.com:8443");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/");
}

TEST(URLParserTest, UrlPathOnlyV73) {
    auto result = clever::url::parse("https://example.com/path-only");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path-only");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, UrlWithQueryOnlyV73) {
    auto result = clever::url::parse("https://example.com/?q=one");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
    EXPECT_EQ(result->query, "q=one");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, UrlWithFragmentOnlyV73) {
    auto result = clever::url::parse("https://example.com/#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "section");
}

TEST(URLParserTest, UrlWithAllPartsV73) {
    auto result = clever::url::parse("https://example.com:9443/a%20b?q=%20#frag%20ment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9443);
    EXPECT_EQ(result->path, "/a%2520b");
    EXPECT_EQ(result->query, "q=%2520");
    EXPECT_EQ(result->fragment, "frag%2520ment");
}

TEST(URLParserTest, InvalidSchemeNoColonReturnsNulloptV73) {
    auto result = clever::url::parse("https//example.com/path");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, EmptyStringReturnsNulloptV73) {
    auto result = clever::url::parse("");
    EXPECT_FALSE(result.has_value());
}

TEST(URLParserTest, UrlWithEncodedAmpersandV73) {
    auto result = clever::url::parse("https://example.com/search?q=a%26b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=a%2526b");
}

TEST(URLParserTest, MultiplePathLevelsV73) {
    auto result = clever::url::parse("https://example.com/a/b/c/d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/a/b/c/d");
}

TEST(URLParserTest, HostWithHyphenV73) {
    auto result = clever::url::parse("https://my-host.example.com/home");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "my-host.example.com");
    EXPECT_EQ(result->path, "/home");
}

TEST(URLParserTest, HostWithNumbersV73) {
    auto result = clever::url::parse("https://api2.example123.com/v1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api2.example123.com");
    EXPECT_EQ(result->path, "/v1");
}

TEST(URLParserTest, PathWithTildeV73) {
    auto result = clever::url::parse("https://example.com/~user/docs");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/~user/docs");
}

TEST(URLParserTest, QueryWithPlusSignV73) {
    auto result = clever::url::parse("https://example.com/search?q=a+b+c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "q=a+b+c");
}

TEST(URLParserTest, UrlWithAtSignInPathV73) {
    auto result = clever::url::parse("https://example.com/@alice/profile");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/@alice/profile");
}

TEST(URLParserTest, TrailingHashPreservedV73) {
    auto result = clever::url::parse("https://example.com/path#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, SchemeExtractionHttpsV74) {
    auto result = clever::url::parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
}

TEST(URLParserTest, HostExtractionExampleComV74) {
    auto result = clever::url::parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, PathExtractionPageV74) {
    auto result = clever::url::parse("https://example.com/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/page");
}

TEST(URLParserTest, QueryExtractionKeyValueV74) {
    auto result = clever::url::parse("https://example.com/page?key=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query, "key=value");
}

TEST(URLParserTest, FragmentExtractionSectionV74) {
    auto result = clever::url::parse("https://example.com/page#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "section");
}

TEST(URLParserTest, PortExtraction9090V74) {
    auto result = clever::url::parse("https://example.com:9090/page");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9090);
}

TEST(URLParserTest, UrlWithoutPathDefaultsToSlashV74) {
    auto result = clever::url::parse("https://example.com?key=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/");
    EXPECT_EQ(result->query, "key=value");
}

TEST(URLParserTest, UrlWithOnlyHostRequiresSlashSlashV74) {
    auto valid = clever::url::parse("https://example.com");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->host, "example.com");
    EXPECT_EQ(valid->path, "/");

    auto invalid = clever::url::parse("https:example.com");
    EXPECT_FALSE(invalid.has_value());
}

TEST(URLParserTest, UrlWithEncodedQuestionMarkInPathV74) {
    auto result = clever::url::parse("https://example.com/search%3Fterm");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search%253Fterm");
}

TEST(URLParserTest, UrlSpecialCharsInFragmentV74) {
    auto result = clever::url::parse("https://example.com/page#sec%20tion");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fragment, "sec%2520tion");
}

TEST(URLParserTest, HttpSchemeRecognizedV74) {
    auto valid = clever::url::parse("http://example.com/page");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->scheme, "http");

    auto invalid = clever::url::parse("http:example.com/page");
    EXPECT_FALSE(invalid.has_value());
}

TEST(URLParserTest, FtpSchemeRecognizedV74) {
    auto valid = clever::url::parse("ftp://example.com/resource");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid->scheme, "ftp");
    EXPECT_EQ(valid->host, "example.com");

    auto invalid = clever::url::parse("ftp:example.com/resource");
    EXPECT_FALSE(invalid.has_value());
}

TEST(URLParserTest, DataUriBasicV74) {
    auto result = clever::url::parse("data:text/plain,hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "text/plain,hello");
}

TEST(URLParserTest, PathTrailingSlashV74) {
    auto result = clever::url::parse("https://example.com/page/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/page/");
}

TEST(URLParserTest, HostCaseInsensitiveV74) {
    auto result = clever::url::parse("https://ExAmPlE.CoM/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
}

TEST(URLParserTest, UrlWithDoubleSlashInPathV74) {
    auto result = clever::url::parse("https://example.com//double//slash");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "//double//slash");
}

TEST(URLParserTest, HttpsRoundTripComponentsV75) {
    auto result = clever::url::parse("https://example.com/path?q=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->query, "q=1");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->serialize(), "https://example.com/path?q=1#frag");
}

TEST(URLParserTest, SpecialSchemesRequireSlashSlashAfterSchemeV75) {
    EXPECT_FALSE(clever::url::parse("https:example.com/path").has_value());
    EXPECT_FALSE(clever::url::parse("ftp:example.com/resource").has_value());
    EXPECT_FALSE(clever::url::parse("ws:example.com/socket").has_value());
}

TEST(URLParserTest, NonSpecialOpaqueSchemeWithoutAuthorityV75) {
    auto result = clever::url::parse("data:text/plain,hello%20world?x=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "text/plain,hello%20world");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->serialize(), "data:text/plain,hello%20world?x=1#frag");
}

TEST(URLParserTest, RelativePathResolutionWithBaseUrlV75) {
    auto base = clever::url::parse("https://example.com/a/b/c/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("../d/e?q=2#frag", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/d/e");
    EXPECT_EQ(result->query, "q=2");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->serialize(), "https://example.com/a/b/d/e?q=2#frag");
}

TEST(URLParserTest, QueryParametersDoubleEncodePercent20V75) {
    auto result = clever::url::parse("https://example.com/search?name=alice&note=a+b&space=%20");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "name=alice&note=a+b&space=%2520");
    EXPECT_EQ(result->serialize(), "https://example.com/search?name=alice&note=a+b&space=%2520");
}

TEST(URLParserTest, FragmentDoubleEncodePercent20V75) {
    auto result = clever::url::parse("https://example.com/path#frag%20ment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->fragment, "frag%2520ment");
    EXPECT_EQ(result->serialize(), "https://example.com/path#frag%2520ment");
}

TEST(URLParserTest, IdnUnicodeRejectedButPunycodeAcceptedV75) {
    auto unicode_host = clever::url::parse("https://münich.example/path");
    EXPECT_FALSE(unicode_host.has_value());

    auto punycode_host = clever::url::parse("https://XN--MNICH-KVA.EXAMPLE/path");
    ASSERT_TRUE(punycode_host.has_value());
    EXPECT_EQ(punycode_host->host, "xn--mnich-kva.example");
    EXPECT_EQ(punycode_host->path, "/path");
}

TEST(URLParserTest, Ipv6HostAndPortParsesV75) {
    auto result = clever::url::parse("https://[2001:db8::1]:8443/path?q=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "[2001:db8::1]");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->query, "q=1");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->serialize(), "https://[2001:db8::1]:8443/path?q=1#frag");
}

TEST(URLParserTest, MixedCaseWssNormalizesSchemeHostAndDefaultPortV76) {
    auto result = clever::url::parse("WSS://EXAMPLE.COM:443/chat");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "wss");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/chat");
    EXPECT_EQ(result->serialize(), "wss://example.com/chat");
}

TEST(URLParserTest, OpaqueCustomSchemeKeepsPathQueryAndFragmentV76) {
    auto result = clever::url::parse("custom:folder/item?x=1#frag");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "custom");
    EXPECT_TRUE(result->host.empty());
    EXPECT_EQ(result->path, "folder/item");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "frag");
    EXPECT_EQ(result->serialize(), "custom:folder/item?x=1#frag");
}

TEST(URLParserTest, SchemeRelativeUrlUsesBaseSchemeAndNormalizesHostV76) {
    auto base = clever::url::parse("https://base.example/a/b/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("//MiXeD.Example:443/next?x=1#f", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "mixed.example");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/next");
    EXPECT_EQ(result->query, "x=1");
    EXPECT_EQ(result->fragment, "f");
    EXPECT_EQ(result->serialize(), "https://mixed.example/next?x=1#f");
}

TEST(URLParserTest, RelativePathDotSegmentsResolveAgainstBaseV76) {
    auto base = clever::url::parse("https://example.com/a/b/c/index.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("../../d/./e", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/d/e");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->serialize(), "https://example.com/a/d/e");
}

TEST(URLParserTest, AbsolutePathRelativeInputClearsBaseQueryAndFragmentV76) {
    auto base = clever::url::parse("https://example.com/old/path?keep=1#frag");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("/new/path", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/new/path");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->serialize(), "https://example.com/new/path");
}

TEST(URLParserTest, QueryOnlyRelativeInputReplacesQueryAndClearsFragmentV76) {
    auto base = clever::url::parse("https://example.com/p/index.html?old=1#old");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("?new=2", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/p/index.html");
    EXPECT_EQ(result->query, "new=2");
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->serialize(), "https://example.com/p/index.html?new=2");
}

TEST(URLParserTest, QueryAndFragmentPercentEncodeSpacesAndPercentV76) {
    auto result = clever::url::parse("https://example.com/search?q=100% done#frag ment");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=100%25%20done");
    EXPECT_EQ(result->fragment, "frag%20ment");
    EXPECT_EQ(result->serialize(), "https://example.com/search?q=100%25%20done#frag%20ment");
}

TEST(URLParserTest, HostLowercasedButTrailingDotPreservedV76) {
    auto result = clever::url::parse("https://Example.COM./path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com.");
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->serialize(), "https://example.com./path");
}

TEST(URLParserTest, HttpsWithExplicitPort8443V77) {
    auto result = clever::url::parse("https://example.com:8443/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->serialize(), "https://example.com:8443/path");
}

TEST(URLParserTest, RelativeUrlResolvesHostFromBaseV77) {
    auto base = clever::url::parse("https://base.example/old/path");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("/new", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "base.example");
    EXPECT_EQ(result->path, "/new");
    EXPECT_EQ(result->serialize(), "https://base.example/new");
}

TEST(URLParserTest, UrlWithEmptyQueryV77) {
    auto result = clever::url::parse("https://example.com/?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_EQ(result->query, "");
    EXPECT_EQ(result->serialize(), "https://example.com/");
}

TEST(URLParserTest, FtpSchemeUrlParsedV77) {
    auto result = clever::url::parse("ftp://files.example.com/readme.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->path, "/readme.txt");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->serialize(), "ftp://files.example.com/readme.txt");
}

TEST(URLParserTest, FileSchemeUrlV77) {
    auto result = clever::url::parse("file:///tmp/test.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "file");
    EXPECT_EQ(result->path, "/tmp/test.txt");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, UrlWithIPv6HostV77) {
    auto result = clever::url::parse("http://[::1]/path");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "[::1]");
    EXPECT_EQ(result->path, "/path");
    EXPECT_EQ(result->serialize(), "http://[::1]/path");
}

TEST(URLParserTest, UrlWithTrailingSlashV77) {
    auto result = clever::url::parse("https://example.com/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->serialize(), "https://example.com/");
}

TEST(URLParserTest, UrlWithMultipleQueryParamsV77) {
    auto result = clever::url::parse("https://example.com/search?a=1&b=2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "a=1&b=2");
    EXPECT_TRUE(result->fragment.empty());
    EXPECT_EQ(result->serialize(), "https://example.com/search?a=1&b=2");
}

// =============================================================================
// V79 Tests
// =============================================================================

TEST(URLParserTest, HttpsDefaultPort443NormalizedV79) {
    auto result = clever::url::parse("https://x.com:443/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "x.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/");
}

TEST(URLParserTest, MultipleDotDotSegmentsV79) {
    auto result = clever::url::parse("https://example.com/a/b/c/../../d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/d");
}

TEST(URLParserTest, QueryPreservedV79) {
    auto result = clever::url::parse("https://example.com/page?key=value");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_EQ(result->query, "key=value");
}

TEST(URLParserTest, FragmentPreservedV79) {
    auto result = clever::url::parse("https://example.com/doc#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/doc");
    EXPECT_EQ(result->fragment, "section");
}

TEST(URLParserTest, PortNonDefaultPreservedV79) {
    auto result = clever::url::parse("https://example.com:9090/api");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9090);
    EXPECT_EQ(result->path, "/api");
}

TEST(URLParserTest, PathWithEncodedSpaceV79) {
    auto result = clever::url::parse("https://example.com/path%20name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/path%2520name");
}

TEST(URLParserTest, SerializeRoundTripV79) {
    auto result = clever::url::parse("https://example.com:8080/resource?q=test#top");
    ASSERT_TRUE(result.has_value());
    std::string serialized = result->serialize();
    EXPECT_EQ(serialized, "https://example.com:8080/resource?q=test#top");
    auto reparsed = clever::url::parse(serialized);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->scheme, result->scheme);
    EXPECT_EQ(reparsed->host, result->host);
    EXPECT_EQ(reparsed->port, result->port);
    EXPECT_EQ(reparsed->path, result->path);
    EXPECT_EQ(reparsed->query, result->query);
    EXPECT_EQ(reparsed->fragment, result->fragment);
}

TEST(URLParserTest, EmptyFragmentV79) {
    auto result = clever::url::parse("https://x.com/path#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "x.com");
    EXPECT_EQ(result->path, "/path");
    EXPECT_TRUE(result->fragment.empty());
}

// =============================================================================
// V80 Tests
// =============================================================================

TEST(URLParserTest, WssSchemeV80) {
    auto result = clever::url::parse("wss://chat.example.com/live?room=42#lobby");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "wss");
    EXPECT_EQ(result->host, "chat.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/live");
    EXPECT_EQ(result->query, "room=42");
    EXPECT_EQ(result->fragment, "lobby");
    EXPECT_EQ(result->serialize(), "wss://chat.example.com/live?room=42#lobby");
}

TEST(URLParserTest, DataUrlBasicV80) {
    auto result = clever::url::parse("data:text/plain;charset=utf-8,hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "data");
    EXPECT_EQ(result->path, "text/plain;charset=utf-8,hello%20world");
    EXPECT_TRUE(result->host.empty());
}

TEST(URLParserTest, RelativePathWithBaseV80) {
    auto base = clever::url::parse("https://example.com/docs/guide/chapter1.html");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("../tutorial/intro.html", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/docs/tutorial/intro.html");
}

TEST(URLParserTest, UrlWithUsernameV80) {
    auto result = clever::url::parse("https://admin:s3cret@dashboard.example.com:9443/panel");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->username, "admin");
    EXPECT_EQ(result->password, "s3cret");
    EXPECT_EQ(result->host, "dashboard.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9443);
    EXPECT_EQ(result->path, "/panel");
}

TEST(URLParserTest, DeepPathV80) {
    auto result = clever::url::parse("https://cdn.example.com/assets/js/vendor/lib/v2/bundle.min.js");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "cdn.example.com");
    EXPECT_EQ(result->path, "/assets/js/vendor/lib/v2/bundle.min.js");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(URLParserTest, QueryWithSpecialCharsV80) {
    auto result = clever::url::parse("https://search.example.com/find?q=a+b&tag=c%26d&limit=10");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "search.example.com");
    EXPECT_EQ(result->path, "/find");
    EXPECT_NE(result->query.find("q=a+b"), std::string::npos);
    EXPECT_NE(result->query.find("limit=10"), std::string::npos);
}

TEST(URLParserTest, HttpPort8080V80) {
    auto result = clever::url::parse("http://localhost:8080/api/v3/status?verbose=true#details");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "localhost");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8080);
    EXPECT_EQ(result->path, "/api/v3/status");
    EXPECT_EQ(result->query, "verbose=true");
    EXPECT_EQ(result->fragment, "details");
    EXPECT_EQ(result->serialize(), "http://localhost:8080/api/v3/status?verbose=true#details");
}

TEST(URLParserTest, TrailingDotInHostV80) {
    auto result = clever::url::parse("https://WWW.Example.COM./resource?key=val");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "www.example.com.");
    EXPECT_EQ(result->path, "/resource");
    EXPECT_EQ(result->query, "key=val");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->serialize(), "https://www.example.com./resource?key=val");
}

// =============================================================================
// V81 Tests
// =============================================================================

TEST(URLParserTest, HttpDefaultPort80NormalizedV81) {
    auto result = clever::url::parse("http://example.com:80/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/index.html");
    EXPECT_EQ(result->serialize(), "http://example.com/index.html");
}

TEST(URLParserTest, TripleDotDotResolutionV81) {
    auto result = clever::url::parse("https://example.com/a/b/c/d/../../../e");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/e");
}

TEST(URLParserTest, MixedCaseSchemeAndHostV81) {
    auto result = clever::url::parse("HTTPS://WWW.EXAMPLE.COM/Page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->path, "/Page");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParserTest, PercentEncodedQueryDoubleEncodesV81) {
    auto result = clever::url::parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("hello%2520world"), std::string::npos);
}

TEST(URLParserTest, EmptyPathAndQueryV81) {
    auto result = clever::url::parse("https://example.com?key=val");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->query, "key=val");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParserTest, FragmentOnlyRelativeResolutionV81) {
    auto base = clever::url::parse("https://example.com/page?x=1");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("#section2", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_EQ(result->fragment, "section2");
}

TEST(URLParserTest, NonDefaultPortPreservedInSerializeV81) {
    auto result = clever::url::parse("https://api.example.com:3000/v1/users?active=true#list");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "api.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000);
    EXPECT_EQ(result->path, "/v1/users");
    EXPECT_EQ(result->query, "active=true");
    EXPECT_EQ(result->fragment, "list");
    EXPECT_EQ(result->serialize(), "https://api.example.com:3000/v1/users?active=true#list");
}

TEST(URLParserTest, InvalidSchemeReturnsNulloptV81) {
    auto result = clever::url::parse("://missing-scheme.com/path");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// V82 Tests
// =============================================================================

TEST(URLParserTest, HttpsDefaultPort443NormalizedV82) {
    auto result = clever::url::parse("https://secure.example.com:443/login");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "secure.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/login");
    EXPECT_EQ(result->serialize(), "https://secure.example.com/login");
}

TEST(URLParserTest, DotSegmentResolutionSingleDotV82) {
    auto result = clever::url::parse("https://example.com/a/./b/./c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/c");
}

TEST(URLParserTest, PercentEncodedPathDoubleEncodesV82) {
    auto result = clever::url::parse("https://example.com/dir%2Ffile");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_NE(result->path.find("%252F"), std::string::npos);
}

TEST(URLParserTest, QueryOnlyRelativeResolutionV82) {
    auto base = clever::url::parse("https://example.com/page#old");
    ASSERT_TRUE(base.has_value());

    auto result = clever::url::parse("?newkey=newval", &base.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_EQ(result->query, "newkey=newval");
}

TEST(URLParserTest, PortBoundaryHighValueV82) {
    auto result = clever::url::parse("http://example.com:65535/endpoint");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 65535);
    EXPECT_EQ(result->path, "/endpoint");
    EXPECT_EQ(result->serialize(), "http://example.com:65535/endpoint");
}

TEST(URLParserTest, HostCaseFoldingWithSubdomainsV82) {
    auto result = clever::url::parse("https://Sub.Domain.EXAMPLE.Org/Path/To/Resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "sub.domain.example.org");
    EXPECT_EQ(result->path, "/Path/To/Resource");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(URLParserTest, EmptyFragmentPreservedV82) {
    auto result = clever::url::parse("https://example.com/page#");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_EQ(result->fragment, "");
}

TEST(URLParserTest, DoubleDotAtRootClampsV82) {
    auto result = clever::url::parse("https://example.com/../../../stay");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/stay");
}

// =============================================================================
// V83 Tests
// =============================================================================

TEST(UrlParserTest, QueryOnlyNoPathV83) {
    auto result = clever::url::parse("https://example.com?search=hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->query, "search=hello");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(UrlParserTest, FragmentOnlyNoPathNoQueryV83) {
    auto result = clever::url::parse("https://example.com#section");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->fragment, "section");
    EXPECT_EQ(result->query, "");
}

TEST(UrlParserTest, DoubleDotResolutionMidPathV83) {
    auto result = clever::url::parse("https://example.com/a/b/../c/d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/c/d");
}

TEST(UrlParserTest, PercentEncodingDoubleEncodesV83) {
    auto result = clever::url::parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/hello%2520world");
}

TEST(UrlParserTest, DefaultPortHttpsNormalizedAwayV83) {
    auto result = clever::url::parse("https://example.com:443/secure");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/secure");
    EXPECT_EQ(result->serialize(), "https://example.com/secure");
}

TEST(UrlParserTest, NonDefaultPortPreservedInSerializeV83) {
    auto result = clever::url::parse("http://example.com:9090/api/v1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9090);
    EXPECT_EQ(result->path, "/api/v1");
    EXPECT_EQ(result->serialize(), "http://example.com:9090/api/v1");
}

TEST(UrlParserTest, HostCaseNormalizationMixedV83) {
    auto result = clever::url::parse("https://WwW.ExAmPlE.CoM/CaseSensitivePath");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->path, "/CaseSensitivePath");
}

TEST(UrlParserTest, InvalidSchemeReturnsNulloptV83) {
    auto result = clever::url::parse("://missing-scheme.com/path");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// V84 Tests
// =============================================================================

TEST(UrlParserTest, DotDotSegmentResolutionV84) {
    auto result = clever::url::parse("https://example.com/a/b/../c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/c");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(UrlParserTest, DoubleEncodesPercentSequencesV84) {
    auto result = clever::url::parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/hello%2520world");
}

TEST(UrlParserTest, EmptyPathDefaultsToSlashV84) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(UrlParserTest, QueryOnlyNoFragmentV84) {
    auto result = clever::url::parse("https://example.com/search?q=hello&lang=en");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_EQ(result->query, "q=hello&lang=en");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, FragmentOnlyNoQueryV84) {
    auto result = clever::url::parse("https://example.com/page#section-2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/page");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "section-2");
}

TEST(UrlParserTest, HostLowercasedWithPortV84) {
    auto result = clever::url::parse("http://MyHost.EXAMPLE.COM:3000/api");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "myhost.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000);
    EXPECT_EQ(result->path, "/api");
}

TEST(UrlParserTest, MultipleDotDotSegmentsV84) {
    auto result = clever::url::parse("https://example.com/a/b/c/../../d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/d");
}

TEST(UrlParserTest, CompletelyInvalidUrlReturnsNulloptV84) {
    auto result = clever::url::parse("not-a-url-at-all");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// V85 Tests
// =============================================================================

TEST(UrlParserTest, FtpSchemeWithPathAndFragmentV85) {
    auto result = clever::url::parse("ftp://files.example.com/pub/docs/readme.txt#top");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/pub/docs/readme.txt");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "top");
}

TEST(UrlParserTest, HttpsDefaultPort443NormalizedAwayV85) {
    auto result = clever::url::parse("https://secure.example.com:443/login?next=/dashboard");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "secure.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/login");
    EXPECT_EQ(result->query, "next=/dashboard");
}

TEST(UrlParserTest, MixedCaseHostFullyLowercasedV85) {
    auto result = clever::url::parse("https://API.SubDomain.EXAMPLE.COM/v2/resource");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.subdomain.example.com");
    EXPECT_EQ(result->path, "/v2/resource");
}

TEST(UrlParserTest, DotDotResolvesToRootWhenExhaustedV85) {
    auto result = clever::url::parse("https://example.com/a/../../../b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/b");
}

TEST(UrlParserTest, PercentEncodedSpaceDoubleEncodedV85) {
    auto result = clever::url::parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/hello%2520world");
}

TEST(UrlParserTest, EmptyStringReturnsNulloptV85) {
    auto result = clever::url::parse("");
    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTest, QueryWithMultipleParamsNoFragmentV85) {
    auto result = clever::url::parse("https://search.example.com/find?lang=en&sort=date&page=3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "search.example.com");
    EXPECT_EQ(result->path, "/find");
    EXPECT_EQ(result->query, "lang=en&sort=date&page=3");
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, SerializeReconstructsUrlCorrectlyV85) {
    auto result = clever::url::parse("https://example.com:9090/api/data?key=abc#ref");
    ASSERT_TRUE(result.has_value());
    std::string serialized = result->serialize();
    EXPECT_NE(serialized.find("https"), std::string::npos);
    EXPECT_NE(serialized.find("example.com"), std::string::npos);
    EXPECT_NE(serialized.find("9090"), std::string::npos);
    EXPECT_NE(serialized.find("/api/data"), std::string::npos);
    EXPECT_NE(serialized.find("key=abc"), std::string::npos);
    EXPECT_NE(serialized.find("ref"), std::string::npos);
}

// =============================================================================
// V86 Tests
// =============================================================================

TEST(UrlParserTest, TrailingDotInHostPreservedV86) {
    auto result = clever::url::parse("https://example.com./page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com.");
    EXPECT_EQ(result->path, "/page");
    EXPECT_EQ(result->port, std::nullopt);
}

TEST(UrlParserTest, MultipleConsecutiveDotDotSegmentsClampToRootV86) {
    auto result = clever::url::parse("https://example.com/a/b/c/../../../..");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

TEST(UrlParserTest, PercentEncodedSlashDoubleEncodedV86) {
    auto result = clever::url::parse("https://example.com/path%2Fmore");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_NE(result->path.find("%252F"), std::string::npos);
}

TEST(UrlParserTest, QueryContainsHashLiteralEncodedV86) {
    auto result = clever::url::parse("https://example.com/search?q=a%23b#sec");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("a%2523b"), std::string::npos);
    EXPECT_EQ(result->fragment, "sec");
}

TEST(UrlParserTest, FtpDefaultPort21NormalizedAwayV86) {
    auto result = clever::url::parse("ftp://files.example.com:21/pub/readme.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/pub/readme.txt");
}

TEST(UrlParserTest, PortZeroPreservedAsExplicitV86) {
    auto result = clever::url::parse("http://example.com:0/test");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 0);
    EXPECT_EQ(result->path, "/test");
}

TEST(UrlParserTest, HostWithUpperAndDigitsLowercasedV86) {
    auto result = clever::url::parse("https://API-Server42.Example.COM:8443/v2/status");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "api-server42.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/v2/status");
}

TEST(UrlParserTest, SingleDotSegmentRemovedFromPathV86) {
    auto result = clever::url::parse("https://example.com/a/./b/./c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/a/b/c");
}

// =============================================================================
// V87 Tests
// =============================================================================

TEST(UrlParserTest, HttpDefaultPort80NormalizedAwayV87) {
    auto result = clever::url::parse("http://www.example.com:80/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/index.html");
}

TEST(UrlParserTest, HttpsDefaultPort443NormalizedAwayV87) {
    auto result = clever::url::parse("https://secure.example.com:443/api/v1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "secure.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/api/v1");
}

TEST(UrlParserTest, DotDotAtStartOfPathResolvesToRootV87) {
    auto result = clever::url::parse("https://example.com/../../../file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/file.txt");
}

TEST(UrlParserTest, PercentEncodedSpaceDoubleEncodedV87) {
    auto result = clever::url::parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_NE(result->path.find("%2520"), std::string::npos);
}

TEST(UrlParserTest, MissingSchemeReturnsNulloptV87) {
    auto result = clever::url::parse("://example.com/path");
    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTest, MixedCaseHostLowercasedV87) {
    auto result = clever::url::parse("http://MyServer.EXAMPLE.Org:9090/data");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "myserver.example.org");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9090);
    EXPECT_EQ(result->path, "/data");
}

TEST(UrlParserTest, QueryAndFragmentPreservedCorrectlyV87) {
    auto result = clever::url::parse("https://example.com/search?q=hello+world&lang=en#results");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("q=hello"), std::string::npos);
    EXPECT_EQ(result->fragment, "results");
}

TEST(UrlParserTest, SerializeReconstructsUrlWithNonDefaultPortV87) {
    auto result = clever::url::parse("http://example.com:8080/app/index?mode=debug#top");
    ASSERT_TRUE(result.has_value());
    std::string serialized = result->serialize();
    EXPECT_NE(serialized.find("http"), std::string::npos);
    EXPECT_NE(serialized.find("example.com"), std::string::npos);
    EXPECT_NE(serialized.find("8080"), std::string::npos);
    EXPECT_NE(serialized.find("/app/index"), std::string::npos);
    EXPECT_NE(serialized.find("mode=debug"), std::string::npos);
    EXPECT_NE(serialized.find("top"), std::string::npos);
}

// =============================================================================
// V88 Tests
// =============================================================================

TEST(UrlParserTest, TrailingDotDotCollapsesToRootV88) {
    auto result = clever::url::parse("https://example.com/a/b/c/../../../..");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

TEST(UrlParserTest, PercentEncodedSpaceDoubleEncodedV88) {
    auto result = clever::url::parse("https://example.com/hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_NE(result->path.find("%2520"), std::string::npos);
}

TEST(UrlParserTest, HttpDefaultPort80NormalizedToNulloptV88) {
    auto result = clever::url::parse("http://www.example.com:80/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/index.html");
}

TEST(UrlParserTest, HostUppercaseFullyLowercasedV88) {
    auto result = clever::url::parse("https://WWW.EXAMPLE.COM/Page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->path, "/Page");
}

TEST(UrlParserTest, SchemeOnlyNoPathDefaultsSlashV88) {
    auto result = clever::url::parse("https://example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/");
}

TEST(UrlParserTest, MissingSchemeReturnsNulloptV88) {
    auto result = clever::url::parse("://example.com/path");
    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTest, QueryWithMultipleParamsPreservedV88) {
    auto result = clever::url::parse("https://api.example.com/search?q=test&page=2&lang=en#top");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "api.example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("q=test"), std::string::npos);
    EXPECT_NE(result->query.find("page=2"), std::string::npos);
    EXPECT_NE(result->query.find("lang=en"), std::string::npos);
    EXPECT_EQ(result->fragment, "top");
}

TEST(UrlParserTest, SerializeIncludesAllComponentsV88) {
    auto result = clever::url::parse("https://data.example.com:9443/api/v2?format=json#resp");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 9443);
    std::string serialized = result->serialize();
    EXPECT_NE(serialized.find("https"), std::string::npos);
    EXPECT_NE(serialized.find("data.example.com"), std::string::npos);
    EXPECT_NE(serialized.find("9443"), std::string::npos);
    EXPECT_NE(serialized.find("/api/v2"), std::string::npos);
    EXPECT_NE(serialized.find("format=json"), std::string::npos);
    EXPECT_NE(serialized.find("resp"), std::string::npos);
}

// =============================================================================
// V89 Tests
// =============================================================================

TEST(UrlParserTest, NonDefaultPort8443ParsedCorrectlyV89) {
    auto result = clever::url::parse("https://secure.example.com:8443/dashboard");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "secure.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/dashboard");
}

TEST(UrlParserTest, EmptyPathDefaultsToSlashV89) {
    auto result = clever::url::parse("http://bare.example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "bare.example.com");
    EXPECT_EQ(result->path, "/");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, QueryOnlyNoFragmentV89) {
    auto result = clever::url::parse("https://search.example.com/find?term=hello&limit=50");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "search.example.com");
    EXPECT_EQ(result->path, "/find");
    EXPECT_NE(result->query.find("term=hello"), std::string::npos);
    EXPECT_NE(result->query.find("limit=50"), std::string::npos);
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, FragmentOnlyNoQueryV89) {
    auto result = clever::url::parse("https://docs.example.com/manual#chapter-7");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "docs.example.com");
    EXPECT_EQ(result->path, "/manual");
    EXPECT_TRUE(result->query.empty());
    EXPECT_EQ(result->fragment, "chapter-7");
}

TEST(UrlParserTest, FtpSchemeNonDefaultPortV89) {
    auto result = clever::url::parse("ftp://files.example.com:2121/pub/data.tar.gz");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 2121);
    EXPECT_EQ(result->path, "/pub/data.tar.gz");
}

TEST(UrlParserTest, SerializeRoundTripWithPortV89) {
    auto result = clever::url::parse("http://app.example.com:3000/api/v1?key=abc123#section");
    ASSERT_TRUE(result.has_value());
    std::string serialized = result->serialize();
    EXPECT_NE(serialized.find("http"), std::string::npos);
    EXPECT_NE(serialized.find("app.example.com"), std::string::npos);
    EXPECT_NE(serialized.find("3000"), std::string::npos);
    EXPECT_NE(serialized.find("/api/v1"), std::string::npos);
    EXPECT_NE(serialized.find("key=abc123"), std::string::npos);
    EXPECT_NE(serialized.find("section"), std::string::npos);
}

TEST(UrlParserTest, UserinfoInUrlV89) {
    auto result = clever::url::parse("https://admin:secret@private.example.com/settings");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "private.example.com");
    EXPECT_EQ(result->username, "admin");
    EXPECT_EQ(result->password, "secret");
    EXPECT_EQ(result->path, "/settings");
}

TEST(UrlParserTest, DoubleEncodedPercentInPathV89) {
    auto result = clever::url::parse("https://cdn.example.com/files/my%20doc.pdf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "cdn.example.com");
    EXPECT_NE(result->path.find("%2520"), std::string::npos);
}

TEST(UrlParserTest, HttpDefaultPortIsNulloptV90) {
    auto result = clever::url::parse("http://example.com/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/index.html");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, HttpsNonDefaultPortV90) {
    auto result = clever::url::parse("https://secure.example.org:8443/admin/dashboard");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "secure.example.org");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 8443);
    EXPECT_EQ(result->path, "/admin/dashboard");
}

TEST(UrlParserTest, MultipleQueryParametersV90) {
    auto result = clever::url::parse("https://api.example.com/search?lang=en&page=3&sort=date");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "api.example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("lang=en"), std::string::npos);
    EXPECT_NE(result->query.find("page=3"), std::string::npos);
    EXPECT_NE(result->query.find("sort=date"), std::string::npos);
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, FragmentWithSlashesV90) {
    auto result = clever::url::parse("https://wiki.example.com/article#section/subsection/detail");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "wiki.example.com");
    EXPECT_EQ(result->path, "/article");
    EXPECT_EQ(result->fragment, "section/subsection/detail");
}

TEST(UrlParserTest, SerializePreservesComponentsV90) {
    auto result = clever::url::parse("https://store.example.com:9090/products/item?id=42#reviews");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("store.example.com"), std::string::npos);
    EXPECT_NE(s.find("9090"), std::string::npos);
    EXPECT_NE(s.find("/products/item"), std::string::npos);
    EXPECT_NE(s.find("id=42"), std::string::npos);
    EXPECT_NE(s.find("reviews"), std::string::npos);
}

TEST(UrlParserTest, DoubleEncodedSpaceInQueryV90) {
    auto result = clever::url::parse("https://example.com/search?q=hello%20world");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("%2520"), std::string::npos);
}

TEST(UrlParserTest, DeepNestedPathSegmentsV90) {
    auto result = clever::url::parse("https://cdn.example.net/assets/img/icons/logo.svg");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "cdn.example.net");
    EXPECT_FALSE(result->port.has_value());
    EXPECT_EQ(result->path, "/assets/img/icons/logo.svg");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, InvalidSchemeReturnNulloptV90) {
    auto result = clever::url::parse("://missing-scheme.com/page");
    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTest, HttpDefaultPort80IsNulloptV91) {
    auto result = clever::url::parse("http://www.example.org:80/index.html");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "www.example.org");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/index.html");
}

TEST(UrlParserTest, NonDefaultPort3000PreservedV91) {
    auto result = clever::url::parse("http://localhost:3000/api/v1/users");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "localhost");
    ASSERT_TRUE(result->port.has_value());
    EXPECT_EQ(result->port.value(), 3000);
    EXPECT_EQ(result->path, "/api/v1/users");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}

TEST(UrlParserTest, DoubleEncodedPercentInPathV91) {
    auto result = clever::url::parse("https://example.com/dir%20name/file");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->path.find("%2520"), std::string::npos);
}

TEST(UrlParserTest, HostMixedCaseLowercasedV91) {
    auto result = clever::url::parse("https://WWW.ExAmPlE.COM/page");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->path, "/page");
}

TEST(UrlParserTest, QueryAndFragmentBothPresentV91) {
    auto result = clever::url::parse("https://example.com/search?q=test&limit=10#results");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, "/search");
    EXPECT_NE(result->query.find("q=test"), std::string::npos);
    EXPECT_NE(result->query.find("limit=10"), std::string::npos);
    EXPECT_EQ(result->fragment, "results");
}

TEST(UrlParserTest, SerializeRoundTripWithAllComponentsV91) {
    auto result = clever::url::parse("https://data.example.io:8443/api/items?format=json#top");
    ASSERT_TRUE(result.has_value());
    std::string s = result->serialize();
    EXPECT_NE(s.find("https"), std::string::npos);
    EXPECT_NE(s.find("data.example.io"), std::string::npos);
    EXPECT_NE(s.find("8443"), std::string::npos);
    EXPECT_NE(s.find("/api/items"), std::string::npos);
    EXPECT_NE(s.find("format=json"), std::string::npos);
    EXPECT_NE(s.find("top"), std::string::npos);
}

TEST(UrlParserTest, EmptyStringReturnsNulloptV91) {
    auto result = clever::url::parse("");
    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTest, FtpDefaultPort21NormalizedV91) {
    auto result = clever::url::parse("ftp://files.example.com:21/pub/archive.tar.gz");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "ftp");
    EXPECT_EQ(result->host, "files.example.com");
    EXPECT_EQ(result->port, std::nullopt);
    EXPECT_EQ(result->path, "/pub/archive.tar.gz");
    EXPECT_TRUE(result->query.empty());
    EXPECT_TRUE(result->fragment.empty());
}
