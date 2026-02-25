// Test: Deterministic caching policy behavior
// Story 5.2 acceptance test

#include "browser/net/http_client.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: CachePolicy names
    {
        bool ok = true;
        if (std::string(browser::net::cache_policy_name(browser::net::CachePolicy::NoCache)) != "NoCache") ok = false;
        if (std::string(browser::net::cache_policy_name(browser::net::CachePolicy::CacheAll)) != "CacheAll") ok = false;

        if (!ok) {
            std::cerr << "FAIL: cache_policy_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: cache_policy_name returns correct values\n";
        }
    }

    // Test 2: NoCache policy never stores or returns cached responses
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::NoCache);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "test body";

        cache.store("http://example.com/test", resp);

        browser::net::Response out;
        if (cache.lookup("http://example.com/test", out)) {
            std::cerr << "FAIL: NoCache should not return cached response\n";
            ++failures;
        } else {
            std::cerr << "PASS: NoCache policy doesn't cache\n";
        }

        if (cache.size() != 0) {
            std::cerr << "FAIL: NoCache size should be 0\n";
            ++failures;
        } else {
            std::cerr << "PASS: NoCache size is 0\n";
        }
    }

    // Test 3: CacheAll policy stores and returns cached responses
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "cached body";

        cache.store("http://example.com/page", resp);

        browser::net::Response out;
        if (!cache.lookup("http://example.com/page", out)) {
            std::cerr << "FAIL: CacheAll should find cached response\n";
            ++failures;
        } else if (out.status_code != 200 || out.body != "cached body") {
            std::cerr << "FAIL: cached response data mismatch\n";
            ++failures;
        } else {
            std::cerr << "PASS: CacheAll stores and retrieves correctly\n";
        }
    }

    // Test 4: Cache returns same response deterministically
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "deterministic";
        resp.headers["content-type"] = "text/html";

        cache.store("http://example.com/det", resp);

        browser::net::Response out1, out2;
        cache.lookup("http://example.com/det", out1);
        cache.lookup("http://example.com/det", out2);

        if (out1.body != out2.body || out1.status_code != out2.status_code ||
            out1.headers != out2.headers) {
            std::cerr << "FAIL: cached lookups not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: cached lookups are deterministic\n";
        }
    }

    // Test 5: Cache does not store error responses
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.error = "Connection refused";

        cache.store("http://example.com/error", resp);

        browser::net::Response out;
        if (cache.lookup("http://example.com/error", out)) {
            std::cerr << "FAIL: should not cache error responses\n";
            ++failures;
        } else {
            std::cerr << "PASS: error responses not cached\n";
        }
    }

    // Test 6: Cache miss for unknown URL
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response out;
        if (cache.lookup("http://example.com/missing", out)) {
            std::cerr << "FAIL: lookup should miss for unknown URL\n";
            ++failures;
        } else {
            std::cerr << "PASS: cache miss for unknown URL\n";
        }
    }

    // Test 7: clear() removes all entries
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;

        cache.store("http://a.com", resp);
        cache.store("http://b.com", resp);

        if (cache.size() != 2) {
            std::cerr << "FAIL: expected 2 entries\n";
            ++failures;
        } else {
            cache.clear();
            if (cache.size() != 0) {
                std::cerr << "FAIL: clear didn't empty cache\n";
                ++failures;
            } else {
                std::cerr << "PASS: clear removes all entries\n";
            }
        }
    }

    // Test 8: set_policy changes behavior
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "test";

        cache.store("http://example.com/x", resp);
        if (cache.size() != 1) {
            std::cerr << "FAIL: expected 1 entry\n";
            ++failures;
        }

        cache.set_policy(browser::net::CachePolicy::NoCache);
        browser::net::Response out;
        if (cache.lookup("http://example.com/x", out)) {
            std::cerr << "FAIL: NoCache should prevent lookup\n";
            ++failures;
        } else {
            std::cerr << "PASS: set_policy changes cache behavior\n";
        }
    }

    // Test 9: Multiple URLs cached independently
    {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);

        browser::net::Response r1;
        r1.status_code = 200;
        r1.body = "page1";

        browser::net::Response r2;
        r2.status_code = 404;
        r2.body = "not found";

        cache.store("http://a.com", r1);
        cache.store("http://b.com", r2);

        browser::net::Response out1, out2;
        cache.lookup("http://a.com", out1);
        cache.lookup("http://b.com", out2);

        if (out1.body != "page1" || out2.body != "not found") {
            std::cerr << "FAIL: URLs not cached independently\n";
            ++failures;
        } else {
            std::cerr << "PASS: URLs cached independently\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll cache policy tests PASSED\n";
    return 0;
}
