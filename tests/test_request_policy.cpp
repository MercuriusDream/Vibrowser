// Test: Enforce redirects, origin boundaries, and request constraints
// Story 5.4 acceptance test

#include "browser/net/http_client.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: PolicyViolation names
    {
        bool ok = true;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::None)) != "None") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::TooManyRedirects)) != "TooManyRedirects") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::CrossOriginBlocked)) != "CrossOriginBlocked") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::CorsResponseBlocked)) != "CorsResponseBlocked") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::CspConnectSrcBlocked)) != "CspConnectSrcBlocked") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::UnsupportedScheme)) != "UnsupportedScheme") ok = false;
        if (std::string(browser::net::policy_violation_name(browser::net::PolicyViolation::EmptyUrl)) != "EmptyUrl") ok = false;

        if (!ok) {
            std::cerr << "FAIL: policy_violation_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: policy_violation_name correct\n";
        }
    }

    // Test 2: Valid URL passes default policy
    {
        browser::net::RequestPolicy policy;
        auto result = browser::net::check_request_policy("http://example.com/page", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: valid URL should be allowed: " << result.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: valid URL passes default policy\n";
        }
    }

    // Test 3: Empty URL is rejected
    {
        browser::net::RequestPolicy policy;
        auto result = browser::net::check_request_policy("", policy);
        if (result.allowed) {
            std::cerr << "FAIL: empty URL should be rejected\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::EmptyUrl) {
            std::cerr << "FAIL: expected EmptyUrl violation\n";
            ++failures;
        } else {
            std::cerr << "PASS: empty URL rejected with EmptyUrl violation\n";
        }
    }

    // Test 4: Unsupported scheme is rejected
    {
        browser::net::RequestPolicy policy;
        policy.allowed_schemes = {"http", "https"};
        auto result = browser::net::check_request_policy("ftp://example.com/file", policy);
        if (result.allowed) {
            std::cerr << "FAIL: ftp scheme should be rejected\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::UnsupportedScheme) {
            std::cerr << "FAIL: expected UnsupportedScheme violation\n";
            ++failures;
        } else {
            std::cerr << "PASS: unsupported scheme rejected\n";
        }
    }

    // Test 5: Cross-origin blocked when not allowed
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";
        auto result = browser::net::check_request_policy("http://other.com/page", policy);
        if (result.allowed) {
            std::cerr << "FAIL: cross-origin should be blocked\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::CrossOriginBlocked) {
            std::cerr << "FAIL: expected CrossOriginBlocked violation\n";
            ++failures;
        } else {
            std::cerr << "PASS: cross-origin request blocked\n";
        }
    }

    // Test 6: Same-origin allowed when cross-origin blocked
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";
        auto result = browser::net::check_request_policy("http://example.com/other", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: same-origin should be allowed: " << result.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: same-origin allowed when cross-origin blocked\n";
        }
    }

    // Test 7: Cross-origin allowed when policy permits
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = true;
        policy.origin = "http://example.com";
        auto result = browser::net::check_request_policy("http://other.com/page", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: cross-origin should be allowed with permissive policy\n";
            ++failures;
        } else {
            std::cerr << "PASS: cross-origin allowed with permissive policy\n";
        }
    }

    // Test 8: HTTPS scheme allowed by default
    {
        browser::net::RequestPolicy policy;
        auto result = browser::net::check_request_policy("https://secure.example.com", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: HTTPS should be allowed by default\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTPS allowed by default\n";
        }
    }

    // Test 9: file:// scheme allowed by default
    {
        browser::net::RequestPolicy policy;
        auto result = browser::net::check_request_policy("file:///tmp/test.html", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: file:// should be allowed by default: " << result.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: file:// allowed by default\n";
        }
    }

    // Test 10: Deterministic — same check produces same result
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";

        auto r1 = browser::net::check_request_policy("http://other.com/x", policy);
        auto r2 = browser::net::check_request_policy("http://other.com/x", policy);

        if (r1.allowed != r2.allowed || r1.violation != r2.violation) {
            std::cerr << "FAIL: policy check not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: policy check is deterministic\n";
        }
    }

    // Test 11: build_request_headers_for_policy sets Origin for cross-origin requests
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        auto headers = browser::net::build_request_headers_for_policy("https://api.example.com/data", policy);
        auto it = headers.find("Origin");
        if (it == headers.end()) {
            std::cerr << "FAIL: expected Origin header for cross-origin request\n";
            ++failures;
        } else if (it->second != "https://app.example.com") {
            std::cerr << "FAIL: incorrect Origin header value\n";
            ++failures;
        } else {
            std::cerr << "PASS: Origin header attached for cross-origin request\n";
        }
    }

    // Test 12: build_request_headers_for_policy omits Origin for same-origin requests
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        auto headers = browser::net::build_request_headers_for_policy("https://app.example.com/page", policy);
        if (!headers.empty()) {
            std::cerr << "FAIL: same-origin request should not include Origin header\n";
            ++failures;
        } else {
            std::cerr << "PASS: same-origin request omits Origin header\n";
        }
    }

    // Test 13: check_cors_response_policy rejects cross-origin response without ACAO
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        auto result = browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: expected missing ACAO to be blocked\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::CorsResponseBlocked) {
            std::cerr << "FAIL: expected CorsResponseBlocked violation\n";
            ++failures;
        } else {
            std::cerr << "PASS: missing ACAO blocked for cross-origin response\n";
        }
    }

    // Test 14: check_cors_response_policy allows explicit origin in ACAO
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com";
        auto result = browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (!result.allowed) {
            std::cerr << "FAIL: explicit ACAO origin should be allowed\n";
            ++failures;
        } else {
            std::cerr << "PASS: explicit ACAO origin allowed\n";
        }
    }

    // Test 15: check_cors_response_policy allows wildcard ACAO
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "*";
        auto result = browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (!result.allowed) {
            std::cerr << "FAIL: wildcard ACAO should be allowed\n";
            ++failures;
        } else {
            std::cerr << "PASS: wildcard ACAO allowed\n";
        }
    }

    // Test 16: connect-src 'none' blocks all network requests
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"'none'"};
        auto result = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (result.allowed) {
            std::cerr << "FAIL: connect-src 'none' should block request\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::CspConnectSrcBlocked) {
            std::cerr << "FAIL: expected CspConnectSrcBlocked for 'none'\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src 'none' blocks request\n";
        }
    }

    // Test 17: connect-src 'self' allows same-origin requests
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.origin = "https://app.example.com";
        policy.connect_src_sources = {"'self'"};
        auto result = browser::net::check_request_policy("https://app.example.com/api", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: connect-src 'self' should allow same-origin\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src 'self' allows same-origin request\n";
        }
    }

    // Test 18: connect-src scheme source allows matching scheme
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https:"};
        auto result = browser::net::check_request_policy("https://third-party.example/path", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: https: scheme source should allow HTTPS request\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src scheme source allows matching URL scheme\n";
        }
    }

    // Test 19: connect-src explicit origin blocks non-listed origins
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com"};
        auto result = browser::net::check_request_policy("https://cdn.example.com/data", policy);
        if (result.allowed) {
            std::cerr << "FAIL: non-listed origin should be blocked by connect-src\n";
            ++failures;
        } else if (result.violation != browser::net::PolicyViolation::CspConnectSrcBlocked) {
            std::cerr << "FAIL: expected CspConnectSrcBlocked for non-listed origin\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src blocks non-listed origins\n";
        }
    }

    // Test 19b: scheme-qualified host-source without port only matches default port
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com"};
        auto allowed = browser::net::check_request_policy("https://api.example.com/data", policy);
        auto blocked = browser::net::check_request_policy("https://api.example.com:8443/data", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: scheme-qualified host-source should allow default port\n";
            ++failures;
        } else if (blocked.allowed || blocked.violation != browser::net::PolicyViolation::CspConnectSrcBlocked) {
            std::cerr << "FAIL: scheme-qualified host-source should block non-default port\n";
            ++failures;
        } else {
            std::cerr << "PASS: scheme-qualified host-source enforces default port when unspecified\n";
        }
    }

    // Test 20: connect-src host-source without scheme allows matching host
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"api.example.com"};
        auto result = browser::net::check_request_policy("https://api.example.com/v1/data", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: host-source should allow matching host\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src host-source allows matching host\n";
        }
    }

    // Test 20b: host-source without scheme uses policy origin scheme when available
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.allowed_schemes = {"http", "https", "file", "ws", "wss"};
        policy.origin = "https://app.example.com";
        policy.connect_src_sources = {"api.example.com"};
        auto allowed = browser::net::check_request_policy("https://api.example.com/socket", policy);
        auto blocked = browser::net::check_request_policy("wss://api.example.com/socket", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: scheme-less host-source should allow policy-origin scheme\n";
            ++failures;
        } else if (blocked.allowed ||
                   blocked.violation != browser::net::PolicyViolation::CspConnectSrcBlocked) {
            std::cerr << "FAIL: scheme-less host-source should block cross-scheme request\n";
            ++failures;
        } else {
            std::cerr << "PASS: scheme-less host-source honors policy origin scheme\n";
        }
    }

    // Test 20c: host-source without scheme enforces default port for policy origin scheme
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.origin = "https://app.example.com";
        policy.connect_src_sources = {"api.example.com"};
        auto allowed = browser::net::check_request_policy("https://api.example.com/data", policy);
        auto blocked = browser::net::check_request_policy("https://api.example.com:8443/data", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: scheme-less host-source should allow default port for inferred scheme\n";
            ++failures;
        } else if (blocked.allowed ||
                   blocked.violation != browser::net::PolicyViolation::CspConnectSrcBlocked) {
            std::cerr << "FAIL: scheme-less host-source should block non-default port for inferred scheme\n";
            ++failures;
        } else {
            std::cerr << "PASS: scheme-less host-source enforces inferred default port\n";
        }
    }

    // Test 21: connect-src wildcard host-source allows subdomains but not apex host
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"*.example.com"};
        auto allowed = browser::net::check_request_policy("https://cdn.example.com/asset.js", policy);
        auto blocked = browser::net::check_request_policy("https://example.com/index.html", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: wildcard host-source should allow subdomain\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: wildcard host-source should not allow apex host\n";
            ++failures;
        } else {
            std::cerr << "PASS: wildcard host-source matches only subdomains\n";
        }
    }

    // Test 22: connect-src explicit port only allows matching port
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com:8443"};
        auto allowed = browser::net::check_request_policy("https://api.example.com:8443/data", policy);
        auto blocked = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: explicit port source should allow matching port\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: explicit port source should block default-port request\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src explicit port enforces port match\n";
        }
    }

    // Test 23: connect-src wildcard port allows any port for matching host
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com:*"};
        auto allowed_non_default =
            browser::net::check_request_policy("https://api.example.com:9443/data", policy);
        auto allowed_default =
            browser::net::check_request_policy("https://api.example.com/data", policy);
        auto blocked_other_host =
            browser::net::check_request_policy("https://cdn.example.com:9443/data", policy);
        if (!allowed_non_default.allowed || !allowed_default.allowed) {
            std::cerr << "FAIL: wildcard port should allow matching host on any port\n";
            ++failures;
        } else if (blocked_other_host.allowed) {
            std::cerr << "FAIL: wildcard port should not allow non-matching host\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src wildcard port enforces host while allowing all ports\n";
        }
    }

    // Test 23b: connect-src IPv6 host-source matches bracketed URL host
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://[::1]"};
        auto allowed = browser::net::check_request_policy("https://[::1]/data", policy);
        auto blocked = browser::net::check_request_policy("https://[::2]/data", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: IPv6 host-source should allow matching bracketed host\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: IPv6 host-source should block non-matching host\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src IPv6 host-source matches canonical host form\n";
        }
    }

    // Test 23c: connect-src IPv6 wildcard port allows any port for matching host
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://[::1]:*"};
        auto allowed_default = browser::net::check_request_policy("https://[::1]/data", policy);
        auto allowed_non_default =
            browser::net::check_request_policy("https://[::1]:9443/data", policy);
        auto blocked_other_host =
            browser::net::check_request_policy("https://[::2]:9443/data", policy);
        if (!allowed_default.allowed || !allowed_non_default.allowed) {
            std::cerr << "FAIL: IPv6 wildcard port should allow matching host on any port\n";
            ++failures;
        } else if (blocked_other_host.allowed) {
            std::cerr << "FAIL: IPv6 wildcard port should block non-matching host\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src IPv6 wildcard port enforces host while allowing all ports\n";
        }
    }

    // Test 24: default-src is used when connect-src is not specified
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.origin = "https://app.example.com";
        policy.default_src_sources = {"'self'"};
        auto allowed = browser::net::check_request_policy("https://app.example.com/api", policy);
        auto blocked = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: default-src fallback should allow same-origin for 'self'\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: default-src fallback should block non-self origin\n";
            ++failures;
        } else {
            std::cerr << "PASS: default-src fallback enforces connect-src behavior when connect-src unset\n";
        }
    }

    // Test 25: connect-src overrides default-src when both are present
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.default_src_sources = {"*"};
        policy.connect_src_sources = {"'none'"};
        auto result = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (result.allowed) {
            std::cerr << "FAIL: connect-src should take precedence over default-src fallback\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src takes precedence over default-src fallback\n";
        }
    }

    // Test 26: cross-origin check treats default ports as same-origin
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "https://app.example.com:443";
        auto result = browser::net::check_request_policy("https://app.example.com/data", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: explicit default port should still be same-origin\n";
            ++failures;
        } else {
            std::cerr << "PASS: default-port origin normalization preserves same-origin\n";
        }
    }

    // Test 27: connect-src 'self' matches normalized origin value
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.origin = "HTTPS://APP.EXAMPLE.COM:443";
        policy.connect_src_sources = {"'self'"};
        auto result = browser::net::check_request_policy("https://app.example.com/api", policy);
        if (!result.allowed) {
            std::cerr << "FAIL: connect-src 'self' should normalize origin case/default port\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src 'self' uses canonical origin comparison\n";
        }
    }

    // Test 28: CORS response policy normalizes ACAO origin case/default port
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com:443";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "HTTPS://APP.EXAMPLE.COM";
        auto result = browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (!result.allowed) {
            std::cerr << "FAIL: ACAO origin normalization should allow equivalent origin values\n";
            ++failures;
        } else {
            std::cerr << "PASS: ACAO origin comparison is canonicalized\n";
        }
    }

    // Test 29: Origin header serialization uses canonical origin
    {
        browser::net::RequestPolicy policy;
        policy.origin = "HTTPS://APP.EXAMPLE.COM:443";
        auto headers = browser::net::build_request_headers_for_policy("https://api.example.com/data", policy);
        auto it = headers.find("Origin");
        if (it == headers.end()) {
            std::cerr << "FAIL: expected Origin header for cross-origin request\n";
            ++failures;
        } else if (it->second != "https://app.example.com") {
            std::cerr << "FAIL: expected canonicalized Origin header value\n";
            ++failures;
        } else {
            std::cerr << "PASS: Origin header uses canonical origin serialization\n";
        }
    }

    // Test 30: credentialed CORS rejects wildcard ACAO
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        policy.credentials_mode_include = true;
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "*";
        response.headers["access-control-allow-credentials"] = "true";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: credentialed CORS should reject wildcard ACAO\n";
            ++failures;
        } else {
            std::cerr << "PASS: credentialed CORS rejects wildcard ACAO\n";
        }
    }

    // Test 31: credentialed CORS requires ACAC=true for explicit origin
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        policy.credentials_mode_include = true;
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: credentialed CORS should require ACAC=true\n";
            ++failures;
        } else {
            std::cerr << "PASS: credentialed CORS requires ACAC=true\n";
        }
    }

    // Test 32: credentialed CORS allows explicit origin with ACAC=true
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        policy.credentials_mode_include = true;
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com";
        response.headers["access-control-allow-credentials"] = "true";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (!result.allowed) {
            std::cerr << "FAIL: credentialed CORS should allow explicit origin + ACAC=true\n";
            ++failures;
        } else {
            std::cerr << "PASS: credentialed CORS allows explicit origin + ACAC=true\n";
        }
    }

    // Test 33: CORS rejects multi-valued ACAO response header
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] =
            "https://app.example.com, https://other.example.com";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: multi-valued ACAO should be rejected\n";
            ++failures;
        } else {
            std::cerr << "PASS: multi-valued ACAO is rejected\n";
        }
    }

    // Test 34: CORS same-origin bypass uses effective URL
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        auto result = browser::net::check_cors_response_policy(
            "https://cdn.example.com/redirect-target", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: cross-origin effective URL should require ACAO\n";
            ++failures;
        } else {
            std::cerr << "PASS: effective cross-origin URL enforces ACAO gate\n";
        }
    }

    // Test 35: CORS accepts case-variant ACAO header name
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["Access-Control-Allow-Origin"] = "https://app.example.com";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (!result.allowed) {
            std::cerr << "FAIL: expected case-variant ACAO header name to be recognized\n";
            ++failures;
        } else {
            std::cerr << "PASS: case-variant ACAO header name is recognized\n";
        }
    }

    // Test 36: CORS rejects malformed ACAO with trailing comma
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com,";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: malformed ACAO with trailing comma should be rejected\n";
            ++failures;
        } else {
            std::cerr << "PASS: malformed ACAO with trailing comma is rejected\n";
        }
    }

    // Test 37: credentialed CORS requires exact-case ACAC=true value
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        policy.credentials_mode_include = true;
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com";
        response.headers["access-control-allow-credentials"] = "TRUE";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: credentialed CORS should reject non-literal ACAC value\n";
            ++failures;
        } else {
            std::cerr << "PASS: credentialed CORS enforces literal ACAC=true value\n";
        }
    }

    // Test 38: CORS rejects ACAO origin values that contain a trailing slash/path
    {
        browser::net::RequestPolicy policy;
        policy.origin = "https://app.example.com";
        browser::net::Response response;
        response.headers["access-control-allow-origin"] = "https://app.example.com/";
        auto result =
            browser::net::check_cors_response_policy("https://api.example.com/data", response, policy);
        if (result.allowed) {
            std::cerr << "FAIL: ACAO serialized-origin check should reject trailing slash/path\n";
            ++failures;
        } else {
            std::cerr << "PASS: ACAO rejects trailing slash/path origin values\n";
        }
    }

    // Test 39: connect-src host-source with trailing-slash path uses prefix matching
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com/v1/"};
        auto allowed =
            browser::net::check_request_policy("https://api.example.com/v1/users?id=1", policy);
        auto blocked =
            browser::net::check_request_policy("https://api.example.com/v2/users", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: path-prefix source should allow matching request path\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: path-prefix source should block non-matching request path\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src path-prefix source enforces request path prefix\n";
        }
    }

    // Test 40: connect-src host-source path without trailing slash requires exact path match
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com/v1"};
        auto allowed = browser::net::check_request_policy("https://api.example.com/v1", policy);
        auto blocked = browser::net::check_request_policy("https://api.example.com/v1/users", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: exact-path source should allow exact request path\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: exact-path source should block non-exact request path\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src exact-path source enforces exact request path\n";
        }
    }

    // Test 41: connect-src path matching normalizes dot-segments to prevent traversal bypass
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com/v1/"};
        auto allowed =
            browser::net::check_request_policy("https://api.example.com/v1/./users", policy);
        auto blocked =
            browser::net::check_request_policy("https://api.example.com/v1/../admin", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: normalized in-scope path should remain allowed\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: dot-segment traversal should not bypass connect-src path scope\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src path matching blocks traversal after normalization\n";
        }
    }

    // Test 42: connect-src path matching decodes unreserved percent-encoding before normalization
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com/v1/"};
        auto blocked =
            browser::net::check_request_policy("https://api.example.com/v1/%2e%2e/admin", policy);
        if (blocked.allowed) {
            std::cerr << "FAIL: encoded dot-segment traversal should not bypass connect-src path scope\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src blocks encoded traversal after percent-decoding\n";
        }
    }

    // Test 43: wss host-source without explicit port enforces default secure WebSocket port
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.allowed_schemes = {"http", "https", "file", "ws", "wss"};
        policy.connect_src_sources = {"wss://socket.example.com"};
        auto allowed = browser::net::check_request_policy("wss://socket.example.com/chat", policy);
        auto blocked =
            browser::net::check_request_policy("wss://socket.example.com:8443/chat", policy);
        if (!allowed.allowed) {
            std::cerr << "FAIL: scheme-qualified wss source should allow default secure websocket port\n";
            ++failures;
        } else if (blocked.allowed) {
            std::cerr << "FAIL: scheme-qualified wss source should block non-default websocket port\n";
            ++failures;
        } else {
            std::cerr << "PASS: scheme-qualified wss source enforces default websocket port when unspecified\n";
        }
    }

    // Test 44: connect-src host-source with invalid explicit port 0 is rejected
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com:0"};
        auto blocked = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (blocked.allowed) {
            std::cerr << "FAIL: host-source with explicit port 0 must be treated as invalid and blocked\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src rejects host-sources with explicit port 0\n";
        }
    }

    // Test 45: connect-src host-source with out-of-range explicit port is rejected
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"https://api.example.com:70000"};
        auto blocked = browser::net::check_request_policy("https://api.example.com/data", policy);
        if (blocked.allowed) {
            std::cerr << "FAIL: host-source with out-of-range port must be treated as invalid and blocked\n";
            ++failures;
        } else {
            std::cerr << "PASS: connect-src rejects host-sources with out-of-range ports\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll request policy tests PASSED\n";
    return 0;
}
