#include "browser/net/url.h"

#include <cctype>
#include <string>
#include <vector>

namespace browser::net {
namespace {

std::string to_lower_ascii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

int default_port_for_scheme(const std::string& scheme) {
  if (scheme == "http") {
    return 80;
  }
  if (scheme == "https") {
    return 443;
  }
  if (scheme == "ws") {
    return 80;
  }
  if (scheme == "wss") {
    return 443;
  }
  return 0;
}

bool is_valid_port(const std::string& raw, int& port) {
  if (raw.empty()) {
    return false;
  }
  long value = 0;
  for (char ch : raw) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
    value = value * 10 + (ch - '0');
    if (value > 65535) {
      return false;
    }
  }
  if (value <= 0) {
    return false;
  }
  port = static_cast<int>(value);
  return true;
}

bool is_ipv6_literal(const std::string& host) {
  return host.find(':') != std::string::npos;
}

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool is_ascii_alpha(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalpha(uch) != 0;
}

bool is_ascii_alnum(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) != 0;
}

bool is_url_scheme_char(char ch) {
  return is_ascii_alnum(ch) || ch == '+' || ch == '-' || ch == '.';
}

bool looks_like_windows_drive_path(const std::string& value) {
  if (value.size() < 2) {
    return false;
  }
  if (!is_ascii_alpha(value[0]) || value[1] != ':') {
    return false;
  }
  return value.size() == 2 || value[2] == '/' || value[2] == '\\';
}

bool extract_scheme(const std::string& value, std::string& scheme) {
  scheme.clear();
  const std::size_t colon = value.find(':');
  if (colon == std::string::npos || colon == 0) {
    return false;
  }

  if (!is_ascii_alpha(value[0])) {
    return false;
  }
  for (std::size_t i = 1; i < colon; ++i) {
    if (!is_url_scheme_char(value[i])) {
      return false;
    }
  }

  scheme = to_lower_ascii(value.substr(0, colon));
  return true;
}

std::string format_host_for_url(const std::string& host) {
  if (is_ipv6_literal(host) && (host.empty() || host.front() != '[')) {
    return "[" + host + "]";
  }
  return host;
}

std::string make_origin(const Url& url) {
  std::string origin = url.scheme + "://" + format_host_for_url(url.host);
  const int default_port = default_port_for_scheme(url.scheme);
  if (url.port > 0 && url.port != default_port) {
    origin += ":" + std::to_string(url.port);
  }
  return origin;
}

std::string extract_path_only(const std::string& path_and_query) {
  const std::size_t query_pos = path_and_query.find('?');
  if (query_pos == std::string::npos) {
    return path_and_query;
  }
  return path_and_query.substr(0, query_pos);
}

std::string extract_query_only(const std::string& path_and_query) {
  const std::size_t query_pos = path_and_query.find('?');
  if (query_pos == std::string::npos) {
    return "";
  }
  return path_and_query.substr(query_pos);
}

struct ReferenceParts {
  std::string path;
  std::string query;
  std::string fragment;
};

ReferenceParts split_reference(const std::string& ref) {
  ReferenceParts parts;

  const std::size_t fragment_pos = ref.find('#');
  const std::string without_fragment =
      (fragment_pos == std::string::npos) ? ref : ref.substr(0, fragment_pos);
  if (fragment_pos != std::string::npos) {
    parts.fragment = ref.substr(fragment_pos);
  }

  const std::size_t query_pos = without_fragment.find('?');
  if (query_pos == std::string::npos) {
    parts.path = without_fragment;
  } else {
    parts.path = without_fragment.substr(0, query_pos);
    parts.query = without_fragment.substr(query_pos);
  }

  return parts;
}

std::string strip_fragment(const std::string& value) {
  const std::size_t fragment_pos = value.find('#');
  if (fragment_pos == std::string::npos) {
    return value;
  }
  return value.substr(0, fragment_pos);
}

std::string directory_of_path(const std::string& path) {
  if (path.empty()) {
    return "";
  }
  if (path.back() == '/') {
    return path;
  }

  const std::size_t slash_pos = path.rfind('/');
  if (slash_pos == std::string::npos) {
    return "";
  }
  return path.substr(0, slash_pos + 1);
}

