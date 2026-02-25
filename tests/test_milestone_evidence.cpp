// Test: Track milestone acceptance evidence from fixture runs
// Story 6.2 acceptance test

#include "browser/core/milestone.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: GateStatus names
    {
        bool ok = true;
        if (std::string(browser::core::gate_status_name(browser::core::GateStatus::Pending)) != "Pending") ok = false;
        if (std::string(browser::core::gate_status_name(browser::core::GateStatus::Passed)) != "Passed") ok = false;
        if (std::string(browser::core::gate_status_name(browser::core::GateStatus::Failed)) != "Failed") ok = false;
        if (!ok) {
            std::cerr << "FAIL: gate_status_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: gate_status_name correct\n";
        }
    }

    // Test 2: Add gates and evaluate all — all pass
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("parse_ok", [](std::string& detail) {
            detail = "HTML parsing produces valid DOM";
            return true;
        });
        tracker.add_gate("style_ok", [](std::string& detail) {
            detail = "CSS cascade resolves correctly";
            return true;
        });
        tracker.evaluate_all();

        auto s = tracker.summary();
        if (!s.all_passed()) {
            std::cerr << "FAIL: expected all_passed\n";
            ++failures;
        } else if (s.total != 2 || s.passed != 2 || s.failed != 0) {
            std::cerr << "FAIL: summary counts wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: all gates pass, summary correct\n";
        }
    }

    // Test 3: Mixed pass/fail
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("layout", [](std::string& d) { d = "ok"; return true; });
        tracker.add_gate("render", [](std::string& d) { d = "canvas empty"; return false; });
        tracker.add_gate("export", [](std::string& d) { d = "ok"; return true; });
        tracker.evaluate_all();

        auto s = tracker.summary();
        if (s.all_passed()) {
            std::cerr << "FAIL: should not be all_passed with a failure\n";
            ++failures;
        } else if (s.passed != 2 || s.failed != 1) {
            std::cerr << "FAIL: expected 2 passed, 1 failed\n";
            ++failures;
        } else {
            std::cerr << "PASS: mixed pass/fail tracked correctly\n";
        }
    }

    // Test 4: Evidence includes detail and timestamp
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("check_1", [](std::string& d) { d = "detail_value"; return true; });
        tracker.evaluate_all();

        if (tracker.evidence().size() != 1) {
            std::cerr << "FAIL: expected 1 evidence entry\n";
            ++failures;
        } else {
            const auto& ev = tracker.evidence()[0];
            bool ok = true;
            if (ev.gate_name != "check_1") ok = false;
            if (ev.status != browser::core::GateStatus::Passed) ok = false;
            if (ev.detail != "detail_value") ok = false;
            if (ev.evaluated_at == std::chrono::steady_clock::time_point{}) ok = false;
            if (!ok) {
                std::cerr << "FAIL: evidence fields incorrect\n";
                ++failures;
            } else {
                std::cerr << "PASS: evidence includes all fields\n";
            }
        }
    }

    // Test 5: evaluate_gate updates single gate
    {
        browser::core::MilestoneTracker tracker;
        int call_count = 0;
        tracker.add_gate("flaky", [&](std::string& d) {
            ++call_count;
            if (call_count == 1) { d = "first try"; return false; }
            d = "second try"; return true;
        });
        tracker.add_gate("stable", [](std::string& d) { d = "ok"; return true; });

        tracker.evaluate_all();
        if (tracker.evidence()[0].status != browser::core::GateStatus::Failed) {
            std::cerr << "FAIL: first eval should fail\n";
            ++failures;
        } else {
            tracker.evaluate_gate("flaky");
            if (tracker.evidence()[0].status != browser::core::GateStatus::Passed) {
                std::cerr << "FAIL: re-eval should pass\n";
                ++failures;
            } else {
                std::cerr << "PASS: evaluate_gate updates single gate\n";
            }
        }
    }

    // Test 6: Summary with pending (no evaluate)
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("a", [](std::string& d) { d = ""; return true; });
        tracker.add_gate("b", [](std::string& d) { d = ""; return true; });

        auto s = tracker.summary();
        if (s.pending != 2 || s.total != 2) {
            std::cerr << "FAIL: unevaluated gates should be pending\n";
            ++failures;
        } else if (s.all_passed()) {
            std::cerr << "FAIL: pending gates should prevent all_passed\n";
            ++failures;
        } else {
            std::cerr << "PASS: unevaluated gates are pending\n";
        }
    }

    // Test 7: format_report includes gate names and status
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("gate_A", [](std::string& d) { d = "good"; return true; });
        tracker.add_gate("gate_B", [](std::string& d) { d = "bad"; return false; });
        tracker.evaluate_all();

        std::string report = tracker.format_report();
        bool ok = true;
        if (report.find("gate_A") == std::string::npos) ok = false;
        if (report.find("gate_B") == std::string::npos) ok = false;
        if (report.find("Passed") == std::string::npos) ok = false;
        if (report.find("Failed") == std::string::npos) ok = false;
        if (report.find("1/2 passed") == std::string::npos) ok = false;
        if (!ok) {
            std::cerr << "FAIL: format_report missing content: " << report << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: format_report includes gates and status\n";
        }
    }

    // Test 8: Clear removes gates and evidence
    {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("x", [](std::string& d) { d = ""; return true; });
        tracker.evaluate_all();
        tracker.clear();

        if (tracker.gate_count() != 0 || !tracker.evidence().empty()) {
            std::cerr << "FAIL: clear should remove everything\n";
            ++failures;
        } else {
            std::cerr << "PASS: clear removes gates and evidence\n";
        }
    }

    // Test 9: Deterministic — same gates produce same evidence
    {
        auto make_tracker = []() {
            browser::core::MilestoneTracker t;
            t.add_gate("g1", [](std::string& d) { d = "ok"; return true; });
            t.add_gate("g2", [](std::string& d) { d = "fail"; return false; });
            t.evaluate_all();
            return t;
        };

        auto t1 = make_tracker();
        auto t2 = make_tracker();

        bool match = (t1.evidence().size() == t2.evidence().size());
        for (std::size_t i = 0; match && i < t1.evidence().size(); ++i) {
            if (t1.evidence()[i].gate_name != t2.evidence()[i].gate_name) match = false;
            if (t1.evidence()[i].status != t2.evidence()[i].status) match = false;
            if (t1.evidence()[i].detail != t2.evidence()[i].detail) match = false;
        }

        if (!match) {
            std::cerr << "FAIL: milestone evidence not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: milestone evidence is deterministic\n";
        }
    }

    // Test 10: MilestoneSummary::all_passed edge case — empty tracker
    {
        browser::core::MilestoneSummary s;
        s.total = 0;
        s.passed = 0;
        s.failed = 0;
        s.pending = 0;
        if (s.all_passed()) {
            std::cerr << "FAIL: empty tracker should not be all_passed\n";
            ++failures;
        } else {
            std::cerr << "PASS: empty tracker not all_passed\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll milestone evidence tests PASSED\n";
    return 0;
}
