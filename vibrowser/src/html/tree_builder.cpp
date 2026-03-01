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

static bool is_phrasing_element_tag(const std::string& tag) {
    static const std::unordered_set<std::string> s = {
        "a", "abbr", "area", "audio", "b", "bdi", "bdo", "br", "button",
        "canvas", "cite", "code", "data", "datalist", "del", "dfn", "em",
        "embed", "font", "i", "iframe", "img", "input", "ins", "kbd",
        "label", "link", "map", "mark", "meta", "meter", "nobr", "noscript",
        "object", "output", "picture", "progress", "q", "ruby", "s", "samp",
        "script", "select", "slot", "small", "span", "strike", "strong",
        "sub", "sup", "textarea", "time", "tt", "u", "var", "video", "wbr"
    };
    return s.count(tag) > 0;
}

// Elements that implicitly close a <p> when encountered as a start tag
static bool closes_p(const std::string& tag) {
    static const std::unordered_set<std::string> s = {
        "address", "article", "aside", "blockquote", "center", "details",
        "dialog", "dir", "div", "dl", "fieldset", "figcaption", "figure",
        "footer", "header", "hgroup", "hr", "li", "listing", "main",
        "menu", "nav", "ol", "p", "pre", "search", "section", "summary",
        "form",
        "table", "ul",
        "h1", "h2", "h3", "h4", "h5", "h6"
    };
    return s.count(tag) > 0;
}

// Raw text elements: their content is parsed as raw text
static bool is_raw_text_element(const std::string& tag) {
    return tag == "script" || tag == "style" || tag == "xmp"
        || tag == "iframe" || tag == "noembed" || tag == "noframes" || tag == "noscript";
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

bool TreeBuilder::has_element_in_button_scope(const std::string& tag) const {
    // Button scope = general scope + "button" as a scope barrier
    static const std::unordered_set<std::string> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template",
        "button"
    };
    for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
        if ((*it)->tag_name == tag) return true;
        if (scope_markers.count((*it)->tag_name)) return false;
    }
    return false;
}

bool TreeBuilder::has_element_in_list_item_scope(const std::string& tag) const {
    // List item scope = general scope + "ol", "ul" as scope barriers
    static const std::unordered_set<std::string> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template",
        "ol", "ul"
    };
    for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
        if ((*it)->tag_name == tag) return true;
        if (scope_markers.count((*it)->tag_name)) return false;
    }
    return false;
}

