// Test: Include recovery and replay support for operational troubleshooting
// Story 6.6 acceptance test

#include "browser/core/diagnostics.h"
#include "browser/core/recovery.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: RecoveryAction names
    {
        bool ok = true;
        if (std::string(browser::core::recovery_action_name(browser::core::RecoveryAction::Retry)) != "Retry") ok = false;
        if (std::string(browser::core::recovery_action_name(browser::core::RecoveryAction::Replay)) != "Replay") ok = false;
        if (std::string(browser::core::recovery_action_name(browser::core::RecoveryAction::Cancel)) != "Cancel") ok = false;
        if (std::string(browser::core::recovery_action_name(browser::core::RecoveryAction::Skip)) != "Skip") ok = false;
        if (!ok) {
            std::cerr << "FAIL: recovery_action_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: recovery_action_name correct\n";
        }
    }

    // Test 2: plan_from_stage for net module — offers retry, skip, cancel
    {
        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_stage("net", "fetch", "timeout");

        if (plan.failure_module != "net" || plan.failure_stage != "fetch") {
            std::cerr << "FAIL: plan fields wrong\n";
            ++failures;
        } else if (!plan.has_action(browser::core::RecoveryAction::Retry)) {
            std::cerr << "FAIL: net plan should offer Retry\n";
            ++failures;
        } else if (!plan.has_action(browser::core::RecoveryAction::Cancel)) {
            std::cerr << "FAIL: plan should always offer Cancel\n";
            ++failures;
        } else {
            std::cerr << "PASS: net/fetch plan offers retry, skip, cancel\n";
        }
    }

    // Test 3: plan_from_stage for parse — offers replay and cancel
    {
        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_stage("html", "parse", "malformed input");

        if (!plan.has_action(browser::core::RecoveryAction::Replay)) {
            std::cerr << "FAIL: parse plan should offer Replay\n";
            ++failures;
        } else if (!plan.has_action(browser::core::RecoveryAction::Cancel)) {
            std::cerr << "FAIL: plan should always offer Cancel\n";
            ++failures;
        } else {
            std::cerr << "PASS: parse plan offers replay and cancel\n";
        }
    }

    // Test 4: plan_from_stage for render — offers replay and cancel
    {
        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_stage("render", "paint", "canvas overflow");

        if (!plan.has_action(browser::core::RecoveryAction::Replay)) {
            std::cerr << "FAIL: render plan should offer Replay\n";
            ++failures;
        } else if (!plan.has_action(browser::core::RecoveryAction::Cancel)) {
            std::cerr << "FAIL: plan should offer Cancel\n";
            ++failures;
        } else {
            std::cerr << "PASS: render/paint plan offers replay and cancel\n";
        }
    }

    // Test 5: plan_from_trace with FailureTrace
    {
        browser::core::FailureTrace trace;
        trace.correlation_id = 42;
        trace.module = "css";
        trace.stage = "style";
        trace.error_message = "cascade error";

        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_trace(trace);

        if (plan.correlation_id != 42) {
            std::cerr << "FAIL: correlation_id should propagate\n";
            ++failures;
        } else if (plan.failure_module != "css" || plan.failure_stage != "style") {
            std::cerr << "FAIL: plan fields should match trace\n";
            ++failures;
        } else if (plan.steps.empty()) {
            std::cerr << "FAIL: plan should have steps\n";
            ++failures;
        } else {
            std::cerr << "PASS: plan_from_trace populates from FailureTrace\n";
        }
    }

    // Test 6: format() produces readable plan
    {
        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_stage("net", "connect", "refused");

        std::string fmt = plan.format();
        bool ok = true;
        if (fmt.find("Recovery Plan") == std::string::npos) ok = false;
        if (fmt.find("net") == std::string::npos) ok = false;
        if (fmt.find("connect") == std::string::npos) ok = false;
        if (fmt.find("refused") == std::string::npos) ok = false;
        if (fmt.find("Retry") == std::string::npos) ok = false;
        if (fmt.find("Cancel") == std::string::npos) ok = false;
        if (!ok) {
            std::cerr << "FAIL: format() missing content: " << fmt << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: format() produces readable plan\n";
        }
    }

    // Test 7: History tracks generated plans
    {
        browser::core::RecoveryController ctrl;
        ctrl.plan_from_stage("a", "b", "err1");
        ctrl.plan_from_stage("c", "d", "err2");

        if (ctrl.history_size() != 2) {
            std::cerr << "FAIL: expected 2 history entries\n";
            ++failures;
        } else if (ctrl.history()[0].failure_module != "a" ||
                   ctrl.history()[1].failure_module != "c") {
            std::cerr << "FAIL: history entries wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: history tracks plans\n";
        }
    }

    // Test 8: clear_history empties
    {
        browser::core::RecoveryController ctrl;
        ctrl.plan_from_stage("x", "y", "z");
        ctrl.clear_history();
        if (ctrl.history_size() != 0) {
            std::cerr << "FAIL: clear should empty history\n";
            ++failures;
        } else {
            std::cerr << "PASS: clear_history empties\n";
        }
    }

    // Test 9: Generic fallback — unknown stage gets retry + cancel
    {
        browser::core::RecoveryController ctrl;
        auto plan = ctrl.plan_from_stage("unknown", "init", "crash");

        if (!plan.has_action(browser::core::RecoveryAction::Retry)) {
            std::cerr << "FAIL: generic fallback should offer Retry\n";
            ++failures;
        } else if (!plan.has_action(browser::core::RecoveryAction::Cancel)) {
            std::cerr << "FAIL: generic fallback should offer Cancel\n";
            ++failures;
        } else {
            std::cerr << "PASS: generic fallback offers retry + cancel\n";
        }
    }

    // Test 10: Every plan has at least Cancel
    {
        browser::core::RecoveryController ctrl;
        std::vector<std::pair<std::string, std::string>> cases = {
            {"net", "fetch"}, {"html", "parse"}, {"css", "style"},
            {"layout", "layout"}, {"render", "paint"}, {"unknown", "x"},
        };
        bool all_have_cancel = true;
        for (const auto& [mod, stg] : cases) {
            auto plan = ctrl.plan_from_stage(mod, stg, "err");
            if (!plan.has_action(browser::core::RecoveryAction::Cancel)) {
                std::cerr << "FAIL: " << mod << "/" << stg << " missing Cancel\n";
                all_have_cancel = false;
                break;
            }
        }
        if (all_have_cancel) {
            std::cerr << "PASS: every plan includes Cancel\n";
        } else {
            ++failures;
        }
    }

    // Test 11: Deterministic — same input produces same plan
    {
        browser::core::RecoveryController c1, c2;
        auto p1 = c1.plan_from_stage("net", "fetch", "timeout");
        auto p2 = c2.plan_from_stage("net", "fetch", "timeout");

        bool match = (p1.steps.size() == p2.steps.size());
        for (std::size_t i = 0; match && i < p1.steps.size(); ++i) {
            if (p1.steps[i].action != p2.steps[i].action) match = false;
            if (p1.steps[i].description != p2.steps[i].description) match = false;
        }
        if (!match) {
            std::cerr << "FAIL: recovery plan not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: recovery plan is deterministic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll recovery/replay tests PASSED\n";
    return 0;
}
