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

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
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

bool is_invalid_document_origin(std::string_view document_origin) {
    if (document_origin.empty() || document_origin == "null") {
        return false;
    }
    return !is_serialized_http_origin(document_origin);
}

} // namespace

bool has_enforceable_document_origin(std::string_view document_origin) {
    return is_serialized_http_origin(document_origin);
}

bool is_cross_origin(std::string_view document_origin, std::string_view request_url) {
    if (!has_enforceable_document_origin(document_origin)) {
        return false;
    }

    auto request = parse_httpish_url(request_url);
    if (!request.has_value()) {
        return false;
    }

    return request->origin() != document_origin;
}

bool should_attach_origin_header(std::string_view document_origin, std::string_view request_url) {
    return has_enforceable_document_origin(document_origin) &&
           is_cross_origin(document_origin, request_url);
}

bool cors_allows_response(std::string_view document_origin,
                          std::string_view request_url,
                          const clever::net::HeaderMap& response_headers,
                          bool credentials_requested) {
    if (is_invalid_document_origin(document_origin)) {
        return false;
    }

    if (has_enforceable_document_origin(document_origin) &&
        !parse_httpish_url(request_url).has_value()) {
        return false;
    }

    if (!is_cross_origin(document_origin, request_url)) {
        return true;
    }

    auto acao_opt = response_headers.get("access-control-allow-origin");
    if (!acao_opt.has_value()) {
        return false;
    }

    std::string acao = trim_copy(*acao_opt);
    if (acao.empty()) {
        return false;
    }
    if (has_invalid_header_octet(acao)) {
        return false;
    }
    if (acao.find(',') != std::string::npos) {
        return false;
    }

    if (!credentials_requested) {
        return acao == "*" || acao == document_origin;
    }

    if (acao != document_origin) {
        return false;
    }

    auto acac_opt = response_headers.get("access-control-allow-credentials");
    if (!acac_opt.has_value()) {
        return false;
    }

    std::string acac = trim_copy(*acac_opt);
    if (acac.empty() || has_invalid_header_octet(acac)) {
        return false;
    }
    return lower_copy(acac) == "true";
}

} // namespace clever::js::cors
