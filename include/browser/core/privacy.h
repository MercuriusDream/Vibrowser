#pragma once

#include <string>
#include <vector>

namespace browser::core {

struct PrivacySettings {
    bool telemetry_enabled = false;
    bool crash_reporting_enabled = false;
    bool usage_analytics_enabled = false;
    bool diagnostic_export_enabled = false;

    bool any_enabled() const;
    bool all_disabled() const;
    std::vector<std::string> enabled_features() const;
};

const char* privacy_feature_name(const std::string& feature);

struct PrivacyAuditEntry {
    std::string feature;
    bool was_allowed = false;
    std::string reason;
};

class PrivacyGuard {
public:
    explicit PrivacyGuard(const PrivacySettings& settings = {});

    void update_settings(const PrivacySettings& settings);
    const PrivacySettings& settings() const;

    bool is_allowed(const std::string& feature) const;
    PrivacyAuditEntry check(const std::string& feature) const;

    const std::vector<PrivacyAuditEntry>& audit_log() const;
    void clear_audit_log();

private:
    PrivacySettings settings_;
    mutable std::vector<PrivacyAuditEntry> audit_log_;
};

}  // namespace browser::core
