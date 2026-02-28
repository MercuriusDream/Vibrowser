#include <clever/css/parser/tokenizer.h>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace clever::css {

// ---------------------------------------------------------------------------
// CSSToken
// ---------------------------------------------------------------------------

bool CSSToken::operator==(const CSSToken& other) const {
    return type == other.type && value == other.value &&
           numeric_value == other.numeric_value && unit == other.unit &&
           is_integer == other.is_integer;
}

// ---------------------------------------------------------------------------
// CSSTokenizer
// ---------------------------------------------------------------------------

CSSTokenizer::CSSTokenizer(std::string_view input) : input_(input), pos_(0) {}

char CSSTokenizer::consume() {
    if (pos_ < input_.size()) {
        return input_[pos_++];
    }
    return '\0';
}

char CSSTokenizer::peek() const {
    if (pos_ < input_.size()) {
        return input_[pos_];
    }
    return '\0';
}

char CSSTokenizer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx < input_.size()) {
        return input_[idx];
    }
    return '\0';
}

bool CSSTokenizer::at_end() const {
    return pos_ >= input_.size();
}

void CSSTokenizer::reconsume() {
    if (pos_ > 0) {
        --pos_;
    }
}

void CSSTokenizer::consume_whitespace() {
    while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' ||
                         peek() == '\r' || peek() == '\f')) {
        consume();
    }
}

void CSSTokenizer::consume_comment() {
    // We've already consumed '/' and '*'
    while (!at_end()) {
        char c = consume();
        if (c == '*' && peek() == '/') {
            consume(); // consume '/'
            return;
        }
    }
    // Unterminated comment: just return
}

bool CSSTokenizer::is_name_start_char(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' ||
           (static_cast<unsigned char>(c) >= 0x80);
}

bool CSSTokenizer::is_name_char(char c) const {
    return is_name_start_char(c) || std::isdigit(static_cast<unsigned char>(c)) ||
           c == '-';
}

bool CSSTokenizer::starts_identifier() const {
    char c = peek();
    if (is_name_start_char(c)) return true;
    if (c == '-') {
        char next = peek(1);
        return is_name_start_char(next) || next == '-' || next == '\\';
    }
    if (c == '\\') {
        // Valid escape: backslash not followed by newline
        char next = peek(1);
        return next != '\n' && next != '\0';
    }
    return false;
}

bool CSSTokenizer::starts_number() const {
    char c = peek();
    if (std::isdigit(static_cast<unsigned char>(c))) return true;
    if (c == '.') {
        return std::isdigit(static_cast<unsigned char>(peek(1)));
    }
    if (c == '+' || c == '-') {
        char next = peek(1);
        if (std::isdigit(static_cast<unsigned char>(next))) return true;
        if (next == '.' && std::isdigit(static_cast<unsigned char>(peek(2))))
            return true;
    }
    return false;
}

std::string CSSTokenizer::consume_name() {
    std::string result;
    while (!at_end()) {
        char c = peek();
        if (is_name_char(c)) {
            result += consume();
        } else if (c == '\\') {
            // Consume escape
            consume(); // consume backslash
            if (!at_end()) {
                char escaped = consume();
                if (std::isxdigit(static_cast<unsigned char>(escaped))) {
                    // Hex escape - consume up to 6 hex digits
                    std::string hex;
                    hex += escaped;
                    for (int i = 0; i < 5 && !at_end() &&
                         std::isxdigit(static_cast<unsigned char>(peek())); ++i) {
                        hex += consume();
                    }
                    // Skip optional whitespace after hex escape
                    if (!at_end() && (peek() == ' ' || peek() == '\t' ||
                                      peek() == '\n')) {
                        consume();
                    }
                    unsigned long code = std::strtoul(hex.c_str(), nullptr, 16);
                    if (code > 0 && code <= 0x7F) {
                        result += static_cast<char>(code);
                    } else {
                        result += '?'; // simplified: replace non-ASCII with ?
                    }
                } else {
                    result += escaped;
                }
            }
        } else {
            break;
        }
    }
    return result;
}

