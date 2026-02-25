#pragma once

#include "browser/core/diagnostics.h"
#include "browser/core/lifecycle.h"
#include "browser/engine/navigation.h"

#include <atomic>
#include <string>
#include <vector>

namespace browser::engine {

struct SessionInfo {
    NavigationInput navigation;
    core::LifecycleStage stage = core::LifecycleStage::Idle;
    std::vector<core::DiagnosticEvent> diagnostics;
    core::LifecycleTrace trace;
};

enum class OutputMode {
    Headless,  // PPM image file
    Shell,     // Text to stdout
};

struct RenderOptions {
    int viewport_width = 1280;
    int viewport_height = 720;
    std::string output_path = "output.ppm";
    OutputMode output_mode = OutputMode::Headless;
};

struct EngineResult {
    bool ok = false;
    std::string message;
    SessionInfo session;
};

class BrowserEngine {
public:
    BrowserEngine();

    EngineResult navigate(const std::string& input,
                          const RenderOptions& options);

    EngineResult retry();

    void cancel();

    const SessionInfo& session() const;
    core::LifecycleStage current_stage() const;

private:
    SessionInfo session_;
    std::string last_input_;
    RenderOptions last_options_;
    std::atomic<bool> cancel_requested_{false};

    void transition_to(core::LifecycleStage stage,
                       const std::string& detail = {});
    void emit_diagnostic(core::Severity severity,
                         const std::string& module,
                         const std::string& message);
};

}  // namespace browser::engine
