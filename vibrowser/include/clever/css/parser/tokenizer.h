#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace clever::css {

struct CSSToken {
    enum Type {
        Ident, Function, AtKeyword, Hash, String, Number, Percentage,
        Dimension, Whitespace, Colon, Semicolon, Comma,
        LeftBrace, RightBrace, LeftParen, RightParen, LeftBracket, RightBracket,
        Delim, CDC, CDO, EndOfFile
    };
    Type type;
    std::string value;
    double numeric_value = 0;
    std::string unit;
    bool is_integer = false;  // for Number tokens

    bool operator==(const CSSToken& other) const;
};

class CSSTokenizer {
public:
    explicit CSSTokenizer(std::string_view input);
    CSSToken next_token();

    // Tokenize all at once
    static std::vector<CSSToken> tokenize_all(std::string_view input);

private:
    std::string_view input_;
    size_t pos_ = 0;

    char consume();
    char peek() const;
    char peek(size_t offset) const;
    bool at_end() const;
    void reconsume();

    void consume_whitespace();
    void consume_comment();
    CSSToken consume_string(char ending);
    CSSToken consume_numeric();
    CSSToken consume_ident_like();
    CSSToken consume_hash();
    double consume_number_value();
    std::string consume_name();
    bool starts_identifier() const;
    bool starts_number() const;
    bool is_name_start_char(char c) const;
    bool is_name_char(char c) const;
};

} // namespace clever::css
