#include "browser/render/renderer.h"
#include "browser/layout/layout_engine.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace browser::render {
namespace {

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string trim_copy(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && is_space(text[begin])) {
    ++begin;
  }

  std::size_t end = text.size();
  while (end > begin && is_space(text[end - 1])) {
    --end;
  }

  return std::string(text.substr(begin, end - begin));
}

std::string to_lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

template <typename T>
std::string value_to_string(const T& value) {
  using Decayed = std::decay_t<T>;

  if constexpr (is_optional<Decayed>::value) {
    if (value.has_value()) {
      return value_to_string(*value);
    }
    return {};
  } else if constexpr (std::is_same_v<Decayed, std::string>) {
    return value;
  } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
    return std::string(value);
  } else if constexpr (std::is_same_v<Decayed, const char*> || std::is_same_v<Decayed, char*>) {
    return value == nullptr ? std::string() : std::string(value);
  } else if constexpr (std::is_convertible_v<Decayed, std::string>) {
    return static_cast<std::string>(value);
  } else {
    return {};
  }
}

template <typename T>
int value_to_int(const T& value) {
  using Decayed = std::decay_t<T>;

  if constexpr (is_optional<Decayed>::value) {
    if (value.has_value()) {
      return value_to_int(*value);
    }
    return 0;
  } else if constexpr (std::is_integral_v<Decayed>) {
    return static_cast<int>(value);
  } else if constexpr (std::is_floating_point_v<Decayed>) {
    return static_cast<int>(value);
  } else {
    return 0;
  }
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

bool parse_hex_byte(char high, char low, std::uint8_t& out) {
  const int hi = hex_value(high);
  const int lo = hex_value(low);
  if (hi < 0 || lo < 0) {
    return false;
  }
  out = static_cast<std::uint8_t>((hi << 4) | lo);
  return true;
}

bool parse_short_hex_byte(char digit, std::uint8_t& out) {
  const int value = hex_value(digit);
  if (value < 0) {
    return false;
  }
  out = static_cast<std::uint8_t>((value << 4) | value);
  return true;
}

bool parse_rgb_component(std::string_view token, std::uint8_t& out) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty()) {
    return false;
  }

  if (trimmed.back() == '%') {
    const std::string number = trim_copy(std::string_view(trimmed).substr(0, trimmed.size() - 1));
    if (number.empty()) {
      return false;
    }

    const char* begin = number.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (begin == end || end == nullptr || *end != '\0' || errno == ERANGE) {
      return false;
    }

    if (!(value >= 0.0 && value <= 100.0)) {
      return false;
    }

    const int byte_value = static_cast<int>(std::round((value * 255.0) / 100.0));
    out = static_cast<std::uint8_t>(std::clamp(byte_value, 0, 255));
    return true;
  }

  int value = 0;
  for (char c : trimmed) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = (value * 10) + (c - '0');
    if (value > 255) {
      return false;
    }
  }

  out = static_cast<std::uint8_t>(value);
  return true;
}

bool parse_alpha_component(std::string_view token, double& out) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty()) {
    return false;
  }

  const char* begin = trimmed.c_str();
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(begin, &end);
  if (begin == end || end == nullptr || *end != '\0' || errno == ERANGE) {
    return false;
  }

  if (!(value >= 0.0 && value <= 1.0)) {
    return false;
  }

  out = value;
  return true;
}

bool parse_hsl_hue_component(std::string_view token, double& out) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty()) {
    return false;
  }

  const char* begin = trimmed.c_str();
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(begin, &end);
  if (begin == end || end == nullptr || *end != '\0' || errno == ERANGE) {
    return false;
  }

  out = value;
  return true;
}

bool parse_hsl_percentage_component(std::string_view token, double& out) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty() || trimmed.back() != '%') {
    return false;
  }

  const std::string number = trim_copy(std::string_view(trimmed).substr(0, trimmed.size() - 1));
  if (number.empty()) {
    return false;
  }

  const char* begin = number.c_str();
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(begin, &end);
  if (begin == end || end == nullptr || *end != '\0' || errno == ERANGE) {
    return false;
  }

  if (!(value >= 0.0 && value <= 100.0)) {
    return false;
  }

  out = value / 100.0;
  return true;
}

template <std::size_t N>
bool split_comma_components(std::string_view body, std::array<std::string_view, N>& components) {
  std::size_t component_index = 0;
  std::size_t start = 0;

  while (start <= body.size()) {
    if (component_index >= components.size()) {
      return false;
    }

    const std::size_t comma = body.find(',', start);
    if (comma == std::string::npos) {
      components[component_index++] = body.substr(start);
      break;
    }

    components[component_index++] = body.substr(start, comma - start);
    start = comma + 1;
  }

  return component_index == components.size();
}

std::uint8_t blend_over_white(std::uint8_t channel, double alpha) {
  const double blended = (static_cast<double>(channel) * alpha) + (255.0 * (1.0 - alpha));
  const int rounded = static_cast<int>(blended + 0.5);
  return static_cast<std::uint8_t>(std::clamp(rounded, 0, 255));
}

std::uint8_t normalized_channel_to_byte(double value) {
  const double clamped = std::clamp(value, 0.0, 1.0);
  const int rounded = static_cast<int>((clamped * 255.0) + 0.5);
  return static_cast<std::uint8_t>(std::clamp(rounded, 0, 255));
}

void hsl_to_rgb(double hue_degrees, double saturation, double lightness,
                std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
  double hue = std::fmod(hue_degrees, 360.0);
  if (hue < 0.0) {
    hue += 360.0;
  }

  const double chroma = (1.0 - std::abs((2.0 * lightness) - 1.0)) * saturation;
  const double hue_prime = hue / 60.0;
  const double x = chroma * (1.0 - std::abs(std::fmod(hue_prime, 2.0) - 1.0));

  double r1 = 0.0;
  double g1 = 0.0;
  double b1 = 0.0;

  if (hue_prime < 1.0) {
    r1 = chroma;
    g1 = x;
  } else if (hue_prime < 2.0) {
    r1 = x;
    g1 = chroma;
  } else if (hue_prime < 3.0) {
    g1 = chroma;
    b1 = x;
  } else if (hue_prime < 4.0) {
    g1 = x;
    b1 = chroma;
  } else if (hue_prime < 5.0) {
    r1 = x;
    b1 = chroma;
  } else {
    r1 = chroma;
    b1 = x;
  }

  const double m = lightness - (chroma / 2.0);
  r = normalized_channel_to_byte(r1 + m);
  g = normalized_channel_to_byte(g1 + m);
  b = normalized_channel_to_byte(b1 + m);
}

