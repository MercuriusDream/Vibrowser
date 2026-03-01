#pragma once
#include <clever/css/parser/selector.h>
#include <string>
#include <vector>

namespace clever::css {

// Minimal element interface for matching (avoids depending on full DOM)
struct ElementView {
    std::string tag_name;
    std::string id;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> attributes;
    ElementView* parent = nullptr;
    ElementView* prev_sibling = nullptr;
    std::vector<ElementView*> children;  // direct element children (for :has())
    size_t child_index = 0;  // 0-based index among siblings
    size_t sibling_count = 0;
    size_t same_type_index = 0;  // 0-based index among siblings of same tag
    size_t same_type_count = 0;  // total count of siblings with same tag
    size_t child_element_count = 0;  // number of element children (for :empty)
    bool has_text_children = false;  // has non-empty text children (for :empty)
};

class SelectorMatcher {
public:
    bool matches(const ElementView& element, const ComplexSelector& selector) const;
    bool matches_compound(const ElementView& element, const CompoundSelector& compound) const;
    bool has_selector_matches(const ElementView& element, const ComplexSelector& selector) const;
    bool matches_simple(const ElementView& element, const SimpleSelector& simple) const;
};

} // namespace clever::css
