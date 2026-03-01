#include <clever/paint/render_pipeline.h>
#include <clever/paint/image_fetch.h>
#include <clever/paint/software_renderer.h>
#include <clever/paint/painter.h>
#include <clever/paint/text_renderer.h>
#include <clever/html/tree_builder.h>
#include <clever/css/style/computed_style.h>
#include <clever/css/style/style_resolver.h>
#include <clever/css/style/selector_matcher.h>
#include <clever/css/parser/stylesheet.h>
#include <clever/layout/layout_engine.h>
#include <clever/layout/box.h>
#include <clever/net/http_client.h>
#include <clever/net/request.h>
#include <clever/net/cookie_jar.h>
#include <clever/js/js_engine.h>
#include <clever/js/js_dom_bindings.h>
#include <clever/js/js_fetch_bindings.h>
#include <clever/js/js_timers.h>
#include <clever/js/js_window.h>
#include <clever/url/url.h>
#include <clever/url/percent_encoding.h>
#include <clever/platform/thread_pool.h>
#include <stb_image.h>
#include <nanosvg.h>
#include <nanosvgrast.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <zlib.h>
#endif
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// Forward declaration: consume_pending_post_submission from js_window.cpp
namespace clever::js {
bool consume_pending_post_submission(std::string& out_body, std::string& out_content_type);
} // namespace clever::js

namespace clever::css {
std::vector<std::pair<std::string, int>> parse_font_feature_settings(const std::string& value);
} // namespace clever::css

namespace clever::paint {

// Thread-local state for interactive <details> toggle (accessible from both
// the anonymous namespace where build_layout_tree_styled lives, and the
// render_html functions outside it)
static thread_local int g_details_id_counter = 0;
static thread_local const std::set<int>* g_toggled_details = nullptr;
// When true, <noscript> content is rendered (JS failed or produced many errors)
static thread_local bool g_noscript_fallback = false;
// Shadow DOM: pointer to the shadow_roots map from DOMState (set before build_layout_tree_styled)
static thread_local const std::unordered_map<clever::html::SimpleNode*, clever::html::SimpleNode*>*
    g_shadow_roots = nullptr;

namespace {

// Thread-local CSS counter state for counter-reset / counter-increment / counter()
thread_local std::unordered_map<std::string, int> css_counters;

// Thread-local CSS counter scopes for counters() function (nested counter levels)
thread_local std::unordered_map<std::string, std::vector<int>> css_counter_scopes;

// Thread-local form data collection during layout tree building
thread_local std::vector<clever::paint::FormData> collected_forms;

// Thread-local datalist option collection during layout tree building
thread_local std::unordered_map<std::string, std::vector<std::string>> collected_datalists;

// Transition runtime state persisted across render_html calls on this thread.
thread_local bool g_transition_runtime_enabled = false;
thread_local std::unique_ptr<AnimationController> g_transition_animation_controller;
thread_local std::unordered_map<std::string, clever::css::ComputedStyle> g_previous_styles_by_key;
thread_local std::unordered_map<std::string, clever::layout::LayoutNode*> g_current_layout_nodes_by_key;
thread_local std::unordered_set<std::string> g_current_style_keys;
thread_local std::unordered_map<std::string, std::unique_ptr<clever::layout::LayoutNode>> g_transition_runtime_nodes;
thread_local std::unordered_map<clever::layout::LayoutNode*, std::string> g_transition_runtime_node_to_key;
thread_local bool g_transition_has_last_tick = false;
thread_local std::chrono::steady_clock::time_point g_transition_last_tick_time;

// Keyframe animation tracking: layout nodes that already have animations started
// keyed by style_key so we don't restart animations on re-render
thread_local std::unordered_set<std::string> g_keyframe_animated_keys;

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> parts;
    std::istringstream iss(s);
    std::string word;
    while (iss >> word) parts.push_back(word);
    return parts;
}

// Split a CSS multi-background value into individual layers.
// Commas inside parentheses (e.g. inside gradient functions) are NOT treated as separators.
// Returns each layer as a trimmed string.
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

// Split on whitespace but respect parentheses — tokens inside () are not split.
// e.g. "hsl(0, 100%, 50%) red" → ["hsl(0, 100%, 50%)", "red"]
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

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static constexpr int kFontWeightMin = 100;
static constexpr int kFontWeightMax = 900;
static constexpr int kDefaultWeight = 400;
static constexpr int kFontDisplayBlockMs = 100;
[[maybe_unused]] static constexpr int kFontDisplayFallbackMs = 100;
static constexpr int kFontDisplayOptionalMs = 50;
static constexpr int kUnicodeMin = 0;
static constexpr int kUnicodeMax = 0x10FFFF;

enum class FontFaceStyle { Normal, Italic, Oblique };
enum class FontDisplayPolicy { Auto, Block, Swap, Fallback, Optional };

struct FontFaceCandidate {
    const clever::css::FontFaceRule* rule = nullptr;
    int weight = 400;
    bool italic = false;
    int unicode_min = kUnicodeMin;
    int unicode_max = kUnicodeMax;
};

std::string normalize_font_family_key(const std::string& family_name) {
    std::string value;
    value.reserve(family_name.size());
    for (char ch : family_name) {
        if (ch == '"' || ch == '\'') continue;
        value += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return trim(value);
}

FontFaceStyle normalize_font_face_style(const std::string& style) {
    auto normalized = to_lower(trim(style));
    if (normalized == "italic") return FontFaceStyle::Italic;
    if (normalized == "oblique") return FontFaceStyle::Oblique;
    return FontFaceStyle::Normal;
}

int normalize_font_weight(int weight) {
    if (weight < kFontWeightMin || weight > kFontWeightMax) return kDefaultWeight;
    return weight;
}

FontDisplayPolicy parse_font_display_policy(const std::string& value) {
    auto normalized = to_lower(trim(value));
    if (normalized == "block") return FontDisplayPolicy::Block;
    if (normalized == "swap") return FontDisplayPolicy::Swap;
    if (normalized == "fallback") return FontDisplayPolicy::Fallback;
    if (normalized == "optional") return FontDisplayPolicy::Optional;
    return FontDisplayPolicy::Auto;
}

std::chrono::milliseconds font_display_timeout(FontDisplayPolicy policy) {
    switch (policy) {
        case FontDisplayPolicy::Block:
        case FontDisplayPolicy::Fallback:
            return std::chrono::milliseconds(kFontDisplayBlockMs);
        case FontDisplayPolicy::Optional:
            return std::chrono::milliseconds(kFontDisplayOptionalMs);
        case FontDisplayPolicy::Auto:
        case FontDisplayPolicy::Swap:
        default:
            return std::chrono::milliseconds(0);
    }
}

bool should_cache_font(FontDisplayPolicy policy) {
    return policy != FontDisplayPolicy::Optional;
}

bool font_face_matches_codepoint(const clever::css::FontFaceRule& rule,
                                 std::optional<uint32_t> codepoint) {
    if (!codepoint.has_value()) return true;
    int cp = static_cast<int>(*codepoint);
    return cp >= rule.unicode_min && cp <= rule.unicode_max;
}

struct FontCodepointRange {
    int min_codepoint = kUnicodeMin;
    int max_codepoint = kUnicodeMax;
    bool has_codepoint = false;
};

int decode_utf8_codepoint(const std::string& text, size_t& i) {
    if (i >= text.size()) return -1;

    const unsigned char b0 = static_cast<unsigned char>(text[i]);
    if ((b0 & 0x80) == 0) {
        ++i;
        return b0;
    }

    if ((b0 & 0xE0) == 0xC0 && i + 1 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
        if ((b1 & 0xC0) == 0x80) {
            i += 2;
            return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        }
    }

    if ((b0 & 0xF0) == 0xE0 && i + 2 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
        const unsigned char b2 = static_cast<unsigned char>(text[i + 2]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            i += 3;
            return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        }
    }

    if ((b0 & 0xF8) == 0xF0 && i + 3 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
        const unsigned char b2 = static_cast<unsigned char>(text[i + 2]);
        const unsigned char b3 = static_cast<unsigned char>(text[i + 3]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
            i += 4;
            return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                   ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        }
    }

    ++i;
    return -1;
}

FontCodepointRange collect_text_codepoint_range(const clever::html::SimpleNode& root_node) {
    FontCodepointRange span;
    const std::string text = root_node.text_content();
    for (size_t i = 0; i < text.size();) {
        int cp = decode_utf8_codepoint(text, i);
        if (cp < 0 || cp > kUnicodeMax) continue;
        if (!span.has_codepoint) {
            span.min_codepoint = cp;
            span.max_codepoint = cp;
            span.has_codepoint = true;
            continue;
        }
        if (cp < span.min_codepoint) span.min_codepoint = cp;
        if (cp > span.max_codepoint) span.max_codepoint = cp;
    }
    return span;
}

bool font_face_intersects_text_codepoints(const clever::css::FontFaceRule& rule,
                                         const FontCodepointRange& text_span) {
    if (!text_span.has_codepoint) return true;
    return !(text_span.max_codepoint < rule.unicode_min || text_span.min_codepoint > rule.unicode_max);
}

const clever::css::FontFaceRule* match_best_font_face(
    const std::vector<clever::css::FontFaceRule>& font_faces,
    const std::string& font_family,
    int font_weight,
    const std::string& font_style,
    std::optional<uint32_t> codepoint = std::nullopt) {
    auto requested_family = normalize_font_family_key(font_family);
    auto requested_style = normalize_font_face_style(font_style);
    const int requested_weight = normalize_font_weight(font_weight);
    const clever::css::FontFaceRule* best_rule = nullptr;

    int best_style_rank = std::numeric_limits<int>::max();
    int best_weight_rank = std::numeric_limits<int>::max();
    int best_distance = std::numeric_limits<int>::max();
    int best_span = std::numeric_limits<int>::max();

    for (const auto& ff : font_faces) {
        if (!font_face_matches_codepoint(ff, codepoint)) continue;
        if (normalize_font_family_key(ff.font_family) != requested_family) continue;

        FontFaceStyle rule_style = normalize_font_face_style(ff.font_style);
        int style_rank = 2;
        if (requested_style == FontFaceStyle::Normal) {
            style_rank = (rule_style == FontFaceStyle::Normal) ? 0 : 1;
        } else if (requested_style == FontFaceStyle::Italic) {
            style_rank = (rule_style == FontFaceStyle::Italic) ? 0 :
                        (rule_style == FontFaceStyle::Normal) ? 1 : 2;
        } else {
            style_rank = (rule_style == FontFaceStyle::Oblique) ? 0 :
                        (rule_style == FontFaceStyle::Normal) ? 1 : 2;
        }

        int min_weight = ff.min_weight;
        int max_weight = ff.max_weight;
        if (min_weight == 0 && max_weight == 0) {
            min_weight = kDefaultWeight;
            max_weight = kDefaultWeight;
        }
        if (min_weight > max_weight) std::swap(min_weight, max_weight);
        if (min_weight == 0 && max_weight == 900) {
            min_weight = kFontWeightMin;
            max_weight = kFontWeightMax;
        }

        int weight_rank = 2;
        int weight_distance = 0;
        if (min_weight == max_weight) {
            if (requested_weight == min_weight) {
                weight_rank = 0;
                weight_distance = 0;
            } else {
                weight_rank = 2;
                weight_distance = std::abs(requested_weight - min_weight);
            }
        } else if (requested_weight >= min_weight && requested_weight <= max_weight) {
            weight_rank = 1;
            weight_distance = 0;
        } else if (requested_weight < min_weight) {
            weight_rank = 2;
            weight_distance = min_weight - requested_weight;
        } else {
            weight_rank = 2;
            weight_distance = requested_weight - max_weight;
        }

        int span = std::abs(max_weight - min_weight);
        if (!best_rule || style_rank < best_style_rank ||
            (style_rank == best_style_rank &&
             (weight_rank < best_weight_rank ||
              (weight_rank == best_weight_rank &&
               (weight_distance < best_distance ||
                (weight_distance == best_distance && span < best_span)))))) {
            best_rule = &ff;
            best_style_rank = style_rank;
            best_weight_rank = weight_rank;
            best_distance = weight_distance;
            best_span = span;
        }
    }
    return best_rule;
}

bool transition_property_list_matches(const std::string& transition_property,
                                      const std::string& property) {
    std::string list = to_lower(trim(transition_property));
    if (list.empty() || list == "all") return true;
    if (list == "none") return false;

    for (char& ch : list) {
        if (ch == ',') ch = ' ';
    }
    auto tokens = split_whitespace(list);
    return std::find(tokens.begin(), tokens.end(), property) != tokens.end();
}

std::optional<clever::css::TransitionDef> transition_def_for_property(
    const clever::css::ComputedStyle& style,
    const std::string& property) {
    std::optional<clever::css::TransitionDef> exact_match;
    std::optional<clever::css::TransitionDef> all_match;
    for (const auto& def : style.transitions) {
        std::string def_property = to_lower(trim(def.property));
        if (def_property == property) {
            exact_match = def;
            break;
        }
        if (def_property == "all" && !all_match.has_value()) {
            all_match = def;
        }
    }

    if (exact_match.has_value() && exact_match->duration_ms > 0.0f) {
        return exact_match;
    }
    if (all_match.has_value() && all_match->duration_ms > 0.0f) {
        auto def = *all_match;
        def.property = property;
        return def;
    }

    auto scalar_def = build_transition_def_from_style(style);
    if (scalar_def.duration_ms <= 0.0f) return std::nullopt;
    if (!transition_property_list_matches(style.transition_property, property)) {
        return std::nullopt;
    }
    scalar_def.property = property;
    return scalar_def;
}

std::string build_style_key_for_node(const clever::html::SimpleNode& node) {
    if (node.type != clever::html::SimpleNode::Element) return "";

    for (const auto& attr : node.attributes) {
        if (attr.name == "id" && !attr.value.empty()) {
            return "id:" + attr.value;
        }
    }

    std::vector<std::string> segments;
    const clever::html::SimpleNode* current = &node;
    while (current && current->type != clever::html::SimpleNode::Document) {
        if (current->type == clever::html::SimpleNode::Element) {
            std::string tag = to_lower(current->tag_name);
            size_t same_tag_index = 1;
            if (current->parent) {
                for (const auto& sibling : current->parent->children) {
                    if (!sibling || sibling->type != clever::html::SimpleNode::Element) continue;
                    if (to_lower(sibling->tag_name) != tag) continue;
                    if (sibling.get() == current) break;
                    same_tag_index++;
                }
            }
            segments.push_back(tag + "[" + std::to_string(same_tag_index) + "]");
        }
        current = current->parent;
    }

    std::reverse(segments.begin(), segments.end());
    std::string key;
    for (size_t i = 0; i < segments.size(); i++) {
        if (i > 0) key += "/";
        key += segments[i];
    }
    return key;
}

clever::layout::LayoutNode* get_or_create_transition_runtime_node(const std::string& style_key) {
    auto it = g_transition_runtime_nodes.find(style_key);
    if (it != g_transition_runtime_nodes.end()) {
        return it->second.get();
    }

    auto runtime_node = std::make_unique<clever::layout::LayoutNode>();
    runtime_node->tag_name = "#transition-runtime";
    auto* node_ptr = runtime_node.get();
    g_transition_runtime_nodes[style_key] = std::move(runtime_node);
    g_transition_runtime_node_to_key[node_ptr] = style_key;
    return node_ptr;
}

void prune_transition_runtime_state() {
    for (auto it = g_previous_styles_by_key.begin(); it != g_previous_styles_by_key.end();) {
        if (g_current_style_keys.find(it->first) == g_current_style_keys.end()) {
            it = g_previous_styles_by_key.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = g_transition_runtime_nodes.begin(); it != g_transition_runtime_nodes.end();) {
        if (g_current_style_keys.find(it->first) == g_current_style_keys.end()) {
            auto* runtime_node = it->second.get();
            if (g_transition_animation_controller) {
                g_transition_animation_controller->remove_animations_for_element(runtime_node);
            }
            g_transition_runtime_node_to_key.erase(runtime_node);
            it = g_transition_runtime_nodes.erase(it);
        } else {
            ++it;
        }
    }
}

} // end anonymous namespace (temporarily, for easing/interpolation functions)

bool render_has_active_animations() {
    if (!g_transition_animation_controller) return false;
    for (const auto& kv : g_transition_runtime_node_to_key) {
        if (g_transition_animation_controller->is_animating(kv.first)) return true;
    }
    return false;
}

std::string extract_preferred_font_url(const std::string& src) {
    // Parse comma-separated source descriptors and return the first URL
    // whose optional format(...) is supported by the current pipeline.
    static const std::unordered_set<std::string> k_supported_formats = {
        "woff2",
        "woff",
        "truetype",
        "opentype",
        "woff2-variations",
        "woff-variations",
        "truetype-variations",
        "opentype-variations"
    };
    static const std::unordered_set<std::string> k_supported_techs = {
        "variations"
    };

    auto find_function_open = [&](const std::string& entry,
                                  const std::string& function_name) -> std::optional<size_t> {
        const std::string lower_entry = to_lower(entry);
        const std::string lower_function_name = to_lower(function_name);
        bool in_single = false;
        bool in_double = false;
        int depth = 0;

        for (size_t i = 0; i < lower_entry.size(); ++i) {
            const char ch = lower_entry[i];
            if (ch == '"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (ch == '\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (in_single || in_double) continue;

            if (ch == '(') {
                depth++;
                continue;
            }
            if (ch == ')') {
                if (depth > 0) depth--;
                continue;
            }

            if (depth != 0) continue;
            if (i + lower_function_name.size() >= lower_entry.size()) continue;
            if (lower_entry.compare(i, lower_function_name.size(), lower_function_name) != 0) continue;

            const size_t after_name = i + lower_function_name.size();
            if (after_name >= lower_entry.size() || lower_entry[after_name] != '(') continue;

            if (i > 0) {
                const unsigned char prev = static_cast<unsigned char>(lower_entry[i - 1]);
                if (std::isalnum(prev) || lower_entry[i - 1] == '-' || lower_entry[i - 1] == '_') continue;
            }
            return after_name;
        }
        return std::nullopt;
    };

    auto parse_function_arg = [&](const std::string& entry,
                                  const std::string& function_name) -> std::string {
        const auto open_opt = find_function_open(entry, function_name);
        if (!open_opt.has_value()) return "";
        const size_t open = *open_opt;

        size_t close = std::string::npos;
        bool in_single = false;
        bool in_double = false;
        int depth = 0;
        for (size_t i = open; i < entry.size(); ++i) {
            const char ch = entry[i];
            if (ch == '"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (ch == '\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (in_single || in_double) continue;
            if (ch == '(') {
                depth++;
            } else if (ch == ')') {
                depth--;
                if (depth == 0) {
                    close = i;
                    break;
                }
            }
        }
        if (close == std::string::npos || close <= open + 1) return "";

        return trim(entry.substr(open + 1, close - open - 1));
    };
    auto has_function_call = [&](const std::string& entry,
                                 const std::string& function_name) -> bool {
        return find_function_open(entry, function_name).has_value();
    };
    auto count_function_calls = [&](const std::string& entry,
                                    const std::string& function_name) -> size_t {
        const std::string lower_entry = to_lower(entry);
        const std::string lower_function_name = to_lower(function_name);
        size_t count = 0;
        bool in_single = false;
        bool in_double = false;
        int depth = 0;

        for (size_t i = 0; i < lower_entry.size(); ++i) {
            const char ch = lower_entry[i];
            if (ch == '"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (ch == '\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (in_single || in_double) continue;

            if (ch == '(') {
                depth++;
                continue;
            }
            if (ch == ')') {
                if (depth > 0) depth--;
                continue;
            }

            if (depth != 0) continue;
            if (i + lower_function_name.size() >= lower_entry.size()) continue;
            if (lower_entry.compare(i, lower_function_name.size(), lower_function_name) != 0) continue;

            const size_t after_name = i + lower_function_name.size();
            if (after_name >= lower_entry.size() || lower_entry[after_name] != '(') continue;
            if (i > 0) {
                const unsigned char prev = static_cast<unsigned char>(lower_entry[i - 1]);
                if (std::isalnum(prev) || lower_entry[i - 1] == '-' || lower_entry[i - 1] == '_') continue;
            }
            count++;
        }
        return count;
    };

    auto split_csv_tokens = [&](const std::string& value) -> std::vector<std::string> {
        std::vector<std::string> tokens;
        std::string current;
        bool saw_separator = false;
        bool in_single_quote = false;
        bool in_double_quote = false;
        int paren_depth = 0;

        for (char ch : value) {
            if (ch == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                current.push_back(ch);
                continue;
            }
            if (ch == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                current.push_back(ch);
                continue;
            }
            if (!in_single_quote && !in_double_quote) {
                if (ch == '(') paren_depth++;
                else if (ch == ')') {
                    if (paren_depth == 0) return {};
                    paren_depth--;
                }
                if (ch == ',' && paren_depth == 0) {
                    tokens.push_back(trim(current));
                    current.clear();
                    saw_separator = true;
                    continue;
                }
            }
            current.push_back(ch);
        }
        if (in_single_quote || in_double_quote || paren_depth != 0) {
            return {};
        }
        if (!current.empty() || saw_separator) {
            tokens.push_back(trim(current));
        }
        return tokens;
    };
    auto descriptor_list_has_supported_token = [&](const std::string& descriptor_value,
                                                   const std::unordered_set<std::string>& supported_tokens) -> bool {
        const auto tokens = split_csv_tokens(descriptor_value);
        if (tokens.empty()) return false;

        bool has_supported_token = false;
        for (auto token : tokens) {
            token = trim(token);
            if (token.empty()) return false;

            if (token.size() >= 2 &&
                ((token.front() == '"' && token.back() == '"') ||
                 (token.front() == '\'' && token.back() == '\''))) {
                token = token.substr(1, token.size() - 2);
            }
            token = to_lower(trim(token));
            if (token.empty()) return false;

            if (supported_tokens.find(token) != supported_tokens.end()) {
                has_supported_token = true;
            }
        }
        return has_supported_token;
    };

    std::vector<std::string> entries;
    std::string current;
    bool saw_separator = false;
    int paren_depth = 0;
    bool in_single = false;
    bool in_double = false;
    for (char ch : src) {
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            current.push_back(ch);
            continue;
        }
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            current.push_back(ch);
            continue;
        }
        if (!in_single && !in_double) {
            if (ch == '(') paren_depth++;
            else if (ch == ')') {
                if (paren_depth == 0) return "";
                paren_depth--;
            }
            if (ch == ',' && paren_depth == 0) {
                const std::string entry = trim(current);
                if (entry.empty()) return "";
                entries.push_back(entry);
                current.clear();
                saw_separator = true;
                continue;
            }
        }
        current.push_back(ch);
    }
    if (in_single || in_double || paren_depth != 0) return "";
    const std::string trailing_entry = trim(current);
    if (saw_separator && trailing_entry.empty()) return "";
    if (!trailing_entry.empty()) {
        entries.push_back(trailing_entry);
    }
    for (const auto& entry : entries) {
        if (entry.empty()) return "";
    }

    for (const auto& entry : entries) {
        if (count_function_calls(entry, "local") > 0 && count_function_calls(entry, "url") > 0) continue;
        if (count_function_calls(entry, "url") > 1) continue;
        if (count_function_calls(entry, "format") > 1) continue;
        if (count_function_calls(entry, "tech") > 1) continue;

        std::string url = trim(parse_function_arg(entry, "url"));
        if (url.size() >= 2 &&
            ((url.front() == '"' && url.back() == '"') ||
             (url.front() == '\'' && url.back() == '\''))) {
            url = url.substr(1, url.size() - 2);
        }
        if (url.empty()) continue;

        const bool has_format_descriptor = has_function_call(entry, "format");
        const std::string format_value = parse_function_arg(entry, "format");
        if (has_format_descriptor && format_value.empty()) continue;
        if (!format_value.empty()) {
            if (!descriptor_list_has_supported_token(format_value, k_supported_formats)) continue;
        }

        const bool has_tech_descriptor = has_function_call(entry, "tech");
        const std::string tech_value = parse_function_arg(entry, "tech");
        if (has_tech_descriptor && tech_value.empty()) continue;
        if (!tech_value.empty()) {
            if (!descriptor_list_has_supported_token(tech_value, k_supported_techs)) continue;
        }
        return url;
    }
    return "";
}

std::optional<std::vector<uint8_t>> decode_font_data_url(const std::string& url) {
    if (url.size() < 5 || to_lower(url.substr(0, 5)) != "data:") return std::nullopt;

    const auto comma = url.find(',');
    if (comma == std::string::npos || comma + 1 >= url.size()) return std::nullopt;

    const std::string metadata = to_lower(url.substr(5, comma - 5));
    const std::string payload = url.substr(comma + 1);
    if (payload.empty()) return std::nullopt;

    bool is_base64 = false;
    {
        size_t start = 0;
        while (start <= metadata.size()) {
            const size_t end = metadata.find(';', start);
            const std::string param = trim(metadata.substr(start, end == std::string::npos
                ? std::string::npos
                : end - start));
            if (!param.empty()) {
                if (param == "base64") {
                    is_base64 = true;
                } else if (param.rfind("base64", 0) == 0) {
                    return std::nullopt;
                }
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    if (!is_base64) {
        auto has_invalid_percent_escape = [&payload]() -> bool {
            auto is_hex_digit = [](unsigned char c) -> bool {
                return std::isxdigit(c) != 0;
            };
            for (size_t i = 0; i < payload.size(); ++i) {
                if (payload[i] != '%') continue;
                if (i + 2 >= payload.size()) return true;
                const unsigned char hi = static_cast<unsigned char>(payload[i + 1]);
                const unsigned char lo = static_cast<unsigned char>(payload[i + 2]);
                if (!is_hex_digit(hi) || !is_hex_digit(lo)) return true;
                i += 2;
            }
            return false;
        };
        if (has_invalid_percent_escape()) return std::nullopt;

        const auto decoded = clever::url::percent_decode(payload);
        if (decoded.empty()) return std::nullopt;
        return std::vector<uint8_t>(decoded.begin(), decoded.end());
    }

    std::string compact_payload;
    compact_payload.reserve(payload.size());
    for (unsigned char ch : payload) {
        if (!std::isspace(ch)) compact_payload.push_back(static_cast<char>(ch));
    }

    auto decode_char = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    const auto first_padding = compact_payload.find('=');
    if (first_padding != std::string::npos) {
        for (size_t i = first_padding; i < compact_payload.size(); ++i) {
            if (compact_payload[i] != '=') return std::nullopt;
        }
        const size_t padding_count = compact_payload.size() - first_padding;
        if (padding_count > 2 || (compact_payload.size() % 4) != 0) return std::nullopt;
    } else if ((compact_payload.size() % 4) == 1) {
        return std::nullopt;
    }

    std::vector<uint8_t> decoded;
    decoded.reserve((compact_payload.size() * 3) / 4);
    int val = 0;
    int bits = -8;
    for (char c : compact_payload) {
        if (c == '=') break;
        const int d = decode_char(c);
        if (d < 0) return std::nullopt;
        val = (val << 6) + d;
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    if (decoded.empty()) return std::nullopt;
    return decoded;
}

// =========================================================================
// WOFF font decompression
// WOFF (Web Open Font Format 1.0) is a zlib-compressed container around
// SFNT (TTF/OTF) font data. CoreText only accepts raw SFNT, so we must
// decompress WOFF to SFNT bytes before passing to CGFontCreateWithDataProvider.
//
// WOFF binary layout (all fields big-endian):
//   0:  signature      uint32  0x774F4646 ('wOFF')
//   4:  flavor         uint32  original sfnt version tag
//   8:  length         uint32  total WOFF file size
//  12:  numTables      uint16  number of table entries
//  14:  reserved       uint16
//  16:  totalSfntSize  uint32  uncompressed SFNT size hint
//  ... metadata fields ...
//  44+: table directory entries (20 bytes each):
//       tag(4) offset(4) compLength(4) origLength(4) origChecksum(4)
// =========================================================================

static inline uint32_t woff_read_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}
static inline uint16_t woff_read_u16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
static inline void woff_write_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >>  8);
    p[3] = static_cast<uint8_t>(v);
}
static inline void woff_write_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

// Convert WOFF1 font bytes to raw SFNT. Returns empty vector on failure.
static std::vector<uint8_t> decompress_woff(const std::vector<uint8_t>& woff_data) {
    // WOFF header is 44 bytes minimum
    if (woff_data.size() < 44) return {};

    const uint8_t* data = woff_data.data();
    const size_t   data_size = woff_data.size();

    // Check signature: 'wOFF' = 0x774F4646
    if (woff_read_u32(data) != 0x774F4646u) return {};

    const uint32_t sfnt_flavor = woff_read_u32(data +  4);
    const uint16_t num_tables  = woff_read_u16(data + 12);
    const uint32_t total_sfnt  = woff_read_u32(data + 16);

    if (num_tables == 0 || num_tables > 128) return {};
    if (data_size < 44u + static_cast<size_t>(num_tables) * 20u) return {};

    // Compute SFNT searchRange / entrySelector / rangeShift
    uint16_t sr = 1;
    uint16_t es = 0;
    while (static_cast<uint32_t>(sr) * 2 <= num_tables) { sr = static_cast<uint16_t>(sr * 2); ++es; }
    const uint16_t search_range   = static_cast<uint16_t>(sr * 16);
    const uint16_t entry_selector = es;
    const uint16_t range_shift    = static_cast<uint16_t>(num_tables * 16 - search_range);

    // Output SFNT buffer
    std::vector<uint8_t> out;
    out.reserve(total_sfnt > 0 ? total_sfnt : 256u * 1024u);
    out.resize(12 + static_cast<size_t>(num_tables) * 16, 0u);

    // Write SFNT offset table (12 bytes)
    woff_write_u32(out.data(),      sfnt_flavor);
    woff_write_u16(out.data() + 4,  num_tables);
    woff_write_u16(out.data() + 6,  search_range);
    woff_write_u16(out.data() + 8,  entry_selector);
    woff_write_u16(out.data() + 10, range_shift);

    // Append each table's data and fill in the SFNT table directory
    const size_t dir_base = 12;
    size_t write_pos = dir_base + static_cast<size_t>(num_tables) * 16;

    for (uint16_t i = 0; i < num_tables; ++i) {
        const uint8_t* entry = data + 44 + i * 20u;

        const uint32_t tag         = woff_read_u32(entry +  0);
        const uint32_t woff_offset = woff_read_u32(entry +  4);
        const uint32_t comp_len    = woff_read_u32(entry +  8);
        const uint32_t orig_len    = woff_read_u32(entry + 12);
        const uint32_t checksum    = woff_read_u32(entry + 16);

        if (static_cast<size_t>(woff_offset) + comp_len > data_size) return {};

        // Align write position to 4 bytes
        const size_t aligned = (write_pos + 3u) & ~static_cast<size_t>(3u);
        if (aligned + orig_len > out.size()) out.resize(aligned + orig_len, 0u);

        const uint8_t* src = data + woff_offset;

        if (comp_len == orig_len) {
            // Uncompressed table — copy directly
            std::memcpy(out.data() + aligned, src, orig_len);
        } else {
#ifdef __APPLE__
            // zlib-deflate-compressed table
            uLongf dest_len = static_cast<uLongf>(orig_len);
            const int zret = uncompress(out.data() + aligned, &dest_len,
                                        src, static_cast<uLong>(comp_len));
            if (zret != Z_OK || dest_len != static_cast<uLongf>(orig_len)) return {};
#else
            return {};
#endif
        }

        // Write SFNT table directory entry at dir_base + i*16
        uint8_t* sfnt_entry = out.data() + dir_base + i * 16u;
        woff_write_u32(sfnt_entry +  0, tag);
        woff_write_u32(sfnt_entry +  4, checksum);
        woff_write_u32(sfnt_entry +  8, static_cast<uint32_t>(aligned));
        woff_write_u32(sfnt_entry + 12, orig_len);

        write_pos = aligned + orig_len;
    }

    return out;
}

// =========================================================================
// CSS Transition: Easing Functions (Cubic Bezier approximations)
// Exposed in clever::paint namespace for testing
// =========================================================================

// Solve cubic bezier curve at parameter t for given control points (p1, p2)
// Uses Newton-Raphson for the x->t mapping, then evaluates y.
static float cubic_bezier_sample(float p1x, float p1y, float p2x, float p2y, float t) {
    // Newton-Raphson iteration to find parameter u such that bezier_x(u) = t
    // Bezier x(u) = 3*(1-u)^2*u*p1x + 3*(1-u)*u^2*p2x + u^3
    auto bezier_x = [p1x, p2x](float u) -> float {
        float inv = 1.0f - u;
        return 3.0f * inv * inv * u * p1x + 3.0f * inv * u * u * p2x + u * u * u;
    };
    auto bezier_x_deriv = [p1x, p2x](float u) -> float {
        float inv = 1.0f - u;
        return 3.0f * inv * inv * p1x + 6.0f * inv * u * (p2x - p1x) + 3.0f * u * u * (1.0f - p2x);
    };

    float u = t; // initial guess
    for (int i = 0; i < 8; i++) {
        float x = bezier_x(u) - t;
        float dx = bezier_x_deriv(u);
        if (std::fabs(dx) < 1e-6f) break;
        u -= x / dx;
        u = std::max(0.0f, std::min(1.0f, u));
    }

    // Evaluate bezier y at parameter u
    float inv = 1.0f - u;
    return 3.0f * inv * inv * u * p1y + 3.0f * inv * u * u * p2y + u * u * u;
}

float ease_linear(float t) { return t; }
float ease_ease(float t) { return cubic_bezier_sample(0.25f, 0.1f, 0.25f, 1.0f, t); }
float ease_in(float t) { return cubic_bezier_sample(0.42f, 0.0f, 1.0f, 1.0f, t); }
float ease_out(float t) { return cubic_bezier_sample(0.0f, 0.0f, 0.58f, 1.0f, t); }
float ease_in_out(float t) { return cubic_bezier_sample(0.42f, 0.0f, 0.58f, 1.0f, t); }

// Apply easing by timing function index (0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out)
float apply_easing(int timing_function, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    switch (timing_function) {
        case 1: return ease_linear(t);
        case 2: return ease_in(t);
        case 3: return ease_out(t);
        case 4: return ease_in_out(t);
        default: return ease_ease(t);
    }
}

// Apply easing with custom parameters for cubic-bezier and steps timing functions
float apply_easing_custom(int timing_function, float t,
                          float bx1, float by1, float bx2, float by2,
                          int steps_count) {
    t = std::max(0.0f, std::min(1.0f, t));
    switch (timing_function) {
        case 5: // cubic-bezier (custom)
            return cubic_bezier_sample(bx1, by1, bx2, by2, t);
        case 6: // steps-end (jump-end): floor(t * n) / n
            if (steps_count <= 0) steps_count = 1;
            return std::floor(t * steps_count) / steps_count;
        case 7: // steps-start (jump-start): ceil(t * n) / n
            if (steps_count <= 0) steps_count = 1;
            return std::ceil(t * steps_count) / steps_count;
        default:
            return apply_easing(timing_function, t);
    }
}

// =========================================================================
// CSS Transition: Interpolation Functions
// =========================================================================

float interpolate_float(float from, float to, float t) {
    return from + (to - from) * t;
}

clever::css::Color interpolate_color(const clever::css::Color& from, const clever::css::Color& to, float t) {
    clever::css::Color result;
    result.r = static_cast<uint8_t>(std::round(interpolate_float(
        static_cast<float>(from.r), static_cast<float>(to.r), t)));
    result.g = static_cast<uint8_t>(std::round(interpolate_float(
        static_cast<float>(from.g), static_cast<float>(to.g), t)));
    result.b = static_cast<uint8_t>(std::round(interpolate_float(
        static_cast<float>(from.b), static_cast<float>(to.b), t)));
    result.a = static_cast<uint8_t>(std::round(interpolate_float(
        static_cast<float>(from.a), static_cast<float>(to.a), t)));
    return result;
}

clever::css::Transform interpolate_transform(const clever::css::Transform& from,
                                              const clever::css::Transform& to, float t) {
    clever::css::Transform result;
    result.type = to.type; // use destination type
    result.x = interpolate_float(from.x, to.x, t);
    result.y = interpolate_float(from.y, to.y, t);
    result.angle = interpolate_float(from.angle, to.angle, t);
    for (int i = 0; i < 6; i++) {
        result.m[i] = interpolate_float(from.m[i], to.m[i], t);
    }
    return result;
}

clever::css::TransitionDef build_transition_def_from_style(
    const clever::css::ComputedStyle& style) {
    clever::css::TransitionDef def;
    def.property = to_lower(trim(style.transition_property.empty() ? "all" : style.transition_property));
    def.duration_ms = style.transition_duration * 1000.0f;
    def.delay_ms = style.transition_delay * 1000.0f;
    def.timing_function = style.transition_timing;
    def.bezier_x1 = style.transition_bezier_x1;
    def.bezier_y1 = style.transition_bezier_y1;
    def.bezier_x2 = style.transition_bezier_x2;
    def.bezier_y2 = style.transition_bezier_y2;
    def.steps_count = style.transition_steps_count;
    return def;
}

std::vector<std::string> detect_changed_transition_properties(
    const clever::css::ComputedStyle& old_style,
    const clever::css::ComputedStyle& new_style) {
    std::vector<std::string> changed;
    auto float_changed = [](float a, float b) {
        return std::fabs(a - b) > 1e-4f;
    };

    if (float_changed(old_style.opacity, new_style.opacity)) {
        changed.push_back("opacity");
    }
    if (old_style.background_color != new_style.background_color) {
        changed.push_back("background-color");
    }
    if (old_style.color != new_style.color) {
        changed.push_back("color");
    }
    if (float_changed(old_style.font_size.to_px(), new_style.font_size.to_px())) {
        changed.push_back("font-size");
    }
    if (float_changed(old_style.border_radius, new_style.border_radius)) {
        changed.push_back("border-radius");
    }

    // Width and height
    if (!old_style.width.is_auto() && !new_style.width.is_auto() &&
        float_changed(old_style.width.to_px(), new_style.width.to_px())) {
        changed.push_back("width");
    }
    if (!old_style.height.is_auto() && !new_style.height.is_auto() &&
        float_changed(old_style.height.to_px(), new_style.height.to_px())) {
        changed.push_back("height");
    }

    // Margin edges
    if (float_changed(old_style.margin.top.to_px(), new_style.margin.top.to_px())) {
        changed.push_back("margin-top");
    }
    if (float_changed(old_style.margin.right.to_px(), new_style.margin.right.to_px())) {
        changed.push_back("margin-right");
    }
    if (float_changed(old_style.margin.bottom.to_px(), new_style.margin.bottom.to_px())) {
        changed.push_back("margin-bottom");
    }
    if (float_changed(old_style.margin.left.to_px(), new_style.margin.left.to_px())) {
        changed.push_back("margin-left");
    }

    // Padding edges
    if (float_changed(old_style.padding.top.to_px(), new_style.padding.top.to_px())) {
        changed.push_back("padding-top");
    }
    if (float_changed(old_style.padding.right.to_px(), new_style.padding.right.to_px())) {
        changed.push_back("padding-right");
    }
    if (float_changed(old_style.padding.bottom.to_px(), new_style.padding.bottom.to_px())) {
        changed.push_back("padding-bottom");
    }
    if (float_changed(old_style.padding.left.to_px(), new_style.padding.left.to_px())) {
        changed.push_back("padding-left");
    }

    // Positioning (top/left/right/bottom)
    if (!old_style.top.is_auto() && !new_style.top.is_auto() &&
        float_changed(old_style.top.to_px(), new_style.top.to_px())) {
        changed.push_back("top");
    }
    if (!old_style.left_pos.is_auto() && !new_style.left_pos.is_auto() &&
        float_changed(old_style.left_pos.to_px(), new_style.left_pos.to_px())) {
        changed.push_back("left");
    }
    if (!old_style.right_pos.is_auto() && !new_style.right_pos.is_auto() &&
        float_changed(old_style.right_pos.to_px(), new_style.right_pos.to_px())) {
        changed.push_back("right");
    }
    if (!old_style.bottom.is_auto() && !new_style.bottom.is_auto() &&
        float_changed(old_style.bottom.to_px(), new_style.bottom.to_px())) {
        changed.push_back("bottom");
    }

    // Border widths
    if (float_changed(old_style.border_top.width.to_px(), new_style.border_top.width.to_px())) {
        changed.push_back("border-top-width");
    }
    if (float_changed(old_style.border_right.width.to_px(), new_style.border_right.width.to_px())) {
        changed.push_back("border-right-width");
    }
    if (float_changed(old_style.border_bottom.width.to_px(), new_style.border_bottom.width.to_px())) {
        changed.push_back("border-bottom-width");
    }
    if (float_changed(old_style.border_left.width.to_px(), new_style.border_left.width.to_px())) {
        changed.push_back("border-left-width");
    }

    // Letter-spacing and word-spacing
    if (float_changed(old_style.letter_spacing.to_px(), new_style.letter_spacing.to_px())) {
        changed.push_back("letter-spacing");
    }
    if (float_changed(old_style.word_spacing.to_px(), new_style.word_spacing.to_px())) {
        changed.push_back("word-spacing");
    }

    // Min/max width and height (only compare when not set to extremes)
    if (!old_style.min_width.is_zero() || !new_style.min_width.is_zero()) {
        if (float_changed(old_style.min_width.to_px(), new_style.min_width.to_px())) {
            changed.push_back("min-width");
        }
    }
    if (!old_style.min_height.is_zero() || !new_style.min_height.is_zero()) {
        if (float_changed(old_style.min_height.to_px(), new_style.min_height.to_px())) {
            changed.push_back("min-height");
        }
    }
    {
        float old_mw = old_style.max_width.to_px();
        float new_mw = new_style.max_width.to_px();
        if (old_mw < 1e8f || new_mw < 1e8f) {
            if (float_changed(old_mw, new_mw)) {
                changed.push_back("max-width");
            }
        }
    }
    {
        float old_mh = old_style.max_height.to_px();
        float new_mh = new_style.max_height.to_px();
        if (old_mh < 1e8f || new_mh < 1e8f) {
            if (float_changed(old_mh, new_mh)) {
                changed.push_back("max-height");
            }
        }
    }

    // Outline width and offset
    if (float_changed(old_style.outline_width.to_px(), new_style.outline_width.to_px())) {
        changed.push_back("outline-width");
    }
    if (float_changed(old_style.outline_offset.to_px(), new_style.outline_offset.to_px())) {
        changed.push_back("outline-offset");
    }

    // Box shadow (detect any change, even if interpolation is limited)
    bool box_shadow_changed = false;
    if (old_style.box_shadows.size() != new_style.box_shadows.size()) {
        box_shadow_changed = true;
    } else {
        for (size_t i = 0; i < old_style.box_shadows.size() && !box_shadow_changed; ++i) {
            const auto& os = old_style.box_shadows[i];
            const auto& ns = new_style.box_shadows[i];
            if (float_changed(os.offset_x, ns.offset_x) ||
                float_changed(os.offset_y, ns.offset_y) ||
                float_changed(os.blur, ns.blur) ||
                float_changed(os.spread, ns.spread) ||
                os.color != ns.color) {
                box_shadow_changed = true;
            }
        }
        // Also check legacy single shadow fields if no multi-shadow
        if (old_style.box_shadows.empty() && new_style.box_shadows.empty()) {
            if (float_changed(old_style.shadow_offset_x, new_style.shadow_offset_x) ||
                float_changed(old_style.shadow_offset_y, new_style.shadow_offset_y) ||
                float_changed(old_style.shadow_blur, new_style.shadow_blur) ||
                float_changed(old_style.shadow_spread, new_style.shadow_spread) ||
                old_style.shadow_color != new_style.shadow_color) {
                box_shadow_changed = true;
            }
        }
    }
    if (box_shadow_changed) {
        changed.push_back("box-shadow");
    }

    // Transform (compare transform vectors)
    bool transform_changed = false;
    if (old_style.transforms.size() != new_style.transforms.size()) {
        transform_changed = true;
    } else {
        for (size_t i = 0; i < old_style.transforms.size() && !transform_changed; ++i) {
            const auto& ot = old_style.transforms[i];
            const auto& nt = new_style.transforms[i];
            if (ot.type != nt.type ||
                float_changed(ot.x, nt.x) ||
                float_changed(ot.y, nt.y) ||
                float_changed(ot.angle, nt.angle)) {
                transform_changed = true;
            }
        }
    }
    if (transform_changed) {
        changed.push_back("transform");
    }

    return changed;
}

// =========================================================================

namespace { // reopen anonymous namespace for remaining internal helpers

std::string get_attr(const clever::html::SimpleNode& node, const std::string& name) {
    for (auto& attr : node.attributes) {
        if (attr.name == name) return attr.value;
    }
    return "";
}

bool has_attr(const clever::html::SimpleNode& node, const std::string& name) {
    for (auto& attr : node.attributes) {
        if (attr.name == name) return true;
    }
    return false;
}

struct MetaViewportInfo {
    bool found = false;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<float> initial_scale;
    std::optional<float> minimum_scale;
    std::optional<float> maximum_scale;
    std::optional<bool> user_scalable;
};

MetaViewportInfo parse_meta_viewport_from_head(const clever::html::SimpleNode& doc,
                                               int device_width, int device_height) {
    MetaViewportInfo info;
    const auto* head = doc.find_element("head");
    if (!head) return info;

    auto parse_number = [](std::string value) -> std::optional<float> {
        value = trim(to_lower(value));
        if (value.empty()) return std::nullopt;

        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = trim(value.substr(1, value.size() - 2));
        }
        if (value.size() > 2 && value.substr(value.size() - 2) == "px") {
            value = trim(value.substr(0, value.size() - 2));
        }

        char* end_ptr = nullptr;
        float parsed = std::strtof(value.c_str(), &end_ptr);
        if (end_ptr == value.c_str()) return std::nullopt;
        while (*end_ptr != '\0' && std::isspace(static_cast<unsigned char>(*end_ptr))) end_ptr++;
        if (*end_ptr != '\0' || !std::isfinite(parsed)) return std::nullopt;
        return parsed;
    };

    auto meta_nodes = head->find_all_elements("meta");
    for (const auto* meta : meta_nodes) {
        if (!meta) continue;
        if (to_lower(trim(get_attr(*meta, "name"))) != "viewport") continue;

        info.found = true;
        std::string content = get_attr(*meta, "content");
        if (content.empty()) break;

        size_t start = 0;
        while (start <= content.size()) {
            size_t end = content.find_first_of(",;", start);
            std::string token = trim(content.substr(start, end == std::string::npos
                                                               ? std::string::npos
                                                               : end - start));
            if (!token.empty()) {
                size_t eq = token.find('=');
                std::string key = to_lower(trim(token.substr(0, eq)));
                std::string value;
                if (eq != std::string::npos) {
                    value = to_lower(trim(token.substr(eq + 1)));
                }
                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = trim(value.substr(1, value.size() - 2));
                }

                if (key == "width") {
                    if (value == "device-width") {
                        info.width = std::max(1, device_width);
                    } else if (auto parsed = parse_number(value); parsed.has_value() && *parsed > 0.0f) {
                        info.width = std::max(1, static_cast<int>(std::lround(*parsed)));
                    }
                } else if (key == "height") {
                    if (value == "device-height") {
                        info.height = std::max(1, device_height);
                    } else if (auto parsed = parse_number(value); parsed.has_value() && *parsed > 0.0f) {
                        info.height = std::max(1, static_cast<int>(std::lround(*parsed)));
                    }
                } else if (key == "initial-scale") {
                    if (auto parsed = parse_number(value); parsed.has_value() && *parsed > 0.0f) {
                        info.initial_scale = *parsed;
                    }
                } else if (key == "minimum-scale") {
                    if (auto parsed = parse_number(value); parsed.has_value() && *parsed > 0.0f) {
                        info.minimum_scale = *parsed;
                    }
                } else if (key == "maximum-scale") {
                    if (auto parsed = parse_number(value); parsed.has_value() && *parsed > 0.0f) {
                        info.maximum_scale = *parsed;
                    }
                } else if (key == "user-scalable") {
                    if (value == "yes" || value == "1" || value == "true") {
                        info.user_scalable = true;
                    } else if (value == "no" || value == "0" || value == "false") {
                        info.user_scalable = false;
                    }
                }
            }

            if (end == std::string::npos) break;
            start = end + 1;
        }

        break; // First <meta name="viewport"> in <head> wins.
    }

    return info;
}

// Forward declaration for evaluate_media_query (defined later in this file)
bool evaluate_media_query(const std::string& condition, int vw, int vh);

int parse_color_scheme_content(const std::string& content) {
    std::string normalized = to_lower(trim(content));
    if (normalized.empty()) return 0;

    for (auto& c : normalized) {
        if (c == ',') c = ' ';
    }

    auto tokens = split_whitespace(normalized);
    bool has_light = false;
    bool has_dark = false;
    bool normal_only = false;
    for (const auto& token : tokens) {
        if (token == "normal") normal_only = true;
        if (token == "light") has_light = true;
        else if (token == "dark") has_dark = true;
    }

    if (normal_only) return 0;
    if (has_light && has_dark) return 3;
    if (has_light) return 1;
    if (has_dark) return 2;
    return 0;
}

int resolve_color_scheme_from_layout_tag(const clever::layout::LayoutNode& node,
                                        const std::string& target_tag) {
    if (to_lower(node.tag_name) == target_tag) {
        return node.color_scheme;
    }
    for (const auto& child : node.children) {
        if (!child) continue;
        int found = resolve_color_scheme_from_layout_tag(*child, target_tag);
        if (found != -1) return found;
    }
    return -1;
}

int resolve_prefers_color_scheme(int root_scheme, int meta_scheme) {
    if (root_scheme == 1 || root_scheme == 2) return root_scheme;
    if (root_scheme == 3) return 1;

    if (meta_scheme == 1 || meta_scheme == 2) return meta_scheme;
    if (meta_scheme == 3) return 1;

    return 1;
}

// Parse an HTML color attribute value: supports #RRGGBB, RRGGBB, and named colors
// Returns 0 on failure, ARGB uint32_t on success
uint32_t parse_html_color_attr(const std::string& value) {
    if (value.empty()) return 0;
    // Try hex: #RRGGBB or RRGGBB
    std::string hex = value;
    if (hex[0] != '#') hex = "#" + hex;
    if (hex.size() == 7 && hex[0] == '#') {
        try {
            uint32_t rgb = static_cast<uint32_t>(std::stoul(hex.substr(1), nullptr, 16));
            return 0xFF000000 | rgb;
        } catch (...) {}
    }
    // Try named colors
    std::string lower;
    lower.reserve(value.size());
    for (char c : value) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::unordered_map<std::string, uint32_t> html_colors = {
        {"black", 0xFF000000}, {"white", 0xFFFFFFFF}, {"red", 0xFFFF0000},
        {"green", 0xFF008000}, {"blue", 0xFF0000FF}, {"yellow", 0xFFFFFF00},
        {"orange", 0xFFFFA500}, {"purple", 0xFF800080}, {"gray", 0xFF808080},
        {"grey", 0xFF808080}, {"cyan", 0xFF00FFFF}, {"magenta", 0xFFFF00FF},
        {"lime", 0xFF00FF00}, {"maroon", 0xFF800000}, {"navy", 0xFF000080},
        {"olive", 0xFF808000}, {"teal", 0xFF008080}, {"silver", 0xFFC0C0C0},
        {"aqua", 0xFF00FFFF}, {"fuchsia", 0xFFFF00FF},
    };
    auto it = html_colors.find(lower);
    if (it != html_colors.end()) return it->second;
    return 0;
}

// Process counter-reset and counter-increment from a ComputedStyle
void process_css_counters(const clever::css::ComputedStyle& style) {
    // Handle counter-reset: "name [value]"
    if (!style.counter_reset.empty()) {
        std::istringstream iss(style.counter_reset);
        std::string name;
        while (iss >> name) {
            int val = 0;
            // Check if next token is a number
            std::streampos pos = iss.tellg();
            std::string maybe_num;
            if (iss >> maybe_num) {
                try {
                    val = std::stoi(maybe_num);
                } catch (...) {
                    // Not a number, it's another counter name — put it back
                    iss.seekg(pos);
                }
            }
            css_counters[name] = val;
        }
    }

    // Handle counter-increment: "name [value]"
    if (!style.counter_increment.empty()) {
        std::istringstream iss(style.counter_increment);
        std::string name;
        while (iss >> name) {
            int inc = 1;
            std::streampos pos = iss.tellg();
            std::string maybe_num;
            if (iss >> maybe_num) {
                try {
                    inc = std::stoi(maybe_num);
                } catch (...) {
                    iss.seekg(pos);
                }
            }
            css_counters[name] += inc;
        }
    }
}

// Resolve content value, handling counter() and attr() functions
// content_raw: the raw content string from ComputedStyle
// attr_name: for content: attr(...), the attribute name
// node: the DOM node for attr() lookup
// Stub implementations for counter() CSS function support
std::vector<std::string> split_css_function_args(const std::string& args) {
    std::vector<std::string> result;
    std::string current;
    int paren_depth = 0;
    bool in_double = false;
    bool in_single = false;

    for (char ch : args) {
        if (!in_double && !in_single) {
            if (ch == '"') {
                in_double = true;
                current += ch;
            } else if (ch == '\'') {
                in_single = true;
                current += ch;
            } else if (ch == '(') {
                paren_depth++;
                current += ch;
            } else if (ch == ')') {
                if (paren_depth > 0) paren_depth--;
                current += ch;
            } else if (ch == ',' && paren_depth == 0) {
                result.push_back(trim(current));
                current.clear();
            } else {
                current += ch;
            }
        } else if (in_double) {
            current += ch;
            if (ch == '"') in_double = false;
        } else if (in_single) {
            current += ch;
            if (ch == '\'') in_single = false;
        }
    }

    if (!current.empty()) {
        result.push_back(trim(current));
    }
    return result;
}

std::string strip_matching_quotes(const std::string& s) {
    std::string trimmed = trim(s);
    if (trimmed.size() >= 2) {
        if ((trimmed.front() == '"' && trimmed.back() == '"') ||
            (trimmed.front() == '\'' && trimmed.back() == '\'')) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
    }
    return trimmed;
}

// Map CSS list-style-type string to the integer type used in LayoutNode
// 0=disc, 1=circle, 2=square, 3=decimal, 4=decimal-leading-zero,
// 5=lower-roman, 6=upper-roman, 7=lower-alpha, 8=upper-alpha, 9=none,
// 10=lower-greek, 11=lower-latin, 12=upper-latin
int parse_counter_list_style_type(const std::string& style_str) {
    std::string s = style_str;
    // Strip surrounding quotes
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                           (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    // Lowercase and trim
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();

    if (s == "disc")                  return 0;
    if (s == "circle")                return 1;
    if (s == "square")                return 2;
    if (s == "decimal")               return 3;
    if (s == "decimal-leading-zero")  return 4;
    if (s == "lower-roman")           return 5;
    if (s == "upper-roman")           return 6;
    if (s == "lower-alpha")           return 7;
    if (s == "upper-alpha")           return 8;
    if (s == "none")                  return 9;
    if (s == "lower-greek")           return 10;
    if (s == "lower-latin")           return 11;
    if (s == "upper-latin")           return 12;
    return 3; // default to decimal
}

int get_css_counter_value(const std::string& counter_name) {
    auto it = css_counters.find(counter_name);
    return it != css_counters.end() ? it->second : 0;
}

// Convert a positive integer to roman numeral string (lower or upper case).
// Handles full range: 1-3999+
static std::string int_to_roman(int n, bool upper) {
    if (n <= 0) return std::to_string(n);
    const std::pair<int,const char*> tbl_lower[] = {
        {1000,"m"},{900,"cm"},{500,"d"},{400,"cd"},{100,"c"},{90,"xc"},
        {50,"l"},{40,"xl"},{10,"x"},{9,"ix"},{5,"v"},{4,"iv"},{1,"i"}
    };
    const std::pair<int,const char*> tbl_upper[] = {
        {1000,"M"},{900,"CM"},{500,"D"},{400,"CD"},{100,"C"},{90,"XC"},
        {50,"L"},{40,"XL"},{10,"X"},{9,"IX"},{5,"V"},{4,"IV"},{1,"I"}
    };
    std::string r;
    if (upper) {
        for (auto& [val, sym] : tbl_upper) { while (n >= val) { r += sym; n -= val; } }
    } else {
        for (auto& [val, sym] : tbl_lower) { while (n >= val) { r += sym; n -= val; } }
    }
    return r;
}

// Convert a positive integer to alphabetic string.
// 1=a, 2=b, ..., 26=z, 27=aa, 28=ab, ... (spreadsheet-column style)
static std::string int_to_alpha(int n, bool upper) {
    if (n <= 0) return std::to_string(n);
    std::string r;
    while (n > 0) {
        int rem = (n - 1) % 26;
        char c = upper ? static_cast<char>('A' + rem) : static_cast<char>('a' + rem);
        r = c + r;
        n = (n - 1) / 26;
    }
    return r;
}

// Format a counter value according to its list-style-type integer code.
std::string format_css_counter_value(int value, int style_type) {
    switch (style_type) {
        case 0: return "\xE2\x80\xA2"; // disc: U+2022 BULLET
        case 1: return "\xE2\x97\xA6"; // circle: U+25E6 WHITE BULLET
        case 2: return "\xE2\x96\xAA"; // square: U+25AA BLACK SMALL SQUARE
        case 3: // decimal
            return std::to_string(value);
        case 4: // decimal-leading-zero
            if (value >= 0 && value < 10) return "0" + std::to_string(value);
            return std::to_string(value);
        case 5: // lower-roman
            return int_to_roman(value, false);
        case 6: // upper-roman
            return int_to_roman(value, true);
        case 7:  // lower-alpha
        case 11: // lower-latin (alias)
            return int_to_alpha(value, false);
        case 8:  // upper-alpha
        case 12: // upper-latin (alias)
            return int_to_alpha(value, true);
        case 9: // none
            return "";
        case 10: { // lower-greek
            static const char* greek[] = {
                "\xCE\xB1","\xCE\xB2","\xCE\xB3","\xCE\xB4","\xCE\xB5",
                "\xCE\xB6","\xCE\xB7","\xCE\xB8","\xCE\xB9","\xCE\xBA",
                "\xCE\xBB","\xCE\xBC","\xCE\xBD","\xCE\xBE","\xCE\xBF",
                "\xCF\x80","\xCF\x81","\xCF\x83","\xCF\x84","\xCF\x85",
                "\xCF\x86","\xCF\x87","\xCF\x88","\xCF\x89"
            };
            if (value <= 0) return std::to_string(value);
            return std::string(greek[(value - 1) % 24]);
        }
        default:
            return std::to_string(value);
    }
}

std::string resolve_content_value(
    const std::string& content_raw,
    const std::string& attr_name,
    const clever::html::SimpleNode& node) {
    static constexpr const char* kLeftDoubleQuote = "\xE2\x80\x9C";  // U+201C
    static constexpr const char* kRightDoubleQuote = "\xE2\x80\x9D"; // U+201D

    // Handle sentinels emitted by style_resolver.cpp.
    if (content_raw == "\x01""ATTR" && !attr_name.empty()) {
        return get_attr(node, attr_name);
    }
    if (content_raw.rfind("\x01URL:", 0) == 0) {
        return "[image placeholder]";
    }

    const std::string content_trim = trim(content_raw);
    const std::string content_lower = to_lower(content_trim);
    if (content_trim.empty() || content_lower == "none" || content_lower == "normal") return "";

    // style_resolver stores open/close quote as literal UTF-8 chars for single-token values.
    if (content_raw == kLeftDoubleQuote || content_raw == kRightDoubleQuote) return content_raw;
    if (content_lower == "open-quote") return kLeftDoubleQuote;
    if (content_lower == "close-quote") return kRightDoubleQuote;
    if (content_lower == "no-open-quote" || content_lower == "no-close-quote") return "";

    // Fast-path for plain literal content.
    auto has_single_quoted_token = [&]() -> bool {
        for (size_t idx = 0; idx < content_raw.size(); idx++) {
            if (content_raw[idx] != '\'') continue;
            bool boundary = (idx == 0 ||
                             std::isspace(static_cast<unsigned char>(content_raw[idx - 1])) ||
                             content_raw[idx - 1] == '(' ||
                             content_raw[idx - 1] == ',');
            if (!boundary) continue;
            if (content_raw.find('\'', idx + 1) != std::string::npos) return true;
        }
        return false;
    };

    const bool requires_token_parse =
        content_raw.find("counter(") != std::string::npos ||
        content_raw.find("counters(") != std::string::npos ||
        content_raw.find("attr(") != std::string::npos ||
        content_raw.find("url(") != std::string::npos ||
        content_raw.find("open-quote") != std::string::npos ||
        content_raw.find("close-quote") != std::string::npos ||
        content_raw.find('"') != std::string::npos ||
        has_single_quoted_token();
    if (!requires_token_parse) return content_raw;

    auto find_matching_paren = [&](size_t open_pos) -> size_t {
        if (open_pos >= content_raw.size() || content_raw[open_pos] != '(') return std::string::npos;
        int depth = 0;
        bool in_single = false;
        bool in_double = false;
        for (size_t pos = open_pos; pos < content_raw.size(); pos++) {
            char ch = content_raw[pos];
            if (ch == '"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (ch == '\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (in_single || in_double) continue;
            if (ch == '(') {
                depth++;
            } else if (ch == ')') {
                depth--;
                if (depth == 0) return pos;
            }
        }
        return std::string::npos;
    };

    std::string result;
    size_t i = 0;
    while (i < content_raw.size()) {
        while (i < content_raw.size() &&
               std::isspace(static_cast<unsigned char>(content_raw[i]))) i++;
        if (i >= content_raw.size()) break;

        // String token ("..." / '...').
        if (content_raw[i] == '"' || content_raw[i] == '\'') {
            char quote = content_raw[i];
            size_t close = i + 1;
            bool escaped = false;
            while (close < content_raw.size()) {
                char ch = content_raw[close];
                if (!escaped && ch == quote) break;
                if (!escaped && ch == '\\') escaped = true;
                else escaped = false;
                close++;
            }
            if (close >= content_raw.size()) {
                // Unmatched quote in literal text: keep char as-is.
                result += content_raw[i++];
                continue;
            }
            for (size_t pos = i + 1; pos < close; pos++) {
                if (content_raw[pos] == '\\' && pos + 1 < close) {
                    pos++;
                }
                result += content_raw[pos];
            }
            i = close + 1;
            continue;
        }

        // Identifier or function token.
        if (std::isalpha(static_cast<unsigned char>(content_raw[i])) ||
            content_raw[i] == '_' || content_raw[i] == '-') {
            const size_t ident_start = i;
            while (i < content_raw.size()) {
                char ch = content_raw[i];
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == ':') {
                    i++;
                } else {
                    break;
                }
            }
            const std::string ident = content_raw.substr(ident_start, i - ident_start);
            const std::string ident_lower = to_lower(ident);

            if (i < content_raw.size() && content_raw[i] == '(') {
                const size_t open = i;
                const size_t close = find_matching_paren(open);
                if (close == std::string::npos) {
                    // Malformed function token; treat identifier text literally.
                    result += ident;
                    continue;
                }
                const std::string args = content_raw.substr(open + 1, close - open - 1);
                i = close + 1;

                if (ident_lower == "counters") {
                    std::string counter_name;
                    std::string separator = ".";
                    std::string counter_style = "decimal";
                    auto arg_parts = split_css_function_args(args);
                    if (arg_parts.size() >= 2) {
                        counter_name = arg_parts[0];
                        separator = arg_parts[1];
                        if (arg_parts.size() >= 3) counter_style = arg_parts[2];
                    } else {
                        auto parts = split_whitespace(args);
                        if (!parts.empty()) counter_name = parts[0];
                        if (parts.size() > 1) separator = parts[1];
                        if (parts.size() > 2) counter_style = parts[2];
                    }
                    counter_name = trim(counter_name);
                    separator = strip_matching_quotes(separator);
                    auto style_type = parse_counter_list_style_type(counter_style);

                    auto scope_it = css_counter_scopes.find(counter_name);
                    if (scope_it != css_counter_scopes.end() && !scope_it->second.empty()) {
                        for (size_t idx = 0; idx < scope_it->second.size(); idx++) {
                            if (idx > 0) result += separator;
                            result += format_css_counter_value(scope_it->second[idx], style_type);
                        }
                    } else {
                        result += format_css_counter_value(get_css_counter_value(counter_name), style_type);
                    }
                    continue;
                }

                if (ident_lower == "counter") {
                    std::string counter_name;
                    std::string counter_style = "decimal";
                    auto arg_parts = split_css_function_args(args);
                    if (arg_parts.size() >= 2) {
                        counter_name = arg_parts[0];
                        counter_style = arg_parts[1];
                    } else if (arg_parts.size() == 1) {
                        counter_name = arg_parts[0];
                    } else {
                        auto parts = split_whitespace(args);
                        if (!parts.empty()) {
                            counter_name = parts[0];
                            if (parts.size() > 1) counter_style = parts[1];
                        } else {
                            counter_name = args;
                        }
                    }
                    counter_name = trim(counter_name);
                    auto style_type = parse_counter_list_style_type(counter_style);
                    result += format_css_counter_value(get_css_counter_value(counter_name), style_type);
                    continue;
                }

                if (ident_lower == "attr") {
                    std::string a_name = trim(args);
                    auto comma = a_name.find(',');
                    if (comma != std::string::npos) a_name = trim(a_name.substr(0, comma));
                    auto ws = a_name.find_first_of(" \t\n\r");
                    if (ws != std::string::npos) a_name = a_name.substr(0, ws);
                    result += get_attr(node, a_name);
                    continue;
                }

                if (ident_lower == "url") {
                    result += "[image placeholder]";
                    continue;
                }

                // Unknown function for now: ignore.
                continue;
            }

            if (ident_lower == "open-quote") {
                result += kLeftDoubleQuote;
            } else if (ident_lower == "close-quote") {
                result += kRightDoubleQuote;
            } else if (ident_lower == "no-open-quote" || ident_lower == "no-close-quote" ||
                       ident_lower == "none" || ident_lower == "normal") {
                // Produces no text.
            } else {
                // Literal fallback token.
                result += ident;
            }
            continue;
        }

        // Fallback character passthrough.
        result += content_raw[i];
        i++;
    }

    return result;
}

struct StyleDecl {
    std::string property;
    std::string value;
};

std::vector<StyleDecl> parse_inline_style(const std::string& style_str) {
    std::vector<StyleDecl> decls;
    std::istringstream iss(style_str);
    std::string token;
    while (std::getline(iss, token, ';')) {
        auto colon = token.find(':');
        if (colon == std::string::npos) continue;
        std::string prop = trim(to_lower(token.substr(0, colon)));
        std::string val = trim(token.substr(colon + 1));
        // Strip !important flag from inline styles
        {
            auto imp = val.find("!important");
            if (imp == std::string::npos) imp = val.find("! important");
            if (imp != std::string::npos) {
                val = trim(val.substr(0, imp));
            }
        }
        if (!prop.empty() && !val.empty()) {
            decls.push_back({prop, val});
        }
    }
    return decls;
}

// Parse linear-gradient() into angle and color stops
bool parse_linear_gradient(const std::string& value,
                           float& angle,
                           std::vector<std::pair<uint32_t, float>>& stops) {
    // Check for linear-gradient(
    auto start = value.find("linear-gradient(");
    if (start == std::string::npos) return false;
    start += 16; // skip "linear-gradient("
    auto end = value.rfind(')');
    if (end == std::string::npos || end <= start) return false;

    std::string inner = value.substr(start, end - start);

    // Split on commas (but not inside parentheses)
    std::vector<std::string> parts;
    int paren_depth = 0;
    std::string current;
    for (char c : inner) {
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        if (c == ',' && paren_depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(trim(current));
    if (parts.size() < 2) return false;

    // Check if first part is a direction or angle
    size_t color_start = 0;
    angle = 180.0f; // default: to bottom

    std::string first = to_lower(parts[0]);
    if (first.find("deg") != std::string::npos) {
        try { angle = std::stof(first); } catch (...) {}
        color_start = 1;
    } else if (first.find("to ") == 0) {
        std::string dir = first.substr(3);
        dir = trim(dir);
        if (dir == "top") angle = 0.0f;
        else if (dir == "right") angle = 90.0f;
        else if (dir == "bottom") angle = 180.0f;
        else if (dir == "left") angle = 270.0f;
        else if (dir == "top right" || dir == "right top") angle = 45.0f;
        else if (dir == "bottom right" || dir == "right bottom") angle = 135.0f;
        else if (dir == "bottom left" || dir == "left bottom") angle = 225.0f;
        else if (dir == "top left" || dir == "left top") angle = 315.0f;
        color_start = 1;
    } else {
        // First part might be a color (no direction given)
        color_start = 0;
    }

    // Parse color stops
    stops.clear();
    size_t num_colors = parts.size() - color_start;
    if (num_colors < 2) return false;
    for (size_t i = color_start; i < parts.size(); i++) {
        std::string part = trim(parts[i]);
        // Split into color and optional position
        // Color might be "red", "#ff0000", "rgb(...)"
        auto c = clever::css::parse_color(part);
        float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);
        if (c) {
            uint32_t argb = (static_cast<uint32_t>(c->a) << 24) |
                            (static_cast<uint32_t>(c->r) << 16) |
                            (static_cast<uint32_t>(c->g) << 8) |
                            static_cast<uint32_t>(c->b);
            stops.push_back({argb, pos});
        } else {
            // Try splitting: "red 50%", "#ff0000 25%"
            auto sp = part.rfind(' ');
            if (sp != std::string::npos) {
                auto color_part = trim(part.substr(0, sp));
                auto pos_part = trim(part.substr(sp + 1));
                auto cc = clever::css::parse_color(color_part);
                if (cc) {
                    uint32_t argb = (static_cast<uint32_t>(cc->a) << 24) |
                                    (static_cast<uint32_t>(cc->r) << 16) |
                                    (static_cast<uint32_t>(cc->g) << 8) |
                                    static_cast<uint32_t>(cc->b);
                    // Parse position (percentage or length)
                    if (pos_part.back() == '%') {
                        try { pos = std::stof(pos_part) / 100.0f; } catch (...) {}
                    } else {
                        auto l = clever::css::parse_length(pos_part);
                        if (l) pos = l->to_px() / 100.0f; // approximate
                    }
                    stops.push_back({argb, pos});
                }
            }
        }
    }

    return stops.size() >= 2;
}

// Parse radial-gradient() into shape and color stops
bool parse_radial_gradient(const std::string& value,
                            int& shape,
                            std::vector<std::pair<uint32_t, float>>& stops) {
    // Check for radial-gradient(
    auto start = value.find("radial-gradient(");
    if (start == std::string::npos) return false;
    start += 16; // skip "radial-gradient("
    auto end = value.rfind(')');
    if (end == std::string::npos || end <= start) return false;

    std::string inner = value.substr(start, end - start);

    // Split on commas (but not inside parentheses)
    std::vector<std::string> parts;
    int paren_depth = 0;
    std::string current;
    for (char c : inner) {
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        if (c == ',' && paren_depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(trim(current));
    if (parts.size() < 2) return false;

    // Check if first part is a shape keyword
    size_t color_start = 0;
    shape = 0; // default: ellipse

    std::string first = to_lower(parts[0]);
    if (first == "circle") {
        shape = 1;
        color_start = 1;
    } else if (first == "ellipse") {
        shape = 0;
        color_start = 1;
    } else {
        // First part might be a color (no shape given)
        color_start = 0;
    }

    // Parse color stops
    stops.clear();
    size_t num_colors = parts.size() - color_start;
    if (num_colors < 2) return false;
    for (size_t i = color_start; i < parts.size(); i++) {
        std::string part = trim(parts[i]);
        auto c = clever::css::parse_color(part);
        float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);
        if (c) {
            uint32_t argb = (static_cast<uint32_t>(c->a) << 24) |
                            (static_cast<uint32_t>(c->r) << 16) |
                            (static_cast<uint32_t>(c->g) << 8) |
                            static_cast<uint32_t>(c->b);
            stops.push_back({argb, pos});
        } else {
            // Try splitting: "red 50%", "#ff0000 25%"
            auto sp = part.rfind(' ');
            if (sp != std::string::npos) {
                auto color_part = trim(part.substr(0, sp));
                auto pos_part = trim(part.substr(sp + 1));
                auto cc = clever::css::parse_color(color_part);
                if (cc) {
                    uint32_t argb = (static_cast<uint32_t>(cc->a) << 24) |
                                    (static_cast<uint32_t>(cc->r) << 16) |
                                    (static_cast<uint32_t>(cc->g) << 8) |
                                    static_cast<uint32_t>(cc->b);
                    if (pos_part.back() == '%') {
                        try { pos = std::stof(pos_part) / 100.0f; } catch (...) {}
                    } else {
                        auto l = clever::css::parse_length(pos_part);
                        if (l) pos = l->to_px() / 100.0f; // approximate
                    }
                    stops.push_back({argb, pos});
                }
            }
        }
    }

    return stops.size() >= 2;
}

// Helper: parse an angular string (deg/turn/rad/grad) into degrees.
// Checks "grad" before "rad" to avoid prefix collision.
static bool conic_parse_angle(const std::string& s, float& out_deg) {
    if (s.empty()) return false;
    if (s.find("turn") != std::string::npos) {
        try { out_deg = std::stof(s) * 360.0f; return true; } catch (...) {}
    } else if (s.find("grad") != std::string::npos) {
        // gradians: 400grad = 360deg
        try { out_deg = std::stof(s) * 360.0f / 400.0f; return true; } catch (...) {}
    } else if (s.find("rad") != std::string::npos) {
        try { out_deg = std::stof(s) * 180.0f / static_cast<float>(M_PI); return true; } catch (...) {}
    } else if (s.find("deg") != std::string::npos) {
        try { out_deg = std::stof(s); return true; } catch (...) {}
    }
    return false;
}

// Helper: parse a conic gradient stop position string into a 0-1 fraction.
// Handles: % (percentage) and angular units (deg/turn/rad/grad) where 360deg = 1.0.
static bool conic_parse_stop_pos(const std::string& s, float& out_pos) {
    if (s.empty()) return false;
    if (s.back() == '%') {
        try { out_pos = std::stof(s) / 100.0f; return true; } catch (...) { return false; }
    }
    float deg = 0;
    if (conic_parse_angle(s, deg)) { out_pos = deg / 360.0f; return true; }
    return false;
}

// Parse conic-gradient() into from-angle and color stops
bool parse_conic_gradient(const std::string& value,
                           float& from_angle,
                           std::vector<std::pair<uint32_t, float>>& stops) {
    auto start = value.find("conic-gradient(");
    if (start == std::string::npos) return false;
    start += 15; // skip "conic-gradient("
    auto end = value.rfind(')');
    if (end == std::string::npos || end <= start) return false;

    std::string inner = value.substr(start, end - start);

    // Split on commas (respecting parentheses)
    std::vector<std::string> parts;
    int paren_depth = 0;
    std::string current;
    for (char c : inner) {
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        if (c == ',' && paren_depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(trim(current));
    if (parts.size() < 2) return false;

    // Check first part for "from Xangle [at X Y]"
    size_t color_start = 0;
    from_angle = 0;
    std::string first = to_lower(parts[0]);
    if (first.find("from ") == 0) {
        std::string angle_str = trim(first.substr(5));
        // Strip "at ..." part if present
        auto at_pos = angle_str.find(" at ");
        if (at_pos != std::string::npos) angle_str = trim(angle_str.substr(0, at_pos));
        conic_parse_angle(angle_str, from_angle);
        color_start = 1;
    } else {
        // Try to parse first part as a color — if it works, start from 0
        auto c = clever::css::parse_color(first);
        if (c) {
            color_start = 0;
        } else {
            // Skip unrecognized shape/position keywords (e.g. "at center")
            color_start = 1;
        }
    }

    // Parse color stops. Each stop: "color [pos1 [pos2]]"
    // pos can be a percentage or angular value. Two positions create a hard stop.
    stops.clear();
    size_t num_colors = parts.size() - color_start;
    if (num_colors < 2) return false;

    auto make_argb = [](const clever::css::Color& c) -> uint32_t {
        return (static_cast<uint32_t>(c.a) << 24) |
               (static_cast<uint32_t>(c.r) << 16) |
               (static_cast<uint32_t>(c.g) << 8)  |
               static_cast<uint32_t>(c.b);
    };

    for (size_t i = color_start; i < parts.size(); i++) {
        std::string part = trim(parts[i]);
        float pos = static_cast<float>(i - color_start) / static_cast<float>(num_colors - 1);

        // Try parsing entire part as a plain color (no position token)
        auto c = clever::css::parse_color(part);
        if (c) {
            stops.push_back({make_argb(*c), pos});
            continue;
        }

        // Split on rightmost space(s) to separate color from position token(s).
        // Walk backwards to find a valid position suffix.
        bool parsed = false;
        for (int split = static_cast<int>(part.size()) - 1; split > 0 && !parsed; split--) {
            if (part[split] != ' ') continue;
            std::string color_part = trim(part.substr(0, split));
            std::string rest = trim(part.substr(split + 1));
            auto cc = clever::css::parse_color(color_part);
            if (!cc) continue;

            uint32_t argb = make_argb(*cc);
            float pos1 = pos, pos2 = -1.0f;
            std::string tok1, tok2;
            auto space_in_rest = rest.find(' ');
            if (space_in_rest != std::string::npos) {
                tok1 = trim(rest.substr(0, space_in_rest));
                tok2 = trim(rest.substr(space_in_rest + 1));
            } else {
                tok1 = rest;
            }

            if (!conic_parse_stop_pos(tok1, pos1)) continue;
            if (!tok2.empty()) conic_parse_stop_pos(tok2, pos2);

            stops.push_back({argb, pos1});
            if (pos2 >= 0.0f) {
                // Hard stop: repeat color at second position
                stops.push_back({argb, pos2});
            }
            parsed = true;
        }
        // If unparseable, skip this stop
    }

    return stops.size() >= 2;
}

// Resolve var() references in a CSS value string
static std::string resolve_css_var(const std::string& val, const clever::css::ComputedStyle& style) {
    std::string value = val;
    for (int pass = 0; pass < 8; pass++) {
        auto pos = value.find("var(");
        if (pos == std::string::npos) break;

        int depth = 1;
        size_t end = pos + 4;
        while (end < value.size() && depth > 0) {
            if (value[end] == '(') depth++;
            else if (value[end] == ')') depth--;
            if (depth > 0) end++;
        }
        if (depth != 0) break;

        std::string inner = value.substr(pos + 4, end - pos - 4);
        std::string var_name;
        std::string fallback;
        int inner_depth = 0;
        size_t split = std::string::npos;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '(') inner_depth++;
            else if (inner[i] == ')') inner_depth--;
            else if (inner[i] == ',' && inner_depth == 0) {
                split = i;
                break;
            }
        }
        if (split != std::string::npos) {
            var_name = trim(inner.substr(0, split));
            fallback = trim(inner.substr(split + 1));
        } else {
            var_name = trim(inner);
        }

        auto it = style.custom_properties.find(var_name);
        if (it != style.custom_properties.end()) {
            value = value.substr(0, pos) + it->second + value.substr(end + 1);
        } else if (!fallback.empty()) {
            value = value.substr(0, pos) + fallback + value.substr(end + 1);
        } else {
            break;
        }
    }
    return value;
}

void apply_inline_style(clever::css::ComputedStyle& style, const std::string& style_attr,
                        const clever::css::ComputedStyle* parent_style = nullptr) {
    auto decls = parse_inline_style(style_attr);
    // Default parent for inherit: use a default-constructed style if no parent provided
    clever::css::ComputedStyle default_parent;
    default_parent.z_index = clever::layout::Z_INDEX_AUTO;
    const auto& parent = parent_style ? *parent_style : default_parent;

    for (auto& d : decls) {
        // Store custom properties (--foo: value)
        if (d.property.size() > 2 && d.property[0] == '-' && d.property[1] == '-') {
            style.custom_properties[d.property] = d.value;
            continue;
        }

        // Resolve var() references in values
        if (d.value.find("var(") != std::string::npos) {
            d.value = resolve_css_var(d.value, style);
            if (d.value.find("var(") != std::string::npos) {
                // Invalid at computed-value time (unresolved custom property).
                continue;
            }
        }

        std::string val_lower = to_lower(d.value);

        // Handle inherit/initial/unset/revert keywords in inline styles
        if (val_lower == "inherit" && d.property != "all") {
            // Copy property from parent — comprehensive list matching style_resolver.cpp
            const auto& p = d.property;
            if (p == "color") { style.color = parent.color; continue; }
            if (p == "font-family") { style.font_family = parent.font_family; continue; }
            if (p == "font-size") { style.font_size = parent.font_size; continue; }
            if (p == "font-weight") { style.font_weight = parent.font_weight; continue; }
            if (p == "font-style") { style.font_style = parent.font_style; continue; }
            if (p == "line-height") { style.line_height = parent.line_height; style.line_height_unitless = parent.line_height_unitless; continue; }
            if (p == "text-align") { style.text_align = parent.text_align; continue; }
            if (p == "text-transform") { style.text_transform = parent.text_transform; continue; }
            if (p == "white-space") { style.white_space = parent.white_space; continue; }
            if (p == "letter-spacing") { style.letter_spacing = parent.letter_spacing; continue; }
            if (p == "word-spacing") { style.word_spacing = parent.word_spacing; continue; }
            if (p == "visibility") { style.visibility = parent.visibility; continue; }
            if (p == "cursor") { style.cursor = parent.cursor; continue; }
            if (p == "direction") { style.direction = parent.direction; continue; }
            if (p == "display") { style.display = parent.display; continue; }
            if (p == "position") { style.position = parent.position; continue; }
            if (p == "background-color") { style.background_color = parent.background_color; continue; }
            if (p == "opacity") { style.opacity = parent.opacity; continue; }
            if (p == "overflow") { style.overflow_x = parent.overflow_x; style.overflow_y = parent.overflow_y; continue; }
            if (p == "overflow-x") { style.overflow_x = parent.overflow_x; continue; }
            if (p == "overflow-y") { style.overflow_y = parent.overflow_y; continue; }
            if (p == "z-index") { style.z_index = parent.z_index; continue; }
            if (p == "width") { style.width = parent.width; continue; }
            if (p == "height") { style.height = parent.height; continue; }
            if (p == "margin") { style.margin = parent.margin; continue; }
            if (p == "margin-top") { style.margin.top = parent.margin.top; continue; }
            if (p == "margin-right") { style.margin.right = parent.margin.right; continue; }
            if (p == "margin-bottom") { style.margin.bottom = parent.margin.bottom; continue; }
            if (p == "margin-left") { style.margin.left = parent.margin.left; continue; }
            if (p == "padding") { style.padding = parent.padding; continue; }
            if (p == "padding-top") { style.padding.top = parent.padding.top; continue; }
            if (p == "padding-right") { style.padding.right = parent.padding.right; continue; }
            if (p == "padding-bottom") { style.padding.bottom = parent.padding.bottom; continue; }
            if (p == "padding-left") { style.padding.left = parent.padding.left; continue; }
            if (p == "border-radius") { style.border_radius = parent.border_radius; style.border_radius_tl = parent.border_radius_tl; style.border_radius_tr = parent.border_radius_tr; style.border_radius_bl = parent.border_radius_bl; style.border_radius_br = parent.border_radius_br; continue; }
            if (p == "text-decoration") { style.text_decoration = parent.text_decoration; style.text_decoration_bits = parent.text_decoration_bits; continue; }
            if (p == "box-sizing") { style.box_sizing = parent.box_sizing; continue; }
            if (p == "vertical-align") { style.vertical_align = parent.vertical_align; continue; }
            if (p == "flex-direction") { style.flex_direction = parent.flex_direction; continue; }
            if (p == "flex-wrap") { style.flex_wrap = parent.flex_wrap; continue; }
            if (p == "flex-flow") { style.flex_direction = parent.flex_direction; style.flex_wrap = parent.flex_wrap; continue; }
            if (p == "justify-content") { style.justify_content = parent.justify_content; continue; }
            if (p == "align-items") { style.align_items = parent.align_items; continue; }
            if (p == "place-items") { style.align_items = parent.align_items; style.justify_items = parent.justify_items; continue; }
            if (p == "place-content") { style.align_content = parent.align_content; style.justify_content = parent.justify_content; continue; }
            if (p == "flex-grow") { style.flex_grow = parent.flex_grow; continue; }
            if (p == "flex-shrink") { style.flex_shrink = parent.flex_shrink; continue; }
            if (p == "gap" || p == "grid-gap") { style.gap = parent.gap; style.column_gap_val = parent.column_gap_val; continue; }
            if (p == "row-gap" || p == "grid-row-gap") { style.gap = parent.gap; continue; }
            if (p == "column-gap" || p == "grid-column-gap") { style.column_gap_val = parent.column_gap_val; continue; }
            if (p == "order") { style.order = parent.order; continue; }
            if (p == "user-select") { style.user_select = parent.user_select; continue; }
            if (p == "pointer-events") { style.pointer_events = parent.pointer_events; continue; }
            continue; // unrecognized property — skip
        }
        if (val_lower == "initial" && d.property != "all") {
            // Reset to CSS initial values
            const auto& p = d.property;
            if (p == "color") { style.color = clever::css::Color::black(); continue; }
            if (p == "font-size") { style.font_size = clever::css::Length::px(16); continue; }
            if (p == "font-weight") { style.font_weight = 400; continue; }
            if (p == "font-style") { style.font_style = clever::css::FontStyle::Normal; continue; }
            if (p == "display") { style.display = clever::css::Display::Inline; continue; }
            if (p == "position") { style.position = clever::css::Position::Static; continue; }
            if (p == "background-color") { style.background_color = clever::css::Color::transparent(); continue; }
            if (p == "opacity") { style.opacity = 1.0f; continue; }
            if (p == "overflow") { style.overflow_x = clever::css::Overflow::Visible; style.overflow_y = clever::css::Overflow::Visible; continue; }
            if (p == "z-index") { style.z_index = clever::layout::Z_INDEX_AUTO; continue; }
            if (p == "width") { style.width = clever::css::Length::auto_val(); continue; }
            if (p == "height") { style.height = clever::css::Length::auto_val(); continue; }
            if (p == "margin" || p == "margin-top" || p == "margin-right" || p == "margin-bottom" || p == "margin-left") {
                if (p == "margin" || p == "margin-top") style.margin.top = clever::css::Length::zero();
                if (p == "margin" || p == "margin-right") style.margin.right = clever::css::Length::zero();
                if (p == "margin" || p == "margin-bottom") style.margin.bottom = clever::css::Length::zero();
                if (p == "margin" || p == "margin-left") style.margin.left = clever::css::Length::zero();
                continue;
            }
            if (p == "padding" || p == "padding-top" || p == "padding-right" || p == "padding-bottom" || p == "padding-left") {
                if (p == "padding" || p == "padding-top") style.padding.top = clever::css::Length::zero();
                if (p == "padding" || p == "padding-right") style.padding.right = clever::css::Length::zero();
                if (p == "padding" || p == "padding-bottom") style.padding.bottom = clever::css::Length::zero();
                if (p == "padding" || p == "padding-left") style.padding.left = clever::css::Length::zero();
                continue;
            }
            if (p == "border-radius") { style.border_radius = 0; style.border_radius_tl = 0; style.border_radius_tr = 0; style.border_radius_bl = 0; style.border_radius_br = 0; continue; }
            if (p == "text-decoration") { style.text_decoration = clever::css::TextDecoration::None; style.text_decoration_bits = 0; continue; }
            continue;
        }
        if ((val_lower == "unset" || val_lower == "revert") && d.property != "all") {
            continue; // simplified: no-op for inline styles
        }

        if (d.property == "background-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.background_color = *c;
        } else if (d.property == "background" || d.property == "background-image") {
            // Multiple backgrounds support: split by top-level commas
            // CSS spec: last layer is painted first (bottom). We use the last
            // gradient/image layer as the primary background.
            auto bg_layers = split_background_layers(d.value);
            // Use the last layer as primary (bottom-most in CSS painting order)
            std::string bg_value = bg_layers.empty() ? d.value : bg_layers.back();

            // Check for linear-gradient (including repeating)
            if (bg_value.find("linear-gradient") != std::string::npos) {
                bool is_repeating = bg_value.find("repeating-linear-gradient") != std::string::npos;
                float angle;
                std::vector<std::pair<uint32_t, float>> stops;
                if (parse_linear_gradient(bg_value, angle, stops)) {
                    style.gradient_type = is_repeating ? 4 : 1;
                    style.gradient_angle = angle;
                    style.gradient_stops = std::move(stops);
                }
            } else if (bg_value.find("radial-gradient") != std::string::npos) {
                bool is_repeating = bg_value.find("repeating-radial-gradient") != std::string::npos;
                int shape;
                std::vector<std::pair<uint32_t, float>> stops;
                if (parse_radial_gradient(bg_value, shape, stops)) {
                    style.gradient_type = is_repeating ? 5 : 2;
                    style.radial_shape = shape;
                    style.gradient_stops = std::move(stops);
                }
            } else if (bg_value.find("conic-gradient") != std::string::npos) {
                bool is_repeating = bg_value.find("repeating-conic-gradient") != std::string::npos;
                float angle;
                std::vector<std::pair<uint32_t, float>> stops;
                if (parse_conic_gradient(bg_value, angle, stops)) {
                    style.gradient_type = is_repeating ? 6 : 3;
                    style.gradient_angle = angle;
                    style.gradient_stops = std::move(stops);
                }
            } else if (bg_value.find("url(") != std::string::npos) {
                // background-image: url(...)
                auto start = bg_value.find("url(");
                auto inner_start = start + 4;
                auto inner_end = bg_value.find(')', inner_start);
                if (inner_end != std::string::npos) {
                    std::string img_url = trim(bg_value.substr(inner_start, inner_end - inner_start));
                    // Remove quotes
                    if (img_url.size() >= 2 &&
                        ((img_url.front() == '\'' && img_url.back() == '\'') ||
                         (img_url.front() == '"' && img_url.back() == '"'))) {
                        img_url = img_url.substr(1, img_url.size() - 2);
                    }
                    style.bg_image_url = img_url;
                }
            } else {
                // Treat as background-color
                auto c = clever::css::parse_color(d.value);
                if (c) style.background_color = *c;
            }
        } else if (d.property == "background-size") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "cover") style.background_size = 1;
            else if (val_low == "contain") style.background_size = 2;
            else if (val_low == "auto") style.background_size = 0;
            else {
                style.background_size = 3;
                auto sp = val_low.find(' ');
                if (sp != std::string::npos) {
                    auto lw = clever::css::parse_length(trim(d.value.substr(0, sp)));
                    auto lh = clever::css::parse_length(trim(d.value.substr(sp + 1)));
                    if (lw) style.bg_size_width = lw->to_px();
                    if (lh) style.bg_size_height = lh->to_px();
                } else {
                    auto lw = clever::css::parse_length(trim(d.value));
                    if (lw) { style.bg_size_width = lw->to_px(); style.bg_size_height = 0; }
                }
            }
        } else if (d.property == "background-repeat") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "repeat") style.background_repeat = 0;
            else if (val_low == "repeat-x") style.background_repeat = 1;
            else if (val_low == "repeat-y") style.background_repeat = 2;
            else if (val_low == "no-repeat") style.background_repeat = 3;
        } else if (d.property == "background-position") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto sp = val_low.find(' ');
            std::string xp = (sp != std::string::npos) ? trim(val_low.substr(0, sp)) : val_low;
            std::string yp = (sp != std::string::npos) ? trim(val_low.substr(sp + 1)) : "center";
            if (xp == "left") style.background_position_x = 0;
            else if (xp == "center") style.background_position_x = 1;
            else if (xp == "right") style.background_position_x = 2;
            else { auto lx = clever::css::parse_length(xp); if (lx) style.background_position_x = static_cast<int>(lx->to_px()); }
            if (yp == "top") style.background_position_y = 0;
            else if (yp == "center") style.background_position_y = 1;
            else if (yp == "bottom") style.background_position_y = 2;
            else { auto ly = clever::css::parse_length(yp); if (ly) style.background_position_y = static_cast<int>(ly->to_px()); }
        } else if (d.property == "background-position-x") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "left") style.background_position_x = 0;
            else if (val_low == "center") style.background_position_x = 1;
            else if (val_low == "right") style.background_position_x = 2;
            else { auto lx = clever::css::parse_length(val_low); if (lx) style.background_position_x = static_cast<int>(lx->to_px()); }
        } else if (d.property == "background-position-y") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "top") style.background_position_y = 0;
            else if (val_low == "center") style.background_position_y = 1;
            else if (val_low == "bottom") style.background_position_y = 2;
            else { auto ly = clever::css::parse_length(val_low); if (ly) style.background_position_y = static_cast<int>(ly->to_px()); }
        } else if (d.property == "background-clip" || d.property == "-webkit-background-clip") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "border-box") style.background_clip = 0;
            else if (val_low == "padding-box") style.background_clip = 1;
            else if (val_low == "content-box") style.background_clip = 2;
            else if (val_low == "text") style.background_clip = 3;
        } else if (d.property == "background-attachment") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (val_low == "scroll") style.background_attachment = 0;
            else if (val_low == "fixed") style.background_attachment = 1;
            else if (val_low == "local") style.background_attachment = 2;
        } else if (d.property == "color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.color = *c;
        } else if (d.property == "width") {
            if (val_lower == "min-content") style.width_keyword = -2;
            else if (val_lower == "max-content") style.width_keyword = -3;
            else if (val_lower == "fit-content") style.width_keyword = -4;
            else { auto l = clever::css::parse_length(d.value); if (l) style.width = *l; }
        } else if (d.property == "height") {
            if (val_lower == "min-content") style.height_keyword = -2;
            else if (val_lower == "max-content") style.height_keyword = -3;
            else if (val_lower == "fit-content") style.height_keyword = -4;
            else { auto l = clever::css::parse_length(d.value); if (l) style.height = *l; }
        } else if (d.property == "margin") {
            // Parse 1-4 value shorthand: margin: top [right [bottom [left]]]
            std::istringstream mss(d.value);
            std::string part;
            std::vector<clever::css::Length> vals;
            while (mss >> part) {
                std::string part_lower = to_lower(trim(part));
                if (part_lower == "auto") {
                    vals.push_back(clever::css::Length::auto_val());
                } else {
                    auto l = clever::css::parse_length(part);
                    if (l) vals.push_back(*l);
                }
            }
            if (vals.size() == 1) {
                style.margin.top = style.margin.right = style.margin.bottom = style.margin.left = vals[0];
            } else if (vals.size() == 2) {
                style.margin.top = style.margin.bottom = vals[0];
                style.margin.right = style.margin.left = vals[1];
            } else if (vals.size() == 3) {
                style.margin.top = vals[0];
                style.margin.right = style.margin.left = vals[1];
                style.margin.bottom = vals[2];
            } else if (vals.size() >= 4) {
                style.margin.top = vals[0]; style.margin.right = vals[1];
                style.margin.bottom = vals[2]; style.margin.left = vals[3];
            }
        } else if (d.property == "margin-top") {
            if (val_lower == "auto") style.margin.top = clever::css::Length::auto_val();
            else { auto l = clever::css::parse_length(d.value); if (l) style.margin.top = *l; }
        } else if (d.property == "margin-right") {
            if (val_lower == "auto") style.margin.right = clever::css::Length::auto_val();
            else { auto l = clever::css::parse_length(d.value); if (l) style.margin.right = *l; }
        } else if (d.property == "margin-bottom") {
            if (val_lower == "auto") style.margin.bottom = clever::css::Length::auto_val();
            else { auto l = clever::css::parse_length(d.value); if (l) style.margin.bottom = *l; }
        } else if (d.property == "margin-left") {
            if (val_lower == "auto") style.margin.left = clever::css::Length::auto_val();
            else { auto l = clever::css::parse_length(d.value); if (l) style.margin.left = *l; }
        } else if (d.property == "margin-block") {
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                if (parts[0] == "auto") {
                    style.margin.top = clever::css::Length::auto_val();
                    style.margin.bottom = clever::css::Length::auto_val();
                } else {
                    auto v = clever::css::parse_length(parts[0]);
                    if (v) { style.margin.top = *v; style.margin.bottom = *v; }
                }
            } else if (parts.size() >= 2) {
                if (parts[0] == "auto") style.margin.top = clever::css::Length::auto_val();
                else { auto v1 = clever::css::parse_length(parts[0]); if (v1) style.margin.top = *v1; }
                if (parts[1] == "auto") style.margin.bottom = clever::css::Length::auto_val();
                else { auto v2 = clever::css::parse_length(parts[1]); if (v2) style.margin.bottom = *v2; }
            }
        } else if (d.property == "margin-inline") {
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                if (parts[0] == "auto") {
                    style.margin.left = clever::css::Length::auto_val();
                    style.margin.right = clever::css::Length::auto_val();
                } else {
                    auto v = clever::css::parse_length(parts[0]);
                    if (v) { style.margin.left = *v; style.margin.right = *v; }
                }
            } else if (parts.size() >= 2) {
                if (parts[0] == "auto") style.margin.left = clever::css::Length::auto_val();
                else { auto v1 = clever::css::parse_length(parts[0]); if (v1) style.margin.left = *v1; }
                if (parts[1] == "auto") style.margin.right = clever::css::Length::auto_val();
                else { auto v2 = clever::css::parse_length(parts[1]); if (v2) style.margin.right = *v2; }
            }
        } else if (d.property == "margin-inline-start") {
            // Maps to margin-left in LTR, margin-right in RTL
            if (val_lower == "auto") style.margin.left = clever::css::Length::auto_val();
            else { auto v = clever::css::parse_length(val_lower); if (v) style.margin.left = *v; }
        } else if (d.property == "margin-inline-end") {
            if (val_lower == "auto") style.margin.right = clever::css::Length::auto_val();
            else { auto v = clever::css::parse_length(val_lower); if (v) style.margin.right = *v; }
        } else if (d.property == "margin-block-start") {
            if (val_lower == "auto") style.margin.top = clever::css::Length::auto_val();
            else { auto v = clever::css::parse_length(val_lower); if (v) style.margin.top = *v; }
        } else if (d.property == "margin-block-end") {
            if (val_lower == "auto") style.margin.bottom = clever::css::Length::auto_val();
            else { auto v = clever::css::parse_length(val_lower); if (v) style.margin.bottom = *v; }
        } else if (d.property == "padding-inline-start") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.padding.left = *v;
        } else if (d.property == "padding-inline-end") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.padding.right = *v;
        } else if (d.property == "padding-block-start") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.padding.top = *v;
        } else if (d.property == "padding-block-end") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.padding.bottom = *v;
        } else if (d.property == "padding") {
            // Parse 1-4 value shorthand: padding: top [right [bottom [left]]]
            std::istringstream pss(d.value);
            std::string part;
            std::vector<clever::css::Length> vals;
            while (pss >> part) {
                auto l = clever::css::parse_length(part);
                if (l) vals.push_back(*l);
            }
            if (vals.size() == 1) {
                style.padding.top = style.padding.right = style.padding.bottom = style.padding.left = vals[0];
            } else if (vals.size() == 2) {
                style.padding.top = style.padding.bottom = vals[0];
                style.padding.right = style.padding.left = vals[1];
            } else if (vals.size() == 3) {
                style.padding.top = vals[0];
                style.padding.right = style.padding.left = vals[1];
                style.padding.bottom = vals[2];
            } else if (vals.size() >= 4) {
                style.padding.top = vals[0]; style.padding.right = vals[1];
                style.padding.bottom = vals[2]; style.padding.left = vals[3];
            }
        } else if (d.property == "padding-top") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.padding.top = *l;
        } else if (d.property == "padding-right") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.padding.right = *l;
        } else if (d.property == "padding-bottom") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.padding.bottom = *l;
        } else if (d.property == "padding-left") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.padding.left = *l;
        } else if (d.property == "padding-block") {
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) { style.padding.top = *v; style.padding.bottom = *v; }
            } else if (parts.size() >= 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) style.padding.top = *v1;
                if (v2) style.padding.bottom = *v2;
            }
        } else if (d.property == "padding-inline") {
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) { style.padding.left = *v; style.padding.right = *v; }
            } else if (parts.size() >= 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) style.padding.left = *v1;
                if (v2) style.padding.right = *v2;
            }
        } else if (d.property == "font-size") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.font_size = *l;
        } else if (d.property == "font-weight") {
            if (val_lower == "bold") style.font_weight = 700;
            else if (val_lower == "normal") style.font_weight = 400;
            else {
                try { style.font_weight = std::stoi(d.value); } catch (...) {}
            }
        } else if (d.property == "text-align") {
            if (val_lower == "center") style.text_align = clever::css::TextAlign::Center;
            else if (val_lower == "-webkit-center") style.text_align = clever::css::TextAlign::WebkitCenter;
            else if (val_lower == "right" || val_lower == "end" || val_lower == "-webkit-right") style.text_align = clever::css::TextAlign::Right;
            else if (val_lower == "justify") style.text_align = clever::css::TextAlign::Justify;
            else style.text_align = clever::css::TextAlign::Left;
        } else if (d.property == "text-align-last") {
            if (val_lower == "start" || val_lower == "left") style.text_align_last = 1;
            else if (val_lower == "end" || val_lower == "right") style.text_align_last = 2;
            else if (val_lower == "center") style.text_align_last = 3;
            else if (val_lower == "justify") style.text_align_last = 4;
            else style.text_align_last = 0; // auto
        } else if (d.property == "display") {
            if (val_lower == "block") style.display = clever::css::Display::Block;
            else if (val_lower == "inline") style.display = clever::css::Display::Inline;
            else if (val_lower == "inline-block") style.display = clever::css::Display::InlineBlock;
            else if (val_lower == "table") style.display = clever::css::Display::Table;
            else if (val_lower == "table-row") style.display = clever::css::Display::TableRow;
            else if (val_lower == "table-cell") style.display = clever::css::Display::TableCell;
            else if (val_lower == "table-header-group") style.display = clever::css::Display::TableHeaderGroup;
            else if (val_lower == "table-row-group") style.display = clever::css::Display::TableRowGroup;
            else if (val_lower == "table-footer-group") style.display = clever::css::Display::TableRowGroup;
            else if (val_lower == "table-column") style.display = clever::css::Display::TableCell;
            else if (val_lower == "table-column-group") style.display = clever::css::Display::TableRow;
            else if (val_lower == "table-caption") style.display = clever::css::Display::Block;
            else if (val_lower == "inline-table") style.display = clever::css::Display::Table;
            else if (val_lower == "none") style.display = clever::css::Display::None;
            else if (val_lower == "flex") style.display = clever::css::Display::Flex;
            else if (val_lower == "inline-flex") style.display = clever::css::Display::InlineFlex;
            else if (val_lower == "grid") style.display = clever::css::Display::Grid;
            else if (val_lower == "inline-grid") style.display = clever::css::Display::InlineGrid;
            else if (val_lower == "-webkit-box" || val_lower == "-webkit-inline-box") {
                style.display = clever::css::Display::Flex; // legacy flex
            }
            else if (val_lower == "contents") style.display = clever::css::Display::Contents;
            else if (val_lower == "flow-root") { style.display = clever::css::Display::Block; style.is_flow_root = true; } // flow-root = block with BFC
        } else if (d.property == "-webkit-box-orient") {
            if (val_lower == "vertical") style.flex_direction = clever::css::FlexDirection::Column;
            else if (val_lower == "horizontal") style.flex_direction = clever::css::FlexDirection::Row;
        } else if (d.property == "flex-direction") {
            if (val_lower == "row") style.flex_direction = clever::css::FlexDirection::Row;
            else if (val_lower == "column") style.flex_direction = clever::css::FlexDirection::Column;
            else if (val_lower == "row-reverse") style.flex_direction = clever::css::FlexDirection::RowReverse;
            else if (val_lower == "column-reverse") style.flex_direction = clever::css::FlexDirection::ColumnReverse;
        } else if (d.property == "flex-wrap") {
            if (val_lower == "wrap") style.flex_wrap = clever::css::FlexWrap::Wrap;
            else if (val_lower == "wrap-reverse") style.flex_wrap = clever::css::FlexWrap::WrapReverse;
            else style.flex_wrap = clever::css::FlexWrap::NoWrap;
        } else if (d.property == "flex-flow") {
            // Shorthand: flex-flow: <direction> <wrap>
            auto parts = split_whitespace(val_lower);
            auto apply_part = [&](const std::string& part) {
                if (part == "row") style.flex_direction = clever::css::FlexDirection::Row;
                else if (part == "column") style.flex_direction = clever::css::FlexDirection::Column;
                else if (part == "row-reverse") style.flex_direction = clever::css::FlexDirection::RowReverse;
                else if (part == "column-reverse") style.flex_direction = clever::css::FlexDirection::ColumnReverse;
                else if (part == "wrap") style.flex_wrap = clever::css::FlexWrap::Wrap;
                else if (part == "wrap-reverse") style.flex_wrap = clever::css::FlexWrap::WrapReverse;
                else if (part == "nowrap") style.flex_wrap = clever::css::FlexWrap::NoWrap;
            };
            for (const auto& part : parts) apply_part(part);
        } else if (d.property == "place-items") {
            // Shorthand: place-items: <align-items> [<justify-items>]
            auto parts = split_whitespace(val_lower);
            auto parse_align_items = [](const std::string& s) -> clever::css::AlignItems {
                if (s == "center") return clever::css::AlignItems::Center;
                if (s == "flex-start" || s == "start") return clever::css::AlignItems::FlexStart;
                if (s == "flex-end" || s == "end") return clever::css::AlignItems::FlexEnd;
                if (s == "baseline") return clever::css::AlignItems::Baseline;
                return clever::css::AlignItems::Stretch;
            };
            auto parse_justify_items = [](const std::string& s) -> int {
                if (s == "start" || s == "flex-start" || s == "self-start" || s == "left") return 0;
                if (s == "end" || s == "flex-end" || s == "self-end" || s == "right") return 1;
                if (s == "center") return 2;
                return 3; // stretch
            };
            if (parts.size() == 1) {
                style.align_items = parse_align_items(parts[0]);
                style.justify_items = parse_justify_items(parts[0]);
            } else if (parts.size() >= 2) {
                style.align_items = parse_align_items(parts[0]);
                style.justify_items = parse_justify_items(parts[1]);
            }
        } else if (d.property == "justify-content") {
            if (val_lower == "center") style.justify_content = clever::css::JustifyContent::Center;
            else if (val_lower == "flex-end") style.justify_content = clever::css::JustifyContent::FlexEnd;
            else if (val_lower == "space-between") style.justify_content = clever::css::JustifyContent::SpaceBetween;
            else if (val_lower == "space-around") style.justify_content = clever::css::JustifyContent::SpaceAround;
            else if (val_lower == "space-evenly") style.justify_content = clever::css::JustifyContent::SpaceEvenly;
        } else if (d.property == "align-items") {
            if (val_lower == "center") style.align_items = clever::css::AlignItems::Center;
            else if (val_lower == "flex-end") style.align_items = clever::css::AlignItems::FlexEnd;
            else if (val_lower == "stretch") style.align_items = clever::css::AlignItems::Stretch;
            else if (val_lower == "baseline") style.align_items = clever::css::AlignItems::Baseline;
        } else if (d.property == "align-self") {
            if (val_lower == "auto") style.align_self = -1;
            else if (val_lower == "flex-start") style.align_self = 0;
            else if (val_lower == "flex-end") style.align_self = 1;
            else if (val_lower == "center") style.align_self = 2;
            else if (val_lower == "baseline") style.align_self = 3;
            else if (val_lower == "stretch") style.align_self = 4;
        } else if (d.property == "justify-self") {
            if (val_lower == "auto") style.justify_self = -1;
            else if (val_lower == "flex-start" || val_lower == "start" || val_lower == "self-start") style.justify_self = 0;
            else if (val_lower == "flex-end" || val_lower == "end" || val_lower == "self-end") style.justify_self = 1;
            else if (val_lower == "center") style.justify_self = 2;
            else if (val_lower == "baseline") style.justify_self = 3;
            else if (val_lower == "stretch") style.justify_self = 4;
        } else if (d.property == "place-self") {
            auto parts = split_whitespace(val_lower);
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
        } else if (d.property == "contain-intrinsic-size") {
            if (val_lower == "none") {
                style.contain_intrinsic_width = 0;
                style.contain_intrinsic_height = 0;
            } else {
                auto parts = split_whitespace(val_lower);
                if (parts.size() == 1) {
                    auto v = clever::css::parse_length(parts[0]);
                    if (v) { style.contain_intrinsic_width = v->to_px(); style.contain_intrinsic_height = v->to_px(); }
                } else if (parts.size() >= 2) {
                    auto v1 = clever::css::parse_length(parts[0]);
                    auto v2 = clever::css::parse_length(parts[1]);
                    if (v1) style.contain_intrinsic_width = v1->to_px();
                    if (v2) style.contain_intrinsic_height = v2->to_px();
                }
            }
        } else if (d.property == "object-fit") {
            if (val_lower == "fill") style.object_fit = 0;
            else if (val_lower == "contain") style.object_fit = 1;
            else if (val_lower == "cover") style.object_fit = 2;
            else if (val_lower == "none") style.object_fit = 3;
            else if (val_lower == "scale-down") style.object_fit = 4;
        } else if (d.property == "object-position") {
            auto parts = split_whitespace(val_lower);
            auto parse_pos = [](const std::string& s) -> float {
                if (s == "left" || s == "top") return 0.0f;
                if (s == "center") return 50.0f;
                if (s == "right" || s == "bottom") return 100.0f;
                if (!s.empty() && s.back() == '%') {
                    try { return std::stof(s.substr(0, s.size() - 1)); } catch (...) { return 50.0f; }
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
        } else if (d.property == "image-rendering") {
            if (val_lower == "smooth") style.image_rendering = 1;
            else if (val_lower == "high-quality") style.image_rendering = 2;
            else if (val_lower == "crisp-edges" || val_lower == "-webkit-optimize-contrast") style.image_rendering = 3;
            else if (val_lower == "pixelated") style.image_rendering = 4;
            else style.image_rendering = 0;
        } else if (d.property == "hanging-punctuation") {
            if (val_lower == "first") style.hanging_punctuation = 1;
            else if (val_lower == "last") style.hanging_punctuation = 2;
            else if (val_lower == "force-end") style.hanging_punctuation = 3;
            else if (val_lower == "allow-end") style.hanging_punctuation = 4;
            else if (val_lower == "first last") style.hanging_punctuation = 5;
            else style.hanging_punctuation = 0;
        } else if (d.property == "cursor") {
            if (val_lower == "pointer") style.cursor = clever::css::Cursor::Pointer;
            else if (val_lower == "text") style.cursor = clever::css::Cursor::Text;
            else if (val_lower == "move") style.cursor = clever::css::Cursor::Move;
            else if (val_lower == "not-allowed") style.cursor = clever::css::Cursor::NotAllowed;
            else if (val_lower == "default") style.cursor = clever::css::Cursor::Default;
            else style.cursor = clever::css::Cursor::Auto;
        } else if (d.property == "flex") {
            // Shorthand: flex: <grow> [<shrink> [<basis>]]
            // Common: flex:1 → grow=1,shrink=1,basis=0
            //         flex:0 1 auto → grow=0,shrink=1,basis=auto
            //         flex:none → grow=0,shrink=0,basis=auto
            //         flex:auto → grow=1,shrink=1,basis=auto
            if (val_lower == "none") {
                style.flex_grow = 0; style.flex_shrink = 0;
                style.flex_basis = clever::css::Length::auto_val();
            } else if (val_lower == "auto") {
                style.flex_grow = 1; style.flex_shrink = 1;
                style.flex_basis = clever::css::Length::auto_val();
            } else {
                std::istringstream fss(d.value);
                std::string part;
                int idx = 0;
                while (fss >> part && idx < 3) {
                    if (idx == 0) {
                        try { style.flex_grow = std::stof(part); } catch (...) {}
                        style.flex_shrink = 1; // reset
                        style.flex_basis = clever::css::Length::px(0); // unitless → basis=0
                    } else if (idx == 1) {
                        auto l = clever::css::parse_length(part);
                        if (l) { style.flex_basis = *l; }
                        else { try { style.flex_shrink = std::stof(part); } catch (...) {} }
                    } else if (idx == 2) {
                        auto l = clever::css::parse_length(part);
                        if (l) style.flex_basis = *l;
                    }
                    idx++;
                }
            }
        } else if (d.property == "flex-grow") {
            try { style.flex_grow = std::stof(d.value); } catch (...) {}
        } else if (d.property == "flex-shrink") {
            try { style.flex_shrink = std::stof(d.value); } catch (...) {}
        } else if (d.property == "flex-basis") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.flex_basis = *l;
        } else if (d.property == "order") {
            try { style.order = std::stoi(d.value); } catch (...) {}
        } else if (d.property == "gap" || d.property == "grid-gap") {
            // gap shorthand: one or two values (row-gap [column-gap])
            std::istringstream gap_ss(d.value);
            std::string first_tok, second_tok;
            gap_ss >> first_tok >> second_tok;
            auto row_l = clever::css::parse_length(first_tok);
            if (row_l) {
                style.gap = *row_l;
                style.column_gap_val = *row_l; // single value: both equal
                if (!second_tok.empty()) {
                    auto col_l = clever::css::parse_length(second_tok);
                    if (col_l) style.column_gap_val = *col_l;
                }
            }
        } else if (d.property == "row-gap" || d.property == "grid-row-gap") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.gap = *l;
        } else if (d.property == "column-gap" || d.property == "grid-column-gap") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.column_gap_val = *l;
        } else if (d.property == "opacity") {
            try { style.opacity = std::stof(d.value); } catch (...) {}
        } else if (d.property == "border") {
            // Parse shorthand: "1px solid black" or "1px dashed" or "1px"
            // Split by spaces (respecting parentheses for color functions like hsl())
            auto border_parts = split_whitespace_paren(d.value);
            clever::css::Length border_width = clever::css::Length::px(1);
            clever::css::BorderStyle border_style_val = clever::css::BorderStyle::None;
            clever::css::Color border_color = style.color;
            for (const auto& part : border_parts) {
                auto bw = clever::css::parse_length(part);
                if (bw) { border_width = *bw; continue; }
                std::string part_lower = to_lower(part);
                if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                    part_lower == "double" || part_lower == "none" || part_lower == "groove" ||
                    part_lower == "ridge" || part_lower == "inset" || part_lower == "outset" ||
                    part_lower == "hidden") {
                    if (part_lower == "none" || part_lower == "hidden") {
                        border_style_val = clever::css::BorderStyle::None;
                        border_width = clever::css::Length::zero();
                    }
                    else if (part_lower == "solid") border_style_val = clever::css::BorderStyle::Solid;
                    else if (part_lower == "dashed") border_style_val = clever::css::BorderStyle::Dashed;
                    else if (part_lower == "dotted") border_style_val = clever::css::BorderStyle::Dotted;
                    else if (part_lower == "double") border_style_val = clever::css::BorderStyle::Double;
                    else if (part_lower == "groove") border_style_val = clever::css::BorderStyle::Groove;
                    else if (part_lower == "ridge") border_style_val = clever::css::BorderStyle::Ridge;
                    else if (part_lower == "inset") border_style_val = clever::css::BorderStyle::Inset;
                    else if (part_lower == "outset") border_style_val = clever::css::BorderStyle::Outset;
                    continue;
                }
                auto bc = clever::css::parse_color(part);
                if (bc) { border_color = *bc; continue; }
            }
            style.border_top = {border_width, border_style_val, border_color};
            style.border_right = {border_width, border_style_val, border_color};
            style.border_bottom = {border_width, border_style_val, border_color};
            style.border_left = {border_width, border_style_val, border_color};
        } else if (d.property == "border-top" || d.property == "border-right" ||
                   d.property == "border-bottom" || d.property == "border-left") {
            // Per-side border shorthand: "width style color"
            auto bp = split_whitespace_paren(d.value);
            clever::css::Length bw = clever::css::Length::px(1);
            clever::css::BorderStyle bs_val = clever::css::BorderStyle::None;
            clever::css::Color bc = style.color;
            for (const auto& part : bp) {
                auto w = clever::css::parse_length(part);
                if (w) { bw = *w; continue; }
                std::string pl = to_lower(part);
                if (pl == "solid" || pl == "dashed" || pl == "dotted" || pl == "double" ||
                    pl == "none" || pl == "hidden" || pl == "groove" || pl == "ridge" ||
                    pl == "inset" || pl == "outset") {
                    if (pl == "none" || pl == "hidden") {
                        bs_val = clever::css::BorderStyle::None;
                        bw = clever::css::Length::zero();
                    }
                    else if (pl == "solid") bs_val = clever::css::BorderStyle::Solid;
                    else if (pl == "dashed") bs_val = clever::css::BorderStyle::Dashed;
                    else if (pl == "dotted") bs_val = clever::css::BorderStyle::Dotted;
                    else if (pl == "double") bs_val = clever::css::BorderStyle::Double;
                    else if (pl == "groove") bs_val = clever::css::BorderStyle::Groove;
                    else if (pl == "ridge") bs_val = clever::css::BorderStyle::Ridge;
                    else if (pl == "inset") bs_val = clever::css::BorderStyle::Inset;
                    else if (pl == "outset") bs_val = clever::css::BorderStyle::Outset;
                    continue;
                }
                auto c = clever::css::parse_color(part);
                if (c) { bc = *c; continue; }
            }
            clever::css::BorderEdge side = {bw, bs_val, bc};
            if (d.property == "border-top") style.border_top = side;
            else if (d.property == "border-right") style.border_right = side;
            else if (d.property == "border-bottom") style.border_bottom = side;
            else style.border_left = side;
        } else if (d.property == "border-block" || d.property == "border-block-start" || d.property == "border-block-end") {
            // CSS border-block logical shorthands: parse "width style color"
            auto bb_parts = split_whitespace_paren(d.value);
            clever::css::Length bw = clever::css::Length::px(1);
            clever::css::BorderStyle bs_val = clever::css::BorderStyle::None;
            clever::css::Color bc = style.color;
            for (const auto& part : bb_parts) {
                auto bwp = clever::css::parse_length(part);
                if (bwp) { bw = *bwp; continue; }
                std::string pl = to_lower(part);
                if (pl == "solid" || pl == "dashed" || pl == "dotted" || pl == "double" || pl == "none") {
                    if (pl == "none") {
                        bs_val = clever::css::BorderStyle::None;
                        bw = clever::css::Length::zero();
                    }
                    else if (pl == "solid") bs_val = clever::css::BorderStyle::Solid;
                    else if (pl == "dashed") bs_val = clever::css::BorderStyle::Dashed;
                    else if (pl == "dotted") bs_val = clever::css::BorderStyle::Dotted;
                    continue;
                }
                auto bcp = clever::css::parse_color(part);
                if (bcp) { bc = *bcp; continue; }
            }
            if (d.property == "border-block") {
                style.border_top = {bw, bs_val, bc};
                style.border_bottom = {bw, bs_val, bc};
            } else if (d.property == "border-block-start") {
                style.border_top = {bw, bs_val, bc};
            } else {
                style.border_bottom = {bw, bs_val, bc};
            }
        } else if (d.property == "border-top-color" || d.property == "border-right-color" ||
                   d.property == "border-bottom-color" || d.property == "border-left-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) {
                if (d.property == "border-top-color") style.border_top.color = *c;
                else if (d.property == "border-right-color") style.border_right.color = *c;
                else if (d.property == "border-bottom-color") style.border_bottom.color = *c;
                else style.border_left.color = *c;
            }
        } else if (d.property == "border-top-style" || d.property == "border-right-style" ||
                   d.property == "border-bottom-style" || d.property == "border-left-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v == "none" || v == "hidden") return clever::css::BorderStyle::None;
                if (v == "dashed") return clever::css::BorderStyle::Dashed;
                if (v == "dotted") return clever::css::BorderStyle::Dotted;
                if (v == "double") return clever::css::BorderStyle::Double;
                if (v == "groove") return clever::css::BorderStyle::Groove;
                if (v == "ridge") return clever::css::BorderStyle::Ridge;
                if (v == "inset") return clever::css::BorderStyle::Inset;
                if (v == "outset") return clever::css::BorderStyle::Outset;
                return clever::css::BorderStyle::Solid;
            };
            auto bs = parse_bs(val_lower);
            if (d.property == "border-top-style") style.border_top.style = bs;
            else if (d.property == "border-right-style") style.border_right.style = bs;
            else if (d.property == "border-bottom-style") style.border_bottom.style = bs;
            else style.border_left.style = bs;
        } else if (d.property == "border-top-width" || d.property == "border-right-width" ||
                   d.property == "border-bottom-width" || d.property == "border-left-width") {
            clever::css::Length bw;
            if (val_lower == "thin") bw = clever::css::Length::px(1);
            else if (val_lower == "medium") bw = clever::css::Length::px(3);
            else if (val_lower == "thick") bw = clever::css::Length::px(5);
            else {
                auto l = clever::css::parse_length(d.value);
                if (l) bw = *l;
                else bw = clever::css::Length::px(0);
            }
            if (d.property == "border-top-width") style.border_top.width = bw;
            else if (d.property == "border-right-width") style.border_right.width = bw;
            else if (d.property == "border-bottom-width") style.border_bottom.width = bw;
            else style.border_left.width = bw;
        } else if (d.property == "box-shadow") {
            if (val_lower == "none") {
                style.shadow_color = clever::css::Color::transparent();
                style.shadow_offset_x = 0;
                style.shadow_offset_y = 0;
                style.shadow_blur = 0;
                style.shadow_spread = 0;
                style.shadow_inset = false;
                style.box_shadows.clear();
            } else {
                // Split on commas respecting parentheses
                style.box_shadows.clear();
                std::vector<std::string> shadow_strs;
                {
                    size_t start = 0;
                    int paren_depth = 0;
                    for (size_t i = 0; i < d.value.size(); ++i) {
                        if (d.value[i] == '(') paren_depth++;
                        else if (d.value[i] == ')') paren_depth--;
                        else if (d.value[i] == ',' && paren_depth == 0) {
                            shadow_strs.push_back(d.value.substr(start, i - start));
                            start = i + 1;
                        }
                    }
                    shadow_strs.push_back(d.value.substr(start));
                }
                for (auto& ss : shadow_strs) {
                    size_t s = ss.find_first_not_of(" \t");
                    size_t e = ss.find_last_not_of(" \t");
                    if (s == std::string::npos) continue;
                    std::string trimmed = ss.substr(s, e - s + 1);

                    clever::css::ComputedStyle::BoxShadowEntry entry;
                    auto parts = split_whitespace_paren(trimmed);
                    std::vector<std::string> lengths;
                    std::string color_str;
                    for (auto& p : parts) {
                        std::string pl = p;
                        for (auto& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (pl == "inset") {
                            entry.inset = true;
                        } else if (clever::css::parse_length(p)) {
                            lengths.push_back(p);
                        } else {
                            if (color_str.empty()) color_str = p;
                            else color_str += " " + p;
                        }
                    }
                    if (lengths.size() >= 2) {
                        auto ox = clever::css::parse_length(lengths[0]);
                        auto oy = clever::css::parse_length(lengths[1]);
                        if (ox) entry.offset_x = ox->to_px();
                        if (oy) entry.offset_y = oy->to_px();
                        if (lengths.size() >= 3) {
                            auto blur = clever::css::parse_length(lengths[2]);
                            if (blur) entry.blur = blur->to_px();
                        }
                        if (lengths.size() >= 4) {
                            auto spread = clever::css::parse_length(lengths[3]);
                            if (spread) entry.spread = spread->to_px();
                        }
                    }
                    if (!color_str.empty()) {
                        auto c = clever::css::parse_color(color_str);
                        if (c) entry.color = *c;
                        else entry.color = {0, 0, 0, 128};
                    } else {
                        entry.color = {0, 0, 0, 128};
                    }
                    style.box_shadows.push_back(entry);
                }
                // Legacy single-shadow fields from first entry
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
        } else if (d.property == "text-shadow") {
            if (val_lower != "none") {
                // Split on commas to get individual shadow entries
                style.text_shadows.clear();
                std::string raw = d.value;
                std::vector<std::string> shadow_strs;
                {
                    size_t start = 0;
                    for (size_t i = 0; i < raw.size(); ++i) {
                        if (raw[i] == ',') {
                            shadow_strs.push_back(raw.substr(start, i - start));
                            start = i + 1;
                        }
                    }
                    shadow_strs.push_back(raw.substr(start));
                }
                for (auto& shadow_str : shadow_strs) {
                    auto parts = split_whitespace(shadow_str);
                    if (parts.size() >= 2) {
                        clever::css::ComputedStyle::TextShadowEntry entry;
                        auto ox = clever::css::parse_length(parts[0]);
                        auto oy = clever::css::parse_length(parts[1]);
                        if (ox) entry.offset_x = ox->to_px();
                        if (oy) entry.offset_y = oy->to_px();
                        if (parts.size() >= 3) {
                            auto blur_val = clever::css::parse_length(parts[2]);
                            if (blur_val) {
                                entry.blur = blur_val->to_px();
                                if (parts.size() >= 4) {
                                    auto c = clever::css::parse_color(parts[3]);
                                    if (c) entry.color = *c;
                                    else entry.color = {0, 0, 0, 128};
                                } else {
                                    entry.color = {0, 0, 0, 128};
                                }
                            } else {
                                auto c = clever::css::parse_color(parts[2]);
                                if (c) entry.color = *c;
                                else entry.color = {0, 0, 0, 128};
                            }
                        } else {
                            entry.color = {0, 0, 0, 128};
                        }
                        style.text_shadows.push_back(entry);
                    }
                }
                // Backward compat: set single text_shadow fields from first entry
                if (!style.text_shadows.empty()) {
                    auto& first = style.text_shadows[0];
                    style.text_shadow_offset_x = first.offset_x;
                    style.text_shadow_offset_y = first.offset_y;
                    style.text_shadow_blur = first.blur;
                    style.text_shadow_color = first.color;
                }
            }
        } else if (d.property == "border-radius") {
            auto parts = split_whitespace(d.value);
            // CSS border-radius: 1-4 values [ / 1-4 values ] (elliptical)
            // Order: TL=0, TR=1, BR=2, BL=3
            std::vector<float> h_radii, v_radii;
            bool after_slash = false;
            for (auto& p : parts) {
                if (p == "/") { after_slash = true; continue; }
                auto l = clever::css::parse_length(p);
                if (l) {
                    if (after_slash) v_radii.push_back(l->to_px());
                    else h_radii.push_back(l->to_px());
                }
            }
            auto expand = [](const std::vector<float>& r, size_t i) -> float {
                if (r.empty()) return 0;
                if (r.size() == 1) return r[0];
                if (r.size() == 2) return r[(i == 0 || i == 2) ? 0 : 1];
                if (r.size() == 3) { const size_t m[] = {0,1,2,1}; return r[m[i]]; }
                return r[i < r.size() ? i : 0];
            };
            if (!h_radii.empty()) {
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
        } else if (d.property == "border-top-left-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_radius_tl = l->to_px();
        } else if (d.property == "border-top-right-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_radius_tr = l->to_px();
        } else if (d.property == "border-bottom-left-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_radius_bl = l->to_px();
        } else if (d.property == "border-bottom-right-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_radius_br = l->to_px();
        } else if (d.property == "border-start-start-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_start_start_radius = l->to_px();
        } else if (d.property == "border-start-end-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_start_end_radius = l->to_px();
        } else if (d.property == "border-end-start-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_end_start_radius = l->to_px();
        } else if (d.property == "border-end-end-radius") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_end_end_radius = l->to_px();
        } else if (d.property == "border-style") {
            auto parts = split_whitespace(val_lower);
            auto parse_bs = [](const std::string& s) -> clever::css::BorderStyle {
                if (s == "solid") return clever::css::BorderStyle::Solid;
                if (s == "dashed") return clever::css::BorderStyle::Dashed;
                if (s == "dotted") return clever::css::BorderStyle::Dotted;
                if (s == "double") return clever::css::BorderStyle::Double;
                if (s == "groove") return clever::css::BorderStyle::Groove;
                if (s == "ridge") return clever::css::BorderStyle::Ridge;
                if (s == "inset") return clever::css::BorderStyle::Inset;
                if (s == "outset") return clever::css::BorderStyle::Outset;
                return clever::css::BorderStyle::None;
            };
            if (parts.size() == 1) {
                auto bs = parse_bs(parts[0]);
                style.border_top.style = style.border_right.style = style.border_bottom.style = style.border_left.style = bs;
            } else if (parts.size() == 2) {
                style.border_top.style = style.border_bottom.style = parse_bs(parts[0]);
                style.border_right.style = style.border_left.style = parse_bs(parts[1]);
            } else if (parts.size() == 3) {
                style.border_top.style = parse_bs(parts[0]);
                style.border_right.style = style.border_left.style = parse_bs(parts[1]);
                style.border_bottom.style = parse_bs(parts[2]);
            } else if (parts.size() >= 4) {
                style.border_top.style = parse_bs(parts[0]);
                style.border_right.style = parse_bs(parts[1]);
                style.border_bottom.style = parse_bs(parts[2]);
                style.border_left.style = parse_bs(parts[3]);
            }
        } else if (d.property == "border-color") {
            auto parts = split_whitespace_paren(d.value);
            auto c1 = clever::css::parse_color(parts.size() > 0 ? parts[0] : "");
            auto c2 = clever::css::parse_color(parts.size() > 1 ? parts[1] : (parts.size() > 0 ? parts[0] : ""));
            auto c3 = clever::css::parse_color(parts.size() > 2 ? parts[2] : (parts.size() > 0 ? parts[0] : ""));
            auto c4 = clever::css::parse_color(parts.size() > 3 ? parts[3] : (parts.size() > 1 ? parts[1] : (parts.size() > 0 ? parts[0] : "")));
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
        } else if (d.property == "border-width") {
            auto parts = split_whitespace(d.value);
            if (parts.size() == 1) {
                auto w = clever::css::parse_length(parts[0]);
                if (w) {
                    style.border_top.width = style.border_right.width = style.border_bottom.width = style.border_left.width = *w;
                }
            } else if (parts.size() == 2) {
                auto w1 = clever::css::parse_length(parts[0]);
                auto w2 = clever::css::parse_length(parts[1]);
                if (w1) { style.border_top.width = *w1; style.border_bottom.width = *w1; }
                if (w2) { style.border_right.width = *w2; style.border_left.width = *w2; }
            } else if (parts.size() == 3) {
                auto w1 = clever::css::parse_length(parts[0]);
                auto w2 = clever::css::parse_length(parts[1]);
                auto w3 = clever::css::parse_length(parts[2]);
                if (w1) style.border_top.width = *w1;
                if (w2) { style.border_right.width = *w2; style.border_left.width = *w2; }
                if (w3) style.border_bottom.width = *w3;
            } else if (parts.size() >= 4) {
                auto w1 = clever::css::parse_length(parts[0]);
                auto w2 = clever::css::parse_length(parts[1]);
                auto w3 = clever::css::parse_length(parts[2]);
                auto w4 = clever::css::parse_length(parts[3]);
                if (w1) style.border_top.width = *w1;
                if (w2) style.border_right.width = *w2;
                if (w3) style.border_bottom.width = *w3;
                if (w4) style.border_left.width = *w4;
            }
        } else if (d.property == "table-layout") {
            style.table_layout = (val_lower == "fixed") ? 1 : 0;
        } else if (d.property == "border-collapse") {
            std::string val_low = d.value;
            std::transform(val_low.begin(), val_low.end(), val_low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            style.border_collapse = (val_low == "collapse");
        } else if (d.property == "border-spacing") {
            std::istringstream bss(d.value);
            std::string part1, part2;
            bss >> part1 >> part2;
            auto l1 = clever::css::parse_length(part1);
            if (l1) {
                style.border_spacing = l1->to_px();
                if (!part2.empty()) {
                    auto l2 = clever::css::parse_length(part2);
                    if (l2) style.border_spacing_v = l2->to_px();
                    else style.border_spacing_v = 0;
                } else {
                    style.border_spacing_v = 0;
                }
            }
        } else if (d.property == "position") {
            if (val_lower == "relative") style.position = clever::css::Position::Relative;
            else if (val_lower == "absolute") style.position = clever::css::Position::Absolute;
            else if (val_lower == "fixed") style.position = clever::css::Position::Fixed;
            else if (val_lower == "sticky" || val_lower == "-webkit-sticky") style.position = clever::css::Position::Sticky;
            else style.position = clever::css::Position::Static;
        } else if (d.property == "float") {
            if (val_lower == "left") style.float_val = clever::css::Float::Left;
            else if (val_lower == "right") style.float_val = clever::css::Float::Right;
            else style.float_val = clever::css::Float::None;
        } else if (d.property == "clear") {
            if (val_lower == "left") style.clear = clever::css::Clear::Left;
            else if (val_lower == "right") style.clear = clever::css::Clear::Right;
            else if (val_lower == "both") style.clear = clever::css::Clear::Both;
            else style.clear = clever::css::Clear::None;
        } else if (d.property == "overflow") {
            auto parts = split_whitespace(val_lower);
            auto parse_ov = [](const std::string& v) -> clever::css::Overflow {
                if (v == "hidden") return clever::css::Overflow::Hidden;
                if (v == "scroll" || v == "auto") return clever::css::Overflow::Scroll;
                return clever::css::Overflow::Visible;
            };
            if (parts.size() >= 2) {
                // overflow: x y
                style.overflow_x = parse_ov(parts[0]);
                style.overflow_y = parse_ov(parts[1]);
            } else {
                auto ov = parse_ov(val_lower);
                style.overflow_x = ov;
                style.overflow_y = ov;
            }
        } else if (d.property == "overflow-x") {
            if (val_lower == "hidden") style.overflow_x = clever::css::Overflow::Hidden;
            else if (val_lower == "scroll" || val_lower == "auto") style.overflow_x = clever::css::Overflow::Scroll;
            else style.overflow_x = clever::css::Overflow::Visible;
        } else if (d.property == "overflow-y") {
            if (val_lower == "hidden") style.overflow_y = clever::css::Overflow::Hidden;
            else if (val_lower == "scroll" || val_lower == "auto") style.overflow_y = clever::css::Overflow::Scroll;
            else style.overflow_y = clever::css::Overflow::Visible;
        } else if (d.property == "line-height") {
            std::string lh_val = val_lower;
            if (lh_val == "normal") {
                style.line_height = clever::css::Length::px(1.2f * style.font_size.value);
            } else if (lh_val.find('%') != std::string::npos) {
                // Percentage: "150%" -> 1.5x font-size
                try {
                    float pct = std::stof(lh_val);
                    style.line_height = clever::css::Length::px((pct / 100.0f) * style.font_size.value);
                } catch (...) {}
            } else if (lh_val.find("em") != std::string::npos) {
                // em units: "1.5em" -> 1.5x font-size
                try {
                    float em = std::stof(lh_val);
                    style.line_height = clever::css::Length::px(em * style.font_size.value);
                } catch (...) {}
            } else if (lh_val.find("px") != std::string::npos) {
                // px: "24px" -> absolute pixel value
                auto l = clever::css::parse_length(d.value);
                if (l) style.line_height = *l;
            } else {
                // Unitless number: "1.5" -> 1.5x font-size multiplier
                try {
                    float factor = std::stof(lh_val);
                    style.line_height = clever::css::Length::px(factor * style.font_size.value);
                } catch (...) {}
            }
        } else if (d.property == "font-family") {
            style.font_family = d.value;
        } else if (d.property == "font-style") {
            if (val_lower == "italic") style.font_style = clever::css::FontStyle::Italic;
            else if (val_lower == "oblique") style.font_style = clever::css::FontStyle::Oblique;
            else style.font_style = clever::css::FontStyle::Normal;
        } else if (d.property == "font") {
            // CSS font shorthand: [font-style] [font-variant] [font-weight] font-size[/line-height] font-family
            // System fonts: caption, icon, menu, message-box, small-caption, status-bar
            if (val_lower == "caption" || val_lower == "icon" || val_lower == "menu" ||
                val_lower == "message-box" || val_lower == "small-caption" || val_lower == "status-bar") {
                style.font_size = clever::css::Length::px(13);
                style.font_family = "sans-serif";
            } else {
                auto parts = split_whitespace_paren(d.value);
                // Walk parts: style/variant/weight come first, then size[/line-height], then family
                size_t idx = 0;
                // Parse optional style, variant, weight
                while (idx < parts.size()) {
                    std::string pl = to_lower(parts[idx]);
                    if (pl == "italic") { style.font_style = clever::css::FontStyle::Italic; idx++; }
                    else if (pl == "oblique") { style.font_style = clever::css::FontStyle::Oblique; idx++; }
                    else if (pl == "bold") { style.font_weight = 700; idx++; }
                    else if (pl == "bolder") { style.font_weight = 700; idx++; }
                    else if (pl == "lighter") { style.font_weight = 300; idx++; }
                    else if (pl == "normal") { idx++; } // could be style, variant, or weight
                    else if (pl == "small-caps") { style.font_variant = 1; idx++; }
                    else {
                        // Check if it's a numeric weight
                        bool is_weight = false;
                        if (!pl.empty() && std::isdigit(static_cast<unsigned char>(pl[0]))) {
                            try {
                                int w = std::stoi(pl);
                                if (w >= 100 && w <= 900) { style.font_weight = w; idx++; is_weight = true; }
                            } catch (...) {}
                        }
                        if (!is_weight) break;
                    }
                }
                // Next part should be font-size (possibly with /line-height)
                // Helper lambda: resolve keyword font sizes
                auto resolve_keyword_size = [](const std::string& kw) -> float {
                    if (kw == "xx-small") return 9;
                    if (kw == "x-small") return 10;
                    if (kw == "small") return 13;
                    if (kw == "medium") return 16;
                    if (kw == "large") return 18;
                    if (kw == "x-large") return 24;
                    if (kw == "xx-large") return 32;
                    return 0; // not a keyword
                };
                if (idx < parts.size()) {
                    std::string size_part = parts[idx];
                    // Check for size/line-height syntax
                    auto slash = size_part.find('/');
                    if (slash != std::string::npos) {
                        std::string fs = size_part.substr(0, slash);
                        std::string lh = size_part.substr(slash + 1);
                        float kw_size = resolve_keyword_size(to_lower(fs));
                        if (kw_size > 0) {
                            style.font_size = clever::css::Length::px(kw_size);
                        } else {
                            auto fsl = clever::css::parse_length(fs);
                            if (fsl) style.font_size = *fsl;
                        }
                        // Line-height: try unitless multiplier first, then length
                        bool lh_set = false;
                        // Check if it has a unit suffix (px, em, %, etc.)
                        bool has_unit = false;
                        for (char c : lh) {
                            if (std::isalpha(static_cast<unsigned char>(c)) || c == '%') { has_unit = true; break; }
                        }
                        if (!has_unit) {
                            // Unitless number: treat as multiplier
                            try {
                                float factor = std::stof(lh);
                                style.line_height = clever::css::Length::px(factor * style.font_size.value);
                                lh_set = true;
                            } catch (...) {}
                        }
                        if (!lh_set) {
                            auto lhl = clever::css::parse_length(lh);
                            if (lhl) style.line_height = *lhl;
                        }
                    } else {
                        float kw_size = resolve_keyword_size(to_lower(size_part));
                        if (kw_size > 0) {
                            style.font_size = clever::css::Length::px(kw_size);
                        } else {
                            auto fsl = clever::css::parse_length(size_part);
                            if (fsl) style.font_size = *fsl;
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
            }
        } else if (d.property == "text-indent") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.text_indent = *l;
        } else if (d.property == "vertical-align") {
            if (val_lower == "top") style.vertical_align = clever::css::VerticalAlign::Top;
            else if (val_lower == "middle") style.vertical_align = clever::css::VerticalAlign::Middle;
            else if (val_lower == "bottom") style.vertical_align = clever::css::VerticalAlign::Bottom;
            else if (val_lower == "text-top") style.vertical_align = clever::css::VerticalAlign::TextTop;
            else if (val_lower == "text-bottom") style.vertical_align = clever::css::VerticalAlign::TextBottom;
            else style.vertical_align = clever::css::VerticalAlign::Baseline;
        } else if (d.property == "text-decoration" || d.property == "text-decoration-line") {
            // text-decoration shorthand: [line] [style] [color] [thickness]
            // text_decoration_bits: bitmask of line types: 1=underline, 2=overline, 4=line-through
            auto parts = split_whitespace_paren(val_lower);
            if (parts.size() == 1) {
                // Single value — line type only
                if (val_lower == "underline") style.text_decoration = clever::css::TextDecoration::Underline;
                else if (val_lower == "line-through") style.text_decoration = clever::css::TextDecoration::LineThrough;
                else if (val_lower == "overline") style.text_decoration = clever::css::TextDecoration::Overline;
                else if (val_lower == "none") style.text_decoration = clever::css::TextDecoration::None;
                // Also set bits for single values
                style.text_decoration_bits = 0;
                if (val_lower == "underline") style.text_decoration_bits = 1;
                else if (val_lower == "overline") style.text_decoration_bits = 2;
                else if (val_lower == "line-through") style.text_decoration_bits = 4;
            } else {
                // Multi-value shorthand — parse each token, combine with OR
                style.text_decoration_bits = 0;
                for (auto& tok : parts) {
                    auto tl = to_lower(tok);
                    // Line types (combine into bitmask)
                    if (tl == "underline") {
                        style.text_decoration = clever::css::TextDecoration::Underline;
                        style.text_decoration_bits |= 1;
                    } else if (tl == "line-through") {
                        style.text_decoration = clever::css::TextDecoration::LineThrough;
                        style.text_decoration_bits |= 4;
                    } else if (tl == "overline") {
                        style.text_decoration = clever::css::TextDecoration::Overline;
                        style.text_decoration_bits |= 2;
                    } else if (tl == "none") {
                        style.text_decoration = clever::css::TextDecoration::None;
                        style.text_decoration_bits = 0;
                    }
                    // Style types
                    else if (tl == "solid") style.text_decoration_style = clever::css::TextDecorationStyle::Solid;
                    else if (tl == "dashed") style.text_decoration_style = clever::css::TextDecorationStyle::Dashed;
                    else if (tl == "dotted") style.text_decoration_style = clever::css::TextDecorationStyle::Dotted;
                    else if (tl == "wavy") style.text_decoration_style = clever::css::TextDecorationStyle::Wavy;
                    else if (tl == "double") style.text_decoration_style = clever::css::TextDecorationStyle::Double;
                    // Thickness (length)
                    else {
                        auto l = clever::css::parse_length(tok);
                        if (l) {
                            style.text_decoration_thickness = l->to_px(0);
                        } else {
                            // Try as color
                            auto c = clever::css::parse_color(tok);
                            if (c) style.text_decoration_color = *c;
                        }
                    }
                }
            }
        } else if (d.property == "text-decoration-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) style.text_decoration_color = *c;
        } else if (d.property == "text-decoration-style") {
            if (val_lower == "solid") style.text_decoration_style = clever::css::TextDecorationStyle::Solid;
            else if (val_lower == "dashed") style.text_decoration_style = clever::css::TextDecorationStyle::Dashed;
            else if (val_lower == "dotted") style.text_decoration_style = clever::css::TextDecorationStyle::Dotted;
            else if (val_lower == "wavy") style.text_decoration_style = clever::css::TextDecorationStyle::Wavy;
            else if (val_lower == "double") style.text_decoration_style = clever::css::TextDecorationStyle::Double;
        } else if (d.property == "text-decoration-thickness") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.text_decoration_thickness = l->to_px(0);
        } else if (d.property == "text-transform") {
            if (val_lower == "uppercase") style.text_transform = clever::css::TextTransform::Uppercase;
            else if (val_lower == "lowercase") style.text_transform = clever::css::TextTransform::Lowercase;
            else if (val_lower == "capitalize") style.text_transform = clever::css::TextTransform::Capitalize;
            else style.text_transform = clever::css::TextTransform::None;
        } else if (d.property == "letter-spacing") {
            if (val_lower == "normal") {
                style.letter_spacing = clever::css::Length::zero();
            } else {
                auto l = clever::css::parse_length(d.value);
                if (l) style.letter_spacing = *l;
            }
        } else if (d.property == "word-spacing") {
            if (val_lower == "normal") {
                style.word_spacing = clever::css::Length::zero();
            } else {
                auto l = clever::css::parse_length(d.value);
                if (l) style.word_spacing = *l;
            }
        } else if (d.property == "visibility") {
            if (val_lower == "hidden") style.visibility = clever::css::Visibility::Hidden;
            else if (val_lower == "collapse") style.visibility = clever::css::Visibility::Collapse;
            else style.visibility = clever::css::Visibility::Visible;
        } else if (d.property == "white-space") {
            if (val_lower == "pre") style.white_space = clever::css::WhiteSpace::Pre;
            else if (val_lower == "pre-wrap") style.white_space = clever::css::WhiteSpace::PreWrap;
            else if (val_lower == "pre-line") style.white_space = clever::css::WhiteSpace::PreLine;
            else if (val_lower == "nowrap") style.white_space = clever::css::WhiteSpace::NoWrap;
            else if (val_lower == "break-spaces") style.white_space = clever::css::WhiteSpace::BreakSpaces;
            else style.white_space = clever::css::WhiteSpace::Normal;
        } else if (d.property == "text-overflow") {
            if (val_lower == "ellipsis") style.text_overflow = clever::css::TextOverflow::Ellipsis;
            else if (val_lower == "fade") style.text_overflow = clever::css::TextOverflow::Fade;
            else style.text_overflow = clever::css::TextOverflow::Clip;
        } else if (d.property == "word-break") {
            if (val_lower == "break-all") style.word_break = 1;
            else if (val_lower == "keep-all") style.word_break = 2;
            else style.word_break = 0;
        } else if (d.property == "overflow-wrap" || d.property == "word-wrap") {
            if (val_lower == "break-word") style.overflow_wrap = 1;
            else if (val_lower == "anywhere") style.overflow_wrap = 2;
            else style.overflow_wrap = 0;
        } else if (d.property == "text-wrap" || d.property == "text-wrap-mode") {
            if (val_lower == "nowrap") style.text_wrap = 1;
            else if (val_lower == "balance") style.text_wrap = 2;
            else if (val_lower == "pretty") style.text_wrap = 3;
            else if (val_lower == "stable") style.text_wrap = 4;
            else style.text_wrap = 0; // wrap
        } else if (d.property == "text-wrap-style") {
            if (val_lower == "balance") style.text_wrap = 2;
            else if (val_lower == "pretty") style.text_wrap = 3;
            else if (val_lower == "stable") style.text_wrap = 4;
        } else if (d.property == "white-space-collapse") {
            if (val_lower == "collapse") style.white_space_collapse = 0;
            else if (val_lower == "preserve") style.white_space_collapse = 1;
            else if (val_lower == "preserve-breaks") style.white_space_collapse = 2;
            else if (val_lower == "break-spaces") style.white_space_collapse = 3;
        } else if (d.property == "line-break") {
            if (val_lower == "auto") style.line_break = 0;
            else if (val_lower == "loose") style.line_break = 1;
            else if (val_lower == "normal") style.line_break = 2;
            else if (val_lower == "strict") style.line_break = 3;
            else if (val_lower == "anywhere") style.line_break = 4;
        } else if (d.property == "orphans") {
            try { style.orphans = std::stoi(d.value); } catch (...) {}
        } else if (d.property == "widows") {
            try { style.widows = std::stoi(d.value); } catch (...) {}
        } else if (d.property == "column-span") {
            if (val_lower == "all") style.column_span = 1;
            else style.column_span = 0;
        } else if (d.property == "break-before") {
            if (val_lower == "auto") style.break_before = 0;
            else if (val_lower == "avoid") style.break_before = 1;
            else if (val_lower == "always") style.break_before = 2;
            else if (val_lower == "page") style.break_before = 3;
            else if (val_lower == "column") style.break_before = 4;
            else if (val_lower == "region") style.break_before = 5;
        } else if (d.property == "break-after") {
            if (val_lower == "auto") style.break_after = 0;
            else if (val_lower == "avoid") style.break_after = 1;
            else if (val_lower == "always") style.break_after = 2;
            else if (val_lower == "page") style.break_after = 3;
            else if (val_lower == "column") style.break_after = 4;
            else if (val_lower == "region") style.break_after = 5;
        } else if (d.property == "break-inside") {
            if (val_lower == "auto") style.break_inside = 0;
            else if (val_lower == "avoid") style.break_inside = 1;
            else if (val_lower == "avoid-page") style.break_inside = 2;
            else if (val_lower == "avoid-column") style.break_inside = 3;
            else if (val_lower == "avoid-region") style.break_inside = 4;
        } else if (d.property == "page-break-before") {
            if (val_lower == "auto") style.page_break_before = 0;
            else if (val_lower == "always") style.page_break_before = 1;
            else if (val_lower == "avoid") style.page_break_before = 2;
            else if (val_lower == "left") style.page_break_before = 3;
            else if (val_lower == "right") style.page_break_before = 4;
        } else if (d.property == "page-break-after") {
            if (val_lower == "auto") style.page_break_after = 0;
            else if (val_lower == "always") style.page_break_after = 1;
            else if (val_lower == "avoid") style.page_break_after = 2;
            else if (val_lower == "left") style.page_break_after = 3;
            else if (val_lower == "right") style.page_break_after = 4;
        } else if (d.property == "page-break-inside") {
            if (val_lower == "auto") style.page_break_inside = 0;
            else if (val_lower == "avoid") style.page_break_inside = 1;
        } else if (d.property == "page") {
            style.page = val_lower;
        } else if (d.property == "background-clip" || d.property == "-webkit-background-clip") {
            if (val_lower == "border-box") style.background_clip = 0;
            else if (val_lower == "padding-box") style.background_clip = 1;
            else if (val_lower == "content-box") style.background_clip = 2;
            else if (val_lower == "text") style.background_clip = 3;
        } else if (d.property == "background-origin") {
            if (val_lower == "padding-box") style.background_origin = 0;
            else if (val_lower == "border-box") style.background_origin = 1;
            else if (val_lower == "content-box") style.background_origin = 2;
        } else if (d.property == "background-blend-mode") {
            if (val_lower == "normal") style.background_blend_mode = 0;
            else if (val_lower == "multiply") style.background_blend_mode = 1;
            else if (val_lower == "screen") style.background_blend_mode = 2;
            else if (val_lower == "overlay") style.background_blend_mode = 3;
            else if (val_lower == "darken") style.background_blend_mode = 4;
            else if (val_lower == "lighten") style.background_blend_mode = 5;
        } else if (d.property == "background-attachment") {
            if (val_lower == "scroll") style.background_attachment = 0;
            else if (val_lower == "fixed") style.background_attachment = 1;
            else if (val_lower == "local") style.background_attachment = 2;
        } else if (d.property == "unicode-bidi") {
            if (val_lower == "normal") style.unicode_bidi = 0;
            else if (val_lower == "embed") style.unicode_bidi = 1;
            else if (val_lower == "bidi-override") style.unicode_bidi = 2;
            else if (val_lower == "isolate") style.unicode_bidi = 3;
            else if (val_lower == "isolate-override") style.unicode_bidi = 4;
            else if (val_lower == "plaintext") style.unicode_bidi = 5;
        } else if (d.property == "top") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.top = *l;
        } else if (d.property == "right") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.right_pos = *l;
        } else if (d.property == "bottom") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.bottom = *l;
        } else if (d.property == "left") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.left_pos = *l;
        } else if (d.property == "box-sizing") {
            if (val_lower == "border-box") style.box_sizing = clever::css::BoxSizing::BorderBox;
            else style.box_sizing = clever::css::BoxSizing::ContentBox;
        } else if (d.property == "z-index") {
            if (val_lower == "auto") style.z_index = clever::layout::Z_INDEX_AUTO;
            else {
                try { style.z_index = std::stoi(d.value); } catch (...) {}
            }
        } else if (d.property == "outline") {
            // Parse shorthand: "2px solid red" (like border shorthand)
            std::istringstream oss(d.value);
            std::string part;
            clever::css::Length outline_width = clever::css::Length::px(1);
            clever::css::Color outline_color = style.color;
            clever::css::BorderStyle outline_style = clever::css::BorderStyle::None;
            while (oss >> part) {
                auto ow = clever::css::parse_length(part);
                if (ow) { outline_width = *ow; continue; }
                std::string part_lower = to_lower(part);
                if (part_lower == "solid" || part_lower == "dashed" || part_lower == "dotted" ||
                    part_lower == "double" || part_lower == "groove" || part_lower == "ridge" ||
                    part_lower == "inset" || part_lower == "outset" ||
                    part_lower == "none") {
                    if (part_lower == "none") {
                        outline_style = clever::css::BorderStyle::None;
                        outline_width = clever::css::Length::zero();
                    }
                    else if (part_lower == "solid") outline_style = clever::css::BorderStyle::Solid;
                    else if (part_lower == "dashed") outline_style = clever::css::BorderStyle::Dashed;
                    else if (part_lower == "dotted") outline_style = clever::css::BorderStyle::Dotted;
                    else if (part_lower == "double") outline_style = clever::css::BorderStyle::Double;
                    else if (part_lower == "groove") outline_style = clever::css::BorderStyle::Groove;
                    else if (part_lower == "ridge") outline_style = clever::css::BorderStyle::Ridge;
                    else if (part_lower == "inset") outline_style = clever::css::BorderStyle::Inset;
                    else if (part_lower == "outset") outline_style = clever::css::BorderStyle::Outset;
                    continue;
                }
                auto oc = clever::css::parse_color(part);
                if (oc) { outline_color = *oc; continue; }
            }
            style.outline_width = outline_width;
            style.outline_style = outline_style;
            style.outline_color = outline_color;
        } else if (d.property == "outline-width") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.outline_width = *l;
        } else if (d.property == "outline-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.outline_color = *c;
        } else if (d.property == "outline-style") {
            if (val_lower == "none") style.outline_style = clever::css::BorderStyle::None;
            else if (val_lower == "solid") style.outline_style = clever::css::BorderStyle::Solid;
            else if (val_lower == "dashed") style.outline_style = clever::css::BorderStyle::Dashed;
            else if (val_lower == "dotted") style.outline_style = clever::css::BorderStyle::Dotted;
            else if (val_lower == "double") style.outline_style = clever::css::BorderStyle::Double;
            else if (val_lower == "groove") style.outline_style = clever::css::BorderStyle::Groove;
            else if (val_lower == "ridge") style.outline_style = clever::css::BorderStyle::Ridge;
            else if (val_lower == "inset") style.outline_style = clever::css::BorderStyle::Inset;
            else if (val_lower == "outset") style.outline_style = clever::css::BorderStyle::Outset;
        } else if (d.property == "outline-offset") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.outline_offset = *l;
        } else if (d.property == "border-inline-start" || d.property == "border-inline-end") {
            // Parse shorthand: [width] [style] [color]
            auto parts = split_whitespace(d.value);
            clever::css::Length bw = clever::css::Length::px(0);
            clever::css::BorderStyle bs = clever::css::BorderStyle::None;
            clever::css::Color bc = style.color;
            for (auto& p : parts) {
                std::string pl = to_lower(p);
                if (pl == "solid") { bs = clever::css::BorderStyle::Solid; continue; }
                else if (pl == "dashed") { bs = clever::css::BorderStyle::Dashed; continue; }
                else if (pl == "dotted") { bs = clever::css::BorderStyle::Dotted; continue; }
                else if (pl == "double") { bs = clever::css::BorderStyle::Double; continue; }
                else if (pl == "none") { bs = clever::css::BorderStyle::None; continue; }
                auto len = clever::css::parse_length(pl);
                if (len) { bw = *len; continue; }
                auto col = clever::css::parse_color(p);
                if (col) { bc = *col; continue; }
            }
            // LTR: start=left, end=right
            if (d.property == "border-inline-start") {
                style.border_left = {bw, bs, bc};
            } else {
                style.border_right = {bw, bs, bc};
            }
        } else if (d.property == "border-inline-width") {
            // CSS border-inline-width: sets inline (left/right) border widths
            std::istringstream biw_iss(d.value);
            std::string biw_v1, biw_v2;
            biw_iss >> biw_v1 >> biw_v2;
            auto biw_start = clever::css::parse_length(biw_v1);
            auto biw_end = biw_v2.empty() ? biw_start : clever::css::parse_length(biw_v2);
            if (biw_start) {
                style.border_left.width = *biw_start;
            }
            if (biw_end) {
                style.border_right.width = *biw_end;
            }
        } else if (d.property == "border-block-width") {
            // CSS border-block-width: sets block (top/bottom) border widths
            std::istringstream bbw_iss(d.value);
            std::string bbw_v1, bbw_v2;
            bbw_iss >> bbw_v1 >> bbw_v2;
            auto bbw_start = clever::css::parse_length(bbw_v1);
            auto bbw_end = bbw_v2.empty() ? bbw_start : clever::css::parse_length(bbw_v2);
            if (bbw_start) {
                style.border_top.width = *bbw_start;
            }
            if (bbw_end) {
                style.border_bottom.width = *bbw_end;
            }
        } else if (d.property == "border-inline-color") {
            // CSS border-inline-color: sets inline (left/right) border colors
            auto bic_c = clever::css::parse_color(d.value);
            if (bic_c) {
                style.border_left.color = *bic_c;
                style.border_right.color = *bic_c;
            }
        } else if (d.property == "border-block-color") {
            auto bbc_c = clever::css::parse_color(d.value);
            if (bbc_c) {
                style.border_top.color = *bbc_c;
                style.border_bottom.color = *bbc_c;
            }
        } else if (d.property == "border-inline-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                if (v=="groove") return clever::css::BorderStyle::Groove;
                if (v=="ridge") return clever::css::BorderStyle::Ridge;
                if (v=="inset") return clever::css::BorderStyle::Inset;
                if (v=="outset") return clever::css::BorderStyle::Outset;
                return clever::css::BorderStyle::None;
            };
            style.border_left.style = parse_bs(val_lower);
            style.border_right.style = parse_bs(val_lower);
        } else if (d.property == "border-block-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                if (v=="groove") return clever::css::BorderStyle::Groove;
                if (v=="ridge") return clever::css::BorderStyle::Ridge;
                if (v=="inset") return clever::css::BorderStyle::Inset;
                if (v=="outset") return clever::css::BorderStyle::Outset;
                return clever::css::BorderStyle::None;
            };
            style.border_top.style = parse_bs(val_lower);
            style.border_bottom.style = parse_bs(val_lower);
        } else if (d.property == "border-inline-start-width") {
            auto v = clever::css::parse_length(val_lower);
            if (v) { style.border_left.width = *v; }
        } else if (d.property == "border-inline-end-width") {
            auto v = clever::css::parse_length(val_lower);
            if (v) { style.border_right.width = *v; }
        } else if (d.property == "border-block-start-width") {
            auto v = clever::css::parse_length(val_lower);
            if (v) { style.border_top.width = *v; }
        } else if (d.property == "border-block-end-width") {
            auto v = clever::css::parse_length(val_lower);
            if (v) { style.border_bottom.width = *v; }
        } else if (d.property == "border-inline-start-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.border_left.color = *c;
        } else if (d.property == "border-inline-end-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.border_right.color = *c;
        } else if (d.property == "border-block-start-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.border_top.color = *c;
        } else if (d.property == "border-block-end-color") {
            auto c = clever::css::parse_color(d.value);
            if (c) style.border_bottom.color = *c;
        } else if (d.property == "border-inline-start-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                return clever::css::BorderStyle::None;
            };
            style.border_left.style = parse_bs(val_lower);
        } else if (d.property == "border-inline-end-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                return clever::css::BorderStyle::None;
            };
            style.border_right.style = parse_bs(val_lower);
        } else if (d.property == "border-block-start-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                return clever::css::BorderStyle::None;
            };
            style.border_top.style = parse_bs(val_lower);
        } else if (d.property == "border-block-end-style") {
            auto parse_bs = [](const std::string& v) -> clever::css::BorderStyle {
                if (v=="solid") return clever::css::BorderStyle::Solid;
                if (v=="dashed") return clever::css::BorderStyle::Dashed;
                if (v=="dotted") return clever::css::BorderStyle::Dotted;
                if (v=="double") return clever::css::BorderStyle::Double;
                return clever::css::BorderStyle::None;
            };
            style.border_bottom.style = parse_bs(val_lower);
        } else if (d.property == "border-image") {
            // border-image shorthand: source || slice || width || outset || repeat
            // Most common: border-image: linear-gradient(...) 1;
            // Extract the source (gradient or url function)
            std::string val = d.value;
            std::string source;
            if (val.find("linear-gradient(") != std::string::npos ||
                val.find("radial-gradient(") != std::string::npos ||
                val.find("conic-gradient(") != std::string::npos) {
                // Find the matching closing paren for the gradient
                auto gstart = val.find("-gradient(");
                if (gstart != std::string::npos) {
                    // Back up to find the gradient type prefix
                    auto prefix_start = val.rfind(' ', gstart);
                    if (prefix_start == std::string::npos) prefix_start = 0;
                    else prefix_start++;
                    auto paren_start = val.find('(', gstart);
                    int depth = 1;
                    size_t pos = paren_start + 1;
                    while (pos < val.size() && depth > 0) {
                        if (val[pos] == '(') depth++;
                        else if (val[pos] == ')') depth--;
                        pos++;
                    }
                    source = val.substr(prefix_start, pos - prefix_start);
                }
            } else if (val.find("url(") != std::string::npos) {
                auto start = val.find("url(");
                auto end = val.find(')', start);
                if (end != std::string::npos) source = val.substr(start, end - start + 1);
            }
            if (!source.empty()) style.border_image_source = source;
        } else if (d.property == "border-image-source") {
            if (val_lower == "none") {
                style.border_image_source.clear();
            } else if (d.value.find("url(") != std::string::npos) {
                auto start = d.value.find("url(");
                auto inner_start = start + 4;
                auto inner_end = d.value.find(')', inner_start);
                if (inner_end != std::string::npos) {
                    std::string img_url = trim(d.value.substr(inner_start, inner_end - inner_start));
                    if (img_url.size() >= 2 &&
                        ((img_url.front() == '\'' && img_url.back() == '\'') ||
                         (img_url.front() == '"' && img_url.back() == '"'))) {
                        img_url = img_url.substr(1, img_url.size() - 2);
                    }
                    style.border_image_source = img_url;
                }
            } else {
                style.border_image_source = d.value;
            }
        } else if (d.property == "border-image-slice") {
            std::istringstream iss(d.value);
            std::string part;
            while (iss >> part) {
                std::string part_lower = to_lower(part);
                if (part_lower == "fill") {
                    style.border_image_slice_fill = true;
                } else {
                    // Strip trailing '%' if present
                    std::string num = part;
                    if (!num.empty() && num.back() == '%') num.pop_back();
                    try { style.border_image_slice = std::stof(num); } catch (...) {}
                }
            }
        } else if (d.property == "border-image-width") {
            std::string num = d.value;
            if (!num.empty() && (num.back() == 'x' || num.back() == 'X')) {
                // Strip "px" suffix
                auto px_pos = val_lower.find("px");
                if (px_pos != std::string::npos) num = d.value.substr(0, px_pos);
            }
            try { style.border_image_width_val = std::stof(trim(num)); } catch (...) {}
        } else if (d.property == "border-image-outset") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.border_image_outset = l->to_px(0);
        } else if (d.property == "border-image-repeat") {
            if (val_lower == "stretch") style.border_image_repeat = 0;
            else if (val_lower == "repeat") style.border_image_repeat = 1;
            else if (val_lower == "round") style.border_image_repeat = 2;
            else if (val_lower == "space") style.border_image_repeat = 3;
        } else if (d.property == "min-width") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.min_width = *l;
        } else if (d.property == "max-width") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.max_width = *l;
        } else if (d.property == "min-inline-size") {
            // CSS logical property: maps to min-width (horizontal-tb LTR)
            auto l = clever::css::parse_length(d.value);
            if (l) style.min_width = *l;
        } else if (d.property == "max-inline-size") {
            // CSS logical property: maps to max-width (horizontal-tb LTR)
            if (val_lower == "none") style.max_width = clever::css::Length::px(-1.0f);
            else { auto l = clever::css::parse_length(d.value); if (l) style.max_width = *l; }
        } else if (d.property == "min-height") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.min_height = *l;
        } else if (d.property == "max-height") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.max_height = *l;
        } else if (d.property == "min-block-size") {
            // CSS logical property: maps to min-height (horizontal-tb)
            auto l = clever::css::parse_length(d.value);
            if (l) style.min_height = *l;
        } else if (d.property == "max-block-size") {
            // CSS logical property: maps to max-height (horizontal-tb)
            if (val_lower == "none") style.max_height = clever::css::Length::px(-1.0f);
            else { auto l = clever::css::parse_length(d.value); if (l) style.max_height = *l; }
        } else if (d.property == "inline-size") {
            // CSS logical property: maps to width (horizontal-tb)
            auto l = clever::css::parse_length(d.value);
            if (l) style.width = *l;
        } else if (d.property == "block-size") {
            // CSS logical property: maps to height (horizontal-tb)
            auto l = clever::css::parse_length(d.value);
            if (l) style.height = *l;
        } else if (d.property == "aspect-ratio") {
            // Parse: "auto" | <number> | <number>/<number>
            if (val_lower == "auto") {
                style.aspect_ratio = 0; // 0 = auto
            } else {
                auto slash = val_lower.find('/');
                if (slash != std::string::npos) {
                    try {
                        float w = std::stof(d.value.substr(0, slash));
                        float h = std::stof(d.value.substr(slash + 1));
                        if (h > 0) style.aspect_ratio = w / h;
                    } catch (...) {}
                } else {
                    try { style.aspect_ratio = std::stof(d.value); } catch (...) {}
                }
            }
        } else if (d.property == "transform") {
            if (val_lower == "none") {
                style.transforms.clear();
            } else {
                // Parse transform functions: translate(x, y), rotate(deg), scale(x[, y])
                style.transforms.clear();
                std::string v = d.value;
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
                    int paren_depth = 1;
                    while (pos < v.size() && paren_depth > 0) {
                        if (v[pos] == '(') paren_depth++;
                        else if (v[pos] == ')') paren_depth--;
                        if (paren_depth > 0) pos++;
                    }
                    if (pos >= v.size() && paren_depth > 0) break;
                    std::string args = trim(v.substr(arg_start, pos - arg_start));
                    pos++; // skip ')'

                    // Parse based on function name
                    if (fn_name == "translate") {
                        // translate(x, y) or translate(x)
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Translate;
                        auto comma = args.find(',');
                        if (comma != std::string::npos) {
                            auto lx = clever::css::parse_length(trim(args.substr(0, comma)));
                            auto ly = clever::css::parse_length(trim(args.substr(comma + 1)));
                            if (lx) t.x = lx->to_px();
                            if (ly) t.y = ly->to_px();
                        } else {
                            auto lx = clever::css::parse_length(trim(args));
                            if (lx) t.x = lx->to_px();
                            t.y = 0;
                        }
                        style.transforms.push_back(t);
                    } else if (fn_name == "translatex") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Translate;
                        auto lx = clever::css::parse_length(trim(args));
                        if (lx) t.x = lx->to_px();
                        t.y = 0;
                        style.transforms.push_back(t);
                    } else if (fn_name == "translatey") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Translate;
                        t.x = 0;
                        auto ly = clever::css::parse_length(trim(args));
                        if (ly) t.y = ly->to_px();
                        style.transforms.push_back(t);
                    } else if (fn_name == "rotate") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Rotate;
                        // Parse angle: "45deg", "0.5rad", "100grad", "0.25turn"
                        std::string arg_lower = to_lower(trim(args));
                        if (arg_lower.find("deg") != std::string::npos) {
                            try { t.angle = std::stof(arg_lower); } catch (...) {}
                        } else if (arg_lower.find("rad") != std::string::npos) {
                            try {
                                float rad = std::stof(arg_lower);
                                t.angle = rad * 180.0f / 3.14159265f;
                            } catch (...) {}
                        } else if (arg_lower.find("turn") != std::string::npos) {
                            try {
                                float turns = std::stof(arg_lower);
                                t.angle = turns * 360.0f;
                            } catch (...) {}
                        } else {
                            // Default: assume degrees
                            try { t.angle = std::stof(arg_lower); } catch (...) {}
                        }
                        style.transforms.push_back(t);
                    } else if (fn_name == "scale") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Scale;
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
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Scale;
                        try { t.x = std::stof(trim(args)); } catch (...) {}
                        t.y = 1;
                        style.transforms.push_back(t);
                    } else if (fn_name == "scaley") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Scale;
                        t.x = 1;
                        try { t.y = std::stof(trim(args)); } catch (...) {}
                        style.transforms.push_back(t);
                    } else if (fn_name == "skew") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Skew;
                        auto parse_angle = [](const std::string& s) -> float {
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
                            t.x = parse_angle(args.substr(0, comma));
                            t.y = parse_angle(args.substr(comma + 1));
                        } else {
                            t.x = parse_angle(args);
                            t.y = 0;
                        }
                        style.transforms.push_back(t);
                    } else if (fn_name == "skewx") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Skew;
                        std::string sl = to_lower(trim(args));
                        try { t.x = std::stof(sl); } catch (...) {}
                        t.y = 0;
                        style.transforms.push_back(t);
                    } else if (fn_name == "skewy") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Skew;
                        t.x = 0;
                        std::string sl = to_lower(trim(args));
                        try { t.y = std::stof(sl); } catch (...) {}
                        style.transforms.push_back(t);
                    } else if (fn_name == "matrix") {
                        // matrix(a, b, c, d, e, f)
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Matrix;
                        // Split on commas or spaces
                        std::vector<float> vals;
                        std::string s = args;
                        size_t p = 0;
                        while (p < s.size() && vals.size() < 6) {
                            while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                            if (p >= s.size()) break;
                            size_t start = p;
                            while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                            try { vals.push_back(std::stof(s.substr(start, p - start))); } catch (...) { vals.push_back(0); }
                        }
                        for (size_t i = 0; i < 6 && i < vals.size(); i++) t.m[i] = vals[i];
                        style.transforms.push_back(t);
                    } else if (fn_name == "translate3d") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Translate;
                        std::vector<std::string> parts3d;
                        {
                            std::string s = args;
                            size_t p = 0;
                            while (p < s.size() && parts3d.size() < 3) {
                                while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                                if (p >= s.size()) break;
                                size_t sp = p;
                                while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                                parts3d.push_back(s.substr(sp, p - sp));
                            }
                        }
                        if (parts3d.size() >= 1) { auto lx = clever::css::parse_length(trim(parts3d[0])); if (lx) t.x = lx->to_px(); }
                        if (parts3d.size() >= 2) { auto ly = clever::css::parse_length(trim(parts3d[1])); if (ly) t.y = ly->to_px(); }
                        style.transforms.push_back(t);
                    } else if (fn_name == "translatez") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Translate;
                        t.x = 0;
                        t.y = 0;
                        style.transforms.push_back(t);
                    } else if (fn_name == "scale3d") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Scale;
                        std::vector<float> vals3d;
                        {
                            std::string s = args;
                            size_t p = 0;
                            while (p < s.size() && vals3d.size() < 3) {
                                while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                                if (p >= s.size()) break;
                                size_t sp = p;
                                while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                                try { vals3d.push_back(std::stof(s.substr(sp, p - sp))); } catch (...) { vals3d.push_back(1); }
                            }
                        }
                        t.x = vals3d.size() >= 1 ? vals3d[0] : 1;
                        t.y = vals3d.size() >= 2 ? vals3d[1] : 1;
                        style.transforms.push_back(t);
                    } else if (fn_name == "scalez") {
                        // scaleZ — no-op in 2D
                    } else if (fn_name == "rotate3d") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Rotate;
                        std::vector<std::string> rparts;
                        {
                            std::string s = args;
                            size_t p = 0;
                            while (p < s.size() && rparts.size() < 4) {
                                while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                                if (p >= s.size()) break;
                                size_t sp = p;
                                while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                                rparts.push_back(s.substr(sp, p - sp));
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
                        // rotateX/rotateY — no-op in 2D
                    } else if (fn_name == "rotatez") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Rotate;
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
                        // perspective(d) — no-op in 2D
                    } else if (fn_name == "matrix3d") {
                        clever::css::Transform t;
                        t.type = clever::css::TransformType::Matrix;
                        std::vector<float> vals16;
                        {
                            std::string s = args;
                            size_t p = 0;
                            while (p < s.size() && vals16.size() < 16) {
                                while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\t')) p++;
                                if (p >= s.size()) break;
                                size_t sp = p;
                                while (p < s.size() && s[p] != ' ' && s[p] != ',' && s[p] != '\t') p++;
                                try { vals16.push_back(std::stof(s.substr(sp, p - sp))); } catch (...) { vals16.push_back(0); }
                            }
                        }
                        if (vals16.size() >= 16) {
                            t.m[0] = vals16[0];
                            t.m[1] = vals16[1];
                            t.m[2] = vals16[4];
                            t.m[3] = vals16[5];
                            t.m[4] = vals16[12];
                            t.m[5] = vals16[13];
                        }
                        style.transforms.push_back(t);
                    }
                }
            }
        } else if (d.property == "background-size") {
            if (val_lower == "cover") {
                style.background_size = 1;
            } else if (val_lower == "contain") {
                style.background_size = 2;
            } else if (val_lower == "auto") {
                style.background_size = 0;
            } else {
                // Parse explicit sizes: "Wpx Hpx" or "Wpx"
                style.background_size = 3;
                auto space = val_lower.find(' ');
                if (space != std::string::npos) {
                    auto lw = clever::css::parse_length(trim(d.value.substr(0, space)));
                    auto lh = clever::css::parse_length(trim(d.value.substr(space + 1)));
                    if (lw) style.bg_size_width = lw->to_px();
                    if (lh) style.bg_size_height = lh->to_px();
                } else {
                    auto lw = clever::css::parse_length(trim(d.value));
                    if (lw) {
                        style.bg_size_width = lw->to_px();
                        style.bg_size_height = 0; // auto for height
                    }
                }
            }
        } else if (d.property == "background-repeat") {
            if (val_lower == "repeat") style.background_repeat = 0;
            else if (val_lower == "repeat-x") style.background_repeat = 1;
            else if (val_lower == "repeat-y") style.background_repeat = 2;
            else if (val_lower == "no-repeat") style.background_repeat = 3;
        } else if (d.property == "background-position") {
            // Parse: "left|center|right top|center|bottom" or "Npx Npx"
            auto space = val_lower.find(' ');
            std::string x_part = (space != std::string::npos) ? val_lower.substr(0, space) : val_lower;
            std::string y_part = (space != std::string::npos) ? val_lower.substr(space + 1) : "center";
            x_part = trim(x_part);
            y_part = trim(y_part);

            if (x_part == "left") style.background_position_x = 0;
            else if (x_part == "center") style.background_position_x = 1;
            else if (x_part == "right") style.background_position_x = 2;
            else {
                // Pixel value — store as-is (re-use field, distinguish in layout wiring)
                auto lx = clever::css::parse_length(x_part);
                if (lx) style.background_position_x = static_cast<int>(lx->to_px());
            }

            if (y_part == "top") style.background_position_y = 0;
            else if (y_part == "center") style.background_position_y = 1;
            else if (y_part == "bottom") style.background_position_y = 2;
            else {
                auto ly = clever::css::parse_length(y_part);
                if (ly) style.background_position_y = static_cast<int>(ly->to_px());
            }
        } else if (d.property == "background-position-x") {
            if (val_lower == "left") style.background_position_x = 0;
            else if (val_lower == "center") style.background_position_x = 1;
            else if (val_lower == "right") style.background_position_x = 2;
            else { auto lx = clever::css::parse_length(val_lower); if (lx) style.background_position_x = static_cast<int>(lx->to_px()); }
        } else if (d.property == "background-position-y") {
            if (val_lower == "top") style.background_position_y = 0;
            else if (val_lower == "center") style.background_position_y = 1;
            else if (val_lower == "bottom") style.background_position_y = 2;
            else { auto ly = clever::css::parse_length(val_lower); if (ly) style.background_position_y = static_cast<int>(ly->to_px()); }
        } else if (d.property == "border-collapse") {
            if (val_lower == "collapse") style.border_collapse = true;
            else style.border_collapse = false;
        } else if (d.property == "border-spacing") {
            // border-spacing can have one or two values: "10px" or "10px 5px"
            std::istringstream bss(val_lower);
            std::string first_tok, second_tok;
            bss >> first_tok >> second_tok;
            auto h_len = clever::css::parse_length(first_tok);
            if (h_len) {
                style.border_spacing = h_len->to_px();
                if (!second_tok.empty()) {
                    auto v_len = clever::css::parse_length(second_tok);
                    if (v_len) {
                        style.border_spacing_v = v_len->to_px();
                    } else {
                        style.border_spacing_v = 0;
                    }
                } else {
                    style.border_spacing_v = 0;
                }
            }
        } else if (d.property == "table-layout") {
            if (val_lower == "fixed") style.table_layout = 1;
            else style.table_layout = 0; // auto
        } else if (d.property == "caption-side") {
            if (val_lower == "bottom") style.caption_side = 1;
            else style.caption_side = 0; // top
        } else if (d.property == "empty-cells") {
            if (val_lower == "hide") style.empty_cells = 1;
            else style.empty_cells = 0; // show
        } else if (d.property == "quotes") {
            if (val_lower == "none") style.quotes = "none";
            else if (val_lower == "auto") style.quotes = "";
            else style.quotes = d.value;
        } else if (d.property == "list-style-position") {
            if (val_lower == "inside") style.list_style_position = clever::css::ListStylePosition::Inside;
            else style.list_style_position = clever::css::ListStylePosition::Outside;
        } else if (d.property == "list-style-type") {
            if (val_lower == "disc") style.list_style_type = clever::css::ListStyleType::Disc;
            else if (val_lower == "circle") style.list_style_type = clever::css::ListStyleType::Circle;
            else if (val_lower == "square") style.list_style_type = clever::css::ListStyleType::Square;
            else if (val_lower == "decimal") style.list_style_type = clever::css::ListStyleType::Decimal;
            else if (val_lower == "decimal-leading-zero") style.list_style_type = clever::css::ListStyleType::DecimalLeadingZero;
            else if (val_lower == "lower-roman") style.list_style_type = clever::css::ListStyleType::LowerRoman;
            else if (val_lower == "upper-roman") style.list_style_type = clever::css::ListStyleType::UpperRoman;
            else if (val_lower == "lower-alpha") style.list_style_type = clever::css::ListStyleType::LowerAlpha;
            else if (val_lower == "upper-alpha") style.list_style_type = clever::css::ListStyleType::UpperAlpha;
            else if (val_lower == "none") style.list_style_type = clever::css::ListStyleType::None;
            else if (val_lower == "lower-greek") style.list_style_type = clever::css::ListStyleType::LowerGreek;
            else if (val_lower == "lower-latin") style.list_style_type = clever::css::ListStyleType::LowerLatin;
            else if (val_lower == "upper-latin") style.list_style_type = clever::css::ListStyleType::UpperLatin;
        } else if (d.property == "list-style-image") {
            if (val_lower == "none") {
                style.list_style_image.clear();
            } else {
                // Extract URL from url(...)
                auto pos = val_lower.find("url(");
                if (pos != std::string::npos) {
                    auto start = d.value.find('(', pos) + 1;
                    auto end = d.value.find(')', start);
                    if (end != std::string::npos) {
                        std::string url = d.value.substr(start, end - start);
                        // Strip quotes
                        if (url.size() >= 2 && (url.front() == '"' || url.front() == '\'')) {
                            url = url.substr(1, url.size() - 2);
                        }
                        style.list_style_image = url;
                    }
                }
            }
        } else if (d.property == "list-style") {
            // list-style shorthand: [type] [position] [image]
            auto parts = split_whitespace_paren(val_lower);
            for (auto& tok : parts) {
                auto tl = to_lower(tok);
                // Position keywords
                if (tl == "inside") { style.list_style_position = clever::css::ListStylePosition::Inside; continue; }
                if (tl == "outside") { style.list_style_position = clever::css::ListStylePosition::Outside; continue; }
                // Image
                if (tl.find("url(") != std::string::npos) {
                    auto ps = tok.find('(') + 1;
                    auto pe = tok.find(')', ps);
                    if (pe != std::string::npos) {
                        std::string url = tok.substr(ps, pe - ps);
                        if (url.size() >= 2 && (url.front() == '"' || url.front() == '\''))
                            url = url.substr(1, url.size() - 2);
                        style.list_style_image = url;
                    }
                    continue;
                }
                // Type keywords
                if (tl == "disc") style.list_style_type = clever::css::ListStyleType::Disc;
                else if (tl == "circle") style.list_style_type = clever::css::ListStyleType::Circle;
                else if (tl == "square") style.list_style_type = clever::css::ListStyleType::Square;
                else if (tl == "decimal") style.list_style_type = clever::css::ListStyleType::Decimal;
                else if (tl == "decimal-leading-zero") style.list_style_type = clever::css::ListStyleType::DecimalLeadingZero;
                else if (tl == "lower-roman") style.list_style_type = clever::css::ListStyleType::LowerRoman;
                else if (tl == "upper-roman") style.list_style_type = clever::css::ListStyleType::UpperRoman;
                else if (tl == "lower-alpha") style.list_style_type = clever::css::ListStyleType::LowerAlpha;
                else if (tl == "upper-alpha") style.list_style_type = clever::css::ListStyleType::UpperAlpha;
                else if (tl == "none") style.list_style_type = clever::css::ListStyleType::None;
                else if (tl == "lower-greek") style.list_style_type = clever::css::ListStyleType::LowerGreek;
                else if (tl == "lower-latin") style.list_style_type = clever::css::ListStyleType::LowerLatin;
                else if (tl == "upper-latin") style.list_style_type = clever::css::ListStyleType::UpperLatin;
            }
        } else if (d.property == "pointer-events") {
            if (val_lower == "none") style.pointer_events = clever::css::PointerEvents::None;
            else if (val_lower == "visiblepainted") style.pointer_events = clever::css::PointerEvents::VisiblePainted;
            else if (val_lower == "visiblefill") style.pointer_events = clever::css::PointerEvents::VisibleFill;
            else if (val_lower == "visiblestroke") style.pointer_events = clever::css::PointerEvents::VisibleStroke;
            else if (val_lower == "visible") style.pointer_events = clever::css::PointerEvents::Visible;
            else if (val_lower == "painted") style.pointer_events = clever::css::PointerEvents::Painted;
            else if (val_lower == "fill") style.pointer_events = clever::css::PointerEvents::Fill;
            else if (val_lower == "stroke") style.pointer_events = clever::css::PointerEvents::Stroke;
            else if (val_lower == "all") style.pointer_events = clever::css::PointerEvents::All;
            else style.pointer_events = clever::css::PointerEvents::Auto;
        } else if (d.property == "user-select" || d.property == "-webkit-user-select") {
            if (val_lower == "none") style.user_select = clever::css::UserSelect::None;
            else if (val_lower == "text") style.user_select = clever::css::UserSelect::Text;
            else if (val_lower == "all") style.user_select = clever::css::UserSelect::All;
            else style.user_select = clever::css::UserSelect::Auto;
        } else if (d.property == "tab-size" || d.property == "-moz-tab-size") {
            try { style.tab_size = std::stoi(d.value); } catch (...) {}
        } else if (d.property == "filter") {
            if (val_lower == "none") {
                style.filters.clear();
            } else {
                style.filters.clear();
                std::string v = d.value;
                size_t pos = 0;
                while (pos < v.size()) {
                    while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t')) pos++;
                    if (pos >= v.size()) break;
                    size_t fn_start = pos;
                    while (pos < v.size() && v[pos] != '(') pos++;
                    if (pos >= v.size()) break;
                    std::string fn = to_lower(trim(v.substr(fn_start, pos - fn_start)));
                    pos++;
                    size_t arg_start = pos;
                    while (pos < v.size() && v[pos] != ')') pos++;
                    if (pos >= v.size()) break;
                    std::string arg = trim(v.substr(arg_start, pos - arg_start));
                    pos++;

                    int type = 0;
                    float fval = 0;
                    if (fn == "grayscale") { type = 1; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "sepia") { type = 2; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "brightness") { type = 3; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "contrast") { type = 4; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "invert") { type = 5; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "saturate") { type = 6; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "opacity") { type = 7; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "hue-rotate") {
                        type = 8;
                        try {
                            auto arg_lower = to_lower(arg);
                            if (arg_lower.size() >= 4 && arg_lower.substr(arg_lower.size() - 4) == "turn") {
                                fval = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 4))) * 360.0f;
                            } else if (arg_lower.size() >= 3 && arg_lower.substr(arg_lower.size() - 3) == "rad") {
                                fval = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 3))) * (180.0f / 3.14159265f);
                            } else if (arg_lower.size() >= 3 && arg_lower.substr(arg_lower.size() - 3) == "deg") {
                                fval = std::stof(trim(arg_lower.substr(0, arg_lower.size() - 3)));
                            } else {
                                fval = std::stof(trim(arg_lower));
                            }
                        } catch (...) { fval = 0; }
                    }
                    else if (fn == "blur") {
                        type = 9;
                        auto l = clever::css::parse_length(arg);
                        if (l) fval = l->to_px(0);
                    }
                    else if (fn == "drop-shadow") {
                        type = 10;
                        auto ds_parts = split_whitespace(arg);
                        float ds_ox = 0, ds_oy = 0, ds_blur = 0;
                        uint32_t ds_color = 0xFF000000;
                        int num_idx = 0;
                        for (auto& p : ds_parts) {
                            auto l = clever::css::parse_length(p);
                            if (l && num_idx < 3) {
                                float pxv = l->to_px();
                                if (num_idx == 0) ds_ox = pxv;
                                else if (num_idx == 1) ds_oy = pxv;
                                else if (num_idx == 2) ds_blur = pxv;
                                num_idx++;
                            } else {
                                auto c = clever::css::parse_color(p);
                                if (c) {
                                    ds_color = (static_cast<uint32_t>(c->a) << 24) |
                                               (static_cast<uint32_t>(c->r) << 16) |
                                               (static_cast<uint32_t>(c->g) << 8) |
                                               static_cast<uint32_t>(c->b);
                                }
                            }
                        }
                        fval = ds_blur;
                        style.drop_shadow_ox = ds_ox;
                        style.drop_shadow_oy = ds_oy;
                        style.drop_shadow_color = ds_color;
                    }
                    if (type > 0) style.filters.push_back({type, fval});
                }
            }
        } else if (d.property == "backdrop-filter" || d.property == "-webkit-backdrop-filter") {
            if (val_lower == "none") {
                style.backdrop_filters.clear();
            } else {
                style.backdrop_filters.clear();
                std::string v = d.value;
                size_t pos = 0;
                while (pos < v.size()) {
                    while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t')) pos++;
                    if (pos >= v.size()) break;
                    size_t fn_start = pos;
                    while (pos < v.size() && v[pos] != '(') pos++;
                    if (pos >= v.size()) break;
                    std::string fn = to_lower(trim(v.substr(fn_start, pos - fn_start)));
                    pos++;
                    size_t arg_start = pos;
                    while (pos < v.size() && v[pos] != ')') pos++;
                    if (pos >= v.size()) break;
                    std::string arg = trim(v.substr(arg_start, pos - arg_start));
                    pos++;

                    int type = 0;
                    float fval = 0;
                    if (fn == "grayscale") { type = 1; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "sepia") { type = 2; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "brightness") { type = 3; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "contrast") { type = 4; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "invert") { type = 5; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "saturate") { type = 6; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "opacity") { type = 7; try { fval = std::stof(arg); } catch (...) { fval = 1; } }
                    else if (fn == "hue-rotate") { type = 8; try { fval = std::stof(arg); } catch (...) { fval = 0; } }
                    else if (fn == "blur") {
                        type = 9;
                        auto l = clever::css::parse_length(arg);
                        if (l) fval = l->to_px(0);
                    }
                    if (type > 0) style.backdrop_filters.push_back({type, fval});
                }
            }
        } else if (d.property == "resize") {
            if (val_lower == "both") style.resize = 1;
            else if (val_lower == "horizontal") style.resize = 2;
            else if (val_lower == "vertical") style.resize = 3;
            else style.resize = 0;
        } else if (d.property == "direction") {
            if (val_lower == "rtl") style.direction = clever::css::Direction::Rtl;
            else style.direction = clever::css::Direction::Ltr;
        } else if (d.property == "isolation") {
            if (val_lower == "isolate") style.isolation = 1;
            else style.isolation = 0;
        } else if (d.property == "mix-blend-mode") {
            if (val_lower == "multiply") style.mix_blend_mode = 1;
            else if (val_lower == "screen") style.mix_blend_mode = 2;
            else if (val_lower == "overlay") style.mix_blend_mode = 3;
            else if (val_lower == "darken") style.mix_blend_mode = 4;
            else if (val_lower == "lighten") style.mix_blend_mode = 5;
            else if (val_lower == "color-dodge") style.mix_blend_mode = 6;
            else if (val_lower == "color-burn") style.mix_blend_mode = 7;
            else if (val_lower == "hard-light") style.mix_blend_mode = 8;
            else if (val_lower == "soft-light") style.mix_blend_mode = 9;
            else if (val_lower == "difference") style.mix_blend_mode = 10;
            else if (val_lower == "exclusion") style.mix_blend_mode = 11;
            else style.mix_blend_mode = 0;
        } else if (d.property == "contain") {
            if (val_lower == "none") style.contain = 0;
            else if (val_lower == "strict") style.contain = 1;
            else if (val_lower == "content") style.contain = 2;
            else if (val_lower == "size") style.contain = 3;
            else if (val_lower == "layout") style.contain = 4;
            else if (val_lower == "style") style.contain = 5;
            else if (val_lower == "paint") style.contain = 6;
            else style.contain = 0;
        } else if (d.property == "clip-path") {
            if (val_lower == "none") {
                style.clip_path_type = 0;
                style.clip_path_values.clear();
            } else if (val_lower.find("circle(") == 0) {
                // clip-path: circle(50%) or circle(50% at 25% 25%)
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
                    float radius = 50.0f;
                    float at_x = -1.0f, at_y = -1.0f;
                    auto at_pos = inner.find(" at ");
                    if (at_pos != std::string::npos) {
                        std::string rad_str = trim(inner.substr(0, at_pos));
                        std::string at_str = trim(inner.substr(at_pos + 4));
                        if (!rad_str.empty()) {
                            try {
                                if (rad_str.back() == '%') radius = std::stof(rad_str.substr(0, rad_str.size() - 1));
                                else { auto l = clever::css::parse_length(rad_str); if (l) radius = l->to_px(); }
                            } catch (...) {}
                        }
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
                    } else if (!inner.empty()) {
                        try {
                            if (inner.back() == '%') radius = std::stof(inner.substr(0, inner.size() - 1));
                            else { auto l = clever::css::parse_length(inner); if (l) radius = l->to_px(); }
                        } catch (...) {}
                    }
                    style.clip_path_type = 1;
                    if (at_x >= 0) {
                        style.clip_path_values = {radius, at_x, at_y};
                    } else {
                        style.clip_path_values = {radius};
                    }
                }
            } else if (val_lower.find("ellipse(") == 0) {
                // clip-path: ellipse(50% 40%) or ellipse(50% 40% at 50% 50%)
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
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
            } else if (val_lower.find("inset(") == 0) {
                // clip-path: inset(10px), inset(10px 20px), inset(10px 20px 30px 40px)
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
                    auto parts = split_whitespace(inner);
                    float top = 0, right_v = 0, bottom_v = 0, left_v = 0;
                    auto parse_val = [](const std::string& s) -> float {
                        try {
                            if (s.back() == '%') return std::stof(s.substr(0, s.size() - 1));
                            // Strip "px" suffix if present
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
            }
        } else if (d.property == "shape-outside") {
            // Also store raw string form
            if (val_lower == "none") style.shape_outside_str = "";
            else style.shape_outside_str = d.value;
            if (val_lower == "none") {
                style.shape_outside_type = 0;
                style.shape_outside_values.clear();
            } else if (val_lower == "margin-box") {
                style.shape_outside_type = 5;
                style.shape_outside_values.clear();
            } else if (val_lower == "border-box") {
                style.shape_outside_type = 6;
                style.shape_outside_values.clear();
            } else if (val_lower == "padding-box") {
                style.shape_outside_type = 7;
                style.shape_outside_values.clear();
            } else if (val_lower == "content-box") {
                style.shape_outside_type = 8;
                style.shape_outside_values.clear();
            } else if (val_lower.find("circle(") == 0) {
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
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
            } else if (val_lower.find("ellipse(") == 0) {
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
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
            } else if (val_lower.find("inset(") == 0) {
                auto lp = val_lower.find('(');
                auto rp = val_lower.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string inner = trim(val_lower.substr(lp + 1, rp - lp - 1));
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
            }
        } else if (d.property == "shape-margin") {
            auto l = clever::css::parse_length(val_lower);
            if (l) style.shape_margin = l->to_px();
        } else if (d.property == "shape-image-threshold") {
            try { style.shape_image_threshold = std::stof(val_lower); } catch (...) {}
        } else if (d.property == "line-clamp" || d.property == "-webkit-line-clamp") {
            if (val_lower == "none") style.line_clamp = -1;
            else { try { style.line_clamp = std::stoi(d.value); } catch (...) {} }
        } else if (d.property == "caret-color") {
            if (val_lower != "auto") {
                auto c = clever::css::parse_color(val_lower);
                if (c) style.caret_color = *c;
            }
        } else if (d.property == "accent-color") {
            if (val_lower != "auto") {
                auto c = clever::css::parse_color(val_lower);
                if (c) {
                    style.accent_color = (static_cast<uint32_t>(c->a) << 24) |
                                         (static_cast<uint32_t>(c->r) << 16) |
                                         (static_cast<uint32_t>(c->g) << 8) |
                                         static_cast<uint32_t>(c->b);
                }
            }
        } else if (d.property == "color-interpolation") {
            if (val_lower == "auto") style.color_interpolation = 0;
            else if (val_lower == "srgb") style.color_interpolation = 1;
            else if (val_lower == "linearrgb") style.color_interpolation = 2;
        } else if (d.property == "scroll-behavior") {
            if (val_lower == "auto") style.scroll_behavior = 0;
            else if (val_lower == "smooth") style.scroll_behavior = 1;
        } else if (d.property == "scroll-snap-type") {
            auto parts = split_whitespace(val_lower);
            if (!parts.empty()) {
                if (parts[0] == "none") {
                    style.scroll_snap_type_axis = 0;
                    style.scroll_snap_type_strictness = 0;
                } else {
                    int axis = 0;
                    if (parts[0] == "x") axis = 1;
                    else if (parts[0] == "y") axis = 2;
                    else if (parts[0] == "both") axis = 3;

                    if (axis != 0) {
                        int strictness = 1; // default mandatory when axis is provided
                        if (parts.size() >= 2) {
                            if (parts[1] == "mandatory") strictness = 1;
                            else if (parts[1] == "proximity") strictness = 2;
                            else strictness = -1;
                        }
                        if (strictness >= 0) {
                            style.scroll_snap_type_axis = axis;
                            style.scroll_snap_type_strictness = strictness;
                        }
                    }
                }
            }
        } else if (d.property == "scroll-snap-align") {
            auto parts = split_whitespace(val_lower);
            if (!parts.empty()) {
                if (parts[0] == "none") {
                    style.scroll_snap_align_x = 0;
                    style.scroll_snap_align_y = 0;
                } else {
                    auto parse_snap_align = [](const std::string& token) -> int {
                        if (token == "none") return 0;
                        if (token == "start") return 1;
                        if (token == "center") return 2;
                        if (token == "end") return 3;
                        return -1;
                    };
                    int x = parse_snap_align(parts[0]);
                    if (x >= 0) {
                        int y = x;
                        if (parts.size() >= 2) y = parse_snap_align(parts[1]);
                        if (y >= 0) {
                            style.scroll_snap_align_x = x;
                            style.scroll_snap_align_y = y;
                        }
                    }
                }
            }
        } else if (d.property == "placeholder-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) style.placeholder_color = *c;
        } else if (d.property == "writing-mode") {
            if (val_lower == "horizontal-tb") style.writing_mode = 0;
            else if (val_lower == "vertical-rl") style.writing_mode = 1;
            else if (val_lower == "vertical-lr") style.writing_mode = 2;
        } else if (d.property == "counter-increment") {
            style.counter_increment = d.value;
        } else if (d.property == "counter-reset") {
            style.counter_reset = d.value;
        } else if (d.property == "counter-set") {
            style.counter_set = d.value;
        } else if (d.property == "column-count") {
            if (val_lower == "auto") style.column_count = -1;
            else { try { style.column_count = std::stoi(d.value); } catch (...) {} }
        } else if (d.property == "column-fill") {
            if (val_lower == "balance") style.column_fill = 0;
            else if (val_lower == "auto") style.column_fill = 1;
            else if (val_lower == "balance-all") style.column_fill = 2;
        } else if (d.property == "column-width") {
            if (val_lower == "auto") style.column_width = clever::css::Length::auto_val();
            else {
                auto l = clever::css::parse_length(d.value);
                if (l) style.column_width = *l;
            }
        } else if (d.property == "column-gap") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.column_gap_val = *l;
        } else if (d.property == "column-rule-width") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.column_rule_width = l->to_px(0);
        } else if (d.property == "column-rule-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) style.column_rule_color = *c;
        } else if (d.property == "column-rule-style") {
            if (val_lower == "none") style.column_rule_style = 0;
            else if (val_lower == "solid") style.column_rule_style = 1;
            else if (val_lower == "dashed") style.column_rule_style = 2;
            else if (val_lower == "dotted") style.column_rule_style = 3;
        } else if (d.property == "columns") {
            // Shorthand: columns: <count> <width> or columns: <width> <count>
            auto parts = split_whitespace(d.value);
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
                    auto l = clever::css::parse_length(part);
                    if (l) style.column_width = *l;
                }
            }
        } else if (d.property == "column-rule") {
            // Shorthand: column-rule: <width> <style> <color>
            auto parts = split_whitespace(d.value);
            for (auto& part : parts) {
                std::string pl = to_lower(part);
                if (pl == "none" || pl == "solid" || pl == "dashed" || pl == "dotted") {
                    if (pl == "none") style.column_rule_style = 0;
                    else if (pl == "solid") style.column_rule_style = 1;
                    else if (pl == "dashed") style.column_rule_style = 2;
                    else if (pl == "dotted") style.column_rule_style = 3;
                } else {
                    auto c = clever::css::parse_color(pl);
                    if (c) {
                        style.column_rule_color = *c;
                    } else {
                        auto l = clever::css::parse_length(part);
                        if (l) style.column_rule_width = l->to_px(0);
                    }
                }
            }
        } else if (d.property == "appearance" || d.property == "-webkit-appearance") {
            if (val_lower == "auto") style.appearance = 0;
            else if (val_lower == "none") style.appearance = 1;
            else if (val_lower == "menulist-button") style.appearance = 2;
            else if (val_lower == "textfield") style.appearance = 3;
            else if (val_lower == "button") style.appearance = 4;
            else style.appearance = 0;
        } else if (d.property == "touch-action") {
            if (val_lower == "auto") style.touch_action = 0;
            else if (val_lower == "none") style.touch_action = 1;
            else if (val_lower == "manipulation") style.touch_action = 2;
            else if (val_lower == "pan-x") style.touch_action = 3;
            else if (val_lower == "pan-y") style.touch_action = 4;
            else style.touch_action = 0;
        } else if (d.property == "will-change") {
            if (val_lower == "auto") style.will_change.clear();
            else style.will_change = d.value;
        } else if (d.property == "color-scheme") {
            if (val_lower == "normal") style.color_scheme = 0;
            else if (val_lower == "light") style.color_scheme = 1;
            else if (val_lower == "dark") style.color_scheme = 2;
            else if (val_lower == "light dark" || val_lower == "dark light") style.color_scheme = 3;
            else style.color_scheme = 0;
        } else if (d.property == "container-type") {
            if (val_lower == "normal") style.container_type = 0;
            else if (val_lower == "size") style.container_type = 1;
            else if (val_lower == "inline-size") style.container_type = 2;
            else if (val_lower == "block-size") style.container_type = 3;
            else style.container_type = 0;
        } else if (d.property == "container-name") {
            style.container_name = d.value;
        } else if (d.property == "container") {
            // Shorthand: "name / type" e.g. "sidebar / inline-size"
            auto slash_pos = d.value.find('/');
            if (slash_pos != std::string::npos) {
                std::string name_part = d.value.substr(0, slash_pos);
                std::string type_part = d.value.substr(slash_pos + 1);
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
                if (val_lower == "normal") style.container_type = 0;
                else if (val_lower == "size") style.container_type = 1;
                else if (val_lower == "inline-size") style.container_type = 2;
                else if (val_lower == "block-size") style.container_type = 3;
                else style.container_type = 0;
            }
        } else if (d.property == "hyphens") {
            if (val_lower == "none") style.hyphens = 0;
            else if (val_lower == "manual") style.hyphens = 1;
            else if (val_lower == "auto") style.hyphens = 2;
        } else if (d.property == "text-justify") {
            if (val_lower == "auto") style.text_justify = 0;
            else if (val_lower == "inter-word") style.text_justify = 1;
            else if (val_lower == "inter-character") style.text_justify = 2;
            else if (val_lower == "none") style.text_justify = 3;
        } else if (d.property == "text-underline-offset") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.text_underline_offset = l->to_px(0);
        } else if (d.property == "font-variant") {
            if (val_lower == "small-caps") style.font_variant = 1;
            else style.font_variant = 0;
        } else if (d.property == "font-variant-caps") {
            if (val_lower == "small-caps") style.font_variant_caps = 1;
            else if (val_lower == "all-small-caps") style.font_variant_caps = 2;
            else if (val_lower == "petite-caps") style.font_variant_caps = 3;
            else if (val_lower == "all-petite-caps") style.font_variant_caps = 4;
            else if (val_lower == "unicase") style.font_variant_caps = 5;
            else if (val_lower == "titling-caps") style.font_variant_caps = 6;
            else style.font_variant_caps = 0; // normal
        } else if (d.property == "font-variant-numeric") {
            if (val_lower == "ordinal") style.font_variant_numeric = 1;
            else if (val_lower == "slashed-zero") style.font_variant_numeric = 2;
            else if (val_lower == "lining-nums") style.font_variant_numeric = 3;
            else if (val_lower == "oldstyle-nums") style.font_variant_numeric = 4;
            else if (val_lower == "proportional-nums") style.font_variant_numeric = 5;
            else if (val_lower == "tabular-nums") style.font_variant_numeric = 6;
            else style.font_variant_numeric = 0; // normal
        } else if (d.property == "font-synthesis") {
            if (val_lower == "none") { style.font_synthesis = 0; }
            else {
                int mask = 0;
                auto parts = split_whitespace(val_lower);
                for (auto& p : parts) {
                    if (p == "weight") mask |= 1;
                    else if (p == "style") mask |= 2;
                    else if (p == "small-caps") mask |= 4;
                }
                style.font_synthesis = mask;
            }
        } else if (d.property == "font-variant-alternates") {
            if (val_lower == "historical-forms") style.font_variant_alternates = 1;
            else style.font_variant_alternates = 0; // normal
        } else if (d.property == "font-feature-settings") {
            style.font_feature_settings = clever::css::parse_font_feature_settings(d.value);
        } else if (d.property == "font-variation-settings") {
            style.font_variation_settings = d.value;
        } else if (d.property == "font-optical-sizing") {
            if (val_lower == "none") style.font_optical_sizing = 1;
            else style.font_optical_sizing = 0; // auto
        } else if (d.property == "print-color-adjust" || d.property == "-webkit-print-color-adjust") {
            if (val_lower == "economy") style.print_color_adjust = 0;
            else if (val_lower == "exact") style.print_color_adjust = 1;
        } else if (d.property == "image-orientation") {
            if (val_lower == "from-image") { style.image_orientation = 0; style.image_orientation_explicit = true; }
            else if (val_lower == "none") { style.image_orientation = 1; style.image_orientation_explicit = true; }
            else if (val_lower == "flip") { style.image_orientation = 2; style.image_orientation_explicit = true; }
        } else if (d.property == "font-kerning") {
            if (val_lower == "auto") style.font_kerning = 0;
            else if (val_lower == "normal") style.font_kerning = 1;
            else if (val_lower == "none") style.font_kerning = 2;
        } else if (d.property == "font-variant-ligatures") {
            if (val_lower == "normal") style.font_variant_ligatures = 0;
            else if (val_lower == "none") style.font_variant_ligatures = 1;
            else if (val_lower == "common-ligatures") style.font_variant_ligatures = 2;
            else if (val_lower == "no-common-ligatures") style.font_variant_ligatures = 3;
            else if (val_lower == "discretionary-ligatures") style.font_variant_ligatures = 4;
            else if (val_lower == "no-discretionary-ligatures") style.font_variant_ligatures = 5;
        } else if (d.property == "font-variant-east-asian") {
            if (val_lower == "normal") style.font_variant_east_asian = 0;
            else if (val_lower == "jis78") style.font_variant_east_asian = 1;
            else if (val_lower == "jis83") style.font_variant_east_asian = 2;
            else if (val_lower == "jis90") style.font_variant_east_asian = 3;
            else if (val_lower == "jis04") style.font_variant_east_asian = 4;
            else if (val_lower == "simplified") style.font_variant_east_asian = 5;
            else if (val_lower == "traditional") style.font_variant_east_asian = 6;
            else if (val_lower == "full-width") style.font_variant_east_asian = 7;
            else if (val_lower == "proportional-width") style.font_variant_east_asian = 8;
            else if (val_lower == "ruby") style.font_variant_east_asian = 9;
        } else if (d.property == "font-palette") {
            style.font_palette = d.value;
        } else if (d.property == "font-variant-position") {
            if (val_lower == "normal") style.font_variant_position = 0;
            else if (val_lower == "sub") style.font_variant_position = 1;
            else if (val_lower == "super") style.font_variant_position = 2;
        } else if (d.property == "font-language-override") {
            if (val_lower == "normal") style.font_language_override = "";
            else {
                std::string val = d.value;
                if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'')) {
                    val = val.substr(1, val.size() - 2);
                }
                style.font_language_override = val;
            }
        } else if (d.property == "font-size-adjust") {
            if (val_lower == "none") {
                style.font_size_adjust = 0;
            } else {
                float v = std::strtof(val_lower.c_str(), nullptr);
                if (v > 0) style.font_size_adjust = v;
                else style.font_size_adjust = 0;
            }
        } else if (d.property == "font-stretch") {
            if (val_lower == "ultra-condensed") style.font_stretch = 1;
            else if (val_lower == "extra-condensed") style.font_stretch = 2;
            else if (val_lower == "condensed") style.font_stretch = 3;
            else if (val_lower == "semi-condensed") style.font_stretch = 4;
            else if (val_lower == "normal") style.font_stretch = 5;
            else if (val_lower == "semi-expanded") style.font_stretch = 6;
            else if (val_lower == "expanded") style.font_stretch = 7;
            else if (val_lower == "extra-expanded") style.font_stretch = 8;
            else if (val_lower == "ultra-expanded") style.font_stretch = 9;
            else style.font_stretch = 5; // default normal
        } else if (d.property == "text-decoration-skip-ink") {
            if (val_lower == "auto") style.text_decoration_skip_ink = 0;
            else if (val_lower == "none") style.text_decoration_skip_ink = 1;
            else if (val_lower == "all") style.text_decoration_skip_ink = 2;
            else style.text_decoration_skip_ink = 0; // default auto
        } else if (d.property == "text-decoration-skip") {
            if (val_lower == "none") style.text_decoration_skip = 0;
            else if (val_lower == "objects") style.text_decoration_skip = 1;
            else if (val_lower == "spaces") style.text_decoration_skip = 2;
            else if (val_lower == "ink") style.text_decoration_skip = 3;
            else if (val_lower == "edges") style.text_decoration_skip = 4;
            else if (val_lower == "box-decoration") style.text_decoration_skip = 5;
        } else if (d.property == "transition-property") {
            style.transition_property = trim(d.value);
        } else if (d.property == "transition-duration") {
            float t = 0;
            if (val_lower.size() > 2 && val_lower.substr(val_lower.size() - 2) == "ms") {
                t = std::strtof(val_lower.c_str(), nullptr) / 1000.0f;
            } else if (val_lower.size() > 1 && val_lower.back() == 's') {
                t = std::strtof(val_lower.c_str(), nullptr);
            }
            style.transition_duration = t;
        } else if (d.property == "transition-timing-function") {
            // Parse cubic-bezier() and steps() in addition to keywords
            auto cb_pos = val_lower.find("cubic-bezier(");
            auto st_pos = val_lower.find("steps(");
            if (cb_pos != std::string::npos) {
                auto start = cb_pos + 13;
                auto end = val_lower.find(')', start);
                if (end != std::string::npos) {
                    std::string inner = val_lower.substr(start, end - start);
                    for (char& c : inner) { if (c == ',') c = ' '; }
                    std::istringstream iss(inner);
                    float x1, y1, x2, y2;
                    if (iss >> x1 >> y1 >> x2 >> y2) {
                        style.transition_timing = 5;
                        style.transition_bezier_x1 = x1; style.transition_bezier_y1 = y1;
                        style.transition_bezier_x2 = x2; style.transition_bezier_y2 = y2;
                    }
                }
            } else if (st_pos != std::string::npos) {
                auto start = st_pos + 6;
                auto end = val_lower.find(')', start);
                if (end != std::string::npos) {
                    std::string inner = val_lower.substr(start, end - start);
                    for (char& c : inner) { if (c == ',') c = ' '; }
                    std::istringstream iss(inner);
                    int n; std::string dir;
                    if (iss >> n) {
                        style.transition_steps_count = n > 0 ? n : 1;
                        style.transition_timing = 6; // default: steps-end
                        if (iss >> dir) {
                            if (dir == "start" || dir == "jump-start") style.transition_timing = 7;
                        }
                    }
                }
            } else if (val_lower == "ease") style.transition_timing = 0;
            else if (val_lower == "linear") style.transition_timing = 1;
            else if (val_lower == "ease-in") style.transition_timing = 2;
            else if (val_lower == "ease-out") style.transition_timing = 3;
            else if (val_lower == "ease-in-out") style.transition_timing = 4;
        } else if (d.property == "transition-delay") {
            float t = 0;
            if (val_lower.size() > 2 && val_lower.substr(val_lower.size() - 2) == "ms") {
                t = std::strtof(val_lower.c_str(), nullptr) / 1000.0f;
            } else if (val_lower.size() > 1 && val_lower.back() == 's') {
                t = std::strtof(val_lower.c_str(), nullptr);
            }
            style.transition_delay = t;
        } else if (d.property == "transition") {
            // Shorthand: "property duration timing-function delay"
            auto parts = split_whitespace(d.value);
            if (!parts.empty()) {
                // First token: property name (or "all", "none")
                style.transition_property = to_lower(parts[0]);
                // Second token: duration
                if (parts.size() > 1) {
                    std::string dur = to_lower(parts[1]);
                    float t = 0;
                    if (dur.size() > 2 && dur.substr(dur.size() - 2) == "ms") {
                        t = std::strtof(dur.c_str(), nullptr) / 1000.0f;
                    } else if (dur.size() > 1 && dur.back() == 's') {
                        t = std::strtof(dur.c_str(), nullptr);
                    }
                    style.transition_duration = t;
                }
                // Third token: timing function
                if (parts.size() > 2) {
                    std::string tf = to_lower(parts[2]);
                    // Reconstruct remaining for function-value parsing
                    std::string rest_str;
                    for (size_t pi = 2; pi < parts.size(); pi++) {
                        if (pi > 2) rest_str += ' ';
                        rest_str += to_lower(parts[pi]);
                    }
                    auto cb_p = rest_str.find("cubic-bezier(");
                    auto st_p = rest_str.find("steps(");
                    if (cb_p != std::string::npos) {
                        auto s2 = cb_p + 13;
                        auto e2 = rest_str.find(')', s2);
                        if (e2 != std::string::npos) {
                            std::string inner = rest_str.substr(s2, e2 - s2);
                            for (char& c : inner) { if (c == ',') c = ' '; }
                            std::istringstream iss(inner);
                            float x1, y1, x2, y2;
                            if (iss >> x1 >> y1 >> x2 >> y2) {
                                style.transition_timing = 5;
                                style.transition_bezier_x1 = x1; style.transition_bezier_y1 = y1;
                                style.transition_bezier_x2 = x2; style.transition_bezier_y2 = y2;
                            }
                        }
                    } else if (st_p != std::string::npos) {
                        auto s2 = st_p + 6;
                        auto e2 = rest_str.find(')', s2);
                        if (e2 != std::string::npos) {
                            std::string inner = rest_str.substr(s2, e2 - s2);
                            for (char& c : inner) { if (c == ',') c = ' '; }
                            std::istringstream iss(inner);
                            int n; std::string dir;
                            if (iss >> n) {
                                style.transition_steps_count = n > 0 ? n : 1;
                                style.transition_timing = 6;
                                if (iss >> dir) {
                                    if (dir == "start" || dir == "jump-start") style.transition_timing = 7;
                                }
                            }
                        }
                    } else if (tf == "ease") style.transition_timing = 0;
                    else if (tf == "linear") style.transition_timing = 1;
                    else if (tf == "ease-in") style.transition_timing = 2;
                    else if (tf == "ease-out") style.transition_timing = 3;
                    else if (tf == "ease-in-out") style.transition_timing = 4;
                }
                // Fourth token: delay
                if (parts.size() > 3) {
                    std::string del = to_lower(parts[3]);
                    float t = 0;
                    if (del.size() > 2 && del.substr(del.size() - 2) == "ms") {
                        t = std::strtof(del.c_str(), nullptr) / 1000.0f;
                    } else if (del.size() > 1 && del.back() == 's') {
                        t = std::strtof(del.c_str(), nullptr);
                    }
                    style.transition_delay = t;
                }
            }
        } else if (d.property == "animation-name") {
            style.animation_name = d.value;
        } else if (d.property == "animation-duration") {
            float t = 0;
            if (val_lower.size() > 2 && val_lower.substr(val_lower.size() - 2) == "ms") {
                t = std::strtof(val_lower.c_str(), nullptr) / 1000.0f;
            } else if (val_lower.size() > 1 && val_lower.back() == 's') {
                t = std::strtof(val_lower.c_str(), nullptr);
            }
            style.animation_duration = t;
        } else if (d.property == "animation-timing-function") {
            auto cb_pos = val_lower.find("cubic-bezier(");
            auto st_pos = val_lower.find("steps(");
            if (cb_pos != std::string::npos) {
                auto start = cb_pos + 13;
                auto end = val_lower.find(')', start);
                if (end != std::string::npos) {
                    std::string inner = val_lower.substr(start, end - start);
                    for (char& c : inner) { if (c == ',') c = ' '; }
                    std::istringstream iss(inner);
                    float x1, y1, x2, y2;
                    if (iss >> x1 >> y1 >> x2 >> y2) {
                        style.animation_timing = 5;
                        style.animation_bezier_x1 = x1; style.animation_bezier_y1 = y1;
                        style.animation_bezier_x2 = x2; style.animation_bezier_y2 = y2;
                    }
                }
            } else if (st_pos != std::string::npos) {
                auto start = st_pos + 6;
                auto end = val_lower.find(')', start);
                if (end != std::string::npos) {
                    std::string inner = val_lower.substr(start, end - start);
                    for (char& c : inner) { if (c == ',') c = ' '; }
                    std::istringstream iss(inner);
                    int n; std::string dir;
                    if (iss >> n) {
                        style.animation_steps_count = n > 0 ? n : 1;
                        style.animation_timing = 6;
                        if (iss >> dir) {
                            if (dir == "start" || dir == "jump-start") style.animation_timing = 7;
                        }
                    }
                }
            } else if (val_lower == "ease") style.animation_timing = 0;
            else if (val_lower == "linear") style.animation_timing = 1;
            else if (val_lower == "ease-in") style.animation_timing = 2;
            else if (val_lower == "ease-out") style.animation_timing = 3;
            else if (val_lower == "ease-in-out") style.animation_timing = 4;
        } else if (d.property == "animation-delay") {
            float t = 0;
            if (val_lower.size() > 2 && val_lower.substr(val_lower.size() - 2) == "ms") {
                t = std::strtof(val_lower.c_str(), nullptr) / 1000.0f;
            } else if (val_lower.size() > 1 && val_lower.back() == 's') {
                t = std::strtof(val_lower.c_str(), nullptr);
            }
            style.animation_delay = t;
        } else if (d.property == "animation-iteration-count") {
            if (val_lower == "infinite") style.animation_iteration_count = -1;
            else style.animation_iteration_count = std::strtof(val_lower.c_str(), nullptr);
        } else if (d.property == "animation-direction") {
            if (val_lower == "normal") style.animation_direction = 0;
            else if (val_lower == "reverse") style.animation_direction = 1;
            else if (val_lower == "alternate") style.animation_direction = 2;
            else if (val_lower == "alternate-reverse") style.animation_direction = 3;
        } else if (d.property == "animation-fill-mode") {
            if (val_lower == "none") style.animation_fill_mode = 0;
            else if (val_lower == "forwards") style.animation_fill_mode = 1;
            else if (val_lower == "backwards") style.animation_fill_mode = 2;
            else if (val_lower == "both") style.animation_fill_mode = 3;
        } else if (d.property == "animation") {
            // Shorthand: "name duration timing-function delay count direction fill-mode"
            auto parts = split_whitespace(d.value);
            if (!parts.empty()) {
                // First token: animation name
                style.animation_name = parts[0];
                // Second token: duration
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
                // Third token: timing function
                if (parts.size() > 2) {
                    std::string tf = to_lower(parts[2]);
                    std::string rest_str;
                    for (size_t pi = 2; pi < parts.size(); pi++) {
                        if (pi > 2) rest_str += ' ';
                        rest_str += to_lower(parts[pi]);
                    }
                    auto cb_p = rest_str.find("cubic-bezier(");
                    auto st_p = rest_str.find("steps(");
                    if (cb_p != std::string::npos) {
                        auto s2 = cb_p + 13;
                        auto e2 = rest_str.find(')', s2);
                        if (e2 != std::string::npos) {
                            std::string inner = rest_str.substr(s2, e2 - s2);
                            for (char& c : inner) { if (c == ',') c = ' '; }
                            std::istringstream iss(inner);
                            float x1, y1, x2, y2;
                            if (iss >> x1 >> y1 >> x2 >> y2) {
                                style.animation_timing = 5;
                                style.animation_bezier_x1 = x1; style.animation_bezier_y1 = y1;
                                style.animation_bezier_x2 = x2; style.animation_bezier_y2 = y2;
                            }
                        }
                    } else if (st_p != std::string::npos) {
                        auto s2 = st_p + 6;
                        auto e2 = rest_str.find(')', s2);
                        if (e2 != std::string::npos) {
                            std::string inner = rest_str.substr(s2, e2 - s2);
                            for (char& c : inner) { if (c == ',') c = ' '; }
                            std::istringstream iss(inner);
                            int n; std::string dir;
                            if (iss >> n) {
                                style.animation_steps_count = n > 0 ? n : 1;
                                style.animation_timing = 6;
                                if (iss >> dir) {
                                    if (dir == "start" || dir == "jump-start") style.animation_timing = 7;
                                }
                            }
                        }
                    } else if (tf == "ease") style.animation_timing = 0;
                    else if (tf == "linear") style.animation_timing = 1;
                    else if (tf == "ease-in") style.animation_timing = 2;
                    else if (tf == "ease-out") style.animation_timing = 3;
                    else if (tf == "ease-in-out") style.animation_timing = 4;
                }
                // Fourth token: delay
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
                // Fifth token: iteration count
                if (parts.size() > 4) {
                    std::string ic = to_lower(parts[4]);
                    if (ic == "infinite") style.animation_iteration_count = -1;
                    else style.animation_iteration_count = std::strtof(ic.c_str(), nullptr);
                }
                // Sixth token: direction
                if (parts.size() > 5) {
                    std::string dir = to_lower(parts[5]);
                    if (dir == "normal") style.animation_direction = 0;
                    else if (dir == "reverse") style.animation_direction = 1;
                    else if (dir == "alternate") style.animation_direction = 2;
                    else if (dir == "alternate-reverse") style.animation_direction = 3;
                }
                // Seventh token: fill mode
                if (parts.size() > 6) {
                    std::string fm = to_lower(parts[6]);
                    if (fm == "none") style.animation_fill_mode = 0;
                    else if (fm == "forwards") style.animation_fill_mode = 1;
                    else if (fm == "backwards") style.animation_fill_mode = 2;
                    else if (fm == "both") style.animation_fill_mode = 3;
                }
            }
        } else if (d.property == "grid-template-columns") {
            style.grid_template_columns = d.value;
        } else if (d.property == "grid-template-rows") {
            style.grid_template_rows = d.value;
        } else if (d.property == "grid-column") {
            style.grid_column = d.value;
        } else if (d.property == "grid-row") {
            style.grid_row = d.value;
        } else if (d.property == "grid-column-start") {
            style.grid_column_start = d.value;
            if (!style.grid_column_end.empty())
                style.grid_column = d.value + " / " + style.grid_column_end;
            else
                style.grid_column = d.value;
        } else if (d.property == "grid-column-end") {
            style.grid_column_end = d.value;
            if (!style.grid_column_start.empty())
                style.grid_column = style.grid_column_start + " / " + d.value;
            else
                style.grid_column = "auto / " + d.value;
        } else if (d.property == "grid-row-start") {
            style.grid_row_start = d.value;
            if (!style.grid_row_end.empty())
                style.grid_row = d.value + " / " + style.grid_row_end;
            else
                style.grid_row = d.value;
        } else if (d.property == "grid-row-end") {
            style.grid_row_end = d.value;
            if (!style.grid_row_start.empty())
                style.grid_row = style.grid_row_start + " / " + d.value;
            else
                style.grid_row = "auto / " + d.value;
        } else if (d.property == "grid-auto-rows") {
            style.grid_auto_rows = d.value;
        } else if (d.property == "grid-auto-columns") {
            style.grid_auto_columns = d.value;
        } else if (d.property == "grid-auto-flow") {
            if (val_lower == "row") style.grid_auto_flow = 0;
            else if (val_lower == "column") style.grid_auto_flow = 1;
            else if (val_lower == "row dense" || val_lower == "dense row" || val_lower == "dense") style.grid_auto_flow = 2;
            else if (val_lower == "column dense" || val_lower == "dense column") style.grid_auto_flow = 3;
        } else if (d.property == "grid-template-areas") {
            style.grid_template_areas = d.value;
        } else if (d.property == "grid-template" || d.property == "grid") {
            // grid-template: <rows> / <columns>
            auto slash_pos = d.value.find('/');
            if (slash_pos != std::string::npos) {
                auto rows = trim(d.value.substr(0, slash_pos));
                auto cols = trim(d.value.substr(slash_pos + 1));
                if (!rows.empty()) style.grid_template_rows = rows;
                if (!cols.empty()) style.grid_template_columns = cols;
            } else {
                // Single value: treat as rows
                style.grid_template_rows = d.value;
            }
        } else if (d.property == "grid-area") {
            style.grid_area = d.value;
        } else if (d.property == "justify-items") {
            if (val_lower == "start") style.justify_items = 0;
            else if (val_lower == "end") style.justify_items = 1;
            else if (val_lower == "center") style.justify_items = 2;
            else if (val_lower == "stretch") style.justify_items = 3;
        } else if (d.property == "align-content") {
            if (val_lower == "start" || val_lower == "flex-start" || val_lower == "normal") style.align_content = 0;
            else if (val_lower == "end" || val_lower == "flex-end") style.align_content = 1;
            else if (val_lower == "center") style.align_content = 2;
            else if (val_lower == "stretch") style.align_content = 3;
            else if (val_lower == "space-between") style.align_content = 4;
            else if (val_lower == "space-around") style.align_content = 5;
            else if (val_lower == "space-evenly") style.align_content = 6;
        } else if (d.property == "forced-color-adjust") {
            if (val_lower == "auto") style.forced_color_adjust = 0;
            else if (val_lower == "none") style.forced_color_adjust = 1;
            else if (val_lower == "preserve-parent-color") style.forced_color_adjust = 2;
        } else if (d.property == "math-style") {
            if (val_lower == "normal") style.math_style = 0;
            else if (val_lower == "compact") style.math_style = 1;
        } else if (d.property == "math-depth") {
            if (val_lower == "auto-add") style.math_depth = -1;
            else { try { style.math_depth = std::stoi(d.value); } catch (...) {} }
        } else if (d.property == "content-visibility") {
            if (val_lower == "visible") style.content_visibility = 0;
            else if (val_lower == "hidden") style.content_visibility = 1;
            else if (val_lower == "auto") style.content_visibility = 2;
        } else if (d.property == "overscroll-behavior") {
            std::istringstream iss_ob(d.value);
            std::string ob_first, ob_second;
            iss_ob >> ob_first >> ob_second;
            auto ob_to_lower = [](std::string s) { for (auto& c : s) c = std::tolower(c); return s; };
            ob_first = ob_to_lower(ob_first);
            if (ob_second.empty()) ob_second = ob_first; else ob_second = ob_to_lower(ob_second);
            auto parse_ob = [](const std::string& v) -> int {
                if (v == "auto") return 0; if (v == "contain") return 1; if (v == "none") return 2; return 0;
            };
            style.overscroll_behavior_x = parse_ob(ob_first);
            style.overscroll_behavior_y = parse_ob(ob_second);
            style.overscroll_behavior = parse_ob(ob_first);
        } else if (d.property == "overscroll-behavior-x") {
            if (val_lower == "auto") style.overscroll_behavior_x = 0;
            else if (val_lower == "contain") style.overscroll_behavior_x = 1;
            else if (val_lower == "none") style.overscroll_behavior_x = 2;
        } else if (d.property == "overscroll-behavior-y") {
            if (val_lower == "auto") style.overscroll_behavior_y = 0;
            else if (val_lower == "contain") style.overscroll_behavior_y = 1;
            else if (val_lower == "none") style.overscroll_behavior_y = 2;
        } else if (d.property == "paint-order") {
            style.paint_order = val_lower;
        } else if (d.property == "dominant-baseline") {
            if (val_lower == "auto") style.dominant_baseline = 0;
            else if (val_lower == "text-bottom") style.dominant_baseline = 1;
            else if (val_lower == "alphabetic") style.dominant_baseline = 2;
            else if (val_lower == "ideographic") style.dominant_baseline = 3;
            else if (val_lower == "middle") style.dominant_baseline = 4;
            else if (val_lower == "central") style.dominant_baseline = 5;
            else if (val_lower == "mathematical") style.dominant_baseline = 6;
            else if (val_lower == "hanging") style.dominant_baseline = 7;
            else if (val_lower == "text-top") style.dominant_baseline = 8;
            else style.dominant_baseline = 0;
        } else if (d.property == "initial-letter") {
            if (val_lower == "normal") {
                style.initial_letter_size = 0;
                style.initial_letter_sink = 0;
                style.initial_letter = 0;
            } else {
                std::istringstream iss(d.value);
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
        } else if (d.property == "initial-letter-align") {
            if (val_lower == "border-box") style.initial_letter_align = 1;
            else if (val_lower == "alphabetic") style.initial_letter_align = 2;
            else style.initial_letter_align = 0; // auto
        } else if (d.property == "text-emphasis-style") {
            style.text_emphasis_style = val_lower;
        } else if (d.property == "text-emphasis-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) {
                style.text_emphasis_color = (static_cast<uint32_t>(c->a) << 24) |
                                            (static_cast<uint32_t>(c->r) << 16) |
                                            (static_cast<uint32_t>(c->g) << 8) |
                                            (static_cast<uint32_t>(c->b));
            }
        } else if (d.property == "-webkit-text-stroke" || d.property == "text-stroke") {
            auto parts = split_whitespace(val_lower);
            for (const auto& part : parts) {
                auto l = clever::css::parse_length(part);
                if (l && l->value > 0) {
                    style.text_stroke_width = l->to_px();
                } else {
                    auto c = clever::css::parse_color(part);
                    if (c) style.text_stroke_color = *c;
                }
            }
        } else if (d.property == "-webkit-text-stroke-width") {
            auto l = clever::css::parse_length(val_lower);
            if (l) style.text_stroke_width = l->to_px();
        } else if (d.property == "-webkit-text-stroke-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) style.text_stroke_color = *c;
        } else if (d.property == "-webkit-text-fill-color") {
            auto c = clever::css::parse_color(val_lower);
            if (c) style.text_fill_color = *c;
        } else if (d.property == "inset") {
            // CSS inset shorthand: sets top/right/bottom/left (1-4 values like margin)
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) { style.top = *v; style.right_pos = *v; style.bottom = *v; style.left_pos = *v; }
            } else if (parts.size() == 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) { style.top = *v1; style.bottom = *v1; }
                if (v2) { style.right_pos = *v2; style.left_pos = *v2; }
            } else if (parts.size() == 3) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                if (v1) style.top = *v1;
                if (v2) { style.right_pos = *v2; style.left_pos = *v2; }
                if (v3) style.bottom = *v3;
            } else if (parts.size() >= 4) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                auto v4 = clever::css::parse_length(parts[3]);
                if (v1) style.top = *v1;
                if (v2) style.right_pos = *v2;
                if (v3) style.bottom = *v3;
                if (v4) style.left_pos = *v4;
            }
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-block") {
            // CSS inset-block logical shorthand: maps to top/bottom
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) { style.top = *v; style.bottom = *v; }
            } else if (parts.size() >= 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) style.top = *v1;
                if (v2) style.bottom = *v2;
            }
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-inline") {
            // CSS inset-inline logical shorthand: maps to left/right
            auto parts = split_whitespace(val_lower);
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) { style.left_pos = *v; style.right_pos = *v; }
            } else if (parts.size() >= 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) style.left_pos = *v1;
                if (v2) style.right_pos = *v2;
            }
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-inline-start") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.left_pos = *v;
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-inline-end") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.right_pos = *v;
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-block-start") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.top = *v;
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "inset-block-end") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.bottom = *v;
            if (style.position == clever::css::Position::Static)
                style.position = clever::css::Position::Relative;
        } else if (d.property == "place-content") {
            // CSS place-content shorthand: sets align-content and justify-content
            auto parts = split_whitespace(val_lower);
            auto parse_align_val = [](const std::string& s) -> int {
                if (s == "flex-start" || s == "start" || s == "normal") return 0;
                if (s == "flex-end" || s == "end") return 1;
                if (s == "center") return 2;
                if (s == "stretch") return 3;
                if (s == "space-between") return 4;
                if (s == "space-around") return 5;
                if (s == "space-evenly") return 6;
                return 0;
            };
            auto int_to_jc = [](int v) -> clever::css::JustifyContent {
                switch (v) {
                    case 0: return clever::css::JustifyContent::FlexStart;
                    case 1: return clever::css::JustifyContent::FlexEnd;
                    case 2: return clever::css::JustifyContent::Center;
                    case 3: return clever::css::JustifyContent::FlexStart; // stretch -> flex-start for JC
                    case 4: return clever::css::JustifyContent::SpaceBetween;
                    case 5: return clever::css::JustifyContent::SpaceAround;
                    case 6: return clever::css::JustifyContent::SpaceEvenly;
                    default: return clever::css::JustifyContent::FlexStart;
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
        } else if (d.property == "text-underline-position") {
            if (val_lower == "auto") style.text_underline_position = 0;
            else if (val_lower == "under") style.text_underline_position = 1;
            else if (val_lower == "left") style.text_underline_position = 2;
            else if (val_lower == "right") style.text_underline_position = 3;
        } else if (d.property == "scroll-margin") {
            auto parts = split_whitespace(val_lower);
            float t=0, r=0, b=0, l=0;
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) t = r = b = l = v->to_px(0);
            } else if (parts.size() == 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) t = b = v1->to_px(0);
                if (v2) r = l = v2->to_px(0);
            } else if (parts.size() == 3) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                if (v1) t = v1->to_px(0);
                if (v2) r = l = v2->to_px(0);
                if (v3) b = v3->to_px(0);
            } else if (parts.size() >= 4) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                auto v4 = clever::css::parse_length(parts[3]);
                if (v1) t = v1->to_px(0);
                if (v2) r = v2->to_px(0);
                if (v3) b = v3->to_px(0);
                if (v4) l = v4->to_px(0);
            }
            style.scroll_margin_top = t;
            style.scroll_margin_right = r;
            style.scroll_margin_bottom = b;
            style.scroll_margin_left = l;
        } else if (d.property == "scroll-margin-top") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_top = v->to_px(0);
        } else if (d.property == "scroll-margin-right") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_right = v->to_px(0);
        } else if (d.property == "scroll-margin-bottom") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_bottom = v->to_px(0);
        } else if (d.property == "scroll-margin-left") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_left = v->to_px(0);
        } else if (d.property == "scroll-padding") {
            auto parts = split_whitespace(val_lower);
            float t=0, r=0, b=0, l=0;
            if (parts.size() == 1) {
                auto v = clever::css::parse_length(parts[0]);
                if (v) t = r = b = l = v->to_px(0);
            } else if (parts.size() == 2) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                if (v1) t = b = v1->to_px(0);
                if (v2) r = l = v2->to_px(0);
            } else if (parts.size() == 3) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                if (v1) t = v1->to_px(0);
                if (v2) r = l = v2->to_px(0);
                if (v3) b = v3->to_px(0);
            } else if (parts.size() >= 4) {
                auto v1 = clever::css::parse_length(parts[0]);
                auto v2 = clever::css::parse_length(parts[1]);
                auto v3 = clever::css::parse_length(parts[2]);
                auto v4 = clever::css::parse_length(parts[3]);
                if (v1) t = v1->to_px(0);
                if (v2) r = v2->to_px(0);
                if (v3) b = v3->to_px(0);
                if (v4) l = v4->to_px(0);
            }
            style.scroll_padding_top = t;
            style.scroll_padding_right = r;
            style.scroll_padding_bottom = b;
            style.scroll_padding_left = l;
        } else if (d.property == "scroll-padding-top") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_padding_top = v->to_px(0);
        } else if (d.property == "scroll-padding-right") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_padding_right = v->to_px(0);
        } else if (d.property == "scroll-padding-bottom") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_padding_bottom = v->to_px(0);
        } else if (d.property == "scroll-padding-left") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_padding_left = v->to_px(0);
        } else if (d.property == "scroll-padding-inline") {
            auto v = clever::css::parse_length(d.value);
            if (v) { style.scroll_padding_left = v->to_px(0); style.scroll_padding_right = v->to_px(0); }
        } else if (d.property == "scroll-padding-block") {
            auto v = clever::css::parse_length(d.value);
            if (v) { style.scroll_padding_top = v->to_px(0); style.scroll_padding_bottom = v->to_px(0); }
        } else if (d.property == "text-rendering") {
            if (val_lower == "auto") style.text_rendering = 0;
            else if (val_lower == "optimizespeed") style.text_rendering = 1;
            else if (val_lower == "optimizelegibility") style.text_rendering = 2;
            else if (val_lower == "geometricprecision") style.text_rendering = 3;
        } else if (d.property == "ruby-align") {
            if (val_lower == "space-around") style.ruby_align = 0;
            else if (val_lower == "start") style.ruby_align = 1;
            else if (val_lower == "center") style.ruby_align = 2;
            else if (val_lower == "space-between") style.ruby_align = 3;
        } else if (d.property == "ruby-position") {
            if (val_lower == "over") style.ruby_position = 0;
            else if (val_lower == "under") style.ruby_position = 1;
            else if (val_lower == "inter-character") style.ruby_position = 2;
        } else if (d.property == "ruby-overhang") {
            if (val_lower == "auto") style.ruby_overhang = 0;
            else if (val_lower == "none") style.ruby_overhang = 1;
            else if (val_lower == "start") style.ruby_overhang = 2;
            else if (val_lower == "end") style.ruby_overhang = 3;
        } else if (d.property == "text-combine-upright") {
            if (val_lower == "none") style.text_combine_upright = 0;
            else if (val_lower == "all") style.text_combine_upright = 1;
            else if (val_lower == "digits") style.text_combine_upright = 2;
        } else if (d.property == "text-orientation") {
            if (val_lower == "mixed") style.text_orientation = 0;
            else if (val_lower == "upright") style.text_orientation = 1;
            else if (val_lower == "sideways") style.text_orientation = 2;
        } else if (d.property == "backface-visibility") {
            if (val_lower == "visible") style.backface_visibility = 0;
            else if (val_lower == "hidden") style.backface_visibility = 1;
        } else if (d.property == "overflow-anchor") {
            if (val_lower == "auto") style.overflow_anchor = 0;
            else if (val_lower == "none") style.overflow_anchor = 1;
        } else if (d.property == "overflow-clip-margin") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.overflow_clip_margin = v->to_px(0);
            else style.overflow_clip_margin = 0.0f;
        } else if (d.property == "perspective") {
            if (val_lower == "none") {
                style.perspective = 0;
            } else {
                auto v = clever::css::parse_length(d.value);
                if (v) style.perspective = v->to_px(0);
            }
        } else if (d.property == "transform-style") {
            if (val_lower == "flat") style.transform_style = 0;
            else if (val_lower == "preserve-3d") style.transform_style = 1;
        } else if (d.property == "transform-origin") {
            auto parse_origin_token = [](const std::string& s) -> clever::css::Length {
                if (s == "left" || s == "top") return clever::css::Length::percent(0.0f);
                if (s == "center") return clever::css::Length::percent(50.0f);
                if (s == "right" || s == "bottom") return clever::css::Length::percent(100.0f);
                if (s.size() > 1 && s.back() == '%') {
                    try { return clever::css::Length::percent(std::stof(s.substr(0, s.size()-1))); } catch(...) {}
                }
                auto len = clever::css::parse_length(s);
                if (len) return *len;
                return clever::css::Length::percent(50.0f);
            };
            auto parts = split_whitespace(val_lower);
            if (parts.size() >= 2) {
                auto lx = parse_origin_token(parts[0]);
                auto ly = parse_origin_token(parts[1]);
                style.transform_origin_x_len = lx;
                style.transform_origin_y_len = ly;
                style.transform_origin_x = (lx.unit == clever::css::Length::Unit::Percent) ? lx.value : 50.0f;
                style.transform_origin_y = (ly.unit == clever::css::Length::Unit::Percent) ? ly.value : 50.0f;
                if (parts.size() >= 3) {
                    auto lz = clever::css::parse_length(parts[2]);
                    if (lz) style.transform_origin_z = lz->to_px();
                }
            } else if (parts.size() == 1) {
                auto lx = parse_origin_token(parts[0]);
                style.transform_origin_x_len = lx;
                style.transform_origin_y_len = clever::css::Length::percent(50.0f);
                style.transform_origin_x = (lx.unit == clever::css::Length::Unit::Percent) ? lx.value : 50.0f;
                style.transform_origin_y = 50.0f;
            }
        } else if (d.property == "perspective-origin") {
            auto parse_origin_token = [](const std::string& s) -> clever::css::Length {
                if (s == "left" || s == "top") return clever::css::Length::percent(0.0f);
                if (s == "center") return clever::css::Length::percent(50.0f);
                if (s == "right" || s == "bottom") return clever::css::Length::percent(100.0f);
                if (s.size() > 1 && s.back() == '%') {
                    try { return clever::css::Length::percent(std::stof(s.substr(0, s.size()-1))); } catch(...) {}
                }
                auto len = clever::css::parse_length(s);
                if (len) return *len;
                return clever::css::Length::percent(50.0f);
            };
            auto parts = split_whitespace(val_lower);
            if (parts.size() >= 2) {
                auto lx = parse_origin_token(parts[0]);
                auto ly = parse_origin_token(parts[1]);
                style.perspective_origin_x_len = lx;
                style.perspective_origin_y_len = ly;
                style.perspective_origin_x = (lx.unit == clever::css::Length::Unit::Percent) ? lx.value : 50.0f;
                style.perspective_origin_y = (ly.unit == clever::css::Length::Unit::Percent) ? ly.value : 50.0f;
            } else if (parts.size() == 1) {
                auto lx = parse_origin_token(parts[0]);
                style.perspective_origin_x_len = lx;
                style.perspective_origin_y_len = clever::css::Length::percent(50.0f);
                style.perspective_origin_x = (lx.unit == clever::css::Length::Unit::Percent) ? lx.value : 50.0f;
                style.perspective_origin_y = 50.0f;
            }
        } else if (d.property == "fill") {
            if (val_lower == "none") {
                style.svg_fill_none = true;
            } else {
                auto col = clever::css::parse_color(d.value);
                if (col) {
                    style.svg_fill_color = (static_cast<uint32_t>(col->a) << 24) |
                                           (static_cast<uint32_t>(col->r) << 16) |
                                           (static_cast<uint32_t>(col->g) << 8) |
                                           static_cast<uint32_t>(col->b);
                    style.svg_fill_none = false;
                }
            }
        } else if (d.property == "stroke") {
            if (val_lower == "none") {
                style.svg_stroke_none = true;
            } else {
                auto col = clever::css::parse_color(d.value);
                if (col) {
                    style.svg_stroke_color = (static_cast<uint32_t>(col->a) << 24) |
                                             (static_cast<uint32_t>(col->r) << 16) |
                                             (static_cast<uint32_t>(col->g) << 8) |
                                             static_cast<uint32_t>(col->b);
                    style.svg_stroke_none = false;
                }
            }
        } else if (d.property == "fill-opacity") {
            try { style.svg_fill_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
        } else if (d.property == "stroke-opacity") {
            try { style.svg_stroke_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
        } else if (d.property == "fill-rule") {
            if (val_lower == "nonzero") style.fill_rule = 0;
            else if (val_lower == "evenodd") style.fill_rule = 1;
        } else if (d.property == "clip-rule") {
            if (val_lower == "nonzero") style.clip_rule = 0;
            else if (val_lower == "evenodd") style.clip_rule = 1;
        } else if (d.property == "stroke-miterlimit") {
            try { style.stroke_miterlimit = std::stof(val_lower); } catch(...) {}
        } else if (d.property == "shape-rendering") {
            if (val_lower == "auto") style.shape_rendering = 0;
            else if (val_lower == "optimizespeed") style.shape_rendering = 1;
            else if (val_lower == "crispedges") style.shape_rendering = 2;
            else if (val_lower == "geometricprecision") style.shape_rendering = 3;
        } else if (d.property == "vector-effect") {
            if (val_lower == "none") style.vector_effect = 0;
            else if (val_lower == "non-scaling-stroke") style.vector_effect = 1;
        } else if (d.property == "stop-color") {
            auto col = clever::css::parse_color(d.value);
            if (col) {
                style.stop_color = (static_cast<uint32_t>(col->a) << 24) |
                                   (static_cast<uint32_t>(col->r) << 16) |
                                   (static_cast<uint32_t>(col->g) << 8) |
                                   static_cast<uint32_t>(col->b);
            }
        } else if (d.property == "stop-opacity") {
            try { style.stop_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
        } else if (d.property == "flood-color") {
            auto col = clever::css::parse_color(d.value);
            if (col) {
                style.flood_color = (static_cast<uint32_t>(col->a) << 24) |
                                   (static_cast<uint32_t>(col->r) << 16) |
                                   (static_cast<uint32_t>(col->g) << 8) |
                                   static_cast<uint32_t>(col->b);
            }
        } else if (d.property == "flood-opacity") {
            try { style.flood_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
        } else if (d.property == "lighting-color") {
            auto col = clever::css::parse_color(d.value);
            if (col) {
                style.lighting_color = (static_cast<uint32_t>(col->a) << 24) |
                                      (static_cast<uint32_t>(col->r) << 16) |
                                      (static_cast<uint32_t>(col->g) << 8) |
                                      static_cast<uint32_t>(col->b);
            }
        } else if (d.property == "marker") {
            style.marker_shorthand = d.value;
            style.marker_start = d.value;
            style.marker_mid = d.value;
            style.marker_end = d.value;
        } else if (d.property == "marker-start") {
            style.marker_start = d.value;
        } else if (d.property == "marker-mid") {
            style.marker_mid = d.value;
        } else if (d.property == "marker-end") {
            style.marker_end = d.value;
        } else if (d.property == "scrollbar-color") {
            if (val_lower == "auto") {
                style.scrollbar_thumb_color = 0;
                style.scrollbar_track_color = 0;
            } else {
                auto parts = split_whitespace(d.value);
                if (parts.size() >= 2) {
                    auto c1 = clever::css::parse_color(parts[0]);
                    auto c2 = clever::css::parse_color(parts[1]);
                    if (c1) { auto& cc = *c1; style.scrollbar_thumb_color = (uint32_t(cc.a)<<24)|(uint32_t(cc.r)<<16)|(uint32_t(cc.g)<<8)|cc.b; }
                    if (c2) { auto& cc = *c2; style.scrollbar_track_color = (uint32_t(cc.a)<<24)|(uint32_t(cc.r)<<16)|(uint32_t(cc.g)<<8)|cc.b; }
                }
            }
        } else if (d.property == "scrollbar-width") {
            if (val_lower == "auto") style.scrollbar_width = 0;
            else if (val_lower == "thin") style.scrollbar_width = 1;
            else if (val_lower == "none") style.scrollbar_width = 2;
        } else if (d.property == "scrollbar-gutter") {
            if (val_lower == "auto") style.scrollbar_gutter = 0;
            else if (val_lower == "stable") style.scrollbar_gutter = 1;
            else if (val_lower == "stable both-edges") style.scrollbar_gutter = 2;
        } else if (d.property == "scroll-snap-stop") {
            if (val_lower == "normal") style.scroll_snap_stop = 0;
            else if (val_lower == "always") style.scroll_snap_stop = 1;
        } else if (d.property == "scroll-margin-block-start") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_top = v->to_px(0);
        } else if (d.property == "scroll-margin-block-end") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_bottom = v->to_px(0);
        } else if (d.property == "scroll-margin-inline-start") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_left = v->to_px(0);
        } else if (d.property == "scroll-margin-inline-end") {
            auto v = clever::css::parse_length(d.value);
            if (v) style.scroll_margin_right = v->to_px(0);
        } else if (d.property == "animation-composition") {
            if (val_lower == "replace") style.animation_composition = 0;
            else if (val_lower == "add") style.animation_composition = 1;
            else if (val_lower == "accumulate") style.animation_composition = 2;
        } else if (d.property == "animation-timeline") {
            if (val_lower == "auto") style.animation_timeline = "auto";
            else if (val_lower == "none") style.animation_timeline = "none";
            else style.animation_timeline = d.value;
        } else if (d.property == "transform-box") {
            if (val_lower == "content-box") style.transform_box = 0;
            else if (val_lower == "border-box") style.transform_box = 1;
            else if (val_lower == "fill-box") style.transform_box = 2;
            else if (val_lower == "stroke-box") style.transform_box = 3;
            else if (val_lower == "view-box") style.transform_box = 4;
        } else if (d.property == "mask-image" || d.property == "-webkit-mask-image") {
            style.mask_image = d.value;
        } else if (d.property == "mask-size" || d.property == "-webkit-mask-size") {
            if (val_lower == "auto") style.mask_size = 0;
            else if (val_lower == "cover") style.mask_size = 1;
            else if (val_lower == "contain") style.mask_size = 2;
            else {
                style.mask_size = 3; // explicit
                auto parts = split_whitespace(val_lower);
                if (parts.size() >= 1) { auto v = clever::css::parse_length(parts[0]); if (v) style.mask_size_width = v->to_px(); }
                if (parts.size() >= 2) { auto v = clever::css::parse_length(parts[1]); if (v) style.mask_size_height = v->to_px(); }
            }
        } else if (d.property == "contain-intrinsic-width") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.contain_intrinsic_width = v->to_px();
            else if (val_lower == "none" || val_lower == "auto") style.contain_intrinsic_width = 0;
        } else if (d.property == "contain-intrinsic-height") {
            auto v = clever::css::parse_length(val_lower);
            if (v) style.contain_intrinsic_height = v->to_px();
            else if (val_lower == "none" || val_lower == "auto") style.contain_intrinsic_height = 0;
        } else if (d.property == "mask-repeat" || d.property == "-webkit-mask-repeat") {
            if (val_lower == "repeat") style.mask_repeat = 0;
            else if (val_lower == "repeat-x") style.mask_repeat = 1;
            else if (val_lower == "repeat-y") style.mask_repeat = 2;
            else if (val_lower == "no-repeat") style.mask_repeat = 3;
            else if (val_lower == "space") style.mask_repeat = 4;
            else if (val_lower == "round") style.mask_repeat = 5;
        } else if (d.property == "mask-composite" || d.property == "-webkit-mask-composite") {
            if (val_lower == "add") style.mask_composite = 0;
            else if (val_lower == "subtract") style.mask_composite = 1;
            else if (val_lower == "intersect") style.mask_composite = 2;
            else if (val_lower == "exclude") style.mask_composite = 3;
        } else if (d.property == "mask-mode") {
            if (val_lower == "match-source") style.mask_mode = 0;
            else if (val_lower == "alpha") style.mask_mode = 1;
            else if (val_lower == "luminance") style.mask_mode = 2;
        } else if (d.property == "mask" || d.property == "-webkit-mask") {
            style.mask_shorthand = d.value;
        } else if (d.property == "mask-origin" || d.property == "-webkit-mask-origin") {
            if (val_lower == "border-box") style.mask_origin = 0;
            else if (val_lower == "padding-box") style.mask_origin = 1;
            else if (val_lower == "content-box") style.mask_origin = 2;
        } else if (d.property == "mask-position" || d.property == "-webkit-mask-position") {
            style.mask_position = d.value;
        } else if (d.property == "mask-clip" || d.property == "-webkit-mask-clip") {
            if (val_lower == "border-box") style.mask_clip = 0;
            else if (val_lower == "padding-box") style.mask_clip = 1;
            else if (val_lower == "content-box") style.mask_clip = 2;
            else if (val_lower == "no-clip") style.mask_clip = 3;
        } else if (d.property == "mask-border" || d.property == "mask-border-source" ||
                   d.property == "mask-border-slice" || d.property == "mask-border-width" ||
                   d.property == "mask-border-outset" || d.property == "mask-border-repeat" ||
                   d.property == "mask-border-mode") {
            style.mask_border = d.value;
        } else if (d.property == "font-smooth" || d.property == "-webkit-font-smoothing") {
            if (val_lower == "auto") style.font_smooth = 0;
            else if (val_lower == "none") style.font_smooth = 1;
            else if (val_lower == "antialiased") style.font_smooth = 2;
            else if (val_lower == "subpixel-antialiased") style.font_smooth = 3;
        } else if (d.property == "text-size-adjust" || d.property == "-webkit-text-size-adjust") {
            if (val_lower == "auto") style.text_size_adjust = "auto";
            else if (val_lower == "none") style.text_size_adjust = "none";
            else style.text_size_adjust = d.value; // preserve percentage string
        } else if (d.property == "offset-path") {
            if (val_lower == "none") style.offset_path = "none";
            else style.offset_path = d.value;
        } else if (d.property == "offset-distance") {
            auto l = clever::css::parse_length(d.value);
            if (l) style.offset_distance = l->to_px();
        } else if (d.property == "offset-rotate") {
            style.offset_rotate = d.value;
        } else if (d.property == "offset") {
            style.offset = d.value;
        } else if (d.property == "offset-anchor") {
            style.offset_anchor = d.value;
        } else if (d.property == "offset-position") {
            style.offset_position = d.value;
        } else if (d.property == "transition-behavior") {
            if (val_lower == "allow-discrete") style.transition_behavior = 1;
            else style.transition_behavior = 0;
        } else if (d.property == "animation-range") {
            style.animation_range = d.value;
        } else if (d.property == "rotate") {
            if (val_lower == "none") style.css_rotate = "none";
            else style.css_rotate = d.value;
        } else if (d.property == "scale") {
            if (val_lower == "none") style.css_scale = "none";
            else style.css_scale = d.value;
        } else if (d.property == "translate") {
            if (val_lower == "none") style.css_translate = "none";
            else style.css_translate = d.value;
        } else if (d.property == "overflow-block") {
            if (val_lower == "visible") style.overflow_block = 0;
            else if (val_lower == "hidden") style.overflow_block = 1;
            else if (val_lower == "scroll") style.overflow_block = 2;
            else if (val_lower == "auto") style.overflow_block = 3;
            else if (val_lower == "clip") style.overflow_block = 4;
        } else if (d.property == "overflow-inline") {
            if (val_lower == "visible") style.overflow_inline = 0;
            else if (val_lower == "hidden") style.overflow_inline = 1;
            else if (val_lower == "scroll") style.overflow_inline = 2;
            else if (val_lower == "auto") style.overflow_inline = 3;
            else if (val_lower == "clip") style.overflow_inline = 4;
        } else if (d.property == "box-decoration-break" || d.property == "-webkit-box-decoration-break") {
            if (val_lower == "slice") style.box_decoration_break = 0;
            else if (val_lower == "clone") style.box_decoration_break = 1;
        } else if (d.property == "margin-trim") {
            if (val_lower == "none") style.margin_trim = 0;
            else if (val_lower == "block") style.margin_trim = 1;
            else if (val_lower == "inline") style.margin_trim = 2;
            else if (val_lower == "block-start") style.margin_trim = 3;
            else if (val_lower == "block-end") style.margin_trim = 4;
            else if (val_lower == "inline-start") style.margin_trim = 5;
            else if (val_lower == "inline-end") style.margin_trim = 6;
        } else if (d.property == "all") {
            if (val_lower == "initial" || val_lower == "inherit" || val_lower == "unset" || val_lower == "revert") {
                style.css_all = val_lower;
            }
        }
    }
}

// Convert css::Display to layout::LayoutMode
clever::layout::LayoutMode display_to_mode(clever::css::Display d) {
    switch (d) {
        case clever::css::Display::Block:
        case clever::css::Display::ListItem:
        case clever::css::Display::TableRow:
        case clever::css::Display::TableCell:
        case clever::css::Display::TableHeaderGroup:
        case clever::css::Display::TableRowGroup:
            return clever::layout::LayoutMode::Block;
        case clever::css::Display::Table:
            return clever::layout::LayoutMode::Table;
        case clever::css::Display::Inline:
            return clever::layout::LayoutMode::Inline;
        case clever::css::Display::InlineBlock:
            return clever::layout::LayoutMode::InlineBlock;
        case clever::css::Display::Flex:
        case clever::css::Display::InlineFlex:
            return clever::layout::LayoutMode::Flex;
        case clever::css::Display::Grid:
        case clever::css::Display::InlineGrid:
            return clever::layout::LayoutMode::Grid;
        case clever::css::Display::None:
            return clever::layout::LayoutMode::None;
        case clever::css::Display::Contents:
            return clever::layout::LayoutMode::Block; // contents elements don't have a box
    }
    return clever::layout::LayoutMode::Block;
}

clever::layout::DisplayType display_to_type(clever::css::Display d) {
    switch (d) {
        case clever::css::Display::Block: return clever::layout::DisplayType::Block;
        case clever::css::Display::Inline: return clever::layout::DisplayType::Inline;
        case clever::css::Display::InlineBlock: return clever::layout::DisplayType::InlineBlock;
        case clever::css::Display::Flex: return clever::layout::DisplayType::Flex;
        case clever::css::Display::InlineFlex: return clever::layout::DisplayType::InlineFlex;
        case clever::css::Display::None: return clever::layout::DisplayType::None;
        case clever::css::Display::ListItem: return clever::layout::DisplayType::ListItem;
        case clever::css::Display::Table: return clever::layout::DisplayType::Table;
        case clever::css::Display::TableRow: return clever::layout::DisplayType::TableRow;
        case clever::css::Display::TableCell: return clever::layout::DisplayType::TableCell;
        case clever::css::Display::TableHeaderGroup: return clever::layout::DisplayType::Block;
        case clever::css::Display::TableRowGroup: return clever::layout::DisplayType::Block;
        case clever::css::Display::Grid: return clever::layout::DisplayType::Grid;
        case clever::css::Display::InlineGrid: return clever::layout::DisplayType::InlineGrid;
        case clever::css::Display::Contents: return clever::layout::DisplayType::Block;
    }
    return clever::layout::DisplayType::Block;
}

uint32_t color_to_argb(const clever::css::Color& c) {
    return (static_cast<uint32_t>(c.a) << 24) |
           (static_cast<uint32_t>(c.r) << 16) |
           (static_cast<uint32_t>(c.g) << 8) |
           static_cast<uint32_t>(c.b);
}

// ---- URL resolution ----

// Resolve a potentially relative URL against a base URL
std::string resolve_url(const std::string& href, const std::string& base_url) {
    if (href.empty()) return "";

    // Preserve already-absolute references exactly as authored.
    if (std::isalpha(static_cast<unsigned char>(href[0]))) {
        size_t i = 1;
        while (i < href.size()) {
            const char c = href[i];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.') {
                ++i;
                continue;
            }
            break;
        }
        if (i < href.size() && href[i] == ':') return href;
    }

    // Use the URL parser for standards-based resolution first.
    // This handles dot-segments, query/fragment-only refs, protocol-relative
    // URLs, and normalization in one place.
    if (auto base = clever::url::parse(base_url); base.has_value()) {
        if (auto resolved = clever::url::parse(href, &*base); resolved.has_value()) {
            return resolved->serialize();
        }
    }

    // Fallback for non-standard base URLs the URL parser rejects.
    // Keep legacy behavior so local/synthetic documents still resolve links.
    if (base_url.empty()) return href;

    auto strip_fragment = [](const std::string& value) {
        std::string out = value;
        auto hash_pos = out.find('#');
        if (hash_pos != std::string::npos) out.erase(hash_pos);
        return out;
    };

    auto strip_query = [](const std::string& value) {
        std::string out = value;
        auto query_pos = out.find('?');
        if (query_pos != std::string::npos) out.erase(query_pos);
        return out;
    };

    // Query/fragment-only references
    if (href[0] == '?') {
        std::string base = strip_query(strip_fragment(base_url));
        return base + href;
    }
    if (href[0] == '#') {
        std::string base = strip_fragment(base_url);
        return base + href;
    }

    // Protocol-relative
    if (href.size() >= 2 && href[0] == '/' && href[1] == '/') {
        auto scheme_end = base_url.find("://");
        if (scheme_end != std::string::npos) {
            return base_url.substr(0, scheme_end + 1) + href;
        }
        return "http:" + href;
    }

    auto normalize_path = [](const std::string& raw_path) {
        if (raw_path.empty()) return std::string();

        const bool absolute = raw_path[0] == '/';
        std::vector<std::string> segments;
        size_t pos = 0;
        while (pos <= raw_path.size()) {
            size_t next = raw_path.find('/', pos);
            if (next == std::string::npos) next = raw_path.size();
            std::string segment = raw_path.substr(pos, next - pos);

            if (segment.empty() || segment == ".") {
                // Skip
            } else if (segment == "..") {
                if (!segments.empty() && segments.back() != "..") {
                    segments.pop_back();
                } else if (!absolute) {
                    segments.push_back("..");
                }
            } else {
                segments.push_back(std::move(segment));
            }

            if (next == raw_path.size()) break;
            pos = next + 1;
        }

        std::string normalized;
        if (absolute) normalized.push_back('/');
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) normalized.push_back('/');
            normalized += segments[i];
        }

        if (raw_path.size() > 1 && raw_path.back() == '/' &&
            (normalized.empty() || normalized.back() != '/')) {
            normalized.push_back('/');
        } else if (normalized.empty() && absolute) {
            normalized = "/";
        }
        return normalized;
    };

    const std::string base_no_query = strip_query(strip_fragment(base_url));
    auto scheme_end = base_no_query.find("://");

    std::string prefix;
    std::string base_path;
    if (scheme_end != std::string::npos) {
        const size_t authority_start = scheme_end + 3;
        const size_t path_start = base_no_query.find('/', authority_start);
        if (path_start == std::string::npos) {
            prefix = base_no_query;
            base_path = "/";
        } else {
            prefix = base_no_query.substr(0, path_start);
            base_path = base_no_query.substr(path_start);
        }
    } else {
        base_path = base_no_query;
    }

    std::string merged_path;
    if (href[0] == '/') {
        merged_path = href;
    } else {
        std::string base_dir = base_path;
        if (base_dir.empty()) {
            base_dir = "/";
        } else {
            auto last_slash = base_dir.rfind('/');
            if (last_slash == std::string::npos) {
                base_dir.clear();
            } else {
                base_dir.erase(last_slash + 1);
            }
        }
        if (!base_dir.empty() && base_dir.back() != '/') {
            base_dir.push_back('/');
        }
        merged_path = base_dir + href;
    }

    std::string normalized_path = normalize_path(merged_path);
    if (normalized_path.empty()) {
        normalized_path = (merged_path.size() > 0 && merged_path[0] == '/') ? "/" : href;
    }

    if (!prefix.empty()) {
        return prefix + normalized_path;
    }
    if (!base_path.empty() && base_path[0] == '/' &&
        (normalized_path.empty() || normalized_path[0] != '/')) {
        return "/" + normalized_path;
    }
    return normalized_path.empty() ? href : normalized_path;
}

// Fetch a URL with redirect following (up to 5 hops)
std::optional<clever::net::Response> fetch_with_redirects(
    const std::string& url, const std::string& accept,
    int timeout_secs = 5, std::string* final_url = nullptr) {

    clever::net::HttpClient client;
    client.set_timeout(std::chrono::seconds(timeout_secs));
    // Keep redirect ownership inside this function so we can persist
    // intermediate Set-Cookie headers and update final_url consistently.
    client.set_max_redirects(0);

    auto& jar = clever::net::CookieJar::shared();
    std::string current_url = url;

    // Check for pending POST form submission data (only on first iteration)
    std::string pending_post_body;
    std::string pending_post_content_type;
    bool has_pending_post = clever::js::consume_pending_post_submission(pending_post_body, pending_post_content_type);

    for (int i = 0; i < 5; i++) {
        clever::net::Request req;
        req.url = current_url;
        req.method = clever::net::Method::GET;
        req.parse_url();
        req.headers.set("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Vibrowser/0.7.0 Safari/537.36");
        req.headers.set("Accept", accept);
        req.headers.set("Connection", "keep-alive");

        // If this is the first request and we have pending POST data, use it
        if (i == 0 && has_pending_post && !pending_post_body.empty()) {
            req.method = clever::net::Method::POST;
            req.body = std::vector<uint8_t>(pending_post_body.begin(), pending_post_body.end());
            if (!pending_post_content_type.empty()) {
                req.headers.set("Content-Type", pending_post_content_type);
            }
        }

        // Attach cookies
        std::string cookies = jar.get_cookie_header(req.host, req.path, req.use_tls);
        if (!cookies.empty()) {
            req.headers.set("Cookie", cookies);
        }

        auto response = client.fetch(req);
        if (!response) return std::nullopt;
        const std::string response_url =
            response->url.empty() ? current_url : response->url;

        // Store cookies from response
        for (auto& cookie_val : response->headers.get_all("set-cookie")) {
            jar.set_from_header(cookie_val, req.host);
        }

        if (response->status == 301 || response->status == 302 ||
            response->status == 303 || response->status == 307 ||
            response->status == 308) {
            auto location = response->headers.get("location");
            if (!location || location->empty()) {
                if (final_url) *final_url = response_url;
                return response;
            }
            current_url = resolve_url(*location, response_url);
            continue;
        }
        if (final_url) *final_url = response_url;
        return response;
    }
    return std::nullopt;
}

// Fetch a CSS stylesheet from a URL
std::string fetch_css(const std::string& url, std::string* final_url = nullptr) {
    auto response = fetch_with_redirects(url, "text/css, */*", 5, final_url);
    if (!response || response->status < 200 || response->status >= 300) {
        return "";
    }

    const std::string body = response->body_as_string();
    auto content_type = response->headers.get("content-type");
    if (content_type.has_value()) {
        const std::string ct = to_lower(*content_type);
        if (ct.find("text/css") == std::string::npos &&
            ct.find("application/x-css") == std::string::npos &&
            ct.find("text/plain") == std::string::npos) {
            return "";
        }
    }

    const std::string probe = to_lower(trim(body.substr(0, std::min<size_t>(body.size(), 256))));
    if (probe.rfind("<!doctype html", 0) == 0 || probe.rfind("<html", 0) == 0) {
        return "";
    }

    return body;
}


// Fetch and decode an image from a URL, returns decoded RGBA pixels
struct DecodedImage {
    std::shared_ptr<std::vector<uint8_t>> pixels;
    int width = 0;
    int height = 0;
};

// Try native macOS CGImageSource for broader format support (WebP, HEIC, TIFF, ICO, etc.)
#ifdef __APPLE__
DecodedImage decode_image_native(const uint8_t* data, size_t length) {
    DecodedImage result;
    CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault, data, static_cast<CFIndex>(length));
    if (!cf_data) return result;

    CGImageSourceRef source = CGImageSourceCreateWithData(cf_data, nullptr);
    CFRelease(cf_data);
    if (!source) return result;

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!image) return result;

    int w = static_cast<int>(CGImageGetWidth(image));
    int h = static_cast<int>(CGImageGetHeight(image));
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        CGImageRelease(image);
        return result;
    }

    // Render into RGBA buffer
    size_t bytes_per_row = static_cast<size_t>(w) * 4;
    auto pixels = std::make_shared<std::vector<uint8_t>>(bytes_per_row * static_cast<size_t>(h), 0);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        pixels->data(), w, h, 8, bytes_per_row, color_space,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(color_space);

    if (!ctx) {
        CGImageRelease(image);
        return result;
    }

    // Draw image into the RGBA buffer as-is. The CGBitmapContext memory layout
    // already matches our renderer's top-left row-major expectation here.
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), image);
    CGContextRelease(ctx);
    CGImageRelease(image);

    // Un-premultiply alpha (our renderer expects straight alpha)
    uint8_t* p = pixels->data();
    size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < total; ++i) {
        uint8_t a = p[3];
        if (a > 0 && a < 255) {
            p[0] = static_cast<uint8_t>(std::min(255, (p[0] * 255 + a / 2) / a));
            p[1] = static_cast<uint8_t>(std::min(255, (p[1] * 255 + a / 2) / a));
            p[2] = static_cast<uint8_t>(std::min(255, (p[2] * 255 + a / 2) / a));
        }
        p += 4;
    }

    result.width = w;
    result.height = h;
    result.pixels = pixels;
    return result;
}
#endif

// In-memory image cache: avoids re-fetching/decoding images on hover re-renders.
// Keyed by URL. Max ~64MB of decoded pixel data. LRU eviction when full.
// Protected by s_image_cache_mutex for thread-safe parallel prefetching.
static std::mutex s_image_cache_mutex;
static std::unordered_map<std::string, DecodedImage> s_image_cache;
static std::vector<std::string> s_image_cache_order;
static size_t s_image_cache_bytes = 0;
static constexpr size_t IMAGE_CACHE_MAX_BYTES = 64 * 1024 * 1024; // 64MB

// Internal helpers — callers must hold s_image_cache_mutex
static void image_cache_remove_from_order_locked(const std::string& url) {
    auto order_it = std::find(s_image_cache_order.begin(), s_image_cache_order.end(), url);
    if (order_it != s_image_cache_order.end()) {
        s_image_cache_order.erase(order_it);
    }
}

static void image_cache_evict_locked() {
    while (s_image_cache_bytes > IMAGE_CACHE_MAX_BYTES && !s_image_cache_order.empty()) {
        const auto& oldest_url = s_image_cache_order.front();
        auto it = s_image_cache.find(oldest_url);
        if (it != s_image_cache.end()) {
            if (it->second.pixels) {
                s_image_cache_bytes -= it->second.pixels->size();
            }
            s_image_cache.erase(it);
        }
        s_image_cache_order.erase(s_image_cache_order.begin());
    }
}

// Thread-safe public cache operations
static void image_cache_touch(const std::string& url) {
    // Note: caller must hold s_image_cache_mutex, OR this is called within
    // fetch_and_decode_image which already holds the lock.
    image_cache_remove_from_order_locked(url);
    s_image_cache_order.push_back(url);
}

static void image_cache_store(const std::string& url, const DecodedImage& image) {
    std::lock_guard<std::mutex> lock(s_image_cache_mutex);
    auto existing = s_image_cache.find(url);
    if (existing != s_image_cache.end()) {
        if (existing->second.pixels) {
            s_image_cache_bytes -= existing->second.pixels->size();
        }
        s_image_cache.erase(existing);
        image_cache_remove_from_order_locked(url);
    }

    s_image_cache[url] = image;
    if (image.pixels) {
        s_image_cache_bytes += image.pixels->size();
    }
    s_image_cache_order.push_back(url);
    image_cache_evict_locked();
}

// Helper: add SVG style attributes (fill, stroke, opacity) to SVG string
static void add_svg_style_attrs(std::ostringstream& svg, const clever::layout::LayoutNode& node) {
    // fill color
    if (!node.svg_fill_none) {
        uint32_t c = node.svg_fill_color;
        svg << " fill=\"#" << std::hex << std::setfill('0')
            << std::setw(6) << (c & 0xFFFFFFu) << std::dec << "\"";
    } else {
        svg << " fill=\"none\"";
    }

    // stroke color
    if (!node.svg_stroke_none) {
        uint32_t c = node.svg_stroke_color;
        svg << " stroke=\"#" << std::hex << std::setfill('0')
            << std::setw(6) << (c & 0xFFFFFFu) << std::dec << "\"";
    }

    // opacity
    if (node.svg_fill_opacity < 1.0f) {
        svg << " fill-opacity=\"" << node.svg_fill_opacity << "\"";
    }
    if (node.svg_stroke_opacity < 1.0f) {
        svg << " stroke-opacity=\"" << node.svg_stroke_opacity << "\"";
    }
}

// Serialize parsed inline SVG nodes back to SVG XML string
static std::string serialize_svg_node(const clever::layout::LayoutNode& node) {
    if (!node.is_svg) return "";

    std::ostringstream svg;

    if (node.svg_type == 0 && !node.is_svg_group) {
        // SVG container root element
        svg << "<svg";
        if (node.geometry.width > 0 && node.geometry.height > 0) {
            svg << " width=\"" << static_cast<int>(node.geometry.width) << "\"";
            svg << " height=\"" << static_cast<int>(node.geometry.height) << "\"";
        } else {
            svg << " width=\"300\" height=\"150\"";
        }

        if (node.svg_has_viewbox) {
            svg << " viewBox=\"" << node.svg_viewbox_x << " " << node.svg_viewbox_y
                << " " << node.svg_viewbox_w << " " << node.svg_viewbox_h << "\"";
        }
        svg << " xmlns=\"http://www.w3.org/2000/svg\">";

        // Serialize children
        for (const auto& child : node.children) {
            svg << serialize_svg_node(*child);
        }

        svg << "</svg>";
    }
    else if (node.is_svg_group) {
        // Group element
        svg << "<g";
        if (node.svg_transform_tx != 0 || node.svg_transform_ty != 0 ||
            node.svg_transform_sx != 1 || node.svg_transform_sy != 1 ||
            node.svg_transform_rotate != 0) {
            svg << " transform=\"";
            if (node.svg_transform_tx != 0 || node.svg_transform_ty != 0) {
                svg << "translate(" << node.svg_transform_tx << " " << node.svg_transform_ty << ")";
            }
            if (node.svg_transform_rotate != 0) {
                svg << " rotate(" << node.svg_transform_rotate << ")";
            }
            svg << "\"";
        }
        svg << ">";

        for (const auto& child : node.children) {
            svg << serialize_svg_node(*child);
        }

        svg << "</g>";
    }
    else if (node.svg_type == 1) {
        // rect
        if (node.svg_attrs.size() >= 5) {
            svg << "<rect x=\"" << node.svg_attrs[0] << "\" y=\"" << node.svg_attrs[1]
                << "\" width=\"" << node.svg_attrs[2] << "\" height=\"" << node.svg_attrs[3] << "\"";
            add_svg_style_attrs(svg, node);
            svg << "/>";
        }
    }
    else if (node.svg_type == 2) {
        // circle
        if (node.svg_attrs.size() >= 3) {
            svg << "<circle cx=\"" << node.svg_attrs[0] << "\" cy=\"" << node.svg_attrs[1]
                << "\" r=\"" << node.svg_attrs[2] << "\"";
            add_svg_style_attrs(svg, node);
            svg << "/>";
        }
    }
    else if (node.svg_type == 3) {
        // ellipse
        if (node.svg_attrs.size() >= 4) {
            svg << "<ellipse cx=\"" << node.svg_attrs[0] << "\" cy=\"" << node.svg_attrs[1]
                << "\" rx=\"" << node.svg_attrs[2] << "\" ry=\"" << node.svg_attrs[3] << "\"";
            add_svg_style_attrs(svg, node);
            svg << "/>";
        }
    }
    else if (node.svg_type == 4) {
        // line
        if (node.svg_attrs.size() >= 4) {
            svg << "<line x1=\"" << node.svg_attrs[0] << "\" y1=\"" << node.svg_attrs[1]
                << "\" x2=\"" << node.svg_attrs[2] << "\" y2=\"" << node.svg_attrs[3] << "\"";
            add_svg_style_attrs(svg, node);
            svg << "/>";
        }
    }
    else if (node.svg_type == 5) {
        // path
        svg << "<path d=\"" << node.svg_path_d << "\"";
        add_svg_style_attrs(svg, node);
        svg << "/>";
    }
    else if (node.svg_type == 7) {
        // polygon
        svg << "<polygon points=\"";
        for (size_t i = 0; i < node.svg_points.size(); i++) {
            if (i > 0) svg << " ";
            svg << node.svg_points[i].first << "," << node.svg_points[i].second;
        }
        svg << "\"";
        add_svg_style_attrs(svg, node);
        svg << "/>";
    }
    else if (node.svg_type == 8) {
        // polyline
        svg << "<polyline points=\"";
        for (size_t i = 0; i < node.svg_points.size(); i++) {
            if (i > 0) svg << " ";
            svg << node.svg_points[i].first << "," << node.svg_points[i].second;
        }
        svg << "\"";
        add_svg_style_attrs(svg, node);
        svg << "/>";
    }
    else if (node.svg_type == 6) {
        // text
        svg << "<text x=\"" << node.svg_text_x << "\" y=\"" << node.svg_text_y << "\"";
        if (node.svg_font_size > 0) {
            svg << " font-size=\"" << node.svg_font_size << "\"";
        }
        svg << ">";
        svg << node.svg_text_content;
        svg << "</text>";
    }

    return svg.str();
}

// Rasterize SVG data to RGBA pixels using nanosvg
static DecodedImage decode_svg_image(const char* svg_data, size_t length, float target_width = 0) {
    DecodedImage result;
    // nsvgParse modifies the input string, so we need a mutable copy
    std::string svg_copy(svg_data, length);
    NSVGimage* svg = nsvgParse(svg_copy.data(), "px", 96.0f);
    if (!svg) return result;

    if (svg->width <= 0 || svg->height <= 0) {
        nsvgDelete(svg);
        return result;
    }

    // Determine rasterization scale — default to 1x, or scale to target_width
    float scale = 1.0f;
    if (target_width > 0 && svg->width > 0) {
        scale = target_width / svg->width;
    }
    // Clamp to reasonable sizes
    int w = static_cast<int>(svg->width * scale);
    int h = static_cast<int>(svg->height * scale);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        // Fall back to original size or clamp
        if (w > 4096 || h > 4096) {
            float max_dim = std::max(svg->width, svg->height);
            scale = 4096.0f / max_dim;
            w = static_cast<int>(svg->width * scale);
            h = static_cast<int>(svg->height * scale);
        }
        if (w <= 0 || h <= 0) {
            nsvgDelete(svg);
            return result;
        }
    }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) {
        nsvgDelete(svg);
        return result;
    }

    auto pixels = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(w) * h * 4, 0);
    nsvgRasterize(rast, svg, 0, 0, scale, pixels->data(), w, h, w * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);

    result.width = w;
    result.height = h;
    result.pixels = pixels;
    return result;
}

// Base64 decode helper for data: URI image support
static bool base64_decode_bytes(const std::string& input, std::vector<uint8_t>& output) {
    output.clear();
    output.reserve(input.size() * 3 / 4);
    unsigned int val = 0;
    int valb = -8;
    for (char c : input) {
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int d = -1;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        if (d < 0) return false;
        val = (val << 6) + static_cast<unsigned>(d);
        valb += 6;
        if (valb >= 0) {
            output.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

DecodedImage fetch_and_decode_image(const std::string& url) {
    DecodedImage result;
    if (url.empty()) return result;

    // Check cache first (thread-safe)
    {
        std::lock_guard<std::mutex> lock(s_image_cache_mutex);
        auto cache_it = s_image_cache.find(url);
        if (cache_it != s_image_cache.end()) {
            image_cache_touch(url);
            return cache_it->second;
        }
    }

    // Handle data: URIs (e.g., data:image/png;base64,iVBOR...)
    if (url.size() > 5 && to_lower(url.substr(0, 5)) == "data:") {
        auto comma_pos = url.find(',');
        if (comma_pos == std::string::npos || comma_pos + 1 >= url.size()) return result;

        std::string metadata = to_lower(url.substr(5, comma_pos - 5));
        std::string payload = url.substr(comma_pos + 1);

        bool is_base64 = (metadata.find("base64") != std::string::npos);
        bool is_svg = (metadata.find("image/svg") != std::string::npos);

        // Handle SVG data: URIs
        if (is_svg) {
            std::string svg_text;
            if (is_base64) {
                std::vector<uint8_t> decoded;
                if (!base64_decode_bytes(payload, decoded) || decoded.empty()) return result;
                svg_text.assign(decoded.begin(), decoded.end());
            } else {
                svg_text = payload;
            }
            result = decode_svg_image(svg_text.c_str(), svg_text.size());
            if (result.pixels) image_cache_store(url, result);
            return result;
        }

        std::vector<uint8_t> raw_bytes;
        if (is_base64) {
            if (!base64_decode_bytes(payload, raw_bytes) || raw_bytes.empty()) return result;
        } else {
            // URL-encoded data
            raw_bytes.assign(payload.begin(), payload.end());
        }

#ifdef __APPLE__
        result = decode_image_native(raw_bytes.data(), raw_bytes.size());
        if (result.pixels) {
            image_cache_store(url, result);
            return result;
        }
#endif
        int w = 0, h = 0, channels = 0;
        unsigned char* data = stbi_load_from_memory(
            raw_bytes.data(), static_cast<int>(raw_bytes.size()),
            &w, &h, &channels, 4);
        if (data) {
            result.width = w;
            result.height = h;
            result.pixels = std::make_shared<std::vector<uint8_t>>(data, data + w * h * 4);
            stbi_image_free(data);
            image_cache_store(url, result);
        }
        return result;
    }

    auto response = fetch_with_redirects(url, "image/*", 10);
    if (!response || response->status >= 400) return result;

    const auto& body = response->body;
    if (body.empty()) return result;

    // Check for SVG: by URL extension or by content sniffing (starts with "<svg" or "<?xml")
    {
        bool is_svg = false;
        // Check URL extension
        std::string url_lower = to_lower(url);
        auto q_pos = url_lower.find('?');
        std::string path_part = (q_pos != std::string::npos) ? url_lower.substr(0, q_pos) : url_lower;
        if (path_part.size() >= 4 && path_part.substr(path_part.size() - 4) == ".svg") {
            is_svg = true;
        }
        // Check content-type header
        if (!is_svg) {
            std::string ct = to_lower(response->headers.get("content-type").value_or(""));
            if (ct.find("image/svg") != std::string::npos) is_svg = true;
        }
        // Content sniff: look for SVG markers
        if (!is_svg && body.size() >= 4) {
            size_t sniff_len = std::min(body.size(), size_t(256));
            std::string start(body.begin(), body.begin() + static_cast<ptrdiff_t>(sniff_len));
            std::string start_lower = to_lower(start);
            if (start_lower.find("<svg") != std::string::npos ||
                (start_lower.find("<?xml") != std::string::npos && start_lower.find("<svg") != std::string::npos)) {
                is_svg = true;
            }
        }
        if (is_svg) {
            std::string svg_text(body.begin(), body.end());
            result = decode_svg_image(svg_text.c_str(), svg_text.size());
            if (result.pixels) {
                image_cache_store(url, result);
                return result;
            }
        }
    }

#ifdef __APPLE__
    // Try native macOS decoder first (supports WebP, HEIC, TIFF, ICO, etc.)
    result = decode_image_native(
        reinterpret_cast<const uint8_t*>(body.data()), body.size());
    if (result.pixels) {
        image_cache_store(url, result);
        return result;
    }
#endif

    // Fallback to stb_image (JPEG, PNG, GIF, BMP)
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(body.data()),
        static_cast<int>(body.size()),
        &w, &h, &channels, 4  // force RGBA
    );

    if (!data) return result;

    result.width = w;
    result.height = h;
    result.pixels = std::make_shared<std::vector<uint8_t>>(data, data + w * h * 4);
    stbi_image_free(data);

    // Cache the decoded result
    image_cache_store(url, result);

    return result;
}

std::string normalize_mime_type(const std::string& raw_type) {
    std::string type = to_lower(trim(raw_type));
    const size_t semicolon = type.find(';');
    if (semicolon != std::string::npos) {
        type = trim(type.substr(0, semicolon));
    }
    return type;
}

bool media_targets_screen(const std::string& raw_media) {
    const std::string media = to_lower(trim(raw_media));
    if (media.empty() || media == "all" || media == "screen") return true;
    if (media.find("screen") != std::string::npos) return true;
    if (media.find("print") != std::string::npos) return false;
    if (media.find("speech") != std::string::npos) return false;
    // Be permissive for unhandled media-query syntax rather than dropping styles.
    return true;
}

bool is_in_inert_subtree(const clever::html::SimpleNode* node) {
    if (!node) return false;
    const clever::html::SimpleNode* cur = node->parent;
    while (cur) {
        if (cur->type == clever::html::SimpleNode::Element) {
            const std::string tag = to_lower(cur->tag_name);
            if (tag == "template") return true;
            if (tag == "noscript" && !g_noscript_fallback) return true;
        }
        cur = cur->parent;
    }
    return false;
}

// Extract external stylesheet URLs from <link> elements
std::vector<std::string> extract_link_stylesheets(
    const clever::html::SimpleNode& node, const std::string& base_url) {
    std::vector<std::string> urls;
    std::unordered_set<std::string> seen_urls;

    auto visit = [&](const auto& self, const clever::html::SimpleNode& current) -> void {
        if (current.type == clever::html::SimpleNode::Element) {
            const std::string tag = to_lower(current.tag_name);

            // Skip template always; skip noscript only when JS is active
            if (tag == "template") return;
            if (tag == "noscript" && !g_noscript_fallback) return;

            if (tag == "link") {
                const std::string rel_raw = to_lower(get_attr(current, "rel"));
                const std::string href = trim(get_attr(current, "href"));
                const std::string type = normalize_mime_type(get_attr(current, "type"));
                const std::string media = get_attr(current, "media");
                const bool disabled = has_attr(current, "disabled");

                bool has_stylesheet = false;
                bool has_alternate = false;
                std::istringstream rel_tokens(rel_raw);
                std::string token;
                while (rel_tokens >> token) {
                    if (token == "stylesheet") has_stylesheet = true;
                    if (token == "alternate") has_alternate = true;
                }

                // Treat rel as a token list, skip alternate/disabled sheets.
                if (has_stylesheet && !has_alternate && !disabled &&
                    (type.empty() || type == "text/css") &&
                    media_targets_screen(media) &&
                    !href.empty()) {
                    std::string resolved = resolve_url(href, base_url);
                    if (!resolved.empty() && seen_urls.insert(resolved).second) {
                        urls.push_back(std::move(resolved));
                    }
                }
            }
        }

        for (auto& child : current.children) {
            self(self, *child);
        }
    };

    visit(visit, node);
    return urls;
}

// ---- CSS <style> extraction ----

// Collect CSS text from all <style> elements in the document
std::string extract_style_content(const clever::html::SimpleNode& node) {
    std::string css;
    if (node.type == clever::html::SimpleNode::Element) {
        const std::string tag = to_lower(node.tag_name);

        // Skip template always; skip noscript only when JS is active
        if (tag == "template") return css;
        if (tag == "noscript" && !g_noscript_fallback) return css;

        if (tag == "style") {
            const std::string type = normalize_mime_type(get_attr(node, "type"));
            const std::string media = get_attr(node, "media");
            const bool css_type = type.empty() || type == "text/css";
            const bool media_matches = media_targets_screen(media);
            if (css_type && media_matches) {
                css += node.text_content();
                css += "\n";
            }
        }
    }
    for (auto& child : node.children) {
        css += extract_style_content(*child);
    }
    return css;
}

// ---- ElementView builder for CSS matching ----

struct ElementViewTree {
    std::vector<std::unique_ptr<clever::css::ElementView>> views;

    // Build an ElementView for a SimpleNode. Caller manages sibling info.
    clever::css::ElementView* build(const clever::html::SimpleNode& node,
                                     clever::css::ElementView* parent,
                                     size_t child_index,
                                     size_t sibling_count,
                                     clever::css::ElementView* prev_sibling) {
        if (node.type != clever::html::SimpleNode::Element) return nullptr;

        auto view = std::make_unique<clever::css::ElementView>();
        view->tag_name = to_lower(node.tag_name);
        view->id = get_attr(node, "id");
        view->parent = parent;
        view->child_index = child_index;
        view->sibling_count = sibling_count;
        view->prev_sibling = prev_sibling;

        // Parse class attribute
        std::string class_attr = get_attr(node, "class");
        if (!class_attr.empty()) {
            std::istringstream iss(class_attr);
            std::string cls;
            while (iss >> cls) {
                view->classes.push_back(cls);
            }
        }

        // Copy all attributes
        for (auto& attr : node.attributes) {
            view->attributes.emplace_back(attr.name, attr.value);
        }

        // Count element children and text children for :empty pseudo-class
        size_t elem_children = 0;
        bool has_text = false;
        for (auto& child : node.children) {
            if (child->type == clever::html::SimpleNode::Element) {
                elem_children++;
            } else if (child->type == clever::html::SimpleNode::Text) {
                // CSS :empty treats any text node as content, including whitespace-only text.
                has_text = true;
            }
        }
        view->child_element_count = elem_children;
        view->has_text_children = has_text;

        auto* ptr = view.get();
        views.push_back(std::move(view));
        return ptr;
    }
};

// ---- Form data collection ----

// Collect name=value pairs from all <input> elements in a form
std::string build_form_query_string(const clever::html::SimpleNode& form_node) {
    std::string query;
    std::function<void(const clever::html::SimpleNode&)> collect =
        [&](const clever::html::SimpleNode& n) {
            if (n.type == clever::html::SimpleNode::Element) {
                std::string tag = to_lower(n.tag_name);
                if (tag == "input") {
                    std::string name = get_attr(n, "name");
                    std::string type = to_lower(get_attr(n, "type"));
                    if (type.empty()) type = "text";
                    // Skip submit/button/reset/checkbox/radio without checked
                    if (!name.empty() && type != "submit" && type != "button" && type != "reset") {
                        std::string value = get_attr(n, "value");
                        if (!query.empty()) query += "&";
                        // Simple URL encoding (just spaces and &)
                        for (char c : name) {
                            if (c == ' ') query += "+";
                            else if (c == '&') query += "%26";
                            else if (c == '=') query += "%3D";
                            else query += c;
                        }
                        query += "=";
                        for (char c : value) {
                            if (c == ' ') query += "+";
                            else if (c == '&') query += "%26";
                            else if (c == '=') query += "%3D";
                            else query += c;
                        }
                    }
                }
            }
            for (auto& child : n.children) {
                collect(*child);
            }
        };
    collect(form_node);
    return query;
}


// ---- Enhanced layout tree builder with StyleResolver ----

// Recursively build LayoutNode tree with full CSS cascade
std::unique_ptr<clever::layout::LayoutNode> build_layout_tree_styled(
    const clever::html::SimpleNode& node,
    const clever::css::ComputedStyle& parent_style,
    clever::css::StyleResolver& resolver,
    ElementViewTree& view_tree,
    clever::css::ElementView* parent_view,
    const std::string& base_url = "",
    const std::string& current_link = "",
    const clever::html::SimpleNode* current_form = nullptr,
    const std::string& current_link_target = "") {

    // Use the auto margin sentinel from box.h
    using clever::layout::MARGIN_AUTO;

    // Guard against deeply nested DOM trees causing stack overflow.
    // Each frame holds a ComputedStyle (~3.7 KB) plus other local variables;
    // capping at 64 keeps peak stack usage well under 1 MB while still handling
    // deeply-nested real-world HTML.
    static constexpr int kMaxTreeDepth = 64;
    thread_local int tree_depth = 0;
    if (tree_depth >= kMaxTreeDepth) return nullptr;
    struct DepthGuard { DepthGuard() { ++tree_depth; } ~DepthGuard() { --tree_depth; } } dg;

    auto layout_node = std::make_unique<clever::layout::LayoutNode>();
    float parent_font_size = parent_style.font_size.to_px(16.0f);

    if (node.type == clever::html::SimpleNode::Text) {
        layout_node->is_text = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = parent_font_size;
        layout_node->font_family = parent_style.font_family;
        layout_node->is_monospace = (parent_style.font_family == "monospace");
        layout_node->color = color_to_argb(parent_style.color);
        layout_node->font_weight = parent_style.font_weight;
        layout_node->font_italic = (parent_style.font_style != clever::css::FontStyle::Normal);
        layout_node->line_height = (parent_font_size > 0.0f)
            ? parent_style.line_height.to_px(parent_font_size) / parent_font_size
            : 1.2f;
        layout_node->letter_spacing = parent_style.letter_spacing.to_px(parent_font_size);
        layout_node->word_spacing = parent_style.word_spacing.to_px(parent_font_size);
        // Text transform from parent (inherited)
        layout_node->text_transform = static_cast<int>(parent_style.text_transform);
        // Text decoration from parent
        switch (parent_style.text_decoration) {
            case clever::css::TextDecoration::Underline: layout_node->text_decoration = 1; break;
            case clever::css::TextDecoration::LineThrough: layout_node->text_decoration = 2; break;
            case clever::css::TextDecoration::Overline: layout_node->text_decoration = 3; break;
            default: layout_node->text_decoration = 0; break;
        }
        layout_node->text_decoration_bits = parent_style.text_decoration_bits;
        layout_node->text_decoration_color = color_to_argb(parent_style.text_decoration_color);
        layout_node->text_decoration_style = static_cast<int>(parent_style.text_decoration_style);
        layout_node->text_decoration_thickness = parent_style.text_decoration_thickness;
        layout_node->pointer_events = static_cast<int>(parent_style.pointer_events);
        layout_node->user_select = static_cast<int>(parent_style.user_select);
        layout_node->tab_size = parent_style.tab_size;
        layout_node->line_clamp = parent_style.line_clamp;
        layout_node->link_href = current_link;
        layout_node->link_target = current_link_target;
        // Text nodes inherit the accumulated opacity from their parent element
        layout_node->opacity = parent_style.opacity;
        layout_node->visibility_hidden = (parent_style.visibility == clever::css::Visibility::Hidden ||
                                          parent_style.visibility == clever::css::Visibility::Collapse);
        layout_node->visibility_collapse = (parent_style.visibility == clever::css::Visibility::Collapse);
        layout_node->word_break = parent_style.word_break;
        layout_node->overflow_wrap = parent_style.overflow_wrap;
        layout_node->text_wrap = parent_style.text_wrap;
        layout_node->white_space_collapse = parent_style.white_space_collapse;
        layout_node->line_break = parent_style.line_break;
        layout_node->math_style = parent_style.math_style;
        layout_node->math_depth = parent_style.math_depth;
        layout_node->orphans = parent_style.orphans;
        layout_node->widows = parent_style.widows;
        layout_node->column_span = parent_style.column_span;
        layout_node->break_before = parent_style.break_before;
        layout_node->break_after = parent_style.break_after;
        layout_node->break_inside = parent_style.break_inside;
        layout_node->page_break_before = parent_style.page_break_before;
        layout_node->page_break_after = parent_style.page_break_after;
        layout_node->page_break_inside = parent_style.page_break_inside;
        layout_node->page = parent_style.page;
        layout_node->hyphens = parent_style.hyphens;
        layout_node->text_justify = parent_style.text_justify;
        layout_node->text_underline_offset = parent_style.text_underline_offset;
        layout_node->text_underline_position = parent_style.text_underline_position;
        layout_node->font_variant = parent_style.font_variant;
        layout_node->font_variant_caps = parent_style.font_variant_caps;
        layout_node->font_variant_numeric = parent_style.font_variant_numeric;
        layout_node->font_synthesis = parent_style.font_synthesis;
        layout_node->font_variant_alternates = parent_style.font_variant_alternates;
        layout_node->font_feature_settings = parent_style.font_feature_settings;
        layout_node->font_variation_settings = parent_style.font_variation_settings;
        layout_node->font_optical_sizing = parent_style.font_optical_sizing;
        layout_node->print_color_adjust = parent_style.print_color_adjust;
        layout_node->image_orientation = parent_style.image_orientation;
        layout_node->image_orientation_explicit = parent_style.image_orientation_explicit;
        layout_node->font_kerning = parent_style.font_kerning;
        layout_node->font_variant_ligatures = parent_style.font_variant_ligatures;
        layout_node->font_variant_east_asian = parent_style.font_variant_east_asian;
        layout_node->font_palette = parent_style.font_palette;
        layout_node->font_variant_position = parent_style.font_variant_position;
        layout_node->font_language_override = parent_style.font_language_override;
        layout_node->font_size_adjust = parent_style.font_size_adjust;
        layout_node->font_stretch = parent_style.font_stretch;
        layout_node->text_decoration_skip_ink = parent_style.text_decoration_skip_ink;
        layout_node->text_emphasis_style = parent_style.text_emphasis_style;
        layout_node->text_emphasis_color = parent_style.text_emphasis_color;
        layout_node->text_stroke_width = parent_style.text_stroke_width;
        layout_node->text_stroke_color = color_to_argb(parent_style.text_stroke_color);
        if (parent_style.text_fill_color.a > 0) {
            layout_node->text_fill_color = color_to_argb(parent_style.text_fill_color);
        }
        layout_node->hanging_punctuation = parent_style.hanging_punctuation;
        layout_node->ruby_align = parent_style.ruby_align;
        layout_node->ruby_position = parent_style.ruby_position;
        layout_node->ruby_overhang = parent_style.ruby_overhang;
        layout_node->text_orientation = parent_style.text_orientation;
        layout_node->writing_mode = parent_style.writing_mode;
        layout_node->direction = (parent_style.direction == clever::css::Direction::Rtl) ? 1 : 0;
        layout_node->quotes = parent_style.quotes;
        layout_node->text_rendering = parent_style.text_rendering;
        layout_node->font_smooth = parent_style.font_smooth;
        layout_node->text_size_adjust = parent_style.text_size_adjust;
        layout_node->caret_color = color_to_argb(parent_style.caret_color);
        layout_node->accent_color = parent_style.accent_color;
        layout_node->color_interpolation = parent_style.color_interpolation;
        // Text shadow from parent (inherited for text nodes)
        layout_node->text_shadow_offset_x = parent_style.text_shadow_offset_x;
        layout_node->text_shadow_offset_y = parent_style.text_shadow_offset_y;
        layout_node->text_shadow_blur = parent_style.text_shadow_blur;
        layout_node->text_shadow_color = color_to_argb(parent_style.text_shadow_color);
        // Multiple text shadows from parent (inherited for text nodes)
        layout_node->text_shadows.clear();
        for (auto& ts : parent_style.text_shadows) {
            clever::layout::LayoutNode::TextShadowEntry e;
            e.offset_x = ts.offset_x;
            e.offset_y = ts.offset_y;
            e.blur = ts.blur;
            e.color = color_to_argb(ts.color);
            layout_node->text_shadows.push_back(e);
        }

        // Inherit white-space mode from parent
        if (parent_style.white_space == clever::css::WhiteSpace::Pre) {
            layout_node->white_space = 2;
            layout_node->white_space_pre = true;
            layout_node->white_space_nowrap = true;
        } else if (parent_style.white_space == clever::css::WhiteSpace::PreWrap) {
            layout_node->white_space = 3;
            layout_node->white_space_pre = true;
        } else if (parent_style.white_space == clever::css::WhiteSpace::NoWrap) {
            layout_node->white_space = 1;
            layout_node->white_space_nowrap = true;
        } else if (parent_style.white_space == clever::css::WhiteSpace::PreLine) {
            layout_node->white_space = 4;
        } else if (parent_style.white_space == clever::css::WhiteSpace::BreakSpaces) {
            layout_node->white_space = 5;
            layout_node->white_space_pre = true; // preserve whitespace
        }

        // Preserve whitespace for pre-formatted text
        if (parent_style.white_space == clever::css::WhiteSpace::Pre ||
            parent_style.white_space == clever::css::WhiteSpace::PreWrap ||
            parent_style.white_space == clever::css::WhiteSpace::BreakSpaces) {
            layout_node->text_content = node.data;
        } else if (parent_style.white_space == clever::css::WhiteSpace::PreLine) {
            // pre-line: collapse spaces and tabs, but preserve newlines
            std::string text = node.data;
            std::string collapsed;
            bool last_was_space = false;
            for (char c : text) {
                if (c == '\r') continue; // skip carriage returns
                if (c == '\n') {
                    // Preserve newlines; reset space collapsing
                    collapsed += '\n';
                    last_was_space = false;
                } else if (c == '\t') {
                    // Collapse tabs to space
                    if (!last_was_space) {
                        collapsed += ' ';
                        last_was_space = true;
                    }
                } else if (c == ' ') {
                    if (!last_was_space) {
                        collapsed += ' ';
                        last_was_space = true;
                    }
                } else {
                    collapsed += c;
                    last_was_space = false;
                }
            }
            layout_node->text_content = collapsed;
        } else {
            // Normal / nowrap mode: collapse whitespace
            std::string text = node.data;
            // Replace newlines/tabs with spaces, collapse multiple spaces
            std::string collapsed;
            bool last_was_space = false;
            for (char c : text) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                if (c == ' ') {
                    if (!last_was_space) {
                        collapsed += ' ';
                        last_was_space = true;
                    }
                } else {
                    collapsed += c;
                    last_was_space = false;
                }
            }
            layout_node->text_content = collapsed;

            // Suppress whitespace-only text nodes between block-level elements
            // Only suppress when BOTH adjacent siblings (prev and next) are blocks.
            // Whitespace between inline elements must be preserved (e.g., <a>foo</a> <span>bar</span>).
            if (std::all_of(collapsed.begin(), collapsed.end(),
                            [](char c) { return c == ' '; }) && node.parent) {
                static const std::unordered_set<std::string> block_tags = {
                    "div", "p", "section", "article", "aside", "nav", "header",
                    "footer", "main", "blockquote", "pre", "figure", "ul", "ol",
                    "li", "h1", "h2", "h3", "h4", "h5", "h6", "table", "tr",
                    "td", "th", "form", "fieldset", "dl", "dd", "dt", "hr",
                    "details", "summary", "address", "noscript", "html", "body",
                    "search", "menu"
                };
                // Find this node's index in parent's children
                int my_idx = -1;
                auto& children = node.parent->children;
                for (int i = 0; i < (int)children.size(); i++) {
                    if (children[i].get() == &node) { my_idx = i; break; }
                }
                if (my_idx >= 0) {
                    // Check previous non-whitespace-text sibling
                    bool prev_is_block = false;
                    for (int i = my_idx - 1; i >= 0; i--) {
                        auto& sib = children[i];
                        if (sib->type == clever::html::SimpleNode::Element) {
                            prev_is_block = block_tags.count(to_lower(sib->tag_name)) > 0;
                            break;
                        }
                        // Skip other whitespace-only text nodes
                        if (sib->type == clever::html::SimpleNode::Text) {
                            bool all_ws = std::all_of(sib->data.begin(), sib->data.end(),
                                [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; });
                            if (!all_ws) break; // Non-whitespace text = inline content
                        }
                    }
                    // Check next non-whitespace-text sibling
                    bool next_is_block = false;
                    for (int i = my_idx + 1; i < (int)children.size(); i++) {
                        auto& sib = children[i];
                        if (sib->type == clever::html::SimpleNode::Element) {
                            next_is_block = block_tags.count(to_lower(sib->tag_name)) > 0;
                            break;
                        }
                        if (sib->type == clever::html::SimpleNode::Text) {
                            bool all_ws = std::all_of(sib->data.begin(), sib->data.end(),
                                [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; });
                            if (!all_ws) break;
                        }
                    }
                    // Also suppress if at start/end of a block container with block children
                    if (my_idx == 0) prev_is_block = true; // leading whitespace in block
                    if (my_idx == (int)children.size() - 1) next_is_block = true; // trailing
                    if (prev_is_block && next_is_block) {
                        return nullptr; // Skip inter-block whitespace
                    }
                }
            }
        }

        // Apply text-transform
        if (parent_style.text_transform == clever::css::TextTransform::Uppercase) {
            auto& t = layout_node->text_content;
            std::transform(t.begin(), t.end(), t.begin(), ::toupper);
        } else if (parent_style.text_transform == clever::css::TextTransform::Lowercase) {
            auto& t = layout_node->text_content;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        } else if (parent_style.text_transform == clever::css::TextTransform::Capitalize) {
            auto& t = layout_node->text_content;
            bool cap_next = true;
            for (size_t i = 0; i < t.size(); i++) {
                if (t[i] == ' ') { cap_next = true; }
                else if (cap_next) { t[i] = static_cast<char>(::toupper(t[i])); cap_next = false; }
            }
        }

        return layout_node;
    }

    if (node.type == clever::html::SimpleNode::Comment ||
        node.type == clever::html::SimpleNode::DocumentType) {
        return nullptr;
    }

    if (node.type == clever::html::SimpleNode::Document) {
        layout_node->tag_name = "#document";
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->background_color = 0xFFFFFFFF;

        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, parent_style, resolver, view_tree, parent_view, base_url, current_link, current_form, current_link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Skip non-renderable elements
    std::string tag_lower = to_lower(node.tag_name);
    if (tag_lower == "head" || tag_lower == "meta" || tag_lower == "title" ||
        tag_lower == "link" || tag_lower == "script" || tag_lower == "style" ||
        tag_lower == "template" ||
        (tag_lower == "noscript" && !g_noscript_fallback)) {
        return nullptr;
    }

    layout_node->tag_name = node.tag_name;
    layout_node->element_id = get_attr(node, "id");
    layout_node->dom_node = const_cast<clever::html::SimpleNode*>(&node);

    // Copy CSS classes for container query post-layout matching
    {
        std::string class_attr = get_attr(node, "class");
        if (!class_attr.empty()) {
            std::istringstream cls_iss(class_attr);
            std::string cls;
            while (cls_iss >> cls) {
                layout_node->css_classes.push_back(cls);
            }
        }
    }

    // Note: <noscript> is skipped above (JS engine is enabled)

    // Handle <slot> — Web Components slot placeholder; render inline with fallback children
    if (tag_lower == "slot") {
        layout_node->is_slot = true;
        layout_node->slot_name = get_attr(node, "name");
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
    }

    // Handle <br> as a line break (newline text node)
    if (tag_lower == "br") {
        layout_node->is_text = true;
        layout_node->text_content = "\n";
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = parent_style.font_size.to_px(16.0f);
        // HTML <br clear="left|right|all"> attribute (legacy float clearing)
        std::string br_clear = get_attr(node, "clear");
        if (!br_clear.empty()) {
            std::string cl;
            cl.reserve(br_clear.size());
            for (char c : br_clear) cl += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (cl == "left") layout_node->clear_type = 1;
            else if (cl == "right") layout_node->clear_type = 2;
            else if (cl == "all" || cl == "both") layout_node->clear_type = 3;
        }
        return layout_node;
    }

    // Handle <hr> as a horizontal rule
    if (tag_lower == "hr") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->specified_height = 0;
        layout_node->geometry.margin = {8, 0, 8, 0};
        // Inset-style border: 1px solid top (#ccc) + 1px solid bottom (#eee)
        layout_node->geometry.border.top = 1;
        layout_node->geometry.border.bottom = 1;
        layout_node->border_color = 0xFFCCCCCC;
        layout_node->border_color_top = 0xFFCCCCCC;
        layout_node->border_color_bottom = 0xFFEEEEEE;
        layout_node->border_color_left = 0xFFCCCCCC;
        layout_node->border_color_right = 0xFFCCCCCC;
        layout_node->border_style = 1; // solid
        layout_node->border_style_top = 1;
        layout_node->border_style_bottom = 1;
        // Legacy HTML attributes
        std::string hr_color = get_attr(node, "color");
        if (!hr_color.empty()) {
            uint32_t hrc = parse_html_color_attr(hr_color);
            if (hrc != 0) layout_node->border_color = hrc;
        }
        std::string hr_size = get_attr(node, "size");
        if (!hr_size.empty()) {
            try {
                float sz = std::stof(hr_size);
                if (sz >= 1) {
                    layout_node->geometry.border.top = sz;
                    layout_node->specified_height = 0;
                }
            } catch (...) {}
        }
        std::string hr_width = get_attr(node, "width");
        if (!hr_width.empty()) {
            try {
                if (hr_width.back() == '%') {
                    // percentage width handled by setting max-width later
                } else {
                    layout_node->specified_width = std::stof(hr_width);
                }
            } catch (...) {}
        }
        bool hr_noshade = has_attr(node, "noshade");
        if (hr_noshade) {
            layout_node->background_color = layout_node->border_color;
        }
        return layout_node;
    }

    // Handle <wbr> as a word break opportunity (zero-width inline marker)
    if (tag_lower == "wbr") {
        layout_node->is_wbr = true;
        layout_node->is_text = true;
        layout_node->text_content = "\u200B"; // zero-width space
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        // Zero width — it's just a break opportunity marker
        layout_node->geometry.width = 0;
        layout_node->geometry.height = 0;
        return layout_node;
    }

    // Count element siblings for CSS matching
    size_t child_index = 0;
    size_t sibling_count = 0;
    size_t same_type_index = 0;
    size_t same_type_count = 0;
    clever::css::ElementView* prev_elem_view = nullptr;
    if (node.parent) {
        std::string our_tag = to_lower(node.tag_name);
        for (auto& sibling : node.parent->children) {
            if (sibling->type == clever::html::SimpleNode::Element) {
                sibling_count++;
                if (to_lower(sibling->tag_name) == our_tag)
                    same_type_count++;
            }
        }
        size_t idx = 0;
        size_t type_idx = 0;
        const clever::html::SimpleNode* prev_elem_node = nullptr;
        for (auto& sibling : node.parent->children) {
            if (sibling.get() == &node) {
                child_index = idx;
                same_type_index = type_idx;
                break;
            }
            if (sibling->type == clever::html::SimpleNode::Element) {
                prev_elem_node = sibling.get();
                idx++;
                if (to_lower(sibling->tag_name) == our_tag)
                    type_idx++;
            }
        }
        // Find the ElementView for the previous element sibling
        if (prev_elem_node) {
            for (auto& v : view_tree.views) {
                if (v->tag_name == to_lower(prev_elem_node->tag_name) &&
                    v->parent == parent_view &&
                    v->child_index == child_index - 1) {
                    prev_elem_view = v.get();
                    break;
                }
            }
        }
    }

    // Build ElementView for CSS matching
    auto* elem_view = view_tree.build(node, parent_view,
                                       child_index, sibling_count, prev_elem_view);
    if (elem_view) {
        elem_view->same_type_index = same_type_index;
        elem_view->same_type_count = same_type_count;
        // Link child to parent's children list (for :has() selector)
        if (parent_view) {
            parent_view->children.push_back(elem_view);
        }
        // Pre-build child ElementViews recursively for :has() selector matching
        // These need to exist before we resolve this element's style
        static constexpr int kMaxViewTreeDepth = 64;
        std::function<void(const clever::html::SimpleNode&, clever::css::ElementView*, int)> pre_build_views;
        pre_build_views = [&](const clever::html::SimpleNode& parent_node,
                              clever::css::ElementView* pview, int depth) {
            if (depth >= kMaxViewTreeDepth) return;
            size_t cec = 0;
            for (auto& c : parent_node.children) {
                if (c->type == clever::html::SimpleNode::Element) cec++;
            }
            size_t ci = 0;
            clever::css::ElementView* pcv = nullptr;
            for (auto& c : parent_node.children) {
                if (c->type != clever::html::SimpleNode::Element) continue;
                auto* cv = view_tree.build(*c, pview, ci, cec, pcv);
                if (cv) {
                    std::string ctag = to_lower(c->tag_name);
                    size_t stc = 0, sti = 0, ti = 0;
                    for (auto& s : parent_node.children) {
                        if (s->type == clever::html::SimpleNode::Element &&
                            to_lower(s->tag_name) == ctag) stc++;
                    }
                    for (auto& s : parent_node.children) {
                        if (s.get() == c.get()) { sti = ti; break; }
                        if (s->type == clever::html::SimpleNode::Element &&
                            to_lower(s->tag_name) == ctag) ti++;
                    }
                    cv->same_type_index = sti;
                    cv->same_type_count = stc;
                    pview->children.push_back(cv);
                    pcv = cv;
                    // Recurse into children
                    pre_build_views(*c, cv, depth + 1);
                }
                ci++;
            }
        };
        pre_build_views(node, elem_view, 0);
    }

    // Resolve style through the full cascade
    clever::css::ComputedStyle style;
    if (elem_view) {
        style = resolver.resolve(*elem_view, parent_style);
    } else {
        style = clever::css::default_style_for_tag(node.tag_name);
        style.z_index = clever::layout::Z_INDEX_AUTO;
    }
    // CSS custom properties are inherited by default. Seed this element's map
    // from the parent style, while preserving any properties resolved on self.
    for (const auto& [name, custom_value] : parent_style.custom_properties) {
        style.custom_properties.try_emplace(name, custom_value);
    }

    // Apply inline style (highest priority, overrides cascade)
    std::string style_attr = get_attr(node, "style");
    if (!style_attr.empty()) {
        apply_inline_style(style, style_attr, &parent_style);
    }

    // Check HTML boolean attributes
    auto has_bool_attr = [&](const std::string& name) {
        return std::any_of(node.attributes.begin(), node.attributes.end(),
            [&](const auto& a) { return a.name == name; });
    };
    // HTML `hidden` attribute → display: none
    if (has_bool_attr("hidden")) {
        style.display = clever::css::Display::None;
    }
    // HTML `popover` attribute → hidden by default unless showPopover() was called
    if (has_bool_attr("popover")) {
        bool popover_showing = std::any_of(node.attributes.begin(), node.attributes.end(),
            [](const auto& a) { return a.name == "data-popover-showing"; });
        if (!popover_showing) {
            style.display = clever::css::Display::None;
        }
    }
    // HTML `inert` attribute → pointer-events: none, user-select: none
    if (has_bool_attr("inert")) {
        style.pointer_events = clever::css::PointerEvents::None;
        style.user_select = clever::css::UserSelect::None;
    }

    // Process CSS counter-reset and counter-increment
    process_css_counters(style);

    if (style.display == clever::css::Display::None) {
        return nullptr;
    }

    // display: contents — element itself doesn't generate a box,
    // children are rendered as if they were direct children of the parent
    if (style.display == clever::css::Display::Contents && node.type == clever::html::SimpleNode::Type::Element) {
        // Build a transparent wrapper that passes children through
        // Use the parent style as the inherited style for children
        auto wrapper = std::make_unique<clever::layout::LayoutNode>();
        wrapper->tag_name = tag_lower;
        wrapper->display_contents = true;
        // Process children with parent's style as context
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, parent_style, resolver, view_tree, parent_view, base_url, current_link, current_form, current_link_target);
            if (child_layout) {
                wrapper->append_child(std::move(child_layout));
            }
        }
        return wrapper;
    }

    // Determine link context for this node and descendants
    std::string link = current_link;
    std::string link_target = current_link_target;
    if (tag_lower == "a") {
        std::string href = get_attr(node, "href");
        if (!href.empty()) {
            link = resolve_url(href, base_url);
            link_target = get_attr(node, "target");
            // Default link styling: blue color (unless color explicitly set via CSS)
            bool color_is_default = (style.color.r == 0 && style.color.g == 0 &&
                                      style.color.b == 0 && style.color.a == 255);
            if (color_is_default) {
                style.color = {0, 0, 238, 255}; // standard link blue #0000EE
            }
            // Default text-decoration: underline for links
            if (style.text_decoration == clever::css::TextDecoration::None) {
                style.text_decoration = clever::css::TextDecoration::Underline;
            }
        }
    }

    // Track form context for form submission
    const clever::html::SimpleNode* form = current_form;
    if (tag_lower == "form") {
        form = &node;

        // Collect form metadata for POST submission support
        clever::paint::FormData form_data;
        {
            std::string raw_action = get_attr(node, "action");
            form_data.action = raw_action.empty() ? base_url : resolve_url(raw_action, base_url);
        }
        form_data.method = to_lower(get_attr(node, "method"));
        if (form_data.method.empty()) form_data.method = "get";
        form_data.enctype = get_attr(node, "enctype");
        if (form_data.enctype.empty()) form_data.enctype = "application/x-www-form-urlencoded";

        // Recursively collect fields from this form's DOM subtree
        std::function<void(const clever::html::SimpleNode&)> collect_fields =
            [&](const clever::html::SimpleNode& n) {
                if (n.type == clever::html::SimpleNode::Element) {
                    std::string child_tag = to_lower(n.tag_name);
                    if (child_tag == "input") {
                        clever::paint::FormField field;
                        field.name = get_attr(n, "name");
                        field.type = to_lower(get_attr(n, "type"));
                        if (field.type.empty()) field.type = "text";
                        field.value = get_attr(n, "value");
                        field.checked = !get_attr(n, "checked").empty();
                        form_data.fields.push_back(field);
                    } else if (child_tag == "textarea") {
                        clever::paint::FormField field;
                        field.name = get_attr(n, "name");
                        field.type = "textarea";
                        // Textarea value is its text content
                        for (auto& tc : n.children) {
                            if (tc->type == clever::html::SimpleNode::Text) {
                                field.value += tc->data;
                            }
                        }
                        form_data.fields.push_back(field);
                    } else if (child_tag == "select") {
                        clever::paint::FormField field;
                        field.name = get_attr(n, "name");
                        field.type = "select";
                        // Helper to process a single <option> for form value
                        auto form_process_option = [&](const clever::html::SimpleNode& opt_node) {
                            std::string sel = get_attr(opt_node, "selected");
                            if (!sel.empty() || field.value.empty()) {
                                std::string val = get_attr(opt_node, "value");
                                if (val.empty()) {
                                    for (auto& tc : opt_node.children) {
                                        if (tc->type == clever::html::SimpleNode::Text) {
                                            val += trim(tc->data);
                                        }
                                    }
                                }
                                field.value = val;
                            }
                        };
                        // Find the selected (or first) option value,
                        // including options inside <optgroup> elements
                        bool found_selected = false;
                        for (auto& opt : n.children) {
                            if (opt->type != clever::html::SimpleNode::Element) continue;
                            std::string opt_tag = to_lower(opt->tag_name);
                            if (opt_tag == "option") {
                                form_process_option(*opt);
                                if (!get_attr(*opt, "selected").empty()) { found_selected = true; break; }
                            } else if (opt_tag == "optgroup") {
                                for (auto& og_child : opt->children) {
                                    if (og_child->type == clever::html::SimpleNode::Element &&
                                        to_lower(og_child->tag_name) == "option") {
                                        form_process_option(*og_child);
                                        if (!get_attr(*og_child, "selected").empty()) { found_selected = true; break; }
                                    }
                                }
                                if (found_selected) break;
                            }
                        }
                        form_data.fields.push_back(field);
                    }
                }
                for (auto& child : n.children) {
                    collect_fields(*child);
                }
            };
        // Collect from form's children (not the form node itself)
        for (auto& child : node.children) {
            collect_fields(*child);
        }
        collected_forms.push_back(std::move(form_data));
    }

    layout_node->link_href = link;
    layout_node->link_target = link_target;

    // Convert CSS style to layout properties
    layout_node->mode = display_to_mode(style.display);
    layout_node->display = display_to_type(style.display);

    float font_size = style.font_size.to_px(parent_font_size);
    layout_node->font_size = font_size;
    layout_node->font_family = style.font_family;
    layout_node->font_weight = style.font_weight;
    layout_node->font_italic = (style.font_style != clever::css::FontStyle::Normal);
    layout_node->line_height = (font_size > 0.0f)
        ? style.line_height.to_px(font_size) / font_size
        : 1.2f;
    // CSS opacity creates a compositing group: children are affected by parent opacity.
    // Multiply this element's own opacity with the parent's accumulated opacity.
    // Since opacity is not CSS-inherited, parent_style.opacity holds the parent's
    // own opacity (or accumulated if we've already multiplied it in).
    // We accumulate into style.opacity so children also see the full chain.
    style.opacity = style.opacity * parent_style.opacity;
    layout_node->opacity = style.opacity;
    layout_node->mix_blend_mode = style.mix_blend_mode;
    layout_node->visibility_hidden = (style.visibility == clever::css::Visibility::Hidden ||
                                      style.visibility == clever::css::Visibility::Collapse);
    layout_node->visibility_collapse = (style.visibility == clever::css::Visibility::Collapse);
    layout_node->letter_spacing = style.letter_spacing.to_px(font_size);
    layout_node->word_spacing = style.word_spacing.to_px(font_size);
    // Text transform
    layout_node->text_transform = static_cast<int>(style.text_transform);
    // Text decoration
    switch (style.text_decoration) {
        case clever::css::TextDecoration::Underline: layout_node->text_decoration = 1; break;
        case clever::css::TextDecoration::LineThrough: layout_node->text_decoration = 2; break;
        case clever::css::TextDecoration::Overline: layout_node->text_decoration = 3; break;
        default: layout_node->text_decoration = 0; break;
    }
    layout_node->text_decoration_bits = style.text_decoration_bits;
    layout_node->text_decoration_color = color_to_argb(style.text_decoration_color);
    layout_node->text_decoration_style = static_cast<int>(style.text_decoration_style);
    layout_node->text_decoration_thickness = style.text_decoration_thickness;
    layout_node->border_collapse = style.border_collapse;
    layout_node->border_spacing = style.border_spacing;
    layout_node->border_spacing_v = style.border_spacing_v;
    layout_node->table_layout = style.table_layout;
    layout_node->caption_side = style.caption_side;
    layout_node->empty_cells = style.empty_cells;
    layout_node->quotes = style.quotes;
    layout_node->list_style_position = (style.list_style_position == clever::css::ListStylePosition::Inside) ? 1 : 0;
    layout_node->list_style_image = style.list_style_image;
    layout_node->pointer_events = static_cast<int>(style.pointer_events);
    layout_node->user_select = static_cast<int>(style.user_select);
    layout_node->tab_size = style.tab_size;
    layout_node->filters = style.filters;
    layout_node->drop_shadow_ox = style.drop_shadow_ox;
    layout_node->drop_shadow_oy = style.drop_shadow_oy;
    layout_node->drop_shadow_color = style.drop_shadow_color;
    layout_node->backdrop_filters = style.backdrop_filters;
    layout_node->resize = style.resize;
    layout_node->isolation = style.isolation;
    layout_node->contain = style.contain;
    layout_node->clip_path_type = style.clip_path_type;
    layout_node->clip_path_values = style.clip_path_values;
    layout_node->clip_path_path_data = style.clip_path_path_data;
    layout_node->shape_outside_type = style.shape_outside_type;
    layout_node->shape_outside_values = style.shape_outside_values;
    layout_node->shape_outside_str = style.shape_outside_str;
    layout_node->shape_margin = style.shape_margin;
    layout_node->shape_image_threshold = style.shape_image_threshold;
    layout_node->direction = (style.direction == clever::css::Direction::Rtl) ? 1 : 0;
    layout_node->line_clamp = style.line_clamp;
    // Multi-column layout
    layout_node->column_count = style.column_count;
    layout_node->column_fill = style.column_fill;
    layout_node->column_width = style.column_width.is_auto() ? -1.0f : style.column_width.to_px(0);
    layout_node->column_gap_val = style.column_gap_val.to_px(0);
    layout_node->row_gap = style.gap.to_px(0);
    layout_node->column_gap = style.column_gap_val.to_px(0);
    layout_node->column_rule_width = style.column_rule_width;
    layout_node->column_rule_color = color_to_argb(style.column_rule_color);
    layout_node->column_rule_style = style.column_rule_style;
    layout_node->caret_color = color_to_argb(style.caret_color);
    layout_node->accent_color = style.accent_color;
    layout_node->color_interpolation = style.color_interpolation;
    layout_node->mask_composite = style.mask_composite;
    layout_node->mask_mode = style.mask_mode;
    layout_node->mask_shorthand = style.mask_shorthand;
    layout_node->mask_origin = style.mask_origin;
    layout_node->mask_position = style.mask_position;
    layout_node->mask_clip = style.mask_clip;
    layout_node->mask_border = style.mask_border;
    layout_node->marker_shorthand = style.marker_shorthand;
    layout_node->marker_start = style.marker_start;
    layout_node->marker_mid = style.marker_mid;
    layout_node->marker_end = style.marker_end;
    layout_node->overflow_block = style.overflow_block;
    layout_node->overflow_inline = style.overflow_inline;
    layout_node->box_decoration_break = style.box_decoration_break;
    layout_node->margin_trim = style.margin_trim;
    layout_node->css_all = style.css_all;
    layout_node->scroll_behavior = style.scroll_behavior;
    layout_node->scroll_snap_type_axis = style.scroll_snap_type_axis;
    layout_node->scroll_snap_type_strictness = style.scroll_snap_type_strictness;
    layout_node->scroll_snap_align_x = style.scroll_snap_align_x;
    layout_node->scroll_snap_align_y = style.scroll_snap_align_y;
    layout_node->scroll_snap_stop = style.scroll_snap_stop;
    layout_node->scroll_margin_top = style.scroll_margin_top;
    layout_node->scroll_margin_right = style.scroll_margin_right;
    layout_node->scroll_margin_bottom = style.scroll_margin_bottom;
    layout_node->scroll_margin_left = style.scroll_margin_left;
    layout_node->scroll_padding_top = style.scroll_padding_top;
    layout_node->scroll_padding_right = style.scroll_padding_right;
    layout_node->scroll_padding_bottom = style.scroll_padding_bottom;
    layout_node->scroll_padding_left = style.scroll_padding_left;
    layout_node->text_rendering = style.text_rendering;
    layout_node->font_smooth = style.font_smooth;
    layout_node->text_size_adjust = style.text_size_adjust;
    layout_node->offset_path = style.offset_path;
    layout_node->offset_distance = style.offset_distance;
    layout_node->offset_rotate = style.offset_rotate;
    layout_node->offset = style.offset;
    layout_node->offset_anchor = style.offset_anchor;
    layout_node->offset_position = style.offset_position;
    layout_node->transition_behavior = style.transition_behavior;
    layout_node->animation_range = style.animation_range;
    layout_node->css_rotate = style.css_rotate;
    layout_node->css_scale = style.css_scale;
    layout_node->css_translate = style.css_translate;
    layout_node->ruby_align = style.ruby_align;
    layout_node->ruby_position = style.ruby_position;
    layout_node->ruby_overhang = style.ruby_overhang;
    layout_node->text_combine_upright = style.text_combine_upright;
    layout_node->text_orientation = style.text_orientation;
    layout_node->backface_visibility = style.backface_visibility;
    layout_node->cursor = static_cast<int>(style.cursor);
    layout_node->overflow_anchor = style.overflow_anchor;
    layout_node->overflow_clip_margin = style.overflow_clip_margin;
    layout_node->perspective = style.perspective;
    layout_node->transform_style = style.transform_style;
    layout_node->transform_box = style.transform_box;
    layout_node->transform_origin_x = style.transform_origin_x;
    layout_node->transform_origin_y = style.transform_origin_y;
    layout_node->transform_origin_x_len = style.transform_origin_x_len;
    layout_node->transform_origin_y_len = style.transform_origin_y_len;
    layout_node->transform_origin_z = style.transform_origin_z;
    layout_node->perspective_origin_x = style.perspective_origin_x;
    layout_node->perspective_origin_y = style.perspective_origin_y;
    layout_node->perspective_origin_x_len = style.perspective_origin_x_len;
    layout_node->perspective_origin_y_len = style.perspective_origin_y_len;
    {
        uint32_t pc = color_to_argb(style.placeholder_color);
        if (pc != 0) layout_node->placeholder_color = pc; // 0 means auto (keep default gray)
    }
    layout_node->writing_mode = style.writing_mode;
    layout_node->counter_increment = style.counter_increment;
    layout_node->counter_reset = style.counter_reset;
    layout_node->counter_set = style.counter_set;
    layout_node->appearance = style.appearance;
    layout_node->touch_action = style.touch_action;
    layout_node->will_change = style.will_change;
    layout_node->color_scheme = style.color_scheme;
    layout_node->container_type = style.container_type;
    layout_node->container_name = style.container_name;
    layout_node->forced_color_adjust = style.forced_color_adjust;
    layout_node->math_style = style.math_style;
    layout_node->math_depth = style.math_depth;
    layout_node->content_visibility = style.content_visibility;
    layout_node->overscroll_behavior = style.overscroll_behavior;
    layout_node->overscroll_behavior_x = style.overscroll_behavior_x;
    layout_node->overscroll_behavior_y = style.overscroll_behavior_y;
    layout_node->paint_order = style.paint_order;
    layout_node->dominant_baseline = style.dominant_baseline;
    layout_node->svg_fill_color = style.svg_fill_color;
    layout_node->svg_fill_none = style.svg_fill_none;
    layout_node->svg_stroke_color = style.svg_stroke_color;
    layout_node->svg_stroke_none = style.svg_stroke_none;
    layout_node->svg_fill_opacity = style.svg_fill_opacity;
    layout_node->svg_stroke_opacity = style.svg_stroke_opacity;
    layout_node->svg_stroke_linecap = style.svg_stroke_linecap;
    layout_node->svg_stroke_linejoin = style.svg_stroke_linejoin;
    layout_node->fill_rule = style.fill_rule;
    layout_node->clip_rule = style.clip_rule;
    layout_node->stroke_miterlimit = style.stroke_miterlimit;
    layout_node->shape_rendering = style.shape_rendering;
    layout_node->vector_effect = style.vector_effect;
    layout_node->stop_color = style.stop_color;
    layout_node->stop_opacity = style.stop_opacity;
    layout_node->flood_color = style.flood_color;
    layout_node->flood_opacity = style.flood_opacity;
    layout_node->lighting_color = style.lighting_color;
    if (style.svg_text_anchor != 0) layout_node->svg_text_anchor = style.svg_text_anchor;
    // Wire CSS cascade stroke-dasharray to LayoutNode
    if (!style.svg_stroke_dasharray_str.empty() && style.svg_stroke_dasharray_str != "none") {
        std::string da_val = style.svg_stroke_dasharray_str;
        for (auto& ch : da_val) { if (ch == ',') ch = ' '; }
        std::istringstream da_ss(da_val);
        float dv;
        while (da_ss >> dv) layout_node->svg_stroke_dasharray.push_back(dv);
    }
    // Wire CSS cascade stroke-width (if set via cascade, override attribute)
    if (style.svg_stroke_width > 0 && layout_node->is_svg) {
        // Will be applied in shape parsing
    }
    layout_node->initial_letter_size = style.initial_letter_size;
    layout_node->initial_letter_sink = style.initial_letter_sink;
    layout_node->initial_letter = style.initial_letter;
    layout_node->initial_letter_align = style.initial_letter_align;
    layout_node->text_emphasis_style = style.text_emphasis_style;
    layout_node->text_emphasis_color = style.text_emphasis_color;
    layout_node->text_stroke_width = style.text_stroke_width;
    layout_node->text_stroke_color = color_to_argb(style.text_stroke_color);
    if (style.text_fill_color.a > 0) {
        layout_node->text_fill_color = color_to_argb(style.text_fill_color);
    }
    layout_node->scrollbar_thumb_color = style.scrollbar_thumb_color;
    layout_node->scrollbar_track_color = style.scrollbar_track_color;
    layout_node->scrollbar_width = style.scrollbar_width;
    layout_node->scrollbar_gutter = style.scrollbar_gutter;
    layout_node->hyphens = style.hyphens;
    layout_node->text_justify = style.text_justify;
    layout_node->text_underline_offset = style.text_underline_offset;
    layout_node->text_underline_position = style.text_underline_position;
    layout_node->font_variant = style.font_variant;
    layout_node->font_variant_caps = style.font_variant_caps;
    layout_node->font_variant_numeric = style.font_variant_numeric;
    layout_node->font_synthesis = style.font_synthesis;
    layout_node->font_variant_alternates = style.font_variant_alternates;
    layout_node->font_feature_settings = style.font_feature_settings;
    layout_node->font_variation_settings = style.font_variation_settings;
    layout_node->font_optical_sizing = style.font_optical_sizing;
    layout_node->print_color_adjust = style.print_color_adjust;
    layout_node->image_orientation = style.image_orientation;
    layout_node->image_orientation_explicit = style.image_orientation_explicit;
    layout_node->font_kerning = style.font_kerning;
    layout_node->font_variant_ligatures = style.font_variant_ligatures;
    layout_node->font_variant_east_asian = style.font_variant_east_asian;
    layout_node->font_palette = style.font_palette;
    layout_node->font_variant_position = style.font_variant_position;
    layout_node->font_language_override = style.font_language_override;
    layout_node->font_size_adjust = style.font_size_adjust;
    layout_node->font_stretch = style.font_stretch;
    layout_node->text_decoration_skip_ink = style.text_decoration_skip_ink;
    layout_node->text_decoration_skip = style.text_decoration_skip;
    // CSS Grid layout
    layout_node->grid_template_columns = style.grid_template_columns;
    layout_node->grid_template_rows = style.grid_template_rows;
    layout_node->grid_template_columns_is_subgrid = style.grid_template_columns_is_subgrid;
    layout_node->grid_template_rows_is_subgrid = style.grid_template_rows_is_subgrid;
    layout_node->grid_column = style.grid_column;
    layout_node->grid_row = style.grid_row;
    layout_node->grid_column_start = style.grid_column_start;
    layout_node->grid_column_end = style.grid_column_end;
    layout_node->grid_row_start = style.grid_row_start;
    layout_node->grid_row_end = style.grid_row_end;
    layout_node->grid_auto_rows = style.grid_auto_rows;
    layout_node->grid_auto_columns = style.grid_auto_columns;
    layout_node->grid_auto_flow = style.grid_auto_flow;
    layout_node->grid_template_areas = style.grid_template_areas;
    layout_node->grid_area = style.grid_area;
    layout_node->justify_items = style.justify_items;
    layout_node->align_content = style.align_content;
    layout_node->justify_self = style.justify_self;
    layout_node->contain_intrinsic_width = style.contain_intrinsic_width;
    layout_node->contain_intrinsic_height = style.contain_intrinsic_height;
    layout_node->transition_property = style.transition_property;
    layout_node->transition_duration = style.transition_duration;
    layout_node->transition_timing = style.transition_timing;
    layout_node->transition_delay = style.transition_delay;
    layout_node->transition_bezier_x1 = style.transition_bezier_x1;
    layout_node->transition_bezier_y1 = style.transition_bezier_y1;
    layout_node->transition_bezier_x2 = style.transition_bezier_x2;
    layout_node->transition_bezier_y2 = style.transition_bezier_y2;
    layout_node->transition_steps_count = style.transition_steps_count;
    layout_node->animation_name = style.animation_name;
    layout_node->animation_duration = style.animation_duration;
    layout_node->animation_timing = style.animation_timing;
    layout_node->animation_delay = style.animation_delay;
    layout_node->animation_bezier_x1 = style.animation_bezier_x1;
    layout_node->animation_bezier_y1 = style.animation_bezier_y1;
    layout_node->animation_bezier_x2 = style.animation_bezier_x2;
    layout_node->animation_bezier_y2 = style.animation_bezier_y2;
    layout_node->animation_steps_count = style.animation_steps_count;
    layout_node->animation_iteration_count = style.animation_iteration_count;
    layout_node->animation_direction = style.animation_direction;
    layout_node->animation_fill_mode = style.animation_fill_mode;
    layout_node->animation_composition = style.animation_composition;
    layout_node->animation_timeline = style.animation_timeline;
    // CSS Custom Properties (CSS Variables)
    layout_node->custom_properties.insert(style.custom_properties.begin(), style.custom_properties.end());
    // Text align
    switch (style.text_align) {
        case clever::css::TextAlign::Left: layout_node->text_align = 0; break;
        case clever::css::TextAlign::Center: layout_node->text_align = 1; break;
        case clever::css::TextAlign::Right: layout_node->text_align = 2; break;
        case clever::css::TextAlign::Justify: layout_node->text_align = 3; break;
        case clever::css::TextAlign::WebkitCenter: layout_node->text_align = 4; break;
    }
    layout_node->text_align_last = style.text_align_last;
    // Text indent
    layout_node->text_indent = style.text_indent.to_px(font_size);
    // Vertical align
    switch (style.vertical_align) {
        case clever::css::VerticalAlign::Top: layout_node->vertical_align = 1; break;
        case clever::css::VerticalAlign::Middle: layout_node->vertical_align = 2; break;
        case clever::css::VerticalAlign::Bottom: layout_node->vertical_align = 3; break;
        case clever::css::VerticalAlign::TextTop: layout_node->vertical_align = 4; break;
        case clever::css::VerticalAlign::TextBottom: layout_node->vertical_align = 5; break;
        default: {
            // Baseline: check for length/percentage offset value
            if (style.vertical_align_offset != 0) {
                layout_node->vertical_align = 8; // length/percentage offset
                layout_node->vertical_align_offset = style.vertical_align_offset;
            } else {
                layout_node->vertical_align = 0; // pure baseline
            }
            break;
        }
    }
    layout_node->color = color_to_argb(style.color);
    layout_node->background_color = color_to_argb(style.background_color);

    if (g_transition_runtime_enabled) {
        const std::string style_key = build_style_key_for_node(node);
        if (!style_key.empty()) {
            g_current_style_keys.insert(style_key);
            g_current_layout_nodes_by_key[style_key] = layout_node.get();

            if (!g_transition_animation_controller) {
                g_transition_animation_controller = std::make_unique<AnimationController>();
            }

            auto prev_it = g_previous_styles_by_key.find(style_key);
            if (prev_it != g_previous_styles_by_key.end() && g_transition_animation_controller) {
                auto changed_properties = detect_changed_transition_properties(prev_it->second, style);
                if (!changed_properties.empty()) {
                    auto* runtime_node = get_or_create_transition_runtime_node(style_key);
                    if (runtime_node && !g_transition_animation_controller->is_animating(runtime_node)) {
                        for (const auto& property : changed_properties) {
                            auto transition_def_opt = transition_def_for_property(style, property);
                            if (!transition_def_opt.has_value()) continue;
                            const auto& transition_def = *transition_def_opt;
                            if (transition_def.duration_ms <= 0.0f) continue;

                            if (property == "opacity") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property, prev_it->second.opacity, style.opacity, transition_def);
                            } else if (property == "font-size") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.font_size.to_px(),
                                    style.font_size.to_px(),
                                    transition_def);
                            } else if (property == "border-radius") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.border_radius,
                                    style.border_radius,
                                    transition_def);
                            } else if (property == "background-color") {
                                g_transition_animation_controller->start_color_transition(
                                    runtime_node, property,
                                    prev_it->second.background_color,
                                    style.background_color,
                                    transition_def);
                            } else if (property == "color") {
                                g_transition_animation_controller->start_color_transition(
                                    runtime_node, property,
                                    prev_it->second.color,
                                    style.color,
                                    transition_def);
                            } else if (property == "width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.width.to_px(),
                                    style.width.to_px(),
                                    transition_def);
                            } else if (property == "height") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.height.to_px(),
                                    style.height.to_px(),
                                    transition_def);
                            } else if (property == "margin-top") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.margin.top.to_px(),
                                    style.margin.top.to_px(),
                                    transition_def);
                            } else if (property == "margin-right") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.margin.right.to_px(),
                                    style.margin.right.to_px(),
                                    transition_def);
                            } else if (property == "margin-bottom") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.margin.bottom.to_px(),
                                    style.margin.bottom.to_px(),
                                    transition_def);
                            } else if (property == "margin-left") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.margin.left.to_px(),
                                    style.margin.left.to_px(),
                                    transition_def);
                            } else if (property == "padding-top") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.padding.top.to_px(),
                                    style.padding.top.to_px(),
                                    transition_def);
                            } else if (property == "padding-right") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.padding.right.to_px(),
                                    style.padding.right.to_px(),
                                    transition_def);
                            } else if (property == "padding-bottom") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.padding.bottom.to_px(),
                                    style.padding.bottom.to_px(),
                                    transition_def);
                            } else if (property == "padding-left") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.padding.left.to_px(),
                                    style.padding.left.to_px(),
                                    transition_def);
                            } else if (property == "top") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.top.to_px(),
                                    style.top.to_px(),
                                    transition_def);
                            } else if (property == "left") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.left_pos.to_px(),
                                    style.left_pos.to_px(),
                                    transition_def);
                            } else if (property == "right") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.right_pos.to_px(),
                                    style.right_pos.to_px(),
                                    transition_def);
                            } else if (property == "bottom") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.bottom.to_px(),
                                    style.bottom.to_px(),
                                    transition_def);
                            } else if (property == "border-top-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.border_top.width.to_px(),
                                    style.border_top.width.to_px(),
                                    transition_def);
                            } else if (property == "border-right-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.border_right.width.to_px(),
                                    style.border_right.width.to_px(),
                                    transition_def);
                            } else if (property == "border-bottom-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.border_bottom.width.to_px(),
                                    style.border_bottom.width.to_px(),
                                    transition_def);
                            } else if (property == "border-left-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.border_left.width.to_px(),
                                    style.border_left.width.to_px(),
                                    transition_def);
                            } else if (property == "letter-spacing") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.letter_spacing.to_px(),
                                    style.letter_spacing.to_px(),
                                    transition_def);
                            } else if (property == "word-spacing") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.word_spacing.to_px(),
                                    style.word_spacing.to_px(),
                                    transition_def);
                            } else if (property == "min-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.min_width.to_px(),
                                    style.min_width.to_px(),
                                    transition_def);
                            } else if (property == "min-height") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.min_height.to_px(),
                                    style.min_height.to_px(),
                                    transition_def);
                            } else if (property == "max-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.max_width.to_px(),
                                    style.max_width.to_px(),
                                    transition_def);
                            } else if (property == "max-height") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.max_height.to_px(),
                                    style.max_height.to_px(),
                                    transition_def);
                            } else if (property == "outline-width") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.outline_width.to_px(),
                                    style.outline_width.to_px(),
                                    transition_def);
                            } else if (property == "outline-offset") {
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    prev_it->second.outline_offset.to_px(),
                                    style.outline_offset.to_px(),
                                    transition_def);
                            } else if (property == "box-shadow") {
                                // For box-shadow, transition the blur radius of the first shadow
                                // as a proxy for the overall shadow transition
                                float old_blur = prev_it->second.box_shadows.empty()
                                    ? prev_it->second.shadow_blur
                                    : prev_it->second.box_shadows[0].blur;
                                float new_blur = style.box_shadows.empty()
                                    ? style.shadow_blur
                                    : style.box_shadows[0].blur;
                                g_transition_animation_controller->start_transition(
                                    runtime_node, property,
                                    old_blur, new_blur,
                                    transition_def);
                            } else if (property == "transform") {
                                // Transition transform if both old and new have at least one
                                // transform of the same type; otherwise skip interpolation
                                const auto& old_transforms = prev_it->second.transforms;
                                const auto& new_transforms = style.transforms;
                                if (!old_transforms.empty() && !new_transforms.empty() &&
                                    old_transforms[0].type == new_transforms[0].type) {
                                    g_transition_animation_controller->start_transform_transition(
                                        runtime_node, property,
                                        old_transforms[0],
                                        new_transforms[0],
                                        transition_def);
                                }
                            }
                        }
                    }
                }
            }

            g_previous_styles_by_key[style_key] = style;
        }
    }

    // Gradient
    if (!style.gradient_stops.empty()) {
        layout_node->gradient_type = style.gradient_type;
        layout_node->gradient_angle = style.gradient_angle;
        layout_node->radial_shape = style.radial_shape;
        layout_node->gradient_stops = style.gradient_stops;
    }

    // Fetch background image if specified
    if (!style.bg_image_url.empty()) {
        std::string img_url = resolve_url(style.bg_image_url, base_url);
        auto decoded = fetch_and_decode_image(img_url);
        if (decoded.pixels && !decoded.pixels->empty()) {
            layout_node->bg_image_pixels = decoded.pixels;
            layout_node->bg_image_width = decoded.width;
            layout_node->bg_image_height = decoded.height;
        }
    }

    // Wire background-size, background-repeat, background-position to layout node
    layout_node->background_size = style.background_size;
    layout_node->bg_size_width = style.bg_size_width;
    layout_node->bg_size_height = style.bg_size_height;
    layout_node->background_repeat = style.background_repeat;
    // Convert keyword-based position to pixel offsets (resolved at paint time)
    // Store keywords as special values: negative numbers indicate keyword positions
    // -1 = left/top, -2 = center, -3 = right/bottom
    if (style.background_position_x == 0) layout_node->bg_position_x = -1; // left
    else if (style.background_position_x == 1) layout_node->bg_position_x = -2; // center
    else if (style.background_position_x == 2) layout_node->bg_position_x = -3; // right
    else layout_node->bg_position_x = static_cast<float>(style.background_position_x); // px value
    if (style.background_position_y == 0) layout_node->bg_position_y = -1; // top
    else if (style.background_position_y == 1) layout_node->bg_position_y = -2; // center
    else if (style.background_position_y == 2) layout_node->bg_position_y = -3; // bottom
    else layout_node->bg_position_y = static_cast<float>(style.background_position_y); // px value

    // For font-relative units (em, ch, lh), pass font_size as context
    float fs = style.font_size.to_px(16.0f, 16.0f);
    float lh_px = style.line_height.to_px(fs, 16.0f);
    // Handle min-content/max-content/fit-content keywords
    if (style.width_keyword != 0) {
        layout_node->specified_width = static_cast<float>(style.width_keyword); // -2, -3, or -4
    } else if (!style.width.is_auto()) {
        if (style.width.unit == clever::css::Length::Unit::Calc ||
            style.width.unit == clever::css::Length::Unit::Percent) {
            // Defer resolution to layout time when container width is known
            layout_node->css_width = style.width;
            layout_node->specified_width = style.width.to_px(fs);
        } else if (style.width.unit == clever::css::Length::Unit::Em ||
                   style.width.unit == clever::css::Length::Unit::Ch ||
                   style.width.unit == clever::css::Length::Unit::Lh) {
            layout_node->specified_width = style.width.to_px(fs, 16.0f, lh_px);
        } else {
            layout_node->specified_width = style.width.to_px(0);
        }
    }
    // Handle min-content/max-content/fit-content height keywords
    if (style.height_keyword != 0) {
        layout_node->specified_height = static_cast<float>(style.height_keyword); // -2, -3, or -4
    } else if (!style.height.is_auto()) {
        if (style.height.unit == clever::css::Length::Unit::Calc ||
            style.height.unit == clever::css::Length::Unit::Percent) {
            layout_node->css_height = style.height;
            if (tag_lower == "html" || tag_lower == "body") {
                // Root-level percentage heights must resolve against the viewport.
                layout_node->specified_height = style.height.to_px(clever::css::Length::s_viewport_h);
            } else {
                // Defer percent/calc height resolution until containing block height is definite.
                layout_node->specified_height = -1.0f;
            }
        } else if (style.height.unit == clever::css::Length::Unit::Em ||
                   style.height.unit == clever::css::Length::Unit::Ch ||
                   style.height.unit == clever::css::Length::Unit::Lh) {
            layout_node->specified_height = style.height.to_px(fs, 16.0f, lh_px);
        } else {
            layout_node->specified_height = style.height.to_px(0);
        }
    }

    // Min/max width/height constraints
    // For percentage/calc units, defer resolution to layout time (when containing width is known)
    if (!style.min_width.is_zero()) {
        if (style.min_width.unit == clever::css::Length::Unit::Percent ||
            style.min_width.unit == clever::css::Length::Unit::Calc) {
            layout_node->css_min_width = style.min_width;
        } else {
            layout_node->min_width = style.min_width.to_px(0);
        }
    }
    if (style.max_width.unit != clever::css::Length::Unit::Px || style.max_width.value < 1e9f) {
        if (style.max_width.unit == clever::css::Length::Unit::Percent ||
            style.max_width.unit == clever::css::Length::Unit::Calc) {
            layout_node->css_max_width = style.max_width;
        } else {
            float mw = style.max_width.to_px(0);
            if (mw < 1e9f) layout_node->max_width = mw;
        }
    }
    if (!style.min_height.is_zero()) {
        if (style.min_height.unit == clever::css::Length::Unit::Percent ||
            style.min_height.unit == clever::css::Length::Unit::Calc) {
            layout_node->css_min_height = style.min_height;
        } else {
            layout_node->min_height = style.min_height.to_px(0);
        }
    }
    if (style.max_height.unit != clever::css::Length::Unit::Px || style.max_height.value < 1e9f) {
        if (style.max_height.unit == clever::css::Length::Unit::Percent ||
            style.max_height.unit == clever::css::Length::Unit::Calc) {
            layout_node->css_max_height = style.max_height;
        } else {
            float mh = style.max_height.to_px(0);
            if (mh < 1e9f) layout_node->max_height = mh;
        }
    }

    // Handle <picture> element: select best source and render as image
    if (tag_lower == "picture") {
        layout_node->is_picture = true;

        // Scan children for <source> elements and the fallback <img>
        std::string selected_src;
        const clever::html::SimpleNode* fallback_img = nullptr;

        int vw = static_cast<int>(clever::css::Length::s_viewport_w);
        int vh = static_cast<int>(clever::css::Length::s_viewport_h);
        if (vw <= 0) vw = 1280;
        if (vh <= 0) vh = 720;

        for (auto& child : node.children) {
            if (child->type != clever::html::SimpleNode::Element) continue;
            std::string child_tag = to_lower(child->tag_name);

            if (child_tag == "source" && selected_src.empty()) {
                // Check media attribute — skip if doesn't match
                std::string media = get_attr(*child, "media");
                if (!media.empty() && !evaluate_media_query(media, vw, vh)) continue;

                // Check type attribute — skip unsupported formats
                std::string type = get_attr(*child, "type");
                if (!type.empty()) {
                    std::string tl = to_lower(type);
                    // We support jpeg, png, gif, webp, svg — not avif, jxl
                    if (tl.find("avif") != std::string::npos ||
                        tl.find("jxl") != std::string::npos) continue;
                }

                std::string srcset = get_attr(*child, "srcset");
                if (!srcset.empty()) {
                    // Parse srcset: "url1 480w, url2 800w" or "url1 1x, url2 2x"
                    // Select best candidate based on viewport width
                    std::string best_url;
                    float best_w = -1;
                    float best_density = -1;

                    std::istringstream ss(srcset);
                    std::string token;
                    std::string candidate_url;
                    while (ss >> token) {
                        if (token.back() == ',') token.pop_back();
                        if (candidate_url.empty()) {
                            candidate_url = token;
                            continue;
                        }
                        // This token is the descriptor
                        if (!token.empty() && (token.back() == 'w' || token.back() == 'W')) {
                            float w_val = 0;
                            try { w_val = std::stof(token.substr(0, token.size() - 1)); } catch (...) {}
                            if (w_val > 0) {
                                if (best_url.empty() || (w_val >= vw && (best_w < vw || w_val < best_w)) ||
                                    (best_w < vw && w_val > best_w)) {
                                    best_url = candidate_url;
                                    best_w = w_val;
                                }
                            }
                        } else if (!token.empty() && (token.back() == 'x' || token.back() == 'X')) {
                            float d_val = 1;
                            try { d_val = std::stof(token.substr(0, token.size() - 1)); } catch (...) {}
                            if (d_val > best_density) {
                                best_density = d_val;
                                best_url = candidate_url;
                            }
                        }
                        candidate_url.clear();
                    }
                    // If only URL with no descriptor, use it
                    if (!candidate_url.empty() && best_url.empty()) best_url = candidate_url;
                    if (best_url.empty() && !srcset.empty()) {
                        // Fallback: just take the first URL
                        auto sp = srcset.find(' ');
                        best_url = (sp != std::string::npos) ? srcset.substr(0, sp) : srcset;
                    }
                    selected_src = best_url;
                } else {
                    std::string src = get_attr(*child, "src");
                    if (!src.empty()) selected_src = src;
                }
            } else if (child_tag == "img") {
                fallback_img = child.get();
            }
        }

        // Fall back to the <img> child's srcset, then src
        if (selected_src.empty() && fallback_img) {
            std::string fb_srcset = get_attr(*fallback_img, "srcset");
            if (!fb_srcset.empty()) {
                auto sp = fb_srcset.find(' ');
                selected_src = (sp != std::string::npos) ? fb_srcset.substr(0, sp) : fb_srcset;
            }
            if (selected_src.empty()) selected_src = get_attr(*fallback_img, "src");
        }

        layout_node->picture_srcset = selected_src;

        // Determine width/height from <img> child attributes
        float attr_w = -1, attr_h = -1;
        if (fallback_img) {
            std::string w_str = get_attr(*fallback_img, "width");
            std::string h_str = get_attr(*fallback_img, "height");
            if (!w_str.empty()) { try { attr_w = std::stof(w_str); } catch (...) {} }
            if (!h_str.empty()) { try { attr_h = std::stof(h_str); } catch (...) {} }
        }

        // Try to fetch and decode the selected image
        std::string img_url = resolve_url(selected_src, base_url);
        auto decoded = fetch_and_decode_image(img_url);

        if (decoded.pixels && !decoded.pixels->empty()) {
            layout_node->image_pixels = decoded.pixels;
            layout_node->image_width = decoded.width;
            layout_node->image_height = decoded.height;

            if (layout_node->specified_width < 0 && attr_w > 0)
                layout_node->specified_width = attr_w;
            if (layout_node->specified_height < 0 && attr_h > 0)
                layout_node->specified_height = attr_h;

            if (decoded.height > 0 && decoded.width > 0) {
                float aspect = static_cast<float>(decoded.width) / static_cast<float>(decoded.height);
                if (layout_node->specified_width > 0 && layout_node->specified_height < 0) {
                    layout_node->specified_height = layout_node->specified_width / aspect;
                } else if (layout_node->specified_height > 0 && layout_node->specified_width < 0) {
                    layout_node->specified_width = layout_node->specified_height * aspect;
                } else if (layout_node->specified_width < 0 && layout_node->specified_height < 0) {
                    layout_node->specified_width = static_cast<float>(decoded.width);
                    layout_node->specified_height = static_cast<float>(decoded.height);
                }
            } else {
                if (layout_node->specified_width < 0)
                    layout_node->specified_width = static_cast<float>(decoded.width > 0 ? decoded.width : 150);
                if (layout_node->specified_height < 0)
                    layout_node->specified_height = static_cast<float>(decoded.height > 0 ? decoded.height : 150);
            }
        } else {
            // Fallback placeholder — broken image indicator with alt text
            if (layout_node->specified_width < 0)
                layout_node->specified_width = (attr_w > 0) ? attr_w : 150;
            if (layout_node->specified_height < 0)
                layout_node->specified_height = (attr_h > 0) ? attr_h : 150;

            layout_node->background_color = 0xFFF0F0F0;
            layout_node->geometry.border = {1, 1, 1, 1};
            layout_node->border_color = 0xFFCCCCCC;
            layout_node->border_color_top = 0xFFCCCCCC;
            layout_node->border_color_right = 0xFFCCCCCC;
            layout_node->border_color_bottom = 0xFFCCCCCC;
            layout_node->border_color_left = 0xFFCCCCCC;
            layout_node->geometry.padding = {4, 4, 4, 4};

            std::string alt;
            if (fallback_img) alt = get_attr(*fallback_img, "alt");
            layout_node->img_alt_text = alt.empty() ? "[image]" : alt;
            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->text_content = layout_node->img_alt_text;
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = 12.0f;
            text_child->color = 0xFF666666;
            layout_node->append_child(std::move(text_child));
        }

        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        // Don't recurse into children — <picture> is rendered as its selected image
        return layout_node;
    }

    // Skip <source> elements outside of <picture> (handled above)
    if (tag_lower == "source") {
        return nullptr;
    }

    // Handle <img> as replaced inline element
    if (tag_lower == "img") {
        std::string loading = get_attr(node, "loading");
        if (to_lower(loading) == "lazy") {
            layout_node->loading_lazy = true;
        }
        // Use width/height attributes if not set by CSS
        float attr_w = -1, attr_h = -1;
        {
            std::string w_str = get_attr(node, "width");
            std::string h_str = get_attr(node, "height");
            if (!w_str.empty()) { try { attr_w = std::stof(w_str); } catch (...) {} }
            if (!h_str.empty()) { try { attr_h = std::stof(h_str); } catch (...) {} }
        }

        // Try to fetch and decode the actual image
        // Check srcset first for responsive images
        std::string src = get_attr(node, "src");
        std::string srcset_attr = get_attr(node, "srcset");
        if (!srcset_attr.empty()) {
            int vw = static_cast<int>(clever::css::Length::s_viewport_w);
            if (vw <= 0) vw = 1280;
            std::string best_url;
            float best_w = -1;
            std::istringstream ss(srcset_attr);
            std::string token;
            std::string candidate_url;
            while (ss >> token) {
                if (token.back() == ',') token.pop_back();
                if (candidate_url.empty()) { candidate_url = token; continue; }
                if (!token.empty() && (token.back() == 'w' || token.back() == 'W')) {
                    float w_val = 0;
                    try { w_val = std::stof(token.substr(0, token.size() - 1)); } catch (...) {}
                    if (w_val > 0 && (best_url.empty() ||
                        (w_val >= vw && (best_w < vw || w_val < best_w)) ||
                        (best_w < vw && w_val > best_w))) {
                        best_url = candidate_url; best_w = w_val;
                    }
                } else if (!token.empty() && (token.back() == 'x' || token.back() == 'X')) {
                    if (best_url.empty()) best_url = candidate_url;
                }
                candidate_url.clear();
            }
            if (!candidate_url.empty() && best_url.empty()) best_url = candidate_url;
            if (!best_url.empty()) src = best_url;
        }
        std::string img_url = resolve_url(src, base_url);

        // Handle lazy loading
        if (layout_node->loading_lazy) {
            layout_node->lazy_img_url = img_url;
            if (layout_node->specified_width < 0)
                layout_node->specified_width = (attr_w > 0) ? attr_w : 150;
            if (layout_node->specified_height < 0)
                layout_node->specified_height = (attr_h > 0) ? attr_h : 150;
        } else {
            auto decoded = fetch_and_decode_image(img_url);

            if (decoded.pixels && !decoded.pixels->empty()) {
                // Successfully decoded — use real image data
                layout_node->image_pixels = decoded.pixels;
                layout_node->image_width = decoded.width;
                layout_node->image_height = decoded.height;

                // Determine display size: CSS > HTML attrs > intrinsic
                if (layout_node->specified_width < 0 && attr_w > 0)
                    layout_node->specified_width = attr_w;
                if (layout_node->specified_height < 0 && attr_h > 0)
                    layout_node->specified_height = attr_h;

                // If only one dimension specified, compute the other from aspect ratio
                if (decoded.height > 0 && decoded.width > 0) {
                    float aspect = static_cast<float>(decoded.width) / static_cast<float>(decoded.height);
                    if (layout_node->specified_width > 0 && layout_node->specified_height < 0) {
                        layout_node->specified_height = layout_node->specified_width / aspect;
                    } else if (layout_node->specified_height > 0 && layout_node->specified_width < 0) {
                        layout_node->specified_width = layout_node->specified_height * aspect;
                    } else if (layout_node->specified_width < 0 && layout_node->specified_height < 0) {
                        layout_node->specified_width = static_cast<float>(decoded.width);
                        layout_node->specified_height = static_cast<float>(decoded.height);
                    }
                } else if (layout_node->specified_width < 0 && layout_node->specified_height < 0) {
                    layout_node->specified_width = static_cast<float>(decoded.width > 0 ? decoded.width : 150);
                    layout_node->specified_height = static_cast<float>(decoded.height > 0 ? decoded.height : 150);
                }
            } else {
                // Fallback to placeholder — broken image indicator with alt text
                if (layout_node->specified_width < 0)
                    layout_node->specified_width = (attr_w > 0) ? attr_w : 150;
                if (layout_node->specified_height < 0)
                    layout_node->specified_height = (attr_h > 0) ? attr_h : 150;

                // Light gray background with 1px border (broken image style)
                layout_node->background_color = 0xFFF0F0F0;
                layout_node->geometry.border = {1, 1, 1, 1};
                layout_node->border_color = 0xFFCCCCCC;
                layout_node->border_color_top = 0xFFCCCCCC;
                layout_node->border_color_right = 0xFFCCCCCC;
                layout_node->border_color_bottom = 0xFFCCCCCC;
                layout_node->border_color_left = 0xFFCCCCCC;
                layout_node->geometry.padding = {4, 4, 4, 4};

                std::string alt = get_attr(node, "alt");
                // Store alt text on the node for painter to render broken image indicator
                layout_node->img_alt_text = alt.empty() ? "[image]" : alt;

                auto text_child = std::make_unique<clever::layout::LayoutNode>();
                text_child->is_text = true;
                text_child->text_content = layout_node->img_alt_text;
                text_child->mode = clever::layout::LayoutMode::Inline;
                text_child->display = clever::layout::DisplayType::Inline;
                text_child->font_size = 12.0f;
                text_child->color = 0xFF666666;
                layout_node->append_child(std::move(text_child));
            }
        }

        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->image_rendering = style.image_rendering;
        layout_node->hanging_punctuation = style.hanging_punctuation;
        layout_node->object_fit = style.object_fit;
        layout_node->object_position_x = style.object_position_x;
        layout_node->object_position_y = style.object_position_y;

        // Compute rendered image dimensions based on object-fit
        {
            float box_w = layout_node->specified_width;
            float box_h = layout_node->specified_height;

            // Natural image aspect ratio: use actual image if available, else default 4:3
            float nat_w = (layout_node->image_width > 0) ? static_cast<float>(layout_node->image_width) : (box_w > 0 ? box_w * 4.0f / 3.0f : 200.0f);
            float nat_h = (layout_node->image_height > 0) ? static_cast<float>(layout_node->image_height) : (box_h > 0 ? box_h : 150.0f);
            if (box_w <= 0) box_w = nat_w;
            if (box_h <= 0) box_h = nat_h;

            float rw = box_w, rh = box_h;

            switch (layout_node->object_fit) {
                case 0: // fill — stretch to fill the element box
                    rw = box_w;
                    rh = box_h;
                    break;
                case 1: // contain — fit within box, preserve aspect ratio
                {
                    float scale = std::min(box_w / nat_w, box_h / nat_h);
                    rw = nat_w * scale;
                    rh = nat_h * scale;
                    break;
                }
                case 2: // cover — cover entire box, preserve aspect ratio
                {
                    float scale = std::max(box_w / nat_w, box_h / nat_h);
                    rw = nat_w * scale;
                    rh = nat_h * scale;
                    break;
                }
                case 3: // none — use natural dimensions
                    rw = nat_w;
                    rh = nat_h;
                    break;
                case 4: // scale-down — same as contain if scaling down, else none
                {
                    if (nat_w > box_w || nat_h > box_h) {
                        // Would need to scale down → use contain
                        float scale = std::min(box_w / nat_w, box_h / nat_h);
                        rw = nat_w * scale;
                        rh = nat_h * scale;
                    } else {
                        // Natural size fits → use none
                        rw = nat_w;
                        rh = nat_h;
                    }
                    break;
                }
            }

            // Center using object-position (percentage offsets)
            float ox = layout_node->object_position_x;
            float oy = layout_node->object_position_y;
            layout_node->rendered_img_w = rw;
            layout_node->rendered_img_h = rh;
            layout_node->rendered_img_x = (box_w - rw) * (ox / 100.0f);
            layout_node->rendered_img_y = (box_h - rh) * (oy / 100.0f);
        }

        // Don't recurse into children for <img> (void element)
        return layout_node;
    }

    // Resolve ::placeholder pseudo-element styling for input/textarea elements
    if (elem_view && (tag_lower == "input" || tag_lower == "textarea")) {
        auto ph_style = resolver.resolve_pseudo(*elem_view, "placeholder", style);
        if (ph_style) {
            uint32_t ph_color = color_to_argb(ph_style->color);
            if (ph_color != 0) layout_node->placeholder_color = ph_color;
            float ph_fs = ph_style->font_size.to_px(font_size);
            if (ph_fs != font_size) layout_node->placeholder_font_size = ph_fs;
            layout_node->placeholder_italic =
                (ph_style->font_style != clever::css::FontStyle::Normal);
        }
    }

    // Handle <input> as replaced inline element
    if (tag_lower == "input") {
        std::string type = to_lower(get_attr(node, "type"));
        if (type.empty()) type = "text";

        // Inputs return early below, so apply CSS border/outline now.
        auto convert_bs = [](clever::css::BorderStyle bs) -> int {
            if (bs == clever::css::BorderStyle::None) return 0;
            if (bs == clever::css::BorderStyle::Solid) return 1;
            if (bs == clever::css::BorderStyle::Dashed) return 2;
            if (bs == clever::css::BorderStyle::Dotted) return 3;
            if (bs == clever::css::BorderStyle::Double) return 4;
            if (bs == clever::css::BorderStyle::Groove) return 5;
            if (bs == clever::css::BorderStyle::Ridge) return 6;
            if (bs == clever::css::BorderStyle::Inset) return 7;
            if (bs == clever::css::BorderStyle::Outset) return 8;
            return 0;
        };
        auto border_width_or_zero = [](const clever::css::BorderEdge& edge) -> float {
            if (edge.style == clever::css::BorderStyle::None) return 0.0f;
            return edge.width.to_px(0);
        };
        layout_node->geometry.border.top = border_width_or_zero(style.border_top);
        layout_node->geometry.border.right = border_width_or_zero(style.border_right);
        layout_node->geometry.border.bottom = border_width_or_zero(style.border_bottom);
        layout_node->geometry.border.left = border_width_or_zero(style.border_left);
        layout_node->border_color_top = color_to_argb(style.border_top.color);
        layout_node->border_color_right = color_to_argb(style.border_right.color);
        layout_node->border_color_bottom = color_to_argb(style.border_bottom.color);
        layout_node->border_color_left = color_to_argb(style.border_left.color);
        layout_node->border_color = layout_node->border_color_top;
        layout_node->border_style_top = convert_bs(style.border_top.style);
        layout_node->border_style_right = convert_bs(style.border_right.style);
        layout_node->border_style_bottom = convert_bs(style.border_bottom.style);
        layout_node->border_style_left = convert_bs(style.border_left.style);
        layout_node->border_style = layout_node->border_style_top;

        if (style.outline_style != clever::css::BorderStyle::None) {
            float outline_width = style.outline_width.to_px(0);
            if (outline_width > 0) {
                layout_node->outline_width = outline_width;
                // Resolve currentColor sentinel (alpha=0 transparent) to the element's text color
                clever::css::Color oc = style.outline_color;
                if (oc.a == 0 && oc.r == 0 && oc.g == 0 && oc.b == 0) {
                    oc = style.color; // currentColor — use element's text color
                }
                layout_node->outline_color = color_to_argb(oc);
                layout_node->outline_style = convert_bs(style.outline_style);
                layout_node->outline_offset = style.outline_offset.to_px(0);
            }
        }

        if (type == "text" || type == "password" || type == "email" ||
            type == "search" || type == "url" || type == "number" || type == "tel") {
            // Mark as text/password input for dedicated painter rendering
            layout_node->is_text_input = true;
            layout_node->is_password_input = (type == "password");
            // Text input: placeholder/value text and sizing defaults.
            if (layout_node->specified_width < 0) layout_node->specified_width = 150.0f;
            if (layout_node->specified_height < 0) layout_node->specified_height = 20.0f;
            if (layout_node->geometry.border.top <= 0.0f &&
                layout_node->geometry.border.right <= 0.0f &&
                layout_node->geometry.border.bottom <= 0.0f &&
                layout_node->geometry.border.left <= 0.0f) {
                layout_node->geometry.border = {2.0f, 2.0f, 2.0f, 2.0f};
                layout_node->border_style = 1;
                layout_node->border_style_top = 1;
                layout_node->border_style_right = 1;
                layout_node->border_style_bottom = 1;
                layout_node->border_style_left = 1;
            }

            // Apply dark mode colors when color-scheme: dark.
            bool dark_input = (layout_node->color_scheme == 2);
            if (layout_node->background_color == 0x00000000 ||
                layout_node->background_color == 0xFF000000) {
                layout_node->background_color = dark_input ? 0xFF1E1E1E : 0xFFFFFFFF;
            }
            layout_node->color = dark_input ? 0xFFE0E0E0 : 0xFF333333;
            if (layout_node->geometry.padding.top <= 0) {
                layout_node->geometry.padding = {2, 4, 2, 4};
            }

            // UA default cursor: text (I-beam) for text-type inputs
            if (layout_node->cursor == 0) { // only if not explicitly set by CSS
                layout_node->cursor = 3; // Cursor::Text
            }

            // Show value or placeholder
            std::string value_text = get_attr(node, "value");
            bool showing_placeholder = value_text.empty();
            std::string placeholder_attr = get_attr(node, "placeholder");
            std::string text = showing_placeholder ? placeholder_attr : value_text;
            if (text.empty()) text = " ";

            // Store placeholder and value on the layout node for inspection
            layout_node->placeholder_text = placeholder_attr;
            layout_node->input_value = value_text;

            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            // Password: render bullet characters (\u2022) instead of actual value
            if (type == "password" && !showing_placeholder) {
                std::string bullets;
                for (size_t i = 0; i < text.size(); ++i)
                    bullets += "\xE2\x80\xA2"; // U+2022 BULLET
                text_child->text_content = bullets;
            } else {
                text_child->text_content = text;
            }
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = (showing_placeholder && layout_node->placeholder_font_size > 0)
                ? layout_node->placeholder_font_size : 13.0f;
            text_child->color = showing_placeholder ? layout_node->placeholder_color
                                                    : (dark_input ? 0xFFE0E0E0 : 0xFF333333);
            text_child->font_italic = showing_placeholder && layout_node->placeholder_italic;
            layout_node->append_child(std::move(text_child));

        } else if (type == "submit" || type == "button" || type == "reset") {
            // Mark as button input for dedicated painter rendering
            layout_node->is_button_input = true;
            // Button-like input
            std::string label = get_attr(node, "value");
            if (label.empty()) {
                if (type == "submit") label = "Submit";
                else if (type == "reset") label = "Reset";
                else label = "Button";
            }
            if (layout_node->specified_height < 0) layout_node->specified_height = 26;

            bool dark_btn = (layout_node->color_scheme == 2);
            layout_node->background_color = dark_btn ? 0xFF1E1E1E : 0xFFE0E0E0;
            layout_node->color = dark_btn ? 0xFFE0E0E0 : 0xFF333333;
            if (layout_node->geometry.padding.top <= 0) {
                layout_node->geometry.padding = {4, 12, 4, 12};
            }
            layout_node->border_radius = 3.0f;

            // Form submission: make submit buttons clickable
            if (type == "submit" && form) {
                std::string action = get_attr(*form, "action");
                std::string method = to_lower(get_attr(*form, "method"));
                if (method.empty()) method = "get";

                std::string action_url = action.empty() ? base_url : resolve_url(action, base_url);
                if (method == "get") {
                    // GET: build query string and navigate as a link
                    std::string query = build_form_query_string(*form);
                    if (!query.empty()) {
                        if (action_url.find('?') != std::string::npos)
                            action_url += "&" + query;
                        else
                            action_url += "?" + query;
                    }
                    layout_node->link_href = action_url;
                } else {
                    // POST (or other methods): use form submit region
                    // so the delegate can send a proper POST request.
                    for (int fi = static_cast<int>(collected_forms.size()) - 1; fi >= 0; fi--) {
                        if (collected_forms[fi].action == action_url &&
                            collected_forms[fi].method == method) {
                            layout_node->form_index = fi;
                            break;
                        }
                    }
                }
            }

            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->text_content = label;
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = 13.0f;
            text_child->color = dark_btn ? 0xFFE0E0E0 : 0xFF333333;
            layout_node->append_child(std::move(text_child));

        } else if (type == "file") {
            // File input: "Choose File" button + "No file chosen" text
            if (layout_node->specified_height < 0) layout_node->specified_height = 26;
            layout_node->background_color = 0xFFE0E0E0;
            layout_node->color = 0xFF333333;
            layout_node->geometry.padding = {4, 8, 4, 8};
            layout_node->border_radius = 3.0f;

            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->text_content = "Choose File  No file chosen";
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = 13.0f;
            text_child->color = 0xFF333333;
            layout_node->append_child(std::move(text_child));

        } else if (type == "date" || type == "time" || type == "datetime-local" ||
                   type == "week" || type == "month") {
            // Date/time inputs: styled like text inputs with placeholder
            if (layout_node->specified_width < 0) layout_node->specified_width = 200;
            if (layout_node->specified_height < 0) layout_node->specified_height = 24;
            layout_node->background_color = 0xFFFFFFFF;
            layout_node->color = 0xFF333333;
            layout_node->geometry.padding = {2, 4, 2, 4};

            // UA default cursor: text for date/time inputs
            if (layout_node->cursor == 0) {
                layout_node->cursor = 3; // Cursor::Text
            }

            std::string value_text = get_attr(node, "value");
            std::string placeholder;
            if (type == "date") placeholder = "yyyy-mm-dd";
            else if (type == "time") placeholder = "hh:mm";
            else if (type == "datetime-local") placeholder = "yyyy-mm-ddThh:mm";
            else if (type == "week") placeholder = "yyyy-Www";
            else if (type == "month") placeholder = "yyyy-mm";
            std::string text = value_text.empty() ? placeholder : value_text;

            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->text_content = text;
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = 13.0f;
            text_child->color = value_text.empty() ? 0xFF999999 : 0xFF333333;
            layout_node->append_child(std::move(text_child));

        } else if (type == "checkbox" || type == "radio") {
            if (layout_node->specified_width < 0) layout_node->specified_width = 16;
            if (layout_node->specified_height < 0) layout_node->specified_height = 16;
            layout_node->background_color = 0x00000000; // transparent — painting handled by painter
            layout_node->is_checkbox = (type == "checkbox");
            layout_node->is_radio = (type == "radio");
            layout_node->is_checked = has_attr(node, "checked");
            layout_node->accent_color = style.accent_color;
            // No default border/padding — painter handles all rendering
            layout_node->geometry.padding = {0, 0, 0, 0};
            layout_node->geometry.border = {0, 0, 0, 0};
            layout_node->border_color = 0x00000000;
            layout_node->mode = clever::layout::LayoutMode::InlineBlock;
            layout_node->display = clever::layout::DisplayType::InlineBlock;
            return layout_node;

        } else if (type == "range") {
            // Slider input: set default dimensions and parse min/max/value
            if (layout_node->specified_width < 0) layout_node->specified_width = 150;
            if (layout_node->specified_height < 0) layout_node->specified_height = 20;
            layout_node->is_range_input = true;
            layout_node->background_color = 0x00000000; // transparent — painting handled by painter

            std::string min_attr = get_attr(node, "min");
            std::string max_attr = get_attr(node, "max");
            std::string val_attr = get_attr(node, "value");
            if (!min_attr.empty()) { try { layout_node->input_range_min = std::stoi(min_attr); } catch (...) {} }
            if (!max_attr.empty()) { try { layout_node->input_range_max = std::stoi(max_attr); } catch (...) {} }
            if (!val_attr.empty()) { try { layout_node->input_range_value = std::stoi(val_attr); } catch (...) {} }

            // Clamp value to [min, max]
            if (layout_node->input_range_value < layout_node->input_range_min)
                layout_node->input_range_value = layout_node->input_range_min;
            if (layout_node->input_range_value > layout_node->input_range_max)
                layout_node->input_range_value = layout_node->input_range_max;

            // Range inputs should not have visible border or padding
            layout_node->mode = clever::layout::LayoutMode::InlineBlock;
            layout_node->display = clever::layout::DisplayType::InlineBlock;
            layout_node->geometry.padding = {0, 0, 0, 0};
            layout_node->geometry.border = {0, 0, 0, 0};
            return layout_node;

        } else if (type == "color") {
            // Color picker input: show a color swatch
            if (layout_node->specified_width < 0) layout_node->specified_width = 44;
            if (layout_node->specified_height < 0) layout_node->specified_height = 23;
            layout_node->is_color_input = true;
            layout_node->background_color = 0x00000000; // transparent — painting handled by painter

            // Parse value attribute as hex color (e.g., "#ff0000")
            std::string color_val = get_attr(node, "value");
            if (!color_val.empty() && color_val[0] == '#' && color_val.size() == 7) {
                try {
                    uint32_t rgb = static_cast<uint32_t>(std::stoul(color_val.substr(1), nullptr, 16));
                    layout_node->color_input_value = 0xFF000000 | rgb;
                } catch (...) {}
            }

            // Color inputs should not have visible border or padding — painter handles it
            layout_node->mode = clever::layout::LayoutMode::InlineBlock;
            layout_node->display = clever::layout::DisplayType::InlineBlock;
            layout_node->geometry.padding = {0, 0, 0, 0};
            layout_node->geometry.border = {0, 0, 0, 0};
            return layout_node;
        }

        layout_node->mode = clever::layout::LayoutMode::InlineBlock;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->geometry.padding = {4, 6, 4, 6};
        return layout_node;
    }

    // Handle <video> element (media placeholder with play button)
    if (tag_lower == "video") {
        // Parse width/height attributes
        std::string w_str = get_attr(node, "width");
        std::string h_str = get_attr(node, "height");
        float attr_w = 300, attr_h = 150; // default video dimensions
        if (!w_str.empty()) { try { attr_w = std::stof(w_str); } catch (...) {} }
        if (!h_str.empty()) { try { attr_h = std::stof(h_str); } catch (...) {} }

        if (layout_node->specified_width < 0) layout_node->specified_width = attr_w;
        if (layout_node->specified_height < 0) layout_node->specified_height = attr_h;

        layout_node->background_color = 0xFF000000; // black background
        layout_node->media_type = 1;
        layout_node->media_src = get_attr(node, "src");

        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->text_align = 1; // center

        // Add a centered play button triangle
        auto play_btn = std::make_unique<clever::layout::LayoutNode>();
        play_btn->is_text = true;
        play_btn->text_content = "\xE2\x96\xB6"; // U+25B6 BLACK RIGHT-POINTING TRIANGLE
        play_btn->font_size = std::min(attr_w, attr_h) * 0.2f;
        if (play_btn->font_size < 16) play_btn->font_size = 16;
        play_btn->color = 0xCCFFFFFF; // semi-transparent white
        play_btn->line_height = 1.2f;
        play_btn->text_align = 1; // center
        layout_node->append_child(std::move(play_btn));
        return layout_node;
    }

    // Handle <audio> element (media placeholder with controls)
    if (tag_lower == "audio") {
        // Check for controls attribute (boolean attribute)
        std::string controls_val = get_attr(node, "controls");
        bool has_controls = !controls_val.empty();
        if (!has_controls) {
            for (const auto& attr : node.attributes) {
                if (attr.name == "controls") { has_controls = true; break; }
            }
        }

        if (has_controls) {
            float attr_w = 300, attr_h = 32; // audio player bar dimensions
            std::string w_str = get_attr(node, "width");
            if (!w_str.empty()) { try { attr_w = std::stof(w_str); } catch (...) {} }

            if (layout_node->specified_width < 0) layout_node->specified_width = attr_w;
            if (layout_node->specified_height < 0) layout_node->specified_height = attr_h;

            layout_node->background_color = 0xFFF1F3F4; // light gray audio bar
            layout_node->media_type = 2;
            layout_node->media_src = get_attr(node, "src");
            layout_node->border_style = 1;
            layout_node->border_color = 0xFFDDDDDD;
            layout_node->geometry.border = {1, 1, 1, 1};
            layout_node->border_radius_tl = 4;
            layout_node->border_radius_tr = 4;
            layout_node->border_radius_bl = 4;
            layout_node->border_radius_br = 4;

            layout_node->mode = clever::layout::LayoutMode::Block;
            layout_node->display = clever::layout::DisplayType::InlineBlock;

            // Play icon and time indicator
            auto play_text = std::make_unique<clever::layout::LayoutNode>();
            play_text->is_text = true;
            play_text->text_content = "\xE2\x96\xB6  0:00 / 0:00";
            play_text->font_size = 12;
            play_text->color = 0xFF333333;
            play_text->line_height = 1.2f;
            play_text->geometry.margin.left = 16;
            play_text->geometry.margin.top = 16;
            layout_node->append_child(std::move(play_text));
        } else {
            // Per HTML spec: audio without controls is not rendered
            layout_node->mode = clever::layout::LayoutMode::None;
            layout_node->display = clever::layout::DisplayType::None;
        }
        return layout_node;
    }

    // Handle <button> element
    if (tag_lower == "button") {
        layout_node->is_button_input = true;
        if (layout_node->specified_height < 0) layout_node->specified_height = 26;
        bool dark_btn = (layout_node->color_scheme == 2);
        layout_node->background_color = dark_btn ? 0xFF1E1E1E : 0xFFE0E0E0;
        if (dark_btn) layout_node->color = 0xFFE0E0E0;
        layout_node->mode = clever::layout::LayoutMode::InlineBlock;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->geometry.padding = {2.0f, 6.0f, 2.0f, 6.0f};
        layout_node->geometry.border = {1, 1, 1, 1};
        if (dark_btn) {
            layout_node->border_color = 0xFF555555;
            layout_node->border_color_top = 0xFF555555;
            layout_node->border_color_right = 0xFF555555;
            layout_node->border_color_bottom = 0xFF555555;
            layout_node->border_color_left = 0xFF555555;
        }
        // Fall through to render children normally (button text)
    }

    // Handle <progress> element
    if (tag_lower == "progress") {
        layout_node->is_progress = true;

        float max_val = 1.0f;
        float cur_val = 0.0f;
        std::string max_attr = get_attr(node, "max");
        std::string val_attr = get_attr(node, "value");
        if (!max_attr.empty()) { try { max_val = std::stof(max_attr); } catch (...) {} }
        if (max_val <= 0) max_val = 1.0f;
        layout_node->progress_max = max_val;

        bool indeterminate = val_attr.empty();
        layout_node->progress_indeterminate = indeterminate;

        if (!indeterminate) {
            try { cur_val = std::stof(val_attr); } catch (...) {}
        }
        float ratio = indeterminate ? 0.0f : std::min(cur_val / max_val, 1.0f);
        layout_node->progress_value = ratio;

        float bar_w = 200.0f;
        float bar_h = 16.0f;
        if (layout_node->specified_width >= 0) bar_w = layout_node->specified_width;
        if (layout_node->specified_height >= 0) bar_h = layout_node->specified_height;
        layout_node->specified_width = bar_w;
        layout_node->specified_height = bar_h;
        bool dark_progress = (layout_node->color_scheme == 2);
        layout_node->background_color = dark_progress ? 0xFF333333 : 0xFFE0E0E0; // track
        layout_node->border_radius = 4.0f;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        // Determine fill color: use accent-color if set, else default blue
        uint32_t fill_color = 0xFF007AFFu; // default blue
        uint32_t ac = layout_node->accent_color;
        if (ac != 0) fill_color = ac;

        if (indeterminate) {
            // Indeterminate state: create 3 alternating colored stripes
            float stripe_w = bar_w / 3.0f;
            // Lighter variant of fill color
            uint8_t lr = static_cast<uint8_t>(std::min(255u, ((fill_color >> 16) & 0xFF) + 80u));
            uint8_t lg = static_cast<uint8_t>(std::min(255u, ((fill_color >> 8) & 0xFF) + 80u));
            uint8_t lb = static_cast<uint8_t>(std::min(255u, (fill_color & 0xFF) + 80u));
            uint32_t light_color = 0xFF000000 | (static_cast<uint32_t>(lr) << 16) |
                                   (static_cast<uint32_t>(lg) << 8) | lb;
            for (int i = 0; i < 3; ++i) {
                auto stripe = std::make_unique<clever::layout::LayoutNode>();
                stripe->specified_width = stripe_w;
                stripe->specified_height = bar_h;
                stripe->background_color = (i % 2 == 0) ? fill_color : light_color;
                stripe->border_radius = (i == 0 || i == 2) ? 4.0f : 0.0f;
                stripe->mode = clever::layout::LayoutMode::Block;
                stripe->display = clever::layout::DisplayType::InlineBlock;
                layout_node->append_child(std::move(stripe));
            }
        } else {
            // Determinate state: single fill child (the progress indicator)
            auto fill = std::make_unique<clever::layout::LayoutNode>();
            fill->specified_width = bar_w * ratio;
            fill->specified_height = bar_h;
            fill->background_color = fill_color;
            fill->border_radius = 4.0f;
            fill->mode = clever::layout::LayoutMode::Block;
            fill->display = clever::layout::DisplayType::Block;
            layout_node->append_child(std::move(fill));
        }
        return layout_node;
    }

    // Handle <meter> element
    if (tag_lower == "meter") {
        float min_val = 0.0f, max_val = 1.0f, cur_val = 0.0f;
        float low_val = -1, high_val = -1, optimum_val = -1;
        std::string attr;
        attr = get_attr(node, "min"); if (!attr.empty()) { try { min_val = std::stof(attr); } catch (...) {} }
        attr = get_attr(node, "max"); if (!attr.empty()) { try { max_val = std::stof(attr); } catch (...) {} }
        attr = get_attr(node, "value"); if (!attr.empty()) { try { cur_val = std::stof(attr); } catch (...) {} }
        attr = get_attr(node, "low"); if (!attr.empty()) { try { low_val = std::stof(attr); } catch (...) {} }
        attr = get_attr(node, "high"); if (!attr.empty()) { try { high_val = std::stof(attr); } catch (...) {} }
        attr = get_attr(node, "optimum"); if (!attr.empty()) { try { optimum_val = std::stof(attr); } catch (...) {} }

        // Default low/high/optimum per HTML spec
        if (low_val < 0) low_val = min_val;
        if (high_val < 0) high_val = max_val;
        if (optimum_val < 0) optimum_val = (min_val + max_val) / 2.0f;

        float range = max_val - min_val;
        float ratio = (range > 0) ? std::min(std::max((cur_val - min_val) / range, 0.0f), 1.0f) : 0.0f;

        // Color based on HTML spec algorithm using low/high/optimum:
        // Determine which region the optimum is in:
        //   optimum <= low:  optimum region is [min, low]
        //   optimum >= high: optimum region is [high, max]
        //   otherwise:       optimum region is [low, high]
        // Then color the value based on how far it is from the optimum region:
        //   value in optimum region -> green (good)
        //   value in suboptimal region (one step away) -> yellow
        //   value in even-less-good region (two steps away) -> red
        uint32_t fill_color = 0xFF4CAF50; // green (optimum)
        if (optimum_val <= low_val) {
            // Optimum is in [min, low] — lower values are better
            if (cur_val <= low_val) fill_color = 0xFF4CAF50;        // green: in optimum region
            else if (cur_val <= high_val) fill_color = 0xFFFFC107;  // yellow: suboptimal
            else fill_color = 0xFFF44336;                           // red: even-less-good
        } else if (optimum_val >= high_val) {
            // Optimum is in [high, max] — higher values are better
            if (cur_val >= high_val) fill_color = 0xFF4CAF50;       // green: in optimum region
            else if (cur_val >= low_val) fill_color = 0xFFFFC107;   // yellow: suboptimal
            else fill_color = 0xFFF44336;                           // red: even-less-good
        } else {
            // Optimum is in [low, high] — middle values are better
            if (cur_val >= low_val && cur_val <= high_val) fill_color = 0xFF4CAF50;  // green
            else fill_color = 0xFFFFC107;                                            // yellow: outside preferred
        }

        // Store meter properties on layout node
        layout_node->is_meter = true;
        layout_node->meter_value = cur_val;
        layout_node->meter_min = min_val;
        layout_node->meter_max = max_val;
        layout_node->meter_low = low_val;
        layout_node->meter_high = high_val;
        layout_node->meter_optimum = optimum_val;
        layout_node->meter_bar_color = fill_color;

        float bar_w = 200.0f, bar_h = 16.0f;
        if (layout_node->specified_width >= 0) bar_w = layout_node->specified_width;
        if (layout_node->specified_height >= 0) bar_h = layout_node->specified_height;
        layout_node->specified_width = bar_w;
        layout_node->specified_height = bar_h;
        bool dark_meter = (layout_node->color_scheme == 2);
        layout_node->background_color = dark_meter ? 0xFF333333 : 0xFFE0E0E0;
        layout_node->border_radius = 4.0f;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        auto fill = std::make_unique<clever::layout::LayoutNode>();
        fill->specified_width = bar_w * ratio;
        fill->specified_height = bar_h;
        fill->background_color = fill_color;
        fill->border_radius = 4.0f;
        fill->mode = clever::layout::LayoutMode::Block;
        fill->display = clever::layout::DisplayType::Block;
        layout_node->append_child(std::move(fill));
        return layout_node;
    }

    // Handle <textarea>
    if (tag_lower == "textarea") {
        layout_node->is_textarea = true;

        // Parse rows/cols for size defaults (~18px per line, ~7.5px per char column)
        int ta_rows = 2, ta_cols = 20;
        std::string rows_attr = get_attr(node, "rows");
        std::string cols_attr = get_attr(node, "cols");
        if (!rows_attr.empty()) { try { ta_rows = std::max(1, std::stoi(rows_attr)); } catch (...) {} }
        if (!cols_attr.empty()) { try { ta_cols = std::max(1, std::stoi(cols_attr)); } catch (...) {} }
        layout_node->textarea_rows = ta_rows;
        layout_node->textarea_cols = ta_cols;

        // Derive default size from rows/cols if CSS hasn't set explicit dimensions
        float ta_width  = static_cast<float>(ta_cols) * 7.5f + 12.0f;
        float ta_height = static_cast<float>(ta_rows) * 18.0f + 8.0f;
        if (layout_node->specified_width  < 0) layout_node->specified_width  = ta_width;
        if (layout_node->specified_height < 0) layout_node->specified_height = ta_height;

        bool dark_ta = (layout_node->color_scheme == 2);
        layout_node->background_color = dark_ta ? 0xFF1E1E1E : 0xFFFFFFFF;
        if (dark_ta) layout_node->color = 0xFFE0E0E0;
        layout_node->mode = clever::layout::LayoutMode::InlineBlock;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->geometry.padding = {4, 6, 4, 6};
        layout_node->geometry.border = {1, 1, 1, 1};
        layout_node->border_color = dark_ta ? 0xFF555555 : 0xFF767676;
        if (dark_ta) {
            layout_node->border_color_top    = 0xFF555555;
            layout_node->border_color_right  = 0xFF555555;
            layout_node->border_color_bottom = 0xFF555555;
            layout_node->border_color_left   = 0xFF555555;
        }

        // UA default cursor: text (I-beam) for textarea
        if (layout_node->cursor == 0) {
            layout_node->cursor = 3; // Cursor::Text
        }

        // Show textarea text content, or placeholder if empty
        std::string content_text = node.text_content();
        std::string placeholder_attr = get_attr(node, "placeholder");
        bool showing_placeholder = content_text.empty();
        std::string text = showing_placeholder ? placeholder_attr : content_text;
        layout_node->textarea_has_content = !content_text.empty();

        // Store placeholder and value on the layout node for inspection
        layout_node->placeholder_text = placeholder_attr;
        layout_node->input_value = content_text;

        if (!text.empty()) {
            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->text_content = text;
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->font_size = (layout_node->placeholder_font_size > 0)
                ? layout_node->placeholder_font_size : 13.0f;
            text_child->color = showing_placeholder ? layout_node->placeholder_color
                                                    : (dark_ta ? 0xFFE0E0E0 : layout_node->color);
            text_child->font_italic = showing_placeholder && layout_node->placeholder_italic;
            layout_node->append_child(std::move(text_child));
        }
        return layout_node;
    }

    // Handle <select>
    if (tag_lower == "select") {
        layout_node->is_select_element = true;
        if (layout_node->specified_width < 0) layout_node->specified_width = 150.0f;
        bool is_multiple = has_attr(node, "multiple");
        layout_node->select_is_multiple = is_multiple;
        // Parse size attribute for visible row count
        int visible_rows = is_multiple ? 4 : 1;
        std::string size_attr = get_attr(node, "size");
        if (!size_attr.empty()) {
            try { visible_rows = std::max(1, std::stoi(size_attr)); } catch (...) {}
        }
        layout_node->select_visible_rows = visible_rows;
        float row_h = 20.0f; // each option row is ~20px high
        if (is_multiple || visible_rows > 1) {
            // Listbox mode: show multiple options
            if (layout_node->specified_height < 0)
                layout_node->specified_height = static_cast<float>(visible_rows) * row_h + 4;
            layout_node->geometry.padding = {2, 6, 2, 6};
            layout_node->overflow = 1; // clip overflow
            layout_node->is_scroll_container = true;
        } else {
            // Dropdown mode (single select)
            if (layout_node->specified_height < 0) layout_node->specified_height = 20.0f;
            layout_node->geometry.padding = {2, 20, 2, 6}; // right padding for arrow
        }
        bool dark_sel = (layout_node->color_scheme == 2);
        layout_node->background_color = dark_sel ? 0xFF1E1E1E
                                        : (is_multiple ? 0xFFFFFFFF : 0xFFF8F8F8);
        layout_node->border_color = dark_sel ? 0xFF555555 : 0xFF767676;
        if (dark_sel) {
            layout_node->color = 0xFFE0E0E0;
            layout_node->border_color_top = 0xFF555555;
            layout_node->border_color_right = 0xFF555555;
            layout_node->border_color_bottom = 0xFF555555;
            layout_node->border_color_left = 0xFF555555;
        }
        layout_node->mode = clever::layout::LayoutMode::InlineBlock;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        layout_node->geometry.border = {1, 1, 1, 1};
        layout_node->font_size = 13.0f;

        // Helper lambda to extract text from an <option> element
        auto extract_option_text = [&](const clever::html::SimpleNode& opt_node) -> std::string {
            for (auto& text_node : opt_node.children) {
                if (text_node->type == clever::html::SimpleNode::Text &&
                    !text_node->data.empty()) {
                    return trim(text_node->data);
                }
            }
            return "";
        };

        // Store form field name for the select element
        layout_node->select_name = get_attr(node, "name");

        // Helper lambda to process an <option> element for selected text tracking
        // Also collects option text into select_options and tracks selected_index
        // Tracks per-option disabled flag in select_option_disabled
        int option_index = 0;
        int found_selected_index = -1;
        auto process_option = [&](const clever::html::SimpleNode& opt_node,
                                   std::string& first_opt_text,
                                   std::string& selected_opt_text,
                                   bool parent_optgroup_disabled = false) {
            std::string opt_text = extract_option_text(opt_node);
            // Collect option text for dropdown popup menu
            if (!opt_text.empty()) {
                layout_node->select_options.push_back(opt_text);
            } else {
                layout_node->select_options.push_back(" ");
            }
            // Track disabled state: option is disabled if it has disabled attr
            // or if its parent optgroup is disabled
            bool opt_disabled = has_attr(opt_node, "disabled") || parent_optgroup_disabled;
            layout_node->select_option_disabled.push_back(opt_disabled);

            if (first_opt_text.empty() && !opt_text.empty() && !opt_disabled) {
                first_opt_text = opt_text;
            }
            if (has_attr(opt_node, "selected") && !opt_text.empty()) {
                selected_opt_text = opt_text;
                found_selected_index = option_index;
            }
            option_index++;
        };

        // Find the selected option text: prefer option with "selected" attribute,
        // fall back to first <option> text.
        // Collect option data and optgroup metadata.
        std::string first_option_text;
        std::string selected_option_text;
        for (auto& child : node.children) {
            if (child->type != clever::html::SimpleNode::Element) continue;
            std::string child_tag = to_lower(child->tag_name);

            if (child_tag == "option") {
                process_option(*child, first_option_text, selected_option_text);
            } else if (child_tag == "optgroup") {
                bool og_disabled = has_attr(*child, "disabled");
                std::string og_label = get_attr(*child, "label");

                // Process <option> children within the <optgroup> for option tracking
                for (auto& opt_child : child->children) {
                    if (opt_child->type == clever::html::SimpleNode::Element &&
                        to_lower(opt_child->tag_name) == "option") {
                        process_option(*opt_child, first_option_text, selected_option_text, og_disabled);
                    }
                }

                // In dropdown mode, create optgroup metadata child nodes
                if (!is_multiple && visible_rows <= 1) {
                    auto optgroup_node = std::make_unique<clever::layout::LayoutNode>();
                    optgroup_node->tag_name = "optgroup";
                    optgroup_node->is_optgroup = true;
                    optgroup_node->optgroup_label = og_label;
                    optgroup_node->optgroup_disabled = og_disabled;
                    optgroup_node->mode = clever::layout::LayoutMode::Block;
                    optgroup_node->display = clever::layout::DisplayType::Block;
                    layout_node->append_child(std::move(optgroup_node));
                }
            }
        }

        // Helper lambda to create a listbox option child node
        auto make_listbox_option = [&](const clever::html::SimpleNode& opt_elem,
                                       bool parent_disabled) {
            std::string opt_text = extract_option_text(opt_elem);
            if (opt_text.empty()) opt_text = " ";
            bool opt_disabled = has_attr(opt_elem, "disabled") || parent_disabled;
            auto opt_node = std::make_unique<clever::layout::LayoutNode>();
            opt_node->mode = clever::layout::LayoutMode::Block;
            opt_node->display = clever::layout::DisplayType::Block;
            opt_node->specified_height = row_h;
            opt_node->geometry.padding = {1, 4, 1, 4};
            opt_node->font_size = 13.0f;
            opt_node->is_option_disabled = opt_disabled;
            opt_node->color = layout_node->color != 0 ? layout_node->color : 0xFF000000;
            if (opt_disabled) {
                // Disabled options render in gray
                opt_node->color = dark_sel ? 0xFF666666 : 0xFF999999;
            } else if (has_attr(opt_elem, "selected")) {
                opt_node->background_color = 0xFF3875D7;
                opt_node->color = 0xFFFFFFFF;
            }
            auto text_node = std::make_unique<clever::layout::LayoutNode>();
            text_node->text_content = opt_text;
            text_node->is_text = true;
            text_node->font_size = 13.0f;
            text_node->color = opt_node->color;
            opt_node->append_child(std::move(text_node));
            return opt_node;
        };

        if (is_multiple || visible_rows > 1) {
            // Listbox mode: create visible option children (including optgroup support)
            for (auto& child : node.children) {
                if (child->type != clever::html::SimpleNode::Element) continue;
                std::string child_tag = to_lower(child->tag_name);
                if (child_tag == "option") {
                    layout_node->append_child(make_listbox_option(*child, false));
                } else if (child_tag == "optgroup") {
                    bool og_disabled = has_attr(*child, "disabled");
                    std::string og_label = get_attr(*child, "label");
                    // Render optgroup label as a non-selectable header row
                    if (!og_label.empty()) {
                        auto label_node = std::make_unique<clever::layout::LayoutNode>();
                        label_node->mode = clever::layout::LayoutMode::Block;
                        label_node->display = clever::layout::DisplayType::Block;
                        label_node->specified_height = row_h;
                        label_node->geometry.padding = {1, 4, 1, 4};
                        label_node->font_size = 13.0f;
                        label_node->font_weight = 700;
                        label_node->is_optgroup = true;
                        label_node->optgroup_label = og_label;
                        label_node->optgroup_disabled = og_disabled;
                        label_node->color = layout_node->color != 0 ? layout_node->color : 0xFF000000;
                        if (og_disabled) {
                            label_node->color = dark_sel ? 0xFF666666 : 0xFF999999;
                        }
                        auto label_text = std::make_unique<clever::layout::LayoutNode>();
                        label_text->text_content = og_label;
                        label_text->is_text = true;
                        label_text->font_size = 13.0f;
                        label_text->font_weight = 700;
                        label_text->color = label_node->color;
                        label_node->append_child(std::move(label_text));
                        layout_node->append_child(std::move(label_node));
                    }
                    // Render options within the optgroup (indented)
                    for (auto& opt_child : child->children) {
                        if (opt_child->type == clever::html::SimpleNode::Element &&
                            to_lower(opt_child->tag_name) == "option") {
                            auto opt_node = make_listbox_option(*opt_child, og_disabled);
                            // Indent options inside optgroup
                            opt_node->geometry.padding = {1, 4, 1, 16};
                            layout_node->append_child(std::move(opt_node));
                        }
                    }
                }
            }
            layout_node->select_display_text = ""; // no dropdown text in listbox mode
        } else {
            layout_node->select_display_text =
                !selected_option_text.empty() ? selected_option_text :
                !first_option_text.empty() ? first_option_text : "Select...";
        }

        // Set the selected index (default to 0 if none explicitly selected)
        layout_node->select_selected_index = (found_selected_index >= 0) ? found_selected_index : 0;

        // In dropdown mode, painter draws the text and arrow
        // In listbox mode, options are rendered as children
        return layout_node;
    }

    // Handle <datalist> — not rendered visually, provides options for input[list]
    if (tag_lower == "datalist") {
        layout_node->is_datalist = true;
        // Get id attribute for linking input[list] to this datalist
        std::string dl_id = get_attr(node, "id");
        if (!dl_id.empty()) {
            layout_node->datalist_id = dl_id;
        }
        // Collect option values from children
        for (auto& child : node.children) {
            if (child->type == clever::html::SimpleNode::Element &&
                to_lower(child->tag_name) == "option") {
                std::string val = get_attr(*child, "value");
                if (!val.empty()) {
                    layout_node->datalist_options.push_back(val);
                }
            }
        }
        // Store in thread-local collection for RenderResult
        if (!dl_id.empty() && !layout_node->datalist_options.empty()) {
            collected_datalists[dl_id] = layout_node->datalist_options;
        }
        // Datalist is display:none by default — don't render it
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;
        return layout_node;
    }

    // Handle <details> / <summary>
    if (tag_lower == "details") {
        int this_details_id = g_details_id_counter++;
        bool is_open = false;
        for (auto& [k, v] : node.attributes) {
            if (to_lower(k) == "open") { is_open = true; break; }
        }
        // If this details element has been interactively toggled, flip its open state
        if (g_toggled_details && g_toggled_details->count(this_details_id)) {
            is_open = !is_open;
        }

        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->geometry.padding = {4, 8, 4, 8};
        layout_node->geometry.border = {1, 1, 1, 1};
        layout_node->border_color = 0xFFCBD5E0;
        layout_node->border_radius = 4.0f;

        // Only render <summary> (or first child) when closed
        for (auto& child : node.children) {
            if (child->type == clever::html::SimpleNode::Element) {
                std::string child_tag = to_lower(child->tag_name);
                if (child_tag == "summary" || is_open) {
                    auto child_layout = build_layout_tree_styled(
                        *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
                    if (child_layout) {
                        if (child_tag == "summary") {
                            // Mark the summary node and whether details is open
                            child_layout->is_summary = true;
                            child_layout->details_open = is_open;
                            child_layout->details_id = this_details_id;

                            // Prepend disclosure triangle
                            auto arrow = std::make_unique<clever::layout::LayoutNode>();
                            arrow->is_text = true;
                            arrow->text_content = is_open ? "\xE2\x96\xBC " : "\xE2\x96\xB6 "; // ▼ or ▶
                            arrow->mode = clever::layout::LayoutMode::Inline;
                            arrow->display = clever::layout::DisplayType::Inline;
                            arrow->font_size = font_size;
                            arrow->color = color_to_argb(style.color);
                            child_layout->children.insert(child_layout->children.begin(), std::move(arrow));
                        }
                        layout_node->append_child(std::move(child_layout));
                    }
                    if (!is_open && child_tag == "summary") break;
                }
            }
        }
        return layout_node;
    }

    // Handle <dialog> element
    if (tag_lower == "dialog") {
        layout_node->is_dialog = true;

        // Check for `open` and `data-modal` attributes
        bool has_open = false;
        bool has_modal = false;
        for (auto& [k, v] : node.attributes) {
            auto k_lower = to_lower(k);
            if (k_lower == "open") { has_open = true; }
            if (k_lower == "data-modal") { has_modal = true; }
        }

        if (!has_open) {
            // Closed dialog — hidden, takes no space
            layout_node->mode = clever::layout::LayoutMode::None;
            layout_node->display = clever::layout::DisplayType::None;
            return layout_node;
        }

        // Open dialog — centered overlay with default UA styling
        layout_node->dialog_open = true;
        layout_node->dialog_modal = has_modal;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->position_type = 2; // absolute

        // Default UA styles for <dialog>
        layout_node->geometry.border = {1, 1, 1, 1};
        layout_node->border_color = 0xFF999999;
        layout_node->border_style = 1; // solid
        layout_node->geometry.padding = {16, 16, 16, 16};
        layout_node->background_color = 0xFFFFFFFF; // white
        layout_node->specified_width = 600.0f;
        layout_node->max_width = 600.0f;

        // Recurse into children
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Handle <marquee> element — nostalgic HTML easter egg (static representation)
    if (tag_lower == "marquee") {
        layout_node->is_marquee = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;

        // Parse direction attribute: "left"(0), "right"(1), "up"(2), "down"(3)
        std::string dir_attr = get_attr(node, "direction");
        if (dir_attr == "right") layout_node->marquee_direction = 1;
        else if (dir_attr == "up") layout_node->marquee_direction = 2;
        else if (dir_attr == "down") layout_node->marquee_direction = 3;
        else layout_node->marquee_direction = 0; // default: left

        // Parse bgcolor attribute (hex or named colors)
        std::string bgcolor_attr = get_attr(node, "bgcolor");
        if (!bgcolor_attr.empty()) {
            uint32_t mqc = parse_html_color_attr(bgcolor_attr);
            if (mqc != 0) layout_node->marquee_bg_color = mqc;
        }

        // Ensure minimum height
        float min_h = layout_node->font_size * 1.5f;
        if (layout_node->min_height < min_h) layout_node->min_height = min_h;

        // Recurse into children (marquee can contain text and other elements)
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Handle <map> element — named image map container (display:none)
    if (tag_lower == "map") {
        layout_node->is_map = true;
        layout_node->map_name = get_attr(node, "name");
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;
        // Process child <area> elements so they appear in the layout tree
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Handle <area> element — clickable region within an image map (display:none)
    if (tag_lower == "area") {
        layout_node->is_area = true;
        layout_node->area_shape = get_attr(node, "shape");
        layout_node->area_coords = get_attr(node, "coords");
        layout_node->area_href = get_attr(node, "href");
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;
        return layout_node;
    }

    // Handle <canvas> element — render a placeholder since JS is not available
    if (tag_lower == "canvas") {
        layout_node->is_canvas = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        // Per HTML spec, default canvas dimensions are 300x150
        int cw = 300;
        int ch = 150;
        std::string canvas_w = get_attr(node, "width");
        std::string canvas_h = get_attr(node, "height");
        if (!canvas_w.empty()) {
            try { cw = std::stoi(canvas_w); } catch (...) {}
        }
        if (!canvas_h.empty()) {
            try { ch = std::stoi(canvas_h); } catch (...) {}
        }
        layout_node->canvas_width = cw;
        layout_node->canvas_height = ch;
        layout_node->specified_width = static_cast<float>(cw);
        layout_node->specified_height = static_cast<float>(ch);

        // Check for JS-created canvas buffer (stored as data-canvas-buffer-ptr attribute)
        std::string buf_ptr_str = get_attr(node, "data-canvas-buffer-ptr");
        if (!buf_ptr_str.empty()) {
            try {
                uint64_t ptr_val = std::stoull(buf_ptr_str);
                auto* vec_ptr = reinterpret_cast<std::vector<uint8_t>*>(
                    static_cast<uintptr_t>(ptr_val));
                if (vec_ptr && !vec_ptr->empty()) {
                    // Wrap the existing vector in a shared_ptr with a no-op deleter
                    // so we don't double-free (JS side owns the buffer)
                    layout_node->canvas_buffer = std::shared_ptr<std::vector<uint8_t>>(
                        vec_ptr, [](std::vector<uint8_t>*) { /* no-op: JS owns buffer */ });
                }
            } catch (...) {}
        }

        // Transparent/white background (canvas is a blank drawing surface)
        layout_node->background_color = 0xFFFFFFFF;

        return layout_node;
    }

    // Handle <iframe> element — render a placeholder (nested browsing contexts not yet supported)
    if (tag_lower == "iframe") {
        layout_node->is_iframe = true;
        std::string loading = get_attr(node, "loading");
        if (to_lower(loading) == "lazy") {
            layout_node->loading_lazy = true;
        }
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        // Per HTML spec, default iframe dimensions are 300x150
        float iw = 300, ih = 150;
        std::string iframe_w = get_attr(node, "width");
        std::string iframe_h = get_attr(node, "height");
        if (!iframe_w.empty()) {
            try { iw = std::stof(iframe_w); } catch (...) {}
        }
        if (!iframe_h.empty()) {
            try { ih = std::stof(iframe_h); } catch (...) {}
        }

        if (layout_node->specified_width < 0) layout_node->specified_width = iw;
        if (layout_node->specified_height < 0) layout_node->specified_height = ih;

        layout_node->iframe_src = get_attr(node, "src");
        if (layout_node->loading_lazy && !layout_node->iframe_src.empty()) {
            layout_node->lazy_iframe_url = layout_node->iframe_src;
            layout_node->iframe_src.clear();
        }

        // Light gray background (#F0F0F0) with thin #CCC border
        layout_node->background_color = 0xFFF0F0F0;
        layout_node->border_style = 1; // solid
        layout_node->border_color = 0xFFCCCCCC;
        layout_node->geometry.border = {1, 1, 1, 1};

        return layout_node;
    }

    // Handle <embed> element — placeholder (plugin content not supported)
    if (tag_lower == "embed") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        float ew = 300, eh = 150;
        std::string embed_w = get_attr(node, "width");
        std::string embed_h = get_attr(node, "height");
        if (!embed_w.empty()) { try { ew = std::stof(embed_w); } catch (...) {} }
        if (!embed_h.empty()) { try { eh = std::stof(embed_h); } catch (...) {} }
        if (layout_node->specified_width < 0) layout_node->specified_width = ew;
        if (layout_node->specified_height < 0) layout_node->specified_height = eh;
        layout_node->background_color = 0xFFF5F5F5;
        layout_node->border_style = 1;
        layout_node->border_color = 0xFFDDDDDD;
        layout_node->geometry.border = {1, 1, 1, 1};
        return layout_node;
    }

    // Handle <object> element — placeholder (plugin content not supported)
    if (tag_lower == "object") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
        float ow = 300, oh = 150;
        std::string obj_w = get_attr(node, "width");
        std::string obj_h = get_attr(node, "height");
        if (!obj_w.empty()) { try { ow = std::stof(obj_w); } catch (...) {} }
        if (!obj_h.empty()) { try { oh = std::stof(obj_h); } catch (...) {} }
        if (layout_node->specified_width < 0) layout_node->specified_width = ow;
        if (layout_node->specified_height < 0) layout_node->specified_height = oh;
        layout_node->background_color = 0xFFF5F5F5;
        layout_node->border_style = 1;
        layout_node->border_color = 0xFFDDDDDD;
        layout_node->geometry.border = {1, 1, 1, 1};
        // Render fallback children (content between <object> tags)
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) layout_node->append_child(std::move(child_layout));
        }
        return layout_node;
    }

    // Handle SVG elements
    if (tag_lower == "svg") {
        layout_node->is_svg = true;
        layout_node->svg_type = 0; // container
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::InlineBlock;

        // Parse width/height from attributes
        std::string svg_w = get_attr(node, "width");
        std::string svg_h = get_attr(node, "height");
        if (!svg_w.empty()) {
            try { layout_node->specified_width = std::stof(svg_w); } catch (...) {}
        }
        if (!svg_h.empty()) {
            try { layout_node->specified_height = std::stof(svg_h); } catch (...) {}
        }

        // Parse viewBox attribute: "minX minY width height"
        std::string viewbox = get_attr(node, "viewBox");
        if (viewbox.empty()) viewbox = get_attr(node, "viewbox");
        if (!viewbox.empty()) {
            // Replace commas with spaces for uniform parsing
            for (auto& ch : viewbox) { if (ch == ',') ch = ' '; }
            std::istringstream vb(viewbox);
            float vx = 0, vy = 0, vw = 0, vh = 0;
            if (vb >> vx >> vy >> vw >> vh && vw > 0 && vh > 0) {
                layout_node->svg_has_viewbox = true;
                layout_node->svg_viewbox_x = vx;
                layout_node->svg_viewbox_y = vy;
                layout_node->svg_viewbox_w = vw;
                layout_node->svg_viewbox_h = vh;
                // If no explicit width/height, derive from viewBox
                if (svg_w.empty() && svg_h.empty()) {
                    layout_node->specified_width = vw;
                    layout_node->specified_height = vh;
                } else if (svg_w.empty()) {
                    // Width from aspect ratio
                    layout_node->specified_width = layout_node->specified_height * (vw / vh);
                } else if (svg_h.empty()) {
                    // Height from aspect ratio
                    layout_node->specified_height = layout_node->specified_width * (vh / vw);
                }
            }
        }

        // Recursively build SVG children
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }

        // Collect SVG gradient definitions from all descendants (defs/linearGradient/radialGradient)
        std::function<void(const clever::layout::LayoutNode&)> collect_gradients =
            [&](const clever::layout::LayoutNode& n) {
                for (auto& [id, grad] : n.svg_gradient_defs) {
                    layout_node->svg_gradient_defs[id] = grad;
                }
                for (auto& c : n.children) collect_gradients(*c);
            };
        for (auto& c : layout_node->children) collect_gradients(*c);

        // Serialize the inline SVG for rasterization
        layout_node->svg_content = serialize_svg_node(*layout_node);

        return layout_node;
    }

    // Handle SVG <g> group elements
    if (tag_lower == "g") {
        layout_node->is_svg = true;
        layout_node->is_svg_group = true;
        layout_node->svg_type = 0; // group, not a shape
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;

        // Parse transform attribute (translate, scale, rotate)
        auto transform = get_attr(node, "transform");
        if (!transform.empty()) {
            // Helper to extract params from a function call
            auto extract_params = [&](const std::string& s, const std::string& func) -> std::string {
                auto pos = s.find(func + "(");
                if (pos == std::string::npos) return "";
                auto start = pos + func.size() + 1;
                auto end = s.find(')', start);
                if (end == std::string::npos) return "";
                return s.substr(start, end - start);
            };
            auto split_param = [](const std::string& p) -> std::pair<float, float> {
                float a = 0, b = 0;
                auto sep = p.find(',');
                if (sep == std::string::npos) sep = p.find(' ');
                if (sep != std::string::npos) {
                    try { a = std::stof(p.substr(0, sep)); } catch (...) {}
                    try { b = std::stof(p.substr(sep + 1)); } catch (...) {}
                } else {
                    try { a = std::stof(p); } catch (...) {}
                    b = a; // single value: same for both
                }
                return {a, b};
            };

            auto tp = extract_params(transform, "translate");
            if (!tp.empty()) {
                auto [tx, ty] = split_param(tp);
                // For translate, single value means ty=0
                if (tp.find(',') == std::string::npos && tp.find(' ') == std::string::npos) ty = 0;
                layout_node->svg_transform_tx = tx;
                layout_node->svg_transform_ty = ty;
            }

            auto sp = extract_params(transform, "scale");
            if (!sp.empty()) {
                auto [sx, sy] = split_param(sp);
                layout_node->svg_transform_sx = sx;
                layout_node->svg_transform_sy = sy;
            }

            auto rp = extract_params(transform, "rotate");
            if (!rp.empty()) {
                try { layout_node->svg_transform_rotate = std::stof(rp); } catch (...) {}
            }
        }

        // Recursively build children (same as SVG container)
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Handle SVG <linearGradient> / <radialGradient> — store as gradient definitions
    if (tag_lower == "lineargradient" || tag_lower == "radialgradient") {
        layout_node->is_svg = true;
        layout_node->svg_type = 0;
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;

        // Parse gradient attributes and stops, store on nearest SVG ancestor
        clever::layout::LayoutNode::SvgGradient grad;
        grad.is_radial = (tag_lower == "radialgradient");
        std::string grad_id = get_attr(node, "id");

        if (!grad.is_radial) {
            auto x1s = get_attr(node, "x1"), y1s = get_attr(node, "y1");
            auto x2s = get_attr(node, "x2"), y2s = get_attr(node, "y2");
            if (!x1s.empty()) { try { grad.x1 = std::stof(x1s); if (x1s.back() == '%') grad.x1 /= 100.0f; } catch (...) {} }
            if (!y1s.empty()) { try { grad.y1 = std::stof(y1s); if (y1s.back() == '%') grad.y1 /= 100.0f; } catch (...) {} }
            if (!x2s.empty()) { try { grad.x2 = std::stof(x2s); if (x2s.back() == '%') grad.x2 /= 100.0f; } catch (...) {} }
            if (!y2s.empty()) { try { grad.y2 = std::stof(y2s); if (y2s.back() == '%') grad.y2 /= 100.0f; } catch (...) {} }
        } else {
            auto cxs = get_attr(node, "cx"), cys = get_attr(node, "cy"), rs = get_attr(node, "r");
            if (!cxs.empty()) { try { grad.cx = std::stof(cxs); if (cxs.back() == '%') grad.cx /= 100.0f; } catch (...) {} }
            if (!cys.empty()) { try { grad.cy = std::stof(cys); if (cys.back() == '%') grad.cy /= 100.0f; } catch (...) {} }
            if (!rs.empty()) { try { grad.r = std::stof(rs); if (rs.back() == '%') grad.r /= 100.0f; } catch (...) {} }
        }

        // Parse <stop> children
        for (auto& child : node.children) {
            if (to_lower(child->tag_name) == "stop") {
                float offset = 0;
                uint32_t color = 0xFF000000;
                auto offset_str = get_attr(*child, "offset");
                if (!offset_str.empty()) {
                    try {
                        offset = std::stof(offset_str);
                        if (offset_str.back() == '%') offset /= 100.0f;
                    } catch (...) {}
                }
                auto stop_color = get_attr(*child, "stop-color");
                if (!stop_color.empty()) {
                    auto c = clever::css::parse_color(stop_color);
                    if (c) {
                        color = (static_cast<uint32_t>(c->a) << 24) |
                                (static_cast<uint32_t>(c->r) << 16) |
                                (static_cast<uint32_t>(c->g) << 8) |
                                static_cast<uint32_t>(c->b);
                    }
                }
                // Check stop-opacity
                auto stop_opacity = get_attr(*child, "stop-opacity");
                if (!stop_opacity.empty()) {
                    try {
                        float op = std::stof(stop_opacity);
                        uint8_t a = static_cast<uint8_t>(op * 255.0f);
                        color = (static_cast<uint32_t>(a) << 24) | (color & 0x00FFFFFF);
                    } catch (...) {}
                }
                // Also check style attribute for stop-color/stop-opacity
                auto style_attr = get_attr(*child, "style");
                if (!style_attr.empty()) {
                    auto sc_pos = style_attr.find("stop-color:");
                    if (sc_pos != std::string::npos) {
                        auto sc_start = sc_pos + 11;
                        auto sc_end = style_attr.find(';', sc_start);
                        auto sc_val = style_attr.substr(sc_start, sc_end == std::string::npos ? std::string::npos : sc_end - sc_start);
                        while (!sc_val.empty() && sc_val.front() == ' ') sc_val.erase(sc_val.begin());
                        while (!sc_val.empty() && sc_val.back() == ' ') sc_val.pop_back();
                        auto c = clever::css::parse_color(sc_val);
                        if (c) {
                            color = (static_cast<uint32_t>(c->a) << 24) |
                                    (static_cast<uint32_t>(c->r) << 16) |
                                    (static_cast<uint32_t>(c->g) << 8) |
                                    static_cast<uint32_t>(c->b);
                        }
                    }
                }
                grad.stops.push_back({color, offset});
            }
        }

        // Store gradient def on the layout_node (will be collected by SVG parent)
        if (!grad_id.empty() && grad.stops.size() >= 2) {
            layout_node->svg_gradient_defs[grad_id] = std::move(grad);
        }
        return layout_node;
    }

    // Handle SVG <defs> — container for reusable definitions, not rendered directly
    if (tag_lower == "defs") {
        layout_node->is_svg = true;
        layout_node->is_svg_defs = true;
        layout_node->svg_type = 0; // container, not a shape
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;

        // Recursively build children so they get IDs, but mark as not rendered
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                layout_node->append_child(std::move(child_layout));
            }
        }
        return layout_node;
    }

    // Handle SVG <use> — reference to a defined element
    if (tag_lower == "use") {
        layout_node->is_svg = true;
        layout_node->is_svg_use = true;
        layout_node->svg_type = 0; // not a shape itself
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;

        // Parse href or xlink:href attribute for the reference ID
        std::string href = get_attr(node, "href");
        if (href.empty()) {
            href = get_attr(node, "xlink:href");
        }
        layout_node->svg_use_href = href;

        // Parse x and y position offsets
        std::string use_x = get_attr(node, "x");
        std::string use_y = get_attr(node, "y");
        if (!use_x.empty()) {
            try { layout_node->svg_use_x = std::stof(use_x); } catch (...) {}
        }
        if (!use_y.empty()) {
            try { layout_node->svg_use_y = std::stof(use_y); } catch (...) {}
        }

        return layout_node;
    }

    // Handle SVG shape elements
    if (tag_lower == "rect" || tag_lower == "circle" || tag_lower == "ellipse" ||
        tag_lower == "line" || tag_lower == "text" || tag_lower == "tspan" ||
        tag_lower == "polygon" || tag_lower == "polyline" || tag_lower == "path") {
        // Only treat as SVG if inside an SVG container (check parent context)
        // For now, always treat these tags as SVG shapes when encountered
        layout_node->is_svg = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;

        // Determine svg_type
        if (tag_lower == "rect") layout_node->svg_type = 1;
        else if (tag_lower == "circle") layout_node->svg_type = 2;
        else if (tag_lower == "ellipse") layout_node->svg_type = 3;
        else if (tag_lower == "line") layout_node->svg_type = 4;
        else if (tag_lower == "path") layout_node->svg_type = 5;
        else if (tag_lower == "text") layout_node->svg_type = 6;
        else if (tag_lower == "tspan") layout_node->svg_type = 9; // tspan: text span
        else if (tag_lower == "polygon") layout_node->svg_type = 7;
        else if (tag_lower == "polyline") layout_node->svg_type = 8;

        // Parse fill color from HTML attribute
        std::string fill_str = get_attr(node, "fill");
        if (!fill_str.empty() && fill_str != "none") {
            // Check for gradient reference: fill="url(#gradientId)"
            if (fill_str.substr(0, 4) == "url(" && fill_str.back() == ')') {
                std::string ref = fill_str.substr(4, fill_str.size() - 5);
                while (!ref.empty() && ref.front() == ' ') ref.erase(ref.begin());
                while (!ref.empty() && ref.back() == ' ') ref.pop_back();
                if (!ref.empty() && ref.front() == '#') ref = ref.substr(1);
                layout_node->svg_fill_gradient_id = ref;
            } else {
                auto fill_c = clever::css::parse_color(fill_str);
                if (fill_c) {
                    layout_node->background_color = color_to_argb(*fill_c);
                    layout_node->svg_fill_color = color_to_argb(*fill_c);
                    layout_node->svg_fill_none = false;
                }
            }
        } else if (fill_str == "none") {
            layout_node->background_color = 0x00000000; // transparent
            layout_node->svg_fill_none = true;
        } else {
            // SVG default fill is black
            layout_node->background_color = 0xFF000000;
            layout_node->svg_fill_color = 0xFF000000;
            layout_node->svg_fill_none = false;
        }

        // Parse stroke color from HTML attribute
        std::string stroke_str = get_attr(node, "stroke");
        if (!stroke_str.empty() && stroke_str != "none") {
            auto stroke_c = clever::css::parse_color(stroke_str);
            if (stroke_c) {
                layout_node->border_color = color_to_argb(*stroke_c);
                layout_node->svg_stroke_color = color_to_argb(*stroke_c);
                layout_node->svg_stroke_none = false;
            }
        } else if (stroke_str == "none") {
            layout_node->border_color = 0x00000000; // no stroke
            layout_node->svg_stroke_none = true;
        } else {
            layout_node->border_color = 0x00000000; // no stroke by default
            layout_node->svg_stroke_none = true;
        }

        // Parse fill-opacity from HTML attribute
        std::string fill_opacity_str = get_attr(node, "fill-opacity");
        if (!fill_opacity_str.empty()) {
            try { layout_node->svg_fill_opacity = std::clamp(std::stof(fill_opacity_str), 0.0f, 1.0f); } catch (...) {}
        }

        // Parse stroke-opacity from HTML attribute
        std::string stroke_opacity_str = get_attr(node, "stroke-opacity");
        if (!stroke_opacity_str.empty()) {
            try { layout_node->svg_stroke_opacity = std::clamp(std::stof(stroke_opacity_str), 0.0f, 1.0f); } catch (...) {}
        }

        // CSS inline style overrides HTML attributes for SVG fill/stroke
        if (!style_attr.empty()) {
            auto svg_decls = parse_inline_style(style_attr);
            for (auto& d : svg_decls) {
                std::string val_lower = to_lower(d.value);
                if (d.property == "fill") {
                    if (val_lower == "none") {
                        layout_node->svg_fill_none = true;
                    } else {
                        auto col = clever::css::parse_color(d.value);
                        if (col) { layout_node->svg_fill_color = color_to_argb(*col); layout_node->svg_fill_none = false; }
                    }
                } else if (d.property == "stroke") {
                    if (val_lower == "none") {
                        layout_node->svg_stroke_none = true;
                    } else {
                        auto col = clever::css::parse_color(d.value);
                        if (col) { layout_node->svg_stroke_color = color_to_argb(*col); layout_node->svg_stroke_none = false; }
                    }
                } else if (d.property == "fill-opacity") {
                    try { layout_node->svg_fill_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
                } else if (d.property == "stroke-opacity") {
                    try { layout_node->svg_stroke_opacity = std::clamp(std::stof(val_lower), 0.0f, 1.0f); } catch(...) {}
                } else if (d.property == "stroke-dasharray") {
                    if (val_lower != "none") {
                        layout_node->svg_stroke_dasharray.clear();
                        std::string da_val = val_lower;
                        for (auto& ch : da_val) { if (ch == ',') ch = ' '; }
                        std::istringstream da_ss(da_val);
                        float dv;
                        while (da_ss >> dv) layout_node->svg_stroke_dasharray.push_back(dv);
                    }
                } else if (d.property == "stroke-dashoffset") {
                    try { layout_node->svg_stroke_dashoffset = std::stof(val_lower); } catch(...) {}
                } else if (d.property == "stroke-linecap") {
                    if (val_lower == "butt") layout_node->svg_stroke_linecap = 0;
                    else if (val_lower == "round") layout_node->svg_stroke_linecap = 1;
                    else if (val_lower == "square") layout_node->svg_stroke_linecap = 2;
                } else if (d.property == "stroke-linejoin") {
                    if (val_lower == "miter") layout_node->svg_stroke_linejoin = 0;
                    else if (val_lower == "round") layout_node->svg_stroke_linejoin = 1;
                    else if (val_lower == "bevel") layout_node->svg_stroke_linejoin = 2;
                }
            }
        }

        // Parse stroke-width
        float stroke_w = 0;
        std::string sw_str = get_attr(node, "stroke-width");
        if (!sw_str.empty()) {
            try { stroke_w = std::stof(sw_str); } catch (...) {}
        } else if (!stroke_str.empty() && stroke_str != "none") {
            stroke_w = 1.0f; // default stroke-width when stroke is specified
        }

        // Parse stroke-linecap
        std::string linecap_str = get_attr(node, "stroke-linecap");
        if (linecap_str == "round") layout_node->svg_stroke_linecap = 1;
        else if (linecap_str == "square") layout_node->svg_stroke_linecap = 2;

        // Parse stroke-linejoin
        std::string linejoin_str = get_attr(node, "stroke-linejoin");
        if (linejoin_str == "round") layout_node->svg_stroke_linejoin = 1;
        else if (linejoin_str == "bevel") layout_node->svg_stroke_linejoin = 2;

        // Parse stroke-dasharray
        std::string dasharray_str = get_attr(node, "stroke-dasharray");
        if (!dasharray_str.empty() && dasharray_str != "none") {
            // Replace commas with spaces for uniform parsing
            for (auto& ch : dasharray_str) { if (ch == ',') ch = ' '; }
            std::istringstream da_ss(dasharray_str);
            float dv;
            while (da_ss >> dv) {
                layout_node->svg_stroke_dasharray.push_back(dv);
            }
        }

        // Parse stroke-dashoffset
        std::string dashoffset_str = get_attr(node, "stroke-dashoffset");
        if (!dashoffset_str.empty()) {
            try { layout_node->svg_stroke_dashoffset = std::stof(dashoffset_str); } catch (...) {}
        }

        // Store attributes in svg_attrs based on type
        // rect (1): [x, y, width, height, stroke_width]
        // circle (2): [cx, cy, r, stroke_width]
        // ellipse (3): [cx, cy, rx, ry, stroke_width]
        // line (4): [x1, y1, x2, y2, stroke_width]

        auto get_float_attr = [&](const std::string& name, float def = 0) -> float {
            std::string val = get_attr(node, name);
            if (val.empty()) return def;
            try { return std::stof(val); } catch (...) { return def; }
        };

        if (layout_node->svg_type == 1) { // rect
            float rx = get_float_attr("x");
            float ry = get_float_attr("y");
            float rw = get_float_attr("width");
            float rh = get_float_attr("height");
            layout_node->svg_attrs = {rx, ry, rw, rh, stroke_w};
            layout_node->specified_width = rw;
            layout_node->specified_height = rh;
        } else if (layout_node->svg_type == 2) { // circle
            float ccx = get_float_attr("cx");
            float ccy = get_float_attr("cy");
            float cr = get_float_attr("r");
            layout_node->svg_attrs = {ccx, ccy, cr, stroke_w};
        } else if (layout_node->svg_type == 3) { // ellipse
            float ecx = get_float_attr("cx");
            float ecy = get_float_attr("cy");
            float erx = get_float_attr("rx");
            float ery = get_float_attr("ry");
            layout_node->svg_attrs = {ecx, ecy, erx, ery, stroke_w};
        } else if (layout_node->svg_type == 4) { // line
            float lx1 = get_float_attr("x1");
            float ly1 = get_float_attr("y1");
            float lx2 = get_float_attr("x2");
            float ly2 = get_float_attr("y2");
            layout_node->svg_attrs = {lx1, ly1, lx2, ly2, stroke_w};
        } else if (layout_node->svg_type == 5) { // path
            std::string d_attr = get_attr(node, "d");
            layout_node->svg_path_d = d_attr;
            layout_node->svg_attrs = {stroke_w};
        } else if (layout_node->svg_type == 6) { // text
            layout_node->svg_text_x = get_float_attr("x");
            layout_node->svg_text_y = get_float_attr("y");
            layout_node->svg_text_dx = get_float_attr("dx");
            layout_node->svg_text_dy = get_float_attr("dy");

            // Parse font-size attribute (default 16)
            std::string fs_str = get_attr(node, "font-size");
            if (!fs_str.empty()) {
                try { layout_node->svg_font_size = std::stof(fs_str); } catch (...) {}
            }

            // Parse text-anchor attribute
            std::string anchor = get_attr(node, "text-anchor");
            if (anchor == "middle") layout_node->svg_text_anchor = 1;
            else if (anchor == "end") layout_node->svg_text_anchor = 2;

            // Parse dominant-baseline attribute
            std::string db = get_attr(node, "dominant-baseline");
            if (db == "middle") layout_node->svg_dominant_baseline = 1;
            else if (db == "hanging") layout_node->svg_dominant_baseline = 2;
            else if (db == "central") layout_node->svg_dominant_baseline = 3;
            else if (db == "text-top" || db == "text-before-edge") layout_node->svg_dominant_baseline = 4;

            // Parse font-family
            std::string ff = get_attr(node, "font-family");
            if (!ff.empty()) layout_node->svg_font_family = ff;

            // Parse font-weight
            std::string fw = get_attr(node, "font-weight");
            if (fw == "bold" || fw == "700") layout_node->svg_font_weight = 700;
            else if (fw == "normal" || fw == "400") layout_node->svg_font_weight = 400;
            else if (!fw.empty()) {
                try { layout_node->svg_font_weight = std::stoi(fw); } catch (...) {}
            }

            // Parse font-style
            std::string fst = get_attr(node, "font-style");
            if (fst == "italic" || fst == "oblique") layout_node->svg_font_italic = true;

            // Fallback: use CSS cascade font properties if no SVG attribute was set
            if (ff.empty() && !style.font_family.empty())
                layout_node->svg_font_family = style.font_family;
            if (fw.empty() && style.font_weight != 400)
                layout_node->svg_font_weight = style.font_weight;
            if (fst.empty() && style.font_style != clever::css::FontStyle::Normal)
                layout_node->svg_font_italic = true;
            // Use cascade font-size if no SVG attribute set
            if (fs_str.empty() && style.font_size.value > 0)
                layout_node->svg_font_size = style.font_size.value;

            // Extract direct text content (not from tspan children)
            // If there are <tspan> children, only get the text before them
            {
                std::string direct_text;
                bool has_tspan = false;
                for (auto& child : node.children) {
                    if (child->type == clever::html::SimpleNode::Text) {
                        direct_text += child->data;
                    } else if (child->type == clever::html::SimpleNode::Element &&
                               child->tag_name == "tspan") {
                        has_tspan = true;
                    }
                }
                if (!has_tspan) {
                    layout_node->svg_text_content = node.text_content();
                } else {
                    layout_node->svg_text_content = direct_text;
                }
            }

            // Build <tspan> children as layout nodes
            for (auto& child : node.children) {
                if (child->type == clever::html::SimpleNode::Element) {
                    auto child_layout = build_layout_tree_styled(
                        *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
                    if (child_layout) {
                        layout_node->append_child(std::move(child_layout));
                    }
                }
            }
        } else if (layout_node->svg_type == 9) { // tspan
            // <tspan> inherits parent <text> positioning but can override
            layout_node->svg_text_x = get_float_attr("x");
            layout_node->svg_text_y = get_float_attr("y");
            layout_node->svg_text_dx = get_float_attr("dx");
            layout_node->svg_text_dy = get_float_attr("dy");

            // Parse font properties (same as <text>)
            std::string fs_str = get_attr(node, "font-size");
            if (!fs_str.empty()) {
                try { layout_node->svg_font_size = std::stof(fs_str); } catch (...) {}
            } else if (style.font_size.value > 0) {
                layout_node->svg_font_size = style.font_size.value;
            }

            std::string ff = get_attr(node, "font-family");
            if (!ff.empty()) layout_node->svg_font_family = ff;
            else if (!style.font_family.empty()) layout_node->svg_font_family = style.font_family;

            std::string fw = get_attr(node, "font-weight");
            if (fw == "bold" || fw == "700") layout_node->svg_font_weight = 700;
            else if (!fw.empty()) {
                try { layout_node->svg_font_weight = std::stoi(fw); } catch (...) {}
            } else if (style.font_weight != 400) {
                layout_node->svg_font_weight = style.font_weight;
            }

            std::string fst = get_attr(node, "font-style");
            if (fst == "italic" || fst == "oblique") layout_node->svg_font_italic = true;

            // Extract direct text content of this tspan only
            layout_node->svg_text_content = node.text_content();
        } else if (layout_node->svg_type == 7 || layout_node->svg_type == 8) { // polygon/polyline
            std::string pts_attr = get_attr(node, "points");
            layout_node->svg_attrs = {stroke_w};
            if (!pts_attr.empty()) {
                std::istringstream ss(pts_attr);
                std::string pair;
                while (ss >> pair) {
                    auto comma = pair.find(',');
                    if (comma != std::string::npos) {
                        try {
                            float px = std::stof(pair.substr(0, comma));
                            float py = std::stof(pair.substr(comma + 1));
                            layout_node->svg_points.push_back({px, py});
                        } catch (...) {}
                    }
                }
            }
        }

        return layout_node;
    }

    // Handle <pre> — preformatted text with monospace font
    if (tag_lower == "pre") {
        style.font_family = "monospace";
        layout_node->font_family = "monospace";
        layout_node->is_monospace = true;
        // Only default to white-space:pre if CSS didn't set a value
        if (style.white_space == clever::css::WhiteSpace::Normal) {
            style.white_space = clever::css::WhiteSpace::Pre;
        }
    }

    // Legacy <nobr> and nowrap HTML attributes force no wrapping.
    if (tag_lower == "nobr" || has_attr(node, "nowrap")) {
        style.white_space = clever::css::WhiteSpace::NoWrap;
    }

    // Handle <code>, <samp>, <tt> — monospace font with subtle background
    if (tag_lower == "code" || tag_lower == "samp" || tag_lower == "tt") {
        style.font_family = "monospace";
        layout_node->font_family = "monospace";
        layout_node->is_monospace = true;
        layout_node->font_size *= 0.9f;  // slightly smaller for inline code
        style.font_size = clever::css::Length::px(layout_node->font_size);
        // Light gray background for inline code
        if (layout_node->background_color == 0x00000000) {
            layout_node->background_color = 0xFFF5F5F5;
        }
        // Inline by default
        if (style.display == clever::css::Display::Block) {
            style.display = clever::css::Display::Inline;
            layout_node->mode = clever::layout::LayoutMode::Inline;
            layout_node->display = clever::layout::DisplayType::Inline;
        }
    }

    // Handle <kbd> — keyboard input styling with border and background
    if (tag_lower == "kbd") {
        style.font_family = "monospace";
        layout_node->font_family = "monospace";
        layout_node->is_monospace = true;
        layout_node->is_kbd = true;
        layout_node->font_size *= 0.9f;
        style.font_size = clever::css::Length::px(layout_node->font_size);
        // Border: 1px solid #ccc
        layout_node->geometry.border = {1, 1, 1, 1};
        layout_node->border_color = 0xFFCCCCCC;
        layout_node->border_style = 1; // solid
        // Padding: 2px top/bottom, 4px left/right
        layout_node->geometry.padding = {2, 4, 2, 4};
        // Border radius
        layout_node->border_radius = 3;
        // Background
        if (layout_node->background_color == 0x00000000) {
            layout_node->background_color = 0xFFF7F7F7;
        }
        // Inline by default
        if (style.display == clever::css::Display::Block) {
            style.display = clever::css::Display::Inline;
            layout_node->mode = clever::layout::LayoutMode::Inline;
            layout_node->display = clever::layout::DisplayType::Inline;
        }
    }

    // Handle <var> — variable/placeholder, italic style
    if (tag_lower == "var") {
        layout_node->font_italic = true;
        style.font_style = clever::css::FontStyle::Italic;
        // Inline by default
        if (style.display == clever::css::Display::Block) {
            style.display = clever::css::Display::Inline;
            layout_node->mode = clever::layout::LayoutMode::Inline;
            layout_node->display = clever::layout::DisplayType::Inline;
        }
    }

    // Handle <abbr> / <acronym> — abbreviation with dotted underline and title tooltip
    if (tag_lower == "abbr" || tag_lower == "acronym") {
        layout_node->is_abbr = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        // Store title attribute for tooltip text
        std::string title_attr = get_attr(node, "title");
        if (!title_attr.empty()) {
            layout_node->title_text = title_attr;
        }
        // Ensure dotted underline decoration
        layout_node->text_decoration = 1; // underline
        layout_node->text_decoration_bits |= 1; // underline bit
        layout_node->text_decoration_style = 2; // dotted
    }

    // Handle <mark> — highlighted text (yellow background, black text)
    if (tag_lower == "mark") {
        layout_node->is_mark = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        // Default mark styling: yellow background, black text
        if (layout_node->background_color == 0 || layout_node->background_color == 0x00000000) {
            layout_node->background_color = 0xFFFFFF00;  // yellow
        }
        layout_node->color = 0xFF000000;  // black text
    }

    // Handle <ins> — inserted text (underline decoration)
    if (tag_lower == "ins") {
        layout_node->is_ins = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->text_decoration = 1; // underline
        layout_node->text_decoration_bits |= 1; // underline bit
    }

    // Handle <del>/<s>/<strike> — deleted text (line-through decoration)
    if (tag_lower == "del" || tag_lower == "s" || tag_lower == "strike") {
        layout_node->is_del = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->text_decoration = 2; // line-through
        layout_node->text_decoration_bits |= 4; // line-through bit
    }

    // Handle <cite> — citation (italic by default)
    if (tag_lower == "cite") {
        layout_node->is_cite = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_italic = true;
    }

    // Handle <q> — inline quotation
    if (tag_lower == "q") {
        layout_node->is_q = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
    }

    // Handle <bdi> — bidirectional isolation (inline, no visual change)
    if (tag_lower == "bdi") {
        layout_node->is_bdi = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
    }

    // Handle <bdo> — bidirectional override (inline, forces text direction via dir attribute)
    if (tag_lower == "bdo") {
        layout_node->is_bdo = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        std::string dir_attr = to_lower(get_attr(node, "dir"));
        if (dir_attr == "rtl") {
            layout_node->direction = 1;
        } else {
            layout_node->direction = 0; // default: ltr
        }
    }

    // Handle <time> — date/time element (inline, optional datetime attribute)
    if (tag_lower == "time") {
        layout_node->is_time = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        std::string dt_attr = get_attr(node, "datetime");
        if (!dt_attr.empty()) {
            layout_node->datetime_attr = dt_attr;
        }
    }

    // Handle <dfn> — definition element (italic by default)
    if (tag_lower == "dfn") {
        layout_node->is_dfn = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_italic = true;
    }

    // Handle <data> — machine-readable data (inline, value attribute)
    if (tag_lower == "data") {
        layout_node->is_data = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        std::string val_attr = get_attr(node, "value");
        if (!val_attr.empty()) {
            layout_node->data_value = val_attr;
        }
    }

    // Handle <output> — result of a calculation or user action (inline)
    if (tag_lower == "output") {
        layout_node->is_output = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        std::string for_attr = get_attr(node, "for");
        if (!for_attr.empty()) {
            layout_node->output_for = for_attr;
        }
    }

    // Handle <label> — form label element (inline, optional for attribute)
    if (tag_lower == "label") {
        layout_node->is_label = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        std::string for_attr = get_attr(node, "for");
        if (!for_attr.empty()) {
            layout_node->label_for = for_attr;
        }
    }

    // Handle <sub> — subscript (reduced font size, shifted down)
    if (tag_lower == "sub") {
        layout_node->is_sub = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = layout_node->font_size * 0.83f;
        // Use layout-time sub alignment so line box height accounts for the shift.
        // vertical_align=6 (sub) is handled in position_inline_children.
        // Also keep vertical_offset for paint-time compatibility.
        layout_node->vertical_align = 6; // sub
        layout_node->vertical_offset = layout_node->font_size * 0.3f; // shift down (paint fallback)
    }

    // Handle <sup> — superscript (reduced font size, shifted up)
    if (tag_lower == "sup") {
        layout_node->is_sup = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = layout_node->font_size * 0.83f;
        // Use layout-time super alignment so line box height accounts for the shift.
        // vertical_align=7 (super) is handled in position_inline_children.
        // Also keep vertical_offset for paint-time compatibility.
        layout_node->vertical_align = 7; // super
        layout_node->vertical_offset = -(layout_node->font_size * 0.4f); // shift up (paint fallback)
    }

    // Handle <small> — smaller text (0.83x font size, inline)
    if (tag_lower == "small") {
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = layout_node->font_size * 0.83f;
    }

    // Handle <big> — larger text (1.17x font size, inline)
    if (tag_lower == "big") {
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->font_size = layout_node->font_size * 1.17f;
    }

    // Handle <font> — legacy font styling element
    if (tag_lower == "font") {
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        // Legacy color attribute (hex or named colors)
        std::string fc = get_attr(node, "color");
        if (!fc.empty()) {
            uint32_t fcc = parse_html_color_attr(fc);
            if (fcc != 0) layout_node->color = fcc;
        }
        // Legacy size attribute (1-7, 3=default/16px)
        std::string fs = get_attr(node, "size");
        if (!fs.empty()) {
            try {
                int sz = std::atoi(fs.c_str());
                // HTML font sizes: 1=10px, 2=13px, 3=16px, 4=18px, 5=24px, 6=32px, 7=48px
                static const float size_map[] = {10, 10, 13, 16, 18, 24, 32, 48};
                if (sz >= 1 && sz <= 7) {
                    layout_node->font_size = size_map[sz];
                } else if (sz > 7) {
                    layout_node->font_size = 48;
                }
            } catch (...) {}
        }
        // Legacy face attribute (font-family)
        std::string ff = get_attr(node, "face");
        if (!ff.empty()) {
            layout_node->font_family = ff;
        }
    }

    // Handle <center> — legacy block-level centering element
    if (tag_lower == "center") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->text_align = 1; // center
        // Also update ComputedStyle so children inherit text-align: center
        style.text_align = clever::css::TextAlign::Center;
    }

    // Handle legacy HTML `align` attribute on generic block elements
    // (div, p, h1-h6, blockquote, etc.) — common in legacy markup and Google's homepage
    if (tag_lower == "div" || tag_lower == "p" || tag_lower == "blockquote" ||
        (tag_lower.size() == 2 && tag_lower[0] == 'h' && tag_lower[1] >= '1' && tag_lower[1] <= '6')) {
        std::string align_attr = get_attr(node, "align");
        if (!align_attr.empty()) {
            std::string al = align_attr;
            for (auto& c : al) c = std::tolower(c);
            if (al == "center") { layout_node->text_align = 1; style.text_align = clever::css::TextAlign::Center; }
            else if (al == "right") { layout_node->text_align = 2; style.text_align = clever::css::TextAlign::Right; }
            else if (al == "left") { layout_node->text_align = 0; style.text_align = clever::css::TextAlign::Left; }
            else if (al == "justify") { layout_node->text_align = 3; style.text_align = clever::css::TextAlign::Justify; }
        }
    }

    // Handle <body> — legacy presentational attributes
    if (tag_lower == "body") {
        std::string body_bg = get_attr(node, "bgcolor");
        if (!body_bg.empty()) {
            uint32_t bgc = parse_html_color_attr(body_bg);
            if (bgc != 0) layout_node->background_color = bgc;
        }
        std::string body_text = get_attr(node, "text");
        if (!body_text.empty()) {
            uint32_t btc = parse_html_color_attr(body_text);
            if (btc != 0) layout_node->color = btc;
        }
    }

    // Handle <form> — block-level container with full-width default
    if (tag_lower == "form") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        if (layout_node->specified_width < 0.0f && !layout_node->css_width.has_value()) {
            layout_node->css_width = clever::css::Length::percent(100.0f);
        }
    }

    // Apply color-scheme: dark — only set dark defaults on html/body
    // when the page explicitly opts in via CSS color-scheme property.
    // Do NOT force dark defaults based on OS dark mode — most websites
    // (including Google) expect a white background regardless of OS theme.
    // color_scheme == 2 means the page explicitly declared color-scheme: dark.
    // We only apply dark defaults if the page set color-scheme to dark AND
    // didn't set any explicit background/text colors itself.
    if ((tag_lower == "html" || tag_lower == "body") && layout_node->color_scheme == 2) {
        // Only apply if the element has NO explicit background from CSS/HTML
        // AND no ancestor has set a background. The page must truly want dark.
        bool has_explicit_bg = layout_node->background_color != 0;
        bool has_explicit_color = (layout_node->color != 0xFF000000 && layout_node->color != 0);
        if (!has_explicit_bg && !has_explicit_color) {
            // Page declared dark scheme with no colors — apply dark defaults
            layout_node->background_color = 0xFF1a1a2e;
            layout_node->color = 0xFFE0E0E0;
        }
    }

    // Handle <ruby> — ruby annotation container (East Asian typography)
    if (tag_lower == "ruby") {
        layout_node->is_ruby = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        layout_node->ruby_align = style.ruby_align;
        layout_node->ruby_position = style.ruby_position;
    }

    // Handle <rt> — ruby text (annotation above base text)
    if (tag_lower == "rt") {
        layout_node->is_ruby_text = true;
        layout_node->mode = clever::layout::LayoutMode::Inline;
        layout_node->display = clever::layout::DisplayType::Inline;
        // Ruby text is smaller (roughly 50% of parent font size)
        layout_node->font_size = std::max(8.0f, layout_node->font_size * 0.5f);
    }

    // Handle <rp> — ruby parenthesis (hidden when ruby is supported)
    if (tag_lower == "rp") {
        layout_node->is_ruby_paren = true;
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;
    }

    // Handle <math> — MathML inline-block element (treated like span/inline)
    if (tag_lower == "math") {
        layout_node->mode = clever::layout::LayoutMode::InlineBlock;
        layout_node->display = clever::layout::DisplayType::InlineBlock;
    }

    // Handle <fieldset> — form grouping with border
    if (tag_lower == "fieldset") {
        layout_node->is_fieldset = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        // Default fieldset styling: 2px groove border, padding, margin
        // border: 2px groove ThreeDFace (approximated as groove gray)
        if (layout_node->geometry.border.top == 0 && layout_node->geometry.border.left == 0) {
            layout_node->geometry.border = {2, 2, 2, 2};
            layout_node->border_color = 0xFF808080; // gray (ThreeDFace)
            layout_node->border_color_top = 0xFF808080;
            layout_node->border_color_right = 0xFF808080;
            layout_node->border_color_bottom = 0xFF808080;
            layout_node->border_color_left = 0xFF808080;
            layout_node->border_style = 5; // groove
            layout_node->border_style_top = 5;
            layout_node->border_style_right = 5;
            layout_node->border_style_bottom = 5;
            layout_node->border_style_left = 5;
        }
        // padding: 0.35em 0.75em 0.625em
        if (layout_node->geometry.padding.top == 0) {
            float em = layout_node->font_size;
            layout_node->geometry.padding.top = 0.35f * em;
            layout_node->geometry.padding.right = 0.75f * em;
            layout_node->geometry.padding.bottom = 0.625f * em;
            layout_node->geometry.padding.left = 0.75f * em;
        }
        // margin: 0 2px
        if (layout_node->geometry.margin.left == 0 && layout_node->geometry.margin.right == 0) {
            layout_node->geometry.margin.left = 2;
            layout_node->geometry.margin.right = 2;
        }
    }

    // Handle <legend> — caption for fieldset
    if (tag_lower == "legend") {
        layout_node->is_legend = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        // Legend has default padding and auto width (fit content)
        if (layout_node->geometry.padding.left == 0 && layout_node->geometry.padding.right == 0) {
            layout_node->geometry.padding.left = 4;
            layout_node->geometry.padding.right = 4;
        }
    }

    // Handle <address> — contact information (italic block)
    if (tag_lower == "address") {
        layout_node->is_address = true;
        layout_node->font_italic = true;  // Default italic styling
        // Block element (already default)
    }

    // Handle <figure> — self-contained content (images, diagrams, code listings)
    if (tag_lower == "figure") {
        layout_node->is_figure = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        // Default UA margin: 1em 40px (top/bottom 16px, left/right 40px)
        if (layout_node->geometry.margin.top == 0)
            layout_node->geometry.margin.top = 16;
        if (layout_node->geometry.margin.bottom == 0)
            layout_node->geometry.margin.bottom = 16;
        if (layout_node->geometry.margin.left == 0)
            layout_node->geometry.margin.left = 40;
        if (layout_node->geometry.margin.right == 0)
            layout_node->geometry.margin.right = 40;
    }

    // Handle <figcaption> — caption for a <figure> element
    if (tag_lower == "figcaption") {
        layout_node->is_figcaption = true;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
    }

    // Handle <blockquote> — indented block
    if (tag_lower == "blockquote") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        // Default 40px left margin for indentation
        if (layout_node->geometry.margin.left == 0) {
            layout_node->geometry.margin.left = 40;
        }
        // Add left border for visual indicator
        if (layout_node->geometry.border.left == 0) {
            layout_node->geometry.border.left = 3;
            layout_node->border_color = 0xFFCCCCCC;
        }
        if (layout_node->geometry.padding.left == 0) {
            layout_node->geometry.padding.left = 12;
        }
    }

    // Handle <table> — render as table with border
    if (tag_lower == "table") {
        layout_node->mode = clever::layout::LayoutMode::Table;
        layout_node->display = clever::layout::DisplayType::Table;
        // Parse HTML border attribute: border=0 → no borders; border=N → N-px border
        std::string border_attr = get_attr(node, "border");
        int table_border_val = -1; // -1 = not specified
        if (!border_attr.empty()) {
            try { table_border_val = std::stoi(border_attr); } catch (...) { table_border_val = 0; }
        }
        layout_node->table_border = table_border_val;
        if (table_border_val > 0) {
            float bw = static_cast<float>(table_border_val);
            if (layout_node->geometry.border.top == 0) {
                layout_node->geometry.border = {bw, bw, bw, bw};
                layout_node->border_color = 0xFFCCCCCC;
            }
        } else if (table_border_val == 0) {
            // border=0: explicitly no border
            layout_node->geometry.border = {0, 0, 0, 0};
        }
        // No default border when border attribute is absent (per spec)
        // Legacy HTML bgcolor attribute (hex or named colors)
        std::string bg = get_attr(node, "bgcolor");
        if (!bg.empty()) {
            uint32_t bgc = parse_html_color_attr(bg);
            if (bgc != 0) layout_node->background_color = bgc;
        }
        // Legacy HTML width attribute (supports "100%", "500", etc.)
        std::string tw = get_attr(node, "width");
        if (!tw.empty()) {
            if (!tw.empty() && tw.back() == '%') {
                // Percentage width — use css_width for deferred resolution
                try {
                    float pct = std::stof(tw.substr(0, tw.size() - 1));
                    layout_node->css_width = clever::css::Length::percent(pct);
                } catch (...) {}
            } else {
                try { layout_node->specified_width = std::stof(tw); } catch (...) {}
            }
        }
        // Legacy HTML align attribute (table alignment)
        std::string ta = get_attr(node, "align");
        if (ta == "center") {
            layout_node->geometry.margin.left = MARGIN_AUTO; // auto
            layout_node->geometry.margin.right = MARGIN_AUTO; // auto
        }
        // Legacy HTML cellpadding attribute (applies padding to all td/th cells)
        std::string cp = get_attr(node, "cellpadding");
        if (!cp.empty()) {
            try { layout_node->table_cellpadding = std::stof(cp); } catch (...) {}
        }
        if (tag_lower == "table" && layout_node->table_cellpadding < 0.0f) {
            layout_node->table_cellpadding = 1.0f;
        }
        // Legacy HTML cellspacing attribute (sets border-spacing between cells)
        std::string cs = get_attr(node, "cellspacing");
        if (!cs.empty()) {
            try {
                float csval = std::stof(cs);
                layout_node->table_cellspacing = csval;
                layout_node->border_spacing = csval; // directly sets CSS border-spacing
            } catch (...) {}
        }
        // Legacy HTML frame attribute: controls which outer borders are visible
        // void=none, above=top, below=bottom, hsides=top+bottom,
        // lhs=left, rhs=right, vsides=left+right, box/border=all
        std::string frame_attr = to_lower(get_attr(node, "frame"));
        if (!frame_attr.empty()) {
            auto& b = layout_node->geometry.border;
            if (frame_attr == "void") {
                b = {0, 0, 0, 0};
            } else if (frame_attr == "above") {
                b = {1, 0, 0, 0};
            } else if (frame_attr == "below") {
                b = {0, 0, 1, 0};
            } else if (frame_attr == "hsides") {
                b = {1, 0, 1, 0};
            } else if (frame_attr == "lhs") {
                b = {0, 0, 0, 1};
            } else if (frame_attr == "rhs") {
                b = {0, 1, 0, 0};
            } else if (frame_attr == "vsides") {
                b = {0, 1, 0, 1};
            } else if (frame_attr == "box" || frame_attr == "border") {
                b = {1, 1, 1, 1};
            }
        }
        // Legacy HTML rules attribute: controls which inner cell borders are visible
        // Stored on the table node; used during cell border propagation later
        std::string rules_attr = to_lower(get_attr(node, "rules"));
        if (!rules_attr.empty()) {
            layout_node->table_rules = rules_attr;
        }
    }

    // Handle <thead>/<tbody>/<tfoot> — pass through as block containers
    if (tag_lower == "thead" || tag_lower == "tbody" || tag_lower == "tfoot") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
    }

    // Handle <caption> — block element with centered text
    if (tag_lower == "caption") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        layout_node->text_align = 1; // center
        layout_node->font_weight = 700;
        layout_node->geometry.padding = {4, 8, 4, 8};
    }

    // Handle <colgroup> — invisible column group container
    if (tag_lower == "colgroup") {
        layout_node->is_colgroup = true;
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;
    }

    // Handle <col> — column metadata element (invisible, stores width/span)
    if (tag_lower == "col") {
        layout_node->is_col = true;
        layout_node->mode = clever::layout::LayoutMode::None;
        layout_node->display = clever::layout::DisplayType::None;

        // Parse span attribute (default 1)
        std::string span_attr = get_attr(node, "span");
        if (!span_attr.empty()) {
            try { layout_node->col_span = std::stoi(span_attr); } catch (...) {}
            if (layout_node->col_span < 1) layout_node->col_span = 1;
        }

        // Parse width from attribute or inline style
        std::string width_attr = get_attr(node, "width");
        if (!width_attr.empty()) {
            try { layout_node->specified_width = std::stof(width_attr); } catch (...) {}
        }
        // Parse bgcolor attribute for column background
        std::string col_bg = get_attr(node, "bgcolor");
        if (!col_bg.empty()) {
            uint32_t bgc = parse_html_color_attr(col_bg);
            if (bgc != 0) layout_node->background_color = bgc;
        }
        // CSS background-color from inline style (already parsed in main style block)
    }

    // Handle <tr> — render as flex row
    if (tag_lower == "tr") {
        layout_node->mode = clever::layout::LayoutMode::Flex;
        layout_node->display = clever::layout::DisplayType::Flex;
        layout_node->flex_direction = 0; // row
        // Legacy HTML bgcolor attribute on <tr>
        std::string tr_bg = get_attr(node, "bgcolor");
        if (!tr_bg.empty()) {
            uint32_t trc = parse_html_color_attr(tr_bg);
            if (trc != 0) layout_node->background_color = trc;
        }
        // Legacy HTML align attribute on <tr>
        std::string tr_align = get_attr(node, "align");
        if (tr_align == "center") { layout_node->text_align = 1; style.text_align = clever::css::TextAlign::Center; }
        else if (tr_align == "right") { layout_node->text_align = 2; style.text_align = clever::css::TextAlign::Right; }
        else if (tr_align == "left") { layout_node->text_align = 0; style.text_align = clever::css::TextAlign::Left; }
    }

    // Handle <td>/<th> — render as table cells
    if (tag_lower == "td" || tag_lower == "th") {
        layout_node->flex_grow = 1;
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
        if (layout_node->geometry.padding.top == 0) {
            layout_node->geometry.padding = {1, 1, 1, 1}; // UA default 1px padding
        }
        // No default border on cells — borders come from table border attribute or CSS
        if (tag_lower == "th") {
            layout_node->font_weight = 700;
        }
        // Parse colspan and rowspan attributes
        std::string cs = get_attr(node, "colspan");
        if (!cs.empty()) {
            int val = std::atoi(cs.c_str());
            if (val > 1) layout_node->colspan = val;
        }
        std::string rs = get_attr(node, "rowspan");
        if (!rs.empty()) {
            int val = std::atoi(rs.c_str());
            if (val > 1) layout_node->rowspan = val;
        }
        // Legacy HTML bgcolor attribute on <td>/<th>
        std::string td_bg = get_attr(node, "bgcolor");
        if (!td_bg.empty()) {
            uint32_t tdc = parse_html_color_attr(td_bg);
            if (tdc != 0) layout_node->background_color = tdc;
        }
        // Legacy HTML align attribute on <td>/<th>
        std::string td_align = get_attr(node, "align");
        if (td_align == "center") { layout_node->text_align = 1; style.text_align = clever::css::TextAlign::Center; }
        else if (td_align == "right") { layout_node->text_align = 2; style.text_align = clever::css::TextAlign::Right; }
        else if (td_align == "left") { layout_node->text_align = 0; style.text_align = clever::css::TextAlign::Left; }
        // Legacy HTML valign attribute on <td>/<th>
        std::string td_valign = get_attr(node, "valign");
        if (td_valign == "middle") layout_node->vertical_align = 2; // middle
        else if (td_valign == "bottom") layout_node->vertical_align = 3; // bottom
        else if (td_valign == "top") layout_node->vertical_align = 1; // top
        // Legacy HTML nowrap attribute on <td>/<th>
        std::string td_nowrap = get_attr(node, "nowrap");
        if (!td_nowrap.empty()) {
            style.white_space = clever::css::WhiteSpace::NoWrap;
            layout_node->white_space = 1;
        }
        // Legacy HTML width attribute on <td>/<th>
        std::string td_w = get_attr(node, "width");
        if (!td_w.empty()) {
            if (td_w.back() == '%') {
                try {
                    float pct = std::stof(td_w.substr(0, td_w.size() - 1));
                    layout_node->css_width = clever::css::Length::percent(pct);
                } catch (...) {}
            } else {
                try { layout_node->specified_width = std::stof(td_w); } catch (...) {}
            }
        }
        // Legacy HTML height attribute on <td>/<th>
        std::string td_h = get_attr(node, "height");
        if (!td_h.empty()) {
            try { layout_node->specified_height = std::stof(td_h); } catch (...) {}
        }
    }

    // Handle <li> — prepend bullet/number marker
    if (tag_lower == "li") {
        // Determine list-style-type from parent or style
        std::string list_style = "disc"; // default for <ul>
        bool is_ordered = false;
        if (node.parent && to_lower(node.parent->tag_name) == "ol") {
            list_style = "decimal";
            is_ordered = true;
        }

        auto list_style_from_computed = [](clever::css::ListStyleType t) -> std::string {
            switch (t) {
                case clever::css::ListStyleType::Disc: return "disc";
                case clever::css::ListStyleType::Circle: return "circle";
                case clever::css::ListStyleType::Square: return "square";
                case clever::css::ListStyleType::Decimal: return "decimal";
                case clever::css::ListStyleType::DecimalLeadingZero: return "decimal-leading-zero";
                case clever::css::ListStyleType::LowerRoman: return "lower-roman";
                case clever::css::ListStyleType::UpperRoman: return "upper-roman";
                case clever::css::ListStyleType::LowerAlpha: return "lower-alpha";
                case clever::css::ListStyleType::UpperAlpha: return "upper-alpha";
                case clever::css::ListStyleType::None: return "none";
                case clever::css::ListStyleType::LowerGreek: return "lower-greek";
                case clever::css::ListStyleType::LowerLatin: return "lower-latin";
                case clever::css::ListStyleType::UpperLatin: return "upper-latin";
                default: return "";
            }
        };

        // Respect cascaded parent list-style-type (stylesheet rules), but preserve
        // legacy ordered-list fallback when it resolves to disc.
        {
            std::string cascaded = list_style_from_computed(parent_style.list_style_type);
            if (!cascaded.empty() && !(is_ordered && cascaded == "disc")) {
                list_style = cascaded;
            }
        }

        // Check for list-style-type in inline style of parent
        if (node.parent) {
            std::string parent_style_attr = get_attr(*node.parent, "style");
            if (!parent_style_attr.empty()) {
                auto parent_decls = parse_inline_style(parent_style_attr);
                for (auto& pd : parent_decls) {
                    if (pd.property == "list-style-type" || pd.property == "list-style") {
                        list_style = to_lower(pd.value);
                    }
                }
            }
        }

        // Set list item properties on the layout node
        layout_node->is_list_item = true;
        if (list_style == "none") {
            layout_node->list_style_type = 9; // none
        } else if (list_style == "disc") {
            layout_node->list_style_type = 0; // disc
        } else if (list_style == "circle") {
            layout_node->list_style_type = 1; // circle
        } else if (list_style == "square") {
            layout_node->list_style_type = 2; // square
        } else if (list_style == "decimal") {
            layout_node->list_style_type = 3; // decimal
        } else if (list_style == "decimal-leading-zero") {
            layout_node->list_style_type = 4; // decimal-leading-zero
        } else if (list_style == "lower-roman") {
            layout_node->list_style_type = 5; // lower-roman
        } else if (list_style == "upper-roman") {
            layout_node->list_style_type = 6; // upper-roman
        } else if (list_style == "lower-alpha") {
            layout_node->list_style_type = 7; // lower-alpha
        } else if (list_style == "upper-alpha") {
            layout_node->list_style_type = 8; // upper-alpha
        } else if (list_style == "lower-greek") {
            layout_node->list_style_type = 10; // lower-greek
        } else if (list_style == "lower-latin") {
            layout_node->list_style_type = 11; // lower-latin
        } else if (list_style == "upper-latin") {
            layout_node->list_style_type = 12; // upper-latin
        } else if (is_ordered) {
            layout_node->list_style_type = 3; // default to decimal for <ol>
        } else {
            layout_node->list_style_type = 0; // default to disc
        }

        // Compute list item index by counting preceding <li> siblings
        {
            int index = 1;
            if (node.parent) {
                for (auto& sibling : node.parent->children) {
                    if (sibling.get() == &node) break;
                    if (sibling->type == clever::html::SimpleNode::Element &&
                        to_lower(sibling->tag_name) == "li") {
                        index++;
                    }
                }
            }
            layout_node->list_item_index = index;
        }

        std::string marker;
        if (list_style == "none") {
            // No marker
        } else if (list_style == "disc") {
            marker = "\xE2\x80\xA2 "; // • U+2022 BULLET
        } else if (list_style == "circle") {
            marker = "\xE2\x97\x8B "; // ○ U+25CB WHITE CIRCLE
        } else if (list_style == "square") {
            marker = "\xE2\x96\xAA "; // ▪ U+25AA BLACK SMALL SQUARE
        } else {
            // Ordered (and named) list types — use already-computed index
            int index = layout_node->list_item_index;
            if (list_style == "decimal") {
                marker = std::to_string(index) + ". ";
            } else if (list_style == "decimal-leading-zero") {
                if (index < 10) marker = "0" + std::to_string(index) + ". ";
                else marker = std::to_string(index) + ". ";
            } else if (list_style == "lower-roman") {
                marker = int_to_roman(index, false) + ". ";
            } else if (list_style == "upper-roman") {
                marker = int_to_roman(index, true) + ". ";
            } else if (list_style == "lower-alpha" || list_style == "lower-latin") {
                marker = int_to_alpha(index, false) + ". ";
            } else if (list_style == "upper-alpha" || list_style == "upper-latin") {
                marker = int_to_alpha(index, true) + ". ";
            } else if (list_style == "lower-greek") {
                // Greek lowercase letters: alpha=1, beta=2, ...
                static const char* greek[] = {
                    "\xCE\xB1","\xCE\xB2","\xCE\xB3","\xCE\xB4","\xCE\xB5",
                    "\xCE\xB6","\xCE\xB7","\xCE\xB8","\xCE\xB9","\xCE\xBA",
                    "\xCE\xBB","\xCE\xBC","\xCE\xBD","\xCE\xBE","\xCE\xBF",
                    "\xCF\x80","\xCF\x81","\xCF\x83","\xCF\x84","\xCF\x85",
                    "\xCF\x86","\xCF\x87","\xCF\x88","\xCF\x89"
                };
                int gi = (index - 1) % 24;
                marker = std::string(greek[gi]) + ". ";
            } else if (is_ordered) {
                marker = std::to_string(index) + ". "; // fallback decimal for <ol>
            } else {
                marker = "\xE2\x80\xA2 "; // Default bullet
            }
        }
        if (!marker.empty()) {
            if (layout_node->list_style_position == 1) {
                // Inside: create inline marker text node that flows with content
                auto marker_node = std::make_unique<clever::layout::LayoutNode>();
                marker_node->is_text = true;
                marker_node->text_content = marker;
                marker_node->mode = clever::layout::LayoutMode::Inline;
                marker_node->display = clever::layout::DisplayType::Inline;
                marker_node->font_size = (layout_node->marker_font_size > 0)
                    ? layout_node->marker_font_size
                    : style.font_size.to_px(parent_font_size);
                marker_node->color = (layout_node->marker_color != 0)
                    ? layout_node->marker_color
                    : color_to_argb(style.color);
                layout_node->children.insert(layout_node->children.begin(), std::move(marker_node));
            }
            // Outside (default): no inline marker — paint_list_marker() handles it
        }
    }

    // Margin (auto margins use -1e30 as sentinel for centering in layout engine)
    // Percentage margins are deferred to layout time (resolve against containing block width)
    {
        auto resolve_margin = [&](const clever::css::Length& m, float& geom_val,
                                  std::optional<clever::css::Length>& css_val, float auto_val) {
            if (m.is_auto()) {
                geom_val = auto_val;
            } else if (m.unit == clever::css::Length::Unit::Percent ||
                       m.unit == clever::css::Length::Unit::Calc) {
                css_val = m;
                geom_val = 0; // placeholder — resolved at layout time
            } else {
                geom_val = m.to_px(fs, 16.0f, lh_px);
            }
        };
        resolve_margin(style.margin.top, layout_node->geometry.margin.top,
                       layout_node->css_margin_top, 0);
        resolve_margin(style.margin.right, layout_node->geometry.margin.right,
                       layout_node->css_margin_right, MARGIN_AUTO);
        resolve_margin(style.margin.bottom, layout_node->geometry.margin.bottom,
                       layout_node->css_margin_bottom, 0);
        resolve_margin(style.margin.left, layout_node->geometry.margin.left,
                       layout_node->css_margin_left, MARGIN_AUTO);
    }

    // Padding (percentage padding resolves against containing block width)
    {
        auto resolve_padding = [&](const clever::css::Length& p, float& geom_val,
                                   std::optional<clever::css::Length>& css_val) {
            if (p.unit == clever::css::Length::Unit::Percent ||
                p.unit == clever::css::Length::Unit::Calc) {
                css_val = p;
                geom_val = 0; // placeholder — resolved at layout time
            } else {
                geom_val = p.to_px(fs, 16.0f, lh_px);
            }
        };
        resolve_padding(style.padding.top, layout_node->geometry.padding.top,
                        layout_node->css_padding_top);
        resolve_padding(style.padding.right, layout_node->geometry.padding.right,
                        layout_node->css_padding_right);
        resolve_padding(style.padding.bottom, layout_node->geometry.padding.bottom,
                        layout_node->css_padding_bottom);
        resolve_padding(style.padding.left, layout_node->geometry.padding.left,
                        layout_node->css_padding_left);
    }

    // Border
    if (style.border_top.style != clever::css::BorderStyle::None) {
        layout_node->geometry.border.top = style.border_top.width.to_px(0);
        layout_node->border_color = color_to_argb(style.border_top.color);
    }
    if (style.border_right.style != clever::css::BorderStyle::None) {
        layout_node->geometry.border.right = style.border_right.width.to_px(0);
        layout_node->border_color = color_to_argb(style.border_right.color);
    }
    if (style.border_bottom.style != clever::css::BorderStyle::None) {
        layout_node->geometry.border.bottom = style.border_bottom.width.to_px(0);
        layout_node->border_color = color_to_argb(style.border_bottom.color);
    }
    if (style.border_left.style != clever::css::BorderStyle::None) {
        layout_node->geometry.border.left = style.border_left.width.to_px(0);
        layout_node->border_color = color_to_argb(style.border_left.color);
    }

    // Per-side border colors
    layout_node->border_color_top = color_to_argb(style.border_top.color);
    layout_node->border_color_right = color_to_argb(style.border_right.color);
    layout_node->border_color_bottom = color_to_argb(style.border_bottom.color);
    layout_node->border_color_left = color_to_argb(style.border_left.color);

    // Border style: convert from CSS enum to int (use top border as representative)
    // 0=none, 1=solid, 2=dashed, 3=dotted
    {
        auto bs = style.border_top.style;
        if (bs == clever::css::BorderStyle::None) layout_node->border_style = 0;
        else if (bs == clever::css::BorderStyle::Solid) layout_node->border_style = 1;
        else if (bs == clever::css::BorderStyle::Dashed) layout_node->border_style = 2;
        else if (bs == clever::css::BorderStyle::Dotted) layout_node->border_style = 3;
        else layout_node->border_style = 1; // default to solid for unsupported styles
    }

    // Per-side border styles
    {
        auto convert_bs = [](clever::css::BorderStyle bs) -> int {
            if (bs == clever::css::BorderStyle::None) return 0;
            if (bs == clever::css::BorderStyle::Solid) return 1;
            if (bs == clever::css::BorderStyle::Dashed) return 2;
            if (bs == clever::css::BorderStyle::Dotted) return 3;
            if (bs == clever::css::BorderStyle::Double) return 4;
            if (bs == clever::css::BorderStyle::Groove) return 5;
            if (bs == clever::css::BorderStyle::Ridge) return 6;
            if (bs == clever::css::BorderStyle::Inset) return 7;
            if (bs == clever::css::BorderStyle::Outset) return 8;
            return 1;
        };
        layout_node->border_style_top = convert_bs(style.border_top.style);
        layout_node->border_style_right = convert_bs(style.border_right.style);
        layout_node->border_style_bottom = convert_bs(style.border_bottom.style);
        layout_node->border_style_left = convert_bs(style.border_left.style);
    }

    // Border radius
    layout_node->border_radius = style.border_radius;
    layout_node->border_radius_tl = style.border_radius_tl;
    layout_node->border_radius_tr = style.border_radius_tr;
    layout_node->border_radius_bl = style.border_radius_bl;
    layout_node->border_radius_br = style.border_radius_br;

    // Logical border radius → map to physical corners based on direction
    layout_node->border_start_start_radius = style.border_start_start_radius;
    layout_node->border_start_end_radius = style.border_start_end_radius;
    layout_node->border_end_start_radius = style.border_end_start_radius;
    layout_node->border_end_end_radius = style.border_end_end_radius;
    // Apply logical radii to physical corners (overrides physical if set)
    if (style.border_start_start_radius > 0 || style.border_start_end_radius > 0 ||
        style.border_end_start_radius > 0 || style.border_end_end_radius > 0) {
        bool rtl = (layout_node->direction == 1);
        if (!rtl) {
            // LTR: start-start=TL, start-end=TR, end-start=BL, end-end=BR
            if (style.border_start_start_radius > 0)
                layout_node->border_radius_tl = style.border_start_start_radius;
            if (style.border_start_end_radius > 0)
                layout_node->border_radius_tr = style.border_start_end_radius;
            if (style.border_end_start_radius > 0)
                layout_node->border_radius_bl = style.border_end_start_radius;
            if (style.border_end_end_radius > 0)
                layout_node->border_radius_br = style.border_end_end_radius;
        } else {
            // RTL: start-start=TR, start-end=TL, end-start=BR, end-end=BL
            if (style.border_start_start_radius > 0)
                layout_node->border_radius_tr = style.border_start_start_radius;
            if (style.border_start_end_radius > 0)
                layout_node->border_radius_tl = style.border_start_end_radius;
            if (style.border_end_start_radius > 0)
                layout_node->border_radius_br = style.border_end_start_radius;
            if (style.border_end_end_radius > 0)
                layout_node->border_radius_bl = style.border_end_end_radius;
        }
    }

    // Outline (does not affect layout)
    layout_node->outline_width = 0;
    layout_node->outline_style = 0;
    layout_node->outline_offset = 0;
    if (style.outline_style != clever::css::BorderStyle::None) {
        float outline_width = style.outline_width.to_px(0);
        if (outline_width > 0) {
            layout_node->outline_width = outline_width;
            // Resolve currentColor sentinel (alpha=0 transparent) to the element's text color
            clever::css::Color oc = style.outline_color;
            if (oc.a == 0 && oc.r == 0 && oc.g == 0 && oc.b == 0) {
                oc = style.color; // currentColor — use element's text color
            }
            layout_node->outline_color = color_to_argb(oc);
            // Map BorderStyle enum to int: 0=none, 1=solid, 2=dashed, 3=dotted, 4=double, 5=groove, 6=ridge, 7=inset, 8=outset
            auto os = style.outline_style;
            if (os == clever::css::BorderStyle::Solid) layout_node->outline_style = 1;
            else if (os == clever::css::BorderStyle::Dashed) layout_node->outline_style = 2;
            else if (os == clever::css::BorderStyle::Dotted) layout_node->outline_style = 3;
            else if (os == clever::css::BorderStyle::Double) layout_node->outline_style = 4;
            else if (os == clever::css::BorderStyle::Groove) layout_node->outline_style = 5;
            else if (os == clever::css::BorderStyle::Ridge) layout_node->outline_style = 6;
            else if (os == clever::css::BorderStyle::Inset) layout_node->outline_style = 7;
            else if (os == clever::css::BorderStyle::Outset) layout_node->outline_style = 8;
            layout_node->outline_offset = style.outline_offset.to_px(0);
        }
    }

    // Border image
    layout_node->border_image_source = style.border_image_source;
    layout_node->border_image_slice = style.border_image_slice;
    layout_node->border_image_slice_fill = style.border_image_slice_fill;
    layout_node->border_image_width_val = style.border_image_width_val;
    layout_node->border_image_outset = style.border_image_outset;
    layout_node->border_image_repeat = style.border_image_repeat;
    // Pre-parse border-image gradient source for painter, or fetch URL image
    if (!style.border_image_source.empty()) {
        if (style.border_image_source.find("linear-gradient") != std::string::npos) {
            float angle;
            std::vector<std::pair<uint32_t, float>> stops;
            if (parse_linear_gradient(style.border_image_source, angle, stops)) {
                layout_node->border_image_gradient_type = 1;
                layout_node->border_image_gradient_angle = angle;
                layout_node->border_image_gradient_stops = std::move(stops);
            }
        } else if (style.border_image_source.find("radial-gradient") != std::string::npos) {
            int shape;
            std::vector<std::pair<uint32_t, float>> stops;
            if (parse_radial_gradient(style.border_image_source, shape, stops)) {
                layout_node->border_image_gradient_type = 2;
                layout_node->border_image_radial_shape = shape;
                layout_node->border_image_gradient_stops = std::move(stops);
            }
        } else {
            // URL-based border image: extract URL and decode pixels
            std::string bi_url;
            const std::string& bi_src = style.border_image_source;
            auto url_pos = bi_src.find("url(");
            if (url_pos != std::string::npos) {
                auto inner_start = url_pos + 4;
                auto inner_end = bi_src.find(')', inner_start);
                if (inner_end != std::string::npos) {
                    bi_url = bi_src.substr(inner_start, inner_end - inner_start);
                    if (bi_url.size() >= 2 &&
                        ((bi_url.front() == '"' && bi_url.back() == '"') ||
                         (bi_url.front() == '\'' && bi_url.back() == '\''))) {
                        bi_url = bi_url.substr(1, bi_url.size() - 2);
                    }
                }
            } else if (bi_src.find("://") != std::string::npos ||
                       (!bi_src.empty() && bi_src.front() == '/') ||
                       bi_src.find('.') != std::string::npos) {
                bi_url = bi_src;
            }
            if (!bi_url.empty()) {
                std::string resolved_bi_url = resolve_url(bi_url, base_url);
                auto decoded = fetch_and_decode_image(resolved_bi_url);
                if (decoded.pixels && !decoded.pixels->empty()) {
                    layout_node->border_image_pixels = decoded.pixels;
                    layout_node->border_image_img_width = decoded.width;
                    layout_node->border_image_img_height = decoded.height;
                }
            }
        }
    }

    // CSS Mask properties
    layout_node->mask_image = style.mask_image;
    layout_node->mask_size = style.mask_size;
    layout_node->mask_size_width = style.mask_size_width;
    layout_node->mask_size_height = style.mask_size_height;
    layout_node->mask_repeat = style.mask_repeat;

    // Shape outside (string form), shape-margin, shape-image-threshold
    layout_node->shape_outside_str = style.shape_outside_str;
    layout_node->shape_margin = style.shape_margin;
    layout_node->shape_image_threshold = style.shape_image_threshold;

    // Box shadow
    layout_node->shadow_offset_x = style.shadow_offset_x;
    layout_node->shadow_offset_y = style.shadow_offset_y;
    layout_node->shadow_blur = style.shadow_blur;
    layout_node->shadow_spread = style.shadow_spread;
    layout_node->shadow_color = color_to_argb(style.shadow_color);
    layout_node->shadow_inset = style.shadow_inset;
    // Multiple box shadows
    layout_node->box_shadows.clear();
    for (auto& bs : style.box_shadows) {
        clever::layout::LayoutNode::BoxShadowEntry e;
        e.offset_x = bs.offset_x;
        e.offset_y = bs.offset_y;
        e.blur = bs.blur;
        e.spread = bs.spread;
        e.color = color_to_argb(bs.color);
        e.inset = bs.inset;
        layout_node->box_shadows.push_back(e);
    }

    // Text shadow
    layout_node->text_shadow_offset_x = style.text_shadow_offset_x;
    layout_node->text_shadow_offset_y = style.text_shadow_offset_y;
    layout_node->text_shadow_blur = style.text_shadow_blur;
    layout_node->text_shadow_color = color_to_argb(style.text_shadow_color);
    // Multiple text shadows
    layout_node->text_shadows.clear();
    for (auto& ts : style.text_shadows) {
        clever::layout::LayoutNode::TextShadowEntry e;
        e.offset_x = ts.offset_x;
        e.offset_y = ts.offset_y;
        e.blur = ts.blur;
        e.color = color_to_argb(ts.color);
        layout_node->text_shadows.push_back(e);
    }

    // Overflow
    if (style.overflow_x == clever::css::Overflow::Hidden ||
        style.overflow_y == clever::css::Overflow::Hidden) {
        layout_node->overflow = 1;
    } else if (style.overflow_x == clever::css::Overflow::Scroll ||
               style.overflow_y == clever::css::Overflow::Scroll) {
        layout_node->overflow = 2;
    } else if (style.overflow_x == clever::css::Overflow::Auto ||
               style.overflow_y == clever::css::Overflow::Auto) {
        layout_node->overflow = 3;
    }
    // Mark as scroll container when overflow is not visible
    layout_node->is_scroll_container = (layout_node->overflow >= 1);

    // display: flow-root — always establishes a BFC
    layout_node->is_flow_root = style.is_flow_root;

    // White-space: map enum to integer + legacy booleans
    switch (style.white_space) {
        case clever::css::WhiteSpace::Normal:
            layout_node->white_space = 0;
            break;
        case clever::css::WhiteSpace::NoWrap:
            layout_node->white_space = 1;
            layout_node->white_space_nowrap = true;
            break;
        case clever::css::WhiteSpace::Pre:
            layout_node->white_space = 2;
            layout_node->white_space_pre = true;
            layout_node->white_space_nowrap = true; // pre does not wrap
            break;
        case clever::css::WhiteSpace::PreWrap:
            layout_node->white_space = 3;
            layout_node->white_space_pre = true;
            break;
        case clever::css::WhiteSpace::PreLine:
            layout_node->white_space = 4;
            break;
        case clever::css::WhiteSpace::BreakSpaces:
            layout_node->white_space = 5;
            layout_node->white_space_pre = true; // preserve whitespace
            break;
    }

    // Text overflow
    if (style.text_overflow == clever::css::TextOverflow::Ellipsis) {
        layout_node->text_overflow = 1;
    } else if (style.text_overflow == clever::css::TextOverflow::Fade) {
        layout_node->text_overflow = 2;
    }

    // Word break and overflow wrap
    layout_node->word_break = style.word_break;
    layout_node->overflow_wrap = style.overflow_wrap;
    layout_node->text_wrap = style.text_wrap;
    layout_node->white_space_collapse = style.white_space_collapse;
    layout_node->line_break = style.line_break;
    layout_node->orphans = style.orphans;
    layout_node->widows = style.widows;
    layout_node->column_span = style.column_span;
    layout_node->break_before = style.break_before;
    layout_node->break_after = style.break_after;
    layout_node->break_inside = style.break_inside;
    layout_node->page_break_before = style.page_break_before;
    layout_node->page_break_after = style.page_break_after;
    layout_node->page_break_inside = style.page_break_inside;
    layout_node->page = style.page;
    layout_node->background_clip = style.background_clip;
    layout_node->background_origin = style.background_origin;
    layout_node->background_blend_mode = style.background_blend_mode;
    layout_node->bg_attachment = style.background_attachment;
    layout_node->unicode_bidi = style.unicode_bidi;
    layout_node->font_smooth = style.font_smooth;
    layout_node->text_size_adjust = style.text_size_adjust;
    layout_node->offset_path = style.offset_path;
    layout_node->offset_distance = style.offset_distance;
    layout_node->offset_rotate = style.offset_rotate;
    layout_node->offset = style.offset;
    layout_node->offset_anchor = style.offset_anchor;
    layout_node->offset_position = style.offset_position;
    layout_node->transition_behavior = style.transition_behavior;
    layout_node->animation_range = style.animation_range;
    layout_node->css_rotate = style.css_rotate;
    layout_node->css_scale = style.css_scale;
    layout_node->css_translate = style.css_translate;

    // Flex properties
    layout_node->flex_grow = style.flex_grow;
    layout_node->flex_shrink = style.flex_shrink;
    if (style.flex_basis.is_auto()) {
        layout_node->flex_basis = -1;  // auto
    } else {
        layout_node->flex_basis = style.flex_basis.to_px(0);
    }
    if (!style.gap.is_zero()) {
        layout_node->gap = style.gap.to_px(0);
    }
    layout_node->row_gap = style.gap.to_px(0);
    layout_node->column_gap = style.column_gap_val.to_px(0);

    // CSS Grid layout
    layout_node->grid_template_columns = style.grid_template_columns;
    layout_node->grid_template_rows = style.grid_template_rows;
    layout_node->grid_template_columns_is_subgrid = style.grid_template_columns_is_subgrid;
    layout_node->grid_template_rows_is_subgrid = style.grid_template_rows_is_subgrid;
    layout_node->grid_column = style.grid_column;
    layout_node->grid_row = style.grid_row;
    layout_node->grid_column_start = style.grid_column_start;
    layout_node->grid_column_end = style.grid_column_end;
    layout_node->grid_row_start = style.grid_row_start;
    layout_node->grid_row_end = style.grid_row_end;
    layout_node->grid_auto_rows = style.grid_auto_rows;
    layout_node->grid_auto_columns = style.grid_auto_columns;
    layout_node->grid_auto_flow = style.grid_auto_flow;
    layout_node->grid_template_areas = style.grid_template_areas;
    layout_node->grid_area = style.grid_area;
    layout_node->justify_items = style.justify_items;
    layout_node->align_content = style.align_content;

    // Multi-column layout
    layout_node->column_count = style.column_count;
    layout_node->column_fill = style.column_fill;
    layout_node->column_width = style.column_width.is_auto() ? -1.0f : style.column_width.to_px(0);
    layout_node->column_gap_val = style.column_gap_val.to_px(0);
    layout_node->column_rule_width = style.column_rule_width;
    layout_node->column_rule_color = color_to_argb(style.column_rule_color);
    layout_node->column_rule_style = style.column_rule_style;
    layout_node->caret_color = color_to_argb(style.caret_color);
    layout_node->accent_color = style.accent_color;
    layout_node->color_interpolation = style.color_interpolation;
    layout_node->mask_composite = style.mask_composite;
    layout_node->mask_mode = style.mask_mode;
    layout_node->mask_shorthand = style.mask_shorthand;
    layout_node->mask_origin = style.mask_origin;
    layout_node->mask_position = style.mask_position;
    layout_node->mask_clip = style.mask_clip;
    layout_node->mask_border = style.mask_border;
    layout_node->marker_shorthand = style.marker_shorthand;
    layout_node->marker_start = style.marker_start;
    layout_node->marker_mid = style.marker_mid;
    layout_node->marker_end = style.marker_end;
    layout_node->overflow_block = style.overflow_block;
    layout_node->overflow_inline = style.overflow_inline;
    layout_node->box_decoration_break = style.box_decoration_break;
    layout_node->margin_trim = style.margin_trim;
    layout_node->css_all = style.css_all;
    layout_node->scroll_snap_stop = style.scroll_snap_stop;
    layout_node->animation_composition = style.animation_composition;
    layout_node->animation_timeline = style.animation_timeline;
    layout_node->transform_box = style.transform_box;
    // CSS Custom Properties (CSS Variables)
    layout_node->custom_properties.insert(style.custom_properties.begin(), style.custom_properties.end());

    // Flex direction/alignment -> map to layout node
    switch (style.flex_direction) {
        case clever::css::FlexDirection::Row:
            layout_node->flex_direction = 0; break;
        case clever::css::FlexDirection::RowReverse:
            layout_node->flex_direction = 1; break;
        case clever::css::FlexDirection::Column:
            layout_node->flex_direction = 2; break;
        case clever::css::FlexDirection::ColumnReverse:
            layout_node->flex_direction = 3; break;
    }

    switch (style.flex_wrap) {
        case clever::css::FlexWrap::NoWrap: layout_node->flex_wrap = 0; break;
        case clever::css::FlexWrap::Wrap: layout_node->flex_wrap = 1; break;
        case clever::css::FlexWrap::WrapReverse: layout_node->flex_wrap = 2; break;
    }

    switch (style.justify_content) {
        case clever::css::JustifyContent::FlexStart:
            layout_node->justify_content = 0; break;
        case clever::css::JustifyContent::FlexEnd:
            layout_node->justify_content = 1; break;
        case clever::css::JustifyContent::Center:
            layout_node->justify_content = 2; break;
        case clever::css::JustifyContent::SpaceBetween:
            layout_node->justify_content = 3; break;
        case clever::css::JustifyContent::SpaceAround:
            layout_node->justify_content = 4; break;
        case clever::css::JustifyContent::SpaceEvenly:
            layout_node->justify_content = 5; break;
    }

    switch (style.align_items) {
        case clever::css::AlignItems::FlexStart:
            layout_node->align_items = 0; break;
        case clever::css::AlignItems::FlexEnd:
            layout_node->align_items = 1; break;
        case clever::css::AlignItems::Center:
            layout_node->align_items = 2; break;
        case clever::css::AlignItems::Baseline:
            layout_node->align_items = 3; break;
        case clever::css::AlignItems::Stretch:
            layout_node->align_items = 4; break;
    }
    layout_node->align_self = style.align_self;
    layout_node->justify_self = style.justify_self;
    layout_node->contain = style.contain;
    layout_node->contain_intrinsic_width = style.contain_intrinsic_width;
    layout_node->contain_intrinsic_height = style.contain_intrinsic_height;
    layout_node->object_fit = style.object_fit;
    layout_node->object_position_x = style.object_position_x;
    layout_node->object_position_y = style.object_position_y;
    layout_node->image_rendering = style.image_rendering;
    layout_node->hanging_punctuation = style.hanging_punctuation;
    layout_node->initial_letter = style.initial_letter;
    layout_node->initial_letter_align = style.initial_letter_align;
    layout_node->color_scheme = style.color_scheme;
    layout_node->forced_color_adjust = style.forced_color_adjust;
    layout_node->content_visibility = style.content_visibility;
    layout_node->container_type = style.container_type;
    layout_node->container_name = style.container_name;
    layout_node->touch_action = style.touch_action;
    layout_node->will_change = style.will_change;
    layout_node->paint_order = style.paint_order;
    layout_node->dominant_baseline = style.dominant_baseline;
    layout_node->order = style.order;
    layout_node->aspect_ratio = style.aspect_ratio;
    layout_node->aspect_ratio_is_auto = style.aspect_ratio_is_auto;

    // Position
    switch (style.position) {
        case clever::css::Position::Static: layout_node->position_type = 0; break;
        case clever::css::Position::Relative: layout_node->position_type = 1; break;
        case clever::css::Position::Absolute: layout_node->position_type = 2; break;
        case clever::css::Position::Fixed: layout_node->position_type = 3; break;
        case clever::css::Position::Sticky: layout_node->position_type = 4; break;
    }
    // Float
    switch (style.float_val) {
        case clever::css::Float::Left: layout_node->float_type = 1; break;
        case clever::css::Float::Right: layout_node->float_type = 2; break;
        default: layout_node->float_type = 0; break;
    }
    // Clear
    switch (style.clear) {
        case clever::css::Clear::Left: layout_node->clear_type = 1; break;
        case clever::css::Clear::Right: layout_node->clear_type = 2; break;
        case clever::css::Clear::Both: layout_node->clear_type = 3; break;
        default: layout_node->clear_type = 0; break;
    }
    // Position offsets
    if (!style.top.is_auto()) {
        layout_node->pos_top = style.top.to_px(parent_font_size);
        layout_node->pos_top_set = true;
    }
    if (!style.right_pos.is_auto()) {
        layout_node->pos_right = style.right_pos.to_px(parent_font_size);
        layout_node->pos_right_set = true;
    }
    if (!style.bottom.is_auto()) {
        layout_node->pos_bottom = style.bottom.to_px(parent_font_size);
        layout_node->pos_bottom_set = true;
    }
    if (!style.left_pos.is_auto()) {
        layout_node->pos_left = style.left_pos.to_px(parent_font_size);
        layout_node->pos_left_set = true;
    }
    layout_node->z_index = style.z_index;
    layout_node->border_box = (style.box_sizing == clever::css::BoxSizing::BorderBox);

    // CSS Transforms
    layout_node->transforms = style.transforms;

    auto convert_pseudo_border_style = [](clever::css::BorderStyle bs) -> int {
        if (bs == clever::css::BorderStyle::None) return 0;
        if (bs == clever::css::BorderStyle::Solid) return 1;
        if (bs == clever::css::BorderStyle::Dashed) return 2;
        if (bs == clever::css::BorderStyle::Dotted) return 3;
        if (bs == clever::css::BorderStyle::Double) return 4;
        if (bs == clever::css::BorderStyle::Groove) return 5;
        if (bs == clever::css::BorderStyle::Ridge) return 6;
        if (bs == clever::css::BorderStyle::Inset) return 7;
        if (bs == clever::css::BorderStyle::Outset) return 8;
        return 0;
    };

    auto apply_generated_text_style =
        [&](const clever::css::ComputedStyle& text_style,
            clever::layout::LayoutNode& target,
            float inherited_font_size_px) {
        const float text_fs = text_style.font_size.to_px(inherited_font_size_px);
        target.font_size = text_fs;
        target.font_family = text_style.font_family;
        target.is_monospace = (text_style.font_family == "monospace");
        target.color = color_to_argb(text_style.color);
        target.font_weight = text_style.font_weight;
        target.font_italic = (text_style.font_style != clever::css::FontStyle::Normal);
        target.line_height = (text_fs > 0.0f)
            ? text_style.line_height.to_px(text_fs) / text_fs
            : 1.2f;
        target.letter_spacing = text_style.letter_spacing.to_px(text_fs);
        target.word_spacing = text_style.word_spacing.to_px(text_fs);
        target.text_transform = static_cast<int>(text_style.text_transform);
        switch (text_style.text_decoration) {
            case clever::css::TextDecoration::Underline: target.text_decoration = 1; break;
            case clever::css::TextDecoration::LineThrough: target.text_decoration = 2; break;
            case clever::css::TextDecoration::Overline: target.text_decoration = 3; break;
            default: target.text_decoration = 0; break;
        }
        target.text_decoration_bits = text_style.text_decoration_bits;
        target.text_decoration_color = color_to_argb(text_style.text_decoration_color);
        target.text_decoration_style = static_cast<int>(text_style.text_decoration_style);
        target.text_decoration_thickness = text_style.text_decoration_thickness;
        target.pointer_events = static_cast<int>(text_style.pointer_events);
        target.user_select = static_cast<int>(text_style.user_select);
        target.tab_size = text_style.tab_size;
        target.line_clamp = text_style.line_clamp;
        target.word_break = text_style.word_break;
        target.overflow_wrap = text_style.overflow_wrap;
        target.text_wrap = text_style.text_wrap;
        target.white_space_collapse = text_style.white_space_collapse;
        target.line_break = text_style.line_break;
        target.math_style = text_style.math_style;
        target.math_depth = text_style.math_depth;
        target.orphans = text_style.orphans;
        target.widows = text_style.widows;
        target.column_span = text_style.column_span;
        target.break_before = text_style.break_before;
        target.break_after = text_style.break_after;
        target.break_inside = text_style.break_inside;
        target.page_break_before = text_style.page_break_before;
        target.page_break_after = text_style.page_break_after;
        target.page_break_inside = text_style.page_break_inside;
        target.page = text_style.page;
        target.hyphens = text_style.hyphens;
        target.text_justify = text_style.text_justify;
        target.text_underline_offset = text_style.text_underline_offset;
        target.text_underline_position = text_style.text_underline_position;
        target.font_variant = text_style.font_variant;
        target.font_variant_caps = text_style.font_variant_caps;
        target.font_variant_numeric = text_style.font_variant_numeric;
        target.font_synthesis = text_style.font_synthesis;
        target.font_variant_alternates = text_style.font_variant_alternates;
        target.font_feature_settings = text_style.font_feature_settings;
        target.font_variation_settings = text_style.font_variation_settings;
        target.font_optical_sizing = text_style.font_optical_sizing;
        target.print_color_adjust = text_style.print_color_adjust;
        target.image_orientation = text_style.image_orientation;
        target.image_orientation_explicit = text_style.image_orientation_explicit;
        target.font_kerning = text_style.font_kerning;
        target.font_variant_ligatures = text_style.font_variant_ligatures;
        target.font_variant_east_asian = text_style.font_variant_east_asian;
        target.font_palette = text_style.font_palette;
        target.font_variant_position = text_style.font_variant_position;
        target.font_language_override = text_style.font_language_override;
        target.font_size_adjust = text_style.font_size_adjust;
        target.font_stretch = text_style.font_stretch;
        target.text_decoration_skip_ink = text_style.text_decoration_skip_ink;
        target.text_emphasis_style = text_style.text_emphasis_style;
        target.text_emphasis_color = text_style.text_emphasis_color;
        target.text_stroke_width = text_style.text_stroke_width;
        target.text_stroke_color = color_to_argb(text_style.text_stroke_color);
        if (text_style.text_fill_color.a > 0) {
            target.text_fill_color = color_to_argb(text_style.text_fill_color);
        }
        target.hanging_punctuation = text_style.hanging_punctuation;
        target.ruby_align = text_style.ruby_align;
        target.ruby_position = text_style.ruby_position;
        target.ruby_overhang = text_style.ruby_overhang;
        target.text_orientation = text_style.text_orientation;
        target.writing_mode = text_style.writing_mode;
        target.direction = (text_style.direction == clever::css::Direction::Rtl) ? 1 : 0;
        target.quotes = text_style.quotes;
        target.text_rendering = text_style.text_rendering;
        target.font_smooth = text_style.font_smooth;
        target.text_size_adjust = text_style.text_size_adjust;
        target.caret_color = color_to_argb(text_style.caret_color);
        target.accent_color = text_style.accent_color;
        target.color_interpolation = text_style.color_interpolation;
        target.text_shadow_offset_x = text_style.text_shadow_offset_x;
        target.text_shadow_offset_y = text_style.text_shadow_offset_y;
        target.text_shadow_blur = text_style.text_shadow_blur;
        target.text_shadow_color = color_to_argb(text_style.text_shadow_color);
        target.text_shadows.clear();
        for (const auto& ts : text_style.text_shadows) {
            clever::layout::LayoutNode::TextShadowEntry e;
            e.offset_x = ts.offset_x;
            e.offset_y = ts.offset_y;
            e.blur = ts.blur;
            e.color = color_to_argb(ts.color);
            target.text_shadows.push_back(e);
        }
        target.white_space_pre = false;
        target.white_space_nowrap = false;
        switch (text_style.white_space) {
            case clever::css::WhiteSpace::Pre:
                target.white_space = 2;
                target.white_space_pre = true;
                target.white_space_nowrap = true;
                break;
            case clever::css::WhiteSpace::PreWrap:
                target.white_space = 3;
                target.white_space_pre = true;
                break;
            case clever::css::WhiteSpace::NoWrap:
                target.white_space = 1;
                target.white_space_nowrap = true;
                break;
            case clever::css::WhiteSpace::PreLine:
                target.white_space = 4;
                break;
            case clever::css::WhiteSpace::BreakSpaces:
                target.white_space = 5;
                target.white_space_pre = true;
                break;
            default:
                target.white_space = 0;
                break;
        }
    };

    auto apply_generated_text_transform = [](std::string& text, clever::css::TextTransform transform) {
        if (transform == clever::css::TextTransform::Uppercase) {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        } else if (transform == clever::css::TextTransform::Lowercase) {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        } else if (transform == clever::css::TextTransform::Capitalize) {
            bool cap_next = true;
            for (size_t i = 0; i < text.size(); i++) {
                if (std::isspace(static_cast<unsigned char>(text[i]))) {
                    cap_next = true;
                } else if (cap_next) {
                    text[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
                    cap_next = false;
                }
            }
        }
    };

    auto build_generated_pseudo_node =
        [&](const clever::css::ComputedStyle& pseudo_style) -> std::unique_ptr<clever::layout::LayoutNode> {
        const std::string content_raw = pseudo_style.content;
        const std::string content_trim = trim(content_raw);
        const std::string content_lower = to_lower(content_trim);
        if (content_lower == "none" || content_lower == "normal") return nullptr;

        const bool has_declared_content = !content_trim.empty();
        if (!has_declared_content && pseudo_style.display == clever::css::Display::Inline) {
            return nullptr;
        }

        auto pseudo_node = std::make_unique<clever::layout::LayoutNode>();
        pseudo_node->mode = display_to_mode(pseudo_style.display);
        pseudo_node->display = display_to_type(pseudo_style.display);
        pseudo_node->link_href = link;
        pseudo_node->link_target = link_target;
        pseudo_node->background_color = color_to_argb(pseudo_style.background_color);
        pseudo_node->opacity = pseudo_style.opacity * style.opacity;
        pseudo_node->mix_blend_mode = pseudo_style.mix_blend_mode;
        pseudo_node->visibility_hidden = (pseudo_style.visibility == clever::css::Visibility::Hidden ||
                                          pseudo_style.visibility == clever::css::Visibility::Collapse);
        pseudo_node->visibility_collapse = (pseudo_style.visibility == clever::css::Visibility::Collapse);
        pseudo_node->transforms = pseudo_style.transforms;
        pseudo_node->text_align_last = pseudo_style.text_align_last;
        switch (pseudo_style.text_align) {
            case clever::css::TextAlign::Center: pseudo_node->text_align = 1; break;
            case clever::css::TextAlign::Right: pseudo_node->text_align = 2; break;
            case clever::css::TextAlign::Justify: pseudo_node->text_align = 3; break;
            case clever::css::TextAlign::WebkitCenter: pseudo_node->text_align = 4; break;
            default: pseudo_node->text_align = 0; break;
        }

        apply_generated_text_style(pseudo_style, *pseudo_node, font_size);
        const float ps_fs = pseudo_node->font_size;
        const float ps_lh = pseudo_style.line_height.to_px(ps_fs, 16.0f);

        pseudo_node->text_indent = pseudo_style.text_indent.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->specified_width = pseudo_style.width.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->specified_height = pseudo_style.height.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.padding.left = pseudo_style.padding.left.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.padding.right = pseudo_style.padding.right.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.padding.top = pseudo_style.padding.top.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.padding.bottom = pseudo_style.padding.bottom.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.margin.left = pseudo_style.margin.left.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.margin.right = pseudo_style.margin.right.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.margin.top = pseudo_style.margin.top.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.margin.bottom = pseudo_style.margin.bottom.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.border.left =
            (pseudo_style.border_left.style == clever::css::BorderStyle::None)
                ? 0.0f
                : pseudo_style.border_left.width.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.border.right =
            (pseudo_style.border_right.style == clever::css::BorderStyle::None)
                ? 0.0f
                : pseudo_style.border_right.width.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.border.top =
            (pseudo_style.border_top.style == clever::css::BorderStyle::None)
                ? 0.0f
                : pseudo_style.border_top.width.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->geometry.border.bottom =
            (pseudo_style.border_bottom.style == clever::css::BorderStyle::None)
                ? 0.0f
                : pseudo_style.border_bottom.width.to_px(ps_fs, 16.0f, ps_lh);
        pseudo_node->border_color_top = color_to_argb(pseudo_style.border_top.color);
        pseudo_node->border_color_right = color_to_argb(pseudo_style.border_right.color);
        pseudo_node->border_color_bottom = color_to_argb(pseudo_style.border_bottom.color);
        pseudo_node->border_color_left = color_to_argb(pseudo_style.border_left.color);
        pseudo_node->border_style_top = convert_pseudo_border_style(pseudo_style.border_top.style);
        pseudo_node->border_style_right = convert_pseudo_border_style(pseudo_style.border_right.style);
        pseudo_node->border_style_bottom = convert_pseudo_border_style(pseudo_style.border_bottom.style);
        pseudo_node->border_style_left = convert_pseudo_border_style(pseudo_style.border_left.style);
        pseudo_node->border_style = pseudo_node->border_style_top;
        pseudo_node->border_radius = pseudo_style.border_radius;
        pseudo_node->border_radius_tl = pseudo_style.border_radius_tl;
        pseudo_node->border_radius_tr = pseudo_style.border_radius_tr;
        pseudo_node->border_radius_bl = pseudo_style.border_radius_bl;
        pseudo_node->border_radius_br = pseudo_style.border_radius_br;

        if (has_declared_content) {
            std::string text = resolve_content_value(
                pseudo_style.content, pseudo_style.content_attr_name, node);
            apply_generated_text_transform(text, pseudo_style.text_transform);

            auto text_child = std::make_unique<clever::layout::LayoutNode>();
            text_child->is_text = true;
            text_child->mode = clever::layout::LayoutMode::Inline;
            text_child->display = clever::layout::DisplayType::Inline;
            text_child->text_content = std::move(text);
            text_child->link_href = link;
            text_child->link_target = link_target;
            text_child->opacity = pseudo_node->opacity;
            text_child->visibility_hidden = pseudo_node->visibility_hidden;
            text_child->visibility_collapse = pseudo_node->visibility_collapse;
            apply_generated_text_style(pseudo_style, *text_child, font_size);
            pseudo_node->append_child(std::move(text_child));
        }

        return pseudo_node;
    };

    // CSS text-decoration propagation: text-decoration is NOT inherited per CSS
    // spec, but the decoration visually propagates to all inline descendants.
    // If this element doesn't have its own text-decoration but the parent does,
    // propagate the parent's decoration so descendant text nodes display it.
    if (style.text_decoration == clever::css::TextDecoration::None &&
        style.text_decoration_bits == 0 &&
        (parent_style.text_decoration != clever::css::TextDecoration::None ||
         parent_style.text_decoration_bits != 0)) {
        style.text_decoration = parent_style.text_decoration;
        style.text_decoration_bits |= parent_style.text_decoration_bits;
        // Propagate decoration styling too (color, style, thickness) if not explicitly set
        if (style.text_decoration_color == clever::css::Color{0, 0, 0, 0}) {
            style.text_decoration_color = parent_style.text_decoration_color;
        }
        if (style.text_decoration_style == clever::css::TextDecorationStyle::Solid &&
            parent_style.text_decoration_style != clever::css::TextDecorationStyle::Solid) {
            style.text_decoration_style = parent_style.text_decoration_style;
        }
        if (style.text_decoration_thickness == 0 && parent_style.text_decoration_thickness > 0) {
            style.text_decoration_thickness = parent_style.text_decoration_thickness;
        }
    }

    // Check for ::before pseudo-element and insert synthetic node as first child
    if (elem_view) {
        auto before_style = resolver.resolve_pseudo(*elem_view, "before", style);
        if (before_style) {
            auto pseudo_node = build_generated_pseudo_node(*before_style);
            if (pseudo_node) {
                pseudo_node->parent = layout_node.get();
                layout_node->children.insert(layout_node->children.begin(), std::move(pseudo_node));
            }
        }
    }

    // Shadow DOM: if this element has an attached shadow root, render the shadow
    // tree's children instead of the element's light DOM children.
    // <slot> elements in the shadow tree are replaced with the host's light DOM children.
    clever::html::SimpleNode* shadow_root_node = nullptr;
    if (g_shadow_roots && node.type == clever::html::SimpleNode::Element) {
        auto shadow_it = g_shadow_roots->find(const_cast<clever::html::SimpleNode*>(&node));
        if (shadow_it != g_shadow_roots->end()) {
            shadow_root_node = shadow_it->second;
        }
    }

    if (shadow_root_node) {
        // Helper lambda: iterate shadow root children, replacing <slot> with
        // matching light DOM children from the host node.
        std::function<void(const clever::html::SimpleNode&, clever::layout::LayoutNode&)>
            build_shadow_subtree = [&](const clever::html::SimpleNode& shadow_parent,
                                       clever::layout::LayoutNode& parent_layout_node) {
            for (auto& shadow_child : shadow_parent.children) {
                if (!shadow_child) continue;
                std::string sc_tag;
                if (shadow_child->type == clever::html::SimpleNode::Element) {
                    sc_tag = to_lower(shadow_child->tag_name);
                }
                if (sc_tag == "slot") {
                    // <slot> — substitute matching light DOM children from the host.
                    // Named slot: only children with slot="<name>" match.
                    // Default slot: children without a slot attribute match.
                    std::string slot_name = get_attr(*shadow_child, "name");
                    bool any_slotted = false;
                    for (auto& light_child : node.children) {
                        if (!light_child) continue;
                        std::string child_slot_attr;
                        if (light_child->type == clever::html::SimpleNode::Element) {
                            child_slot_attr = get_attr(*light_child, "slot");
                        }
                        bool matches = slot_name.empty()
                            ? child_slot_attr.empty()
                            : (child_slot_attr == slot_name);
                        if (matches) {
                            auto slotted_layout = build_layout_tree_styled(
                                *light_child, style, resolver, view_tree, elem_view,
                                base_url, link, form, link_target);
                            if (slotted_layout) {
                                if (slotted_layout->display_contents) {
                                    for (auto& gc : slotted_layout->children) {
                                        parent_layout_node.append_child(std::move(gc));
                                    }
                                } else {
                                    parent_layout_node.append_child(std::move(slotted_layout));
                                }
                                any_slotted = true;
                            }
                        }
                    }
                    // If no light DOM children matched, render the slot's fallback content
                    if (!any_slotted) {
                        build_shadow_subtree(*shadow_child, parent_layout_node);
                    }
                } else {
                    // Regular shadow tree node — build via normal pipeline
                    auto shadow_child_layout = build_layout_tree_styled(
                        *shadow_child, style, resolver, view_tree, elem_view,
                        base_url, link, form, link_target);
                    if (shadow_child_layout) {
                        if (shadow_child_layout->display_contents) {
                            for (auto& gc : shadow_child_layout->children) {
                                parent_layout_node.append_child(std::move(gc));
                            }
                        } else {
                            parent_layout_node.append_child(std::move(shadow_child_layout));
                        }
                    }
                }
            }
        };
        build_shadow_subtree(*shadow_root_node, *layout_node);
    } else {
        // Normal (non-shadow-DOM) child iteration
        for (auto& child : node.children) {
            auto child_layout = build_layout_tree_styled(
                *child, style, resolver, view_tree, elem_view, base_url, link, form, link_target);
            if (child_layout) {
                // display: contents — flatten grandchildren into this node
                if (child_layout->display_contents) {
                    for (auto& grandchild : child_layout->children) {
                        layout_node->append_child(std::move(grandchild));
                    }
                } else {
                    layout_node->append_child(std::move(child_layout));
                }
            }
        }
    }

    // Legacy <center> centers not only inline content but also common block/table
    // descendants in quirks-era markup.
    // -webkit-center (text_align == 4) does the same: auto-center block children.
    if (tag_lower == "center" || layout_node->text_align == 4) {
        for (auto& child : layout_node->children) {
            if (!child) continue;
            // Auto-center block children that don't already have auto margins.
            // <center> and -webkit-center override default/explicit left/right margins.
            if (!clever::layout::is_margin_auto(child->geometry.margin.left) && !clever::layout::is_margin_auto(child->geometry.margin.right)) {
                if (child->display == clever::layout::DisplayType::Block ||
                    child->display == clever::layout::DisplayType::Table ||
                    child->display == clever::layout::DisplayType::InlineBlock) {
                    child->geometry.margin.left = MARGIN_AUTO;  // auto
                    child->geometry.margin.right = MARGIN_AUTO; // auto
                }
            }
        }
        // For -webkit-center, also treat inline text centering as Center
        if (layout_node->text_align == 4) {
            layout_node->text_align = 1; // center for inline content
        }
    }

    // Check for ::after pseudo-element and insert synthetic node as last child
    if (elem_view) {
        auto after_style = resolver.resolve_pseudo(*elem_view, "after", style);
        if (after_style) {
            auto pseudo_node = build_generated_pseudo_node(*after_style);
            if (pseudo_node) {
                layout_node->append_child(std::move(pseudo_node));
            }
        }
    }

    // Check for ::first-letter pseudo-element and propagate styling to text children
    if (elem_view) {
        auto fl_style = resolver.resolve_pseudo(*elem_view, "first-letter", style);
        if (fl_style) {
            // Propagate first-letter styling to the first text child node
            std::function<bool(clever::layout::LayoutNode&)> propagate_first_letter =
                [&](clever::layout::LayoutNode& n) -> bool {
                if (n.is_text && !n.text_content.empty()) {
                    n.has_first_letter = true;
                    float fl_fs = fl_style->font_size.to_px(font_size);
                    n.first_letter_font_size = (fl_fs != font_size) ? fl_fs : 0;
                    uint32_t fl_color = color_to_argb(fl_style->color);
                    n.first_letter_color = (fl_color != n.color) ? fl_color : 0;
                    n.first_letter_bold = (fl_style->font_weight >= 700);
                    return true; // found first text node, stop
                }
                for (auto& child : n.children) {
                    if (propagate_first_letter(*child)) return true;
                }
                return false;
            };
            for (auto& child : layout_node->children) {
                if (propagate_first_letter(*child)) break;
            }
        }
    }

    // Check for ::first-line pseudo-element and propagate styling to text children
    if (elem_view) {
        auto fline_style = resolver.resolve_pseudo(*elem_view, "first-line", style);
        if (fline_style) {
            // Propagate first-line styling to the first text child node
            std::function<bool(clever::layout::LayoutNode&)> propagate_first_line =
                [&](clever::layout::LayoutNode& n) -> bool {
                if (n.is_text && !n.text_content.empty()) {
                    n.has_first_line = true;
                    float fl_fs = fline_style->font_size.to_px(font_size);
                    n.first_line_font_size = (fl_fs != font_size) ? fl_fs : 0;
                    uint32_t fl_color = color_to_argb(fline_style->color);
                    n.first_line_color = (fl_color != n.color) ? fl_color : 0;
                    n.first_line_bold = (fline_style->font_weight >= 700);
                    n.first_line_italic = (fline_style->font_style != clever::css::FontStyle::Normal);
                    return true; // found first text node, stop
                }
                for (auto& child : n.children) {
                    if (propagate_first_line(*child)) return true;
                }
                return false;
            };
            for (auto& child : layout_node->children) {
                if (propagate_first_line(*child)) break;
            }
        }
    }

    // Check for ::selection pseudo-element and propagate styling to text children
    if (elem_view) {
        auto sel_style = resolver.resolve_pseudo(*elem_view, "selection", style);
        if (sel_style) {
            uint32_t sel_color = color_to_argb(sel_style->color);
            uint32_t sel_bg = color_to_argb(sel_style->background_color);
            // Set selection styling on the element itself
            if (sel_color != 0) layout_node->selection_color = sel_color;
            if (sel_bg != 0) layout_node->selection_bg_color = sel_bg;
            // Propagate selection styling to all text children
            std::function<void(clever::layout::LayoutNode&)> propagate_selection =
                [&](clever::layout::LayoutNode& n) {
                if (n.is_text && !n.text_content.empty()) {
                    if (sel_color != 0) n.selection_color = sel_color;
                    if (sel_bg != 0) n.selection_bg_color = sel_bg;
                }
                for (auto& child : n.children) {
                    propagate_selection(*child);
                }
            };
            for (auto& child : layout_node->children) {
                propagate_selection(*child);
            }
        }
    }

    // Check for ::marker pseudo-element and propagate styling to list item children
    if (elem_view) {
        auto marker_style = resolver.resolve_pseudo(*elem_view, "marker", style);
        if (marker_style) {
            uint32_t mk_color = color_to_argb(marker_style->color);
            float mk_fs = marker_style->font_size.to_px(font_size);
            auto is_generated_marker_text = [](const clever::layout::LayoutNode& n) {
                if (!n.is_text) return false;
                const auto& txt = n.text_content;
                if (txt.empty()) return false;
                if (txt[0] == '\xE2') return true; // bullet/circle/square UTF-8 markers
                return txt.size() >= 2 && txt.back() == ' ' &&
                       (std::isdigit(static_cast<unsigned char>(txt[0])) ||
                        std::isalpha(static_cast<unsigned char>(txt[0])));
            };
            auto apply_marker_style_to_list_item = [&](clever::layout::LayoutNode& n) {
                if (!n.is_list_item) return;
                if (mk_color != 0) n.marker_color = mk_color;
                if (mk_fs > 0) n.marker_font_size = mk_fs;

                // Inside markers are emitted as inline text children during list-item
                // construction; sync them with ::marker styles resolved later here.
                if (n.list_style_position == 1 &&
                    !n.children.empty() &&
                    is_generated_marker_text(*n.children[0])) {
                    if (mk_color != 0) n.children[0]->color = mk_color;
                    if (mk_fs > 0) n.children[0]->font_size = mk_fs;
                    return;
                }

                // Outside markers are painted by paint_list_marker(). Add a zero-width
                // leading marker placeholder so ::marker text style is still represented
                // in the layout tree without causing duplicate visible markers.
                if (n.list_style_position == 0 && (mk_color != 0 || mk_fs > 0)) {
                    bool has_marker_text_child =
                        !n.children.empty() && is_generated_marker_text(*n.children[0]);
                    if (!has_marker_text_child) {
                        auto marker_placeholder = std::make_unique<clever::layout::LayoutNode>();
                        marker_placeholder->is_text = true;
                        marker_placeholder->text_content = "";
                        marker_placeholder->mode = clever::layout::LayoutMode::Inline;
                        marker_placeholder->display = clever::layout::DisplayType::Inline;
                        marker_placeholder->color = (mk_color != 0) ? mk_color : n.color;
                        marker_placeholder->font_size = (mk_fs > 0) ? mk_fs : n.font_size;
                        n.children.insert(n.children.begin(), std::move(marker_placeholder));
                    }
                }
            };

            apply_marker_style_to_list_item(*layout_node);

            // Propagate marker styling to descendant list items
            std::function<void(clever::layout::LayoutNode&)> propagate_marker =
                [&](clever::layout::LayoutNode& n) {
                apply_marker_style_to_list_item(n);
                for (auto& child : n.children) {
                    propagate_marker(*child);
                }
            };
            for (auto& child : layout_node->children) {
                propagate_marker(*child);
            }
        }
    }

    // Collect col_widths on table node from colgroup/col children
    if (tag_lower == "table") {
        std::function<void(const clever::layout::LayoutNode&)> collect_col_widths =
            [&](const clever::layout::LayoutNode& n) {
            if (n.is_col) {
                float w = n.specified_width; // -1 if not set
                for (int s = 0; s < n.col_span; s++) {
                    layout_node->col_widths.push_back(w);
                }
            }
            for (auto& child : n.children) {
                collect_col_widths(*child);
            }
        };
        for (auto& child : layout_node->children) {
            collect_col_widths(*child);
        }

        // Collect column backgrounds from <col> elements and apply to cells
        {
            std::vector<uint32_t> col_bgs;
            std::function<void(const clever::layout::LayoutNode&)> collect_col_bgs =
                [&](const clever::layout::LayoutNode& n) {
                if (n.is_col) {
                    for (int s = 0; s < n.col_span; s++) {
                        col_bgs.push_back(n.background_color);
                    }
                }
                for (auto& child : n.children) collect_col_bgs(*child);
            };
            for (auto& child : layout_node->children) collect_col_bgs(*child);

            // Apply column backgrounds to td/th cells
            if (!col_bgs.empty()) {
                std::function<void(clever::layout::LayoutNode&)> apply_col_bg =
                    [&](clever::layout::LayoutNode& n) {
                    std::string tn = n.tag_name;
                    std::transform(tn.begin(), tn.end(), tn.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (tn == "tr") {
                        // Apply column backgrounds to cells in this row
                        int col_idx = 0;
                        for (auto& cell : n.children) {
                            std::string ct = cell->tag_name;
                            std::transform(ct.begin(), ct.end(), ct.begin(),
                                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if ((ct == "td" || ct == "th") && col_idx < static_cast<int>(col_bgs.size())) {
                                if (col_bgs[col_idx] != 0 && cell->background_color == 0) {
                                    cell->background_color = col_bgs[col_idx];
                                }
                            }
                            col_idx++;
                        }
                    }
                    for (auto& child : n.children) apply_col_bg(*child);
                };
                for (auto& child : layout_node->children) apply_col_bg(*child);
            }
        }

        // Propagate cellpadding to td/th cells
        if (layout_node->table_cellpadding >= 0) {
            float cp = layout_node->table_cellpadding;
            std::function<void(clever::layout::LayoutNode&)> apply_cellpadding =
                [&](clever::layout::LayoutNode& n) {
                std::string tn = n.tag_name;
                std::transform(tn.begin(), tn.end(), tn.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (tn == "td" || tn == "th") {
                    // Cellpadding overrides default padding (4,8,4,8) but not CSS-specified
                    n.geometry.padding = {cp, cp, cp, cp};
                }
                for (auto& child : n.children) apply_cellpadding(*child);
            };
            for (auto& child : layout_node->children) apply_cellpadding(*child);
        }

        // HTML border attribute: propagate cell borders
        // border=0 → no cell borders; border>0 → 1px cell borders; not set → no default
        if (layout_node->table_border >= 0) {
            int bval = layout_node->table_border;
            std::function<void(clever::layout::LayoutNode&)> apply_table_border =
                [&](clever::layout::LayoutNode& n) {
                std::string tn = n.tag_name;
                std::transform(tn.begin(), tn.end(), tn.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (tn == "td" || tn == "th") {
                    if (bval == 0) {
                        n.geometry.border = {0, 0, 0, 0};
                    } else if (bval > 0) {
                        n.geometry.border = {1, 1, 1, 1};
                        if (n.border_color == 0) n.border_color = 0xFFCCCCCC;
                    }
                }
                for (auto& child : n.children) apply_table_border(*child);
            };
            for (auto& child : layout_node->children) apply_table_border(*child);
        }

        // HTML rules attribute: apply cell borders based on rules value
        if (!layout_node->table_rules.empty()) {
            auto& rules = layout_node->table_rules;
            std::function<void(clever::layout::LayoutNode&)> apply_rules =
                [&](clever::layout::LayoutNode& n) {
                std::string tn = n.tag_name;
                std::transform(tn.begin(), tn.end(), tn.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (tn == "td" || tn == "th") {
                    if (rules == "none") {
                        n.geometry.border = {0, 0, 0, 0};
                    } else if (rules == "all") {
                        n.geometry.border = {1, 1, 1, 1};
                        if (n.border_color == 0) n.border_color = 0xFFCCCCCC;
                    } else if (rules == "rows") {
                        n.geometry.border = {1, 0, 1, 0}; // top+bottom only
                        if (n.border_color == 0) n.border_color = 0xFFCCCCCC;
                    } else if (rules == "cols") {
                        n.geometry.border = {0, 1, 0, 1}; // left+right only
                        if (n.border_color == 0) n.border_color = 0xFFCCCCCC;
                    }
                }
                for (auto& child : n.children) apply_rules(*child);
            };
            for (auto& child : layout_node->children) apply_rules(*child);
        }

        // caption-side: bottom — move caption children to end of children list
        // (block layout renders children top-to-bottom in DOM order)
        if (layout_node->caption_side == 1) {
            std::vector<std::unique_ptr<clever::layout::LayoutNode>> captions;
            std::vector<std::unique_ptr<clever::layout::LayoutNode>> others;
            for (auto& child : layout_node->children) {
                std::string ct = child->tag_name;
                std::transform(ct.begin(), ct.end(), ct.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ct == "caption") {
                    captions.push_back(std::move(child));
                } else {
                    others.push_back(std::move(child));
                }
            }
            layout_node->children.clear();
            for (auto& o : others) layout_node->children.push_back(std::move(o));
            for (auto& c : captions) layout_node->children.push_back(std::move(c));
        }

        // Build and apply a table layout pass for caption positioning, columns,
        // rows, spanning, border collapse, and vertical alignment.
        auto compute_table_layout = [&](clever::layout::LayoutNode& table_node) -> void {
            if (table_node.children.empty()) {
                return;
            }

            auto is_row_tag = [](const std::string& tag) -> bool {
                if (tag.size() != 2) return false;
                auto t = to_lower(tag);
                return t == "tr";
            };
            auto is_cell_tag = [](const std::string& tag) -> bool {
                if (tag.size() != 2) return false;
                auto t = to_lower(tag);
                return t == "td" || t == "th";
            };
            auto is_caption_tag = [](const std::string& tag) -> bool {
                if (tag.size() != 7) return false;
                auto t = to_lower(tag);
                return t == "caption";
            };
            auto is_section_tag = [](const std::string& tag) -> bool {
                if (tag.size() < 5) return false;
                auto t = to_lower(tag);
                return t == "thead" || t == "tbody" || t == "tfoot";
            };
            auto is_table_row = [&](const clever::layout::LayoutNode& candidate) -> bool {
                return candidate.display == clever::layout::DisplayType::TableRow || is_row_tag(candidate.tag_name);
            };
            auto is_table_cell = [&](const clever::layout::LayoutNode& candidate) -> bool {
                return candidate.display == clever::layout::DisplayType::TableCell || is_cell_tag(candidate.tag_name);
            };

            struct CellPlacement {
                clever::layout::LayoutNode* cell = nullptr;
                int row = 0;
                int col = 0;
                int colspan = 1;
                int rowspan = 1;
            };

            std::vector<clever::layout::LayoutNode*> rows;
            std::vector<clever::layout::LayoutNode*> top_captions;
            std::vector<clever::layout::LayoutNode*> bottom_captions;

            for (auto& child : table_node.children) {
                if (child->display == clever::layout::DisplayType::None || child->mode == clever::layout::LayoutMode::None) {
                    continue;
                }
                if (child->position_type == 2 || child->position_type == 3) {
                    continue;
                }
                if (is_caption_tag(child->tag_name)) {
                    if (child->caption_side == 0) {
                        top_captions.push_back(child.get());
                    } else {
                        bottom_captions.push_back(child.get());
                    }
                    continue;
                }
                if (is_section_tag(child->tag_name)) {
                    for (auto& grandchild : child->children) {
                        if (grandchild->display == clever::layout::DisplayType::None || grandchild->mode == clever::layout::LayoutMode::None) {
                            continue;
                        }
                        if (grandchild->position_type == 2 || grandchild->position_type == 3) {
                            continue;
                        }
                        if (is_table_row(*grandchild)) {
                            rows.push_back(grandchild.get());
                        }
                    }
                    continue;
                }
                if (is_table_row(*child)) {
                    rows.push_back(child.get());
                }
            }

            float h_spacing = table_node.border_collapse ? 0.0f : table_node.border_spacing;
            float v_spacing = table_node.border_collapse ? 0.0f
                               : (table_node.border_spacing_v > 0 ? table_node.border_spacing_v : table_node.border_spacing);
            bool fixed_layout = table_node.table_layout == 1;

            int row_count = static_cast<int>(rows.size());
            int col_count = 0;

            auto estimate_node_width = [&](auto&& self,
                                          const clever::layout::LayoutNode& node) -> float {
                float w = 0.0f;
                if (node.geometry.width > 0.0f) {
                    w = node.geometry.width;
                } else if (node.specified_width >= 0.0f) {
                    w = node.specified_width;
                } else if (node.css_width.has_value()) {
                    float base = table_node.geometry.width > 0.0f ? table_node.geometry.width : 800.0f;
                    float parsed = node.css_width->to_px(base);
                    if (parsed > 0.0f) w = parsed;
                } else if (node.image_width > 0) {
                    w = static_cast<float>(node.image_width);
                } else if (node.is_text) {
                    float fs = node.font_size > 0.0f ? node.font_size : 16.0f;
                    float per_ch = fs * 0.55f + std::max(0.0f, node.letter_spacing);
                    w = static_cast<float>(node.text_content.size()) * per_ch;
                }

                if (w > 0.0f) {
                    return w + node.geometry.padding.left + node.geometry.padding.right
                           + node.geometry.border.left + node.geometry.border.right;
                }

                float inline_sum = 0.0f;
                float block_max = 0.0f;
                bool inline_seen = false;
                bool block_seen = false;
                for (auto& child : node.children) {
                    float child_w = self(self, *child);
                    bool is_inline = child->mode == clever::layout::LayoutMode::Inline
                                  || child->display == clever::layout::DisplayType::Inline
                                  || child->is_text
                                  || child->display == clever::layout::DisplayType::InlineBlock;
                    if (is_inline) {
                        inline_sum += child_w;
                        inline_seen = true;
                    } else {
                        block_max = std::max(block_max, child_w);
                        block_seen = true;
                    }
                }

                if (block_seen) {
                    w = std::max(block_max, inline_sum);
                } else if (inline_seen) {
                    w = inline_sum;
                } else {
                    float fs = node.font_size > 0.0f ? node.font_size : 16.0f;
                    w = fs;
                }

                return w + node.geometry.padding.left + node.geometry.padding.right
                       + node.geometry.border.left + node.geometry.border.right;
            };

            auto estimate_node_height = [&](auto&& self,
                                           const clever::layout::LayoutNode& node) -> float {
                if (node.geometry.height > 0.0f) {
                    return node.geometry.height;
                }
                if (node.is_text) {
                    float fs = node.font_size > 0.0f ? node.font_size : 16.0f;
                    float lh = node.line_height > 0.0f ? node.line_height : 1.2f;
                    float lines = 1.0f;
                    for (auto ch : node.text_content) {
                        if (ch == '\n') lines += 1.0f;
                    }
                    return lines * fs * lh;
                }
                if (node.image_height > 0) {
                    return static_cast<float>(node.image_height);
                }

                float inline_max = 0.0f;
                float block_sum = 0.0f;
                bool block_seen = false;
                for (auto& child : node.children) {
                    float child_h = self(self, *child);
                    bool is_inline = child->mode == clever::layout::LayoutMode::Inline
                                  || child->display == clever::layout::DisplayType::Inline
                                  || child->is_text
                                  || child->display == clever::layout::DisplayType::InlineBlock;
                    if (is_inline) {
                        inline_max = std::max(inline_max, child_h);
                    } else {
                        block_sum += child_h;
                        block_seen = true;
                    }
                }

                if (block_seen) {
                    return block_sum + inline_max
                           + node.geometry.padding.top + node.geometry.padding.bottom
                           + node.geometry.border.top + node.geometry.border.bottom;
                }
                if (inline_max > 0.0f) {
                    return inline_max
                           + node.geometry.padding.top + node.geometry.padding.bottom
                           + node.geometry.border.top + node.geometry.border.bottom;
                }

                float fs = node.font_size > 0.0f ? node.font_size : 16.0f;
                float lh = node.line_height > 0.0f ? node.line_height : 1.2f;
                return fs * lh
                       + node.geometry.padding.top + node.geometry.padding.bottom
                       + node.geometry.border.top + node.geometry.border.bottom;
            };

            auto measure_color_brightness = [](uint32_t c) -> float {
                float a = static_cast<float>((c >> 24) & 0xFF) / 255.0f;
                if (a <= 0.0f) return 0.0f;
                float r = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
                float g = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
                float b = static_cast<float>(c & 0xFF) / 255.0f;
                return 0.299f * r + 0.587f * g + 0.114f * b;
            };

            auto spread_span_width = [&](std::vector<float>& widths,
                                        const std::vector<int>& is_locked,
                                        int start, int span, float requested,
                                        float spacing) {
                if (start < 0 || span <= 1 || requested <= 0.0f || col_count <= 0) return;
                int end = std::min(col_count, start + span);
                if (end <= start) return;
                float current = 0.0f;
                int variable = 0;
                for (int i = start; i < end; ++i) {
                    current += widths[static_cast<size_t>(i)];
                    if (is_locked[static_cast<size_t>(i)] == 0) {
                        ++variable;
                    }
                }
                float needed = std::max(0.0f, requested - spacing * static_cast<float>(std::max(0, end - start - 1)));
                float deficit = needed - current;
                if (deficit <= 0.0f) return;

                if (variable > 0) {
                    float weight_sum = 0.0f;
                    for (int i = start; i < end; ++i) {
                        if (is_locked[static_cast<size_t>(i)] == 0) {
                            weight_sum += std::max(1.0f, widths[static_cast<size_t>(i)]);
                        }
                    }
                    if (weight_sum <= 0.0f) {
                        weight_sum = static_cast<float>(variable);
                    }
                    for (int i = start; i < end; ++i) {
                        if (is_locked[static_cast<size_t>(i)] == 0) {
                            widths[static_cast<size_t>(i)] += deficit
                                * (std::max(1.0f, widths[static_cast<size_t>(i)]) / weight_sum);
                        }
                    }
                } else if (current > 0.0f) {
                    float factor = (current + deficit) / current;
                    for (int i = start; i < end; ++i) {
                        widths[static_cast<size_t>(i)] *= factor;
                    }
                } else {
                    float each = needed / static_cast<float>(end - start);
                    for (int i = start; i < end; ++i) {
                        widths[static_cast<size_t>(i)] = std::max(widths[static_cast<size_t>(i)], each);
                    }
                }
            };

            auto width_hint = [&](const clever::layout::LayoutNode& cell) -> float {
                if (cell.specified_width >= 0.0f) return cell.specified_width;
                if (cell.css_width.has_value()) {
                    float base = table_node.geometry.width > 0.0f ? table_node.geometry.width : 800.0f;
                    float w = cell.css_width->to_px(base);
                    if (w > 0.0f) return w;
                }
                return estimate_node_width(estimate_node_width, cell);
            };

            auto recompute_row_tops = [&](const std::vector<float>& heights,
                                          std::vector<float>& tops,
                                          float first_top) {
                if (heights.empty()) return;
                tops[0] = first_top;
                for (size_t i = 1; i < heights.size(); ++i) {
                    tops[static_cast<size_t>(i)] = tops[static_cast<size_t>(i - 1)]
                        + heights[static_cast<size_t>(i - 1)] + v_spacing;
                }
            };

            auto take_collapsed_border = [&](float left_w, float right_w, uint32_t left_c, uint32_t right_c) -> bool {
                if (left_w > right_w) return true;
                if (right_w > left_w) return false;
                float luma_left = measure_color_brightness(left_c);
                float luma_right = measure_color_brightness(right_c);
                if (luma_left == luma_right) return true;
                return luma_left < luma_right;
            };

            auto row_cell_offset = [&](float row_h, float cell_h, int valign, float baseline) -> float {
                float v_off = 0.0f;
                switch (valign) {
                    case 1:
                        v_off = 0.0f;
                        break;
                    case 2:
                        v_off = (row_h - cell_h) * 0.5f;
                        break;
                    case 3:
                        v_off = row_h - cell_h;
                        break;
                    case 0:
                    default:
                        v_off = baseline - (cell_h * 0.8f);
                        break;
                }
                return v_off < 0.0f ? 0.0f : v_off;
            };

            auto ensure_min_width = [&](float value) {
                if (value <= 0.0f) value = 1.0f;
                return value;
            };

            std::vector<CellPlacement> placements;
            std::vector<std::vector<clever::layout::LayoutNode*>> cell_matrix;
            cell_matrix.resize(static_cast<size_t>(row_count));

            for (int ri = 0; ri < row_count; ++ri) {
                if (static_cast<int>(cell_matrix[static_cast<size_t>(ri)].size()) < col_count) {
                    cell_matrix[static_cast<size_t>(ri)].resize(static_cast<size_t>(col_count), nullptr);
                }

                int col_idx = 0;
                for (auto& child : rows[static_cast<size_t>(ri)]->children) {
                    if (child->display == clever::layout::DisplayType::None || child->mode == clever::layout::LayoutMode::None) {
                        continue;
                    }
                    if (child->position_type == 2 || child->position_type == 3) {
                        continue;
                    }
                    if (!is_table_cell(*child)) {
                        continue;
                    }
                    if (child->display == clever::layout::DisplayType::Table) {
                        continue;
                    }

                    int span = std::max(1, child->colspan);
                    int row_span = std::max(1, child->rowspan);
                    while (col_idx < col_count &&
                           static_cast<size_t>(col_idx) < cell_matrix[static_cast<size_t>(ri)].size() &&
                           cell_matrix[static_cast<size_t>(ri)][static_cast<size_t>(col_idx)] != nullptr) {
                        ++col_idx;
                    }

                    if (col_idx + span > col_count) {
                        col_count = col_idx + span;
                        for (auto& matrix_row : cell_matrix) {
                            if (static_cast<int>(matrix_row.size()) < col_count) {
                                matrix_row.resize(static_cast<size_t>(col_count), nullptr);
                            }
                        }
                    }

                    int occupy_rows = std::min(row_span, std::max(0, row_count - ri));
                    for (int dr = 0; dr < occupy_rows; ++dr) {
                        int target_row = ri + dr;
                        if (target_row >= row_count) continue;
                        if (static_cast<int>(cell_matrix[static_cast<size_t>(target_row)].size()) < col_count) {
                            cell_matrix[static_cast<size_t>(target_row)].resize(static_cast<size_t>(col_count), nullptr);
                        }
                        for (int dc = 0; dc < span; ++dc) {
                            int cc = col_idx + dc;
                            if (cc < col_count) {
                                cell_matrix[static_cast<size_t>(target_row)][static_cast<size_t>(cc)] = child.get();
                            }
                        }
                    }

                    placements.push_back({child.get(), ri, col_idx, span, row_span});
                    col_idx += span;
                }
            }

            if (col_count <= 0) col_count = 1;
            for (auto& matrix_row : cell_matrix) {
                if (static_cast<int>(matrix_row.size()) < col_count) {
                    matrix_row.resize(static_cast<size_t>(col_count), nullptr);
                }
            }

            std::vector<float> min_width(static_cast<size_t>(col_count), 0.0f);
            std::vector<float> max_width(static_cast<size_t>(col_count), 0.0f);
            std::vector<float> final_width(static_cast<size_t>(col_count), 0.0f);
            std::vector<int> locked(static_cast<size_t>(col_count), 0);
            for (int ci = 0; ci < col_count; ++ci) {
                if (ci < static_cast<int>(table_node.col_widths.size()) && table_node.col_widths[static_cast<size_t>(ci)] >= 0.0f) {
                    float v = table_node.col_widths[static_cast<size_t>(ci)];
                    min_width[static_cast<size_t>(ci)] = v;
                    max_width[static_cast<size_t>(ci)] = v;
                    final_width[static_cast<size_t>(ci)] = v;
                    locked[static_cast<size_t>(ci)] = 1;
                }
            }

            if (fixed_layout) {
                for (const auto& p : placements) {
                    if (p.cell == nullptr || p.row != 0) continue;
                    if (p.col < 0 || p.col >= col_count) continue;
                    int span = std::max(1, p.colspan);
                    float hint = width_hint(*p.cell);
                    if (span == 1) {
                        final_width[static_cast<size_t>(p.col)] = std::max(final_width[static_cast<size_t>(p.col)], hint);
                        min_width[static_cast<size_t>(p.col)] = std::max(min_width[static_cast<size_t>(p.col)], hint);
                        max_width[static_cast<size_t>(p.col)] = std::max(max_width[static_cast<size_t>(p.col)], hint);
                        locked[static_cast<size_t>(p.col)] = 1;
                    } else {
                        float each = hint / static_cast<float>(span);
                        for (int i = 0; i < span && (p.col + i) < col_count; ++i) {
                            final_width[static_cast<size_t>(p.col + i)] = std::max(final_width[static_cast<size_t>(p.col + i)], each);
                            min_width[static_cast<size_t>(p.col + i)] = std::max(min_width[static_cast<size_t>(p.col + i)], each);
                            max_width[static_cast<size_t>(p.col + i)] = std::max(max_width[static_cast<size_t>(p.col + i)], each);
                            locked[static_cast<size_t>(p.col + i)] = 1;
                        }
                    }
                }
            }

            for (const auto& placement : placements) {
                if (placement.cell == nullptr) continue;
                if (!fixed_layout || placement.row == 0) {
                    int c = placement.col;
                    if (c < 0 || c >= col_count) continue;
                    int span = std::max(1, placement.colspan);
                    float hint = width_hint(*placement.cell);
                    if (span <= 1) {
                        min_width[static_cast<size_t>(c)] = std::max(min_width[static_cast<size_t>(c)], hint);
                        max_width[static_cast<size_t>(c)] = std::max(max_width[static_cast<size_t>(c)], hint);
                        if (!fixed_layout) {
                            final_width[static_cast<size_t>(c)] = std::max(final_width[static_cast<size_t>(c)], hint);
                        }
                    } else {
                        spread_span_width(min_width, locked, c, span, hint, h_spacing);
                        spread_span_width(max_width, locked, c, span, hint, h_spacing);
                        if (!fixed_layout) {
                            spread_span_width(final_width, locked, c, span, hint, h_spacing);
                        }
                    }
                }
            }

            for (int c = 0; c < col_count; ++c) {
                float fw = final_width[static_cast<size_t>(c)];
                if (fw <= 0.0f) {
                    fw = std::max(min_width[static_cast<size_t>(c)], max_width[static_cast<size_t>(c)]);
                }
                final_width[static_cast<size_t>(c)] = ensure_min_width(fw);
            }

            if (!fixed_layout) {
                // Only distribute remaining space if the table has an explicit width
                // (from HTML width attribute or CSS width property).
                // Tables with auto width should shrink-to-fit their content.
                bool has_explicit_width = table_node.specified_width >= 0.0f
                                       || table_node.css_width.has_value();
                float table_content_hint = 0.0f;
                if (has_explicit_width) {
                    table_content_hint = table_node.geometry.width > 0.0f ? table_node.geometry.width
                        : (table_node.specified_width >= 0.0f ? table_node.specified_width : 0.0f);
                    table_content_hint -= table_node.geometry.padding.left + table_node.geometry.padding.right
                                         + table_node.geometry.border.left + table_node.geometry.border.right;
                    if (table_content_hint < 0.0f) table_content_hint = 0.0f;
                }

                float occupied = 0.0f;
                for (int c = 0; c < col_count; ++c) {
                    occupied += final_width[static_cast<size_t>(c)];
                }
                float requested = std::max(0.0f, table_content_hint);
                float gutter = h_spacing * static_cast<float>(col_count + 1);
                float remaining = requested - (occupied + gutter);
                if (remaining > 0.0f) {
                    float grow_weight = 0.0f;
                    for (int c = 0; c < col_count; ++c) {
                        if (locked[static_cast<size_t>(c)] == 0) {
                            grow_weight += std::max(1.0f, final_width[static_cast<size_t>(c)]);
                        }
                    }
                    if (grow_weight <= 0.0f) grow_weight = static_cast<float>(col_count);
                    for (int c = 0; c < col_count; ++c) {
                        if (locked[static_cast<size_t>(c)] == 0) {
                            final_width[static_cast<size_t>(c)] += remaining
                                * (std::max(1.0f, final_width[static_cast<size_t>(c)]) / grow_weight);
                        }
                    }
                }

                // For auto-width tables, also update table geometry to shrink-to-fit
                if (!has_explicit_width) {
                    float sum_cols = 0.0f;
                    for (int c = 0; c < col_count; ++c) sum_cols += final_width[static_cast<size_t>(c)];
                    float shrink_width = sum_cols + gutter
                        + table_node.geometry.padding.left + table_node.geometry.padding.right
                        + table_node.geometry.border.left + table_node.geometry.border.right;
                    if (table_node.geometry.width > shrink_width) {
                        table_node.geometry.width = shrink_width;
                    }
                }
            }

            float content_area_width = 0.0f;
            for (int c = 0; c < col_count; ++c) content_area_width += final_width[static_cast<size_t>(c)];
            float content_area_total = content_area_width + h_spacing * static_cast<float>(col_count + 1);
            float caption_block_w = content_area_total
                                  + table_node.geometry.padding.left + table_node.geometry.padding.right
                                  + table_node.geometry.border.left + table_node.geometry.border.right;

            float cursor_top = 0.0f;
            for (auto* cap : top_captions) {
                if (!cap) continue;
                cap->geometry.x = 0.0f;
                if (caption_block_w > cap->geometry.width) {
                    cap->geometry.width = caption_block_w;
                }
                if (cap->geometry.height <= 0.0f) {
                    cap->geometry.height = estimate_node_height(estimate_node_height, *cap);
                }
                cap->geometry.y = cursor_top;
                cursor_top += cap->geometry.margin_box_height();
            }

            std::vector<float> row_top(static_cast<size_t>(row_count), 0.0f);
            std::vector<float> row_height(static_cast<size_t>(row_count), 0.0f);
            std::vector<float> row_baseline(static_cast<size_t>(row_count), 0.0f);
            std::vector<std::vector<CellPlacement*>> row_cells(row_count);
            std::vector<CellPlacement*> rowspan_cells;
            for (auto& p : placements) {
                if (p.cell == nullptr) continue;
                if (p.row >= 0 && p.row < row_count) {
                    row_cells[static_cast<size_t>(p.row)].push_back(&p);
                }
                if (p.rowspan > 1) {
                    rowspan_cells.push_back(&p);
                }
            }

            if (row_count > 0) {
                row_top[0] = cursor_top + v_spacing;
                float running_y = row_top[0];
                for (int ri = 0; ri < row_count; ++ri) {
                    auto* row = rows[static_cast<size_t>(ri)];
                    if (!row) continue;

                    if (row->visibility_collapse) {
                        row->geometry.height = 0.0f;
                        row->geometry.y = running_y;
                        row->geometry.width = content_area_width;
                        row->geometry.x = 0.0f;
                        row_top[static_cast<size_t>(ri)] = running_y;
                        row_height[static_cast<size_t>(ri)] = 0.0f;
                        row_baseline[static_cast<size_t>(ri)] = 0.0f;
                        running_y += v_spacing;
                        continue;
                    }

                    row->geometry.y = running_y;
                    row->geometry.x = 0.0f;
                    row->geometry.width = content_area_width;
                    row_top[static_cast<size_t>(ri)] = running_y;

                    float cursor_x = h_spacing;
                    int cursor_col = 0;
                    float height_in_row = 0.0f;
                    float baseline = 0.0f;
                    for (auto* placement : row_cells[static_cast<size_t>(ri)]) {
                        if (!placement || !placement->cell) continue;

                        int c = placement->col;
                        int span = std::max(1, placement->colspan);
                        while (cursor_col < c && cursor_col < col_count) {
                            cursor_x += final_width[static_cast<size_t>(cursor_col)] + h_spacing;
                            ++cursor_col;
                        }

                        float cell_w = 0.0f;
                        int end = std::min(col_count, c + span);
                        for (int cc = c; cc < end; ++cc) {
                            cell_w += final_width[static_cast<size_t>(cc)];
                        }
                        if (span > 1) {
                            cell_w += h_spacing * static_cast<float>(std::max(0, span - 1));
                        }

                        placement->cell->geometry.x = cursor_x;
                        placement->cell->geometry.width = cell_w;
                        float content_h = estimate_node_height(estimate_node_height, *placement->cell);
                        if (placement->cell->geometry.height <= 0.0f) {
                            placement->cell->geometry.height = content_h;
                        } else if (placement->cell->geometry.height < content_h) {
                            placement->cell->geometry.height = content_h;
                        }

                        float cell_h = placement->cell->geometry.margin_box_height();
                        if (placement->cell->rowspan <= 1) {
                            height_in_row = std::max(height_in_row, cell_h);
                            baseline = std::max(baseline, cell_h * 0.8f);
                        }

                        cursor_col = c + span;
                        cursor_x += cell_w;
                        if (cursor_col < col_count) cursor_x += h_spacing;
                    }

                    if (height_in_row < 0.0f) height_in_row = 0.0f;
                    // Use row's specified height (from style="height:...") as minimum
                    if (row->specified_height > height_in_row) {
                        height_in_row = row->specified_height;
                    }
                    row->geometry.height = height_in_row;
                    row_height[static_cast<size_t>(ri)] = height_in_row;
                    row_baseline[static_cast<size_t>(ri)] = baseline;

                    for (auto* placement : row_cells[static_cast<size_t>(ri)]) {
                        if (!placement || !placement->cell) continue;
                        if (placement->cell->rowspan > 1) {
                            placement->cell->geometry.y = 0.0f;
                            continue;
                        }
                        float cell_h = placement->cell->geometry.margin_box_height();
                        placement->cell->geometry.y = row_cell_offset(height_in_row, cell_h, placement->cell->vertical_align, baseline);
                    }

                    running_y = row_top[static_cast<size_t>(ri)] + row_height[static_cast<size_t>(ri)];
                    if (ri + 1 < row_count) running_y += v_spacing;
                }
            }

            for (auto* p : rowspan_cells) {
                if (!p || !p->cell) continue;
                if (p->row < 0 || p->row >= row_count) continue;
                int row_start = p->row;
                int row_end = std::min(row_count, p->row + std::max(1, p->rowspan));
                if (row_end <= row_start) continue;
                float span_h = 0.0f;
                for (int r = row_start; r < row_end; ++r) {
                    span_h += row_height[static_cast<size_t>(r)];
                    if (r + 1 < row_end) span_h += v_spacing;
                }
                float needed = p->cell->geometry.margin_box_height();
                if (needed > span_h) {
                    float extra = needed - span_h;
                    int last = row_end - 1;
                    if (last >= 0 && last < row_count) {
                        row_height[static_cast<size_t>(last)] += extra;
                        if (rows[static_cast<size_t>(last)]) {
                            rows[static_cast<size_t>(last)]->geometry.height = row_height[static_cast<size_t>(last)];
                        }
                    }
                    if (!row_top.empty()) {
                        recompute_row_tops(row_height, row_top, row_top[0]);
                    }
                }
                float final_span_h = 0.0f;
                for (int r = row_start; r < row_end; ++r) {
                    final_span_h += row_height[static_cast<size_t>(r)];
                    if (r + 1 < row_end) final_span_h += v_spacing;
                }
                if (p->cell->geometry.height < final_span_h) {
                    p->cell->geometry.height = final_span_h;
                }
            }

            for (int ri = 0; ri < row_count; ++ri) {
                if (ri < static_cast<int>(rows.size()) && rows[ri]) {
                    rows[static_cast<size_t>(ri)]->geometry.y = row_top[static_cast<size_t>(ri)];
                    rows[static_cast<size_t>(ri)]->geometry.height = row_height[static_cast<size_t>(ri)];
                }
            }

            if (row_count > 0) {
                for (auto& pvec : row_cells) {
                    if (pvec.empty()) continue;
                    int ri = pvec[0]->row;
                    float h = row_height[static_cast<size_t>(ri)];
                    if (h <= 0.0f) continue;
                    float baseline = row_baseline[static_cast<size_t>(ri)];
                    for (auto* p : pvec) {
                        if (!p || !p->cell || p->cell->rowspan > 1) continue;
                        float cell_h = p->cell->geometry.margin_box_height();
                        p->cell->geometry.y = row_cell_offset(h, cell_h, p->cell->vertical_align, baseline);
                    }
                }
            }

            float cursor_bottom = row_count > 0 ? row_top[0] : cursor_top;
            if (row_count > 0) {
                size_t last = static_cast<size_t>(row_count - 1);
                cursor_bottom = row_top[last] + row_height[last] + v_spacing;
            }

            for (auto* cap : bottom_captions) {
                if (!cap) continue;
                cap->geometry.x = 0.0f;
                if (caption_block_w > cap->geometry.width) {
                    cap->geometry.width = caption_block_w;
                }
                if (cap->geometry.height <= 0.0f) {
                    cap->geometry.height = estimate_node_height(estimate_node_height, *cap);
                }
                cap->geometry.y = cursor_bottom;
                cursor_bottom += cap->geometry.margin_box_height();
            }

            float final_height = cursor_bottom
                               + table_node.geometry.padding.top + table_node.geometry.padding.bottom
                               + table_node.geometry.border.top + table_node.geometry.border.bottom;
            if (table_node.geometry.height <= 0.0f || table_node.geometry.height < final_height) {
                table_node.geometry.height = final_height;
            }

            float final_width_value = content_area_total
                                   + table_node.geometry.padding.left + table_node.geometry.padding.right
                                   + table_node.geometry.border.left + table_node.geometry.border.right;
            if (table_node.geometry.width <= 0.0f || table_node.geometry.width < final_width_value) {
                table_node.geometry.width = final_width_value;
            }

            if (table_node.border_collapse) {
                for (int r = 0; r < row_count; ++r) {
                    for (int c = 0; c + 1 < col_count; ++c) {
                        auto* left = cell_matrix[static_cast<size_t>(r)][static_cast<size_t>(c)];
                        auto* right = cell_matrix[static_cast<size_t>(r)][static_cast<size_t>(c + 1)];
                        if (!left || !right || left == right) continue;
                        float left_w = left->geometry.border.right;
                        float right_w = right->geometry.border.left;
                        uint32_t left_c = left->border_color_right != 0 ? left->border_color_right : left->border_color;
                        uint32_t right_c = right->border_color_left != 0 ? right->border_color_left : right->border_color;
                        int left_style = left->border_style_right;
                        int right_style = right->border_style_left;
                        bool take_left = take_collapsed_border(left_w, right_w, left_c, right_c);
                        float winner_w = take_left ? left_w : right_w;
                        uint32_t winner_c = take_left ? left_c : right_c;
                        int winner_style = take_left ? left_style : right_style;
                        left->geometry.border.right = winner_w;
                        left->border_color_right = winner_c;
                        left->border_style_right = winner_style;
                        right->geometry.border.left = winner_w;
                        right->border_color_left = winner_c;
                        right->border_style_left = winner_style;
                        if (left->border_color == 0) left->border_color = winner_c;
                        if (right->border_color == 0) right->border_color = winner_c;
                    }
                }

                for (int r = 0; r + 1 < row_count; ++r) {
                    for (int c = 0; c < col_count; ++c) {
                        auto* top = cell_matrix[static_cast<size_t>(r)][static_cast<size_t>(c)];
                        auto* bottom = cell_matrix[static_cast<size_t>(r + 1)][static_cast<size_t>(c)];
                        if (!top || !bottom || top == bottom) continue;
                        float top_w = top->geometry.border.bottom;
                        float bottom_w = bottom->geometry.border.top;
                        uint32_t top_c = top->border_color_bottom != 0 ? top->border_color_bottom : top->border_color;
                        uint32_t bottom_c = bottom->border_color_top != 0 ? bottom->border_color_top : bottom->border_color;
                        int top_style = top->border_style_bottom;
                        int bottom_style = bottom->border_style_top;
                        bool take_top = take_collapsed_border(top_w, bottom_w, top_c, bottom_c);
                        float winner_w = take_top ? top_w : bottom_w;
                        uint32_t winner_c = take_top ? top_c : bottom_c;
                        int winner_style = take_top ? top_style : bottom_style;
                        top->geometry.border.bottom = winner_w;
                        top->border_color_bottom = winner_c;
                        top->border_style_bottom = winner_style;
                        bottom->geometry.border.top = winner_w;
                        bottom->border_color_top = winner_c;
                        bottom->border_style_top = winner_style;
                        if (top->border_color == 0) top->border_color = winner_c;
                        if (bottom->border_color == 0) bottom->border_color = winner_c;
                    }
                }
            }
        };

        compute_table_layout(*layout_node);
    }

    // Post-cascade UA defaults — apply AFTER cascade transfer to avoid being overwritten
    // Only set defaults when the cascade didn't provide explicit values
    if (tag_lower == "dd" && layout_node->geometry.margin.left == 0) {
        layout_node->geometry.margin.left = 40;
    }
    if (tag_lower == "dt" && layout_node->font_weight < 700) {
        layout_node->font_weight = 700;
    }
    if (tag_lower == "dl") {
        if (layout_node->geometry.margin.top == 0) layout_node->geometry.margin.top = 16;
        if (layout_node->geometry.margin.bottom == 0) layout_node->geometry.margin.bottom = 16;
    }
    if (tag_lower == "hgroup") {
        layout_node->mode = clever::layout::LayoutMode::Block;
        layout_node->display = clever::layout::DisplayType::Block;
    }
    // List defaults are provided by the UA stylesheet. Avoid post-cascade
    // fallback here so explicit zero values (e.g. margin: 0; padding: 0;)
    // are not overwritten.
    // <h1>-<h6> default styles: bold + appropriate sizes
    if (tag_lower == "h1" || tag_lower == "h2" || tag_lower == "h3" ||
        tag_lower == "h4" || tag_lower == "h5" || tag_lower == "h6") {
        if (layout_node->font_weight < 700) layout_node->font_weight = 700;
        // Default font sizes if not overridden by CSS
        float default_size = 16.0f;
        if (tag_lower == "h1") default_size = 32.0f;
        else if (tag_lower == "h2") default_size = 24.0f;
        else if (tag_lower == "h3") default_size = 18.72f;
        else if (tag_lower == "h4") default_size = 16.0f;
        else if (tag_lower == "h5") default_size = 13.28f;
        else if (tag_lower == "h6") default_size = 10.72f;
        if (layout_node->font_size == 16.0f) layout_node->font_size = default_size;
    }

    return layout_node;
}

// ---- Media query evaluation ----

// Evaluate a single parenthesized media feature expression against viewport.
// Returns true if the feature condition is satisfied.
static bool evaluate_media_feature(const std::string& expr, int vw, int vh) {
    // Trim the expression
    std::string trimmed = expr;
    while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();

    // Parse "feature: value"
    auto colon = trimmed.find(':');
    if (colon == std::string::npos) {
        // Bare feature name without value (e.g., "(color)") — treat as true
        return true;
    }

    std::string feature = trimmed.substr(0, colon);
    std::string value = trimmed.substr(colon + 1);
    // Trim both
    while (!feature.empty() && feature.back() == ' ') feature.pop_back();
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());

    // Handle prefers-color-scheme: detect macOS system appearance
    if (feature == "prefers-color-scheme") {
#ifdef __APPLE__
        // Read macOS dark mode via CoreFoundation (works from C++)
        bool system_dark = false;
        CFStringRef key = CFSTR("AppleInterfaceStyle");
        CFPropertyListRef val = CFPreferencesCopyAppValue(key, kCFPreferencesCurrentApplication);
        if (val) {
            if (CFGetTypeID(val) == CFStringGetTypeID()) {
                CFStringRef str = (CFStringRef)val;
                char buf[32] = {};
                CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8);
                if (std::string(buf) == "Dark") system_dark = true;
            }
            CFRelease(val);
        }
        if (value == "dark") return system_dark;
        if (value == "light") return !system_dark;
        return false;
#else
        return (value == "light");
#endif
    }

    // Parse numeric value (remove "px"/"em" suffix)
    float num_val = 0;
    if (value.size() > 2 && value.substr(value.size() - 2) == "px") {
        num_val = std::strtof(value.c_str(), nullptr);
    } else if (value.size() > 2 && value.substr(value.size() - 2) == "em") {
        num_val = std::strtof(value.c_str(), nullptr) * 16.0f;
    } else {
        num_val = std::strtof(value.c_str(), nullptr);
    }

    if (feature == "min-width") {
        return static_cast<float>(vw) >= num_val;
    } else if (feature == "max-width") {
        return static_cast<float>(vw) <= num_val;
    } else if (feature == "min-height") {
        return static_cast<float>(vh) >= num_val;
    } else if (feature == "max-height") {
        return static_cast<float>(vh) <= num_val;
    }

    // Common media features with sensible defaults for desktop browser
    if (feature == "prefers-reduced-motion") return (value == "no-preference");
    if (feature == "prefers-contrast") return (value == "no-preference");
    if (feature == "hover") return (value == "hover");
    if (feature == "pointer") return (value == "fine");
    if (feature == "any-hover") return (value == "hover");
    if (feature == "any-pointer") return (value == "fine");
    if (feature == "update") return (value == "fast");
    if (feature == "color") return true; // screen is color
    if (feature == "color-gamut") return (value == "srgb");
    if (feature == "display-mode") return (value == "browser");
    if (feature == "orientation") {
        return (value == "landscape") ? (vw >= vh) : (vw < vh);
    }
    if (feature == "forced-colors") return (value == "none");
    if (feature == "prefers-transparency") return (value == "no-preference");
    if (feature == "inverted-colors") return (value == "none");
    if (feature == "scripting") return (value == "enabled");
    if (feature == "prefers-reduced-data") return (value == "no-preference");
    if (feature == "overflow-block") return (value == "scroll");
    if (feature == "overflow-inline") return (value == "scroll");
    if (feature == "dynamic-range") return (value == "standard");
    if (feature == "video-dynamic-range") return (value == "standard");

    // Unknown features default to not matching (strict)
    return false;
}

// Evaluate a CSS media query condition against viewport dimensions.
// Supports: "all", "screen", "(min-width: Npx)", "(max-width: Npx)",
// "(min-height: Npx)", "(max-height: Npx)", "(prefers-color-scheme: dark/light)",
// and combinations with "and", "not", "only".
bool evaluate_media_query(const std::string& condition, int vw, int vh) {
    std::string lower_cond = to_lower(condition);
    while (!lower_cond.empty() && std::isspace(static_cast<unsigned char>(lower_cond.front()))) {
        lower_cond.erase(lower_cond.begin());
    }
    while (!lower_cond.empty() && std::isspace(static_cast<unsigned char>(lower_cond.back()))) {
        lower_cond.pop_back();
    }
    if (lower_cond.empty() || lower_cond == "all" || lower_cond == "screen") return true;

    // Comma-separated media query list uses OR semantics.
    // Example: "@media (max-width: 600px), (min-width: 1000px)"
    {
        int depth = 0;
        size_t start = 0;
        bool has_comma = false;
        for (size_t i = 0; i < lower_cond.size(); ++i) {
            if (lower_cond[i] == '(') {
                ++depth;
            } else if (lower_cond[i] == ')') {
                if (depth > 0) --depth;
            } else if (lower_cond[i] == ',' && depth == 0) {
                has_comma = true;
                if (evaluate_media_query(lower_cond.substr(start, i - start), vw, vh)) {
                    return true;
                }
                start = i + 1;
            }
        }
        if (has_comma) {
            return evaluate_media_query(lower_cond.substr(start), vw, vh);
        }
    }

    // Reject "print" media type
    if (lower_cond == "print") {
        return false;
    }

    // Handle "not" prefix — negate the entire query result
    bool negate = false;
    std::string cond = lower_cond;
    if (cond.substr(0, 4) == "not ") {
        negate = true;
        cond = cond.substr(4);
    }

    // Strip "only" prefix — "only" is a no-op for modern browsers
    if (cond.substr(0, 5) == "only ") {
        cond = cond.substr(5);
    }

    // Check for unsupported media types
    // If the condition is just a media type with no features:
    if (cond == "print") {
        return negate; // "not print" = true, "print" = false
    }
    if (cond == "all" || cond == "screen") {
        return !negate; // "not screen" = false, "screen" = true
    }

    // Strip leading "screen and " or "all and "
    if (cond.size() > 11 && cond.substr(0, 11) == "screen and ") {
        cond = cond.substr(11);
    } else if (cond.size() > 8 && cond.substr(0, 8) == "all and ") {
        cond = cond.substr(8);
    }

    // Evaluate individual conditions connected by "and"
    // All parenthesized expressions must match (logical AND).
    size_t pos = 0;
    bool all_match = true;
    while (pos < cond.size()) {
        // Find next parenthesized expression
        auto lparen = cond.find('(', pos);
        if (lparen == std::string::npos) break;
        auto rparen = cond.find(')', lparen);
        if (rparen == std::string::npos) break;

        std::string expr = cond.substr(lparen + 1, rparen - lparen - 1);
        if (!evaluate_media_feature(expr, vw, vh)) {
            all_match = false;
            break;
        }

        pos = rparen + 1;
    }

    return negate ? !all_match : all_match;
}

// Convert parsed @keyframes rules into KeyframesDefinition structs
void collect_keyframes(const clever::css::StyleSheet& sheet,
                       std::vector<clever::css::KeyframesDefinition>& out) {
    for (auto& kr : sheet.keyframes) {
        clever::css::KeyframesDefinition def;
        def.name = kr.name;
        for (auto& kf : kr.keyframes) {
            clever::css::KeyframeStop stop;
            // Parse selector to offset
            std::string sel = kf.selector;
            if (sel == "from") {
                stop.offset = 0.0f;
            } else if (sel == "to") {
                stop.offset = 1.0f;
            } else {
                // Try to parse as percentage (e.g. "50%")
                auto pct_pos = sel.find('%');
                if (pct_pos != std::string::npos) {
                    stop.offset = std::strtof(sel.c_str(), nullptr) / 100.0f;
                } else {
                    stop.offset = std::strtof(sel.c_str(), nullptr) / 100.0f;
                }
            }
            // Convert declarations to string pairs
            for (auto& decl : kf.declarations) {
                std::string value_str;
                for (auto& cv : decl.values) {
                    if (!value_str.empty()) value_str += " ";
                    value_str += cv.value;
                }
                stop.declarations.emplace_back(decl.property, value_str);
            }
            def.rules.push_back(std::move(stop));
        }
        out.push_back(std::move(def));
    }
}

// Build KeyframeAnimation map from KeyframesDefinition vector
// This provides the new KeyframeStep/KeyframeAnimation struct format alongside
// the existing KeyframesDefinition/KeyframeStop format.
void build_keyframe_animation_map(
    const std::vector<clever::css::KeyframesDefinition>& defs,
    std::unordered_map<std::string, clever::css::KeyframeAnimation>& out) {
    for (const auto& def : defs) {
        clever::css::KeyframeAnimation anim;
        anim.name = def.name;
        for (const auto& stop : def.rules) {
            clever::css::KeyframeStep step;
            step.offset = stop.offset;
            for (const auto& [prop, val] : stop.declarations) {
                step.properties[prop] = val;
            }
            anim.steps.push_back(std::move(step));
        }
        // Sort steps by offset
        std::sort(anim.steps.begin(), anim.steps.end(),
                  [](const clever::css::KeyframeStep& a, const clever::css::KeyframeStep& b) {
                      return a.offset < b.offset;
                  });
        out[anim.name] = std::move(anim);
    }
}

// Flatten matching media query rules into a stylesheet's main rules list
void flatten_media_queries(clever::css::StyleSheet& sheet, int vw, int vh) {
    for (auto& mq : sheet.media_queries) {
        if (evaluate_media_query(mq.condition, vw, vh)) {
            for (auto& rule : mq.rules) {
                sheet.rules.push_back(rule);
            }
        }
    }
}

} // anonymous namespace

// Evaluate a simple @supports condition.
// Supports: "(property: value)", "not (...)", "(prop1: val1) and (prop2: val2)",
//           "(prop1: val1) or (prop2: val2)"
bool evaluate_supports_condition(const std::string& condition) {
    std::string cond = condition;
    // Trim whitespace
    while (!cond.empty() && (cond.front() == ' ' || cond.front() == '\t')) cond.erase(0, 1);
    while (!cond.empty() && (cond.back() == ' ' || cond.back() == '\t')) cond.pop_back();

    if (cond.empty()) return false;

    // Handle "not (...)"
    if (cond.size() > 4 && cond.substr(0, 4) == "not " ) {
        return !evaluate_supports_condition(cond.substr(4));
    }

    // Handle "(...) and (...)" — split on " and "
    {
        auto pos = cond.find(" and ");
        if (pos != std::string::npos) {
            return evaluate_supports_condition(cond.substr(0, pos)) &&
                   evaluate_supports_condition(cond.substr(pos + 5));
        }
    }

    // Handle "(...) or (...)" — split on " or "
    {
        auto pos = cond.find(" or ");
        if (pos != std::string::npos) {
            return evaluate_supports_condition(cond.substr(0, pos)) ||
                   evaluate_supports_condition(cond.substr(pos + 4));
        }
    }

    // Handle "(property: value)" — strip outer parens and check property support
    if (cond.front() == '(' && cond.back() == ')') {
        std::string inner = cond.substr(1, cond.size() - 2);
        // Trim
        while (!inner.empty() && inner.front() == ' ') inner.erase(0, 1);
        while (!inner.empty() && inner.back() == ' ') inner.pop_back();

        auto colon = inner.find(':');
        if (colon != std::string::npos) {
            std::string prop = inner.substr(0, colon);
            while (!prop.empty() && prop.back() == ' ') prop.pop_back();

            // We support a very wide range of CSS properties — return true for known ones
            // This is a simplified check; a full browser would test property+value validity
            static const std::unordered_set<std::string> supported_props = {
                // Box model
                "display", "position", "float", "clear", "box-sizing", "width", "height",
                "min-width", "max-width", "min-height", "max-height", "margin", "padding",
                "margin-top", "margin-right", "margin-bottom", "margin-left",
                "padding-top", "padding-right", "padding-bottom", "padding-left",
                "border", "border-top", "border-right", "border-bottom", "border-left",
                "border-width", "border-color", "border-style", "border-radius",
                "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
                "border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
                "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
                "border-top-left-radius", "border-top-right-radius", "border-bottom-left-radius", "border-bottom-right-radius",
                "border-collapse", "border-spacing", "outline", "outline-width", "outline-color", "outline-style", "outline-offset",
                // Color and background
                "color", "background", "background-color", "background-image", "background-position",
                "background-size", "background-repeat", "background-attachment", "background-clip",
                "background-origin", "background-position-x", "background-position-y",
                // Text and font
                "font-size", "font-weight", "font-family", "font-style", "font-variant",
                "font-stretch", "font", "text-align", "text-decoration", "text-decoration-line",
                "text-decoration-color", "text-decoration-style", "text-decoration-thickness",
                "text-transform", "text-indent", "text-shadow", "text-overflow", "text-rendering",
                "text-align-last", "text-justify", "text-underline-offset", "text-underline-position",
                "line-height", "letter-spacing", "word-spacing", "word-break", "word-wrap",
                "white-space", "vertical-align", "hanging-punctuation", "tab-size", "hyphens",
                // List
                "list-style", "list-style-type", "list-style-position", "list-style-image",
                // Box shadow and decorations
                "box-shadow", "box-decoration-break", "opacity",
                // Visibility and interaction
                "visibility", "overflow", "overflow-x", "overflow-y", "overflow-wrap",
                "z-index", "cursor", "user-select", "pointer-events", "resize",
                // Transform and effects
                "transform", "transform-origin", "transform-style", "perspective",
                "perspective-origin", "backface-visibility",
                "filter", "backdrop-filter", "mix-blend-mode", "clip-path",
                // Transition and animation
                "transition", "transition-property", "transition-duration", "transition-timing-function",
                "transition-delay",
                "animation", "animation-name", "animation-duration", "animation-timing-function",
                "animation-delay", "animation-direction", "animation-fill-mode", "animation-iteration-count",
                "animation-play-state",
                // Flexbox
                "flex", "flex-direction", "flex-wrap", "flex-flow", "flex-grow", "flex-shrink",
                "flex-basis", "justify-content", "justify-items", "justify-self", "align-content",
                "align-items", "align-self", "gap", "row-gap", "column-gap", "order",
                // Grid
                "grid", "grid-auto-flow", "grid-auto-columns", "grid-auto-rows", "grid-column",
                "grid-column-start", "grid-column-end", "grid-row", "grid-row-start", "grid-row-end",
                "grid-template", "grid-template-columns", "grid-template-rows", "grid-template-areas",
                "grid-area", "grid-gap",
                // Aspect and sizing
                "aspect-ratio", "object-fit", "object-position", "contain",
                // Mask and clipping
                "mask", "mask-image", "mask-size", "mask-position", "mask-repeat", "mask-clip",
                "mask-origin", "clip-path", "clip",
                // Writing and direction
                "writing-mode", "direction", "unicode-bidi", "text-orientation",
                // Columns
                "column-count", "column-width", "column-gap", "column-fill", "column-rule",
                "column-rule-width", "column-rule-style", "column-rule-color", "columns",
                "column-span", "break-before", "break-after", "break-inside",
                "page-break-before", "page-break-after", "page-break-inside", "orphans", "widows",
                // Scroll behavior
                "scroll-behavior", "scroll-snap-type", "scroll-snap-align", "scroll-snap-stop",
                "scroll-margin", "scroll-margin-top", "scroll-margin-right", "scroll-margin-bottom", "scroll-margin-left",
                "scroll-padding", "scroll-padding-top", "scroll-padding-right", "scroll-padding-bottom", "scroll-padding-left",
                // Colors and appearance
                "accent-color", "caret-color", "color-scheme", "appearance", "-webkit-appearance",
                // Container and visibility
                "content-visibility", "container-type", "container-name", "will-change",
                "isolation", "mix-blend-mode",
                // SVG properties
                "fill", "fill-opacity", "fill-rule", "stroke", "stroke-width", "stroke-linecap",
                "stroke-linejoin", "stroke-dasharray", "stroke-dashoffset", "stroke-miterlimit", "stroke-opacity",
                "stop-color", "stop-opacity", "flood-color", "flood-opacity", "lighting-color",
                "marker-start", "marker-mid", "marker-end",
                // Webkit-specific
                "-webkit-user-select", "-webkit-text-fill-color", "-webkit-text-stroke", "-webkit-text-stroke-width",
                "-webkit-text-stroke-color", "-webkit-background-clip", "-webkit-background-size", "-webkit-box-shadow",
                "-webkit-box-sizing", "-webkit-mask", "-webkit-mask-image", "-webkit-mask-position", "-webkit-mask-size",
                "-webkit-mask-repeat", "-webkit-mask-clip", "-webkit-mask-origin", "-webkit-mask-composite",
                "-webkit-filter", "-webkit-backdrop-filter", "-webkit-font-smoothing", "-webkit-text-size-adjust",
                "-webkit-line-clamp", "-webkit-box-orient", "-webkit-box-direction", "-webkit-box-pack", "-webkit-box-align",
                "-webkit-box-flex", "-webkit-min-device-pixel-ratio", "-webkit-max-device-pixel-ratio",
                "-webkit-optimize-contrast", "-webkit-print-color-adjust", "-webkit-transform", "-webkit-transform-origin",
                "-webkit-transform-style", "-webkit-transition", "-webkit-transition-property", "-webkit-transition-duration",
                "-webkit-transition-timing-function", "-webkit-transition-delay", "-webkit-animation", "-webkit-animation-name",
                "-webkit-animation-duration", "-webkit-animation-timing-function", "-webkit-animation-delay",
                "-webkit-animation-direction", "-webkit-animation-fill-mode", "-webkit-animation-iteration-count",
                "-webkit-animation-play-state", "-webkit-flex", "-webkit-flex-direction", "-webkit-flex-wrap",
                "-webkit-flex-flow", "-webkit-flex-grow", "-webkit-flex-shrink", "-webkit-flex-basis",
                "-webkit-justify-content", "-webkit-align-items", "-webkit-align-content", "-webkit-align-self",
                "-webkit-user-drag", "-webkit-user-modify", "-webkit-column-count", "-webkit-column-fill",
                "-webkit-column-gap", "-webkit-column-rule", "-webkit-column-rule-color", "-webkit-column-rule-style",
                "-webkit-column-rule-width", "-webkit-column-span", "-webkit-column-width", "-webkit-columns",
                "-webkit-margin-before", "-webkit-margin-after", "-webkit-margin-start", "-webkit-margin-end",
                "-webkit-padding-before", "-webkit-padding-after", "-webkit-padding-start", "-webkit-padding-end",
                "-webkit-border-start", "-webkit-border-end", "-webkit-border-start-width", "-webkit-border-start-style",
                "-webkit-border-start-color", "-webkit-border-end-width", "-webkit-border-end-style", "-webkit-border-end-color",
                "-webkit-border-before", "-webkit-border-after", "-webkit-border-before-width", "-webkit-border-before-style",
                "-webkit-border-before-color", "-webkit-border-after-width", "-webkit-border-after-style",
                "-webkit-border-after-color", "-webkit-writing-mode",
                // Moz-specific
                "-moz-appearance", "-moz-user-select", "-moz-box-sizing", "-moz-border-radius",
                "-moz-box-shadow", "-moz-column-count", "-moz-column-fill", "-moz-column-gap", "-moz-column-rule",
                "-moz-column-rule-color", "-moz-column-rule-style", "-moz-column-rule-width", "-moz-column-span",
                "-moz-column-width", "-moz-columns", "-moz-transform", "-moz-transform-origin", "-moz-transform-style",
                "-moz-transition", "-moz-transition-property", "-moz-transition-duration", "-moz-transition-timing-function",
                "-moz-transition-delay", "-moz-animation", "-moz-animation-name", "-moz-animation-duration",
                "-moz-animation-timing-function", "-moz-animation-delay", "-moz-animation-direction",
                "-moz-animation-fill-mode", "-moz-animation-iteration-count", "-moz-animation-play-state",
                "-moz-osx-font-smoothing", "-moz-user-focus", "-moz-tab-size", "-moz-font-feature-settings",
                "-moz-hyphens", "-moz-text-size-adjust",
                // Ms-specific
                "-ms-flex", "-ms-flex-direction", "-ms-flex-wrap", "-ms-flex-flow", "-ms-flex-grow", "-ms-flex-shrink",
                "-ms-flex-basis", "-ms-flex-align", "-ms-flex-pack", "-ms-flex-order", "-ms-filter",
                "-ms-transform", "-ms-transform-origin", "-ms-transform-style", "-ms-transition",
                "-ms-transition-property", "-ms-transition-duration", "-ms-transition-timing-function",
                "-ms-transition-delay", "-ms-animation", "-ms-animation-name", "-ms-animation-duration",
                "-ms-animation-timing-function", "-ms-animation-delay", "-ms-animation-direction",
                "-ms-animation-fill-mode", "-ms-animation-iteration-count", "-ms-animation-play-state",
                "-ms-appearance", "-ms-background-position-x", "-ms-background-position-y", "-ms-behavior",
                "-ms-box-sizing", "-ms-box-shadow", "-ms-font-feature-settings", "-ms-line-break",
                "-ms-overflow-style", "-ms-scroll-snap-type", "-ms-text-align-last", "-ms-text-transform",
                "-ms-touch-action", "-ms-user-select", "-ms-word-break", "-ms-word-wrap", "-ms-writing-mode",
            };
            return supported_props.count(prop) > 0;
        }
    }

    return false;
}

void flatten_supports_rules(clever::css::StyleSheet& sheet) {
    for (auto& sr : sheet.supports_rules) {
        if (evaluate_supports_condition(sr.condition)) {
            for (auto& rule : sr.rules) {
                sheet.rules.push_back(rule);
            }
        }
    }
}

namespace {

// Thread pool for parallel resource fetching (CSS, scripts, images)
static thread_local clever::platform::ThreadPool* g_resource_thread_pool = nullptr;

clever::platform::ThreadPool& get_resource_thread_pool() {
    if (!g_resource_thread_pool) {
        static thread_local clever::platform::ThreadPool pool(std::thread::hardware_concurrency());
        g_resource_thread_pool = &pool;
    }
    return *g_resource_thread_pool;
}

void flatten_layer_rules(clever::css::StyleSheet& sheet) {
    // Simplified @layer: promote all layer rules into the main rule set.
    // Full layer ordering/priority is not yet implemented — rules are
    // appended after existing rules (lower cascade priority than un-layered).
    for (auto& lr : sheet.layer_rules) {
        for (auto& rule : lr.rules) {
            sheet.rules.push_back(rule);
        }
    }
}

// Evaluate a container query condition against a container's computed dimensions.
// Supports:
//   - (min-width: Npx), (max-width: Npx), (width: Npx)
//   - (min-height: Npx), (max-height: Npx), (height: Npx)
//   - (width > Npx), (width < Npx), (width >= Npx), (width <= Npx)
//   - (aspect-ratio: 1), (aspect-ratio: 16/9)
//   - (aspect-ratio > 0.5)
//   - (orientation: portrait), (orientation: landscape)
//   - Combined queries: "(min-width: 800px) and (height > 200px)" and "(width: 100cqw) or (height: 50cqh)"
static bool is_container_cond_word_char(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '-';
}

static bool is_wrapped_by_matching_parens(const std::string& s) {
    if (s.size() < 2 || s.front() != '(' || s.back() != ')') return false;

    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') {
            ++depth;
        } else if (s[i] == ')') {
            if (depth == 0) return false;
            --depth;
            if (depth == 0 && i != s.size() - 1) return false;
        }
    }
    return depth == 0;
}

static std::string trim_container_parens(std::string c) {
    c = trim(c);
    while (is_wrapped_by_matching_parens(c)) {
        c = trim(c.substr(1, c.size() - 2));
    }
    return c;
}

static std::vector<std::string> split_container_condition_parts(
    const std::string& cond, const std::string& keyword) {
    std::vector<std::string> parts;
    std::string c = cond;
    std::string lower_cond = to_lower(c);
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < lower_cond.size();) {
        if (lower_cond[i] == '(') {
            ++depth;
            ++i;
            continue;
        }
        if (lower_cond[i] == ')') {
            if (depth > 0) --depth;
            ++i;
            continue;
        }

        if (depth == 0 &&
            i + keyword.size() <= lower_cond.size() &&
            lower_cond.compare(i, keyword.size(), keyword) == 0) {
            bool left_ok = (i == 0) || !is_container_cond_word_char(lower_cond[i - 1]);
            bool right_ok = (i + keyword.size() >= lower_cond.size()) ||
                            !is_container_cond_word_char(lower_cond[i + keyword.size()]);
            if (left_ok && right_ok) {
                parts.push_back(trim(c.substr(start, i - start)));
                i += keyword.size();
                start = i;
                continue;
            }
        }

        ++i;
    }

    std::string tail = trim(c.substr(start));
    if (!tail.empty() || parts.empty()) {
        parts.push_back(tail);
    }

    return parts;
}

static float parse_container_length_value(const std::string& value,
                                        bool use_container_width,
                                        float container_w,
                                        float container_h) {
    float px = 0;
    auto l = clever::css::parse_length(value);
    if (!l) return px;

    if (use_container_width) return l->to_px(container_w, 16.0f, 0);
    return l->to_px(container_h, 16.0f, 0);
}

static bool evaluate_single_container_condition(const std::string& c,
                                              float container_w,
                                              float container_h) {
    auto parse_aspect_ratio = [](const std::string& value, float& ratio) -> bool {
        std::string raw = trim(value);
        if (raw.empty()) return false;

        size_t slash = raw.find('/');
        auto parse_number = [](const std::string& text, float& out) -> bool {
            std::string trimmed_text = trim(text);
            if (trimmed_text.empty()) return false;

            char* end = nullptr;
            float parsed = std::strtof(trimmed_text.c_str(), &end);
            if (end == trimmed_text.c_str()) return false;
            if (!trim(std::string(end)).empty()) return false;
            out = parsed;
            return true;
        };

        if (slash == std::string::npos) {
            return parse_number(raw, ratio);
        }

        std::string numerator = raw.substr(0, slash);
        std::string denominator = raw.substr(slash + 1);
        float num = 0.0f;
        float den = 0.0f;
        if (!parse_number(numerator, num) || !parse_number(denominator, den) || den == 0.0f) {
            return false;
        }

        ratio = num / den;
        return true;
    };

    // Handle comparison operators: "width > 300px", "width >= 300px", etc.
    struct OpEntry { const char* str; size_t len; };
    OpEntry ops[] = {{">=", 2}, {"<=", 2}, {">", 1}, {"<", 1}};
    for (auto& op : ops) {
        auto op_pos = c.find(op.str);
        if (op_pos != std::string::npos) {
            std::string prop = trim(c.substr(0, op_pos));
            std::string val = trim(c.substr(op_pos + op.len));

            std::string prop_lower = to_lower(prop);
            if (prop_lower == "aspect-ratio") {
                float ratio_value = 0.0f;
                if (!parse_aspect_ratio(val, ratio_value)) return false;
                if (container_h <= 0.0f) return false;

                float actual_ratio = container_w / container_h;

                if (std::string(op.str) == ">=") return actual_ratio >= ratio_value;
                if (std::string(op.str) == "<=") return actual_ratio <= ratio_value;
                if (std::string(op.str) == ">") return actual_ratio > ratio_value;
                if (std::string(op.str) == "<") return actual_ratio < ratio_value;
                return false;
            }

            bool uses_width = (prop_lower.find("width") != std::string::npos &&
                              prop_lower.find("height") == std::string::npos);
            float px_val = parse_container_length_value(
                val, uses_width, container_w, container_h);
            float dim = uses_width ? container_w : container_h;

            if (std::string(op.str) == ">=") return dim >= px_val;
            if (std::string(op.str) == "<=") return dim <= px_val;
            if (std::string(op.str) == ">") return dim > px_val;
            if (std::string(op.str) == "<") return dim < px_val;
        }
    }

    // Handle "min-width: Npx", "max-width: Npx", etc.
    auto colon = c.find(':');
    if (colon != std::string::npos) {
        std::string prop = trim(c.substr(0, colon));
        std::string val = trim(c.substr(colon + 1));

        std::string prop_lower = to_lower(prop);
        bool orientation_prop = (prop_lower == "orientation");
        bool width_prop = (prop_lower == "min-width" ||
                           prop_lower == "max-width" ||
                           prop_lower == "width");
        bool height_prop = (prop_lower == "min-height" ||
                            prop_lower == "max-height" ||
                            prop_lower == "height");
        bool aspect_ratio_prop = (prop_lower == "aspect-ratio");
        if (!width_prop && !height_prop && !aspect_ratio_prop && !orientation_prop) return false;

        if (orientation_prop) {
            if (to_lower(val) == "portrait") return container_h >= container_w;
            if (to_lower(val) == "landscape") return container_w > container_h;
            return false;
        }

        float px_val = parse_container_length_value(
            val, width_prop, container_w, container_h);

        if (aspect_ratio_prop) {
            float ratio_value = 0.0f;
            if (!parse_aspect_ratio(val, ratio_value)) return false;
            if (container_h <= 0.0f) return false;
            float actual_ratio = container_w / container_h;
            return std::fabs(actual_ratio - ratio_value) < 0.0001f;
        }

        if (prop_lower == "min-width") return container_w >= px_val;
        if (prop_lower == "max-width") return container_w <= px_val;
        if (prop_lower == "width") return container_w == px_val;
        if (prop_lower == "min-height") return container_h >= px_val;
        if (prop_lower == "max-height") return container_h <= px_val;
        if (prop_lower == "height") return container_h == px_val;
    }

    return true;
}

static bool evaluate_container_condition(const std::string& cond,
                                          float container_w,
                                          float container_h = 0) {
    clever::layout::LayoutNode::s_container_width = container_w;
    clever::layout::LayoutNode::s_container_height = container_h;

    std::string c = trim_container_parens(cond);
    if (c.empty()) return true;

    auto or_parts = split_container_condition_parts(c, "or");
    if (or_parts.size() > 1) {
        for (const auto& part : or_parts) {
            if (evaluate_container_condition(part, container_w, container_h)) return true;
        }
        return false;
    }

    auto and_parts = split_container_condition_parts(c, "and");
    if (and_parts.size() > 1) {
        for (const auto& part : and_parts) {
            if (!evaluate_container_condition(part, container_w, container_h)) return false;
        }
        return true;
    }

    return evaluate_single_container_condition(c, container_w, container_h);
}

// Legacy flatten: used during @import processing where we don't yet have a layout tree.
// Just promotes container rules without evaluation (they'll be evaluated post-layout).
void flatten_container_rules(clever::css::StyleSheet& /*sheet*/, int /*viewport_w*/) {
    // No-op: container rules are evaluated post-layout against actual container sizes.
    // Container rules remain in sheet.container_rules for post-layout evaluation.
}

// ---- Post-layout container query evaluation ----

// Build a parent pointer map for the layout tree
static void build_parent_map(
    clever::layout::LayoutNode& node,
    std::unordered_map<clever::layout::LayoutNode*, clever::layout::LayoutNode*>& parent_map) {
    for (auto& child : node.children) {
        parent_map[child.get()] = &node;
        build_parent_map(*child, parent_map);
    }
}

// Find the nearest container ancestor using the parent map
static clever::layout::LayoutNode* find_container_ancestor_via_map(
    clever::layout::LayoutNode* node,
    const std::string& container_name,
    const std::unordered_map<clever::layout::LayoutNode*, clever::layout::LayoutNode*>& parent_map) {

    auto it = parent_map.find(node);
    while (it != parent_map.end()) {
        clever::layout::LayoutNode* ancestor = it->second;
        if (ancestor->container_type != 0) {
            if (container_name.empty() || ancestor->container_name == container_name) {
                return ancestor;
            }
        }
        it = parent_map.find(ancestor);
    }
    return nullptr;
}

// Apply container query declarations to a LayoutNode's style properties.
// This transfers relevant CSS properties from ComputedStyle to LayoutNode fields.
static void apply_style_to_layout_node(
    clever::layout::LayoutNode& node,
    const clever::css::ComputedStyle& style) {
    // Background color
    node.background_color = (static_cast<uint32_t>(style.background_color.a) << 24) |
                            (static_cast<uint32_t>(style.background_color.r) << 16) |
                            (static_cast<uint32_t>(style.background_color.g) << 8) |
                            (static_cast<uint32_t>(style.background_color.b));

    // Text color
    node.color = (static_cast<uint32_t>(style.color.a) << 24) |
                 (static_cast<uint32_t>(style.color.r) << 16) |
                 (static_cast<uint32_t>(style.color.g) << 8) |
                 (static_cast<uint32_t>(style.color.b));

    // Font size
    node.font_size = style.font_size.to_px(16.0f);

    // CSS Grid properties (container)
    if (!style.grid_template_columns.empty())
        node.grid_template_columns = style.grid_template_columns;
    if (!style.grid_template_rows.empty())
        node.grid_template_rows = style.grid_template_rows;
    node.grid_template_columns_is_subgrid = style.grid_template_columns_is_subgrid;
    node.grid_template_rows_is_subgrid = style.grid_template_rows_is_subgrid;
    if (!style.grid_template_areas.empty())
        node.grid_template_areas = style.grid_template_areas;
    if (!style.grid_auto_rows.empty())
        node.grid_auto_rows = style.grid_auto_rows;
    if (!style.grid_auto_columns.empty())
        node.grid_auto_columns = style.grid_auto_columns;
    node.grid_auto_flow  = style.grid_auto_flow;
    node.justify_items   = style.justify_items;
    node.align_content   = style.align_content;
    // CSS Grid properties (item placement)
    if (!style.grid_column.empty())
        node.grid_column = style.grid_column;
    if (!style.grid_row.empty())
        node.grid_row = style.grid_row;
    if (!style.grid_column_start.empty())
        node.grid_column_start = style.grid_column_start;
    if (!style.grid_column_end.empty())
        node.grid_column_end = style.grid_column_end;
    if (!style.grid_row_start.empty())
        node.grid_row_start = style.grid_row_start;
    if (!style.grid_row_end.empty())
        node.grid_row_end = style.grid_row_end;
    if (!style.grid_area.empty())
        node.grid_area = style.grid_area;
    // justify_self / align_self are also used by grid item placement
    node.justify_self    = style.justify_self;
    if (style.align_self != -1)
        node.align_self  = style.align_self;

    // Display mode
    node.mode = display_to_mode(style.display);
    node.display = display_to_type(style.display);
    node.direction = static_cast<int>(style.direction);
    node.writing_mode = style.writing_mode;
    node.container_type = style.container_type;
    node.container_name = style.container_name;
    node.color_scheme = style.color_scheme;
}

// Post-layout container query evaluation.
// Walks the layout tree, matches @container rule selectors against elements,
// finds container ancestors, evaluates conditions against actual container sizes,
// and applies matching declarations.
// Returns true if any rules were applied (meaning re-layout is needed).
static bool evaluate_container_queries_post_layout(
    clever::layout::LayoutNode& root,
    const std::vector<clever::css::ContainerRule>& all_container_rules) {

    if (all_container_rules.empty()) return false;

    // Build parent map for upward traversal
    std::unordered_map<clever::layout::LayoutNode*, clever::layout::LayoutNode*> parent_map;
    build_parent_map(root, parent_map);

    clever::css::PropertyCascade cascade;
    clever::css::SelectorMatcher matcher;
    bool any_applied = false;

    // Walk the entire layout tree
    std::function<void(clever::layout::LayoutNode&)> walk =
        [&](clever::layout::LayoutNode& node) {
            float saved_container_width = clever::layout::LayoutNode::s_container_width;
            float saved_container_height = clever::layout::LayoutNode::s_container_height;
            bool is_container = (node.container_type != 0);
            if (is_container) {
                clever::layout::LayoutNode::s_container_width = node.geometry.width;
                clever::layout::LayoutNode::s_container_height = node.geometry.height;
            }

            if (node.is_text) {
                for (auto& child : node.children) walk(*child);
                clever::layout::LayoutNode::s_container_width = saved_container_width;
                clever::layout::LayoutNode::s_container_height = saved_container_height;
                return;
            }

            // Build a minimal ElementView for selector matching
            clever::css::ElementView ev;
            ev.tag_name = node.tag_name;
            for (auto& ch : ev.tag_name)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            ev.id = node.element_id;
            ev.classes = node.css_classes;

            for (auto& cr : all_container_rules) {
                for (auto& style_rule : cr.rules) {
                    // Check if this node matches the rule's selectors
                    bool matches = false;
                    for (auto& sel : style_rule.selectors.selectors) {
                        if (matcher.matches(ev, sel)) {
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) continue;

                    // Find the nearest container ancestor
                    auto* container = find_container_ancestor_via_map(
                        &node, cr.name, parent_map);
                    if (!container) continue;

                    // Evaluate the condition
                    float cw = container->geometry.width;
                    float ch_val = container->geometry.height;

                    // For inline-size containers, only width is queryable
                    if (container->container_type == 2) {
                        ch_val = 0;
                    }
                    // For block-size containers, only height is queryable
                    if (container->container_type == 3) {
                        cw = 0;
                    }

                    clever::layout::LayoutNode::s_container_width = cw;
                    clever::layout::LayoutNode::s_container_height = ch_val;

                    if (evaluate_container_condition(cr.condition, cw, ch_val)) {
                        // Apply each declaration directly to the layout node
                        // We parse the declarations and apply the relevant properties
                        clever::css::ComputedStyle temp_style;
                        temp_style.background_color = clever::css::Color::transparent();
                        for (auto& decl : style_rule.declarations) {
                            cascade.apply_declaration(temp_style, decl, temp_style);
                        }
                        apply_style_to_layout_node(node, temp_style);
                        any_applied = true;
                    }
                }
            }

            for (auto& child : node.children) walk(*child);

            if (is_container) {
                clever::layout::LayoutNode::s_container_width = saved_container_width;
                clever::layout::LayoutNode::s_container_height = saved_container_height;
            }
        };

    walk(root);
    return any_applied;
}

static void apply_ruby_layout_pass(clever::layout::LayoutNode& root) {
    std::function<void(clever::layout::LayoutNode&)> walk =
        [&](clever::layout::LayoutNode& node) {
            for (auto& child : node.children) {
                if (child) walk(*child);
            }

            if (!node.is_ruby) return;

            struct RubyPair {
                clever::layout::LayoutNode* rt = nullptr;
                clever::layout::LayoutNode* base = nullptr;
                size_t base_index = 0;
            };

            std::vector<RubyPair> rt_pairs;
            std::vector<clever::layout::LayoutNode*> base_children;

            for (auto& child_ptr : node.children) {
                auto* child = child_ptr.get();
                if (!child || child->is_ruby_paren) continue;
                if (child->display == clever::layout::DisplayType::None ||
                    child->mode == clever::layout::LayoutMode::None) {
                    continue;
                }
                if (child->is_ruby_text) {
                    if (!base_children.empty()) {
                        rt_pairs.push_back({child, base_children.back(), base_children.size() - 1});
                    }
                    continue;
                }
                base_children.push_back(child);
            }

            for (auto& pair : rt_pairs) {
                auto* rt_node = pair.rt;
                auto* base_node = pair.base;
                if (!rt_node || !base_node) continue;

                const float base_left = base_node->geometry.x;
                const float base_right = base_node->geometry.x + base_node->geometry.width;
                const float base_width = base_right - base_left;
                const float rt_width = rt_node->geometry.width;
                float annotation_x = base_left;

                if (node.ruby_align == 1) {
                    annotation_x = base_left;
                } else if (node.ruby_align == 2 || node.ruby_align == 0) {
                    annotation_x = base_left + (base_width - rt_width) * 0.5f;
                } else if (node.ruby_align == 3) {
                    if (base_children.size() > 1 && node.geometry.width > 0.0f) {
                        const float slot_width =
                            node.geometry.width / static_cast<float>(base_children.size());
                        annotation_x = node.geometry.x + slot_width *
                                               (static_cast<float>(pair.base_index) + 0.5f) -
                                       (rt_width * 0.5f);
                    } else {
                        annotation_x = base_left + (base_width - rt_width) * 0.5f;
                    }
                }

                rt_node->geometry.x = annotation_x;
                rt_node->geometry.y = base_node->geometry.y - (rt_node->font_size + 1.5f);
            }

            float min_child_y = 0.0f;
            float max_child_y = 0.0f;
            bool has_child = false;
            for (auto& child_ptr : node.children) {
                auto* child = child_ptr.get();
                if (!child || child->display == clever::layout::DisplayType::None ||
                    child->mode == clever::layout::LayoutMode::None || child->is_ruby_paren) {
                    continue;
                }
                const float child_top = child->geometry.y;
                const float child_bottom = child->geometry.y + child->geometry.margin_box_height();
                if (!has_child) {
                    min_child_y = child_top;
                    max_child_y = child_bottom;
                    has_child = true;
                } else {
                    min_child_y = std::min(min_child_y, child_top);
                    max_child_y = std::max(max_child_y, child_bottom);
                }
            }

            if (has_child) {
                const float needed_height = max_child_y - min_child_y;
                if (needed_height > node.geometry.height) {
                    node.geometry.height = needed_height;
                }
            }
        };

    walk(root);
}

void apply_property_rules(clever::css::StyleSheet& sheet,
                          std::unordered_map<std::string, clever::css::PropertyRule>& registry) {
    for (auto& pr : sheet.property_rules) {
        registry[pr.name] = pr;
    }
}

void flatten_scope_rules(clever::css::StyleSheet& sheet) {
    // @scope (scope-start) [to (scope-end)] { rules }
    // Flatten by prepending scope-start as a descendant combinator to each rule selector.
    // scope-end is a lower boundary — elements matching scope-end are excluded.
    // Simplified: we prepend scope-start to selectors. scope-end is noted but not enforced
    // at the cascade level (would need runtime DOM walking).
    for (auto& sr : sheet.scope_rules) {
        for (auto& rule : sr.rules) {
            if (!sr.scope_start.empty()) {
                // Prepend scope-start as ancestor: ".card .inner" → ".card .inner"
                // Rule selector "p" → ".card p" (descendant)
                std::string new_sel = sr.scope_start + " " + rule.selector_text;
                rule.selector_text = new_sel;
                rule.selectors = clever::css::parse_selector_list(new_sel);
            }
            sheet.rules.push_back(rule);
        }
    }
}

// Internal implementation: Recursively fetch and merge @import rules into a stylesheet.
// Imported rules are inserted before the sheet's own rules (for proper cascade order).
//
// Parameters:
//   sheet          — stylesheet to process (modified in place)
//   base_url       — base URL for resolving relative @import URLs
//   viewport_width / viewport_height — used to evaluate media queries on imports
//   depth          — current recursion depth (starts at 0)
//   visited        — set of already-imported absolute URLs (prevents circular imports)
//   fetch_cache    — per-render CSS fetch cache (avoids re-fetching the same URL)
static void process_css_imports_impl(clever::css::StyleSheet& sheet,
                                     const std::string& base_url,
                                     int viewport_width, int viewport_height,
                                     int depth,
                                     std::unordered_set<std::string>& visited,
                                     std::unordered_map<std::string, std::string>& fetch_cache) {
    // Hard limit on recursion depth (spec says browsers may limit; 10 is generous)
    static constexpr int kMaxImportDepth = 10;
    if (depth >= kMaxImportDepth || sheet.imports.empty()) return;

    // Collect rules from all @import sheets (in order)
    std::vector<clever::css::StyleRule> imported_rules;
    for (auto& imp : sheet.imports) {
        if (imp.url.empty()) continue;

        // Evaluate media condition before fetching
        if (!imp.media.empty() &&
            !evaluate_media_query(imp.media, viewport_width, viewport_height)) {
            continue;
        }

        std::string resolved = resolve_url(imp.url, base_url);
        if (resolved.empty()) continue;

        // Circular import guard: skip if we have already started processing this URL
        if (visited.count(resolved)) continue;
        visited.insert(resolved);

        // CSS fetch cache: avoid re-fetching the same URL in one render pass
        std::string fetched_url = resolved;
        std::string css;
        auto cache_it = fetch_cache.find(resolved);
        if (cache_it != fetch_cache.end()) {
            css = cache_it->second;
        } else {
            css = fetch_css(resolved, &fetched_url);
            // Store under both the pre-redirect and post-redirect URLs
            fetch_cache[resolved] = css;
            if (fetched_url != resolved) {
                fetch_cache[fetched_url] = css;
                // Also mark the final URL as visited so a redirect target
                // cannot be imported again under a different alias
                visited.insert(fetched_url);
            }
        }

        if (css.empty()) continue;

        auto imported_sheet = clever::css::parse_stylesheet(css);
        // Recurse into the imported sheet's own @imports
        process_css_imports_impl(imported_sheet, fetched_url,
                                 viewport_width, viewport_height,
                                 depth + 1, visited, fetch_cache);
        // Flatten media/supports queries in the imported sheet
        flatten_media_queries(imported_sheet, viewport_width, viewport_height);
        flatten_supports_rules(imported_sheet);
        flatten_layer_rules(imported_sheet);
        flatten_container_rules(imported_sheet, viewport_width);
        flatten_scope_rules(imported_sheet);
        // Collect imported rules
        imported_rules.insert(imported_rules.end(),
                              imported_sheet.rules.begin(),
                              imported_sheet.rules.end());
        // Merge @font-face, @keyframes, @container, @property, @counter-style, etc.
        sheet.font_faces.insert(sheet.font_faces.end(),
                                imported_sheet.font_faces.begin(),
                                imported_sheet.font_faces.end());
        sheet.keyframes.insert(sheet.keyframes.end(),
                               imported_sheet.keyframes.begin(),
                               imported_sheet.keyframes.end());
        sheet.container_rules.insert(sheet.container_rules.end(),
                                     imported_sheet.container_rules.begin(),
                                     imported_sheet.container_rules.end());
        sheet.property_rules.insert(sheet.property_rules.end(),
                                    imported_sheet.property_rules.begin(),
                                    imported_sheet.property_rules.end());
        sheet.counter_style_rules.insert(sheet.counter_style_rules.end(),
                                         imported_sheet.counter_style_rules.begin(),
                                         imported_sheet.counter_style_rules.end());
    }

    if (!imported_rules.empty()) {
        // Imported rules come before the sheet's own rules (CSS cascade order)
        imported_rules.insert(imported_rules.end(),
                              sheet.rules.begin(), sheet.rules.end());
        sheet.rules = std::move(imported_rules);
    }
}

// Public entry point: process @import rules in a stylesheet.
// Creates per-call visited set and fetch cache, then delegates to the implementation.
void process_css_imports(clever::css::StyleSheet& sheet,
                         const std::string& base_url,
                         int viewport_width, int viewport_height,
                         int depth = 0) {
    // Seed visited set with the stylesheet's own base URL so a stylesheet
    // cannot directly or indirectly import itself
    std::unordered_set<std::string> visited;
    if (!base_url.empty()) visited.insert(base_url);

    std::unordered_map<std::string, std::string> fetch_cache;
    process_css_imports_impl(sheet, base_url, viewport_width, viewport_height,
                             depth, visited, fetch_cache);
}

// ---------------------------------------------------------------------------
// Parallel resource prefetching
// ---------------------------------------------------------------------------

// Pre-scan the DOM tree and collect all image URLs that will be needed during
// layout tree building.  This includes:
//   - <img src="...">
//   - <picture> / <source srcset="...">
//   - CSS background-image: url(...)  (from inline style attributes)
// The collected URLs are resolved against base_url.
void extract_image_urls(const clever::html::SimpleNode& node,
                        const std::string& base_url,
                        std::vector<std::string>& urls) {
    if (node.type == clever::html::SimpleNode::Element) {
        std::string tag = to_lower(node.tag_name);

        // Skip inert subtrees
        if (tag == "template") return;
        if (tag == "noscript" && !g_noscript_fallback) return;

        if (tag == "img") {
            std::string src = get_attr(node, "src");
            if (!src.empty()) {
                std::string resolved = resolve_url(src, base_url);
                if (!resolved.empty()) urls.push_back(std::move(resolved));
            }
        } else if (tag == "picture") {
            // Collect <source srcset="..."> and fallback <img src="...">
            for (auto& child : node.children) {
                if (child->type != clever::html::SimpleNode::Element) continue;
                std::string child_tag = to_lower(child->tag_name);
                if (child_tag == "source") {
                    std::string srcset = get_attr(*child, "srcset");
                    if (!srcset.empty()) {
                        // Use first candidate (strip descriptor suffix)
                        auto space_pos = srcset.find(' ');
                        if (space_pos != std::string::npos)
                            srcset = srcset.substr(0, space_pos);
                        std::string resolved = resolve_url(srcset, base_url);
                        if (!resolved.empty()) urls.push_back(std::move(resolved));
                    }
                } else if (child_tag == "img") {
                    std::string src = get_attr(*child, "src");
                    if (!src.empty()) {
                        std::string resolved = resolve_url(src, base_url);
                        if (!resolved.empty()) urls.push_back(std::move(resolved));
                    }
                }
            }
        }

        // Check inline style for background-image: url(...)
        std::string inline_style = get_attr(node, "style");
        if (!inline_style.empty()) {
            // Simple extraction of url(...) from background-image or background
            auto find_url_in_style = [&](const std::string& style_str) {
                size_t pos = 0;
                while (pos < style_str.size()) {
                    size_t url_start = style_str.find("url(", pos);
                    if (url_start == std::string::npos) break;
                    url_start += 4;
                    // Skip whitespace and quotes
                    while (url_start < style_str.size() &&
                           (style_str[url_start] == '\'' || style_str[url_start] == '"' ||
                            style_str[url_start] == ' '))
                        url_start++;
                    size_t url_end = url_start;
                    while (url_end < style_str.size() &&
                           style_str[url_end] != ')' && style_str[url_end] != '\'' &&
                           style_str[url_end] != '"')
                        url_end++;
                    if (url_end > url_start) {
                        std::string img_url = style_str.substr(url_start, url_end - url_start);
                        // Skip data: URIs (already handled locally, no network fetch needed)
                        if (img_url.size() < 5 || to_lower(img_url.substr(0, 5)) != "data:") {
                            std::string resolved = resolve_url(img_url, base_url);
                            if (!resolved.empty()) urls.push_back(std::move(resolved));
                        }
                    }
                    pos = url_end;
                }
            };
            find_url_in_style(inline_style);
        }
    }

    for (auto& child : node.children) {
        extract_image_urls(*child, base_url, urls);
    }
}

// Extract external script URLs from <script src="..."> elements
void extract_script_urls(const clever::html::SimpleNode& node,
                         const std::string& base_url,
                         std::vector<std::string>& urls) {
    if (node.type == clever::html::SimpleNode::Element) {
        std::string tag = to_lower(node.tag_name);
        if (tag == "template") return;
        if (tag == "noscript" && !g_noscript_fallback) return;

        if (tag == "script") {
            std::string src = get_attr(node, "src");
            if (!src.empty()) {
                // Skip non-JS types
                std::string script_type = normalize_mime_type(get_attr(node, "type"));
                if (script_type.empty() || script_type == "text/javascript" ||
                    script_type == "application/javascript" ||
                    script_type == "text/ecmascript" ||
                    script_type == "application/ecmascript") {
                    std::string resolved = resolve_url(src, base_url);
                    if (!resolved.empty()) urls.push_back(std::move(resolved));
                }
            }
        }
    }
    for (auto& child : node.children) {
        extract_script_urls(*child, base_url, urls);
    }
}

// Prefetch all images in parallel using std::async.
// Results are stored directly into the image cache so that subsequent calls to
// fetch_and_decode_image() will hit the cache instead of making network requests.
void prefetch_images_parallel(const std::vector<std::string>& urls) {
    if (urls.empty()) return;

    // Deduplicate and skip already-cached URLs
    std::vector<std::string> to_fetch;
    {
        std::lock_guard<std::mutex> lock(s_image_cache_mutex);
        std::unordered_set<std::string> seen;
        for (auto& url : urls) {
            if (seen.count(url)) continue;
            seen.insert(url);
            if (s_image_cache.find(url) != s_image_cache.end()) continue;
            to_fetch.push_back(url);
        }
    }

    if (to_fetch.empty()) return;

    // Cap concurrency to avoid overwhelming the system
    const size_t max_concurrent = std::min(to_fetch.size(), size_t(8));

    // Launch parallel fetches using std::async
    std::vector<std::future<DecodedImage>> futures;
    futures.reserve(to_fetch.size());

    for (size_t i = 0; i < to_fetch.size(); ++i) {
        // Copy URL into the lambda to avoid dangling reference
        std::string url_copy = to_fetch[i];
        futures.push_back(get_resource_thread_pool().submit([url_copy]() -> DecodedImage {
            // fetch_and_decode_image handles all decoding and caching internally
            return fetch_and_decode_image(url_copy);
        }));

        // Throttle: when we've launched max_concurrent, wait for the oldest to finish
        if (futures.size() >= max_concurrent) {
            futures[futures.size() - max_concurrent].wait();
        }
    }

    // Wait for all remaining futures
    for (auto& f : futures) {
        if (f.valid()) f.wait();
    }
}

// Fetch all CSS stylesheets in parallel.
// Returns a vector of (url, css_text, final_url) tuples in the same order as the input.
struct CssFetchResult {
    std::string url;        // original URL
    std::string css_text;   // fetched CSS text (empty on failure)
    std::string final_url;  // URL after redirects
};

std::vector<CssFetchResult> fetch_css_parallel(const std::vector<std::string>& urls) {
    std::vector<CssFetchResult> results(urls.size());

    if (urls.empty()) return results;

    std::vector<std::future<CssFetchResult>> futures;
    futures.reserve(urls.size());

    for (size_t i = 0; i < urls.size(); ++i) {
        std::string url_copy = urls[i];
        futures.push_back(get_resource_thread_pool().submit([url_copy]() -> CssFetchResult {
            CssFetchResult r;
            r.url = url_copy;
            r.final_url = url_copy;
            r.css_text = fetch_css(url_copy, &r.final_url);
            return r;
        }));
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        results[i] = futures[i].get();
    }

    return results;
}

// Prefetch external script sources in parallel.
// Returns a map from resolved URL -> fetched script body.
std::unordered_map<std::string, std::string> prefetch_scripts_parallel(
    const std::vector<std::string>& urls) {
    std::unordered_map<std::string, std::string> results;
    if (urls.empty()) return results;

    // Deduplicate
    std::vector<std::string> unique_urls;
    {
        std::unordered_set<std::string> seen;
        for (auto& u : urls) {
            if (seen.insert(u).second) unique_urls.push_back(u);
        }
    }

    struct ScriptFetchResult {
        std::string url;
        std::string body;
    };

    std::vector<std::future<ScriptFetchResult>> futures;
    futures.reserve(unique_urls.size());

    for (auto& url : unique_urls) {
        std::string url_copy = url;
        futures.push_back(get_resource_thread_pool().submit([url_copy]() -> ScriptFetchResult {
            ScriptFetchResult r;
            r.url = url_copy;
            auto response = fetch_with_redirects(url_copy, "application/javascript, */*", 5);
            if (response && response->status >= 200 && response->status < 300) {
                bool looks_like_html = false;
                if (auto ct = response->headers.get("content-type"); ct.has_value()) {
                    std::string ct_norm = normalize_mime_type(*ct);
                    if (ct_norm == "text/html" || ct_norm == "application/xhtml+xml") {
                        looks_like_html = true;
                    }
                }
                if (!looks_like_html) {
                    r.body = response->body_as_string();
                }
            }
            return r;
        }));
    }

    for (auto& f : futures) {
        auto r = f.get();
        if (!r.body.empty()) {
            results[r.url] = std::move(r.body);
        }
    }

    return results;
}

void trigger_lazy_loading_if_near_viewport(clever::layout::LayoutNode* layout_node,
                                           float viewport_scroll_y,
                                           float viewport_height) {
    if (!layout_node || !layout_node->loading_lazy || layout_node->lazy_img_url.empty())
        return;
    (void)viewport_height;

    float abs_y = layout_node->geometry.y;
    auto* current = layout_node->parent;
    while (current) {
        abs_y += current->geometry.y + current->geometry.border.top + current->geometry.padding.top;
        current = current->parent;
    }

    if ((abs_y + layout_node->geometry.border_box_height()) > (viewport_scroll_y - 200.0f)) {
        auto decoded = fetch_and_decode_image(layout_node->lazy_img_url);
        if (decoded.pixels && !decoded.pixels->empty()) {
            layout_node->image_pixels = decoded.pixels;
            layout_node->image_width = decoded.width;
            layout_node->image_height = decoded.height;
        }
        layout_node->lazy_img_url.clear();
    }
}

} // anonymous namespace

RenderResult render_html(const std::string& html, int viewport_width, int viewport_height) {
    return render_html(html, "", viewport_width, viewport_height);
}

RenderResult render_html(const std::string& html, const std::string& base_url,
                         int viewport_width, int viewport_height,
                         const std::set<int>& toggled_details) {
    g_toggled_details = &toggled_details;
    auto result = render_html(html, base_url, viewport_width, viewport_height);
    g_toggled_details = nullptr;
    return result;
}

RenderResult render_html(const std::string& html, const std::string& base_url,
                         int viewport_width, int viewport_height) {
    const int device_viewport_width = std::max(1, viewport_width);
    const int device_viewport_height = std::max(1, viewport_height);
    int layout_viewport_width = device_viewport_width;
    int layout_viewport_height = device_viewport_height;

    RenderResult result;
    result.width = layout_viewport_width;
    result.height = layout_viewport_height;
    result.success = false;

    // Reset interactive <details> toggle counter
    g_details_id_counter = 0;
    g_noscript_fallback = false;

    // Force light mode for CSS resolution. Most websites (including Google)
    // expect prefers-color-scheme: light. Our browser doesn't have a dark mode
    // UI, so forcing system dark mode causes pages to render with dark
    // backgrounds that clash with our light chrome.
    // The override mechanism is still respected for testing.
    if (clever::css::get_dark_mode_override() < 0) {
        clever::css::set_dark_mode(false);
    }

    // Set default viewport dimensions for vw/vh/vmin/vmax CSS units.
    // This may be overridden after parsing <meta name="viewport">.
    clever::css::Length::set_viewport(static_cast<float>(layout_viewport_width),
                                      static_cast<float>(layout_viewport_height));

    // Initialize container query dimensions to viewport as default
    clever::layout::LayoutNode::s_container_width = static_cast<float>(layout_viewport_width);
    clever::layout::LayoutNode::s_container_height = static_cast<float>(layout_viewport_height);

    try {
        // Step 1: Parse HTML -> SimpleNode tree
        auto doc = clever::html::parse(html);
        if (!doc) {
            result.error = "Failed to parse HTML";
            return result;
        }

        MetaViewportInfo meta_viewport =
            parse_meta_viewport_from_head(*doc, device_viewport_width, device_viewport_height);
        if (meta_viewport.width.has_value()) {
            layout_viewport_width = std::max(1, *meta_viewport.width);
        }
        if (meta_viewport.height.has_value()) {
            layout_viewport_height = std::max(1, *meta_viewport.height);
        }
        result.width = layout_viewport_width;
        result.height = layout_viewport_height;

        // Apply effective viewport dimensions (including <meta name="viewport">)
        // so vw/vh/vmin/vmax resolve against the layout viewport.
        clever::css::Length::set_viewport(static_cast<float>(layout_viewport_width),
                                          static_cast<float>(layout_viewport_height));

        // Initialize container query units to viewport dimensions as fallback
        // (when inside an @container, these are updated to the container's dimensions)
        clever::layout::LayoutNode::s_container_width = static_cast<float>(layout_viewport_width);
        clever::layout::LayoutNode::s_container_height = static_cast<float>(layout_viewport_height);

        // Extract page title from <title> element
        auto title_nodes = doc->find_all_elements("title");
        if (!title_nodes.empty()) {
            result.page_title = title_nodes[0]->text_content();
        }

        // Extract <meta> tag information: charset, http-equiv, description, theme-color
        auto meta_nodes = doc->find_all_elements("meta");
        bool refresh_found = false;
        int document_color_scheme = 0;
        for (const auto* meta : meta_nodes) {
            if (!meta) continue;

            // --- charset attribute: <meta charset="utf-8"> ---
            if (result.page_charset.empty()) {
                std::string charset_attr = trim(get_attr(*meta, "charset"));
                if (!charset_attr.empty()) {
                    result.page_charset = to_lower(charset_attr);
                }
            }

            std::string name_attr = to_lower(trim(get_attr(*meta, "name")));
            std::string http_equiv = to_lower(trim(get_attr(*meta, "http-equiv")));
            std::string content = get_attr(*meta, "content");

            // --- http-equiv="refresh" ---
            if (!refresh_found && http_equiv == "refresh") {
                if (!content.empty()) {
                    // Parse content: "N" or "N;url=URL" or "N; url=URL"
                    size_t pos = 0;
                    while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos])))
                        pos++;
                    size_t num_start = pos;
                    while (pos < content.size() && std::isdigit(static_cast<unsigned char>(content[pos])))
                        pos++;
                    if (pos > num_start) {
                        result.meta_refresh_delay = std::stoi(content.substr(num_start, pos - num_start));
                    } else {
                        result.meta_refresh_delay = 0;
                    }
                    while (pos < content.size() &&
                           (content[pos] == ';' || content[pos] == ',' ||
                            std::isspace(static_cast<unsigned char>(content[pos]))))
                        pos++;
                    if (pos + 3 <= content.size() && to_lower(content.substr(pos, 3)) == "url") {
                        pos += 3;
                        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos])))
                            pos++;
                        if (pos < content.size() && content[pos] == '=')
                            pos++;
                        while (pos < content.size() &&
                               (content[pos] == '\'' || content[pos] == '"' ||
                                std::isspace(static_cast<unsigned char>(content[pos]))))
                            pos++;
                        size_t url_start = pos;
                        size_t url_end = content.size();
                        while (url_end > url_start &&
                               (content[url_end - 1] == '\'' || content[url_end - 1] == '"' ||
                                std::isspace(static_cast<unsigned char>(content[url_end - 1]))))
                            url_end--;
                        if (url_end > url_start) {
                            result.meta_refresh_url = content.substr(url_start, url_end - url_start);
                        }
                    }
                    refresh_found = true;
                }
            }

            // --- http-equiv="Content-Type": extract charset from content="text/html; charset=utf-8" ---
            if (result.page_charset.empty() && http_equiv == "content-type" && !content.empty()) {
                auto cs_pos = to_lower(content).find("charset=");
                if (cs_pos != std::string::npos) {
                    std::string cs = trim(content.substr(cs_pos + 8));
                    // Strip any trailing semicolon or quote
                    auto end_pos = cs.find_first_of(";\"' \t");
                    if (end_pos != std::string::npos) cs = cs.substr(0, end_pos);
                    if (!cs.empty()) result.page_charset = to_lower(cs);
                }
            }

            // --- name="description" ---
            if (result.page_description.empty() && name_attr == "description") {
                result.page_description = trim(content);
            }

            // --- name="theme-color" ---
            if (result.theme_color.empty() && name_attr == "theme-color") {
                result.theme_color = trim(content);
            }

            if (name_attr == "color-scheme" && !content.empty()) {
                document_color_scheme = parse_color_scheme_content(content);
            }
        }

        // Extract <link> tag information: favicon (with priority), canonical URL
        // Favicon candidate scoring: prefer rel="icon" over apple-touch-icon; PNG/ICO > SVG;
        // optimal sizes (32x32 > 16x16 > 64x64); penalize very large icons.
        struct FaviconCandidate {
            std::string url;
            int priority = 0;
            int pixel_size = 0;
        };
        std::vector<FaviconCandidate> favicon_candidates;

        auto link_nodes = doc->find_all_elements("link");
        for (const auto* link : link_nodes) {
            if (!link) continue;
            std::string rel, href, sizes_attr, type_attr;
            for (const auto& attr : link->attributes) {
                if (attr.name == "rel") rel = attr.value;
                else if (attr.name == "href") href = attr.value;
                else if (attr.name == "sizes") sizes_attr = attr.value;
                else if (attr.name == "type") type_attr = attr.value;
            }
            if (href.empty()) continue;

            std::string rel_lower = to_lower(rel);

            // Helper to check if rel contains a token
            auto has_rel_token = [&](const std::string& token) -> bool {
                // Tokenize rel on whitespace
                std::istringstream iss(rel_lower);
                std::string tok;
                while (iss >> tok) {
                    if (tok == token) return true;
                }
                return false;
            };

            // --- rel="canonical" ---
            if (result.canonical_url.empty() && has_rel_token("canonical")) {
                result.canonical_url = resolve_url(href, base_url);
                continue;
            }

            // --- favicon rel tokens ---
            bool is_icon = has_rel_token("icon");
            bool is_shortcut_icon = (rel_lower == "shortcut icon");
            bool is_apple = has_rel_token("apple-touch-icon");

            if (!is_icon && !is_shortcut_icon && !is_apple) continue;

            FaviconCandidate cand;
            cand.url = resolve_url(href, base_url);

            // Base priority by rel type
            if (is_icon || is_shortcut_icon) cand.priority = 20;
            else if (is_apple)               cand.priority = 10;

            // Bonus for image type
            std::string type_lower = to_lower(type_attr);
            if (type_lower == "image/png" || type_lower == "image/x-icon" ||
                type_lower == "image/vnd.microsoft.icon") {
                cand.priority += 5;
            } else if (type_lower == "image/svg+xml") {
                cand.priority += 1;
            }

            // Bonus/penalty for sizes attribute (e.g., "32x32", "16x16")
            if (!sizes_attr.empty() && to_lower(sizes_attr) != "any") {
                auto x_pos = sizes_attr.find_first_of("xX");
                if (x_pos != std::string::npos) {
                    try {
                        int sz = std::stoi(sizes_attr.substr(0, x_pos));
                        cand.pixel_size = sz;
                        if (sz == 32) cand.priority += 8;
                        else if (sz == 16) cand.priority += 6;
                        else if (sz >= 24 && sz <= 48) cand.priority += 3;
                        else if (sz > 96) cand.priority -= 2;
                    } catch (...) {}
                }
            }

            favicon_candidates.push_back(std::move(cand));
        }

        // Select best favicon candidate
        if (!favicon_candidates.empty()) {
            auto best = std::max_element(favicon_candidates.begin(), favicon_candidates.end(),
                [](const FaviconCandidate& a, const FaviconCandidate& b) {
                    return a.priority < b.priority;
                });
            result.favicon_url = best->url;
        }

        // Fallback: try /favicon.ico if no <link rel="icon"> found
        if (result.favicon_url.empty() && !base_url.empty()) {
            auto scheme_end = base_url.find("://");
            if (scheme_end != std::string::npos) {
                auto host_end = base_url.find('/', scheme_end + 3);
                if (host_end == std::string::npos)
                    result.favicon_url = base_url + "/favicon.ico";
                else
                    result.favicon_url = base_url.substr(0, host_end) + "/favicon.ico";
            }
        }

        // Extract <base href="..."> element to override base URL
        std::string effective_base_url = base_url;
        auto base_nodes = doc->find_all_elements("base");
        if (!base_nodes.empty()) {
            for (const auto* base_node : base_nodes) {
                if (!base_node || is_in_inert_subtree(base_node)) continue;
                for (const auto& attr : base_node->attributes) {
                    if (attr.name == "href" && !attr.value.empty()) {
                        effective_base_url = resolve_url(attr.value, base_url);
                        break;
                    }
                }
                if (effective_base_url != base_url) {
                    break;
                }
            }
        }

        // Step 2: Build StyleResolver with user-agent stylesheet + page styles

        // User-agent stylesheet
        static const std::string ua_css =
            // Block-level elements (HTML spec default display values)
            "html, body, div, p, blockquote, pre, figure, figcaption, "
            "h1, h2, h3, h4, h5, h6, "
            "ul, ol, li, dl, dd, dt, "
            "form, fieldset, legend, details, summary, "
            "table, thead, tbody, tfoot, caption, "
            "nav, aside, section, article, main, header, footer, search, menu, "
            "address, hr, noscript, center, dialog, hgroup "
            "{ display: block; }"
            "html { display: block; }"
            "body { display: block; margin: 8px; }"
            "h1 { margin-top: 21px; margin-bottom: 21px; }"
            "h2 { margin-top: 19px; margin-bottom: 19px; }"
            "h3 { margin-top: 18px; margin-bottom: 18px; }"
            "h4, h5, h6 { margin-top: 21px; margin-bottom: 21px; }"
            "p { margin-top: 16px; margin-bottom: 16px; }"
            "ul, ol, menu { margin-top: 16px; margin-bottom: 16px; padding-left: 40px; list-style-type: disc; }"
            "li { display: list-item; margin-bottom: 4px; }"
            "blockquote { margin: 16px 40px; }"
            "pre { margin: 16px 0; padding: 8px; background-color: #f5f5f5; white-space: pre; font-family: monospace; }"
            "hr { margin: 8px 0; }"
            "a { color: #0000ee; text-decoration: underline; }"
            "em, i, cite, dfn, var { font-style: italic; }"
            "strong, b { font-weight: bold; }"
            "u, ins { text-decoration: underline; }"
            "s, del { text-decoration: line-through; }"
            "small { font-size: 13px; }"
            "sub { font-size: 12px; vertical-align: sub; }"
            "sup { font-size: 12px; vertical-align: super; }"
            "mark { background-color: #ffff00; color: #000000; }"
            "abbr, acronym { text-decoration: underline; text-decoration-style: dotted; }"
            "address { font-style: italic; }"
            "figcaption { font-size: 14px; color: #666; }"
            "figure { margin: 16px 40px; }"
            "fieldset { border: 1px solid #999; padding: 8px; margin: 8px 0; }"
            "legend { font-weight: bold; padding: 0 4px; }"
            "table { display: table; margin: 0; border-collapse: separate; border-spacing: 2px; }"
            "tr { display: table-row; }"
            "td, th { display: table-cell; padding: 4px 8px; }"
            "thead { display: table-header-group; font-weight: bold; }"
            "tbody { display: table-row-group; }"
            "tfoot { display: table-footer-group; }"
            "caption { display: table-caption; text-align: center; font-weight: bold; padding: 4px 0; }"
            "col { display: table-column; }"
            "colgroup { display: table-column-group; }"
            "code, kbd, samp, tt { font-family: monospace; }"
            "img { display: inline-block; }"
            "input, button, select, textarea { display: inline-block; }"
            "hidden, [hidden] { display: none; }";

        auto ua_stylesheet = clever::css::parse_stylesheet(ua_css);

        clever::css::StyleResolver resolver;
        resolver.set_viewport(static_cast<float>(layout_viewport_width),
                              static_cast<float>(layout_viewport_height));
        resolver.add_stylesheet(ua_stylesheet);

        // Fetch and parse external stylesheets from <link> elements.
        // All stylesheet URLs are fetched in parallel, then parsed and
        // processed sequentially to maintain cascade order.
        std::vector<clever::css::StyleSheet> external_sheets;
        auto link_urls = extract_link_stylesheets(*doc, effective_base_url);
        auto css_fetched = fetch_css_parallel(link_urls);
        for (auto& css_result : css_fetched) {
            if (!css_result.css_text.empty()) {
                external_sheets.push_back(clever::css::parse_stylesheet(css_result.css_text));
                // Fetch and merge @import rules from the external stylesheet
                process_css_imports(external_sheets.back(), css_result.final_url,
                                    layout_viewport_width, layout_viewport_height);
                // Evaluate @media/@supports queries and promote matching rules
                flatten_media_queries(external_sheets.back(), layout_viewport_width, layout_viewport_height);
                flatten_supports_rules(external_sheets.back());
                flatten_layer_rules(external_sheets.back());
                flatten_container_rules(external_sheets.back(), layout_viewport_width);
                flatten_scope_rules(external_sheets.back());
                resolver.add_stylesheet(external_sheets.back());
            }
        }

        // Extract and parse inline <style> elements
        std::string css_text = extract_style_content(*doc);
        clever::css::StyleSheet page_stylesheet;

        if (!css_text.empty()) {
            page_stylesheet = clever::css::parse_stylesheet(css_text);
            // Fetch and merge @import rules from inline <style> elements
            process_css_imports(page_stylesheet, effective_base_url,
                                layout_viewport_width, layout_viewport_height);
            // Evaluate @media/@supports queries and promote matching rules
            flatten_media_queries(page_stylesheet, layout_viewport_width, layout_viewport_height);
            flatten_supports_rules(page_stylesheet);
            flatten_layer_rules(page_stylesheet);
            flatten_container_rules(page_stylesheet, layout_viewport_width);
            flatten_scope_rules(page_stylesheet);
            resolver.add_stylesheet(page_stylesheet);
        }

        // Collect @property rules and set initial values as default custom properties
        std::unordered_map<std::string, clever::css::PropertyRule> property_registry;
        apply_property_rules(ua_stylesheet, property_registry);
        for (auto& ext : external_sheets) {
            apply_property_rules(ext, property_registry);
        }
        apply_property_rules(page_stylesheet, property_registry);
        // Set initial values as default custom properties on the resolver
        for (auto& [name, pr] : property_registry) {
            if (!pr.initial_value.empty()) {
                resolver.set_default_custom_property(name, pr.initial_value);
            }
        }

        // Collect @keyframes definitions from all stylesheets
        collect_keyframes(ua_stylesheet, result.keyframes);
        for (auto& ext : external_sheets) {
            collect_keyframes(ext, result.keyframes);
        }
        collect_keyframes(page_stylesheet, result.keyframes);

        // Build KeyframeAnimation map from collected definitions
        build_keyframe_animation_map(result.keyframes, result.keyframe_animations);

        // Collect @font-face rules from all stylesheets
        for (auto& ff : ua_stylesheet.font_faces) {
            result.font_faces.push_back(ff);
        }
        for (auto& ext : external_sheets) {
            for (auto& ff : ext.font_faces) {
                result.font_faces.push_back(ff);
            }
        }
        for (auto& ff : page_stylesheet.font_faces) {
            result.font_faces.push_back(ff);
        }

        // Download and register @font-face web fonts
        {
            // Static cache: URL -> raw (possibly decompressed) font data.
            // Persists across renders so each URL is fetched at most once.
            static std::unordered_map<std::string, std::vector<uint8_t>> font_cache;

            std::vector<FontFaceCandidate> font_requests;
            const FontCodepointRange text_codepoint_span = collect_text_codepoint_range(*doc);

            auto register_request = [&](const clever::css::FontFaceRule& ff,
                                        int weight,
                                        const std::string& style_for_match) {
                const auto* matched = match_best_font_face(result.font_faces,
                                                            ff.font_family,
                                                            normalize_font_weight(weight),
                                                            style_for_match);
                if (!matched) return;
                if (matched->src.empty() || matched->font_family.empty()) return;
                if (!font_face_intersects_text_codepoints(*matched, text_codepoint_span)) return;

                FontFaceCandidate req;
                req.rule = matched;
                req.weight = normalize_font_weight(weight);
                req.italic = (normalize_font_face_style(matched->font_style) != FontFaceStyle::Normal);
                req.unicode_min = matched->unicode_min;
                req.unicode_max = matched->unicode_max;

                for (auto& existing : font_requests) {
                    if (existing.rule == req.rule &&
                        existing.weight == req.weight &&
                        existing.italic == req.italic &&
                        existing.unicode_min == req.unicode_min &&
                        existing.unicode_max == req.unicode_max) {
                        return;
                    }
                }
                font_requests.push_back(req);
            };

            auto collect_weight_requests = [&](const clever::css::FontFaceRule& ff) {
                int min_weight = ff.min_weight;
                int max_weight = ff.max_weight;
                if (min_weight == 0 && max_weight == 0) {
                    min_weight = kDefaultWeight;
                    max_weight = kDefaultWeight;
                }
                if (min_weight > max_weight) std::swap(min_weight, max_weight);
                if (min_weight == 0 && max_weight == 900) {
                    min_weight = kFontWeightMin;
                    max_weight = kFontWeightMax;
                }
                const std::string request_style =
                    ff.font_style.empty() ? "normal" : ff.font_style;

                if (min_weight == max_weight) {
                    register_request(ff, min_weight, request_style);
                    return;
                }

                register_request(ff, min_weight, request_style);
                if (max_weight != min_weight) {
                    register_request(ff, max_weight, request_style);
                }
            };

            for (auto& ff : result.font_faces) {
                if (ff.font_family.empty() || ff.src.empty()) continue;
                if (!font_face_intersects_text_codepoints(ff, text_codepoint_span)) continue;
                collect_weight_requests(ff);
            }

            // Prepare font bytes for CoreText: decompress WOFF1 to SFNT if needed.
            // WOFF2 (Brotli-compressed) is passed through as-is; modern CoreText
            // may handle it, and there is no bundled Brotli decompressor.
            auto prepare_font_bytes = [](const std::vector<uint8_t>& raw) -> const std::vector<uint8_t>* {
                // WOFF1 magic: 'wOFF' = 0x774F4646
                if (raw.size() >= 4 &&
                    raw[0] == 0x77 && raw[1] == 0x4F &&
                    raw[2] == 0x46 && raw[3] == 0x46) {
                    // Returns a thread-local buffer to avoid repeated heap alloc.
                    static thread_local std::vector<uint8_t> decompressed_buf;
                    decompressed_buf = decompress_woff(raw);
                    if (!decompressed_buf.empty()) return &decompressed_buf;
                    // Decompression failed — fall back to raw bytes
                }
                return &raw;
            };

            auto load_font_bytes = [&](const std::string& font_url,
                                       FontDisplayPolicy font_display) -> std::optional<std::vector<uint8_t>> {
                auto timeout = font_display_timeout(font_display);
                if (timeout.count() == 0) {
                    auto response = fetch_with_redirects(font_url, "*/*", 10);
                    if (!response || response->status != 200 || response->body.empty()) {
                        return std::nullopt;
                    }
                    return response->body;
                }

                auto async_response = get_resource_thread_pool().submit([font_url]() -> std::optional<std::vector<uint8_t>> {
                    auto response = fetch_with_redirects(font_url, "*/*", 10);
                    if (!response || response->status != 200 || response->body.empty()) {
                        return std::nullopt;
                    }
                    return response->body;
                });

                if (async_response.wait_for(timeout) != std::future_status::ready) {
                    return std::nullopt;
                }
                return async_response.get();
            };

            for (auto& req : font_requests) {
                const auto* ff = req.rule;
                if (!ff || ff->font_family.empty() || ff->src.empty()) continue;

                std::string font_url = extract_preferred_font_url(ff->src);
                if (font_url.empty()) continue;

                const std::string lower_font_url = to_lower(font_url);
                const FontDisplayPolicy font_display = parse_font_display_policy(ff->font_display);
                const bool should_cache = should_cache_font(font_display);

                if (lower_font_url.rfind("data:", 0) == 0) {
                    auto cache_it = should_cache ? font_cache.find(font_url) : font_cache.end();
                    if (cache_it != font_cache.end()) {
                        const auto* font_bytes = prepare_font_bytes(cache_it->second);
                        clever::paint::TextRenderer::register_font(
                            ff->font_family, *font_bytes, req.weight, req.italic);
                        continue;
                    }

                    auto decoded_font_data = decode_font_data_url(font_url);
                    if (!decoded_font_data || decoded_font_data->empty()) continue;

                    if (should_cache) {
                        // Cache raw decoded bytes (WOFF decompression happens per-registration)
                        font_cache[font_url] = *decoded_font_data;
                    }
                    const auto* font_bytes = prepare_font_bytes(*decoded_font_data);
                    clever::paint::TextRenderer::register_font(
                        ff->font_family, *font_bytes, req.weight, req.italic);
                    continue;
                }

                // Resolve relative URL against the page base URL
                font_url = resolve_url(font_url, effective_base_url);
                if (font_url.empty()) continue;

                // Check cache first
                if (should_cache) {
                    auto cache_it = font_cache.find(font_url);
                    if (cache_it != font_cache.end()) {
                        const auto* font_bytes = prepare_font_bytes(cache_it->second);
                        clever::paint::TextRenderer::register_font(
                            ff->font_family, *font_bytes, req.weight, req.italic);
                        continue;
                    }
                }

                auto response = load_font_bytes(font_url, font_display);
                if (!response.has_value() || response->empty()) {
                    continue;
                }

                // Store raw bytes in cache
                if (should_cache) {
                    font_cache[font_url] = *response;
                }

                // Decompress WOFF1 if necessary, then register with CoreText
                const auto& downloaded = should_cache ? font_cache[font_url] : *response;
                const auto* font_bytes = prepare_font_bytes(downloaded);
                clever::paint::TextRenderer::register_font(
                    ff->font_family, *font_bytes, req.weight, req.italic);
            }
        }

        // Step 3: Build layout tree with full CSS cascade
        clever::css::ComputedStyle root_style;
        root_style.z_index = clever::layout::Z_INDEX_AUTO;
        root_style.display = clever::css::Display::Block;
        root_style.font_size = clever::css::Length::px(16);
        root_style.color = clever::css::Color::black();
        root_style.background_color = clever::css::Color::white();

        // Reset CSS counters and form collection before tree build
        css_counters.clear();
        css_counter_scopes.clear();
        collected_forms.clear();
        collected_datalists.clear();

        // Step 3a: Pre-scan DOM for resource URLs and launch parallel prefetches.
        // Images are prefetched in the background while scripts execute; by the
        // time build_layout_tree_styled() runs, most images will already be in
        // the cache.  Script sources are also prefetched in parallel so the
        // serial execution loop can use them from the map without re-fetching.
        std::vector<std::string> image_urls;
        extract_image_urls(*doc, effective_base_url, image_urls);

        // Launch image prefetch on a background thread (non-blocking)
        // Use ThreadPool::submit() to get a future we can wait on
        auto image_prefetch_future = get_resource_thread_pool().submit([image_urls]() {
            prefetch_images_parallel(image_urls);
        });

        // Prefetch external script sources in parallel
        std::vector<std::string> script_urls;
        extract_script_urls(*doc, effective_base_url, script_urls);
        auto prefetched_scripts = prefetch_scripts_parallel(script_urls);

        // Step 3b: Execute inline <script> elements
        // The JS engine is allocated on the heap so it can optionally be
        // persisted in the RenderResult for interactive event dispatch
        // (e.g., dispatching "click" events to addEventListener handlers).
        std::unique_ptr<clever::js::JSEngine> js_engine_ptr;
        {
            auto script_elements = doc->find_all_elements("script");
            if (!script_elements.empty()) {
                js_engine_ptr = std::make_unique<clever::js::JSEngine>();
                auto& js_engine = *js_engine_ptr;
                clever::js::install_dom_bindings(js_engine.context(), doc.get());
                clever::js::install_timer_bindings(js_engine.context());
                clever::js::install_window_bindings(js_engine.context(), effective_base_url,
                                                    layout_viewport_width, layout_viewport_height);
                clever::js::install_fetch_bindings(js_engine.context());

                // Set up module fetcher for dynamic imports
                js_engine.set_module_fetcher([&effective_base_url](const std::string& module_url) -> std::optional<std::string> {
                    // Resolve relative URLs
                    std::string resolved = resolve_url(module_url, effective_base_url);

                    // Fetch the module source
                    auto response = fetch_with_redirects(resolved, "application/javascript, */*", 5);
                    if (response && response->status >= 200 && response->status < 300) {
                        // Check content type to avoid loading HTML instead of JS
                        bool looks_like_html = false;
                        if (auto ct = response->headers.get("content-type"); ct.has_value()) {
                            std::string ct_norm = normalize_mime_type(*ct);
                            if (ct_norm == "text/html" || ct_norm == "application/xhtml+xml") {
                                looks_like_html = true;
                            }
                        }
                        if (!looks_like_html) {
                            return response->body_as_string();
                        }
                    }
                    return std::nullopt;
                });

                // Run preliminary layout so getBoundingClientRect/dimension properties work
                {
                    ElementViewTree pre_view_tree;
                    g_transition_runtime_enabled = false;
                    // Expose shadow roots so build_layout_tree_styled can render Web Components
                    g_shadow_roots = clever::js::get_shadow_roots_map(js_engine.context());
                    auto pre_root = build_layout_tree_styled(
                        *doc, root_style, resolver, pre_view_tree, nullptr, effective_base_url);
                    if (pre_root) {
                        clever::layout::LayoutEngine pre_engine;
                        clever::paint::TextRenderer pre_measurer;
                        pre_engine.set_text_measurer([&pre_measurer](const std::string& text, float font_size,
                                                                      const std::string& font_family, int font_weight,
                                                                      bool is_italic, float letter_spacing) -> float {
                            return pre_measurer.measure_text_width(text, font_size, font_family,
                                                                    font_weight, is_italic, letter_spacing);
                        });
                        pre_engine.compute(*pre_root, static_cast<float>(layout_viewport_width),
                                           static_cast<float>(layout_viewport_height));
                        clever::js::populate_layout_geometry(js_engine.context(), pre_root.get());

                        int html_color_scheme = resolve_color_scheme_from_layout_tag(*pre_root, "html");
                        if (html_color_scheme == -1) {
                            html_color_scheme = resolve_color_scheme_from_layout_tag(*pre_root, "body");
                        }
                        int resolved_color_scheme = resolve_prefers_color_scheme(
                            html_color_scheme, document_color_scheme);
                        js_engine.evaluate(
                            "globalThis.__vibrowser_prefers_color_scheme = " +
                            std::to_string(resolved_color_scheme) + ";",
                            "<vibrowser-color-scheme>");
                    }
                }

                std::unordered_set<const clever::html::SimpleNode*> executed_script_nodes;
                auto execute_pending_scripts = [&](int max_rounds) {
                    for (int round = 0; round < max_rounds; round++) {
                        int executed_this_round = 0;
                        auto pending_scripts = doc->find_all_elements("script");
                        for (const auto* script_elem : pending_scripts) {
                            if (!script_elem) continue;
                            if (executed_script_nodes.find(script_elem) != executed_script_nodes.end()) continue;

                            // Keep script execution aligned with style/render inert-subtree rules.
                            if (is_in_inert_subtree(script_elem)) {
                                executed_script_nodes.insert(script_elem);
                                continue;
                            }

                            // Determine script type and src attribute
                            std::string script_type;
                            std::string script_src;
                            for (const auto& attr : script_elem->attributes) {
                                if (attr.name == "type") script_type = normalize_mime_type(attr.value);
                                else if (attr.name == "src") script_src = attr.value;
                            }

                            // Skip non-JavaScript types (json, importmap, etc.)
                            if (!script_type.empty() && script_type != "text/javascript" &&
                                script_type != "application/javascript" &&
                                script_type != "text/ecmascript" &&
                                script_type != "application/ecmascript" &&
                                script_type != "module") {
                                executed_script_nodes.insert(script_elem);
                                continue;
                            }

                            const bool is_module = (script_type == "module");

                            std::string script_code;

                            if (!script_src.empty()) {
                                // Use prefetched script if available, otherwise fetch on demand
                                std::string resolved = resolve_url(script_src, effective_base_url);
                                auto prefetch_it = prefetched_scripts.find(resolved);
                                if (prefetch_it != prefetched_scripts.end()) {
                                    script_code = prefetch_it->second;
                                } else {
                                    // Fallback: fetch on demand (script may have been
                                    // dynamically inserted by JS after the pre-scan)
                                    auto response = fetch_with_redirects(resolved, "application/javascript, */*", 5);
                                    if (response && response->status >= 200 && response->status < 300) {
                                        bool looks_like_html = false;
                                        if (auto ct = response->headers.get("content-type"); ct.has_value()) {
                                            std::string ct_norm = normalize_mime_type(*ct);
                                            if (ct_norm == "text/html" || ct_norm == "application/xhtml+xml") {
                                                looks_like_html = true;
                                            }
                                        }
                                        if (!looks_like_html) {
                                            script_code = response->body_as_string();
                                        }
                                    }
                                }
                            } else {
                                // Extract inline script content
                                for (const auto& child : script_elem->children) {
                                    if (child->type == clever::html::SimpleNode::Text) {
                                        script_code += child->data;
                                    }
                                }
                            }

                            if (!script_code.empty()) {
                                if (is_module) {
                                    // ES module: document.currentScript is always null
                                    // per the HTML spec for module scripts
                                    js_engine.evaluate_module(script_code);
                                    if (js_engine.has_error()) {
                                        result.js_errors.push_back(js_engine.last_error());
                                    }
                                } else {
                                    // Classic script: set document.currentScript
                                    clever::js::set_current_script(
                                        js_engine.context(),
                                        const_cast<clever::html::SimpleNode*>(script_elem));

                                    js_engine.evaluate(script_code);
                                    if (js_engine.has_error()) {
                                        result.js_errors.push_back(js_engine.last_error());
                                    }

                                    // Clear document.currentScript after evaluation
                                    clever::js::set_current_script(js_engine.context(), nullptr);
                                }
                            }

                            executed_script_nodes.insert(script_elem);
                            executed_this_round++;
                        }

                        if (executed_this_round == 0) {
                            break;
                        }
                    }
                };

                // Initial script drain pass (also catches scripts inserted by earlier scripts).
                execute_pending_scripts(8);

                // Flush any pending zero-delay timers (without destroying state)
                clever::js::flush_ready_timers(js_engine.context(), 0);

                // Execute pending Promise microtasks (fetch().then(), etc.)
                clever::js::flush_fetch_promise_jobs(js_engine.context());
                execute_pending_scripts(4);

                // Fire DOMContentLoaded event on document and window
                clever::js::dispatch_dom_content_loaded(js_engine.context());

                // Flush timers set by DOMContentLoaded handlers (state preserved)
                clever::js::flush_ready_timers(js_engine.context(), 0);

                // Execute Promise microtasks again (handlers may create promises)
                clever::js::flush_fetch_promise_jobs(js_engine.context());
                execute_pending_scripts(4);

                // Fire any pending MutationObserver callbacks (from DOM mutations during scripts)
                clever::js::fire_mutation_observers(js_engine.context());

                // Fire IntersectionObserver callbacks (scripts have now set up observers)
                clever::js::fire_intersection_observers(
                    js_engine.context(), layout_viewport_width, layout_viewport_height);

                // Fire ResizeObserver callbacks with loop detection (max depth 4)
                {
                    static int resize_observer_depth = 0;
                    if (resize_observer_depth < 4) {
                        resize_observer_depth++;
                        clever::js::fire_resize_observers(
                            js_engine.context(), layout_viewport_width, layout_viewport_height);
                        resize_observer_depth--;
                    }
                }

                // Flush rAF callbacks queued by scripts before layout.
                clever::js::flush_animation_frames(js_engine.context());

                // Flush timers/promises set by observer callbacks
                clever::js::flush_ready_timers(js_engine.context(), 0);
                clever::js::flush_fetch_promise_jobs(js_engine.context());
                execute_pending_scripts(2);

                // Flush any MutationObserver callbacks triggered by observer callbacks
                clever::js::fire_mutation_observers(js_engine.context());

                // Convergence loop: flush chained timer/promise effects.
                // Many sites use patterns like setTimeout(fn,0) inside
                // DOMContentLoaded or other callbacks.  We do two phases:
                //   Phase 1: flush zero-delay timers iteratively (up to 8 rounds)
                //   Phase 2: flush short-delay timers (<=100ms) once + drain chains
                // This handles chained setTimeout(fn,0) and short-delay timers
                // that sites use for initialization.
                {
                    // Phase 1: zero-delay timer chains
                    for (int round = 0; round < 8; round++) {
                        int fired = clever::js::flush_ready_timers(
                            js_engine.context(), 0);
                        clever::js::flush_fetch_promise_jobs(js_engine.context());
                        execute_pending_scripts(1);
                        if (fired == 0) break;
                    }

                    // Phase 2: short-delay timers (treat <=100ms as "immediate"
                    // in our synchronous engine)
                    {
                        int fired = clever::js::flush_ready_timers(
                            js_engine.context(), 100);
                        clever::js::flush_fetch_promise_jobs(js_engine.context());
                        execute_pending_scripts(1);
                        // Drain any zero-delay chains spawned by short-delay timers
                        if (fired > 0) {
                            for (int round = 0; round < 4; round++) {
                                int f2 = clever::js::flush_ready_timers(
                                    js_engine.context(), 0);
                                clever::js::flush_fetch_promise_jobs(js_engine.context());
                                execute_pending_scripts(1);
                                if (f2 == 0) break;
                            }
                        }
                    }
                }

                // If JS modified document.title, update result
                auto js_title = clever::js::get_document_title(js_engine.context());
                if (!js_title.empty()) {
                    result.page_title = js_title;
                }

                // Capture console output
                result.js_console_output = js_engine.console_output();

                // Enable noscript fallback if JS produced many errors.
                // Sites like Google are heavily JS-dependent; when our JS
                // engine can't handle the scripts, the noscript content
                // provides a usable fallback.
                if (result.js_errors.size() >= 3) {
                    g_noscript_fallback = true;
                }

                // Clean up timer state (timers are not needed after initial render).
                // DOM bindings are intentionally kept alive so the persisted
                // JSEngine can dispatch interactive events (click, etc.) later.
                clever::js::cleanup_timers(js_engine.context());
                // Free any remaining rAF callbacks to prevent GC assertion on teardown.
                clever::js::cleanup_animation_frames(js_engine.context());
            }
        }

        // Wait for background image prefetch to complete before building the
        // layout tree.  At this point most images should already be cached,
        // making the tree build nearly instant for image-heavy pages.
        if (image_prefetch_future.valid()) {
            image_prefetch_future.wait();
        }

        ElementViewTree view_tree;
        if (!g_transition_animation_controller) {
            g_transition_animation_controller = std::make_unique<AnimationController>();
        }
        g_transition_runtime_enabled = true;
        g_current_layout_nodes_by_key.clear();
        g_current_style_keys.clear();
        // Set shadow roots map so build_layout_tree_styled can render Web Components
        g_shadow_roots = js_engine_ptr
            ? clever::js::get_shadow_roots_map(js_engine_ptr->context())
            : nullptr;
        auto layout_root = build_layout_tree_styled(
            *doc, root_style, resolver, view_tree, nullptr, effective_base_url);
        g_shadow_roots = nullptr; // Clear after use
        g_transition_runtime_enabled = false;
        prune_transition_runtime_state();

        if (!layout_root) {
            result.error = "Failed to build layout tree";
            return result;
        }

        // Step 4: Run layout engine
        clever::layout::LayoutEngine engine;

        // Inject real text measurement via CoreText so layout uses accurate widths
        clever::paint::TextRenderer layout_text_measurer;
        engine.set_text_measurer([&layout_text_measurer](const std::string& text, float font_size,
                                                          const std::string& font_family, int font_weight,
                                                          bool is_italic, float letter_spacing) -> float {
            return layout_text_measurer.measure_text_width(text, font_size, font_family,
                                                           font_weight, is_italic, letter_spacing);
        });

        engine.compute(*layout_root, static_cast<float>(layout_viewport_width),
                       static_cast<float>(layout_viewport_height));

        // Step 4a: Evaluate @container queries against actual container sizes
        // (two-pass approach: first layout determined container sizes, now evaluate)
        {
            std::vector<clever::css::ContainerRule> all_container_rules;
            // Collect container rules from all stylesheets
            for (auto& cr : ua_stylesheet.container_rules)
                all_container_rules.push_back(cr);
            for (auto& ext : external_sheets) {
                for (auto& cr : ext.container_rules)
                    all_container_rules.push_back(cr);
            }
            for (auto& cr : page_stylesheet.container_rules)
                all_container_rules.push_back(cr);

            if (!all_container_rules.empty() && layout_root) {
                bool changed = evaluate_container_queries_post_layout(
                    *layout_root, all_container_rules);
                if (changed) {
                    // Re-run layout since container query rules changed styles
                    engine.compute(*layout_root, static_cast<float>(layout_viewport_width),
                                   static_cast<float>(layout_viewport_height));
                }
            }
        }

        // Apply ruby annotation layout now that inline geometry is final.
        apply_ruby_layout_pass(*layout_root);

        // Step 4b: Detect overflow indicators and compute scroll content dimensions
        {
            std::function<void(clever::layout::LayoutNode&)> detect_overflow =
                [&](clever::layout::LayoutNode& node) {
                    // Compute scroll content dimensions for all scroll containers
                    // using all descendant content bounds, not just direct children.
                    float max_bottom = 0.0f;
                    float max_right = 0.0f;
                    if (node.is_scroll_container) {
                        auto accumulate_descendant_bounds =
                            [&](const clever::layout::LayoutNode& child,
                                float offset_x,
                                float offset_y,
                                auto&& self,
                                float& max_right,
                                float& max_bottom) -> void {
                                const float child_x = offset_x + child.geometry.x;
                                const float child_y = offset_y + child.geometry.y;
                                const float child_bottom = child_y + child.geometry.margin_box_height();
                                const float child_right = child_x + child.geometry.margin_box_width();

                                if (child_bottom > max_bottom) max_bottom = child_bottom;
                                if (child_right > max_right) max_right = child_right;

                                for (auto& grandchild : child.children) {
                                    if (grandchild) {
                                        self(*grandchild, child_x, child_y, self, max_right, max_bottom);
                                    }
                                }
                            };

                        float max_bottom = 0;
                        float max_right = 0;
                        for (auto& child : node.children) {
                            if (child) {
                                accumulate_descendant_bounds(
                                    *child, 0.0f, 0.0f, accumulate_descendant_bounds, max_right, max_bottom);
                            }
                        }
                        node.scroll_content_height = max_bottom;
                        node.scroll_content_width = max_right;

                        // Clamp scroll offsets to valid range
                        float max_scroll_y = std::max(0.0f, max_bottom - node.geometry.height);
                        float max_scroll_x = std::max(0.0f, max_right - node.geometry.width);
                        if (node.scroll_top > max_scroll_y) node.scroll_top = max_scroll_y;
                        if (node.scroll_left > max_scroll_x) node.scroll_left = max_scroll_x;
                        if (node.scroll_top < 0) node.scroll_top = 0;
                        if (node.scroll_left < 0) node.scroll_left = 0;
                    }

                    node.overflow_indicator_bottom = false;
                    node.overflow_indicator_right = false;
                    if (node.overflow >= 2) {
                        // overflow: 2=scroll, 3=auto
                        // Child and descendant positions are relative to the content area, so compare
                        // against the content box dimensions (geometry.width, geometry.height).
                        if (max_bottom > node.geometry.height) {
                            node.overflow_indicator_bottom = true;
                        }
                        if (max_right > node.geometry.width) {
                            node.overflow_indicator_right = true;
                        }

                        // For overflow:scroll (2), always show indicators
                        if (node.overflow == 2) {
                            node.overflow_indicator_bottom = true;
                            node.overflow_indicator_right = true;
                        }
                    }

                    for (auto& child : node.children) {
                        detect_overflow(*child);
                    }
                };
            detect_overflow(*layout_root);
        }

        // Trigger lazy loading for images/iframes near viewport
        {
            const float viewport_scroll_y = 0.0f;
            std::function<void(clever::layout::LayoutNode&)> trigger_lazy_loads;
            trigger_lazy_loads = [&](clever::layout::LayoutNode& node) {
                trigger_lazy_loading_if_near_viewport(
                    &node,
                    viewport_scroll_y,
                    static_cast<float>(layout_viewport_height));
                for (auto& child : node.children) {
                    if (child) trigger_lazy_loads(*child);
                }
            };
            if (layout_root) {
                trigger_lazy_loads(*layout_root);
            }
        }

        if (g_transition_animation_controller) {
            auto tick_now = std::chrono::steady_clock::now();
            double delta_ms = 0.0;
            if (g_transition_has_last_tick) {
                delta_ms = std::chrono::duration<double, std::milli>(
                    tick_now - g_transition_last_tick_time).count();
                if (!std::isfinite(delta_ms) || delta_ms < 0.0) delta_ms = 0.0;
                if (delta_ms > 200.0) delta_ms = 200.0;
            }
            g_transition_last_tick_time = tick_now;
            g_transition_has_last_tick = true;

            g_transition_animation_controller->tick(delta_ms);
            const auto& active_updates = g_transition_animation_controller->get_active_property_updates();
            for (const auto& update : active_updates) {
                if (!update.element) continue;
                auto runtime_key_it = g_transition_runtime_node_to_key.find(update.element);
                if (runtime_key_it == g_transition_runtime_node_to_key.end()) continue;
                auto node_it = g_current_layout_nodes_by_key.find(runtime_key_it->second);
                if (node_it == g_current_layout_nodes_by_key.end() || !node_it->second) continue;

                auto* target = node_it->second;
                if (update.has_float) {
                    if (update.property_name == "opacity") {
                        target->opacity = std::clamp(update.float_value, 0.0f, 1.0f);
                    } else if (update.property_name == "font-size") {
                        target->font_size = update.float_value;
                    } else if (update.property_name == "border-radius") {
                        target->border_radius = update.float_value;
                        target->border_radius_tl = update.float_value;
                        target->border_radius_tr = update.float_value;
                        target->border_radius_bl = update.float_value;
                        target->border_radius_br = update.float_value;
                    }
                } else if (update.has_color) {
                    uint32_t argb = (static_cast<uint32_t>(update.color_value.a) << 24) |
                                    (static_cast<uint32_t>(update.color_value.r) << 16) |
                                    (static_cast<uint32_t>(update.color_value.g) << 8) |
                                    static_cast<uint32_t>(update.color_value.b);
                    if (update.property_name == "background-color") {
                        target->background_color = argb;
                    } else if (update.property_name == "color") {
                        target->color = argb;
                    }
                } else if (update.has_length) {
                    if (update.property_name == "width") {
                        target->geometry.width = update.length_value;
                    } else if (update.property_name == "height") {
                        target->geometry.height = update.length_value;
                    } else if (update.property_name == "margin-top") {
                        target->geometry.margin.top = update.length_value;
                    } else if (update.property_name == "margin-right") {
                        target->geometry.margin.right = update.length_value;
                    } else if (update.property_name == "margin-bottom") {
                        target->geometry.margin.bottom = update.length_value;
                    } else if (update.property_name == "margin-left") {
                        target->geometry.margin.left = update.length_value;
                    } else if (update.property_name == "padding-top") {
                        target->geometry.padding.top = update.length_value;
                    } else if (update.property_name == "padding-right") {
                        target->geometry.padding.right = update.length_value;
                    } else if (update.property_name == "padding-bottom") {
                        target->geometry.padding.bottom = update.length_value;
                    } else if (update.property_name == "padding-left") {
                        target->geometry.padding.left = update.length_value;
                    }
                }
            }
            g_transition_animation_controller->clear_updates();
        }

        // Wire up CSS @keyframes animations: walk the layout tree and start
        // animations for elements that have animation-name set.
        if (g_transition_animation_controller && !result.keyframes.empty()) {
            std::function<void(clever::layout::LayoutNode&)> start_keyframe_anims;
            start_keyframe_anims = [&](clever::layout::LayoutNode& node) {
                if (!node.animation_name.empty() && node.animation_duration > 0) {
                    std::string style_key = node.tag_name;
                    if (!node.element_id.empty()) {
                        style_key = "id:" + node.element_id;
                    }
                    // Only start if not already animating this key
                    if (g_keyframe_animated_keys.find(style_key) == g_keyframe_animated_keys.end()) {
                        // Find matching keyframes definition
                        for (const auto& kf : result.keyframes) {
                            if (kf.name == node.animation_name) {
                                // Build a temporary ComputedStyle with animation parameters
                                clever::css::ComputedStyle anim_style;
                                anim_style.animation_duration = node.animation_duration;
                                anim_style.animation_delay = node.animation_delay;
                                anim_style.animation_timing = node.animation_timing;
                                anim_style.animation_bezier_x1 = node.animation_bezier_x1;
                                anim_style.animation_bezier_y1 = node.animation_bezier_y1;
                                anim_style.animation_bezier_x2 = node.animation_bezier_x2;
                                anim_style.animation_bezier_y2 = node.animation_bezier_y2;
                                anim_style.animation_steps_count = node.animation_steps_count;
                                anim_style.animation_fill_mode = node.animation_fill_mode;
                                anim_style.animation_direction = node.animation_direction;
                                anim_style.animation_iteration_count = node.animation_iteration_count;
                                // Copy current values for interpolation start points
                                anim_style.opacity = node.opacity;
                                anim_style.background_color = {
                                    static_cast<uint8_t>((node.background_color >> 16) & 0xFF),
                                    static_cast<uint8_t>((node.background_color >> 8) & 0xFF),
                                    static_cast<uint8_t>(node.background_color & 0xFF),
                                    static_cast<uint8_t>((node.background_color >> 24) & 0xFF)
                                };
                                anim_style.color = {
                                    static_cast<uint8_t>((node.color >> 16) & 0xFF),
                                    static_cast<uint8_t>((node.color >> 8) & 0xFF),
                                    static_cast<uint8_t>(node.color & 0xFF),
                                    static_cast<uint8_t>((node.color >> 24) & 0xFF)
                                };
                                g_transition_animation_controller->start_animation(
                                    &node, node.animation_name, kf, anim_style);
                                g_keyframe_animated_keys.insert(style_key);
                                break;
                            }
                        }
                    }
                }
                for (auto& child : node.children) {
                    if (child) start_keyframe_anims(*child);
                }
            };
            start_keyframe_anims(*layout_root);

            // Tick again to apply the first frame of new animations
            g_transition_animation_controller->tick(0.0);
            const auto& kf_updates = g_transition_animation_controller->get_active_property_updates();
            for (const auto& update : kf_updates) {
                if (!update.element) continue;
                auto* target = update.element;
                if (update.has_float) {
                    if (update.property_name == "opacity") {
                        target->opacity = std::clamp(update.float_value, 0.0f, 1.0f);
                    } else if (update.property_name == "font-size") {
                        target->font_size = update.float_value;
                    } else if (update.property_name == "border-radius") {
                        target->border_radius = update.float_value;
                        target->border_radius_tl = update.float_value;
                        target->border_radius_tr = update.float_value;
                        target->border_radius_bl = update.float_value;
                        target->border_radius_br = update.float_value;
                    }
                } else if (update.has_color) {
                    uint32_t argb = (static_cast<uint32_t>(update.color_value.a) << 24) |
                                    (static_cast<uint32_t>(update.color_value.r) << 16) |
                                    (static_cast<uint32_t>(update.color_value.g) << 8) |
                                    static_cast<uint32_t>(update.color_value.b);
                    if (update.property_name == "background-color") {
                        target->background_color = argb;
                    } else if (update.property_name == "color") {
                        target->color = argb;
                    }
                } else if (update.has_length) {
                    if (update.property_name == "width") {
                        target->geometry.width = update.length_value;
                    } else if (update.property_name == "height") {
                        target->geometry.height = update.length_value;
                    } else if (update.property_name == "margin-top") {
                        target->geometry.margin.top = update.length_value;
                    } else if (update.property_name == "margin-right") {
                        target->geometry.margin.right = update.length_value;
                    } else if (update.property_name == "margin-bottom") {
                        target->geometry.margin.bottom = update.length_value;
                    } else if (update.property_name == "margin-left") {
                        target->geometry.margin.left = update.length_value;
                    } else if (update.property_name == "padding-top") {
                        target->geometry.padding.top = update.length_value;
                    } else if (update.property_name == "padding-right") {
                        target->geometry.padding.right = update.length_value;
                    } else if (update.property_name == "padding-bottom") {
                        target->geometry.padding.bottom = update.length_value;
                    } else if (update.property_name == "padding-left") {
                        target->geometry.padding.left = update.length_value;
                    }
                }
            }
            g_transition_animation_controller->clear_updates();
        }

        // Step 5: Compute render height from laid out content.
        // We keep the full content height in a single buffer so below-the-fold
        // content (including content-visibility regions) can be painted correctly.
        // layout_root->geometry.height already includes root padding/border in this engine.
        // Adding them again here inflates the final render buffer height and creates
        // large blank regions below the actual page content.
        float content_h = layout_root->geometry.y +
                          layout_root->geometry.margin.top +
                          layout_root->geometry.height +
                          layout_root->geometry.margin.bottom;
        int render_height = std::max(layout_viewport_height,
                                     static_cast<int>(std::ceil(content_h)));
        // Cap to reasonable max to avoid OOM (e.g. 16384px)
        render_height = std::min(render_height, 16384);

        // Step 6: Paint -> DisplayList
        // Pass viewport dimensions; scroll offsets are 0 at static render time
        // (the shell composites sticky overlays for live viewport scrolling).
        Painter painter;
        auto display_list = painter.paint(*layout_root,
                                          static_cast<float>(render_height),
                                          static_cast<float>(layout_viewport_width),
                                          0.0f, 0.0f);

        // Step 7: Render to pixel buffer
        auto renderer = std::make_unique<SoftwareRenderer>(layout_viewport_width, render_height);
        renderer->clear({255, 255, 255, 255});
        renderer->render(display_list);

        result.renderer = std::move(renderer);
        result.links = display_list.links();
        result.cursor_regions = display_list.cursor_regions();
        result.form_submit_regions = display_list.form_submit_regions();
        result.details_toggle_regions = display_list.details_toggle_regions();
        result.select_click_regions = display_list.select_click_regions();
        // Extract text commands for text selection
        for (auto& cmd : display_list.commands()) {
            if (cmd.type == PaintCommand::DrawText && !cmd.text.empty()) {
                result.text_commands.push_back(cmd);
            }
        }
        // Extract ::selection colors from the layout tree root
        if (layout_root) {
            std::function<void(const clever::layout::LayoutNode*)> find_selection;
            find_selection = [&](const clever::layout::LayoutNode* n) {
                if (n->selection_color != 0) result.selection_color = n->selection_color;
                if (n->selection_bg_color != 0) result.selection_bg_color = n->selection_bg_color;
                if (result.selection_color != 0 && result.selection_bg_color != 0) return;
                for (auto& c : n->children) find_selection(c.get());
            };
            find_selection(layout_root.get());
        }
        // Collect element id -> Y position map for anchor/fragment scrolling.
        // Walk the layout tree accumulating absolute Y offsets.
        if (layout_root) {
            std::function<void(const clever::layout::LayoutNode&, float, float)> collect_ids;
            collect_ids = [&](const clever::layout::LayoutNode& n, float parent_abs_x, float parent_abs_y) {
                float abs_x = parent_abs_x + n.geometry.x;
                float abs_y = parent_abs_y + n.geometry.y;
                if (!n.element_id.empty()) {
                    result.id_positions[n.element_id] = abs_y;
                }
                float child_x = abs_x + n.geometry.border.left + n.geometry.padding.left;
                float child_y = abs_y + n.geometry.border.top + n.geometry.padding.top;
                for (auto& c : n.children) {
                    collect_ids(*c, child_x, child_y);
                }
            };
            collect_ids(*layout_root, 0, 0);
        }
        // Collect element regions for JS event hit-testing.
        // Walk the layout tree to map each element's bounding rect to its
        // DOM node pointer, so click events can find the correct SimpleNode.
        if (layout_root) {
            std::function<void(const clever::layout::LayoutNode&, float, float)> collect_regions;
            collect_regions = [&](const clever::layout::LayoutNode& n, float parent_abs_x, float parent_abs_y) {
                float abs_x = parent_abs_x + n.geometry.x;
                float abs_y = parent_abs_y + n.geometry.y;
                if (n.dom_node && !n.is_text) {
                    ElementRegion region;
                    region.bounds.x = abs_x;
                    region.bounds.y = abs_y;
                    region.bounds.width = n.geometry.border_box_width();
                    region.bounds.height = n.geometry.border_box_height();
                    region.dom_node = n.dom_node;
                    // Propagate pointer-events: none so hit-testing can skip this region.
                    region.pointer_events = n.pointer_events;
                    result.element_regions.push_back(region);
                }
                float child_x = abs_x + n.geometry.border.left + n.geometry.padding.left;
                float child_y = abs_y + n.geometry.border.top + n.geometry.padding.top;
                for (auto& c : n.children) {
                    collect_regions(*c, child_x, child_y);
                }
            };
            collect_regions(*layout_root, 0, 0);
        }

        result.root = std::move(layout_root);
        result.forms = std::move(collected_forms);
        result.datalists = std::move(collected_datalists);

        // Persist the JS engine and DOM tree for interactive event dispatch.
        // The DOM tree must outlive the JS engine (DOM bindings reference it).
        if (js_engine_ptr) {
            result.js_engine = std::move(js_engine_ptr);
            result.dom_tree = std::move(doc);
        }

        result.success = true;

    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
    } catch (...) {
        result.error = "Unknown exception";
    }

    return result;
}

// ---------------------------------------------------------------------------
// Public API: fetch and decode an image for the JS Image element
// ---------------------------------------------------------------------------
JSImageData fetch_image_for_js(const std::string& url) {
    JSImageData out;
    if (url.empty()) return out;
    DecodedImage img = fetch_and_decode_image(url);
    if (img.pixels && !img.pixels->empty() && img.width > 0 && img.height > 0) {
        out.pixels = img.pixels;
        out.width  = img.width;
        out.height = img.height;
    }
    return out;
}

} // namespace clever::paint
