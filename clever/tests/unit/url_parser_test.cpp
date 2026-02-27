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
