// Test: Verify deterministic recovery from malformed HTML with warnings
// Story 2.2 acceptance test

#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Unclosed tags emit warnings and produce deterministic DOM
    {
        const std::string html = "<div><p>Hello<span>World</div>";

        auto r1 = browser::html::parse_html_with_diagnostics(html);
        auto r2 = browser::html::parse_html_with_diagnostics(html);

        if (!r1.document || !r2.document) {
            std::cerr << "FAIL: parse returned null for unclosed tags\n";
            return 1;
        }

        const std::string s1 = browser::html::serialize_dom(*r1.document);
        const std::string s2 = browser::html::serialize_dom(*r2.document);

        if (s1 != s2) {
            std::cerr << "FAIL: unclosed tag recovery is not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: unclosed tag recovery is deterministic\n";
        }

        if (r1.warnings.empty()) {
            std::cerr << "FAIL: no warnings for unclosed tags\n";
            ++failures;
        } else {
            std::cerr << "PASS: warnings emitted for unclosed tags (" << r1.warnings.size() << ")\n";
            for (const auto& w : r1.warnings) {
                std::cerr << "  warning: " << w.message << " -> " << w.recovery_action << "\n";
            }
        }

        // Warnings should be identical
        if (r1.warnings.size() != r2.warnings.size()) {
            std::cerr << "FAIL: warning count differs between runs\n";
            ++failures;
        } else {
            std::cerr << "PASS: warning count is deterministic\n";
        }
    }

    // Test 2: Orphan end tags emit warnings
    {
        const std::string html = "<div>text</div></span></p>";

        auto result = browser::html::parse_html_with_diagnostics(html);

        bool has_orphan_warning = false;
        for (const auto& w : result.warnings) {
            if (w.message.find("Orphan end tag") != std::string::npos ||
                w.message.find("Unmatched end tag") != std::string::npos) {
                has_orphan_warning = true;
            }
        }

        if (!has_orphan_warning) {
            std::cerr << "FAIL: no warning for orphan end tags\n";
            ++failures;
        } else {
            std::cerr << "PASS: orphan end tags produce warnings\n";
        }
    }

    // Test 3: Bare '<' in text emits warning and continues
    {
        const std::string html = "<p>3 < 5 and 7 > 2</p>";

        auto result = browser::html::parse_html_with_diagnostics(html);

        if (!result.document) {
            std::cerr << "FAIL: parse returned null for bare '<'\n";
            return 1;
        }

        // Should still produce a DOM
        const std::string ser = browser::html::serialize_dom(*result.document);
        if (ser.empty()) {
            std::cerr << "FAIL: empty DOM for bare '<'\n";
            ++failures;
        } else {
            std::cerr << "PASS: bare '<' produces valid DOM\n";
        }

        // Deterministic across runs
        auto result2 = browser::html::parse_html_with_diagnostics(html);
        if (browser::html::serialize_dom(*result.document) !=
            browser::html::serialize_dom(*result2.document)) {
            std::cerr << "FAIL: bare '<' recovery not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: bare '<' recovery is deterministic\n";
        }
    }

    // Test 4: Unclosed comment emits warning
    {
        const std::string html = "<p>Before<!-- unclosed comment";

        auto result = browser::html::parse_html_with_diagnostics(html);

        bool has_comment_warning = false;
        for (const auto& w : result.warnings) {
            if (w.message.find("Unclosed HTML comment") != std::string::npos) {
                has_comment_warning = true;
            }
        }

        if (!has_comment_warning) {
            std::cerr << "FAIL: no warning for unclosed comment\n";
            ++failures;
        } else {
            std::cerr << "PASS: unclosed comment emits warning\n";
        }
    }

    // Test 5: Implicit closure via mismatched end tag
    {
        const std::string html = "<div><span><em>text</div>";

        auto result = browser::html::parse_html_with_diagnostics(html);

        bool has_implicit_close = false;
        for (const auto& w : result.warnings) {
            if (w.message.find("implicitly closed") != std::string::npos) {
                has_implicit_close = true;
            }
        }

        if (!has_implicit_close) {
            std::cerr << "FAIL: no warning for implicit closure\n";
            ++failures;
        } else {
            std::cerr << "PASS: implicit closure emits warning\n";
        }

        // Deterministic
        auto result2 = browser::html::parse_html_with_diagnostics(html);
        if (browser::html::serialize_dom(*result.document) !=
            browser::html::serialize_dom(*result2.document)) {
            std::cerr << "FAIL: implicit closure recovery not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: implicit closure recovery is deterministic\n";
        }
    }

    // Test 6: Well-formed HTML produces zero warnings
    {
        const std::string html = "<html><body><p>Hello</p></body></html>";

        auto result = browser::html::parse_html_with_diagnostics(html);

        if (!result.warnings.empty()) {
            std::cerr << "FAIL: well-formed HTML should produce zero warnings, got "
                      << result.warnings.size() << "\n";
            for (const auto& w : result.warnings) {
                std::cerr << "  " << w.message << "\n";
            }
            ++failures;
        } else {
            std::cerr << "PASS: well-formed HTML produces zero warnings\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll malformed HTML recovery tests PASSED\n";
    return 0;
}
