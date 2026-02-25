#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace browser::core {

enum class LifecycleStage {
    Idle,
    Fetching,
    Parsing,
    Styling,
    Layout,
    Rendering,
    Complete,
    Error,
    Cancelled,
};

const char* lifecycle_stage_name(LifecycleStage stage);

struct StageTimingEntry {
    LifecycleStage stage;
    std::chrono::steady_clock::time_point entered_at;
    double elapsed_since_prev_ms = 0.0;
};

struct LifecycleTrace {
    std::vector<StageTimingEntry> entries;

    void record(LifecycleStage stage);

    bool is_reproducible_with(const LifecycleTrace& other,
                              double tolerance_factor = 3.0) const;
};

}  // namespace browser::core
