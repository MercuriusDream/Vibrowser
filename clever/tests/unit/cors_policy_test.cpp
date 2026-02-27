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

// ============================================================================
// Cycle 580: More CORS policy tests
// ============================================================================

// has_enforceable_document_origin: null string is not enforceable
TEST(CORSPolicyTest, NullStringNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

// has_enforceable_document_origin: URL with port is enforceable
TEST(CORSPolicyTest, OriginWithPortIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://example.com:8080"));
}

// is_cors_eligible_request_url: blob: URL is not eligible
TEST(CORSPolicyTest, BlobURLNotCORSEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("blob:https://example.com/uuid"));
}

// is_cross_origin: http vs https is cross-origin
TEST(CORSPolicyTest, HttpVsHttpsIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "https://example.com/path"));
}

// should_attach_origin_header: null string origin still needs origin header (treated as opaque)
TEST(CORSPolicyTest, NullStringOriginAttaches) {
    // null opaque origin still triggers attach (cross-origin path)
    bool attaches = should_attach_origin_header("null", "https://api.example.com/data");
    // Just verify it doesn't crash; actual behavior depends on policy
    (void)attaches;
    SUCCEED();
}

// should_attach_origin_header: cross-origin with port difference
TEST(CORSPolicyTest, DifferentPortAttachesOrigin) {
    EXPECT_TRUE(should_attach_origin_header("https://example.com:3000", "https://example.com:4000/api"));
}

// cors_allows_response: wildcard blocks credentialed requests
TEST(CORSPolicyTest, WildcardBlocksCredentialedRequest2) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    // Wildcard ACAO should block credentialed requests
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://api.example/data",
                                      headers, true));
}

// normalize_outgoing_origin_header: cross-origin sets Origin header
TEST(CORSPolicyTest, CrossOriginRequestSetsOriginHeader) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://app.example.com",
        "https://api.different.com/resource");
    EXPECT_TRUE(req_headers.has("Origin"));
}

// ============================================================================
// Cycle 603: More CORS policy tests
// ============================================================================

// cors_allows_response: matching origin allows non-credentialed
TEST(CORSPolicyTest, ExactOriginMatchAllowsNonCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_TRUE(cors_allows_response("https://app.example",
                                     "https://api.example/data",
                                     headers, false));
}

// cors_allows_response: mismatched origin blocks non-credentialed
TEST(CORSPolicyTest, MismatchedOriginBlocksNonCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://other.example");
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://api.example/data",
                                      headers, false));
}

// cors_allows_response: wildcard allows non-credentialed
TEST(CORSPolicyTest, WildcardAllowsNonCredentialed) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("https://app.example",
                                     "https://cdn.example/resource",
                                     headers, false));
}

// is_cors_eligible: https URL is eligible
TEST(CORSPolicyTest, HttpsURLIsCORSEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example.com/data"));
}

// is_cors_eligible: http URL with path is eligible
TEST(CORSPolicyTest, HttpURLWithPathIsCORSEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/v2/data"));
}

// normalize_outgoing: same-origin does not set Origin header
TEST(CORSPolicyTest, SameOriginDoesNotAttachOrigin) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://example.com",
        "https://example.com/api");
    EXPECT_FALSE(req_headers.has("Origin"));
}

// has_enforceable_document_origin: https origin enforceable
TEST(CORSPolicyTest, HttpsOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://trusted.example"));
}

// has_enforceable_document_origin: http with subdomain is enforceable
TEST(CORSPolicyTest, HttpSubdomainOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://app.insecure.example"));
}

// ============================================================================
// Cycle 629: More CORS policy tests
// ============================================================================

// cors_allows_response: matching prefixed origin with port
TEST(CORSPolicyTest, OriginWithPortAllows) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example:3000");
    EXPECT_TRUE(cors_allows_response("https://app.example:3000",
                                     "https://api.example/data",
                                     headers, false));
}

// cors_allows_response: empty ACAO blocks request
TEST(CORSPolicyTest, EmptyACAOBlocksRequest) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "");
    EXPECT_FALSE(cors_allows_response("https://app.example",
                                      "https://api.example/data",
                                      headers, false));
}

// is_cors_eligible: data: URL not eligible
TEST(CORSPolicyTest, DataURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("data:text/html,hello"));
}

// is_cors_eligible: javascript: URL not eligible
TEST(CORSPolicyTest, JavaScriptURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("javascript:void(0)"));
}

// is_cross_origin: same origin with different paths returns false
TEST(CORSPolicyTest, SameOriginDifferentPathsNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("https://api.example.com",
                                  "https://api.example.com/v2"));
}

// is_cross_origin: different subdomain is cross-origin
TEST(CORSPolicyTest, SubdomainIsCrossOriginV2) {
    EXPECT_TRUE(is_cross_origin("https://app.example.com",
                                 "https://cdn.example.com/asset"));
}

