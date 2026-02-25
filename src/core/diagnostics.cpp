#include "browser/core/diagnostics.h"

#include <sstream>

namespace browser::core {

const char* severity_name(Severity severity) {
    switch (severity) {
        case Severity::Info:    return "info";
        case Severity::Warning: return "warning";
        case Severity::Error:   return "error";
    }
    return "unknown";
}

std::string format_diagnostic(const DiagnosticEvent& event) {
    std::ostringstream oss;
    oss << "[" << severity_name(event.severity) << "]";
    if (!event.module.empty()) {
        oss << " " << event.module;
    }
    if (!event.stage.empty()) {
        oss << "/" << event.stage;
    }
    if (event.correlation_id != 0) {
        oss << " (cid:" << event.correlation_id << ")";
    }
    oss << ": " << event.message;
    return oss.str();
}

void DiagnosticEmitter::emit(Severity severity, const std::string& module,
                              const std::string& stage, const std::string& message) {
    if (severity < min_severity_) {
        return;
    }

    DiagnosticEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.severity = severity;
    event.module = module;
    event.stage = stage;
    event.message = message;
    event.correlation_id = correlation_id_;

    events_.push_back(event);

    for (const auto& observer : observers_) {
        observer(event);
    }
}

void DiagnosticEmitter::set_correlation_id(std::uint64_t id) {
    correlation_id_ = id;
}

std::uint64_t DiagnosticEmitter::correlation_id() const {
    return correlation_id_;
}

void DiagnosticEmitter::set_min_severity(Severity min) {
    min_severity_ = min;
}

Severity DiagnosticEmitter::min_severity() const {
    return min_severity_;
}

void DiagnosticEmitter::add_observer(DiagnosticObserver observer) {
    observers_.push_back(std::move(observer));
}

const std::vector<DiagnosticEvent>& DiagnosticEmitter::events() const {
    return events_;
}

std::vector<DiagnosticEvent> DiagnosticEmitter::events_by_severity(Severity severity) const {
    std::vector<DiagnosticEvent> result;
    for (const auto& e : events_) {
        if (e.severity == severity) {
            result.push_back(e);
        }
    }
    return result;
}

std::vector<DiagnosticEvent> DiagnosticEmitter::events_by_module(const std::string& module) const {
    std::vector<DiagnosticEvent> result;
    for (const auto& e : events_) {
        if (e.module == module) {
            result.push_back(e);
        }
    }
    return result;
}

void DiagnosticEmitter::clear() {
    events_.clear();
}

std::size_t DiagnosticEmitter::size() const {
    return events_.size();
}

void FailureTrace::add_snapshot(const std::string& key, const std::string& value) {
    snapshots.push_back({key, value});
}

std::string FailureTrace::format() const {
    std::ostringstream oss;
    oss << "FailureTrace";
    if (correlation_id != 0) {
        oss << " (cid:" << correlation_id << ")";
    }
    oss << "\n";
    oss << "  module: " << module << "\n";
    oss << "  stage: " << stage << "\n";
    oss << "  error: " << error_message << "\n";
    if (!snapshots.empty()) {
        oss << "  snapshots:\n";
        for (const auto& s : snapshots) {
            oss << "    " << s.key << "=" << s.value << "\n";
        }
    }
    if (!context_events.empty()) {
        oss << "  context_events: " << context_events.size() << "\n";
    }
    return oss.str();
}

bool FailureTrace::is_reproducible_with(const FailureTrace& other) const {
    if (module != other.module) return false;
    if (stage != other.stage) return false;
    if (error_message != other.error_message) return false;
    if (correlation_id != other.correlation_id) return false;
    if (snapshots.size() != other.snapshots.size()) return false;
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        if (snapshots[i].key != other.snapshots[i].key) return false;
        if (snapshots[i].value != other.snapshots[i].value) return false;
    }
    if (context_events.size() != other.context_events.size()) return false;
    for (std::size_t i = 0; i < context_events.size(); ++i) {
        if (context_events[i].severity != other.context_events[i].severity) return false;
        if (context_events[i].module != other.context_events[i].module) return false;
        if (context_events[i].stage != other.context_events[i].stage) return false;
        if (context_events[i].message != other.context_events[i].message) return false;
    }
    return true;
}

FailureTrace FailureTraceCollector::capture(const DiagnosticEmitter& emitter,
                                             const std::string& module,
                                             const std::string& stage,
                                             const std::string& error_message) {
    FailureTrace trace;
    trace.correlation_id = emitter.correlation_id();
    trace.module = module;
    trace.stage = stage;
    trace.error_message = error_message;
    trace.context_events = emitter.events();
    traces_.push_back(trace);
    return trace;
}

const std::vector<FailureTrace>& FailureTraceCollector::traces() const {
    return traces_;
}

void FailureTraceCollector::clear() {
    traces_.clear();
}

std::size_t FailureTraceCollector::size() const {
    return traces_.size();
}

}  // namespace browser::core