bool try_parse_rgba_function(std::string_view value, Color& out) {
  if (value.rfind("rgba", 0) != 0) {
    return false;
  }

  std::size_t cursor = 4;
  while (cursor < value.size() && is_space(value[cursor])) {
    ++cursor;
  }

  if (cursor >= value.size() || value[cursor] != '(') {
    return false;
  }

  ++cursor;
  const std::size_t close = value.find(')', cursor);
  if (close == std::string::npos) {
    return false;
  }

  for (std::size_t i = close + 1; i < value.size(); ++i) {
    if (!is_space(value[i])) {
      return false;
    }
  }

  const std::string_view body = value.substr(cursor, close - cursor);
  std::array<std::string_view, 4> components{};
  if (!split_comma_components(body, components)) {
    return false;
  }

  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  double alpha = 0.0;
  if (!parse_rgb_component(components[0], r) ||
      !parse_rgb_component(components[1], g) ||
      !parse_rgb_component(components[2], b) ||
      !parse_alpha_component(components[3], alpha)) {
    return false;
  }

  out = Color{
      blend_over_white(r, alpha),
      blend_over_white(g, alpha),
      blend_over_white(b, alpha),
  };
  return true;
}

bool try_parse_rgb_function(std::string_view value, Color& out) {
  if (value.rfind("rgb", 0) != 0) {
    return false;
  }

  std::size_t cursor = 3;
  while (cursor < value.size() && is_space(value[cursor])) {
    ++cursor;
  }

  if (cursor >= value.size() || value[cursor] != '(') {
    return false;
  }

  ++cursor;
  const std::size_t close = value.find(')', cursor);
  if (close == std::string::npos) {
    return false;
  }

  for (std::size_t i = close + 1; i < value.size(); ++i) {
    if (!is_space(value[i])) {
      return false;
    }
  }

  const std::string_view body = value.substr(cursor, close - cursor);
  std::array<std::string_view, 3> components{};
  if (!split_comma_components(body, components)) {
    return false;
  }

  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  if (!parse_rgb_component(components[0], r) ||
      !parse_rgb_component(components[1], g) ||
      !parse_rgb_component(components[2], b)) {
    return false;
  }

  out = Color{r, g, b};
  return true;
}

bool try_parse_hsl_function(std::string_view value, Color& out) {
  if (value.rfind("hsl", 0) != 0) {
    return false;
  }

  std::size_t cursor = 3;
  while (cursor < value.size() && is_space(value[cursor])) {
    ++cursor;
  }

  if (cursor >= value.size() || value[cursor] != '(') {
    return false;
  }

  ++cursor;
  const std::size_t close = value.find(')', cursor);
  if (close == std::string::npos) {
    return false;
  }

  for (std::size_t i = close + 1; i < value.size(); ++i) {
    if (!is_space(value[i])) {
      return false;
    }
  }

  const std::string_view body = value.substr(cursor, close - cursor);
  std::array<std::string_view, 3> components{};
  if (!split_comma_components(body, components)) {
    return false;
  }

  double hue = 0.0;
  double saturation = 0.0;
  double lightness = 0.0;
  if (!parse_hsl_hue_component(components[0], hue) ||
      !parse_hsl_percentage_component(components[1], saturation) ||
      !parse_hsl_percentage_component(components[2], lightness)) {
    return false;
  }

  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  hsl_to_rgb(hue, saturation, lightness, r, g, b);
  out = Color{r, g, b};
  return true;
}

bool try_parse_hsla_function(std::string_view value, Color& out) {
  if (value.rfind("hsla", 0) != 0) {
    return false;
  }

  std::size_t cursor = 4;
  while (cursor < value.size() && is_space(value[cursor])) {
    ++cursor;
  }

  if (cursor >= value.size() || value[cursor] != '(') {
    return false;
  }

  ++cursor;
  const std::size_t close = value.find(')', cursor);
  if (close == std::string::npos) {
    return false;
  }

  for (std::size_t i = close + 1; i < value.size(); ++i) {
    if (!is_space(value[i])) {
      return false;
    }
  }

  const std::string_view body = value.substr(cursor, close - cursor);
  std::array<std::string_view, 4> components{};
  if (!split_comma_components(body, components)) {
    return false;
  }

  double hue = 0.0;
  double saturation = 0.0;
  double lightness = 0.0;
  double alpha = 0.0;
  if (!parse_hsl_hue_component(components[0], hue) ||
      !parse_hsl_percentage_component(components[1], saturation) ||
      !parse_hsl_percentage_component(components[2], lightness) ||
      !parse_alpha_component(components[3], alpha)) {
    return false;
  }

  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  hsl_to_rgb(hue, saturation, lightness, r, g, b);
  out = Color{
      blend_over_white(r, alpha),
      blend_over_white(g, alpha),
      blend_over_white(b, alpha),
  };
  return true;
}

