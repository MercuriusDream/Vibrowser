#pragma once

#include <functional>
#include <string>
#include <vector>

namespace browser::core {

struct ContractCheck {
    std::string module;
    std::string interface_name;
    std::string description;
    std::function<bool(std::string& detail)> check;
};

struct ContractResult {
    std::string module;
    std::string interface_name;
    bool passed = false;
    std::string detail;
};

class ContractValidator {
public:
    void add_check(const std::string& module,
                   const std::string& interface_name,
                   const std::string& description,
                   std::function<bool(std::string&)> check);

    void validate_all();
    void validate_module(const std::string& module);

    const std::vector<ContractResult>& results() const;
    std::vector<ContractResult> results_for_module(const std::string& module) const;

    bool all_passed() const;
    std::size_t pass_count() const;
    std::size_t fail_count() const;
    std::size_t check_count() const;

    std::string format_report() const;
    void clear();

private:
    std::vector<ContractCheck> checks_;
    std::vector<ContractResult> results_;
};

}  // namespace browser::core
