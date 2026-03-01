#include <clever/css/style/style_resolver.h>
#include <clever/layout/box.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace clever::css {

std::vector<std::pair<std::string, int>> parse_font_feature_settings(const std::string& value);

void apply_inline_property(EdgeSizes& edge, const std::string& side, const Length& value, Direction dir) {
    if (side == "start") {
        if (dir == Direction::Ltr) edge.left = value;
        else if (dir == Direction::Rtl) edge.right = value;
        return;
    }
    if (side == "end") {
        if (dir == Direction::Ltr) edge.right = value;
        else if (dir == Direction::Rtl) edge.left = value;
    }
}

// Specificity operators and compute_specificity are defined in selector.cpp
// (clever_css_parser library). They are linked transitively via clever_css_style -> clever_css_parser.

// ============================================================================
// Helper functions
// ============================================================================

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// Get the string value from a declaration's ComponentValue vector
std::string decl_value_string(const Declaration& decl) {
    return component_values_to_string(decl.values);
}

// Split a string by whitespace
std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

// Split a CSS multi-background value into individual layers.
// Commas inside parentheses (e.g. inside gradient functions) are NOT treated as separators.
std::vector<std::string> split_background_layers(const std::string& value) {
    std::vector<std::string> layers;
    int paren_depth = 0;
    std::string current;
    for (char ch : value) {
        if (ch == '(') paren_depth++;
        else if (ch == ')') { if (paren_depth > 0) paren_depth--; }
        if (ch == ',' && paren_depth == 0) {
            layers.push_back(trim(current));
            current.clear();
        } else {
            current += ch;
        }
    }
    if (!current.empty()) layers.push_back(trim(current));
    return layers;
}