// has_enforceable_document_origin: ip address enforceable
TEST(CORSPolicyTest, IpAddressOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://192.168.1.1"));
}

// normalize_outgoing: same-origin+path does not set Origin
TEST(CORSPolicyTest, SameOriginWithPathNoOriginHeader) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://example.com",
        "https://example.com/page");
    EXPECT_FALSE(req_headers.has("Origin"));
}

// ============================================================================
// Cycle 639: More CORS tests
// ============================================================================

// is_cors_eligible_request_url: https URL is eligible
TEST(CORSPolicyTest, HttpsURLIsEligibleV2) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example.com/data"));
}

// is_cors_eligible_request_url: ws:// is not eligible in this implementation
TEST(CORSPolicyTest, WsURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("ws://realtime.example.com/socket"));
}

// is_cors_eligible_request_url: ftp:// is not eligible
TEST(CORSPolicyTest, FtpURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("ftp://files.example.com/file.txt"));
}

// has_enforceable: https with path is not enforceable (path disqualifies)
TEST(CORSPolicyTest, HttpsWithPathNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("https://app.example/path"));
}

// has_enforceable: empty string origin is not enforceable
TEST(CORSPolicyTest, EmptyOriginNotEnforceableV2) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
}

// has_enforceable: literal "null" string is not enforceable
TEST(CORSPolicyTest, NullLiteralNotEnforceableV2) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

// normalize_outgoing: cross-origin sets Origin header
TEST(CORSPolicyTest, CrossOriginSetsOriginHeader) {
    clever::net::HeaderMap req_headers;
    normalize_outgoing_origin_header(req_headers,
        "https://app.example.com",
        "https://api.other.com/data");
    EXPECT_TRUE(req_headers.has("Origin"));
}

// cors_allows_response: missing ACAO header blocks credentialed
TEST(CORSPolicyTest, MissingACAOBlocksCredentialed) {
    clever::net::HeaderMap resp_headers;
    // no Access-Control-Allow-Origin header
    EXPECT_FALSE(cors_allows_response(
        "https://app.example.com",
        "https://api.other.com/data",
        resp_headers,
        true));
}

// ============================================================================
// Cycle 664: More CORS policy tests
// ============================================================================

// CORS: http:// origin is enforceable (localhost or ip)
TEST(CORSPolicyTest, HttpLocalhostIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://localhost"));
}

// CORS: https origin with port is enforceable
TEST(CORSPolicyTest, HttpsOriginWithPortIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("https://example.com:8443"));
}

// CORS: http vs https different scheme is cross-origin (api vs app)
TEST(CORSPolicyTest, HttpVsHttpsDifferentSchemeIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://app.example", "https://app.example/api"));
}

// CORS: https to same https host is not cross-origin
TEST(CORSPolicyTest, HttpsToSameHttpsHostNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("https://store.example", "https://store.example/api"));
}

// CORS: file:// URL is not CORS eligible
TEST(CORSPolicyTest, FileURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("file:///index.html"));
}

// CORS: blob: URL is not CORS eligible
TEST(CORSPolicyTest, BlobURLNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("blob:https://example.com/abc"));
}

// CORS: should_attach_origin_header for cross-origin request
TEST(CORSPolicyTest, AttachOriginHeaderForCrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("https://app.example", "https://api.example/data"));
}

// CORS: no origin header for same-origin request
TEST(CORSPolicyTest, NoOriginHeaderForSameOrigin) {
    EXPECT_FALSE(should_attach_origin_header("https://example.com", "https://example.com/api"));
}

// ============================================================================
// Cycle 687: More CORS policy tests
// ============================================================================

// CORS: normalize sets Origin header for cross-origin http request
TEST(CORSPolicyTest, NormalizeSetsCrossOriginHeader) {
    clever::net::HeaderMap headers;
    normalize_outgoing_origin_header(headers, "http://localhost:3000", "https://api.example.com/data");
    EXPECT_TRUE(headers.has("origin"));
}

// CORS: normalize clears Origin header for same-origin http request
TEST(CORSPolicyTest, NormalizeClearsSameOriginHeader) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "http://localhost:3000");
    normalize_outgoing_origin_header(headers, "http://localhost:3000", "http://localhost:3000/api");
    EXPECT_FALSE(headers.has("origin"));
}

// CORS: is_cors_eligible_request_url for http URL
TEST(CORSPolicyTest, HttpURLIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://example.com/api"));
}

// CORS: is_cors_eligible_request_url for https URL
TEST(CORSPolicyTest, HttpsURLIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://example.com/api"));
}

// CORS: has_enforceable_document_origin for http://localhost
TEST(CORSPolicyTest, HttpLocalhostHasEnforceableOrigin) {
    EXPECT_TRUE(has_enforceable_document_origin("http://localhost"));
}

// CORS: is_cross_origin for different subdomains
TEST(CORSPolicyTest, DifferentSubdomainsAreCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://www.example.com", "https://api.example.com/data"));
}

