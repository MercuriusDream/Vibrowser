#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace clever::url {

// For now: just lowercase ASCII domains. Full IDNA via ICU comes later.
std::optional<std::string> domain_to_ascii(std::string_view domain);

} // namespace clever::url
