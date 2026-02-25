#include "browser/engine/engine.h"

#include "browser/browser.h"
#include "browser/core/config.h"

#include <chrono>
#include <string>

namespace browser::engine {

namespace {

core::LifecycleStage map_pipeline_stage(browser::app::PipelineStage stage) {
    switch (stage) {
        case browser::app::PipelineStage::Fetching:  return core::LifecycleStage::Fetching;
        case browser::app::PipelineStage::Parsing:   return core::LifecycleStage::Parsing;
        case browser::app::PipelineStage::Styling:   return core::LifecycleStage::Styling;
        case browser::app::PipelineStage::Layout:    return core::LifecycleStage::Layout;
        case browser::app::PipelineStage::Rendering: return core::LifecycleStage::Rendering;
    }
    return core::LifecycleStage::Error;
}

}  // namespace

BrowserEngine::BrowserEngine() = default;

void BrowserEngine::transition_to(core::LifecycleStage stage,
                                   const std::string& detail) {
    session_.stage = stage;
    session_.trace.record(stage);
    std::string message = std::string("Stage transition: ") +
                          core::lifecycle_stage_name(stage);
    if (!detail.empty()) {
        message += " (" + detail + ")";
    }
    emit_diagnostic(core::Severity::Info, "engine", message);
}

void BrowserEngine::emit_diagnostic(core::Severity severity,
                                     const std::string& module,
                                     const std::string& message) {
    core::DiagnosticEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.severity = severity;
    event.module = module;
    event.stage = core::lifecycle_stage_name(session_.stage);
    event.message = message;
    session_.diagnostics.push_back(std::move(event));
}

void BrowserEngine::cancel() {
    cancel_requested_.store(true, std::memory_order_release);
    transition_to(core::LifecycleStage::Cancelled);
    emit_diagnostic(core::Severity::Info, "engine",
                    "Cancel requested");
}

EngineResult BrowserEngine::navigate(const std::string& input,
                                      const RenderOptions& options) {
    cancel_requested_.store(false, std::memory_order_release);
    last_input_ = input;
    last_options_ = options;

    session_ = {};
    transition_to(core::LifecycleStage::Idle);

    // Normalize input
    std::string norm_err;
    if (!normalize_input(input, session_.navigation, norm_err)) {
        transition_to(core::LifecycleStage::Error, norm_err);
        EngineResult result;
        result.ok = false;
        result.message = norm_err;
        result.session = session_;
        return result;
    }

    emit_diagnostic(
        core::Severity::Info, "engine",
        "Navigation target: " + session_.navigation.canonical_url +
            " (type: " + input_type_name(session_.navigation.input_type) + ")");

    // Delegate to existing pipeline with stage tracking and cancellation
    browser::app::RunOptions run_opts;
    run_opts.width = options.viewport_width;
    run_opts.height = options.viewport_height;
    run_opts.output_path = options.output_path;
    run_opts.on_stage_enter = [this](browser::app::PipelineStage stage) {
        transition_to(map_pipeline_stage(stage));
    };
    run_opts.is_cancelled = [this]() {
        return cancel_requested_.load(std::memory_order_acquire);
    };

    const browser::app::RunResult run_result =
        browser::app::run(input, run_opts);

    if (cancel_requested_.load(std::memory_order_acquire)) {
        transition_to(core::LifecycleStage::Cancelled);
    } else if (run_result.ok) {
        transition_to(core::LifecycleStage::Complete);
    } else {
        transition_to(core::LifecycleStage::Error, run_result.message);
    }

    EngineResult result;
    result.ok = run_result.ok;
    result.message = run_result.message;
    result.session = session_;
    return result;
}

EngineResult BrowserEngine::retry() {
    if (last_input_.empty()) {
        EngineResult result;
        result.ok = false;
        result.message = "No previous navigation to retry";
        result.session = session_;
        return result;
    }

    // Preserve prior diagnostics and add retry marker
    std::vector<core::DiagnosticEvent> prior_diagnostics =
        std::move(session_.diagnostics);

    core::DiagnosticEvent retry_event;
    retry_event.timestamp = std::chrono::steady_clock::now();
    retry_event.severity = core::Severity::Info;
    retry_event.module = "engine";
    retry_event.stage = core::lifecycle_stage_name(session_.stage);
    retry_event.message = "Retry requested from stage: " +
                          std::string(core::lifecycle_stage_name(session_.stage));
    prior_diagnostics.push_back(std::move(retry_event));

    // Re-run navigate, which resets session_
    EngineResult result = navigate(last_input_, last_options_);

    // Prepend prior diagnostics so context is preserved
    prior_diagnostics.insert(prior_diagnostics.end(),
                             result.session.diagnostics.begin(),
                             result.session.diagnostics.end());
    result.session.diagnostics = std::move(prior_diagnostics);
    session_.diagnostics = result.session.diagnostics;

    return result;
}

const SessionInfo& BrowserEngine::session() const {
    return session_;
}

core::LifecycleStage BrowserEngine::current_stage() const {
    return session_.stage;
}

}  // namespace browser::engine
