#include <clever/html/tokenizer.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace clever::html {

Tokenizer::Tokenizer(std::string_view input) : input_(input) {}

char Tokenizer::consume() {
    if (pos_ < input_.size()) {
        return input_[pos_++];
    }
    return '\0';
}

char Tokenizer::peek() const {
    if (pos_ < input_.size()) {
        return input_[pos_];
    }
    return '\0';
}

bool Tokenizer::at_end() const {
    return pos_ >= input_.size();
}

void Tokenizer::reconsume() {
    if (pos_ > 0) {
        --pos_;
    }
}

bool Tokenizer::is_appropriate_end_tag() const {
    return !last_start_tag_.empty() && current_token_.name == last_start_tag_;
}

Token Tokenizer::emit_character(char c) {
    Token t;
    t.type = Token::Character;
    t.data = std::string(1, c);
    return t;
}

Token Tokenizer::emit_string(const std::string& s) {
    Token t;
    t.type = Token::Character;
    t.data = s;
    return t;
}

std::string Tokenizer::try_consume_entity() {
    // Called after '&' has been consumed. Try to match named or numeric entity.
    size_t start = pos_;

    if (at_end()) return "&";

    // Numeric character reference: &#...;
    if (peek() == '#') {
        consume(); // '#'
        if (at_end()) { pos_ = start; return "&"; }

        bool hex = false;
        if (peek() == 'x' || peek() == 'X') {
            hex = true;
            consume();
        }

        std::string digits;
        if (hex) {
            while (!at_end() && std::isxdigit(static_cast<unsigned char>(peek()))) {
                digits += consume();
            }
        } else {
            while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
                digits += consume();
            }
        }

        if (digits.empty()) { pos_ = start; return "&"; }

        // Consume optional ';'
        if (!at_end() && peek() == ';') consume();

        unsigned long codepoint = std::strtoul(digits.c_str(), nullptr, hex ? 16 : 10);
        if (codepoint == 0 || codepoint > 0x10FFFF) return "\xEF\xBF\xBD"; // replacement char

        // Encode UTF-8
        std::string result;
        if (codepoint < 0x80) {
            result += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            result += static_cast<char>(0xC0 | (codepoint >> 6));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            result += static_cast<char>(0xE0 | (codepoint >> 12));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (codepoint >> 18));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        return result;
    }

    // Named character reference: &name;
    // Read alphanumeric characters
    std::string name;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == ';')) {
        char c = consume();
        name += c;
        if (c == ';') break;
    }

    // Strip trailing ';' for lookup
    std::string lookup = name;
    if (!lookup.empty() && lookup.back() == ';') {
        lookup.pop_back();
    }

    // Common named entities
    static const std::unordered_map<std::string, std::string> entities = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
        {"nbsp", "\xC2\xA0"}, // U+00A0 non-breaking space
        {"copy", "\xC2\xA9"}, {"reg", "\xC2\xAE"}, {"trade", "\xE2\x84\xA2"},
        {"mdash", "\xE2\x80\x94"}, {"ndash", "\xE2\x80\x93"},
        {"laquo", "\xC2\xAB"}, {"raquo", "\xC2\xBB"},
        {"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"},
        {"lsquo", "\xE2\x80\x98"}, {"rsquo", "\xE2\x80\x99"},
        {"hellip", "\xE2\x80\xA6"}, {"bull", "\xE2\x80\xA2"},
        {"deg", "\xC2\xB0"}, {"plusmn", "\xC2\xB1"},
        {"times", "\xC3\x97"}, {"divide", "\xC3\xB7"},
        {"euro", "\xE2\x82\xAC"}, {"pound", "\xC2\xA3"}, {"yen", "\xC2\xA5"},
        {"cent", "\xC2\xA2"}, {"sect", "\xC2\xA7"}, {"para", "\xC2\xB6"},
        {"middot", "\xC2\xB7"}, {"frac12", "\xC2\xBD"}, {"frac14", "\xC2\xBC"},
        {"frac34", "\xC2\xBE"}, {"iexcl", "\xC2\xA1"}, {"iquest", "\xC2\xBF"},
        {"larr", "\xE2\x86\x90"}, {"rarr", "\xE2\x86\x92"},
        {"uarr", "\xE2\x86\x91"}, {"darr", "\xE2\x86\x93"},
        {"hearts", "\xE2\x99\xA5"}, {"diams", "\xE2\x99\xA6"},
        {"clubs", "\xE2\x99\xA3"}, {"spades", "\xE2\x99\xA0"},
    };

    auto it = entities.find(lookup);
    if (it != entities.end()) {
        return it->second;
    }

    // Not a recognized entity — rewind and return '&'
    pos_ = start;
    return "&";
}

Token Tokenizer::emit_eof() {
    Token t;
    t.type = Token::EndOfFile;
    return t;
}

void Tokenizer::set_state(TokenizerState state) {
    state_ = state;
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
    return c;
}