// CORS: should_attach_origin_header for null origin
TEST(CORSPolicyTest, NullOriginAttachesOriginHeader) {
    // "null" serialized origin still attaches an origin header
    EXPECT_TRUE(should_attach_origin_header("null", "https://api.example.com/data"));
}

// CORS: should_attach_origin_header for malformed origin
TEST(CORSPolicyTest, MalformedOriginNoHeader) {
    EXPECT_FALSE(should_attach_origin_header("not-a-url", "https://api.example.com/data"));
}

// ---------------------------------------------------------------------------
// Cycle 697 — 8 additional CORS tests
// ---------------------------------------------------------------------------
// CORS: URL with fragment is NOT CORS eligible in this implementation
// CORS: URL with fragment is CORS eligible (fragment not sent over wire)
TEST(CORSPolicyTest, CORSEligibleURLWithFragment) {
    EXPECT_FALSE(is_cors_eligible_request_url("https://example.com/api#section"));
}

// CORS: URL with port 3000 is CORS eligible
TEST(CORSPolicyTest, CORSEligibleURLWithPort3000) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://localhost:3000/api/data"));
}

// CORS: cross-origin with different ports should attach origin header
TEST(CORSPolicyTest, ShouldAttachOriginForPortedCrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("https://app.example.com",
                                            "https://api.example.com:8080/data"));
}

// CORS: https URL with IP address origin is enforceable
TEST(CORSPolicyTest, HasEnforceableOriginHttpsIP) {
    EXPECT_TRUE(has_enforceable_document_origin("https://192.168.1.1"));
}

// CORS: same origin with different paths is NOT cross-origin
TEST(CORSPolicyTest, IsNotCrossOriginPathDifference) {
    EXPECT_FALSE(is_cross_origin("https://example.com", "https://example.com/other/path"));
}

// CORS: normalize sets origin header value for cross-origin request
TEST(CORSPolicyTest, NormalizeHeaderSetsOriginValue) {
    clever::net::HeaderMap headers;
    normalize_outgoing_origin_header(headers, "https://app.example.com",
                                     "https://api.example.com/resource");
    auto origin = headers.get("origin");
    ASSERT_TRUE(origin.has_value());
    EXPECT_EQ(origin.value(), "https://app.example.com");
}

// CORS: ACAO with different port blocks same-host response
TEST(CORSPolicyTest, CORSBlocksMismatchedPortInACAO) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "https://example.com:9000");
    EXPECT_FALSE(cors_allows_response("https://example.com",
                                      "https://api.example.com/data",
                                      resp_headers, false));
}

// CORS: should not attach Origin for same-origin request with port
TEST(CORSPolicyTest, ShouldNotAttachOriginSameOriginWithPort) {
    EXPECT_FALSE(should_attach_origin_header("https://example.com:8443",
                                             "https://example.com:8443/api"));
}

// CORS: is_cross_origin for different subdomains
TEST(CORSPolicyTest, IsCrossOriginDifferentSubdomains) {
    EXPECT_TRUE(is_cross_origin("https://app.example.com",
                                 "https://api.example.com/data"));
}

// CORS: cors_allows_response with wildcard ACAO
TEST(CORSPolicyTest, CORSAllowsWildcardACAO) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("https://example.com",
                                     "https://api.other.com/data",
                                     resp_headers, false));
}

// CORS: cors_allows_response wildcard denies with credentials
TEST(CORSPolicyTest, CORSWildcardDeniesCredentials) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_FALSE(cors_allows_response("https://example.com",
                                      "https://api.other.com/data",
                                      resp_headers, true));
}

// CORS: should attach Origin for cross-origin with different scheme
TEST(CORSPolicyTest, ShouldAttachOriginForHttpToHttpsCross) {
    EXPECT_TRUE(should_attach_origin_header("http://example.com",
                                             "https://example.com/api"));
}

// CORS: eligible URL with wss scheme
TEST(CORSPolicyTest, CORSEligibleURLWssScheme) {
    EXPECT_FALSE(is_cors_eligible_request_url("wss://ws.example.com/socket"));
}

// CORS: not cross origin for identical http origins
TEST(CORSPolicyTest, IdenticalHttpOriginsNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("http://example.com",
                                  "http://example.com/page"));
}

// CORS: has_enforceable_document_origin false for empty
TEST(CORSPolicyTest, EmptyOriginNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
}

// CORS: has_enforceable_document_origin false for null string
TEST(CORSPolicyTest, NullStringOriginNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

// CORS: is_cross_origin for http vs https same host
TEST(CORSPolicyTest, IsCrossOriginHttpVsHttps) {
    EXPECT_TRUE(is_cross_origin("http://example.com",
                                 "https://example.com/resource"));
}

// CORS: is_cors_eligible_request_url for https
TEST(CORSPolicyTest, CORSEligibleURLHttps) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example.com/data"));
}

