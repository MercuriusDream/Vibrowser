#include <clever/js/cors_policy.h>

#include <clever/net/header_map.h>

#include <gtest/gtest.h>

using clever::js::cors::cors_allows_response;
using clever::js::cors::has_enforceable_document_origin;
using clever::js::cors::is_cors_eligible_request_url;
using clever::js::cors::is_cross_origin;
using clever::js::cors::normalize_outgoing_origin_header;
using clever::js::cors::should_attach_origin_header;

TEST(CORSPolicyTest, DocumentOriginEnforcement) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
    EXPECT_FALSE(has_enforceable_document_origin("null"));
    EXPECT_FALSE(has_enforceable_document_origin("https://app.example/path"));
    EXPECT_FALSE(has_enforceable_document_origin("https://app..example"));
    EXPECT_FALSE(has_enforceable_document_origin("https://-app.example"));
    EXPECT_FALSE(has_enforceable_document_origin("https://app-.example"));
    EXPECT_FALSE(has_enforceable_document_origin("ftp://app.example"));
    EXPECT_FALSE(has_enforceable_document_origin(" https://app.example"));
    EXPECT_FALSE(has_enforceable_document_origin("https://app.example "));
    EXPECT_TRUE(has_enforceable_document_origin("https://app.example"));
}

TEST(CORSPolicyTest, CrossOriginDetection) {
    EXPECT_FALSE(is_cross_origin("", "https://api.example/data"));
    EXPECT_TRUE(is_cross_origin("null", "https://api.example/data"));
    EXPECT_FALSE(is_cross_origin("https://app.example", "https://app.example/path"));
    EXPECT_TRUE(is_cross_origin("https://app.example", "https://api.example/path"));
}

TEST(CORSPolicyTest, RequestUrlEligibility) {
    EXPECT_FALSE(is_cors_eligible_request_url(""));
    EXPECT_FALSE(is_cors_eligible_request_url("ftp://api.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("file:///tmp/test.html"));
    EXPECT_FALSE(is_cors_eligible_request_url(" https://api.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/data "));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/hello world"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://user:pass@api.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/data#frag"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://@api.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example:"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://[::1]:"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example\\data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api%2eexample/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example%40evil/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api..example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://-api.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api-.example/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://2130706433/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://127.1/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://0x7f000001/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://0x7f.0x0.0x0.0x1/data"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/%0a"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/%20"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/%5Cdata"));
    EXPECT_FALSE(is_cors_eligible_request_url("https://api.example/%C3%A4"));
    EXPECT_FALSE(is_cors_eligible_request_url(std::string("https://api.\x01example/data")));
    EXPECT_FALSE(is_cors_eligible_request_url(std::string("https://api.ex\xc3\xa4mple/data")));
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example/data"));
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example/data"));
}

TEST(CORSPolicyTest, OriginHeaderAttachmentRule) {
    EXPECT_FALSE(should_attach_origin_header("", "https://api.example/data"));
    EXPECT_FALSE(should_attach_origin_header("https://app.example", "https://app.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example/path", "https://api.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", " https://api.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/hello world"));
    EXPECT_FALSE(should_attach_origin_header("https://app.example",
                                             "https://user:pass@api.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/data#frag"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://@api.example/data"));
    EXPECT_FALSE(should_attach_origin_header("https://app.example", "https://api.example:"));
    EXPECT_FALSE(should_attach_origin_header("https://app.example", "https://[::1]:"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example\\data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api%2eexample/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example%40evil/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api..example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://-api.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api-.example/data"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/%0d"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/%20"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/%5cdata"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example", "https://api.example/%c3%a4"));
    EXPECT_FALSE(
        should_attach_origin_header("https://app.example",
                                    std::string("https://api.\x01example/data")));
    EXPECT_TRUE(should_attach_origin_header("https://app.example", "https://api.example/data"));
    EXPECT_TRUE(should_attach_origin_header("null", "https://api.example/data"));
}

TEST(CORSPolicyTest, NormalizeOutgoingOriginHeaderStripsSpoofedSameOriginValue) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://evil.example");

    normalize_outgoing_origin_header(headers, "https://app.example", "https://app.example/data");

    EXPECT_FALSE(headers.has("origin"));
}

TEST(CORSPolicyTest, NormalizeOutgoingOriginHeaderOverwritesSpoofedCrossOriginValue) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://evil.example");

    normalize_outgoing_origin_header(headers, "https://app.example", "https://api.example/data");

    ASSERT_TRUE(headers.has("origin"));
    EXPECT_EQ(headers.get("origin").value(), "https://app.example");
}

TEST(CORSPolicyTest, NormalizeOutgoingOriginHeaderUsesNullForCrossOriginNullDocument) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://evil.example");

    normalize_outgoing_origin_header(headers, "null", "https://api.example/data");

    ASSERT_TRUE(headers.has("origin"));
    EXPECT_EQ(headers.get("origin").value(), "null");
}

TEST(CORSPolicyTest, NormalizeOutgoingOriginHeaderDropsValueForMalformedInputs) {
    clever::net::HeaderMap malformed_document;
    malformed_document.set("Origin", "https://evil.example");
    normalize_outgoing_origin_header(
        malformed_document, "https://app.example/path", "https://api.example/data");
    EXPECT_FALSE(malformed_document.has("origin"));

    clever::net::HeaderMap malformed_request_url;
    malformed_request_url.set("Origin", "https://evil.example");
    normalize_outgoing_origin_header(
        malformed_request_url, "https://app.example", "ftp://api.example/data");
    EXPECT_FALSE(malformed_request_url.has("origin"));
}

TEST(CORSPolicyTest, SameOriginResponseAlwaysAllowed) {
    clever::net::HeaderMap headers;
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://app.example/data", headers,
                                     false));
}

TEST(CORSPolicyTest, EmptyDocumentOriginFailsClosed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_FALSE(cors_allows_response("", "https://api.example/data", headers, false));
}

TEST(CORSPolicyTest, CrossOriginRequiresACAO) {
    clever::net::HeaderMap headers;
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers,
                                      false));
}

