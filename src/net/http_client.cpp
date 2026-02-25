#include "browser/net/http_client.h"

#include "browser/net/url.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef BROWSER_USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <mutex>
#endif

namespace browser::net {
namespace {

constexpr std::size_t kReadBufferSize = 8192;
constexpr std::size_t kMaxHeaderBytes = 1024 * 1024;

std::string to_lower_ascii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

int hex_digit_value(char ch) {
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

bool is_unreserved_uri_char(unsigned char ch) {
  return std::isalnum(ch) != 0 || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

std::string decode_unreserved_percent_encoding(const std::string& value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const int hi = hex_digit_value(value[i + 1]);
      const int lo = hex_digit_value(value[i + 2]);
      if (hi >= 0 && lo >= 0) {
        const unsigned char byte = static_cast<unsigned char>((hi << 4) | lo);
        if (is_unreserved_uri_char(byte)) {
          decoded.push_back(static_cast<char>(byte));
          i += 2;
          continue;
        }
      }
    }
    decoded.push_back(value[i]);
  }
  return decoded;
}

std::string trim_ascii(const std::string& value) {
  std::size_t first = 0;
  while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
    ++first;
  }

  std::size_t last = value.size();
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
    --last;
  }

  return value.substr(first, last - first);
}

const std::string* find_header_value_case_insensitive(
    const std::map<std::string, std::string>& headers,
    const std::string& header_name_lower) {
  auto it = headers.find(header_name_lower);
  if (it != headers.end()) {
    return &it->second;
  }
  for (const auto& pair : headers) {
    if (to_lower_ascii(pair.first) == header_name_lower) {
      return &pair.second;
    }
  }
  return nullptr;
}

std::string strip_single_quotes(const std::string& token) {
  if (token.size() >= 2 && token.front() == '\'' && token.back() == '\'') {
    return token.substr(1, token.size() - 2);
  }
  return token;
}

std::string normalize_source_token(const std::string& token) {
  return to_lower_ascii(trim_ascii(strip_single_quotes(token)));
}

bool host_matches_pattern(const std::string& host, const std::string& pattern) {
  auto normalize_host_for_match = [](std::string value) {
    if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
      value = value.substr(1, value.size() - 2);
    }
    return to_lower_ascii(value);
  };

  const std::string normalized_host = normalize_host_for_match(host);
  const std::string normalized_pattern = normalize_host_for_match(pattern);
  if (normalized_pattern.empty()) {
    return false;
  }
  if (normalized_pattern.rfind("*.", 0) == 0) {
    const std::string suffix = normalized_pattern.substr(2);
    if (suffix.empty() || normalized_host.size() <= suffix.size()) {
      return false;
    }
    if (normalized_host.compare(normalized_host.size() - suffix.size(), suffix.size(), suffix) != 0) {
      return false;
    }
    const std::size_t boundary = normalized_host.size() - suffix.size();
    return boundary > 0 && normalized_host[boundary - 1] == '.';
  }
  return normalized_host == normalized_pattern;
}

bool parse_host_source_token(const std::string& source,
                             std::string& scheme,
                             std::string& host_pattern,
                             std::string& path_part,
                             int& port,
                             bool& wildcard_port) {
  scheme.clear();
  host_pattern.clear();
  path_part.clear();
  port = 0;
  wildcard_port = false;

  std::string rest = source;
  const std::size_t scheme_pos = source.find("://");
  if (scheme_pos != std::string::npos) {
    scheme = source.substr(0, scheme_pos);
    rest = source.substr(scheme_pos + 3);
  }

  const std::size_t path_pos = rest.find('/');
  if (path_pos != std::string::npos) {
    path_part = rest.substr(path_pos);
    rest = rest.substr(0, path_pos);
  }
  if (!path_part.empty()) {
    const std::size_t query_pos = path_part.find_first_of("?#");
    if (query_pos != std::string::npos) {
      path_part = path_part.substr(0, query_pos);
    }
  }
  if (rest.empty()) {
    return false;
  }

  auto parse_and_validate_port = [](const std::string& value, int& out_port) -> bool {
    try {
      out_port = std::stoi(value);
    } catch (...) {
      return false;
    }
    return out_port >= 1 && out_port <= 65535;
  };

  if (rest.front() == '[') {
    const std::size_t bracket = rest.find(']');
    if (bracket == std::string::npos) {
      return false;
    }
    host_pattern = rest.substr(0, bracket + 1);
    if (bracket + 1 < rest.size()) {
      if (rest[bracket + 1] != ':') {
        return false;
      }
      const std::string port_part = rest.substr(bracket + 2);
      if (port_part.empty()) {
        return false;
      }
      if (port_part == "*") {
        wildcard_port = true;
        return true;
      }
      if (!parse_and_validate_port(port_part, port)) {
        return false;
      }
    }
    return true;
  }

  const std::size_t colon = rest.rfind(':');
  if (colon != std::string::npos) {
    const std::string port_part = rest.substr(colon + 1);
    if (port_part == "*") {
      wildcard_port = true;
      rest = rest.substr(0, colon);
    } else if (!port_part.empty() &&
               std::all_of(port_part.begin(), port_part.end(),
                           [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; })) {
      if (!parse_and_validate_port(port_part, port)) {
        return false;
      }
      rest = rest.substr(0, colon);
    }
  }

  host_pattern = rest;
  return !host_pattern.empty();
}

std::string extract_path_for_match(const std::string& path_and_query) {
  if (path_and_query.empty()) {
    return "/";
  }
  const std::size_t query_pos = path_and_query.find_first_of("?#");
  const std::string path = query_pos == std::string::npos
                               ? path_and_query
                               : path_and_query.substr(0, query_pos);
  return path.empty() ? "/" : path;
}

