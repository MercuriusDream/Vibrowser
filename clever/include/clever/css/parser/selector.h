#pragma once
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace clever::css {

enum class SimpleSelectorType {
    Type,        // div, p, span
    Class,       // .foo
    Id,          // #bar
    Universal,   // *
    Attribute,   // [attr=val]
    PseudoClass, // :hover, :first-child
    PseudoElement // ::before, ::after
};

enum class AttributeMatch {
    Exists,     // [attr]
    Exact,      // [attr=val]
    Includes,   // [attr~=val]
    DashMatch,  // [attr|=val]
    Prefix,     // [attr^=val]
    Suffix,     // [attr$=val]
    Substring   // [attr*=val]
};

struct SimpleSelector {
    SimpleSelectorType type;
    std::string value;

    // Attribute selector specifics
    AttributeMatch attr_match = AttributeMatch::Exists;
    std::string attr_name;
    std::string attr_value;

    // Pseudo-class specifics (e.g., nth-child argument)
    std::string argument;
};

enum class Combinator {
    Descendant,   // space
    Child,        // >
    NextSibling,  // +
    SubsequentSibling  // ~
};

struct CompoundSelector {
    std::vector<SimpleSelector> simple_selectors;
};

struct ComplexSelector {
    struct Part {
        CompoundSelector compound;
        std::optional<Combinator> combinator;  // combinator BEFORE this compound
    };
    std::vector<Part> parts;
};

struct SelectorList {
    std::vector<ComplexSelector> selectors;
};

struct Specificity {
    int a = 0;  // ID selectors
    int b = 0;  // class, attribute, pseudo-class
    int c = 0;  // type, pseudo-element

    bool operator<(const Specificity& other) const;
    bool operator==(const Specificity& other) const;
    bool operator>(const Specificity& other) const { return other < *this; }
};

Specificity compute_specificity(const ComplexSelector& selector);
SelectorList parse_selector_list(std::string_view input);

} // namespace clever::css
