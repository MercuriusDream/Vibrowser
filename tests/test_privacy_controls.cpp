// Test: Make privacy-sensitive behaviors explicit and opt-in only
// Story 6.3 acceptance test

#include "browser/core/privacy.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: Default PrivacySettings — all disabled
    {
        browser::core::PrivacySettings settings;
        if (settings.any_enabled()) {
            std::cerr << "FAIL: default settings should have nothing enabled\n";
            ++failures;
        } else if (!settings.all_disabled()) {
            std::cerr << "FAIL: all_disabled should be true\n";
            ++failures;
        } else if (!settings.enabled_features().empty()) {
            std::cerr << "FAIL: enabled_features should be empty\n";
            ++failures;
        } else {
            std::cerr << "PASS: default settings all disabled\n";
        }
    }

    // Test 2: Default PrivacyGuard blocks all features
    {
        browser::core::PrivacyGuard guard;
        bool ok = true;
        if (guard.is_allowed("telemetry")) ok = false;
        if (guard.is_allowed("crash_reporting")) ok = false;
        if (guard.is_allowed("usage_analytics")) ok = false;
        if (guard.is_allowed("diagnostic_export")) ok = false;
        if (!ok) {
            std::cerr << "FAIL: default guard should block all features\n";
            ++failures;
        } else {
            std::cerr << "PASS: default guard blocks all features\n";
        }
    }

    // Test 3: Explicit opt-in enables specific feature
    {
        browser::core::PrivacySettings settings;
        settings.telemetry_enabled = true;
        browser::core::PrivacyGuard guard(settings);

        if (!guard.is_allowed("telemetry")) {
            std::cerr << "FAIL: telemetry should be allowed when opted in\n";
            ++failures;
        } else if (guard.is_allowed("crash_reporting")) {
            std::cerr << "FAIL: crash_reporting should still be blocked\n";
            ++failures;
        } else {
            std::cerr << "PASS: explicit opt-in enables only specified feature\n";
        }
    }

    // Test 4: enabled_features lists only opted-in features
    {
        browser::core::PrivacySettings settings;
        settings.crash_reporting_enabled = true;
        settings.diagnostic_export_enabled = true;

        auto features = settings.enabled_features();
        if (features.size() != 2) {
            std::cerr << "FAIL: expected 2 enabled features, got " << features.size() << "\n";
            ++failures;
        } else if (features[0] != "crash_reporting" || features[1] != "diagnostic_export") {
            std::cerr << "FAIL: wrong feature names\n";
            ++failures;
        } else {
            std::cerr << "PASS: enabled_features lists correct features\n";
        }
    }

    // Test 5: Unknown feature is always blocked
    {
        browser::core::PrivacySettings settings;
        settings.telemetry_enabled = true;
        settings.crash_reporting_enabled = true;
        settings.usage_analytics_enabled = true;
        settings.diagnostic_export_enabled = true;
        browser::core::PrivacyGuard guard(settings);

        if (guard.is_allowed("unknown_feature")) {
            std::cerr << "FAIL: unknown feature should be blocked\n";
            ++failures;
        } else {
            std::cerr << "PASS: unknown feature blocked\n";
        }
    }

    // Test 6: Audit log records all checks
    {
        browser::core::PrivacyGuard guard;
        guard.is_allowed("telemetry");
        guard.is_allowed("crash_reporting");

        if (guard.audit_log().size() != 2) {
            std::cerr << "FAIL: expected 2 audit entries\n";
            ++failures;
        } else {
            const auto& e1 = guard.audit_log()[0];
            const auto& e2 = guard.audit_log()[1];
            if (e1.feature != "telemetry" || e1.was_allowed) {
                std::cerr << "FAIL: first audit entry wrong\n";
                ++failures;
            } else if (e2.feature != "crash_reporting" || e2.was_allowed) {
                std::cerr << "FAIL: second audit entry wrong\n";
                ++failures;
            } else {
                std::cerr << "PASS: audit log records all checks\n";
            }
        }
    }

    // Test 7: check() returns reason
    {
        browser::core::PrivacyGuard guard;
        auto entry = guard.check("telemetry");
        if (entry.reason.empty()) {
            std::cerr << "FAIL: check should provide a reason\n";
            ++failures;
        } else if (entry.was_allowed) {
            std::cerr << "FAIL: default should not allow\n";
            ++failures;
        } else {
            std::cerr << "PASS: check returns denial reason\n";
        }
    }

    // Test 8: update_settings changes behavior
    {
        browser::core::PrivacyGuard guard;
        if (guard.is_allowed("telemetry")) {
            std::cerr << "FAIL: should be blocked initially\n";
            ++failures;
        } else {
            browser::core::PrivacySettings updated;
            updated.telemetry_enabled = true;
            guard.update_settings(updated);
            if (!guard.is_allowed("telemetry")) {
                std::cerr << "FAIL: should be allowed after update\n";
                ++failures;
            } else {
                std::cerr << "PASS: update_settings changes behavior\n";
            }
        }
    }

    // Test 9: clear_audit_log empties the log
    {
        browser::core::PrivacyGuard guard;
        guard.is_allowed("telemetry");
        guard.clear_audit_log();
        if (!guard.audit_log().empty()) {
            std::cerr << "FAIL: audit log should be empty after clear\n";
            ++failures;
        } else {
            std::cerr << "PASS: clear_audit_log empties log\n";
        }
    }

    // Test 10: Deterministic — same settings produce same results
    {
        browser::core::PrivacySettings settings;
        settings.telemetry_enabled = true;

        browser::core::PrivacyGuard g1(settings), g2(settings);
        auto e1 = g1.check("telemetry");
        auto e2 = g2.check("telemetry");

        if (e1.was_allowed != e2.was_allowed || e1.reason != e2.reason) {
            std::cerr << "FAIL: privacy check not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: privacy check is deterministic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll privacy control tests PASSED\n";
    return 0;
}
