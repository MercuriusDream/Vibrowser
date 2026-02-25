// Test: Mutate style and attributes via runtime bridge calls
// Story 4.2 acceptance test

#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    const std::string html = R"(
        <html><body>
            <h1 id="title">Hello World</h1>
            <p id="para" class="text">Original text.</p>
            <div id="box" style="color: red;">Styled box</div>
        </body></html>
    )";

    // Test 1: set_attribute_by_id sets attribute
    {
        auto dom = browser::html::parse_html(html);
        auto result = browser::js::set_attribute_by_id(*dom, "title", "data-test", "value1");

        if (!result.ok) {
            std::cerr << "FAIL: set_attribute_by_id failed: " << result.message << "\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "title");
            auto it = query.elements[0].attributes.find("data-test");
            if (it == query.elements[0].attributes.end() || it->second != "value1") {
                std::cerr << "FAIL: attribute not set correctly\n";
                ++failures;
            } else {
                std::cerr << "PASS: set_attribute_by_id works\n";
            }
        }
    }

    // Test 2: set_attribute_by_id overwrites existing attribute
    {
        auto dom = browser::html::parse_html(html);
        browser::js::set_attribute_by_id(*dom, "para", "class", "new-class");

        auto query = browser::js::query_by_id(*dom, "para");
        auto it = query.elements[0].attributes.find("class");
        if (it == query.elements[0].attributes.end() || it->second != "new-class") {
            std::cerr << "FAIL: attribute not overwritten\n";
            ++failures;
        } else {
            std::cerr << "PASS: set_attribute_by_id overwrites existing\n";
        }
    }

    // Test 3: remove_attribute_by_id removes attribute
    {
        auto dom = browser::html::parse_html(html);
        auto result = browser::js::remove_attribute_by_id(*dom, "para", "class");

        if (!result.ok) {
            std::cerr << "FAIL: remove_attribute_by_id failed\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "para");
            if (query.elements[0].attributes.count("class") != 0) {
                std::cerr << "FAIL: attribute not removed\n";
                ++failures;
            } else {
                std::cerr << "PASS: remove_attribute_by_id works\n";
            }
        }
    }

    // Test 4: set_style_by_id applies inline style
    {
        auto dom = browser::html::parse_html(html);
        auto result = browser::js::set_style_by_id(*dom, "title", "backgroundColor", "blue");

        if (!result.ok) {
            std::cerr << "FAIL: set_style_by_id failed: " << result.message << "\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "title");
            auto it = query.elements[0].attributes.find("style");
            if (it == query.elements[0].attributes.end()) {
                std::cerr << "FAIL: style attribute not set\n";
                ++failures;
            } else if (it->second.find("background-color") == std::string::npos) {
                std::cerr << "FAIL: style missing background-color, got: " << it->second << "\n";
                ++failures;
            } else {
                std::cerr << "PASS: set_style_by_id applies style\n";
            }
        }
    }

    // Test 5: set_style_by_id on element with existing style
    {
        auto dom = browser::html::parse_html(html);
        auto result = browser::js::set_style_by_id(*dom, "box", "backgroundColor", "green");

        if (!result.ok) {
            std::cerr << "FAIL: set_style_by_id on styled element failed\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "box");
            auto it = query.elements[0].attributes.find("style");
            if (it == query.elements[0].attributes.end()) {
                std::cerr << "FAIL: style attribute lost\n";
                ++failures;
            } else if (it->second.find("background-color") == std::string::npos) {
                std::cerr << "FAIL: new style not applied\n";
                ++failures;
            } else {
                std::cerr << "PASS: set_style_by_id on styled element works\n";
            }
        }
    }

    // Test 6: set_text_by_id replaces text content
    {
        auto dom = browser::html::parse_html(html);
        auto result = browser::js::set_text_by_id(*dom, "para", "New text content");

        if (!result.ok) {
            std::cerr << "FAIL: set_text_by_id failed\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "para");
            if (query.elements[0].text_content != "New text content") {
                std::cerr << "FAIL: text not updated, got: '"
                          << query.elements[0].text_content << "'\n";
                ++failures;
            } else {
                std::cerr << "PASS: set_text_by_id replaces text\n";
            }
        }
    }

    // Test 7: Operations on non-existent element return error
    {
        auto dom = browser::html::parse_html(html);
        auto r1 = browser::js::set_attribute_by_id(*dom, "missing", "x", "y");
        auto r2 = browser::js::remove_attribute_by_id(*dom, "missing", "x");
        auto r3 = browser::js::set_style_by_id(*dom, "missing", "color", "red");
        auto r4 = browser::js::set_text_by_id(*dom, "missing", "text");

        if (r1.ok || r2.ok || r3.ok || r4.ok) {
            std::cerr << "FAIL: operations on missing element should fail\n";
            ++failures;
        } else {
            std::cerr << "PASS: all mutation ops fail on missing element\n";
        }
    }

    // Test 8: Empty id/attribute rejected
    {
        auto dom = browser::html::parse_html(html);
        auto r1 = browser::js::set_attribute_by_id(*dom, "", "x", "y");
        auto r2 = browser::js::set_attribute_by_id(*dom, "title", "", "y");
        auto r3 = browser::js::remove_attribute_by_id(*dom, "", "x");

        if (r1.ok || r2.ok || r3.ok) {
            std::cerr << "FAIL: empty id/attribute should be rejected\n";
            ++failures;
        } else {
            std::cerr << "PASS: empty id/attribute rejected\n";
        }
    }

    // Test 9: Deterministic — same mutation sequence produces same result
    {
        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        browser::js::set_attribute_by_id(*dom1, "title", "data-x", "1");
        browser::js::set_style_by_id(*dom1, "para", "color", "blue");
        browser::js::set_text_by_id(*dom1, "box", "Updated");

        browser::js::set_attribute_by_id(*dom2, "title", "data-x", "1");
        browser::js::set_style_by_id(*dom2, "para", "color", "blue");
        browser::js::set_text_by_id(*dom2, "box", "Updated");

        auto q1 = browser::js::query_by_id(*dom1, "title");
        auto q2 = browser::js::query_by_id(*dom2, "title");

        bool match = (q1.elements[0].attributes == q2.elements[0].attributes);

        auto p1 = browser::js::query_by_id(*dom1, "para");
        auto p2 = browser::js::query_by_id(*dom2, "para");
        match = match && (p1.elements[0].attributes == p2.elements[0].attributes);

        auto b1 = browser::js::query_by_id(*dom1, "box");
        auto b2 = browser::js::query_by_id(*dom2, "box");
        match = match && (b1.elements[0].text_content == b2.elements[0].text_content);

        if (!match) {
            std::cerr << "FAIL: mutations not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: mutations are deterministic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll bridge mutation tests PASSED\n";
    return 0;
}
