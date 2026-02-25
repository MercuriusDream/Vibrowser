// Test: Verify unsupported selectors/media produce deterministic fallback with logging
// Story 2.4 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Unsupported selector is skipped with warning via parse_css_with_diagnostics
    {
        // ::before and ::after are pseudo-elements not supported by our parser
        const std::string css = R"(
            p { color: blue; }
            p::before { content: "x"; }
            .valid { font-size: 14px; }
        )";

        auto r1 = browser::css::parse_css_with_diagnostics(css);
        auto r2 = browser::css::parse_css_with_diagnostics(css);

        // Should have warning(s) about unsupported selectors
        if (r1.warnings.empty()) {
            std::cerr << "FAIL: expected warnings for unsupported selector\n";
            ++failures;
        } else {
            std::cerr << "PASS: unsupported selector produces warnings (" << r1.warnings.size() << ")\n";
            for (const auto& w : r1.warnings) {
                std::cerr << "  " << w.message << ": " << w.selector << "\n";
            }
        }

        // Warning count should be deterministic
        if (r1.warnings.size() != r2.warnings.size()) {
            std::cerr << "FAIL: warning count differs between runs\n";
            ++failures;
        } else {
            std::cerr << "PASS: warning count is deterministic\n";
        }

        // Valid rules should still be parsed
        if (r1.stylesheet.rules.size() < 2) {
            std::cerr << "FAIL: expected at least 2 valid rules, got " << r1.stylesheet.rules.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: valid rules still parsed alongside unsupported ones\n";
        }
    }

    // Test 2: compute_style_for_node with diagnostics captures selector warnings
    {
        const std::string html = "<p class=\"x\">Hello</p>";
        const std::string css = R"(
            p { color: red; }
            .x { font-size: 16px; }
        )";

        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);
        auto* p = browser::html::query_first_by_tag(*dom, "p");

        std::vector<browser::css::StyleWarning> warnings;
        auto style = browser::css::compute_style_for_node(*p, sheet, warnings);

        // For valid CSS, no warnings expected
        if (!warnings.empty()) {
            std::cerr << "FAIL: unexpected warnings for valid CSS\n";
            ++failures;
        } else {
            std::cerr << "PASS: valid CSS produces zero style warnings\n";
        }

        // Style should still work
        if (style["color"] != "red" || style["font-size"] != "16px") {
            std::cerr << "FAIL: style values wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: style values correct with diagnostics overload\n";
        }
    }

    // Test 3: Fallback is deterministic — unsupported selector doesn't corrupt state
    {
        const std::string html = "<div><span>text</span></div>";
        const std::string css = R"(
            span { color: green; }
            div:hover span { color: red; }
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        auto r1 = browser::css::parse_css_with_diagnostics(css);
        auto r2 = browser::css::parse_css_with_diagnostics(css);

        auto* span1 = browser::html::query_first_by_tag(*dom1, "span");
        auto* span2 = browser::html::query_first_by_tag(*dom2, "span");

        std::vector<browser::css::StyleWarning> w1, w2;
        auto s1 = browser::css::compute_style_for_node(*span1, r1.stylesheet, w1);
        auto s2 = browser::css::compute_style_for_node(*span2, r2.stylesheet, w2);

        if (s1 != s2) {
            std::cerr << "FAIL: fallback style differs between runs\n";
            ++failures;
        } else {
            std::cerr << "PASS: fallback style is deterministic\n";
        }

        // span should get color: green (the :hover rule may or may not parse
        // but either way it shouldn't match)
        if (s1["color"] != "green") {
            std::cerr << "FAIL: expected color green, got: " << s1["color"] << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: valid rule applies correctly alongside unsupported one\n";
        }
    }

    // Test 4: parse_css (non-diagnostics) still works unchanged
    {
        const std::string css = "p { color: blue; } .x { font-size: 14px; }";
        auto sheet = browser::css::parse_css(css);
        if (sheet.rules.size() != 2) {
            std::cerr << "FAIL: parse_css should return 2 rules, got " << sheet.rules.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: original parse_css still works\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll selector/media fallback tests PASSED\n";
    return 0;
}
