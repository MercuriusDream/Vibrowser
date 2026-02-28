#pragma once
#include <clever/dom/element.h>
#include <clever/dom/text.h>
#include <clever/dom/comment.h>
#include <unordered_map>

namespace clever::dom {

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
