#include <clever/html/tree_builder.h>
#include <clever/dom/document.h>
#include <clever/dom/element.h>
#include <clever/dom/text.h>
#include <clever/dom/comment.h>
#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace clever::html {

// ============================================================================
// Helper utilities
// ============================================================================

static const std::unordered_set<std::string>& void_elements() {
    static const std::unordered_set<std::string> s = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "source", "track", "wbr"
    };
    return s;
}

static const std::unordered_set<std::string>& formatting_elements() {
    static const std::unordered_set<std::string> s = {
        "a", "b", "big", "code", "em", "font", "i", "nobr",
        "s", "small", "strike", "strong", "tt", "u"
    };
    return s;
}

static const std::unordered_set<std::string>& special_elements() {
    static const std::unordered_set<std::string> s = {
        "address", "applet", "area", "article", "aside", "base",
        "basefont", "bgsound", "blockquote", "body", "br", "button",
        "caption", "center", "col", "colgroup", "dd", "details",
        "dir", "div", "dl", "dt", "embed", "fieldset", "figcaption",
        "figure", "footer", "form", "frame", "frameset", "h1", "h2",
        "h3", "h4", "h5", "h6", "head", "header", "hgroup", "hr",
        "html", "iframe", "img", "input", "li", "link", "listing",
        "main", "marquee", "menu", "meta", "nav", "noembed",
        "noframes", "noscript", "object", "ol", "p", "param",
        "plaintext", "pre", "script", "section", "select", "source",
        "style", "summary", "table", "tbody", "td", "template",
        "textarea", "tfoot", "th", "thead", "title", "tr", "track",
        "ul", "wbr", "xmp"
    };
    return s;
}

// Elements that implicitly close a <p> when encountered as a start tag
static bool closes_p(const std::string& tag) {
    static const std::unordered_set<std::string> s = {
        "address", "article", "aside", "blockquote", "center", "details",
        "dialog", "dir", "div", "dl", "fieldset", "figcaption", "figure",
        "footer", "header", "hgroup", "hr", "li", "listing", "main",
        "menu", "nav", "ol", "p", "pre", "search", "section", "summary",
        "table", "ul",
        "h1", "h2", "h3", "h4", "h5", "h6"
    };
    return s.count(tag) > 0;
}

// Raw text elements: their content is parsed as raw text
static bool is_raw_text_element(const std::string& tag) {
    return tag == "script" || tag == "style" || tag == "xmp"
        || tag == "iframe" || tag == "noembed" || tag == "noframes";
}

// RCDATA elements: title, textarea
static bool is_rcdata_element(const std::string& tag) {
    return tag == "title" || tag == "textarea";
}

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}

static bool is_all_whitespace(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
        [](char c) { return is_whitespace(c); });
}

