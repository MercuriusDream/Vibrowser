// Test: Re-render output after runtime DOM/style mutations
// Story 4.4 acceptance test

#include "browser/css/css_parser.h"
#include "browser/engine/render_pipeline.h"
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
            <p id="para">Some text.</p>
        </body></html>
    )";
    const std::string css = "h1 { font-size: 24px; color: black; } p { font-size: 14px; }";

    // Test 1: Initial render produces valid output
    {
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

        if (pipeline.canvas().empty()) {
            std::cerr << "FAIL: initial render produced empty canvas\n";
            ++failures;
        } else {
            std::cerr << "PASS: initial render produces valid canvas\n";
        }

        if (pipeline.render_count() != 1) {
            std::cerr << "FAIL: expected render_count 1, got " << pipeline.render_count() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: render_count is 1 after construction\n";
        }
    }

    // Test 2: Mutation + rerender produces different output
    {
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

        // Save initial pixels
        auto initial_pixels = pipeline.canvas().pixels();

        // Mutate the DOM — change style to add a background color
        browser::js::set_style_by_id(pipeline.document(), "title", "backgroundColor", "red");

        // Re-render
        auto result = pipeline.rerender();
        if (!result.ok) {
            std::cerr << "FAIL: rerender failed: " << result.message << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: rerender succeeds after mutation\n";
        }

        if (pipeline.render_count() != 2) {
            std::cerr << "FAIL: expected render_count 2, got " << pipeline.render_count() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: render_count incremented to 2\n";
        }

        // Pixels should be different after adding red background
        if (initial_pixels == pipeline.canvas().pixels()) {
            std::cerr << "FAIL: pixels unchanged after style mutation + rerender\n";
            ++failures;
        } else {
            std::cerr << "PASS: pixels changed after style mutation + rerender\n";
        }
    }

    // Test 3: Text mutation + rerender
    {
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

        auto initial_pixels = pipeline.canvas().pixels();

        browser::js::set_text_by_id(pipeline.document(), "title", "CHANGED TITLE TEXT");
        pipeline.rerender();

        // Layout should be different since the text content changed
        // Verify via the render text output which collects text from the layout tree
        std::string text_output = browser::render::render_to_text(pipeline.layout(), 80);
        if (text_output.find("CHANGED TITLE TEXT") == std::string::npos) {
            std::cerr << "FAIL: render text doesn't reflect text mutation, got: "
                      << text_output << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: render output reflects text mutation after rerender\n";
        }
    }

    // Test 4: Multiple mutations + single rerender
    {
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

        browser::js::set_style_by_id(pipeline.document(), "title", "backgroundColor", "blue");
        browser::js::set_text_by_id(pipeline.document(), "para", "Updated paragraph");
        browser::js::set_attribute_by_id(pipeline.document(), "para", "class", "highlight");

        auto result = pipeline.rerender();
        if (!result.ok) {
            std::cerr << "FAIL: rerender after multiple mutations failed\n";
            ++failures;
        } else if (pipeline.render_count() != 2) {
            std::cerr << "FAIL: render_count expected 2, got " << pipeline.render_count() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: multiple mutations + single rerender works\n";
        }
    }

    // Test 5: Deterministic rerender — same mutations produce same output
    {
        auto dom1 = browser::html::parse_html(html);
        auto sheet1 = browser::css::parse_css(css);
        browser::engine::RenderPipeline p1(std::move(dom1), std::move(sheet1), 800, 600);

        auto dom2 = browser::html::parse_html(html);
        auto sheet2 = browser::css::parse_css(css);
        browser::engine::RenderPipeline p2(std::move(dom2), std::move(sheet2), 800, 600);

        // Apply same mutations
        browser::js::set_style_by_id(p1.document(), "title", "color", "green");
        browser::js::set_style_by_id(p2.document(), "title", "color", "green");

        p1.rerender();
        p2.rerender();

        if (p1.canvas().pixels() != p2.canvas().pixels()) {
            std::cerr << "FAIL: deterministic rerender produced different pixels\n";
            ++failures;
        } else {
            std::cerr << "PASS: deterministic rerender produces identical output\n";
        }
    }

    // Test 6: Event-driven mutation + rerender
    {
        auto dom = browser::html::parse_html(html);
        auto sheet = browser::css::parse_css(css);

        browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

        browser::js::EventRegistry registry;
        registry.add_listener("title", browser::js::EventType::Click,
            [](browser::html::Node& doc, const browser::js::DomEvent&) {
                browser::js::set_style_by_id(doc, "title", "backgroundColor", "yellow");
            });

        auto initial_pixels = pipeline.canvas().pixels();

        // Dispatch click event
        browser::js::DomEvent click{browser::js::EventType::Click, "title", ""};
        registry.dispatch(pipeline.document(), click);

        // Re-render after event
        pipeline.rerender();

        if (initial_pixels == pipeline.canvas().pixels()) {
            std::cerr << "FAIL: event-driven mutation didn't change render\n";
            ++failures;
        } else {
            std::cerr << "PASS: event-driven mutation + rerender works\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll rerender mutation tests PASSED\n";
    return 0;
}