std::string normalize_path_for_csp_match(const std::string& raw_path) {
  if (raw_path.empty()) {
    return "";
  }

  std::string path = decode_unreserved_percent_encoding(raw_path);
  if (!path.empty() && path.front() != '/') {
    path = "/" + path;
  }

  const bool trailing_slash = path.size() > 1 && path.back() == '/';
  std::vector<std::string> parts;
  std::size_t pos = 0;
  while (pos <= path.size()) {
    const std::size_t slash = path.find('/', pos);
    const std::string segment =
        (slash == std::string::npos) ? path.substr(pos) : path.substr(pos, slash - pos);
    if (segment == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else if (!segment.empty() && segment != ".") {
      parts.push_back(segment);
    }
    if (slash == std::string::npos) {
      break;
    }
    pos = slash + 1;
  }

  std::string normalized = "/";
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      normalized += "/";
    }
    normalized += parts[i];
  }
  if (trailing_slash && normalized.size() > 1) {
    normalized += "/";
  }
  return normalized;
}

bool source_path_matches_request_path(const std::string& source_path,
                                      const std::string& request_path) {
  if (source_path.empty()) {
    return true;
  }
  if (source_path.back() == '/') {
    return starts_with(request_path, source_path);
  }
  return request_path == source_path;
}

bool source_token_matches_url(const std::string& source,
                              const Url& parsed,
                              const std::string& policy_origin_scheme) {
  std::string source_scheme;
  std::string source_host_pattern;
  std::string source_path_part;
  int source_port = 0;
  bool wildcard_port = false;
  if (!parse_host_source_token(source,
                               source_scheme,
                               source_host_pattern,
                               source_path_part,
                               source_port,
                               wildcard_port)) {
    return false;
  }
  std::string effective_scheme = source_scheme;
  if (effective_scheme.empty() && !policy_origin_scheme.empty()) {
    effective_scheme = policy_origin_scheme;
  }
  if (!effective_scheme.empty() && effective_scheme != parsed.scheme) {
    return false;
  }
  const std::string request_host = to_lower_ascii(parsed.host);
  if (!host_matches_pattern(request_host, source_host_pattern)) {
    return false;
  }
  if (!effective_scheme.empty() && source_port == 0 && !wildcard_port) {
    // CSP host-sources with an explicit scheme and no port only match
    // requests on that scheme's default port.
    int default_port = 0;
    if (effective_scheme == "http") {
      default_port = 80;
    } else if (effective_scheme == "https") {
      default_port = 443;
    } else if (effective_scheme == "ws") {
      default_port = 80;
    } else if (effective_scheme == "wss") {
      default_port = 443;
    }
    if (default_port != 0 && parsed.port != default_port) {
      return false;
    }
  }
  if (!wildcard_port && source_port != 0 && parsed.port != source_port) {
    return false;
  }
  const std::string source_path = normalize_path_for_csp_match(source_path_part);
  const std::string request_path =
      normalize_path_for_csp_match(extract_path_for_match(parsed.path_and_query));
  if (!source_path_matches_request_path(source_path, request_path)) {
    return false;
  }
  return true;
}

std::string build_origin_for_url(const Url& parsed) {
  std::string request_origin = parsed.scheme + "://" + parsed.host;
  if (parsed.port != 0) {
    int default_p = 0;
    if (parsed.scheme == "http") default_p = 80;
    else if (parsed.scheme == "https") default_p = 443;
    else if (parsed.scheme == "ws") default_p = 80;
    else if (parsed.scheme == "wss") default_p = 443;
    if (parsed.port != default_p) {
      request_origin += ":" + std::to_string(parsed.port);
    }
  }
  return request_origin;
}

std::string canonicalize_origin(const std::string& origin) {
  const std::string trimmed = trim_ascii(origin);
  if (trimmed.empty()) {
    return "";
  }

  Url parsed;
  std::string err;
  if (parse_url(trimmed, parsed, err)) {
    return to_lower_ascii(build_origin_for_url(parsed));
  }

  return to_lower_ascii(trimmed);
}

bool csp_connect_src_allows_url(const std::string& url, const RequestPolicy& policy) {
  if (!policy.enforce_connect_src) {
    return true;
  }

  const std::vector<std::string>& effective_sources =
      policy.connect_src_sources.empty() ? policy.default_src_sources : policy.connect_src_sources;
  if (effective_sources.empty()) {
    return true;
  }

  Url parsed;
  std::string err;
  if (!parse_url(url, parsed, err)) {
    return true;
  }
  std::string policy_origin_scheme;
  if (!policy.origin.empty()) {
    Url policy_origin_parsed;
    std::string policy_origin_err;
    if (parse_url(policy.origin, policy_origin_parsed, policy_origin_err)) {
      policy_origin_scheme = policy_origin_parsed.scheme;
    }
  }
  const std::string request_origin = to_lower_ascii(build_origin_for_url(parsed));
  const std::string policy_origin = canonicalize_origin(policy.origin);

  for (const std::string& raw_source : effective_sources) {
    const std::string source = normalize_source_token(raw_source);
    if (source.empty()) {
      continue;
    }
    if (source == "*") {
      return true;
    }
    if (source == "none") {
      continue;
    }
    if (source == "self") {
      if (!policy_origin.empty() && request_origin == policy_origin) {
        return true;
      }
      continue;
    }
    if (source.back() == ':') {
      const std::string scheme = source.substr(0, source.size() - 1);
      if (parsed.scheme == scheme) {
        return true;
      }
      continue;
    }
    if (source_token_matches_url(source, parsed, policy_origin_scheme)) {
      return true;
    }
    if (source == request_origin) {
      return true;
    }
  }

  return false;
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

bool is_default_port(const Url& url) {
  return url.port == default_port_for_scheme(url.scheme);
}

bool is_ipv6_literal(const std::string& host) {
  return host.find(':') != std::string::npos;
}

std::string format_host(const std::string& host) {
  if (is_ipv6_literal(host) && (host.empty() || host.front() != '[')) {
    return "[" + host + "]";
  }
  return host;
}

std::string make_origin(const Url& url) {
  std::string origin = url.scheme + "://" + format_host(url.host);
  if (!is_default_port(url)) {
    origin += ":" + std::to_string(url.port);
  }
  return origin;
}

std::string make_host_header(const Url& url) {
  std::string host_header = format_host(url.host);
  if (!is_default_port(url)) {
    host_header += ":" + std::to_string(url.port);
  }
  return host_header;
}

bool is_redirect_status(int status_code) {
  switch (status_code) {
    case 301:
    case 302:
    case 303:
    case 307:
    case 308:
      return true;
    default:
      return false;
  }
}

bool set_nonblocking(int fd, bool nonblocking, std::string& err) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    err = "fcntl(F_GETFL) failed: " + std::string(std::strerror(errno));
    return false;
  }

  const int target_flags = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  if (fcntl(fd, F_SETFL, target_flags) < 0) {
    err = "fcntl(F_SETFL) failed: " + std::string(std::strerror(errno));
    return false;
  }

  return true;
}