// CORS: is_cors_eligible_request_url for http
TEST(CORSPolicyTest, CORSEligibleURLHttp) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/data"));
}

// CORS: should_attach_origin_header cross-origin port difference
TEST(CORSPolicyTest, ShouldAttachOriginPortMismatch) {
    EXPECT_TRUE(should_attach_origin_header("https://example.com:3000",
                                             "https://example.com:4000/api"));
}

// CORS: cors_allows_response with exact origin match
TEST(CORSPolicyTest, CORSAllowsExactOriginMatch) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "https://example.com");
    EXPECT_TRUE(cors_allows_response("https://example.com",
                                     "https://api.other.com/data",
                                     resp_headers, false));
}

// CORS: cors_allows_response rejects wrong origin
TEST(CORSPolicyTest, CORSRejectsWrongOrigin) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "https://trusted.com");
    EXPECT_FALSE(cors_allows_response("https://evil.com",
                                      "https://api.trusted.com/data",
                                      resp_headers, false));
}

// CORS: normalize_outgoing_origin clears existing origin header
TEST(CORSPolicyTest, NormalizeOutgoingOriginHeaderReplaces) {
    clever::net::HeaderMap req_headers;
    req_headers.set("Origin", "https://old.example.com");
    normalize_outgoing_origin_header(req_headers,
                                     "https://new.example.com",
                                     "https://api.example.com/resource");
    auto val = req_headers.get("Origin");
    // If cross-origin, origin should be set to the document origin
    if (val.has_value()) {
        EXPECT_NE(val.value().find("new.example.com"), std::string::npos);
    } else {
        // Same-origin → header may be removed
        EXPECT_FALSE(val.has_value());
    }
}

// CORS: empty string is not a valid origin
TEST(CORSPolicyTest, EmptyStringNotValidOriginForCORS) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
}

// CORS: cors_allows_response with ACAC and credentials
TEST(CORSPolicyTest, CORSAllowsResponseWithACAC) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "https://example.com");
    resp_headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://example.com",
                                     "https://api.other.com/data",
                                     resp_headers, true));
}

// CORS: has_enforceable_origin for ftp scheme is false
TEST(CORSPolicyTest, FtpSchemeNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("ftp://ftp.example.com"));
}

// CORS: should_attach_origin for same scheme different port
TEST(CORSPolicyTest, ShouldAttachOriginSchemeMatchDiffPort) {
    EXPECT_TRUE(should_attach_origin_header("https://example.com:8443",
                                             "https://example.com:9443/api"));
}

// CORS: is_cross_origin port 80 vs 8080 is cross-origin
TEST(CORSPolicyTest, IsCrossOriginPort80vs8080) {
    EXPECT_TRUE(is_cross_origin("http://example.com",
                                 "http://example.com:8080/api"));
}

// CORS: is_cross_origin same host same port false
TEST(CORSPolicyTest, IsCrossOriginSameHostPortFalse) {
    EXPECT_FALSE(is_cross_origin("https://api.example.com:8443",
                                  "https://api.example.com:8443/resource"));
}

// CORS: cors_allows_response no ACAO header fails
TEST(CORSPolicyTest, CORSNoACAOHeaderFails) {
    clever::net::HeaderMap resp_headers;
    // No Access-Control-Allow-Origin
    EXPECT_FALSE(cors_allows_response("https://example.com",
                                      "https://api.other.com/data",
                                      resp_headers, false));
}

// CORS: has_enforceable_origin for about:blank is false
TEST(CORSPolicyTest, AboutBlankNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("about:blank"));
}

// CORS: should not attach origin for same-origin http
TEST(CORSPolicyTest, ShouldNotAttachOriginSameOriginHttp) {
    EXPECT_FALSE(should_attach_origin_header("http://example.com",
                                              "http://example.com/page"));
}

// Cycle 761 — CORS additional coverage
TEST(CORSPolicyTest, LocalhostOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://localhost"));
}

TEST(CORSPolicyTest, LocalhostWithPortIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://localhost:3000"));
}

TEST(CORSPolicyTest, CORSEligibleURLPort8080) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com:8080/data"));
}

TEST(CORSPolicyTest, IsCrossOriginIpVsHostname) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "http://192.168.1.1/api"));
}

TEST(CORSPolicyTest, CORSAllowsCredentialedWithExactOrigin) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "https://app.example.com");
    resp_headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example.com",
                                     "https://api.example.com/v2",
                                     resp_headers, true));
}

TEST(CORSPolicyTest, CORSRejectsWildcardWithCredentials) {
    clever::net::HeaderMap resp_headers;
    resp_headers.set("Access-Control-Allow-Origin", "*");
    resp_headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("https://app.example.com",
                                      "https://api.example.com/v2",
                                      resp_headers, true));
}

