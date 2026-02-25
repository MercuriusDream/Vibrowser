#include "browser/browser.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"
#include "browser/html/html_parser.h"
#include "browser/js/runtime.h"
#include "browser/layout/layout_engine.h"
#include "browser/net/http_client.h"
#include "browser/net/url.h"
#include "browser/render/renderer.h"

namespace browser::app {
namespace {

struct TextLoadResult {
  bool ok = false;
  std::string text;
  std::string final_url;
  std::string error;
  double total_duration_seconds = 0.0;
  bool timed_out = false;
  std::string fetch_diagnostic;
};

using TextResourceCache = std::unordered_map<std::string, TextLoadResult>;
constexpr double kSlowHttpFetchThresholdSeconds = 2.0;

RunResult make_error(std::string message) {
  RunResult result;
  result.ok = false;
  result.message = std::move(message);
  return result;
}

std::string format_duration_seconds(double seconds) {
  if (seconds <= 0.0) {
    return "0";
  }

  const long total_milliseconds = static_cast<long>((seconds * 1000.0) + 0.5);
  const long whole_seconds = total_milliseconds / 1000;
  long fractional_milliseconds = total_milliseconds % 1000;
  if (fractional_milliseconds == 0) {
    return std::to_string(whole_seconds);
  }

  std::string fractional = std::to_string(fractional_milliseconds);
  while (fractional.size() < 3) {
    fractional.insert(fractional.begin(), '0');
  }
  while (!fractional.empty() && fractional.back() == '0') {
    fractional.pop_back();
  }

  return std::to_string(whole_seconds) + "." + fractional;
}

std::string make_fetch_timing_diagnostic(const std::string& url,
                                         double total_duration_seconds,
                                         bool timed_out) {
  if (timed_out) {
    std::string diagnostic = "HTTP fetch timed out";
    if (total_duration_seconds > 0.0) {
      diagnostic += " after " + format_duration_seconds(total_duration_seconds) +
                    "s";
    }
    diagnostic += ": " + url;
    return diagnostic;
  }

  if (total_duration_seconds >= kSlowHttpFetchThresholdSeconds) {
    return "Slow HTTP fetch (" + format_duration_seconds(total_duration_seconds) +
           "s): " + url;
  }

  return {};
}

void append_fetch_diagnostic_warning(const TextLoadResult& resource,
                                     std::vector<std::string>& warnings) {
  if (!resource.fetch_diagnostic.empty()) {
    warnings.push_back(resource.fetch_diagnostic);
  }
}

std::string trim_copy(std::string_view value) {
  std::size_t first = 0;
  while (first < value.size() &&
         std::isspace(static_cast<unsigned char>(value[first])) != 0) {
    ++first;
  }

  std::size_t last = value.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
    --last;
  }

