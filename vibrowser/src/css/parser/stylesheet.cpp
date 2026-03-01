#include <clever/css/parser/stylesheet.h>
#include <clever/css/parser/tokenizer.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>

namespace clever::css {

// ---------------------------------------------------------------------------
// Internal stylesheet parser
// ---------------------------------------------------------------------------

class StyleSheetParser {
public:
    explicit StyleSheetParser(std::vector<CSSToken> tokens)
        : tokens_(std::move(tokens)), pos_(0) {}

    StyleSheet parse();
    std::vector<Declaration> parse_declarations();

private:
    std::vector<CSSToken> tokens_;
    size_t pos_;

    const CSSToken& current() const;
    bool at_end() const;
    void advance();
    void skip_whitespace();
    void skip_whitespace_and_semicolons();

    // Top-level parsing
    void parse_at_rule(StyleSheet& sheet);
    void parse_style_rule(StyleSheet& sheet);

    // At-rule specifics
    void parse_import_rule(StyleSheet& sheet);
    void parse_media_rule(StyleSheet& sheet);
    void parse_keyframes_rule(StyleSheet& sheet);
    void parse_font_face_rule(StyleSheet& sheet);
    void parse_supports_rule(StyleSheet& sheet);
    void parse_layer_rule(StyleSheet& sheet, const std::string& parent_layer = "");
    void parse_container_rule(StyleSheet& sheet);
    void parse_scope_rule(StyleSheet& sheet);
    void parse_property_rule(StyleSheet& sheet);
    void parse_counter_style_rule(StyleSheet& sheet);
    void parse_starting_style_rule();
    void parse_font_palette_values_rule();

    // Nesting
    bool is_nested_rule_start();
    void parse_nested_block(const std::string& parent_selector,
                            std::vector<Declaration>& out_declarations,
                            std::vector<StyleRule>& out_nested_rules);

    // Declarations
    Declaration parse_declaration();
    std::vector<ComponentValue> parse_component_values_until(CSSToken::Type stop1,
                                                              CSSToken::Type stop2);
    ComponentValue consume_component_value();
    ComponentValue consume_function();

    // Selectors
    std::string consume_selector_text();

    // Utilities
    std::string extract_url_from_tokens();
    void skip_to(CSSToken::Type type);
    void skip_block();
    std::vector<std::string> split_layer_name_list(const std::string& prelude) const;
    std::string canonical_layer_name(const std::string& name,
                                     const std::string& parent_layer) const;
    size_t ensure_layer_order(const std::string& layer_name);

    std::unordered_map<std::string, size_t> layer_order_map_;
    size_t next_layer_order_ = 0;
    size_t next_anonymous_layer_id_ = 0;
};

const CSSToken& StyleSheetParser::current() const {
    if (pos_ < tokens_.size()) {
        return tokens_[pos_];
    }
    static CSSToken eof{CSSToken::EndOfFile, "", 0, "", false};
    return eof;
}

bool StyleSheetParser::at_end() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == CSSToken::EndOfFile;
}

void StyleSheetParser::advance() {
    if (pos_ < tokens_.size()) {
        ++pos_;
    }
}

void StyleSheetParser::skip_whitespace() {
    while (!at_end() && current().type == CSSToken::Whitespace) {
        advance();
    }
}

void StyleSheetParser::skip_whitespace_and_semicolons() {
    while (!at_end() && (current().type == CSSToken::Whitespace ||
                         current().type == CSSToken::Semicolon)) {
        advance();
    }
}

void StyleSheetParser::skip_to(CSSToken::Type type) {
    while (!at_end() && current().type != type) {
        advance();
    }
    if (!at_end()) advance();
}

void StyleSheetParser::skip_block() {
    // Assumes we're at '{'
    if (current().type == CSSToken::LeftBrace) {
        advance();
    }
    int depth = 1;
    while (!at_end() && depth > 0) {
        if (current().type == CSSToken::LeftBrace) depth++;
        else if (current().type == CSSToken::RightBrace) depth--;
        advance();
    }
}

std::vector<std::string> StyleSheetParser::split_layer_name_list(const std::string& prelude) const {
    std::vector<std::string> names;
    std::string current;
    int paren_depth = 0;
    for (char ch : prelude) {
        if (ch == '(') paren_depth++;
        if (ch == ')' && paren_depth > 0) paren_depth--;
        if (ch == ',' && paren_depth == 0) {
            if (!current.empty()) names.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) names.push_back(current);

    auto trim = [](std::string s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    };

    for (auto& name : names) {
        name = trim(name);
    }
    names.erase(std::remove_if(names.begin(), names.end(),
                               [](const std::string& s) { return s.empty(); }),
                names.end());
    return names;
}

std::string StyleSheetParser::canonical_layer_name(
        const std::string& name,
        const std::string& parent_layer) const {
    if (name.empty()) return parent_layer;
    if (parent_layer.empty()) return name;
    if (name.rfind(parent_layer + ".", 0) == 0) return name;
    return parent_layer + "." + name;
}

size_t StyleSheetParser::ensure_layer_order(const std::string& layer_name) {
    auto it = layer_order_map_.find(layer_name);
    if (it != layer_order_map_.end()) {
        return it->second;
    }
    size_t order = next_layer_order_++;
    layer_order_map_[layer_name] = order;
    return order;
}

StyleSheet StyleSheetParser::parse() {
    StyleSheet sheet;

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;

        // Skip CDO/CDC tokens (HTML comment markers)
        if (current().type == CSSToken::CDO || current().type == CSSToken::CDC) {
            advance();
            continue;
        }

        if (current().type == CSSToken::AtKeyword) {
            parse_at_rule(sheet);
        } else if (current().type == CSSToken::Semicolon) {
            advance(); // skip stray semicolons
        } else {
            parse_style_rule(sheet);
        }
    }

    return sheet;
}

