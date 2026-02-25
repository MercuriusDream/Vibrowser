// Test: Verify retry preserves session context and re-runs navigation
// Story 1.3 acceptance test

#include "browser/engine/engine.h"
#include "browser/core/lifecycle.h"

#include <algorithm>
#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Retry with no prior navigation
    {
        browser::engine::BrowserEngine engine;
        const auto result = engine.retry();
        if (result.ok) {
            std::cerr << "FAIL: retry should fail with no prior navigation\n";
            ++failures;
        } else {
            std::cerr << "PASS: retry correctly fails with no prior navigation\n";
        }
    }

    // Test 2: Retry after failed navigation preserves prior diagnostics
    {
        browser::engine::BrowserEngine engine;
        browser::engine::RenderOptions opts;
        opts.output_path = "test_retry_out.ppm";

        // Navigate to nonexistent file - should fail
        const auto first = engine.navigate("nonexistent_file_12345.html", opts);
        if (first.ok) {
            std::cerr << "SKIP: expected first navigation to fail\n";
        } else {
            std::cerr << "PASS: first navigation failed as expected\n";

            const std::size_t prior_diag_count = first.session.diagnostics.size();
            if (prior_diag_count == 0) {
                std::cerr << "FAIL: no diagnostics from failed navigation\n";
                ++failures;
            }

            // Retry - should also fail (same nonexistent file) but preserve prior diags
            const auto retry_result = engine.retry();

            if (retry_result.session.diagnostics.size() <= prior_diag_count) {
                std::cerr << "FAIL: retry did not preserve prior diagnostics. "
                          << "prior=" << prior_diag_count
                          << " retry=" << retry_result.session.diagnostics.size() << "\n";
                ++failures;
            } else {
                std::cerr << "PASS: retry preserved prior diagnostics ("
                          << prior_diag_count << " prior + new = "
                          << retry_result.session.diagnostics.size() << " total)\n";
            }

            // Check that a retry diagnostic message exists
            bool has_retry_msg = std::any_of(
                retry_result.session.diagnostics.begin(),
                retry_result.session.diagnostics.end(),
                [](const browser::core::DiagnosticEvent& e) {
                    return e.message.find("Retry requested") != std::string::npos;
                });
            if (!has_retry_msg) {
                std::cerr << "FAIL: no 'Retry requested' diagnostic found\n";
                ++failures;
            } else {
                std::cerr << "PASS: 'Retry requested' diagnostic found\n";
            }
        }
    }

    // Test 3: Retry after successful navigation re-runs and succeeds
    {
        browser::engine::BrowserEngine engine;
        browser::engine::RenderOptions opts;
        opts.output_path = "test_retry_success_out.ppm";

        const auto first = engine.navigate("examples/smoke_sample.html", opts);
        if (!first.ok) {
            std::cerr << "FAIL: first navigation should succeed: " << first.message << "\n";
            ++failures;
        } else {
            const auto retry_result = engine.retry();
            if (!retry_result.ok) {
                std::cerr << "FAIL: retry should succeed: " << retry_result.message << "\n";
                ++failures;
            } else {
                std::cerr << "PASS: retry after successful navigation succeeds\n";
            }
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll retry navigation tests PASSED\n";
    return 0;
}