Token Tokenizer::next_token() {
    while (true) {
        switch (state_) {

        // ====================================================================
        // Data state
        // ====================================================================
        case TokenizerState::Data: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::TagOpen;
                continue;
            }
            if (c == '&') {
                return emit_string(try_consume_entity());
            }
            return emit_character(c);
        }

        // ====================================================================
        // Tag Open state
        // ====================================================================
        case TokenizerState::TagOpen: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_character('<');
            }
            char c = consume();
            if (c == '!') {
                state_ = TokenizerState::MarkupDeclarationOpen;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::EndTagOpen;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::StartTag;
                current_token_.name.clear();
                current_token_.attributes.clear();
                current_token_.self_closing = false;
                reconsume();
                state_ = TokenizerState::TagName;
                continue;
            }
            if (c == '?') {
                current_token_ = Token{};
                current_token_.type = Token::Comment;
                current_token_.data.clear();
                reconsume();
                state_ = TokenizerState::BogusComment;
                continue;
            }
            // Parse error, emit '<' as character
            state_ = TokenizerState::Data;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // End Tag Open state
        // ====================================================================
        case TokenizerState::EndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                current_token_.attributes.clear();
                current_token_.self_closing = false;
                reconsume();
                state_ = TokenizerState::TagName;
                continue;
            }
            if (c == '>') {
                // Parse error: </>
                state_ = TokenizerState::Data;
                continue;
            }
            current_token_ = Token{};
            current_token_.type = Token::Comment;
            current_token_.data.clear();
            reconsume();
            state_ = TokenizerState::BogusComment;
            continue;
        }

        // ====================================================================
        // Tag Name state
        // ====================================================================
        case TokenizerState::TagName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.name += to_lower(c);
            continue;
        }

        // ====================================================================
        // Before Attribute Name state
        // ====================================================================
        case TokenizerState::BeforeAttributeName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '/' || c == '>') {
                reconsume();
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            if (c == '=') {
                Attribute attr;
                attr.name = "=";
                current_token_.attributes.push_back(attr);
                state_ = TokenizerState::AttributeName;
                continue;
            }
            current_token_.attributes.push_back(Attribute{});
            reconsume();
            state_ = TokenizerState::AttributeName;
            continue;
        }

        // ====================================================================
        // Attribute Name state
        // ====================================================================
        case TokenizerState::AttributeName: {
            if (at_end()) {
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' '
                || c == '/' || c == '>') {
                reconsume();
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            if (c == '=') {
                state_ = TokenizerState::BeforeAttributeValue;
                continue;
            }
            current_token_.attributes.back().name += to_lower(c);
            continue;
        }

        // ====================================================================
        // After Attribute Name state
        // ====================================================================
        case TokenizerState::AfterAttributeName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '=') {
                state_ = TokenizerState::BeforeAttributeValue;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.attributes.push_back(Attribute{});
            reconsume();
            state_ = TokenizerState::AttributeName;
            continue;
        }

        // ====================================================================
        // Before Attribute Value state
        // ====================================================================
        case TokenizerState::BeforeAttributeValue: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '"') {
                state_ = TokenizerState::AttributeValueDoubleQuoted;
                continue;
            }
            if (c == '\'') {
                state_ = TokenizerState::AttributeValueSingleQuoted;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::AttributeValueUnquoted;
            continue;
        }

        // ====================================================================
        // Attribute Value (Double-Quoted) state
        // ====================================================================
        case TokenizerState::AttributeValueDoubleQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '"') {
                state_ = TokenizerState::AfterAttributeValueQuoted;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // Attribute Value (Single-Quoted) state
        // ====================================================================
        case TokenizerState::AttributeValueSingleQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\'') {
                state_ = TokenizerState::AfterAttributeValueQuoted;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // Attribute Value (Unquoted) state
        // ====================================================================
        case TokenizerState::AttributeValueUnquoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // After Attribute Value (Quoted) state
        // ====================================================================
        case TokenizerState::AfterAttributeValueQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeAttributeName;
            continue;
        }

        // ====================================================================
        // Self-Closing Start Tag state
        // ====================================================================
        case TokenizerState::SelfClosingStartTag: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '>') {
                current_token_.self_closing = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeAttributeName;
            continue;
        }

        // ====================================================================
        // Bogus Comment state
        // ====================================================================
        case TokenizerState::BogusComment: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += c;
            continue;
        }

        // ====================================================================
        // Markup Declaration Open state
        // ====================================================================
        case TokenizerState::MarkupDeclarationOpen: {
            // Check for "--" (comment)
            if (pos_ + 1 < input_.size() && input_[pos_] == '-' && input_[pos_ + 1] == '-') {
                pos_ += 2;
                current_token_ = Token{};
                current_token_.type = Token::Comment;
                current_token_.data.clear();
                state_ = TokenizerState::CommentStart;
                continue;
            }
            // Check for DOCTYPE (case-insensitive)
            if (pos_ + 6 < input_.size()) {
                std::string next7;
                for (size_t i = 0; i < 7; ++i) {
                    next7 += static_cast<char>(std::toupper(
                        static_cast<unsigned char>(input_[pos_ + i])));
                }
                if (next7 == "DOCTYPE") {
                    pos_ += 7;
                    state_ = TokenizerState::DOCTYPE;
                    continue;
                }
            }
            // Bogus comment
            current_token_ = Token{};
            current_token_.type = Token::Comment;
            current_token_.data.clear();
            state_ = TokenizerState::BogusComment;
            continue;
        }

        // ====================================================================
        // Comment Start state
        // ====================================================================
        case TokenizerState::CommentStart: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentStartDash;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment Start Dash state
        // ====================================================================
        case TokenizerState::CommentStartDash: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEnd;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += '-';
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment state
        // ====================================================================
        case TokenizerState::Comment: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEndDash;
                continue;
            }
            current_token_.data += c;
            continue;
        }

        // ====================================================================
        // Comment End Dash state
        // ====================================================================
        case TokenizerState::CommentEndDash: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEnd;
                continue;
            }
            current_token_.data += '-';
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment End state
        // ====================================================================
        case TokenizerState::CommentEnd: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (c == '!') {
                state_ = TokenizerState::CommentEndBang;
                continue;
            }
            if (c == '-') {
                current_token_.data += '-';
                continue;
            }
            current_token_.data += "--";
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment End Bang state
        // ====================================================================
        case TokenizerState::CommentEndBang: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                current_token_.data += "--!";
                state_ = TokenizerState::CommentEndDash;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += "--!";
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // DOCTYPE state
        // ====================================================================
        case TokenizerState::DOCTYPE: {
            if (at_end()) {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeDOCTYPEName;
                continue;
            }
            if (c == '>') {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeDOCTYPEName;
            continue;
        }

        // ====================================================================
        // Before DOCTYPE Name state
        // ====================================================================
        case TokenizerState::BeforeDOCTYPEName: {
            if (at_end()) {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '>') {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_ = Token{};
            current_token_.type = Token::DOCTYPE;
            current_token_.name = std::string(1, to_lower(c));
            state_ = TokenizerState::DOCTYPEName;
            continue;
        }

        // ====================================================================
        // DOCTYPE Name state
        // ====================================================================
        case TokenizerState::DOCTYPEName: {
            if (at_end()) {
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::AfterDOCTYPEName;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.name += to_lower(c);
            continue;
        }

        // ====================================================================
        // After DOCTYPE Name state
        // ====================================================================
        case TokenizerState::AfterDOCTYPEName: {
            if (at_end()) {
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            // Simplified: skip PUBLIC/SYSTEM identifiers
            while (!at_end()) {
                c = consume();
                if (c == '>') {
                    state_ = TokenizerState::Data;
                    return current_token_;
                }
            }
            current_token_.force_quirks = true;
            state_ = TokenizerState::Data;
            return current_token_;
        }

        // ====================================================================
        // RAWTEXT state
        // ====================================================================
        case TokenizerState::RAWTEXT: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::RAWTEXTLessThanSign;
                continue;
            }
            return emit_character(c);
        }

        // ====================================================================
        // RAWTEXT Less-Than Sign state
        // ====================================================================
        case TokenizerState::RAWTEXTLessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::RAWTEXTEndTagOpen;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RAWTEXT End Tag Open state
        // ====================================================================
        case TokenizerState::RAWTEXTEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::RAWTEXTEndTagName;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RAWTEXT End Tag Name state
        // ====================================================================
        case TokenizerState::RAWTEXTEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA state
        // ====================================================================
        case TokenizerState::RCDATA: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::RCDATALessThanSign;
                continue;
            }
            if (c == '&') {
                return emit_string(try_consume_entity());
            }
            return emit_character(c);
        }

        // ====================================================================
        // RCDATA Less-Than Sign state
        // ====================================================================
        case TokenizerState::RCDATALessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::RCDATAEndTagOpen;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA End Tag Open state
        // ====================================================================
        case TokenizerState::RCDATAEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::RCDATAEndTagName;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA End Tag Name state
        // ====================================================================
        case TokenizerState::RCDATAEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData state
        // ====================================================================
        case TokenizerState::ScriptData: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::ScriptDataLessThanSign;
                continue;
            }
            return emit_character(c);
        }

        // ====================================================================
        // ScriptData Less-Than Sign state
        // ====================================================================
        case TokenizerState::ScriptDataLessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::ScriptDataEndTagOpen;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData End Tag Open state
        // ====================================================================
        case TokenizerState::ScriptDataEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::ScriptDataEndTagName;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData End Tag Name state
        // ====================================================================
        case TokenizerState::ScriptDataEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // PLAINTEXT state
        // ====================================================================
        case TokenizerState::PLAINTEXT: {
            if (at_end()) return emit_eof();
            return emit_character(consume());
        }

        // ====================================================================
        // Character Reference state (simplified)
        // ====================================================================
        case TokenizerState::CharacterReference: {
            state_ = TokenizerState::Data;
            return emit_character('&');
        }

        } // end switch
    } // end while
}

} // namespace clever::html
