// Test: Validate module contracts independently with maintainability tooling
// Story 6.5 acceptance test

#include "browser/core/contract.h"
#include "browser/core/diagnostics.h"
#include "browser/core/lifecycle.h"
#include "browser/core/milestone.h"
#include "browser/core/privacy.h"
#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"
#include "browser/layout/layout_engine.h"
#include "browser/net/http_client.h"
#include "browser/render/canvas.h"
#include "browser/render/renderer.h"

#include <iostream>
#include <memory>
#include <string>

int main() {
    int failures = 0;

    // =========================================================
    // Part 1: ContractValidator framework tests
    // =========================================================

    // Test 1: Empty validator
    {
        browser::core::ContractValidator v;
        v.validate_all();
        if (v.all_passed()) {
            std::cerr << "FAIL: empty validator should not be all_passed\n";
            ++failures;
        } else {
            std::cerr << "PASS: empty validator not all_passed\n";
        }
    }

    // Test 2: All checks pass
    {
        browser::core::ContractValidator v;
        v.add_check("core", "severity_name", "returns valid names",
            [](std::string& d) { d = "ok"; return true; });
        v.add_check("html", "parse_html", "parses without crash",
            [](std::string& d) { d = "ok"; return true; });
        v.validate_all();

        if (!v.all_passed() || v.pass_count() != 2 || v.fail_count() != 0) {
            std::cerr << "FAIL: expected all passed\n";
            ++failures;
        } else {
            std::cerr << "PASS: all checks pass\n";
        }
    }

    // Test 3: Mixed pass/fail
    {
        browser::core::ContractValidator v;
        v.add_check("a", "x", "d", [](std::string& d) { d = "ok"; return true; });
        v.add_check("b", "y", "d", [](std::string& d) { d = "bad"; return false; });
        v.validate_all();

        if (v.all_passed() || v.pass_count() != 1 || v.fail_count() != 1) {
            std::cerr << "FAIL: expected mixed results\n";
            ++failures;
        } else {
            std::cerr << "PASS: mixed pass/fail tracked\n";
        }
    }

    // Test 4: validate_module only runs specified module
    {
        browser::core::ContractValidator v;
        int a_count = 0, b_count = 0;
        v.add_check("a", "fn", "d", [&](std::string& d) { ++a_count; d = ""; return true; });
        v.add_check("b", "fn", "d", [&](std::string& d) { ++b_count; d = ""; return true; });
        v.validate_module("a");

        if (a_count != 1 || b_count != 0) {
            std::cerr << "FAIL: validate_module should only run specified module\n";
            ++failures;
        } else if (v.results_for_module("a").size() != 1) {
            std::cerr << "FAIL: results_for_module wrong\n";
            ++failures;
        } else {
            std::cerr << "PASS: validate_module isolates module\n";
        }
    }

    // Test 5: format_report includes module and interface
    {
        browser::core::ContractValidator v;
        v.add_check("net", "fetch", "basic fetch", [](std::string& d) { d = "200"; return true; });
        v.validate_all();
        std::string report = v.format_report();
        if (report.find("net::fetch") == std::string::npos || report.find("PASS") == std::string::npos) {
            std::cerr << "FAIL: report missing content\n";
            ++failures;
        } else {
            std::cerr << "PASS: format_report includes module::interface\n";
        }
    }

    // =========================================================
    // Part 2: Real module contract validations
    // =========================================================

    browser::core::ContractValidator validator;

    // Core module contracts
    validator.add_check("core", "severity_name", "enum names", [](std::string& d) {
        d = browser::core::severity_name(browser::core::Severity::Error);
        return d == "error";
    });

    validator.add_check("core", "lifecycle_stage_name", "enum names", [](std::string& d) {
        const char* name = browser::core::lifecycle_stage_name(browser::core::LifecycleStage::Idle);
        d = name ? name : "(null)";
        return name != nullptr && d == "idle";
    });

    validator.add_check("core", "DiagnosticEmitter", "emit and retrieve", [](std::string& d) {
        browser::core::DiagnosticEmitter emitter;
        emitter.emit(browser::core::Severity::Info, "test", "check", "ok");
        d = "size=" + std::to_string(emitter.size());
        return emitter.size() == 1;
    });

    validator.add_check("core", "MilestoneTracker", "add and evaluate", [](std::string& d) {
        browser::core::MilestoneTracker tracker;
        tracker.add_gate("g", [](std::string& dd) { dd = "ok"; return true; });
        tracker.evaluate_all();
        d = "passed=" + std::to_string(tracker.summary().passed);
        return tracker.summary().all_passed();
    });

    validator.add_check("core", "PrivacySettings", "defaults disabled", [](std::string& d) {
        browser::core::PrivacySettings settings;
        d = settings.all_disabled() ? "all disabled" : "not all disabled";
        return settings.all_disabled();
    });

    // HTML module contracts
    validator.add_check("html", "parse_html", "basic parse", [](std::string& d) {
        auto dom = browser::html::parse_html("<html><body><p>test</p></body></html>");
        d = dom ? "parsed" : "null";
        return dom != nullptr;
    });

    validator.add_check("html", "Node", "tree structure", [](std::string& d) {
        auto dom = browser::html::parse_html("<html><body></body></html>");
        d = "children=" + std::to_string(dom->children.size());
        return !dom->children.empty();
    });

    // CSS module contracts
    validator.add_check("css", "parse_css", "basic stylesheet", [](std::string& d) {
        auto ss = browser::css::parse_css("h1 { color: red; }");
        d = "rules=" + std::to_string(ss.rules.size());
        return ss.rules.size() == 1;
    });

    validator.add_check("css", "extract_linked_css", "finds link refs", [](std::string& d) {
        auto dom = browser::html::parse_html(
            "<html><head><link rel=\"stylesheet\" href=\"s.css\"/></head><body></body></html>");
        auto refs = browser::css::extract_linked_css(*dom);
        d = "refs=" + std::to_string(refs.size());
        return refs.size() == 1;
    });

    // Layout module contracts
    validator.add_check("layout", "layout_document", "produces layout tree", [](std::string& d) {
        auto dom = browser::html::parse_html("<html><body><p>text</p></body></html>");
        auto ss = browser::css::parse_css("p { display: block; }");
        auto layout = browser::layout::layout_document(*dom, ss, 800);
        d = "width=" + std::to_string(layout.width);
        return layout.width > 0;
    });

    // Render module contracts
    validator.add_check("render", "render_to_canvas", "produces canvas", [](std::string& d) {
        auto dom = browser::html::parse_html("<html><body>hi</body></html>");
        auto ss = browser::css::parse_css("");
        auto layout = browser::layout::layout_document(*dom, ss, 100);
        auto canvas = browser::render::render_to_canvas(layout, 100, 50);
        d = std::to_string(canvas.width()) + "x" + std::to_string(canvas.height());
        return canvas.width() == 100 && canvas.height() == 50;
    });

    // Net module contracts
    validator.add_check("net", "check_request_policy", "validates URLs", [](std::string& d) {
        browser::net::RequestPolicy policy;
        auto r1 = browser::net::check_request_policy("http://example.com", policy);
        auto r2 = browser::net::check_request_policy("", policy);
        d = r1.allowed ? "allowed" : "blocked";
        return r1.allowed && !r2.allowed;
    });

    validator.add_check("net", "ResponseCache", "store and lookup", [](std::string& d) {
        browser::net::ResponseCache cache(browser::net::CachePolicy::CacheAll);
        browser::net::Response resp;
        resp.status_code = 200;
        resp.body = "test";
        cache.store("http://test.com/", resp);
        browser::net::Response out;
        bool found = cache.lookup("http://test.com/", out);
        d = found ? "found" : "not found";
        return found && out.body == "test";
    });

    // JS/Runtime module contracts
    validator.add_check("js", "query_by_id", "finds element by id", [](std::string& d) {
        auto dom = browser::html::parse_html(
            "<html><body><div id=\"main\">hello</div></body></html>");
        auto result = browser::js::query_by_id(*dom, "main");
        bool found = result.ok && !result.elements.empty() && result.elements[0].found;
        d = found ? "found" : "not found";
        return found && result.elements[0].tag_name == "div";
    });

    validator.add_check("js", "EventRegistry", "add and count", [](std::string& d) {
        browser::js::EventRegistry reg;
        reg.add_listener("btn", browser::js::EventType::Click,
            [](browser::html::Node&, const browser::js::DomEvent&) -> browser::js::MutationResult {
                return {true, ""};
            });
        d = "listeners=" + std::to_string(reg.listener_count());
        return reg.listener_count() == 1;
    });

    // Now validate all
    validator.validate_all();

    // Test 6: All real module contracts pass
    {
        if (!validator.all_passed()) {
            std::cerr << "FAIL: not all module contracts passed\n";
            std::cerr << validator.format_report();
            ++failures;
        } else {
            std::cerr << "PASS: all " << validator.pass_count()
                      << " module contracts validated\n";
        }
    }

    // Test 7: Each module has at least one contract
    {
        std::vector<std::string> modules = {"core", "html", "css", "layout", "render", "net", "js"};
        bool ok = true;
        for (const auto& m : modules) {
            auto results = validator.results_for_module(m);
            if (results.empty()) {
                std::cerr << "FAIL: module '" << m << "' has no contracts\n";
                ok = false;
                break;
            }
        }
        if (ok) {
            std::cerr << "PASS: all major modules have contracts\n";
        } else {
            ++failures;
        }
    }

    // Test 8: Deterministic — same contracts produce same results
    {
        browser::core::ContractValidator v1, v2;
        auto check = [](std::string& d) { d = "ok"; return true; };
        v1.add_check("m", "f", "d", check);
        v2.add_check("m", "f", "d", check);
        v1.validate_all();
        v2.validate_all();

        if (v1.pass_count() != v2.pass_count() || v1.fail_count() != v2.fail_count()) {
            std::cerr << "FAIL: contract validation not deterministic\n";
            ++failures;
        } else {
            std::cerr << "PASS: contract validation is deterministic\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll module contract tests PASSED\n";
    return 0;
}
