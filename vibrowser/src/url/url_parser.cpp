#include <clever/url/url.h>
#include <clever/url/percent_encoding.h>
#include <clever/url/idna.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clever::url {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::optional<uint16_t> default_port_for_scheme(std::string_view scheme) {
    if (scheme == "http")  return 80;
    if (scheme == "https") return 443;
    if (scheme == "ftp")   return 21;
    if (scheme == "ws")    return 80;
    if (scheme == "wss")   return 443;
    return std::nullopt;
}

bool is_special_scheme(std::string_view scheme) {
    return scheme == "http" || scheme == "https" ||
           scheme == "ftp" || scheme == "ws" ||
           scheme == "wss" || scheme == "file";
}

bool is_ascii_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ascii_alphanumeric(char c) {
    return is_ascii_alpha(c) || is_ascii_digit(c);
}

bool is_ascii_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// Strip leading and trailing C0 control characters and spaces
std::string_view trim_input(std::string_view input) {
    size_t start = 0;
    while (start < input.size() &&
           (input[start] == ' ' || input[start] == '\t' ||
            input[start] == '\n' || input[start] == '\r' ||
            (static_cast<unsigned char>(input[start]) <= 0x1F))) {
        ++start;
    }
    size_t end = input.size();
    while (end > start &&
           (input[end - 1] == ' ' || input[end - 1] == '\t' ||
            input[end - 1] == '\n' || input[end - 1] == '\r' ||
            (static_cast<unsigned char>(input[end - 1]) <= 0x1F))) {
        --end;
    }
    return input.substr(start, end - start);
}

// Remove all ASCII tab and newline characters from input
std::string remove_tab_newline(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (c != '\t' && c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

// Percent-encode for the userinfo encode set
std::string percent_encode_userinfo(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~') {
            result += static_cast<char>(c);
        } else if (c == '!' || c == '$' || c == '&' || c == '\'' ||
                   c == '(' || c == ')' || c == '*' || c == '+' ||
                   c == ',' || c == ';' || c == '=') {
            result += static_cast<char>(c);
        } else {
            static const char hex[] = "0123456789ABCDEF";
            result += '%';
            result += hex[(c >> 4) & 0xF];
            result += hex[c & 0xF];
        }
    }
    return result;
}

// Percent-encode for the path encode set
std::string percent_encode_path(std::string_view input) {
    return percent_encode(input, false);
}

// Percent-encode for query strings (preserves existing %XX sequences)
std::string percent_encode_query(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = input[i];
        // Preserve existing percent-encoded sequences
        if (c == '%' && i + 2 < input.size() &&
            is_ascii_hex(input[i + 1]) && is_ascii_hex(input[i + 2])) {
            result += input[i];
            result += input[i + 1];
            result += input[i + 2];
            i += 2;
        } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~' ||
            c == '!' || c == '$' || c == '&' || c == '\'' ||
            c == '(' || c == ')' || c == '*' || c == '+' ||
            c == ',' || c == ';' || c == '=' || c == ':' ||
            c == '@' || c == '/' || c == '?') {
            result += static_cast<char>(c);
        } else {
            static const char hex[] = "0123456789ABCDEF";
            result += '%';
            result += hex[(c >> 4) & 0xF];
            result += hex[c & 0xF];
        }
    }
    return result;
}

// Percent-encode for fragment
std::string percent_encode_fragment(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~' ||
            c == '!' || c == '$' || c == '&' || c == '\'' ||
            c == '(' || c == ')' || c == '*' || c == '+' ||
            c == ',' || c == ';' || c == '=' || c == ':' ||
            c == '@' || c == '/' || c == '?') {
            result += static_cast<char>(c);
        } else {
            static const char hex[] = "0123456789ABCDEF";
            result += '%';
            result += hex[(c >> 4) & 0xF];
            result += hex[c & 0xF];
        }
    }
    return result;
}

