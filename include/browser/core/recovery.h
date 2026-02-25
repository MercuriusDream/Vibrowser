#pragma once

#include "browser/core/diagnostics.h"

#include <string>
#include <vector>

namespace browser::core {

enum class RecoveryAction {
    Retry,
    Replay,
    Cancel,
    Skip,
};

const char* recovery_action_name(RecoveryAction action);

struct RecoveryStep {
    RecoveryAction action;
    std::string stage;
    std::string description;
};

struct RecoveryPlan {
    std::string failure_module;
    std::string failure_stage;
    std::string failure_message;
    std::uint64_t correlation_id = 0;
    std::vector<RecoveryStep> steps;

    bool has_action(RecoveryAction action) const;
    std::string format() const;
};

class RecoveryController {
public:
    RecoveryPlan plan_from_trace(const FailureTrace& trace) const;
    RecoveryPlan plan_from_stage(const std::string& module,
                                 const std::string& stage,
                                 const std::string& error) const;

    const std::vector<RecoveryPlan>& history() const;
    void clear_history();
    std::size_t history_size() const;

private:
    mutable std::vector<RecoveryPlan> history_;
    std::vector<RecoveryStep> generate_steps(const std::string& module,
                                              const std::string& stage) const;
};

}  // namespace browser::core