namespace {

std::unique_ptr<clever::dom::Node> convert_simple_to_dom_node(
    const SimpleNode& node,
    clever::dom::Document& document) {

    switch (node.type) {
        case SimpleNode::Document:
        case SimpleNode::DocumentType: {
            break;
        }
        case SimpleNode::Element: {
            auto element = document.create_element(node.tag_name);
            clever::dom::Element* element_ptr = element.get();
            for (const auto& attr : node.attributes) {
                element_ptr->set_attribute(attr.name, attr.value);
            }
            for (auto& child : node.children) {
                auto dom_child = convert_simple_to_dom_node(*child, document);
                if (dom_child) {
                    element_ptr->append_child(std::move(dom_child));
                }
            }
            if (!element_ptr->id().empty()) {
                document.register_id(element_ptr->id(), element_ptr);
            }
            return element;
        }
        case SimpleNode::Text:
            return document.create_text_node(node.data);
        case SimpleNode::Comment:
            return document.create_comment(node.data);
        default:
            return nullptr;
    }

    return nullptr;
}

std::unique_ptr<SimpleNode> convert_dom_to_simple_node(const clever::dom::Node& node) {
    using clever::dom::NodeType;

    switch (node.node_type()) {
        case NodeType::Document: {
            auto simple = std::make_unique<SimpleNode>();
            simple->type = SimpleNode::Document;
            node.for_each_child([&](const clever::dom::Node& child) {
                auto child_node = convert_dom_to_simple_node(child);
                if (child_node) {
                    simple->append_child(std::move(child_node));
                }
            });
            return simple;
        }
        case NodeType::Element: {
            const auto& element = static_cast<const clever::dom::Element&>(node);
            auto simple = std::make_unique<SimpleNode>();
            simple->type = SimpleNode::Element;
            simple->tag_name = element.tag_name();
            for (const auto& attr : element.attributes()) {
                simple->attributes.push_back({attr.name, attr.value});
            }
            element.for_each_child([&](const clever::dom::Node& child) {
                auto child_node = convert_dom_to_simple_node(child);
                if (child_node) {
                    simple->append_child(std::move(child_node));
                }
            });
            return simple;
        }
        case NodeType::Text: {
            const auto& text = static_cast<const clever::dom::Text&>(node);
            auto simple = std::make_unique<SimpleNode>();
            simple->type = SimpleNode::Text;
            simple->data = text.data();
            return simple;
        }
        case NodeType::Comment: {
            const auto& comment = static_cast<const clever::dom::Comment&>(node);
            auto simple = std::make_unique<SimpleNode>();
            simple->type = SimpleNode::Comment;
            simple->data = comment.data();
            return simple;
        }
        case NodeType::DocumentFragment: {
            auto simple = std::make_unique<SimpleNode>();
            simple->type = SimpleNode::Document;
            node.for_each_child([&](const clever::dom::Node& child) {
                auto child_node = convert_dom_to_simple_node(child);
                if (child_node) {
                    simple->append_child(std::move(child_node));
                }
            });
            return simple;
        }
        case NodeType::DocumentType:
            return nullptr;
        default:
            return nullptr;
    }
}

} // namespace

// ============================================================================
// TreeBuilder
// ============================================================================

TreeBuilder::TreeBuilder() {
    document_ = std::make_unique<SimpleNode>();
    document_->type = SimpleNode::Document;
}

SimpleNode* TreeBuilder::current_node() {
    if (open_elements_.empty()) return document_.get();
    return open_elements_.back();
}

SimpleNode* TreeBuilder::insert_element(const Token& token) {
    auto node = std::make_unique<SimpleNode>();
    node->type = SimpleNode::Element;
    node->tag_name = token.name;
    node->attributes = token.attributes;
    auto* raw = current_node()->append_child(std::move(node));
    if (!is_void_element(token.name) && !token.self_closing) {
        open_elements_.push_back(raw);
    }
    return raw;
}

SimpleNode* TreeBuilder::insert_element(const std::string& tag) {
    auto node = std::make_unique<SimpleNode>();
    node->type = SimpleNode::Element;
    node->tag_name = tag;
    auto* raw = current_node()->append_child(std::move(node));
    if (!is_void_element(tag)) {
        open_elements_.push_back(raw);
    }
    return raw;
}

void TreeBuilder::insert_text(const std::string& data) {
    auto* cur = current_node();
    // Merge with previous text node if possible
    if (!cur->children.empty() && cur->children.back()->type == SimpleNode::Text) {
        cur->children.back()->data += data;
        return;
    }
    auto node = std::make_unique<SimpleNode>();
    node->type = SimpleNode::Text;
    node->data = data;
    cur->append_child(std::move(node));
}

void TreeBuilder::insert_comment(const std::string& data) {
    auto node = std::make_unique<SimpleNode>();
    node->type = SimpleNode::Comment;
    node->data = data;
    current_node()->append_child(std::move(node));
}

void TreeBuilder::generate_implied_end_tags(const std::string& except) {
    static const std::unordered_set<std::string> implied = {
        "dd", "dt", "li", "optgroup", "option", "p", "rb", "rp", "rt", "rtc"
    };
    while (!open_elements_.empty()) {
        auto& tag = current_node()->tag_name;
        if (tag == except) break;
        if (implied.count(tag) == 0) break;
        open_elements_.pop_back();
    }
}

