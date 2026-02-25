#include <clever/dom/comment.h>

namespace clever::dom {

Comment::Comment(const std::string& data)
    : Node(NodeType::Comment)
    , data_(data) {}

void Comment::set_data(const std::string& data) {
    data_ = data;
}

} // namespace clever::dom
