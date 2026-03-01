#pragma once
#include <clever/css/parser/selector.h>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace clever::css {

struct ComponentValue {
    enum Type { Token, Function, Block };
    Type type;
    std::string value;
    double numeric_value = 0;
    std::string unit;
    std::vector<ComponentValue> children;  // for Function/Block
};

struct Declaration {
    std::string property;
    std::vector<ComponentValue> values;
    bool important = false;
};

struct StyleRule {
    SelectorList selectors;
    std::vector<Declaration> declarations;
    std::string selector_text;  // original selector text
    // CSS @layer metadata used by cascade ordering.
    bool in_layer = false;
    size_t layer_order = 0;
    std::string layer_name;
};

struct MediaQuery {
    std::string condition;
    std::vector<StyleRule> rules;
};

struct ImportRule {
    std::string url;
    std::string media;
};

struct KeyframeRule {
    std::string selector;  // "from", "to", or percentage
    std::vector<Declaration> declarations;
};

struct KeyframesRule {
    std::string name;
    std::vector<KeyframeRule> keyframes;
};

struct FontFaceRule {
    std::string font_family;
    std::string src;           // URL or local() reference
    std::string font_weight;   // "normal", "bold", "100"-"900"
    int min_weight = 0;        // Parsed font-weight minimum (defaults to 0)
    int max_weight = 900;      // Parsed font-weight maximum (defaults to 900)
    std::string font_style;    // "normal", "italic", "oblique"
    std::string unicode_range; // e.g., "U+0000-00FF"
    int unicode_min = 0;       // Parsed Unicode range minimum codepoint.
    int unicode_max = 0x10FFFF; // Parsed Unicode range maximum codepoint.
    std::string font_display;  // "auto", "block", "swap", "fallback", "optional"
    std::string size_adjust;
};

struct SupportsRule {
    std::string condition;  // e.g., "(display: grid)" or "not (display: grid)"
    std::vector<StyleRule> rules;
};

struct LayerRule {
    std::string name;  // layer name (empty for anonymous layers)
    size_t order = 0;
    std::vector<StyleRule> rules;
};

struct ContainerRule {
    std::string name;       // container name (empty for any container)
    std::string condition;  // e.g., "(min-width: 400px)"
    std::vector<StyleRule> rules;
};

struct ScopeRule {
    std::string scope_start;  // e.g., ".card" — root of scope
    std::string scope_end;    // e.g., ".content" — lower boundary (optional)
    std::vector<StyleRule> rules;
};

struct PropertyRule {
    std::string name;           // e.g., "--my-color"
    std::string syntax;         // e.g., "<color>", "<length>", "*"
    bool inherits = true;       // whether the property inherits
    std::string initial_value;  // initial value for the property
};

struct CounterStyleRule {
    std::string name;  // e.g., "thumbs", "custom-decimal"
    std::map<std::string, std::string> descriptors; // system, symbols, suffix, prefix, etc.
};

struct StyleSheet {
    std::vector<StyleRule> rules;
    std::vector<ImportRule> imports;
    std::vector<MediaQuery> media_queries;
    std::vector<KeyframesRule> keyframes;
    std::vector<FontFaceRule> font_faces;
    std::vector<SupportsRule> supports_rules;
    std::vector<LayerRule> layer_rules;
    std::vector<ContainerRule> container_rules;
    std::vector<ScopeRule> scope_rules;
    std::vector<PropertyRule> property_rules;
    std::vector<CounterStyleRule> counter_style_rules;
};

// Parse functions
StyleSheet parse_stylesheet(std::string_view css);
std::vector<Declaration> parse_declaration_block(std::string_view css);

} // namespace clever::css