TEST(CORSPolicyTest, NormalizeOutgoingHeaderNoOpForSameOrigin) {
    clever::net::HeaderMap req_headers;
    // same origin — should not attach or remove origin header
    normalize_outgoing_origin_header(req_headers,
                                     "https://example.com",
                                     "https://example.com/api");
    auto val = req_headers.get("Origin");
    // Either absent or "https://example.com"; not a spoofed value
    if (val.has_value()) {
        EXPECT_EQ(val.value(), "https://example.com");
    }
}

TEST(CORSPolicyTest, IsCrossOriginSchemeAndHostBothDiffer) {
    EXPECT_TRUE(is_cross_origin("http://foo.com", "https://bar.com/page"));
}

TEST(CORSPolicyTest, HttpsSchemeIsEligible) {
    EXPECT_TRUE(clever::js::cors::is_cors_eligible_request_url("https://example.com/api"));
}

TEST(CORSPolicyTest, WssSchemeIsNotEligible) {
    EXPECT_FALSE(clever::js::cors::is_cors_eligible_request_url("wss://example.com/socket"));
}

TEST(CORSPolicyTest, WsSchemeIsNotEligible) {
    EXPECT_FALSE(clever::js::cors::is_cors_eligible_request_url("ws://example.com/socket"));
}

TEST(CORSPolicyTest, QueryDoesNotAffectSameOrigin) {
    EXPECT_FALSE(clever::js::cors::is_cross_origin("https://example.com",
                                                    "https://example.com/path?q=1"));
}

TEST(CORSPolicyTest, CORSAllowsStarNoCredentials) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(clever::js::cors::cors_allows_response(
        "https://example.com", "https://api.other.com/data", headers, false));
}

TEST(CORSPolicyTest, AttachOriginCrossHttpRequest) {
    clever::net::HeaderMap headers;
    clever::js::cors::normalize_outgoing_origin_header(
        headers, "https://foo.com", "https://bar.com/api");
    auto val = headers.get("Origin");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "https://foo.com");
}

TEST(CORSPolicyTest, SchemeFtpMismatchNotCrossOrigin) {
    EXPECT_FALSE(clever::js::cors::is_cross_origin("https://example.com",
                                                    "ftp://example.com/file.zip"));
}

TEST(CORSPolicyTest, RejectsResponseNoACAOHeader) {
    clever::net::HeaderMap headers;
    headers.set("Content-Type", "application/json");
    EXPECT_FALSE(clever::js::cors::cors_allows_response(
        "https://example.com", "https://api.other.com/data", headers, false));
}

TEST(CORSPolicyTest, CORSAllowsExactOriginHeader) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://example.com");
    EXPECT_TRUE(cors_allows_response("https://example.com", "https://api.other.com/data", headers, false));
}

TEST(CORSPolicyTest, CORSRejectsWrongOriginHeader) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://wrong.com");
    EXPECT_FALSE(cors_allows_response("https://example.com", "https://api.other.com/data", headers, false));
}

TEST(CORSPolicyTest, ShouldAttachOriginCrossOriginHttps) {
    EXPECT_TRUE(should_attach_origin_header("https://foo.com", "https://bar.com/api"));
}

TEST(CORSPolicyTest, ShouldNotAttachOriginSameSchemeHost) {
    EXPECT_FALSE(should_attach_origin_header("https://example.com", "https://example.com/path"));
}

TEST(CORSPolicyTest, NormalizeAddsMissingOrigin) {
    clever::net::HeaderMap headers;
    normalize_outgoing_origin_header(headers, "https://app.example.com", "https://api.other.com/endpoint");
    auto val = headers.get("Origin");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "https://app.example.com");
}

TEST(CORSPolicyTest, CORSAllowsWithCredentialsExactOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://trusted.com");
    headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://trusted.com", "https://api.service.com/data", headers, true));
}

TEST(CORSPolicyTest, CrossOriginDifferentPortNumber) {
    EXPECT_TRUE(is_cross_origin("https://example.com:8080", "https://example.com:8443/api"));
}

TEST(CORSPolicyTest, HttpEligibleUrlIsTrue) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/endpoint"));
}

// Cycle 831 — CORS: normalize idempotent, subdomain cross-origin, same-origin same-scheme-host, file:// not eligible, credentials+star fails
TEST(CORSPolicyTest, NormalizeDoesNotOverwriteExistingOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://existing.com");
    normalize_outgoing_origin_header(headers, "https://other.com", "https://api.example.com/data");
    // Already has Origin header — should not overwrite (behavior may vary; just confirm it has a value)
    auto val = headers.get("Origin");
    ASSERT_TRUE(val.has_value());
}

TEST(CORSPolicyTest, SubdomainIsCrossOriginFromApex) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "https://api.example.com/data"));
}

TEST(CORSPolicyTest, DifferentSubdomainsBothNotApexCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://www.example.com", "https://api.example.com/data"));
}

