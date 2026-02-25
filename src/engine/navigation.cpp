#include "browser/engine/navigation.h"

#include "browser/net/url.h"

#include <cctype>
#include <filesystem>
#include <system_error>

namespace browser::engine {
namespace {

bool starts_with_data_scheme(const std::string& value) {
    if (value.size() < 5) {
        return false;
    }
    return std::tolower(static_cast<unsigned char>(value[0])) == 'd' &&
           std::tolower(static_cast<unsigned char>(value[1])) == 'a' &&
           std::tolower(static_cast<unsigned char>(value[2])) == 't' &&
           std::tolower(static_cast<unsigned char>(value[3])) == 'a' &&
           value[4] == ':';
}

std::filesystem::path normalize_file_path(const std::filesystem::path& path) {
    std::filesystem::path normalized = path;
    if (normalized.is_relative()) {
        normalized = std::filesystem::absolute(normalized);
    }
    return normalized.lexically_normal();
}

}  // namespace

const char* input_type_name(InputType type) {
    switch (type) {
        case InputType::Unknown:   return "unknown";
        case InputType::HttpUrl:   return "http_url";
        case InputType::FileUrl:   return "file_url";
        case InputType::LocalPath: return "local_path";
        case InputType::DataUrl:   return "data_url";
    }
    return "unknown";
}

InputType classify_input(const std::string& input) {
    if (input.empty()) {
        return InputType::Unknown;
    }

    if (browser::net::is_file_url(input)) {
        return InputType::FileUrl;
    }

    if (starts_with_data_scheme(input)) {
        return InputType::DataUrl;
    }

    browser::net::Url parsed;
    std::string err;
    if (browser::net::parse_url(input, parsed, err)) {
        return InputType::HttpUrl;
    }

    std::error_code fs_err;
    if (std::filesystem::exists(input, fs_err) && !fs_err) {
        return InputType::LocalPath;
    }

    return InputType::Unknown;
}

bool normalize_input(const std::string& raw_input,
                     NavigationInput& result,
                     std::string& err) {
    result = {};
    result.raw_input = raw_input;
    result.input_type = classify_input(raw_input);

    switch (result.input_type) {
        case InputType::FileUrl: {
            std::string path;
            if (!browser::net::file_url_to_path(raw_input, path, err)) {
                return false;
            }
            const std::filesystem::path normalized = normalize_file_path(path);
            result.canonical_url =
                browser::net::path_to_file_url(normalized.generic_string());
            return true;
        }

        case InputType::HttpUrl: {
            browser::net::Url parsed;
            if (!browser::net::parse_url(raw_input, parsed, err)) {
                return false;
            }
            result.canonical_url = parsed.to_string();
            return true;
        }

        case InputType::LocalPath: {
            const std::filesystem::path normalized =
                normalize_file_path(raw_input);
            result.canonical_url =
                browser::net::path_to_file_url(normalized.generic_string());
            return true;
        }

        case InputType::DataUrl:
            result.canonical_url = raw_input;
            return true;

        case InputType::Unknown:
            break;
    }

    err = "Unable to resolve input: " + raw_input;
    return false;
}

}  // namespace browser::engine
