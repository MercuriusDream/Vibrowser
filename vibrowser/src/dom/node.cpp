#include <clever/dom/node.h>
#include <algorithm>
#include <cassert>

namespace clever::dom {

Node::Node(NodeType type) : type_(type) {}

Node::~Node() = default;

Node* Node::first_child() const {
    if (children_.empty()) return nullptr;
    return children_.front().get();
}

Node* Node::last_child() const {
    if (children_.empty()) return nullptr;
    return children_.back().get();
}

Node& Node::append_child(std::unique_ptr<Node> child) {
    return insert_before(std::move(child), nullptr);
}

Node& Node::insert_before(std::unique_ptr<Node> child, Node* reference) {
    assert(child != nullptr);

    // If reference is null, append at the end
    if (reference == nullptr) {
        Node* new_child = child.get();

        // Set up sibling links
        if (!children_.empty()) {
            Node* old_last = children_.back().get();
            old_last->next_sibling_ = new_child;
            new_child->prev_sibling_ = old_last;
        }
        new_child->next_sibling_ = nullptr;
        new_child->parent_ = this;

        children_.push_back(std::move(child));
        return *new_child;
    }

    // Find the reference child's position
    auto it = std::find_if(children_.begin(), children_.end(),
        [reference](const std::unique_ptr<Node>& c) {
            return c.get() == reference;
        });
    assert(it != children_.end() && "reference node is not a child of this node");

    Node* new_child = child.get();
    new_child->parent_ = this;

    // Set up sibling links
    Node* prev = reference->prev_sibling_;
    new_child->prev_sibling_ = prev;
    new_child->next_sibling_ = reference;
    reference->prev_sibling_ = new_child;
    if (prev) {
        prev->next_sibling_ = new_child;
    }

    children_.insert(it, std::move(child));
    return *new_child;
}

std::unique_ptr<Node> Node::remove_child(Node& child) {
    auto it = std::find_if(children_.begin(), children_.end(),
        [&child](const std::unique_ptr<Node>& c) {
            return c.get() == &child;
        });
    assert(it != children_.end() && "child is not a child of this node");

    // Fix sibling links
    Node* prev = child.prev_sibling_;
    Node* next = child.next_sibling_;

    if (prev) {
        prev->next_sibling_ = next;
    }
    if (next) {
        next->prev_sibling_ = prev;
    }

    child.parent_ = nullptr;
    child.prev_sibling_ = nullptr;
    child.next_sibling_ = nullptr;

    std::unique_ptr<Node> removed = std::move(*it);
    children_.erase(it);
    return removed;
}

void Node::mark_dirty(DirtyFlags flags) {
    dirty_ = dirty_ | flags;
    // Propagate to ancestors
    if (parent_) {
        parent_->mark_dirty(flags);
    }
}

size_t Node::child_count() const {
    return children_.size();
}

std::string Node::text_content() const {
    std::string result;
    for (auto& child : children_) {
        result += child->text_content();
    }
    return result;
}

} // namespace clever::dom