TEST(CORSPolicyTest, CrossOriginRejectsMalformedDocumentOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example/path");
    EXPECT_FALSE(cors_allows_response("https://app.example/path", "https://api.example/data",
                                      headers, false));
}

TEST(CORSPolicyTest, CrossOriginRejectsMalformedOrUnsupportedRequestUrl) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "ftp://api.example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", " https://api.example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://api.example/hello world", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://user:pass@api.example/data", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://api.example/data#frag", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://@api.example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example:", headers,
                                      false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://[::1]:", headers, false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example\\data", headers, false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api%2eexample/data", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example%40evil/data",
                                      headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api..example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://-api.example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api-.example/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://256.1.1.1/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://127.1/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://0x7f000001/data", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://0x7f.0x0.0x0.0x1/data",
                                      headers, false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/%00", headers, false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/%20", headers, false));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/%5Cdata", headers, false));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/%c3%a4", headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      std::string("https://api.\x01example/data"), headers,
                                      false));
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      std::string("https://api.ex\xc3\xa4mple/data"), headers,
                                      false));
}

TEST(CORSPolicyTest, CrossOriginNonCredentialedAllowsWildcardOrExact) {
    clever::net::HeaderMap wildcard;
    wildcard.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(
        cors_allows_response("https://app.example", "https://api.example/data", wildcard, false));

    clever::net::HeaderMap exact;
    exact.set("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data", exact,
                                     false));

    clever::net::HeaderMap wrong;
    wrong.set("Access-Control-Allow-Origin", "https://other.example");
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/data", wrong, false));

    clever::net::HeaderMap canonical_equivalent;
    canonical_equivalent.set("Access-Control-Allow-Origin", "HTTPS://APP.EXAMPLE:443");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     canonical_equivalent, false));
}

