#include "browser/html/html_parser.h"

#include <cctype>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace browser::html {
namespace {

constexpr char kDocumentTag[] = "#document";

const std::unordered_set<std::string> kVoidElements = {
    "area", "base", "br",   "col",  "embed", "hr",    "img",
    "input","link", "meta", "param","source","track", "wbr",
};

unsigned char uchar(char c) {
    return static_cast<unsigned char>(c);
}

bool is_space(char c) {
    return std::isspace(uchar(c)) != 0;
}

bool is_ascii_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

bool is_name_char(char c) {
    return std::isalnum(uchar(c)) != 0 || c == '-' || c == '_' || c == ':' || c == '.';
}

std::string to_lower_ascii(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (char c : text) {
        lowered.push_back(static_cast<char>(std::tolower(uchar(c))));
    }
    return lowered;
}

bool is_void_element(const std::string& tag_name) {
    return kVoidElements.find(tag_name) != kVoidElements.end();
}

int decode_decimal_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    return -1;
}

int decode_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

bool append_utf8_code_point(std::uint32_t code_point, std::string& decoded) {
    if (code_point == 0 || code_point > 0x10FFFFu ||
        (code_point >= 0xD800u && code_point <= 0xDFFFu)) {
        return false;
    }

    if (code_point <= 0x7Fu) {
        decoded.push_back(static_cast<char>(code_point));
        return true;
    }

    if (code_point <= 0x7FFu) {
        decoded.push_back(static_cast<char>(0xC0u | (code_point >> 6)));
        decoded.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
        return true;
    }

    if (code_point <= 0xFFFFu) {
        decoded.push_back(static_cast<char>(0xE0u | (code_point >> 12)));
        decoded.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
        decoded.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
        return true;
    }

    decoded.push_back(static_cast<char>(0xF0u | (code_point >> 18)));
    decoded.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3Fu)));
    decoded.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
    decoded.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    return true;
}

bool decode_numeric_entity(const std::string& text,
                           std::size_t entity_start,
                           std::size_t semicolon,
                           std::string& decoded) {
    if (entity_start + 3 > semicolon ||
        text[entity_start] != '&' ||
        text[entity_start + 1] != '#') {
        return false;
    }

    std::size_t pos = entity_start + 2;
    std::uint32_t base = 10;

    if (text[pos] == 'x' || text[pos] == 'X') {
        base = 16;
        ++pos;
    }

    if (pos >= semicolon) {
        return false;
    }

    std::uint32_t value = 0;
    for (; pos < semicolon; ++pos) {
        const int digit = (base == 10) ? decode_decimal_digit(text[pos])
                                       : decode_hex_digit(text[pos]);
        if (digit < 0) {
            return false;
        }

        if (value > (0x10FFFFu - static_cast<std::uint32_t>(digit)) / base) {
            return false;
        }
        value = value * base + static_cast<std::uint32_t>(digit);
    }

    return append_utf8_code_point(value, decoded);
}

