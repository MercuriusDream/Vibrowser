#include <clever/js/cors_policy.h>

#include <clever/net/header_map.h>

#include <gtest/gtest.h>

using clever::js::cors::cors_allows_response;
using clever::js::cors::has_enforceable_document_origin;
using clever::js::cors::is_cross_origin;
using clever::js::cors::should_attach_origin_header;

TEST(CORSPolicyTest, DocumentOriginEnforcement) {
    EXPECT_FALSE(has_enforceable_document_origin(""));
    EXPECT_FALSE(has_enforceable_document_origin("null"));
    EXPECT_TRUE(has_enforceable_document_origin("https://app.example"));
}

TEST(CORSPolicyTest, CrossOriginDetection) {
    EXPECT_FALSE(is_cross_origin("", "https://api.example/data"));
    EXPECT_FALSE(is_cross_origin("null", "https://api.example/data"));
    EXPECT_FALSE(is_cross_origin("https://app.example", "https://app.example/path"));
    EXPECT_TRUE(is_cross_origin("https://app.example", "https://api.example/path"));
}

TEST(CORSPolicyTest, OriginHeaderAttachmentRule) {
    EXPECT_FALSE(should_attach_origin_header("", "https://api.example/data"));
    EXPECT_FALSE(should_attach_origin_header("https://app.example", "https://app.example/data"));
    EXPECT_TRUE(should_attach_origin_header("https://app.example", "https://api.example/data"));
}

TEST(CORSPolicyTest, SameOriginResponseAlwaysAllowed) {
    clever::net::HeaderMap headers;
    EXPECT_TRUE(cors_allows_response("https://app.example", "https://app.example/data", headers,
                                     false));
}

TEST(CORSPolicyTest, CrossOriginRequiresACAO) {
    clever::net::HeaderMap headers;
    EXPECT_FALSE(cors_allows_response("https://app.example", "https://api.example/data", headers,
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
}
