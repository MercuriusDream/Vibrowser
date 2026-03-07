#pragma once
#include <clever/dom/element.h>
#include <clever/dom/text.h>
#include <clever/dom/comment.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace clever::dom {

class DocumentVisibilityLifecycle {
public:
    DocumentVisibilityLifecycle();

    const std::string& visibility_state() const;
    bool hidden() const;

    void synchronize(std::string_view visibility_state, bool hidden);
    void arm_dispatch();
    bool update_and_should_dispatch(std::string_view visibility_state, bool hidden);

private:
    std::string visibility_state_;
    bool hidden_ = false;
    bool dispatch_armed_ = false;
};

class Document : public Node {
public:
    Document();

    // Element accessors
    Element* document_element() const;  // <html>
    Element* body() const;
    Element* head() const;

    // Factory methods
    std::unique_ptr<Element> create_element(const std::string& tag);
    std::unique_ptr<Text> create_text_node(const std::string& data);
    std::unique_ptr<Comment> create_comment(const std::string& data);

    // ID-based lookup
    Element* get_element_by_id(std::string_view id);
    void register_id(const std::string& id, Element* element);
    void unregister_id(const std::string& id);

private:
    std::unordered_map<std::string, Element*> id_map_;
};

} // namespace clever::dom