bool TreeBuilder::has_element_in_scope(const std::string& tag) const {
    static const std::unordered_set<std::string> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template"
    };
    for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
        if ((*it)->tag_name == tag) return true;
        if (scope_markers.count((*it)->tag_name)) return false;
    }
    return false;
}

void TreeBuilder::pop_until(const std::string& tag) {
    while (!open_elements_.empty()) {
        auto* node = open_elements_.back();
        open_elements_.pop_back();
        if (node->tag_name == tag) break;
    }
}

void TreeBuilder::close_element(const std::string& tag) {
    if (has_element_in_scope(tag)) {
        generate_implied_end_tags(tag);
        pop_until(tag);
    }
}

bool TreeBuilder::is_formatting_element(const std::string& tag) const {
    return formatting_elements().count(tag) > 0;
}

bool TreeBuilder::is_special_element(const std::string& tag) const {
    return special_elements().count(tag) > 0;
}

bool TreeBuilder::is_void_element(const std::string& tag) const {
    return void_elements().count(tag) > 0;
}

void TreeBuilder::handle_formatting_element(const Token& token) {
    // Simplified adoption agency algorithm:
    // For closing formatting tags, just close them if they're in scope
    if (token.type == Token::EndTag) {
        close_element(token.name);
    }
}

// ============================================================================
// Token processing dispatch
// ============================================================================

void TreeBuilder::process_token(const Token& token) {
    switch (mode_) {
        case InsertionMode::Initial:         handle_initial(token); break;
        case InsertionMode::BeforeHtml:      handle_before_html(token); break;
        case InsertionMode::BeforeHead:      handle_before_head(token); break;
        case InsertionMode::InHead:          handle_in_head(token); break;
        case InsertionMode::AfterHead:       handle_after_head(token); break;
        case InsertionMode::InBody:          handle_in_body(token); break;
        case InsertionMode::Text:            handle_text(token); break;
        case InsertionMode::InTable:         handle_in_table(token); break;
        case InsertionMode::AfterBody:       handle_after_body(token); break;
        case InsertionMode::AfterAfterBody:  handle_after_after_body(token); break;
        default:
            // For unhandled modes, use InBody as fallback
            handle_in_body(token);
            break;
    }
}

// ============================================================================
// Insertion mode handlers
// ============================================================================

void TreeBuilder::handle_initial(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        return; // Ignore whitespace
    }
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }
    if (token.type == Token::DOCTYPE) {
        auto node = std::make_unique<SimpleNode>();
        node->type = SimpleNode::DocumentType;
        node->doctype_name = token.name;
        document_->append_child(std::move(node));
        mode_ = InsertionMode::BeforeHtml;
        return;
    }
    // Anything else: switch to BeforeHtml and reprocess
    mode_ = InsertionMode::BeforeHtml;
    handle_before_html(token);
}

void TreeBuilder::handle_before_html(const Token& token) {
    if (token.type == Token::DOCTYPE) {
        return; // Ignore
    }
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        return; // Ignore whitespace
    }
    if (token.type == Token::StartTag && token.name == "html") {
        auto* html = insert_element(token);
        (void)html;
        mode_ = InsertionMode::BeforeHead;
        return;
    }
    if (token.type == Token::EndTag) {
        if (token.name != "head" && token.name != "body"
            && token.name != "html" && token.name != "br") {
            return; // Parse error, ignore
        }
    }
    // Anything else: create html element and reprocess
    auto* html = insert_element("html");
    (void)html;
    mode_ = InsertionMode::BeforeHead;
    process_token(token);
}

void TreeBuilder::handle_before_head(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        return;
    }
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }
    if (token.type == Token::DOCTYPE) {
        return;
    }
    if (token.type == Token::StartTag && token.name == "html") {
        // Process as if in InBody
        // For now, ignore duplicate html tags
        return;
    }
    if (token.type == Token::StartTag && token.name == "head") {
        head_ = insert_element(token);
        mode_ = InsertionMode::InHead;
        return;
    }
    if (token.type == Token::EndTag) {
        if (token.name != "head" && token.name != "body"
            && token.name != "html" && token.name != "br") {
            return;
        }
    }
    // Implied head
    head_ = insert_element("head");
    mode_ = InsertionMode::InHead;
    process_token(token);
}