std::string decode_html_entities(const std::string& text) {
    if (text.find('&') == std::string::npos) {
        return text;
    }

    std::string decoded;
    decoded.reserve(text.size());

    std::size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] != '&') {
            decoded.push_back(text[pos]);
            ++pos;
            continue;
        }

        const std::size_t semicolon = text.find(';', pos + 1);
        if (semicolon == std::string::npos) {
            decoded.push_back(text[pos]);
            ++pos;
            continue;
        }

        const std::size_t entity_length = semicolon - pos + 1;
        if (text.compare(pos, entity_length, "&amp;") == 0) {
            decoded.push_back('&');
        } else if (text.compare(pos, entity_length, "&lt;") == 0) {
            decoded.push_back('<');
        } else if (text.compare(pos, entity_length, "&gt;") == 0) {
            decoded.push_back('>');
        } else if (text.compare(pos, entity_length, "&quot;") == 0) {
            decoded.push_back('"');
        } else if (text.compare(pos, entity_length, "&apos;") == 0) {
            decoded.push_back('\'');
        } else if (text.compare(pos, entity_length, "&nbsp;") == 0) {
            append_utf8_code_point(0xA0u, decoded);
        } else if (text.compare(pos, entity_length, "&cent;") == 0) {
            append_utf8_code_point(0xA2u, decoded);
        } else if (text.compare(pos, entity_length, "&pound;") == 0) {
            append_utf8_code_point(0xA3u, decoded);
        } else if (text.compare(pos, entity_length, "&yen;") == 0) {
            append_utf8_code_point(0xA5u, decoded);
        } else if (text.compare(pos, entity_length, "&sect;") == 0) {
            append_utf8_code_point(0xA7u, decoded);
        } else if (text.compare(pos, entity_length, "&deg;") == 0) {
            append_utf8_code_point(0xB0u, decoded);
        } else if (text.compare(pos, entity_length, "&euro;") == 0) {
            append_utf8_code_point(0x20ACu, decoded);
        } else if (text.compare(pos, entity_length, "&copy;") == 0) {
            append_utf8_code_point(0xA9u, decoded);
        } else if (text.compare(pos, entity_length, "&reg;") == 0) {
            append_utf8_code_point(0xAEu, decoded);
        } else if (text.compare(pos, entity_length, "&trade;") == 0) {
            append_utf8_code_point(0x2122u, decoded);
        } else if (text.compare(pos, entity_length, "&ndash;") == 0) {
            append_utf8_code_point(0x2013u, decoded);
        } else if (text.compare(pos, entity_length, "&mdash;") == 0) {
            append_utf8_code_point(0x2014u, decoded);
        } else if (text.compare(pos, entity_length, "&#39;") == 0) {
            decoded.push_back('\'');
        } else if (decode_numeric_entity(text, pos, semicolon, decoded)) {
            // Numeric entity consumed and appended as UTF-8.
        } else {
            decoded.append(text, pos, entity_length);
        }

        pos = semicolon + 1;
    }

    return decoded;
}

class Parser {
  public:
    explicit Parser(const std::string& html, bool collect_warnings = false)
        : html_(html), collect_warnings_(collect_warnings) {}

    std::unique_ptr<Node> parse() {
        auto document = std::make_unique<Node>(NodeType::Document, kDocumentTag);
        stack_.push_back(document.get());

        while (position_ < html_.size()) {
            if (html_[position_] != '<') {
                parse_text(document.get());
                continue;
            }

            if (starts_with("<!--")) {
                skip_comment();
                continue;
            }

            if (starts_with("</")) {
                parse_end_tag();
                continue;
            }

            if (starts_with("<!")) {
                skip_declaration();
                continue;
            }

            if (parse_start_tag(document.get())) {
                continue;
            }

            if (collect_warnings_) {
                add_warning("Bare '<' treated as text",
                            "Inserted literal '<' into text content");
            }
            append_text(current_parent(document.get()), "<");
            ++position_;
        }

        // Check for unclosed elements at end of parse
        if (collect_warnings_ && stack_.size() > 1) {
            for (std::size_t i = stack_.size() - 1; i >= 1; --i) {
                add_warning("Unclosed element <" + stack_[i]->tag_name + ">",
                            "Implicitly closed at end of document");
            }
        }

        return document;
    }

    std::vector<ParseWarning> take_warnings() { return std::move(warnings_); }

  private:
    const std::string& html_;
    std::size_t position_ = 0;
    std::vector<Node*> stack_;
    bool collect_warnings_ = false;
    std::vector<ParseWarning> warnings_;

    void add_warning(std::string message, std::string recovery) {
        ParseWarning w;
        w.message = std::move(message);
        w.recovery_action = std::move(recovery);
        warnings_.push_back(std::move(w));
    }

    Node* current_parent(Node* document) const {
        if (!stack_.empty()) {
            return stack_.back();
        }
        return document;
    }

    bool starts_with(const std::string& token) const {
        if (position_ + token.size() > html_.size()) {
            return false;
        }
        return html_.compare(position_, token.size(), token) == 0;
    }

    void skip_spaces(std::size_t& pos) const {
        while (pos < html_.size() && is_space(html_[pos])) {
            ++pos;
        }
    }

    std::string parse_name(std::size_t& pos) const {
        const std::size_t start = pos;
        while (pos < html_.size() && is_name_char(html_[pos])) {
            ++pos;
        }
        return html_.substr(start, pos - start);
    }