bool TreeBuilder::has_element_in_select_scope(const std::string& tag) const {
    // Select scope: everything except "optgroup" and "option" is a barrier
    for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
        if ((*it)->tag_name == tag) return true;
        if ((*it)->tag_name != "optgroup" && (*it)->tag_name != "option") return false;
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

void TreeBuilder::push_active_formatting_element(SimpleNode* node) {
    // Noah's Ark: remove earlier entries with the same tag name if there are
    // already 3 matching elements (simplified -- real spec checks attributes too)
    int count = 0;
    for (auto it = active_formatting_elements_.rbegin();
         it != active_formatting_elements_.rend(); ++it) {
        if (*it == nullptr) break; // scope marker
        if ((*it)->tag_name == node->tag_name) {
            ++count;
            if (count >= 3) {
                // Remove the earliest one
                auto fwd = (it + 1).base(); // convert to forward iterator
                active_formatting_elements_.erase(fwd);
                break;
            }
        }
    }
    active_formatting_elements_.push_back(node);
}

void TreeBuilder::remove_active_formatting_element(SimpleNode* node) {
    for (auto it = active_formatting_elements_.begin();
         it != active_formatting_elements_.end(); ++it) {
        if (*it == node) {
            active_formatting_elements_.erase(it);
            return;
        }
    }
}

SimpleNode* TreeBuilder::find_active_formatting_element(const std::string& tag) const {
    // Walk backwards through the active formatting elements list.
    // Stop if we hit a scope marker (nullptr).
    for (auto it = active_formatting_elements_.rbegin();
         it != active_formatting_elements_.rend(); ++it) {
        if (*it == nullptr) return nullptr; // scope marker
        if ((*it)->tag_name == tag) return *it;
    }
    return nullptr;
}

void TreeBuilder::reconstruct_active_formatting_elements() {
    if (active_formatting_elements_.empty()) return;

    // If the last entry is a scope marker or is in the open elements stack,
    // there's nothing to reconstruct.
    auto* last = active_formatting_elements_.back();
    if (last == nullptr) return;

    // Check if the last entry is already in the stack of open elements
    for (auto* el : open_elements_) {
        if (el == last) return;
    }

    // Walk backwards to find entries that need reconstruction
    int idx = static_cast<int>(active_formatting_elements_.size()) - 1;
    while (idx >= 0) {
        auto* entry = active_formatting_elements_[idx];
        if (entry == nullptr) {
            ++idx;
            break;
        }
        // Check if it's in the stack of open elements
        bool in_stack = false;
        for (auto* el : open_elements_) {
            if (el == entry) { in_stack = true; break; }
        }
        if (in_stack) {
            ++idx;
            break;
        }
        if (idx == 0) break;
        --idx;
    }

    // Now reconstruct from idx forward
    for (int i = idx; i < static_cast<int>(active_formatting_elements_.size()); ++i) {
        auto* entry = active_formatting_elements_[i];
        if (entry == nullptr) continue;

        // Create a new element with the same tag and attributes
        auto node = std::make_unique<SimpleNode>();
        node->type = SimpleNode::Element;
        node->tag_name = entry->tag_name;
        node->attributes = entry->attributes;
        auto* raw = current_node()->append_child(std::move(node));
        open_elements_.push_back(raw);

        // Replace the active formatting element entry with the new element
        active_formatting_elements_[i] = raw;
    }
}

void TreeBuilder::handle_formatting_element(const Token& token) {
    if (token.type == Token::EndTag) {
        run_adoption_agency(token.name);
    }
}

void TreeBuilder::run_adoption_agency(const std::string& tag) {
    // Step 1: Let formatting_element be the last element in the active
    // formatting elements list that has the same tag name.
    SimpleNode* formatting_element = find_active_formatting_element(tag);

    // Step 2: If there's no such element, this is like "any other end tag"
    if (!formatting_element) {
        // Fall back to the normal end-tag processing
        close_element(tag);
        return;
    }

    // Step 3: If the formatting element is not in the stack of open elements,
    // remove it from the active list and return.
    int fe_stack_idx = -1;
    for (int i = 0; i < static_cast<int>(open_elements_.size()); ++i) {
        if (open_elements_[i] == formatting_element) {
            fe_stack_idx = i;
            break;
        }
    }
    if (fe_stack_idx < 0) {
        remove_active_formatting_element(formatting_element);
        return;
    }

    // Step 4: If the formatting element is not in scope, ignore and return.
    if (!has_element_in_scope(tag)) {
        return;
    }

    // Step 5: Find the furthest block -- the topmost element in the stack
    // after the formatting element that is not phrasing content.
    SimpleNode* furthest_block = nullptr;
    int fb_stack_idx = -1;
    for (int i = static_cast<int>(open_elements_.size()) - 1; i > fe_stack_idx; --i) {
        if (!is_phrasing_element_tag(open_elements_[i]->tag_name)) {
            furthest_block = open_elements_[i];
            fb_stack_idx = i;
            break;
        }
    }

    // Step 6: If there's no furthest block, pop elements from the stack until
    // we've popped the formatting element, and remove it from the active list.
    if (!furthest_block) {
        while (!open_elements_.empty()) {
            auto* popped = open_elements_.back();
            open_elements_.pop_back();
            if (popped == formatting_element) break;
        }
        remove_active_formatting_element(formatting_element);
        return;
    }

    // Step 7: The adoption agency loop.
    // common_ancestor is the element immediately before formatting_element in the stack.
    SimpleNode* common_ancestor = (fe_stack_idx > 0)
        ? open_elements_[fe_stack_idx - 1]
        : document_.get();

    // Outer loop: max 8 iterations
    for (int outer = 0; outer < 8; ++outer) {
        // Refresh fe_stack_idx (it may have shifted)
        fe_stack_idx = -1;
        for (int i = 0; i < static_cast<int>(open_elements_.size()); ++i) {
            if (open_elements_[i] == formatting_element) {
                fe_stack_idx = i;
                break;
            }
        }
        if (fe_stack_idx < 0) break;

        // Refresh fb_stack_idx
        fb_stack_idx = -1;
        for (int i = 0; i < static_cast<int>(open_elements_.size()); ++i) {
            if (open_elements_[i] == furthest_block) {
                fb_stack_idx = i;
                break;
            }
        }
        if (fb_stack_idx < 0) break;

        // Inner loop: walk backwards from furthest_block toward formatting_element.
        // node starts as the element before furthest_block in the stack.
        SimpleNode* node = nullptr;
        int node_stack_idx = fb_stack_idx - 1;
        SimpleNode* last_node = furthest_block;

        for (int inner = 0; inner < 3; ++inner) {
            // Step 7.6: node = the element before node in the stack
            if (node_stack_idx <= fe_stack_idx) break;
            node = open_elements_[node_stack_idx];

            // Step 7.7: If node is not in the active formatting elements list,
            // remove it from the stack and continue.
            bool in_active = false;
            for (auto* afe : active_formatting_elements_) {
                if (afe == node) { in_active = true; break; }
            }
            if (!in_active) {
                // Remove node from stack
                open_elements_.erase(open_elements_.begin() + node_stack_idx);
                // Refresh fb_stack_idx
                for (int i = 0; i < static_cast<int>(open_elements_.size()); ++i) {
                    if (open_elements_[i] == furthest_block) { fb_stack_idx = i; break; }
                }
                node_stack_idx--;
                continue;
            }

            // Step 7.8: If node is the formatting element, break inner loop.
            if (node == formatting_element) break;

            // Step 7.9: Create a replacement element for node (clone).
            auto replacement = std::make_unique<SimpleNode>();
            replacement->type = SimpleNode::Element;
            replacement->tag_name = node->tag_name;
            replacement->attributes = node->attributes;
            SimpleNode* replacement_raw = replacement.get();

            // Move all children of node into the replacement
            while (!node->children.empty()) {
                auto child = std::move(node->children.front());
                node->children.erase(node->children.begin());
                child->parent = replacement_raw;
                replacement_raw->children.push_back(std::move(child));
            }

            // Replace node in the tree: put replacement as child of node's parent
            if (node->parent) {
                // Find where node is among its parent's children and replace
                auto& siblings = node->parent->children;
                for (size_t s = 0; s < siblings.size(); ++s) {
                    if (siblings[s].get() == node) {
                        replacement->parent = node->parent;
                        siblings[s] = std::move(replacement);
                        break;
                    }
                }
            }

            // Update the stack and active formatting list
            open_elements_[node_stack_idx] = replacement_raw;
            for (auto& afe : active_formatting_elements_) {
                if (afe == node) { afe = replacement_raw; break; }
            }
            node = replacement_raw;

            // Step 7.10: If last_node is furthest_block, update bookmark
            // (simplified: we just track the replacement)

            // Step 7.11: Detach last_node from its parent, append to node
            if (last_node->parent) {
                auto taken = last_node->parent->take_child(last_node);
                if (taken) {
                    node->append_child(std::move(taken));
                }
            }

            last_node = node;
            node_stack_idx--;
        }

        // Step 7.12: Detach last_node from its parent, append to common_ancestor
        if (last_node->parent) {
            auto taken = last_node->parent->take_child(last_node);
            if (taken) {
                common_ancestor->append_child(std::move(taken));
            }
        }

        // Step 7.13: Create a new element based on the formatting element
        auto new_element = std::make_unique<SimpleNode>();
        new_element->type = SimpleNode::Element;
        new_element->tag_name = formatting_element->tag_name;
        new_element->attributes = formatting_element->attributes;
        SimpleNode* new_element_raw = new_element.get();

        // Step 7.14: Move all children of furthest_block into the new element
        while (!furthest_block->children.empty()) {
            auto child = std::move(furthest_block->children.front());
            furthest_block->children.erase(furthest_block->children.begin());
            child->parent = new_element_raw;
            new_element_raw->children.push_back(std::move(child));
        }

        // Step 7.15: Append the new element to furthest_block
        furthest_block->append_child(std::move(new_element));

        // Step 7.16: Remove the formatting element from the active list
        // and insert the new element at the position after where furthest_block
        // is in the active list (or at the end if fb isn't in the active list).
        remove_active_formatting_element(formatting_element);
        // Find furthest_block in active list
        {
            auto it = std::find(active_formatting_elements_.begin(),
                                active_formatting_elements_.end(), furthest_block);
            if (it != active_formatting_elements_.end()) {
                active_formatting_elements_.insert(it + 1, new_element_raw);
            } else {
                active_formatting_elements_.push_back(new_element_raw);
            }
        }

        // Step 7.17: Remove the formatting element from the stack
        // and insert the new element immediately below furthest_block.
        // open_elements_ is ordered from root -> current node, so "below"
        // means before furthest_block in this vector.
        {
            auto it = std::find(open_elements_.begin(), open_elements_.end(),
                                formatting_element);
            if (it != open_elements_.end()) {
                open_elements_.erase(it);
            }
        }
        {
            auto it = std::find(open_elements_.begin(), open_elements_.end(),
                                furthest_block);
            if (it != open_elements_.end()) {
                open_elements_.insert(it, new_element_raw);
            }
        }

        // The algorithm repeats (outer loop) but typically only runs once
        // for most misnested cases. We break here since the formatting
        // element has been processed.
        break;
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
        case InsertionMode::InTableBody:     handle_in_table_body(token); break;
        case InsertionMode::InRow:           handle_in_row(token); break;
        case InsertionMode::InCell:          handle_in_cell(token); break;
        case InsertionMode::InTemplate:      handle_in_template(token); break;
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
        // Implicit head close: non-head elements should close head
        const std::string& tag = token.name;
        if (tag != "html" && tag != "head" && tag != "base" && tag != "basefont" &&
            tag != "bgsound" && tag != "link" && tag != "meta" && tag != "title" &&
            tag != "noframes" && tag != "style" && tag != "script" && tag != "noscript") {
            // Implicit head close
            if (!open_elements_.empty() && current_node()->tag_name == "head") {
                open_elements_.pop_back();
                mode_ = InsertionMode::AfterHead;
                process_token(token);
                return;
            }
        }
        if (token.name == "html") {
            return;
        }
        if (token.name == "base" || token.name == "basefont"
            || token.name == "bgsound" || token.name == "link"
            || token.name == "meta") {
            insert_element(token);
            // Extract href from base element for base URL tracking
            if (token.name == "base") {
                for (const auto& attr : token.attributes) {
                    if (attr.name == "href") {
                        base_url_ = attr.value;
                        break;
                    }
                }
            }
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
        if (token.name == "template") {
            insert_element(token);
            // Create template_content as DocumentFragment
            SimpleNode* template_elem = current_node();
            if (template_elem) {
                template_elem->template_content = std::make_unique<SimpleNode>();
                template_elem->template_content->type = SimpleNode::DocumentFragment;
                // Push the template_content fragment onto open_elements to capture content
                open_elements_.push_back(template_elem->template_content.get());
                // Save current mode and switch to InTemplate
                original_mode_ = mode_;
                mode_ = InsertionMode::InTemplate;
            }
            return;
        }
        if (token.name == "noscript") {
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
        handle_in_body(token);
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
    if (token.type == Token::StartTag &&
        (token.name == "base" || token.name == "basefont" ||
         token.name == "bgsound" || token.name == "link" ||
         token.name == "meta" || token.name == "noframes" ||
         token.name == "script" || token.name == "style" ||
         token.name == "title")) {
        if (head_ != nullptr) {
            open_elements_.push_back(head_);
            handle_in_head(token);
            if (!open_elements_.empty() && open_elements_.back() == head_) {
                open_elements_.pop_back();
            }
        }
        return;
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
        reconstruct_active_formatting_elements();
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
            auto* html = document_->find_element("html");
            if (!html) return;
            for (const auto& incoming : token.attributes) {
                bool exists = false;
                for (const auto& existing : html->attributes) {
                    if (existing.name == incoming.name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    html->attributes.push_back(incoming);
                }
            }
            return;
        }

        auto apply_implicit_start_tag_closures = [&](const std::string& incoming_tag) {
            // Elements that implicitly end an open <p>.
            if (closes_p(incoming_tag) && has_element_in_button_scope("p")) {
                close_element("p");
            }

            // A new <li> implicitly closes a previous <li>.
            if (incoming_tag == "li") {
                for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
                    if ((*it)->tag_name == "li") {
                        generate_implied_end_tags("li");
                        pop_until("li");
                        break;
                    }
                    if (is_special_element((*it)->tag_name)
                        && (*it)->tag_name != "address"
                        && (*it)->tag_name != "div"
                        && (*it)->tag_name != "p") {
                        break;
                    }
                }

                if (has_element_in_button_scope("p")) {
                    close_element("p");
                }
            }

            // A new <dd> or <dt> implicitly closes an open <dd>/<dt>.
            if (incoming_tag == "dd" || incoming_tag == "dt") {
                for (auto it = open_elements_.rbegin(); it != open_elements_.rend(); ++it) {
                    const auto& node_tag = (*it)->tag_name;
                    if (node_tag == "dd" || node_tag == "dt") {
                        generate_implied_end_tags(node_tag);
                        pop_until(node_tag);
                        break;
                    }
                    if (is_special_element(node_tag)
                        && node_tag != "address"
                        && node_tag != "div"
                        && node_tag != "p") {
                        break;
                    }
                }

                if (has_element_in_button_scope("p")) {
                    close_element("p");
                }
            }

            // A new <option> or <optgroup> implicitly closes an open <option>.
            if (incoming_tag == "option" || incoming_tag == "optgroup") {
                if (!open_elements_.empty() && current_node()->tag_name == "option") {
                    open_elements_.pop_back();
                }
            }

            // A new <optgroup> implicitly closes an open <optgroup>.
            if (incoming_tag == "optgroup") {
                if (!open_elements_.empty() && current_node()->tag_name == "optgroup") {
                    open_elements_.pop_back();
                }
            }
        };

        apply_implicit_start_tag_closures(tag);

        // Heading elements: close any open heading
        if (tag == "h1" || tag == "h2" || tag == "h3"
            || tag == "h4" || tag == "h5" || tag == "h6") {
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

        // <li> closes previous <li> (per spec: use list item scope)
        if (tag == "li") {
            insert_element(token);
            return;
        }

        // <dd>/<dt> close previous <dd>/<dt>
        if (tag == "dd" || tag == "dt") {
            insert_element(token);
            return;
        }

        // <option> is inserted after implicit-closure handling.
        if (tag == "option") {
            insert_element(token);
            return;
        }

        // <optgroup> is inserted after implicit-closure handling.
        if (tag == "optgroup") {
            insert_element(token);
            return;
        }

        // <select>
        if (tag == "select") {
            reconstruct_active_formatting_elements();
            insert_element(token);
            // Note: in a full implementation, this would switch to InSelect mode
            return;
        }

        // <form> insertion (implicit <p> closure handled above).
        if (tag == "form") {
            insert_element(token);
            return;
        }

        // <button> closes <button> in scope, then closes <p> in button scope
        if (tag == "button") {
            if (has_element_in_scope("button")) {
                generate_implied_end_tags();
                pop_until("button");
            }
            reconstruct_active_formatting_elements();
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
            reconstruct_active_formatting_elements();
            auto* el = insert_element(token);
            push_active_formatting_element(el);
            return;
        }

        // <body>
        if (tag == "body") {
            if (body_ == nullptr) {
                body_ = document_->find_element("body");
            }
            if (!body_) return;
            for (const auto& incoming : token.attributes) {
                bool exists = false;
                for (const auto& existing : body_->attributes) {
                    if (existing.name == incoming.name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) body_->attributes.push_back(incoming);
            }
            return;
        }

        if (tag == "head") {
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
        reconstruct_active_formatting_elements();
        insert_element(token);
        return;
    }

    // End tags
    if (token.type == Token::EndTag) {
        const auto& tag = token.name;

        if (tag == "caption") {
            if (!has_element_in_scope("caption")) return;
            close_element("caption");
            if (has_element_in_scope("table")) {
                mode_ = InsertionMode::InTable;
            }
            return;
        }

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
        if (!open_elements_.empty() && current_node()->tag_name == token.name) {
            open_elements_.pop_back();
        }
        mode_ = original_mode_;
        return;
    }
}

void TreeBuilder::handle_in_table(const Token& token) {
    // Handle colgroup-specific sub-mode while we stay in InTable.
    if (!open_elements_.empty() && current_node()->tag_name == "colgroup") {
        if (token.type == Token::Character && is_all_whitespace(token.data)) {
            insert_text(token.data);
            return;
        }
        if (token.type == Token::StartTag && token.name == "col") {
            insert_element(token);
            return;
        }
        if (token.type == Token::EndTag && token.name == "colgroup") {
            open_elements_.pop_back();
            return;
        }
        open_elements_.pop_back();
        process_token(token);
        return;
    }

    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        insert_text(token.data);
        return;
    }

    if (token.type == Token::Comment) {
        insert_comment(token.data);
        return;
    }

    if (token.type == Token::StartTag) {
        if (token.name == "caption") {
            insert_element(token);
            mode_ = InsertionMode::InBody;
            return;
        }

        if (token.name == "colgroup") {
            insert_element(token);
            return;
        }

        if (token.name == "col") {
            insert_element("colgroup");
            handle_in_table(token);
            return;
        }

        if (token.name == "tbody" || token.name == "thead" || token.name == "tfoot") {
            insert_element(token);
            mode_ = InsertionMode::InTableBody;
            return;
        }

        if (token.name == "tr") {
            insert_element("tbody");
            mode_ = InsertionMode::InTableBody;
            handle_in_table_body(token);
            return;
        }

        if (token.name == "td" || token.name == "th") {
            insert_element("tbody");
            mode_ = InsertionMode::InTableBody;
            handle_in_table_body(token);
            return;
        }
    }

    if (token.type == Token::EndTag && token.name == "table") {
        pop_until("table");
        mode_ = InsertionMode::InBody;
        return;
    }

    if (token.type == Token::EndTag && token.name == "caption") {
        if (!has_element_in_scope("caption")) return;
        pop_until("caption");
        return;
    }

    if (token.type == Token::EndTag &&
        (token.name == "body" || token.name == "caption" || token.name == "col" ||
         token.name == "colgroup" || token.name == "html" || token.name == "tbody" ||
         token.name == "td" || token.name == "tfoot" || token.name == "th" ||
         token.name == "thead" || token.name == "tr")) {
        return;
    }

    // Fallback to InBody for table-misnested content.
    handle_in_body(token);
}

void TreeBuilder::handle_in_table_body(const Token& token) {
    auto has_table_section_open = [&]() {
        return has_element_in_scope("tbody") || has_element_in_scope("thead") || has_element_in_scope("tfoot");
    };
    auto pop_current_table_section = [&]() {
        if (has_element_in_scope("tbody")) {
            pop_until("tbody");
            return;
        }
        if (has_element_in_scope("thead")) {
            pop_until("thead");
            return;
        }
        if (has_element_in_scope("tfoot")) {
            pop_until("tfoot");
        }
    };

    if (token.type == Token::StartTag) {
        if (token.name == "tr") {
            insert_element(token);
            mode_ = InsertionMode::InRow;
            return;
        }

        if (token.name == "td" || token.name == "th") {
            insert_element("tr");
            mode_ = InsertionMode::InRow;
            handle_in_row(token);
            return;
        }

        if (token.name == "tbody" || token.name == "thead" || token.name == "tfoot") {
            if (!has_table_section_open()) return;
            pop_current_table_section();
            mode_ = InsertionMode::InTable;
            process_token(token);
            return;
        }
    }

    if (token.type == Token::EndTag) {
        if (token.name == "tbody" || token.name == "thead" || token.name == "tfoot") {
            if (!has_element_in_scope(token.name)) return;
            pop_until(token.name);
            mode_ = InsertionMode::InTable;
            return;
        }

        if (token.name == "table") {
            if (!has_table_section_open()) return;
            pop_current_table_section();
            mode_ = InsertionMode::InTable;
            process_token(token);
            return;
        }

        if (token.name == "body" || token.name == "caption" || token.name == "col" ||
            token.name == "colgroup" || token.name == "html" || token.name == "td" ||
            token.name == "th" || token.name == "tr") {
            return;
        }
    }

    if (token.type == Token::StartTag &&
        (token.name == "caption" || token.name == "col" || token.name == "colgroup")) {
        if (!has_table_section_open()) return;
        pop_current_table_section();
        mode_ = InsertionMode::InTable;
        process_token(token);
        return;
    }

    handle_in_table(token);
}

void TreeBuilder::handle_in_row(const Token& token) {
    if (token.type == Token::Character && is_all_whitespace(token.data)) {
        return;
    }

    if (token.type == Token::StartTag) {
        if (token.name == "td" || token.name == "th") {
            insert_element(token);
            mode_ = InsertionMode::InCell;
            return;
        }

        if (token.name == "tr") {
            if (!has_element_in_scope("tr")) return;
            pop_until("tr");
            mode_ = InsertionMode::InTableBody;
            process_token(token);
            return;
        }

        if (token.name == "caption" || token.name == "col" || token.name == "colgroup" ||
            token.name == "tbody" || token.name == "tfoot" || token.name == "thead") {
            if (!has_element_in_scope("tr")) return;
            pop_until("tr");
            mode_ = InsertionMode::InTableBody;
            process_token(token);
            return;
        }
    }

    if (token.type == Token::EndTag) {
        if (token.name == "tr") {
            if (!has_element_in_scope("tr")) return;
            pop_until("tr");
            mode_ = InsertionMode::InTableBody;
            return;
        }

        if (token.name == "table") {
            if (!has_element_in_scope("tr")) return;
            pop_until("tr");
            mode_ = InsertionMode::InTableBody;
            process_token(token);
            return;
        }

        if (token.name == "tbody" || token.name == "tfoot" || token.name == "thead") {
            if (!has_element_in_scope(token.name) || !has_element_in_scope("tr")) return;
            pop_until("tr");
            mode_ = InsertionMode::InTableBody;
            process_token(token);
            return;
        }

        if (token.name == "body" || token.name == "caption" || token.name == "col" ||
            token.name == "colgroup" || token.name == "html" || token.name == "td" ||
            token.name == "th") {
            return;
        }
    }

    handle_in_table(token);
}

void TreeBuilder::handle_in_cell(const Token& token) {
    auto close_current_cell = [&]() {
        if (has_element_in_scope("td")) {
            close_element("td");
        } else if (has_element_in_scope("th")) {
            close_element("th");
        }
        mode_ = InsertionMode::InRow;
    };

    if (token.type == Token::EndTag && (token.name == "td" || token.name == "th")) {
        if (!has_element_in_scope(token.name)) return;
        generate_implied_end_tags(token.name);
        pop_until(token.name);
        mode_ = InsertionMode::InRow;
        return;
    }

    if (token.type == Token::StartTag &&
        (token.name == "caption" || token.name == "col" || token.name == "colgroup" ||
         token.name == "tbody" || token.name == "td" || token.name == "tfoot" ||
         token.name == "th" || token.name == "thead" || token.name == "tr")) {
        if (!has_element_in_scope("td") && !has_element_in_scope("th")) return;
        close_current_cell();
        process_token(token);
        return;
    }

    if (token.type == Token::EndTag &&
        (token.name == "table" || token.name == "tbody" || token.name == "tfoot" ||
         token.name == "thead" || token.name == "tr")) {
        if (!has_element_in_scope("td") && !has_element_in_scope("th")) return;
        close_current_cell();
        process_token(token);
        return;
    }

    if (token.type == Token::EndTag &&
        (token.name == "body" || token.name == "caption" || token.name == "col" ||
         token.name == "colgroup" || token.name == "html")) {
        return;
    }

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
        mode_ = InsertionMode::InBody;
        handle_in_body(token);
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
        mode_ = InsertionMode::InBody;
        handle_in_body(token);
        return;
    }
    if (token.type == Token::EndOfFile) {
        return;
    }
    // Parse error
    mode_ = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::handle_in_template(const Token& token) {
    // Handle tokens while inside a <template> element.
    // For now, we treat templates the same as InBody mode - children are parsed
    // normally into the template element's children[] array.
    // The template_content DocumentFragment is created but children parse normally;
    // the render pipeline skips templates automatically.
    handle_in_body(token);
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