std::string join_paths(const std::string& base_dir, const std::string& relative) {
  if (base_dir.empty()) {
    return relative;
  }
  if (relative.empty()) {
    return base_dir;
  }
  if (base_dir.back() == '/') {
    return base_dir + relative;
  }
  return base_dir + "/" + relative;
}

std::string normalize_path(const std::string& input) {
  if (input.empty()) {
    return "";
  }

  const bool absolute = input.front() == '/';
  const bool trailing_slash = input.size() > 1 && input.back() == '/';

  std::vector<std::string> segments;
  std::size_t pos = 0;
  while (pos <= input.size()) {
    const std::size_t slash = input.find('/', pos);
    const std::string segment =
        (slash == std::string::npos) ? input.substr(pos) : input.substr(pos, slash - pos);

    if (!segment.empty() && segment != ".") {
      if (segment == "..") {
        if (!segments.empty() && segments.back() != "..") {
          segments.pop_back();
        } else if (!absolute) {
          segments.push_back(segment);
        }
      } else {
        segments.push_back(segment);
      }
    }

    if (slash == std::string::npos) {
      break;
    }
    pos = slash + 1;
  }

  std::string normalized;
  if (absolute) {
    normalized = "/";
  }
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (!normalized.empty() && normalized.back() != '/') {
      normalized += "/";
    }
    normalized += segments[i];
  }

  if (normalized.empty() && absolute) {
    normalized = "/";
  }

  if (trailing_slash && !normalized.empty() && normalized.back() != '/') {
    normalized += "/";
  }

  return normalized;
}

bool is_hex_digit(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isxdigit(uch) != 0;
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

bool percent_decode(const std::string& input, std::string& output, std::string& err) {
  output.clear();
  err.clear();

  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if (ch != '%') {
      output += ch;
      continue;
    }

    if (i + 2 >= input.size() || !is_hex_digit(input[i + 1]) || !is_hex_digit(input[i + 2])) {
      err = "Invalid percent-encoding in file URL path";
      return false;
    }

    const int high = from_hex(input[i + 1]);
    const int low = from_hex(input[i + 2]);
    output += static_cast<char>((high << 4) | low);
    i += 2;
  }

  return true;
}

bool is_unreserved_path_char(char ch) {
  return is_ascii_alnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

char to_hex_upper(unsigned char value) {
  constexpr char kHexDigits[] = "0123456789ABCDEF";
  return kHexDigits[value & 0x0F];
}

std::string percent_encode_path(const std::string& path) {
  std::string output;
  output.reserve(path.size());

  for (const char ch : path) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (is_unreserved_path_char(ch) || ch == '/' || ch == ':') {
      output += ch;
      continue;
    }

    output += '%';
    output += to_hex_upper(static_cast<unsigned char>(uch >> 4));
    output += to_hex_upper(uch);
  }

  return output;
}

bool is_windows_drive_path(const std::string& path) {
  if (path.size() < 2) {
    return false;
  }
  if (!is_ascii_alpha(path[0]) || path[1] != ':') {
    return false;
  }
  return path.size() == 2 || path[2] == '/';
}

std::string resolve_http_reference(const Url& base, const std::string& ref) {
  const std::string origin = make_origin(base);
  if (ref.empty()) {
    return origin + base.path_and_query;
  }

  if (ref.front() == '#') {
    return origin + base.path_and_query + ref;
  }

  const ReferenceParts parts = split_reference(ref);
  std::string base_path = extract_path_only(base.path_and_query);
  if (base_path.empty()) {
    base_path = "/";
  }

  if (parts.path.empty()) {
    const std::string query = parts.query.empty() ? extract_query_only(base.path_and_query) : parts.query;
    return origin + base_path + query + parts.fragment;
  }

  std::string resolved_path;
  if (parts.path.front() == '/') {
    resolved_path = normalize_path(parts.path);
  } else {
    std::string base_dir = directory_of_path(base_path);
    if (base_dir.empty()) {
      base_dir = "/";
    }
    resolved_path = normalize_path(join_paths(base_dir, parts.path));
  }

  if (resolved_path.empty() || resolved_path.front() != '/') {
    resolved_path = "/" + resolved_path;
  }

  return origin + resolved_path + parts.query + parts.fragment;
}