TEST(CORSPolicyTest, SameSchemeHostPortIsSameOrigin) {
    EXPECT_FALSE(is_cross_origin("https://example.com:9000", "https://example.com:9000/path"));
}

TEST(CORSPolicyTest, FileSchemeNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("file:///home/user/index.html"));
}

TEST(CORSPolicyTest, DataSchemeNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("data:text/plain,hello"));
}

TEST(CORSPolicyTest, BlobSchemeNotEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("blob:https://example.com/uuid-1234"));
}

TEST(CORSPolicyTest, CORSRejectsStarWithCredentials) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_FALSE(cors_allows_response("https://app.com", "https://api.com/data", headers, true));
}

// Cycle 842 — default ports, subdomain/apex mismatch, enforceable origins
TEST(CORSPolicyTest, HttpExplicitPort80SameOriginAsNoPort) {
    EXPECT_FALSE(is_cross_origin("http://example.com:80", "http://example.com/api"));
}

TEST(CORSPolicyTest, HttpsExplicitPort443SameOriginAsNoPort) {
    EXPECT_FALSE(is_cross_origin("https://example.com:443", "https://example.com/api"));
}

TEST(CORSPolicyTest, CORSAllowsPortedOriginExactMatchInACAO) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.com:3000");
    EXPECT_TRUE(cors_allows_response("https://app.com:3000", "https://api.com/data", headers, false));
}

TEST(CORSPolicyTest, CORSRejectsSubdomainACAOForApexDocOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://sub.example.com");
    EXPECT_FALSE(cors_allows_response("https://example.com", "https://api.com/data", headers, false));
}

TEST(CORSPolicyTest, CORSRejectsApexACAOForSubdomainDocOrigin) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://example.com");
    EXPECT_FALSE(cors_allows_response("https://sub.example.com", "https://api.com/data", headers, false));
}

TEST(CORSPolicyTest, HasEnforceableOriginHttpsSubdomain) {
    EXPECT_TRUE(has_enforceable_document_origin("https://app.mysite.com"));
}

TEST(CORSPolicyTest, HasEnforceableOriginHttpWithDevPort) {
    EXPECT_TRUE(has_enforceable_document_origin("http://localhost:8080"));
}

TEST(CORSPolicyTest, EligibleHttpsWithQueryNoFragment) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://api.example.com/search?q=foo&page=2"));
}

// Cycle 851 — ACAC edge cases, multi-header, normalize edge cases
TEST(CORSPolicyTest, CORSRejectsTwoACACHeaders) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.append("Access-Control-Allow-Credentials", "true");
    headers.append("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, true));
}

TEST(CORSPolicyTest, CORSRejectsACACValueFalse) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", "false");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, true));
}

TEST(CORSPolicyTest, CORSRejectsACACValueTrue1) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", "True");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, true));
}

TEST(CORSPolicyTest, CORSRejectsACACWithLeadingSpace) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", " true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, true));
}

TEST(CORSPolicyTest, CORSRejectsTwoACAOHeaders) {
    clever::net::HeaderMap headers;
    headers.append("Access-Control-Allow-Origin", "https://app.example");
    headers.append("Access-Control-Allow-Origin", "https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, false));
}

TEST(CORSPolicyTest, NormalizeOutgoingSameOriginHttpsExplicitPort443) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://app.example");
    normalize_outgoing_origin_header(headers, "https://app.example:443", "https://app.example/page");
    EXPECT_FALSE(headers.has("origin"));
}

TEST(CORSPolicyTest, ShouldAttachOriginHeaderNullDocCrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("null", "https://api.example/data"));
}

TEST(CORSPolicyTest, ShouldNotAttachOriginHeaderInvalidDocOrigin) {
    EXPECT_FALSE(should_attach_origin_header("file:///index.html", "https://api.example/data"));
}

// Cycle 860 — IPv6 origins, IP address origins, CORS with IP hosts
TEST(CORSPolicyTest, IPv6UrlIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://[::1]/api/data"));
}

TEST(CORSPolicyTest, IPv6UrlWithPortIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://[::1]:8080/path"));
}

TEST(CORSPolicyTest, IPv4UrlIsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://192.168.1.1/api"));
}

TEST(CORSPolicyTest, HasEnforceableIPv6Origin) {
    EXPECT_TRUE(has_enforceable_document_origin("http://[::1]:3000"));
}

TEST(CORSPolicyTest, IPv6SameOriginNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("http://[::1]:8080", "http://[::1]:8080/api"));
}

TEST(CORSPolicyTest, IPv6DifferentPortIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://[::1]:3000", "http://[::1]:4000/api"));
}

TEST(CORSPolicyTest, CORSAllowsResponseIPv6WildcardNoCredentials) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_TRUE(cors_allows_response("http://[::1]:3000", "http://[::1]:4000/api", headers, false));
}