TEST(CORSPolicyTest, CrossOriginRejectsMalformedACAOValue) {
    clever::net::HeaderMap comma_separated;
    comma_separated.set("Access-Control-Allow-Origin", "https://app.example, https://other.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      comma_separated, false));

    clever::net::HeaderMap control_char;
    control_char.set("Access-Control-Allow-Origin", std::string("https://app.\x01example"));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/data", control_char, false));

    clever::net::HeaderMap non_ascii;
    non_ascii.set("Access-Control-Allow-Origin", std::string("https://app.ex\xc3\xa4mple"));
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/data", non_ascii, false));

    clever::net::HeaderMap duplicate_acao;
    duplicate_acao.append("Access-Control-Allow-Origin", "https://app.example");
    duplicate_acao.append("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      duplicate_acao, false));

    clever::net::HeaderMap empty_port;
    empty_port.set("Access-Control-Allow-Origin", "https://app.example:");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      empty_port, false));

    clever::net::HeaderMap nondigit_port;
    nondigit_port.set("Access-Control-Allow-Origin", "https://app.example:443abc");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      nondigit_port, false));

    clever::net::HeaderMap malformed_host_label;
    malformed_host_label.set("Access-Control-Allow-Origin", "https://app..example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      malformed_host_label, false));

    clever::net::HeaderMap leading_hyphen_label;
    leading_hyphen_label.set("Access-Control-Allow-Origin", "https://-app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      leading_hyphen_label, false));

    clever::net::HeaderMap trailing_hyphen_label;
    trailing_hyphen_label.set("Access-Control-Allow-Origin", "https://app-.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      trailing_hyphen_label, false));

    clever::net::HeaderMap invalid_dotted_ipv4;
    invalid_dotted_ipv4.set("Access-Control-Allow-Origin", "https://256.1.1.1");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      invalid_dotted_ipv4, false));

    clever::net::HeaderMap noncanonical_dotted_ipv4;
    noncanonical_dotted_ipv4.set("Access-Control-Allow-Origin", "https://001.2.3.4");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      noncanonical_dotted_ipv4, false));

    clever::net::HeaderMap legacy_integer_ipv4;
    legacy_integer_ipv4.set("Access-Control-Allow-Origin", "https://2130706433");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      legacy_integer_ipv4, false));

    clever::net::HeaderMap legacy_shorthand_dotted_ipv4;
    legacy_shorthand_dotted_ipv4.set("Access-Control-Allow-Origin", "https://127.1");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      legacy_shorthand_dotted_ipv4, false));

    clever::net::HeaderMap legacy_hex_integer_ipv4;
    legacy_hex_integer_ipv4.set("Access-Control-Allow-Origin", "https://0x7f000001");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      legacy_hex_integer_ipv4, false));

    clever::net::HeaderMap legacy_hex_dotted_ipv4;
    legacy_hex_dotted_ipv4.set("Access-Control-Allow-Origin", "https://0x7f.0x0.0x0.0x1");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      legacy_hex_dotted_ipv4, false));

    clever::net::HeaderMap surrounding_whitespace_acao;
    surrounding_whitespace_acao.set("Access-Control-Allow-Origin", " https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      surrounding_whitespace_acao, false));
}