void StyleSheetParser::parse_at_rule(StyleSheet& sheet) {
    std::string keyword = current().value;
    advance(); // skip at-keyword

    if (keyword == "import") {
        parse_import_rule(sheet);
    } else if (keyword == "media") {
        parse_media_rule(sheet);
    } else if (keyword == "keyframes" || keyword == "-webkit-keyframes") {
        parse_keyframes_rule(sheet);
    } else if (keyword == "font-face") {
        parse_font_face_rule(sheet);
    } else if (keyword == "supports") {
        parse_supports_rule(sheet);
    } else if (keyword == "layer") {
        parse_layer_rule(sheet);
    } else if (keyword == "container") {
        parse_container_rule(sheet);
    } else if (keyword == "scope") {
        parse_scope_rule(sheet);
    } else if (keyword == "property") {
        parse_property_rule(sheet);
    } else if (keyword == "counter-style") {
        parse_counter_style_rule(sheet);
    } else if (keyword == "starting-style") {
        parse_starting_style_rule();
    } else if (keyword == "font-palette-values") {
        parse_font_palette_values_rule();
    } else if (keyword == "charset" || keyword == "namespace" || keyword == "page") {
        // @charset, @namespace, @page — skip to semicolon or block
        while (!at_end()) {
            if (current().type == CSSToken::Semicolon) { advance(); break; }
            if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
            advance();
        }
    } else {
        // Unknown at-rule: skip to semicolon or block
        while (!at_end()) {
            if (current().type == CSSToken::Semicolon) {
                advance();
                break;
            }
            if (current().type == CSSToken::LeftBrace) {
                skip_block();
                break;
            }
            advance();
        }
    }
}

void StyleSheetParser::parse_import_rule(StyleSheet& sheet) {
    ImportRule rule;
    skip_whitespace();

    // @import can be:
    //   url("...")     — quoted URL inside url()
    //   url('...')     — single-quoted URL inside url()
    //   url(bare.css)  — bare (unquoted) URL: collect all tokens until ')'
    //   "..."          — string literal
    //   '...'          — single-quoted string literal
    if (!at_end() && current().type == CSSToken::Function && current().value == "url") {
        advance(); // skip 'url('
        skip_whitespace();
        if (!at_end() && current().type == CSSToken::String) {
            // Quoted URL: the tokenizer already stripped the quotes for String tokens
            rule.url = current().value;
            advance();
            skip_whitespace();
        } else {
            // Bare (unquoted) URL: collect all token values until ')' or ';' or EOF.
            // Bare URLs can contain characters like '/', ':', '.' which the CSS tokenizer
            // emits as individual Delim tokens, so we must concatenate them.
            std::string bare;
            while (!at_end() &&
                   current().type != CSSToken::RightParen &&
                   current().type != CSSToken::Semicolon) {
                // Whitespace inside a bare url() terminates the URL
                if (current().type == CSSToken::Whitespace) break;
                bare += current().value;
                advance();
            }
            rule.url = bare;
            skip_whitespace();
        }
        // Skip closing ')'
        if (!at_end() && current().type == CSSToken::RightParen) {
            advance();
        }
    } else if (!at_end() && current().type == CSSToken::String) {
        // @import "url" or @import 'url'
        rule.url = current().value;
        advance();
    }

    skip_whitespace();

    // Optional media query
    std::string media;
    while (!at_end() && current().type != CSSToken::Semicolon) {
        if (!media.empty() && current().type != CSSToken::Whitespace) {
            media += " ";
        }
        if (current().type != CSSToken::Whitespace) {
            media += current().value;
        }
        advance();
    }
    rule.media = media;

    // Skip semicolon
    if (!at_end() && current().type == CSSToken::Semicolon) {
        advance();
    }

    sheet.imports.push_back(std::move(rule));
}

void StyleSheetParser::parse_media_rule(StyleSheet& sheet) {
    MediaQuery mq;
    skip_whitespace();

    // Collect condition tokens until '{'
    std::string condition;
    while (!at_end() && current().type != CSSToken::LeftBrace) {
        if (current().type == CSSToken::Whitespace) {
            if (!condition.empty() && condition.back() != ' ' &&
                condition.back() != '(') {
                condition += " ";
            }
        } else {
            condition += current().value;
        }
        advance();
    }

    // Trim trailing whitespace
    while (!condition.empty() && condition.back() == ' ') {
        condition.pop_back();
    }
    mq.condition = condition;

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse rules inside media block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        // Parse a style rule inside the media block
        StyleRule rule;
        std::string sel_text;

        // Consume selector text until '{'
        while (!at_end() && current().type != CSSToken::LeftBrace) {
            if (current().type == CSSToken::Whitespace) {
                if (!sel_text.empty() && sel_text.back() != ' ') {
                    sel_text += " ";
                }
            } else if (current().type == CSSToken::Hash) {
                // Preserve '#' prefix for ID selectors (tokenizer strips it)
                sel_text += "#" + current().value;
            } else if (current().type == CSSToken::Function) {
                // Function token value is just the name without '('
                sel_text += current().value + "(";
            } else {
                sel_text += current().value;
            }
            advance();
        }

        // Trim
        while (!sel_text.empty() && sel_text.back() == ' ') {
            sel_text.pop_back();
        }

        rule.selector_text = sel_text;
        rule.selectors = parse_selector_list(sel_text);

        // Skip '{'
        if (!at_end() && current().type == CSSToken::LeftBrace) {
            advance();
        }

        // Parse declarations
        while (!at_end() && current().type != CSSToken::RightBrace) {
            skip_whitespace();
            if (at_end() || current().type == CSSToken::RightBrace) break;
            if (current().type == CSSToken::Semicolon) {
                advance();
                continue;
            }
            auto decl = parse_declaration();
            if (!decl.property.empty()) {
                rule.declarations.push_back(std::move(decl));
            }
        }

        // Skip '}'
        if (!at_end() && current().type == CSSToken::RightBrace) {
            advance();
        }

        mq.rules.push_back(std::move(rule));
    }

    // Skip closing '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.media_queries.push_back(std::move(mq));
}

