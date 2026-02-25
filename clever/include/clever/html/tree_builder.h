#pragma once
#include <clever/html/tokenizer.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace clever::html {

// Simplified DOM interface for tree building.
// The real DOM module provides the full implementation.
struct SimpleNode {
    enum Type { Element, Text, Comment, Document, DocumentType };
    Type type;
    std::string tag_name;
    std::string data;  // for text/comment
    std::string doctype_name;
    std::vector<Attribute> attributes;
    SimpleNode* parent = nullptr;
    std::vector<std::unique_ptr<SimpleNode>> children;

    SimpleNode* append_child(std::unique_ptr<SimpleNode> child);
    SimpleNode* insert_before(std::unique_ptr<SimpleNode> child, SimpleNode* ref);
    void remove_child(SimpleNode* child);

    // Get text content recursively
    std::string text_content() const;

    // Find element by tag
    SimpleNode* find_element(const std::string& tag) const;
    std::vector<SimpleNode*> find_all_elements(const std::string& tag) const;
};

enum class InsertionMode {
    Initial,
    BeforeHtml,
    BeforeHead,
    InHead,
    AfterHead,
    InBody,
    Text,
    InTable,
    InTableBody,
    InRow,
    InCell,
    AfterBody,
    AfterAfterBody
};

class TreeBuilder {
public:
    TreeBuilder();

    // Process a token
    void process_token(const Token& token);

    // Get the built document
    SimpleNode* document() const { return document_.get(); }
    std::unique_ptr<SimpleNode> take_document() { return std::move(document_); }

    InsertionMode mode() const { return mode_; }

private:
    std::unique_ptr<SimpleNode> document_;
    SimpleNode* head_ = nullptr;
    SimpleNode* body_ = nullptr;
    std::vector<SimpleNode*> open_elements_;
    InsertionMode mode_ = InsertionMode::Initial;
    InsertionMode original_mode_ = InsertionMode::Initial;
    // Tokenizer* tokenizer_ = nullptr;  // Reserved for future spec-compliant re-entry

    // Insertion mode handlers
    void handle_initial(const Token& token);
    void handle_before_html(const Token& token);
    void handle_before_head(const Token& token);
    void handle_in_head(const Token& token);
    void handle_after_head(const Token& token);
    void handle_in_body(const Token& token);
    void handle_text(const Token& token);
    void handle_in_table(const Token& token);
    void handle_after_body(const Token& token);
    void handle_after_after_body(const Token& token);

    // Helpers
    SimpleNode* current_node();
    SimpleNode* insert_element(const Token& token);
    SimpleNode* insert_element(const std::string& tag);
    void insert_text(const std::string& data);
    void insert_comment(const std::string& data);
    void generate_implied_end_tags(const std::string& except = "");
    bool has_element_in_scope(const std::string& tag) const;
    void pop_until(const std::string& tag);
    void close_element(const std::string& tag);

    // Formatting elements (simplified adoption agency)
    void handle_formatting_element(const Token& token);
    bool is_formatting_element(const std::string& tag) const;
    bool is_special_element(const std::string& tag) const;
    bool is_void_element(const std::string& tag) const;
};

// Convenience: parse HTML string to document
std::unique_ptr<SimpleNode> parse(std::string_view html);

} // namespace clever::html

// Forward-declare DOM types in the correct namespace
namespace clever::dom {
class Node;
class Document;
} // namespace clever::dom

namespace clever::html {

// Convert a SimpleNode tree into the internal DOM representation.
std::unique_ptr<clever::dom::Document> to_dom_document(const SimpleNode& root);

// Convert a DOM tree into a SimpleNode tree.
std::unique_ptr<SimpleNode> to_simple_node(const clever::dom::Node& root);

} // namespace clever::html