bool try_parse_color(std::string_view raw, Color& out) {
  std::string value = to_lower_ascii(trim_copy(raw));
  if (value.empty()) {
    return false;
  }

  if (value == "black") {
    out = Color{0, 0, 0};
    return true;
  }
  if (value == "white") {
    out = Color{255, 255, 255};
    return true;
  }
  if (value == "red") {
    out = Color{255, 0, 0};
    return true;
  }
  if (value == "green") {
    out = Color{0, 128, 0};
    return true;
  }
  if (value == "lime") {
    out = Color{0, 255, 0};
    return true;
  }
  if (value == "blue") {
    out = Color{0, 0, 255};
    return true;
  }
  if (value == "navy") {
    out = Color{0, 0, 128};
    return true;
  }
  if (value == "teal") {
    out = Color{0, 128, 128};
    return true;
  }
  if (value == "olive") {
    out = Color{128, 128, 0};
    return true;
  }
  if (value == "maroon") {
    out = Color{128, 0, 0};
    return true;
  }
  if (value == "orange") {
    out = Color{255, 165, 0};
    return true;
  }
  if (value == "gold") {
    out = Color{255, 215, 0};
    return true;
  }
  if (value == "yellow") {
    out = Color{255, 255, 0};
    return true;
  }
  if (value == "beige") {
    out = Color{245, 245, 220};
    return true;
  }
  if (value == "chartreuse") {
    out = Color{127, 255, 0};
    return true;
  }
  if (value == "coral") {
    out = Color{255, 127, 80};
    return true;
  }
  if (value == "crimson") {
    out = Color{220, 20, 60};
    return true;
  }
  if (value == "firebrick") {
    out = Color{178, 34, 34};
    return true;
  }
  if (value == "salmon") {
    out = Color{250, 128, 114};
    return true;
  }
  if (value == "khaki") {
    out = Color{240, 230, 140};
    return true;
  }
  if (value == "tan") {
    out = Color{210, 180, 140};
    return true;
  }
  if (value == "peru") {
    out = Color{205, 133, 63};
    return true;
  }
  if (value == "sienna") {
    out = Color{160, 82, 45};
    return true;
  }
  if (value == "plum") {
    out = Color{221, 160, 221};
    return true;
  }
  if (value == "orchid") {
    out = Color{218, 112, 214};
    return true;
  }
  if (value == "lavender") {
    out = Color{230, 230, 250};
    return true;
  }
  if (value == "tomato") {
    out = Color{255, 99, 71};
    return true;
  }
  if (value == "seagreen") {
    out = Color{46, 139, 87};
    return true;
  }
  if (value == "slateblue") {
    out = Color{106, 90, 205};
    return true;
  }
  if (value == "turquoise") {
    out = Color{64, 224, 208};
    return true;
  }
  if (value == "indigo") {
    out = Color{75, 0, 130};
    return true;
  }
  if (value == "rebeccapurple") {
    out = Color{102, 51, 153};
    return true;
  }
  if (value == "cyan" || value == "aqua") {
    out = Color{0, 255, 255};
    return true;
  }
  if (value == "magenta" || value == "fuchsia") {
    out = Color{255, 0, 255};
    return true;
  }
  if (value == "gray" || value == "grey") {
    out = Color{128, 128, 128};
    return true;
  }
  if (value == "silver") {
    out = Color{192, 192, 192};
    return true;
  }
  if (value == "transparent") {
    constexpr std::uint8_t transparent_black_channel = 0;
    constexpr double fully_transparent_alpha = 0.0;
    out = Color{
        blend_over_white(transparent_black_channel, fully_transparent_alpha),
        blend_over_white(transparent_black_channel, fully_transparent_alpha),
        blend_over_white(transparent_black_channel, fully_transparent_alpha),
    };
    return true;
  }

  if (try_parse_rgba_function(value, out)) {
    return true;
  }

  if (try_parse_rgb_function(value, out)) {
    return true;
  }

  if (try_parse_hsla_function(value, out)) {
    return true;
  }

  if (try_parse_hsl_function(value, out)) {
    return true;
  }

  if (value.size() == 4 && value.front() == '#') {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    if (parse_short_hex_byte(value[1], r) &&
        parse_short_hex_byte(value[2], g) &&
        parse_short_hex_byte(value[3], b)) {
      out = Color{r, g, b};
      return true;
    }
  }

  if (value.size() == 5 && value.front() == '#') {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
    if (parse_short_hex_byte(value[1], r) &&
        parse_short_hex_byte(value[2], g) &&
        parse_short_hex_byte(value[3], b) &&
        parse_short_hex_byte(value[4], a)) {
      const double alpha = static_cast<double>(a) / 255.0;
      out = Color{
          blend_over_white(r, alpha),
          blend_over_white(g, alpha),
          blend_over_white(b, alpha),
      };
      return true;
    }
  }

  if (value.size() == 7 && value.front() == '#') {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    if (parse_hex_byte(value[1], value[2], r) &&
        parse_hex_byte(value[3], value[4], g) &&
        parse_hex_byte(value[5], value[6], b)) {
      out = Color{r, g, b};
      return true;
    }
  }

  if (value.size() == 9 && value.front() == '#') {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
    if (parse_hex_byte(value[1], value[2], r) &&
        parse_hex_byte(value[3], value[4], g) &&
        parse_hex_byte(value[5], value[6], b) &&
        parse_hex_byte(value[7], value[8], a)) {
      const double alpha = static_cast<double>(a) / 255.0;
      out = Color{
          blend_over_white(r, alpha),
          blend_over_white(g, alpha),
          blend_over_white(b, alpha),
      };
      return true;
    }
  }

  return false;
}

bool try_parse_paint_color(std::string_view raw, const Color& current_color, Color& out) {
  const std::string value = to_lower_ascii(trim_copy(raw));
  if (value.empty()) {
    return false;
  }

  if (value == "currentcolor") {
    out = current_color;
    return true;
  }

  return try_parse_color(value, out);
}

std::vector<std::string> split_whitespace(const std::string& text) {
  std::vector<std::string> tokens;
  std::size_t cursor = 0;

  while (cursor < text.size()) {
    while (cursor < text.size() && is_space(text[cursor])) {
      ++cursor;
    }

    const std::size_t start = cursor;
    while (cursor < text.size() && !is_space(text[cursor])) {
      ++cursor;
    }

    if (start < cursor) {
      tokens.push_back(text.substr(start, cursor - start));
    }
  }

  return tokens;
}

