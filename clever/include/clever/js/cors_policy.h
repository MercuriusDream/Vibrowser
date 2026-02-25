#pragma once

#include <clever/net/header_map.h>
#include <string_view>

namespace clever::js::cors {

bool has_enforceable_document_origin(std::string_view document_origin);
bool is_cross_origin(std::string_view document_origin, std::string_view request_url);
bool should_attach_origin_header(std::string_view document_origin, std::string_view request_url);
bool cors_allows_response(std::string_view document_origin,
                          std::string_view request_url,
                          const clever::net::HeaderMap& response_headers,
                          bool credentials_requested);

} // namespace clever::js::cors