void StyleSheetParser::parse_keyframes_rule(StyleSheet& sheet) {
    KeyframesRule kr;
    skip_whitespace();

    // Name
    if (!at_end() && current().type == CSSToken::Ident) {
        kr.name = current().value;
        advance();
    } else if (!at_end() && current().type == CSSToken::String) {
        kr.name = current().value;
        advance();
    }

    skip_whitespace();

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse keyframe blocks
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        KeyframeRule kf;

        // Keyframe selector: "from", "to", or percentage
        std::string sel;
        while (!at_end() && current().type != CSSToken::LeftBrace) {
            if (current().type != CSSToken::Whitespace) {
                sel += current().value;
            }
            advance();
        }
        kf.selector = sel;

        // Skip '{'
        if (!at_end() && current().type == CSSToken::LeftBrace) {
            advance();
        }

        // Parse declarations
        while (!at_end() && current().type != CSSToken::RightBrace) {
            skip_whitespace();
            if (at_end() || current().type == CSSToken::RightBrace) break;
            if (current().type == CSSToken::Semicolon) {
                advance();
                continue;
            }
            auto decl = parse_declaration();
            if (!decl.property.empty()) {
                kf.declarations.push_back(std::move(decl));
            }
        }

        // Skip '}'
        if (!at_end() && current().type == CSSToken::RightBrace) {
            advance();
        }

        kr.keyframes.push_back(std::move(kf));
    }

    // Skip closing '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.keyframes.push_back(std::move(kr));
}

void StyleSheetParser::parse_font_face_rule(StyleSheet& sheet) {
    FontFaceRule rule;
    skip_whitespace();

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse descriptor declarations inside @font-face block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;
        if (current().type == CSSToken::Semicolon) {
            advance();
            continue;
        }

        auto decl = parse_declaration();
        if (decl.property.empty()) continue;

        // Reconstruct the value string from component values
        std::string value_str;
        for (auto& cv : decl.values) {
            if (cv.type == ComponentValue::Function) {
                // For url() functions, join children without separators
                // to preserve the URL as a single string
                bool is_url = (cv.value == "url");
                value_str += cv.value + "(";
                for (size_t i = 0; i < cv.children.size(); ++i) {
                    if (!is_url && i > 0) value_str += ", ";
                    value_str += cv.children[i].value;
                }
                value_str += ")";
            } else {
                if (!value_str.empty() && value_str.back() != '(' &&
                    value_str.back() != ' ') {
                    value_str += " ";
                }
                value_str += cv.value;
            }
        }

        // Remove surrounding quotes from value if present
        auto unquote = [](const std::string& s) -> std::string {
            if (s.size() >= 2 &&
                ((s.front() == '"' && s.back() == '"') ||
                 (s.front() == '\'' && s.back() == '\''))) {
                return s.substr(1, s.size() - 2);
            }
            return s;
        };

        auto to_lower = [](std::string s) -> std::string {
            for (auto& ch : s) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return s;
        };

        auto trim = [](const std::string& s) -> std::string {
            size_t start = 0;
            while (start < s.size() &&
                   std::isspace(static_cast<unsigned char>(s[start]))) {
                ++start;
            }
            size_t end = s.size();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(s[end - 1]))) {
                --end;
            }
            return (end <= start) ? "" : s.substr(start, end - start);
        };

        auto parse_single_font_weight = [&](const std::string& token, int& out) -> bool {
            const auto lower = to_lower(trim(token));
            if (lower.empty()) return false;
            if (lower == "normal") {
                out = 400;
                return true;
            }
            if (lower == "bold") {
                out = 700;
                return true;
            }
            if (!std::all_of(lower.begin(), lower.end(),
                             [](unsigned char c) { return std::isdigit(c); })) {
                return false;
            }
            try {
                int value = std::stoi(lower);
                if (value < 100 || value > 900) return false;
                out = value;
                return true;
            } catch (...) {
                return false;
            }
        };

        auto parse_font_weight_range = [&](const std::string& value,
                                          int& min_weight,
                                          int& max_weight) -> bool {
            int first = 0;
            int second = 0;
            std::vector<std::string> tokens;
            bool in_token = false;
            std::string token;
            for (char ch : value) {
                if (std::isspace(static_cast<unsigned char>(ch))) {
                    if (in_token) {
                        tokens.push_back(token);
                        token.clear();
                        in_token = false;
                    }
                    continue;
                }
                if (ch == ',') break;
                token += ch;
                in_token = true;
            }
            if (in_token) tokens.push_back(token);
            if (tokens.empty() || tokens.size() > 2) return false;
            if (!parse_single_font_weight(tokens[0], first)) return false;
            if (tokens.size() == 1) {
                min_weight = first;
                max_weight = first;
                return true;
            }
            if (!parse_single_font_weight(tokens[1], second)) return false;
            min_weight = std::min(first, second);
            max_weight = std::max(first, second);
            return true;
        };

        auto parse_unicode_codepoint = [&](const std::string& hex, int& out) -> bool {
            const auto trimmed = trim(hex);
            if (trimmed.empty()) return false;
            if (!std::all_of(trimmed.begin(), trimmed.end(),
                             [](unsigned char c) { return std::isxdigit(c); })) {
                return false;
            }
            try {
                long value = std::stol(trimmed, nullptr, 16);
                if (value < 0 || value > 0x10FFFF) return false;
                out = static_cast<int>(value);
                return true;
            } catch (...) {
                return false;
            }
        };

        auto parse_unicode_range = [&](const std::string& value,
                                      int& min_cp,
                                      int& max_cp) -> bool {
            bool parsed_any = false;
            int parsed_min = std::numeric_limits<int>::max();
            int parsed_max = -1;
            size_t pos = 0;
            while (pos < value.size()) {
                size_t comma = value.find(',', pos);
                std::string token = trim(value.substr(pos, comma - pos));
                pos = (comma == std::string::npos) ? value.size() : comma + 1;
                if (token.empty()) continue;

                token = to_lower(token);
                if (token.size() < 3 ||
                    token[0] != 'u' ||
                    token[1] != '+') {
                    continue;
                }

                std::string range = token.substr(2);
                const auto dash = range.find('-');
                int min_value = 0;
                int max_value = 0;
                if (dash == std::string::npos) {
                    if (!parse_unicode_codepoint(range, min_value)) continue;
                    max_value = min_value;
                } else {
                    if (dash == 0 || dash + 1 >= range.size()) continue;
                    if (!parse_unicode_codepoint(range.substr(0, dash), min_value)) continue;
                    if (!parse_unicode_codepoint(range.substr(dash + 1), max_value)) continue;
                    if (min_value > max_value) std::swap(min_value, max_value);
                }

                parsed_any = true;
                parsed_min = std::min(parsed_min, min_value);
                parsed_max = std::max(parsed_max, max_value);
            }
            if (!parsed_any) return false;
            min_cp = parsed_min;
            max_cp = parsed_max;
            return true;
        };

        if (decl.property == "font-family") {
            rule.font_family = unquote(value_str);
        } else if (decl.property == "src") {
            rule.src = value_str;
        } else if (decl.property == "font-weight") {
            rule.font_weight = value_str;
            int min_weight = 0;
            int max_weight = 0;
            if (parse_font_weight_range(value_str, min_weight, max_weight)) {
                rule.min_weight = min_weight;
                rule.max_weight = max_weight;
            } else {
                rule.min_weight = 400;
                rule.max_weight = 400;
            }
        } else if (decl.property == "font-style") {
            rule.font_style = value_str;
        } else if (decl.property == "unicode-range") {
            rule.unicode_range = value_str;
            int unicode_min = 0;
            int unicode_max = 0x10FFFF;
            if (parse_unicode_range(value_str, unicode_min, unicode_max)) {
                rule.unicode_min = unicode_min;
                rule.unicode_max = unicode_max;
            } else {
                rule.unicode_min = 0;
                rule.unicode_max = 0x10FFFF;
            }
        } else if (decl.property == "font-display") {
            rule.font_display = value_str;
        } else if (decl.property == "size-adjust") {
            rule.size_adjust = value_str;
        }
    }

    // Skip closing '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.font_faces.push_back(std::move(rule));
}