bool try_parse_length_token(const std::string& token, int& out) {
  const std::string lower = to_lower_ascii(trim_copy(token));
  if (lower.empty()) {
    return false;
  }

  if (lower == "thin") {
    out = 1;
    return true;
  }
  if (lower == "medium") {
    out = 3;
    return true;
  }
  if (lower == "thick") {
    out = 5;
    return true;
  }

  std::size_t i = 0;
  while (i < lower.size() && std::isdigit(static_cast<unsigned char>(lower[i])) != 0) {
    ++i;
  }
  if (i == 0) {
    return false;
  }

  const std::size_t digits_end = i;
  if (i < lower.size()) {
    if (i + 2 == lower.size() && lower.compare(i, 2, "px") == 0) {
      i += 2;
    } else {
      return false;
    }
  }

  if (i != lower.size()) {
    return false;
  }

  out = std::stoi(lower.substr(0, digits_end));
  return true;
}

int parse_border_width(const std::string& border_width_value, const std::string& border_value) {
  int width = 0;
  if (try_parse_length_token(border_width_value, width)) {
    return std::max(width, 0);
  }

  for (const auto& token : split_whitespace(border_value)) {
    if (try_parse_length_token(token, width)) {
      return std::max(width, 0);
    }
  }

  return 0;
}

bool parse_border_color(const std::string& border_color_value,
                        const std::string& border_value,
                        const Color& current_color,
                        Color& out) {
  if (try_parse_paint_color(border_color_value, current_color, out)) {
    return true;
  }

  for (const auto& token : split_whitespace(border_value)) {
    if (try_parse_paint_color(token, current_color, out)) {
      return true;
    }
  }

  return false;
}

template <typename Box>
std::string get_style_property(const Box& box, const std::string& key);

template <typename Box>
Color resolve_box_text_color(const Box& box) {
  Color text_color{0, 0, 0};
  const std::string text_color_value = get_style_property(box, "color");
  if (try_parse_color(text_color_value, text_color)) {
    return text_color;
  }

  return Color{0, 0, 0};
}

[[maybe_unused]] std::string lookup_inline_style(std::string_view style_text, const std::string& key) {
  const std::string target = to_lower_ascii(trim_copy(key));
  if (target.empty()) {
    return {};
  }

  std::size_t cursor = 0;
  while (cursor < style_text.size()) {
    const std::size_t semi = style_text.find(';', cursor);
    const std::string_view part =
        semi == std::string_view::npos ? style_text.substr(cursor) : style_text.substr(cursor, semi - cursor);

    const std::size_t colon = part.find(':');
    if (colon != std::string_view::npos) {
      std::string property = to_lower_ascii(trim_copy(part.substr(0, colon)));
      if (property == target) {
        return trim_copy(part.substr(colon + 1));
      }
    }

    if (semi == std::string_view::npos) {
      break;
    }
    cursor = semi + 1;
  }

  return {};
}

template <typename T, typename = void>
struct has_map_lookup_by_string_key : std::false_type {};

template <typename T>
struct has_map_lookup_by_string_key<
    T,
    std::void_t<
        decltype(std::declval<const T&>().find(std::declval<const std::string&>())),
        decltype(std::declval<const T&>().end()),
        decltype(std::declval<const T&>().find(std::declval<const std::string&>())->second)>> : std::true_type {};

template <typename MapLike>
std::string lookup_map_like(const MapLike& map_like, const std::string& key) {
  if constexpr (has_map_lookup_by_string_key<MapLike>::value) {
    const auto it = map_like.find(key);
    if (it != map_like.end()) {
      return trim_copy(value_to_string(it->second));
    }
  }
  return {};
}

#define BROWSER_RENDER_DECLARE_HAS_MEMBER(name)                                          \
  template <typename T, typename = void>                                                  \
  struct has_member_##name : std::false_type {};                                          \
  template <typename T>                                                                   \
  struct has_member_##name<T, std::void_t<decltype(std::declval<const T&>().name)>>      \
      : std::true_type {}

#define BROWSER_RENDER_DECLARE_HAS_METHOD0(name)                                          \
  template <typename T, typename = void>                                                  \
  struct has_method_##name : std::false_type {};                                          \
  template <typename T>                                                                   \
  struct has_method_##name<T, std::void_t<decltype(std::declval<const T&>().name())>>    \
      : std::true_type {}

#define BROWSER_RENDER_DECLARE_HAS_METHOD1(name)                                                            \
  template <typename T, typename = void>                                                                    \
  struct has_method_##name : std::false_type {};                                                            \
  template <typename T>                                                                                     \
  struct has_method_##name<                                                                                 \
      T,                                                                                                    \
      std::void_t<decltype(std::declval<const T&>().name(std::declval<const std::string&>()))>>           \
      : std::true_type {}

BROWSER_RENDER_DECLARE_HAS_MEMBER(style);
BROWSER_RENDER_DECLARE_HAS_MEMBER(styles);
BROWSER_RENDER_DECLARE_HAS_MEMBER(computed_style);
BROWSER_RENDER_DECLARE_HAS_MEMBER(computed_styles);
BROWSER_RENDER_DECLARE_HAS_MEMBER(properties);
BROWSER_RENDER_DECLARE_HAS_MEMBER(css);

BROWSER_RENDER_DECLARE_HAS_METHOD1(style_value);
BROWSER_RENDER_DECLARE_HAS_METHOD1(get_style);
BROWSER_RENDER_DECLARE_HAS_METHOD1(get_property);
BROWSER_RENDER_DECLARE_HAS_METHOD1(property);

template <typename T, typename = void>
struct has_method_style_key : std::false_type {};

template <typename T>
struct has_method_style_key<T, std::void_t<decltype(std::declval<const T&>().style(std::declval<const std::string&>()))>>
    : std::true_type {};

BROWSER_RENDER_DECLARE_HAS_MEMBER(rect);
BROWSER_RENDER_DECLARE_HAS_MEMBER(bounds);
BROWSER_RENDER_DECLARE_HAS_MEMBER(frame);
BROWSER_RENDER_DECLARE_HAS_MEMBER(box);
BROWSER_RENDER_DECLARE_HAS_MEMBER(x);
BROWSER_RENDER_DECLARE_HAS_MEMBER(y);
BROWSER_RENDER_DECLARE_HAS_MEMBER(width);
BROWSER_RENDER_DECLARE_HAS_MEMBER(height);
BROWSER_RENDER_DECLARE_HAS_MEMBER(left);
BROWSER_RENDER_DECLARE_HAS_MEMBER(top);
BROWSER_RENDER_DECLARE_HAS_MEMBER(w);
BROWSER_RENDER_DECLARE_HAS_MEMBER(h);