void TreeBuilder::handle_in_head(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        insert_text(token.data);
        return;
    }
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }
    if (token.type == Token::DOCTYPE) {
        return;
    }
    if (token.type == Token::StartTag) {
        if (token.name == "html") {
            return;
        }
        if (token.name == "base" || token.name == "basefont"
            || token.name == "bgsound" || token.name == "link"
            || token.name == "meta") {
            insert_element(token);
            // Void elements -- already not pushed to open_elements
            return;
        }
        if (token.name == "title") {
            insert_element(token);
            // Switch to RCDATA-like text mode
            original_mode_ = mode_;
            mode_ = InsertionMode::Text;
            return;
        }
        if (token.name == "noframes" || token.name == "style") {
            insert_element(token);
            original_mode_ = mode_;
            mode_ = InsertionMode::Text;
            return;
        }
        if (token.name == "script") {
            insert_element(token);
            original_mode_ = mode_;
            mode_ = InsertionMode::Text;
            return;
        }
        if (token.name == "head") {
            return; // Ignore duplicate head
        }
    }
    if (token.type == Token::EndTag && token.name == "head") {
        open_elements_.pop_back(); // Pop head
        mode_ = InsertionMode::AfterHead;
        return;
    }
    if (token.type == Token::EndTag) {
        if (token.name != "body" && token.name != "html" && token.name != "br") {
            return;
        }
    }
    // Implied end of head
    if (!open_elements_.empty() && current_node()->tag_name == "head") {
        open_elements_.pop_back();
    }
    mode_ = InsertionMode::AfterHead;
    process_token(token);
}