bool set_socket_timeouts(int fd, int timeout_seconds, std::string& err) {
  if (timeout_seconds <= 0) {
    timeout_seconds = 10;
  }

  timeval timeout;
  timeout.tv_sec = timeout_seconds;
  timeout.tv_usec = 0;

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    err = "setsockopt(SO_RCVTIMEO) failed: " + std::string(std::strerror(errno));
    return false;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
    err = "setsockopt(SO_SNDTIMEO) failed: " + std::string(std::strerror(errno));
    return false;
  }

  return true;
}

int connect_tcp(const std::string& host, int port, int timeout_seconds, std::string& err) {
  err.clear();
  if (timeout_seconds <= 0) {
    timeout_seconds = 10;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* results = nullptr;
  const std::string port_str = std::to_string(port);
  const int gai_rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &results);
  if (gai_rc != 0) {
    err = "DNS resolution failed for host '" + host + "': " + std::string(gai_strerror(gai_rc));
    return -1;
  }

  int connected_fd = -1;
  std::string last_error;

  for (addrinfo* addr = results; addr != nullptr; addr = addr->ai_next) {
    int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd < 0) {
      last_error = "socket() failed: " + std::string(std::strerror(errno));
      continue;
    }

    std::string step_error;
    if (!set_nonblocking(fd, true, step_error)) {
      last_error = step_error;
      close(fd);
      continue;
    }

    int rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
    if (rc < 0 && errno != EINPROGRESS) {
      last_error = "connect() failed: " + std::string(std::strerror(errno));
      close(fd);
      continue;
    }

    if (rc < 0 && errno == EINPROGRESS) {
      fd_set write_set;
      FD_ZERO(&write_set);
      FD_SET(fd, &write_set);

      timeval timeout;
      timeout.tv_sec = timeout_seconds;
      timeout.tv_usec = 0;

      rc = select(fd + 1, nullptr, &write_set, nullptr, &timeout);
      if (rc == 0) {
        last_error = "connect() timed out";
        close(fd);
        continue;
      }
      if (rc < 0) {
        last_error = "select() failed while connecting: " + std::string(std::strerror(errno));
        close(fd);
        continue;
      }

      int socket_error = 0;
      socklen_t socket_error_len = sizeof(socket_error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) < 0) {
        last_error = "getsockopt(SO_ERROR) failed: " + std::string(std::strerror(errno));
        close(fd);
        continue;
      }
      if (socket_error != 0) {
        last_error = "connect() failed: " + std::string(std::strerror(socket_error));
        close(fd);
        continue;
      }
    }

    if (!set_nonblocking(fd, false, step_error)) {
      last_error = step_error;
      close(fd);
      continue;
    }

    if (!set_socket_timeouts(fd, timeout_seconds, step_error)) {
      last_error = step_error;
      close(fd);
      continue;
    }

    connected_fd = fd;
    break;
  }

  freeaddrinfo(results);

  if (connected_fd < 0) {
    err = last_error.empty() ? "Unable to connect" : last_error;
  }

  return connected_fd;
}

#ifdef BROWSER_USE_OPENSSL
void init_openssl_once() {
  static std::once_flag once;
  std::call_once(once, []() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
  });
}
#endif

class Connection {
 public:
  Connection() = default;

  ~Connection() {
    Close();
  }

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  bool Open(const Url& url, int timeout_seconds, std::string& err) {
    err.clear();

    fd_ = connect_tcp(url.host, url.port, timeout_seconds, err);
    if (fd_ < 0) {
      return false;
    }

    if (url.scheme == "https") {
#ifdef BROWSER_USE_OPENSSL
      init_openssl_once();

      ssl_ctx_ = SSL_CTX_new(TLS_client_method());
      if (!ssl_ctx_) {
        err = "SSL_CTX_new() failed";
        return false;
      }
      SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
      if (SSL_CTX_set_default_verify_paths(ssl_ctx_) != 1) {
        err = "SSL_CTX_set_default_verify_paths() failed";
        return false;
      }

      ssl_ = SSL_new(ssl_ctx_);
      if (!ssl_) {
        err = "SSL_new() failed";
        return false;
      }

      SSL_set_tlsext_host_name(ssl_, url.host.c_str());
      SSL_set_fd(ssl_, fd_);

      while (true) {
        const int rc = SSL_connect(ssl_);
        if (rc == 1) {
          use_ssl_ = true;
          break;
        }

        const int ssl_error = SSL_get_error(ssl_, rc);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
          continue;
        }

        err = "TLS handshake failed";
        return false;
      }

      SSL_set_verify(ssl_, SSL_VERIFY_PEER, nullptr);
      if (SSL_get_verify_result(ssl_) != X509_V_OK) {
        err = "TLS certificate verification failed: " +
              std::string(X509_verify_cert_error_string(SSL_get_verify_result(ssl_)));
        return false;
      }

      X509* peer_cert = SSL_get_peer_certificate(ssl_);
      if (!peer_cert) {
        err = "TLS certificate verification failed: missing peer certificate";
        return false;
      }

      const int hostname_ok =
          X509_check_host(peer_cert, url.host.c_str(), url.host.size(), 0, nullptr);
      X509_free(peer_cert);
      if (hostname_ok != 1) {
        err = "TLS certificate verification failed: hostname mismatch for " + url.host;
        return false;
      }
#else
      err = "HTTPS requested but OpenSSL support is not enabled (compile with BROWSER_USE_OPENSSL)";
      return false;
#endif
    }