// Resolve dot segments in a path per RFC 3986
std::string resolve_dot_segments(std::string_view path) {
    if (path.empty()) return std::string{path};

    std::vector<std::string> segments;
    size_t pos = 0;

    bool has_leading_slash = (!path.empty() && path[0] == '/');
    if (has_leading_slash) pos = 1;

    while (pos <= path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string_view::npos) next = path.size();

        std::string_view segment = path.substr(pos, next - pos);

        if (segment == ".") {
            // skip
        } else if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
        } else {
            segments.emplace_back(segment);
        }

        if (next >= path.size()) break;
        pos = next + 1;
    }

    std::string result;
    if (has_leading_slash) {
        result += '/';
    }
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result += '/';
        result += segments[i];
    }

    // Preserve trailing slash if the original path ended with /. or /..
    if (path.size() >= 2) {
        std::string_view last2 = path.substr(path.size() - 2);
        if (last2 == "/." || last2 == "..") {
            if (result.empty() || result.back() != '/') {
                result += '/';
            }
        }
    }

    return result;
}

// Merge a relative path with a base URL path
std::string merge_paths(const URL& base, std::string_view relative_path) {
    if (!base.host.empty() && base.path.empty()) {
        return "/" + std::string(relative_path);
    }

    auto last_slash = base.path.rfind('/');
    if (last_slash != std::string::npos) {
        return std::string(base.path.substr(0, last_slash + 1)) + std::string(relative_path);
    }

    return std::string(relative_path);
}

// Parse a port string
std::optional<std::optional<uint16_t>> parse_port(std::string_view port_str, std::string_view scheme) {
    if (port_str.empty()) {
        return std::optional<uint16_t>{std::nullopt};
    }

    for (char c : port_str) {
        if (!is_ascii_digit(c)) return std::nullopt;
    }

    uint32_t port_val = 0;
    for (char c : port_str) {
        port_val = port_val * 10 + static_cast<uint32_t>(c - '0');
        if (port_val > 65535) return std::nullopt;
    }

    auto port = static_cast<uint16_t>(port_val);

    auto def = default_port_for_scheme(scheme);
    if (def.has_value() && def.value() == port) {
        return std::optional<uint16_t>{std::nullopt};
    }

    return std::optional<uint16_t>{port};
}

// Parse host, handling IPv6 brackets
std::optional<std::string> parse_host_string(std::string_view host_str, bool special) {
    if (host_str.empty()) {
        return std::string{};
    }

    if (host_str.front() == '[') {
        if (host_str.back() != ']') {
            return std::nullopt;
        }
        return std::string{host_str};
    }

    if (special) {
        auto ascii_domain = domain_to_ascii(host_str);
        if (!ascii_domain.has_value()) {
            return std::nullopt;
        }
        return ascii_domain.value();
    }

    return percent_encode(host_str, true);
}