    std::string parse_attr_name(std::size_t& pos) const {
        const std::size_t start = pos;
        while (pos < html_.size() &&
               !is_space(html_[pos]) &&
               html_[pos] != '=' &&
               html_[pos] != '>' &&
               html_[pos] != '/') {
            ++pos;
        }
        return html_.substr(start, pos - start);
    }

    void append_text(Node* parent, std::string text) {
        if (text.empty()) {
            return;
        }
        if (!parent->children.empty() &&
            parent->children.back()->type == NodeType::Text) {
            parent->children.back()->text_content += text;
            return;
        }

        auto text_node = std::make_unique<Node>(NodeType::Text);
        text_node->text_content = std::move(text);
        text_node->parent = parent;
        parent->children.push_back(std::move(text_node));
    }

    void parse_text(Node* document) {
        const std::size_t next_tag = html_.find('<', position_);
        const std::size_t end = (next_tag == std::string::npos) ? html_.size() : next_tag;

        append_text(current_parent(document),
                    decode_html_entities(html_.substr(position_, end - position_)));
        position_ = end;
    }

    void skip_comment() {
        const std::size_t comment_end = html_.find("-->", position_ + 4);
        if (comment_end == std::string::npos) {
            if (collect_warnings_) {
                add_warning("Unclosed HTML comment",
                            "Consumed remaining input as comment");
            }
            position_ = html_.size();
            return;
        }
        position_ = comment_end + 3;
    }

    void skip_declaration() {
        const std::size_t declaration_end = html_.find('>', position_ + 2);
        if (declaration_end == std::string::npos) {
            if (collect_warnings_) {
                add_warning("Unclosed declaration/DOCTYPE",
                            "Consumed remaining input as declaration");
            }
            position_ = html_.size();
            return;
        }
        position_ = declaration_end + 1;
    }

    void parse_end_tag() {
        std::size_t pos = position_ + 2;
        skip_spaces(pos);

        std::string tag = to_lower_ascii(parse_name(pos));
        const std::size_t tag_end = html_.find('>', pos);
        position_ = (tag_end == std::string::npos) ? html_.size() : tag_end + 1;

        if (tag.empty() || stack_.size() <= 1) {
            if (collect_warnings_ && !tag.empty()) {
                add_warning("Orphan end tag </" + tag + "> with no matching open tag",
                            "Ignored orphan end tag");
            }
            return;
        }

        // Check for tag mismatch — unwind stack to matching tag
        bool found = false;
        std::size_t match_index = 0;
        for (std::size_t i = stack_.size(); i > 1; --i) {
            if (to_lower_ascii(stack_[i - 1]->tag_name) == tag) {
                found = true;
                match_index = i - 1;
                break;
            }
        }

        if (!found) {
            if (collect_warnings_) {
                add_warning("Unmatched end tag </" + tag + ">",
                            "Ignored unmatched end tag");
            }
            return;
        }

        // Warn about implicitly closed elements during stack unwinding
        if (collect_warnings_ && match_index < stack_.size() - 1) {
            for (std::size_t i = stack_.size() - 1; i > match_index; --i) {
                add_warning("Element <" + stack_[i]->tag_name +
                            "> implicitly closed by </" + tag + ">",
                            "Implicitly closed intervening element");
            }
        }

        stack_.resize(match_index);
    }

