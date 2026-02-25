// Test: Export render artifacts and render metadata
// Story 3.4 acceptance test

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
            <h1>Artifact Test</h1>
            <p>Export metadata and artifacts.</p>
        </body></html>
    )";
    const std::string css = "h1 { font-size: 24px; } p { font-size: 14px; }";

    auto dom = browser::html::parse_html(html);
    auto sheet = browser::css::parse_css(css);
    auto layout = browser::layout::layout_document(*dom, sheet, 800);

    // Test 1: render_to_canvas with metadata populates all fields
    {
        browser::render::RenderMetadata meta;
        auto canvas = browser::render::render_to_canvas(layout, 800, 600, meta);

        if (meta.width != 800) {
            std::cerr << "FAIL: metadata width expected 800, got " << meta.width << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: metadata width is correct\n";
        }

        if (meta.height != 600) {
            std::cerr << "FAIL: metadata height expected 600, got " << meta.height << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: metadata height is correct\n";
        }

        const std::size_t expected_pixels = 800 * 600;
        if (meta.pixel_count != expected_pixels) {
            std::cerr << "FAIL: metadata pixel_count expected " << expected_pixels
                      << ", got " << meta.pixel_count << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: metadata pixel_count is correct\n";
        }

        // byte_count = width * height * 3 (RGB)
        const std::size_t expected_bytes = 800 * 600 * 3;
        if (meta.byte_count != expected_bytes) {
            std::cerr << "FAIL: metadata byte_count expected " << expected_bytes
                      << ", got " << meta.byte_count << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: metadata byte_count is correct\n";
        }

        if (meta.render_duration_ms < 0.0) {
            std::cerr << "FAIL: render_duration_ms should be non-negative, got "
                      << meta.render_duration_ms << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: render_duration_ms is non-negative ("
                      << meta.render_duration_ms << " ms)\n";
        }

        // Canvas should still be valid
        if (canvas.empty()) {
            std::cerr << "FAIL: canvas from metadata overload is empty\n";
            ++failures;
        } else {
            std::cerr << "PASS: canvas from metadata overload is valid\n";
        }
    }

    // Test 2: write_render_metadata writes valid key=value file
    {
        browser::render::RenderMetadata meta;
        auto canvas = browser::render::render_to_canvas(layout, 640, 480, meta);

        const std::string meta_path = "test_render_metadata.txt";
        if (!browser::render::write_render_metadata(meta, meta_path)) {
            std::cerr << "FAIL: write_render_metadata returned false\n";
            ++failures;
        } else {
            std::ifstream f(meta_path);
            std::string line;
            int line_count = 0;
            bool has_width = false;
            bool has_height = false;
            bool has_pixel_count = false;
            bool has_byte_count = false;
            bool has_duration = false;

            while (std::getline(f, line)) {
                ++line_count;
                if (line.find("width=640") != std::string::npos) has_width = true;
                if (line.find("height=480") != std::string::npos) has_height = true;
                if (line.find("pixel_count=") != std::string::npos) has_pixel_count = true;
                if (line.find("byte_count=") != std::string::npos) has_byte_count = true;
                if (line.find("render_duration_ms=") != std::string::npos) has_duration = true;
            }

            if (line_count < 5) {
                std::cerr << "FAIL: metadata file has only " << line_count << " lines\n";
                ++failures;
            } else {
                std::cerr << "PASS: metadata file has " << line_count << " lines\n";
            }

            if (!has_width || !has_height) {
                std::cerr << "FAIL: metadata file missing width/height\n";
                ++failures;
            } else {
                std::cerr << "PASS: metadata file has width and height\n";
            }

            if (!has_pixel_count || !has_byte_count) {
                std::cerr << "FAIL: metadata file missing pixel_count/byte_count\n";
                ++failures;
            } else {
                std::cerr << "PASS: metadata file has pixel_count and byte_count\n";
            }

            if (!has_duration) {
                std::cerr << "FAIL: metadata file missing render_duration_ms\n";
                ++failures;
            } else {
                std::cerr << "PASS: metadata file has render_duration_ms\n";
            }

            std::filesystem::remove(meta_path);
        }
    }

    // Test 3: PPM artifact + metadata can both be written for same render
    {
        browser::render::RenderMetadata meta;
        auto canvas = browser::render::render_to_canvas(layout, 800, 600, meta);

        const std::string ppm_path = "test_artifact.ppm";
        const std::string meta_path = "test_artifact_meta.txt";

        bool ppm_ok = browser::render::write_ppm(canvas, ppm_path);
        bool meta_ok = browser::render::write_render_metadata(meta, meta_path);

        if (!ppm_ok) {
            std::cerr << "FAIL: write_ppm failed for artifact export\n";
            ++failures;
        } else if (!meta_ok) {
            std::cerr << "FAIL: write_render_metadata failed for artifact export\n";
            ++failures;
        } else {
            // Verify both files exist
            bool ppm_exists = std::filesystem::exists(ppm_path);
            bool meta_exists = std::filesystem::exists(meta_path);

            if (!ppm_exists || !meta_exists) {
                std::cerr << "FAIL: artifact files not found on disk\n";
                ++failures;
            } else {
                auto ppm_size = std::filesystem::file_size(ppm_path);
                auto meta_size = std::filesystem::file_size(meta_path);
                if (ppm_size == 0 || meta_size == 0) {
                    std::cerr << "FAIL: artifact files are empty\n";
                    ++failures;
                } else {
                    std::cerr << "PASS: both PPM (" << ppm_size << " bytes) and metadata ("
                              << meta_size << " bytes) artifacts exported\n";
                }
            }
        }

        std::filesystem::remove(ppm_path);
        std::filesystem::remove(meta_path);
    }

    // Test 4: write_render_metadata rejects empty path
    {
        browser::render::RenderMetadata meta;
        if (browser::render::write_render_metadata(meta, "")) {
            std::cerr << "FAIL: write_render_metadata should reject empty path\n";
            ++failures;
        } else {
            std::cerr << "PASS: write_render_metadata rejects empty path\n";
        }
    }

    // Test 5: Metadata-collecting overload produces same canvas as non-metadata overload
    {
        browser::render::RenderMetadata meta;
        auto c1 = browser::render::render_to_canvas(layout, 800, 600);
        auto c2 = browser::render::render_to_canvas(layout, 800, 600, meta);

        if (c1.pixels() != c2.pixels()) {
            std::cerr << "FAIL: metadata overload produces different pixels\n";
            ++failures;
        } else {
            std::cerr << "PASS: metadata overload produces identical pixels\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll render artifact tests PASSED\n";
    return 0;
}
