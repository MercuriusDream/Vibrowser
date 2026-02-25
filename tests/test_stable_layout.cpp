// Test: Verify stable block, inline, positioned layout computation
// Story 3.1 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/layout/layout_engine.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Simple block layout is deterministic
    {
        const std::string html = R"(
            <html><body>
                <div style="width: 200px; height: 100px; padding: 10px;">
                    <p style="font-size: 16px;">Hello World</p>
                </div>
            </body></html>
        )";
        const std::string css = "body { margin: 0; }";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        auto layout1 = browser::layout::layout_document(*dom1, sheet, 800);
        auto layout2 = browser::layout::layout_document(*dom2, sheet, 800);

        auto s1 = browser::layout::serialize_layout(layout1);
        auto s2 = browser::layout::serialize_layout(layout2);

        if (s1 != s2) {
            std::cerr << "FAIL: block layout differs between runs\n";
            ++failures;
        } else {
            std::cerr << "PASS: block layout is deterministic\n";
        }
    }

    // Test 2: Nested layout with mixed styles
    {
        const std::string html = R"(
            <div>
                <h1>Title</h1>
                <div style="padding: 5px; margin: 10px;">
                    <p>Paragraph one</p>
                    <p>Paragraph two with more text</p>
                </div>
                <ul>
                    <li>A</li>
                    <li>B</li>
                    <li>C</li>
                </ul>
            </div>
        )";
        const std::string css = R"(
            h1 { font-size: 24px; margin: 10px 0; }
            p { font-size: 14px; line-height: 20px; }
            li { font-size: 12px; }
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        auto l1 = browser::layout::layout_document(*dom1, sheet, 1024);
        auto l2 = browser::layout::layout_document(*dom2, sheet, 1024);

        if (browser::layout::serialize_layout(l1) !=
            browser::layout::serialize_layout(l2)) {
            std::cerr << "FAIL: nested layout differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: nested layout is deterministic\n";
        }
    }

    // Test 3: Consistent at multiple viewport widths
    {
        const std::string html = "<div><p>Short</p><p>A longer paragraph with wrapping text content</p></div>";
        const std::string css = "p { font-size: 14px; }";

        auto sheet = browser::css::parse_css(css);
        const int widths[] = {320, 640, 800, 1024, 1280};

        bool all_consistent = true;
        for (int w : widths) {
            auto dom1 = browser::html::parse_html(html);
            auto dom2 = browser::html::parse_html(html);

            auto l1 = browser::layout::layout_document(*dom1, sheet, w);
            auto l2 = browser::layout::layout_document(*dom2, sheet, w);

            if (browser::layout::serialize_layout(l1) !=
                browser::layout::serialize_layout(l2)) {
                std::cerr << "FAIL: layout differs at viewport width " << w << "\n";
                all_consistent = false;
                ++failures;
            }
        }
        if (all_consistent) {
            std::cerr << "PASS: layout consistent at all viewport widths\n";
        }
    }

    // Test 4: display:none subtree pruning is deterministic
    {
        const std::string html = R"(
            <div>
                <p>Visible</p>
                <div style="display: none;">
                    <p>Hidden</p>
                </div>
                <p>Also visible</p>
            </div>
        )";
        const std::string css = "";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        auto l1 = browser::layout::layout_document(*dom1, sheet, 800);
        auto l2 = browser::layout::layout_document(*dom2, sheet, 800);

        if (browser::layout::serialize_layout(l1) !=
            browser::layout::serialize_layout(l2)) {
            std::cerr << "FAIL: display:none pruning not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: display:none pruning is deterministic\n";
        }
    }

    // Test 5: 100 consecutive layouts match
    {
        const std::string html = "<div><span>text</span></div>";
        const std::string css = "div { padding: 5px; } span { font-size: 14px; }";

        auto ref_dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);
        auto ref_layout = browser::layout::layout_document(*ref_dom, sheet, 800);
        const auto ref_ser = browser::layout::serialize_layout(ref_layout);

        bool all_match = true;
        for (int i = 0; i < 100; ++i) {
            auto dom = browser::html::parse_html(html);
            auto layout = browser::layout::layout_document(*dom, sheet, 800);
            if (browser::layout::serialize_layout(layout) != ref_ser) {
                all_match = false;
                break;
            }
        }

        if (!all_match) {
            std::cerr << "FAIL: 100-run layout consistency check failed\n";
            ++failures;
        } else {
            std::cerr << "PASS: 100 consecutive layouts match\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll stable layout tests PASSED\n";
    return 0;
}