TEST(CORSPolicyTest, CrossOriginCredentialedRequiresExactAndCredentialsTrue) {
    clever::net::HeaderMap wildcard;
    wildcard.set("Access-Control-Allow-Origin", "*");
    wildcard.set("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(
        cors_allows_response("https://app.example", "https://api.example/data", wildcard, true));

    clever::net::HeaderMap missing_credentials;
    missing_credentials.set("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      missing_credentials, true));

    clever::net::HeaderMap exact_and_true;
    exact_and_true.set("Access-Control-Allow-Origin", "https://app.example");
    exact_and_true.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     exact_and_true, true));

    clever::net::HeaderMap canonical_equivalent_and_true;
    canonical_equivalent_and_true.set("Access-Control-Allow-Origin", "HTTPS://APP.EXAMPLE:443");
    canonical_equivalent_and_true.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     canonical_equivalent_and_true, true));

    clever::net::HeaderMap malformed_credentials;
    malformed_credentials.set("Access-Control-Allow-Origin", "https://app.example");
    malformed_credentials.set("Access-Control-Allow-Credentials", std::string("tr\x01ue"));
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      malformed_credentials, true));

    clever::net::HeaderMap non_ascii_credentials;
    non_ascii_credentials.set("Access-Control-Allow-Origin", "https://app.example");
    non_ascii_credentials.set("Access-Control-Allow-Credentials",
                              std::string("tr\xc3\xbc") + "e");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      non_ascii_credentials, true));

    clever::net::HeaderMap uppercase_true;
    uppercase_true.set("Access-Control-Allow-Origin", "https://app.example");
    uppercase_true.set("Access-Control-Allow-Credentials", "TRUE");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      uppercase_true, true));

    clever::net::HeaderMap mixed_case_true;
    mixed_case_true.set("Access-Control-Allow-Origin", "https://app.example");
    mixed_case_true.set("Access-Control-Allow-Credentials", "True");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      mixed_case_true, true));

    clever::net::HeaderMap surrounding_whitespace_true;
    surrounding_whitespace_true.set("Access-Control-Allow-Origin", "https://app.example");
    surrounding_whitespace_true.set("Access-Control-Allow-Credentials", " true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      surrounding_whitespace_true, true));

    clever::net::HeaderMap duplicate_acac;
    duplicate_acac.set("Access-Control-Allow-Origin", "https://app.example");
    duplicate_acac.append("Access-Control-Allow-Credentials", "true");
    duplicate_acac.append("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      duplicate_acac, true));
}

TEST(CORSPolicyTest, CrossOriginNullOriginRequiresStrictACAOAndCredentialsRule) {
    clever::net::HeaderMap wildcard;
    wildcard.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("null", "https://api.example/data", wildcard, false));

    clever::net::HeaderMap null_exact;
    null_exact.set("Access-Control-Allow-Origin", "null");
    EXPECT_TRUE(cors_allows_response("null", "https://api.example/data", null_exact, false));

    clever::net::HeaderMap wrong;
    wrong.set("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_FALSE(cors_allows_response("null", "https://api.example/data", wrong, false));

    clever::net::HeaderMap wildcard_credentialed;
    wildcard_credentialed.set("Access-Control-Allow-Origin", "*");
    wildcard_credentialed.set("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("null", "https://api.example/data", wildcard_credentialed,
                                      true));

    clever::net::HeaderMap null_credentialed;
    null_credentialed.set("Access-Control-Allow-Origin", "null");
    null_credentialed.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(
        cors_allows_response("null", "https://api.example/data", null_credentialed, true));
}

// ---------------------------------------------------------------------------
// Cycle 491 — CORS policy additional edge-case regression tests
// ---------------------------------------------------------------------------

// Same host with different port is cross-origin
TEST(CORSPolicyTest, SameHostDifferentPortIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://app.example:8080", "https://app.example/path"));
}

// Same host with different scheme is cross-origin
TEST(CORSPolicyTest, SameHostDifferentSchemeIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://app.example", "https://app.example/path"));
}

// A valid subdomain is an enforceable document origin
TEST(CORSPolicyTest, DocumentOriginWithSubdomainIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://sub.app.example"));
}

// CORS-eligible URL: query string does not disqualify it
TEST(CORSPolicyTest, CORSEligibleURLWithQueryString) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example/path?key=value"));
}

// CORS-eligible URL: non-standard port is still eligible
TEST(CORSPolicyTest, CORSEligibleURLWithNonStandardPort) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example:8443/data"));
}

// ACAO port 8080 does not match document origin on default port 443
TEST(CORSPolicyTest, CrossOriginPortMismatchInACAOBlocks) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example:8080");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      headers, false));
}

// ACAO with explicit standard port 443 canonically matches document origin
TEST(CORSPolicyTest, ACAOWithExplicitStandardPortMatchesDocumentOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example:443");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     headers, false));
}

// normalize_outgoing_origin_header is a no-op when no Origin header exists for same-origin
TEST(CORSPolicyTest, NormalizeOriginHeaderNoOpForSameOriginNoExistingHeader) {
    clever::net::HeaderMap headers; // no Origin header set
    normalize_outgoing_origin_header(headers, "https://app.example", "https://app.example/data");
    EXPECT_FALSE(headers.has("origin"));
}