void TreeBuilder::handle_after_head(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        insert_text(token.data);
        return;
    }
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }
    if (token.type == Token::DOCTYPE) {
        return;
    }
    if (token.type == Token::StartTag && token.name == "html") {
        return;
    }
    if (token.type == Token::StartTag && token.name == "body") {
        body_ = insert_element(token);
        mode_ = InsertionMode::InBody;
        return;
    }
    if (token.type == Token::StartTag && token.name == "head") {
        return; // Ignore
    }
    if (token.type == Token::EndTag) {
        if (token.name != "body" && token.name != "html" && token.name != "br") {
            return;
        }
    }
    // Implied body
    body_ = insert_element("body");
    mode_ = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::handle_in_body(const Token& token) {
    // Character tokens
    if (token.type == Token::Character) {
        insert_text(token.data);
        return;
    }

    // Comments
    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }

    // DOCTYPE -- ignore
    if (token.type == Token::DOCTYPE) {
        return;
    }

    // EOF
    if (token.type == Token::EndOfFile) {
        return;
    }

    // Start tags
    if (token.type == Token::StartTag) {
        const auto& tag = token.name;

        if (tag == "html") {
            // Merge attributes onto existing html element
            return;
        }

        // Heading elements: close any open heading
        if (tag == "h1" || tag == "h2" || tag == "h3"
            || tag == "h4" || tag == "h5" || tag == "h6") {
            if (has_element_in_scope("p")) {
                close_element("p");
            }
            // If current node is a heading, pop it (implicit close)
            if (!open_elements_.empty()) {
                auto& cur = current_node()->tag_name;
                if (cur == "h1" || cur == "h2" || cur == "h3"
                    || cur == "h4" || cur == "h5" || cur == "h6") {
                    open_elements_.pop_back();
                }
            }
            insert_element(token);
            return;
        }

        // Tags that close an open <p>
        if (closes_p(tag)) {
            if (has_element_in_scope("p")) {
                close_element("p");
            }
        }

        // <li> closes previous <li>
        if (tag == "li") {
            // Walk the stack and close any open <li>
            for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
                if ((*it)->tag_name == "li") {
                    close_element("li");
                    break;
                }
                if (is_special_element((*it)->tag_name)
                    && (*it)->tag_name != "address"
                    && (*it)->tag_name != "div"
                    && (*it)->tag_name != "p") {
                    break;
                }
            }
            if (has_element_in_scope("p")) {
                close_element("p");
            }
            insert_element(token);
            return;
        }

        // <dd>/<dt> close previous <dd>/<dt>
        if (tag == "dd" || tag == "dt") {
            for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
                if ((*it)->tag_name == "dd" || (*it)->tag_name == "dt") {
                    close_element((*it)->tag_name);
                    break;
                }
                if (is_special_element((*it)->tag_name)
                    && (*it)->tag_name != "address"
                    && (*it)->tag_name != "div"
                    && (*it)->tag_name != "p") {
                    break;
                }
            }
            if (has_element_in_scope("p")) {
                close_element("p");
            }
            insert_element(token);
            return;
        }

        // Raw text elements (script, style, etc.)
        if (is_raw_text_element(tag)) {
            insert_element(token);
            original_mode_ = mode_;
            mode_ = InsertionMode::Text;
            return;
        }

        // RCDATA elements (title, textarea)
        if (is_rcdata_element(tag)) {
            insert_element(token);
            original_mode_ = mode_;
            mode_ = InsertionMode::Text;
            return;
        }

        // Void elements
        if (is_void_element(tag)) {
            insert_element(token);
            return;
        }

        // Formatting elements
        if (is_formatting_element(tag)) {
            insert_element(token);
            return;
        }

        // <body>
        if (tag == "body") {
            return;
        }

        // Table
        if (tag == "table") {
            if (has_element_in_scope("p")) {
                close_element("p");
            }
            insert_element(token);
            mode_ = InsertionMode::InTable;
            return;
        }

        // Default: insert the element
        insert_element(token);
        return;
    }

    // End tags
    if (token.type == Token::EndTag) {
        const auto& tag = token.name;

        if (tag == "body") {
            if (!has_element_in_scope("body")) {
                return;
            }
            mode_ = InsertionMode::AfterBody;
            return;
        }

        if (tag == "html") {
            if (!has_element_in_scope("body")) {
                return;
            }
            mode_ = InsertionMode::AfterBody;
            // Reprocess in AfterBody
            handle_after_body(token);
            return;
        }

        // Heading elements
        if (tag == "h1" || tag == "h2" || tag == "h3"
            || tag == "h4" || tag == "h5" || tag == "h6") {
            // Check if any heading is in scope
            bool in_scope = has_element_in_scope("h1")
                || has_element_in_scope("h2")
                || has_element_in_scope("h3")
                || has_element_in_scope("h4")
                || has_element_in_scope("h5")
                || has_element_in_scope("h6");
            if (!in_scope) return;
            generate_implied_end_tags();
            // Pop until we find a heading
            while (!open_elements_.empty()) {
                auto& cur_tag = current_node()->tag_name;
                bool is_heading = cur_tag == "h1" || cur_tag == "h2"
                    || cur_tag == "h3" || cur_tag == "h4"
                    || cur_tag == "h5" || cur_tag == "h6";
                open_elements_.pop_back();
                if (is_heading) break;
            }
            return;
        }

        // Formatting elements
        if (is_formatting_element(tag)) {
            handle_formatting_element(token);
            return;
        }

        // <p> end tag
        if (tag == "p") {
            if (!has_element_in_scope("p")) {
                // Parse error: insert an implicit <p>, then close it
                insert_element("p");
            }
            close_element("p");
            return;
        }

        // <li> end tag
        if (tag == "li") {
            if (!has_element_in_scope("li")) return;
            generate_implied_end_tags("li");
            pop_until("li");
            return;
        }

        // <dd>/<dt> end tags
        if (tag == "dd" || tag == "dt") {
            if (!has_element_in_scope(tag)) return;
            generate_implied_end_tags(tag);
            pop_until(tag);
            return;
        }

        // Any other end tag: walk the stack
        for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
            auto* node = *it;
            if (node->tag_name == tag) {
                generate_implied_end_tags(tag);
                // Pop until this node
                while (!open_elements_.empty() && open_elements_.back() != node) {
                    open_elements_.pop_back();
                }
                if (!open_elements_.empty()) {
                    open_elements_.pop_back();
                }
                return;
            }
            if (is_special_element(node->tag_name)) {
                return; // Parse error, ignore
            }
        }
    }
}

