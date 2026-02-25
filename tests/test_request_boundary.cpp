// Test: Request/response boundary fixtures
// Story 5.5 acceptance test

#include "browser/net/http_client.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    int failures = 0;

    // =========================================================
    // Section A: Policy boundary fixtures — scheme edge cases
    // =========================================================

    // Fixture A1: Only HTTPS allowed — HTTP rejected
    {
        browser::net::RequestPolicy policy;
        policy.allowed_schemes = {"https"};
        auto r = browser::net::check_request_policy("http://example.com/", policy);
        if (r.allowed || r.violation != browser::net::PolicyViolation::UnsupportedScheme) {
            std::cerr << "FAIL A1: HTTP should be rejected when only HTTPS allowed\n";
            ++failures;
        } else {
            std::cerr << "PASS A1: HTTP rejected when only HTTPS allowed\n";
        }
    }

    // Fixture A2: Only HTTPS allowed — HTTPS accepted
    {
        browser::net::RequestPolicy policy;
        policy.allowed_schemes = {"https"};
        auto r = browser::net::check_request_policy("https://example.com/", policy);
        if (!r.allowed) {
            std::cerr << "FAIL A2: HTTPS should be allowed: " << r.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS A2: HTTPS allowed when only HTTPS allowed\n";
        }
    }

    // Fixture A3: Empty allowed_schemes — everything rejected
    {
        browser::net::RequestPolicy policy;
        policy.allowed_schemes = {};
        auto r = browser::net::check_request_policy("http://example.com/", policy);
        if (r.allowed) {
            std::cerr << "FAIL A3: should reject when no schemes allowed\n";
            ++failures;
        } else {
            std::cerr << "PASS A3: rejected when no schemes allowed\n";
        }
    }

    // Fixture A4: Custom scheme — data: rejected by default
    {
        browser::net::RequestPolicy policy;
        auto r = browser::net::check_request_policy("data:text/html,<h1>hi</h1>", policy);
        if (r.allowed) {
            std::cerr << "FAIL A4: data: should be rejected by default policy\n";
            ++failures;
        } else {
            std::cerr << "PASS A4: data: scheme rejected by default\n";
        }
    }

    // Fixture A5: file:// URL passes default policy
    {
        browser::net::RequestPolicy policy;
        auto r = browser::net::check_request_policy("file:///etc/hosts", policy);
        if (!r.allowed) {
            std::cerr << "FAIL A5: file:// should pass default policy: " << r.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS A5: file:// passes default policy\n";
        }
    }

    // =========================================================
    // Section B: Cross-origin boundary fixtures
    // =========================================================

    // Fixture B1: Different subdomain is cross-origin
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://www.example.com";
        auto r = browser::net::check_request_policy("http://api.example.com/v1", policy);
        if (r.allowed) {
            std::cerr << "FAIL B1: subdomain should be cross-origin\n";
            ++failures;
        } else if (r.violation != browser::net::PolicyViolation::CrossOriginBlocked) {
            std::cerr << "FAIL B1: expected CrossOriginBlocked violation\n";
            ++failures;
        } else {
            std::cerr << "PASS B1: subdomain is cross-origin\n";
        }
    }

    // Fixture B2: Different port is cross-origin
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";
        auto r = browser::net::check_request_policy("http://example.com:8080/api", policy);
        if (r.allowed) {
            std::cerr << "FAIL B2: different port should be cross-origin\n";
            ++failures;
        } else if (r.violation != browser::net::PolicyViolation::CrossOriginBlocked) {
            std::cerr << "FAIL B2: expected CrossOriginBlocked violation\n";
            ++failures;
        } else {
            std::cerr << "PASS B2: different port is cross-origin\n";
        }
    }

    // Fixture B3: HTTP vs HTTPS is cross-origin
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";
        auto r = browser::net::check_request_policy("https://example.com/page", policy);
        if (r.allowed) {
            std::cerr << "FAIL B3: different scheme should be cross-origin\n";
            ++failures;
        } else if (r.violation != browser::net::PolicyViolation::CrossOriginBlocked) {
            std::cerr << "FAIL B3: expected CrossOriginBlocked violation\n";
            ++failures;
        } else {
            std::cerr << "PASS B3: HTTP vs HTTPS is cross-origin\n";
        }
    }

    // Fixture B4: Same origin with path variation allowed
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";
        auto r = browser::net::check_request_policy("http://example.com/deeply/nested/page?q=1#frag", policy);
        if (!r.allowed) {
            std::cerr << "FAIL B4: same origin with different path should be allowed: " << r.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS B4: same origin with path variation allowed\n";
        }
    }

    // Fixture B5: Empty origin means no cross-origin check
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "";
        auto r = browser::net::check_request_policy("http://any-domain.com/page", policy);
        if (!r.allowed) {
            std::cerr << "FAIL B5: empty origin should skip cross-origin check: " << r.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS B5: empty origin skips cross-origin check\n";
        }
    }

    // =========================================================
    // Section C: Cache boundary fixtures
    // =========================================================

    // Fixture C1: NoCache policy never caches
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::NoCache);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "cached";
        cache.store("http://example.com/", resp);
        browser::net::Response out;
        bool found = cache.lookup("http://example.com/", out);
        if (found) {
            std::cerr << "FAIL C1: NoCache should never return cached entry\n";
            ++failures;
        } else if (cache.size() != 0) {
            std::cerr << "FAIL C1: NoCache should not store entries\n";
            ++failures;
        } else {
            std::cerr << "PASS C1: NoCache never caches\n";
        }
    }

    // Fixture C2: CacheAll stores and retrieves
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "hello";
        cache.store("http://example.com/page", resp);
        browser::net::Response out;
        bool found = cache.lookup("http://example.com/page", out);
        if (!found || out.body != "hello") {
            std::cerr << "FAIL C2: CacheAll should store and retrieve\n";
            ++failures;
        } else {
            std::cerr << "PASS C2: CacheAll stores and retrieves\n";
        }
    }

    // Fixture C3: Error responses not cached
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 0;
        resp.error = "connection failed";
        cache.store("http://example.com/fail", resp);
        browser::net::Response out;
        bool found = cache.lookup("http://example.com/fail", out);
        if (found) {
            std::cerr << "FAIL C3: error responses should not be cached\n";
            ++failures;
        } else {
            std::cerr << "PASS C3: error responses not cached\n";
        }
    }

    // Fixture C4: Cache clear removes all entries
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "a";
        cache.store("http://a.com/", resp);
        resp.body = "b";
        cache.store("http://b.com/", resp);
        if (cache.size() != 2) {
            std::cerr << "FAIL C4: expected 2 entries before clear\n";
            ++failures;
        } else {
            cache.clear();
            if (cache.size() != 0) {
                std::cerr << "FAIL C4: clear should remove all entries\n";
                ++failures;
            } else {
                std::cerr << "PASS C4: cache clear removes all entries\n";
            }
        }
    }

    // Fixture C5: Policy change from CacheAll to NoCache — lookup fails
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "data";
        cache.store("http://example.com/", resp);
        cache.set_policy(browser::net::CachePolicy::NoCache);
        browser::net::Response out;
        bool found = cache.lookup("http://example.com/", out);
        if (found) {
            std::cerr << "FAIL C5: NoCache policy should block lookup\n";
            ++failures;
        } else {
            std::cerr << "PASS C5: policy change blocks lookup\n";
        }
    }

    // =========================================================
    // Section D: Transaction lifecycle boundary fixtures
    // =========================================================

    // Fixture D1: Transaction events record in order
    {
        browser::net::RequestTransaction txn;
        txn.record(browser::net::RequestStage::Created);
        txn.record(browser::net::RequestStage::Dispatched);
        txn.record(browser::net::RequestStage::Received);
        txn.record(browser::net::RequestStage::Complete, "ok");

        if (txn.events.size() != 4) {
            std::cerr << "FAIL D1: expected 4 events, got " << txn.events.size() << "\n";
            ++failures;
        } else if (txn.events[0].stage != browser::net::RequestStage::Created ||
                   txn.events[1].stage != browser::net::RequestStage::Dispatched ||
                   txn.events[2].stage != browser::net::RequestStage::Received ||
                   txn.events[3].stage != browser::net::RequestStage::Complete) {
            std::cerr << "FAIL D1: events in wrong order\n";
            ++failures;
        } else if (txn.events[3].detail != "ok") {
            std::cerr << "FAIL D1: detail not recorded\n";
            ++failures;
        } else {
            std::cerr << "PASS D1: transaction events in correct order\n";
        }
    }

    // Fixture D2: Stage names are all non-null
    {
        bool ok = true;
        const browser::net::RequestStage stages[] = {
            browser::net::RequestStage::Created,
            browser::net::RequestStage::Dispatched,
            browser::net::RequestStage::Received,
            browser::net::RequestStage::Complete,
            browser::net::RequestStage::Error,
        };
        for (auto s : stages) {
            const char* name = browser::net::request_stage_name(s);
            if (!name || std::string(name).empty()) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            std::cerr << "FAIL D2: stage names should be non-empty\n";
            ++failures;
        } else {
            std::cerr << "PASS D2: all stage names are non-empty\n";
        }
    }

    // Fixture D3: Request method names
    {
        bool ok = true;
        if (std::string(browser::net::request_method_name(browser::net::RequestMethod::Get)) != "GET") ok = false;
        if (std::string(browser::net::request_method_name(browser::net::RequestMethod::Head)) != "HEAD") ok = false;
        if (!ok) {
            std::cerr << "FAIL D3: request method names incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS D3: request method names correct\n";
        }
    }

    // Fixture D4: Transaction timestamps are monotonically non-decreasing
    {
        browser::net::RequestTransaction txn;
        txn.record(browser::net::RequestStage::Created);
        txn.record(browser::net::RequestStage::Dispatched);
        txn.record(browser::net::RequestStage::Complete);

        bool monotonic = true;
        for (std::size_t i = 1; i < txn.events.size(); ++i) {
            if (txn.events[i].timestamp < txn.events[i - 1].timestamp) {
                monotonic = false;
                break;
            }
        }
        if (!monotonic) {
            std::cerr << "FAIL D4: timestamps should be monotonically non-decreasing\n";
            ++failures;
        } else {
            std::cerr << "PASS D4: timestamps are monotonic\n";
        }
    }

    // =========================================================
    // Section E: Combined policy + cache boundary fixtures
    // =========================================================

    // Fixture E1: Policy rejects URL — should not reach cache
    {
        browser::net::RequestPolicy policy;
        policy.allowed_schemes = {"https"};
        auto check = browser::net::check_request_policy("http://example.com/", policy);

        // Simulating: if policy rejects, we shouldn't even look up cache
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "cached content";
        cache.store("http://example.com/", resp);

        if (check.allowed) {
            std::cerr << "FAIL E1: policy should reject\n";
            ++failures;
        } else {
            // Even though cache has it, policy blocks it first
            std::cerr << "PASS E1: policy rejects before cache lookup\n";
        }
    }

    // Fixture E2: Deterministic — policy + cache produce same results
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";

        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "deterministic";
        cache.store("http://example.com/page", resp);

        // Run twice
        auto c1 = browser::net::check_request_policy("http://example.com/page", policy);
        auto c2 = browser::net::check_request_policy("http://example.com/page", policy);
        browser::net::Response out1, out2;
        bool f1 = cache.lookup("http://example.com/page", out1);
        bool f2 = cache.lookup("http://example.com/page", out2);

        if (c1.allowed != c2.allowed || c1.violation != c2.violation ||
            f1 != f2 || out1.body != out2.body) {
            std::cerr << "FAIL E2: policy+cache not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS E2: policy+cache deterministic\n";
        }
    }

    // Fixture E3: Multiple URLs — mixed allow/deny
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";

        std::vector<std::pair<std::string, bool>> cases = {
            {"http://example.com/ok", true},
            {"http://example.com/also-ok", true},
            {"http://other.com/blocked", false},
            {"http://example.com/still-ok?q=1", true},
            {"https://example.com/wrong-scheme-origin", false},
        };

        bool ok = true;
        for (const auto& [url, expected] : cases) {
            auto r = browser::net::check_request_policy(url, policy);
            if (r.allowed != expected) {
                std::cerr << "FAIL E3: " << url << " expected allowed=" << expected
                          << " got " << r.allowed << "\n";
                ok = false;
                ++failures;
                break;
            }
        }
        if (ok) {
            std::cerr << "PASS E3: mixed allow/deny batch correct\n";
        }
    }

    // Summary
    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll request boundary fixture tests PASSED\n";
    return 0;
}
