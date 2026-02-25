#include "browser/core/privacy.h"

namespace browser::core {

bool PrivacySettings::any_enabled() const {
    return telemetry_enabled || crash_reporting_enabled ||
           usage_analytics_enabled || diagnostic_export_enabled;
}

bool PrivacySettings::all_disabled() const {
    return !any_enabled();
}

std::vector<std::string> PrivacySettings::enabled_features() const {
    std::vector<std::string> result;
    if (telemetry_enabled) result.push_back("telemetry");
    if (crash_reporting_enabled) result.push_back("crash_reporting");
    if (usage_analytics_enabled) result.push_back("usage_analytics");
    if (diagnostic_export_enabled) result.push_back("diagnostic_export");
    return result;
}

PrivacyGuard::PrivacyGuard(const PrivacySettings& settings) : settings_(settings) {}

void PrivacyGuard::update_settings(const PrivacySettings& settings) {
    settings_ = settings;
}

const PrivacySettings& PrivacyGuard::settings() const {
    return settings_;
}

bool PrivacyGuard::is_allowed(const std::string& feature) const {
    PrivacyAuditEntry entry = check(feature);
    audit_log_.push_back(entry);
    return entry.was_allowed;
}

PrivacyAuditEntry PrivacyGuard::check(const std::string& feature) const {
    PrivacyAuditEntry entry;
    entry.feature = feature;

    if (feature == "telemetry") {
        entry.was_allowed = settings_.telemetry_enabled;
        entry.reason = entry.was_allowed ? "telemetry opt-in" : "telemetry not enabled";
    } else if (feature == "crash_reporting") {
        entry.was_allowed = settings_.crash_reporting_enabled;
        entry.reason = entry.was_allowed ? "crash reporting opt-in" : "crash reporting not enabled";
    } else if (feature == "usage_analytics") {
        entry.was_allowed = settings_.usage_analytics_enabled;
        entry.reason = entry.was_allowed ? "usage analytics opt-in" : "usage analytics not enabled";
    } else if (feature == "diagnostic_export") {
        entry.was_allowed = settings_.diagnostic_export_enabled;
        entry.reason = entry.was_allowed ? "diagnostic export opt-in" : "diagnostic export not enabled";
    } else {
        entry.was_allowed = false;
        entry.reason = "unknown feature: " + feature;
    }

    return entry;
}

const std::vector<PrivacyAuditEntry>& PrivacyGuard::audit_log() const {
    return audit_log_;
}

void PrivacyGuard::clear_audit_log() {
    audit_log_.clear();
}

}  // namespace browser::core
