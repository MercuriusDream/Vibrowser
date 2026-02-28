#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clever::html {

struct Attribute {
    std::string name;
    std::string value;
};

struct Token {
    enum Type { DOCTYPE, StartTag, EndTag, Character, Comment, EndOfFile };
    Type type;
    std::string name;
    std::vector<Attribute> attributes;
    bool self_closing = false;
    std::string data;  // For Character/Comment tokens

    // DOCTYPE-specific
    std::string public_id;
    std::string system_id;
    bool force_quirks = false;
};

enum class TokenizerState {
    Data, TagOpen, EndTagOpen, TagName,
    BeforeAttributeName, AttributeName, AfterAttributeName,
    BeforeAttributeValue, AttributeValueDoubleQuoted, AttributeValueSingleQuoted,
    AttributeValueUnquoted, AfterAttributeValueQuoted,
    SelfClosingStartTag, BogusComment,
    MarkupDeclarationOpen, CommentStart, CommentStartDash, Comment,
    CommentEndDash, CommentEnd, CommentEndBang,
    DOCTYPE, BeforeDOCTYPEName, DOCTYPEName, AfterDOCTYPEName,
    RAWTEXT, RAWTEXTLessThanSign, RAWTEXTEndTagOpen, RAWTEXTEndTagName,
    RCDATA, RCDATALessThanSign, RCDATAEndTagOpen, RCDATAEndTagName,
    ScriptData, ScriptDataLessThanSign, ScriptDataEndTagOpen, ScriptDataEndTagName,
    PLAINTEXT, CharacterReference,
    CDATASection
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);

    Token next_token();
    void set_state(TokenizerState state);
    TokenizerState state() const { return state_; }
    void set_last_start_tag(const std::string& tag) { last_start_tag_ = tag; }

private:
    std::string_view input_;
    size_t pos_ = 0;
    TokenizerState state_ = TokenizerState::Data;
    std::string last_start_tag_;
    Token current_token_;
    std::string temp_buffer_;
    std::string pending_character_data_;

    char consume();
    char peek() const;
    bool at_end() const;
    void reconsume();
    bool is_appropriate_end_tag() const;

    Token emit_character(char c);
    Token emit_string(const std::string& s);
    Token emit_eof();

    // HTML entity decoding: tries to consume &...; reference
    // Returns decoded string, or "&" if not a valid entity
    std::string try_consume_entity();
};

} // namespace clever::html
