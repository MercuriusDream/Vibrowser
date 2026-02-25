#include "browser/core/milestone.h"

#include <sstream>

namespace browser::core {

const char* gate_status_name(GateStatus status) {
    switch (status) {
        case GateStatus::Pending: return "Pending";
        case GateStatus::Passed:  return "Passed";
        case GateStatus::Failed:  return "Failed";
    }
    return "Unknown";
}

bool MilestoneSummary::all_passed() const {
    return failed == 0 && pending == 0 && passed == total && total > 0;
}

void MilestoneTracker::add_gate(const std::string& name, GateCheck check) {
    gates_.push_back({name, std::move(check)});
}

void MilestoneTracker::evaluate_all() {
    evidence_.clear();
    for (const auto& gate : gates_) {
        GateEvidence ev;
        ev.gate_name = gate.name;
        ev.evaluated_at = std::chrono::steady_clock::now();
        std::string detail;
        if (gate.check(detail)) {
            ev.status = GateStatus::Passed;
        } else {
            ev.status = GateStatus::Failed;
        }
        ev.detail = detail;
        evidence_.push_back(std::move(ev));
    }
}

void MilestoneTracker::evaluate_gate(const std::string& name) {
    for (const auto& gate : gates_) {
        if (gate.name != name) continue;

        GateEvidence ev;
        ev.gate_name = gate.name;
        ev.evaluated_at = std::chrono::steady_clock::now();
        std::string detail;
        if (gate.check(detail)) {
            ev.status = GateStatus::Passed;
        } else {
            ev.status = GateStatus::Failed;
        }
        ev.detail = detail;

        // Replace existing evidence for this gate, or append
        bool replaced = false;
        for (auto& existing : evidence_) {
            if (existing.gate_name == name) {
                existing = ev;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            evidence_.push_back(std::move(ev));
        }
        return;
    }
}

const std::vector<GateEvidence>& MilestoneTracker::evidence() const {
    return evidence_;
}

MilestoneSummary MilestoneTracker::summary() const {
    MilestoneSummary s;
    s.total = gates_.size();
    for (const auto& ev : evidence_) {
        if (ev.status == GateStatus::Passed) ++s.passed;
        else if (ev.status == GateStatus::Failed) ++s.failed;
        else ++s.pending;
    }
    // Gates without evidence are pending
    if (evidence_.size() < gates_.size()) {
        s.pending += (gates_.size() - evidence_.size());
    }
    return s;
}

std::string MilestoneTracker::format_report() const {
    auto s = summary();
    std::ostringstream oss;
    oss << "Milestone Report: " << s.passed << "/" << s.total << " passed";
    if (s.failed > 0) oss << ", " << s.failed << " failed";
    if (s.pending > 0) oss << ", " << s.pending << " pending";
    oss << "\n";
    for (const auto& ev : evidence_) {
        oss << "  [" << gate_status_name(ev.status) << "] " << ev.gate_name;
        if (!ev.detail.empty()) {
            oss << " — " << ev.detail;
        }
        oss << "\n";
    }
    return oss.str();
}

void MilestoneTracker::clear() {
    gates_.clear();
    evidence_.clear();
}

std::size_t MilestoneTracker::gate_count() const {
    return gates_.size();
}

}  // namespace browser::core