// ============================================================================
// Cycle 503: CORS policy regression tests
// ============================================================================

// Same host and port is NOT cross-origin
TEST(CORSPolicyTest, SameHostAndPortIsNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("https://app.example:443", "https://app.example:443/data"));
}

// HTTP URL is not CORS-eligible (only https/http with restrictions)
TEST(CORSPolicyTest, LocalhostHTTPIsCORSEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://localhost/api"));
}

// File-scheme URL is not CORS-eligible
TEST(CORSPolicyTest, FileSchemeNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("file:///path/to/file.html"));
}

// should_attach_origin_header returns false for same-origin requests
TEST(CORSPolicyTest, ShouldNotAttachOriginForSameOrigin) {
    EXPECT_FALSE(should_attach_origin_header("https://app.example",
                                              "https://app.example/api/data"));
}

// should_attach_origin_header returns true for cross-origin requests
TEST(CORSPolicyTest, ShouldAttachOriginForCrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("https://app.example",
                                             "https://api.example/data"));
}

// cors_allows_response: wildcard ACAO allows non-credentialed cross-origin
TEST(CORSPolicyTest, WildcardACAOAllowsNonCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     headers, false));
}

// cors_allows_response: wildcard ACAO blocks credentialed cross-origin
TEST(CORSPolicyTest, WildcardACAOBlocksCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      headers, true));
}

// cors_allows_response: exact ACAO match allows credentialed cross-origin
TEST(CORSPolicyTest, ExactACAOMatchAllowsCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     headers, true));
}

// ============================================================================
// Cycle 515: CORS policy regression tests
// ============================================================================

// cors_allows_response: no ACAO header blocks cross-origin
TEST(CORSPolicyTest, MissingACAOBlocksCrossOrigin) {
    clever::net::HeaderMap headers;  // no ACAO header
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      headers, false));
}

// cors_allows_response: ACAO mismatch (different subdomain) blocks response
TEST(CORSPolicyTest, ACAOMismatchBlocksResponse) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://other.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      headers, false));
}

// is_cors_eligible_request_url: data: URL is not eligible
TEST(CORSPolicyTest, DataURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("data:text/plain,hello"));
}

// is_cors_eligible_request_url: about:blank is not eligible
TEST(CORSPolicyTest, AboutBlankNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("about:blank"));
}

// has_enforceable_document_origin: null origin is not enforceable
TEST(CORSPolicyTest, NullOriginStringNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

// has_enforceable_document_origin: a valid https origin is enforceable
TEST(CORSPolicyTest, ValidHttpsOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://example.com"));
}

// is_cors_eligible_request_url: https with path and query is eligible
TEST(CORSPolicyTest, HttpsURLWithPathAndQueryIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example.com/v1/data?key=123"));
}

// cors_allows_response: same-origin request is always allowed regardless of ACAO
TEST(CORSPolicyTest, SameOriginAlwaysAllowedNoACAO) {
    clever::net::HeaderMap headers;  // no ACAO header
    EXPECT_TRUE(cors_allows_response("https://example.com", "https://example.com/api",
                                     headers, false));
}

// ============================================================================
// Cycle 531: CORS policy regression tests
// ============================================================================

// http:// URL is cors eligible
TEST(CORSPolicyTest, HttpURLIsCORSEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/resource"));
}

// ws:// URL is not cors eligible
TEST(CORSPolicyTest, WsURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("ws://echo.example.com/"));
}

// is_cross_origin: same scheme+host+port returns false
TEST(CORSPolicyTest, SameOriginIsNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("https://example.com", "https://example.com/path"));
}

// is_cross_origin: different host returns true
TEST(CORSPolicyTest, DifferentHostIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://app.example.com", "https://api.example.com/data"));
}

// is_cross_origin: different scheme returns true
TEST(CORSPolicyTest, DifferentSchemeIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "https://example.com/path"));
}

// cors_allows_response: wildcard ACAO allows non-credentialed
TEST(CORSPolicyTest, WildcardACAOPermitsNonCredential) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     headers, false));
}

