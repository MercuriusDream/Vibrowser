#include <clever/js/cors_policy.h>

#include <clever/net/header_map.h>

#include <gtest/gtest.h>

using clever::js::cors::cors_allows_response;
using clever::js::cors::has_enforceable_document_origin;
using clever::js::cors::is_cors_eligible_request_url;
using clever::js::cors::is_cross_origin;
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
