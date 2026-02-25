#include <clever/dom/text.h>

namespace clever::dom {

Text::Text(const std::string& data)
    : Node(NodeType::Text)
    , data_(data) {}

void Text::set_data(const std::string& data) {
    data_ = data;
    mark_dirty(DirtyFlags::Layout | DirtyFlags::Paint);
}

std::string Text::text_content() const {
    return data_;
}

} // namespace clever::dom
