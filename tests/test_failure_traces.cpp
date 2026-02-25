// Test: Collect reproducible failure traces and correlation identifiers
// Story 6.4 acceptance test

#include "browser/core/diagnostics.h"

#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // Test 1: FailureTrace basic fields
    {
        browser::core::FailureTrace trace;
        trace.correlation_id = 100;
        trace.module = "net";
        trace.stage = "fetch";
        trace.error_message = "timeout";

        bool ok = true;
        if (trace.correlation_id != 100) ok = false;
        if (trace.module != "net") ok = false;
        if (trace.stage != "fetch") ok = false;
        if (trace.error_message != "timeout") ok = false;
        if (!ok) {
            std::cerr << "FAIL: FailureTrace basic fields\n";
            ++failures;
        } else {
            std::cerr << "PASS: FailureTrace basic fields\n";
        }
    }

    // Test 2: add_snapshot stores key-value pairs
    {
        browser::core::FailureTrace trace;
        trace.add_snapshot("url", "http://example.com");
        trace.add_snapshot("status", "0");

        if (trace.snapshots.size() != 2) {
            std::cerr << "FAIL: expected 2 snapshots\n";
            ++failures;
        } else if (trace.snapshots[0].key != "url" || trace.snapshots[0].value != "http://example.com") {
            std::cerr << "FAIL: snapshot values wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: add_snapshot stores key-value pairs\n";
        }
    }

    // Test 3: format() produces readable output
    {
        browser::core::FailureTrace trace;
        trace.correlation_id = 42;
        trace.module = "html";
        trace.stage = "parse";
        trace.error_message = "unexpected EOF";
        trace.add_snapshot("input_size", "1024");

        std::string fmt = trace.format();
        bool ok = true;
        if (fmt.find("cid:42") == std::string::npos) ok = false;
        if (fmt.find("html") == std::string::npos) ok = false;
        if (fmt.find("parse") == std::string::npos) ok = false;
        if (fmt.find("unexpected EOF") == std::string::npos) ok = false;
        if (fmt.find("input_size=1024") == std::string::npos) ok = false;
        if (!ok) {
            std::cerr << "FAIL: format() missing fields: " << fmt << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: format() produces readable output\n";
        }
    }

    // Test 4: is_reproducible_with — identical traces
    {
        browser::core::FailureTrace t1, t2;
        t1.correlation_id = 1;
        t1.module = "net";
        t1.stage = "connect";
        t1.error_message = "refused";
        t1.add_snapshot("host", "example.com");

        t2 = t1;

        if (!t1.is_reproducible_with(t2)) {
            std::cerr << "FAIL: identical traces should be reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: identical traces are reproducible\n";
        }
    }

    // Test 5: is_reproducible_with — different error messages
    {
        browser::core::FailureTrace t1, t2;
        t1.module = "net";
        t1.stage = "fetch";
        t1.error_message = "timeout";

        t2.module = "net";
        t2.stage = "fetch";
        t2.error_message = "connection refused";

        if (t1.is_reproducible_with(t2)) {
            std::cerr << "FAIL: different errors should not be reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: different errors not reproducible\n";
        }
    }

    // Test 6: is_reproducible_with — different snapshots
    {
        browser::core::FailureTrace t1, t2;
        t1.module = "css";
        t1.stage = "parse";
        t1.error_message = "syntax error";
        t1.add_snapshot("line", "10");

        t2 = t1;
        t2.snapshots[0].value = "20";

        if (t1.is_reproducible_with(t2)) {
            std::cerr << "FAIL: different snapshots should not be reproducible\n";
            ++failures;
        } else {
            std::cerr << "PASS: different snapshots not reproducible\n";
        }
    }

    // Test 7: FailureTraceCollector captures from emitter
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.set_correlation_id(500);
        emitter.emit(browser::core::Severity::Info, "net", "fetch", "starting");
        emitter.emit(browser::core::Severity::Error, "net", "fetch", "failed");

        browser::core::FailureTraceCollector collector;
        auto trace = collector.capture(emitter, "net", "fetch", "connection failed");

        if (trace.correlation_id != 500) {
            std::cerr << "FAIL: correlation_id should come from emitter\n";
            ++failures;
        } else if (trace.context_events.size() != 2) {
            std::cerr << "FAIL: expected 2 context events\n";
            ++failures;
        } else if (trace.module != "net" || trace.stage != "fetch") {
            std::cerr << "FAIL: module/stage wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: collector captures from emitter\n";
        }
    }

    // Test 8: Collector stores multiple traces
    {
        browser::core::DiagnosticEmitter emitter;
        browser::core::FailureTraceCollector collector;

        collector.capture(emitter, "html", "parse", "error1");
        collector.capture(emitter, "css", "cascade", "error2");

        if (collector.size() != 2) {
            std::cerr << "FAIL: expected 2 traces\n";
            ++failures;
        } else if (collector.traces()[0].module != "html" ||
                   collector.traces()[1].module != "css") {
            std::cerr << "FAIL: trace modules wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: collector stores multiple traces\n";
        }
    }

    // Test 9: Collector clear
    {
        browser::core::DiagnosticEmitter emitter;
        browser::core::FailureTraceCollector collector;
        collector.capture(emitter, "x", "y", "z");
        collector.clear();

        if (collector.size() != 0) {
            std::cerr << "FAIL: clear should empty traces\n";
            ++failures;
        } else {
            std::cerr << "PASS: collector clear works\n";
        }
    }

    // Test 10: Reproducibility — same input produces reproducible trace
    {
        auto make_trace = [](std::uint64_t cid) {
            browser::core::DiagnosticEmitter emitter;
            emitter.set_correlation_id(cid);
            emitter.emit(browser::core::Severity::Info, "layout", "compute", "start");
            emitter.emit(browser::core::Severity::Error, "layout", "compute", "overflow");

            browser::core::FailureTraceCollector collector;
            auto trace = collector.capture(emitter, "layout", "compute", "overflow");
            trace.add_snapshot("viewport", "800x600");
            return trace;
        };

        auto t1 = make_trace(777);
        auto t2 = make_trace(777);

        if (!t1.is_reproducible_with(t2)) {
            std::cerr << "FAIL: same input should produce reproducible traces\n";
            ++failures;
        } else {
            std::cerr << "PASS: same input produces reproducible traces\n";
        }
    }

    // Test 11: Context events include correlation ID
    {
        browser::core::DiagnosticEmitter emitter;
        emitter.set_correlation_id(999);
        emitter.emit(browser::core::Severity::Warning, "render", "paint", "slow");

        browser::core::FailureTraceCollector collector;
        auto trace = collector.capture(emitter, "render", "paint", "failed");

        if (trace.context_events.empty()) {
            std::cerr << "FAIL: expected context events\n";
            ++failures;
        } else if (trace.context_events[0].correlation_id != 999) {
            std::cerr << "FAIL: context event should have correlation_id\n";
            ++failures;
        } else {
            std::cerr << "PASS: context events include correlation ID\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll failure trace tests PASSED\n";
    return 0;
}
