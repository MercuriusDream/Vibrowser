#include <clever/dom/document.h>

namespace clever::dom {

Document::Document() : Node(NodeType::Document) {}

Element* Document::document_element() const {
    // The document element is the first Element child (typically <html>)
    for (auto& child : children_) {
        if (child->node_type() == NodeType::Element) {
            return static_cast<Element*>(child.get());
        }
    }
    return nullptr;
}

Element* Document::body() const {
    Element* html = document_element();
    if (!html) return nullptr;

    // Find <body> among html's children
    Node* child = html->first_child();
    while (child) {
        if (child->node_type() == NodeType::Element) {
            Element* elem = static_cast<Element*>(child);
            if (elem->tag_name() == "body") {
                return elem;
            }
        }
        child = child->next_sibling();
    }
    return nullptr;
}

Element* Document::head() const {
    Element* html = document_element();
    if (!html) return nullptr;

    // Find <head> among html's children
    Node* child = html->first_child();
    while (child) {
        if (child->node_type() == NodeType::Element) {
            Element* elem = static_cast<Element*>(child);
            if (elem->tag_name() == "head") {
                return elem;
            }
        }
        child = child->next_sibling();
    }
    return nullptr;
}

std::unique_ptr<Element> Document::create_element(const std::string& tag) {
    return std::make_unique<Element>(tag);
}

std::unique_ptr<Text> Document::create_text_node(const std::string& data) {
    return std::make_unique<Text>(data);
}

std::unique_ptr<Comment> Document::create_comment(const std::string& data) {
    return std::make_unique<Comment>(data);
}

Element* Document::get_element_by_id(std::string_view id) {
    auto it = id_map_.find(std::string(id));
    if (it != id_map_.end()) {
        return it->second;
    }
    return nullptr;
}

void Document::register_id(const std::string& id, Element* element) {
    id_map_[id] = element;
}

void Document::unregister_id(const std::string& id) {
    id_map_.erase(id);
}

} // namespace clever::dom