BROWSER_RENDER_DECLARE_HAS_METHOD0(rect);
BROWSER_RENDER_DECLARE_HAS_METHOD0(bounds);
BROWSER_RENDER_DECLARE_HAS_METHOD0(frame);
BROWSER_RENDER_DECLARE_HAS_METHOD0(box);
BROWSER_RENDER_DECLARE_HAS_METHOD0(x);
BROWSER_RENDER_DECLARE_HAS_METHOD0(y);
BROWSER_RENDER_DECLARE_HAS_METHOD0(width);
BROWSER_RENDER_DECLARE_HAS_METHOD0(height);
BROWSER_RENDER_DECLARE_HAS_METHOD0(left);
BROWSER_RENDER_DECLARE_HAS_METHOD0(top);
BROWSER_RENDER_DECLARE_HAS_METHOD0(w);
BROWSER_RENDER_DECLARE_HAS_METHOD0(h);

BROWSER_RENDER_DECLARE_HAS_MEMBER(text);
BROWSER_RENDER_DECLARE_HAS_MEMBER(text_content);
BROWSER_RENDER_DECLARE_HAS_MEMBER(content);
BROWSER_RENDER_DECLARE_HAS_METHOD0(text);
BROWSER_RENDER_DECLARE_HAS_METHOD0(text_content);
BROWSER_RENDER_DECLARE_HAS_METHOD0(content);

BROWSER_RENDER_DECLARE_HAS_MEMBER(children);
BROWSER_RENDER_DECLARE_HAS_MEMBER(child_boxes);
BROWSER_RENDER_DECLARE_HAS_METHOD0(children);
BROWSER_RENDER_DECLARE_HAS_METHOD0(child_boxes);

#undef BROWSER_RENDER_DECLARE_HAS_MEMBER
#undef BROWSER_RENDER_DECLARE_HAS_METHOD0
#undef BROWSER_RENDER_DECLARE_HAS_METHOD1

