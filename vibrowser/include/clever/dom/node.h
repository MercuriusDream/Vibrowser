#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clever::dom {

enum class NodeType {
    Element, Text, Comment, Document, DocumentFragment, DocumentType
};

enum class DirtyFlags : uint8_t {
    None = 0,
    Style = 1 << 0,
    Layout = 1 << 1,
    Paint = 1 << 2,
    All = Style | Layout | Paint
};

inline DirtyFlags operator|(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline DirtyFlags operator&(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

class Node {
public:
    explicit Node(NodeType type);
    virtual ~Node();

    // Non-copyable
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    NodeType node_type() const { return type_; }
    Node* parent() const { return parent_; }
    Node* first_child() const;
    Node* last_child() const;
    Node* next_sibling() const { return next_sibling_; }
    Node* previous_sibling() const { return prev_sibling_; }

    // Tree manipulation
    Node& append_child(std::unique_ptr<Node> child);
    Node& insert_before(std::unique_ptr<Node> child, Node* reference);
    std::unique_ptr<Node> remove_child(Node& child);

    // Dirty flags for incremental updates
    void mark_dirty(DirtyFlags flags);
    DirtyFlags dirty_flags() const { return dirty_; }
    void clear_dirty() { dirty_ = DirtyFlags::None; }

    // Child count
    size_t child_count() const;

    // Iterate children
    template<typename Fn>
    void for_each_child(Fn&& fn) const {
        for (auto& child : children_) {
            fn(*child);
        }
    }

    // Text content (recursive)
    virtual std::string text_content() const;

protected:
    NodeType type_;
    Node* parent_ = nullptr;
    Node* next_sibling_ = nullptr;
    Node* prev_sibling_ = nullptr;
    std::vector<std::unique_ptr<Node>> children_;
    DirtyFlags dirty_ = DirtyFlags::None;
};

} // namespace clever::dom