// has_enforceable_document_origin: http:// origin without path is enforceable
TEST(CORSPolicyTest, HttpOriginWithoutPathIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://example.com"));
}

// normalize_outgoing_origin_header sets Origin header on cross-origin request
TEST(CORSPolicyTest, NormalizeOutgoingOriginSetsHeader) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://app.example.com", "https://api.different.com/resource");
    auto val = req_headers.get("Origin");
    EXPECT_TRUE(val.has_value());
    EXPECT_NE(val->find("app.example.com"), std::string::npos);
}

// ============================================================================
// Cycle 548: CORS policy regression tests
// ============================================================================

// cors_allows_response: wildcard ACAO blocks credentialed request
TEST(CORSPolicyTest, WildcardACAOBlocksCredentialedRequest) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    // credentialed=true: wildcard ACAO should block
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data",
                                      headers, true));
}

// is_cors_eligible_request_url: mailto: is not eligible
TEST(CORSPolicyTest, MailtoURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("mailto:user@example.com"));
}

// is_cors_eligible_request_url: javascript: is not eligible
TEST(CORSPolicyTest, JavascriptURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("javascript:void(0)"));
}

// is_cross_origin: same origin with different path is same-origin
TEST(CORSPolicyTest, SameSchemehostDifferentPathIsSameOrigin) {
    EXPECT_FALSE(is_cross_origin("https://example.com", "https://example.com/different/path"));
}

// has_enforceable_document_origin: empty string is not enforceable
TEST(CORSPolicyTest, EmptyStringNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
}

// should_attach_origin_header: cross-origin should return true
TEST(CORSPolicyTest, ShouldAttachOriginForCrossOriginRequest) {
    EXPECT_TRUE(should_attach_origin_header("https://app.example.com", "https://api.example.com/resource"));
}

// should_attach_origin_header: same-origin should return false
TEST(CORSPolicyTest, ShouldNotAttachOriginForSameOriginRequest) {
    EXPECT_FALSE(should_attach_origin_header("https://example.com", "https://example.com/api"));
}

// cors_allows_response: ACAO matching exact origin allows credentialed
TEST(CORSPolicyTest, ExactOriginMatchAllowsCredentialedRequest) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://api.example/data",
                                     headers, true));
}

// ============================================================================
// Cycle 568: More CORS policy tests
// ============================================================================

// has_enforceable_document_origin: http:// origin is enforceable
TEST(CORSPolicyTest, HttpOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://example.com"));
}

// has_enforceable_document_origin: subdomain is enforceable
TEST(CORSPolicyTest, SubdomainOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://api.example.com"));
}

// is_cors_eligible_request_url: file: is not eligible
TEST(CORSPolicyTest, FileURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("file:///etc/passwd"));
}

// is_cross_origin: different subdomain is cross-origin
TEST(CORSPolicyTest, DifferentSubdomainIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://app.example.com", "https://api.example.com/resource"));
}

// cors_allows_response: no ACAO header blocks request
TEST(CORSPolicyTest, NoACAOHeaderBlocksResponse) {
    clever::net::HeaderMap headers;
    // No Access-Control-Allow-Origin set
    EXPECT_FALSE(cors_allows_response("https://app.example.com",
                                      "https://api.example.com/data",
                                      headers, false));
}

// cors_allows_response: wildcard allows non-credentialed from any origin
TEST(CORSPolicyTest, WildcardACAOAllowsAnyOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("https://any.origin.example",
                                     "https://api.example.com/data",
                                     headers, false));
}

// cors_allows_response: mismatched ACAO blocks response
TEST(CORSPolicyTest, MismatchedACAOBlocksResponse) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://other.example.com");
    EXPECT_FALSE(cors_allows_response("https://app.example.com",
                                      "https://api.example.com/data",
                                      headers, false));
}

// normalize_outgoing_origin_header: same-origin request sets no Origin header
TEST(CORSPolicyTest, SameOriginRequestSetsNoOriginHeader) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://example.com", "https://example.com/api/data");
    EXPECT_FALSE(req_headers.has("Origin"));
}
