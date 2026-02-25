#include "browser/core/lifecycle.h"

#include <cmath>

namespace browser::core {

const char* lifecycle_stage_name(LifecycleStage stage) {
    switch (stage) {
        case LifecycleStage::Idle:       return "idle";
        case LifecycleStage::Fetching:   return "fetching";
        case LifecycleStage::Parsing:    return "parsing";
        case LifecycleStage::Styling:    return "styling";
        case LifecycleStage::Layout:     return "layout";
        case LifecycleStage::Rendering:  return "rendering";
        case LifecycleStage::Complete:   return "complete";
        case LifecycleStage::Error:      return "error";
        case LifecycleStage::Cancelled:  return "cancelled";
    }
    return "unknown";
}

void LifecycleTrace::record(LifecycleStage stage) {
    StageTimingEntry entry;
    entry.stage = stage;
    entry.entered_at = std::chrono::steady_clock::now();
    entry.elapsed_since_prev_ms = 0.0;

    if (!entries.empty()) {
        const auto delta = entry.entered_at - entries.back().entered_at;
        entry.elapsed_since_prev_ms =
            std::chrono::duration<double, std::milli>(delta).count();
    }

    entries.push_back(entry);
}

bool LifecycleTrace::is_reproducible_with(const LifecycleTrace& other,
                                           double tolerance_factor) const {
    if (entries.size() != other.entries.size()) {
        return false;
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        // Stage order must match exactly
        if (entries[i].stage != other.entries[i].stage) {
            return false;
        }

        // Timing check: each stage's duration should be within tolerance
        // For very fast stages (< 1ms), allow a fixed tolerance of 50ms
        if (i > 0) {
            const double a = entries[i].elapsed_since_prev_ms;
            const double b = other.entries[i].elapsed_since_prev_ms;
            const double max_val = std::max(a, b);
            constexpr double kMinToleranceMs = 50.0;

            if (max_val > kMinToleranceMs) {
                const double ratio = (max_val > 0.0) ? std::min(a, b) / max_val : 1.0;
                const double threshold = 1.0 / tolerance_factor;
                if (ratio < threshold) {
                    return false;
                }
            }
        }
    }

    return true;
}

}  // namespace browser::core
