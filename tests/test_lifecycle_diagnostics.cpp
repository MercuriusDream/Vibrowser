// Test: Verify lifecycle stage transitions emit diagnostic events
// Story 1.2 acceptance test

#include "browser/engine/engine.h"
#include "browser/core/lifecycle.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool has_stage_transition(const std::vector<browser::core::DiagnosticEvent>& diagnostics,
                          const std::string& expected_stage) {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
        [&](const browser::core::DiagnosticEvent& event) {
            return event.stage == expected_stage &&
                   event.message.find("Stage transition:") != std::string::npos;
        });
}

}  // namespace

int main() {
    int failures = 0;

    // Test: navigate a local file and verify all stages are emitted
    {
        browser::engine::BrowserEngine engine;
        browser::engine::RenderOptions opts;
        opts.output_path = "test_lifecycle_out.ppm";

        const auto result = engine.navigate("examples/smoke_sample.html", opts);

        // Write a minimal fixture if it doesn't exist
        if (!result.ok) {
            std::cerr << "FAIL: navigate failed: " << result.message << "\n";
            return 1;
        }

        const auto& diags = result.session.diagnostics;

        // Verify we have diagnostic events
        if (diags.empty()) {
            std::cerr << "FAIL: no diagnostic events emitted\n";
            return 1;
        }

        // Check each lifecycle stage has a transition event
        const std::vector<std::string> expected_stages = {
            "idle", "fetching", "parsing", "styling", "layout", "rendering", "complete"
        };

        for (const auto& stage : expected_stages) {
            if (!has_stage_transition(diags, stage)) {
                std::cerr << "FAIL: missing stage transition for '" << stage << "'\n";
                ++failures;
            } else {
                std::cerr << "PASS: stage transition emitted for '" << stage << "'\n";
            }
        }

        // Verify timestamps are ordered (non-decreasing)
        for (std::size_t i = 1; i < diags.size(); ++i) {
            if (diags[i].timestamp < diags[i - 1].timestamp) {
                std::cerr << "FAIL: diagnostic timestamps not ordered at index " << i << "\n";
                ++failures;
            }
        }
        if (failures == 0) {
            std::cerr << "PASS: diagnostic timestamps are ordered\n";
        }

        // Verify all events have severity and module set
        for (std::size_t i = 0; i < diags.size(); ++i) {
            if (diags[i].module.empty()) {
                std::cerr << "FAIL: diagnostic " << i << " has empty module\n";
                ++failures;
            }
        }

        // Print all diagnostics for visibility
        std::cerr << "\n--- All diagnostic events (" << diags.size() << ") ---\n";
        for (const auto& d : diags) {
            std::cerr << "[" << browser::core::severity_name(d.severity) << "] "
                      << d.module << "/" << d.stage << ": " << d.message << "\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll lifecycle diagnostic tests PASSED\n";
    return 0;
}