void StyleSheetParser::parse_supports_rule(StyleSheet& sheet) {
    SupportsRule rule;
    skip_whitespace();

    // Collect condition tokens until '{'
    std::string condition;
    while (!at_end() && current().type != CSSToken::LeftBrace) {
        if (current().type == CSSToken::Whitespace) {
            if (!condition.empty() && condition.back() != ' ' &&
                condition.back() != '(') {
                condition += " ";
            }
        } else {
            condition += current().value;
        }
        advance();
    }

    while (!condition.empty() && condition.back() == ' ') {
        condition.pop_back();
    }
    rule.condition = condition;

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse style rules inside @supports block (same pattern as @media)
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        if (current().type == CSSToken::AtKeyword) {
            // Nested at-rules inside @supports — skip for now
            while (!at_end()) {
                if (current().type == CSSToken::Semicolon) { advance(); break; }
                if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
                advance();
            }
        } else {
            // Parse as style rule and add to supports rule
            StyleRule style_rule;
            std::string sel_text;
            while (!at_end() && current().type != CSSToken::LeftBrace &&
                   current().type != CSSToken::RightBrace) {
                if (current().type == CSSToken::Whitespace) {
                    if (!sel_text.empty() && sel_text.back() != ' ') sel_text += " ";
                } else if (current().type == CSSToken::Hash) {
                    sel_text += "#" + current().value;
                } else if (current().type == CSSToken::Function) {
                    sel_text += current().value + "(";
                } else {
                    sel_text += current().value;
                }
                advance();
            }
            while (!sel_text.empty() && sel_text.back() == ' ') sel_text.pop_back();
            style_rule.selector_text = sel_text;
            style_rule.selectors = parse_selector_list(sel_text);

            if (!at_end() && current().type == CSSToken::LeftBrace) {
                advance();
            }
            while (!at_end() && current().type != CSSToken::RightBrace) {
                skip_whitespace();
                if (at_end() || current().type == CSSToken::RightBrace) break;
                if (current().type == CSSToken::Semicolon) { advance(); continue; }
                auto decl = parse_declaration();
                if (!decl.property.empty()) style_rule.declarations.push_back(std::move(decl));
            }
            if (!at_end() && current().type == CSSToken::RightBrace) advance();
            if (!style_rule.selectors.selectors.empty()) rule.rules.push_back(std::move(style_rule));
        }
    }

    // Skip closing '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.supports_rules.push_back(std::move(rule));
}