void TreeBuilder::handle_text(const Token& token) {
    if (token.type == Token::Character) {
        insert_text(token.data);
        return;
    }
    if (token.type == Token::EndOfFile) {
        // Parse error
        open_elements_.pop_back();
        mode_ = original_mode_;
        process_token(token);
        return;
    }
    if (token.type == Token::EndTag) {
        open_elements_.pop_back();
        mode_ = original_mode_;
        return;
    }
}

void TreeBuilder::handle_in_table(const Token& token) {
    // Simplified table handling: foster-parent everything into body
    // In a real implementation, this would handle tbody, tr, td, etc.
    if (token.type == Token::EndTag && token.name == "table") {
        pop_until("table");
        mode_ = InsertionMode::InBody;
        return;
    }
    // Fallback to InBody for everything else
    handle_in_body(token);
}

void TreeBuilder::handle_after_body(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        handle_in_body(token);
        return;
    }
    if (token.type == Token::Comment) {
        // Append to html element
        auto* html_el = document_->find_element("html");
        if (html_el) {
            auto node = std::make_unique<SimpleNode>();
            node->type = SimpleNode::Comment;
            node->data = token.data;
            html_el->append_child(std::move(node));
        }
        return;
    }
    if (token.type == Token::DOCTYPE) {
        return;
    }
    if (token.type == Token::StartTag && token.name == "html") {
        return;
    }
    if (token.type == Token::EndTag && token.name == "html") {
        mode_ = InsertionMode::AfterAfterBody;
        return;
    }
    if (token.type == Token::EndOfFile) {
        return;
    }
    // Parse error: switch back to InBody
    mode_ = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::handle_after_after_body(const Token& token) {
    if (token.type == Token::Comment) {
        auto node = std::make_unique<SimpleNode>();
        node->type = SimpleNode::Comment;
        node->data = token.data;
        document_->append_child(std::move(node));
        return;
    }
    if (token.type == Token::DOCTYPE) {
        return;
    }
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        handle_in_body(token);
        return;
    }
    if (token.type == Token::StartTag && token.name == "html") {
        return;
    }
    if (token.type == Token::EndOfFile) {
        return;
    }
    // Parse error
    mode_ = InsertionMode::InBody;
    process_token(token);
}

// ============================================================================
// Convenience parse function
// ============================================================================

std::unique_ptr<SimpleNode> parse(std::string_view html) {
    Tokenizer tokenizer(html);
    TreeBuilder builder;

    while (true) {
        Token token = tokenizer.next_token();

        // Handle raw text / RCDATA state switching for script, style, title, textarea
        if (token.type == Token::StartTag) {
            if (is_raw_text_element(token.name)) {
                tokenizer.set_last_start_tag(token.name);
                if (token.name == "script") {
                    builder.process_token(token);
                    // Switch tokenizer to ScriptData
                    tokenizer.set_state(TokenizerState::ScriptData);
                    continue;
                } else {
                    builder.process_token(token);
                    tokenizer.set_state(TokenizerState::RAWTEXT);
                    continue;
                }
            }
            if (is_rcdata_element(token.name)) {
                tokenizer.set_last_start_tag(token.name);
                builder.process_token(token);
                tokenizer.set_state(TokenizerState::RCDATA);
                continue;
            }
        }

        builder.process_token(token);

        if (token.type == Token::EndOfFile) break;
    }

    return builder.take_document();
}

std::unique_ptr<clever::dom::Document> to_dom_document(const SimpleNode& root) {
    auto document = std::make_unique<clever::dom::Document>();
    for (const auto& child : root.children) {
        auto dom_child = convert_simple_to_dom_node(*child, *document);
        if (dom_child) {
            document->append_child(std::move(dom_child));
        }
    }
    return document;
}

std::unique_ptr<SimpleNode> to_simple_node(const clever::dom::Node& root) {
    return convert_dom_to_simple_node(root);
}

} // namespace clever::html