    return true;
  }

  bool WriteAll(const std::string& data, std::string& err) {
    std::size_t written = 0;
    while (written < data.size()) {
#ifdef BROWSER_USE_OPENSSL
      if (use_ssl_) {
        const int remaining = static_cast<int>(std::min<std::size_t>(data.size() - written, 1 << 20));
        const int rc = SSL_write(ssl_, data.data() + written, remaining);
        if (rc > 0) {
          written += static_cast<std::size_t>(rc);
          continue;
        }

        const int ssl_error = SSL_get_error(ssl_, rc);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
          continue;
        }

        err = "SSL_write() failed";
        return false;
      }
#endif

      const ssize_t rc = send(fd_, data.data() + written, data.size() - written, 0);
      if (rc > 0) {
        written += static_cast<std::size_t>(rc);
        continue;
      }
      if (rc == 0) {
        err = "Connection closed while writing request";
        return false;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        err = "Write timed out";
        return false;
      }

      err = "send() failed: " + std::string(std::strerror(errno));
      return false;
    }

    return true;
  }

  ssize_t ReadSome(char* buffer, std::size_t buffer_size, bool& eof, std::string& err) {
    eof = false;

    while (true) {
#ifdef BROWSER_USE_OPENSSL
      if (use_ssl_) {
        const int rc = SSL_read(ssl_, buffer, static_cast<int>(buffer_size));
        if (rc > 0) {
          return rc;
        }

        const int ssl_error = SSL_get_error(ssl_, rc);
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
          eof = true;
          return 0;
        }
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
          continue;
        }

        err = "SSL_read() failed";
        return -1;
      }
#endif

      const ssize_t rc = recv(fd_, buffer, buffer_size, 0);
      if (rc > 0) {
        return rc;
      }
      if (rc == 0) {
        eof = true;
        return 0;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        err = "Read timed out";
        return -1;
      }

      err = "recv() failed: " + std::string(std::strerror(errno));
      return -1;
    }
  }

 private:
  void Close() {
#ifdef BROWSER_USE_OPENSSL
    if (ssl_) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (ssl_ctx_) {
      SSL_CTX_free(ssl_ctx_);
      ssl_ctx_ = nullptr;
    }
#endif

    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }

    use_ssl_ = false;
  }

  int fd_ = -1;
  bool use_ssl_ = false;

#ifdef BROWSER_USE_OPENSSL
  SSL_CTX* ssl_ctx_ = nullptr;
  SSL* ssl_ = nullptr;
#endif
};

bool read_from_connection(Connection& connection, std::string& buffer, bool& eof, std::string& err) {
  char temp[kReadBufferSize];
  const ssize_t rc = connection.ReadSome(temp, sizeof(temp), eof, err);
  if (rc < 0) {
    return false;
  }
  if (rc > 0) {
    buffer.append(temp, static_cast<std::size_t>(rc));
  }
  return true;
}

bool read_line_crlf(Connection& connection, std::string& buffer, std::string& line, std::string& err) {
  while (true) {
    const std::size_t eol = buffer.find("\r\n");
    if (eol != std::string::npos) {
      line = buffer.substr(0, eol);
      buffer.erase(0, eol + 2);
      return true;
    }

    bool eof = false;
    if (!read_from_connection(connection, buffer, eof, err)) {
      return false;
    }
    if (eof) {
      err = "Unexpected EOF while reading line";
      return false;
    }
  }
}

bool parse_status_line(const std::string& status_line, Response& response, std::string& err) {
  const std::size_t first_space = status_line.find(' ');
  if (first_space == std::string::npos) {
    err = "Malformed HTTP status line";
    return false;
  }
  const std::string version = trim_ascii(status_line.substr(0, first_space));
  if (!starts_with(version, "HTTP/")) {
    err = "Malformed HTTP status line";
    return false;
  }
  response.http_version = version;

  const std::size_t second_space = status_line.find(' ', first_space + 1);
  const std::string status_code_str =
      (second_space == std::string::npos)
          ? status_line.substr(first_space + 1)
          : status_line.substr(first_space + 1, second_space - first_space - 1);

  if (status_code_str.empty()) {
    err = "Missing HTTP status code";
    return false;
  }

  int status_code = 0;
  for (char ch : status_code_str) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      err = "Invalid HTTP status code";
      return false;
    }
    status_code = status_code * 10 + (ch - '0');
  }

  response.status_code = status_code;
  response.reason = (second_space == std::string::npos) ? "" : trim_ascii(status_line.substr(second_space + 1));
  return true;
}

bool parse_headers_block(const std::string& headers_block, Response& response, std::string& err) {
  response.headers.clear();

  const std::size_t first_crlf = headers_block.find("\r\n");
  if (first_crlf == std::string::npos) {
    return parse_status_line(headers_block, response, err);
  }

  const std::string status_line = headers_block.substr(0, first_crlf);
  if (!parse_status_line(status_line, response, err)) {
    return false;
  }

  std::size_t offset = first_crlf + 2;
  while (offset < headers_block.size()) {
    const std::size_t next_crlf = headers_block.find("\r\n", offset);
    const std::string line =
        (next_crlf == std::string::npos)
            ? headers_block.substr(offset)
            : headers_block.substr(offset, next_crlf - offset);
    offset = (next_crlf == std::string::npos) ? headers_block.size() : (next_crlf + 2);

    if (line.empty()) {
      break;
    }

    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }

    const std::string name = to_lower_ascii(trim_ascii(line.substr(0, colon)));
    const std::string value = trim_ascii(line.substr(colon + 1));
    if (name.empty()) {
      continue;
    }

    auto it = response.headers.find(name);
    if (it == response.headers.end()) {
      response.headers.emplace(name, value);
    } else {
      it->second.append(",");
      it->second.append(value);
    }
  }

  return true;
}