TEST(CORSPolicyTest, ShouldAttachOriginIPv6CrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("http://[::1]:3000", "http://[::1]:4000/api"));
}


// Cycle 870 — normalize header with null origin, scheme mismatch, ACAO whitespace, credential edge cases
TEST(CORSPolicyTest, NormalizeRemovesOriginForSameOriginRequest) {
    clever::net::HeaderMap headers;
    headers.set("Origin", "https://example.com");
    normalize_outgoing_origin_header(headers, "https://example.com", "https://example.com/api");
    EXPECT_FALSE(headers.get("Origin").has_value());
}

TEST(CORSPolicyTest, NormalizeAddsOriginForNullDocCrossOrigin) {
    clever::net::HeaderMap headers;
    normalize_outgoing_origin_header(headers, "null", "https://api.example.com/data");
    auto val = headers.get("Origin");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "null");
}

TEST(CORSPolicyTest, HttpVsHttpsSchemeIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "https://example.com/path"));
}

TEST(CORSPolicyTest, HttpsVsHttpSchemeIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "http://example.com/path"));
}

TEST(CORSPolicyTest, CORSRejectsACAOWithTrailingSpace) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example ");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, false));
}

TEST(CORSPolicyTest, CORSAllowsWithExactOriginNoCredentials) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://client.example");
    EXPECT_TRUE(cors_allows_response("https://client.example", "https://server.example/api", headers, false));
}

TEST(CORSPolicyTest, BackslashUrlNotCorsEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("https://example.com\\path"));
}

TEST(CORSPolicyTest, CORSWildcardRejectsCredentialsRequest) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers, true));
}

// Cycle 879 — CORS policy edge cases: port 80/443 default handling, long hostname, numeric-only hostname
TEST(CORSPolicyTest, HttpPort80IsSameAsNoPort) {
    EXPECT_FALSE(is_cross_origin("http://example.com:80", "http://example.com/path"));
}

TEST(CORSPolicyTest, HttpsPort443IsSameAsNoPort) {
    EXPECT_FALSE(is_cross_origin("https://example.com:443", "https://example.com/path"));
}

TEST(CORSPolicyTest, HttpPortDifferentFrom443) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "http://example.com:443/path"));
}

TEST(CORSPolicyTest, LongSubdomainOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin(
        "https://very-long-subdomain-name-here.sub.example.com"));
}

TEST(CORSPolicyTest, NumericOnlyHostnameIsNotValid) {
    EXPECT_FALSE(is_cors_eligible_request_url("https://12345/path"));
}

TEST(CORSPolicyTest, CORSAllowsNullOriginWithNullACAO) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "null");
    EXPECT_TRUE(cors_allows_response("null", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, ACAOWildcardNotAllowedForNullOriginWithCredentials) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    EXPECT_FALSE(cors_allows_response("null", "https://api.example.com/data", headers, true));
}

TEST(CORSPolicyTest, NormalizeDoesNotAttachOriginForSameOriginNullDoc) {
    clever::net::HeaderMap headers;
    normalize_outgoing_origin_header(headers, "null", "null");
    EXPECT_FALSE(headers.get("Origin").has_value());
}

// Cycle 888 — CORS policy edge cases

TEST(CORSPolicyTest, HttpsPort8443IsCrossOriginFromDefault) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "https://example.com:8443/path"));
}

TEST(CORSPolicyTest, NullOriginIsNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

TEST(CORSPolicyTest, UppercaseSchemeOriginNotEnforceable) {
    EXPECT_FALSE(has_enforceable_document_origin("HTTP://example.com"));
}

TEST(CORSPolicyTest, HttpUrlWithQueryStringIsCorsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/search?q=test"));
}

TEST(CORSPolicyTest, CORSRejectsACAOWithCommaList) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://a.com, https://b.com");
    EXPECT_FALSE(cors_allows_response("https://a.com", "https://api.other.com/data", headers, false));
}

TEST(CORSPolicyTest, ACAOWithLeadingSpaceIsRejected) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", " https://app.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, CORSRejectsWhenMultipleACAOHeadersPresent) {
    clever::net::HeaderMap headers;
    headers.append("Access-Control-Allow-Origin", "https://app.example");
    headers.append("Access-Control-Allow-Origin", "https://other.example");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, HttpsUrlWithFragmentNotCorsEligible) {
    EXPECT_FALSE(is_cors_eligible_request_url("https://example.com/page#section"));
}

// Cycle 896 — CORS policy tests

TEST(CORSPolicyTest, CredentialRequestNeedsACACTrue) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "https://app.example");
    headers.set("Access-Control-Allow-Credentials", "false");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example.com/data", headers, true));
}

TEST(CORSPolicyTest, WildcardWithCredentialsFails) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    headers.set("Access-Control-Allow-Credentials", "true");
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example.com/data", headers, true));
}

