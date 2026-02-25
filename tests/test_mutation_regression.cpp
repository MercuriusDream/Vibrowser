// Test: Deterministic mutation regression fixtures
// Story 4.5 acceptance test

#include "browser/css/css_parser.h"
#include "browser/engine/render_pipeline.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"
#include "browser/layout/layout_engine.h"
#include "browser/render/renderer.h"

#include <iostream>
#include <string>
#include <vector>

struct MutationFixture {
    std::string name;
    std::string html;
    std::string css;
    struct Step {
        enum class Type { SetStyle, SetText, SetAttribute, DispatchEvent };
        Type type;
        std::string target_id;
        std::string key;
        std::string value;
    };
    std::vector<Step> steps;
};

struct FixtureResult {
    std::string name;
    std::vector<std::uint8_t> final_pixels;
    std::string final_text;
    int render_count;
};

FixtureResult run_fixture(const MutationFixture& fixture) {
    auto dom = browser::html::parse_html(fixture.html);
    auto sheet = browser::css::parse_css(fixture.css);

    browser::engine::RenderPipeline pipeline(std::move(dom), std::move(sheet), 800, 600);

    for (const auto& step : fixture.steps) {
        switch (step.type) {
            case MutationFixture::Step::Type::SetStyle:
                browser::js::set_style_by_id(pipeline.document(), step.target_id, step.key, step.value);
                break;
            case MutationFixture::Step::Type::SetText:
                browser::js::set_text_by_id(pipeline.document(), step.target_id, step.value);
                break;
            case MutationFixture::Step::Type::SetAttribute:
                browser::js::set_attribute_by_id(pipeline.document(), step.target_id, step.key, step.value);
                break;
            case MutationFixture::Step::Type::DispatchEvent:
                // For regression, we just dispatch and let a pre-wired handler do its work
                break;
        }
    }

    pipeline.rerender();

    FixtureResult result;
    result.name = fixture.name;
    result.final_pixels = pipeline.canvas().pixels();
    result.final_text = browser::render::render_to_text(pipeline.layout(), 80);
    result.render_count = pipeline.render_count();
    return result;
}

int main() {
    int failures = 0;

    // Define fixtures
    std::vector<MutationFixture> fixtures = {
        {
            "style-background-change",
            R"(<html><body><div id="box" style="width:100px;height:100px;">Box</div></body></html>)",
            "div { background-color: white; }",
            {
                {MutationFixture::Step::Type::SetStyle, "box", "backgroundColor", "red"},
            }
        },
        {
            "text-content-update",
            R"(<html><body><h1 id="title">Original</h1><p id="body">Content</p></body></html>)",
            "h1 { font-size: 24px; } p { font-size: 14px; }",
            {
                {MutationFixture::Step::Type::SetText, "title", "", "Updated Title"},
                {MutationFixture::Step::Type::SetText, "body", "", "Updated Content"},
            }
        },
        {
            "multi-mutation-sequence",
            R"(<html><body><div id="a">A</div><div id="b">B</div></body></html>)",
            "div { color: black; }",
            {
                {MutationFixture::Step::Type::SetStyle, "a", "backgroundColor", "blue"},
                {MutationFixture::Step::Type::SetText, "a", "", "Modified A"},
                {MutationFixture::Step::Type::SetAttribute, "b", "class", "highlight"},
                {MutationFixture::Step::Type::SetStyle, "b", "color", "green"},
            }
        },
        {
            "attribute-mutations",
            R"(<html><body><p id="p1" class="old">Text</p></body></html>)",
            ".old { color: gray; } .new { color: black; }",
            {
                {MutationFixture::Step::Type::SetAttribute, "p1", "class", "new"},
                {MutationFixture::Step::Type::SetAttribute, "p1", "data-updated", "true"},
            }
        },
    };

    // Test 1: Each fixture produces deterministic output across two runs
    for (const auto& fixture : fixtures) {
        auto r1 = run_fixture(fixture);
        auto r2 = run_fixture(fixture);

        if (r1.final_pixels != r2.final_pixels) {
            std::cerr << "FAIL: fixture '" << fixture.name << "' pixels not deterministic\n";
            ++failures;
        } else if (r1.final_text != r2.final_text) {
            std::cerr << "FAIL: fixture '" << fixture.name << "' text not deterministic\n";
            ++failures;
        } else if (r1.render_count != r2.render_count) {
            std::cerr << "FAIL: fixture '" << fixture.name << "' render_count differs\n";
            ++failures;
        } else {
            std::cerr << "PASS: fixture '" << fixture.name << "' is deterministic\n";
        }
    }

    // Test 2: All fixtures produce non-empty render output
    for (const auto& fixture : fixtures) {
        auto result = run_fixture(fixture);

        if (result.final_pixels.empty()) {
            std::cerr << "FAIL: fixture '" << fixture.name << "' produced empty pixels\n";
            ++failures;
        } else {
            std::cerr << "PASS: fixture '" << fixture.name << "' produces non-empty output\n";
        }
    }

    // Test 3: Text mutation fixture reflects changes in text output
    {
        auto result = run_fixture(fixtures[1]);  // text-content-update
        if (result.final_text.find("Updated Title") == std::string::npos) {
            std::cerr << "FAIL: text-content-update missing 'Updated Title'\n";
            ++failures;
        } else if (result.final_text.find("Updated Content") == std::string::npos) {
            std::cerr << "FAIL: text-content-update missing 'Updated Content'\n";
            ++failures;
        } else {
            std::cerr << "PASS: text-content-update reflects mutations in text output\n";
        }
    }

    // Test 4: Each fixture has exactly 2 render cycles (init + rerender)
    for (const auto& fixture : fixtures) {
        auto result = run_fixture(fixture);
        if (result.render_count != 2) {
            std::cerr << "FAIL: fixture '" << fixture.name << "' render_count expected 2, got "
                      << result.render_count << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: fixture '" << fixture.name << "' has expected render count\n";
        }
    }

    // Test 5: Fixtures with different mutations produce different outputs
    {
        auto r1 = run_fixture(fixtures[0]);  // style-background-change
        auto r2 = run_fixture(fixtures[1]);  // text-content-update

        if (r1.final_pixels == r2.final_pixels) {
            std::cerr << "FAIL: different fixtures produced identical pixels\n";
            ++failures;
        } else {
            std::cerr << "PASS: different fixtures produce different outputs\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll mutation regression fixtures PASSED\n";
    return 0;
}