// Split on whitespace but respect parentheses
std::vector<std::string> split_whitespace_paren(const std::string& s) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '(') { depth++; current += c; }
        else if (c == ')') { depth--; current += c; }
        else if ((c == ' ' || c == '\t' || c == '\n') && depth == 0) {
            if (!current.empty()) { parts.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

// Strip surrounding quotes from a string
std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2) {
        if ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

// Parse overflow value
Overflow parse_overflow_value(const std::string& v) {
    if (v == "hidden") return Overflow::Hidden;
    if (v == "scroll") return Overflow::Scroll;
    if (v == "auto") return Overflow::Auto;
    return Overflow::Visible;
}

// Parse border style value
BorderStyle parse_border_style_value(const std::string& v) {
    if (v == "solid") return BorderStyle::Solid;
    if (v == "dashed") return BorderStyle::Dashed;
    if (v == "dotted") return BorderStyle::Dotted;
    if (v == "double") return BorderStyle::Double;
    if (v == "groove") return BorderStyle::Groove;
    if (v == "ridge") return BorderStyle::Ridge;
    if (v == "inset") return BorderStyle::Inset;
    if (v == "outset") return BorderStyle::Outset;
    if (v == "hidden") return BorderStyle::None;
    return BorderStyle::None;
}

void normalize_display_contents_style(ComputedStyle& style) {
    if (style.display != Display::Contents) {
        return;
    }

    // display: contents generates no principal box, so this element's own
    // box model/background are ignored.
    style.margin = {Length::zero(), Length::zero(), Length::zero(), Length::zero()};
    style.padding = {Length::zero(), Length::zero(), Length::zero(), Length::zero()};

    style.border_top.width = Length::zero();
    style.border_right.width = Length::zero();
    style.border_bottom.width = Length::zero();
    style.border_left.width = Length::zero();
    style.border_top.style = BorderStyle::None;
    style.border_right.style = BorderStyle::None;
    style.border_bottom.style = BorderStyle::None;
    style.border_left.style = BorderStyle::None;

    style.background_color = Color::transparent();
    style.bg_image_url.clear();
    style.gradient_type = 0;
    style.gradient_stops.clear();
}

} // anonymous namespace

// ============================================================================
// PropertyCascade
// ============================================================================

ComputedStyle PropertyCascade::cascade(
    const std::vector<MatchedRule>& matched_rules,
    const ComputedStyle& parent_style) const {

    // Start with defaults inherited from parent
    ComputedStyle style = default_style_for_tag("");  // base defaults
    style.z_index = clever::layout::Z_INDEX_AUTO;

    // Apply inherited properties from parent
    style.color = parent_style.color;
    style.font_family = parent_style.font_family;
    style.font_size = parent_style.font_size;
    style.font_weight = parent_style.font_weight;
    style.font_style = parent_style.font_style;
    style.line_height = parent_style.line_height;
    style.line_height_unitless = parent_style.line_height_unitless;
    style.text_align = parent_style.text_align;
    style.text_align_last = parent_style.text_align_last;
    style.text_transform = parent_style.text_transform;
    style.white_space = parent_style.white_space;
    style.letter_spacing = parent_style.letter_spacing;
    style.word_spacing = parent_style.word_spacing;
    style.visibility = parent_style.visibility;
    style.cursor = parent_style.cursor;
    style.list_style_type = parent_style.list_style_type;
    style.list_style_image = parent_style.list_style_image;
    style.table_layout = parent_style.table_layout;
    style.caption_side = parent_style.caption_side;
    style.empty_cells = parent_style.empty_cells;
    style.quotes = parent_style.quotes;
    style.hyphens = parent_style.hyphens;
    style.overflow_wrap = parent_style.overflow_wrap;
    style.text_justify = parent_style.text_justify;
    style.hanging_punctuation = parent_style.hanging_punctuation;
    style.font_variant = parent_style.font_variant;
    style.font_variant_caps = parent_style.font_variant_caps;
    style.font_variant_numeric = parent_style.font_variant_numeric;
    style.font_synthesis = parent_style.font_synthesis;
    style.font_variant_alternates = parent_style.font_variant_alternates;
    style.font_feature_settings = parent_style.font_feature_settings;
    style.font_variation_settings = parent_style.font_variation_settings;
    style.font_optical_sizing = parent_style.font_optical_sizing;
    style.print_color_adjust = parent_style.print_color_adjust;
    style.image_orientation = parent_style.image_orientation;
    style.image_orientation_explicit = false;
    style.tab_size = parent_style.tab_size;
    style.font_kerning = parent_style.font_kerning;
    style.font_variant_ligatures = parent_style.font_variant_ligatures;
    style.font_variant_east_asian = parent_style.font_variant_east_asian;
    style.font_palette = parent_style.font_palette;
    style.font_variant_position = parent_style.font_variant_position;
    style.font_language_override = parent_style.font_language_override;
    style.font_size_adjust = parent_style.font_size_adjust;
    style.font_stretch = parent_style.font_stretch;
    style.text_decoration_skip_ink = parent_style.text_decoration_skip_ink;
    style.text_wrap = parent_style.text_wrap;
    style.white_space_collapse = parent_style.white_space_collapse;
    style.line_break = parent_style.line_break;
    style.math_style = parent_style.math_style;
    style.math_depth = parent_style.math_depth;
    style.orphans = parent_style.orphans;
    style.widows = parent_style.widows;
    style.text_rendering = parent_style.text_rendering;
    style.font_smooth = parent_style.font_smooth;
    style.text_size_adjust = parent_style.text_size_adjust;
    style.ruby_align = parent_style.ruby_align;
    style.ruby_position = parent_style.ruby_position;
    style.ruby_overhang = parent_style.ruby_overhang;
    style.text_orientation = parent_style.text_orientation;
    style.writing_mode = parent_style.writing_mode;
    style.text_underline_position = parent_style.text_underline_position;
    style.color_scheme = parent_style.color_scheme;
    style.paint_order = parent_style.paint_order;
    style.caret_color = parent_style.caret_color;
    style.accent_color = parent_style.accent_color;
    style.color_interpolation = parent_style.color_interpolation;

    // Build a list of all declarations with their priority
    struct PrioritizedDecl {
        const Declaration* decl;
        Specificity specificity;
        size_t source_order;
        bool important;
        bool in_layer;
        size_t layer_order;
    };

    std::vector<PrioritizedDecl> all_decls;

    for (const auto& matched : matched_rules) {
        for (const auto& decl : matched.rule->declarations) {
            all_decls.push_back({
                &decl,
                matched.specificity,
                matched.source_order,
                decl.important,
                matched.rule->in_layer,
                matched.rule->layer_order
            });
        }
    }

    // Sort by cascade order:
    // 1. !important declarations win over normal
    // 2. Higher specificity wins
    // 3. Later source order wins
    std::stable_sort(all_decls.begin(), all_decls.end(),
        [](const PrioritizedDecl& a, const PrioritizedDecl& b) {
            // Sort so that "winning" declarations come LAST
            if (a.important != b.important) {
                return !a.important;  // non-important before important
            }
            // CSS @layer ordering:
            // - Normal declarations: unlayered > layered; later layers win.
            // - !important declarations: layered > unlayered; earlier layers win.
            if (a.important) {
                if (a.in_layer != b.in_layer) {
                    return !a.in_layer;  // unlayered first, layered last
                }
                if (a.in_layer && b.in_layer && a.layer_order != b.layer_order) {
                    return a.layer_order > b.layer_order;  // later first, earlier last
                }
            } else {
                if (a.in_layer != b.in_layer) {
                    return a.in_layer;  // layered first, unlayered last
                }
                if (a.in_layer && b.in_layer && a.layer_order != b.layer_order) {
                    return a.layer_order < b.layer_order;  // earlier first, later last
                }
            }
            if (!(a.specificity == b.specificity)) {
                return a.specificity < b.specificity;  // lower specificity first
            }
            return a.source_order < b.source_order;  // earlier source first
        });

    // Apply declarations in order (last one wins for each property)
    for (const auto& pd : all_decls) {
        apply_declaration(style, *pd.decl, parent_style);
    }

    // CSS spec: unitless line-height is inherited as the *number*, not the
    // computed value. If the parent used a unitless line-height and no explicit
    // line-height was set on this element, recompute line-height using this
    // element's own font-size.
    if (style.line_height_unitless > 0 &&
        style.font_size.value != parent_style.font_size.value) {
        style.line_height = Length::px(style.line_height_unitless * style.font_size.value);
    }

    normalize_display_contents_style(style);

    return style;
}

void PropertyCascade::apply_declaration(
    ComputedStyle& style,
    const Declaration& decl,
    const ComputedStyle& parent) const {

    const std::string& prop = decl.property;
    std::string value_str = trim(decl_value_string(decl));

    // Store custom properties (--foo: value)
    if (prop.size() > 2 && prop[0] == '-' && prop[1] == '-') {
        style.custom_properties[prop] = value_str;
        return;
    }

    // Resolve var() references in values — handles multiple and nested var()
    for (int var_pass = 0; var_pass < 8; var_pass++) {
        auto pos = value_str.find("var(");
        if (pos == std::string::npos) break;

        // Find matching closing paren (respects nesting)
        int depth = 1;
        size_t end = pos + 4;
        while (end < value_str.size() && depth > 0) {
            if (value_str[end] == '(') depth++;
            else if (value_str[end] == ')') depth--;
            if (depth > 0) end++;
        }
        if (depth != 0) break; // unmatched parens

        std::string inner = value_str.substr(pos + 4, end - pos - 4);
        // Split on first comma (fallback may itself contain var())
        std::string var_name;
        std::string fallback;
        auto comma = inner.find(',');
        if (comma != std::string::npos) {
            var_name = inner.substr(0, comma);
            fallback = inner.substr(comma + 1);
            while (!fallback.empty() && fallback.front() == ' ') fallback.erase(0, 1);
            while (!fallback.empty() && fallback.back() == ' ') fallback.pop_back();
        } else {
            var_name = inner;
        }
        while (!var_name.empty() && var_name.front() == ' ') var_name.erase(0, 1);
        while (!var_name.empty() && var_name.back() == ' ') var_name.pop_back();

        auto it = style.custom_properties.find(var_name);
        if (it != style.custom_properties.end()) {
            value_str = value_str.substr(0, pos) + it->second + value_str.substr(end + 1);
        } else {
            auto pit = parent.custom_properties.find(var_name);
            if (pit != parent.custom_properties.end()) {
                value_str = value_str.substr(0, pos) + pit->second + value_str.substr(end + 1);
            } else if (!fallback.empty()) {
                value_str = value_str.substr(0, pos) + fallback + value_str.substr(end + 1);
            } else {
                break; // unresolvable var, stop to prevent infinite loop
            }
        }
    }

    // If unresolved var() remains, declaration is invalid at computed-value
    // time; ignore it rather than forcing property-specific fallbacks.
    if (value_str.find("var(") != std::string::npos) {
        return;
    }

    // Resolve env() references — CSS Environment Variables (safe-area-inset-*, etc.)
    // On desktop, all env() values resolve to 0px (no notch/safe area).
    for (int env_pass = 0; env_pass < 4; env_pass++) {
        auto epos = value_str.find("env(");
        if (epos == std::string::npos) break;
        int edepth = 1;
        size_t eend = epos + 4;
        while (eend < value_str.size() && edepth > 0) {
            if (value_str[eend] == '(') edepth++;
            else if (value_str[eend] == ')') edepth--;
            if (edepth > 0) eend++;
        }
        if (edepth != 0) break;
        std::string env_inner = value_str.substr(epos + 4, eend - epos - 4);
        // Check for fallback value after comma
        std::string env_fallback = "0px";
        auto env_comma = env_inner.find(',');
        if (env_comma != std::string::npos) {
            env_fallback = env_inner.substr(env_comma + 1);
            while (!env_fallback.empty() && env_fallback.front() == ' ') env_fallback.erase(0, 1);
            while (!env_fallback.empty() && env_fallback.back() == ' ') env_fallback.pop_back();
        }
        // Desktop: all env() values (safe-area-inset-top/right/bottom/left) are 0
        value_str = value_str.substr(0, epos) + env_fallback + value_str.substr(eend + 1);
    }

    std::string value_lower = to_lower(value_str);

    // Handle 'inherit' keyword for any property
    if (value_lower == "inherit") {
        // Copy the property value from parent — comprehensive list
        // Text / font (naturally inherited)
        if (prop == "color") { style.color = parent.color; return; }
        if (prop == "font-family") { style.font_family = parent.font_family; return; }
        if (prop == "font-size") { style.font_size = parent.font_size; return; }
        if (prop == "font-weight") { style.font_weight = parent.font_weight; return; }
        if (prop == "font-style") { style.font_style = parent.font_style; return; }
        if (prop == "line-height") { style.line_height = parent.line_height; style.line_height_unitless = parent.line_height_unitless; return; }
        if (prop == "text-align") { style.text_align = parent.text_align; return; }
        if (prop == "text-align-last") { style.text_align_last = parent.text_align_last; return; }
        if (prop == "text-transform") { style.text_transform = parent.text_transform; return; }
        if (prop == "text-indent") { style.text_indent = parent.text_indent; return; }
        if (prop == "white-space") { style.white_space = parent.white_space; return; }
        if (prop == "letter-spacing") { style.letter_spacing = parent.letter_spacing; return; }
        if (prop == "word-spacing") { style.word_spacing = parent.word_spacing; return; }
        if (prop == "word-break") { style.word_break = parent.word_break; return; }
        if (prop == "overflow-wrap" || prop == "word-wrap") { style.overflow_wrap = parent.overflow_wrap; return; }
        if (prop == "text-wrap" || prop == "text-wrap-mode") { style.text_wrap = parent.text_wrap; return; }
        if (prop == "direction") { style.direction = parent.direction; return; }
        if (prop == "tab-size") { style.tab_size = parent.tab_size; return; }
        if (prop == "hyphens") { style.hyphens = parent.hyphens; return; }
        if (prop == "visibility") { style.visibility = parent.visibility; return; }
        if (prop == "cursor") { style.cursor = parent.cursor; return; }
        if (prop == "list-style-type") { style.list_style_type = parent.list_style_type; return; }
        if (prop == "list-style-position") { style.list_style_position = parent.list_style_position; return; }
        if (prop == "list-style-image") { style.list_style_image = parent.list_style_image; return; }
        if (prop == "font-variant") { style.font_variant = parent.font_variant; return; }
        if (prop == "font-variant-caps") { style.font_variant_caps = parent.font_variant_caps; return; }
        if (prop == "font-variant-numeric") { style.font_variant_numeric = parent.font_variant_numeric; return; }
        if (prop == "font-kerning") { style.font_kerning = parent.font_kerning; return; }
        if (prop == "text-rendering") { style.text_rendering = parent.text_rendering; return; }
        if (prop == "orphans") { style.orphans = parent.orphans; return; }
        if (prop == "widows") { style.widows = parent.widows; return; }
        if (prop == "quotes") { style.quotes = parent.quotes; return; }
        // Non-inherited properties that can be forced to inherit
        if (prop == "display") { style.display = parent.display; return; }
        if (prop == "position") { style.position = parent.position; return; }
        if (prop == "float") { style.float_val = parent.float_val; return; }
        if (prop == "clear") { style.clear = parent.clear; return; }
        if (prop == "background-color") { style.background_color = parent.background_color; return; }
        if (prop == "background") { style.background_color = parent.background_color; style.bg_image_url = parent.bg_image_url; style.gradient_type = parent.gradient_type; return; }
        if (prop == "opacity") { style.opacity = parent.opacity; return; }
        if (prop == "overflow") { style.overflow_x = parent.overflow_x; style.overflow_y = parent.overflow_y; return; }
        if (prop == "overflow-x") { style.overflow_x = parent.overflow_x; return; }
        if (prop == "overflow-y") { style.overflow_y = parent.overflow_y; return; }
        if (prop == "z-index") { style.z_index = parent.z_index; return; }
        if (prop == "width") { style.width = parent.width; return; }
        if (prop == "height") { style.height = parent.height; return; }
        if (prop == "min-width") { style.min_width = parent.min_width; return; }
        if (prop == "max-width") { style.max_width = parent.max_width; return; }
        if (prop == "min-height") { style.min_height = parent.min_height; return; }
        if (prop == "max-height") { style.max_height = parent.max_height; return; }
        if (prop == "margin") { style.margin = parent.margin; return; }
        if (prop == "margin-top") { style.margin.top = parent.margin.top; return; }
        if (prop == "margin-right") { style.margin.right = parent.margin.right; return; }
        if (prop == "margin-bottom") { style.margin.bottom = parent.margin.bottom; return; }
        if (prop == "margin-left") { style.margin.left = parent.margin.left; return; }
        if (prop == "padding") { style.padding = parent.padding; return; }
        if (prop == "padding-top") { style.padding.top = parent.padding.top; return; }
        if (prop == "padding-right") { style.padding.right = parent.padding.right; return; }
        if (prop == "padding-bottom") { style.padding.bottom = parent.padding.bottom; return; }
        if (prop == "padding-left") { style.padding.left = parent.padding.left; return; }
        if (prop == "border-color") { style.border_top.color = parent.border_top.color; style.border_right.color = parent.border_right.color; style.border_bottom.color = parent.border_bottom.color; style.border_left.color = parent.border_left.color; return; }
        if (prop == "border-top-color") { style.border_top.color = parent.border_top.color; return; }
        if (prop == "border-right-color") { style.border_right.color = parent.border_right.color; return; }
        if (prop == "border-bottom-color") { style.border_bottom.color = parent.border_bottom.color; return; }
        if (prop == "border-left-color") { style.border_left.color = parent.border_left.color; return; }
        if (prop == "border-width") { style.border_top.width = parent.border_top.width; style.border_right.width = parent.border_right.width; style.border_bottom.width = parent.border_bottom.width; style.border_left.width = parent.border_left.width; return; }
        if (prop == "border-top-width") { style.border_top.width = parent.border_top.width; return; }
        if (prop == "border-right-width") { style.border_right.width = parent.border_right.width; return; }
        if (prop == "border-bottom-width") { style.border_bottom.width = parent.border_bottom.width; return; }
        if (prop == "border-left-width") { style.border_left.width = parent.border_left.width; return; }
        if (prop == "border-style") { style.border_top.style = parent.border_top.style; style.border_right.style = parent.border_right.style; style.border_bottom.style = parent.border_bottom.style; style.border_left.style = parent.border_left.style; return; }
        if (prop == "border-radius") { style.border_radius = parent.border_radius; style.border_radius_tl = parent.border_radius_tl; style.border_radius_tr = parent.border_radius_tr; style.border_radius_bl = parent.border_radius_bl; style.border_radius_br = parent.border_radius_br; return; }
        if (prop == "text-decoration") {
            style.text_decoration = parent.text_decoration;
            style.text_decoration_bits = parent.text_decoration_bits;
            style.text_decoration_color = parent.text_decoration_color;
            style.text_decoration_style = parent.text_decoration_style;
            style.text_decoration_thickness = parent.text_decoration_thickness;
            return;
        }
        if (prop == "text-decoration-color") { style.text_decoration_color = parent.text_decoration_color; return; }
        if (prop == "text-decoration-style") { style.text_decoration_style = parent.text_decoration_style; return; }
        if (prop == "box-sizing") { style.box_sizing = parent.box_sizing; return; }
        if (prop == "vertical-align") { style.vertical_align = parent.vertical_align; return; }
        if (prop == "border-collapse") { style.border_collapse = parent.border_collapse; return; }
        if (prop == "border-spacing") { style.border_spacing = parent.border_spacing; style.border_spacing_v = parent.border_spacing_v; return; }
        if (prop == "table-layout") { style.table_layout = parent.table_layout; return; }
        if (prop == "text-overflow") { style.text_overflow = parent.text_overflow; return; }
        if (prop == "flex-direction") { style.flex_direction = parent.flex_direction; return; }
        if (prop == "flex-wrap") { style.flex_wrap = parent.flex_wrap; return; }
        if (prop == "flex-flow") { style.flex_direction = parent.flex_direction; style.flex_wrap = parent.flex_wrap; return; }
        if (prop == "justify-content") { style.justify_content = parent.justify_content; return; }
        if (prop == "align-items") { style.align_items = parent.align_items; return; }
        if (prop == "place-items") { style.align_items = parent.align_items; style.justify_items = parent.justify_items; return; }
        if (prop == "place-content") { style.align_content = parent.align_content; style.justify_content = parent.justify_content; return; }
        if (prop == "flex") { style.flex_grow = parent.flex_grow; style.flex_shrink = parent.flex_shrink; style.flex_basis = parent.flex_basis; return; }
        if (prop == "flex-grow") { style.flex_grow = parent.flex_grow; return; }
        if (prop == "flex-shrink") { style.flex_shrink = parent.flex_shrink; return; }
        if (prop == "gap" || prop == "grid-gap") { style.gap = parent.gap; style.column_gap_val = parent.column_gap_val; return; }
        if (prop == "row-gap" || prop == "grid-row-gap") { style.gap = parent.gap; return; }
        if (prop == "column-gap" || prop == "grid-column-gap") { style.column_gap_val = parent.column_gap_val; return; }
        if (prop == "order") { style.order = parent.order; return; }
        if (prop == "outline-color") { style.outline_color = parent.outline_color; return; }
        if (prop == "outline-width") { style.outline_width = parent.outline_width; return; }
        if (prop == "outline-style") { style.outline_style = parent.outline_style; return; }
        if (prop == "user-select") { style.user_select = parent.user_select; return; }
        if (prop == "pointer-events") { style.pointer_events = parent.pointer_events; return; }
        if (prop == "aspect-ratio") { style.aspect_ratio = parent.aspect_ratio; style.aspect_ratio_is_auto = parent.aspect_ratio_is_auto; return; }
        if (prop == "writing-mode") { style.writing_mode = parent.writing_mode; return; }
        if (prop == "content") { style.content = parent.content; return; }
        return;
    }

    // Handle 'initial' keyword — reset property to CSS initial value
    // Exclude 'all' shorthand which has its own handler.
    if (value_lower == "initial" && prop != "all") {
        ComputedStyle initial_style; // default-constructed = CSS initial values
        // Text / font
        if (prop == "color") { style.color = Color::black(); return; }
        if (prop == "font-family") { style.font_family = initial_style.font_family; return; }
        if (prop == "font-size") { style.font_size = initial_style.font_size; return; }
        if (prop == "font-weight") { style.font_weight = initial_style.font_weight; return; }
        if (prop == "font-style") { style.font_style = FontStyle::Normal; return; }
        if (prop == "line-height") { style.line_height = initial_style.line_height; style.line_height_unitless = 1.2f; return; }
        if (prop == "text-align") { style.text_align = TextAlign::Left; return; }
        if (prop == "text-transform") { style.text_transform = TextTransform::None; return; }
        if (prop == "text-indent") { style.text_indent = Length::zero(); return; }
        if (prop == "white-space") { style.white_space = WhiteSpace::Normal; return; }
        if (prop == "text-wrap" || prop == "text-wrap-mode") { style.text_wrap = 0; return; }
        if (prop == "letter-spacing") { style.letter_spacing = Length::zero(); return; }
        if (prop == "word-spacing") { style.word_spacing = Length::zero(); return; }
        if (prop == "visibility") { style.visibility = Visibility::Visible; return; }
        if (prop == "cursor") { style.cursor = Cursor::Auto; return; }
        if (prop == "direction") { style.direction = Direction::Ltr; return; }
        // Display & position
        if (prop == "display") { style.display = Display::Inline; return; }
        if (prop == "position") { style.position = Position::Static; return; }
        if (prop == "float") { style.float_val = Float::None; return; }
        if (prop == "clear") { style.clear = Clear::None; return; }
        // Visual
        if (prop == "background-color" || prop == "background") { style.background_color = Color::transparent(); return; }
        if (prop == "opacity") { style.opacity = 1.0f; return; }
        if (prop == "overflow") { style.overflow_x = Overflow::Visible; style.overflow_y = Overflow::Visible; return; }
        if (prop == "overflow-x") { style.overflow_x = Overflow::Visible; return; }
        if (prop == "overflow-y") { style.overflow_y = Overflow::Visible; return; }
        if (prop == "z-index") { style.z_index = clever::layout::Z_INDEX_AUTO; return; }
        // Sizing
        if (prop == "width") { style.width = Length::auto_val(); return; }
        if (prop == "height") { style.height = Length::auto_val(); return; }
        if (prop == "min-width") { style.min_width = Length::zero(); return; }
        if (prop == "max-width") { style.max_width = Length::px(std::numeric_limits<float>::max()); return; }
        if (prop == "min-height") { style.min_height = Length::zero(); return; }
        if (prop == "max-height") { style.max_height = Length::px(std::numeric_limits<float>::max()); return; }
        // Box model
        if (prop == "margin" || prop == "margin-top" || prop == "margin-right" || prop == "margin-bottom" || prop == "margin-left") {
            if (prop == "margin" || prop == "margin-top") style.margin.top = Length::zero();
            if (prop == "margin" || prop == "margin-right") style.margin.right = Length::zero();
            if (prop == "margin" || prop == "margin-bottom") style.margin.bottom = Length::zero();
            if (prop == "margin" || prop == "margin-left") style.margin.left = Length::zero();
            return;
        }
        if (prop == "padding" || prop == "padding-top" || prop == "padding-right" || prop == "padding-bottom" || prop == "padding-left") {
            if (prop == "padding" || prop == "padding-top") style.padding.top = Length::zero();
            if (prop == "padding" || prop == "padding-right") style.padding.right = Length::zero();
            if (prop == "padding" || prop == "padding-bottom") style.padding.bottom = Length::zero();
            if (prop == "padding" || prop == "padding-left") style.padding.left = Length::zero();
            return;
        }
        if (prop == "border-radius") { style.border_radius = 0; style.border_radius_tl = 0; style.border_radius_tr = 0; style.border_radius_bl = 0; style.border_radius_br = 0; return; }
        if (prop == "box-sizing") { style.box_sizing = BoxSizing::ContentBox; return; }
        if (prop == "text-decoration") {
            style.text_decoration = TextDecoration::None;
            style.text_decoration_bits = 0;
            style.text_decoration_color = Color::transparent();
            style.text_decoration_style = TextDecorationStyle::Solid;
            style.text_decoration_thickness = 0;
            return;
        }
        if (prop == "vertical-align") { style.vertical_align = VerticalAlign::Baseline; return; }
        if (prop == "flex-direction") { style.flex_direction = FlexDirection::Row; return; }
        if (prop == "flex-wrap") { style.flex_wrap = FlexWrap::NoWrap; return; }
        if (prop == "flex-flow") { style.flex_direction = FlexDirection::Row; style.flex_wrap = FlexWrap::NoWrap; return; }
        if (prop == "justify-content") { style.justify_content = JustifyContent::FlexStart; return; }
        if (prop == "align-items") { style.align_items = AlignItems::Stretch; return; }
        if (prop == "place-items") { style.align_items = AlignItems::Stretch; style.justify_items = 3; return; }
        if (prop == "place-content") { style.align_content = 0; style.justify_content = JustifyContent::FlexStart; return; }
        if (prop == "flex") { style.flex_grow = 0; style.flex_shrink = 1; style.flex_basis = Length::auto_val(); return; }
        if (prop == "flex-grow") { style.flex_grow = 0; return; }
        if (prop == "flex-shrink") { style.flex_shrink = 1; return; }
        if (prop == "order") { style.order = 0; return; }
        if (prop == "gap" || prop == "grid-gap") { style.gap = Length::zero(); style.column_gap_val = Length::zero(); return; }
        if (prop == "row-gap" || prop == "grid-row-gap") { style.gap = Length::zero(); return; }
        if (prop == "column-gap" || prop == "grid-column-gap") { style.column_gap_val = Length::zero(); return; }
        if (prop == "aspect-ratio") { style.aspect_ratio = 0; style.aspect_ratio_is_auto = false; return; }
        if (prop == "user-select") { style.user_select = UserSelect::Auto; return; }
        if (prop == "pointer-events") { style.pointer_events = PointerEvents::Auto; return; }
        return;
    }

    // Handle 'unset' keyword — inherited properties get inherit, others get initial.
    // Exclude 'all' shorthand which has its own handler that stores the value.
    if (value_lower == "unset" && prop != "all") {
        const bool is_inherited =
            prop == "color" ||
            prop == "font-family" ||
            prop == "font-size" ||
            prop == "font-weight" ||
            prop == "font-style" ||
            prop == "line-height" ||
            prop == "text-align" ||
            prop == "text-align-last" ||
            prop == "text-transform" ||
            prop == "text-indent" ||
            prop == "white-space" ||
            prop == "letter-spacing" ||
            prop == "word-spacing" ||
            prop == "word-break" ||
            prop == "overflow-wrap" || prop == "word-wrap" ||
            prop == "text-wrap" || prop == "text-wrap-mode" ||
            prop == "direction" ||
            prop == "tab-size" ||
            prop == "hyphens" ||
            prop == "visibility" ||
            prop == "cursor" ||
            prop == "list-style-type" ||
            prop == "list-style-position" ||
            prop == "list-style-image" ||
            prop == "font-variant" ||
            prop == "font-variant-caps" ||
            prop == "font-variant-numeric" ||
            prop == "font-kerning" ||
            prop == "text-rendering" ||
            prop == "orphans" ||
            prop == "widows" ||
            prop == "quotes";

        Declaration keyword_decl = decl;
        keyword_decl.values.clear();
        keyword_decl.values.push_back(ComponentValue{
            ComponentValue::Token,
            is_inherited ? "inherit" : "initial",
            0,
            "",
            {}
        });
        apply_declaration(style, keyword_decl, parent);
        return;
    }

    // Also handle 'revert' (exclude 'all' shorthand)
    if (value_lower == "revert" && prop != "all") {
        // revert: use the UA stylesheet default. For simplicity, treat as unset.
        Declaration unset_decl = decl;
        unset_decl.values.clear();
        unset_decl.values.push_back(ComponentValue{
            ComponentValue::Token,
            "unset",
            0,
            "",
            {}
        });
        apply_declaration(style, unset_decl, parent);
        return;
    }

    // ---- Display ----
    if (prop == "display") {
        if (value_lower == "block") style.display = Display::Block;
        else if (value_lower == "inline") style.display = Display::Inline;
        else if (value_lower == "inline-block") style.display = Display::InlineBlock;
        else if (value_lower == "flex") style.display = Display::Flex;
        else if (value_lower == "inline-flex") style.display = Display::InlineFlex;
        else if (value_lower == "inline flex") style.display = Display::InlineFlex;
        else if (value_lower == "none") style.display = Display::None;
        else if (value_lower == "list-item") style.display = Display::ListItem;
        else if (value_lower == "table") style.display = Display::Table;
        else if (value_lower == "table-row") style.display = Display::TableRow;
        else if (value_lower == "table-cell") style.display = Display::TableCell;
        else if (value_lower == "table-row-group") style.display = Display::TableRowGroup;
        else if (value_lower == "table-header-group") style.display = Display::TableHeaderGroup;
        else if (value_lower == "table-footer-group") style.display = Display::TableRowGroup;
        else if (value_lower == "table-column") style.display = Display::TableCell;
        else if (value_lower == "table-column-group") style.display = Display::TableRow;
        else if (value_lower == "table-caption") style.display = Display::Block;
        else if (value_lower == "grid") style.display = Display::Grid;
        else if (value_lower == "inline-grid") style.display = Display::InlineGrid;
        else if (value_lower == "inline grid") style.display = Display::InlineGrid;
        else if (value_lower == "-webkit-box" || value_lower == "-webkit-inline-box") {
            style.display = Display::Flex; // -webkit-box is legacy flex
        }
        else if (value_lower == "contents") style.display = Display::Contents;
        else if (value_lower == "flow-root") { style.display = Display::Block; style.is_flow_root = true; } // flow-root creates BFC
        else if (value_lower == "ruby") style.display = Display::Inline; // ruby: approximate as inline
        else if (value_lower == "ruby-text") style.display = Display::Inline; // ruby-text: approximate as inline
        return;
    }

    // ---- Position ----
    if (prop == "position") {
        if (value_lower == "static") style.position = Position::Static;
        else if (value_lower == "relative") style.position = Position::Relative;
        else if (value_lower == "absolute") style.position = Position::Absolute;
        else if (value_lower == "fixed") style.position = Position::Fixed;
        else if (value_lower == "sticky" || value_lower == "-webkit-sticky") style.position = Position::Sticky;
        return;
    }

    // ---- Float ----
    if (prop == "float") {
        if (value_lower == "left") style.float_val = Float::Left;
        else if (value_lower == "right") style.float_val = Float::Right;
        else if (value_lower == "inline-start") style.float_val = Float::Left; // LTR mapping
        else if (value_lower == "inline-end") style.float_val = Float::Right;  // LTR mapping
        else style.float_val = Float::None;
        return;
    }

    // ---- Clear ----
    if (prop == "clear") {
        if (value_lower == "left") style.clear = Clear::Left;
        else if (value_lower == "right") style.clear = Clear::Right;
        else if (value_lower == "both") style.clear = Clear::Both;
        else style.clear = Clear::None;
        return;
    }

    // ---- Box Sizing ----
    if (prop == "box-sizing") {
        if (value_lower == "border-box") style.box_sizing = BoxSizing::BorderBox;
        else style.box_sizing = BoxSizing::ContentBox;
        return;
    }

    // ---- Width, Height, Min/Max ----
    if (prop == "width") {
        auto l = parse_length(value_str);
        if (l) style.width = *l;
        return;
    }
    if (prop == "height") {
        auto l = parse_length(value_str);
        if (l) style.height = *l;
        return;
    }
    if (prop == "min-width") {
        auto l = parse_length(value_str);
        if (l) style.min_width = *l;
        return;
    }
    if (prop == "max-width") {
        auto l = parse_length(value_str);
        if (l) style.max_width = *l;
        return;
    }
    if (prop == "min-height") {
        auto l = parse_length(value_str);
        if (l) style.min_height = *l;
        return;
    }
    if (prop == "max-height") {
        auto l = parse_length(value_str);
        if (l) style.max_height = *l;
        return;
    }
    if (prop == "min-inline-size") {
        // CSS logical property: maps to min-width (horizontal-tb LTR)
        auto l = parse_length(value_str);
        if (l) style.min_width = *l;
        return;
    }
    if (prop == "max-inline-size") {
        // CSS logical property: maps to max-width (horizontal-tb LTR)
        if (value_lower == "none") style.max_width = Length::px(-1.0f);
        else { auto l = parse_length(value_str); if (l) style.max_width = *l; }
        return;
    }
    if (prop == "min-block-size") {
        // CSS logical property: maps to min-height (horizontal-tb)
        auto l = parse_length(value_str);
        if (l) style.min_height = *l;
        return;
    }
    if (prop == "max-block-size") {
        // CSS logical property: maps to max-height (horizontal-tb)
        if (value_lower == "none") style.max_height = Length::px(-1.0f);
        else { auto l = parse_length(value_str); if (l) style.max_height = *l; }
        return;
    }
    if (prop == "inline-size") {
        // CSS logical property: maps to width (horizontal-tb)
        auto l = parse_length(value_str);
        if (l) style.width = *l;
        return;
    }
    if (prop == "block-size") {
        // CSS logical property: maps to height (horizontal-tb)
        auto l = parse_length(value_str);
        if (l) style.height = *l;
        return;
    }
    if (prop == "aspect-ratio") {
        style.aspect_ratio = 0;
        style.aspect_ratio_is_auto = false;

        auto parse_aspect_ratio_value = [](const std::string& ratio_text, float& out_ratio) {
            std::string ratio = trim(ratio_text);
            if (ratio.empty()) return;

            auto slash = ratio.find('/');
            try {
                if (slash != std::string::npos) {
                    float w = std::stof(trim(ratio.substr(0, slash)));
                    float h = std::stof(trim(ratio.substr(slash + 1)));
                    if (w > 0 && h > 0) {
                        out_ratio = w / h;
                    }
                } else {
                    float parsed = std::stof(ratio);
                    if (parsed > 0) {
                        out_ratio = parsed;
                    }
                }
            } catch (...) {}
        };

        if (value_lower == "auto") {
            style.aspect_ratio_is_auto = true;
        } else if (value_lower.substr(0, 5) == "auto ") {
            // "auto <ratio>" format
            style.aspect_ratio_is_auto = true;
            parse_aspect_ratio_value(value_lower.substr(5), style.aspect_ratio);
        } else {
            // "<ratio>" format (decimal or fraction)
            parse_aspect_ratio_value(value_lower, style.aspect_ratio);
        }
        return;
    }

    // ---- Margin (shorthand and individual) ----
    if (prop == "margin") {
        auto parse_margin_val = [](const std::string& s) -> std::optional<Length> {
            if (s == "auto") return Length::auto_val();
            return parse_length(s);
        };
        auto parts = split_whitespace(value_str);
        if (parts.size() == 1) {
            auto l = parse_margin_val(parts[0]);
            if (l) {
                style.margin.top = *l;
                style.margin.right = *l;
                style.margin.bottom = *l;
                style.margin.left = *l;
            }
        } else if (parts.size() == 2) {
            auto tb = parse_margin_val(parts[0]);
            auto lr = parse_margin_val(parts[1]);
            if (tb) { style.margin.top = *tb; style.margin.bottom = *tb; }
            if (lr) { style.margin.right = *lr; style.margin.left = *lr; }
        } else if (parts.size() == 3) {
            auto t = parse_margin_val(parts[0]);
            auto lr = parse_margin_val(parts[1]);
            auto b = parse_margin_val(parts[2]);
            if (t) style.margin.top = *t;
            if (lr) { style.margin.right = *lr; style.margin.left = *lr; }
            if (b) style.margin.bottom = *b;
        } else if (parts.size() >= 4) {
            auto t = parse_margin_val(parts[0]);
            auto r = parse_margin_val(parts[1]);
            auto b = parse_margin_val(parts[2]);
            auto l = parse_margin_val(parts[3]);
            if (t) style.margin.top = *t;
            if (r) style.margin.right = *r;
            if (b) style.margin.bottom = *b;
            if (l) style.margin.left = *l;
        }
        return;
    }
    if (prop == "margin-top") {
        if (value_lower == "auto") style.margin.top = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.top = *l; }
        return;
    }
    if (prop == "margin-right") {
        if (value_lower == "auto") style.margin.right = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.right = *l; }
        return;
    }
    if (prop == "margin-bottom") {
        if (value_lower == "auto") style.margin.bottom = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.bottom = *l; }
        return;
    }
    if (prop == "margin-left") {
        if (value_lower == "auto") style.margin.left = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.left = *l; }
        return;
    }
    if (prop == "margin-block") {
        auto parts = split_whitespace(value_str);
        auto parse_margin_val = [](const std::string& s) -> std::optional<Length> {
            if (s == "auto") return Length::auto_val();
            return parse_length(s);
        };
        if (parts.size() == 1) {
            auto v = parse_margin_val(parts[0]);
            if (v) { style.margin.top = *v; style.margin.bottom = *v; }
        } else if (parts.size() >= 2) {
            auto v1 = parse_margin_val(parts[0]);
            auto v2 = parse_margin_val(parts[1]);
            if (v1) style.margin.top = *v1;
            if (v2) style.margin.bottom = *v2;
        }
        return;
    }
    if (prop == "margin-inline") {
        auto parts = split_whitespace(value_str);
        auto parse_margin_val = [](const std::string& s) -> std::optional<Length> {
            if (s == "auto") return Length::auto_val();
            return parse_length(s);
        };
        if (parts.size() == 1) {
            auto v = parse_margin_val(parts[0]);
            if (v) {
                apply_inline_property(style.margin, "start", *v, style.direction);
                apply_inline_property(style.margin, "end", *v, style.direction);
            }
        } else if (parts.size() >= 2) {
            auto v1 = parse_margin_val(parts[0]);
            auto v2 = parse_margin_val(parts[1]);
            if (v1) apply_inline_property(style.margin, "start", *v1, style.direction);
            if (v2) apply_inline_property(style.margin, "end", *v2, style.direction);
        }
        return;
    }
    // ---- CSS margin logical longhands ----
    if (prop == "margin-block-start") {
        if (value_lower == "auto") style.margin.top = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.top = *l; }
        return;
    }
    if (prop == "margin-block-end") {
        if (value_lower == "auto") style.margin.bottom = Length::auto_val();
        else { auto l = parse_length(value_str); if (l) style.margin.bottom = *l; }
        return;
    }
    if (prop == "margin-inline-start") {
        if (value_lower == "auto") apply_inline_property(style.margin, "start", Length::auto_val(), style.direction);
        else { auto l = parse_length(value_str); if (l) apply_inline_property(style.margin, "start", *l, style.direction); }
        return;
    }
    if (prop == "margin-inline-end") {
        if (value_lower == "auto") apply_inline_property(style.margin, "end", Length::auto_val(), style.direction);
        else { auto l = parse_length(value_str); if (l) apply_inline_property(style.margin, "end", *l, style.direction); }
        return;
    }

    // ---- Padding (shorthand and individual) ----
    if (prop == "padding") {
        auto parts = split_whitespace(value_str);
        if (parts.size() == 1) {
            auto l = parse_length(parts[0]);
            if (l) {
                style.padding.top = *l;
                style.padding.right = *l;
                style.padding.bottom = *l;
                style.padding.left = *l;
            }
        } else if (parts.size() == 2) {
            auto tb = parse_length(parts[0]);
            auto lr = parse_length(parts[1]);
            if (tb) { style.padding.top = *tb; style.padding.bottom = *tb; }
            if (lr) { style.padding.right = *lr; style.padding.left = *lr; }
        } else if (parts.size() == 3) {
            auto t = parse_length(parts[0]);
            auto lr = parse_length(parts[1]);
            auto b = parse_length(parts[2]);
            if (t) style.padding.top = *t;
            if (lr) { style.padding.right = *lr; style.padding.left = *lr; }
            if (b) style.padding.bottom = *b;
        } else if (parts.size() >= 4) {
            auto t = parse_length(parts[0]);
            auto r = parse_length(parts[1]);
            auto b = parse_length(parts[2]);
            auto l = parse_length(parts[3]);
            if (t) style.padding.top = *t;
            if (r) style.padding.right = *r;
            if (b) style.padding.bottom = *b;
            if (l) style.padding.left = *l;
        }
        return;
    }
    if (prop == "padding-top") {
        auto l = parse_length(value_str);
        if (l) style.padding.top = *l;
        return;
    }
    if (prop == "padding-right") {
        auto l = parse_length(value_str);
        if (l) style.padding.right = *l;
        return;
    }
    if (prop == "padding-bottom") {
        auto l = parse_length(value_str);
        if (l) style.padding.bottom = *l;
        return;
    }
    if (prop == "padding-left") {
        auto l = parse_length(value_str);
        if (l) style.padding.left = *l;
        return;
    }
    if (prop == "padding-block") {
        auto parts = split_whitespace(value_str);
        if (parts.size() == 1) {
            auto v = parse_length(parts[0]);
            if (v) { style.padding.top = *v; style.padding.bottom = *v; }
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) style.padding.top = *v1;
            if (v2) style.padding.bottom = *v2;
        }
        return;
    }
    if (prop == "padding-inline") {
        auto parts = split_whitespace(value_str);
        if (parts.size() == 1) {
            auto v = parse_length(parts[0]);
            if (v) {
                apply_inline_property(style.padding, "start", *v, style.direction);
                apply_inline_property(style.padding, "end", *v, style.direction);
            }
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) apply_inline_property(style.padding, "start", *v1, style.direction);
            if (v2) apply_inline_property(style.padding, "end", *v2, style.direction);
        }
        return;
    }
    // ---- CSS padding logical longhands ----
    if (prop == "padding-block-start") {
        auto l = parse_length(value_str);
        if (l) style.padding.top = *l;
        return;
    }
    if (prop == "padding-block-end") {
        auto l = parse_length(value_str);
        if (l) style.padding.bottom = *l;
        return;
    }
    if (prop == "padding-inline-start") {
        auto l = parse_length(value_str);
        if (l) apply_inline_property(style.padding, "start", *l, style.direction);
        return;
    }
    if (prop == "padding-inline-end") {
        auto l = parse_length(value_str);
        if (l) apply_inline_property(style.padding, "end", *l, style.direction);
        return;
    }

    // ---- Border shorthand ----
    if (prop == "border") {
        // Parse "border: 1px solid red" shorthand
        std::istringstream bss(value_str);
        std::string part;
        Length border_width = Length::px(1);
        BorderStyle border_style = BorderStyle::None;
        Color border_color = style.color;
        while (bss >> part) {
            // Try as length
            auto bw = parse_length(part);
            if (bw) { border_width = *bw; continue; }
            // Try as border style
            std::string part_lower = part;
            std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                part_lower == "double" || part_lower == "none") {
                border_style = parse_border_style_value(part_lower);
                if (part_lower == "none") border_width = Length::zero();
                continue;
            }
            // Try as color
            auto bc = parse_color(part);
            if (bc) { border_color = *bc; continue; }
        }
        style.border_top = {border_width, border_style, border_color};
        style.border_right = {border_width, border_style, border_color};
        style.border_bottom = {border_width, border_style, border_color};
        style.border_left = {border_width, border_style, border_color};
        return;
    }

    // ---- CSS border-block / border-block-start / border-block-end logical shorthands ----
    if (prop == "border-block" || prop == "border-block-start" || prop == "border-block-end") {
        std::istringstream bbs(value_str);
        std::string part;
        Length bw = Length::px(1);
        BorderStyle bs_val = BorderStyle::None;
        Color bc = style.color;
        while (bbs >> part) {
            auto bwp = parse_length(part);
            if (bwp) { bw = *bwp; continue; }
            std::string part_lower = part;
            std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                part_lower == "double" || part_lower == "none") {
                bs_val = parse_border_style_value(part_lower);
                if (part_lower == "none") bw = Length::zero();
                continue;
            }
            auto bcp = parse_color(part);
            if (bcp) { bc = *bcp; continue; }
        }
        if (prop == "border-block") {
            style.border_top = {bw, bs_val, bc};
            style.border_bottom = {bw, bs_val, bc};
        } else if (prop == "border-block-start") {
            style.border_top = {bw, bs_val, bc};
        } else {
            style.border_bottom = {bw, bs_val, bc};
        }
        return;
    }

    if (prop == "border-inline") {
        std::istringstream bis(value_str);
        std::string part;
        Length bw = Length::px(1);
        BorderStyle bs_val = BorderStyle::None;
        Color bc = style.color;
        while (bis >> part) {
            auto bwp = parse_length(part);
            if (bwp) { bw = *bwp; continue; }
            std::string part_lower = part;
            std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                part_lower == "double" || part_lower == "none") {
                bs_val = parse_border_style_value(part_lower);
                if (part_lower == "none") bw = Length::zero();
                continue;
            }
            auto bcp = parse_color(part);
            if (bcp) { bc = *bcp; continue; }
        }
        if (style.direction == Direction::Ltr) {
            style.border_left = {bw, bs_val, bc};
            style.border_right = {bw, bs_val, bc};
        } else {
            style.border_right = {bw, bs_val, bc};
            style.border_left = {bw, bs_val, bc};
        }
        return;
    }

    // ---- Border individual properties ----
    if (prop == "border-top-width") {
        auto l = parse_length(value_str);
        if (l) style.border_top.width = *l;
        return;
    }
    if (prop == "border-right-width") {
        auto l = parse_length(value_str);
        if (l) style.border_right.width = *l;
        return;
    }
    if (prop == "border-bottom-width") {
        auto l = parse_length(value_str);
        if (l) style.border_bottom.width = *l;
        return;
    }
    if (prop == "border-left-width") {
        auto l = parse_length(value_str);
        if (l) style.border_left.width = *l;
        return;
    }
    if (prop == "border-top-style") {
        style.border_top.style = parse_border_style_value(value_lower);
        return;
    }
    if (prop == "border-right-style") {
        style.border_right.style = parse_border_style_value(value_lower);
        return;
    }
    if (prop == "border-bottom-style") {
        style.border_bottom.style = parse_border_style_value(value_lower);
        return;
    }
    if (prop == "border-left-style") {
        style.border_left.style = parse_border_style_value(value_lower);
        return;
    }
    if (prop == "border-top-color") {
        auto c = parse_color(value_str);
        if (c) style.border_top.color = *c;
        return;
    }
    if (prop == "border-right-color") {
        auto c = parse_color(value_str);
        if (c) style.border_right.color = *c;
        return;
    }
    if (prop == "border-bottom-color") {
        auto c = parse_color(value_str);
        if (c) style.border_bottom.color = *c;
        return;
    }
    if (prop == "border-left-color") {
        auto c = parse_color(value_str);
        if (c) style.border_left.color = *c;
        return;
    }

    // ---- Border shorthand: border-color (1-4 values) ----
    if (prop == "border-color") {
        auto parts = split_whitespace(value_str);
        auto c1 = parse_color(parts.size() > 0 ? parts[0] : "");
        auto c2 = parse_color(parts.size() > 1 ? parts[1] : (parts.size() > 0 ? parts[0] : ""));
        auto c3 = parse_color(parts.size() > 2 ? parts[2] : (parts.size() > 0 ? parts[0] : ""));
        auto c4 = parse_color(parts.size() > 3 ? parts[3] : (parts.size() > 1 ? parts[1] : (parts.size() > 0 ? parts[0] : "")));
        if (parts.size() == 1 && c1) {
            style.border_top.color = style.border_right.color = style.border_bottom.color = style.border_left.color = *c1;
        } else if (parts.size() == 2) {
            if (c1) { style.border_top.color = *c1; style.border_bottom.color = *c1; }
            if (c2) { style.border_right.color = *c2; style.border_left.color = *c2; }
        } else if (parts.size() == 3) {
            if (c1) style.border_top.color = *c1;
            if (c2) { style.border_right.color = *c2; style.border_left.color = *c2; }
            if (c3) style.border_bottom.color = *c3;
        } else if (parts.size() >= 4) {
            if (c1) style.border_top.color = *c1;
            if (c2) style.border_right.color = *c2;
            if (c3) style.border_bottom.color = *c3;
            if (c4) style.border_left.color = *c4;
        }
        return;
    }

    // ---- Border shorthand: border-width (1-4 values) ----
    if (prop == "border-width") {
        auto parts = split_whitespace(value_str);
        if (parts.size() == 1) {
            auto w = parse_length(parts[0]);
            if (w) {
                style.border_top.width = style.border_right.width = style.border_bottom.width = style.border_left.width = *w;
            }
        } else if (parts.size() == 2) {
            auto w1 = parse_length(parts[0]);
            auto w2 = parse_length(parts[1]);
            if (w1) { style.border_top.width = *w1; style.border_bottom.width = *w1; }
            if (w2) { style.border_right.width = *w2; style.border_left.width = *w2; }
        } else if (parts.size() == 3) {
            auto w1 = parse_length(parts[0]);
            auto w2 = parse_length(parts[1]);
            auto w3 = parse_length(parts[2]);
            if (w1) style.border_top.width = *w1;
            if (w2) { style.border_right.width = *w2; style.border_left.width = *w2; }
            if (w3) style.border_bottom.width = *w3;
        } else if (parts.size() >= 4) {
            auto w1 = parse_length(parts[0]);
            auto w2 = parse_length(parts[1]);
            auto w3 = parse_length(parts[2]);
            auto w4 = parse_length(parts[3]);
            if (w1) style.border_top.width = *w1;
            if (w2) style.border_right.width = *w2;
            if (w3) style.border_bottom.width = *w3;
            if (w4) style.border_left.width = *w4;
        }
        return;
    }

    // ---- Border shorthand: border-style (1-4 values) ----
    if (prop == "border-style") {
        auto parts = split_whitespace(value_lower);
        if (parts.size() == 1) {
            auto bs = parse_border_style_value(parts[0]);
            style.border_top.style = style.border_right.style = style.border_bottom.style = style.border_left.style = bs;
        } else if (parts.size() == 2) {
            style.border_top.style = style.border_bottom.style = parse_border_style_value(parts[0]);
            style.border_right.style = style.border_left.style = parse_border_style_value(parts[1]);
        } else if (parts.size() == 3) {
            style.border_top.style = parse_border_style_value(parts[0]);
            style.border_right.style = style.border_left.style = parse_border_style_value(parts[1]);
            style.border_bottom.style = parse_border_style_value(parts[2]);
        } else if (parts.size() >= 4) {
            style.border_top.style = parse_border_style_value(parts[0]);
            style.border_right.style = parse_border_style_value(parts[1]);
            style.border_bottom.style = parse_border_style_value(parts[2]);
            style.border_left.style = parse_border_style_value(parts[3]);
        }
        return;
    }

    // ---- Border logical properties (inline-start/end) ----
    if (prop == "border-inline-start" || prop == "border-inline-end") {
        // Parse shorthand: [width] [style] [color]
        auto parts = split_whitespace(value_str);
        Length bw = Length::px(0);
        BorderStyle bs = BorderStyle::None;
        Color bc = style.color;
        for (auto& p : parts) {
            std::string pl = to_lower(p);
            if (pl == "solid" || pl == "dashed" || pl == "dotted" ||
                pl == "double" || pl == "none") {
                bs = parse_border_style_value(pl);
                continue;
            }
            auto len = parse_length(pl);
            if (len) { bw = *len; continue; }
            auto col = parse_color(p);
            if (col) { bc = *col; continue; }
        }
        if ((style.direction == Direction::Ltr && prop == "border-inline-start") ||
            (style.direction == Direction::Rtl && prop == "border-inline-end")) {
            style.border_left = {bw, bs, bc};
        } else {
            style.border_right = {bw, bs, bc};
        }
        return;
    }

    // ---- CSS border-inline-width ----
    if (prop == "border-inline-width") {
        std::istringstream iss(value_str);
        std::string v1, v2;
        iss >> v1 >> v2;
        auto l1 = parse_length(v1);
        if (l1) {
            if (style.direction == Direction::Ltr) {
                style.border_left.width = *l1;
            } else {
                style.border_right.width = *l1;
            }
        }
        auto l2 = v2.empty() ? l1 : parse_length(v2);
        if (l2) {
            if (style.direction == Direction::Ltr) {
                style.border_right.width = *l2;
            } else {
                style.border_left.width = *l2;
            }
        }
        return;
    }

    // ---- CSS border-block-width ----
    if (prop == "border-block-width") {
        std::istringstream iss(value_str);
        std::string v1, v2;
        iss >> v1 >> v2;
        auto l1 = parse_length(v1);
        if (l1) {
            style.border_top.width = *l1;
        }
        auto l2 = v2.empty() ? l1 : parse_length(v2);
        if (l2) {
            style.border_bottom.width = *l2;
        }
        return;
    }

    // ---- CSS border-inline-color ----
    if (prop == "border-inline-color") {
        auto c = parse_color(value_str);
        if (c) {
            style.border_left.color = *c;
            style.border_right.color = *c;
        }
        return;
    }

    // ---- CSS border-block-color ----
    if (prop == "border-block-color") {
        auto c = parse_color(value_str);
        if (c) {
            style.border_top.color = *c;
            style.border_bottom.color = *c;
        }
        return;
    }

    // ---- CSS border-inline-style ----
    if (prop == "border-inline-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        if (style.direction == Direction::Ltr) {
            style.border_left.style = parse_bs(value_lower);
            style.border_right.style = parse_bs(value_lower);
        } else {
            style.border_right.style = parse_bs(value_lower);
            style.border_left.style = parse_bs(value_lower);
        }
        return;
    }

    // ---- CSS border-block-style ----
    if (prop == "border-block-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        style.border_top.style = parse_bs(value_lower);
        style.border_bottom.style = parse_bs(value_lower);
        return;
    }

    // ---- CSS border logical longhand properties (width/style/color per side) ----
    if (prop == "border-block-start-width") {
        auto v = parse_length(value_str);
        if (v) { style.border_top.width = *v; }
        return;
    }
    if (prop == "border-block-end-width") {
        auto v = parse_length(value_str);
        if (v) { style.border_bottom.width = *v; }
        return;
    }
    if (prop == "border-inline-start-width") {
        auto v = parse_length(value_str);
        if (v) {
            if (style.direction == Direction::Ltr) style.border_left.width = *v;
            else style.border_right.width = *v;
        }
        return;
    }
    if (prop == "border-inline-end-width") {
        auto v = parse_length(value_str);
        if (v) {
            if (style.direction == Direction::Ltr) style.border_right.width = *v;
            else style.border_left.width = *v;
        }
        return;
    }
    if (prop == "border-block-start-color") {
        auto c = parse_color(value_str);
        if (c) style.border_top.color = *c;
        return;
    }
    if (prop == "border-block-end-color") {
        auto c = parse_color(value_str);
        if (c) style.border_bottom.color = *c;
        return;
    }
    if (prop == "border-inline-start-color") {
        auto c = parse_color(value_str);
        if (c) {
            if (style.direction == Direction::Ltr) style.border_left.color = *c;
            else style.border_right.color = *c;
        }
        return;
    }
    if (prop == "border-inline-end-color") {
        auto c = parse_color(value_str);
        if (c) {
            if (style.direction == Direction::Ltr) style.border_right.color = *c;
            else style.border_left.color = *c;
        }
        return;
    }
    if (prop == "border-block-start-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        style.border_top.style = parse_bs(value_lower);
        return;
    }
    if (prop == "border-block-end-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        style.border_bottom.style = parse_bs(value_lower);
        return;
    }
    if (prop == "border-inline-start-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        if (style.direction == Direction::Ltr) style.border_left.style = parse_bs(value_lower);
        else style.border_right.style = parse_bs(value_lower);
        return;
    }
    if (prop == "border-inline-end-style") {
        auto parse_bs = [](const std::string& v) -> BorderStyle {
            if (v=="solid") return BorderStyle::Solid;
            if (v=="dashed") return BorderStyle::Dashed;
            if (v=="dotted") return BorderStyle::Dotted;
            if (v=="double") return BorderStyle::Double;
            if (v=="groove") return BorderStyle::Groove;
            if (v=="ridge") return BorderStyle::Ridge;
            if (v=="inset") return BorderStyle::Inset;
            if (v=="outset") return BorderStyle::Outset;
            if (v=="hidden") return BorderStyle::None;
            return BorderStyle::None;
        };
        if (style.direction == Direction::Ltr) style.border_right.style = parse_bs(value_lower);
        else style.border_left.style = parse_bs(value_lower);
        return;
    }

    // ---- Border radius ----
    if (prop == "border-radius") {
        auto parts = split_whitespace(value_str);
        // Separate horizontal and vertical radii (split on '/')
        std::vector<float> h_radii, v_radii;
        bool after_slash = false;
        for (auto& p : parts) {
            if (p == "/") { after_slash = true; continue; }
            auto l = parse_length(p);
            if (l) {
                if (after_slash) v_radii.push_back(l->to_px(0));
                else h_radii.push_back(l->to_px(0));
            }
        }
        // Expand 1-4 values to per-corner using CSS shorthand rules
        // Order: TL=0, TR=1, BR=2, BL=3
        auto expand = [](const std::vector<float>& r, size_t i) -> float {
            if (r.empty()) return 0;
            if (r.size() == 1) return r[0];
            if (r.size() == 2) return r[(i == 0 || i == 2) ? 0 : 1];
            if (r.size() == 3) { const size_t m[] = {0,1,2,1}; return r[m[i]]; }
            return r[i < r.size() ? i : 0];
        };
        if (!h_radii.empty()) {
            // If elliptical (has '/'), average h and v per corner since renderer
            // doesn't support separate x/y radii; otherwise use h values directly
            bool elliptical = !v_radii.empty();
            float tl = elliptical ? (expand(h_radii, 0) + expand(v_radii, 0)) / 2.0f : expand(h_radii, 0);
            float tr = elliptical ? (expand(h_radii, 1) + expand(v_radii, 1)) / 2.0f : expand(h_radii, 1);
            float br = elliptical ? (expand(h_radii, 2) + expand(v_radii, 2)) / 2.0f : expand(h_radii, 2);
            float bl = elliptical ? (expand(h_radii, 3) + expand(v_radii, 3)) / 2.0f : expand(h_radii, 3);
            style.border_radius_tl = tl;
            style.border_radius_tr = tr;
            style.border_radius_br = br;
            style.border_radius_bl = bl;
            style.border_radius = tl;
        }
        return;
    }
    if (prop == "border-top-left-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_radius_tl = l->to_px(0);
        return;
    }
    if (prop == "border-top-right-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_radius_tr = l->to_px(0);
        return;
    }
    if (prop == "border-bottom-left-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_radius_bl = l->to_px(0);
        return;
    }
    if (prop == "border-bottom-right-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_radius_br = l->to_px(0);
        return;
    }
    if (prop == "border-start-start-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_start_start_radius = l->to_px(0);
        return;
    }
    if (prop == "border-start-end-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_start_end_radius = l->to_px(0);
        return;
    }
    if (prop == "border-end-start-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_end_start_radius = l->to_px(0);
        return;
    }
    if (prop == "border-end-end-radius") {
        auto l = parse_length(value_str);
        if (l) style.border_end_end_radius = l->to_px(0);
        return;
    }

    // ---- Positioning offsets ----
    if (prop == "top") {
        auto l = parse_length(value_str);
        if (l) style.top = *l;
        return;
    }
    if (prop == "right") {
        auto l = parse_length(value_str);
        if (l) style.right_pos = *l;
        return;
    }
    if (prop == "bottom") {
        auto l = parse_length(value_str);
        if (l) style.bottom = *l;
        return;
    }
    if (prop == "left") {
        auto l = parse_length(value_str);
        if (l) style.left_pos = *l;
        return;
    }
    if (prop == "z-index") {
        if (value_lower == "auto") {
            style.z_index = clever::layout::Z_INDEX_AUTO;
        } else {
            try {
                style.z_index = std::stoi(value_str);
            } catch (...) {}
        }
        return;
    }

    // ---- Color ----
    if (prop == "color") {
        auto c = parse_color(value_str);
        if (c) style.color = *c;
        return;
    }

    // ---- Font ----
    if (prop == "font-family") {
        std::string font_name = strip_quotes(trim(value_str));
        std::string font_lower = font_name;
        std::transform(font_lower.begin(), font_lower.end(), font_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (font_lower == "system-ui") {
            style.font_family = "-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif";
        } else if (font_lower == "ui-serif") {
            style.font_family = "Georgia, serif";
        } else if (font_lower == "ui-sans-serif") {
            style.font_family = "-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif";
        } else if (font_lower == "ui-monospace") {
            style.font_family = "Menlo, 'Courier New', monospace";
        } else {
            style.font_family = font_name;
        }
        return;
    }
    if (prop == "font-size") {
        // Keyword font sizes
        if (value_lower == "xx-small") { style.font_size = Length::px(9); return; }
        if (value_lower == "x-small") { style.font_size = Length::px(10); return; }
        if (value_lower == "small") { style.font_size = Length::px(13); return; }
        if (value_lower == "medium") { style.font_size = Length::px(16); return; }
        if (value_lower == "large") { style.font_size = Length::px(18); return; }
        if (value_lower == "x-large") { style.font_size = Length::px(24); return; }
        if (value_lower == "xx-large") { style.font_size = Length::px(32); return; }
        if (value_lower == "smaller") { style.font_size = Length::px(std::max(1.0f, parent.font_size.value * 0.833f)); return; }
        if (value_lower == "larger") { style.font_size = Length::px(parent.font_size.value * 1.2f); return; }
        auto l = parse_length(value_str);
        if (l) style.font_size = *l;
        return;
    }
    if (prop == "font-weight") {
        if (value_lower == "bold") {
            style.font_weight = 700;
        } else if (value_lower == "normal") {
            style.font_weight = 400;
        } else if (value_lower == "lighter") {
            style.font_weight = std::max(100, parent.font_weight - 100);
        } else if (value_lower == "bolder") {
            style.font_weight = std::min(900, parent.font_weight + 100);
        } else {
            try {
                style.font_weight = std::stoi(value_str);
            } catch (...) {}
        }
        return;
    }
    if (prop == "font-style") {
        if (value_lower == "italic") style.font_style = FontStyle::Italic;
        else if (value_lower == "oblique") style.font_style = FontStyle::Oblique;
        else style.font_style = FontStyle::Normal;
        return;
    }
    if (to_lower(prop) == "font-display") {
        if (value_lower == "auto") style.font_display = 0;
        else if (value_lower == "block") style.font_display = 1;
        else if (value_lower == "swap") style.font_display = 2;
        else if (value_lower == "fallback") style.font_display = 3;
        else if (value_lower == "optional") style.font_display = 4;
        return;
    }
    // ---- Font shorthand ----
    if (prop == "font") {
        // CSS font shorthand: [font-style] [font-variant] [font-weight] font-size[/line-height] font-family
        // System fonts: just set defaults and return
        if (value_lower == "caption" || value_lower == "icon" || value_lower == "menu" ||
            value_lower == "message-box" || value_lower == "small-caption" || value_lower == "status-bar") {
            style.font_size = Length::px(13);
            style.font_family = "sans-serif";
            return;
        }
        auto parts = split_whitespace_paren(value_str);
        if (parts.empty()) return;
        // Reset font sub-properties to their initial values
        style.font_style = FontStyle::Normal;
        style.font_weight = 400;
        style.font_variant = 0;
        // Walk parts: style/variant/weight come first, then size[/line-height], then family
        size_t idx = 0;
        while (idx < parts.size()) {
            std::string pl = to_lower(parts[idx]);
            if (pl == "italic") { style.font_style = FontStyle::Italic; idx++; }
            else if (pl == "oblique") { style.font_style = FontStyle::Oblique; idx++; }
            else if (pl == "bold") { style.font_weight = 700; idx++; }
            else if (pl == "bolder") { style.font_weight = 700; idx++; }
            else if (pl == "lighter") { style.font_weight = 300; idx++; }
            else if (pl == "normal") { idx++; } // could be style, variant, or weight — skip
            else if (pl == "small-caps") { style.font_variant = 1; idx++; }
            else {
                // Check for numeric weight (100-900)
                bool is_weight = false;
                if (!pl.empty() && std::isdigit(static_cast<unsigned char>(pl[0]))) {
                    try {
                        int w = std::stoi(pl);
                        if (w >= 100 && w <= 900) { style.font_weight = w; idx++; is_weight = true; }
                    } catch (...) {}
                }
                if (!is_weight) break; // Not a pre-size keyword, must be font-size
            }
        }
        // Next part: font-size (possibly with /line-height)
        if (idx < parts.size()) {
            std::string size_part = parts[idx];
            std::string lh_part;
            // Check for size/line-height syntax
            auto slash = size_part.find('/');
            if (slash != std::string::npos) {
                lh_part = size_part.substr(slash + 1);
                size_part = size_part.substr(0, slash);
            }
            // Keyword font sizes
            std::string sp_lower = to_lower(size_part);
            if (sp_lower == "xx-small") style.font_size = Length::px(9);
            else if (sp_lower == "x-small") style.font_size = Length::px(10);
            else if (sp_lower == "small") style.font_size = Length::px(13);
            else if (sp_lower == "medium") style.font_size = Length::px(16);
            else if (sp_lower == "large") style.font_size = Length::px(18);
            else if (sp_lower == "x-large") style.font_size = Length::px(24);
            else if (sp_lower == "xx-large") style.font_size = Length::px(32);
            else if (sp_lower == "smaller") style.font_size = Length::px(std::max(1.0f, parent.font_size.value * 0.833f));
            else if (sp_lower == "larger") style.font_size = Length::px(parent.font_size.value * 1.2f);
            else {
                auto fsl = parse_length(size_part);
                if (fsl) style.font_size = *fsl;
            }
            // Parse line-height if present
            if (!lh_part.empty()) {
                bool has_unit = false;
                for (char c : lh_part) {
                    if (std::isalpha(static_cast<unsigned char>(c)) || c == '%') { has_unit = true; break; }
                }
                if (!has_unit) {
                    // Unitless number: treat as multiplier of font-size
                    try {
                        float factor = std::stof(lh_part);
                        style.line_height = Length::px(factor * style.font_size.value);
                        style.line_height_unitless = factor;
                    } catch (...) {}
                } else {
                    auto lhl = parse_length(lh_part);
                    if (lhl) style.line_height = *lhl;
                    style.line_height_unitless = 0; // explicit unit
                }
            }
            idx++;
        }
        // Remaining parts are font-family (joined with spaces)
        if (idx < parts.size()) {
            std::string family;
            for (size_t i = idx; i < parts.size(); i++) {
                if (!family.empty()) family += " ";
                family += parts[i];
            }
            // Strip quotes and trailing commas
            std::string clean_family;
            for (char c : family) {
                if (c != '\'' && c != '"') clean_family += c;
            }
            style.font_family = clean_family;
        }
        return;
    }
    if (prop == "line-height") {
        if (value_lower == "normal") {
            style.line_height = Length::px(1.2f * style.font_size.value);
            style.line_height_unitless = 1.2f;
        } else if (value_str.find('%') != std::string::npos) {
            // Percentage: "150%" -> 1.5x font-size (NOT unitless — inherits computed value)
            try {
                float pct = std::stof(value_str);
                style.line_height = Length::px((pct / 100.0f) * style.font_size.value);
                style.line_height_unitless = 0; // explicit unit — NOT unitless
            } catch (...) {}
        } else if (value_str.find("em") != std::string::npos) {
            // em units: "1.5em" -> 1.5x font-size (NOT unitless)
            try {
                float em = std::stof(value_str);
                style.line_height = Length::px(em * style.font_size.value);
                style.line_height_unitless = 0;
            } catch (...) {}
        } else if (value_str.find("px") != std::string::npos) {
            // px: "24px" -> absolute pixel value (NOT unitless)
            auto l = parse_length(value_str);
            if (l) style.line_height = *l;
            style.line_height_unitless = 0;
        } else {
            // Unitless number: "1.5" -> 1.5x font-size multiplier
            // CSS spec: unitless values are inherited as the NUMBER, not computed px
            try {
                float factor = std::stof(value_str);
                style.line_height = Length::px(factor * style.font_size.value);
                style.line_height_unitless = factor;
            } catch (...) {}
        }
        return;
    }

    // ---- Text ----
    if (prop == "text-align") {
        if (value_lower == "left" || value_lower == "start") style.text_align = TextAlign::Left;
        else if (value_lower == "right" || value_lower == "end") style.text_align = TextAlign::Right;
        else if (value_lower == "center") style.text_align = TextAlign::Center;
        else if (value_lower == "-webkit-center") style.text_align = TextAlign::WebkitCenter;
        else if (value_lower == "justify") style.text_align = TextAlign::Justify;
        else if (value_lower == "-webkit-left") style.text_align = TextAlign::Left;
        else if (value_lower == "-webkit-right") style.text_align = TextAlign::Right;
        return;
    }
    if (prop == "text-align-last") {
        if (value_lower == "left" || value_lower == "start") style.text_align_last = 1;
        else if (value_lower == "right" || value_lower == "end") style.text_align_last = 2;
        else if (value_lower == "center") style.text_align_last = 3;
        else if (value_lower == "justify") style.text_align_last = 4;
        else style.text_align_last = 0; // auto
        return;
    }
    if (prop == "text-indent") {
        auto l = parse_length(value_str);
        if (l) style.text_indent = *l;
        return;
    }
    if (prop == "vertical-align") {
        if (value_lower == "top") style.vertical_align = VerticalAlign::Top;
        else if (value_lower == "middle") style.vertical_align = VerticalAlign::Middle;
        else if (value_lower == "bottom") style.vertical_align = VerticalAlign::Bottom;
        else if (value_lower == "text-top") style.vertical_align = VerticalAlign::TextTop;
        else if (value_lower == "text-bottom") style.vertical_align = VerticalAlign::TextBottom;
        else if (value_lower == "baseline") style.vertical_align = VerticalAlign::Baseline;
        else {
            // Try to parse as length or percentage value
            auto l = parse_length(value_str);
            if (l) {
                style.vertical_align = VerticalAlign::Baseline;
                style.vertical_align_offset = l->to_px();
            } else {
                style.vertical_align = VerticalAlign::Baseline;
            }
        }
        return;
    }
    if (prop == "text-decoration-line") {
        auto parts = split_whitespace_paren(value_lower);
        style.text_decoration = TextDecoration::None;
        style.text_decoration_bits = 0;
        for (const auto& tok : parts) {
            if (tok == "none") {
                style.text_decoration = TextDecoration::None;
                style.text_decoration_bits = 0;
            } else if (tok == "underline") {
                style.text_decoration = TextDecoration::Underline;
                style.text_decoration_bits |= 1;
            } else if (tok == "overline") {
                style.text_decoration = TextDecoration::Overline;
                style.text_decoration_bits |= 2;
            } else if (tok == "line-through") {
                style.text_decoration = TextDecoration::LineThrough;
                style.text_decoration_bits |= 4;
            }
        }
        return;
    }
    if (prop == "text-decoration") {
        auto parts = split_whitespace_paren(value_lower);
        // Shorthand reset: unspecified sub-properties return to initial values.
        style.text_decoration = TextDecoration::None;
        style.text_decoration_bits = 0;
        style.text_decoration_color = Color::transparent(); // currentColor sentinel
        style.text_decoration_style = TextDecorationStyle::Solid;
        style.text_decoration_thickness = 0;

        for (const auto& tok : parts) {
            if (tok == "none") {
                style.text_decoration = TextDecoration::None;
                style.text_decoration_bits = 0;
            } else if (tok == "underline") {
                style.text_decoration = TextDecoration::Underline;
                style.text_decoration_bits |= 1;
            } else if (tok == "overline") {
                style.text_decoration = TextDecoration::Overline;
                style.text_decoration_bits |= 2;
            } else if (tok == "line-through") {
                style.text_decoration = TextDecoration::LineThrough;
                style.text_decoration_bits |= 4;
            } else if (tok == "solid") {
                style.text_decoration_style = TextDecorationStyle::Solid;
            } else if (tok == "dashed") {
                style.text_decoration_style = TextDecorationStyle::Dashed;
            } else if (tok == "dotted") {
                style.text_decoration_style = TextDecorationStyle::Dotted;
            } else if (tok == "wavy") {
                style.text_decoration_style = TextDecorationStyle::Wavy;
            } else if (tok == "double") {
                style.text_decoration_style = TextDecorationStyle::Double;
            } else {
                auto l = parse_length(tok);
                if (l) {
                    style.text_decoration_thickness = l->to_px(0);
                } else {
                    auto c = parse_color(tok);
                    if (c) style.text_decoration_color = *c;
                }
            }
        }
        return;
    }
    if (prop == "text-decoration-color") {
        auto c = parse_color(value_lower);
        if (c) style.text_decoration_color = *c;
        return;
    }
    if (prop == "text-decoration-style") {
        if (value_lower == "solid") style.text_decoration_style = TextDecorationStyle::Solid;
        else if (value_lower == "dashed") style.text_decoration_style = TextDecorationStyle::Dashed;
        else if (value_lower == "dotted") style.text_decoration_style = TextDecorationStyle::Dotted;
        else if (value_lower == "wavy") style.text_decoration_style = TextDecorationStyle::Wavy;
        else if (value_lower == "double") style.text_decoration_style = TextDecorationStyle::Double;
        return;
    }
    if (prop == "text-decoration-thickness") {
        auto l = parse_length(value_str);
        if (l) style.text_decoration_thickness = l->to_px(0);
        return;
    }
    if (prop == "text-transform") {
        if (value_lower == "capitalize") style.text_transform = TextTransform::Capitalize;
        else if (value_lower == "uppercase") style.text_transform = TextTransform::Uppercase;
        else if (value_lower == "lowercase") style.text_transform = TextTransform::Lowercase;
        else style.text_transform = TextTransform::None;
        return;
    }
    if (prop == "white-space") {
        if (value_lower == "nowrap") style.white_space = WhiteSpace::NoWrap;
        else if (value_lower == "pre") style.white_space = WhiteSpace::Pre;
        else if (value_lower == "pre-wrap") style.white_space = WhiteSpace::PreWrap;
        else if (value_lower == "pre-line") style.white_space = WhiteSpace::PreLine;
        else if (value_lower == "break-spaces") style.white_space = WhiteSpace::BreakSpaces;
        else style.white_space = WhiteSpace::Normal;
        return;
    }
    if (prop == "text-overflow") {
        if (value_lower == "ellipsis") style.text_overflow = TextOverflow::Ellipsis;
        else if (value_lower == "fade") style.text_overflow = TextOverflow::Fade;
        else style.text_overflow = TextOverflow::Clip;
        return;
    }
    if (prop == "word-break") {
        if (value_lower == "break-all") style.word_break = 1;
        else if (value_lower == "keep-all") style.word_break = 2;
        else style.word_break = 0; // normal
        return;
    }
    if (prop == "overflow-wrap" || prop == "word-wrap") {
        if (value_lower == "break-word") style.overflow_wrap = 1;
        else if (value_lower == "anywhere") style.overflow_wrap = 2;
        else style.overflow_wrap = 0; // normal
        return;
    }
    if (prop == "text-wrap" || prop == "text-wrap-mode") {
        if (value_lower == "nowrap") style.text_wrap = 1;
        else if (value_lower == "balance") style.text_wrap = 2;
        else if (value_lower == "pretty") style.text_wrap = 3;
        else if (value_lower == "stable") style.text_wrap = 4;
        else style.text_wrap = 0; // wrap
        return;
    }
    if (prop == "text-wrap-style") {
        if (value_lower == "balance") style.text_wrap = 2;
        else if (value_lower == "pretty") style.text_wrap = 3;
        else if (value_lower == "stable") style.text_wrap = 4;
        return;
    }
    if (prop == "white-space-collapse") {
        if (value_lower == "collapse") style.white_space_collapse = 0;
        else if (value_lower == "preserve") style.white_space_collapse = 1;
        else if (value_lower == "preserve-breaks") style.white_space_collapse = 2;
        else if (value_lower == "break-spaces") style.white_space_collapse = 3;
        return;
    }
    if (prop == "line-break") {
        if (value_lower == "auto") style.line_break = 0;
        else if (value_lower == "loose") style.line_break = 1;
        else if (value_lower == "normal") style.line_break = 2;
        else if (value_lower == "strict") style.line_break = 3;
        else if (value_lower == "anywhere") style.line_break = 4;
        return;
    }
    if (prop == "orphans") {
        try { style.orphans = std::stoi(value_str); } catch (...) {}
        return;
    }
    if (prop == "widows") {
        try { style.widows = std::stoi(value_str); } catch (...) {}
        return;
    }
    if (prop == "column-span") {
        if (value_lower == "all") style.column_span = 1;
        else style.column_span = 0;
        return;
    }
    if (prop == "break-before") {
        if (value_lower == "auto") style.break_before = 0;
        else if (value_lower == "avoid") style.break_before = 1;
        else if (value_lower == "always") style.break_before = 2;
        else if (value_lower == "page") style.break_before = 3;
        else if (value_lower == "column") style.break_before = 4;
        else if (value_lower == "region") style.break_before = 5;
        return;
    }
    if (prop == "break-after") {
        if (value_lower == "auto") style.break_after = 0;
        else if (value_lower == "avoid") style.break_after = 1;
        else if (value_lower == "always") style.break_after = 2;
        else if (value_lower == "page") style.break_after = 3;
        else if (value_lower == "column") style.break_after = 4;
        else if (value_lower == "region") style.break_after = 5;
        return;
    }
    if (prop == "break-inside") {
        if (value_lower == "auto") style.break_inside = 0;
        else if (value_lower == "avoid") style.break_inside = 1;
        else if (value_lower == "avoid-page") style.break_inside = 2;
        else if (value_lower == "avoid-column") style.break_inside = 3;
        else if (value_lower == "avoid-region") style.break_inside = 4;
        return;
    }
    if (prop == "page") {
        style.page = value_lower;
        return;
    }
    if (prop == "page-break-before") {
        if (value_lower == "auto") style.page_break_before = 0;
        else if (value_lower == "always") style.page_break_before = 1;
        else if (value_lower == "avoid") style.page_break_before = 2;
        else if (value_lower == "left") style.page_break_before = 3;
        else if (value_lower == "right") style.page_break_before = 4;
        return;
    }
    if (prop == "page-break-after") {
        if (value_lower == "auto") style.page_break_after = 0;
        else if (value_lower == "always") style.page_break_after = 1;
        else if (value_lower == "avoid") style.page_break_after = 2;
        else if (value_lower == "left") style.page_break_after = 3;
        else if (value_lower == "right") style.page_break_after = 4;
        return;
    }
    if (prop == "page-break-inside") {
        if (value_lower == "auto") style.page_break_inside = 0;
        else if (value_lower == "avoid") style.page_break_inside = 1;
        return;
    }
    if (prop == "background-clip" || prop == "-webkit-background-clip") {
        if (value_lower == "border-box") style.background_clip = 0;
        else if (value_lower == "padding-box") style.background_clip = 1;
        else if (value_lower == "content-box") style.background_clip = 2;
        else if (value_lower == "text") style.background_clip = 3;
        return;
    }
    if (prop == "background-origin") {
        if (value_lower == "padding-box") style.background_origin = 0;
        else if (value_lower == "border-box") style.background_origin = 1;
        else if (value_lower == "content-box") style.background_origin = 2;
        return;
    }
    if (prop == "background-blend-mode") {
        if (value_lower == "normal") style.background_blend_mode = 0;
        else if (value_lower == "multiply") style.background_blend_mode = 1;
        else if (value_lower == "screen") style.background_blend_mode = 2;
        else if (value_lower == "overlay") style.background_blend_mode = 3;
        else if (value_lower == "darken") style.background_blend_mode = 4;
        else if (value_lower == "lighten") style.background_blend_mode = 5;
        return;
    }
    if (prop == "background-attachment") {
        if (value_lower == "scroll") style.background_attachment = 0;
        else if (value_lower == "fixed") style.background_attachment = 1;
        else if (value_lower == "local") style.background_attachment = 2;
        return;
    }
    if (prop == "unicode-bidi") {
        if (value_lower == "normal") style.unicode_bidi = 0;
        else if (value_lower == "embed") style.unicode_bidi = 1;
        else if (value_lower == "bidi-override") style.unicode_bidi = 2;
        else if (value_lower == "isolate") style.unicode_bidi = 3;
        else if (value_lower == "isolate-override") style.unicode_bidi = 4;
        else if (value_lower == "plaintext") style.unicode_bidi = 5;
        return;
    }
    if (prop == "letter-spacing") {
        if (value_lower == "normal") {
            style.letter_spacing = Length::zero();
        } else {
            auto l = parse_length(value_str);
            if (l) style.letter_spacing = *l;
        }
        return;
    }

    if (prop == "word-spacing") {
        if (value_lower == "normal") {
            style.word_spacing = Length::zero();
        } else {
            auto l = parse_length(value_str);
            if (l) style.word_spacing = *l;
        }
        return;
    }

    // ---- Visual ----
    if (prop == "background-color") {
        auto c = parse_color(value_str);
        if (c) style.background_color = *c;
        return;
    }
    if (prop == "background" || prop == "background-image") {
        // Multiple backgrounds support: split by top-level commas, use the
        // last layer as the primary (CSS spec: last listed = bottom-most painted).
        auto bg_layers = split_background_layers(value_str);
        std::string bg_value = bg_layers.empty() ? value_str : bg_layers.back();

        if (bg_value.find("linear-gradient") != std::string::npos) {
            // Parse linear-gradient in the cascade
            auto start_pos = bg_value.find("linear-gradient(");
            if (start_pos != std::string::npos) {
                auto inner_start = start_pos + 16;
                auto inner_end = bg_value.rfind(')');
                if (inner_end != std::string::npos && inner_end > inner_start) {
                    std::string inner = bg_value.substr(inner_start, inner_end - inner_start);
                    // Split on commas first; if no commas found (CSS tokenizer strips them),
                    // fall back to whitespace splitting
                    std::vector<std::string> parts;
                    if (inner.find(',') != std::string::npos) {
                        std::string cur;
                        int pd = 0;
                        for (char ch : inner) {
                            if (ch == '(') pd++;
                            else if (ch == ')') pd--;
                            if (ch == ',' && pd == 0) {
                                parts.push_back(trim(cur));
                                cur.clear();
                            } else {
                                cur += ch;
                            }
                        }
                        if (!cur.empty()) parts.push_back(trim(cur));
                    } else {
                        parts = split_whitespace(inner);
                    }

                    if (parts.size() >= 2) {
                        float angle = 180.0f;
                        size_t color_start = 0;
                        std::string first_lower = parts[0];
                        std::transform(first_lower.begin(), first_lower.end(), first_lower.begin(), ::tolower);
                        if (first_lower.find("deg") != std::string::npos) {
                            try { angle = std::stof(first_lower); } catch (...) {}
                            color_start = 1;
                        } else if (first_lower == "to") {
                            // "to" and direction are separate tokens when commas are stripped
                            if (parts.size() > 1) {
                                std::string dir = to_lower(trim(parts[1]));
                                if (dir == "top") angle = 0.0f;
                                else if (dir == "right") angle = 90.0f;
                                else if (dir == "bottom") angle = 180.0f;
                                else if (dir == "left") angle = 270.0f;
                                color_start = 2;
                            }
                        } else if (first_lower.find("to ") == 0) {
                            std::string dir = trim(first_lower.substr(3));
                            if (dir == "top") angle = 0.0f;
                            else if (dir == "right") angle = 90.0f;
                            else if (dir == "bottom") angle = 180.0f;
                            else if (dir == "left") angle = 270.0f;
                            color_start = 1;
                        }

                        bool is_rep = bg_value.find("repeating-linear-gradient") != std::string::npos;
                        style.gradient_type = is_rep ? 4 : 1; // repeating-linear or linear
                        style.gradient_angle = angle;
                        style.gradient_stops.clear();
                        size_t num_colors = parts.size() - color_start;
                        for (size_t i = color_start; i < parts.size(); i++) {
                            std::string stop_part = trim(parts[i]);
                            float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);
                            auto cc = parse_color(stop_part);
                            if (cc) {
                                uint32_t argb = (static_cast<uint32_t>(cc->a) << 24) |
                                                (static_cast<uint32_t>(cc->r) << 16) |
                                                (static_cast<uint32_t>(cc->g) << 8) |
                                                static_cast<uint32_t>(cc->b);
                                style.gradient_stops.push_back({argb, pos});
                            } else {
                                // Try "color position" format, e.g. "red 20%" or "#ff0000 50px"
                                auto sp = stop_part.rfind(' ');
                                if (sp != std::string::npos) {
                                    auto color_str = trim(stop_part.substr(0, sp));
                                    auto pos_str = trim(stop_part.substr(sp + 1));
                                    auto cc2 = parse_color(color_str);
                                    if (cc2) {
                                        if (!pos_str.empty() && pos_str.back() == '%') {
                                            try { pos = std::stof(pos_str) / 100.0f; } catch (...) {}
                                        } else {
                                            auto l = parse_length(pos_str);
                                            if (l) pos = l->to_px() / 100.0f;
                                        }
                                        uint32_t argb = (static_cast<uint32_t>(cc2->a) << 24) |
                                                        (static_cast<uint32_t>(cc2->r) << 16) |
                                                        (static_cast<uint32_t>(cc2->g) << 8) |
                                                        static_cast<uint32_t>(cc2->b);
                                        style.gradient_stops.push_back({argb, pos});
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (bg_value.find("radial-gradient") != std::string::npos) {
            // Parse radial-gradient in the cascade
            auto start_pos = bg_value.find("radial-gradient(");
            if (start_pos != std::string::npos) {
                auto inner_start = start_pos + 16;
                auto inner_end = bg_value.rfind(')');
                if (inner_end != std::string::npos && inner_end > inner_start) {
                    std::string inner = bg_value.substr(inner_start, inner_end - inner_start);
                    // Split on commas first; if no commas found (CSS tokenizer strips them),
                    // fall back to whitespace splitting
                    std::vector<std::string> parts;
                    if (inner.find(',') != std::string::npos) {
                        std::string cur;
                        int pd = 0;
                        for (char ch : inner) {
                            if (ch == '(') pd++;
                            else if (ch == ')') pd--;
                            if (ch == ',' && pd == 0) {
                                parts.push_back(trim(cur));
                                cur.clear();
                            } else {
                                cur += ch;
                            }
                        }
                        if (!cur.empty()) parts.push_back(trim(cur));
                    } else {
                        parts = split_whitespace(inner);
                    }

                    if (parts.size() >= 2) {
                        int radial_shape = 0; // default: ellipse
                        size_t color_start = 0;
                        std::string first_lower = parts[0];
                        std::transform(first_lower.begin(), first_lower.end(), first_lower.begin(), ::tolower);
                        if (first_lower == "circle") {
                            radial_shape = 1;
                            color_start = 1;
                        } else if (first_lower == "ellipse") {
                            radial_shape = 0;
                            color_start = 1;
                        }

                        size_t num_colors = parts.size() - color_start;
                        if (num_colors >= 2) {
                            bool is_rep_r = bg_value.find("repeating-radial-gradient") != std::string::npos;
                            style.gradient_type = is_rep_r ? 5 : 2; // repeating-radial or radial
                            style.radial_shape = radial_shape;
                            style.gradient_stops.clear();
                            for (size_t i = color_start; i < parts.size(); i++) {
                                std::string stop_part = trim(parts[i]);
                                float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);
                                auto cc = parse_color(stop_part);
                                if (cc) {
                                    uint32_t argb = (static_cast<uint32_t>(cc->a) << 24) |
                                                    (static_cast<uint32_t>(cc->r) << 16) |
                                                    (static_cast<uint32_t>(cc->g) << 8) |
                                                    static_cast<uint32_t>(cc->b);
                                    style.gradient_stops.push_back({argb, pos});
                                } else {
                                    // Try "color position" format, e.g. "red 20%" or "#ff0000 50px"
                                    auto sp = stop_part.rfind(' ');
                                    if (sp != std::string::npos) {
                                        auto color_str = trim(stop_part.substr(0, sp));
                                        auto pos_str = trim(stop_part.substr(sp + 1));
                                        auto cc2 = parse_color(color_str);
                                        if (cc2) {
                                            if (!pos_str.empty() && pos_str.back() == '%') {
                                                try { pos = std::stof(pos_str) / 100.0f; } catch (...) {}
                                            } else {
                                                auto l = parse_length(pos_str);
                                                if (l) pos = l->to_px() / 100.0f;
                                            }
                                            uint32_t argb = (static_cast<uint32_t>(cc2->a) << 24) |
                                                            (static_cast<uint32_t>(cc2->r) << 16) |
                                                            (static_cast<uint32_t>(cc2->g) << 8) |
                                                            static_cast<uint32_t>(cc2->b);
                                            style.gradient_stops.push_back({argb, pos});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (bg_value.find("conic-gradient") != std::string::npos) {
            // Parse conic-gradient in the cascade
            auto start_pos = bg_value.find("conic-gradient(");
            if (start_pos != std::string::npos) {
                auto inner_start = start_pos + 15;
                auto inner_end = bg_value.rfind(')');
                if (inner_end != std::string::npos && inner_end > inner_start) {
                    std::string inner = bg_value.substr(inner_start, inner_end - inner_start);
                    std::vector<std::string> parts;
                    if (inner.find(',') != std::string::npos) {
                        std::string cur;
                        int pd = 0;
                        for (char ch : inner) {
                            if (ch == '(') pd++;
                            else if (ch == ')') pd--;
                            if (ch == ',' && pd == 0) { parts.push_back(trim(cur)); cur.clear(); }
                            else cur += ch;
                        }
                        if (!cur.empty()) parts.push_back(trim(cur));
                    } else {
                        parts = split_whitespace(inner);
                    }

                    if (parts.size() >= 2) {
                        float from_angle = 0;
                        size_t color_start = 0;
                        std::string first_lower = parts[0];
                        std::transform(first_lower.begin(), first_lower.end(), first_lower.begin(), ::tolower);
                        if (first_lower.find("from ") == 0) {
                            std::string angle_str = first_lower.substr(5);
                            auto at_pos = angle_str.find(" at ");
                            if (at_pos != std::string::npos) angle_str = angle_str.substr(0, at_pos);
                            if (angle_str.find("deg") != std::string::npos)
                                try { from_angle = std::stof(angle_str); } catch (...) {}
                            else if (angle_str.find("turn") != std::string::npos)
                                try { from_angle = std::stof(angle_str) * 360.0f; } catch (...) {}
                            color_start = 1;
                        }

                        size_t num_colors = parts.size() - color_start;
                        if (num_colors >= 2) {
                            bool is_rep_c = bg_value.find("repeating-conic-gradient") != std::string::npos;
                            style.gradient_type = is_rep_c ? 6 : 3; // repeating-conic or conic
                            style.gradient_angle = from_angle;
                            style.gradient_stops.clear();
                            for (size_t i = color_start; i < parts.size(); i++) {
                                std::string stop_part = trim(parts[i]);
                                float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);
                                auto cc = parse_color(stop_part);
                                if (cc) {
                                    uint32_t argb = (static_cast<uint32_t>(cc->a) << 24) |
                                                    (static_cast<uint32_t>(cc->r) << 16) |
                                                    (static_cast<uint32_t>(cc->g) << 8) |
                                                    static_cast<uint32_t>(cc->b);
                                    style.gradient_stops.push_back({argb, pos});
                                } else {
                                    // Try "color position" format, e.g. "red 20%" or "#ff0000 50px"
                                    auto sp = stop_part.rfind(' ');
                                    if (sp != std::string::npos) {
                                        auto color_str = trim(stop_part.substr(0, sp));
                                        auto pos_str = trim(stop_part.substr(sp + 1));
                                        auto cc2 = parse_color(color_str);
                                        if (cc2) {
                                            if (!pos_str.empty() && pos_str.back() == '%') {
                                                try { pos = std::stof(pos_str) / 100.0f; } catch (...) {}
                                            } else {
                                                auto l = parse_length(pos_str);
                                                if (l) pos = l->to_px() / 100.0f;
                                            }
                                            uint32_t argb = (static_cast<uint32_t>(cc2->a) << 24) |
                                                            (static_cast<uint32_t>(cc2->r) << 16) |
                                                            (static_cast<uint32_t>(cc2->g) << 8) |
                                                            static_cast<uint32_t>(cc2->b);
                                            style.gradient_stops.push_back({argb, pos});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            auto c = parse_color(bg_value);
            if (c) style.background_color = *c;
        }
        return;
    }
    if (prop == "background-size") {
        if (value_lower == "cover") style.background_size = 1;
        else if (value_lower == "contain") style.background_size = 2;
        else if (value_lower == "auto") style.background_size = 0;
        else {
            style.background_size = 3;
            auto parts = split_whitespace(value_str);
            if (parts.size() >= 2) {
                auto lw = parse_length(parts[0]);
                auto lh = parse_length(parts[1]);
                if (lw) style.bg_size_width = lw->to_px(0);
                if (lh) style.bg_size_height = lh->to_px(0);
            } else {
                auto lw = parse_length(value_str);
                if (lw) { style.bg_size_width = lw->to_px(0); style.bg_size_height = 0; }
            }
        }
        return;
    }
    if (prop == "background-repeat") {
        if (value_lower == "repeat") style.background_repeat = 0;
        else if (value_lower == "repeat-x") style.background_repeat = 1;
        else if (value_lower == "repeat-y") style.background_repeat = 2;
        else if (value_lower == "no-repeat") style.background_repeat = 3;
        return;
    }
    if (prop == "background-position") {
        auto parts = split_whitespace(value_lower);
        std::string x_part = parts.size() > 0 ? parts[0] : value_lower;
        std::string y_part = parts.size() > 1 ? parts[1] : "center";
        if (x_part == "left") style.background_position_x = 0;
        else if (x_part == "center") style.background_position_x = 1;
        else if (x_part == "right") style.background_position_x = 2;
        else {
            auto lx = parse_length(x_part);
            if (lx) style.background_position_x = static_cast<int>(lx->to_px(0));
        }
        if (y_part == "top") style.background_position_y = 0;
        else if (y_part == "center") style.background_position_y = 1;
        else if (y_part == "bottom") style.background_position_y = 2;
        else {
            auto ly = parse_length(y_part);
            if (ly) style.background_position_y = static_cast<int>(ly->to_px(0));
        }
        return;
    }
    if (prop == "background-position-x") {
        if (value_lower == "left") style.background_position_x = 0;
        else if (value_lower == "center") style.background_position_x = 1;
        else if (value_lower == "right") style.background_position_x = 2;
        else {
            auto lx = parse_length(value_str);
            if (lx) style.background_position_x = static_cast<int>(lx->to_px(0));
        }
        return;
    }
    if (prop == "background-position-y") {
        if (value_lower == "top") style.background_position_y = 0;
        else if (value_lower == "center") style.background_position_y = 1;
        else if (value_lower == "bottom") style.background_position_y = 2;
        else {
            auto ly = parse_length(value_str);
            if (ly) style.background_position_y = static_cast<int>(ly->to_px(0));
        }
        return;
    }
    if (prop == "opacity") {
        try {
            style.opacity = std::clamp(std::stof(value_str), 0.0f, 1.0f);
        } catch (...) {}
        return;
    }
    if (prop == "visibility") {
        if (value_lower == "hidden") style.visibility = Visibility::Hidden;
        else if (value_lower == "collapse") style.visibility = Visibility::Collapse;
        else style.visibility = Visibility::Visible;
        return;
    }

    if (prop == "box-shadow") {
        if (value_lower == "none") {
            style.shadow_color = Color::transparent();
            style.shadow_offset_x = 0;
            style.shadow_offset_y = 0;
            style.shadow_blur = 0;
            style.shadow_spread = 0;
            style.shadow_inset = false;
            style.box_shadows.clear();
        } else {
            // Split on commas (respecting parentheses for rgb()/hsl() etc.)
            style.box_shadows.clear();
            std::vector<std::string> shadow_strs;
            {
                size_t start = 0;
                int paren_depth = 0;
                for (size_t i = 0; i < value_str.size(); ++i) {
                    if (value_str[i] == '(') paren_depth++;
                    else if (value_str[i] == ')') paren_depth--;
                    else if (value_str[i] == ',' && paren_depth == 0) {
                        shadow_strs.push_back(value_str.substr(start, i - start));
                        start = i + 1;
                    }
                }
                shadow_strs.push_back(value_str.substr(start));
            }
            for (auto& ss : shadow_strs) {
                // Trim whitespace
                size_t s = ss.find_first_not_of(" \t");
                size_t e = ss.find_last_not_of(" \t");
                if (s == std::string::npos) continue;
                std::string trimmed = ss.substr(s, e - s + 1);

                ComputedStyle::BoxShadowEntry entry;
                // Parse: [inset] offset-x offset-y [blur] [spread] [color]
                std::istringstream iss(trimmed);
                std::vector<std::string> parts;
                std::string word;
                while (iss >> word) parts.push_back(word);

                std::vector<std::string> lengths;
                std::string color_str;
                for (auto& p : parts) {
                    std::string pl = p;
                    for (auto& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (pl == "inset") {
                        entry.inset = true;
                    } else if (parse_length(p)) {
                        lengths.push_back(p);
                    } else {
                        if (color_str.empty()) color_str = p;
                        else color_str += " " + p;
                    }
                }
                if (lengths.size() >= 2) {
                    auto ox = parse_length(lengths[0]);
                    auto oy = parse_length(lengths[1]);
                    if (ox) entry.offset_x = ox->to_px(0);
                    if (oy) entry.offset_y = oy->to_px(0);
                    if (lengths.size() >= 3) {
                        auto b = parse_length(lengths[2]);
                        if (b) entry.blur = b->to_px(0);
                    }
                    if (lengths.size() >= 4) {
                        auto sp = parse_length(lengths[3]);
                        if (sp) entry.spread = sp->to_px(0);
                    }
                }
                if (!color_str.empty()) {
                    auto c = parse_color(color_str);
                    if (c) entry.color = *c;
                    else entry.color = {0, 0, 0, 128};
                } else {
                    entry.color = {0, 0, 0, 128};
                }
                style.box_shadows.push_back(entry);
            }
            // Also set legacy single-shadow fields from first entry
            if (!style.box_shadows.empty()) {
                auto& first = style.box_shadows[0];
                style.shadow_offset_x = first.offset_x;
                style.shadow_offset_y = first.offset_y;
                style.shadow_blur = first.blur;
                style.shadow_spread = first.spread;
                style.shadow_color = first.color;
                style.shadow_inset = first.inset;
            }
        }
        return;
    }

    if (prop == "text-shadow") {
        if (value_lower == "none") {
            style.text_shadow_color = Color::transparent();
            style.text_shadow_offset_x = 0;
            style.text_shadow_offset_y = 0;
            style.text_shadow_blur = 0;
        } else {
            std::istringstream iss2(value_str);
            std::vector<std::string> parts;
            std::string word;
            while (iss2 >> word) parts.push_back(word);
            if (parts.size() >= 3) {
                auto ox = parse_length(parts[0]);
                auto oy = parse_length(parts[1]);
                if (ox) style.text_shadow_offset_x = ox->to_px(0);
                if (oy) style.text_shadow_offset_y = oy->to_px(0);
                auto blur = parse_length(parts[2]);
                if (blur) {
                    style.text_shadow_blur = blur->to_px(0);
                    if (parts.size() >= 4) {
                        auto c = parse_color(parts[3]);
                        if (c) style.text_shadow_color = *c;
                    } else {
                        style.text_shadow_color = {0, 0, 0, 128};
                    }
                } else {
                    auto c = parse_color(parts[2]);
                    if (c) style.text_shadow_color = *c;
                    else style.text_shadow_color = {0, 0, 0, 128};
                }
            }
        }
        return;
    }

    // ---- Overflow ----
    if (prop == "overflow") {
        auto parts = split_whitespace(value_lower);
        if (parts.size() >= 2) {
            style.overflow_x = parse_overflow_value(parts[0]);
            style.overflow_y = parse_overflow_value(parts[1]);
        } else {
            auto ov = parse_overflow_value(value_lower);
            style.overflow_x = ov;
            style.overflow_y = ov;
        }
        return;
    }
    if (prop == "overflow-x") {
        style.overflow_x = parse_overflow_value(value_lower);
        return;
    }
    if (prop == "overflow-y") {
        style.overflow_y = parse_overflow_value(value_lower);
        return;
    }

    // ---- Legacy -webkit-box-orient → flex-direction ----
    if (prop == "-webkit-box-orient") {
        if (value_lower == "vertical") style.flex_direction = FlexDirection::Column;
        else if (value_lower == "horizontal") style.flex_direction = FlexDirection::Row;
        return;
    }

    // ---- Flexbox ----
    if (prop == "flex-direction") {
        if (value_lower == "row") style.flex_direction = FlexDirection::Row;
        else if (value_lower == "row-reverse") style.flex_direction = FlexDirection::RowReverse;
        else if (value_lower == "column") style.flex_direction = FlexDirection::Column;
        else if (value_lower == "column-reverse") style.flex_direction = FlexDirection::ColumnReverse;
        return;
    }
    if (prop == "flex-wrap") {
        if (value_lower == "nowrap") style.flex_wrap = FlexWrap::NoWrap;
        else if (value_lower == "wrap") style.flex_wrap = FlexWrap::Wrap;
        else if (value_lower == "wrap-reverse") style.flex_wrap = FlexWrap::WrapReverse;
        return;
    }
    if (prop == "flex-flow") {
        // Shorthand: flex-flow: <direction> <wrap>
        std::istringstream iss(value_lower);
        std::string part;
        while (iss >> part) {
            if (part == "row") style.flex_direction = FlexDirection::Row;
            else if (part == "column") style.flex_direction = FlexDirection::Column;
            else if (part == "row-reverse") style.flex_direction = FlexDirection::RowReverse;
            else if (part == "column-reverse") style.flex_direction = FlexDirection::ColumnReverse;
            else if (part == "wrap") style.flex_wrap = FlexWrap::Wrap;
            else if (part == "wrap-reverse") style.flex_wrap = FlexWrap::WrapReverse;
            else if (part == "nowrap") style.flex_wrap = FlexWrap::NoWrap;
        }
        return;
    }
    if (prop == "place-items") {
        // Shorthand: place-items: <align-items> [<justify-items>]
        auto parts = split_whitespace(value_lower);
        auto parse_align_items_val = [](const std::string& s) -> AlignItems {
            if (s == "center") return AlignItems::Center;
            if (s == "flex-start" || s == "start") return AlignItems::FlexStart;
            if (s == "flex-end" || s == "end") return AlignItems::FlexEnd;
            if (s == "baseline") return AlignItems::Baseline;
            return AlignItems::Stretch;
        };
        auto parse_justify_items_val = [](const std::string& s) -> int {
            if (s == "start" || s == "flex-start" || s == "self-start" || s == "left") return 0;
            if (s == "end" || s == "flex-end" || s == "self-end" || s == "right") return 1;
            if (s == "center") return 2;
            return 3; // stretch
        };
        if (parts.size() == 1) {
            style.align_items = parse_align_items_val(parts[0]);
            style.justify_items = parse_justify_items_val(parts[0]);
        } else if (parts.size() >= 2) {
            style.align_items = parse_align_items_val(parts[0]);
            style.justify_items = parse_justify_items_val(parts[1]);
        }
        return;
    }
    if (prop == "justify-content") {
        if (value_lower == "flex-start") style.justify_content = JustifyContent::FlexStart;
        else if (value_lower == "flex-end") style.justify_content = JustifyContent::FlexEnd;
        else if (value_lower == "center") style.justify_content = JustifyContent::Center;
        else if (value_lower == "space-between") style.justify_content = JustifyContent::SpaceBetween;
        else if (value_lower == "space-around") style.justify_content = JustifyContent::SpaceAround;
        else if (value_lower == "space-evenly") style.justify_content = JustifyContent::SpaceEvenly;
        return;
    }
    if (prop == "align-items") {
        if (value_lower == "flex-start") style.align_items = AlignItems::FlexStart;
        else if (value_lower == "flex-end") style.align_items = AlignItems::FlexEnd;
        else if (value_lower == "center") style.align_items = AlignItems::Center;
        else if (value_lower == "baseline") style.align_items = AlignItems::Baseline;
        else if (value_lower == "stretch") style.align_items = AlignItems::Stretch;
        return;
    }
    if (prop == "align-self") {
        if (value_lower == "auto") style.align_self = -1;
        else if (value_lower == "flex-start") style.align_self = 0;
        else if (value_lower == "flex-end") style.align_self = 1;
        else if (value_lower == "center") style.align_self = 2;
        else if (value_lower == "baseline") style.align_self = 3;
        else if (value_lower == "stretch") style.align_self = 4;
        return;
    }
    if (prop == "justify-self") {
        if (value_lower == "auto") style.justify_self = -1;
        else if (value_lower == "flex-start" || value_lower == "start" || value_lower == "self-start") style.justify_self = 0;
        else if (value_lower == "flex-end" || value_lower == "end" || value_lower == "self-end") style.justify_self = 1;
        else if (value_lower == "center") style.justify_self = 2;
        else if (value_lower == "baseline") style.justify_self = 3;
        else if (value_lower == "stretch") style.justify_self = 4;
        return;
    }
    if (prop == "place-self") {
        auto parts = split_whitespace(value_lower);
        auto parse_self = [](const std::string& s) -> int {
            if (s == "auto") return -1;
            if (s == "flex-start" || s == "start" || s == "self-start") return 0;
            if (s == "flex-end" || s == "end" || s == "self-end") return 1;
            if (s == "center") return 2;
            if (s == "baseline") return 3;
            if (s == "stretch") return 4;
            return -1;
        };
        if (parts.size() == 1) {
            int v = parse_self(parts[0]);
            style.align_self = v;
            style.justify_self = v;
        } else if (parts.size() >= 2) {
            style.align_self = parse_self(parts[0]);
            style.justify_self = parse_self(parts[1]);
        }
        return;
    }
    if (prop == "contain-intrinsic-size") {
        if (value_lower == "none") {
            style.contain_intrinsic_width = 0;
            style.contain_intrinsic_height = 0;
        } else {
            auto parts = split_whitespace(value_lower);
            if (parts.size() == 1) {
                auto v = parse_length(parts[0]);
                if (v) { style.contain_intrinsic_width = v->to_px(); style.contain_intrinsic_height = v->to_px(); }
            } else if (parts.size() >= 2) {
                auto v1 = parse_length(parts[0]);
                auto v2 = parse_length(parts[1]);
                if (v1) style.contain_intrinsic_width = v1->to_px();
                if (v2) style.contain_intrinsic_height = v2->to_px();
            }
        }
        return;
    }
    if (prop == "contain-intrinsic-width") {
        auto v = parse_length(value_str);
        if (v) style.contain_intrinsic_width = v->to_px();
        else if (value_lower == "none" || value_lower == "auto") style.contain_intrinsic_width = 0;
        return;
    }
    if (prop == "contain-intrinsic-height") {
        auto v = parse_length(value_str);
        if (v) style.contain_intrinsic_height = v->to_px();
        else if (value_lower == "none" || value_lower == "auto") style.contain_intrinsic_height = 0;
        return;
    }
    if (prop == "object-fit") {
        if (value_lower == "fill") style.object_fit = 0;
        else if (value_lower == "contain") style.object_fit = 1;
        else if (value_lower == "cover") style.object_fit = 2;
        else if (value_lower == "none") style.object_fit = 3;
        else if (value_lower == "scale-down") style.object_fit = 4;
        return;
    }
    if (prop == "image-rendering") {
        if (value_lower == "smooth") style.image_rendering = 1;
        else if (value_lower == "high-quality") style.image_rendering = 2;
        else if (value_lower == "crisp-edges" || value_lower == "-webkit-optimize-contrast") style.image_rendering = 3;
        else if (value_lower == "pixelated") style.image_rendering = 4;
        else style.image_rendering = 0;
        return;
    }
    if (prop == "hanging-punctuation") {
        if (value_lower == "first") style.hanging_punctuation = 1;
        else if (value_lower == "last") style.hanging_punctuation = 2;
        else if (value_lower == "force-end") style.hanging_punctuation = 3;
        else if (value_lower == "allow-end") style.hanging_punctuation = 4;
        else if (value_lower == "first last") style.hanging_punctuation = 5;
        else style.hanging_punctuation = 0;
        return;
    }
    if (prop == "object-position") {
        auto parts = split_whitespace(value_lower);
        auto parse_pos = [](const std::string& s) -> float {
            if (s == "left" || s == "top") return 0.0f;
            if (s == "center") return 50.0f;
            if (s == "right" || s == "bottom") return 100.0f;
            if (!s.empty() && s.back() == '%') {
                try { return std::stof(s); } catch (...) { return 50.0f; }
            }
            try { return std::stof(s); } catch (...) { return 50.0f; }
        };
        if (parts.size() >= 2) {
            style.object_position_x = parse_pos(parts[0]);
            style.object_position_y = parse_pos(parts[1]);
        } else if (parts.size() == 1) {
            float v = parse_pos(parts[0]);
            style.object_position_x = v;
            style.object_position_y = v;
        }
        return;
    }
    if (prop == "flex-grow") {
        try {
            style.flex_grow = std::stof(value_str);
        } catch (...) {}
        return;
    }
    if (prop == "flex-shrink") {
        try {
            style.flex_shrink = std::stof(value_str);
        } catch (...) {}
        return;
    }
    if (prop == "flex-basis") {
        auto l = parse_length(value_str);
        if (l) style.flex_basis = *l;
        return;
    }
    if (prop == "flex") {
        if (value_lower == "none") {
            style.flex_grow = 0;
            style.flex_shrink = 0;
            style.flex_basis = Length::auto_val();
            return;
        }
        if (value_lower == "auto") {
            style.flex_grow = 1;
            style.flex_shrink = 1;
            style.flex_basis = Length::auto_val();
            return;
        }

        auto parts = split_whitespace_paren(value_str);
        auto parse_number_token = [](const std::string& token, float& out) -> bool {
            std::string t = trim(token);
            if (t.empty()) return false;
            char* end = nullptr;
            out = std::strtof(t.c_str(), &end);
            return end != t.c_str() && end != nullptr && *end == '\0';
        };

        bool has_grow = false;
        bool has_shrink = false;
        bool has_basis = false;
        float grow = 0.0f;
        float shrink = 1.0f;
        Length basis = Length::auto_val();

        for (const auto& raw_part : parts) {
            float num = 0.0f;
            if (parse_number_token(raw_part, num)) {
                if (!has_grow) {
                    grow = num;
                    has_grow = true;
                    continue;
                }
                if (!has_shrink) {
                    shrink = num;
                    has_shrink = true;
                    continue;
                }
            }

            auto l = parse_length(raw_part);
            if (l && !has_basis) {
                basis = *l;
                has_basis = true;
            }
        }

        if (has_grow) {
            style.flex_grow = grow;
            style.flex_shrink = has_shrink ? shrink : 1.0f;
            // Numeric shorthand defaults flex-basis to 0%.
            style.flex_basis = has_basis ? basis : Length::percent(0);
        } else if (has_basis) {
            // Single basis value in shorthand defaults to 1 1 <basis>.
            style.flex_grow = 1.0f;
            style.flex_shrink = 1.0f;
            style.flex_basis = basis;
        }
        return;
    }
    if (prop == "order") {
        try {
            style.order = std::stoi(value_str);
        } catch (...) {}
        return;
    }
    if (prop == "gap" || prop == "grid-gap") {
        // gap shorthand: one or two values (row-gap [column-gap])
        std::istringstream gap_ss(value_str);
        std::string first_tok, second_tok;
        gap_ss >> first_tok >> second_tok;
        auto row_l = parse_length(first_tok);
        if (row_l) {
            style.gap = *row_l;
            style.column_gap_val = *row_l; // single value: both equal
            if (!second_tok.empty()) {
                auto col_l = parse_length(second_tok);
                if (col_l) style.column_gap_val = *col_l;
            }
        }
        return;
    }
    if (prop == "row-gap" || prop == "grid-row-gap") {
        auto l = parse_length(value_str);
        if (l) style.gap = *l;
        return;
    }
    if (prop == "column-gap" || prop == "grid-column-gap") {
        auto l = parse_length(value_str);
        if (l) style.column_gap_val = *l;
        return;
    }

    // ---- List ----
    if (prop == "list-style-type") {
        if (value_lower == "disc") style.list_style_type = ListStyleType::Disc;
        else if (value_lower == "circle") style.list_style_type = ListStyleType::Circle;
        else if (value_lower == "square") style.list_style_type = ListStyleType::Square;
        else if (value_lower == "decimal") style.list_style_type = ListStyleType::Decimal;
        else if (value_lower == "decimal-leading-zero") style.list_style_type = ListStyleType::DecimalLeadingZero;
        else if (value_lower == "lower-roman") style.list_style_type = ListStyleType::LowerRoman;
        else if (value_lower == "upper-roman") style.list_style_type = ListStyleType::UpperRoman;
        else if (value_lower == "lower-alpha") style.list_style_type = ListStyleType::LowerAlpha;
        else if (value_lower == "upper-alpha") style.list_style_type = ListStyleType::UpperAlpha;
        else if (value_lower == "none") style.list_style_type = ListStyleType::None;
        else if (value_lower == "lower-greek") style.list_style_type = ListStyleType::LowerGreek;
        else if (value_lower == "lower-latin") style.list_style_type = ListStyleType::LowerLatin;
        else if (value_lower == "upper-latin") style.list_style_type = ListStyleType::UpperLatin;
        else if (value_lower == "armenian") style.list_style_type = ListStyleType::Armenian;
        else if (value_lower == "georgian") style.list_style_type = ListStyleType::Georgian;
        else if (value_lower == "cjk-decimal") style.list_style_type = ListStyleType::CjkDecimal;
        return;
    }

    if (prop == "list-style-image") {
        if (value_lower == "none") {
            style.list_style_image.clear();
        } else {
            // Extract URL from url(...)
            // The CSS tokenizer fragments URLs (e.g. "icon.svg" -> "icon . svg"),
            // so we strip all spaces from the extracted URL content.
            auto pos = value_lower.find("url(");
            if (pos != std::string::npos) {
                auto start = value_str.find('(', pos) + 1;
                auto end = value_str.find(')', start);
                if (end != std::string::npos) {
                    std::string url = value_str.substr(start, end - start);
                    // Strip quotes
                    if (url.size() >= 2 && (url.front() == '"' || url.front() == '\'')) {
                        url = url.substr(1, url.size() - 2);
                    }
                    // Remove spaces inserted by tokenizer reconstruction
                    url.erase(std::remove(url.begin(), url.end(), ' '), url.end());
                    style.list_style_image = url;
                }
            }
        }
        return;
    }

    if (prop == "list-style") {
        auto parts = split_whitespace_paren(value_lower);
        for (auto& tok : parts) {
            if (tok == "inside") { style.list_style_position = ListStylePosition::Inside; continue; }
            if (tok == "outside") { style.list_style_position = ListStylePosition::Outside; continue; }
            if (tok.find("url(") != std::string::npos) {
                auto ps = value_str.find("url(");
                if (ps != std::string::npos) {
                    auto start = value_str.find('(', ps) + 1;
                    auto end = value_str.find(')', start);
                    if (end != std::string::npos) {
                        std::string url = value_str.substr(start, end - start);
                        if (url.size() >= 2 && (url.front() == '"' || url.front() == '\''))
                            url = url.substr(1, url.size() - 2);
                        url.erase(std::remove(url.begin(), url.end(), ' '), url.end());
                        style.list_style_image = url;
                    }
                }
                continue;
            }
            if (tok == "disc") style.list_style_type = ListStyleType::Disc;
            else if (tok == "circle") style.list_style_type = ListStyleType::Circle;
            else if (tok == "square") style.list_style_type = ListStyleType::Square;
            else if (tok == "decimal") style.list_style_type = ListStyleType::Decimal;
            else if (tok == "decimal-leading-zero") style.list_style_type = ListStyleType::DecimalLeadingZero;
            else if (tok == "lower-roman") style.list_style_type = ListStyleType::LowerRoman;
            else if (tok == "upper-roman") style.list_style_type = ListStyleType::UpperRoman;
            else if (tok == "lower-alpha") style.list_style_type = ListStyleType::LowerAlpha;
            else if (tok == "upper-alpha") style.list_style_type = ListStyleType::UpperAlpha;
            else if (tok == "none") style.list_style_type = ListStyleType::None;
            else if (tok == "lower-greek") style.list_style_type = ListStyleType::LowerGreek;
            else if (tok == "lower-latin") style.list_style_type = ListStyleType::LowerLatin;
            else if (tok == "upper-latin") style.list_style_type = ListStyleType::UpperLatin;
            else if (tok == "armenian") style.list_style_type = ListStyleType::Armenian;
            else if (tok == "georgian") style.list_style_type = ListStyleType::Georgian;
            else if (tok == "cjk-decimal") style.list_style_type = ListStyleType::CjkDecimal;
        }
        return;
    }

    // ---- Cursor ----
    if (prop == "cursor") {
        if (value_lower == "default") style.cursor = Cursor::Default;
        else if (value_lower == "pointer") style.cursor = Cursor::Pointer;
        else if (value_lower == "text") style.cursor = Cursor::Text;
        else if (value_lower == "move") style.cursor = Cursor::Move;
        else if (value_lower == "not-allowed") style.cursor = Cursor::NotAllowed;
        else style.cursor = Cursor::Auto;
        return;
    }

    // ---- Vertical Align ----
    if (prop == "vertical-align") {
        if (value_lower == "top") style.vertical_align = VerticalAlign::Top;
        else if (value_lower == "middle") style.vertical_align = VerticalAlign::Middle;
        else if (value_lower == "bottom") style.vertical_align = VerticalAlign::Bottom;
        else if (value_lower == "text-top") style.vertical_align = VerticalAlign::TextTop;
        else if (value_lower == "text-bottom") style.vertical_align = VerticalAlign::TextBottom;
        else if (value_lower == "baseline") style.vertical_align = VerticalAlign::Baseline;
        else {
            // Try to parse as length or percentage value
            auto l = parse_length(value_str);
            if (l) {
                style.vertical_align = VerticalAlign::Baseline;
                style.vertical_align_offset = l->to_px();
            } else {
                style.vertical_align = VerticalAlign::Baseline;
            }
        }
        return;
    }

    // ---- Outline ----
    if (prop == "outline") {
        // Parse shorthand: "2px solid red"
        std::istringstream oss(value_str);
        std::string part;
        Length outline_width = Length::px(1);
        BorderStyle outline_style_val = BorderStyle::None;
        Color outline_color = style.color;
        while (oss >> part) {
            auto ow = parse_length(part);
            if (ow) { outline_width = *ow; continue; }
            std::string part_lower = part;
            std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                part_lower == "double" || part_lower == "none" || part_lower == "groove" ||
                part_lower == "ridge" || part_lower == "inset" || part_lower == "outset") {
                outline_style_val = parse_border_style_value(part_lower);
                if (part_lower == "none") outline_width = Length::zero();
                continue;
            }
            auto oc = parse_color(part);
            if (oc) { outline_color = *oc; continue; }
        }
        style.outline_width = outline_width;
        style.outline_style = outline_style_val;
        style.outline_color = outline_color;
        return;
    }
    if (prop == "outline-width") {
        auto l = parse_length(value_str);
        if (l) style.outline_width = *l;
        return;
    }
    if (prop == "outline-color") {
        if (value_lower == "currentcolor" || value_lower == "currentcolour") {
            style.outline_color = style.color; // resolve currentColor to the element's text color
        } else {
            auto c = parse_color(value_str);
            if (c) style.outline_color = *c;
        }
        return;
    }
    if (prop == "outline-style") {
        style.outline_style = parse_border_style_value(value_lower);
        return;
    }
    if (prop == "outline-offset") {
        auto l = parse_length(value_str);
        if (l) style.outline_offset = *l;
        return;
    }

    // ---- Border Image ----
    // border-image shorthand: <source> <slice> [/ <width> [/ <outset>]] [<repeat>]
    if (prop == "border-image") {
        if (value_lower == "none") {
            style.border_image_source.clear();
            return;
        }
        std::string source_part;
        std::string remainder;
        // Check for gradient function as source
        auto grad_pos = value_str.find("linear-gradient(");
        if (grad_pos == std::string::npos) grad_pos = value_str.find("radial-gradient(");
        if (grad_pos == std::string::npos) grad_pos = value_str.find("repeating-linear-gradient(");
        if (grad_pos == std::string::npos) grad_pos = value_str.find("repeating-radial-gradient(");
        if (grad_pos != std::string::npos) {
            // Find matching closing paren
            int depth = 0;
            size_t end_pos = grad_pos;
            for (size_t i = grad_pos; i < value_str.size(); i++) {
                if (value_str[i] == '(') depth++;
                else if (value_str[i] == ')') { depth--; if (depth == 0) { end_pos = i; break; } }
            }
            source_part = value_str.substr(grad_pos, end_pos - grad_pos + 1);
            remainder = trim(value_str.substr(end_pos + 1));
        } else if (value_str.find("url(") != std::string::npos) {
            auto url_start = value_str.find("url(");
            auto url_end = value_str.find(')', url_start);
            if (url_end != std::string::npos) {
                source_part = value_str.substr(url_start, url_end - url_start + 1);
                remainder = trim(value_str.substr(url_end + 1));
            }
        }
        if (!source_part.empty()) {
            style.border_image_source = source_part;
        }
        // Parse remainder for slice, width, outset, repeat
        if (!remainder.empty()) {
            auto parts = split_whitespace(remainder);
            for (auto& p : parts) {
                std::string pl = p;
                std::transform(pl.begin(), pl.end(), pl.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (pl == "stretch") style.border_image_repeat = 0;
                else if (pl == "repeat") style.border_image_repeat = 1;
                else if (pl == "round") style.border_image_repeat = 2;
                else if (pl == "space") style.border_image_repeat = 3;
                else if (pl == "fill") style.border_image_slice_fill = true;
                else if (pl != "/") {
                    // Numeric: first number is slice, after '/' is width, after second '/' is outset
                    std::string num = p;
                    if (!num.empty() && num.back() == '%') num.pop_back();
                    auto px_pos = pl.find("px");
                    if (px_pos != std::string::npos) num = p.substr(0, px_pos);
                    try {
                        float val = std::stof(num);
                        // Check if preceded by '/'
                        // Simple heuristic: if we haven't changed slice yet from default, set slice
                        style.border_image_slice = val;
                    } catch (...) {}
                }
            }
        }
        return;
    }
    if (prop == "border-image-source") {
        if (value_lower == "none") {
            style.border_image_source.clear();
        } else if (value_str.find("url(") != std::string::npos) {
            auto start = value_str.find("url(");
            auto inner_start = start + 4;
            auto inner_end = value_str.find(')', inner_start);
            if (inner_end != std::string::npos) {
                std::string img_url = trim(value_str.substr(inner_start, inner_end - inner_start));
                if (img_url.size() >= 2 &&
                    ((img_url.front() == '\'' && img_url.back() == '\'') ||
                     (img_url.front() == '"' && img_url.back() == '"'))) {
                    img_url = img_url.substr(1, img_url.size() - 2);
                }
                style.border_image_source = img_url;
            }
        } else {
            style.border_image_source = value_str;
        }
        return;
    }
    if (prop == "border-image-slice") {
        std::istringstream iss(value_str);
        std::string part;
        while (iss >> part) {
            std::string pl = part;
            std::transform(pl.begin(), pl.end(), pl.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (pl == "fill") {
                style.border_image_slice_fill = true;
            } else {
                std::string num = part;
                if (!num.empty() && num.back() == '%') num.pop_back();
                try { style.border_image_slice = std::stof(num); } catch (...) {}
            }
        }
        return;
    }
    if (prop == "border-image-width") {
        std::string num = value_str;
        auto px_pos = value_lower.find("px");
        if (px_pos != std::string::npos) num = value_str.substr(0, px_pos);
        try { style.border_image_width_val = std::stof(trim(num)); } catch (...) {}
        return;
    }
    if (prop == "border-image-outset") {
        auto l = parse_length(value_str);
        if (l) style.border_image_outset = l->to_px(0);
        return;
    }
    if (prop == "border-image-repeat") {
        if (value_lower == "stretch") style.border_image_repeat = 0;
        else if (value_lower == "repeat") style.border_image_repeat = 1;
        else if (value_lower == "round") style.border_image_repeat = 2;
        else if (value_lower == "space") style.border_image_repeat = 3;
        return;
    }

    // ---- CSS Mask properties ----
    if (prop == "mask-image" || prop == "-webkit-mask-image") {
        style.mask_image = value_str;
        return;
    }
    if (prop == "mask-size" || prop == "-webkit-mask-size") {
        if (value_lower == "auto") style.mask_size = 0;
        else if (value_lower == "cover") style.mask_size = 1;
        else if (value_lower == "contain") style.mask_size = 2;
        else {
            style.mask_size = 3; // explicit
            auto parts = split_whitespace(value_lower);
            if (parts.size() >= 1) { auto v = parse_length(parts[0]); if (v) style.mask_size_width = v->to_px(); }
            if (parts.size() >= 2) { auto v = parse_length(parts[1]); if (v) style.mask_size_height = v->to_px(); }
        }
        return;
    }
    if (prop == "mask-repeat" || prop == "-webkit-mask-repeat") {
        if (value_lower == "repeat") style.mask_repeat = 0;
        else if (value_lower == "repeat-x") style.mask_repeat = 1;
        else if (value_lower == "repeat-y") style.mask_repeat = 2;
        else if (value_lower == "no-repeat") style.mask_repeat = 3;
        else if (value_lower == "space") style.mask_repeat = 4;
        else if (value_lower == "round") style.mask_repeat = 5;
        return;
    }
    if (prop == "mask-composite" || prop == "-webkit-mask-composite") {
        if (value_lower == "add") style.mask_composite = 0;
        else if (value_lower == "subtract") style.mask_composite = 1;
        else if (value_lower == "intersect") style.mask_composite = 2;
        else if (value_lower == "exclude") style.mask_composite = 3;
        return;
    }
    if (prop == "mask-mode") {
        if (value_lower == "match-source") style.mask_mode = 0;
        else if (value_lower == "alpha") style.mask_mode = 1;
        else if (value_lower == "luminance") style.mask_mode = 2;
        return;
    }
    if (prop == "mask" || prop == "-webkit-mask") {
        style.mask_shorthand = value_str;
        return;
    }
    if (prop == "mask-origin" || prop == "-webkit-mask-origin") {
        if (value_lower == "border-box") style.mask_origin = 0;
        else if (value_lower == "padding-box") style.mask_origin = 1;
        else if (value_lower == "content-box") style.mask_origin = 2;
        return;
    }
    if (prop == "mask-position" || prop == "-webkit-mask-position") {
        style.mask_position = value_str;
        return;
    }
    if (prop == "mask-clip" || prop == "-webkit-mask-clip") {
        if (value_lower == "border-box") style.mask_clip = 0;
        else if (value_lower == "padding-box") style.mask_clip = 1;
        else if (value_lower == "content-box") style.mask_clip = 2;
        else if (value_lower == "no-clip") style.mask_clip = 3;
        return;
    }
    if (prop == "mask-border" || prop == "mask-border-source" || prop == "mask-border-slice" ||
        prop == "mask-border-width" || prop == "mask-border-outset" || prop == "mask-border-repeat" ||
        prop == "mask-border-mode") {
        style.mask_border = value_str;
        return;
    }

    // ---- Content (for ::before / ::after pseudo-elements) ----
    if (prop == "content") {
        style.content_attr_name.clear();
        const std::string content_value = trim(value_str);
        const std::string content_value_lower = to_lower(content_value);

        if (content_value_lower == "none" || content_value_lower == "normal") {
            style.content = "none";
        } else if (content_value_lower == "open-quote") {
            style.content = "\xe2\x80\x9c"; // left double quotation mark
        } else if (content_value_lower == "close-quote") {
            style.content = "\xe2\x80\x9d"; // right double quotation mark
        } else if (content_value_lower == "no-open-quote" || content_value_lower == "no-close-quote") {
            style.content = "none"; // produce no content
        } else {
            // Tokenize content values while respecting quoted strings and function arguments.
            // This allows values like: "Chapter " counter(name) ". "
            auto tokenize_content = [&](const std::string& input, bool& ok) {
                std::vector<std::string> tokens;
                ok = true;
                size_t i = 0;
                while (i < input.size()) {
                    while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) i++;
                    if (i >= input.size()) break;

                    size_t start = i;
                    if (input[i] == '"' || input[i] == '\'') {
                        char quote = input[i++];
                        bool escaped = false;
                        while (i < input.size()) {
                            char ch = input[i];
                            if (!escaped && ch == quote) break;
                            if (!escaped && ch == '\\') escaped = true;
                            else escaped = false;
                            i++;
                        }
                        if (i >= input.size()) {
                            ok = false;
                            return tokens;
                        }
                        i++; // include closing quote
                        tokens.push_back(input.substr(start, i - start));
                        continue;
                    }

                    bool ident_start = std::isalpha(static_cast<unsigned char>(input[i])) ||
                                       input[i] == '_' || input[i] == '-';
                    if (ident_start) {
                        i++;
                        while (i < input.size()) {
                            char ch = input[i];
                            if (std::isalnum(static_cast<unsigned char>(ch)) ||
                                ch == '_' || ch == '-' || ch == ':') {
                                i++;
                            } else {
                                break;
                            }
                        }
                        if (i < input.size() && input[i] == '(') {
                            int depth = 0;
                            bool in_single = false;
                            bool in_double = false;
                            while (i < input.size()) {
                                char ch = input[i];
                                if (ch == '"' && !in_single) {
                                    in_double = !in_double;
                                    i++;
                                    continue;
                                }
                                if (ch == '\'' && !in_double) {
                                    in_single = !in_single;
                                    i++;
                                    continue;
                                }
                                if (in_single || in_double) {
                                    i++;
                                    continue;
                                }
                                if (ch == '(') depth++;
                                else if (ch == ')') {
                                    depth--;
                                    if (depth == 0) {
                                        i++; // include ')'
                                        break;
                                    }
                                }
                                i++;
                            }
                            if (depth != 0) {
                                ok = false;
                                return tokens;
                            }
                        }
                        tokens.push_back(trim(input.substr(start, i - start)));
                        continue;
                    }

                    while (i < input.size() &&
                           !std::isspace(static_cast<unsigned char>(input[i]))) i++;
                    tokens.push_back(input.substr(start, i - start));
                }
                return tokens;
            };

            bool tokenized_ok = false;
            auto tokens = tokenize_content(content_value, tokenized_ok);
            if (!tokenized_ok || tokens.empty()) {
                style.content = strip_quotes(content_value);
                return;
            }

            // Multiple tokens (string/function concatenation): keep raw form for runtime resolution.
            if (tokens.size() > 1) {
                style.content = content_value;
                return;
            }

            const std::string token = trim(tokens[0]);
            const std::string token_lower = to_lower(token);

            if (token_lower.size() >= 6 && token_lower.rfind("attr(", 0) == 0 && token.back() == ')') {
                std::string attr_name = trim(token.substr(5, token.size() - 6));
                style.content_attr_name = attr_name;
                style.content = "\x01""ATTR"; // sentinel for attr() resolution
            } else if (token_lower.size() >= 5 && token_lower.rfind("url(", 0) == 0 && token.back() == ')') {
                std::string url_value = trim(token.substr(4, token.size() - 5));
                style.content = "\x01URL:" + strip_quotes(url_value); // sentinel for url() resolution
            } else if ((token_lower.size() >= 9 && token_lower.rfind("counter(", 0) == 0 && token.back() == ')') ||
                       (token_lower.size() >= 10 && token_lower.rfind("counters(", 0) == 0 && token.back() == ')')) {
                style.content = token; // keep raw for runtime resolution
            } else if (token.size() >= 2 &&
                       ((token.front() == '"' && token.back() == '"') ||
                        (token.front() == '\'' && token.back() == '\''))) {
                // Preserve quoted strings verbatim so runtime token parsing can
                // distinguish literal strings from keyword tokens.
                style.content = token;
            } else {
                style.content = strip_quotes(token);
            }
        }
        return;
    }

    // ---- Transform ----
    if (prop == "transform") {
        if (value_lower == "none") {
            style.transforms.clear();
            return;
        }
        // Parse transform functions: translate(x, y), rotate(deg), scale(x[, y])
        style.transforms.clear();
        std::string v = value_str;
        size_t pos = 0;
        while (pos < v.size()) {
            // Skip whitespace
            while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t')) pos++;
            if (pos >= v.size()) break;

            // Find function name
            size_t fn_start = pos;
            while (pos < v.size() && v[pos] != '(') pos++;
            if (pos >= v.size()) break;
            std::string fn_name = to_lower(trim(v.substr(fn_start, pos - fn_start)));
            pos++; // skip '('

            // Find matching ')'
            size_t arg_start = pos;
            int paren_depth_local = 1;
            while (pos < v.size() && paren_depth_local > 0) {
                if (v[pos] == '(') paren_depth_local++;
                else if (v[pos] == ')') paren_depth_local--;
                if (paren_depth_local > 0) pos++;
            }
            if (pos >= v.size() && paren_depth_local > 0) break;
            std::string args = trim(v.substr(arg_start, pos - arg_start));
            pos++; // skip ')'

            if (fn_name == "translate") {
                Transform t;
                t.type = TransformType::Translate;
                auto comma = args.find(',');
                if (comma != std::string::npos) {
                    auto lx = parse_length(trim(args.substr(0, comma)));
                    auto ly = parse_length(trim(args.substr(comma + 1)));
                    if (lx) t.x = lx->to_px();
                    if (ly) t.y = ly->to_px();
                } else {
                    auto lx = parse_length(trim(args));
                    if (lx) t.x = lx->to_px();
                    t.y = 0;
                }
                style.transforms.push_back(t);
            } else if (fn_name == "translatex") {
                Transform t;
                t.type = TransformType::Translate;
                auto lx = parse_length(trim(args));
                if (lx) t.x = lx->to_px();
                t.y = 0;
                style.transforms.push_back(t);
            } else if (fn_name == "translatey") {
                Transform t;
                t.type = TransformType::Translate;
                t.x = 0;
                auto ly = parse_length(trim(args));
                if (ly) t.y = ly->to_px();
                style.transforms.push_back(t);
            } else if (fn_name == "rotate") {
                Transform t;
                t.type = TransformType::Rotate;
                std::string arg_lower_val = to_lower(trim(args));
                if (arg_lower_val.find("deg") != std::string::npos) {
                    try { t.angle = std::stof(arg_lower_val); } catch (...) {}
                } else if (arg_lower_val.find("rad") != std::string::npos) {
                    try {
                        float rad = std::stof(arg_lower_val);
                        t.angle = rad * 180.0f / 3.14159265f;
                    } catch (...) {}
                } else if (arg_lower_val.find("turn") != std::string::npos) {
                    try {
                        float turns = std::stof(arg_lower_val);
                        t.angle = turns * 360.0f;
                    } catch (...) {}
                } else {
                    try { t.angle = std::stof(arg_lower_val); } catch (...) {}
                }
                style.transforms.push_back(t);
            } else if (fn_name == "scale") {
                Transform t;
                t.type = TransformType::Scale;
                auto comma = args.find(',');
                if (comma != std::string::npos) {
                    try { t.x = std::stof(trim(args.substr(0, comma))); } catch (...) {}
                    try { t.y = std::stof(trim(args.substr(comma + 1))); } catch (...) {}
                } else {
                    try {
                        float s = std::stof(trim(args));
                        t.x = s;
                        t.y = s;
                    } catch (...) {}
                }
                style.transforms.push_back(t);
            } else if (fn_name == "scalex") {
                Transform t;
                t.type = TransformType::Scale;
                try { t.x = std::stof(trim(args)); } catch (...) {}
                t.y = 1;
                style.transforms.push_back(t);
            } else if (fn_name == "scaley") {
                Transform t;
                t.type = TransformType::Scale;
                t.x = 1;
                try { t.y = std::stof(trim(args)); } catch (...) {}
                style.transforms.push_back(t);
            } else if (fn_name == "skew") {
                Transform t;
                t.type = TransformType::Skew;
                auto parse_angle_val = [](const std::string& s) -> float {
                    std::string sl = to_lower(trim(s));
                    if (sl.find("rad") != std::string::npos) {
                        try { return std::stof(sl) * 180.0f / 3.14159265f; } catch (...) { return 0; }
                    } else if (sl.find("turn") != std::string::npos) {
                        try { return std::stof(sl) * 360.0f; } catch (...) { return 0; }
                    } else if (sl.find("grad") != std::string::npos) {
                        try { return std::stof(sl) * 0.9f; } catch (...) { return 0; }
                    } else {
                        try { return std::stof(sl); } catch (...) { return 0; }
                    }
                };
                auto comma = args.find(',');
                if (comma != std::string::npos) {
                    t.x = parse_angle_val(args.substr(0, comma));
                    t.y = parse_angle_val(args.substr(comma + 1));
                } else {
                    // Cascade tokenizer strips commas — try space-separated
                    auto parts = split_whitespace(args);
                    if (parts.size() >= 2) {
                        t.x = parse_angle_val(parts[0]);
                        t.y = parse_angle_val(parts[1]);
                    } else {
                        t.x = parse_angle_val(args);
                        t.y = 0;
                    }
                }
                style.transforms.push_back(t);
            } else if (fn_name == "skewx") {
                Transform t;
                t.type = TransformType::Skew;
                std::string sl = to_lower(trim(args));
                try { t.x = std::stof(sl); } catch (...) {}
                t.y = 0;
                style.transforms.push_back(t);
            } else if (fn_name == "skewy") {
                Transform t;
                t.type = TransformType::Skew;
                t.x = 0;
                std::string sl = to_lower(trim(args));
                try { t.y = std::stof(sl); } catch (...) {}
                style.transforms.push_back(t);
            } else if (fn_name == "matrix") {
                // matrix(a, b, c, d, e, f)
                Transform t;
                t.type = TransformType::Matrix;
                std::vector<float> vals;
                // Split on commas and/or spaces (cascade strips commas)
                std::string s = args;
                size_t p = 0;
                while (p < s.size() && vals.size() < 6) {
                    while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                    if (p >= s.size()) break;
                    size_t start_p = p;
                    while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                    try { vals.push_back(std::stof(s.substr(start_p, p - start_p))); } catch (...) { vals.push_back(0); }
                }
                for (size_t i = 0; i < 6 && i < vals.size(); i++) t.m[i] = vals[i];
                style.transforms.push_back(t);
            } else if (fn_name == "translate3d") {
                // translate3d(x, y, z) — apply as translate(x, y), ignore z
                Transform t;
                t.type = TransformType::Translate;
                std::vector<std::string> parts3d;
                {
                    std::string s = args;
                    size_t p = 0;
                    while (p < s.size() && parts3d.size() < 3) {
                        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                        if (p >= s.size()) break;
                        size_t start_p = p;
                        while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                        parts3d.push_back(s.substr(start_p, p - start_p));
                    }
                }
                if (parts3d.size() >= 1) { auto lx = parse_length(trim(parts3d[0])); if (lx) t.x = lx->to_px(); }
                if (parts3d.size() >= 2) { auto ly = parse_length(trim(parts3d[1])); if (ly) t.y = ly->to_px(); }
                style.transforms.push_back(t);
            } else if (fn_name == "translatez") {
                // translateZ(z) — no visual effect in 2D
                Transform t;
                t.type = TransformType::Translate;
                t.x = 0;
                t.y = 0;
                style.transforms.push_back(t);
            } else if (fn_name == "scale3d") {
                // scale3d(x, y, z) — apply as scale(x, y), ignore z
                Transform t;
                t.type = TransformType::Scale;
                std::vector<float> vals3d;
                {
                    std::string s = args;
                    size_t p = 0;
                    while (p < s.size() && vals3d.size() < 3) {
                        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                        if (p >= s.size()) break;
                        size_t start_p = p;
                        while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                        try { vals3d.push_back(std::stof(s.substr(start_p, p - start_p))); } catch (...) { vals3d.push_back(1); }
                    }
                }
                t.x = vals3d.size() >= 1 ? vals3d[0] : 1;
                t.y = vals3d.size() >= 2 ? vals3d[1] : 1;
                style.transforms.push_back(t);
            } else if (fn_name == "scalez") {
                // scaleZ(z) — no visual effect in 2D (no-op)
            } else if (fn_name == "rotate3d") {
                // rotate3d(x, y, z, angle) — apply as rotate(angle)
                Transform t;
                t.type = TransformType::Rotate;
                std::vector<std::string> rparts;
                {
                    std::string s = args;
                    size_t p = 0;
                    while (p < s.size() && rparts.size() < 4) {
                        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                        if (p >= s.size()) break;
                        size_t start_p = p;
                        while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                        rparts.push_back(s.substr(start_p, p - start_p));
                    }
                }
                if (rparts.size() >= 4) {
                    std::string angle_str = to_lower(trim(rparts[3]));
                    if (angle_str.find("deg") != std::string::npos) {
                        try { t.angle = std::stof(angle_str); } catch (...) {}
                    } else if (angle_str.find("rad") != std::string::npos) {
                        try { t.angle = std::stof(angle_str) * 180.0f / 3.14159265f; } catch (...) {}
                    } else if (angle_str.find("turn") != std::string::npos) {
                        try { t.angle = std::stof(angle_str) * 360.0f; } catch (...) {}
                    } else {
                        try { t.angle = std::stof(angle_str); } catch (...) {}
                    }
                }
                style.transforms.push_back(t);
            } else if (fn_name == "rotatex" || fn_name == "rotatey") {
                // rotateX/rotateY — no visual effect in 2D (no-op)
            } else if (fn_name == "rotatez") {
                // rotateZ(angle) — equivalent to rotate(angle) in 2D
                Transform t;
                t.type = TransformType::Rotate;
                std::string angle_str = to_lower(trim(args));
                if (angle_str.find("deg") != std::string::npos) {
                    try { t.angle = std::stof(angle_str); } catch (...) {}
                } else if (angle_str.find("rad") != std::string::npos) {
                    try { t.angle = std::stof(angle_str) * 180.0f / 3.14159265f; } catch (...) {}
                } else if (angle_str.find("turn") != std::string::npos) {
                    try { t.angle = std::stof(angle_str) * 360.0f; } catch (...) {}
                } else {
                    try { t.angle = std::stof(angle_str); } catch (...) {}
                }
                style.transforms.push_back(t);
            } else if (fn_name == "perspective") {
                // perspective(d) — no-op for 2D rendering
            } else if (fn_name == "matrix3d") {
                // matrix3d(a1..a16) — extract 2D affine from 4x4 column-major
                Transform t;
                t.type = TransformType::Matrix;
                std::vector<float> vals16;
                {
                    std::string s = args;
                    size_t p = 0;
                    while (p < s.size() && vals16.size() < 16) {
                        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                        if (p >= s.size()) break;
                        size_t start_p = p;
                        while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                        try { vals16.push_back(std::stof(s.substr(start_p, p - start_p))); } catch (...) { vals16.push_back(0); }
                    }
                }
                if (vals16.size() >= 16) {
                    t.m[0] = vals16[0];  // a
                    t.m[1] = vals16[1];  // b
                    t.m[2] = vals16[4];  // c
                    t.m[3] = vals16[5];  // d
                    t.m[4] = vals16[12]; // e (tx)
                    t.m[5] = vals16[13]; // f (ty)
                }
                style.transforms.push_back(t);
            }
        }
        return;
    }

    // ---- Border collapse ----
    if (prop == "border-collapse") {
        style.border_collapse = (value_lower == "collapse");
        return;
    }

    // ---- Border spacing ----
    if (prop == "border-spacing") {
        // border-spacing can have one or two values: "10px" or "10px 5px"
        // First value = horizontal, second value = vertical
        std::istringstream bss(value_lower);
        std::string first_tok, second_tok;
        bss >> first_tok >> second_tok;
        auto h_len = parse_length(first_tok);
        if (h_len) {
            style.border_spacing = h_len->to_px();
            if (!second_tok.empty()) {
                auto v_len = parse_length(second_tok);
                if (v_len) {
                    style.border_spacing_v = v_len->to_px();
                } else {
                    style.border_spacing_v = 0; // same as horizontal
                }
            } else {
                style.border_spacing_v = 0; // same as horizontal
            }
        }
        return;
    }

    // ---- Table layout ----
    if (prop == "table-layout") {
        if (value_lower == "fixed") style.table_layout = 1;
        else style.table_layout = 0; // auto
        return;
    }

    // ---- Caption side ----
    if (prop == "caption-side") {
        if (value_lower == "bottom") style.caption_side = 1;
        else style.caption_side = 0; // top
        return;
    }

    // ---- Empty cells ----
    if (prop == "empty-cells") {
        if (value_lower == "hide") style.empty_cells = 1;
        else style.empty_cells = 0; // show
        return;
    }

    // ---- Quotes ----
    if (prop == "quotes") {
        if (value_lower == "none") style.quotes = "none";
        else if (value_lower == "auto") style.quotes = "";
        else style.quotes = value_str;
        return;
    }

    // ---- List style position ----
    if (prop == "list-style-position") {
        if (value_lower == "inside") style.list_style_position = ListStylePosition::Inside;
        else style.list_style_position = ListStylePosition::Outside;
        return;
    }

    // ---- Pointer events ----
    if (prop == "pointer-events") {
        if (value_lower == "none") style.pointer_events = PointerEvents::None;
        else if (value_lower == "visiblepainted") style.pointer_events = PointerEvents::VisiblePainted;
        else if (value_lower == "visiblefill") style.pointer_events = PointerEvents::VisibleFill;
        else if (value_lower == "visiblestroke") style.pointer_events = PointerEvents::VisibleStroke;
        else if (value_lower == "visible") style.pointer_events = PointerEvents::Visible;
        else if (value_lower == "painted") style.pointer_events = PointerEvents::Painted;
        else if (value_lower == "fill") style.pointer_events = PointerEvents::Fill;
        else if (value_lower == "stroke") style.pointer_events = PointerEvents::Stroke;
        else if (value_lower == "all") style.pointer_events = PointerEvents::All;
        else style.pointer_events = PointerEvents::Auto;
        return;
    }

    // ---- User select ----
    if (prop == "user-select" || prop == "-webkit-user-select") {
        if (value_lower == "none") style.user_select = UserSelect::None;
        else if (value_lower == "text") style.user_select = UserSelect::Text;
        else if (value_lower == "all") style.user_select = UserSelect::All;
        else style.user_select = UserSelect::Auto;
        return;
    }

    // ---- Tab size ----
    if (prop == "tab-size" || prop == "-moz-tab-size") {
        try { style.tab_size = std::stoi(value_str); } catch (...) {}
        return;
    }

    // ---- CSS Filter ----
    if (prop == "filter") {
        if (value_lower == "none") {
            style.filters.clear();
            return;
        }
        style.filters.clear();
        // Parse filter functions: e.g. "grayscale(0.5) blur(10px)"
        std::string v = value_str;
        size_t pos = 0;
        while (pos < v.size()) {
            while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t')) pos++;
            if (pos >= v.size()) break;
            size_t fn_start = pos;
            while (pos < v.size() && v[pos] != '(') pos++;
            if (pos >= v.size()) break;
            std::string fn = to_lower(trim(v.substr(fn_start, pos - fn_start)));
            pos++; // skip '('
            size_t arg_start = pos;
            while (pos < v.size() && v[pos] != ')') pos++;
            if (pos >= v.size()) break;
            std::string arg = trim(v.substr(arg_start, pos - arg_start));
            pos++; // skip ')'

            int type = 0;
            float val = 0;
            if (fn == "grayscale") { type = 1; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "sepia") { type = 2; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "brightness") { type = 3; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "contrast") { type = 4; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "invert") { type = 5; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "saturate") { type = 6; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "opacity") { type = 7; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "hue-rotate") {
                type = 8;
                try {
                    auto arg_lower = to_lower(arg);
                    if (arg_lower.size() >= 4 && arg_lower.substr(arg_lower.size() - 4) == "turn") {
                        val = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 4))) * 360.0f;
                    } else if (arg_lower.size() >= 3 && arg_lower.substr(arg_lower.size() - 3) == "rad") {
                        val = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 3))) * (180.0f / 3.14159265f);
                    } else if (arg_lower.size() >= 3 && arg_lower.substr(arg_lower.size() - 3) == "deg") {
                        val = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 3)));
                    } else {
                        val = std::stof(trim(arg_lower));
                    }
                } catch (...) { val = 0; }
            } // degrees
            else if (fn == "blur") {
                type = 9;
                auto l = parse_length(arg);
                if (l) val = l->to_px(0);
            }
            else if (fn == "drop-shadow") {
                // drop-shadow(offset-x offset-y [blur-radius] [color])
                type = 10;
                auto ds_parts = split_whitespace(arg);
                float ds_ox = 0, ds_oy = 0, ds_blur = 0;
                uint32_t ds_color = 0xFF000000; // default black
                int num_idx = 0;
                for (auto& p : ds_parts) {
                    auto l = parse_length(p);
                    if (l && num_idx < 3) {
                        float pxv = l->to_px(0);
                        if (num_idx == 0) ds_ox = pxv;
                        else if (num_idx == 1) ds_oy = pxv;
                        else if (num_idx == 2) ds_blur = pxv;
                        num_idx++;
                    } else {
                        auto c = parse_color(p);
                        if (c) {
                            ds_color = (static_cast<uint32_t>(c->a) << 24) |
                                       (static_cast<uint32_t>(c->r) << 16) |
                                       (static_cast<uint32_t>(c->g) << 8) |
                                       static_cast<uint32_t>(c->b);
                        }
                    }
                }
                val = ds_blur;
                style.drop_shadow_ox = ds_ox;
                style.drop_shadow_oy = ds_oy;
                style.drop_shadow_color = ds_color;
            }

            if (type > 0) style.filters.push_back({type, val});
        }
        return;
    }

    // ---- CSS Backdrop Filter ----
    if (prop == "backdrop-filter" || prop == "-webkit-backdrop-filter") {
        if (value_lower == "none") {
            style.backdrop_filters.clear();
            return;
        }
        style.backdrop_filters.clear();
        // Parse filter functions: e.g. "blur(10px) grayscale(0.5)"
        std::string v = value_str;
        size_t pos = 0;
        while (pos < v.size()) {
            while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t')) pos++;
            if (pos >= v.size()) break;
            size_t fn_start = pos;
            while (pos < v.size() && v[pos] != '(') pos++;
            if (pos >= v.size()) break;
            std::string fn = to_lower(trim(v.substr(fn_start, pos - fn_start)));
            pos++; // skip '('
            size_t arg_start = pos;
            while (pos < v.size() && v[pos] != ')') pos++;
            if (pos >= v.size()) break;
            std::string arg = trim(v.substr(arg_start, pos - arg_start));
            pos++; // skip ')'

            int type = 0;
            float val = 0;
            if (fn == "grayscale") { type = 1; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "sepia") { type = 2; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "brightness") { type = 3; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "contrast") { type = 4; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "invert") { type = 5; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "saturate") { type = 6; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "opacity") { type = 7; try { val = std::stof(arg); } catch (...) { val = 1; } }
            else if (fn == "hue-rotate") { type = 8; try { val = std::stof(arg); } catch (...) { val = 0; } } // degrees
            else if (fn == "blur") {
                type = 9;
                auto l = parse_length(arg);
                if (l) val = l->to_px(0);
            }

            if (type > 0) style.backdrop_filters.push_back({type, val});
        }
        return;
    }

    // ---- CSS Resize ----
    if (prop == "resize") {
        if (value_lower == "both") style.resize = 1;
        else if (value_lower == "horizontal") style.resize = 2;
        else if (value_lower == "vertical") style.resize = 3;
        else style.resize = 0; // none
        return;
    }

    // ---- CSS Isolation ----
    if (prop == "isolation") {
        style.isolation = (value_lower == "isolate") ? 1 : 0;
        return;
    }

    // ---- CSS Mix Blend Mode ----
    if (prop == "mix-blend-mode") {
        if (value_lower == "multiply") style.mix_blend_mode = 1;
        else if (value_lower == "screen") style.mix_blend_mode = 2;
        else if (value_lower == "overlay") style.mix_blend_mode = 3;
        else if (value_lower == "darken") style.mix_blend_mode = 4;
        else if (value_lower == "lighten") style.mix_blend_mode = 5;
        else if (value_lower == "color-dodge") style.mix_blend_mode = 6;
        else if (value_lower == "color-burn") style.mix_blend_mode = 7;
        else if (value_lower == "hard-light") style.mix_blend_mode = 8;
        else if (value_lower == "soft-light") style.mix_blend_mode = 9;
        else if (value_lower == "difference") style.mix_blend_mode = 10;
        else if (value_lower == "exclusion") style.mix_blend_mode = 11;
        else style.mix_blend_mode = 0;
        return;
    }

    // ---- CSS Contain ----
    if (prop == "contain") {
        if (value_lower == "none") style.contain = 0;
        else if (value_lower == "strict") style.contain = 1;
        else if (value_lower == "content") style.contain = 2;
        else if (value_lower == "size") style.contain = 3;
        else if (value_lower == "layout") style.contain = 4;
        else if (value_lower == "style") style.contain = 5;
        else if (value_lower == "paint") style.contain = 6;
        else style.contain = 0;
        return;
    }

    // ---- CSS Clip Path ----
    if (prop == "clip-path") {
        if (value_lower == "none") {
            style.clip_path_type = 0;
            style.clip_path_values.clear();
        } else if (value_lower.find("circle(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                float radius = 50.0f;
                float at_x = -1.0f, at_y = -1.0f; // -1 = use default center
                // Check for "at" keyword: "50% at 50% 50%"
                auto at_pos = inner.find(" at ");
                if (at_pos != std::string::npos) {
                    std::string radius_str = trim(inner.substr(0, at_pos));
                    std::string at_str = trim(inner.substr(at_pos + 4));
                    if (!radius_str.empty()) {
                        try {
                            if (radius_str.back() == '%') radius = std::stof(radius_str.substr(0, radius_str.size() - 1));
                            else { auto l = parse_length(radius_str); if (l) radius = l->to_px(); }
                        } catch (...) {}
                    }
                    auto at_parts = split_whitespace(at_str);
                    auto parse_pos = [](const std::string& s) -> float {
                        if (s == "center") return 50.0f;
                        if (s == "left" || s == "top") return 0.0f;
                        if (s == "right" || s == "bottom") return 100.0f;
                        try {
                            if (s.back() == '%') return std::stof(s.substr(0, s.size() - 1));
                            else return std::stof(s); // px value as-is (negative means absolute)
                        } catch (...) { return 50.0f; }
                    };
                    if (at_parts.size() >= 1) at_x = parse_pos(at_parts[0]);
                    if (at_parts.size() >= 2) at_y = parse_pos(at_parts[1]);
                    else at_y = at_x; // single value: same for both
                } else if (!inner.empty()) {
                    try {
                        if (inner.back() == '%') radius = std::stof(inner.substr(0, inner.size() - 1));
                        else { auto l = parse_length(inner); if (l) radius = l->to_px(); }
                    } catch (...) {}
                }
                style.clip_path_type = 1;
                if (at_x >= 0) {
                    style.clip_path_values = {radius, at_x, at_y};
                } else {
                    style.clip_path_values = {radius};
                }
            }
        } else if (value_lower.find("ellipse(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                float rx = 50.0f, ry = 50.0f;
                float at_x = -1.0f, at_y = -1.0f;
                auto at_pos = inner.find(" at ");
                std::string dims_str = inner;
                if (at_pos != std::string::npos) {
                    dims_str = trim(inner.substr(0, at_pos));
                    std::string at_str = trim(inner.substr(at_pos + 4));
                    auto at_parts = split_whitespace(at_str);
                    auto parse_pos = [](const std::string& s) -> float {
                        if (s == "center") return 50.0f;
                        if (s == "left" || s == "top") return 0.0f;
                        if (s == "right" || s == "bottom") return 100.0f;
                        try {
                            if (s.back() == '%') return std::stof(s.substr(0, s.size() - 1));
                            else return std::stof(s);
                        } catch (...) { return 50.0f; }
                    };
                    if (at_parts.size() >= 1) at_x = parse_pos(at_parts[0]);
                    if (at_parts.size() >= 2) at_y = parse_pos(at_parts[1]);
                    else at_y = at_x;
                }
                auto parts = split_whitespace(dims_str);
                if (parts.size() >= 1) {
                    try {
                        if (parts[0].back() == '%') rx = std::stof(parts[0].substr(0, parts[0].size() - 1));
                        else rx = std::stof(parts[0]);
                    } catch (...) {}
                }
                if (parts.size() >= 2) {
                    try {
                        if (parts[1].back() == '%') ry = std::stof(parts[1].substr(0, parts[1].size() - 1));
                        else ry = std::stof(parts[1]);
                    } catch (...) {}
                }
                style.clip_path_type = 2;
                if (at_x >= 0) {
                    style.clip_path_values = {rx, ry, at_x, at_y};
                } else {
                    style.clip_path_values = {rx, ry};
                }
            }
        } else if (value_lower.find("inset(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                auto parts = split_whitespace(inner);
                float top = 0, right_v = 0, bottom_v = 0, left_v = 0;
                auto parse_val = [](const std::string& s) -> float {
                    try {
                        if (s.back() == '%') return std::stof(s.substr(0, s.size() - 1));
                        std::string v = s;
                        if (v.size() > 2 && v.substr(v.size() - 2) == "px") v = v.substr(0, v.size() - 2);
                        return std::stof(v);
                    } catch (...) { return 0.0f; }
                };
                if (parts.size() == 1) {
                    top = right_v = bottom_v = left_v = parse_val(parts[0]);
                } else if (parts.size() == 2) {
                    top = bottom_v = parse_val(parts[0]);
                    right_v = left_v = parse_val(parts[1]);
                } else if (parts.size() == 3) {
                    top = parse_val(parts[0]);
                    right_v = left_v = parse_val(parts[1]);
                    bottom_v = parse_val(parts[2]);
                } else if (parts.size() >= 4) {
                    top = parse_val(parts[0]);
                    right_v = parse_val(parts[1]);
                    bottom_v = parse_val(parts[2]);
                    left_v = parse_val(parts[3]);
                }
                style.clip_path_type = 3;
                style.clip_path_values = {top, right_v, bottom_v, left_v};
            }
        } else if (value_lower.find("polygon(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                // Parse polygon points: "x1 y1, x2 y2, x3 y3, ..." or "x1% y1%, x2% y2%, ..."
                // Split by comma first, then each pair by space
                style.clip_path_values.clear();
                std::string current;
                std::vector<std::string> point_pairs;
                for (size_t i = 0; i <= inner.size(); i++) {
                    if (i == inner.size() || inner[i] == ',') {
                        auto pt = trim(current);
                        if (!pt.empty()) point_pairs.push_back(pt);
                        current.clear();
                    } else {
                        current += inner[i];
                    }
                }
                for (const auto& pair_str : point_pairs) {
                    // Split pair by whitespace
                    auto sp = pair_str.find(' ');
                    if (sp == std::string::npos) {
                        // Try tab
                        sp = pair_str.find('\t');
                    }
                    if (sp != std::string::npos) {
                        std::string xs = trim(pair_str.substr(0, sp));
                        std::string ys = trim(pair_str.substr(sp + 1));
                        float x_val = 0, y_val = 0;
                        // Parse percentage or pixel values; percentages are resolved later
                        // For now, store as pixel offsets relative to element bounds
                        // Percentage values are stored as-is and resolved in the renderer
                        try {
                            if (!xs.empty() && xs.back() == '%') {
                                x_val = std::stof(xs.substr(0, xs.size() - 1)); // percentage
                                x_val = -x_val; // negative = percentage (convention)
                            } else {
                                auto lv = parse_length(xs);
                                if (lv) x_val = lv->to_px();
                            }
                        } catch (...) {}
                        try {
                            if (!ys.empty() && ys.back() == '%') {
                                y_val = std::stof(ys.substr(0, ys.size() - 1));
                                y_val = -y_val; // negative = percentage
                            } else {
                                auto lv = parse_length(ys);
                                if (lv) y_val = lv->to_px();
                            }
                        } catch (...) {}
                        style.clip_path_values.push_back(x_val);
                        style.clip_path_values.push_back(y_val);
                    }
                }
                if (style.clip_path_values.size() >= 6) { // At least 3 points
                    style.clip_path_type = 4;
                }
            }
        } else if (value_lower.find("path(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                // Strip optional quotes around the SVG path data
                if (inner.size() >= 2 &&
                    ((inner.front() == '\'' && inner.back() == '\'') ||
                     (inner.front() == '"' && inner.back() == '"'))) {
                    inner = inner.substr(1, inner.size() - 2);
                }
                style.clip_path_type = 5; // 5 = path
                style.clip_path_values.clear();
                style.clip_path_path_data = inner;
            }
        } else if (value_lower.find("url(") == 0) {
            // clip-path: url(#myClip) — store the URL reference
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_str.substr(lp + 1, rp - lp - 1));
                // Strip optional quotes
                if (inner.size() >= 2 &&
                    ((inner.front() == '\'' && inner.back() == '\'') ||
                     (inner.front() == '"' && inner.back() == '"'))) {
                    inner = inner.substr(1, inner.size() - 2);
                }
                style.clip_path_type = 6; // 6 = url
                style.clip_path_values.clear();
                style.clip_path_path_data = inner; // store url reference
            }
        }
        return;
    }

    // ---- CSS Shape Outside ----
    if (prop == "shape-outside") {
        // Also store raw string form
        if (value_lower == "none") style.shape_outside_str = "";
        else style.shape_outside_str = value_str;
        if (value_lower == "none") {
            style.shape_outside_type = 0;
            style.shape_outside_values.clear();
        } else if (value_lower == "margin-box") {
            style.shape_outside_type = 5;
            style.shape_outside_values.clear();
        } else if (value_lower == "border-box") {
            style.shape_outside_type = 6;
            style.shape_outside_values.clear();
        } else if (value_lower == "padding-box") {
            style.shape_outside_type = 7;
            style.shape_outside_values.clear();
        } else if (value_lower == "content-box") {
            style.shape_outside_type = 8;
            style.shape_outside_values.clear();
        } else if (value_lower.find("circle(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                float radius = 50.0f;
                if (!inner.empty()) {
                    try {
                        if (inner.back() == '%') radius = std::stof(inner.substr(0, inner.size() - 1));
                        else radius = std::stof(inner);
                    } catch (...) {}
                }
                style.shape_outside_type = 1;
                style.shape_outside_values = {radius};
            }
        } else if (value_lower.find("ellipse(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                auto parts = split_whitespace(inner);
                float rx = 50.0f, ry = 50.0f;
                if (parts.size() >= 1) {
                    try {
                        if (parts[0].back() == '%') rx = std::stof(parts[0].substr(0, parts[0].size() - 1));
                        else rx = std::stof(parts[0]);
                    } catch (...) {}
                }
                if (parts.size() >= 2) {
                    try {
                        if (parts[1].back() == '%') ry = std::stof(parts[1].substr(0, parts[1].size() - 1));
                        else ry = std::stof(parts[1]);
                    } catch (...) {}
                }
                style.shape_outside_type = 2;
                style.shape_outside_values = {rx, ry};
            }
        } else if (value_lower.find("inset(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                auto parts = split_whitespace(inner);
                float top = 0, right_v = 0, bottom_v = 0, left_v = 0;
                auto parse_val = [](const std::string& s) -> float {
                    try {
                        if (s.back() == '%') return std::stof(s.substr(0, s.size() - 1));
                        std::string v = s;
                        if (v.size() > 2 && v.substr(v.size() - 2) == "px") v = v.substr(0, v.size() - 2);
                        return std::stof(v);
                    } catch (...) { return 0.0f; }
                };
                if (parts.size() == 1) {
                    top = right_v = bottom_v = left_v = parse_val(parts[0]);
                } else if (parts.size() == 2) {
                    top = bottom_v = parse_val(parts[0]);
                    right_v = left_v = parse_val(parts[1]);
                } else if (parts.size() == 3) {
                    top = parse_val(parts[0]);
                    right_v = left_v = parse_val(parts[1]);
                    bottom_v = parse_val(parts[2]);
                } else if (parts.size() >= 4) {
                    top = parse_val(parts[0]);
                    right_v = parse_val(parts[1]);
                    bottom_v = parse_val(parts[2]);
                    left_v = parse_val(parts[3]);
                }
                style.shape_outside_type = 3;
                style.shape_outside_values = {top, right_v, bottom_v, left_v};
            }
        } else if (value_lower.find("polygon(") == 0) {
            auto lp = value_lower.find('(');
            auto rp = value_lower.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inner = trim(value_lower.substr(lp + 1, rp - lp - 1));
                // Parse polygon points: "x1 y1, x2 y2, x3 y3, ..."
                style.shape_outside_values.clear();
                std::vector<std::string> point_pairs;
                std::string current;
                for (size_t i = 0; i <= inner.size(); i++) {
                    if (i == inner.size() || inner[i] == ',') {
                        auto pt = trim(current);
                        if (!pt.empty()) point_pairs.push_back(pt);
                        current.clear();
                    } else {
                        current += inner[i];
                    }
                }
                for (const auto& pair_str : point_pairs) {
                    auto sp = pair_str.find(' ');
                    if (sp == std::string::npos) sp = pair_str.find('\t');
                    if (sp != std::string::npos) {
                        std::string xs = trim(pair_str.substr(0, sp));
                        std::string ys = trim(pair_str.substr(sp + 1));
                        float x_val = 0, y_val = 0;
                        try {
                            if (!xs.empty() && xs.back() == '%') {
                                x_val = std::stof(xs.substr(0, xs.size() - 1));
                                x_val = -x_val; // negative = percentage convention
                            } else {
                                auto lv = parse_length(xs);
                                if (lv) x_val = lv->to_px();
                            }
                        } catch (...) {}
                        try {
                            if (!ys.empty() && ys.back() == '%') {
                                y_val = std::stof(ys.substr(0, ys.size() - 1));
                                y_val = -y_val;
                            } else {
                                auto lv = parse_length(ys);
                                if (lv) y_val = lv->to_px();
                            }
                        } catch (...) {}
                        style.shape_outside_values.push_back(x_val);
                        style.shape_outside_values.push_back(y_val);
                    }
                }
                if (style.shape_outside_values.size() >= 6) { // At least 3 points
                    style.shape_outside_type = 4; // 4 = polygon
                }
            }
        }
        return;
    }

    // ---- CSS Shape Margin ----
    if (prop == "shape-margin") {
        auto l = parse_length(value_str);
        if (l) style.shape_margin = l->to_px();
        return;
    }

    // ---- CSS Shape Image Threshold ----
    if (prop == "shape-image-threshold") {
        try { style.shape_image_threshold = std::stof(value_str); } catch (...) {}
        return;
    }

    // ---- CSS Direction ----
    if (prop == "direction") {
        if (value_lower == "rtl") style.direction = Direction::Rtl;
        else style.direction = Direction::Ltr;
        return;
    }

    // ---- CSS Line Clamp ----
    if (prop == "line-clamp" || prop == "-webkit-line-clamp") {
        if (value_lower == "none") style.line_clamp = -1;
        else { try { style.line_clamp = std::stoi(value_str); } catch (...) {} }
        return;
    }

    // ---- Caret color ----
    if (prop == "caret-color") {
        if (value_lower != "auto") {
            auto c = parse_color(value_lower);
            if (c) style.caret_color = *c;
        }
        return;
    }

    // ---- Accent color ----
    if (prop == "accent-color") {
        if (value_lower == "auto") {
            style.accent_color = 0;
            return;
        }
        auto c = parse_color(value_lower);
        if (c) {
            style.accent_color =
                (static_cast<uint32_t>(c->a) << 24) |
                (static_cast<uint32_t>(c->r) << 16) |
                (static_cast<uint32_t>(c->g) << 8) |
                static_cast<uint32_t>(c->b);
        }
        return;
    }

    // ---- Color interpolation ----
    if (prop == "color-interpolation") {
        if (value_lower == "auto") style.color_interpolation = 0;
        else if (value_lower == "srgb") style.color_interpolation = 1;
        else if (value_lower == "linearrgb") style.color_interpolation = 2;
        return;
    }

    // ---- Scroll behavior ----
    if (prop == "scroll-behavior") {
        if (value_lower == "auto") style.scroll_behavior = 0;
        else if (value_lower == "smooth") style.scroll_behavior = 1;
        return;
    }

    // ---- Scroll snap type ----
    if (prop == "scroll-snap-type") {
        auto parts = split_whitespace(value_lower);
        style.scroll_snap_type_axis = 0;
        style.scroll_snap_type_strictness = 0;

        for (const auto& part : parts) {
            if (part == "none") {
                style.scroll_snap_type_axis = 0;
                style.scroll_snap_type_strictness = 0;
                return;
            }
            if (part == "x" || part == "inline") style.scroll_snap_type_axis = 1;
            else if (part == "y" || part == "block") style.scroll_snap_type_axis = 2;
            else if (part == "both") style.scroll_snap_type_axis = 3;
            else if (part == "mandatory") style.scroll_snap_type_strictness = 1;
            else if (part == "proximity") style.scroll_snap_type_strictness = 2;
        }
        if (style.scroll_snap_type_axis != 0 && style.scroll_snap_type_strictness == 0)
            style.scroll_snap_type_strictness = 2; // default proximity
        return;
    }

    // ---- Scroll snap align ----
    if (prop == "scroll-snap-align") {
        auto tokens = split_whitespace(value_lower);
        auto parse_token = [](const std::string& t) -> int {
            if (t == "start") return 1;
            if (t == "center") return 2;
            if (t == "end") return 3;
            return 0; // none or unknown
        };
        int first = 0, second = 0;
        if (!tokens.empty()) {
            first = parse_token(tokens[0]);
            second = (tokens.size() > 1) ? parse_token(tokens[1]) : first;
        }
        // first value = x (inline axis), second value = y (block axis)
        style.scroll_snap_align_x = first;
        style.scroll_snap_align_y = second;
        return;
    }

    // ---- Scroll snap stop ----
    if (prop == "scroll-snap-stop") {
        if (value_lower == "normal") style.scroll_snap_stop = 0;
        else if (value_lower == "always") style.scroll_snap_stop = 1;
        return;
    }

    // ---- Placeholder color (::placeholder pseudo-element support) ----
    if (prop == "placeholder-color") {
        auto c = parse_color(value_lower);
        if (c) style.placeholder_color = *c;
        return;
    }

    // ---- Writing mode ----
    if (prop == "writing-mode") {
        if (value_lower == "horizontal-tb") style.writing_mode = 0;
        else if (value_lower == "vertical-rl") style.writing_mode = 1;
        else if (value_lower == "vertical-lr") style.writing_mode = 2;
        else if (value_lower == "sideways-rl") style.writing_mode = 3;
        else if (value_lower == "sideways-lr") style.writing_mode = 4;
        return;
    }

    // ---- CSS Counter properties ----
    if (prop == "counter-increment") {
        style.counter_increment = value_str;
        return;
    }
    if (prop == "counter-reset") {
        style.counter_reset = value_str;
        return;
    }
    if (prop == "counter-set") {
        style.counter_set = value_str;
        return;
    }

    // ---- CSS Multi-column layout ----
    if (prop == "column-count") {
        if (value_lower == "auto") style.column_count = -1;
        else { try { style.column_count = std::stoi(value_str); } catch (...) {} }
        return;
    }
    if (prop == "column-fill") {
        if (value_lower == "balance") style.column_fill = 0;
        else if (value_lower == "auto") style.column_fill = 1;
        else if (value_lower == "balance-all") style.column_fill = 2;
        return;
    }
    if (prop == "column-width") {
        if (value_lower == "auto") style.column_width = Length::auto_val();
        else {
            auto l = parse_length(value_str);
            if (l) style.column_width = *l;
        }
        return;
    }
    if (prop == "column-gap") {
        auto l = parse_length(value_str);
        if (l) style.column_gap_val = *l;
        return;
    }
    if (prop == "column-rule-width") {
        auto l = parse_length(value_str);
        if (l) style.column_rule_width = l->to_px(0);
        return;
    }
    if (prop == "column-rule-color") {
        auto c = parse_color(value_lower);
        if (c) style.column_rule_color = *c;
        return;
    }
    if (prop == "column-rule-style") {
        if (value_lower == "none") style.column_rule_style = 0;
        else if (value_lower == "solid") style.column_rule_style = 1;
        else if (value_lower == "dashed") style.column_rule_style = 2;
        else if (value_lower == "dotted") style.column_rule_style = 3;
        return;
    }
    if (prop == "columns") {
        // Shorthand: columns: <count> <width> or columns: <width> <count>
        auto parts = split_whitespace(value_str);
        for (auto& part : parts) {
            std::string pl = to_lower(part);
            if (pl == "auto") continue;
            // Try as integer (column-count)
            bool is_count = true;
            for (char ch : part) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) { is_count = false; break; }
            }
            if (is_count && !part.empty()) {
                try { style.column_count = std::stoi(part); } catch (...) {}
            } else {
                // Try as length (column-width)
                auto l = parse_length(part);
                if (l) style.column_width = *l;
            }
        }
        return;
    }
    if (prop == "column-rule") {
        // Shorthand: column-rule: <width> <style> <color>
        auto parts = split_whitespace(value_str);
        for (auto& part : parts) {
            std::string pl = to_lower(part);
            if (pl == "none" || pl == "solid" || pl == "dashed" || pl == "dotted") {
                if (pl == "none") style.column_rule_style = 0;
                else if (pl == "solid") style.column_rule_style = 1;
                else if (pl == "dashed") style.column_rule_style = 2;
                else if (pl == "dotted") style.column_rule_style = 3;
            } else {
                auto c = parse_color(pl);
                if (c) {
                    style.column_rule_color = *c;
                } else {
                    auto l = parse_length(part);
                    if (l) style.column_rule_width = l->to_px(0);
                }
            }
        }
        return;
    }

    // ---- CSS Appearance / -webkit-appearance ----
    if (prop == "appearance" || prop == "-webkit-appearance") {
        if (value_lower == "auto") style.appearance = 0;
        else if (value_lower == "none") style.appearance = 1;
        else if (value_lower == "menulist-button") style.appearance = 2;
        else if (value_lower == "textfield") style.appearance = 3;
        else if (value_lower == "button") style.appearance = 4;
        else style.appearance = 0;
        return;
    }

    // ---- CSS Touch Action ----
    if (prop == "touch-action") {
        if (value_lower == "auto") {
            style.touch_action = 0;
            return;
        }
        if (value_lower == "none") {
            style.touch_action = 1;
            return;
        }
        if (value_lower == "manipulation") {
            style.touch_action = 5;
            return;
        }
        if (value_lower == "pinch-zoom") {
            style.touch_action = 6;
            return;
        }

        bool pan_x = false, pan_y = false;
        auto tokens = split_whitespace(value_lower);
        for (const auto& token : tokens) {
            if (token == "pan-x" || token == "pan-left" || token == "pan-right") pan_x = true;
            else if (token == "pan-y" || token == "pan-up" || token == "pan-down") pan_y = true;
        }
        if (pan_x && pan_y) style.touch_action = 4;
        else if (pan_x) style.touch_action = 2;
        else if (pan_y) style.touch_action = 3;
        else style.touch_action = 0;
        return;
    }

    // ---- CSS Will-Change ----
    if (prop == "will-change") {
        if (value_lower == "auto") style.will_change.clear();
        else style.will_change = value_str;
        return;
    }

    // ---- CSS Container Queries ----
    if (prop == "color-scheme") {
        if (value_lower == "normal") style.color_scheme = 0;
        else if (value_lower == "light") style.color_scheme = 1;
        else if (value_lower == "dark") style.color_scheme = 2;
        else if (value_lower == "light dark" || value_lower == "dark light") style.color_scheme = 3;
        else style.color_scheme = 0;
        return;
    }
    if (prop == "container-type") {
        if (value_lower == "normal") style.container_type = 0;
        else if (value_lower == "size") style.container_type = 1;
        else if (value_lower == "inline-size") style.container_type = 2;
        else if (value_lower == "block-size") style.container_type = 3;
        else style.container_type = 0;
        return;
    }
    if (prop == "container-name") {
        style.container_name = value_str;
        return;
    }
    if (prop == "container") {
        // Shorthand: "name / type" e.g. "sidebar / inline-size"
        auto slash_pos = value_str.find('/');
        if (slash_pos != std::string::npos) {
            std::string name_part = value_str.substr(0, slash_pos);
            std::string type_part = value_str.substr(slash_pos + 1);
            // Trim whitespace
            while (!name_part.empty() && name_part.back() == ' ') name_part.pop_back();
            while (!name_part.empty() && name_part.front() == ' ') name_part.erase(name_part.begin());
            while (!type_part.empty() && type_part.back() == ' ') type_part.pop_back();
            while (!type_part.empty() && type_part.front() == ' ') type_part.erase(type_part.begin());
            style.container_name = name_part;
            std::string type_lower = type_part;
            for (auto& ch : type_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (type_lower == "normal") style.container_type = 0;
            else if (type_lower == "size") style.container_type = 1;
            else if (type_lower == "inline-size") style.container_type = 2;
            else if (type_lower == "block-size") style.container_type = 3;
            else style.container_type = 0;
        } else {
            // No slash — treat entire value as container-type
            if (value_lower == "normal") style.container_type = 0;
            else if (value_lower == "size") style.container_type = 1;
            else if (value_lower == "inline-size") style.container_type = 2;
            else if (value_lower == "block-size") style.container_type = 3;
            else style.container_type = 0;
        }
        return;
    }

    // ---- Forced color adjust ----
    if (prop == "forced-color-adjust") {
        if (value_lower == "auto") style.forced_color_adjust = 0;
        else if (value_lower == "none") style.forced_color_adjust = 1;
        else if (value_lower == "preserve-parent-color") style.forced_color_adjust = 2;
        return;
    }

    // ---- CSS Math properties (MathML) ----
    if (prop == "math-style") {
        if (value_lower == "normal") style.math_style = 0;
        else if (value_lower == "compact") style.math_style = 1;
        return;
    }
    if (prop == "math-depth") {
        if (value_lower == "auto-add") style.math_depth = -1;
        else { try { style.math_depth = std::stoi(value_str); } catch (...) {} }
        return;
    }

    // ---- CSS content-visibility ----
    if (prop == "content-visibility") {
        if (value_lower == "visible") style.content_visibility = 0;
        else if (value_lower == "hidden") style.content_visibility = 1;
        else if (value_lower == "auto") style.content_visibility = 2;
        return;
    }

    // ---- CSS overscroll-behavior ----
    if (prop == "overscroll-behavior") {
        auto ob_parts = split_whitespace(value_lower);
        auto parse_ob = [](const std::string& v) -> int {
            if (v == "auto") return 0; if (v == "contain") return 1; if (v == "none") return 2; return 0;
        };
        if (!ob_parts.empty()) {
            int ob_x = parse_ob(ob_parts[0]);
            int ob_y = (ob_parts.size() >= 2) ? parse_ob(ob_parts[1]) : ob_x;
            style.overscroll_behavior = ob_x;
            style.overscroll_behavior_x = ob_x;
            style.overscroll_behavior_y = ob_y;
        }
        return;
    }

    // ---- CSS overscroll-behavior-x ----
    if (prop == "overscroll-behavior-x") {
        if (value_lower == "auto") {
            style.overscroll_behavior_x = 0;
            style.overscroll_behavior = style.overscroll_behavior_x;
        } else if (value_lower == "contain") {
            style.overscroll_behavior_x = 1;
            style.overscroll_behavior = style.overscroll_behavior_x;
        } else if (value_lower == "none") {
            style.overscroll_behavior_x = 2;
            style.overscroll_behavior = style.overscroll_behavior_x;
        }
        return;
    }

    // ---- CSS overscroll-behavior-y ----
    if (prop == "overscroll-behavior-y") {
        if (value_lower == "auto") {
            style.overscroll_behavior_y = 0;
            style.overscroll_behavior = style.overscroll_behavior_y;
        } else if (value_lower == "contain") {
            style.overscroll_behavior_y = 1;
            style.overscroll_behavior = style.overscroll_behavior_y;
        } else if (value_lower == "none") {
            style.overscroll_behavior_y = 2;
            style.overscroll_behavior = style.overscroll_behavior_y;
        }
        return;
    }

    // ---- CSS paint-order ----
    if (prop == "paint-order") {
        style.paint_order = value_lower;
        return;
    }

    // ---- CSS dominant-baseline ----
    if (prop == "dominant-baseline") {
        if (value_lower == "auto") style.dominant_baseline = 0;
        else if (value_lower == "text-bottom") style.dominant_baseline = 1;
        else if (value_lower == "alphabetic") style.dominant_baseline = 2;
        else if (value_lower == "ideographic") style.dominant_baseline = 3;
        else if (value_lower == "middle") style.dominant_baseline = 4;
        else if (value_lower == "central") style.dominant_baseline = 5;
        else if (value_lower == "mathematical") style.dominant_baseline = 6;
        else if (value_lower == "hanging") style.dominant_baseline = 7;
        else if (value_lower == "text-top") style.dominant_baseline = 8;
        else style.dominant_baseline = 0;
        return;
    }

    // ---- CSS initial-letter ----
    if (prop == "initial-letter") {
        if (value_lower == "normal") {
            style.initial_letter_size = 0;
            style.initial_letter_sink = 0;
            style.initial_letter = 0;
        } else {
            std::istringstream iss(value_str);
            float sz = 0;
            int sk = 0;
            if (iss >> sz) {
                style.initial_letter_size = sz;
                style.initial_letter = sz;
                if (iss >> sk) {
                    style.initial_letter_sink = sk;
                } else {
                    style.initial_letter_sink = static_cast<int>(sz);
                }
            }
        }
        return;
    }

    // ---- CSS initial-letter-align ----
    if (prop == "initial-letter-align") {
        if (value_lower == "border-box") style.initial_letter_align = 1;
        else if (value_lower == "alphabetic") style.initial_letter_align = 2;
        else style.initial_letter_align = 0; // auto
        return;
    }

    // ---- CSS text-emphasis-style ----
    if (prop == "text-emphasis-style") {
        style.text_emphasis_style = value_lower;
        return;
    }

    // ---- CSS text-emphasis-color ----
    if (prop == "text-emphasis-color") {
        auto c = parse_color(value_lower);
        if (c) {
            style.text_emphasis_color = (static_cast<uint32_t>(c->a) << 24) |
                                        (static_cast<uint32_t>(c->r) << 16) |
                                        (static_cast<uint32_t>(c->g) << 8) |
                                        (static_cast<uint32_t>(c->b));
        }
        return;
    }

    // ---- CSS text-emphasis shorthand ----
    if (prop == "text-emphasis") {
        // Shorthand: "style color" or just "style" or just "none"
        // Style keywords: filled/open, dot/circle/double-circle/triangle/sesame
        // The last token that parses as a color is the color; the rest is style.
        if (value_lower == "none") {
            style.text_emphasis_style = "none";
            style.text_emphasis_color = 0;
        } else {
            auto parts = split_whitespace(value_lower);
            std::string style_parts;
            bool found_color = false;
            // Try parsing the last part as a color
            for (int i = static_cast<int>(parts.size()) - 1; i >= 0; i--) {
                auto c = parse_color(parts[i]);
                if (c && !found_color) {
                    style.text_emphasis_color = (static_cast<uint32_t>(c->a) << 24) |
                                                (static_cast<uint32_t>(c->r) << 16) |
                                                (static_cast<uint32_t>(c->g) << 8) |
                                                (static_cast<uint32_t>(c->b));
                    found_color = true;
                } else {
                    if (!style_parts.empty()) style_parts = parts[i] + " " + style_parts;
                    else style_parts = parts[i];
                }
            }
            if (!style_parts.empty()) {
                style.text_emphasis_style = style_parts;
            }
        }
        return;
    }

    // ---- CSS text-emphasis-position ----
    if (prop == "text-emphasis-position") {
        // Values: "over right" (default), "under right", "over left", "under left"
        if (value_lower.find("under") != std::string::npos && value_lower.find("left") != std::string::npos)
            style.text_emphasis_position = 3;
        else if (value_lower.find("over") != std::string::npos && value_lower.find("left") != std::string::npos)
            style.text_emphasis_position = 2;
        else if (value_lower.find("under") != std::string::npos)
            style.text_emphasis_position = 1;
        else
            style.text_emphasis_position = 0; // over right (default)
        return;
    }

    // ---- -webkit-text-stroke / -webkit-text-stroke-width / -webkit-text-stroke-color ----
    if (prop == "-webkit-text-stroke-width") {
        auto l = parse_length(value_lower);
        if (l) style.text_stroke_width = l->to_px();
        return;
    }
    if (prop == "-webkit-text-stroke-color") {
        auto c = parse_color(value_lower);
        if (c) style.text_stroke_color = *c;
        return;
    }
    if (prop == "-webkit-text-stroke") {
        // Shorthand: width color
        auto parts = split_whitespace(value_lower);
        for (const auto& part : parts) {
            auto l = parse_length(part);
            if (l && l->value > 0) {
                style.text_stroke_width = l->to_px();
            } else {
                auto c = parse_color(part);
                if (c) style.text_stroke_color = *c;
            }
        }
        return;
    }
    if (prop == "-webkit-text-fill-color") {
        auto c = parse_color(value_lower);
        if (c) style.text_fill_color = *c;
        return;
    }

    // ---- Hyphens (inherited) ----
    if (prop == "hyphens") {
        if (value_lower == "none") style.hyphens = 0;
        else if (value_lower == "manual") style.hyphens = 1;
        else if (value_lower == "auto") style.hyphens = 2;
        return;
    }

    // ---- Text justify (inherited) ----
    if (prop == "text-justify") {
        if (value_lower == "auto") style.text_justify = 0;
        else if (value_lower == "inter-word") style.text_justify = 1;
        else if (value_lower == "inter-character") style.text_justify = 2;
        else if (value_lower == "none") style.text_justify = 3;
        return;
    }

    // ---- Text underline offset ----
    if (prop == "text-underline-offset") {
        auto l = parse_length(value_str);
        if (l) style.text_underline_offset = l->to_px(0);
        return;
    }

    // ---- Font variant (inherited) ----
    if (prop == "font-variant") {
        if (value_lower == "small-caps") style.font_variant = 1;
        else style.font_variant = 0; // normal
        return;
    }

    // ---- Font variant caps (inherited) ----
    if (prop == "font-variant-caps") {
        if (value_lower == "small-caps") style.font_variant_caps = 1;
        else if (value_lower == "all-small-caps") style.font_variant_caps = 2;
        else if (value_lower == "petite-caps") style.font_variant_caps = 3;
        else if (value_lower == "all-petite-caps") style.font_variant_caps = 4;
        else if (value_lower == "unicase") style.font_variant_caps = 5;
        else if (value_lower == "titling-caps") style.font_variant_caps = 6;
        else style.font_variant_caps = 0; // normal
        return;
    }

    // ---- Font variant numeric (inherited) ----
    if (prop == "font-variant-numeric") {
        if (value_lower == "ordinal") style.font_variant_numeric = 1;
        else if (value_lower == "slashed-zero") style.font_variant_numeric = 2;
        else if (value_lower == "lining-nums") style.font_variant_numeric = 3;
        else if (value_lower == "oldstyle-nums") style.font_variant_numeric = 4;
        else if (value_lower == "proportional-nums") style.font_variant_numeric = 5;
        else if (value_lower == "tabular-nums") style.font_variant_numeric = 6;
        else style.font_variant_numeric = 0; // normal
        return;
    }

    // ---- Font synthesis (inherited) ----
    if (prop == "font-synthesis") {
        if (value_lower == "none") { style.font_synthesis = 0; }
        else {
            int mask = 0;
            std::istringstream iss(value_lower);
            std::string tok;
            while (iss >> tok) {
                if (tok == "weight") mask |= 1;
                else if (tok == "style") mask |= 2;
                else if (tok == "small-caps") mask |= 4;
            }
            style.font_synthesis = mask;
        }
        return;
    }

    // ---- Font variant alternates (inherited) ----
    if (prop == "font-variant-alternates") {
        if (value_lower == "historical-forms") style.font_variant_alternates = 1;
        else style.font_variant_alternates = 0; // normal
        return;
    }

    // ---- Font feature settings (inherited) ----
    if (prop == "font-feature-settings") {
        style.font_feature_settings = parse_font_feature_settings(value_str);
        return;
    }

    // ---- Font variation settings (inherited) ----
    if (prop == "font-variation-settings") {
        style.font_variation_settings = value_str;
        return;
    }

    // ---- Font optical sizing (inherited) ----
    if (prop == "font-optical-sizing") {
        if (value_lower == "none") style.font_optical_sizing = 1;
        else style.font_optical_sizing = 0; // auto
        return;
    }

    // ---- Print color adjust (inherited) ----
    if (prop == "print-color-adjust" || prop == "-webkit-print-color-adjust") {
        if (value_lower == "economy") style.print_color_adjust = 0;
        else if (value_lower == "exact") style.print_color_adjust = 1;
        return;
    }

    // ---- Image orientation (inherited) ----
    if (prop == "image-orientation") {
        if (value_lower == "from-image") { style.image_orientation = 0; style.image_orientation_explicit = true; }
        else if (value_lower == "none") { style.image_orientation = 1; style.image_orientation_explicit = true; }
        else if (value_lower == "flip") { style.image_orientation = 2; style.image_orientation_explicit = true; }
        return;
    }

    // ---- Font kerning (inherited) ----
    if (prop == "font-kerning") {
        if (value_lower == "auto") style.font_kerning = 0;
        else if (value_lower == "normal") style.font_kerning = 1;
        else if (value_lower == "none") style.font_kerning = 2;
        return;
    }

    // ---- Font variant ligatures (inherited) ----
    if (prop == "font-variant-ligatures") {
        if (value_lower == "normal") style.font_variant_ligatures = 0;
        else if (value_lower == "none") style.font_variant_ligatures = 1;
        else if (value_lower == "common-ligatures") style.font_variant_ligatures = 2;
        else if (value_lower == "no-common-ligatures") style.font_variant_ligatures = 3;
        else if (value_lower == "discretionary-ligatures") style.font_variant_ligatures = 4;
        else if (value_lower == "no-discretionary-ligatures") style.font_variant_ligatures = 5;
        return;
    }

    // ---- Font variant east-asian (inherited) ----
    if (prop == "font-variant-east-asian") {
        if (value_lower == "normal") style.font_variant_east_asian = 0;
        else if (value_lower == "jis78") style.font_variant_east_asian = 1;
        else if (value_lower == "jis83") style.font_variant_east_asian = 2;
        else if (value_lower == "jis90") style.font_variant_east_asian = 3;
        else if (value_lower == "jis04") style.font_variant_east_asian = 4;
        else if (value_lower == "simplified") style.font_variant_east_asian = 5;
        else if (value_lower == "traditional") style.font_variant_east_asian = 6;
        else if (value_lower == "full-width") style.font_variant_east_asian = 7;
        else if (value_lower == "proportional-width") style.font_variant_east_asian = 8;
        else if (value_lower == "ruby") style.font_variant_east_asian = 9;
        return;
    }

    // ---- Font palette (inherited) ----
    if (prop == "font-palette") {
        style.font_palette = value_str;
        return;
    }

    // ---- Font variant position (inherited) ----
    if (prop == "font-variant-position") {
        if (value_lower == "normal") style.font_variant_position = 0;
        else if (value_lower == "sub") style.font_variant_position = 1;
        else if (value_lower == "super") style.font_variant_position = 2;
        return;
    }

    // ---- Font language override (inherited) ----
    if (prop == "font-language-override") {
        if (value_lower == "normal") style.font_language_override = "";
        else {
            std::string val = value_str;
            if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'')) {
                val = val.substr(1, val.size() - 2);
            }
            style.font_language_override = val;
        }
        return;
    }

    // ---- Font size adjust (inherited) ----
    if (prop == "font-size-adjust") {
        if (value_lower == "none") {
            style.font_size_adjust = 0;
        } else {
            float v = std::strtof(value_lower.c_str(), nullptr);
            if (v > 0) style.font_size_adjust = v;
            else style.font_size_adjust = 0;
        }
        return;
    }

    // ---- Font stretch (inherited) ----
    if (prop == "font-stretch") {
        if (value_lower == "ultra-condensed") style.font_stretch = 1;
        else if (value_lower == "extra-condensed") style.font_stretch = 2;
        else if (value_lower == "condensed") style.font_stretch = 3;
        else if (value_lower == "semi-condensed") style.font_stretch = 4;
        else if (value_lower == "normal") style.font_stretch = 5;
        else if (value_lower == "semi-expanded") style.font_stretch = 6;
        else if (value_lower == "expanded") style.font_stretch = 7;
        else if (value_lower == "extra-expanded") style.font_stretch = 8;
        else if (value_lower == "ultra-expanded") style.font_stretch = 9;
        else style.font_stretch = 5; // default normal
        return;
    }

    // ---- Text decoration skip ink ----
    if (prop == "text-decoration-skip-ink") {
        if (value_lower == "auto") style.text_decoration_skip_ink = 0;
        else if (value_lower == "none") style.text_decoration_skip_ink = 1;
        else if (value_lower == "all") style.text_decoration_skip_ink = 2;
        else style.text_decoration_skip_ink = 0; // default auto
        return;
    }
    if (prop == "text-decoration-skip") {
        if (value_lower == "none") style.text_decoration_skip = 0;
        else if (value_lower == "objects") style.text_decoration_skip = 1;
        else if (value_lower == "spaces") style.text_decoration_skip = 2;
        else if (value_lower == "ink") style.text_decoration_skip = 3;
        else if (value_lower == "edges") style.text_decoration_skip = 4;
        else if (value_lower == "box-decoration") style.text_decoration_skip = 5;
        return;
    }

    // ---- CSS Transitions ----
    // Helper lambda: parse a CSS time string ("0.3s", "200ms") into milliseconds
    auto parse_time_ms = [](const std::string& s) -> float {
        if (s.size() > 2 && s.substr(s.size() - 2) == "ms") {
            return std::strtof(s.c_str(), nullptr);
        } else if (s.size() > 1 && s.back() == 's') {
            return std::strtof(s.c_str(), nullptr) * 1000.0f;
        }
        return 0.0f;
    };
    // Helper lambda: parse a CSS time string into seconds (for legacy fields)
    auto parse_time_sec = [](const std::string& s) -> float {
        if (s.size() > 2 && s.substr(s.size() - 2) == "ms") {
            return std::strtof(s.c_str(), nullptr) / 1000.0f;
        } else if (s.size() > 1 && s.back() == 's') {
            return std::strtof(s.c_str(), nullptr);
        }
        return 0.0f;
    };
    // Helper lambda: parse timing function name to int
    auto parse_timing_fn = [](const std::string& s) -> int {
        if (s == "ease") return 0;
        if (s == "linear") return 1;
        if (s == "ease-in") return 2;
        if (s == "ease-out") return 3;
        if (s == "ease-in-out") return 4;
        // cubic-bezier() and steps() return 5/6/7 but need extra parsing
        return 0; // default: ease
    };

    // Helper: parse cubic-bezier(x1, y1, x2, y2) and populate params
    // Returns true if successfully parsed, false otherwise
    auto parse_cubic_bezier = [](const std::string& s,
                                  float& x1, float& y1, float& x2, float& y2) -> bool {
        auto pos = s.find("cubic-bezier(");
        if (pos == std::string::npos) return false;
        auto start = pos + 13; // length of "cubic-bezier("
        auto end = s.find(')', start);
        if (end == std::string::npos) return false;
        std::string inner = s.substr(start, end - start);
        // Parse comma-separated or space-separated values
        // Replace commas with spaces for uniform parsing
        for (char& c : inner) { if (c == ',') c = ' '; }
        std::istringstream iss(inner);
        float v1, v2, v3, v4;
        if (iss >> v1 >> v2 >> v3 >> v4) {
            x1 = v1; y1 = v2; x2 = v3; y2 = v4;
            return true;
        }
        return false;
    };

    // Helper: parse steps(n, start|end) and populate params
    // Returns true if successfully parsed, false otherwise
    auto parse_steps = [](const std::string& s,
                           int& count, int& timing_type) -> bool {
        auto pos = s.find("steps(");
        if (pos == std::string::npos) return false;
        auto start = pos + 6; // length of "steps("
        auto end = s.find(')', start);
        if (end == std::string::npos) return false;
        std::string inner = s.substr(start, end - start);
        // Replace commas with spaces
        for (char& c : inner) { if (c == ',') c = ' '; }
        std::istringstream iss(inner);
        int n;
        if (!(iss >> n)) return false;
        count = n > 0 ? n : 1;
        std::string dir;
        timing_type = 6; // default: steps-end (jump-end)
        if (iss >> dir) {
            if (dir == "start" || dir == "jump-start") timing_type = 7;
            else timing_type = 6; // "end", "jump-end", or default
        }
        return true;
    };

    if (prop == "transition-property") {
        style.transition_property = trim(value_str);
        // Also update transitions vector: set property on each existing def, or create new ones
        // Split comma-separated properties
        std::string props_str = trim(value_str);
        std::vector<std::string> prop_list;
        std::istringstream pss(props_str);
        std::string ptok;
        while (std::getline(pss, ptok, ',')) {
            std::string trimmed;
            auto s2 = ptok.find_first_not_of(" \t\n\r");
            if (s2 != std::string::npos) {
                auto e2 = ptok.find_last_not_of(" \t\n\r");
                trimmed = ptok.substr(s2, e2 - s2 + 1);
            }
            if (!trimmed.empty()) prop_list.push_back(to_lower(trimmed));
        }
        style.transitions.resize(prop_list.size());
        for (size_t i = 0; i < prop_list.size(); i++) {
            style.transitions[i].property = prop_list[i];
        }
        return;
    }
    if (prop == "transition-duration") {
        float t = parse_time_sec(value_lower);
        style.transition_duration = t;
        // Update transitions vector
        float ms = parse_time_ms(value_lower);
        if (style.transitions.empty()) style.transitions.emplace_back();
        style.transitions[0].duration_ms = ms;
        return;
    }
    if (prop == "transition-timing-function") {
        float bx1, by1, bx2, by2;
        int steps_n, steps_type;
        if (parse_cubic_bezier(value_lower, bx1, by1, bx2, by2)) {
            style.transition_timing = 5;
            style.transition_bezier_x1 = bx1; style.transition_bezier_y1 = by1;
            style.transition_bezier_x2 = bx2; style.transition_bezier_y2 = by2;
            if (style.transitions.empty()) style.transitions.emplace_back();
            style.transitions[0].timing_function = 5;
            style.transitions[0].bezier_x1 = bx1; style.transitions[0].bezier_y1 = by1;
            style.transitions[0].bezier_x2 = bx2; style.transitions[0].bezier_y2 = by2;
        } else if (parse_steps(value_lower, steps_n, steps_type)) {
            style.transition_timing = steps_type;
            style.transition_steps_count = steps_n;
            if (style.transitions.empty()) style.transitions.emplace_back();
            style.transitions[0].timing_function = steps_type;
            style.transitions[0].steps_count = steps_n;
        } else {
            int tf = parse_timing_fn(value_lower);
            style.transition_timing = tf;
            if (style.transitions.empty()) style.transitions.emplace_back();
            style.transitions[0].timing_function = tf;
        }
        return;
    }
    if (prop == "transition-delay") {
        float t = parse_time_sec(value_lower);
        style.transition_delay = t;
        float ms = parse_time_ms(value_lower);
        if (style.transitions.empty()) style.transitions.emplace_back();
        style.transitions[0].delay_ms = ms;
        return;
    }
    if (prop == "transition") {
        // Shorthand: supports comma-separated multiple transitions
        // e.g. "opacity 0.3s ease, transform 0.5s ease-in"
        // or single: "opacity 0.3s ease 0.1s"
        style.transitions.clear();

        // Split on commas for multiple transitions
        std::vector<std::string> segments;
        {
            std::istringstream css(value_str);
            std::string seg;
            while (std::getline(css, seg, ',')) {
                std::string trimmed;
                auto s2 = seg.find_first_not_of(" \t\n\r");
                if (s2 != std::string::npos) {
                    auto e2 = seg.find_last_not_of(" \t\n\r");
                    trimmed = seg.substr(s2, e2 - s2 + 1);
                }
                if (!trimmed.empty()) segments.push_back(trimmed);
            }
        }

        for (const auto& segment : segments) {
            auto parts = split_whitespace(segment);
            TransitionDef def;
            if (!parts.empty()) {
                def.property = to_lower(parts[0]);
            }
            if (parts.size() > 1) {
                def.duration_ms = parse_time_ms(to_lower(parts[1]));
            }
            if (parts.size() > 2) {
                std::string tf_part = to_lower(parts[2]);
                float bx1, by1, bx2, by2;
                int steps_n, steps_type;
                // Reconstruct the rest of the segment for function-value parsing
                std::string rest_str;
                for (size_t pi = 2; pi < parts.size(); pi++) {
                    if (pi > 2) rest_str += ' ';
                    rest_str += to_lower(parts[pi]);
                }
                if (parse_cubic_bezier(rest_str, bx1, by1, bx2, by2)) {
                    def.timing_function = 5;
                    def.bezier_x1 = bx1; def.bezier_y1 = by1;
                    def.bezier_x2 = bx2; def.bezier_y2 = by2;
                } else if (parse_steps(rest_str, steps_n, steps_type)) {
                    def.timing_function = steps_type;
                    def.steps_count = steps_n;
                } else {
                    def.timing_function = parse_timing_fn(tf_part);
                }
            }
            if (parts.size() > 3) {
                def.delay_ms = parse_time_ms(to_lower(parts[3]));
            }
            style.transitions.push_back(def);
        }

        // Also set legacy scalar fields from the first transition
        if (!style.transitions.empty()) {
            auto& t0 = style.transitions[0];
            style.transition_property = t0.property;
            style.transition_duration = t0.duration_ms / 1000.0f;
            style.transition_timing = t0.timing_function;
            style.transition_delay = t0.delay_ms / 1000.0f;
            style.transition_bezier_x1 = t0.bezier_x1;
            style.transition_bezier_y1 = t0.bezier_y1;
            style.transition_bezier_x2 = t0.bezier_x2;
            style.transition_bezier_y2 = t0.bezier_y2;
            style.transition_steps_count = t0.steps_count;
        }
        return;
    }

    // ---- CSS Animations ----
    if (prop == "animation-name") {
        style.animation_name = trim(value_str);
        return;
    }
    if (prop == "animation-duration") {
        float t = 0;
        if (value_lower.size() > 2 && value_lower.substr(value_lower.size() - 2) == "ms") {
            t = std::strtof(value_lower.c_str(), nullptr) / 1000.0f;
        } else if (value_lower.size() > 1 && value_lower.back() == 's') {
            t = std::strtof(value_lower.c_str(), nullptr);
        }
        style.animation_duration = t;
        return;
    }
    if (prop == "animation-timing-function") {
        float bx1, by1, bx2, by2;
        int steps_n, steps_type;
        if (parse_cubic_bezier(value_lower, bx1, by1, bx2, by2)) {
            style.animation_timing = 5;
            style.animation_bezier_x1 = bx1; style.animation_bezier_y1 = by1;
            style.animation_bezier_x2 = bx2; style.animation_bezier_y2 = by2;
        } else if (parse_steps(value_lower, steps_n, steps_type)) {
            style.animation_timing = steps_type;
            style.animation_steps_count = steps_n;
        } else if (value_lower == "ease") style.animation_timing = 0;
        else if (value_lower == "linear") style.animation_timing = 1;
        else if (value_lower == "ease-in") style.animation_timing = 2;
        else if (value_lower == "ease-out") style.animation_timing = 3;
        else if (value_lower == "ease-in-out") style.animation_timing = 4;
        return;
    }
    if (prop == "animation-delay") {
        float t = 0;
        if (value_lower.size() > 2 && value_lower.substr(value_lower.size() - 2) == "ms") {
            t = std::strtof(value_lower.c_str(), nullptr) / 1000.0f;
        } else if (value_lower.size() > 1 && value_lower.back() == 's') {
            t = std::strtof(value_lower.c_str(), nullptr);
        }
        style.animation_delay = t;
        return;
    }
    if (prop == "animation-iteration-count") {
        if (value_lower == "infinite") style.animation_iteration_count = -1;
        else style.animation_iteration_count = std::strtof(value_lower.c_str(), nullptr);
        return;
    }
    if (prop == "animation-direction") {
        if (value_lower == "normal") style.animation_direction = 0;
        else if (value_lower == "reverse") style.animation_direction = 1;
        else if (value_lower == "alternate") style.animation_direction = 2;
        else if (value_lower == "alternate-reverse") style.animation_direction = 3;
        return;
    }
    if (prop == "animation-fill-mode") {
        if (value_lower == "none") style.animation_fill_mode = 0;
        else if (value_lower == "forwards") style.animation_fill_mode = 1;
        else if (value_lower == "backwards") style.animation_fill_mode = 2;
        else if (value_lower == "both") style.animation_fill_mode = 3;
        return;
    }
    if (prop == "animation-play-state") {
        if (value_lower == "running") style.animation_play_state = 0;
        else if (value_lower == "paused") style.animation_play_state = 1;
        return;
    }

    // ---- CSS animation-composition (NOT inherited) ----
    if (prop == "animation-composition") {
        if (value_lower == "replace") style.animation_composition = 0;
        else if (value_lower == "add") style.animation_composition = 1;
        else if (value_lower == "accumulate") style.animation_composition = 2;
        return;
    }

    // ---- CSS animation-timeline (NOT inherited) ----
    if (prop == "animation-timeline") {
        std::string lower_val = to_lower(value_str);
        style.animation_timeline = value_str;

        if (lower_val == "auto") {
            style.animation_timeline_type = 0;
        } else if (lower_val == "none") {
            style.animation_timeline_type = 1;
        } else if (lower_val.find("scroll(") != std::string::npos) {
            style.animation_timeline_type = 2;
            style.animation_timeline_raw = value_str;
            // Parse axis from scroll(block), scroll(inline), scroll(x), scroll(y)
            if (value_str.find("inline") != std::string::npos) {
                style.animation_timeline_axis = 1;
            } else if (value_str.find("x") != std::string::npos) {
                style.animation_timeline_axis = 2;
            } else if (value_str.find("y") != std::string::npos) {
                style.animation_timeline_axis = 3;
            } else {
                style.animation_timeline_axis = 0;
            }
        } else if (lower_val.find("view(") != std::string::npos) {
            style.animation_timeline_type = 3;
            style.animation_timeline_raw = value_str;
            style.animation_timeline_axis = 0;
        }
        return;
    }

    if (prop == "animation") {
        // Shorthand: "name duration timing-function delay count direction fill-mode"
        auto parts = split_whitespace(value_str);
        if (!parts.empty()) {
            style.animation_name = parts[0];
            if (parts.size() > 1) {
                std::string dur = to_lower(parts[1]);
                float t = 0;
                if (dur.size() > 2 && dur.substr(dur.size() - 2) == "ms") {
                    t = std::strtof(dur.c_str(), nullptr) / 1000.0f;
                } else if (dur.size() > 1 && dur.back() == 's') {
                    t = std::strtof(dur.c_str(), nullptr);
                }
                style.animation_duration = t;
            }
            if (parts.size() > 2) {
                std::string tf = to_lower(parts[2]);
                float bx1, by1, bx2, by2;
                int steps_n, steps_type;
                // Reconstruct remaining for function-value parsing
                std::string rest_str;
                for (size_t pi = 2; pi < parts.size(); pi++) {
                    if (pi > 2) rest_str += ' ';
                    rest_str += to_lower(parts[pi]);
                }
                if (parse_cubic_bezier(rest_str, bx1, by1, bx2, by2)) {
                    style.animation_timing = 5;
                    style.animation_bezier_x1 = bx1; style.animation_bezier_y1 = by1;
                    style.animation_bezier_x2 = bx2; style.animation_bezier_y2 = by2;
                } else if (parse_steps(rest_str, steps_n, steps_type)) {
                    style.animation_timing = steps_type;
                    style.animation_steps_count = steps_n;
                } else if (tf == "ease") style.animation_timing = 0;
                else if (tf == "linear") style.animation_timing = 1;
                else if (tf == "ease-in") style.animation_timing = 2;
                else if (tf == "ease-out") style.animation_timing = 3;
                else if (tf == "ease-in-out") style.animation_timing = 4;
            }
            if (parts.size() > 3) {
                std::string del = to_lower(parts[3]);
                float t = 0;
                if (del.size() > 2 && del.substr(del.size() - 2) == "ms") {
                    t = std::strtof(del.c_str(), nullptr) / 1000.0f;
                } else if (del.size() > 1 && del.back() == 's') {
                    t = std::strtof(del.c_str(), nullptr);
                }
                style.animation_delay = t;
            }
            if (parts.size() > 4) {
                std::string ic = to_lower(parts[4]);
                if (ic == "infinite") style.animation_iteration_count = -1;
                else style.animation_iteration_count = std::strtof(ic.c_str(), nullptr);
            }
            if (parts.size() > 5) {
                std::string dir = to_lower(parts[5]);
                if (dir == "normal") style.animation_direction = 0;
                else if (dir == "reverse") style.animation_direction = 1;
                else if (dir == "alternate") style.animation_direction = 2;
                else if (dir == "alternate-reverse") style.animation_direction = 3;
            }
            if (parts.size() > 6) {
                std::string fm = to_lower(parts[6]);
                if (fm == "none") style.animation_fill_mode = 0;
                else if (fm == "forwards") style.animation_fill_mode = 1;
                else if (fm == "backwards") style.animation_fill_mode = 2;
                else if (fm == "both") style.animation_fill_mode = 3;
            }
        }
        return;
    }

    // ---- CSS Grid layout ----
    if (prop == "grid-template-columns" || prop == "grid-template-rows") {
        auto parse_subgrid_template = [&](std::string &target, bool &is_subgrid_flag) {
            std::string lower = to_lower(value_str);
            bool is_subgrid = false;

            if (lower == "subgrid") {
                is_subgrid = true;
            } else if (lower.size() > 7 && lower.substr(0, 7) == "subgrid" &&
                      std::isspace(static_cast<unsigned char>(lower[7]))) {
                is_subgrid = true;
            }

            if (is_subgrid) {
                is_subgrid_flag = true;
                target = value_str.size() > 7 ? trim(value_str.substr(7)) : "";
            } else {
                is_subgrid_flag = false;
                target = value_str;
            }
        };

        if (prop == "grid-template-columns") {
            parse_subgrid_template(style.grid_template_columns, style.grid_template_columns_is_subgrid);
        } else {
            parse_subgrid_template(style.grid_template_rows, style.grid_template_rows_is_subgrid);
        }
        return;
    }
    if (prop == "grid-column") {
        style.grid_column = value_str;
        return;
    }
    if (prop == "grid-row") {
        style.grid_row = value_str;
        return;
    }
    // Grid individual longhands: grid-column-start, grid-column-end, grid-row-start, grid-row-end
    if (prop == "grid-column-start") {
        style.grid_column_start = value_str;
        // Rebuild grid_column shorthand from longhands
        if (!style.grid_column_end.empty()) {
            style.grid_column = value_str + " / " + style.grid_column_end;
        } else {
            style.grid_column = value_str;
        }
        return;
    }
    if (prop == "grid-column-end") {
        style.grid_column_end = value_str;
        // Rebuild grid_column shorthand from longhands
        if (!style.grid_column_start.empty()) {
            style.grid_column = style.grid_column_start + " / " + value_str;
        } else {
            style.grid_column = "auto / " + value_str;
        }
        return;
    }
    if (prop == "grid-row-start") {
        style.grid_row_start = value_str;
        // Rebuild grid_row shorthand from longhands
        if (!style.grid_row_end.empty()) {
            style.grid_row = value_str + " / " + style.grid_row_end;
        } else {
            style.grid_row = value_str;
        }
        return;
    }
    if (prop == "grid-row-end") {
        style.grid_row_end = value_str;
        // Rebuild grid_row shorthand from longhands
        if (!style.grid_row_start.empty()) {
            style.grid_row = style.grid_row_start + " / " + value_str;
        } else {
            style.grid_row = "auto / " + value_str;
        }
        return;
    }
    if (prop == "grid-auto-rows") {
        style.grid_auto_rows = value_str;
        return;
    }
    if (prop == "grid-auto-columns") {
        style.grid_auto_columns = value_str;
        return;
    }
    if (prop == "grid-auto-flow") {
        if (value_lower == "row") style.grid_auto_flow = 0;
        else if (value_lower == "column") style.grid_auto_flow = 1;
        else if (value_lower == "row dense" || value_lower == "dense row" || value_lower == "dense") style.grid_auto_flow = 2;
        else if (value_lower == "column dense" || value_lower == "dense column") style.grid_auto_flow = 3;
        return;
    }
    if (prop == "grid-template-areas") {
        // Build a properly quoted string by iterating raw ComponentValues.
        // The CSS tokenizer strips quotes from String tokens (cv.unit == "string"),
        // so we must reconstruct them to preserve per-row boundaries.
        std::string areas;
        for (const auto& cv : decl.values) {
            if (cv.type == ComponentValue::Token && cv.unit == "string") {
                // Each quoted string is one row of the grid template
                if (!areas.empty()) areas += ' ';
                areas += '"' + cv.value + '"';
            } else if (cv.type == ComponentValue::Token && !cv.value.empty() &&
                       cv.value != " " && cv.value != "none") {
                // Bare ident fallback (unquoted, non-standard usage)
                if (!areas.empty()) areas += ' ';
                areas += cv.value;
            }
        }
        style.grid_template_areas = areas.empty() ? value_str : areas;
        return;
    }
    if (prop == "grid-template" || prop == "grid") {
        // grid-template: <rows> / <columns>
        auto slash_pos = value_str.find('/');
        if (slash_pos != std::string::npos) {
            auto rows = trim(value_str.substr(0, slash_pos));
            auto cols = trim(value_str.substr(slash_pos + 1));
            if (!rows.empty()) style.grid_template_rows = rows;
            if (!cols.empty()) style.grid_template_columns = cols;
        } else {
            // Single value: treat as rows
            style.grid_template_rows = value_str;
        }
        return;
    }
    if (prop == "grid-area") {
        style.grid_area = value_str;
        return;
    }
    if (prop == "justify-items") {
        if (value_lower == "start") style.justify_items = 0;
        else if (value_lower == "end") style.justify_items = 1;
        else if (value_lower == "center") style.justify_items = 2;
        else if (value_lower == "stretch") style.justify_items = 3;
        return;
    }
    if (prop == "align-content") {
        if (value_lower == "start") style.align_content = 0;
        else if (value_lower == "end") style.align_content = 1;
        else if (value_lower == "center") style.align_content = 2;
        else if (value_lower == "stretch") style.align_content = 3;
        else if (value_lower == "space-between") style.align_content = 4;
        else if (value_lower == "space-around") style.align_content = 5;
        return;
    }

    // ---- CSS inset shorthand ----
    if (prop == "inset") {
        auto parts = split_whitespace(value_lower);
        if (parts.size() == 1) {
            auto v = parse_length(parts[0]);
            if (v) { style.top = *v; style.right_pos = *v; style.bottom = *v; style.left_pos = *v; }
        } else if (parts.size() == 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) { style.top = *v1; style.bottom = *v1; }
            if (v2) { style.right_pos = *v2; style.left_pos = *v2; }
        } else if (parts.size() == 3) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            if (v1) style.top = *v1;
            if (v2) { style.right_pos = *v2; style.left_pos = *v2; }
            if (v3) style.bottom = *v3;
        } else if (parts.size() >= 4) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            auto v4 = parse_length(parts[3]);
            if (v1) style.top = *v1;
            if (v2) style.right_pos = *v2;
            if (v3) style.bottom = *v3;
            if (v4) style.left_pos = *v4;
        }
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }

    // ---- CSS inset-block logical shorthand ----
    if (prop == "inset-block") {
        auto parts = split_whitespace(value_lower);
        if (parts.size() == 1) {
            auto v = parse_length(parts[0]);
            if (v) { style.top = *v; style.bottom = *v; }
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) style.top = *v1;
            if (v2) style.bottom = *v2;
        }
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }

    auto apply_inset_inline = [&](const std::string& side, const Length& value) {
        EdgeSizes inline_edges = {style.top, style.right_pos, style.bottom, style.left_pos};
        apply_inline_property(inline_edges, side, value, style.direction);
        style.right_pos = inline_edges.right;
        style.left_pos = inline_edges.left;
    };

    // ---- CSS inset-inline logical shorthand ----
    if (prop == "inset-inline") {
        auto parts = split_whitespace(value_lower);
        if (parts.size() == 1) {
            auto v = parse_length(parts[0]);
            if (v) {
                apply_inset_inline("start", *v);
                apply_inset_inline("end", *v);
            }
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) apply_inset_inline("start", *v1);
            if (v2) apply_inset_inline("end", *v2);
        }
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }
    // ---- CSS inset logical longhands ----
    if (prop == "inset-block-start") {
        auto v = parse_length(value_lower);
        if (v) style.top = *v;
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }
    if (prop == "inset-block-end") {
        auto v = parse_length(value_lower);
        if (v) style.bottom = *v;
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }
    if (prop == "inset-inline-start") {
        auto v = parse_length(value_lower);
        if (v) apply_inset_inline("start", *v);
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }
    if (prop == "inset-inline-end") {
        auto v = parse_length(value_lower);
        if (v) apply_inset_inline("end", *v);
        if (style.position == Position::Static)
            style.position = Position::Relative;
        return;
    }

    // ---- CSS place-content shorthand ----
    if (prop == "place-content") {
        auto parts = split_whitespace(value_lower);
        auto parse_align_val = [](const std::string& s) -> int {
            if (s == "flex-start" || s == "start") return 0;
            if (s == "flex-end" || s == "end") return 1;
            if (s == "center") return 2;
            if (s == "stretch") return 3;
            if (s == "space-between") return 4;
            if (s == "space-around") return 5;
            return 0;
        };
        auto int_to_jc = [](int v) -> JustifyContent {
            switch (v) {
                case 0: return JustifyContent::FlexStart;
                case 1: return JustifyContent::FlexEnd;
                case 2: return JustifyContent::Center;
                case 3: return JustifyContent::FlexStart;
                case 4: return JustifyContent::SpaceBetween;
                case 5: return JustifyContent::SpaceAround;
                default: return JustifyContent::FlexStart;
            }
        };
        if (parts.size() == 1) {
            int v = parse_align_val(parts[0]);
            style.align_content = v;
            style.justify_content = int_to_jc(v);
        } else if (parts.size() >= 2) {
            style.align_content = parse_align_val(parts[0]);
            style.justify_content = int_to_jc(parse_align_val(parts[1]));
        }
        return;
    }

    // ---- CSS text-underline-position ----
    if (prop == "text-underline-position") {
        if (value_lower == "auto") style.text_underline_position = 0;
        else if (value_lower == "under") style.text_underline_position = 1;
        else if (value_lower == "left") style.text_underline_position = 2;
        else if (value_lower == "right") style.text_underline_position = 3;
        return;
    }

    // ---- Scroll margin shorthand + longhands ----
    if (prop == "scroll-margin") {
        auto parts = split_whitespace(value_lower);
        float t=0, r=0, b=0, l=0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) t = r = b = l = v->to_px(0);
        } else if (parts.size() == 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) t = b = v1->to_px(0);
            if (v2) r = l = v2->to_px(0);
        } else if (parts.size() == 3) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            if (v1) t = v1->to_px(0);
            if (v2) r = l = v2->to_px(0);
            if (v3) b = v3->to_px(0);
        } else if (parts.size() >= 4) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            auto v4 = parse_length(parts[3]);
            if (v1) t = v1->to_px(0);
            if (v2) r = v2->to_px(0);
            if (v3) b = v3->to_px(0);
            if (v4) l = v4->to_px(0);
        }
        style.scroll_margin_top = t;
        style.scroll_margin_right = r;
        style.scroll_margin_bottom = b;
        style.scroll_margin_left = l;
        return;
    }
    if (prop == "scroll-margin-top") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_top = v->to_px(0);
        return;
    }
    if (prop == "scroll-margin-right") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_right = v->to_px(0);
        return;
    }
    if (prop == "scroll-margin-bottom") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_bottom = v->to_px(0);
        return;
    }
    if (prop == "scroll-margin-left") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_left = v->to_px(0);
        return;
    }

    // ---- Scroll margin logical properties (block/inline) ----
    if (prop == "scroll-margin-block-start") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_top = v->to_px(0); // maps to top in horizontal-tb
        return;
    }
    if (prop == "scroll-margin-block-end") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_bottom = v->to_px(0); // maps to bottom in horizontal-tb
        return;
    }
    if (prop == "scroll-margin-inline-start") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_left = v->to_px(0); // maps to left in LTR
        return;
    }
    if (prop == "scroll-margin-inline-end") {
        auto v = parse_length(value_str);
        if (v) style.scroll_margin_right = v->to_px(0); // maps to right in LTR
        return;
    }
    if (prop == "scroll-margin-inline") {
        auto parts = split_whitespace(value_lower);
        float inline_start = 0, inline_end = 0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) inline_start = inline_end = v->to_px(0);
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) inline_start = v1->to_px(0);
            if (v2) inline_end = v2->to_px(0);
        }
        style.scroll_margin_left = inline_start;
        style.scroll_margin_right = inline_end;
        return;
    }
    if (prop == "scroll-margin-block") {
        auto parts = split_whitespace(value_lower);
        float block_start = 0, block_end = 0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) block_start = block_end = v->to_px(0);
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) block_start = v1->to_px(0);
            if (v2) block_end = v2->to_px(0);
        }
        style.scroll_margin_top = block_start;
        style.scroll_margin_bottom = block_end;
        return;
    }

    // ---- Scroll padding shorthand + longhands ----
    if (prop == "scroll-padding") {
        auto parts = split_whitespace(value_lower);
        float t=0, r=0, b=0, l=0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) t = r = b = l = v->to_px(0);
        } else if (parts.size() == 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) t = b = v1->to_px(0);
            if (v2) r = l = v2->to_px(0);
        } else if (parts.size() == 3) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            if (v1) t = v1->to_px(0);
            if (v2) r = l = v2->to_px(0);
            if (v3) b = v3->to_px(0);
        } else if (parts.size() >= 4) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            auto v3 = parse_length(parts[2]);
            auto v4 = parse_length(parts[3]);
            if (v1) t = v1->to_px(0);
            if (v2) r = v2->to_px(0);
            if (v3) b = v3->to_px(0);
            if (v4) l = v4->to_px(0);
        }
        style.scroll_padding_top = t;
        style.scroll_padding_right = r;
        style.scroll_padding_bottom = b;
        style.scroll_padding_left = l;
        return;
    }
    if (prop == "scroll-padding-top") {
        auto v = parse_length(value_str);
        if (v) style.scroll_padding_top = v->to_px(0);
        return;
    }
    if (prop == "scroll-padding-right") {
        auto v = parse_length(value_str);
        if (v) style.scroll_padding_right = v->to_px(0);
        return;
    }
    if (prop == "scroll-padding-bottom") {
        auto v = parse_length(value_str);
        if (v) style.scroll_padding_bottom = v->to_px(0);
        return;
    }
    if (prop == "scroll-padding-left") {
        auto v = parse_length(value_str);
        if (v) style.scroll_padding_left = v->to_px(0);
        return;
    }
    if (prop == "scroll-padding-inline") {
        auto parts = split_whitespace(value_lower);
        float inline_start = 0, inline_end = 0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) inline_start = inline_end = v->to_px(0);
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) inline_start = v1->to_px(0);
            if (v2) inline_end = v2->to_px(0);
        }
        style.scroll_padding_left = inline_start;
        style.scroll_padding_right = inline_end;
        return;
    }
    if (prop == "scroll-padding-block") {
        auto parts = split_whitespace(value_lower);
        float block_start = 0, block_end = 0;
        if (parts.size() == 1) {
            auto v = parse_length(value_str);
            if (v) block_start = block_end = v->to_px(0);
        } else if (parts.size() >= 2) {
            auto v1 = parse_length(parts[0]);
            auto v2 = parse_length(parts[1]);
            if (v1) block_start = v1->to_px(0);
            if (v2) block_end = v2->to_px(0);
        }
        style.scroll_padding_top = block_start;
        style.scroll_padding_bottom = block_end;
        return;
    }

    // ---- Text rendering (inherited) ----
    if (prop == "text-rendering") {
        if (value_lower == "auto") style.text_rendering = 0;
        else if (value_lower == "optimizespeed") style.text_rendering = 1;
        else if (value_lower == "optimizelegibility") style.text_rendering = 2;
        else if (value_lower == "geometricprecision") style.text_rendering = 3;
        return;
    }

    // ---- Font smoothing (inherited) ----
    if (prop == "font-smooth" || prop == "-webkit-font-smoothing") {
        if (value_lower == "auto") style.font_smooth = 0;
        else if (value_lower == "none") style.font_smooth = 1;
        else if (value_lower == "antialiased") style.font_smooth = 2;
        else if (value_lower == "subpixel-antialiased") style.font_smooth = 3;
        return;
    }

    // ---- Text size adjust (inherited) ----
    if (prop == "text-size-adjust" || prop == "-webkit-text-size-adjust") {
        if (value_lower == "auto") style.text_size_adjust = "auto";
        else if (value_lower == "none") style.text_size_adjust = "none";
        else style.text_size_adjust = value_str; // preserve percentage string
        return;
    }

    // ---- Ruby align (inherited) ----
    if (prop == "ruby-align") {
        if (value_lower == "space-around") style.ruby_align = 0;
        else if (value_lower == "start") style.ruby_align = 1;
        else if (value_lower == "center") style.ruby_align = 2;
        else if (value_lower == "space-between") style.ruby_align = 3;
        return;
    }

    // ---- Ruby position (inherited) ----
    if (prop == "ruby-position") {
        if (value_lower == "over") style.ruby_position = 0;
        else if (value_lower == "under") style.ruby_position = 1;
        else if (value_lower == "inter-character") style.ruby_position = 2;
        return;
    }

    // ---- Ruby overhang ----
    if (prop == "ruby-overhang") {
        if (value_lower == "auto") style.ruby_overhang = 0;
        else if (value_lower == "none") style.ruby_overhang = 1;
        else if (value_lower == "start") style.ruby_overhang = 2;
        else if (value_lower == "end") style.ruby_overhang = 3;
        return;
    }

    // ---- Text combine upright (NOT inherited) ----
    if (prop == "text-combine-upright") {
        if (value_lower == "none") style.text_combine_upright = 0;
        else if (value_lower == "all") style.text_combine_upright = 1;
        else if (value_lower == "digits") style.text_combine_upright = 2;
        return;
    }

    // ---- Text orientation (inherited) ----
    if (prop == "text-orientation") {
        if (value_lower == "mixed") style.text_orientation = 0;
        else if (value_lower == "upright") style.text_orientation = 1;
        else if (value_lower == "sideways") style.text_orientation = 2;
        return;
    }

    // ---- CSS backface-visibility ----
    if (prop == "backface-visibility") {
        if (value_lower == "visible") style.backface_visibility = 0;
        else if (value_lower == "hidden") style.backface_visibility = 1;
        return;
    }

    // ---- CSS overflow-anchor ----
    if (prop == "overflow-anchor") {
        if (value_lower == "auto") style.overflow_anchor = 0;
        else if (value_lower == "none") style.overflow_anchor = 1;
        return;
    }
    if (prop == "overflow-clip-margin") {
        auto v = parse_length(value_str);
        if (v) style.overflow_clip_margin = v->to_px(0);
        else style.overflow_clip_margin = 0.0f;
        return;
    }

    // ---- CSS perspective ----
    if (prop == "perspective") {
        if (value_lower == "none") {
            style.perspective = 0;
        } else {
            auto v = parse_length(value_str);
            if (v) style.perspective = v->to_px(0);
        }
        return;
    }

    // ---- CSS transform-style ----
    if (prop == "transform-style") {
        if (value_lower == "flat") style.transform_style = 0;
        else if (value_lower == "preserve-3d") style.transform_style = 1;
        return;
    }

    // ---- CSS transform-box (NOT inherited) ----
    if (prop == "transform-box") {
        if (value_lower == "content-box") style.transform_box = 0;
        else if (value_lower == "border-box") style.transform_box = 1;
        else if (value_lower == "fill-box") style.transform_box = 2;
        else if (value_lower == "stroke-box") style.transform_box = 3;
        else if (value_lower == "view-box") style.transform_box = 4;
        return;
    }

    // ---- CSS transform-origin ----
    if (prop == "transform-origin") {
        // Returns a Length for a single origin component token.
        // Keywords map to percentages; lengths (px/em/%) are returned as-is.
        auto parse_origin_token = [](const std::string& s) -> css::Length {
            if (s == "left" || s == "top") return css::Length::percent(0.0f);
            if (s == "center") return css::Length::percent(50.0f);
            if (s == "right" || s == "bottom") return css::Length::percent(100.0f);
            if (s.size() > 1 && s.back() == '%') {
                try { return css::Length::percent(std::stof(s.substr(0, s.size()-1))); } catch(...) {}
            }
            // Try to parse as a length (px, em, rem, etc.)
            auto len = parse_length(s);
            if (len) return *len;
            // Default: center
            return css::Length::percent(50.0f);
        };
        auto parts = split_whitespace(value_lower);
        if (parts.size() >= 2) {
            auto lx = parse_origin_token(parts[0]);
            auto ly = parse_origin_token(parts[1]);
            style.transform_origin_x_len = lx;
            style.transform_origin_y_len = ly;
            // Keep legacy float field in sync (for percentage values)
            style.transform_origin_x = (lx.unit == css::Length::Unit::Percent) ? lx.value : 50.0f;
            style.transform_origin_y = (ly.unit == css::Length::Unit::Percent) ? ly.value : 50.0f;
            // Optional 3rd value: z-component (only px allowed in CSS spec)
            if (parts.size() >= 3) {
                auto lz = parse_length(parts[2]);
                if (lz) style.transform_origin_z = lz->to_px();
            }
        } else if (parts.size() == 1) {
            auto lx = parse_origin_token(parts[0]);
            style.transform_origin_x_len = lx;
            style.transform_origin_y_len = css::Length::percent(50.0f);
            style.transform_origin_x = (lx.unit == css::Length::Unit::Percent) ? lx.value : 50.0f;
            style.transform_origin_y = 50.0f;
        }
        return;
    }

    // ---- CSS perspective-origin ----
    if (prop == "perspective-origin") {
        auto parse_origin_token = [](const std::string& s) -> css::Length {
            if (s == "left" || s == "top") return css::Length::percent(0.0f);
            if (s == "center") return css::Length::percent(50.0f);
            if (s == "right" || s == "bottom") return css::Length::percent(100.0f);
            if (s.size() > 1 && s.back() == '%') {
                try { return css::Length::percent(std::stof(s.substr(0, s.size()-1))); } catch(...) {}
            }
            auto len = parse_length(s);
            if (len) return *len;
            return css::Length::percent(50.0f);
        };
        auto parts = split_whitespace(value_lower);
        if (parts.size() >= 2) {
            auto lx = parse_origin_token(parts[0]);
            auto ly = parse_origin_token(parts[1]);
            style.perspective_origin_x_len = lx;
            style.perspective_origin_y_len = ly;
            style.perspective_origin_x = (lx.unit == css::Length::Unit::Percent) ? lx.value : 50.0f;
            style.perspective_origin_y = (ly.unit == css::Length::Unit::Percent) ? ly.value : 50.0f;
        } else if (parts.size() == 1) {
            auto lx = parse_origin_token(parts[0]);
            style.perspective_origin_x_len = lx;
            style.perspective_origin_y_len = css::Length::percent(50.0f);
            style.perspective_origin_x = (lx.unit == css::Length::Unit::Percent) ? lx.value : 50.0f;
            style.perspective_origin_y = 50.0f;
        }
        return;
    }

    // ---- SVG fill property ----
    if (prop == "fill") {
        if (value_lower == "none") {
            style.svg_fill_none = true;
        } else {
            auto c = parse_color(value_lower);
            if (c) {
                style.svg_fill_color = (static_cast<uint32_t>(c->a) << 24) |
                                       (static_cast<uint32_t>(c->r) << 16) |
                                       (static_cast<uint32_t>(c->g) << 8) |
                                       static_cast<uint32_t>(c->b);
                style.svg_fill_none = false;
            }
        }
        return;
    }

    // ---- SVG stroke property ----
    if (prop == "stroke") {
        if (value_lower == "none") {
            style.svg_stroke_none = true;
        } else {
            auto c = parse_color(value_lower);
            if (c) {
                style.svg_stroke_color = (static_cast<uint32_t>(c->a) << 24) |
                                         (static_cast<uint32_t>(c->r) << 16) |
                                         (static_cast<uint32_t>(c->g) << 8) |
                                         static_cast<uint32_t>(c->b);
                style.svg_stroke_none = false;
            }
        }
        return;
    }

    // ---- SVG fill-opacity ----
    if (prop == "fill-opacity") {
        try { style.svg_fill_opacity = std::clamp(std::stof(value_str), 0.0f, 1.0f); } catch (...) {}
        return;
    }

    // ---- SVG stroke-opacity ----
    if (prop == "stroke-opacity") {
        try { style.svg_stroke_opacity = std::clamp(std::stof(value_str), 0.0f, 1.0f); } catch (...) {}
        return;
    }

    // ---- SVG stroke-width (CSS cascade) ----
    if (prop == "stroke-width") {
        try { style.svg_stroke_width = std::stof(value_str); } catch (...) {}
        return;
    }

    // ---- SVG stroke-linecap ----
    if (prop == "stroke-linecap") {
        if (value_lower == "butt") style.svg_stroke_linecap = 0;
        else if (value_lower == "round") style.svg_stroke_linecap = 1;
        else if (value_lower == "square") style.svg_stroke_linecap = 2;
        return;
    }

    // ---- SVG stroke-linejoin ----
    if (prop == "stroke-linejoin") {
        if (value_lower == "miter") style.svg_stroke_linejoin = 0;
        else if (value_lower == "round") style.svg_stroke_linejoin = 1;
        else if (value_lower == "bevel") style.svg_stroke_linejoin = 2;
        return;
    }

    // ---- SVG stroke-dasharray (CSS cascade) ----
    if (prop == "stroke-dasharray") {
        style.svg_stroke_dasharray_str = value_str;
        return;
    }

    // ---- SVG stroke-dashoffset (CSS cascade) ----
    if (prop == "stroke-dashoffset") {
        // Parsed at render time since it goes directly to LayoutNode
        return;
    }

    // ---- SVG text-anchor (CSS cascade) ----
    if (prop == "text-anchor") {
        if (value_lower == "start") style.svg_text_anchor = 0;
        else if (value_lower == "middle") style.svg_text_anchor = 1;
        else if (value_lower == "end") style.svg_text_anchor = 2;
        return;
    }

    // ---- SVG fill-rule ----
    if (prop == "fill-rule") {
        if (value_lower == "nonzero") style.fill_rule = 0;
        else if (value_lower == "evenodd") style.fill_rule = 1;
        return;
    }

    // ---- SVG clip-rule ----
    if (prop == "clip-rule") {
        if (value_lower == "nonzero") style.clip_rule = 0;
        else if (value_lower == "evenodd") style.clip_rule = 1;
        return;
    }

    // ---- SVG stroke-miterlimit ----
    if (prop == "stroke-miterlimit") {
        try { style.stroke_miterlimit = std::stof(value_str); } catch (...) {}
        return;
    }

    // ---- SVG shape-rendering ----
    if (prop == "shape-rendering") {
        if (value_lower == "auto") style.shape_rendering = 0;
        else if (value_lower == "optimizespeed") style.shape_rendering = 1;
        else if (value_lower == "crispedges") style.shape_rendering = 2;
        else if (value_lower == "geometricprecision") style.shape_rendering = 3;
        return;
    }

    // ---- SVG vector-effect ----
    if (prop == "vector-effect") {
        if (value_lower == "none") style.vector_effect = 0;
        else if (value_lower == "non-scaling-stroke") style.vector_effect = 1;
        return;
    }

    // ---- SVG stop-color ----
    if (prop == "stop-color") {
        auto c = parse_color(value_lower);
        if (c) {
            style.stop_color = (static_cast<uint32_t>(c->a) << 24) |
                               (static_cast<uint32_t>(c->r) << 16) |
                               (static_cast<uint32_t>(c->g) << 8) |
                               static_cast<uint32_t>(c->b);
        }
        return;
    }

    // ---- SVG stop-opacity ----
    if (prop == "stop-opacity") {
        try { style.stop_opacity = std::clamp(std::stof(value_str), 0.0f, 1.0f); } catch (...) {}
        return;
    }

    // ---- SVG flood-color ----
    if (prop == "flood-color") {
        auto c = parse_color(value_lower);
        if (c) {
            style.flood_color = (static_cast<uint32_t>(c->a) << 24) |
                               (static_cast<uint32_t>(c->r) << 16) |
                               (static_cast<uint32_t>(c->g) << 8) |
                               static_cast<uint32_t>(c->b);
        }
        return;
    }

    // ---- SVG flood-opacity ----
    if (prop == "flood-opacity") {
        try { style.flood_opacity = std::clamp(std::stof(value_str), 0.0f, 1.0f); } catch (...) {}
        return;
    }

    // ---- SVG lighting-color ----
    if (prop == "lighting-color") {
        auto c = parse_color(value_lower);
        if (c) {
            style.lighting_color = (static_cast<uint32_t>(c->a) << 24) |
                                  (static_cast<uint32_t>(c->r) << 16) |
                                  (static_cast<uint32_t>(c->g) << 8) |
                                  static_cast<uint32_t>(c->b);
        }
        return;
    }

    // ---- SVG marker properties ----
    if (prop == "marker") {
        style.marker_shorthand = value_str;
        style.marker_start = value_str;
        style.marker_mid = value_str;
        style.marker_end = value_str;
        return;
    }
    if (prop == "marker-start") {
        style.marker_start = value_str;
        return;
    }
    if (prop == "marker-mid") {
        style.marker_mid = value_str;
        return;
    }
    if (prop == "marker-end") {
        style.marker_end = value_str;
        return;
    }

    // ---- CSS scrollbar-color ----
    if (prop == "scrollbar-color") {
        if (value_lower == "auto") {
            style.scrollbar_thumb_color = 0;
            style.scrollbar_track_color = 0;
        } else {
            auto parts = split_whitespace(value_str);
            if (parts.size() >= 2) {
                auto c1 = parse_color(parts[0]);
                auto c2 = parse_color(parts[1]);
                if (c1) style.scrollbar_thumb_color = (static_cast<uint32_t>(c1->a) << 24) |
                    (static_cast<uint32_t>(c1->r) << 16) |
                    (static_cast<uint32_t>(c1->g) << 8) |
                    (static_cast<uint32_t>(c1->b));
                if (c2) style.scrollbar_track_color = (static_cast<uint32_t>(c2->a) << 24) |
                    (static_cast<uint32_t>(c2->r) << 16) |
                    (static_cast<uint32_t>(c2->g) << 8) |
                    (static_cast<uint32_t>(c2->b));
            }
        }
        return;
    }

    // ---- CSS scrollbar-width ----
    if (prop == "scrollbar-width") {
        if (value_lower == "auto") style.scrollbar_width = 0;
        else if (value_lower == "thin") style.scrollbar_width = 1;
        else if (value_lower == "none") style.scrollbar_width = 2;
        return;
    }

    // ---- CSS scrollbar-gutter ----
    if (prop == "scrollbar-gutter") {
        if (value_lower == "auto") style.scrollbar_gutter = 0;
        else if (value_lower == "stable") style.scrollbar_gutter = 1;
        else if (value_lower == "stable both-edges") style.scrollbar_gutter = 2;
        return;
    }

    // ---- CSS offset-path (NOT inherited) ----
    if (prop == "offset-path") {
        if (value_lower == "none") style.offset_path = "none";
        else style.offset_path = value_str;
        return;
    }

    // ---- CSS offset-distance (NOT inherited) ----
    if (prop == "offset-distance") {
        auto l = parse_length(value_str);
        if (l) style.offset_distance = l->to_px(0);
        return;
    }

    // ---- CSS offset-rotate (NOT inherited) ----
    if (prop == "offset-rotate") {
        style.offset_rotate = value_str;
        return;
    }

    // ---- CSS offset shorthand (NOT inherited) ----
    if (prop == "offset") {
        style.offset = value_str;
        return;
    }

    // ---- CSS offset-anchor (NOT inherited) ----
    if (prop == "offset-anchor") {
        style.offset_anchor = value_str;
        return;
    }

    // ---- CSS offset-position (NOT inherited) ----
    if (prop == "offset-position") {
        style.offset_position = value_str;
        return;
    }

    // ---- CSS transition-behavior (NOT inherited) ----
    if (prop == "transition-behavior") {
        if (value_lower == "allow-discrete") style.transition_behavior = 1;
        else style.transition_behavior = 0; // normal
        return;
    }

    // ---- CSS animation-range (NOT inherited) ----
    if (prop == "animation-range") {
        style.animation_range = value_str;

        // Parse animation-range: "entry 0% cover 100%" format
        auto range_lower = to_lower(value_str);
        auto tokens = split_whitespace(range_lower);

        // Track which percentage we're on (0=start, 1=end)
        int percent_count = 0;
        for (size_t i = 0; i < tokens.size(); i++) {
            if (tokens[i].find('%') != std::string::npos) {
                float pct = std::stof(tokens[i]);
                pct = std::clamp(pct, 0.0f, 100.0f);
                float offset = pct / 100.0f;

                // First percentage is start, second is end
                if (percent_count == 0) {
                    style.animation_range_start = Length::percent(pct);
                    style.animation_range_start_offset = offset;
                } else {
                    style.animation_range_end = Length::percent(pct);
                    style.animation_range_end_offset = offset;
                }
                percent_count++;
            }
        }
        return;
    }

    // ---- CSS rotate (NOT inherited, CSS Transforms Level 2) ----
    if (prop == "rotate") {
        if (value_lower == "none") style.css_rotate = "none";
        else style.css_rotate = value_str;
        return;
    }

    // ---- CSS view-transition-name (NOT inherited) ----
    if (prop == "view-transition-name") {
        if (value_lower == "none") style.view_transition_name.clear();
        else style.view_transition_name = value_str;
        return;
    }

    // ---- CSS scale (NOT inherited, CSS Transforms Level 2) ----
    if (prop == "scale") {
        if (value_lower == "none") style.css_scale = "none";
        else style.css_scale = value_str;
        return;
    }

    // ---- CSS translate (NOT inherited, CSS Transforms Level 2) ----
    if (prop == "translate") {
        if (value_lower == "none") style.css_translate = "none";
        else style.css_translate = value_str;
        return;
    }

    // ---- CSS overflow-block (NOT inherited) ----
    if (prop == "overflow-block") {
        if (value_lower == "visible") style.overflow_block = 0;
        else if (value_lower == "hidden") style.overflow_block = 1;
        else if (value_lower == "scroll") style.overflow_block = 2;
        else if (value_lower == "auto") style.overflow_block = 3;
        else if (value_lower == "clip") style.overflow_block = 4;
        return;
    }

    // ---- CSS overflow-inline (NOT inherited) ----
    if (prop == "overflow-inline") {
        if (value_lower == "visible") style.overflow_inline = 0;
        else if (value_lower == "hidden") style.overflow_inline = 1;
        else if (value_lower == "scroll") style.overflow_inline = 2;
        else if (value_lower == "auto") style.overflow_inline = 3;
        else if (value_lower == "clip") style.overflow_inline = 4;
        return;
    }

    // ---- CSS box-decoration-break / -webkit-box-decoration-break (NOT inherited) ----
    if (prop == "box-decoration-break" || prop == "-webkit-box-decoration-break") {
        if (value_lower == "slice") style.box_decoration_break = 0;
        else if (value_lower == "clone") style.box_decoration_break = 1;
        return;
    }

    // ---- CSS margin-trim (NOT inherited) ----
    if (prop == "margin-trim") {
        if (value_lower == "none") style.margin_trim = 0;
        else if (value_lower == "block") style.margin_trim = 1;
        else if (value_lower == "inline") style.margin_trim = 2;
        else if (value_lower == "block-start") style.margin_trim = 3;
        else if (value_lower == "block-end") style.margin_trim = 4;
        else if (value_lower == "inline-start") style.margin_trim = 5;
        else if (value_lower == "inline-end") style.margin_trim = 6;
        return;
    }

    // ---- CSS all shorthand (NOT inherited) ----
    if (prop == "all") {
        if (value_lower == "initial") {
            // Reset all properties to CSS initial values
            style = ComputedStyle(); // Default constructor = CSS initial values
        } else if (value_lower == "inherit") {
            // For all: inherit, set inherited properties from parent
            // This is complex, so for now just mark it
            style.css_all = "inherit";
        } else if (value_lower == "unset") {
            // Combination: inherited->inherit, non-inherited->initial
            style = ComputedStyle();
            // Inherited properties should come from parent (handled separately)
            style.css_all = "unset";
        } else if (value_lower == "revert") {
            style.css_all = "revert";
        }
        return;
    }
}