bool parse_content_length(const std::string& raw, std::size_t& content_length) {
  const std::string value = trim_ascii(raw);
  if (value.empty()) {
    return false;
  }

  std::size_t result = 0;
  for (char ch : value) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }

    const std::size_t digit = static_cast<std::size_t>(ch - '0');
    if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
      return false;
    }
    result = (result * 10) + digit;
  }

  content_length = result;
  return true;
}

bool contains_chunked_encoding(const std::string& transfer_encoding_header) {
  const std::string lower = to_lower_ascii(transfer_encoding_header);
  return lower.find("chunked") != std::string::npos;
}

bool read_response_headers(Connection& connection, std::string& buffer, Response& response, std::string& err) {
  while (true) {
    const std::size_t headers_end = buffer.find("\r\n\r\n");
    if (headers_end != std::string::npos) {
      const std::string headers_block = buffer.substr(0, headers_end);
      buffer.erase(0, headers_end + 4);
      return parse_headers_block(headers_block, response, err);
    }

    if (buffer.size() > kMaxHeaderBytes) {
      err = "HTTP headers exceed maximum size";
      return false;
    }

    bool eof = false;
    if (!read_from_connection(connection, buffer, eof, err)) {
      return false;
    }
    if (eof) {
      err = "Unexpected EOF while reading response headers";
      return false;
    }
  }
}

bool read_exact_bytes(Connection& connection,
                     std::string& buffer,
                     std::size_t bytes_needed,
                     std::string& output,
                     std::string& err) {
  while (buffer.size() < bytes_needed) {
    bool eof = false;
    if (!read_from_connection(connection, buffer, eof, err)) {
      return false;
    }
    if (eof) {
      err = "Unexpected EOF while reading response body";
      return false;
    }
  }

  output.append(buffer.data(), bytes_needed);
  buffer.erase(0, bytes_needed);
  return true;
}

bool decode_chunked_body(Connection& connection, std::string& buffer, std::string& output, std::string& err) {
  output.clear();

  while (true) {
    std::string size_line;
    if (!read_line_crlf(connection, buffer, size_line, err)) {
      return false;
    }

    const std::string trimmed = trim_ascii(size_line);
    const std::size_t semicolon = trimmed.find(';');
    const std::string hex_size = trim_ascii(trimmed.substr(0, semicolon));
    if (hex_size.empty()) {
      err = "Invalid chunk size line";
      return false;
    }

    std::size_t chunk_size = 0;
    for (char ch : hex_size) {
      int digit = 0;
      if (ch >= '0' && ch <= '9') {
        digit = ch - '0';
      } else if (ch >= 'a' && ch <= 'f') {
        digit = 10 + (ch - 'a');
      } else if (ch >= 'A' && ch <= 'F') {
        digit = 10 + (ch - 'A');
      } else {
        err = "Invalid chunk size value";
        return false;
      }

      if (chunk_size > (std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(digit)) / 16) {
        err = "Chunk size overflows platform size";
        return false;
      }
      chunk_size = (chunk_size * 16) + static_cast<std::size_t>(digit);
    }

    if (chunk_size == 0) {
      while (true) {
        std::string trailer_line;
        if (!read_line_crlf(connection, buffer, trailer_line, err)) {
          return false;
        }
        if (trailer_line.empty()) {
          return true;
        }
      }
    }

    if (chunk_size > std::numeric_limits<std::size_t>::max() - 2) {
      err = "Chunk size overflows platform size";
      return false;
    }

    if (!read_exact_bytes(connection, buffer, chunk_size + 2, output, err)) {
      return false;
    }

    if (output.size() < chunk_size + 2) {
      err = "Internal chunk decoding error";
      return false;
    }

    const std::size_t crlf_index = output.size() - 2;
    if (output[crlf_index] != '\r' || output[crlf_index + 1] != '\n') {
      err = "Malformed chunk terminator";
      return false;
    }

    output.resize(output.size() - 2);
  }
}

bool read_content_length_body(Connection& connection,
                              std::string& buffer,
                              std::size_t content_length,
                              std::string& output,
                              std::string& err) {
  output.clear();
  if (content_length == 0) {
    return true;
  }

  if (buffer.size() >= content_length) {
    output.assign(buffer.data(), content_length);
    buffer.erase(0, content_length);
    return true;
  }

  output.swap(buffer);
  while (output.size() < content_length) {
    bool eof = false;
    if (!read_from_connection(connection, output, eof, err)) {
      return false;
    }
    if (eof) {
      err = "Unexpected EOF while reading content-length body";
      return false;
    }
  }

  if (output.size() > content_length) {
    output.resize(content_length);
  }

  return true;
}

bool read_until_eof(Connection& connection, std::string& buffer, std::string& output, std::string& err) {
  output.swap(buffer);
  while (true) {
    bool eof = false;
    if (!read_from_connection(connection, output, eof, err)) {
      return false;
    }
    if (eof) {
      return true;
    }
  }
}

std::string extract_path_only(const std::string& path_and_query) {
  const std::size_t query_pos = path_and_query.find('?');
  if (query_pos == std::string::npos) {
    return path_and_query;
  }
  return path_and_query.substr(0, query_pos);
}

std::string strip_fragment(const std::string& value) {
  const std::size_t fragment_pos = value.find('#');
  if (fragment_pos == std::string::npos) {
    return value;
  }
  return value.substr(0, fragment_pos);
}

