#pragma once
#include <clever/dom/node.h>

namespace clever::dom {

class Text : public Node {
public:
    explicit Text(const std::string& data);
    const std::string& data() const { return data_; }
    void set_data(const std::string& data);
    std::string text_content() const override;
private:
    std::string data_;
};

} // namespace clever::dom