// ============================================================================
// StyleResolver
// ============================================================================

void StyleResolver::add_stylesheet(const StyleSheet& sheet) {
    stylesheets_.push_back(sheet);
}

void StyleResolver::set_default_custom_property(const std::string& name, const std::string& value) {
    default_custom_props_[name] = value;
}

ComputedStyle StyleResolver::resolve(
    const ElementView& element,
    const ComputedStyle& parent_style) const {

    // Collect matching rules
    auto matched_rules = collect_matching_rules(element);

    // Apply tag defaults first
    ComputedStyle tag_defaults = default_style_for_tag(element.tag_name);
    tag_defaults.z_index = clever::layout::Z_INDEX_AUTO;

    // Build result: start from tag defaults, then cascade matched rules
    ComputedStyle result = tag_defaults;

    // Apply inherited properties from parent
    result.color = parent_style.color;
    result.font_family = parent_style.font_family;
    result.font_size = parent_style.font_size;
    result.font_weight = parent_style.font_weight;
    result.font_style = parent_style.font_style;
    result.line_height = parent_style.line_height;
    result.line_height_unitless = parent_style.line_height_unitless;
    result.text_align = parent_style.text_align;
    result.text_align_last = parent_style.text_align_last;
    result.text_transform = parent_style.text_transform;
    result.white_space = parent_style.white_space;
    result.letter_spacing = parent_style.letter_spacing;
    result.word_spacing = parent_style.word_spacing;
    result.visibility = parent_style.visibility;
    result.cursor = parent_style.cursor;
    result.direction = parent_style.direction;
    result.list_style_type = parent_style.list_style_type;
    result.list_style_position = parent_style.list_style_position;
    result.list_style_image = parent_style.list_style_image;
    result.pointer_events = parent_style.pointer_events;
    result.user_select = parent_style.user_select;
    result.tab_size = parent_style.tab_size;
    result.border_collapse = parent_style.border_collapse;
    result.border_spacing = parent_style.border_spacing;
    result.border_spacing_v = parent_style.border_spacing_v;
    result.table_layout = parent_style.table_layout;
    result.caption_side = parent_style.caption_side;
    result.empty_cells = parent_style.empty_cells;
    result.quotes = parent_style.quotes;
    result.hyphens = parent_style.hyphens;
    result.overflow_wrap = parent_style.overflow_wrap;
    result.text_justify = parent_style.text_justify;
    result.hanging_punctuation = parent_style.hanging_punctuation;
    result.font_variant = parent_style.font_variant;
    result.font_variant_caps = parent_style.font_variant_caps;
    result.font_variant_numeric = parent_style.font_variant_numeric;
    result.font_synthesis = parent_style.font_synthesis;
    result.font_variant_alternates = parent_style.font_variant_alternates;
    result.font_feature_settings = parent_style.font_feature_settings;
    result.font_variation_settings = parent_style.font_variation_settings;
    result.font_optical_sizing = parent_style.font_optical_sizing;
    result.print_color_adjust = parent_style.print_color_adjust;
    result.image_orientation = parent_style.image_orientation;
    result.image_orientation_explicit = false;
    result.font_kerning = parent_style.font_kerning;
    result.font_variant_ligatures = parent_style.font_variant_ligatures;
    result.font_variant_east_asian = parent_style.font_variant_east_asian;
    result.font_palette = parent_style.font_palette;
    result.font_variant_position = parent_style.font_variant_position;
    result.font_language_override = parent_style.font_language_override;
    result.font_size_adjust = parent_style.font_size_adjust;
    result.font_stretch = parent_style.font_stretch;
    result.text_decoration_skip_ink = parent_style.text_decoration_skip_ink;
    result.text_wrap = parent_style.text_wrap;
    result.white_space_collapse = parent_style.white_space_collapse;
    result.line_break = parent_style.line_break;
    result.math_style = parent_style.math_style;
    result.math_depth = parent_style.math_depth;
    result.orphans = parent_style.orphans;
    result.widows = parent_style.widows;
    result.text_underline_position = parent_style.text_underline_position;
    result.writing_mode = parent_style.writing_mode;
    result.ruby_align = parent_style.ruby_align;
    result.ruby_position = parent_style.ruby_position;
    result.ruby_overhang = parent_style.ruby_overhang;
    result.text_orientation = parent_style.text_orientation;
    result.text_rendering = parent_style.text_rendering;
    result.font_smooth = parent_style.font_smooth;
    result.text_size_adjust = parent_style.text_size_adjust;
    result.color_scheme = parent_style.color_scheme;
    result.paint_order = parent_style.paint_order;
    result.caret_color = parent_style.caret_color;
    result.accent_color = parent_style.accent_color;
    result.color_interpolation = parent_style.color_interpolation;

    // Inherit custom properties (CSS variables are inherited)
    result.custom_properties = parent_style.custom_properties;

    // Apply @property initial values for any custom properties not yet set
    for (auto& [name, value] : default_custom_props_) {
        if (result.custom_properties.find(name) == result.custom_properties.end()) {
            result.custom_properties[name] = value;
        }
    }

    // Re-apply tag-specific overrides for non-inherited display-like properties
    // (tag defaults take priority for display, but CSS rules override everything)
    result.display = tag_defaults.display;

    // Re-apply tag-specific defaults that are stronger than parent inheritance
    // For example, <strong> should be bold even if parent is normal weight
    if (tag_defaults.font_weight != 400) {
        result.font_weight = tag_defaults.font_weight;
    }
    if (tag_defaults.font_style != FontStyle::Normal) {
        result.font_style = tag_defaults.font_style;
    }
    if (tag_defaults.text_decoration != TextDecoration::None) {
        result.text_decoration = tag_defaults.text_decoration;
    }
    if (tag_defaults.cursor != Cursor::Auto) {
        result.cursor = tag_defaults.cursor;
    }
    if (tag_defaults.font_size.value != 16.0f) {
        result.font_size = tag_defaults.font_size;
    }

    // Parse inline style declarations from style="" attribute
    std::vector<Declaration> inline_decls;
    for (const auto& attr : element.attributes) {
        if (to_lower(attr.first) == "style") {
            inline_decls = parse_declaration_block(attr.second);
            break;
        }
    }

    // Cascade matched stylesheet rules + inline declarations
    if (!matched_rules.empty() || !inline_decls.empty()) {
        struct PrioritizedDecl {
            const Declaration* decl;
            Specificity specificity;
            size_t source_order;
            bool important;
            bool in_layer;
            size_t layer_order;
            bool is_user_agent;
            bool is_inline;
        };

        std::vector<PrioritizedDecl> all_decls;
        all_decls.reserve(matched_rules.size() * 4 + inline_decls.size());

        // First stylesheet is the UA sheet in our render pipeline.
        const StyleRule* ua_begin = nullptr;
        const StyleRule* ua_end = nullptr;
        if (!stylesheets_.empty() && !stylesheets_.front().rules.empty()) {
            ua_begin = stylesheets_.front().rules.data();
            ua_end = ua_begin + stylesheets_.front().rules.size();
        }

        auto is_user_agent_rule = [ua_begin, ua_end](const StyleRule* rule) {
            return ua_begin != nullptr && rule >= ua_begin && rule < ua_end;
        };

        for (const auto& matched : matched_rules) {
            const bool is_user_agent = is_user_agent_rule(matched.rule);
            for (const auto& decl : matched.rule->declarations) {
                all_decls.push_back({
                    &decl,
                    matched.specificity,
                    matched.source_order,
                    decl.important,
                    matched.rule->in_layer,
                    matched.rule->layer_order,
                    is_user_agent,
                    false
                });
            }
        }

        // Inline styles are author-origin declarations with top specificity.
        // Keep them in their own cascade tier so inline normal beats all rule
        // normal declarations, and inline !important beats author !important.
        const Specificity inline_specificity{1000000, 0, 0};
        for (size_t i = 0; i < inline_decls.size(); ++i) {
            const auto& decl = inline_decls[i];
            all_decls.push_back({
                &decl,
                inline_specificity,
                i,
                decl.important,
                false,
                0,
                false,
                true
            });
        }

        std::stable_sort(all_decls.begin(), all_decls.end(),
            [](const PrioritizedDecl& a, const PrioritizedDecl& b) {
                auto tier = [](const PrioritizedDecl& pd) {
                    if (pd.is_user_agent) return pd.important ? 5 : 0;
                    if (pd.is_inline) return pd.important ? 4 : 2;
                    return pd.important ? 3 : 1; // author stylesheet
                };

                // Sort so that "winning" declarations come LAST.
                // Tier order (low -> high):
                // UA normal < author normal < inline normal <
                // author !important < inline !important < UA !important.
                const int tier_a = tier(a);
                const int tier_b = tier(b);
                if (tier_a != tier_b) {
                    return tier_a < tier_b;
                }

                // @layer ordering applies only to stylesheet declarations.
                if (!a.is_inline && !b.is_inline) {
                    if (a.important) {
                        // Important: layered > unlayered, earlier layers win.
                        if (a.in_layer != b.in_layer) return !a.in_layer;
                        if (a.in_layer && b.in_layer && a.layer_order != b.layer_order) {
                            return a.layer_order > b.layer_order;
                        }
                    } else {
                        // Normal: unlayered > layered, later layers win.
                        if (a.in_layer != b.in_layer) return a.in_layer;
                        if (a.in_layer && b.in_layer && a.layer_order != b.layer_order) {
                            return a.layer_order < b.layer_order;
                        }
                    }
                }

                if (!(a.specificity == b.specificity)) return a.specificity < b.specificity;
                return a.source_order < b.source_order;
            });

        for (const auto& pd : all_decls) {
            cascade_.apply_declaration(result, *pd.decl, parent_style);
        }
    }

    // CSS spec: unitless line-height is inherited as the *number*, not the
    // computed value. Recompute if font-size differs from parent.
    if (result.line_height_unitless > 0 &&
        result.font_size.value != parent_style.font_size.value) {
        result.line_height = Length::px(result.line_height_unitless * result.font_size.value);
    }

    normalize_display_contents_style(result);

    return result;
}

