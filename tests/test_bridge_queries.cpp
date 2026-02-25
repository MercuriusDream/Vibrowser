// Test: Controlled JavaScript bridge element queries
// Story 4.1 acceptance test

#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    const std::string html = R"(
        <html><body>
            <h1 id="title" class="heading">Hello World</h1>
            <p id="intro" class="text">Introduction paragraph.</p>
            <p id="body" class="text">Body paragraph.</p>
            <div id="container">
                <span>Nested text</span>
            </div>
        </body></html>
    )";

    auto dom = browser::html::parse_html(html);

    // Test 1: query_by_id finds element with correct data
    {
        auto result = browser::js::query_by_id(*dom, "title");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_by_id('title') failed\n";
            ++failures;
        } else {
            const auto& elem = result.elements[0];
            if (elem.tag_name != "h1") {
                std::cerr << "FAIL: expected tag_name 'h1', got '" << elem.tag_name << "'\n";
                ++failures;
            } else {
                std::cerr << "PASS: query_by_id returns correct tag_name\n";
            }

            if (elem.text_content != "Hello World") {
                std::cerr << "FAIL: expected text 'Hello World', got '" << elem.text_content << "'\n";
                ++failures;
            } else {
                std::cerr << "PASS: query_by_id returns correct text_content\n";
            }

            auto class_it = elem.attributes.find("class");
            if (class_it == elem.attributes.end() || class_it->second != "heading") {
                std::cerr << "FAIL: expected class 'heading'\n";
                ++failures;
            } else {
                std::cerr << "PASS: query_by_id returns correct attributes\n";
            }
        }
    }

    // Test 2: query_by_id for non-existent id
    {
        auto result = browser::js::query_by_id(*dom, "nonexistent");
        if (!result.ok) {
            std::cerr << "FAIL: query_by_id('nonexistent') should still be ok\n";
            ++failures;
        } else if (!result.elements.empty()) {
            std::cerr << "FAIL: query_by_id('nonexistent') should return empty\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_by_id for missing id returns empty\n";
        }
    }

    // Test 3: query_by_id with empty id
    {
        auto result = browser::js::query_by_id(*dom, "");
        if (result.ok) {
            std::cerr << "FAIL: query_by_id('') should fail\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_by_id rejects empty id\n";
        }
    }

    // Test 4: query_selector with #id selector
    {
        auto result = browser::js::query_selector(*dom, "#intro");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_selector('#intro') failed\n";
            ++failures;
        } else if (result.elements[0].tag_name != "p") {
            std::cerr << "FAIL: query_selector('#intro') tag wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector with #id works\n";
        }
    }

    // Test 5: query_selector with tag selector
    {
        auto result = browser::js::query_selector(*dom, "h1");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_selector('h1') failed\n";
            ++failures;
        } else if (result.elements[0].tag_name != "h1") {
            std::cerr << "FAIL: query_selector('h1') returned wrong element\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector with tag works\n";
        }
    }

    // Test 6: query_selector with .class selector
    {
        auto result = browser::js::query_selector(*dom, ".text");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_selector('.text') failed\n";
            ++failures;
        } else if (result.elements[0].tag_name != "p") {
            std::cerr << "FAIL: query_selector('.text') returned wrong element\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector with .class works (returns first match)\n";
        }
    }

    // Test 7: query_selector_all with tag returns multiple
    {
        auto result = browser::js::query_selector_all(*dom, "p");
        if (!result.ok) {
            std::cerr << "FAIL: query_selector_all('p') not ok\n";
            ++failures;
        } else if (result.elements.size() != 2) {
            std::cerr << "FAIL: expected 2 <p> elements, got " << result.elements.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector_all('p') returns 2 elements\n";
        }
    }

    // Test 8: query_selector_all with .class returns multiple
    {
        auto result = browser::js::query_selector_all(*dom, ".text");
        if (!result.ok) {
            std::cerr << "FAIL: query_selector_all('.text') not ok\n";
            ++failures;
        } else if (result.elements.size() != 2) {
            std::cerr << "FAIL: expected 2 .text elements, got " << result.elements.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector_all('.text') returns 2 elements\n";
        }
    }

    // Test 9: query_selector_all with #id returns at most 1
    {
        auto result = browser::js::query_selector_all(*dom, "#title");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_selector_all('#title') should return 1\n";
            ++failures;
        } else {
            std::cerr << "PASS: query_selector_all('#title') returns 1 element\n";
        }
    }

    // Test 10: Deterministic results — same query twice gives same result
    {
        auto r1 = browser::js::query_selector_all(*dom, "p");
        auto r2 = browser::js::query_selector_all(*dom, "p");

        bool match = (r1.elements.size() == r2.elements.size());
        for (std::size_t i = 0; match && i < r1.elements.size(); ++i) {
            if (r1.elements[i].tag_name != r2.elements[i].tag_name ||
                r1.elements[i].text_content != r2.elements[i].text_content ||
                r1.elements[i].attributes != r2.elements[i].attributes) {
                match = false;
            }
        }

        if (!match) {
            std::cerr << "FAIL: query results not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: query results are deterministic\n";
        }
    }

    // Test 11: Nested text content collection
    {
        auto result = browser::js::query_by_id(*dom, "container");
        if (!result.ok || result.elements.size() != 1) {
            std::cerr << "FAIL: query_by_id('container') failed\n";
            ++failures;
        } else if (result.elements[0].text_content.find("Nested text") == std::string::npos) {
            std::cerr << "FAIL: container text missing 'Nested text', got '"
                      << result.elements[0].text_content << "'\n";
            ++failures;
        } else {
            std::cerr << "PASS: nested text content collected correctly\n";
        }
    }

    // Test 12: child_count is populated
    {
        auto result = browser::js::query_by_id(*dom, "container");
        if (!result.ok || result.elements.empty()) {
            std::cerr << "FAIL: container not found\n";
            ++failures;
        } else if (result.elements[0].child_count == 0) {
            std::cerr << "FAIL: container should have children\n";
            ++failures;
        } else {
            std::cerr << "PASS: child_count is populated (" << result.elements[0].child_count << ")\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll bridge query tests PASSED\n";
    return 0;
}
