// Test: Verify geometry stability for repeated fixture runs
// Story 3.2 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/layout/layout_engine.h"
#include "browser/render/canvas.h"
#include "browser/render/renderer.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    const std::string html = R"(
        <html>
        <head><title>Geometry Fixture</title></head>
        <body style="margin: 0; padding: 0;">
            <div style="width: 400px; padding: 20px; background-color: white;">
                <h1 style="font-size: 24px; margin: 10px 0;">Heading</h1>
                <p style="font-size: 14px; line-height: 20px;">
                    A paragraph with enough text to test wrapping behavior
                    across multiple lines of content.
                </p>
                <div style="padding: 10px; border: 2px solid black;">
                    <span style="font-size: 12px;">Nested content</span>
                </div>
            </div>
        </body>
        </html>
    )";
    const std::string css = "";
    const int viewport_width = 800;
    const int viewport_height = 600;

    // Run 1
    auto dom1 = browser::html::parse_html(html);
    auto sheet1 = browser::css::parse_css(css);
    auto layout1 = browser::layout::layout_document(*dom1, sheet1, viewport_width);
    auto canvas1 = browser::render::render_to_canvas(layout1, viewport_width, viewport_height);

    // Run 2
    auto dom2 = browser::html::parse_html(html);
    auto sheet2 = browser::css::parse_css(css);
    auto layout2 = browser::layout::layout_document(*dom2, sheet2, viewport_width);
    auto canvas2 = browser::render::render_to_canvas(layout2, viewport_width, viewport_height);

    // Test 1: Layout geometry matches
    {
        const auto s1 = browser::layout::serialize_layout(layout1);
        const auto s2 = browser::layout::serialize_layout(layout2);
        if (s1 != s2) {
            std::cerr << "FAIL: layout geometry differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: layout geometry is identical\n";
        }
    }

    // Test 2: Canvas dimensions match
    {
        if (canvas1.width() != canvas2.width() ||
            canvas1.height() != canvas2.height()) {
            std::cerr << "FAIL: canvas dimensions differ\n";
            ++failures;
        } else {
            std::cerr << "PASS: canvas dimensions match ("
                      << canvas1.width() << "x" << canvas1.height() << ")\n";
        }
    }

    // Test 3: Pixel data is identical (raw uint8_t bytes)
    {
        const auto& px1 = canvas1.pixels();
        const auto& px2 = canvas2.pixels();

        if (px1.size() != px2.size()) {
            std::cerr << "FAIL: pixel buffer size differs\n";
            ++failures;
        } else if (px1 != px2) {
            std::size_t diff_count = 0;
            for (std::size_t i = 0; i < px1.size(); ++i) {
                if (px1[i] != px2[i]) ++diff_count;
            }
            std::cerr << "FAIL: " << diff_count << " bytes differ out of "
                      << px1.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: all " << px1.size() << " pixel bytes are identical\n";
        }
    }

    // Test 4: 10 repeated renders all produce identical pixels
    {
        const auto& ref_pixels = canvas1.pixels();
        bool all_identical = true;

        for (int i = 0; i < 10; ++i) {
            auto dom = browser::html::parse_html(html);
            auto sheet = browser::css::parse_css(css);
            auto layout = browser::layout::layout_document(*dom, sheet, viewport_width);
            auto canvas = browser::render::render_to_canvas(layout, viewport_width, viewport_height);
            const auto& pixels = canvas.pixels();

            if (pixels != ref_pixels) {
                all_identical = false;
                break;
            }
        }

        if (!all_identical) {
            std::cerr << "FAIL: 10-run pixel stability check failed\n";
            ++failures;
        } else {
            std::cerr << "PASS: 10 consecutive renders produce identical pixels\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll geometry stability tests PASSED\n";
    return 0;
}