    bool parse_start_tag(Node* document) {
        std::size_t pos = position_ + 1;
        skip_spaces(pos);

        std::string tag = parse_name(pos);
        if (tag.empty()) {
            return false;
        }
        tag = to_lower_ascii(tag);

        std::map<std::string, std::string> attributes;
        bool self_closing = false;

        while (pos < html_.size()) {
            skip_spaces(pos);

            if (pos >= html_.size()) {
                break;
            }
            if (html_[pos] == '>') {
                ++pos;
                break;
            }
            if (html_[pos] == '/' && (pos + 1) < html_.size() && html_[pos + 1] == '>') {
                self_closing = true;
                pos += 2;
                break;
            }

            std::string attr_name = parse_attr_name(pos);
            if (attr_name.empty()) {
                ++pos;
                continue;
            }
            attr_name = to_lower_ascii(attr_name);

            skip_spaces(pos);
            std::string attr_value;

            if (pos < html_.size() && html_[pos] == '=') {
                ++pos;
                skip_spaces(pos);

                if (pos < html_.size() && (html_[pos] == '"' || html_[pos] == '\'')) {
                    const char quote = html_[pos++];
                    const std::size_t value_start = pos;
                    while (pos < html_.size() && html_[pos] != quote) {
                        ++pos;
                    }
                    attr_value = html_.substr(value_start, pos - value_start);
                    if (pos < html_.size() && html_[pos] == quote) {
                        ++pos;
                    }
                } else {
                    const std::size_t value_start = pos;
                    while (pos < html_.size() &&
                           !is_space(html_[pos]) &&
                           html_[pos] != '>') {
                        if (html_[pos] == '/' &&
                            (pos + 1) < html_.size() &&
                            html_[pos + 1] == '>') {
                            break;
                        }
                        ++pos;
                    }
                    attr_value = html_.substr(value_start, pos - value_start);
                }
            }

            attr_value = decode_html_entities(attr_value);
            attributes[std::move(attr_name)] = std::move(attr_value);
        }

        if (is_void_element(tag)) {
            self_closing = true;
        }

        auto element = std::make_unique<Node>(NodeType::Element, std::move(tag));
        element->attributes = std::move(attributes);
        Node* parent = current_parent(document);
        element->parent = parent;
        Node* element_ptr = element.get();

        parent->children.push_back(std::move(element));

        if (!self_closing) {
            stack_.push_back(element_ptr);
        }

        position_ = pos;
        return true;
    }
};

void collect_by_tag(const Node& node, const std::string& tag, std::vector<const Node*>& result) {
    if (node.type == NodeType::Element && to_lower_ascii(node.tag_name) == tag) {
        result.push_back(&node);
    }
    for (const auto& child : node.children) {
        collect_by_tag(*child, tag, result);
    }
}

bool has_attr_token(const std::string& attr_value, const std::string& token) {
    std::size_t pos = 0;
    while (pos < attr_value.size()) {
        while (pos < attr_value.size() && is_ascii_whitespace(attr_value[pos])) {
            ++pos;
        }

        const std::size_t token_start = pos;
        while (pos < attr_value.size() && !is_ascii_whitespace(attr_value[pos])) {
            ++pos;
        }

        const std::size_t token_length = pos - token_start;
        if (token_length == token.size() &&
            attr_value.compare(token_start, token_length, token) == 0) {
            return true;
        }
    }
    return false;
}

template <typename Predicate>
const Node* find_first_if(const Node& node, const Predicate& predicate) {
    if (predicate(node)) {
        return &node;
    }

    for (const auto& child : node.children) {
        if (const Node* match = find_first_if(*child, predicate)) {
            return match;
        }
    }
    return nullptr;
}

const Node* find_first_by_id(const Node& node, const std::string& id) {
    return find_first_if(node, [&id](const Node& candidate) {
        if (candidate.type != NodeType::Element) {
            return false;
        }
        const auto id_it = candidate.attributes.find("id");
        return id_it != candidate.attributes.end() && id_it->second == id;
    });
}

const Node* find_first_by_tag(const Node& node, const std::string& tag) {
    return find_first_if(node, [&tag](const Node& candidate) {
        return candidate.type == NodeType::Element &&
               to_lower_ascii(candidate.tag_name) == tag;
    });
}

const Node* find_first_by_attr(const Node& node,
                               const std::string& attr,
                               const std::string& value) {
    return find_first_if(node, [&attr, &value](const Node& candidate) {
        if (candidate.type != NodeType::Element) {
            return false;
        }
        const auto attr_it = candidate.attributes.find(attr);
        return attr_it != candidate.attributes.end() && attr_it->second == value;
    });
}

const Node* find_first_by_attr_token(const Node& node,
                                     const std::string& attr,
                                     const std::string& token) {
    return find_first_if(node, [&attr, &token](const Node& candidate) {
        if (candidate.type != NodeType::Element) {
            return false;
        }
        const auto attr_it = candidate.attributes.find(attr);
        return attr_it != candidate.attributes.end() &&
               has_attr_token(attr_it->second, token);
    });
}