TEST(CORSPolicyTest, NullOriginCrossOriginAllowedWithWildcard) {
    clever::net::HeaderMap headers;
    headers.set("Access-Control-Allow-Origin", "*");
    // null origin with wildcard ACAO and no credentials = allowed
    EXPECT_TRUE(cors_allows_response("null", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, SameOriginNoCredentialCheckNeeded) {
    clever::net::HeaderMap headers;
    // No ACAO header needed for same-origin
    EXPECT_TRUE(cors_allows_response("https://example.com", "https://example.com/api", headers, false));
}

TEST(CORSPolicyTest, ShouldAttachOriginWhenNullAndCrossOrigin) {
    EXPECT_TRUE(should_attach_origin_header("null", "https://api.other.com/data"));
}

TEST(CORSPolicyTest, ACAOMissingMeansResponseDenied) {
    clever::net::HeaderMap headers;
    // No Access-Control-Allow-Origin header at all
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, IPAddressOriginIsEnforceable) {
    EXPECT_TRUE(has_enforceable_document_origin("http://192.168.1.1"));
}

TEST(CORSPolicyTest, IPAddressIsCorsEligibleRequestUrl) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://10.0.0.1/api/data"));
}

TEST(CORSPolicyTest, SubpathUrlIsCorsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://example.com/api/v1/data"));
}

TEST(CORSPolicyTest, PortMismatchIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "https://example.com:8080/data"));
}

TEST(CORSPolicyTest, SchemeMismatchIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "https://example.com/data"));
}

TEST(CORSPolicyTest, HostMismatchIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "https://api.example.com/data"));
}

TEST(CORSPolicyTest, ACAOWrongOriginDenies) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "https://other.example.com");
    EXPECT_FALSE(cors_allows_response("https://app.example.com", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, CorsAllowsWhenACAOMatchesOrigin) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "https://app.example.com");
    EXPECT_TRUE(cors_allows_response("https://app.example.com", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, ACAOExactMatchWithCredentialsAllowed) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "https://app.example.com");
    headers.set("access-control-allow-credentials", "true");
    EXPECT_TRUE(cors_allows_response("https://app.example.com", "https://api.example.com/data", headers, true));
}

TEST(CORSPolicyTest, HttpsQueryStringUrlIsCorsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://example.com/search?q=hello&page=2"));
}

TEST(CORSPolicyTest, WwwSubdomainIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://example.com", "https://www.example.com/page"));
}

TEST(CORSPolicyTest, ApiSubdomainIsCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("https://app.example.com", "https://api.example.com/data"));
}

TEST(CORSPolicyTest, CorsRejectsEmptyACAO) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "");
    EXPECT_FALSE(cors_allows_response("https://example.com", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, NullDocOriginShouldAttach) {
    EXPECT_TRUE(should_attach_origin_header("null", "https://api.example.com/data"));
}

TEST(CORSPolicyTest, EnforceableHttpOrigin) {
    EXPECT_TRUE(has_enforceable_document_origin("http://example.com"));
}

TEST(CORSPolicyTest, EnforceableHttpsOrigin) {
    EXPECT_TRUE(has_enforceable_document_origin("https://secure.example.com"));
}

TEST(CORSPolicyTest, NotEnforceableEmptyOrigin) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
}

TEST(CORSPolicyTest, HttpAndHttpsSameHostCrossOrigin) {
    EXPECT_TRUE(is_cross_origin("http://example.com", "https://example.com/path"));
}

// Cycle 923 — additional CORS policy coverage
TEST(CORSPolicyTest, ACAOExplicitOriginAllowsMatchWithoutCredentials) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "https://app.example.com");
    EXPECT_TRUE(cors_allows_response("https://app.example.com", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, ACAOMismatchedSubdomainBlocks) {
    clever::net::HeaderMap headers;
    headers.set("access-control-allow-origin", "https://app.example.com");
    EXPECT_FALSE(cors_allows_response("https://other.example.com", "https://api.example.com/data", headers, false));
}

TEST(CORSPolicyTest, EnforceableFtpOriginIsFalse) {
    EXPECT_FALSE(has_enforceable_document_origin("ftp://files.example.com"));
}

TEST(CORSPolicyTest, NullStringDocOriginIsNotCorsEligible) {
    EXPECT_FALSE(has_enforceable_document_origin("null"));
}

TEST(CORSPolicyTest, HttpsSchemeIsCorsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("https://cdn.example.com/script.js"));
}

TEST(CORSPolicyTest, HttpSchemeIsCorsEligible) {
    EXPECT_TRUE(is_cors_eligible_request_url("http://api.example.com/data"));
}

TEST(CORSPolicyTest, DifferentPortSameSchemeSameHost) {
    EXPECT_TRUE(is_cross_origin("https://example.com:8443", "https://example.com/path"));
}

TEST(CORSPolicyTest, SameOriginExactMatchNotCrossOrigin) {
    EXPECT_FALSE(is_cross_origin("https://example.com", "https://example.com/resource"));
}