std::string normalize_path_and_query(const std::string& input) {
  const std::string without_fragment = strip_fragment(input);

  const std::size_t query_pos = without_fragment.find('?');
  const std::string raw_path = (query_pos == std::string::npos) ? without_fragment : without_fragment.substr(0, query_pos);
  const std::string query = (query_pos == std::string::npos) ? "" : without_fragment.substr(query_pos);

  std::string path = raw_path.empty() ? "/" : raw_path;
  if (!path.empty() && path.front() != '/') {
    path = "/" + path;
  }

  const bool trailing_slash = path.size() > 1 && path.back() == '/';

  std::vector<std::string> segments;
  std::size_t pos = 0;
  while (pos <= path.size()) {
    const std::size_t slash = path.find('/', pos);
    const std::string segment =
        (slash == std::string::npos) ? path.substr(pos) : path.substr(pos, slash - pos);

    if (!segment.empty() && segment != ".") {
      if (segment == "..") {
        if (!segments.empty()) {
          segments.pop_back();
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

  std::string normalized = "/";
  for (std::size_t i = 0; i < segments.size(); ++i) {
    normalized += segments[i];
    if (i + 1 < segments.size()) {
      normalized += "/";
    }
  }

  if (trailing_slash && normalized.size() > 1 && normalized.back() != '/') {
    normalized += "/";
  }

  return normalized + query;
}

bool resolve_redirect(const Url& base, const std::string& location, std::string& resolved, std::string& err) {
  const std::string trimmed_location = trim_ascii(location);
  if (trimmed_location.empty()) {
    err = "Redirect response missing Location value";
    return false;
  }

  const std::string location_lower = to_lower_ascii(trimmed_location);
  if (starts_with(location_lower, "http://") || starts_with(location_lower, "https://")) {
    resolved = trimmed_location;
    return true;
  }

  if (starts_with(trimmed_location, "//")) {
    resolved = base.scheme + ":" + trimmed_location;
    return true;
  }

  const std::string origin = make_origin(base);

  if (trimmed_location.front() == '/') {
    resolved = origin + normalize_path_and_query(trimmed_location);
    return true;
  }

  if (trimmed_location.front() == '?') {
    std::string base_path = extract_path_only(base.path_and_query);
    if (base_path.empty()) {
      base_path = "/";
    }
    resolved = origin + base_path + trimmed_location;
    return true;
  }

  if (trimmed_location.front() == '#') {
    resolved = base.to_string();
    return true;
  }

  std::string base_path = extract_path_only(base.path_and_query);
  if (base_path.empty()) {
    base_path = "/";
  }

  const std::size_t slash = base_path.rfind('/');
  const std::string base_dir = (slash == std::string::npos) ? "/" : base_path.substr(0, slash + 1);
  resolved = origin + normalize_path_and_query(base_dir + trimmed_location);
  return true;
}

bool error_indicates_timeout(const std::string& error_message) {
  if (error_message.empty()) {
    return false;
  }

  const std::string lower_error = to_lower_ascii(error_message);
  return lower_error.find("timed out") != std::string::npos;
}

Response fetch_once(const Url& url,
                   const std::map<std::string, std::string>& extra_headers,
                   int timeout_seconds) {
  Response response;

  Connection connection;
  std::string err;
  if (!connection.Open(url, timeout_seconds, err)) {
    response.error = err;
    return response;
  }

  std::string target = url.path_and_query;
  if (target.empty()) {
    target = "/";
  }
  if (!target.empty() && target.front() != '/') {
    target = "/" + target;
  }

  std::string request;
  request.reserve(512);
  request += "GET " + target + " HTTP/1.1\r\n";
  request += "Host: " + make_host_header(url) + "\r\n";
  request += "User-Agent: from_scratch_browser/0.1\r\n";
  request += "Accept: */*\r\n";
  request += "Connection: close\r\n";
  for (const auto& [name, value] : extra_headers) {
    const std::string lower_name = to_lower_ascii(name);
    if (lower_name == "host" || lower_name == "user-agent" || lower_name == "accept" ||
        lower_name == "connection") {
      continue;
    }
    request += name + ": " + value + "\r\n";
  }
  request += "\r\n";

  if (!connection.WriteAll(request, err)) {
    response.error = err;
    return response;
  }

  std::string buffer;
  if (!read_response_headers(connection, buffer, response, err)) {
    response.error = err;
    return response;
  }

  const auto transfer_it = response.headers.find("transfer-encoding");
  if (transfer_it != response.headers.end() && contains_chunked_encoding(transfer_it->second)) {
    if (!decode_chunked_body(connection, buffer, response.body, err)) {
      response.error = err;
      return response;
    }
    return response;
  }

  const auto content_length_it = response.headers.find("content-length");
  if (content_length_it != response.headers.end()) {
    std::size_t content_length = 0;
    if (!parse_content_length(content_length_it->second, content_length)) {
      response.error = "Invalid Content-Length header";
      return response;
    }

    if (!read_content_length_body(connection, buffer, content_length, response.body, err)) {
      response.error = err;
      return response;
    }
    return response;
  }

  if (!read_until_eof(connection, buffer, response.body, err)) {
    response.error = err;
    return response;
  }

  return response;
}

}  // namespace

Response fetch_with_headers(const std::string& url,
                            const std::map<std::string, std::string>& request_headers,
                            int max_redirects,
                            int timeout_seconds) {
  Response response;
  const auto fetch_start = std::chrono::steady_clock::now();
  auto finalize_response = [&](Response& current_response) -> Response {
    current_response.total_duration_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - fetch_start).count();
    if (!current_response.error.empty() && error_indicates_timeout(current_response.error)) {
      current_response.timed_out = true;
    }
    return current_response;
  };

  if (max_redirects < 0) {
    max_redirects = 0;
  }

  std::string current_url = url;

  for (int redirect_count = 0;; ++redirect_count) {
    Url parsed_url;
    std::string parse_error;
    if (!parse_url(current_url, parsed_url, parse_error)) {
      response.error = parse_error;
      response.final_url = current_url;
      return finalize_response(response);
    }

    response = fetch_once(parsed_url, request_headers, timeout_seconds);
    response.final_url = current_url;
    if (!response.error.empty()) {
      return finalize_response(response);
    }

    if (!is_redirect_status(response.status_code)) {
      return finalize_response(response);
    }

    const auto location_it = response.headers.find("location");
    if (location_it == response.headers.end() || trim_ascii(location_it->second).empty()) {
      return finalize_response(response);
    }

    if (redirect_count >= max_redirects) {
      response.error = "Too many redirects";
      return finalize_response(response);
    }

    std::string next_url;
    std::string resolve_error;
    if (!resolve_redirect(parsed_url, location_it->second, next_url, resolve_error)) {
      response.error = resolve_error;
      return finalize_response(response);
    }

    current_url = next_url;
  }
}

Response fetch(const std::string& url, int max_redirects, int timeout_seconds) {
  static const std::map<std::string, std::string> kNoHeaders;
  return fetch_with_headers(url, kNoHeaders, max_redirects, timeout_seconds);
}

bool parse_http_status_line(const std::string& status_line,
                            std::string& http_version,
                            int& status_code,
                            std::string& reason,
                            std::string& err) {
    Response response;
    if (!parse_status_line(status_line, response, err)) {
        http_version.clear();
        status_code = 0;
        reason.clear();
        return false;
    }
    http_version = response.http_version;
    status_code = response.status_code;
    reason = response.reason;
    err.clear();
    return true;
}

const char* request_method_name(RequestMethod method) {
    switch (method) {
        case RequestMethod::Get: return "GET";
        case RequestMethod::Head: return "HEAD";
    }
    return "UNKNOWN";
}

const char* request_stage_name(RequestStage stage) {
    switch (stage) {
        case RequestStage::Created: return "Created";
        case RequestStage::Dispatched: return "Dispatched";
        case RequestStage::Received: return "Received";
        case RequestStage::Complete: return "Complete";
        case RequestStage::Error: return "Error";
    }
    return "Unknown";
}

void RequestTransaction::record(RequestStage stage, const std::string& detail) {
    RequestEvent event;
    event.stage = stage;
    event.timestamp = std::chrono::steady_clock::now();
    event.detail = detail;
    events.push_back(event);
}

RequestTransaction fetch_with_contract(const std::string& url, const FetchOptions& options) {
    RequestTransaction txn;
    txn.request.method = RequestMethod::Get;
    txn.request.url = url;
    txn.record(RequestStage::Created);

    if (options.observer) {
        options.observer(txn, RequestStage::Created);
    }

    txn.record(RequestStage::Dispatched);
    if (options.observer) {
        options.observer(txn, RequestStage::Dispatched);
    }

    txn.response = fetch(url, options.max_redirects, options.timeout_seconds);

    txn.record(RequestStage::Received);
    if (options.observer) {
        options.observer(txn, RequestStage::Received);
    }

    if (!txn.response.error.empty()) {
        txn.record(RequestStage::Error, txn.response.error);
        if (options.observer) {
            options.observer(txn, RequestStage::Error);
        }
    } else {
        txn.record(RequestStage::Complete,
                    "status=" + std::to_string(txn.response.status_code));
        if (options.observer) {
            options.observer(txn, RequestStage::Complete);
        }
    }

    return txn;
}

const char* cache_policy_name(CachePolicy policy) {
    switch (policy) {
        case CachePolicy::NoCache: return "NoCache";
        case CachePolicy::CacheAll: return "CacheAll";
    }
    return "Unknown";
}

ResponseCache::ResponseCache(CachePolicy policy) : policy_(policy) {}

void ResponseCache::set_policy(CachePolicy policy) {
    policy_ = policy;
}

CachePolicy ResponseCache::policy() const {
    return policy_;
}

bool ResponseCache::lookup(const std::string& url, Response& out) const {
    if (policy_ == CachePolicy::NoCache) {
        return false;
    }

    auto it = entries_.find(url);
    if (it == entries_.end()) {
        return false;
    }

    out = it->second.response;
    return true;
}

void ResponseCache::store(const std::string& url, const Response& response) {
    if (policy_ == CachePolicy::NoCache) {
        return;
    }

    if (!response.error.empty()) {
        return;
    }

    CacheEntry entry;
    entry.response = response;
    entry.cached_at = std::chrono::steady_clock::now();
    entries_[url] = std::move(entry);
}

void ResponseCache::clear() {
    entries_.clear();
}

std::size_t ResponseCache::size() const {
    return entries_.size();
}

const char* policy_violation_name(PolicyViolation violation) {
    switch (violation) {
        case PolicyViolation::None: return "None";
        case PolicyViolation::TooManyRedirects: return "TooManyRedirects";
        case PolicyViolation::CrossOriginBlocked: return "CrossOriginBlocked";
        case PolicyViolation::CorsResponseBlocked: return "CorsResponseBlocked";
        case PolicyViolation::CspConnectSrcBlocked: return "CspConnectSrcBlocked";
        case PolicyViolation::UnsupportedScheme: return "UnsupportedScheme";
        case PolicyViolation::EmptyUrl: return "EmptyUrl";
    }
    return "Unknown";
}

PolicyCheckResult check_request_policy(const std::string& url, const RequestPolicy& policy) {
    if (url.empty()) {
        return {false, PolicyViolation::EmptyUrl, "URL is empty"};
    }

    Url parsed;
    std::string err;
    if (!parse_url(url, parsed, err)) {
        // If it's a file path, check file scheme
        if (is_file_url(url)) {
            // file:// is handled separately
        } else {
            return {false, PolicyViolation::UnsupportedScheme,
                    "Failed to parse URL: " + err};
        }
    }

    // Check scheme
    if (!parsed.scheme.empty()) {
        bool scheme_allowed = false;
        for (const auto& allowed : policy.allowed_schemes) {
            if (parsed.scheme == allowed) {
                scheme_allowed = true;
                break;
            }
        }
        if (!scheme_allowed) {
            return {false, PolicyViolation::UnsupportedScheme,
                    "Scheme '" + parsed.scheme + "' not allowed"};
        }
    }

    // Check cross-origin
    if (!policy.allow_cross_origin && !policy.origin.empty()) {
        const std::string request_origin = canonicalize_origin(build_origin_for_url(parsed));
        const std::string policy_origin = canonicalize_origin(policy.origin);
        if (request_origin != policy_origin) {
            return {false, PolicyViolation::CrossOriginBlocked,
                    "Cross-origin request blocked: " + request_origin +
                    " != " + policy_origin};
        }
    }

    if (!csp_connect_src_allows_url(url, policy)) {
        return {false, PolicyViolation::CspConnectSrcBlocked,
                "CSP connect-src blocked request: " + url};
    }

    return {true, PolicyViolation::None, ""};
}

std::map<std::string, std::string> build_request_headers_for_policy(
    const std::string& url,
    const RequestPolicy& policy) {
    std::map<std::string, std::string> headers;
    if (!policy.attach_origin_header_for_cors || policy.origin.empty()) {
        return headers;
    }

    Url parsed;
    std::string err;
    if (!parse_url(url, parsed, err)) {
        return headers;
    }

    const std::string request_origin = canonicalize_origin(build_origin_for_url(parsed));
    const std::string policy_origin = canonicalize_origin(policy.origin);
    if (request_origin != policy_origin) {
        headers["Origin"] = policy_origin.empty() ? policy.origin : policy_origin;
    }
    return headers;
}

PolicyCheckResult check_cors_response_policy(const std::string& url,
                                             const Response& response,
                                             const RequestPolicy& policy) {
    if (policy.origin.empty() || !policy.require_acao_for_cross_origin) {
        return {true, PolicyViolation::None, ""};
    }

    Url parsed;
    std::string err;
    if (!parse_url(url, parsed, err)) {
        return {true, PolicyViolation::None, ""};
    }

    const std::string request_origin = canonicalize_origin(build_origin_for_url(parsed));
    const std::string policy_origin = canonicalize_origin(policy.origin);

    if (request_origin == policy_origin) {
        return {true, PolicyViolation::None, ""};
    }

    const std::string* acao_header =
        find_header_value_case_insensitive(response.headers, "access-control-allow-origin");
    if (!acao_header) {
        return {false,
                PolicyViolation::CorsResponseBlocked,
                "Cross-origin response blocked: missing Access-Control-Allow-Origin"};
    }

    const std::string acao_value = trim_ascii(*acao_header);
    if (acao_value.empty() || acao_value.find(',') != std::string::npos) {
        return {false,
                PolicyViolation::CorsResponseBlocked,
                "Cross-origin response blocked: invalid multi-valued Access-Control-Allow-Origin"};
    }

    bool allows_origin = false;
    bool wildcard_origin = false;
    if (acao_value == "*") {
        wildcard_origin = true;
        allows_origin = true;
    } else {
        const std::size_t scheme_end = acao_value.find("://");
        if (scheme_end == std::string::npos || scheme_end + 3 >= acao_value.size()) {
            return {false,
                    PolicyViolation::CorsResponseBlocked,
                    "Cross-origin response blocked: ACAO must be a serialized origin"};
        }
        const std::size_t authority_end =
            acao_value.find_first_of("/?#", scheme_end + 3);
        if (authority_end != std::string::npos) {
            return {false,
                    PolicyViolation::CorsResponseBlocked,
                    "Cross-origin response blocked: ACAO must be a serialized origin"};
        }
        const std::size_t userinfo_delim = acao_value.find('@', scheme_end + 3);
        if (userinfo_delim != std::string::npos) {
            return {false,
                    PolicyViolation::CorsResponseBlocked,
                    "Cross-origin response blocked: ACAO must be a serialized origin"};
        }
        Url acao_origin;
        std::string acao_err;
        if (!parse_url(acao_value, acao_origin, acao_err)) {
            return {false,
                    PolicyViolation::CorsResponseBlocked,
                    "Cross-origin response blocked: ACAO must be a serialized origin"};
        }
        if (canonicalize_origin(acao_value) == policy_origin) {
            allows_origin = true;
        }
    }

    if (!allows_origin) {
        return {false,
                PolicyViolation::CorsResponseBlocked,
                "Cross-origin response blocked: ACAO does not allow origin " + policy_origin};
    }

    if (policy.credentials_mode_include) {
        if (wildcard_origin) {
            return {false,
                    PolicyViolation::CorsResponseBlocked,
                    "Cross-origin response blocked: ACAO '*' disallowed for credentialed CORS"};
        }
        if (policy.require_acac_for_credentialed_cors) {
            const std::string* acac_header = find_header_value_case_insensitive(
                response.headers, "access-control-allow-credentials");
            if (!acac_header || trim_ascii(*acac_header) != "true") {
                return {false,
                        PolicyViolation::CorsResponseBlocked,
                        "Cross-origin response blocked: missing Access-Control-Allow-Credentials=true"};
            }
        }
    }

    return {true, PolicyViolation::None, ""};
}

RequestTransaction fetch_with_policy_contract(const std::string& url,
                                              const RequestPolicy& policy,
                                              const FetchOptions& options) {
    RequestTransaction txn;
    txn.request.method = RequestMethod::Get;
    txn.request.url = url;
    txn.record(RequestStage::Created);

    if (options.observer) {
        options.observer(txn, RequestStage::Created);
    }

    const PolicyCheckResult request_check = check_request_policy(url, policy);
    if (!request_check.allowed) {
        txn.record(RequestStage::Error, request_check.message);
        txn.response.error = request_check.message;
        if (options.observer) {
            options.observer(txn, RequestStage::Error);
        }
        return txn;
    }

    txn.request.headers = build_request_headers_for_policy(url, policy);

    txn.record(RequestStage::Dispatched);
    if (options.observer) {
        options.observer(txn, RequestStage::Dispatched);
    }

    txn.response = fetch_with_headers(url, txn.request.headers, options.max_redirects, options.timeout_seconds);

    txn.record(RequestStage::Received);
    if (options.observer) {
        options.observer(txn, RequestStage::Received);
    }

    if (txn.response.error.empty()) {
        const std::string& response_policy_url =
            txn.response.final_url.empty() ? url : txn.response.final_url;
        const PolicyCheckResult response_check =
            check_cors_response_policy(response_policy_url, txn.response, policy);
        if (!response_check.allowed) {
            txn.response.error = response_check.message;
        }
    }

    if (!txn.response.error.empty()) {
        txn.record(RequestStage::Error, txn.response.error);
        if (options.observer) {
            options.observer(txn, RequestStage::Error);
        }
    } else {
        txn.record(RequestStage::Complete,
                    "status=" + std::to_string(txn.response.status_code));
        if (options.observer) {
            options.observer(txn, RequestStage::Complete);
        }
    }

    return txn;
}

}  // namespace browser::net