void StyleSheetParser::parse_layer_rule(StyleSheet& sheet, const std::string& parent_layer) {
    LayerRule rule;
    skip_whitespace();

    // Collect layer name (if any) until '{' or ';'
    std::string prelude;
    while (!at_end() && current().type != CSSToken::LeftBrace &&
           current().type != CSSToken::Semicolon) {
        if (current().type == CSSToken::Whitespace) {
            if (!prelude.empty() && prelude.back() != ' ') prelude += " ";
        } else {
            prelude += current().value;
        }
        advance();
    }
    while (!prelude.empty() && prelude.back() == ' ') prelude.pop_back();

    auto declared_names = split_layer_name_list(prelude);
    bool is_declaration_only = (!at_end() && current().type == CSSToken::Semicolon);

    // @layer foo, bar; (and nested equivalents): declaration-only ordering.
    if (is_declaration_only) {
        if (declared_names.empty()) {
            std::string anon_name = "__anon_decl_" + std::to_string(next_anonymous_layer_id_++);
            std::string canonical = canonical_layer_name(anon_name, parent_layer);
            rule.name = prelude;
            rule.order = ensure_layer_order(canonical);
            sheet.layer_rules.push_back(std::move(rule));
        } else {
            for (const auto& declared_name : declared_names) {
                LayerRule decl_rule;
                std::string canonical = canonical_layer_name(declared_name, parent_layer);
                decl_rule.name = canonical;
                decl_rule.order = ensure_layer_order(canonical);
                sheet.layer_rules.push_back(std::move(decl_rule));
            }
        }
        advance();
        return;
    }

    // For block form, CSS grammar expects a single layer name (or anonymous).
    std::string local_name;
    if (!declared_names.empty()) {
        local_name = declared_names.front();
    }
    std::string canonical_name;
    if (local_name.empty()) {
        canonical_name = canonical_layer_name(
            "__anon_" + std::to_string(next_anonymous_layer_id_++), parent_layer);
        rule.name = parent_layer;
    } else {
        canonical_name = canonical_layer_name(local_name, parent_layer);
        rule.name = canonical_name;
    }
    rule.order = ensure_layer_order(canonical_name);

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse style rules inside @layer block (same pattern as @supports/@media)
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        if (current().type == CSSToken::AtKeyword) {
            std::string nested_keyword = current().value;
            advance();
            if (nested_keyword == "layer") {
                parse_layer_rule(sheet, canonical_name);
            } else {
                while (!at_end()) {
                    if (current().type == CSSToken::Semicolon) { advance(); break; }
                    if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
                    advance();
                }
            }
        } else {
            // Parse as style rule
            StyleRule style_rule;
            style_rule.in_layer = true;
            style_rule.layer_order = rule.order;
            style_rule.layer_name = rule.name;
            std::string sel_text;
            while (!at_end() && current().type != CSSToken::LeftBrace &&
                   current().type != CSSToken::RightBrace) {
                if (current().type == CSSToken::Whitespace) {
                    if (!sel_text.empty() && sel_text.back() != ' ') sel_text += " ";
                } else if (current().type == CSSToken::Hash) {
                    sel_text += "#" + current().value;
                } else if (current().type == CSSToken::Function) {
                    sel_text += current().value + "(";
                } else {
                    sel_text += current().value;
                }
                advance();
            }
            while (!sel_text.empty() && sel_text.back() == ' ') sel_text.pop_back();
            style_rule.selector_text = sel_text;
            style_rule.selectors = parse_selector_list(sel_text);

            if (!at_end() && current().type == CSSToken::LeftBrace) {
                advance();
            }
            while (!at_end() && current().type != CSSToken::RightBrace) {
                skip_whitespace();
                if (at_end() || current().type == CSSToken::RightBrace) break;
                if (current().type == CSSToken::Semicolon) { advance(); continue; }
                auto decl = parse_declaration();
                if (!decl.property.empty()) style_rule.declarations.push_back(std::move(decl));
            }
            if (!at_end() && current().type == CSSToken::RightBrace) advance();
            if (!style_rule.selectors.selectors.empty()) rule.rules.push_back(std::move(style_rule));
        }
    }

    // Skip closing '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.layer_rules.push_back(std::move(rule));
}

void StyleSheetParser::parse_container_rule(StyleSheet& sheet) {
    ContainerRule rule;
    skip_whitespace();

    // Parse: @container [name] (condition) { rules }
    // Collect everything until '{' as the container condition
    std::string prelude;
    int paren_depth = 0;
    while (!at_end() && (current().type != CSSToken::LeftBrace || paren_depth > 0)) {
        if (current().type == CSSToken::LeftParen) paren_depth++;
        else if (current().type == CSSToken::RightParen) paren_depth--;

        if (current().type == CSSToken::Whitespace) {
            if (!prelude.empty() && prelude.back() != ' ') prelude += " ";
        } else if (current().type == CSSToken::Function) {
            prelude += current().value + "(";
        } else {
            prelude += current().value;
        }
        advance();
    }
    while (!prelude.empty() && prelude.back() == ' ') prelude.pop_back();

    // Split name and condition: "sidebar (min-width: 400px)" → name="sidebar", condition="(min-width: 400px)"
    auto paren_pos = prelude.find('(');
    if (paren_pos != std::string::npos) {
        std::string before = prelude.substr(0, paren_pos);
        while (!before.empty() && before.back() == ' ') before.pop_back();
        rule.condition = prelude.substr(paren_pos);
        rule.name = before;
    } else {
        rule.condition = prelude;
    }

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse style rules inside @container block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        if (current().type == CSSToken::AtKeyword) {
            while (!at_end()) {
                if (current().type == CSSToken::Semicolon) { advance(); break; }
                if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
                advance();
            }
        } else {
            StyleRule style_rule;
            std::string sel_text;
            while (!at_end() && current().type != CSSToken::LeftBrace &&
                   current().type != CSSToken::RightBrace) {
                if (current().type == CSSToken::Whitespace) {
                    if (!sel_text.empty() && sel_text.back() != ' ') sel_text += " ";
                } else if (current().type == CSSToken::Hash) {
                    sel_text += "#" + current().value;
                } else if (current().type == CSSToken::Function) {
                    sel_text += current().value + "(";
                } else {
                    sel_text += current().value;
                }
                advance();
            }
            while (!sel_text.empty() && sel_text.back() == ' ') sel_text.pop_back();
            style_rule.selector_text = sel_text;
            style_rule.selectors = parse_selector_list(sel_text);

            if (!at_end() && current().type == CSSToken::LeftBrace) {
                advance();
            }
            while (!at_end() && current().type != CSSToken::RightBrace) {
                skip_whitespace();
                if (at_end() || current().type == CSSToken::RightBrace) break;
                if (current().type == CSSToken::Semicolon) { advance(); continue; }
                auto decl = parse_declaration();
                if (!decl.property.empty()) style_rule.declarations.push_back(std::move(decl));
            }
            if (!at_end() && current().type == CSSToken::RightBrace) advance();
            if (!style_rule.selectors.selectors.empty()) rule.rules.push_back(std::move(style_rule));
        }
    }

    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.container_rules.push_back(std::move(rule));
}

void StyleSheetParser::parse_property_rule(StyleSheet& sheet) {
    PropertyRule rule;
    skip_whitespace();

    // Parse property name: @property --my-color { ... }
    std::string name;
    while (!at_end() && current().type != CSSToken::LeftBrace) {
        if (current().type != CSSToken::Whitespace) {
            name += current().value;
        }
        advance();
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    rule.name = name;

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse declarations inside @property block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;
        if (current().type == CSSToken::Semicolon) { advance(); continue; }

        auto decl = parse_declaration();
        if (decl.property == "syntax") {
            // Remove quotes from syntax value
            std::string val;
            for (auto& cv : decl.values) {
                if (cv.type == ComponentValue::Token) val += cv.value;
            }
            // Strip quotes
            if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'')) {
                val = val.substr(1, val.size() - 2);
            }
            rule.syntax = val;
        } else if (decl.property == "inherits") {
            std::string val;
            for (auto& cv : decl.values) {
                if (cv.type == ComponentValue::Token) val += cv.value;
            }
            rule.inherits = (val == "true");
        } else if (decl.property == "initial-value") {
            std::string val;
            for (auto& cv : decl.values) {
                if (!val.empty() && cv.type == ComponentValue::Token) val += " ";
                if (cv.type == ComponentValue::Token) val += cv.value;
            }
            rule.initial_value = val;
        }
    }

    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    if (!rule.name.empty()) {
        sheet.property_rules.push_back(std::move(rule));
    }
}

