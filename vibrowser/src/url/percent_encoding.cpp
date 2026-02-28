#include <clever/url/percent_encoding.h>
#include <cctype>
#include <cstdint>

namespace clever::url {

namespace {

constexpr char hex_digits[] = "0123456789ABCDEF";

// Characters that are unreserved and never percent-encoded
bool is_unreserved(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

// Characters that appear in path components but should not normally be encoded
// when encoding a full path (but should be when encoding a path segment)
bool is_path_char(char c) {
    return c == '/' || c == ':' || c == '@' ||
           c == '!' || c == '$' || c == '&' || c == '\'' ||
           c == '(' || c == ')' || c == '*' || c == '+' ||
           c == ',' || c == ';' || c == '=';
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

} // anonymous namespace

std::string percent_encode(std::string_view input, bool encode_path_chars) {
    std::string result;
    result.reserve(input.size());

    for (unsigned char c : input) {
        if (is_unreserved(static_cast<char>(c))) {
            result += static_cast<char>(c);
        } else if (!encode_path_chars && is_path_char(static_cast<char>(c))) {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex_digits[(c >> 4) & 0xF];
            result += hex_digits[c & 0xF];
        }
    }

    return result;
}

std::string percent_decode(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            int hi = hex_value(input[i + 1]);
            int lo = hex_value(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        result += input[i];
    }

    return result;
}

bool is_url_code_point(char32_t c) {
    // WHATWG URL code points:
    // ASCII alphanumeric
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;

    // Specific allowed characters
    switch (c) {
        case '!': case '$': case '&': case '\'': case '(': case ')':
        case '*': case '+': case ',': case '-': case '.': case '/':
        case ':': case ';': case '=': case '?': case '@': case '_':
        case '~':
            return true;
        default:
            break;
    }

    // Unicode range U+00A0 to U+10FFFD (excluding surrogates and noncharacters)
    if (c >= 0x00A0 && c <= 0x10FFFD) {
        // Exclude surrogates (0xD800-0xDFFF) and noncharacters
        if (c >= 0xD800 && c <= 0xDFFF) return false;
        if ((c & 0xFFFE) == 0xFFFE) return false; // U+xFFFE and U+xFFFF
        if (c >= 0xFDD0 && c <= 0xFDEF) return false;
        return true;
    }

    return false;
}

} // namespace clever::url
