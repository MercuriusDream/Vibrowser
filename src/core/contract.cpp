#include "browser/core/contract.h"

#include <sstream>

namespace browser::core {

void ContractValidator::add_check(const std::string& module,
                                   const std::string& interface_name,
                                   const std::string& description,
                                   std::function<bool(std::string&)> check) {
    checks_.push_back({module, interface_name, description, std::move(check)});
}

void ContractValidator::validate_all() {
    results_.clear();
    for (const auto& c : checks_) {
        ContractResult r;
        r.module = c.module;
        r.interface_name = c.interface_name;
        std::string detail;
        r.passed = c.check(detail);
        r.detail = detail;
        results_.push_back(std::move(r));
    }
}

void ContractValidator::validate_module(const std::string& module) {
    // Remove old results for this module
    std::vector<ContractResult> kept;
    for (auto& r : results_) {
        if (r.module != module) {
            kept.push_back(std::move(r));
        }
    }
    results_ = std::move(kept);

    for (const auto& c : checks_) {
        if (c.module != module) continue;
        ContractResult r;
        r.module = c.module;
        r.interface_name = c.interface_name;
        std::string detail;
        r.passed = c.check(detail);
        r.detail = detail;
        results_.push_back(std::move(r));
    }
}

const std::vector<ContractResult>& ContractValidator::results() const {
    return results_;
}

std::vector<ContractResult> ContractValidator::results_for_module(const std::string& module) const {
    std::vector<ContractResult> result;
    for (const auto& r : results_) {
        if (r.module == module) {
            result.push_back(r);
        }
    }
    return result;
}

bool ContractValidator::all_passed() const {
    if (results_.empty()) return false;
    for (const auto& r : results_) {
        if (!r.passed) return false;
    }
    return true;
}

std::size_t ContractValidator::pass_count() const {
    std::size_t count = 0;
    for (const auto& r : results_) {
        if (r.passed) ++count;
    }
    return count;
}

std::size_t ContractValidator::fail_count() const {
    std::size_t count = 0;
    for (const auto& r : results_) {
        if (!r.passed) ++count;
    }
    return count;
}

std::size_t ContractValidator::check_count() const {
    return checks_.size();
}

std::string ContractValidator::format_report() const {
    std::ostringstream oss;
    oss << "Contract Validation: " << pass_count() << "/" << results_.size() << " passed\n";
    for (const auto& r : results_) {
        oss << "  [" << (r.passed ? "PASS" : "FAIL") << "] "
            << r.module << "::" << r.interface_name;
        if (!r.detail.empty()) {
            oss << " — " << r.detail;
        }
        oss << "\n";
    }
    return oss.str();
}

void ContractValidator::clear() {
    checks_.clear();
    results_.clear();
}

}  // namespace browser::core
