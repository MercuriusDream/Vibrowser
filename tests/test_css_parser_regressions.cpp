#include "browser/css/css_parser.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Regression 1: exact attribute selectors should work for generic attributes.
    {
        const std::string html = "<html><body><input id=\"a\" type=\"text\"></body></html>";
        const std::string css = "input[type=\"text\"] { color: red; }";
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);
        auto* input = browser::html::query_first_by_id(*dom, "a");
        auto style = browser::css::compute_style_for_node(*input, sheet);
        if (style["color"] != "red") {
            std::cerr << "FAIL: [type=\"text\"] selector did not match input element\n";
            ++failures;
        } else {
            std::cerr << "PASS: generic exact attribute selectors match correctly\n";
        }
    }

    // Regression 2: declaration parsing keeps semicolons inside quoted URL values.
    {
        const std::string inline_style =
            "background: url(\"data:image/svg+xml;utf8,<svg/>\"); color: red;";
        auto style = browser::css::parse_inline_style(inline_style);
        auto it = style.find("background");
        if (it == style.end() || it->second.find("data:image/svg+xml;utf8,<svg/>") == std::string::npos) {
            std::cerr << "FAIL: inline style parser truncated background URL value\n";
            ++failures;
        } else {
            std::cerr << "PASS: inline style parser keeps quoted URL with semicolons\n";
        }
    }

    // Regression 3: nested at-rules are ignored safely (no malformed selector rules emitted).
    {
        const std::string css =
            "@media (min-width:1px){ body { color:red; } } p { color: blue; }";
        auto sheet = browser::css::parse_css(css);
        bool malformed_at_rule_selector = false;
        bool saw_p_selector = false;
        for (const auto& rule : sheet.rules) {
            if (rule.selector.find("@media") != std::string::npos) {
                malformed_at_rule_selector = true;
            }
            if (rule.selector == "p") {
                saw_p_selector = true;
            }
        }
        if (malformed_at_rule_selector || !saw_p_selector) {
            std::cerr << "FAIL: nested at-rule parsing emitted malformed selectors or dropped valid trailing rules\n";
            ++failures;
        } else {
            std::cerr << "PASS: nested at-rules are skipped without corrupting trailing rules\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll CSS parser regression tests PASSED\n";
    return 0;
}
