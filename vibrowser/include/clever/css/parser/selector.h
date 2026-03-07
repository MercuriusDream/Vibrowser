#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace clever::css {

struct SelectorList;

struct Specificity {
    int a = 0;  // ID selectors
    int b = 0;  // class, attribute, pseudo-class
    int c = 0;  // type, pseudo-element

    bool operator<(const Specificity& other) const;
    bool operator==(const Specificity& other) const;
    bool operator>(const Specificity& other) const { return other < *this; }
};

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
    std::string compiled_selector_list_cache_key;
    std::shared_ptr<const SelectorList> parsed_selector_list;
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

enum class RightmostSelectorKeyType {
    None,
    Type,
    Class,
    Id,
    Attribute,
};

struct RightmostSelectorKey {
    RightmostSelectorKeyType type = RightmostSelectorKeyType::None;
    std::string value;
};

struct ComplexSelector {
    struct Part {
        CompoundSelector compound;
        std::optional<Combinator> combinator;  // combinator BEFORE this compound
    };
    std::vector<Part> parts;
    std::optional<Specificity> precomputed_specificity;
    RightmostSelectorKey rightmost_match_key;
};

struct SelectorList {
    std::vector<ComplexSelector> selectors;
};

Specificity compute_specificity(const ComplexSelector& selector);
std::string compiled_function_selector_list_cache_key(std::string_view pseudo_name,
                                                      std::string_view argument);
std::shared_ptr<const SelectorList> compile_function_selector_list(std::string_view pseudo_name,
                                                                   std::string_view argument);
std::shared_ptr<const SelectorList> compiled_function_selector_list_for_key(std::string_view cache_key);
std::shared_ptr<const SelectorList> attach_compiled_function_selector_list(SimpleSelector& selector);
const SelectorList* function_selector_list_program(const SimpleSelector& selector);
SelectorList parse_selector_list(std::string_view input);

} // namespace clever::css
