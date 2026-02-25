#pragma once

#include <string>

namespace browser::engine {

enum class InputType {
    Unknown,
    HttpUrl,
    FileUrl,
    LocalPath,
    DataUrl,
};

struct NavigationInput {
    std::string raw_input;
    std::string canonical_url;
    InputType input_type = InputType::Unknown;
};

const char* input_type_name(InputType type);

InputType classify_input(const std::string& input);

bool normalize_input(const std::string& raw_input,
                     NavigationInput& result,
                     std::string& err);

}  // namespace browser::engine