// --- Media query evaluation ---

bool StyleResolver::evaluate_media_condition(const std::string& condition) const {
    // Trim whitespace
    std::string cond = condition;
    while (!cond.empty() && cond.front() == ' ') cond.erase(cond.begin());
    while (!cond.empty() && cond.back() == ' ') cond.pop_back();
    if (cond.empty()) return true; // empty condition = all

    // Lowercase for comparison
    std::string lower = cond;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Handle comma-separated media query lists (OR semantics)
    if (lower.find(',') != std::string::npos) {
        size_t start = 0;
        while (start < cond.size()) {
            size_t comma = cond.find(',', start);
            std::string part = (comma == std::string::npos)
                ? cond.substr(start)
                : cond.substr(start, comma - start);
            if (evaluate_media_condition(part)) return true;
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return false;
    }

    // Handle "not" prefix
    if (lower.substr(0, 4) == "not ") {
        return !evaluate_media_condition(cond.substr(4));
    }

    // Handle "only" prefix (treat same as without)
    if (lower.substr(0, 5) == "only ") {
        return evaluate_media_condition(cond.substr(5));
    }

    // Media types
    if (lower == "all" || lower == "screen") return true;
    if (lower == "print" || lower == "speech" || lower == "tty" ||
        lower == "tv" || lower == "projection" || lower == "handheld" ||
        lower == "braille" || lower == "embossed" || lower == "aural") return false;

    // Handle "and" combinations: "screen and (max-width: 768px)"
    // Split on " and " and evaluate each part
    {
        std::string::size_type and_pos = lower.find(" and ");
        if (and_pos != std::string::npos) {
            std::string left = cond.substr(0, and_pos);
            std::string right = cond.substr(and_pos + 5);
            return evaluate_media_condition(left) && evaluate_media_condition(right);
        }
    }

    // Handle individual media features: (feature: value) or (feature)
    if (lower.front() == '(' && lower.back() == ')') {
        std::string inner = lower.substr(1, lower.size() - 2);
        // Trim
        while (!inner.empty() && inner.front() == ' ') inner.erase(inner.begin());
        while (!inner.empty() && inner.back() == ' ') inner.pop_back();

        auto colon_pos = inner.find(':');
        if (colon_pos == std::string::npos) {
            // Boolean feature like (color), (hover), (pointer)
            if (inner == "color" || inner == "hover" || inner == "grid") return true;
            if (inner == "pointer") return true; // we have a pointer
            return false;
        }

        std::string feature = inner.substr(0, colon_pos);
        std::string value = inner.substr(colon_pos + 1);
        while (!feature.empty() && feature.back() == ' ') feature.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());

        // Parse numeric value (strip units)
        float num_val = 0;
        try {
            size_t end_pos = 0;
            num_val = std::stof(value, &end_pos);
        } catch (...) {}

        if (feature == "min-width") return viewport_width_ >= num_val;
        if (feature == "max-width") return viewport_width_ <= num_val;
        if (feature == "min-height") return viewport_height_ >= num_val;
        if (feature == "max-height") return viewport_height_ <= num_val;
        if (feature == "width") return viewport_width_ == num_val;
        if (feature == "height") return viewport_height_ == num_val;
        if (feature == "min-device-width") return viewport_width_ >= num_val;
        if (feature == "max-device-width") return viewport_width_ <= num_val;
        if (feature == "orientation") {
            if (value == "portrait") return viewport_height_ >= viewport_width_;
            if (value == "landscape") return viewport_width_ >= viewport_height_;
        }
        if (feature == "prefers-color-scheme") {
            if (value == "dark") return is_dark_mode();
            if (value == "light") return !is_dark_mode();
        }
        if (feature == "prefers-reduced-motion") {
            if (value == "reduce") return false;
            else if (value == "no-preference") return true;
            else return true;
        }
        if (feature == "prefers-contrast") {
            if (value == "more") return false;
            if (value == "less") return false;
            return true;
        }
        if (feature == "display-mode") {
            return value == "browser";
        }
        if (feature == "color-gamut") return true;
        if (feature == "-webkit-min-device-pixel-ratio" ||
            feature == "min-resolution") return true;

        // Unknown feature — assume true to be permissive
        return true;
    }

    // Bare media type or unknown — assume true
    return true;
}