double CSSTokenizer::consume_number_value() {
    std::string repr;

    // Optional sign
    if (peek() == '+' || peek() == '-') {
        repr += consume();
    }

    // Digits before decimal point
    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        repr += consume();
    }

    // Decimal point and digits after
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        repr += consume(); // '.'
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            repr += consume();
        }
    }

    // Scientific notation
    if (peek() == 'e' || peek() == 'E') {
        char after_e = peek(1);
        if (std::isdigit(static_cast<unsigned char>(after_e)) ||
            after_e == '+' || after_e == '-') {
            repr += consume(); // 'e' or 'E'
            if (peek() == '+' || peek() == '-') {
                repr += consume();
            }
            while (!at_end() &&
                   std::isdigit(static_cast<unsigned char>(peek()))) {
                repr += consume();
            }
        }
    }

    return std::strtod(repr.c_str(), nullptr);
}

CSSToken CSSTokenizer::consume_string(char ending) {
    CSSToken token;
    token.type = CSSToken::String;
    std::string result;

    while (!at_end()) {
        char c = consume();
        if (c == ending) {
            token.value = result;
            return token;
        }
        if (c == '\\') {
            if (at_end()) {
                // EOF after backslash
                break;
            }
            char next = peek();
            if (next == '\n') {
                // Escaped newline: skip it (line continuation)
                consume();
            } else {
                result += consume();
            }
        } else if (c == '\n') {
            // Unescaped newline in string is an error; return what we have
            reconsume();
            break;
        } else {
            result += c;
        }
    }

    token.value = result;
    return token;
}

CSSToken CSSTokenizer::consume_numeric() {
    CSSToken token;

    // Remember start position to determine if integer
    size_t start = pos_;
    double value = consume_number_value();
    size_t after_num = pos_;

    // Check the number string for a decimal point to decide is_integer
    bool is_int = true;
    std::string_view num_str = input_.substr(start, after_num - start);
    for (char c : num_str) {
        if (c == '.' || c == 'e' || c == 'E') {
            is_int = false;
            break;
        }
    }

    // Check for dimension (unit)
    if (starts_identifier()) {
        token.type = CSSToken::Dimension;
        token.numeric_value = value;
        token.is_integer = is_int;
        token.unit = consume_name();
        token.value = std::string(num_str) + token.unit;
        return token;
    }

    // Check for percentage
    if (peek() == '%') {
        consume();
        token.type = CSSToken::Percentage;
        token.numeric_value = value;
        token.is_integer = is_int;
        token.value = std::string(num_str) + "%";
        return token;
    }

    // Plain number
    token.type = CSSToken::Number;
    token.numeric_value = value;
    token.is_integer = is_int;
    token.value = std::string(num_str);
    return token;
}

CSSToken CSSTokenizer::consume_ident_like() {
    std::string name = consume_name();

    // Check for function token: name followed by '('
    if (peek() == '(') {
        consume(); // consume '('
        CSSToken token;
        token.type = CSSToken::Function;
        token.value = name;
        return token;
    }

    CSSToken token;
    token.type = CSSToken::Ident;
    token.value = name;
    return token;
}

CSSToken CSSTokenizer::consume_hash() {
    // '#' has already been consumed
    CSSToken token;
    if (!at_end() && (is_name_char(peek()) || peek() == '\\')) {
        token.type = CSSToken::Hash;
        token.value = consume_name();
    } else {
        token.type = CSSToken::Delim;
        token.value = "#";
    }
    return token;
}

