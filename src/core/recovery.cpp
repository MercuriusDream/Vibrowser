#include "browser/core/recovery.h"

#include <sstream>

namespace browser::core {

const char* recovery_action_name(RecoveryAction action) {
    switch (action) {
        case RecoveryAction::Retry:  return "Retry";
        case RecoveryAction::Replay: return "Replay";
        case RecoveryAction::Cancel: return "Cancel";
        case RecoveryAction::Skip:   return "Skip";
    }
    return "Unknown";
}

bool RecoveryPlan::has_action(RecoveryAction action) const {
    for (const auto& step : steps) {
        if (step.action == action) return true;
    }
    return false;
}

std::string RecoveryPlan::format() const {
    std::ostringstream oss;
    oss << "Recovery Plan";
    if (correlation_id != 0) {
        oss << " (cid:" << correlation_id << ")";
    }
    oss << "\n";
    oss << "  failure: " << failure_module << "/" << failure_stage
        << " — " << failure_message << "\n";
    oss << "  actions:\n";
    for (std::size_t i = 0; i < steps.size(); ++i) {
        oss << "    " << (i + 1) << ". ["
            << recovery_action_name(steps[i].action) << "] "
            << steps[i].description << "\n";
    }
    return oss.str();
}

std::vector<RecoveryStep> RecoveryController::generate_steps(
    const std::string& module, const std::string& stage) const {

    std::vector<RecoveryStep> steps;

    // Stage-aware recovery logic
    if (module == "net") {
        steps.push_back({RecoveryAction::Retry, stage,
                         "Retry the network request"});
        steps.push_back({RecoveryAction::Skip, stage,
                         "Skip this resource and continue"});
    } else if (stage == "fetch" || stage == "connect") {
        steps.push_back({RecoveryAction::Retry, stage,
                         "Retry the failed connection"});
    } else if (stage == "parse" || stage == "style" || stage == "layout") {
        steps.push_back({RecoveryAction::Replay, stage,
                         "Replay from " + stage + " stage with current input"});
        steps.push_back({RecoveryAction::Skip, stage,
                         "Skip " + stage + " and proceed with partial result"});
    } else if (stage == "render" || stage == "paint") {
        steps.push_back({RecoveryAction::Replay, stage,
                         "Replay render with current layout"});
    } else {
        // Generic fallback
        steps.push_back({RecoveryAction::Retry, stage,
                         "Retry the failed operation"});
    }

    // Always offer cancel as last resort
    steps.push_back({RecoveryAction::Cancel, stage,
                     "Cancel and return to idle state"});

    return steps;
}

RecoveryPlan RecoveryController::plan_from_trace(const FailureTrace& trace) const {
    RecoveryPlan plan;
    plan.failure_module = trace.module;
    plan.failure_stage = trace.stage;
    plan.failure_message = trace.error_message;
    plan.correlation_id = trace.correlation_id;
    plan.steps = generate_steps(trace.module, trace.stage);
    history_.push_back(plan);
    return plan;
}

RecoveryPlan RecoveryController::plan_from_stage(
    const std::string& module, const std::string& stage,
    const std::string& error) const {

    RecoveryPlan plan;
    plan.failure_module = module;
    plan.failure_stage = stage;
    plan.failure_message = error;
    plan.steps = generate_steps(module, stage);
    history_.push_back(plan);
    return plan;
}

const std::vector<RecoveryPlan>& RecoveryController::history() const {
    return history_;
}

void RecoveryController::clear_history() {
    history_.clear();
}

std::size_t RecoveryController::history_size() const {
    return history_.size();
}

}  // namespace browser::core
