#pragma once
#include <clever/css/style/computed_style.h>
#include <clever/css/style/selector_matcher.h>
#include <clever/css/parser/stylesheet.h>
#include <optional>
#include <vector>

namespace clever::css {

struct MatchedRule {
    const StyleRule* rule;
    Specificity specificity;
    size_t source_order;
};

class PropertyCascade {
public:
    ComputedStyle cascade(
        const std::vector<MatchedRule>& matched_rules,
        const ComputedStyle& parent_style) const;

    void apply_declaration(ComputedStyle& style, const Declaration& decl,
                           const ComputedStyle& parent) const;
};

class StyleResolver {
public:
    void add_stylesheet(const StyleSheet& sheet);

    ComputedStyle resolve(const ElementView& element,
                          const ComputedStyle& parent_style) const;

    std::vector<MatchedRule> collect_matching_rules(const ElementView& element) const;

    // Resolve pseudo-element style (::before or ::after).
    // Returns nullopt if no matching rules exist for the pseudo-element.
    // pseudo_name should be "before" or "after".
    std::optional<ComputedStyle> resolve_pseudo(
        const ElementView& element,
        const std::string& pseudo_name,
        const ComputedStyle& element_style) const;

    // Collect rules that match an element with a specific pseudo-element.
    std::vector<MatchedRule> collect_pseudo_rules(
        const ElementView& element,
        const std::string& pseudo_name) const;

    // Set default custom property from @property initial-value
    void set_default_custom_property(const std::string& name, const std::string& value);
    const std::unordered_map<std::string, std::string>& default_custom_properties() const {
        return default_custom_props_;
    }

    // Set viewport dimensions for @media query evaluation
    void set_viewport(float width, float height) {
        viewport_width_ = width;
        viewport_height_ = height;
    }

private:
    // Evaluate a @media condition string against current viewport
    bool evaluate_media_condition(const std::string& condition) const;
    // Evaluate a @supports condition string
    bool evaluate_supports_condition(const std::string& condition) const;
    // Helper: collect rules from a rule list into matched results
    void collect_from_rules(const std::vector<StyleRule>& rules,
                            const ElementView& element,
                            std::vector<MatchedRule>& result,
                            size_t& source_order) const;
    void collect_pseudo_from_rules(const std::vector<StyleRule>& rules,
                                   const ElementView& element,
                                   const std::string& pseudo_name,
                                   std::vector<MatchedRule>& result,
                                   size_t& source_order) const;

    SelectorMatcher matcher_;
    PropertyCascade cascade_;
    std::vector<StyleSheet> stylesheets_;
    std::unordered_map<std::string, std::string> default_custom_props_;
    float viewport_width_ = 1280;
    float viewport_height_ = 800;
};

// Set/get the global dark mode flag used by light-dark() color function.
// Must be called before parse_color to affect light-dark() resolution.
void set_dark_mode(bool dark);
bool is_dark_mode();

// Override dark mode detection for testing. When set to a value, the override
// takes precedence over system detection in render_html(). Pass -1 to clear.
void set_dark_mode_override(int override_val); // 0=force light, 1=force dark, -1=clear
int get_dark_mode_override(); // returns -1 if not overridden

// Parse a CSS color value string to Color
std::optional<Color> parse_color(const std::string& value);

// Parse a CSS length value to Length
std::optional<Length> parse_length(const std::string& value, const std::string& unit = "");

// Helper: serialize ComponentValue vector to string (for value parsing)
std::string component_values_to_string(const std::vector<ComponentValue>& values);

} // namespace clever::css