  return std::string(value.substr(first, last - first));
}

std::string to_lower_ascii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool is_hex_digit(char ch) {
  return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

int from_hex(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

int base64_value(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return 26 + (ch - 'a');
  }
  if (ch >= '0' && ch <= '9') {
    return 52 + (ch - '0');
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  return -1;
}

bool starts_with_data_scheme(std::string_view value) {
  if (value.size() < 5) {
    return false;
  }

  return std::tolower(static_cast<unsigned char>(value[0])) == 'd' &&
         std::tolower(static_cast<unsigned char>(value[1])) == 'a' &&
         std::tolower(static_cast<unsigned char>(value[2])) == 't' &&
         std::tolower(static_cast<unsigned char>(value[3])) == 'a' &&
         value[4] == ':';
}

bool percent_decode_data_payload(std::string_view payload,
                                 std::string& decoded,
                                 std::string& err) {
  decoded.clear();
  err.clear();

  for (std::size_t i = 0; i < payload.size(); ++i) {
    const char ch = payload[i];
    if (ch != '%') {
      decoded.push_back(ch);
      continue;
    }

    if (i + 2 >= payload.size() || !is_hex_digit(payload[i + 1]) ||
        !is_hex_digit(payload[i + 2])) {
      err = "Malformed data URL: invalid percent-encoding in payload";
      return false;
    }

    const int high = from_hex(payload[i + 1]);
    const int low = from_hex(payload[i + 2]);
    decoded.push_back(static_cast<char>((high << 4) | low));
    i += 2;
  }

  return true;
}

bool base64_decode_data_payload(std::string_view payload,
                                std::string& decoded,
                                std::string& err) {
  decoded.clear();
  err.clear();

  if (payload.size() % 4 != 0) {
    err = "Malformed data URL: invalid base64 payload";
    return false;
  }

  decoded.reserve((payload.size() / 4) * 3);
  for (std::size_t i = 0; i < payload.size(); i += 4) {
    const char c0 = payload[i];
    const char c1 = payload[i + 1];
    const char c2 = payload[i + 2];
    const char c3 = payload[i + 3];

    const int v0 = base64_value(c0);
    const int v1 = base64_value(c1);
    if (v0 < 0 || v1 < 0) {
      err = "Malformed data URL: invalid base64 payload";
      return false;
    }

    if (c2 == '=') {
      if (c3 != '=' || i + 4 != payload.size()) {
        err = "Malformed data URL: invalid base64 payload";
        return false;
      }
      decoded.push_back(static_cast<char>((v0 << 2) | (v1 >> 4)));
      continue;
    }

    const int v2 = base64_value(c2);
    if (v2 < 0) {
      err = "Malformed data URL: invalid base64 payload";
      return false;
    }

    decoded.push_back(static_cast<char>((v0 << 2) | (v1 >> 4)));
    decoded.push_back(static_cast<char>(((v1 & 0x0f) << 4) | (v2 >> 2)));

    if (c3 == '=') {
      if (i + 4 != payload.size()) {
        err = "Malformed data URL: invalid base64 payload";
        return false;
      }
      continue;
    }

    const int v3 = base64_value(c3);
    if (v3 < 0) {
      err = "Malformed data URL: invalid base64 payload";
      return false;
    }

    decoded.push_back(static_cast<char>(((v2 & 0x03) << 6) | v3));
  }

  return true;
}

bool is_supported_data_media_type(const std::string& media_type) {
  return media_type == "text/plain" || media_type == "text/css" ||
         media_type == "text/html" ||
         media_type == "application/javascript";
}

bool parse_data_text_url(const std::string& url,
                         std::string& text,
                         std::string& err) {
  text.clear();
  err.clear();

  if (!starts_with_data_scheme(url)) {
    err = "URL is not a data URL";
    return false;
  }

  constexpr std::size_t kDataPrefixLen = 5;
  const std::size_t comma_pos = url.find(',', kDataPrefixLen);
  if (comma_pos == std::string::npos) {
    err = "Malformed data URL: missing ',' separator";
    return false;
  }

  const std::string metadata = url.substr(kDataPrefixLen, comma_pos - kDataPrefixLen);
  const std::size_t param_pos = metadata.find(';');
  const std::string media_type =
      to_lower_ascii(trim_copy(metadata.substr(0, param_pos)));
  if (media_type.empty()) {
    err = "Malformed data URL: missing media type";
    return false;
  }

  if (!is_supported_data_media_type(media_type)) {
    err = "Unsupported data URL media type: " + media_type;
    return false;
  }

  bool uses_base64_payload = false;
  if (param_pos != std::string::npos) {
    std::size_t cursor = param_pos + 1;
    while (cursor <= metadata.size()) {
      const std::size_t next = metadata.find(';', cursor);
      const std::string parameter = to_lower_ascii(
          trim_copy(metadata.substr(
              cursor, next == std::string::npos ? std::string::npos
                                                : next - cursor)));
      if (parameter == "base64") {
        uses_base64_payload = true;
      }

      if (next == std::string::npos) {
        break;
      }
      cursor = next + 1;
    }
  }

  if (uses_base64_payload) {
    return base64_decode_data_payload(url.substr(comma_pos + 1), text, err);
  }

  return percent_decode_data_payload(url.substr(comma_pos + 1), text, err);
}

bool is_rel_separator(char ch) {
  return ch == ',' || std::isspace(static_cast<unsigned char>(ch)) != 0;
}

bool is_stylesheet_rel(std::string_view rel_value) {
  const std::string lower_rel = to_lower_ascii(trim_copy(rel_value));
  std::size_t pos = 0;
  while (pos < lower_rel.size()) {
    while (pos < lower_rel.size() && is_rel_separator(lower_rel[pos])) {
      ++pos;
    }

    const std::size_t begin = pos;
    while (pos < lower_rel.size() && !is_rel_separator(lower_rel[pos])) {
      ++pos;
    }

    if (begin < pos && lower_rel.compare(begin, pos - begin, "stylesheet") == 0) {
      return true;
    }
  }

  return false;
}

bool is_javascript_script_type(const browser::html::Node& script_node) {
  const auto type_it = script_node.attributes.find("type");
  if (type_it == script_node.attributes.end()) {
    return true;
  }

  const std::string lowered_type = to_lower_ascii(trim_copy(type_it->second));
  return lowered_type.empty() || lowered_type == "text/javascript" ||
         lowered_type == "application/javascript" || lowered_type == "module";
}

bool is_css_style_type(const browser::html::Node& style_node) {
  const auto type_it = style_node.attributes.find("type");
  if (type_it == style_node.attributes.end()) {
    return true;
  }

  const std::string lowered_type = to_lower_ascii(trim_copy(type_it->second));
  return lowered_type.empty() || lowered_type == "text/css";
}

bool is_css_stylesheet_link_type(const browser::html::Node& link_node) {
  const auto type_it = link_node.attributes.find("type");
  if (type_it == link_node.attributes.end()) {
    return true;
  }

  const std::string lowered_type = to_lower_ascii(trim_copy(type_it->second));
  return lowered_type.empty() || lowered_type == "text/css";
}

bool is_media_token_char(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' ||
         ch == '_';
}

bool media_value_contains_token(std::string_view media_value,
                                std::string_view token) {
  if (media_value.empty() || token.empty()) {
    return false;
  }

  std::size_t pos = 0;
  while (pos < media_value.size()) {
    pos = media_value.find(token, pos);
    if (pos == std::string::npos) {
      break;
    }

    const std::size_t token_end = pos + token.size();
    const bool left_boundary =
        pos == 0 || !is_media_token_char(media_value[pos - 1]);
    const bool right_boundary =
        token_end >= media_value.size() ||
        !is_media_token_char(media_value[token_end]);
    if (left_boundary && right_boundary) {
      return true;
    }

    pos = token_end;
  }

  return false;
}

bool is_stylesheet_media_supported(const browser::html::Node& link_node) {
  const auto media_it = link_node.attributes.find("media");
  if (media_it == link_node.attributes.end()) {
    return true;
  }

  const std::string lowered_media = to_lower_ascii(trim_copy(media_it->second));
  if (lowered_media.empty()) {
    return true;
  }

  return media_value_contains_token(lowered_media, "all") ||
         media_value_contains_token(lowered_media, "screen");
}

std::filesystem::path normalize_file_path(const std::filesystem::path& path) {
  std::filesystem::path normalized = path;
  if (normalized.is_relative()) {
    normalized = std::filesystem::absolute(normalized);
  }
  return normalized.lexically_normal();
}

bool read_text_file(const std::filesystem::path& path,
                    std::string& out_text,
                    std::string& err) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    err = "Unable to open file: " + path.string();
    return false;
  }

  std::ostringstream stream;
  stream << file.rdbuf();
  if (!file.good() && !file.eof()) {
    err = "Failed to read file: " + path.string();
    return false;
  }

  out_text = stream.str();
  return true;
}

std::string to_normalized_file_url(const std::string& file_url, std::string& err) {
  std::string path;
  if (!browser::net::file_url_to_path(file_url, path, err)) {
    return {};
  }

  const std::filesystem::path normalized = normalize_file_path(path);
  return browser::net::path_to_file_url(normalized.generic_string());
}

bool has_url_authority_delimiter(std::string_view value) {
  return value.find("://") != std::string_view::npos;
}

bool try_local_path_to_file_url(const std::string& input,
                                std::string& file_url,
                                std::string& err) {
  file_url.clear();
  err.clear();

  std::error_code fs_error;
  const std::filesystem::path candidate(input);
  if (!std::filesystem::exists(candidate, fs_error)) {
    if (fs_error) {
      err = "Failed to inspect local path '" + input + "': " + fs_error.message();
      return false;
    }
    err = "Local path does not exist: " + input;
    return false;
  }

  const std::filesystem::path normalized = normalize_file_path(candidate);
  file_url = browser::net::path_to_file_url(normalized.generic_string());
  return true;
}

bool canonicalize_load_target_url(const std::string& input,
                                  std::string& canonical_url,
                                  std::string& err) {
  canonical_url.clear();
  err.clear();

  if (browser::net::is_file_url(input)) {
    canonical_url = to_normalized_file_url(input, err);
    return !canonical_url.empty();
  }

  if (starts_with_data_scheme(input)) {
    canonical_url = input;
    return true;
  }

  browser::net::Url parsed_url;
  std::string parse_error;
  if (browser::net::parse_url(input, parsed_url, parse_error)) {
    canonical_url = parsed_url.to_string();
    return true;
  }

  std::string local_file_url;
  std::string path_error;
  if (try_local_path_to_file_url(input, local_file_url, path_error)) {
    canonical_url = std::move(local_file_url);
    return true;
  }

  if (!has_url_authority_delimiter(input) && !path_error.empty()) {
    err = path_error;
    return false;
  }

  err = parse_error.empty() ? path_error : parse_error;
  return false;
}

bool resolve_resource_url(const std::string& base_url,
                          const std::string& raw_reference,
                          std::string& resolved_url,
                          std::string& err) {
  resolved_url.clear();
  err.clear();

  const std::string reference = trim_copy(raw_reference);
  if (reference.empty()) {
    err = "Resource URL is empty";
    return false;
  }

  resolved_url = browser::net::resolve_url(base_url, reference, err);
  if (resolved_url.empty()) {
    if (err.empty()) {
      err = "Failed to resolve resource URL";
    }
    return false;
  }

  if (browser::net::is_file_url(resolved_url)) {
    const std::string normalized_url = to_normalized_file_url(resolved_url, err);
    if (normalized_url.empty()) {
      return false;
    }
    resolved_url = normalized_url;
    return true;
  }

  if (starts_with_data_scheme(resolved_url)) {
    return true;
  }

  browser::net::Url parsed_url;
  std::string parse_error;
  if (!browser::net::parse_url(resolved_url, parsed_url, parse_error)) {
    err = parse_error.empty() ? ("Unsupported resolved URL: " + resolved_url) : parse_error;
    return false;
  }

  resolved_url = parsed_url.to_string();
  return true;
}

bool resolve_base_href_url(const std::string& document_url,
                           const std::string& raw_base_href,
                           std::string& resolved_base_url,
                           std::string& err) {
  resolved_base_url.clear();
  err.clear();

  const std::string base_href = trim_copy(raw_base_href);
  if (base_href.empty()) {
    err = "Base href is empty";
    return false;
  }

  resolved_base_url = browser::net::resolve_url(document_url, base_href, err);
  if (resolved_base_url.empty()) {
    if (err.empty()) {
      err = "Failed to resolve base href";
    }
    return false;
  }

  if (browser::net::is_file_url(resolved_base_url)) {
    std::string file_path;
    return browser::net::file_url_to_path(resolved_base_url, file_path, err);
  }

  if (starts_with_data_scheme(resolved_base_url)) {
    err = "Unsupported base URL scheme: data";
    return false;
  }

  browser::net::Url parsed_url;
  std::string parse_error;
  if (!browser::net::parse_url(resolved_base_url, parsed_url, parse_error)) {
    err = parse_error.empty() ? ("Unsupported resolved base URL: " + resolved_base_url)
                              : parse_error;
    return false;
  }

  resolved_base_url = parsed_url.to_string();
  return true;
}

TextLoadResult load_text_resource(const std::string& url) {
  TextLoadResult result;
  std::string normalized_url;
  if (!canonicalize_load_target_url(url, normalized_url, result.error)) {
    return result;
  }

  if (browser::net::is_file_url(normalized_url)) {
    std::string file_path;
    if (!browser::net::file_url_to_path(normalized_url, file_path, result.error)) {
      return result;
    }

    const std::filesystem::path normalized_path = normalize_file_path(file_path);
    if (!read_text_file(normalized_path, result.text, result.error)) {
      return result;
    }

    result.ok = true;
    result.final_url = browser::net::path_to_file_url(normalized_path.generic_string());
    return result;
  }

  if (starts_with_data_scheme(normalized_url)) {
    if (!parse_data_text_url(normalized_url, result.text, result.error)) {
      return result;
    }
    result.ok = true;
    result.final_url = normalized_url;
    return result;
  }

  browser::net::Url parsed_url;
  std::string parse_error;
  if (!browser::net::parse_url(normalized_url, parsed_url, parse_error)) {
    result.error = parse_error;
    return result;
  }

  const browser::net::Response response =
      browser::net::fetch(parsed_url.to_string());
  result.total_duration_seconds = response.total_duration_seconds;
  result.timed_out = response.timed_out;
  const std::string fetch_diagnostic_url =
      response.final_url.empty() ? parsed_url.to_string() : response.final_url;
  result.fetch_diagnostic =
      make_fetch_timing_diagnostic(fetch_diagnostic_url,
                                   result.total_duration_seconds,
                                   result.timed_out);
  if (!response.error.empty()) {
    result.error = "Fetch failed: " + response.error;
    if (!result.fetch_diagnostic.empty()) {
      result.error += " [" + result.fetch_diagnostic + "]";
    }
    return result;
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    result.error = "HTTP status " + std::to_string(response.status_code) +
                   " " + response.reason;
    if (!result.fetch_diagnostic.empty()) {
      result.error += " [" + result.fetch_diagnostic + "]";
    }
    return result;
  }

  result.ok = true;
  result.text = response.body;
  result.final_url = response.final_url.empty() ? parsed_url.to_string()
                                                : response.final_url;
  return result;
}

std::string canonical_resource_url(const std::string& url) {
  std::string canonical_url;
  std::string err;
  if (canonicalize_load_target_url(url, canonical_url, err)) {
    return canonical_url;
  }
  return url;
}

const TextLoadResult& load_text_resource_cached(const std::string& resolved_url,
                                                TextResourceCache& cache) {
  const std::string cache_key = canonical_resource_url(resolved_url);
  const auto cached_it = cache.find(cache_key);
  if (cached_it != cache.end()) {
    return cached_it->second;
  }

  TextLoadResult loaded_resource = load_text_resource(cache_key);
  const auto inserted =
      cache.emplace(cache_key, std::move(loaded_resource));
  return inserted.first->second;
}

std::string collect_node_text_content(const browser::html::Node& node) {
  std::string content;
  for (const std::unique_ptr<browser::html::Node>& child : node.children) {
    if (!child) {
      continue;
    }

    if (child->type == browser::html::NodeType::Text) {
      content += child->text_content;
    } else {
      content += browser::html::inner_text(*child);
    }
  }
  return content;
}

bool is_within_head_or_body(const browser::html::Node& node) {
  const browser::html::Node* current = node.parent;
  while (current != nullptr) {
    if (current->type == browser::html::NodeType::Element) {
      const std::string tag = to_lower_ascii(current->tag_name);
      if (tag == "head" || tag == "body") {
        return true;
      }
    }
    current = current->parent;
  }
  return false;
}

std::string resolve_resource_base_url(const browser::html::Node& document,
                                      const std::string& document_url,
                                      std::vector<std::string>& warnings) {
  const std::vector<const browser::html::Node*> base_nodes =
      browser::html::query_all_by_tag(document, "base");
  for (const browser::html::Node* base_node : base_nodes) {
    if (base_node == nullptr || !is_within_head_or_body(*base_node)) {
      continue;
    }

    const auto href_it = base_node->attributes.find("href");
    if (href_it == base_node->attributes.end()) {
      continue;
    }

    std::string resolved_base_url;
    std::string resolve_error;
    if (!resolve_base_href_url(document_url, href_it->second,
                              resolved_base_url, resolve_error)) {
      warnings.push_back("Base href ignored for resource resolution ('" +
                         href_it->second + "'): " + resolve_error);
      return document_url;
    }

    return resolved_base_url;
  }

  return document_url;
}

void append_text_block(std::string& destination, const std::string& block_text) {
  if (block_text.empty()) {
    return;
  }
  if (!destination.empty()) {
    destination.push_back('\n');
  }
  destination += block_text;
}

bool is_css_identifier_char(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_';
}

bool starts_with_ascii_case_insensitive(std::string_view text,
                                        std::size_t pos,
                                        std::string_view expected) {
  if (pos > text.size() || text.size() - pos < expected.size()) {
    return false;
  }

  for (std::size_t i = 0; i < expected.size(); ++i) {
    const unsigned char lhs = static_cast<unsigned char>(text[pos + i]);
    const unsigned char rhs = static_cast<unsigned char>(expected[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }

  return true;
}

void skip_css_whitespace_and_comments(std::string_view css, std::size_t& cursor) {
  while (cursor < css.size()) {
    if (std::isspace(static_cast<unsigned char>(css[cursor])) != 0) {
      ++cursor;
      continue;
    }

    if (cursor + 1 < css.size() && css[cursor] == '/' && css[cursor + 1] == '*') {
      const std::size_t close_pos = css.find("*/", cursor + 2);
      if (close_pos == std::string_view::npos) {
        cursor = css.size();
        return;
      }
      cursor = close_pos + 2;
      continue;
    }

    break;
  }
}

bool parse_css_string_token(std::string_view css,
                            std::size_t& cursor,
                            std::string& parsed_value) {
  if (cursor >= css.size() || (css[cursor] != '"' && css[cursor] != '\'')) {
    return false;
  }

  const char quote = css[cursor];
  ++cursor;
  parsed_value.clear();

  while (cursor < css.size()) {
    const char ch = css[cursor];
    ++cursor;

    if (ch == quote) {
      return true;
    }

    if (ch == '\\') {
      if (cursor >= css.size()) {
        return false;
      }

      const char escaped = css[cursor];
      ++cursor;

      if (escaped == '\n') {
        continue;
      }
      if (escaped == '\r') {
        if (cursor < css.size() && css[cursor] == '\n') {
          ++cursor;
        }
        continue;
      }

      parsed_value.push_back(escaped);
      continue;
    }

    parsed_value.push_back(ch);
  }

  return false;
}

bool parse_css_url_function_reference(std::string_view css,
                                      std::size_t& cursor,
                                      std::string& reference) {
  std::size_t local_cursor = cursor;
  if (!starts_with_ascii_case_insensitive(css, local_cursor, "url")) {
    return false;
  }
  local_cursor += 3;

  skip_css_whitespace_and_comments(css, local_cursor);
  if (local_cursor >= css.size() || css[local_cursor] != '(') {
    return false;
  }
  ++local_cursor;

  skip_css_whitespace_and_comments(css, local_cursor);
  if (local_cursor >= css.size()) {
    return false;
  }

  if (css[local_cursor] == '"' || css[local_cursor] == '\'') {
    if (!parse_css_string_token(css, local_cursor, reference)) {
      return false;
    }
    skip_css_whitespace_and_comments(css, local_cursor);
    if (local_cursor >= css.size() || css[local_cursor] != ')') {
      return false;
    }
    ++local_cursor;
    cursor = local_cursor;
    return !reference.empty();
  }

  std::string raw_reference;
  while (local_cursor < css.size()) {
    const char ch = css[local_cursor];
    if (ch == ')') {
      break;
    }

    if (ch == '\\') {
      ++local_cursor;
      if (local_cursor >= css.size()) {
        return false;
      }
      raw_reference.push_back(css[local_cursor]);
      ++local_cursor;
      continue;
    }

    raw_reference.push_back(ch);
    ++local_cursor;
  }

  if (local_cursor >= css.size() || css[local_cursor] != ')') {
    return false;
  }
  ++local_cursor;

  reference = trim_copy(raw_reference);
  if (reference.empty()) {
    return false;
  }

  cursor = local_cursor;
  return true;
}

bool find_css_statement_end(std::string_view css,
                            std::size_t cursor,
                            std::size_t& statement_end) {
  int paren_depth = 0;
  while (cursor < css.size()) {
    if (cursor + 1 < css.size() && css[cursor] == '/' && css[cursor + 1] == '*') {
      const std::size_t close_pos = css.find("*/", cursor + 2);
      if (close_pos == std::string_view::npos) {
        statement_end = css.size();
        return true;
      }
      cursor = close_pos + 2;
      continue;
    }

    if (css[cursor] == '"' || css[cursor] == '\'') {
      std::string ignored;
      if (!parse_css_string_token(css, cursor, ignored)) {
        statement_end = css.size();
        return true;
      }
      continue;
    }

    const char ch = css[cursor];
    if (ch == '(') {
      ++paren_depth;
      ++cursor;
      continue;
    }
    if (ch == ')') {
      if (paren_depth > 0) {
        --paren_depth;
      }
      ++cursor;
      continue;
    }

    if (ch == ';' && paren_depth == 0) {
      statement_end = cursor + 1;
      return true;
    }

    if (ch == '{' && paren_depth == 0) {
      return false;
    }

    ++cursor;
  }

  statement_end = css.size();
  return true;
}

struct CssImportRule {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::string reference;
};

bool parse_top_level_css_import_rule(std::string_view css,
                                     std::size_t at_pos,
                                     CssImportRule& rule,
                                     std::string& parse_error) {
  parse_error.clear();

  if (at_pos >= css.size() || css[at_pos] != '@') {
    return false;
  }

  std::size_t cursor = at_pos + 1;
  if (!starts_with_ascii_case_insensitive(css, cursor, "import")) {
    return false;
  }
  cursor += 6;

  if (cursor < css.size() && is_css_identifier_char(css[cursor])) {
    return false;
  }

  skip_css_whitespace_and_comments(css, cursor);
  if (cursor >= css.size()) {
    parse_error = "missing import URL";
    return false;
  }

  std::string reference;
  if (css[cursor] == '"' || css[cursor] == '\'') {
    if (!parse_css_string_token(css, cursor, reference) || reference.empty()) {
      parse_error = "invalid quoted import URL";
      return false;
    }
  } else if (!parse_css_url_function_reference(css, cursor, reference)) {
    parse_error = "unsupported import URL syntax";
    return false;
  }

  std::size_t statement_end = cursor;
  if (!find_css_statement_end(css, cursor, statement_end)) {
    parse_error = "missing ';' after @import";
    return false;
  }

  rule.begin = at_pos;
  rule.end = statement_end;
  rule.reference = std::move(reference);
  return true;
}

std::vector<CssImportRule> parse_top_level_css_imports(
    std::string_view css,
    const std::string& css_origin_label,
    std::vector<std::string>& warnings) {
  std::vector<CssImportRule> imports;

  std::size_t cursor = 0;
  int brace_depth = 0;
  while (cursor < css.size()) {
    if (cursor + 1 < css.size() && css[cursor] == '/' && css[cursor + 1] == '*') {
      const std::size_t close_pos = css.find("*/", cursor + 2);
      if (close_pos == std::string_view::npos) {
        break;
      }
      cursor = close_pos + 2;
      continue;
    }

    if (css[cursor] == '"' || css[cursor] == '\'') {
      std::string ignored;
      if (!parse_css_string_token(css, cursor, ignored)) {
        break;
      }
      continue;
    }

    if (brace_depth == 0 && css[cursor] == '@') {
      CssImportRule import_rule;
      std::string parse_error;
      if (parse_top_level_css_import_rule(css, cursor, import_rule, parse_error)) {
        imports.push_back(std::move(import_rule));
        cursor = imports.back().end;
        continue;
      }

      if (!parse_error.empty()) {
        warnings.push_back("Ignoring malformed CSS @import in '" + css_origin_label +
                           "': " + parse_error);
      }
    }

    if (css[cursor] == '{') {
      ++brace_depth;
    } else if (css[cursor] == '}' && brace_depth > 0) {
      --brace_depth;
    }
    ++cursor;
  }

  return imports;
}

std::uint64_t hash_css_content(std::string_view css) {
  constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ull;
  constexpr std::uint64_t kFnvPrime = 1099511628211ull;

  std::uint64_t hash = kFnvOffsetBasis;
  for (const unsigned char ch : css) {
    hash ^= static_cast<std::uint64_t>(ch);
    hash *= kFnvPrime;
  }
  return hash;
}

std::string make_css_content_visit_key(std::string_view css_text) {
  return "css-content:" + std::to_string(css_text.size()) + ":" +
         std::to_string(hash_css_content(css_text));
}

std::string make_css_url_visit_key(const std::string& css_url) {
  return "css-url:" + canonical_resource_url(css_url);
}

std::string expand_css_imports(const std::string& css_text,
                               const std::string& css_base_url,
                               const std::string& css_origin_label,
                               std::vector<std::string>& warnings,
                               TextResourceCache& resource_cache,
                               std::unordered_set<std::string>& visited_css_keys) {
  const std::vector<CssImportRule> imports =
      parse_top_level_css_imports(css_text, css_origin_label, warnings);
  if (imports.empty()) {
    return css_text;
  }

  std::string expanded_css;
  expanded_css.reserve(css_text.size());

  std::size_t cursor = 0;
  for (const CssImportRule& import_rule : imports) {
    if (import_rule.begin > cursor) {
      expanded_css.append(css_text, cursor, import_rule.begin - cursor);
    }

    std::string resolved_import_url;
    std::string resolve_error;
    if (!resolve_resource_url(css_base_url, import_rule.reference,
                              resolved_import_url, resolve_error)) {
      warnings.push_back("CSS @import resolution failed for '" + import_rule.reference +
                         "' in '" + css_origin_label + "': " + resolve_error);
      cursor = import_rule.end;
      continue;
    }

    const std::string import_url_key = make_css_url_visit_key(resolved_import_url);
    if (!visited_css_keys.emplace(import_url_key).second) {
      warnings.push_back("CSS @import skipped to avoid cycle/reload: " +
                         resolved_import_url);
      cursor = import_rule.end;
      continue;
    }

    const TextLoadResult& imported_css_resource =
        load_text_resource_cached(resolved_import_url, resource_cache);
    if (!imported_css_resource.ok) {
      warnings.push_back("CSS @import load failed for '" + resolved_import_url +
                         "': " + imported_css_resource.error);
      cursor = import_rule.end;
      continue;
    }
    append_fetch_diagnostic_warning(imported_css_resource, warnings);

    const std::string imported_content_key =
        make_css_content_visit_key(imported_css_resource.text);
    if (!visited_css_keys.emplace(imported_content_key).second) {
      warnings.push_back("CSS @import skipped to avoid repeated content from '" +
                         resolved_import_url + "'");
      cursor = import_rule.end;
      continue;
    }

    const std::string nested_base_url =
        imported_css_resource.final_url.empty() ? resolved_import_url
                                                : imported_css_resource.final_url;
    const std::string expanded_import =
        expand_css_imports(imported_css_resource.text, nested_base_url,
                           resolved_import_url, warnings,
                           resource_cache, visited_css_keys);
    append_text_block(expanded_css, expanded_import);

    cursor = import_rule.end;
  }

  if (cursor < css_text.size()) {
    expanded_css.append(css_text, cursor, css_text.size() - cursor);
  }

  return expanded_css;
}

std::string collect_style_text(const browser::html::Node& root,
                               const std::string& resource_base_url,
                               std::vector<std::string>& warnings,
                               TextResourceCache& resource_cache) {
  std::string combined_css;
  std::unordered_set<std::string> visited_css_keys;

  const std::vector<const browser::html::Node*> style_nodes =
      browser::html::query_all_by_tag(root, "style");
  std::size_t inline_style_index = 0;
  for (const browser::html::Node* style_node : style_nodes) {
    ++inline_style_index;
    if (style_node == nullptr) {
      continue;
    }
    if (!is_css_style_type(*style_node)) {
      const auto type_it = style_node->attributes.find("type");
      if (type_it != style_node->attributes.end()) {
        warnings.push_back("Inline <style> #" + std::to_string(inline_style_index) +
                           " skipped due to unsupported type '" +
                           trim_copy(type_it->second) + "'");
      }
      continue;
    }
    if (!is_stylesheet_media_supported(*style_node)) {
      const auto media_it = style_node->attributes.find("media");
      if (media_it != style_node->attributes.end()) {
        warnings.push_back("Inline <style> #" + std::to_string(inline_style_index) +
                           " skipped due to non-screen media '" +
                           trim_copy(media_it->second) + "'");
      }
      continue;
    }

    std::string block_css;
    for (const std::unique_ptr<browser::html::Node>& child : style_node->children) {
      if (!child) {
        continue;
      }

      if (child->type == browser::html::NodeType::Text) {
        block_css += child->text_content;
      } else {
        block_css += browser::html::inner_text(*child);
      }
    }

    visited_css_keys.emplace(
        make_css_content_visit_key(block_css));
    const std::string expanded_block_css =
        expand_css_imports(block_css, resource_base_url,
                           "inline <style> #" + std::to_string(inline_style_index),
                           warnings, resource_cache, visited_css_keys);
    append_text_block(combined_css, expanded_block_css);
  }

  const std::vector<const browser::html::Node*> link_nodes =
      browser::html::query_all_by_tag(root, "link");
  for (const browser::html::Node* link_node : link_nodes) {
    if (link_node == nullptr) {
      continue;
    }

    const auto rel_it = link_node->attributes.find("rel");
    const auto href_it = link_node->attributes.find("href");
    if (rel_it == link_node->attributes.end() ||
        href_it == link_node->attributes.end()) {
      continue;
    }
    if (!is_stylesheet_rel(rel_it->second)) {
      continue;
    }
    if (!is_css_stylesheet_link_type(*link_node)) {
      const auto type_it = link_node->attributes.find("type");
      if (type_it != link_node->attributes.end()) {
        warnings.push_back(
            "Stylesheet link skipped due to unsupported type '" +
            trim_copy(type_it->second) + "' for href '" + href_it->second + "'");
      }
      continue;
    }
    if (!is_stylesheet_media_supported(*link_node)) {
      const auto media_it = link_node->attributes.find("media");
      if (media_it != link_node->attributes.end()) {
        warnings.push_back(
            "Stylesheet link skipped due to non-screen media '" +
            trim_copy(media_it->second) + "' for href '" + href_it->second + "'");
      }
      continue;
    }

    std::string resolved_url;
    std::string resolve_error;
    if (!resolve_resource_url(resource_base_url, href_it->second,
                              resolved_url, resolve_error)) {
      warnings.push_back("Stylesheet resolution failed for '" + href_it->second +
                         "': " + resolve_error);
      continue;
    }

    const TextLoadResult& stylesheet =
        load_text_resource_cached(resolved_url, resource_cache);
    if (!stylesheet.ok) {
      warnings.push_back("Stylesheet load failed for '" + resolved_url +
                         "': " + stylesheet.error);
      continue;
    }
    append_fetch_diagnostic_warning(stylesheet, warnings);

    const std::string stylesheet_base_url =
        stylesheet.final_url.empty() ? resolved_url : stylesheet.final_url;
    visited_css_keys.emplace(make_css_url_visit_key(stylesheet_base_url));
    visited_css_keys.emplace(make_css_content_visit_key(stylesheet.text));
    const std::string expanded_stylesheet =
        expand_css_imports(stylesheet.text, stylesheet_base_url,
                           stylesheet_base_url, warnings,
                           resource_cache, visited_css_keys);
    append_text_block(combined_css, expanded_stylesheet);
  }

  return combined_css;
}

void execute_scripts(browser::html::Node& document,
                     const std::string& resource_base_url,
                     std::vector<std::string>& warnings,
                     TextResourceCache& resource_cache) {
  const std::vector<const browser::html::Node*> script_nodes =
      browser::html::query_all_by_tag(static_cast<const browser::html::Node&>(document), "script");
  std::size_t script_index = 0;

  for (const browser::html::Node* script_node : script_nodes) {
    ++script_index;
    if (script_node == nullptr) {
      continue;
    }
    if (!is_javascript_script_type(*script_node)) {
      continue;
    }

    std::string script_source;
    const auto src_it = script_node->attributes.find("src");
    if (src_it != script_node->attributes.end() &&
        !trim_copy(src_it->second).empty()) {
      std::string resolved_url;
      std::string resolve_error;
      if (!resolve_resource_url(resource_base_url, src_it->second,
                                resolved_url, resolve_error)) {
        warnings.push_back("Script #" + std::to_string(script_index) +
                           " resolution failed: " + resolve_error);
        continue;
      }

      const TextLoadResult& script_text =
          load_text_resource_cached(resolved_url, resource_cache);
      if (!script_text.ok) {
        warnings.push_back("Script #" + std::to_string(script_index) +
                           " load failed: " + script_text.error);
        continue;
      }
      append_fetch_diagnostic_warning(script_text, warnings);
      script_source = script_text.text;
    } else {
      script_source = collect_node_text_content(*script_node);
    }

    if (trim_copy(script_source).empty()) {
      continue;
    }

    const browser::js::ScriptResult script_result =
        browser::js::execute_script(document, script_source);
    if (!script_result.ok) {
      warnings.push_back("Script #" + std::to_string(script_index) +
                         " execution failed: " + script_result.message);
    }
  }
}

std::string join_warnings(const std::vector<std::string>& warnings) {
  std::string joined;
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (i != 0) {
      joined += " | ";
    }
    joined += warnings[i];
  }
  return joined;
}

std::string first_warning_snippet(const std::vector<std::string>& warnings) {
  if (warnings.empty()) {
    return {};
  }

  std::string snippet = warnings.front();
  for (char& ch : snippet) {
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
  }
  snippet = trim_copy(snippet);

  constexpr std::size_t kMaxSnippetLength = 96;
  if (snippet.size() > kMaxSnippetLength) {
    snippet = snippet.substr(0, kMaxSnippetLength - 3) + "...";
  }

  return snippet;
}

void notify_stage(const StageObserver& observer, PipelineStage stage) {
  if (observer) {
    observer(stage);
  }
}

bool check_cancelled(const CancelCheck& is_cancelled) {
  return is_cancelled && is_cancelled();
}

}  // namespace

const char* pipeline_stage_name(PipelineStage stage) {
  switch (stage) {
    case PipelineStage::Fetching:  return "fetching";
    case PipelineStage::Parsing:   return "parsing";
    case PipelineStage::Styling:   return "styling";
    case PipelineStage::Layout:    return "layout";
    case PipelineStage::Rendering: return "rendering";
  }
  return "unknown";
}

RunResult run(const std::string& url, const RunOptions& options) {
  if (url.empty()) {
    return make_error("URL is empty.");
  }
  if (options.width <= 0 || options.height <= 0) {
    return make_error("Viewport width and height must be positive.");
  }
  if (options.output_path.empty()) {
    return make_error("Output path is empty.");
  }

  // Fetch stage
  notify_stage(options.on_stage_enter, PipelineStage::Fetching);
  if (check_cancelled(options.is_cancelled)) {
    return make_error("Navigation cancelled during fetch");
  }
  TextResourceCache resource_cache;
  const TextLoadResult document_text =
      load_text_resource_cached(url, resource_cache);
  if (!document_text.ok) {
    return make_error("Document load failed: " + document_text.error);
  }

  std::vector<std::string> warnings;
  append_fetch_diagnostic_warning(document_text, warnings);

  // Parse stage
  if (check_cancelled(options.is_cancelled)) {
    return make_error("Navigation cancelled before parsing");
  }
  notify_stage(options.on_stage_enter, PipelineStage::Parsing);
  std::unique_ptr<browser::html::Node> document =
      browser::html::parse_html(document_text.text);
  if (!document) {
    return make_error("HTML parse failed.");
  }

  const std::string resource_base_url =
      resolve_resource_base_url(*document, document_text.final_url, warnings);

  execute_scripts(*document, resource_base_url, warnings, resource_cache);

  // Style stage
  if (check_cancelled(options.is_cancelled)) {
    return make_error("Navigation cancelled before styling");
  }
  notify_stage(options.on_stage_enter, PipelineStage::Styling);
  const std::string style_text =
      collect_style_text(*document, resource_base_url, warnings,
                         resource_cache);
  const browser::css::Stylesheet stylesheet = browser::css::parse_css(style_text);

  // Layout stage
  if (check_cancelled(options.is_cancelled)) {
    return make_error("Navigation cancelled before layout");
  }
  notify_stage(options.on_stage_enter, PipelineStage::Layout);
  const browser::layout::LayoutBox layout_root =
      browser::layout::layout_document(*document, stylesheet, options.width);

  // Render stage
  if (check_cancelled(options.is_cancelled)) {
    return make_error("Navigation cancelled before rendering");
  }
  notify_stage(options.on_stage_enter, PipelineStage::Rendering);
  const browser::render::Canvas canvas =
      browser::render::render_to_canvas(layout_root, options.width, options.height);
  if (!browser::render::write_ppm(canvas, options.output_path)) {
    return make_error("Failed to write output file: " + options.output_path);
  }

  RunResult result;
  result.ok = true;
  result.message = "Rendered " + document_text.final_url + " to " + options.output_path;
  if (!warnings.empty()) {
    result.message += "\nWarning summary: " + std::to_string(warnings.size()) +
                      " warning(s); first: " + first_warning_snippet(warnings);
    result.message += "\nWarnings: " + join_warnings(warnings);
  }
  return result;
}

}  // namespace browser::app
