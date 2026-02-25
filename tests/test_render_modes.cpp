// Test: Verify rendering through both headless (PPM) and shell (text) paths
// Story 3.3 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/layout/layout_engine.h"
#include "browser/render/canvas.h"
#include "browser/render/renderer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    int failures = 0;

    const std::string html = R"(
        <html><body>
            <h1>Test Page</h1>
            <p>Hello, world!</p>
        </body></html>
    )";
    const std::string css = "h1 { font-size: 24px; } p { font-size: 14px; }";

    auto dom = browser::html::parse_html(html);
    auto sheet = browser::css::parse_css(css);
    auto layout = browser::layout::layout_document(*dom, sheet, 800);

    // Test 1: Headless (PPM) mode produces valid output
    {
        auto canvas = browser::render::render_to_canvas(layout, 800, 600);

        if (canvas.empty()) {
            std::cerr << "FAIL: headless render produced empty canvas\n";
            ++failures;
        } else {
            std::cerr << "PASS: headless render produces non-empty canvas ("
                      << canvas.width() << "x" << canvas.height() << ")\n";
        }

        const std::string ppm_path = "test_render_modes.ppm";
        if (!browser::render::write_ppm(canvas, ppm_path)) {
            std::cerr << "FAIL: write_ppm failed\n";
            ++failures;
        } else {
            // Verify PPM file exists and has valid header
            std::ifstream f(ppm_path, std::ios::binary);
            std::string magic;
            std::getline(f, magic);
            if (magic != "P6") {
                std::cerr << "FAIL: PPM header is not P6: " << magic << "\n";
                ++failures;
            } else {
                std::cerr << "PASS: PPM output has valid P6 header\n";
            }
            std::filesystem::remove(ppm_path);
        }
    }

    // Test 2: Shell (text) mode produces non-empty text output
    {
        const std::string text = browser::render::render_to_text(layout, 80);

        if (text.empty()) {
            std::cerr << "FAIL: shell render produced empty text\n";
            ++failures;
        } else {
            std::cerr << "PASS: shell render produces text output ("
                      << text.size() << " chars)\n";
        }

        // Should contain the visible text content
        if (text.find("Test Page") == std::string::npos) {
            std::cerr << "FAIL: shell output missing 'Test Page' content\n";
            ++failures;
        } else {
            std::cerr << "PASS: shell output contains expected text content\n";
        }

        if (text.find("Hello, world!") == std::string::npos) {
            std::cerr << "FAIL: shell output missing 'Hello, world!' content\n";
            ++failures;
        } else {
            std::cerr << "PASS: shell output contains paragraph text\n";
        }
    }

    // Test 3: Both modes produce deterministic output
    {
        auto dom2 = browser::html::parse_html(html);
        auto sheet2 = browser::css::parse_css(css);
        auto layout2 = browser::layout::layout_document(*dom2, sheet2, 800);

        // PPM determinism
        auto c1 = browser::render::render_to_canvas(layout, 800, 600);
        auto c2 = browser::render::render_to_canvas(layout2, 800, 600);
        if (c1.pixels() != c2.pixels()) {
            std::cerr << "FAIL: headless mode not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: headless mode is deterministic\n";
        }

        // Text determinism
        auto t1 = browser::render::render_to_text(layout, 80);
        auto t2 = browser::render::render_to_text(layout2, 80);
        if (t1 != t2) {
            std::cerr << "FAIL: shell mode not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: shell mode is deterministic\n";
        }
    }

    // Test 4: RenderMode enum exists
    {
        auto headless = browser::render::RenderMode::Headless;
        auto shell = browser::render::RenderMode::Shell;
        if (headless == shell) {
            std::cerr << "FAIL: render modes should differ\n";
            ++failures;
        } else {
            std::cerr << "PASS: RenderMode enum has distinct values\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll render mode tests PASSED\n";
    return 0;
}