void collect_by_attr(const Node& node,
                     const std::string& attr,
                     const std::string& value,
                     std::vector<const Node*>& result) {
    if (node.type == NodeType::Element) {
        const auto attr_it = node.attributes.find(attr);
        if (attr_it != node.attributes.end() && attr_it->second == value) {
            result.push_back(&node);
        }
    }

    for (const auto& child : node.children) {
        collect_by_attr(*child, attr, value, result);
    }
}

void collect_by_attr_token(const Node& node,
                           const std::string& attr,
                           const std::string& token,
                           std::vector<const Node*>& result) {
    if (node.type == NodeType::Element) {
        const auto attr_it = node.attributes.find(attr);
        if (attr_it != node.attributes.end() && has_attr_token(attr_it->second, token)) {
            result.push_back(&node);
        }
    }

    for (const auto& child : node.children) {
        collect_by_attr_token(*child, attr, token, result);
    }
}

void collect_by_class(const Node& node, const std::string& class_name, std::vector<const Node*>& result) {
    collect_by_attr_token(node, "class", class_name, result);
}

void collect_by_text_contains(const Node& node,
                              const std::string& needle,
                              std::vector<const Node*>& result) {
    if (node.type == NodeType::Element) {
        const std::string node_text = inner_text(node);
        if (node_text.find(needle) != std::string::npos) {
            result.push_back(&node);
        }
    }

    for (const auto& child : node.children) {
        collect_by_text_contains(*child, needle, result);
    }
}

void collect_text(const Node& node, std::string& output) {
    if (node.type == NodeType::Text) {
        output += node.text_content;
    }
    for (const auto& child : node.children) {
        collect_text(*child, output);
    }
}

}  // namespace

std::unique_ptr<Node> parse_html(const std::string& html) {
    return Parser(html).parse();
}

ParseResult parse_html_with_diagnostics(const std::string& html) {
    Parser parser(html, true);
    ParseResult result;
    result.document = parser.parse();
    result.warnings = parser.take_warnings();
    return result;
}

std::vector<Node*> query_all_by_tag(Node& root, const std::string& tag) {
    std::vector<Node*> result;
    const std::vector<const Node*> matches = query_all_by_tag(static_cast<const Node&>(root), tag);
    result.reserve(matches.size());
    for (const Node* match : matches) {
        result.push_back(const_cast<Node*>(match));
    }
    return result;
}

std::vector<const Node*> query_all_by_tag(const Node& root, const std::string& tag) {
    std::vector<const Node*> result;
    if (tag.empty()) {
        return result;
    }
    collect_by_tag(root, to_lower_ascii(tag), result);
    return result;
}

Node* query_first_by_tag(Node& root, const std::string& tag) {
    return const_cast<Node*>(query_first_by_tag(static_cast<const Node&>(root), tag));
}

const Node* query_first_by_tag(const Node& root, const std::string& tag) {
    if (tag.empty()) {
        return nullptr;
    }
    return find_first_by_tag(root, to_lower_ascii(tag));
}

Node* query_first_by_id(Node& root, const std::string& id) {
    return const_cast<Node*>(query_first_by_id(static_cast<const Node&>(root), id));
}

const Node* query_first_by_id(const Node& root, const std::string& id) {
    if (id.empty()) {
        return nullptr;
    }
    return find_first_by_id(root, id);
}

std::vector<Node*> query_all_by_attr(Node& root, const std::string& attr, const std::string& value) {
    std::vector<Node*> result;
    const std::vector<const Node*> matches =
        query_all_by_attr(static_cast<const Node&>(root), attr, value);
    result.reserve(matches.size());
    for (const Node* match : matches) {
        result.push_back(const_cast<Node*>(match));
    }
    return result;
}

std::vector<const Node*> query_all_by_attr(const Node& root,
                                           const std::string& attr,
                                           const std::string& value) {
    std::vector<const Node*> result;
    if (attr.empty()) {
        return result;
    }
    collect_by_attr(root, to_lower_ascii(attr), value, result);
    return result;
}

