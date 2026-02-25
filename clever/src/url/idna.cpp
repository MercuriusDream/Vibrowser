#include <clever/url/idna.h>
#include <algorithm>
#include <cctype>

namespace clever::url {

std::optional<std::string> domain_to_ascii(std::string_view domain) {
    // Simplified IDNA: just lowercase ASCII domains.
    // Full IDNA processing with ICU will come later.

    if (domain.empty()) {
        return std::string{};
    }

    std::string result;
    result.reserve(domain.size());

    for (char c : domain) {
        unsigned char uc = static_cast<unsigned char>(c);
        // For now, reject non-ASCII characters
        if (uc > 127) {
            return std::nullopt;
        }
        // Reject control characters
        if (uc < 0x20) {
            return std::nullopt;
        }
        result += static_cast<char>(std::tolower(uc));
    }

    // Validate: domain must not be empty after processing
    if (result.empty()) {
        return std::nullopt;
    }

    return result;
}

} // namespace clever::url