bool StyleResolver::evaluate_supports_condition(const std::string& condition) const {
    std::string cond = condition;
    while (!cond.empty() && cond.front() == ' ') cond.erase(cond.begin());
    while (!cond.empty() && cond.back() == ' ') cond.pop_back();

    std::string lower = cond;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Handle "not"
    if (lower.substr(0, 4) == "not ") {
        return !evaluate_supports_condition(cond.substr(4));
    }

    // Handle "or"
    {
        auto or_pos = lower.find(" or ");
        if (or_pos != std::string::npos) {
            return evaluate_supports_condition(cond.substr(0, or_pos)) ||
                   evaluate_supports_condition(cond.substr(or_pos + 4));
        }
    }

    // Handle "and"
    {
        auto and_pos = lower.find(" and ");
        if (and_pos != std::string::npos) {
            return evaluate_supports_condition(cond.substr(0, and_pos)) &&
                   evaluate_supports_condition(cond.substr(and_pos + 5));
        }
    }

    // Handle (property: value) — we support most CSS properties, so return true
    if (lower.front() == '(' && lower.back() == ')') {
        std::string inner = lower.substr(1, lower.size() - 2);
        auto colon_pos = inner.find(':');
        if (colon_pos != std::string::npos) {
            std::string prop = inner.substr(0, colon_pos);
            while (!prop.empty() && prop.back() == ' ') prop.pop_back();
            while (!prop.empty() && prop.front() == ' ') prop.erase(prop.begin());
            // We support most standard CSS properties
            if (prop == "display" || prop == "flex" || prop == "grid" ||
                prop == "position" || prop == "transform" || prop == "opacity" ||
                prop == "transition" || prop == "animation" || prop == "filter" ||
                prop == "backdrop-filter" || prop == "gap" || prop == "aspect-ratio" ||
                prop == "object-fit" || prop == "scroll-snap-type" ||
                prop == "overflow" || prop == "clip-path" || prop == "mask" ||
                prop == "color" || prop == "background" || prop == "border" ||
                prop == "margin" || prop == "padding" || prop == "width" ||
                prop == "height" || prop == "font" || prop == "text-decoration" ||
                prop == "box-shadow" || prop == "border-radius" ||
                prop == "mix-blend-mode" || prop == "writing-mode" ||
                prop == "contain" || prop == "content-visibility" ||
                prop == "container-type" || prop == "user-select" ||
                prop == "pointer-events" || prop == "resize" ||
                prop == "cursor" || prop == "visibility" ||
                prop == "z-index" || prop == "flex-direction" ||
                prop == "flex-wrap" || prop == "justify-content" ||
                prop == "align-items" || prop == "align-self" ||
                prop == "order") {
                return true;
            }
            // Assume supported for other properties too
            return true;
        }
    }

    return true; // Be permissive
}

