#pragma once
#include <clever/dom/node.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace clever::dom {

struct Attribute {
    std::string name;
    std::string value;
};

class ClassList {
public:
    void add(const std::string& cls);
    void remove(const std::string& cls);
    bool contains(const std::string& cls) const;
    void toggle(const std::string& cls);
    size_t length() const { return classes_.size(); }
    std::string to_string() const;

    const std::vector<std::string>& items() const { return classes_; }
private:
    std::vector<std::string> classes_;
};

class Element : public Node {
public:
    explicit Element(const std::string& tag_name, const std::string& ns = "");

    const std::string& tag_name() const { return tag_name_; }
    const std::string& namespace_uri() const { return namespace_uri_; }

    // Attributes
    std::optional<std::string> get_attribute(std::string_view name) const;
    void set_attribute(const std::string& name, const std::string& value);
    void remove_attribute(const std::string& name);
    bool has_attribute(std::string_view name) const;
    const std::vector<Attribute>& attributes() const { return attributes_; }

    // Shortcuts
    const std::string& id() const { return id_; }
    ClassList& class_list() { return class_list_; }
    const ClassList& class_list() const { return class_list_; }

    std::string text_content() const override;

private:
    std::string tag_name_;
    std::string namespace_uri_;
    std::vector<Attribute> attributes_;
    std::string id_;
    ClassList class_list_;

    void on_attribute_changed(const std::string& name, const std::string& value);
};

} // namespace clever::dom