void StyleSheetParser::parse_scope_rule(StyleSheet& sheet) {
    ScopeRule rule;
    skip_whitespace();

    // Parse: @scope [(scope-start)] [to (scope-end)] { rules }
    // Collect prelude until '{'
    std::string prelude;
    int paren_depth = 0;
    while (!at_end() && (current().type != CSSToken::LeftBrace || paren_depth > 0)) {
        if (current().type == CSSToken::LeftParen) paren_depth++;
        else if (current().type == CSSToken::RightParen) paren_depth--;

        if (current().type == CSSToken::Whitespace) {
            if (!prelude.empty() && prelude.back() != ' ') prelude += " ";
        } else if (current().type == CSSToken::Hash) {
            prelude += "#" + current().value;
        } else if (current().type == CSSToken::Function) {
            prelude += current().value + "(";
        } else {
            prelude += current().value;
        }
        advance();
    }
    while (!prelude.empty() && prelude.back() == ' ') prelude.pop_back();

    // Parse scope-start and scope-end from prelude
    // Format: "(selector)" or "(selector) to (selector)"
    auto to_pos = prelude.find(" to (");
    if (to_pos == std::string::npos) to_pos = prelude.find(" to(");

    std::string start_part, end_part;
    if (to_pos != std::string::npos) {
        start_part = prelude.substr(0, to_pos);
        end_part = prelude.substr(to_pos + 4); // skip " to "
    } else {
        start_part = prelude;
    }

    // Extract selector from parens: "(selector)" → "selector"
    auto extract_selector = [](const std::string& s) -> std::string {
        std::string trimmed = s;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (!trimmed.empty() && trimmed.front() == '(') trimmed.erase(trimmed.begin());
        if (!trimmed.empty() && trimmed.back() == ')') trimmed.pop_back();
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        return trimmed;
    };

    rule.scope_start = extract_selector(start_part);
    if (!end_part.empty()) {
        rule.scope_end = extract_selector(end_part);
    }

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse style rules inside @scope block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;

        if (current().type == CSSToken::AtKeyword) {
            while (!at_end()) {
                if (current().type == CSSToken::Semicolon) { advance(); break; }
                if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
                advance();
            }
        } else {
            StyleRule style_rule;
            std::string sel_text;
            while (!at_end() && current().type != CSSToken::LeftBrace &&
                   current().type != CSSToken::RightBrace) {
                if (current().type == CSSToken::Whitespace) {
                    if (!sel_text.empty() && sel_text.back() != ' ') sel_text += " ";
                } else if (current().type == CSSToken::Hash) {
                    sel_text += "#" + current().value;
                } else if (current().type == CSSToken::Function) {
                    sel_text += current().value + "(";
                } else {
                    sel_text += current().value;
                }
                advance();
            }
            while (!sel_text.empty() && sel_text.back() == ' ') sel_text.pop_back();
            style_rule.selector_text = sel_text;
            style_rule.selectors = parse_selector_list(sel_text);

            if (!at_end() && current().type == CSSToken::LeftBrace) {
                advance();
            }
            while (!at_end() && current().type != CSSToken::RightBrace) {
                skip_whitespace();
                if (at_end() || current().type == CSSToken::RightBrace) break;
                if (current().type == CSSToken::Semicolon) { advance(); continue; }
                auto decl = parse_declaration();
                if (!decl.property.empty()) style_rule.declarations.push_back(std::move(decl));
            }
            if (!at_end() && current().type == CSSToken::RightBrace) advance();
            if (!style_rule.selectors.selectors.empty()) rule.rules.push_back(std::move(style_rule));
        }
    }

    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.scope_rules.push_back(std::move(rule));
}

void StyleSheetParser::parse_counter_style_rule(StyleSheet& sheet) {
    CounterStyleRule rule;
    skip_whitespace();

    // Parse name: @counter-style name { descriptors }
    std::string name;
    while (!at_end() && current().type != CSSToken::LeftBrace &&
           current().type != CSSToken::Semicolon) {
        if (current().type != CSSToken::Whitespace) {
            name += current().value;
        }
        advance();
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    rule.name = name;

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse descriptors inside @counter-style block
    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;
        if (current().type == CSSToken::Semicolon) { advance(); continue; }
        auto decl = parse_declaration();
        if (!decl.property.empty()) {
            // Convert component values to string
            std::string val;
            for (const auto& cv : decl.values) {
                if (!val.empty()) val += " ";
                val += cv.value;
            }
            rule.descriptors[decl.property] = val;
        }
    }
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    if (!rule.name.empty()) {
        sheet.counter_style_rules.push_back(std::move(rule));
    }
}

void StyleSheetParser::parse_starting_style_rule() {
    skip_whitespace();
    // @starting-style { ... } — parse and discard (CSS transitions initial styles)
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        skip_block();
    } else {
        // Skip to semicolon or block
        while (!at_end()) {
            if (current().type == CSSToken::Semicolon) { advance(); break; }
            if (current().type == CSSToken::LeftBrace) { skip_block(); break; }
            advance();
        }
    }
}

void StyleSheetParser::parse_font_palette_values_rule() {
    skip_whitespace();
    // @font-palette-values name { ... } — parse and discard
    // Skip name and any tokens until '{' or ';'
    while (!at_end() && current().type != CSSToken::LeftBrace &&
           current().type != CSSToken::Semicolon) {
        advance();
    }
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        skip_block();
    } else if (!at_end() && current().type == CSSToken::Semicolon) {
        advance();
    }
}

