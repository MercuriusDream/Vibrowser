// Test: Verify HTML parsing produces deterministic DOM structures
// Story 2.1 acceptance test

#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Simple HTML produces identical DOM across multiple parses
    {
        const std::string html = R"(
            <html>
            <head><title>Test</title></head>
            <body>
                <h1 id="main">Hello</h1>
                <p class="intro">World</p>
            </body>
            </html>
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        if (!dom1 || !dom2) {
            std::cerr << "FAIL: parse_html returned null\n";
            return 1;
        }

        const std::string ser1 = browser::html::serialize_dom(*dom1);
        const std::string ser2 = browser::html::serialize_dom(*dom2);

        if (ser1 != ser2) {
            std::cerr << "FAIL: DOM serialization differs between runs\n"
                      << "  run1: " << ser1.substr(0, 200) << "...\n"
                      << "  run2: " << ser2.substr(0, 200) << "...\n";
            ++failures;
        } else {
            std::cerr << "PASS: simple HTML produces identical DOM\n";
        }
    }

    // Test 2: DOM with attributes in ordered map (deterministic key order)
    {
        const std::string html = R"(
            <div id="a" class="b" data-x="c" style="color:red">text</div>
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        const std::string ser1 = browser::html::serialize_dom(*dom1);
        const std::string ser2 = browser::html::serialize_dom(*dom2);

        if (ser1 != ser2) {
            std::cerr << "FAIL: multi-attribute DOM differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: multi-attribute DOM is deterministic\n";
        }
    }

    // Test 3: Nested structures produce deterministic output
    {
        const std::string html = R"(
            <ul>
                <li>One</li>
                <li>Two<ul><li>Nested</li></ul></li>
                <li>Three</li>
            </ul>
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        const std::string ser1 = browser::html::serialize_dom(*dom1);
        const std::string ser2 = browser::html::serialize_dom(*dom2);

        if (ser1 != ser2) {
            std::cerr << "FAIL: nested structure DOM differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: nested structure DOM is deterministic\n";
        }
    }

    // Test 4: Void elements handled deterministically
    {
        const std::string html = R"(
            <div>
                <img src="test.png" alt="test">
                <br>
                <hr>
                <input type="text" value="hello">
            </div>
        )";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        const std::string ser1 = browser::html::serialize_dom(*dom1);
        const std::string ser2 = browser::html::serialize_dom(*dom2);

        if (ser1 != ser2) {
            std::cerr << "FAIL: void element DOM differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: void element DOM is deterministic\n";
        }
    }

    // Test 5: HTML entities produce deterministic output
    {
        const std::string html = R"(<p>&amp; &lt; &gt; &quot; &#169; &#x00A9;</p>)";

        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        const std::string ser1 = browser::html::serialize_dom(*dom1);
        const std::string ser2 = browser::html::serialize_dom(*dom2);

        if (ser1 != ser2) {
            std::cerr << "FAIL: entity DOM differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: entity DOM is deterministic\n";
        }
    }

    // Test 6: 100 consecutive parses all produce same DOM
    {
        const std::string html = "<div><span>a</span><span>b</span></div>";
        auto reference = browser::html::parse_html(html);
        const std::string ref_ser = browser::html::serialize_dom(*reference);

        bool all_match = true;
        for (int i = 0; i < 100; ++i) {
            auto dom = browser::html::parse_html(html);
            if (browser::html::serialize_dom(*dom) != ref_ser) {
                all_match = false;
                break;
            }
        }

        if (!all_match) {
            std::cerr << "FAIL: not all 100 parses match\n";
            ++failures;
        } else {
            std::cerr << "PASS: 100 consecutive parses produce identical DOM\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll deterministic parsing tests PASSED\n";
    return 0;
}
