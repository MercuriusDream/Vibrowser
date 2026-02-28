#include <clever/html/tree_builder.h>
#include <algorithm>

namespace clever::html {

SimpleNode* SimpleNode::append_child(std::unique_ptr<SimpleNode> child) {
    child->parent = this;
    auto* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

SimpleNode* SimpleNode::insert_before(std::unique_ptr<SimpleNode> child, SimpleNode* ref) {
    child->parent = this;
    auto* raw = child.get();
    if (!ref) {
        children.push_back(std::move(child));
        return raw;
    }
    auto it = std::find_if(children.begin(), children.end(),
        [ref](const std::unique_ptr<SimpleNode>& c) { return c.get() == ref; });
    if (it != children.end()) {
        children.insert(it, std::move(child));
    } else {
        children.push_back(std::move(child));
    }
    return raw;
}

void SimpleNode::remove_child(SimpleNode* child) {
    auto it = std::find_if(children.begin(), children.end(),
        [child](const std::unique_ptr<SimpleNode>& c) { return c.get() == child; });
    if (it != children.end()) {
        (*it)->parent = nullptr;
        children.erase(it);
    }
}

std::unique_ptr<SimpleNode> SimpleNode::take_child(SimpleNode* child) {
    auto it = std::find_if(children.begin(), children.end(),
        [child](const std::unique_ptr<SimpleNode>& c) { return c.get() == child; });
    if (it != children.end()) {
        std::unique_ptr<SimpleNode> taken = std::move(*it);
        children.erase(it);
        taken->parent = nullptr;
        return taken;
    }
    return nullptr;
}

std::string SimpleNode::text_content() const {
    if (type == Text || type == Comment) {
        return data;
    }
    std::string result;
    for (auto& child : children) {
        result += child->text_content();
    }
    return result;
}

SimpleNode* SimpleNode::find_element(const std::string& tag) const {
    for (auto& child : children) {
        if (child->type == Element && child->tag_name == tag) {
            return child.get();
        }
        auto* found = child->find_element(tag);
        if (found) return found;
    }
    return nullptr;
}

std::vector<SimpleNode*> SimpleNode::find_all_elements(const std::string& tag) const {
    std::vector<SimpleNode*> result;
    for (auto& child : children) {
        if (child->type == Element && child->tag_name == tag) {
            result.push_back(child.get());
        }
        auto sub = child->find_all_elements(tag);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    return result;
}

} // namespace clever::html
