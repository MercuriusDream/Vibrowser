// Test: Verify CSS selector matching and cascade are deterministic
// Story 2.3 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <map>
#include <string>

namespace {

bool styles_match(const std::map<std::string, std::string>& a,
                  const std::map<std::string, std::string>& b) {
    return a == b;
}

std::string format_style(const std::map<std::string, std::string>& style) {
    std::string out;
    for (const auto& [k, v] : style) {
        if (!out.empty()) out += "; ";
        out += k + ": " + v;
    }
    return out;
}

}  // namespace

int main() {
    int failures = 0;

    // Test 1: Simple type + class + id selectors produce deterministic styles
    {
        const std::string html = R"(
            <html><body>
                <p id="intro" class="highlight">Hello</p>
            </body></html>
        )";
        const std::string css = R"(
            p { color: blue; font-size: 14px; }
            .highlight { color: red; }
            #intro { font-size: 18px; }
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet1 = browser::css::parse_css(css);
        auto sheet2 = browser::css::parse_css(css);

        auto* p1 = browser::html::query_first_by_id(*dom1, "intro");
        auto* p2 = browser::html::query_first_by_id(*dom2, "intro");

        auto style1 = browser::css::compute_style_for_node(*p1, sheet1);
        auto style2 = browser::css::compute_style_for_node(*p2, sheet2);

        if (!styles_match(style1, style2)) {
            std::cerr << "FAIL: styles differ for same input\n"
                      << "  run1: " << format_style(style1) << "\n"
                      << "  run2: " << format_style(style2) << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: simple selector styles are deterministic\n";
        }

        // Verify cascade: #intro specificity (100) > .highlight (10) > p (1)
        // So font-size from #intro, color from .highlight
        if (style1["font-size"] != "18px") {
            std::cerr << "FAIL: expected font-size 18px from #intro, got: " << style1["font-size"] << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: ID selector wins specificity for font-size\n";
        }

        if (style1["color"] != "red") {
            std::cerr << "FAIL: expected color red from .highlight, got: " << style1["color"] << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: class selector wins specificity over type for color\n";
        }
    }

    // Test 2: Descendant, child, sibling combinators
    {
        const std::string html = R"(
            <div id="container">
                <ul>
                    <li class="item">First</li>
                    <li class="item">Second</li>
                </ul>
            </div>
        )";
        const std::string css = R"(
            #container ul li { color: green; }
            div > ul > li { font-size: 16px; }
            li + li { margin-top: 5px; }
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        auto items1 = browser::html::query_all_by_class(*dom1, "item");
        auto items2 = browser::html::query_all_by_class(*dom2, "item");

        for (std::size_t i = 0; i < items1.size() && i < items2.size(); ++i) {
            auto s1 = browser::css::compute_style_for_node(*items1[i], sheet);
            auto s2 = browser::css::compute_style_for_node(*items2[i], sheet);
            if (!styles_match(s1, s2)) {
                std::cerr << "FAIL: combinator style differs for item " << i << "\n";
                ++failures;
            }
        }
        std::cerr << "PASS: combinator styles are deterministic\n";
    }

    // Test 3: Pseudo-class selectors
    {
        const std::string html = R"(
            <ul>
                <li>A</li>
                <li>B</li>
                <li>C</li>
            </ul>
        )";
        const std::string css = R"(
            li:first-child { color: red; }
            li:last-child { color: blue; }
            li:nth-child(2) { color: green; }
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        auto lis1 = browser::html::query_all_by_tag(*dom1, "li");
        auto lis2 = browser::html::query_all_by_tag(*dom2, "li");

        bool all_match = true;
        for (std::size_t i = 0; i < lis1.size() && i < lis2.size(); ++i) {
            auto s1 = browser::css::compute_style_for_node(*lis1[i], sheet);
            auto s2 = browser::css::compute_style_for_node(*lis2[i], sheet);
            if (!styles_match(s1, s2)) {
                all_match = false;
                std::cerr << "FAIL: pseudo-class style differs for li " << i << "\n";
                ++failures;
            }
        }
        if (all_match) {
            std::cerr << "PASS: pseudo-class styles are deterministic\n";
        }
    }

    // Test 4: 100 runs all produce same result
    {
        const std::string html = "<div class=\"x\" id=\"y\"><span>text</span></div>";
        const std::string css = ".x { color: red; } #y { font-size: 20px; } span { color: blue; }";

        auto ref_dom = browser::html::parse_html(html);
        auto ref_sheet = browser::css::parse_css(css);
        auto* ref_span = browser::html::query_first_by_tag(*ref_dom, "span");
        auto ref_style = browser::css::compute_style_for_node(*ref_span, ref_sheet);

        bool all_match = true;
        for (int i = 0; i < 100; ++i) {
            auto dom = browser::html::parse_html(html);
            auto sheet = browser::css::parse_css(css);
            auto* span = browser::html::query_first_by_tag(*dom, "span");
            auto style = browser::css::compute_style_for_node(*span, sheet);
            if (!styles_match(style, ref_style)) {
                all_match = false;
                break;
            }
        }

        if (!all_match) {
            std::cerr << "FAIL: not all 100 runs match\n";
            ++failures;
        } else {
            std::cerr << "PASS: 100 consecutive style computations are identical\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll deterministic styling tests PASSED\n";
    return 0;
}