std::vector<Node*> query_all_by_attr_token(Node& root, const std::string& attr, const std::string& token) {
    std::vector<Node*> result;
    const std::vector<const Node*> matches =
        query_all_by_attr_token(static_cast<const Node&>(root), attr, token);
    result.reserve(matches.size());
    for (const Node* match : matches) {
        result.push_back(const_cast<Node*>(match));
    }
    return result;
}

std::vector<const Node*> query_all_by_attr_token(const Node& root,
                                                 const std::string& attr,
                                                 const std::string& token) {
    std::vector<const Node*> result;
    if (attr.empty() || token.empty()) {
        return result;
    }
    collect_by_attr_token(root, to_lower_ascii(attr), token, result);
    return result;
}

Node* query_first_by_attr_token(Node& root,
                                const std::string& attr,
                                const std::string& token) {
    return const_cast<Node*>(
        query_first_by_attr_token(static_cast<const Node&>(root), attr, token));
}

const Node* query_first_by_attr_token(const Node& root,
                                      const std::string& attr,
                                      const std::string& token) {
    if (attr.empty() || token.empty()) {
        return nullptr;
    }
    return find_first_by_attr_token(root, to_lower_ascii(attr), token);
}

Node* query_first_by_attr(Node& root, const std::string& attr, const std::string& value) {
    return const_cast<Node*>(query_first_by_attr(static_cast<const Node&>(root), attr, value));
}

const Node* query_first_by_attr(const Node& root, const std::string& attr, const std::string& value) {
    if (attr.empty()) {
        return nullptr;
    }
    return find_first_by_attr(root, to_lower_ascii(attr), value);
}

std::vector<Node*> query_all_by_class(Node& root, const std::string& class_name) {
    std::vector<Node*> result;
    const std::vector<const Node*> matches =
        query_all_by_class(static_cast<const Node&>(root), class_name);
    result.reserve(matches.size());
    for (const Node* match : matches) {
        result.push_back(const_cast<Node*>(match));
    }
    return result;
}

std::vector<const Node*> query_all_by_class(const Node& root, const std::string& class_name) {
    std::vector<const Node*> result;
    if (class_name.empty()) {
        return result;
    }
    collect_by_class(root, class_name, result);
    return result;
}

Node* query_first_by_class(Node& root, const std::string& class_name) {
    return const_cast<Node*>(query_first_by_class(static_cast<const Node&>(root), class_name));
}

const Node* query_first_by_class(const Node& root, const std::string& class_name) {
    const std::vector<const Node*> matches = query_all_by_class(root, class_name);
    if (matches.empty()) {
        return nullptr;
    }
    return matches.front();
}

std::vector<Node*> query_all_text_contains(Node& root, const std::string& needle) {
    std::vector<Node*> result;
    const std::vector<const Node*> matches =
        query_all_text_contains(static_cast<const Node&>(root), needle);
    result.reserve(matches.size());
    for (const Node* match : matches) {
        result.push_back(const_cast<Node*>(match));
    }
    return result;
}

std::vector<const Node*> query_all_text_contains(const Node& root, const std::string& needle) {
    std::vector<const Node*> result;
    if (needle.empty()) {
        return result;
    }
    collect_by_text_contains(root, needle, result);
    return result;
}

std::string inner_text(const Node& root) {
    std::string text;
    collect_text(root, text);
    return text;
}

std::string serialize_dom(const Node& node) {
    std::string output;

    switch (node.type) {
        case NodeType::Document:
            output += "#document";
            break;
        case NodeType::Text:
            output += "TEXT(\"" + node.text_content + "\")";
            return output;
        case NodeType::Element:
            output += "<" + node.tag_name;
            for (const auto& [key, value] : node.attributes) {
                output += " " + key + "=\"" + value + "\"";
            }
            output += ">";
            break;
    }

    for (const auto& child : node.children) {
        if (child) {
            output += "[" + serialize_dom(*child) + "]";
        }
    }

    if (node.type == NodeType::Element) {
        output += "</" + node.tag_name + ">";
    }

    return output;
}

}  // namespace browser::html
