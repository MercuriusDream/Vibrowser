// Test: Handle supported input and event updates
// Story 4.3 acceptance test

#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    const std::string html = R"(
        <html><body>
            <button id="btn">Click me</button>
            <input id="input1" value="original"/>
            <select id="sel1"><option>A</option></select>
            <p id="output">Waiting</p>
        </body></html>
    )";

    // Test 1: EventType names
    {
        bool ok = true;
        if (std::string(browser::js::event_type_name(browser::js::EventType::Click)) != "click") ok = false;
        if (std::string(browser::js::event_type_name(browser::js::EventType::Input)) != "input") ok = false;
        if (std::string(browser::js::event_type_name(browser::js::EventType::Change)) != "change") ok = false;

        if (!ok) {
            std::cerr << "FAIL: event_type_name returned incorrect values\n";
            ++failures;
        } else {
            std::cerr << "PASS: event_type_name returns correct names\n";
        }
    }

    // Test 2: Click event dispatches to handler
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        bool handler_called = false;
        registry.add_listener("btn", browser::js::EventType::Click,
            [&](browser::html::Node& doc, const browser::js::DomEvent& event) {
                handler_called = true;
                browser::js::set_text_by_id(doc, "output", "Clicked!");
            });

        browser::js::DomEvent click_event{browser::js::EventType::Click, "btn", ""};
        auto result = registry.dispatch(*dom, click_event);

        if (!result.ok) {
            std::cerr << "FAIL: click dispatch failed\n";
            ++failures;
        } else if (!handler_called) {
            std::cerr << "FAIL: click handler not called\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "output");
            if (query.elements[0].text_content != "Clicked!") {
                std::cerr << "FAIL: click handler didn't update DOM\n";
                ++failures;
            } else {
                std::cerr << "PASS: click event dispatches and mutates DOM\n";
            }
        }
    }

    // Test 3: Input event carries value
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        std::string received_value;
        registry.add_listener("input1", browser::js::EventType::Input,
            [&](browser::html::Node& doc, const browser::js::DomEvent& event) {
                received_value = event.value;
                browser::js::set_attribute_by_id(doc, "input1", "value", event.value);
            });

        browser::js::DomEvent input_event{browser::js::EventType::Input, "input1", "new-value"};
        registry.dispatch(*dom, input_event);

        if (received_value != "new-value") {
            std::cerr << "FAIL: input event value not received\n";
            ++failures;
        } else {
            auto query = browser::js::query_by_id(*dom, "input1");
            auto it = query.elements[0].attributes.find("value");
            if (it == query.elements[0].attributes.end() || it->second != "new-value") {
                std::cerr << "FAIL: input value not applied to DOM\n";
                ++failures;
            } else {
                std::cerr << "PASS: input event carries value and updates DOM\n";
            }
        }
    }

    // Test 4: Change event dispatches correctly
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        std::string changed_to;
        registry.add_listener("sel1", browser::js::EventType::Change,
            [&](browser::html::Node&, const browser::js::DomEvent& event) {
                changed_to = event.value;
            });

        browser::js::DomEvent change_event{browser::js::EventType::Change, "sel1", "B"};
        registry.dispatch(*dom, change_event);

        if (changed_to != "B") {
            std::cerr << "FAIL: change event value not received\n";
            ++failures;
        } else {
            std::cerr << "PASS: change event dispatches correctly\n";
        }
    }

    // Test 5: Dispatch with no matching handler returns ok but "No handler"
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        browser::js::DomEvent event{browser::js::EventType::Click, "nonexistent", ""};
        auto result = registry.dispatch(*dom, event);

        if (!result.ok || result.message != "No handler for event") {
            std::cerr << "FAIL: dispatch should succeed with 'No handler'\n";
            ++failures;
        } else {
            std::cerr << "PASS: no handler dispatch returns correct message\n";
        }
    }

    // Test 6: Multiple handlers on same element/event
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        int call_count = 0;
        registry.add_listener("btn", browser::js::EventType::Click,
            [&](browser::html::Node&, const browser::js::DomEvent&) { ++call_count; });
        registry.add_listener("btn", browser::js::EventType::Click,
            [&](browser::html::Node&, const browser::js::DomEvent&) { ++call_count; });

        browser::js::DomEvent click{browser::js::EventType::Click, "btn", ""};
        registry.dispatch(*dom, click);

        if (call_count != 2) {
            std::cerr << "FAIL: expected 2 handler calls, got " << call_count << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: multiple handlers both called\n";
        }
    }

    // Test 7: Wrong event type does not trigger handler
    {
        auto dom = browser::html::parse_html(html);
        browser::js::EventRegistry registry;

        bool called = false;
        registry.add_listener("btn", browser::js::EventType::Click,
            [&](browser::html::Node&, const browser::js::DomEvent&) { called = true; });

        browser::js::DomEvent input_event{browser::js::EventType::Input, "btn", ""};
        registry.dispatch(*dom, input_event);

        if (called) {
            std::cerr << "FAIL: wrong event type triggered handler\n";
            ++failures;
        } else {
            std::cerr << "PASS: wrong event type does not trigger handler\n";
        }
    }

    // Test 8: listener_count and clear
    {
        browser::js::EventRegistry registry;
        registry.add_listener("a", browser::js::EventType::Click,
            [](browser::html::Node&, const browser::js::DomEvent&) {});
        registry.add_listener("b", browser::js::EventType::Input,
            [](browser::html::Node&, const browser::js::DomEvent&) {});

        if (registry.listener_count() != 2) {
            std::cerr << "FAIL: listener_count expected 2, got " << registry.listener_count() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: listener_count is correct\n";
        }

        registry.clear();
        if (registry.listener_count() != 0) {
            std::cerr << "FAIL: clear didn't reset listener count\n";
            ++failures;
        } else {
            std::cerr << "PASS: clear resets registry\n";
        }
    }

    // Test 9: Deterministic — same events produce same state
    {
        auto dom1 = browser::html::parse_html(html);
        auto dom2 = browser::html::parse_html(html);

        auto handler = [](browser::html::Node& doc, const browser::js::DomEvent& event) {
            browser::js::set_text_by_id(doc, "output", "Handled: " + event.value);
        };

        browser::js::EventRegistry r1, r2;
        r1.add_listener("btn", browser::js::EventType::Click, handler);
        r2.add_listener("btn", browser::js::EventType::Click, handler);

        browser::js::DomEvent click{browser::js::EventType::Click, "btn", "test"};
        r1.dispatch(*dom1, click);
        r2.dispatch(*dom2, click);

        auto q1 = browser::js::query_by_id(*dom1, "output");
        auto q2 = browser::js::query_by_id(*dom2, "output");

        if (q1.elements[0].text_content != q2.elements[0].text_content) {
            std::cerr << "FAIL: event dispatch not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: event dispatch is deterministic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll event dispatch tests PASSED\n";
    return 0;
}
