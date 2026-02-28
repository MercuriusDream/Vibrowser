#pragma once
#include <clever/paint/display_list.h>
#include <clever/css/style/computed_style.h>
#include <clever/css/parser/stylesheet.h>
#include <clever/layout/box.h>
#include <clever/js/js_engine.h>
#include <clever/js/js_dom_bindings.h>
#include <clever/html/tree_builder.h>
#include <clever/paint/animation_controller.h>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace clever::paint {

class SoftwareRenderer;

// Tracks the runtime state of a single CSS transition animation
struct TransitionState {
    std::string property;       // e.g., "opacity", "background-color"
    float start_value = 0.0f;   // starting value
    float end_value = 0.0f;     // target value
    float current_value = 0.0f; // interpolated value
    float duration_ms = 0.0f;   // duration in milliseconds
    float elapsed_ms = 0.0f;    // elapsed time
    bool active = false;        // whether transition is running
};

struct FormField {
    std::string name;
    std::string value;
    std::string type; // "text", "hidden", "password", "checkbox", "radio", etc.
    bool checked = false; // for checkbox/radio
};

struct FormData {
    std::string action; // form action URL
    std::string method; // "GET" or "POST"
    std::string enctype; // "application/x-www-form-urlencoded" or "multipart/form-data"
    std::vector<FormField> fields;
};

struct RenderResult {
    // Clean up DOM bindings before the JSEngine is destroyed.
    ~RenderResult() {
        if (js_engine && js_engine->context()) {
            clever::js::cleanup_dom_bindings(js_engine->context());
        }
    }
    // Move-only (destructor suppresses implicit move ops)
    RenderResult() = default;
    RenderResult(RenderResult&&) = default;
    RenderResult& operator=(RenderResult&&) = default;

    std::unique_ptr<SoftwareRenderer> renderer;
    int width, height;
    bool success;
    std::string error;
    std::string page_title;
    std::string favicon_url; // URL of <link rel="icon"> or /favicon.ico fallback
    std::vector<LinkRegion> links;
    std::vector<CursorRegion> cursor_regions;
    std::vector<PaintCommand> text_commands; // DrawText commands for text selection
    std::vector<clever::css::KeyframesDefinition> keyframes; // parsed @keyframes definitions
    std::unordered_map<std::string, clever::css::KeyframeAnimation> keyframe_animations; // name -> animation map
    std::vector<clever::css::FontFaceRule> font_faces;      // parsed @font-face rules
    std::vector<TransitionState> active_transitions;         // active CSS transition animations
    std::unique_ptr<AnimationController> animation_controller; // CSS animations/transitions runtime
    std::unique_ptr<clever::layout::LayoutNode> root;       // layout tree root (for inspection)
    std::vector<FormData> forms;                             // collected form data for submission
    std::vector<FormSubmitRegion> form_submit_regions;       // clickable regions for submit buttons
    std::vector<DetailsToggleRegion> details_toggle_regions;  // clickable <summary> regions for toggling <details>
    std::vector<SelectClickRegion> select_click_regions;     // clickable <select> dropdown regions
    std::unordered_map<std::string, std::vector<std::string>> datalists; // datalist options keyed by id
    std::unordered_map<std::string, float> id_positions; // element id -> Y position in page coords
    uint32_t selection_color = 0;    // CSS ::selection color (0 = use system default)
    uint32_t selection_bg_color = 0; // CSS ::selection background-color (0 = use system default)
    int meta_refresh_delay = -1;     // <meta http-equiv="refresh"> delay in seconds (-1 = none)
    std::string meta_refresh_url;    // <meta http-equiv="refresh"> target URL (empty = reload current)
    std::vector<std::string> js_console_output; // JavaScript console.log/warn/error output
    std::vector<std::string> js_errors;         // JavaScript runtime errors

    // Element hit-test regions for dispatching JS click events.
    // Each region maps a rendered bounding rectangle to the DOM node (SimpleNode*)
    // that occupies it. Iterated in reverse for z-order (last = topmost).
    std::vector<ElementRegion> element_regions;

    // Persisted JS engine and DOM tree for interactive event dispatch.
    // These survive after render_html() returns so that click events can be
    // dispatched to JS handlers registered via addEventListener.
    // The js_engine owns the JSContext; dom_tree owns the SimpleNode tree.
    // Both must be kept alive together (DOM bindings reference the tree).
    // IMPORTANT: dom_tree must be declared AFTER js_engine so that during
    // destruction, js_engine (and its JSContext) is destroyed first, then
    // dom_tree. This ensures the DOM tree remains valid while the JS context
    // cleans up its references to DOM nodes.
    std::unique_ptr<clever::html::SimpleNode> dom_tree;
    std::unique_ptr<clever::js::JSEngine> js_engine;
};

// Render HTML string to pixels
RenderResult render_html(const std::string& html, int viewport_width = 800, int viewport_height = 600);

// Render HTML with a base URL for resolving relative links (CSS, images)
RenderResult render_html(const std::string& html, const std::string& base_url,
                         int viewport_width = 800, int viewport_height = 600);

// Render HTML with toggle state for interactive <details> elements
// toggled_details: set of details_id values whose open state should be flipped
RenderResult render_html(const std::string& html, const std::string& base_url,
                         int viewport_width, int viewport_height,
                         const std::set<int>& toggled_details);

// Evaluate a @supports condition string (exposed for testing)
bool evaluate_supports_condition(const std::string& condition);

// Flatten @supports rules into the main stylesheet rules (exposed for testing)
void flatten_supports_rules(clever::css::StyleSheet& sheet);

// Extract preferred @font-face URL from a src descriptor list (exposed for testing)
std::string extract_preferred_font_url(const std::string& src);

// Decode a data: URL payload for @font-face font sources (exposed for testing)
std::optional<std::vector<uint8_t>> decode_font_data_url(const std::string& url);

// CSS Transition easing functions (exposed for testing)
float ease_linear(float t);
float ease_ease(float t);
float ease_in(float t);
float ease_out(float t);
float ease_in_out(float t);
float apply_easing(int timing_function, float t);
float apply_easing_custom(int timing_function, float t,
                          float bx1, float by1, float bx2, float by2,
                          int steps_count);

// CSS Transition interpolation functions (exposed for testing)
float interpolate_float(float from, float to, float t);
clever::css::Color interpolate_color(const clever::css::Color& from,
                                      const clever::css::Color& to, float t);
clever::css::Transform interpolate_transform(const clever::css::Transform& from,
                                              const clever::css::Transform& to, float t);

} // namespace clever::paint