CSSToken CSSTokenizer::next_token() {
    // Skip comments
    while (!at_end()) {
        if (peek() == '/' && peek(1) == '*') {
            consume(); // '/'
            consume(); // '*'
            consume_comment();
        } else {
            break;
        }
    }

    if (at_end()) {
        return CSSToken{CSSToken::EndOfFile, "", 0, "", false};
    }

    char c = consume();

    // Whitespace
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
        consume_whitespace();
        return CSSToken{CSSToken::Whitespace, " ", 0, "", false};
    }

    // Strings
    if (c == '"' || c == '\'') {
        return consume_string(c);
    }

    // Hash
    if (c == '#') {
        return consume_hash();
    }

    // Left paren
    if (c == '(') {
        return CSSToken{CSSToken::LeftParen, "(", 0, "", false};
    }
    // Right paren
    if (c == ')') {
        return CSSToken{CSSToken::RightParen, ")", 0, "", false};
    }

    // Comma
    if (c == ',') {
        return CSSToken{CSSToken::Comma, ",", 0, "", false};
    }

    // Colon
    if (c == ':') {
        return CSSToken{CSSToken::Colon, ":", 0, "", false};
    }

    // Semicolon
    if (c == ';') {
        return CSSToken{CSSToken::Semicolon, ";", 0, "", false};
    }

    // Left bracket
    if (c == '[') {
        return CSSToken{CSSToken::LeftBracket, "[", 0, "", false};
    }
    // Right bracket
    if (c == ']') {
        return CSSToken{CSSToken::RightBracket, "]", 0, "", false};
    }

    // Left brace
    if (c == '{') {
        return CSSToken{CSSToken::LeftBrace, "{", 0, "", false};
    }
    // Right brace
    if (c == '}') {
        return CSSToken{CSSToken::RightBrace, "}", 0, "", false};
    }

    // Plus sign: could start a number
    if (c == '+') {
        reconsume();
        if (starts_number()) {
            return consume_numeric();
        }
        consume(); // re-consume '+'
        return CSSToken{CSSToken::Delim, "+", 0, "", false};
    }

    // Hyphen-minus: could start a number, ident, or CDC
    if (c == '-') {
        // Check for CDC: -->
        if (peek() == '-' && peek(1) == '>') {
            consume(); // '-'
            consume(); // '>'
            return CSSToken{CSSToken::CDC, "-->", 0, "", false};
        }
        reconsume();
        if (starts_number()) {
            return consume_numeric();
        }
        if (starts_identifier()) {
            return consume_ident_like();
        }
        consume(); // re-consume '-'
        return CSSToken{CSSToken::Delim, "-", 0, "", false};
    }

    // Period: could start a number
    if (c == '.') {
        reconsume();
        if (starts_number()) {
            return consume_numeric();
        }
        consume(); // re-consume '.'
        return CSSToken{CSSToken::Delim, ".", 0, "", false};
    }

    // Less-than: check for CDO (<!--)
    if (c == '<') {
        if (peek() == '!' && peek(1) == '-' && peek(2) == '-') {
            consume(); // '!'
            consume(); // '-'
            consume(); // '-'
            return CSSToken{CSSToken::CDO, "<!--", 0, "", false};
        }
        return CSSToken{CSSToken::Delim, "<", 0, "", false};
    }

    // At sign
    if (c == '@') {
        if (starts_identifier()) {
            CSSToken token;
            token.type = CSSToken::AtKeyword;
            token.value = consume_name();
            return token;
        }
        return CSSToken{CSSToken::Delim, "@", 0, "", false};
    }

    // Backslash: could start an escaped ident
    if (c == '\\') {
        if (!at_end() && peek() != '\n') {
            reconsume();
            return consume_ident_like();
        }
        return CSSToken{CSSToken::Delim, "\\", 0, "", false};
    }

    // Digits: number
    if (std::isdigit(static_cast<unsigned char>(c))) {
        reconsume();
        return consume_numeric();
    }

    // Name start character: ident-like
    if (is_name_start_char(c)) {
        reconsume();
        return consume_ident_like();
    }

    // Forward slash: check for comment (already handled above), or delim
    if (c == '/') {
        // Comment was already handled at the top of next_token
        return CSSToken{CSSToken::Delim, "/", 0, "", false};
    }

    // Everything else is a delim token
    return CSSToken{CSSToken::Delim, std::string(1, c), 0, "", false};
}

std::vector<CSSToken> CSSTokenizer::tokenize_all(std::string_view input) {
    CSSTokenizer tokenizer(input);
    std::vector<CSSToken> tokens;

    while (true) {
        CSSToken token = tokenizer.next_token();
        tokens.push_back(token);
        if (token.type == CSSToken::EndOfFile) {
            break;
        }
    }

    return tokens;
}

} // namespace clever::css
