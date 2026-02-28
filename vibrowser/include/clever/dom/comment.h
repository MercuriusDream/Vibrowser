#pragma once
#include <clever/dom/node.h>

namespace clever::dom {

class Comment : public Node {
public:
    explicit Comment(const std::string& data);
    const std::string& data() const { return data_; }
    void set_data(const std::string& data);
private:
    std::string data_;
};

} // namespace clever::dom