// Parse the tail: path?query#fragment
void parse_path_query_fragment(
    std::string_view rest,
    bool special,
    std::string& out_path,
    std::string& out_query,
    std::string& out_fragment)
{
    if (rest.empty()) {
        if (special) {
            out_path = "/";
        }
        return;
    }

    std::string_view path_str;
    std::string_view query_str;
    std::string_view frag_str;

    auto q_pos = rest.find('?');
    auto h_pos = rest.find('#');

    if (q_pos != std::string_view::npos &&
        (h_pos == std::string_view::npos || q_pos < h_pos)) {
        path_str = rest.substr(0, q_pos);
        if (h_pos != std::string_view::npos) {
            query_str = rest.substr(q_pos + 1, h_pos - q_pos - 1);
            frag_str = rest.substr(h_pos + 1);
        } else {
            query_str = rest.substr(q_pos + 1);
        }
    } else if (h_pos != std::string_view::npos) {
        path_str = rest.substr(0, h_pos);
        frag_str = rest.substr(h_pos + 1);
    } else {
        path_str = rest;
    }

    if (!path_str.empty()) {
        out_path = resolve_dot_segments(percent_encode_path(path_str));
    } else if (special) {
        out_path = "/";
    }
    if (!query_str.empty()) out_query = percent_encode_query(query_str);
    if (!frag_str.empty()) out_fragment = percent_encode_fragment(frag_str);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// parse() - WHATWG URL parsing
// ---------------------------------------------------------------------------
std::optional<URL> parse(std::string_view raw_input, const URL* base) {
    auto trimmed = trim_input(raw_input);

    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::string input = remove_tab_newline(trimmed);

    if (input.empty()) {
        return std::nullopt;
    }

    URL url;
    size_t pos = 0;

    // ------- Scheme extraction -------
    bool has_scheme = false;
    if (pos < input.size() && is_ascii_alpha(input[pos])) {
        size_t scheme_end = pos + 1;
        while (scheme_end < input.size() &&
               (is_ascii_alphanumeric(input[scheme_end]) ||
                input[scheme_end] == '+' || input[scheme_end] == '-' ||
                input[scheme_end] == '.')) {
            ++scheme_end;
        }
        if (scheme_end < input.size() && input[scheme_end] == ':') {
            std::string scheme_str(input.substr(pos, scheme_end - pos));
            std::transform(scheme_str.begin(), scheme_str.end(), scheme_str.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            url.scheme = std::move(scheme_str);
            pos = scheme_end + 1;
            has_scheme = true;
        }
    }

    if (!has_scheme) {
        // No scheme - this must be a relative URL
        if (!base) {
            return std::nullopt;
        }

        url.scheme = base->scheme;
        std::string_view sv(input);

        if (sv[pos] == '#') {
            url.username = base->username;
            url.password = base->password;
            url.host = base->host;
            url.port = base->port;
            url.path = base->path;
            url.query = base->query;
            url.fragment = percent_encode_fragment(sv.substr(pos + 1));
            return url;
        }

        if (sv[pos] == '?') {
            url.username = base->username;
            url.password = base->password;
            url.host = base->host;
            url.port = base->port;
            url.path = base->path;

            std::string_view rest = sv.substr(pos + 1);
            auto h_pos = rest.find('#');
            if (h_pos != std::string_view::npos) {
                url.query = percent_encode_query(rest.substr(0, h_pos));
                url.fragment = percent_encode_fragment(rest.substr(h_pos + 1));
            } else {
                url.query = percent_encode_query(rest);
            }
            return url;
        }

        if (sv.size() >= pos + 2 && sv[pos] == '/' && sv[pos + 1] == '/') {
            // Scheme-relative
            pos += 2;
            // Fall through to authority parsing
        } else if (sv[pos] == '/') {
            // Path-absolute
            url.username = base->username;
            url.password = base->password;
            url.host = base->host;
            url.port = base->port;

            std::string_view rest = sv.substr(pos);
            parse_path_query_fragment(rest, is_special_scheme(url.scheme),
                                      url.path, url.query, url.fragment);
            return url;
        } else {
            // Path-relative
            url.username = base->username;
            url.password = base->password;
            url.host = base->host;
            url.port = base->port;

            std::string_view rest = sv.substr(pos);
            std::string_view rel_path = rest;
            std::string_view query_str;
            std::string_view frag_str;

            auto q_pos = rest.find('?');
            auto h_pos = rest.find('#');

            if (q_pos != std::string_view::npos &&
                (h_pos == std::string_view::npos || q_pos < h_pos)) {
                rel_path = rest.substr(0, q_pos);
                if (h_pos != std::string_view::npos) {
                    query_str = rest.substr(q_pos + 1, h_pos - q_pos - 1);
                    frag_str = rest.substr(h_pos + 1);
                } else {
                    query_str = rest.substr(q_pos + 1);
                }
            } else if (h_pos != std::string_view::npos) {
                rel_path = rest.substr(0, h_pos);
                frag_str = rest.substr(h_pos + 1);
            }

            std::string merged = merge_paths(*base, percent_encode_path(rel_path));
            url.path = resolve_dot_segments(merged);
            if (!query_str.empty()) url.query = percent_encode_query(query_str);
            if (!frag_str.empty()) url.fragment = percent_encode_fragment(frag_str);

            return url;
        }
    }

    // We have a scheme. pos is past the ':'
    bool is_special = is_special_scheme(url.scheme);
    bool has_authority = false;

    if (has_scheme && !is_special) {
        std::string_view rest = std::string_view(input).substr(pos);

        if (rest.size() >= 2 && rest[0] == '/' && rest[1] == '/') {
            has_authority = true;
            pos += 2;
        } else {
            // Opaque path (data:, blob:, etc.)
            std::string_view path_str = rest;
            std::string_view query_str;
            std::string_view frag_str;

            auto q_pos = rest.find('?');
            auto h_pos = rest.find('#');

            if (q_pos != std::string_view::npos &&
                (h_pos == std::string_view::npos || q_pos < h_pos)) {
                path_str = rest.substr(0, q_pos);
                if (h_pos != std::string_view::npos) {
                    query_str = rest.substr(q_pos + 1, h_pos - q_pos - 1);
                    frag_str = rest.substr(h_pos + 1);
                } else {
                    query_str = rest.substr(q_pos + 1);
                }
            } else if (h_pos != std::string_view::npos) {
                path_str = rest.substr(0, h_pos);
                frag_str = rest.substr(h_pos + 1);
            }

            url.path = std::string{path_str};
            if (!query_str.empty()) url.query = std::string{query_str};
            if (!frag_str.empty()) url.fragment = std::string{frag_str};

            return url;
        }
    } else if (has_scheme && is_special) {
        std::string_view rest = std::string_view(input).substr(pos);
        if (rest.size() >= 2 && rest[0] == '/' && rest[1] == '/') {
            has_authority = true;
            pos += 2;
        } else if (url.scheme == "file") {
            has_authority = true;
        } else {
            return std::nullopt;
        }
    }

    if (!has_scheme) {
        has_authority = true;
    }

    if (has_authority) {
        std::string_view rest = std::string_view(input).substr(pos);

        size_t auth_end = 0;
        bool in_brackets = false;
        while (auth_end < rest.size()) {
            char c = rest[auth_end];
            if (c == '[') in_brackets = true;
            if (c == ']') in_brackets = false;
            if (!in_brackets && (c == '/' || c == '?' || c == '#')) break;
            ++auth_end;
        }

        std::string_view authority = rest.substr(0, auth_end);
        std::string_view after_authority = rest.substr(auth_end);

        std::string_view userinfo_str;
        std::string_view host_port;

        auto at_pos = authority.rfind('@');
        if (at_pos != std::string_view::npos) {
            userinfo_str = authority.substr(0, at_pos);
            host_port = authority.substr(at_pos + 1);
        } else {
            host_port = authority;
        }

        if (!userinfo_str.empty()) {
            auto colon_pos = userinfo_str.find(':');
            if (colon_pos != std::string_view::npos) {
                url.username = percent_encode_userinfo(userinfo_str.substr(0, colon_pos));
                url.password = percent_encode_userinfo(userinfo_str.substr(colon_pos + 1));
            } else {
                url.username = percent_encode_userinfo(userinfo_str);
            }
        }

        std::string_view host_str;
        std::string_view port_str;

        if (!host_port.empty() && host_port[0] == '[') {
            auto bracket_end = host_port.find(']');
            if (bracket_end == std::string_view::npos) {
                return std::nullopt;
            }
            host_str = host_port.substr(0, bracket_end + 1);
            if (bracket_end + 1 < host_port.size()) {
                if (host_port[bracket_end + 1] == ':') {
                    port_str = host_port.substr(bracket_end + 2);
                } else {
                    return std::nullopt;
                }
            }
        } else {
            auto colon_pos = host_port.rfind(':');
            if (colon_pos != std::string_view::npos) {
                host_str = host_port.substr(0, colon_pos);
                port_str = host_port.substr(colon_pos + 1);
            } else {
                host_str = host_port;
            }
        }

        auto parsed_host = parse_host_string(host_str, is_special);
        if (!parsed_host.has_value()) {
            return std::nullopt;
        }
        url.host = parsed_host.value();

        if (!port_str.empty()) {
            auto parsed_port = parse_port(port_str, url.scheme);
            if (!parsed_port.has_value()) {
                return std::nullopt;
            }
            url.port = parsed_port.value();
        }

        parse_path_query_fragment(after_authority, is_special,
                                  url.path, url.query, url.fragment);

        if (is_special && (url.path.empty() || url.path[0] != '/')) {
            url.path = "/" + url.path;
        }
    }

    return url;
}

} // namespace clever::url