std::string resolve_file_reference(const std::string& base_url, const std::string& ref, std::string& err) {
  std::string base_path;
  if (!file_url_to_path(base_url, base_path, err)) {
    return "";
  }

  if (ref.empty()) {
    return path_to_file_url(base_path);
  }

  if (ref.front() == '#') {
    return strip_fragment(base_url) + ref;
  }

  const ReferenceParts parts = split_reference(ref);
  if (parts.path.empty()) {
    return path_to_file_url(base_path) + parts.query + parts.fragment;
  }

  std::string resolved_path;
  if (parts.path.front() == '/') {
    resolved_path = normalize_path(parts.path);
  } else {
    const std::string base_dir = directory_of_path(base_path);
    resolved_path = normalize_path(join_paths(base_dir, parts.path));
  }

  if (resolved_path.empty()) {
    resolved_path = "/";
  }

  return path_to_file_url(resolved_path) + parts.query + parts.fragment;
}

}  // namespace

std::string Url::to_string() const {
  const int default_port = default_port_for_scheme(scheme);
  const bool include_port = (port > 0 && port != default_port);

  std::string host_part = host;
  if (is_ipv6_literal(host_part) && (host_part.empty() || host_part.front() != '[')) {
    host_part = "[" + host_part + "]";
  }

  std::string result = scheme + "://" + host_part;
  if (include_port) {
    result += ":" + std::to_string(port);
  }

  if (path_and_query.empty()) {
    result += "/";
  } else if (path_and_query.front() == '/' || path_and_query.front() == '?') {
    if (path_and_query.front() == '?') {
      result += "/";
    }
    result += path_and_query;
  } else {
    result += "/" + path_and_query;
  }

  return result;
}

bool parse_url(const std::string& input, Url& out, std::string& err) {
  out = Url{};
  err.clear();

  if (input.empty()) {
    err = "URL is empty";
    return false;
  }

  const std::size_t scheme_end = input.find("://");
  if (scheme_end == std::string::npos) {
    err = "URL must include scheme (http://, https://, ws://, or wss://)";
    return false;
  }

  out.scheme = to_lower_ascii(input.substr(0, scheme_end));
  if (out.scheme != "http" && out.scheme != "https" &&
      out.scheme != "ws" && out.scheme != "wss") {
    err = "Unsupported URL scheme: " + out.scheme;
    return false;
  }

  const std::size_t authority_start = scheme_end + 3;
  if (authority_start >= input.size()) {
    err = "URL is missing host";
    return false;
  }

  const std::size_t authority_end = input.find_first_of("/?#", authority_start);
  const std::string authority =
      (authority_end == std::string::npos)
          ? input.substr(authority_start)
          : input.substr(authority_start, authority_end - authority_start);

  if (authority.empty()) {
    err = "URL is missing host";
    return false;
  }

  if (authority.find('@') != std::string::npos) {
    err = "User-info in URL is not supported";
    return false;
  }

  std::string host;
  int port = default_port_for_scheme(out.scheme);

  if (!authority.empty() && authority.front() == '[') {
    const std::size_t bracket_end = authority.find(']');
    if (bracket_end == std::string::npos) {
      err = "Invalid IPv6 host: missing closing bracket";
      return false;
    }

    host = authority.substr(1, bracket_end - 1);
    if (host.empty()) {
      err = "URL host is empty";
      return false;
    }

    if (bracket_end + 1 < authority.size()) {
      if (authority[bracket_end + 1] != ':') {
        err = "Invalid host/port separator";
        return false;
      }
      const std::string raw_port = authority.substr(bracket_end + 2);
      if (!is_valid_port(raw_port, port)) {
        err = "Invalid port: " + raw_port;
        return false;
      }
    }
  } else {
    std::string raw_port;
    const std::size_t first_colon = authority.find(':');
    if (first_colon != std::string::npos) {
      if (authority.find(':', first_colon + 1) != std::string::npos) {
        err = "IPv6 literals must be enclosed in []";
        return false;
      }
      host = authority.substr(0, first_colon);
      raw_port = authority.substr(first_colon + 1);
    } else {
      host = authority;
    }

    if (host.empty()) {
      err = "URL host is empty";
      return false;
    }

    if (!raw_port.empty() && !is_valid_port(raw_port, port)) {
      err = "Invalid port: " + raw_port;
      return false;
    }
  }

  out.host = host;
  out.port = port;

  std::string path_and_query = "/";
  if (authority_end != std::string::npos) {
    const std::size_t fragment_pos = input.find('#', authority_end);
    std::string remainder =
        (fragment_pos == std::string::npos)
            ? input.substr(authority_end)
            : input.substr(authority_end, fragment_pos - authority_end);

    if (!remainder.empty()) {
      if (remainder.front() == '?') {
        path_and_query = "/" + remainder;
      } else {
        path_and_query = remainder;
      }
    }
  }

  if (path_and_query.empty()) {
    path_and_query = "/";
  }

  out.path_and_query = path_and_query;
  return true;
}