// Check if current token starts a nested rule inside a parent rule block.
// Declarations always start with Ident (property name) followed by ':'.
// Nested rules start with selector-like tokens: &, ., #, [, :, >, +, ~, *
bool StyleSheetParser::is_nested_rule_start() {
    auto& tok = current();
    // Explicit nesting ampersand
    if (tok.type == CSSToken::Delim && tok.value == "&") return true;
    // Class selector
    if (tok.type == CSSToken::Delim && tok.value == ".") return true;
    // Combinators (child, adjacent sibling, general sibling)
    if (tok.type == CSSToken::Delim &&
        (tok.value == ">" || tok.value == "+" || tok.value == "~")) return true;
    // Universal selector
    if (tok.type == CSSToken::Delim && tok.value == "*") return true;
    // ID selector — at start of statement, this is a nested rule, not a hex value
    if (tok.type == CSSToken::Hash) return true;
    // Pseudo selector (:hover, ::before) — at statement start, always a selector
    if (tok.type == CSSToken::Colon) return true;
    // Attribute selector
    if (tok.type == CSSToken::LeftBracket) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Resolve a nested selector relative to the parent selector.
// If '&' appears in nested_sel, replace every '&' with parent_selector.
// Otherwise prepend parent_selector as a descendant combinator.
// ---------------------------------------------------------------------------
static std::string resolve_nested_selector(const std::string& parent_selector,
                                           const std::string& nested_sel) {
    std::string resolved;
    if (nested_sel.find('&') != std::string::npos) {
        resolved = nested_sel;
        size_t amp_pos = 0;
        while ((amp_pos = resolved.find('&', amp_pos)) != std::string::npos) {
            resolved.replace(amp_pos, 1, parent_selector);
            amp_pos += parent_selector.size();
        }
    } else {
        resolved = parent_selector + " " + nested_sel;
    }
    return resolved;
}

// ---------------------------------------------------------------------------
// Parse the content inside a { } block, collecting declarations into
// out_declarations and recursively flattening nested rules into
// out_nested_rules.  parent_selector is the fully-resolved selector of the
// enclosing rule (used to resolve '&' references).
// Caller must have already consumed the opening '{'.
// This method does NOT consume the closing '}'.
// ---------------------------------------------------------------------------
void StyleSheetParser::parse_nested_block(
        const std::string& parent_selector,
        std::vector<Declaration>& out_declarations,
        std::vector<StyleRule>& out_nested_rules) {

    while (!at_end() && current().type != CSSToken::RightBrace) {
        skip_whitespace();
        if (at_end() || current().type == CSSToken::RightBrace) break;
        if (current().type == CSSToken::Semicolon) {
            advance();
            continue;
        }

        // CSS Nesting: detect nested rule inside parent rule block
        if (is_nested_rule_start()) {
            // Consume nested selector text until '{'
            std::string nested_sel;
            while (!at_end() && current().type != CSSToken::LeftBrace &&
                   current().type != CSSToken::RightBrace) {
                if (current().type == CSSToken::Whitespace) {
                    if (!nested_sel.empty() && nested_sel.back() != ' ') {
                        nested_sel += " ";
                    }
                } else if (current().type == CSSToken::Delim && current().value == "&") {
                    nested_sel += "&";
                } else if (current().type == CSSToken::Hash) {
                    nested_sel += "#" + current().value;
                } else if (current().type == CSSToken::Function) {
                    nested_sel += current().value + "(";
                } else {
                    nested_sel += current().value;
                }
                advance();
            }
            while (!nested_sel.empty() && nested_sel.back() == ' ') {
                nested_sel.pop_back();
            }

            // If we hit '}' instead of '{', this wasn't a nested rule — skip
            if (at_end() || current().type != CSSToken::LeftBrace) {
                continue;
            }

            // Skip '{'
            advance();

            // Resolve the nested selector relative to the parent
            std::string resolved = resolve_nested_selector(parent_selector, nested_sel);

            // Recursively parse the nested block (handles deeper nesting)
            StyleRule nested_rule;
            std::vector<StyleRule> deeper_rules;
            parse_nested_block(resolved, nested_rule.declarations, deeper_rules);

            // Skip '}'
            if (!at_end() && current().type == CSSToken::RightBrace) {
                advance();
            }

            nested_rule.selector_text = resolved;
            nested_rule.selectors = parse_selector_list(resolved);
            out_nested_rules.push_back(std::move(nested_rule));

            // Append any deeper-nested rules (already fully resolved)
            for (auto& dr : deeper_rules) {
                out_nested_rules.push_back(std::move(dr));
            }
        } else {
            auto decl = parse_declaration();
            if (!decl.property.empty()) {
                out_declarations.push_back(std::move(decl));
            }
        }
    }
}

void StyleSheetParser::parse_style_rule(StyleSheet& sheet) {
    StyleRule rule;

    // Consume selector text until '{'
    std::string sel_text;
    while (!at_end() && current().type != CSSToken::LeftBrace) {
        if (current().type == CSSToken::Whitespace) {
            if (!sel_text.empty() && sel_text.back() != ' ') {
                sel_text += " ";
            }
        } else if (current().type == CSSToken::Hash) {
            // Preserve '#' prefix for ID selectors (tokenizer strips it)
            sel_text += "#" + current().value;
        } else if (current().type == CSSToken::Function) {
            // Function token value is just the name without '('
            sel_text += current().value + "(";
        } else {
            sel_text += current().value;
        }
        advance();
    }

    // Trim trailing whitespace
    while (!sel_text.empty() && sel_text.back() == ' ') {
        sel_text.pop_back();
    }

    rule.selector_text = sel_text;
    rule.selectors = parse_selector_list(sel_text);

    // Skip '{'
    if (!at_end() && current().type == CSSToken::LeftBrace) {
        advance();
    }

    // Parse declarations and nested rules (recursively)
    std::vector<StyleRule> nested_rules;
    parse_nested_block(sel_text, rule.declarations, nested_rules);

    // Skip '}'
    if (!at_end() && current().type == CSSToken::RightBrace) {
        advance();
    }

    sheet.rules.push_back(std::move(rule));

    // Append nested rules after parent (they have higher specificity in cascade order)
    for (auto& nr : nested_rules) {
        sheet.rules.push_back(std::move(nr));
    }
}

Declaration StyleSheetParser::parse_declaration() {
    Declaration decl;
    skip_whitespace();

    // Property name
    if (!at_end() && current().type == CSSToken::Ident) {
        decl.property = current().value;
        advance();
    } else {
        // Not a valid declaration start; skip to next semicolon or brace
        while (!at_end() && current().type != CSSToken::Semicolon &&
               current().type != CSSToken::RightBrace) {
            advance();
        }
        if (!at_end() && current().type == CSSToken::Semicolon) {
            advance();
        }
        return decl;
    }

    skip_whitespace();

    // Expect ':'
    if (!at_end() && current().type == CSSToken::Colon) {
        advance();
    }

    skip_whitespace();

    // Parse component values until ';' or '}' or EOF
    decl.values = parse_component_values_until(CSSToken::Semicolon,
                                                CSSToken::RightBrace);

    // Check for !important at the end of values
    // Look backwards through values for '!' followed by 'important'
    if (decl.values.size() >= 2) {
        size_t n = decl.values.size();
        // Check last two non-whitespace values
        // Find the last non-whitespace value
        int last_idx = static_cast<int>(n) - 1;
        while (last_idx >= 0 && decl.values[last_idx].value.empty()) {
            last_idx--;
        }
        int second_last = last_idx - 1;
        while (second_last >= 0 && decl.values[second_last].value.empty()) {
            second_last--;
        }

        if (last_idx >= 0 && second_last >= 0 &&
            decl.values[second_last].value == "!" &&
            decl.values[last_idx].value == "important") {
            decl.important = true;
            // Remove the ! and important from values
            // We need to remove from second_last to end
            decl.values.erase(decl.values.begin() + second_last,
                              decl.values.end());
            // Also trim trailing whitespace-like values
            while (!decl.values.empty() && decl.values.back().value == " ") {
                decl.values.pop_back();
            }
        }
    }

    // Skip semicolon if present
    if (!at_end() && current().type == CSSToken::Semicolon) {
        advance();
    }

    return decl;
}

std::vector<ComponentValue> StyleSheetParser::parse_component_values_until(
    CSSToken::Type stop1, CSSToken::Type stop2) {
    std::vector<ComponentValue> values;

    while (!at_end() && current().type != stop1 && current().type != stop2) {
        // Skip whitespace between values (but record that there was whitespace)
        if (current().type == CSSToken::Whitespace) {
            advance();
            continue;
        }

        values.push_back(consume_component_value());
    }

    return values;
}

ComponentValue StyleSheetParser::consume_component_value() {
    // Function
    if (current().type == CSSToken::Function) {
        return consume_function();
    }

    // Simple block tokens (left paren, left bracket)
    if (current().type == CSSToken::LeftParen) {
        ComponentValue cv;
        cv.type = ComponentValue::Block;
        cv.value = "(";
        advance(); // skip '('
        while (!at_end() && current().type != CSSToken::RightParen) {
            if (current().type == CSSToken::Whitespace) {
                advance();
                continue;
            }
            cv.children.push_back(consume_component_value());
        }
        if (!at_end()) advance(); // skip ')'
        return cv;
    }

    if (current().type == CSSToken::LeftBracket) {
        ComponentValue cv;
        cv.type = ComponentValue::Block;
        cv.value = "[";
        advance(); // skip '['
        while (!at_end() && current().type != CSSToken::RightBracket) {
            if (current().type == CSSToken::Whitespace) {
                advance();
                continue;
            }
            cv.children.push_back(consume_component_value());
        }
        if (!at_end()) advance(); // skip ']'
        return cv;
    }

    // Regular token -> ComponentValue
    ComponentValue cv;
    cv.type = ComponentValue::Token;
    auto& tok = current();

    // Preserve '#' prefix for hash tokens so color parsing works
    if (tok.type == CSSToken::Hash) {
        cv.value = "#" + tok.value;
    } else {
        cv.value = tok.value;
    }

    if (tok.type == CSSToken::Number || tok.type == CSSToken::Percentage ||
        tok.type == CSSToken::Dimension) {
        cv.numeric_value = tok.numeric_value;
        cv.unit = tok.unit;
    } else if (tok.type == CSSToken::String) {
        cv.unit = "string";  // mark as CSS quoted string literal
    }

    advance();
    return cv;
}

ComponentValue StyleSheetParser::consume_function() {
    ComponentValue cv;
    cv.type = ComponentValue::Function;
    cv.value = current().value; // function name
    advance(); // skip function token (name + '(')

    // Consume arguments until ')'
    while (!at_end() && current().type != CSSToken::RightParen) {
        if (current().type == CSSToken::Whitespace) {
            advance();
            continue;
        }
        if (current().type == CSSToken::Comma) {
            // Preserve comma separators so functions like var(--x, fallback)
            // can be reconstructed faithfully for later evaluation.
            ComponentValue comma;
            comma.type = ComponentValue::Token;
            comma.value = ",";
            cv.children.push_back(std::move(comma));
            advance();
            continue;
        }
        cv.children.push_back(consume_component_value());
    }

    // Skip ')'
    if (!at_end() && current().type == CSSToken::RightParen) {
        advance();
    }

    return cv;
}

std::vector<Declaration> StyleSheetParser::parse_declarations() {
    std::vector<Declaration> decls;

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;
        if (current().type == CSSToken::Semicolon) {
            advance();
            continue;
        }
        if (current().type == CSSToken::RightBrace) {
            break;
        }
        auto decl = parse_declaration();
        if (!decl.property.empty()) {
            decls.push_back(std::move(decl));
        }
    }

    return decls;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

StyleSheet parse_stylesheet(std::string_view css) {
    auto tokens = CSSTokenizer::tokenize_all(css);
    StyleSheetParser parser(std::move(tokens));
    return parser.parse();
}

std::vector<Declaration> parse_declaration_block(std::string_view css) {
    auto tokens = CSSTokenizer::tokenize_all(css);
    StyleSheetParser parser(std::move(tokens));
    return parser.parse_declarations();
}

} // namespace clever::css
