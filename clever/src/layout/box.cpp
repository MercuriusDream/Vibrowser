#include <clever/layout/box.h>

namespace clever::layout {

LayoutNode* LayoutNode::append_child(std::unique_ptr<LayoutNode> child) {
    child->parent = this;
    auto* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

} // namespace clever::layout
