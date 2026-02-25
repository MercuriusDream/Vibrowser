// Test: Verify deterministic lifecycle transitions for repeated same input
// Story 1.5 acceptance test

#include "browser/engine/engine.h"
#include "browser/core/lifecycle.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test: Run same input twice and verify stage order + timing reproducibility
    {
        browser::engine::BrowserEngine engine;
        browser::engine::RenderOptions opts;
        opts.output_path = "test_deterministic_1.ppm";

        const auto result1 = engine.navigate("examples/smoke_sample.html", opts);
        if (!result1.ok) {
            std::cerr << "FAIL: first run failed: " << result1.message << "\n";
            return 1;
        }

        const auto trace1 = result1.session.trace;

        opts.output_path = "test_deterministic_2.ppm";
        const auto result2 = engine.navigate("examples/smoke_sample.html", opts);
        if (!result2.ok) {
            std::cerr << "FAIL: second run failed: " << result2.message << "\n";
            return 1;
        }

        const auto trace2 = result2.session.trace;

        // Stage count must match
        if (trace1.entries.size() != trace2.entries.size()) {
            std::cerr << "FAIL: trace entry count differs: "
                      << trace1.entries.size() << " vs " << trace2.entries.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: trace entry count matches (" << trace1.entries.size() << ")\n";
        }

        // Stage order must match exactly
        bool order_match = true;
        for (std::size_t i = 0; i < trace1.entries.size() && i < trace2.entries.size(); ++i) {
            if (trace1.entries[i].stage != trace2.entries[i].stage) {
                std::cerr << "FAIL: stage mismatch at index " << i << ": "
                          << browser::core::lifecycle_stage_name(trace1.entries[i].stage) << " vs "
                          << browser::core::lifecycle_stage_name(trace2.entries[i].stage) << "\n";
                order_match = false;
                ++failures;
            }
        }
        if (order_match) {
            std::cerr << "PASS: stage order is identical across runs\n";
        }

        // Print timing comparison
        std::cerr << "\n--- Timing comparison ---\n";
        for (std::size_t i = 0; i < trace1.entries.size() && i < trace2.entries.size(); ++i) {
            std::cerr << browser::core::lifecycle_stage_name(trace1.entries[i].stage)
                      << ": run1=" << trace1.entries[i].elapsed_since_prev_ms
                      << "ms, run2=" << trace2.entries[i].elapsed_since_prev_ms << "ms\n";
        }

        // Reproducibility check with 3x tolerance
        if (trace1.is_reproducible_with(trace2, 3.0)) {
            std::cerr << "PASS: traces are reproducible within 3x tolerance\n";
        } else {
            std::cerr << "FAIL: traces are NOT reproducible within 3x tolerance\n";
            ++failures;
        }
    }

    // Test: LifecycleTrace::is_reproducible_with with mismatched stages
    {
        browser::core::LifecycleTrace a;
        a.record(browser::core::LifecycleStage::Idle);
        a.record(browser::core::LifecycleStage::Fetching);

        browser::core::LifecycleTrace b;
        b.record(browser::core::LifecycleStage::Idle);
        b.record(browser::core::LifecycleStage::Parsing);

        if (a.is_reproducible_with(b)) {
            std::cerr << "FAIL: mismatched stages should not be reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: mismatched stages correctly detected\n";
        }
    }

    // Test: Different length traces
    {
        browser::core::LifecycleTrace a;
        a.record(browser::core::LifecycleStage::Idle);

        browser::core::LifecycleTrace b;
        b.record(browser::core::LifecycleStage::Idle);
        b.record(browser::core::LifecycleStage::Fetching);

        if (a.is_reproducible_with(b)) {
            std::cerr << "FAIL: different length traces should not be reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: different length traces correctly detected\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll deterministic lifecycle tests PASSED\n";
    return 0;
}
