#include <clever/dom/element.h>
#include <algorithm>

namespace clever::dom {

// ---------------------------------------------------------------------------
// ClassList
// ---------------------------------------------------------------------------

void ClassList::add(const std::string& cls) {
    if (!contains(cls)) {
        classes_.push_back(cls);
    }
}

void ClassList::remove(const std::string& cls) {
    auto it = std::find(classes_.begin(), classes_.end(), cls);
    if (it != classes_.end()) {
        classes_.erase(it);
    }
}

bool ClassList::contains(const std::string& cls) const {
    return std::find(classes_.begin(), classes_.end(), cls) != classes_.end();
}

void ClassList::toggle(const std::string& cls) {
    if (contains(cls)) {
        remove(cls);
    } else {
        add(cls);
    }
}

std::string ClassList::to_string() const {
    std::string result;
    for (size_t i = 0; i < classes_.size(); ++i) {
        if (i > 0) result += ' ';
        result += classes_[i];
    }
    return result;
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

Element::Element(const std::string& tag_name, const std::string& ns)
    : Node(NodeType::Element)
    , tag_name_(tag_name)
    , namespace_uri_(ns) {}

std::optional<std::string> Element::get_attribute(std::string_view name) const {
    for (auto& attr : attributes_) {
        if (attr.name == name) {
            return attr.value;
        }
    }
    return std::nullopt;
}

void Element::set_attribute(const std::string& name, const std::string& value) {
    // Check if attribute already exists
    for (auto& attr : attributes_) {
        if (attr.name == name) {
            attr.value = value;
            on_attribute_changed(name, value);
            return;
        }
    }
    // New attribute
    attributes_.push_back({name, value});
    on_attribute_changed(name, value);
}

void Element::remove_attribute(const std::string& name) {
    auto it = std::find_if(attributes_.begin(), attributes_.end(),
        [&name](const Attribute& attr) { return attr.name == name; });
    if (it != attributes_.end()) {
        attributes_.erase(it);
        // Clear cached values if needed
        if (name == "id") {
            id_.clear();
        }
    }
}

bool Element::has_attribute(std::string_view name) const {
    for (auto& attr : attributes_) {
        if (attr.name == name) {
            return true;
        }
    }
    return false;
}

std::string Element::text_content() const {
    // Recursively collect text from all descendant text nodes
    std::string result;
    for_each_child([&](const Node& child) {
        result += child.text_content();
    });
    return result;
}

void Element::on_attribute_changed(const std::string& name, const std::string& value) {
    if (name == "id") {
        id_ = value;
    }
    // Mark dirty for style recalculation
    mark_dirty(DirtyFlags::Style);
}

} // namespace clever::dom
