// Test: Trace layout and paint transitions per fixture
// Story 3.5 acceptance test

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
            <h1>Trace Test</h1>
            <p>Layout to paint transitions.</p>
        </body></html>
    )";
    const std::string css = "h1 { font-size: 24px; } p { font-size: 14px; }";

    auto dom = browser::html::parse_html(html);
    auto sheet = browser::css::parse_css(css);
    auto layout = browser::layout::layout_document(*dom, sheet, 800);

    // Test 1: Traced render records expected stages
    {
        browser::render::RenderTrace trace;
        auto canvas = browser::render::render_to_canvas_traced(layout, 800, 600, trace);

        if (trace.entries.size() != 4) {
            std::cerr << "FAIL: expected 4 trace entries, got " << trace.entries.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: trace has 4 entries\n";
        }

        if (trace.entries.size() >= 4) {
            if (trace.entries[0].stage != browser::render::RenderStage::CanvasInit) {
                std::cerr << "FAIL: first stage should be CanvasInit\n";
                ++failures;
            } else {
                std::cerr << "PASS: first stage is CanvasInit\n";
            }

            if (trace.entries[1].stage != browser::render::RenderStage::BackgroundResolve) {
                std::cerr << "FAIL: second stage should be BackgroundResolve\n";
                ++failures;
            } else {
                std::cerr << "PASS: second stage is BackgroundResolve\n";
            }

            if (trace.entries[2].stage != browser::render::RenderStage::Paint) {
                std::cerr << "FAIL: third stage should be Paint\n";
                ++failures;
            } else {
                std::cerr << "PASS: third stage is Paint\n";
            }

            if (trace.entries[3].stage != browser::render::RenderStage::Complete) {
                std::cerr << "FAIL: fourth stage should be Complete\n";
                ++failures;
            } else {
                std::cerr << "PASS: fourth stage is Complete\n";
            }
        }

        // Canvas should still be valid
        if (canvas.empty()) {
            std::cerr << "FAIL: traced canvas is empty\n";
            ++failures;
        } else {
            std::cerr << "PASS: traced canvas is valid\n";
        }
    }

    // Test 2: render_stage_name returns correct names
    {
        bool ok = true;
        if (std::string(browser::render::render_stage_name(browser::render::RenderStage::CanvasInit)) != "CanvasInit") ok = false;
        if (std::string(browser::render::render_stage_name(browser::render::RenderStage::BackgroundResolve)) != "BackgroundResolve") ok = false;
        if (std::string(browser::render::render_stage_name(browser::render::RenderStage::Paint)) != "Paint") ok = false;
        if (std::string(browser::render::render_stage_name(browser::render::RenderStage::Complete)) != "Complete") ok = false;

        if (!ok) {
            std::cerr << "FAIL: render_stage_name returned incorrect values\n";
            ++failures;
        } else {
            std::cerr << "PASS: render_stage_name returns correct names\n";
        }
    }

    // Test 3: Elapsed times are non-negative
    {
        browser::render::RenderTrace trace;
        browser::render::render_to_canvas_traced(layout, 800, 600, trace);

        bool all_ok = true;
        for (std::size_t i = 1; i < trace.entries.size(); ++i) {
            if (trace.entries[i].elapsed_since_prev_ms < 0.0) {
                all_ok = false;
            }
        }

        if (!all_ok) {
            std::cerr << "FAIL: some elapsed times are negative\n";
            ++failures;
        } else {
            std::cerr << "PASS: all elapsed times are non-negative\n";
        }
    }

    // Test 4: Trace is reproducible between runs
    {
        browser::render::RenderTrace trace1;
        browser::render::render_to_canvas_traced(layout, 800, 600, trace1);

        auto dom2 = browser::html::parse_html(html);
        auto sheet2 = browser::css::parse_css(css);
        auto layout2 = browser::layout::layout_document(*dom2, sheet2, 800);

        browser::render::RenderTrace trace2;
        browser::render::render_to_canvas_traced(layout2, 800, 600, trace2);

        if (!trace1.is_reproducible_with(trace2)) {
            std::cerr << "FAIL: traces are not reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: traces are reproducible\n";
        }
    }

    // Test 5: Traced render with metadata
    {
        browser::render::RenderMetadata meta;
        browser::render::RenderTrace trace;
        auto canvas = browser::render::render_to_canvas_traced(layout, 800, 600, meta, trace);

        if (meta.width != 800 || meta.height != 600) {
            std::cerr << "FAIL: traced+metadata has wrong dimensions\n";
            ++failures;
        } else {
            std::cerr << "PASS: traced+metadata has correct dimensions\n";
        }

        if (trace.entries.size() != 4) {
            std::cerr << "FAIL: traced+metadata has wrong entry count\n";
            ++failures;
        } else {
            std::cerr << "PASS: traced+metadata has correct trace entries\n";
        }
    }

    // Test 6: write_render_trace writes valid file
    {
        browser::render::RenderTrace trace;
        browser::render::render_to_canvas_traced(layout, 800, 600, trace);

        const std::string trace_path = "test_render_trace.txt";
        if (!browser::render::write_render_trace(trace, trace_path)) {
            std::cerr << "FAIL: write_render_trace returned false\n";
            ++failures;
        } else {
            std::ifstream f(trace_path);
            std::string line;
            int line_count = 0;
            bool has_canvas_init = false;
            bool has_paint = false;
            bool has_complete = false;

            while (std::getline(f, line)) {
                ++line_count;
                if (line.find("stage=CanvasInit") != std::string::npos) has_canvas_init = true;
                if (line.find("stage=Paint") != std::string::npos) has_paint = true;
                if (line.find("stage=Complete") != std::string::npos) has_complete = true;
            }

            if (line_count < 4) {
                std::cerr << "FAIL: trace file has only " << line_count << " lines\n";
                ++failures;
            } else {
                std::cerr << "PASS: trace file has " << line_count << " lines\n";
            }

            if (!has_canvas_init || !has_paint || !has_complete) {
                std::cerr << "FAIL: trace file missing expected stages\n";
                ++failures;
            } else {
                std::cerr << "PASS: trace file contains all expected stages\n";
            }

            std::filesystem::remove(trace_path);
        }
    }

    // Test 7: Traced render produces same pixels as non-traced
    {
        browser::render::RenderTrace trace;
        auto c1 = browser::render::render_to_canvas(layout, 800, 600);
        auto c2 = browser::render::render_to_canvas_traced(layout, 800, 600, trace);

        if (c1.pixels() != c2.pixels()) {
            std::cerr << "FAIL: traced render produces different pixels\n";
            ++failures;
        } else {
            std::cerr << "PASS: traced render produces identical pixels\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll render trace tests PASSED\n";
    return 0;
}
