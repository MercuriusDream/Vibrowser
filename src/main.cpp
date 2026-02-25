#include "browser/core/config.h"
#include "browser/core/lifecycle.h"
#include "browser/engine/engine.h"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr const char kDefaultOutputPath[] = "output.ppm";
constexpr const char kProgramName[] = "from_scratch_browser";
constexpr const char kVersionString[] = "from_scratch_browser 0.1.0";

void print_usage(std::ostream& stream) {
  stream << "usage: " << kProgramName
         << " <url> [output.ppm] [width] [height] [--size=WIDTHxHEIGHT]\n";
}

bool is_help_flag(const char* input) {
  if (input == nullptr) {
    return false;
  }

  const std::string_view text(input);
  return text == "-h" || text == "--help";
}

bool is_version_flag(const char* input) {
  if (input == nullptr) {
    return false;
  }

  const std::string_view text(input);
  return text == "-V" || text == "--version";
}

bool parse_positive_int(const char* input, int& value) {
  if (input == nullptr) {
    return false;
  }

  const std::string text(input);
  if (text.empty()) {
    return false;
  }

  int parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc() || result.ptr != end || parsed <= 0) {
    return false;
  }

  value = parsed;
  return true;
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

bool parse_size_flag(const char* input, int& width, int& height) {
  if (input == nullptr) {
    return false;
  }

  const std::string_view text(input);
  constexpr std::string_view kSizePrefix = "--size=";
  if (!starts_with(text, kSizePrefix)) {
    return false;
  }

  const std::string_view dimensions = text.substr(kSizePrefix.size());
  const std::size_t separator = dimensions.find('x');
  if (separator == std::string_view::npos || separator == 0 || separator + 1 >= dimensions.size()) {
    return false;
  }
  if (dimensions.find('x', separator + 1) != std::string_view::npos) {
    return false;
  }

  const std::string width_text(dimensions.substr(0, separator));
  const std::string height_text(dimensions.substr(separator + 1));

  int parsed_width = 0;
  int parsed_height = 0;
  if (!parse_positive_int(width_text.c_str(), parsed_width) ||
      !parse_positive_int(height_text.c_str(), parsed_height)) {
    return false;
  }

  width = parsed_width;
  height = parsed_height;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && is_help_flag(argv[1])) {
    print_usage(std::cout);
    return 0;
  }
  if (argc == 2 && is_version_flag(argv[1])) {
    std::cout << kVersionString << "\n";
    return 0;
  }

  if (argc < 2) {
    print_usage(std::cerr);
    return 1;
  }

  const std::string url = argv[1];

  std::string output_path = kDefaultOutputPath;
  int width = static_cast<int>(browser::core::config::kDefaultViewportWidth);
  int height = static_cast<int>(browser::core::config::kDefaultViewportHeight);

  std::vector<const char*> positional_args;
  positional_args.reserve(static_cast<std::size_t>(argc));

  bool has_size_flag = false;
  for (int index = 2; index < argc; ++index) {
    const std::string_view argument(argv[index] != nullptr ? argv[index] : "");
    const bool is_size_argument = argument == "--size" || starts_with(argument, "--size=");
    if (is_size_argument) {
      if (has_size_flag) {
        std::cerr << "Invalid --size: duplicate flag '" << argument << "'\n";
        print_usage(std::cerr);
        return 1;
      }
      if (!parse_size_flag(argv[index], width, height)) {
        std::cerr << "Invalid --size: '" << argument
                  << "' (expected --size=WIDTHxHEIGHT with positive integers)\n";
        print_usage(std::cerr);
        return 1;
      }
      has_size_flag = true;
      continue;
    }

    positional_args.push_back(argv[index]);
  }

  if (positional_args.size() > 3) {
    print_usage(std::cerr);
    return 1;
  }

  if (!positional_args.empty()) {
    output_path = positional_args[0];
  }
  if (positional_args.size() >= 2 && !parse_positive_int(positional_args[1], width)) {
    std::cerr << "Invalid width: " << positional_args[1] << "\n";
    print_usage(std::cerr);
    return 1;
  }
  if (positional_args.size() >= 3 && !parse_positive_int(positional_args[2], height)) {
    std::cerr << "Invalid height: " << positional_args[2] << "\n";
    print_usage(std::cerr);
    return 1;
  }

  browser::engine::BrowserEngine engine;
  browser::engine::RenderOptions render_opts;
  render_opts.viewport_width = width;
  render_opts.viewport_height = height;
  render_opts.output_path = output_path;

  const browser::engine::EngineResult result = engine.navigate(url, render_opts);

  if (!result.ok) {
    std::cerr << result.message << "\n";
    return 1;
  }

  std::cout << result.message << "\n";
  return 0;
}
