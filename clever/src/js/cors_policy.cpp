#include <clever/js/cors_policy.h>

#include <clever/url/url.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace clever::js::cors {
namespace {

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool has_invalid_header_octet(std::string_view value) {
    for (unsigned char ch : value) {
        if (ch <= 0x1f || ch == 0x7f) {
            return true;
        }
    }
    return false;
}

bool has_strict_authority_port_syntax(std::string_view authority) {
    if (authority.empty()) {
        return false;
    }

    std::size_t host_end = authority.size();
    std::size_t port_start = std::string::npos;

    if (authority.front() == '[') {
        const std::size_t closing = authority.find(']');
        if (closing == std::string::npos) {
            return false;
        }
        host_end = closing + 1;
        if (host_end < authority.size()) {
            if (authority[host_end] != ':') {
                return false;
            }
            port_start = host_end + 1;
        }
    } else {
        const std::size_t first_colon = authority.find(':');
        if (first_colon != std::string::npos) {
            if (authority.find(':', first_colon + 1) != std::string::npos) {
                return false;
            }
            host_end = first_colon;
            port_start = first_colon + 1;
        }
    }

    if (host_end == 0) {
        return false;
    }

    if (port_start == std::string::npos) {
        return true;
    }
    if (port_start >= authority.size()) {
        return false;
    }

    for (std::size_t i = port_start; i < authority.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(authority[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<clever::url::URL> parse_httpish_url(std::string_view input) {
    auto parsed = clever::url::parse(input);
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    if (parsed->scheme != "http" && parsed->scheme != "https") {
        return std::nullopt;
    }

    return parsed;
}

bool is_serialized_http_origin(std::string_view origin) {
    if (has_invalid_header_octet(origin)) {
        return false;
    }

    auto parsed = parse_httpish_url(origin);
    if (!parsed.has_value()) {
        return false;
    }

    return parsed->origin() == origin;
}

std::optional<std::string> parse_canonical_serialized_origin(std::string_view input) {
    std::string trimmed = trim_copy(std::string(input));
    if (trimmed.empty() || has_invalid_header_octet(trimmed)) {
        return std::nullopt;
    }

    if (trimmed == "null") {
        return std::string("null");
    }

    const std::size_t scheme_end = trimmed.find("://");
    if (scheme_end == std::string::npos || scheme_end + 3 >= trimmed.size()) {
        return std::nullopt;
    }
    if (trimmed.find_first_of("/?#", scheme_end + 3) != std::string::npos) {
        return std::nullopt;
    }
    if (trimmed.find('@', scheme_end + 3) != std::string::npos) {
        return std::nullopt;
    }
    if (!has_strict_authority_port_syntax(trimmed.substr(scheme_end + 3))) {
        return std::nullopt;
    }

    auto parsed = parse_httpish_url(to_lower_ascii(trimmed));
    if (!parsed.has_value() || parsed->host.empty()) {
        return std::nullopt;
    }

    std::string origin = parsed->scheme + "://" + parsed->host;
    if (parsed->port.has_value()) {
        const uint16_t default_port = parsed->scheme == "https" ? 443 : 80;
        if (parsed->port.value() != default_port) {
            origin += ":" + std::to_string(parsed->port.value());
        }
    }
    return origin;
}

bool canonical_origins_match(std::string_view left, std::string_view right) {
    auto left_origin = parse_canonical_serialized_origin(left);
    if (!left_origin.has_value()) {
        return false;
    }

    auto right_origin = parse_canonical_serialized_origin(right);
    if (!right_origin.has_value()) {
        return false;
    }

    return left_origin.value() == right_origin.value();
}

bool is_invalid_document_origin(std::string_view document_origin) {
    if (document_origin.empty() || document_origin == "null") {
        return false;
    }
    return !is_serialized_http_origin(document_origin);
}

bool is_null_document_origin(std::string_view document_origin) {
    return document_origin == "null";
}

} // namespace

bool has_enforceable_document_origin(std::string_view document_origin) {
    return is_serialized_http_origin(document_origin);
}

bool is_cross_origin(std::string_view document_origin, std::string_view request_url) {
    auto request = parse_httpish_url(request_url);
    if (!request.has_value()) {
        return false;
    }

    if (is_null_document_origin(document_origin)) {
        return true;
    }

    if (!has_enforceable_document_origin(document_origin)) {
        return false;
    }

    return request->origin() != document_origin;
}

bool should_attach_origin_header(std::string_view document_origin, std::string_view request_url) {
    return (has_enforceable_document_origin(document_origin) ||
            is_null_document_origin(document_origin)) &&
           is_cross_origin(document_origin, request_url);
}

bool cors_allows_response(std::string_view document_origin,
                          std::string_view request_url,
                          const clever::net::HeaderMap& response_headers,
                          bool credentials_requested) {
    if (document_origin.empty() || is_invalid_document_origin(document_origin)) {
        return false;
    }

    if ((has_enforceable_document_origin(document_origin) ||
         is_null_document_origin(document_origin)) &&
        !parse_httpish_url(request_url).has_value()) {
        return false;
    }

    if (!is_cross_origin(document_origin, request_url)) {
        return true;
    }

    auto acao_values = response_headers.get_all("access-control-allow-origin");
    if (acao_values.size() != 1) {
        return false;
    }

    std::string acao = trim_copy(acao_values.front());
    if (acao.empty()) {
        return false;
    }
    if (has_invalid_header_octet(acao)) {
        return false;
    }
    if (acao.find(',') != std::string::npos) {
        return false;
    }

    const std::string expected_origin =
        is_null_document_origin(document_origin) ? "null" : std::string(document_origin);

    if (!credentials_requested) {
        return acao == "*" || canonical_origins_match(acao, expected_origin);
    }

    if (!canonical_origins_match(acao, expected_origin)) {
        return false;
    }

    auto acac_values = response_headers.get_all("access-control-allow-credentials");
    if (acac_values.size() != 1) {
        return false;
    }

    std::string acac = trim_copy(acac_values.front());
    if (acac.empty() || has_invalid_header_octet(acac)) {
        return false;
    }
    return acac == "true";
}

} // namespace clever::js::cors
