// Test: Verify DOM and style transition consistency
// Story 2.5 acceptance test

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

struct NodeStyleSnapshot {
    std::string tag;
    std::string dom_serialization;
    std::map<std::string, std::string> computed_style;
};

void collect_snapshots(const browser::html::Node& node,
                       const browser::css::Stylesheet& sheet,
                       std::vector<NodeStyleSnapshot>& snapshots) {
    if (node.type == browser::html::NodeType::Element) {
        NodeStyleSnapshot snap;
        snap.tag = node.tag_name;
        snap.dom_serialization = browser::html::serialize_dom(node);
        snap.computed_style = browser::css::compute_style_for_node(node, sheet);
        snapshots.push_back(std::move(snap));
    }
    for (const auto& child : node.children) {
        if (child) {
            collect_snapshots(*child, sheet, snapshots);
        }
    }
}

}  // namespace

int main() {
    int failures = 0;

    // Test 1: Full fixture — parse then style, verify consistency across 2 runs
    {
        const std::string html = R"(
            <html>
            <head><title>Fixture</title></head>
            <body>
                <div id="header" class="banner" style="padding: 10px;">
                    <h1>Title</h1>
                    <nav>
                        <a href="#" class="link">Home</a>
                        <a href="#" class="link active">About</a>
                    </nav>
                </div>
                <main>
                    <p class="intro">First paragraph</p>
                    <p>Second paragraph</p>
                    <ul>
                        <li>Item A</li>
                        <li class="highlight">Item B</li>
                        <li>Item C</li>
                    </ul>
                </main>
                <footer><small>Copyright</small></footer>
            </body>
            </html>
        )";

        const std::string css = R"(
            body { color: black; font-size: 16px; }
            .banner { background-color: navy; color: white; }
            h1 { font-size: 24px; }
            .link { color: blue; }
            .link.active { color: red; font-size: 18px; }
            .intro { font-size: 20px; }
            li:first-child { color: green; }
            li:last-child { color: orange; }
            .highlight { background-color: yellow; }
            footer { color: gray; font-size: 12px; }
        )";

        // Run 1
        auto dom1 = browser::html::parse_html(html);
        auto sheet1 = browser::css::parse_css(css);
        std::vector<NodeStyleSnapshot> snaps1;
        collect_snapshots(*dom1, sheet1, snaps1);

        // Run 2
        auto dom2 = browser::html::parse_html(html);
        auto sheet2 = browser::css::parse_css(css);
        std::vector<NodeStyleSnapshot> snaps2;
        collect_snapshots(*dom2, sheet2, snaps2);

        // Compare
        if (snaps1.size() != snaps2.size()) {
            std::cerr << "FAIL: snapshot count differs: " << snaps1.size()
                      << " vs " << snaps2.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: snapshot count matches (" << snaps1.size() << " elements)\n";
        }

        bool all_match = true;
        for (std::size_t i = 0; i < snaps1.size() && i < snaps2.size(); ++i) {
            if (snaps1[i].dom_serialization != snaps2[i].dom_serialization) {
                std::cerr << "FAIL: DOM snapshot differs at element " << i
                          << " (" << snaps1[i].tag << ")\n";
                all_match = false;
                ++failures;
            }
            if (snaps1[i].computed_style != snaps2[i].computed_style) {
                std::cerr << "FAIL: style snapshot differs at element " << i
                          << " (" << snaps1[i].tag << ")\n";
                all_match = false;
                ++failures;
            }
        }
        if (all_match) {
            std::cerr << "PASS: all DOM and style snapshots are consistent\n";
        }

        // Verify DOM unchanged after style computation
        const std::string dom1_after = browser::html::serialize_dom(*dom1);
        auto dom1_fresh = browser::html::parse_html(html);
        const std::string dom1_before = browser::html::serialize_dom(*dom1_fresh);

        if (dom1_after != dom1_before) {
            std::cerr << "FAIL: DOM was modified by style computation\n";
            ++failures;
        } else {
            std::cerr << "PASS: DOM unchanged after style computation\n";
        }
    }

    // Test 2: 50 consecutive runs produce identical snapshots
    {
        const std::string html = "<div class=\"a\"><span id=\"b\">text</span></div>";
        const std::string css = ".a { color: red; } #b { font-size: 14px; }";

        auto ref_dom = browser::html::parse_html(html);
        auto ref_sheet = browser::css::parse_css(css);
        std::vector<NodeStyleSnapshot> ref_snaps;
        collect_snapshots(*ref_dom, ref_sheet, ref_snaps);

        bool all_match = true;
        for (int run = 0; run < 50; ++run) {
            auto dom = browser::html::parse_html(html);
            auto sheet = browser::css::parse_css(css);
            std::vector<NodeStyleSnapshot> snaps;
            collect_snapshots(*dom, sheet, snaps);

            if (snaps.size() != ref_snaps.size()) {
                all_match = false;
                break;
            }
            for (std::size_t i = 0; i < snaps.size(); ++i) {
                if (snaps[i].computed_style != ref_snaps[i].computed_style ||
                    snaps[i].dom_serialization != ref_snaps[i].dom_serialization) {
                    all_match = false;
                    break;
                }
            }
            if (!all_match) break;
        }

        if (!all_match) {
            std::cerr << "FAIL: 50-run consistency check failed\n";
            ++failures;
        } else {
            std::cerr << "PASS: 50 consecutive runs produce identical snapshots\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll parse-style consistency tests PASSED\n";
    return 0;
}