bool StyleResolver::is_element_in_scope(const ElementView& element, const ScopeRule& scope) const {
    auto scope_start_list = parse_selector_list(scope.scope_start);
    if (scope_start_list.selectors.empty()) {
        return false;
    }

    bool has_scope_start_ancestor = false;
    for (const ElementView* anc = element.parent; anc; anc = anc->parent) {
        for (const auto& complex_sel : scope_start_list.selectors) {
            if (matcher_.matches(*anc, complex_sel)) {
                has_scope_start_ancestor = true;
                break;
            }
        }
        if (has_scope_start_ancestor) break;
    }

    if (!has_scope_start_ancestor) {
        return false;
    }

    if (!scope.scope_end.empty()) {
        auto scope_end_list = parse_selector_list(scope.scope_end);
        for (const ElementView* anc = element.parent; anc; anc = anc->parent) {
            for (const auto& complex_sel : scope_end_list.selectors) {
                if (matcher_.matches(*anc, complex_sel)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// --- Helper: collect from a rule list ---

void StyleResolver::collect_from_rules(const std::vector<StyleRule>& rules,
                                        const ElementView& element,
                                        std::vector<MatchedRule>& result,
                                        size_t& source_order) const {
    for (const auto& rule : rules) {
        for (const auto& complex_sel : rule.selectors.selectors) {
            if (matcher_.matches(element, complex_sel)) {
                MatchedRule matched;
                matched.rule = &rule;
                matched.specificity = compute_specificity(complex_sel);
                matched.source_order = source_order++;
                result.push_back(matched);
                break;
            }
        }
    }
}

void StyleResolver::collect_pseudo_from_rules(const std::vector<StyleRule>& rules,
                                               const ElementView& element,
                                               const std::string& pseudo_name,
                                               std::vector<MatchedRule>& result,
                                               size_t& source_order) const {
    for (const auto& rule : rules) {
        for (const auto& complex_sel : rule.selectors.selectors) {
            if (complex_sel.parts.empty()) continue;

            const auto& last_compound = complex_sel.parts.back().compound;
            bool has_pseudo = false;
            for (const auto& ss : last_compound.simple_selectors) {
                if (ss.type == SimpleSelectorType::PseudoElement &&
                    ss.value == pseudo_name) {
                    has_pseudo = true;
                    break;
                }
            }
            if (!has_pseudo) continue;

            ComplexSelector modified = complex_sel;
            auto& mod_last = modified.parts.back().compound.simple_selectors;
            mod_last.erase(
                std::remove_if(mod_last.begin(), mod_last.end(),
                    [](const SimpleSelector& ss) {
                        return ss.type == SimpleSelectorType::PseudoElement;
                    }),
                mod_last.end());

            bool matches = false;
            if (mod_last.empty() && modified.parts.size() == 1) {
                matches = true;
            } else {
                matches = matcher_.matches(element, modified);
            }

            if (matches) {
                MatchedRule matched;
                matched.rule = &rule;
                matched.specificity = compute_specificity(complex_sel);
                matched.source_order = source_order++;
                result.push_back(matched);
                break;
            }
        }
    }
}

// --- Updated collect functions ---

std::vector<MatchedRule> StyleResolver::collect_matching_rules(const ElementView& element) const {
    std::vector<MatchedRule> result;
    size_t source_order = 0;

    for (const auto& sheet : stylesheets_) {
        collect_from_rules(sheet.rules, element, result, source_order);
        for (const auto& layer : sheet.layer_rules) {
            collect_from_rules(layer.rules, element, result, source_order);
        }

        for (const auto& mq : sheet.media_queries) {
            if (evaluate_media_condition(mq.condition)) {
                collect_from_rules(mq.rules, element, result, source_order);
            }
        }

        for (const auto& scope : sheet.scope_rules) {
            if (is_element_in_scope(element, scope)) {
                collect_from_rules(scope.rules, element, result, source_order);
            }
        }
    }

    return result;
}

std::vector<MatchedRule> StyleResolver::collect_pseudo_rules(
    const ElementView& element,
    const std::string& pseudo_name) const {

    std::vector<MatchedRule> result;
    size_t source_order = 0;

    for (const auto& sheet : stylesheets_) {
        collect_pseudo_from_rules(sheet.rules, element, pseudo_name, result, source_order);
        for (const auto& layer : sheet.layer_rules) {
            collect_pseudo_from_rules(layer.rules, element, pseudo_name, result, source_order);
        }

        for (const auto& mq : sheet.media_queries) {
            if (evaluate_media_condition(mq.condition)) {
                collect_pseudo_from_rules(mq.rules, element, pseudo_name, result, source_order);
            }
        }

        for (const auto& scope : sheet.scope_rules) {
            if (is_element_in_scope(element, scope)) {
                collect_pseudo_from_rules(scope.rules, element, pseudo_name, result, source_order);
            }
        }
    }

    return result;
}

std::optional<ComputedStyle> StyleResolver::resolve_pseudo(
    const ElementView& element,
    const std::string& pseudo_name,
    const ComputedStyle& element_style) const {

    auto matched_rules = collect_pseudo_rules(element, pseudo_name);
    if (matched_rules.empty()) {
        return std::nullopt;
    }

    // Cascade the pseudo-element rules on top of the element's style
    // (pseudo-elements inherit from their originating element)
    ComputedStyle result = cascade_.cascade(matched_rules, element_style);
    return result;
}

} // namespace clever::css