bool is_absolute_url(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  if (starts_with(value, "//")) {
    return false;
  }
  if (looks_like_windows_drive_path(value)) {
    return false;
  }

  std::string scheme;
  return extract_scheme(value, scheme);
}

std::string resolve_url(const std::string& base_url, const std::string& ref, std::string& err) {
  err.clear();

  if (ref.empty()) {
    return base_url;
  }
  if (is_absolute_url(ref)) {
    return ref;
  }

  std::string base_scheme;
  if (!extract_scheme(base_url, base_scheme)) {
    err = "Base URL must include a valid scheme";
    return "";
  }

  if (starts_with(ref, "//")) {
    return base_scheme + ":" + ref;
  }

  if (base_scheme == "http" || base_scheme == "https" ||
      base_scheme == "ws" || base_scheme == "wss") {
    Url base;
    if (!parse_url(base_url, base, err)) {
      return "";
    }
    return resolve_http_reference(base, ref);
  }

  if (base_scheme == "file") {
    return resolve_file_reference(base_url, ref, err);
  }

  err = "Unsupported base URL scheme: " + base_scheme;
  return "";
}

bool is_file_url(const std::string& value) {
  std::string scheme;
  return extract_scheme(value, scheme) && scheme == "file";
}

bool file_url_to_path(const std::string& file_url, std::string& path, std::string& err) {
  path.clear();
  err.clear();

  if (!is_file_url(file_url)) {
    err = "URL is not a file URL";
    return false;
  }

  const std::size_t colon = file_url.find(':');
  std::string remainder = file_url.substr(colon + 1);
  const std::size_t trim_pos = remainder.find_first_of("?#");
  if (trim_pos != std::string::npos) {
    remainder = remainder.substr(0, trim_pos);
  }

  std::string raw_path;
  if (starts_with(remainder, "//")) {
    const std::size_t authority_start = 2;
    const std::size_t authority_end = remainder.find('/', authority_start);
    const std::string authority =
        (authority_end == std::string::npos)
            ? remainder.substr(authority_start)
            : remainder.substr(authority_start, authority_end - authority_start);

    if (!authority.empty() && to_lower_ascii(authority) != "localhost") {
      err = "Unsupported file URL host: " + authority;
      return false;
    }

    raw_path = (authority_end == std::string::npos) ? "/" : remainder.substr(authority_end);
  } else {
    raw_path = remainder;
  }

  if (raw_path.empty()) {
    raw_path = "/";
  }

  std::string decoded_path;
  if (!percent_decode(raw_path, decoded_path, err)) {
    return false;
  }

  if (decoded_path.size() >= 3 && decoded_path[0] == '/' && is_ascii_alpha(decoded_path[1]) &&
      decoded_path[2] == ':') {
    decoded_path.erase(0, 1);
  }

  path = decoded_path;
  return true;
}

std::string path_to_file_url(const std::string& path) {
  std::string normalized_path = path;
  for (char& ch : normalized_path) {
    if (ch == '\\') {
      ch = '/';
    }
  }

  if (normalized_path.empty()) {
    return "file:///";
  }

  const std::string encoded_path = percent_encode_path(normalized_path);
  if (is_windows_drive_path(normalized_path)) {
    return "file:///" + encoded_path;
  }
  if (!normalized_path.empty() && normalized_path.front() == '/') {
    return "file://" + encoded_path;
  }
  return "file:" + encoded_path;
}

}  // namespace browser::net
