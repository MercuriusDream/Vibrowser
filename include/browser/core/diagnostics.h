#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace browser::core {

enum class Severity {
    Info,
    Warning,
    Error,
};

struct DiagnosticEvent {
    std::chrono::steady_clock::time_point timestamp;
    Severity severity = Severity::Info;
    std::string module;
    std::string stage;
    std::string message;
    std::uint64_t correlation_id = 0;
};

const char* severity_name(Severity severity);

std::string format_diagnostic(const DiagnosticEvent& event);

using DiagnosticObserver = std::function<void(const DiagnosticEvent&)>;

class DiagnosticEmitter {
public:
    void emit(Severity severity, const std::string& module,
              const std::string& stage, const std::string& message);

    void set_correlation_id(std::uint64_t id);
    std::uint64_t correlation_id() const;

    void set_min_severity(Severity min);
    Severity min_severity() const;

    void add_observer(DiagnosticObserver observer);

    const std::vector<DiagnosticEvent>& events() const;
    std::vector<DiagnosticEvent> events_by_severity(Severity severity) const;
    std::vector<DiagnosticEvent> events_by_module(const std::string& module) const;

    void clear();
    std::size_t size() const;

private:
    std::vector<DiagnosticEvent> events_;
    std::vector<DiagnosticObserver> observers_;
    std::uint64_t correlation_id_ = 0;
    Severity min_severity_ = Severity::Info;
};

struct FailureSnapshot {
    std::string key;
    std::string value;
};

struct FailureTrace {
    std::uint64_t correlation_id = 0;
    std::string module;
    std::string stage;
    std::string error_message;
    std::vector<DiagnosticEvent> context_events;
    std::vector<FailureSnapshot> snapshots;

    void add_snapshot(const std::string& key, const std::string& value);
    std::string format() const;
    bool is_reproducible_with(const FailureTrace& other) const;
};

class FailureTraceCollector {
public:
    FailureTrace capture(const DiagnosticEmitter& emitter,
                         const std::string& module,
                         const std::string& stage,
                         const std::string& error_message);

    const std::vector<FailureTrace>& traces() const;
    void clear();
    std::size_t size() const;

private:
    std::vector<FailureTrace> traces_;
};

}  // namespace browser::core