template <typename Box>
std::string get_style_property(const Box& box, const std::string& key) {
  const std::string normalized_key = to_lower_ascii(trim_copy(key));

  if constexpr (has_method_style_value<Box>::value) {
    const std::string value = trim_copy(value_to_string(box.style_value(normalized_key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_get_style<Box>::value) {
    const std::string value = trim_copy(value_to_string(box.get_style(normalized_key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_get_property<Box>::value) {
    const std::string value = trim_copy(value_to_string(box.get_property(normalized_key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_property<Box>::value) {
    const std::string value = trim_copy(value_to_string(box.property(normalized_key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_style_key<Box>::value) {
    const std::string value = trim_copy(value_to_string(box.style(normalized_key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_style<Box>::value) {
    const auto& style = box.style;
    const std::string map_value = lookup_map_like(style, normalized_key);
    if (!map_value.empty()) {
      return map_value;
    }

    const std::string inline_style = value_to_string(style);
    const std::string inline_value = lookup_inline_style(inline_style, normalized_key);
    if (!inline_value.empty()) {
      return inline_value;
    }
  }

  if constexpr (has_member_styles<Box>::value) {
    const std::string value = lookup_map_like(box.styles, normalized_key);
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_computed_style<Box>::value) {
    const std::string value = lookup_map_like(box.computed_style, normalized_key);
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_computed_styles<Box>::value) {
    const std::string value = lookup_map_like(box.computed_styles, normalized_key);
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_properties<Box>::value) {
    const std::string value = lookup_map_like(box.properties, normalized_key);
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_css<Box>::value) {
    const std::string value = lookup_map_like(box.css, normalized_key);
    if (!value.empty()) {
      return value;
    }
  }

  return {};
}

template <typename T>
void assign_rect_fields(const T& source, Rect& out) {
  if constexpr (has_member_x<T>::value) {
    out.x = value_to_int(source.x);
  } else if constexpr (has_member_left<T>::value) {
    out.x = value_to_int(source.left);
  } else if constexpr (has_method_x<T>::value) {
    out.x = value_to_int(source.x());
  } else if constexpr (has_method_left<T>::value) {
    out.x = value_to_int(source.left());
  }

  if constexpr (has_member_y<T>::value) {
    out.y = value_to_int(source.y);
  } else if constexpr (has_member_top<T>::value) {
    out.y = value_to_int(source.top);
  } else if constexpr (has_method_y<T>::value) {
    out.y = value_to_int(source.y());
  } else if constexpr (has_method_top<T>::value) {
    out.y = value_to_int(source.top());
  }

  if constexpr (has_member_width<T>::value) {
    out.width = value_to_int(source.width);
  } else if constexpr (has_member_w<T>::value) {
    out.width = value_to_int(source.w);
  } else if constexpr (has_method_width<T>::value) {
    out.width = value_to_int(source.width());
  } else if constexpr (has_method_w<T>::value) {
    out.width = value_to_int(source.w());
  }

  if constexpr (has_member_height<T>::value) {
    out.height = value_to_int(source.height);
  } else if constexpr (has_member_h<T>::value) {
    out.height = value_to_int(source.h);
  } else if constexpr (has_method_height<T>::value) {
    out.height = value_to_int(source.height());
  } else if constexpr (has_method_h<T>::value) {
    out.height = value_to_int(source.h());
  }
}

template <typename Box>
Rect get_box_rect(const Box& box) {
  Rect rect{};

  if constexpr (has_member_rect<Box>::value) {
    assign_rect_fields(box.rect, rect);
  }
  if constexpr (has_method_rect<Box>::value) {
    assign_rect_fields(box.rect(), rect);
  }
  if constexpr (has_member_bounds<Box>::value) {
    assign_rect_fields(box.bounds, rect);
  }
  if constexpr (has_method_bounds<Box>::value) {
    assign_rect_fields(box.bounds(), rect);
  }
  if constexpr (has_member_frame<Box>::value) {
    assign_rect_fields(box.frame, rect);
  }
  if constexpr (has_method_frame<Box>::value) {
    assign_rect_fields(box.frame(), rect);
  }
  if constexpr (has_member_box<Box>::value) {
    assign_rect_fields(box.box, rect);
  }
  if constexpr (has_method_box<Box>::value) {
    assign_rect_fields(box.box(), rect);
  }

  assign_rect_fields(box, rect);
  return rect;
}

template <typename Box>
std::string get_box_text(const Box& box) {
  if constexpr (has_member_text<Box>::value) {
    const std::string value = value_to_string(box.text);
    if (!value.empty()) {
      return value;
    }
  }
  if constexpr (has_method_text<Box>::value) {
    const std::string value = value_to_string(box.text());
    if (!value.empty()) {
      return value;
    }
  }
  if constexpr (has_member_text_content<Box>::value) {
    const std::string value = value_to_string(box.text_content);
    if (!value.empty()) {
      return value;
    }
  }
  if constexpr (has_method_text_content<Box>::value) {
    const std::string value = value_to_string(box.text_content());
    if (!value.empty()) {
      return value;
    }
  }
  if constexpr (has_member_content<Box>::value) {
    const std::string value = value_to_string(box.content);
    if (!value.empty()) {
      return value;
    }
  }
  if constexpr (has_method_content<Box>::value) {
    const std::string value = value_to_string(box.content());
    if (!value.empty()) {
      return value;
    }
  }
  return {};
}

template <typename T, typename = void>
struct is_range : std::false_type {};

template <typename T>
struct is_range<T, std::void_t<decltype(std::begin(std::declval<const T&>())),
                               decltype(std::end(std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct has_get_method : std::false_type {};

template <typename T>
struct has_get_method<T, std::void_t<decltype(std::declval<const T&>().get())>> : std::true_type {};

template <typename Child>
const auto* get_child_pointer(const Child& child) {
  using Decayed = std::decay_t<Child>;

  if constexpr (std::is_pointer_v<Decayed>) {
    return child;
  } else if constexpr (has_get_method<Decayed>::value) {
    if constexpr (std::is_pointer_v<decltype(child.get())>) {
      return child.get();
    } else {
      return &child.get();
    }
  } else {
    return &child;
  }
}

template <typename Range, typename Fn>
void for_each_range_child(const Range& range, Fn&& fn) {
  for (const auto& entry : range) {
    const auto* child = get_child_pointer(entry);
    if (child != nullptr) {
      fn(*child);
    }
  }
}

template <typename Box, typename Fn>
void for_each_box_child(const Box& box, Fn&& fn) {
  if constexpr (has_member_children<Box>::value) {
    if constexpr (is_range<decltype(box.children)>::value) {
      for_each_range_child(box.children, std::forward<Fn>(fn));
      return;
    }
  }

  if constexpr (has_method_children<Box>::value) {
    const auto& children = box.children();
    if constexpr (is_range<decltype(children)>::value) {
      for_each_range_child(children, std::forward<Fn>(fn));
      return;
    }
  }

  if constexpr (has_member_child_boxes<Box>::value) {
    if constexpr (is_range<decltype(box.child_boxes)>::value) {
      for_each_range_child(box.child_boxes, std::forward<Fn>(fn));
      return;
    }
  }

  if constexpr (has_method_child_boxes<Box>::value) {
    const auto& children = box.child_boxes();
    if constexpr (is_range<decltype(children)>::value) {
      for_each_range_child(children, std::forward<Fn>(fn));
      return;
    }
  }
}

template <typename Box>
std::string get_box_background_value(const Box& box) {
  std::string value = get_style_property(box, "background-color");
  if (value.empty()) {
    value = get_style_property(box, "background");
  }
  return value;
}

template <typename Box>
bool try_resolve_box_background_color(const Box& box, Color& out) {
  return try_parse_paint_color(get_box_background_value(box), resolve_box_text_color(box), out);
}

const browser::layout::LayoutBox* find_first_layout_box_with_tag(
    const browser::layout::LayoutBox& box,
    const std::string& normalized_tag) {
  if (normalized_tag.empty()) {
    return nullptr;
  }

  if (to_lower_ascii(trim_copy(box.tag)) == normalized_tag) {
    return &box;
  }

  for (const browser::layout::LayoutBox& child : box.children) {
    const browser::layout::LayoutBox* match = find_first_layout_box_with_tag(child, normalized_tag);
    if (match != nullptr) {
      return match;
    }
  }

  return nullptr;
}

Color resolve_initial_canvas_color(const browser::layout::LayoutBox& root) {
  Color resolved{255, 255, 255};

  const browser::layout::LayoutBox* body_box = find_first_layout_box_with_tag(root, "body");
  if (body_box != nullptr && try_resolve_box_background_color(*body_box, resolved)) {
    return resolved;
  }

  if (try_resolve_box_background_color(root, resolved)) {
    return resolved;
  }

  return Color{255, 255, 255};
}

void draw_border(Canvas& canvas, const Rect& rect, int border_width, Color color) {
  if (border_width <= 0 || rect.width <= 0 || rect.height <= 0) {
    return;
  }

  const int clamped = std::min(border_width, std::max(1, std::min(rect.width, rect.height) / 2));
  canvas.fill_rect(rect.x, rect.y, rect.width, clamped, color);
  canvas.fill_rect(rect.x, rect.y + rect.height - clamped, rect.width, clamped, color);
  canvas.fill_rect(rect.x, rect.y, clamped, rect.height, color);
  canvas.fill_rect(rect.x + rect.width - clamped, rect.y, clamped, rect.height, color);
}

using Glyph = std::array<std::uint8_t, 7>;

const Glyph& fallback_glyph() {
  static const Glyph glyph = {
      0b11111,
      0b00001,
      0b00110,
      0b00100,
      0b00000,
      0b00100,
      0b00000,
  };
  return glyph;
}

const Glyph& glyph_for_char(char c) {
  static const std::map<char, Glyph> kGlyphs = {
      {' ', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}},
      {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100}},
      {'"', {0b01010, 0b01010, 0b01010, 0b00000, 0b00000, 0b00000, 0b00000}},
      {'#', {0b01010, 0b11111, 0b01010, 0b01010, 0b11111, 0b01010, 0b00000}},
      {'$', {0b00100, 0b01111, 0b10100, 0b01110, 0b00101, 0b11110, 0b00100}},
      {'%', {0b11001, 0b11010, 0b00100, 0b01000, 0b10110, 0b00110, 0b00000}},
      {'&', {0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101}},
      {'\'', {0b00110, 0b00100, 0b01000, 0b00000, 0b00000, 0b00000, 0b00000}},
      {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
      {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
      {'*', {0b00000, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00000}},
      {'+', {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}},
      {',', {0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00100, 0b01000}},
      {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
      {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00110}},
      {'/', {0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b00000, 0b00000}},
      {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
      {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
      {'2', {0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111}},
      {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
      {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
      {'5', {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}},
      {'6', {0b00111, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
      {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
      {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
      {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b11100}},
      {':', {0b00000, 0b00110, 0b00110, 0b00000, 0b00110, 0b00110, 0b00000}},
      {';', {0b00000, 0b00110, 0b00110, 0b00000, 0b00110, 0b00100, 0b01000}},
      {'<', {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}},
      {'=', {0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000}},
      {'>', {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}},
      {'?', {0b01110, 0b10001, 0b00001, 0b00110, 0b00100, 0b00000, 0b00100}},
      {'@', {0b01110, 0b10001, 0b10111, 0b10101, 0b10111, 0b10000, 0b01110}},
      {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
      {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
      {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
      {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
      {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
      {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
      {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}},
      {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
      {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
      {'J', {0b00001, 0b00001, 0b00001, 0b00001, 0b10001, 0b10001, 0b01110}},
      {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
      {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
      {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
      {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
      {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
      {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
      {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
      {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
      {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
      {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
      {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
      {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
      {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}},
      {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
      {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
      {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
      {'[', {0b01110, 0b01000, 0b01000, 0b01000, 0b01000, 0b01000, 0b01110}},
      {'\\', {0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00000, 0b00000}},
      {']', {0b01110, 0b00010, 0b00010, 0b00010, 0b00010, 0b00010, 0b01110}},
      {'^', {0b00100, 0b01010, 0b10001, 0b00000, 0b00000, 0b00000, 0b00000}},
      {'_', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111}},
      {'`', {0b01000, 0b00100, 0b00010, 0b00000, 0b00000, 0b00000, 0b00000}},
      {'{', {0b00010, 0b00100, 0b00100, 0b01000, 0b00100, 0b00100, 0b00010}},
      {'|', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
      {'}', {0b01000, 0b00100, 0b00100, 0b00010, 0b00100, 0b00100, 0b01000}},
      {'~', {0b01001, 0b10110, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}},
  };

  unsigned char uc = static_cast<unsigned char>(c);
  if (uc >= 'a' && uc <= 'z') {
    c = static_cast<char>(uc - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
  }

  const auto it = kGlyphs.find(c);
  if (it != kGlyphs.end()) {
    return it->second;
  }
  return fallback_glyph();
}

void draw_glyph(Canvas& canvas, int x, int y, char c, Color color) {
  const Glyph& glyph = glyph_for_char(c);
  for (int row = 0; row < 7; ++row) {
    const std::uint8_t bits = glyph[row];
    for (int col = 0; col < 5; ++col) {
      const std::uint8_t mask = static_cast<std::uint8_t>(1U << (4 - col));
      if ((bits & mask) != 0U) {
        canvas.set_pixel(x + col, y + row, color);
      }
    }
  }
}

void draw_text(Canvas& canvas, int x, int y, const std::string& text, Color color) {
  int cursor_x = x;
  int cursor_y = y;

  for (char c : text) {
    if (c == '\n') {
      cursor_x = x;
      cursor_y += 8;
      continue;
    }
    if (c == '\r') {
      continue;
    }

    draw_glyph(canvas, cursor_x, cursor_y, c, color);
    cursor_x += 6;
  }
}

template <typename Box>
bool subtree_has_positive_area(const Box& box,
                               std::unordered_map<const void*, bool>& cache) {
  const void* key = static_cast<const void*>(&box);
  const auto cache_it = cache.find(key);
  if (cache_it != cache.end()) {
    return cache_it->second;
  }

  const Rect rect = get_box_rect(box);
  if (rect.width > 0 && rect.height > 0) {
    cache.emplace(key, true);
    return true;
  }

  bool has_positive_descendant = false;
  for_each_box_child(box, [&](const auto& child) {
    if (!has_positive_descendant &&
        subtree_has_positive_area(child, cache)) {
      has_positive_descendant = true;
    }
  });

  cache.emplace(key, has_positive_descendant);
  return has_positive_descendant;
}

template <typename Box>
void paint_box_tree_impl(const Box& box,
                         Canvas& canvas,
                         std::unordered_map<const void*, bool>& subtree_positive_area_cache) {
  if (!subtree_has_positive_area(box, subtree_positive_area_cache)) {
    return;
  }

  const Rect rect = get_box_rect(box);
  const bool has_positive_area = rect.width > 0 && rect.height > 0;

  if (has_positive_area) {
    const Color text_color = resolve_box_text_color(box);
    const std::string background_value = get_box_background_value(box);

    Color background_color{};
    if (try_parse_paint_color(background_value, text_color, background_color)) {
      canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, background_color);
    }

    const std::string border_width_value = get_style_property(box, "border-width");
    const std::string border_value = get_style_property(box, "border");
    const int border_width = parse_border_width(border_width_value, border_value);

    Color border_color{};
    if (border_width > 0 &&
        parse_border_color(get_style_property(box, "border-color"), border_value, text_color, border_color)) {
      draw_border(canvas, rect, border_width, border_color);
    }

    const std::string text = get_box_text(box);
    if (!text.empty()) {
      const int inset = std::max(1, border_width);
      draw_text(canvas, rect.x + inset + 1, rect.y + inset + 1, text, text_color);
    }
  }

  for_each_box_child(box, [&](const auto& child) {
    if (subtree_has_positive_area(child, subtree_positive_area_cache)) {
      paint_box_tree_impl(child, canvas, subtree_positive_area_cache);
    }
  });
}

template <typename Box>
void paint_box_tree(const Box& box, Canvas& canvas) {
  std::unordered_map<const void*, bool> subtree_positive_area_cache;
  paint_box_tree_impl(box, canvas, subtree_positive_area_cache);
}

}  // namespace

Canvas render_to_canvas(const browser::layout::LayoutBox& root, int width, int height) {
  Canvas canvas(width, height);
  canvas.clear(resolve_initial_canvas_color(root));
  paint_box_tree(root, canvas);

  return canvas;
}

bool write_ppm(const Canvas& canvas, const std::string& path) {
  if (canvas.width() <= 0 || canvas.height() <= 0 || path.empty()) {
    return false;
  }

  std::ofstream output(path, std::ios::binary);
  if (!output.is_open()) {
    return false;
  }

  output << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";

  const auto& pixels = canvas.pixels();
  if (!pixels.empty()) {
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
  }

  return output.good();
}

namespace {

void collect_text_lines(const browser::layout::LayoutBox& box,
                        std::vector<std::string>& lines,
                        int indent) {
    std::string prefix(static_cast<std::size_t>(indent * 2), ' ');

    if (!box.text.empty()) {
        lines.push_back(prefix + box.text);
    } else if (!box.tag.empty()) {
        lines.push_back(prefix + "<" + box.tag + ">");
    }

    for (const auto& child : box.children) {
        collect_text_lines(child, lines, indent + 1);
    }
}

}  // namespace

std::string render_to_text(const browser::layout::LayoutBox& root, int width) {
    (void)width;
    std::vector<std::string> lines;
    collect_text_lines(root, lines, 0);

    std::string output;
    for (const auto& line : lines) {
        if (!output.empty()) {
            output += '\n';
        }
        output += line;
    }
    return output;
}

Canvas render_to_canvas(const browser::layout::LayoutBox& root, int width, int height,
                        RenderMetadata& metadata) {
    auto start = std::chrono::steady_clock::now();

    Canvas canvas = render_to_canvas(root, width, height);

    auto end = std::chrono::steady_clock::now();
    metadata.width = canvas.width();
    metadata.height = canvas.height();
    metadata.pixel_count = static_cast<std::size_t>(canvas.width()) *
                           static_cast<std::size_t>(canvas.height());
    metadata.byte_count = canvas.pixels().size();
    metadata.render_duration_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    return canvas;
}

bool write_render_metadata(const RenderMetadata& metadata, const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    out << "width=" << metadata.width << "\n";
    out << "height=" << metadata.height << "\n";
    out << "pixel_count=" << metadata.pixel_count << "\n";
    out << "byte_count=" << metadata.byte_count << "\n";
    out << "render_duration_ms=" << metadata.render_duration_ms << "\n";

    return out.good();
}

const char* render_stage_name(RenderStage stage) {
    switch (stage) {
        case RenderStage::CanvasInit: return "CanvasInit";
        case RenderStage::BackgroundResolve: return "BackgroundResolve";
        case RenderStage::Paint: return "Paint";
        case RenderStage::Complete: return "Complete";
    }
    return "Unknown";
}

void RenderTrace::record(RenderStage stage) {
    RenderTraceEntry entry;
    entry.stage = stage;
    entry.entered_at = std::chrono::steady_clock::now();

    if (!entries.empty()) {
        auto prev = entries.back().entered_at;
        entry.elapsed_since_prev_ms =
            std::chrono::duration<double, std::milli>(entry.entered_at - prev).count();
    }

    entries.push_back(entry);
}

bool RenderTrace::is_reproducible_with(const RenderTrace& other,
                                        double tolerance_factor) const {
    if (entries.size() != other.entries.size()) {
        return false;
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].stage != other.entries[i].stage) {
            return false;
        }
    }

    for (std::size_t i = 1; i < entries.size(); ++i) {
        double a = entries[i].elapsed_since_prev_ms;
        double b = other.entries[i].elapsed_since_prev_ms;
        double avg = (a + b) / 2.0;
        double diff = std::abs(a - b);
        double tolerance = std::max(avg * tolerance_factor, 50.0);
        if (diff > tolerance) {
            return false;
        }
    }

    return true;
}

Canvas render_to_canvas_traced(const browser::layout::LayoutBox& root, int width, int height,
                               RenderTrace& trace) {
    trace.record(RenderStage::CanvasInit);
    Canvas canvas(width, height);

    trace.record(RenderStage::BackgroundResolve);
    canvas.clear(resolve_initial_canvas_color(root));

    trace.record(RenderStage::Paint);
    paint_box_tree(root, canvas);

    trace.record(RenderStage::Complete);
    return canvas;
}

Canvas render_to_canvas_traced(const browser::layout::LayoutBox& root, int width, int height,
                               RenderMetadata& metadata, RenderTrace& trace) {
    auto start = std::chrono::steady_clock::now();

    Canvas canvas = render_to_canvas_traced(root, width, height, trace);

    auto end = std::chrono::steady_clock::now();
    metadata.width = canvas.width();
    metadata.height = canvas.height();
    metadata.pixel_count = static_cast<std::size_t>(canvas.width()) *
                           static_cast<std::size_t>(canvas.height());
    metadata.byte_count = canvas.pixels().size();
    metadata.render_duration_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    return canvas;
}

bool write_render_trace(const RenderTrace& trace, const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    for (std::size_t i = 0; i < trace.entries.size(); ++i) {
        const auto& entry = trace.entries[i];
        out << "stage=" << render_stage_name(entry.stage)
            << " elapsed_ms=" << entry.elapsed_since_prev_ms << "\n";
    }

    return out.good();
}

}  // namespace browser::render
