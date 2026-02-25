#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace browser::core {

enum class GateStatus {
    Pending,
    Passed,
    Failed,
};

const char* gate_status_name(GateStatus status);

struct GateEvidence {
    std::string gate_name;
    GateStatus status = GateStatus::Pending;
    std::string detail;
    std::chrono::steady_clock::time_point evaluated_at;
};

using GateCheck = std::function<bool(std::string& detail)>;

struct MilestoneGate {
    std::string name;
    GateCheck check;
};

struct MilestoneSummary {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t pending = 0;
    bool all_passed() const;
};

class MilestoneTracker {
public:
    void add_gate(const std::string& name, GateCheck check);
    void evaluate_all();
    void evaluate_gate(const std::string& name);

    const std::vector<GateEvidence>& evidence() const;
    MilestoneSummary summary() const;
    std::string format_report() const;

    void clear();
    std::size_t gate_count() const;

private:
    std::vector<MilestoneGate> gates_;
    std::vector<GateEvidence> evidence_;
};

}  // namespace browser::core
