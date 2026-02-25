// Test: Emit structured diagnostics with severity, module, and stage context
// Story 6.1 acceptance test

#include "browser/core/diagnostics.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    int failures = 0;

    // Test 1: Severity names
    {
        bool ok = true;
        if (std::string(browser::core::severity_name(browser::core::Severity::Info)) != "info") ok = false;
        if (std::string(browser::core::severity_name(browser::core::Severity::Warning)) != "warning") ok = false;
        if (std::string(browser::core::severity_name(browser::core::Severity::Error)) != "error") ok = false;
        if (!ok) {
            std::cerr << "FAIL: severity_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: severity_name correct\n";
        }
    }

    // Test 2: DiagnosticEmitter emits events with all fields
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.set_correlation_id(42);
        emitter.emit(browser::core::Severity::Error, "net", "fetch", "connection refused");

        if (emitter.size() != 1) {
            std::cerr << "FAIL: expected 1 event\n";
            ++failures;
        } else {
            const auto& e = emitter.events()[0];
            bool ok = true;
            if (e.severity != browser::core::Severity::Error) ok = false;
            if (e.module != "net") ok = false;
            if (e.stage != "fetch") ok = false;
            if (e.message != "connection refused") ok = false;
            if (e.correlation_id != 42) ok = false;
            if (e.timestamp == std::chrono::steady_clock::time_point{}) ok = false;
            if (!ok) {
                std::cerr << "FAIL: event fields incorrect\n";
                ++failures;
            } else {
                std::cerr << "PASS: event includes all structured fields\n";
            }
        }
    }

    // Test 3: format_diagnostic produces readable output
    {
        browser::core::DiagnosticEvent event;
        event.severity = browser::core::Severity::Warning;
        event.module = "css";
        event.stage = "parse";
        event.message = "unknown property";
        event.correlation_id = 99;

        std::string fmt = browser::core::format_diagnostic(event);
        bool ok = true;
        if (fmt.find("[warning]") == std::string::npos) ok = false;
        if (fmt.find("css") == std::string::npos) ok = false;
        if (fmt.find("parse") == std::string::npos) ok = false;
        if (fmt.find("cid:99") == std::string::npos) ok = false;
        if (fmt.find("unknown property") == std::string::npos) ok = false;
        if (!ok) {
            std::cerr << "FAIL: format_diagnostic missing fields: " << fmt << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: format_diagnostic includes all fields\n";
        }
    }

    // Test 4: format_diagnostic without correlation_id
    {
        browser::core::DiagnosticEvent event;
        event.severity = browser::core::Severity::Info;
        event.module = "html";
        event.stage = "parse";
        event.message = "ok";
        event.correlation_id = 0;

        std::string fmt = browser::core::format_diagnostic(event);
        if (fmt.find("cid:") != std::string::npos) {
            std::cerr << "FAIL: should not show cid when 0\n";
            ++failures;
        } else {
            std::cerr << "PASS: format_diagnostic omits cid when 0\n";
        }
    }

    // Test 5: Observer is notified on emit
    {
        browser::core::DiagnosticEmitter emitter;
        std::vector<std::string> observed;
        emitter.add_observer([&](const browser::core::DiagnosticEvent& e) {
            observed.push_back(e.message);
        });
        emitter.emit(browser::core::Severity::Info, "test", "setup", "alpha");
        emitter.emit(browser::core::Severity::Warning, "test", "run", "beta");

        if (observed.size() != 2 || observed[0] != "alpha" || observed[1] != "beta") {
            std::cerr << "FAIL: observer not notified correctly\n";
            ++failures;
        } else {
            std::cerr << "PASS: observer notified on each emit\n";
        }
    }

    // Test 6: Severity filter — Info filtered when min is Warning
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.set_min_severity(browser::core::Severity::Warning);
        emitter.emit(browser::core::Severity::Info, "test", "x", "should be filtered");
        emitter.emit(browser::core::Severity::Warning, "test", "x", "should pass");
        emitter.emit(browser::core::Severity::Error, "test", "x", "should pass too");

        if (emitter.size() != 2) {
            std::cerr << "FAIL: expected 2 events after filter, got " << emitter.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: severity filter works\n";
        }
    }

    // Test 7: events_by_severity
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.emit(browser::core::Severity::Info, "a", "x", "i1");
        emitter.emit(browser::core::Severity::Error, "b", "y", "e1");
        emitter.emit(browser::core::Severity::Info, "c", "z", "i2");
        emitter.emit(browser::core::Severity::Error, "d", "w", "e2");

        auto errors = emitter.events_by_severity(browser::core::Severity::Error);
        auto infos = emitter.events_by_severity(browser::core::Severity::Info);

        if (errors.size() != 2 || infos.size() != 2) {
            std::cerr << "FAIL: events_by_severity wrong count\n";
            ++failures;
        } else if (errors[0].message != "e1" || errors[1].message != "e2") {
            std::cerr << "FAIL: events_by_severity wrong content\n";
            ++failures;
        } else {
            std::cerr << "PASS: events_by_severity filters correctly\n";
        }
    }

    // Test 8: events_by_module
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.emit(browser::core::Severity::Info, "net", "fetch", "n1");
        emitter.emit(browser::core::Severity::Info, "css", "parse", "c1");
        emitter.emit(browser::core::Severity::Error, "net", "connect", "n2");

        auto net_events = emitter.events_by_module("net");
        if (net_events.size() != 2) {
            std::cerr << "FAIL: events_by_module wrong count\n";
            ++failures;
        } else if (net_events[0].stage != "fetch" || net_events[1].stage != "connect") {
            std::cerr << "FAIL: events_by_module wrong content\n";
            ++failures;
        } else {
            std::cerr << "PASS: events_by_module filters correctly\n";
        }
    }

    // Test 9: Correlation ID propagates to all events
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.set_correlation_id(1000);
        emitter.emit(browser::core::Severity::Info, "a", "x", "msg1");
        emitter.set_correlation_id(2000);
        emitter.emit(browser::core::Severity::Info, "a", "x", "msg2");

        if (emitter.events()[0].correlation_id != 1000 ||
            emitter.events()[1].correlation_id != 2000) {
            std::cerr << "FAIL: correlation_id not propagated correctly\n";
            ++failures;
        } else {
            std::cerr << "PASS: correlation_id propagates to events\n";
        }
    }

    // Test 10: Clear empties all events
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.emit(browser::core::Severity::Info, "x", "y", "z");
        emitter.emit(browser::core::Severity::Error, "x", "y", "z");
        emitter.clear();

        if (emitter.size() != 0 || !emitter.events().empty()) {
            std::cerr << "FAIL: clear did not empty events\n";
            ++failures;
        } else {
            std::cerr << "PASS: clear empties all events\n";
        }
    }

    // Test 11: Deterministic — same emits produce same results
    {
        browser::core::DiagnosticEmitter e1, e2;
        e1.set_correlation_id(7);
        e2.set_correlation_id(7);
        e1.emit(browser::core::Severity::Warning, "m", "s", "msg");
        e2.emit(browser::core::Severity::Warning, "m", "s", "msg");

        bool match = (e1.size() == e2.size() &&
                      e1.events()[0].severity == e2.events()[0].severity &&
                      e1.events()[0].module == e2.events()[0].module &&
                      e1.events()[0].stage == e2.events()[0].stage &&
                      e1.events()[0].message == e2.events()[0].message &&
                      e1.events()[0].correlation_id == e2.events()[0].correlation_id);
        if (!match) {
            std::cerr << "FAIL: diagnostic emit not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: diagnostic emit is deterministic\n";
        }
    }

    // Test 12: Timestamps are monotonically non-decreasing
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.emit(browser::core::Severity::Info, "a", "1", "first");
        emitter.emit(browser::core::Severity::Info, "a", "2", "second");
        emitter.emit(browser::core::Severity::Info, "a", "3", "third");

        bool monotonic = true;
        for (std::size_t i = 1; i < emitter.events().size(); ++i) {
            if (emitter.events()[i].timestamp < emitter.events()[i - 1].timestamp) {
                monotonic = false;
                break;
            }
        }
        if (!monotonic) {
            std::cerr << "FAIL: timestamps not monotonic\n";
            ++failures;
        } else {
            std::cerr << "PASS: timestamps are monotonic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll structured diagnostics tests PASSED\n";
    return 0;
}
