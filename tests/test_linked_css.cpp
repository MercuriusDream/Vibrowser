// Test: Load linked CSS resources with deterministic fallback
// Story 5.3 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Extract <link rel="stylesheet"> references
    {
        const std::string html = R"(
            <html><head>
                <link rel="stylesheet" href="style.css"/>
                <link rel="stylesheet" href="theme.css"/>
            </head><body></body></html>
        )";

        auto dom = browser::html::parse_html(html);
        auto refs = browser::css::extract_linked_css(*dom);

        if (refs.size() != 2) {
            std::cerr << "FAIL: expected 2 link refs, got " << refs.size() << "\n";
            ++failures;
        } else if (refs[0].href != "style.css" || refs[1].href != "theme.css") {
            std::cerr << "FAIL: wrong href values\n";
            ++failures;
        } else {
            std::cerr << "PASS: extract_linked_css finds link refs\n";
        }
    }

    // Test 2: Extract <style> blocks
    {
        const std::string html = R"(
            <html><head>
                <style>h1 { color: red; }</style>
            </head><body></body></html>
        )";

        auto dom = browser::html::parse_html(html);
        auto refs = browser::css::extract_linked_css(*dom);

        if (refs.empty()) {
            std::cerr << "FAIL: expected style refs\n";
            ++failures;
        } else if (refs[0].tag != "style") {
            std::cerr << "FAIL: expected tag 'style'\n";
            ++failures;
        } else {
            std::cerr << "PASS: extract_linked_css finds style blocks\n";
        }
    }

    // Test 3: load_linked_css merges inline CSS with style blocks
    {
        const std::string html = R"(
            <html><head>
                <style>p { font-size: 14px; }</style>
            </head><body></body></html>
        )";

        auto dom = browser::html::parse_html(html);
        auto result = browser::css::load_linked_css(*dom, "h1 { color: blue; }");

        if (result.merged.rules.empty()) {
            std::cerr << "FAIL: merged stylesheet has no rules\n";
            ++failures;
        } else if (result.merged.rules.size() < 2) {
            std::cerr << "FAIL: expected at least 2 rules, got "
                      << result.merged.rules.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: load_linked_css merges CSS sources\n";
        }
    }

    // Test 4: External <link> generates fallback warning
    {
        const std::string html = R"(
            <html><head>
                <link rel="stylesheet" href="http://example.com/style.css"/>
            </head><body></body></html>
        )";

        auto dom = browser::html::parse_html(html);
        auto result = browser::css::load_linked_css(*dom, "");

        if (result.warnings.empty()) {
            std::cerr << "FAIL: expected fallback warning for external link\n";
            ++failures;
        } else if (result.failed_urls.empty()) {
            std::cerr << "FAIL: expected failed URL recorded\n";
            ++failures;
        } else if (result.failed_urls[0] != "http://example.com/style.css") {
            std::cerr << "FAIL: wrong failed URL\n";
            ++failures;
        } else {
            std::cerr << "PASS: external link generates fallback warning\n";
        }
    }

    // Test 5: Non-stylesheet links are ignored
    {
        const std::string html = R"(
            <html><head>
                <link rel="icon" href="favicon.ico"/>
                <link rel="alternate" href="feed.xml"/>
            </head><body></body></html>
        )";

        auto dom = browser::html::parse_html(html);
        auto refs = browser::css::extract_linked_css(*dom);

        if (!refs.empty()) {
            std::cerr << "FAIL: non-stylesheet links should be ignored\n";
            ++failures;
        } else {
            std::cerr << "PASS: non-stylesheet links ignored\n";
        }
    }

    // Test 6: Deterministic — same input produces same output
    {
        const std::string html = R"(
            <html><head>
                <style>body { margin: 0; }</style>
                <link rel="stylesheet" href="x.css"/>
            </head><body></body></html>
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        auto r1 = browser::css::load_linked_css(*dom1, "h1 { color: red; }");
        auto r2 = browser::css::load_linked_css(*dom2, "h1 { color: red; }");

        bool match = (r1.merged.rules.size() == r2.merged.rules.size() &&
                      r1.warnings.size() == r2.warnings.size() &&
                      r1.failed_urls == r2.failed_urls);

        if (!match) {
            std::cerr << "FAIL: linked CSS load not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: linked CSS load is deterministic\n";
        }
    }

    // Test 7: Empty document produces empty results
    {
        const std::string html = "<html><body></body></html>";
        auto dom = browser::html::parse_html(html);
        auto result = browser::css::load_linked_css(*dom, "");

        if (!result.merged.rules.empty() || !result.warnings.empty()) {
            std::cerr << "FAIL: empty document should produce empty results\n";
            ++failures;
        } else {
            std::cerr << "PASS: empty document produces empty results\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll linked CSS tests PASSED\n";
    return 0;
}
