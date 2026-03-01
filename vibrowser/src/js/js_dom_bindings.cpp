#include <clever/js/js_dom_bindings.h>
#include <clever/layout/box.h>
#include <clever/net/cookie_jar.h>
#include <clever/url/url.h>

extern "C" {
#include <quickjs.h>
}

#include <clever/css/parser/selector.h>
#include <clever/css/style/selector_matcher.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#include <clever/paint/canvas_text_bridge.h>
#endif

namespace clever::js {

namespace {

// =========================================================================
// Class IDs for custom JS objects
// =========================================================================

static JSClassID element_class_id = 0;
static JSClassID style_class_id = 0;
static JSClassID mutation_observer_class_id = 0;
static JSClassID intersection_observer_class_id = 0;
static JSClassID resize_observer_class_id = 0;
static JSClassID canvas2d_class_id = 0;
static JSClassID url_class_id = 0;
static JSClassID text_encoder_class_id = 0;
static JSClassID text_decoder_class_id = 0;
// range_class_id and selection_class_id reserved for future Range/Selection API

// =========================================================================
// Per-context state for DOM bindings
// =========================================================================

// Per-listener entry: stores the JS callback and whether it's a capture listener
struct EventListenerEntry {
    JSValue handler;
    bool use_capture;
    bool once = false;
    bool passive = false;
};

struct DOMState {
    clever::html::SimpleNode* root = nullptr;
    bool modified = false;
    std::string title;
    bool title_set = false;
    // Owned nodes created by createElement/createTextNode that have not yet
    // been attached to the tree.  Once appendChild moves them into the tree
    // the unique_ptr is released from here.
    std::vector<std::unique_ptr<clever::html::SimpleNode>> owned_nodes;
    // Event listeners: map from SimpleNode* -> event type -> list of listener entries
    std::unordered_map<clever::html::SimpleNode*,
                       std::unordered_map<std::string, std::vector<EventListenerEntry>>> listeners;
    // The context that owns this state (for freeing listener JSValues)
    JSContext* ctx = nullptr;
    // document.cookie storage: name -> value
    std::map<std::string, std::string> cookies;

    // Layout geometry cache: SimpleNode* -> absolute position box geometry + computed style
    struct LayoutRect {
        float x = 0, y = 0, width = 0, height = 0;
        float border_left = 0, border_top = 0, border_right = 0, border_bottom = 0;
        float padding_left = 0, padding_top = 0, padding_right = 0, padding_bottom = 0;
        float margin_left = 0, margin_top = 0, margin_right = 0, margin_bottom = 0;
        // Absolute border-box origin (top-left of border edge, in page coordinates)
        float abs_border_x = 0, abs_border_y = 0;
        // Scroll container data
        float scroll_top = 0, scroll_left = 0;
        float scroll_content_width = 0, scroll_content_height = 0;
        bool is_scroll_container = false;
        // Hit-testing flags
        int pointer_events = 0;         // 0=auto, 1=none
        bool visibility_hidden = false; // CSS visibility:hidden
        // CSS position_type: 0=static, 1=relative, 2=absolute, 3=fixed, 4=sticky
        int position_type = 0;
        // Parent DOM node for offsetParent traversal
        void* parent_dom_node = nullptr;

        // ---- Computed CSS style properties (from LayoutNode) ----
        // Display / flow
        int display_type = 0;        // 0=block,1=inline,2=inline-block,3=flex,4=inline-flex,5=none,6=list-item,7=table,8=table-row,9=table-cell,10=grid,11=inline-grid
        int float_type = 0;          // 0=none, 1=left, 2=right
        int clear_type = 0;          // 0=none, 1=left, 2=right, 3=both
        bool border_box = false;     // box-sizing: true=border-box, false=content-box

        // Sizing constraints (px; -1 = auto/not set)
        float specified_width = -1;
        float specified_height = -1;
        float min_width_val = 0;
        float max_width_val = 1e9f;
        float min_height_val = 0;
        float max_height_val = 1e9f;

        // Typography
        float font_size = 16.0f;
        int font_weight = 400;
        bool font_italic = false;
        std::string font_family;
        float line_height_px = 0;          // 0 = "normal"
        float line_height_unitless = 1.2f; // factor when line_height_px == 0

        // Colors (ARGB uint32_t)
        uint32_t color = 0xFF000000;
        uint32_t background_color = 0x00000000;

        // Background
        std::string bg_image_url;
        int gradient_type = 0;       // 0=none, 1=linear, 2=radial

        // Visual
        float opacity_val = 1.0f;
        int overflow_x_val = 0;      // 0=visible, 1=hidden, 2=scroll, 3=auto
        int overflow_y_val = 0;
        int z_index_val = 0;
        bool z_index_auto = true;

        // Text properties
        int text_align_val = 0;      // 0=left, 1=center, 2=right, 3=justify
        int text_decoration_bits = 0; // 1=underline, 2=overline, 4=line-through
        int white_space_val = 0;     // 0=normal, 1=nowrap, 2=pre, 3=pre-wrap, 4=pre-line, 5=break-spaces
        int word_break_val = 0;      // 0=normal, 1=break-all, 2=keep-all
        int overflow_wrap_val = 0;   // 0=normal, 1=break-word, 2=anywhere
        int text_transform_val = 0;  // 0=none, 1=capitalize, 2=uppercase, 3=lowercase
        int text_overflow_val = 0;   // 0=clip, 1=ellipsis

        // Flex properties
        float flex_grow = 0;
        float flex_shrink = 1;
        float flex_basis = -1;       // -1 = auto
        int flex_direction = 0;      // 0=row, 1=row-reverse, 2=column, 3=column-reverse
        int flex_wrap_val = 0;       // 0=nowrap, 1=wrap, 2=wrap-reverse
        int justify_content_val = 0; // 0=flex-start,1=flex-end,2=center,3=space-between,4=space-around,5=space-evenly
        int align_items_val = 4;     // 0=flex-start,1=flex-end,2=center,3=baseline,4=stretch
        int align_self_val = -1;     // -1=auto, 0-4 same as align_items

        // Border radius (px)
        float border_radius_tl = 0;
        float border_radius_tr = 0;
        float border_radius_bl = 0;
        float border_radius_br = 0;

        // Border styles per side: 0=none, 1=solid, 2=dashed, 3=dotted, 4=double
        int border_style_top = 0;
        int border_style_right = 0;
        int border_style_bottom = 0;
        int border_style_left = 0;

        // Border colors per side (ARGB)
        uint32_t border_color_top = 0xFF000000;
        uint32_t border_color_right = 0xFF000000;
        uint32_t border_color_bottom = 0xFF000000;
        uint32_t border_color_left = 0xFF000000;

        // CSS Transforms (copied from LayoutNode)
        std::vector<clever::css::Transform> transforms;

        // Cursor / pointer-events / user-select
        int cursor_val = 0;          // 0=auto, 1=default, 2=pointer, 3=text, 4=move, 5=not-allowed
        int user_select_val = 0;     // 0=auto, 1=none, 2=text, 3=all
    };
    std::unordered_map<void*, LayoutRect> layout_geometry;

    // IntersectionObserver registry
    struct IntersectionObserverEntry {
        JSValue observer_obj;   // the IntersectionObserver JS object
        JSValue callback;       // the callback function
        std::vector<clever::html::SimpleNode*> observed_elements;
        float root_margin_top = 0, root_margin_right = 0;
        float root_margin_bottom = 0, root_margin_left = 0;
        std::vector<float> thresholds;  // default [0]
    };
    std::vector<IntersectionObserverEntry> intersection_observers;

    // ResizeObserver registry
    struct ResizeObserverEntry {
        JSValue observer_obj;   // the ResizeObserver JS object
        JSValue callback;       // the callback function
        std::vector<clever::html::SimpleNode*> observed_elements;
        std::unordered_map<clever::html::SimpleNode*, std::pair<float, float>> previous_sizes;
    };
    std::vector<ResizeObserverEntry> resize_observers;

    // Shadow DOM: element -> shadow root node
    std::unordered_map<clever::html::SimpleNode*, clever::html::SimpleNode*> shadow_roots;
    // Shadow DOM: closed shadow roots (not accessible via shadowRoot getter)
    std::unordered_set<clever::html::SimpleNode*> closed_shadow_roots;

    // MutationObserver support
    struct MutationObserverEntry {
        JSValue observer_obj;   // the MutationObserver JS object
        JSValue callback;       // the callback function
        std::vector<clever::html::SimpleNode*> observed_targets;  // targets being observed
        bool watch_child_list = false;
        bool watch_attributes = false;
        bool watch_character_data = false;
        bool watch_subtree = false;
        bool record_attribute_old_value = false;
        bool record_character_data_old_value = false;
        std::vector<std::string> attribute_filter;  // empty means all attributes
        std::unordered_map<clever::html::SimpleNode*, std::unordered_map<std::string, std::string>> old_attribute_values;
    };
    std::vector<MutationObserverEntry> mutation_observers;

    // Pending mutations to be delivered asynchronously
    struct PendingMutation {
        JSValue observer_obj;    // which observer to call
        JSValue callback;        // cached callback
        std::vector<JSValue> mutation_records;  // array of MutationRecord objects
    };
    std::vector<DOMState::PendingMutation> pending_mutations;

    int viewport_width = 800, viewport_height = 600;

    // Focus tracking: pointer to the currently focused SimpleNode (may be nullptr)
    clever::html::SimpleNode* focused_element = nullptr;
};

struct URLState {
    clever::url::URL parsed_url;
};

struct TextEncoderState {
};

struct TextDecoderState {
    std::string encoding = "utf-8";
};

struct RangeState {
    JSValue startContainer = JS_UNDEFINED;
    int startOffset = 0;
    JSValue endContainer = JS_UNDEFINED;
    int endOffset = 0;
};

struct SelectionState {
    RangeState range;
    JSValue anchorNode = JS_NULL;
    int anchorOffset = 0;
    JSValue focusNode = JS_NULL;
    int focusOffset = 0;
};

// Forward declaration for MutationObserver notification (defined later)
static void notify_mutation_observers(JSContext* ctx,
                                      DOMState* state,
                                      const std::string& type,
                                      clever::html::SimpleNode* target,
                                      const std::vector<clever::html::SimpleNode*>& added_nodes,
                                      const std::vector<clever::html::SimpleNode*>& removed_nodes,
                                      clever::html::SimpleNode* previous_sibling,
                                      clever::html::SimpleNode* next_sibling,
                                      const std::string& attr_name = "",
                                      const std::string& old_value = "");

static void js_url_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<URLState*>(JS_GetOpaque(val, url_class_id));
    if (state) delete state;
}

static JSClassDef url_class_def = {
    "URL",
    js_url_finalizer,
    nullptr, nullptr, nullptr
};

static void js_text_encoder_finalizer(JSRuntime* /*rt*/, JSValue /*val*/) {
}

static JSClassDef text_encoder_class_def = {
    "TextEncoder",
    js_text_encoder_finalizer,
    nullptr, nullptr, nullptr
};

static void js_text_decoder_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<TextDecoderState*>(JS_GetOpaque(val, text_decoder_class_id));
    if (state) delete state;
}

static JSClassDef text_decoder_class_def = {
    "TextDecoder",
    js_text_decoder_finalizer,
    nullptr, nullptr, nullptr
};

// Retrieve the DOMState stashed in the global object.
static DOMState* get_dom_state(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, "__dom_state_ptr");
    DOMState* state = nullptr;
    if (JS_IsNumber(val)) {
        int64_t ptr_val = 0;
        JS_ToInt64(ctx, &ptr_val, val);
        state = reinterpret_cast<DOMState*>(static_cast<uintptr_t>(ptr_val));
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return state;
}

// =========================================================================
// SimpleNode attribute helpers
// =========================================================================

static std::string get_attr(const clever::html::SimpleNode& node,
                            const std::string& name) {
    for (const auto& attr : node.attributes) {
        if (attr.name == name) return attr.value;
    }
    return "";
}

static bool has_attr(const clever::html::SimpleNode& node,
                     const std::string& name) {
    for (const auto& attr : node.attributes) {
        if (attr.name == name) return true;
    }
    return false;
}

static void set_attr(clever::html::SimpleNode& node,
                     const std::string& name, const std::string& value) {
    for (auto& attr : node.attributes) {
        if (attr.name == name) {
            attr.value = value;
            return;
        }
    }
    node.attributes.push_back({name, value});
}

static void remove_attr(clever::html::SimpleNode& node,
                        const std::string& name) {
    auto& attrs = node.attributes;
    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        if (it->name == name) {
            attrs.erase(it);
            return;
        }
    }
}

// =========================================================================
// Tree search helpers
// =========================================================================

static constexpr int kMaxDOMSearchDepth = 512;

static clever::html::SimpleNode* find_by_id(clever::html::SimpleNode* node,
                                             const std::string& id,
                                             int depth = 0) {
    if (!node || depth >= kMaxDOMSearchDepth) return nullptr;
    if (node->type == clever::html::SimpleNode::Element &&
        get_attr(*node, "id") == id) {
        return node;
    }
    for (auto& child : node->children) {
        auto* found = find_by_id(child.get(), id, depth + 1);
        if (found) return found;
    }
    return nullptr;
}

static void find_by_tag(clever::html::SimpleNode* node,
                         const std::string& tag,
                         std::vector<clever::html::SimpleNode*>& results,
                         int depth = 0) {
    if (!node || depth >= kMaxDOMSearchDepth) return;
    if (node->type == clever::html::SimpleNode::Element &&
        node->tag_name == tag) {
        results.push_back(node);
    }
    for (auto& child : node->children) {
        find_by_tag(child.get(), tag, results, depth + 1);
    }
}

static void find_by_class(clever::html::SimpleNode* node,
                           const std::string& cls,
                           std::vector<clever::html::SimpleNode*>& results,
                           int depth = 0) {
    if (!node || depth >= kMaxDOMSearchDepth) return;
    if (node->type == clever::html::SimpleNode::Element) {
        std::string classes = get_attr(*node, "class");
        // Whitespace-delimited class check
        size_t pos = 0;
        while (pos < classes.size()) {
            size_t end = classes.find(' ', pos);
            if (end == std::string::npos) end = classes.size();
            if (classes.substr(pos, end - pos) == cls) {
                results.push_back(node);
                break;
            }
            pos = end + 1;
        }
    }
    for (auto& child : node->children) {
        find_by_class(child.get(), cls, results, depth + 1);
    }
}

// Basic querySelector implementation: "#id", ".class", or "tag"
[[maybe_unused]] static clever::html::SimpleNode* query_selector_impl(
        clever::html::SimpleNode* root, const std::string& selector) {
    if (selector.empty()) return nullptr;

    if (selector[0] == '#') {
        return find_by_id(root, selector.substr(1));
    } else if (selector[0] == '.') {
        std::vector<clever::html::SimpleNode*> results;
        find_by_class(root, selector.substr(1), results);
        return results.empty() ? nullptr : results[0];
    } else {
        std::vector<clever::html::SimpleNode*> results;
        find_by_tag(root, selector, results);
        return results.empty() ? nullptr : results[0];
    }
}

[[maybe_unused]] static void query_selector_all_impl(
        clever::html::SimpleNode* root, const std::string& selector,
        std::vector<clever::html::SimpleNode*>& results) {
    if (selector.empty()) return;

    if (selector[0] == '#') {
        auto* elem = find_by_id(root, selector.substr(1));
        if (elem) results.push_back(elem);
    } else if (selector[0] == '.') {
        find_by_class(root, selector.substr(1), results);
    } else {
        find_by_tag(root, selector, results);
    }
}

// =========================================================================
// Wrap / unwrap SimpleNode <-> JS Element proxy
// =========================================================================

static JSValue wrap_element(JSContext* ctx, clever::html::SimpleNode* node) {
    if (!node) return JS_NULL;
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(element_class_id));
    if (JS_IsException(obj)) return JS_NULL;
    JS_SetOpaque(obj, node);
    return obj;
}

static clever::html::SimpleNode* unwrap_element(JSContext* /*ctx*/,
                                                  JSValueConst val) {
    return static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(val, element_class_id));
}

// =========================================================================
// Element property getters / setters  (all JSCFunction signature)
// =========================================================================

// --- element.id (getter) ---
static JSValue js_element_get_id(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, get_attr(*node, "id").c_str());
}

// --- element.tagName (getter) ---
static JSValue js_element_get_tag_name(JSContext* ctx, JSValueConst this_val,
                                        int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    std::string tag = node->tag_name;
    std::transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
    return JS_NewString(ctx, tag.c_str());
}

// --- element.className (getter) ---
static JSValue js_element_get_class_name(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, get_attr(*node, "class").c_str());
}

// --- element.className = "..." (setter, called as __setClassName(value)) ---
static JSValue js_element_set_class_name(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
        set_attr(*node, "class", str);
        JS_FreeCString(ctx, str);
        auto* state = get_dom_state(ctx);
        if (state) state->modified = true;
    }
    return JS_UNDEFINED;
}

// --- element.textContent (getter) ---
static JSValue js_element_get_text_content(JSContext* ctx, JSValueConst this_val,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, node->text_content().c_str());
}

// --- element.textContent = "..." (setter, called as __setTextContent(value)) ---
static JSValue js_element_set_text_content(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
        node->children.clear();
        auto text_node = std::make_unique<clever::html::SimpleNode>();
        text_node->type = clever::html::SimpleNode::Text;
        text_node->data = str;
        text_node->parent = node;
        node->children.push_back(std::move(text_node));
        JS_FreeCString(ctx, str);
        auto* state = get_dom_state(ctx);
        if (state) state->modified = true;
    }
    return JS_UNDEFINED;
}

// Forward-declare serialize_node (defined later near outerHTML getter)
static std::string serialize_node(const clever::html::SimpleNode* node);

// Forward-declare focus management helper (defined after event dispatch infrastructure)
static void do_focus_element(JSContext* ctx, DOMState* state,
                              clever::html::SimpleNode* new_focus,
                              clever::html::SimpleNode* related = nullptr);
static void do_blur_element(JSContext* ctx, DOMState* state,
                             clever::html::SimpleNode* element,
                             clever::html::SimpleNode* related = nullptr);

// --- element.innerHTML (getter) ---
static JSValue js_element_get_inner_html(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    // Recursively serialize all children using the full serialize_node helper
    std::string html;
    for (auto& child : node->children) {
        html += serialize_node(child.get());
    }
    return JS_NewString(ctx, html.c_str());
}

// --- element.innerHTML = "..." (setter, called as __setInnerHTML(value)) ---
static JSValue js_element_set_inner_html(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
        auto parsed = clever::html::parse(str);
        node->children.clear();
        if (parsed) {
            // The parser wraps content in <html><body>..., find body
            auto* body = parsed->find_element("body");
            auto* source = body ? body : parsed.get();
            for (auto& child : source->children) {
                child->parent = node;
                node->children.push_back(std::move(child));
            }
        }
        JS_FreeCString(ctx, str);
        auto* state = get_dom_state(ctx);
        if (state) state->modified = true;
    }
    return JS_UNDEFINED;
}

// --- element.children (getter, returns array of child elements) ---
static JSValue js_element_get_children(JSContext* ctx, JSValueConst this_val,
                                        int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NewArray(ctx);
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (auto& child : node->children) {
        if (child->type == clever::html::SimpleNode::Element) {
            JS_SetPropertyUint32(ctx, arr, idx++,
                                 wrap_element(ctx, child.get()));
        }
    }
    return arr;
}

// --- element.childNodes (getter, returns array of ALL child nodes) ---
static JSValue js_element_get_child_nodes(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NewArray(ctx);
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (auto& child : node->children) {
        JS_SetPropertyUint32(ctx, arr, idx++,
                             wrap_element(ctx, child.get()));
    }
    return arr;
}

// --- element.parentNode / element.parentElement (getter) ---
static JSValue js_element_get_parent(JSContext* ctx, JSValueConst this_val,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_NULL;
    return wrap_element(ctx, node->parent);
}

// --- element.firstChild (getter) ---
static JSValue js_element_get_first_child(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || node->children.empty()) return JS_NULL;
    return wrap_element(ctx, node->children.front().get());
}

// --- element.lastChild (getter) ---
static JSValue js_element_get_last_child(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || node->children.empty()) return JS_NULL;
    return wrap_element(ctx, node->children.back().get());
}

// --- element.firstElementChild (getter) ---
static JSValue js_element_get_first_element_child(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;
    for (auto& child : node->children) {
        if (child->type == clever::html::SimpleNode::Element)
            return wrap_element(ctx, child.get());
    }
    return JS_NULL;
}

// --- element.lastElementChild (getter) ---
static JSValue js_element_get_last_element_child(JSContext* ctx,
                                                   JSValueConst this_val,
                                                   int /*argc*/,
                                                   JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;
    for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
        if ((*it)->type == clever::html::SimpleNode::Element)
            return wrap_element(ctx, it->get());
    }
    return JS_NULL;
}

// --- Sibling helpers: find position in parent's children ---
static int find_sibling_index(const clever::html::SimpleNode* node) {
    if (!node || !node->parent) return -1;
    auto& siblings = node->parent->children;
    for (size_t i = 0; i < siblings.size(); i++) {
        if (siblings[i].get() == node) return static_cast<int>(i);
    }
    return -1;
}

// --- element.nextSibling (getter) ---
static JSValue js_element_get_next_sibling(JSContext* ctx, JSValueConst this_val,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_NULL;
    int idx = find_sibling_index(node);
    if (idx < 0) return JS_NULL;
    auto& siblings = node->parent->children;
    size_t next = static_cast<size_t>(idx) + 1;
    if (next < siblings.size())
        return wrap_element(ctx, siblings[next].get());
    return JS_NULL;
}

// --- element.previousSibling (getter) ---
static JSValue js_element_get_previous_sibling(JSContext* ctx,
                                                JSValueConst this_val,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_NULL;
    int idx = find_sibling_index(node);
    if (idx <= 0) return JS_NULL;
    auto& siblings = node->parent->children;
    return wrap_element(ctx, siblings[static_cast<size_t>(idx) - 1].get());
}

// --- element.nextElementSibling (getter) ---
static JSValue js_element_get_next_element_sibling(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_NULL;
    int idx = find_sibling_index(node);
    if (idx < 0) return JS_NULL;
    auto& siblings = node->parent->children;
    for (size_t i = static_cast<size_t>(idx) + 1; i < siblings.size(); i++) {
        if (siblings[i]->type == clever::html::SimpleNode::Element)
            return wrap_element(ctx, siblings[i].get());
    }
    return JS_NULL;
}

// --- element.previousElementSibling (getter) ---
static JSValue js_element_get_previous_element_sibling(JSContext* ctx,
                                                        JSValueConst this_val,
                                                        int /*argc*/,
                                                        JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_NULL;
    int idx = find_sibling_index(node);
    if (idx <= 0) return JS_NULL;
    auto& siblings = node->parent->children;
    for (int i = idx - 1; i >= 0; i--) {
        if (siblings[static_cast<size_t>(i)]->type == clever::html::SimpleNode::Element)
            return wrap_element(ctx, siblings[static_cast<size_t>(i)].get());
    }
    return JS_NULL;
}

// --- element.childElementCount (getter) ---
static JSValue js_element_get_child_element_count(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NewInt32(ctx, 0);
    int count = 0;
    for (auto& child : node->children) {
        if (child->type == clever::html::SimpleNode::Element) count++;
    }
    return JS_NewInt32(ctx, count);
}

// --- element.nodeType (getter) ---
// Returns 1 for Element, 3 for Text, 8 for Comment, 9 for Document
static JSValue js_element_get_node_type(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    switch (node->type) {
        case clever::html::SimpleNode::Element:     return JS_NewInt32(ctx, 1);
        case clever::html::SimpleNode::Text:        return JS_NewInt32(ctx, 3);
        case clever::html::SimpleNode::Comment:     return JS_NewInt32(ctx, 8);
        case clever::html::SimpleNode::Document:    return JS_NewInt32(ctx, 9);
        case clever::html::SimpleNode::DocumentType: return JS_NewInt32(ctx, 10);
        default: return JS_NewInt32(ctx, 1);
    }
}

// --- element.nodeName (getter) ---
// Returns uppercase tag name for Elements, "#text" for Text, etc.
static JSValue js_element_get_node_name(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    switch (node->type) {
        case clever::html::SimpleNode::Element: {
            std::string tag = node->tag_name;
            std::transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
            return JS_NewString(ctx, tag.c_str());
        }
        case clever::html::SimpleNode::Text:
            return JS_NewString(ctx, "#text");
        case clever::html::SimpleNode::Comment:
            return JS_NewString(ctx, "#comment");
        case clever::html::SimpleNode::Document:
            return JS_NewString(ctx, "#document");
        default:
            return JS_NewString(ctx, "");
    }
}

// =========================================================================
// CSS Selector matching bridge (connects JS DOM to real CSS selector engine)
// =========================================================================

// Helper: lowercase a string
static std::string to_lower_str(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Build a full ElementView tree from a SimpleNode and its ancestors.
// This builds views for the node and all ancestors so combinators work.
// It also builds sibling views (prev_sibling chain) and children for :has().
//
// The returned pointer is valid as long as `storage` is alive.
static clever::css::ElementView* build_element_view_chain(
        clever::html::SimpleNode* node,
        std::vector<std::unique_ptr<clever::css::ElementView>>& storage) {
    if (!node || node->type != clever::html::SimpleNode::Element)
        return nullptr;

    // Collect ancestor chain (from root to node) so we build top-down
    std::vector<clever::html::SimpleNode*> chain;
    for (auto* n = node; n != nullptr; n = n->parent) {
        if (n->type == clever::html::SimpleNode::Element)
            chain.push_back(n);
    }
    std::reverse(chain.begin(), chain.end());

    // Map from SimpleNode* -> ElementView* so we can wire parents/siblings
    std::unordered_map<clever::html::SimpleNode*, clever::css::ElementView*> view_map;

    clever::css::ElementView* target_view = nullptr;

    for (auto* sn : chain) {
        auto view = std::make_unique<clever::css::ElementView>();
        view->tag_name = to_lower_str(sn->tag_name);
        view->id = get_attr(*sn, "id");

        // Parse class attribute
        std::string class_attr = get_attr(*sn, "class");
        if (!class_attr.empty()) {
            std::istringstream iss(class_attr);
            std::string cls;
            while (iss >> cls) {
                view->classes.push_back(cls);
            }
        }

        // Copy all attributes
        for (auto& attr : sn->attributes) {
            view->attributes.emplace_back(attr.name, attr.value);
        }

        // Wire parent
        if (sn->parent && sn->parent->type == clever::html::SimpleNode::Element) {
            auto it = view_map.find(sn->parent);
            if (it != view_map.end()) {
                view->parent = it->second;
            }
        }

        // Compute child_index, sibling_count, same_type info, prev_sibling
        if (sn->parent) {
            size_t elem_count = 0;
            size_t same_type_idx = 0;
            size_t same_type_cnt = 0;
            clever::html::SimpleNode* prev_elem = nullptr;

            for (auto& sibling : sn->parent->children) {
                if (sibling->type != clever::html::SimpleNode::Element)
                    continue;
                if (sibling.get() == sn) {
                    view->child_index = elem_count;
                    same_type_idx = same_type_cnt;
                    if (prev_elem) {
                        auto pit = view_map.find(prev_elem);
                        if (pit != view_map.end())
                            view->prev_sibling = pit->second;
                    }
                }
                if (to_lower_str(sibling->tag_name) == view->tag_name) {
                    if (sibling.get() == sn)
                        same_type_idx = same_type_cnt;
                    same_type_cnt++;
                }
                elem_count++;
                if (sibling.get() != sn)
                    prev_elem = sibling.get();
                else
                    prev_elem = nullptr; // don't let node be its own prev
            }
            view->sibling_count = elem_count;
            view->same_type_index = same_type_idx;
            view->same_type_count = same_type_cnt;
        }

        // Count element children and text children (for :empty)
        size_t child_elem_count = 0;
        bool has_text = false;
        for (auto& child : sn->children) {
            if (child->type == clever::html::SimpleNode::Element) {
                child_elem_count++;
            } else if (child->type == clever::html::SimpleNode::Text) {
                // CSS :empty treats any text node as content, including whitespace-only text.
                has_text = true;
            }
        }
        view->child_element_count = child_elem_count;
        view->has_text_children = has_text;

        auto* ptr = view.get();
        view_map[sn] = ptr;
        if (sn == node) target_view = ptr;
        storage.push_back(std::move(view));
    }

    // Also build sibling views (non-ancestors) that may be needed for the
    // prev_sibling chain at the node level.  We need views for all element
    // siblings of `node` that come before it.
    if (node->parent) {
        clever::css::ElementView* prev_sib_view = nullptr;
        for (auto& sibling : node->parent->children) {
            if (sibling->type != clever::html::SimpleNode::Element)
                continue;
            if (sibling.get() == node) break;
            // Check if we already have a view for this sibling
            auto it = view_map.find(sibling.get());
            if (it == view_map.end()) {
                auto sib_view = std::make_unique<clever::css::ElementView>();
                sib_view->tag_name = to_lower_str(sibling->tag_name);
                sib_view->id = get_attr(*sibling, "id");
                std::string cls = get_attr(*sibling, "class");
                if (!cls.empty()) {
                    std::istringstream iss(cls);
                    std::string c;
                    while (iss >> c) sib_view->classes.push_back(c);
                }
                for (auto& attr : sibling->attributes)
                    sib_view->attributes.emplace_back(attr.name, attr.value);
                if (node->parent->type == clever::html::SimpleNode::Element) {
                    auto pit = view_map.find(node->parent);
                    if (pit != view_map.end()) sib_view->parent = pit->second;
                }
                sib_view->prev_sibling = prev_sib_view;
                auto* sp = sib_view.get();
                view_map[sibling.get()] = sp;
                storage.push_back(std::move(sib_view));
                prev_sib_view = sp;
            } else {
                prev_sib_view = it->second;
            }
        }
        // Make sure target_view->prev_sibling is correct
        if (target_view && prev_sib_view) {
            target_view->prev_sibling = prev_sib_view;
        }
    }

    return target_view;
}

// Match a SimpleNode against a CSS selector string using the full CSS engine.
static bool node_matches_selector(clever::html::SimpleNode* node,
                                   const std::string& selector_str) {
    if (!node || node->type != clever::html::SimpleNode::Element)
        return false;
    if (selector_str.empty())
        return false;

    // Parse the selector
    auto selector_list = clever::css::parse_selector_list(selector_str);
    if (selector_list.selectors.empty())
        return false;

    // Build ElementView chain for this node
    std::vector<std::unique_ptr<clever::css::ElementView>> storage;
    auto* view = build_element_view_chain(node, storage);
    if (!view) return false;

    // Match against each complex selector in the list
    clever::css::SelectorMatcher matcher;
    for (auto& sel : selector_list.selectors) {
        if (matcher.matches(*view, sel))
            return true;
    }
    return false;
}

// Walk a subtree depth-first and collect all element nodes matching a selector.
// Parses the selector once for efficiency.
static void collect_matching_nodes(
        clever::html::SimpleNode* root,
        const clever::css::SelectorList& selector_list,
        std::vector<clever::html::SimpleNode*>& results) {
    if (!root) return;

    std::function<void(clever::html::SimpleNode*, int)> walk =
        [&](clever::html::SimpleNode* node, int depth) {
            if (depth > 512) return;
            if (node->type == clever::html::SimpleNode::Element) {
                std::vector<std::unique_ptr<clever::css::ElementView>> storage;
                auto* view = build_element_view_chain(node, storage);
                if (view) {
                    clever::css::SelectorMatcher matcher;
                    for (auto& sel : selector_list.selectors) {
                        if (matcher.matches(*view, sel)) {
                            results.push_back(node);
                            break;
                        }
                    }
                }
            }
            for (auto& child : node->children) {
                walk(child.get(), depth + 1);
            }
        };

    // Walk children of root (not root itself for querySelector/All on elements)
    walk(root, 0);
}

// querySelector: find first matching element in subtree (depth-first)
static clever::html::SimpleNode* query_selector_real(
        clever::html::SimpleNode* root,
        const std::string& selector_str) {
    if (!root || selector_str.empty()) return nullptr;

    auto selector_list = clever::css::parse_selector_list(selector_str);
    if (selector_list.selectors.empty()) return nullptr;

    // Depth-first search for first match
    clever::html::SimpleNode* result = nullptr;
    std::function<bool(clever::html::SimpleNode*, int)> walk =
        [&](clever::html::SimpleNode* node, int depth) -> bool {
            if (depth > 512) return false;
            if (node->type == clever::html::SimpleNode::Element) {
                std::vector<std::unique_ptr<clever::css::ElementView>> storage;
                auto* view = build_element_view_chain(node, storage);
                if (view) {
                    clever::css::SelectorMatcher matcher;
                    for (auto& sel : selector_list.selectors) {
                        if (matcher.matches(*view, sel)) {
                            result = node;
                            return true;
                        }
                    }
                }
            }
            for (auto& child : node->children) {
                if (walk(child.get(), depth + 1)) return true;
            }
            return false;
        };

    walk(root, 0);
    return result;
}

// querySelectorAll: find all matching elements in subtree (depth-first)
static void query_selector_all_real(
        clever::html::SimpleNode* root,
        const std::string& selector_str,
        std::vector<clever::html::SimpleNode*>& results) {
    if (!root || selector_str.empty()) return;

    auto selector_list = clever::css::parse_selector_list(selector_str);
    if (selector_list.selectors.empty()) return;

    collect_matching_nodes(root, selector_list, results);
}

// --- element.matches(selector) ---
static JSValue js_element_matches(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;

    const char* sel_cstr = JS_ToCString(ctx, argv[0]);
    if (!sel_cstr) return JS_FALSE;
    std::string sel(sel_cstr);
    JS_FreeCString(ctx, sel_cstr);

    return JS_NewBool(ctx, node_matches_selector(node, sel));
}

// --- element.closest(selector) ---
static JSValue js_element_closest(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;

    const char* sel_cstr = JS_ToCString(ctx, argv[0]);
    if (!sel_cstr) return JS_NULL;
    std::string sel(sel_cstr);
    JS_FreeCString(ctx, sel_cstr);

    // Walk up from self to root
    auto* current = node;
    while (current) {
        if (current->type == clever::html::SimpleNode::Element &&
            node_matches_selector(current, sel)) {
            return wrap_element(ctx, current);
        }
        current = current->parent;
    }
    return JS_NULL;
}

// --- element.querySelector(selector) — scoped to subtree ---
static JSValue js_element_query_selector(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;

    const char* sel_cstr = JS_ToCString(ctx, argv[0]);
    if (!sel_cstr) return JS_NULL;
    std::string sel(sel_cstr);
    JS_FreeCString(ctx, sel_cstr);

    // Search within this element's children (not including self)
    auto selector_list = clever::css::parse_selector_list(sel);
    if (selector_list.selectors.empty()) return JS_NULL;

    // Depth-first search among children
    clever::html::SimpleNode* result = nullptr;
    std::function<bool(clever::html::SimpleNode*)> walk =
        [&](clever::html::SimpleNode* n) -> bool {
            if (n->type == clever::html::SimpleNode::Element) {
                std::vector<std::unique_ptr<clever::css::ElementView>> storage;
                auto* view = build_element_view_chain(n, storage);
                if (view) {
                    clever::css::SelectorMatcher matcher;
                    for (auto& s : selector_list.selectors) {
                        if (matcher.matches(*view, s)) {
                            result = n;
                            return true;
                        }
                    }
                }
            }
            for (auto& child : n->children) {
                if (walk(child.get())) return true;
            }
            return false;
        };

    for (auto& child : node->children) {
        if (walk(child.get())) break;
    }

    return wrap_element(ctx, result);
}

// --- element.querySelectorAll(selector) — scoped to subtree ---
static JSValue js_element_query_selector_all(JSContext* ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_NewArray(ctx);

    const char* sel_cstr = JS_ToCString(ctx, argv[0]);
    if (!sel_cstr) return JS_NewArray(ctx);
    std::string sel(sel_cstr);
    JS_FreeCString(ctx, sel_cstr);

    auto selector_list = clever::css::parse_selector_list(sel);
    if (selector_list.selectors.empty()) return JS_NewArray(ctx);

    std::vector<clever::html::SimpleNode*> results;

    std::function<void(clever::html::SimpleNode*)> walk =
        [&](clever::html::SimpleNode* n) {
            if (n->type == clever::html::SimpleNode::Element) {
                std::vector<std::unique_ptr<clever::css::ElementView>> storage;
                auto* view = build_element_view_chain(n, storage);
                if (view) {
                    clever::css::SelectorMatcher matcher;
                    for (auto& s : selector_list.selectors) {
                        if (matcher.matches(*view, s)) {
                            results.push_back(n);
                            break;
                        }
                    }
                }
            }
            for (auto& child : n->children) {
                walk(child.get());
            }
        };

    for (auto& child : node->children) {
        walk(child.get());
    }

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < results.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             wrap_element(ctx, results[i]));
    }
    return arr;
}

// =========================================================================
// Element methods  (standard JSCFunction signature)
// =========================================================================

// --- element.getAttribute(name) ---
static JSValue js_element_get_attribute(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;

    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return JS_NULL;

    // Save the name string before freeing -- fixes the use-after-free bug
    std::string name_str(name_cstr);
    JS_FreeCString(ctx, name_cstr);

    if (!has_attr(*node, name_str)) return JS_NULL;
    return JS_NewString(ctx, get_attr(*node, name_str).c_str());
}

// --- element.setAttribute(name, value) ---
static JSValue js_element_set_attribute(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* name = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    if (name && value) {
        auto* state = get_dom_state(ctx);

        // Capture old value for mutation record
        std::string old_value;
        if (state) {
            for (auto& entry : state->mutation_observers) {
                if (entry.record_attribute_old_value) {
                    auto it = entry.old_attribute_values[node].find(name);
                    if (it != entry.old_attribute_values[node].end()) {
                        old_value = it->second;
                    }
                }
            }
        }

        set_attr(*node, name, value);
        if (state) {
            state->modified = true;

            // Fire mutation observers
            std::vector<clever::html::SimpleNode*> empty;
            notify_mutation_observers(ctx, state, "attributes", node, empty, empty,
                                    nullptr, nullptr, name, old_value);

            // Update old attribute values for future mutations
            for (auto& entry : state->mutation_observers) {
                if (entry.record_attribute_old_value) {
                    entry.old_attribute_values[node][name] = value;
                }
            }
        }
    }
    if (name) JS_FreeCString(ctx, name);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// --- element.appendChild(child) ---
static JSValue js_element_append_child(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* parent_node = unwrap_element(ctx, this_val);
    auto* child_node = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!parent_node || !child_node) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // DocumentFragment: move its children into parent, not the fragment itself
    if (child_node->tag_name == "#document-fragment") {
        // Remove fragment from owned_nodes if present
        for (auto it = state->owned_nodes.begin();
             it != state->owned_nodes.end(); ++it) {
            if (it->get() == child_node) {
                // Keep the fragment alive temporarily
                auto frag_owned = std::move(*it);
                state->owned_nodes.erase(it);

                // Collect added nodes for mutation
                std::vector<clever::html::SimpleNode*> added_nodes;
                for (auto& frag_child : frag_owned->children) {
                    frag_child->parent = parent_node;
                    added_nodes.push_back(frag_child.get());
                }

                // Calculate previous/next siblings before adding
                clever::html::SimpleNode* prev_sibling = !parent_node->children.empty()
                    ? parent_node->children.back().get() : nullptr;
                clever::html::SimpleNode* next_sibling = nullptr;

                for (auto& frag_child : frag_owned->children) {
                    parent_node->children.push_back(std::move(frag_child));
                }
                frag_owned->children.clear();
                // Keep the empty fragment alive in owned_nodes
                state->owned_nodes.push_back(std::move(frag_owned));
                state->modified = true;

                // Fire mutation for fragment children
                if (!added_nodes.empty()) {
                    std::vector<clever::html::SimpleNode*> empty;
                    notify_mutation_observers(ctx, state, "childList", parent_node,
                                            added_nodes, empty, prev_sibling, next_sibling);
                }

                return wrap_element(ctx, child_node);
            }
        }
        return wrap_element(ctx, child_node);
    }

    // First check: is the child in the owned_nodes list (created by JS,
    // not yet attached to the tree)?
    for (auto it = state->owned_nodes.begin();
         it != state->owned_nodes.end(); ++it) {
        if (it->get() == child_node) {
            auto owned = std::move(*it);
            state->owned_nodes.erase(it);
            owned->parent = parent_node;

            // Calculate siblings
            clever::html::SimpleNode* prev_sibling = !parent_node->children.empty()
                ? parent_node->children.back().get() : nullptr;
            clever::html::SimpleNode* next_sibling = nullptr;

            parent_node->children.push_back(std::move(owned));
            state->modified = true;

            // Fire mutation
            std::vector<clever::html::SimpleNode*> added = { child_node };
            std::vector<clever::html::SimpleNode*> empty;
            notify_mutation_observers(ctx, state, "childList", parent_node,
                                    added, empty, prev_sibling, next_sibling);

            return wrap_element(ctx, child_node);
        }
    }

    // If the child is already in the tree under a different parent, detach
    // it first (move semantics -- transfer ownership).
    if (child_node->parent && child_node->parent != parent_node) {
        auto* old_parent = child_node->parent;
        // Find and detach the unique_ptr from the old parent
        for (auto it = old_parent->children.begin();
             it != old_parent->children.end(); ++it) {
            if (it->get() == child_node) {
                auto owned = std::move(*it);
                old_parent->children.erase(it);
                owned->parent = parent_node;

                // Calculate siblings
                clever::html::SimpleNode* prev_sibling = !parent_node->children.empty()
                    ? parent_node->children.back().get() : nullptr;
                clever::html::SimpleNode* next_sibling = nullptr;

                parent_node->children.push_back(std::move(owned));
                state->modified = true;

                // Fire mutations
                std::vector<clever::html::SimpleNode*> added = { child_node };
                std::vector<clever::html::SimpleNode*> empty;
                notify_mutation_observers(ctx, state, "childList", parent_node,
                                        added, empty, prev_sibling, next_sibling);

                return wrap_element(ctx, child_node);
            }
        }
    }

    return wrap_element(ctx, child_node);
}

// --- element.removeChild(child) ---
static JSValue js_element_remove_child(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* parent_node = unwrap_element(ctx, this_val);
    auto* child_node = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!parent_node || !child_node) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // Detach the child from its parent but keep it alive in owned_nodes
    // so JS references remain valid.
    for (auto it = parent_node->children.begin();
         it != parent_node->children.end(); ++it) {
        if (it->get() == child_node) {
            // Calculate previous and next siblings before removal
            auto idx = std::distance(parent_node->children.begin(), it);
            clever::html::SimpleNode* prev_sibling = nullptr;
            clever::html::SimpleNode* next_sibling = nullptr;

            if (idx > 0) {
                prev_sibling = parent_node->children[idx - 1].get();
            }
            if (idx + 1 < static_cast<int>(parent_node->children.size())) {
                next_sibling = parent_node->children[idx + 1].get();
            }

            auto owned = std::move(*it);
            parent_node->children.erase(it);
            owned->parent = nullptr;
            state->owned_nodes.push_back(std::move(owned));
            state->modified = true;

            // Fire mutation
            std::vector<clever::html::SimpleNode*> removed = { child_node };
            std::vector<clever::html::SimpleNode*> empty;
            notify_mutation_observers(ctx, state, "childList", parent_node,
                                    empty, removed, prev_sibling, next_sibling);

            return wrap_element(ctx, child_node);
        }
    }

    return JS_UNDEFINED;
}

// Helper: extract listener options from addEventListener's third argument.
// Supports both boolean and options-object forms.
struct ListenerOptions {
    bool capture = false;
    bool once = false;
    bool passive = false;
};

static ListenerOptions extract_listener_options(JSContext* ctx, int argc, JSValueConst* argv) {
    ListenerOptions opts;
    if (argc < 3) return opts;
    // Boolean form: addEventListener(type, cb, true)
    if (JS_IsBool(argv[2])) {
        opts.capture = JS_ToBool(ctx, argv[2]) != 0;
        return opts;
    }
    // Options object form: addEventListener(type, cb, {capture: true, once: true, passive: true})
    if (JS_IsObject(argv[2])) {
        JSValue cap = JS_GetPropertyStr(ctx, argv[2], "capture");
        opts.capture = JS_ToBool(ctx, cap) != 0;
        JS_FreeValue(ctx, cap);

        JSValue once = JS_GetPropertyStr(ctx, argv[2], "once");
        opts.once = JS_ToBool(ctx, once) != 0;
        JS_FreeValue(ctx, once);

        JSValue passive = JS_GetPropertyStr(ctx, argv[2], "passive");
        opts.passive = JS_ToBool(ctx, passive) != 0;
        JS_FreeValue(ctx, passive);
    }
    return opts;
}

// Backwards-compatible helper for code that only needs capture flag
static bool extract_capture_flag(JSContext* ctx, int argc, JSValueConst* argv) {
    return extract_listener_options(ctx, argc, argv).capture;
}

// Helper: handle signal option from addEventListener's third argument.
// If signal.aborted is true, returns true (caller should skip adding).
// Otherwise, if signal is an AbortSignal, adds an abort listener that
// will call removeEventListener when the signal fires.
// Parameters:
//   target_val - the EventTarget (element/document/window globalThis)
//   type_str   - the event type string (JS string)
//   handler    - the event handler function
//   ctx, argc, argv - the original addEventListener arguments
// Returns true if signal.aborted is already true (skip adding listener).
static bool handle_signal_option(JSContext* ctx, JSValueConst target_val,
                                  JSValueConst type_val, JSValueConst handler_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 3 || !JS_IsObject(argv[2])) return false;

    JSValue signal = JS_GetPropertyStr(ctx, argv[2], "signal");
    if (JS_IsUndefined(signal) || JS_IsNull(signal)) {
        JS_FreeValue(ctx, signal);
        return false;
    }

    // Check if signal.aborted is already true
    JSValue aborted = JS_GetPropertyStr(ctx, signal, "aborted");
    bool already_aborted = JS_ToBool(ctx, aborted) != 0;
    JS_FreeValue(ctx, aborted);

    if (already_aborted) {
        JS_FreeValue(ctx, signal);
        return true; // skip adding the listener
    }

    // Add an abort listener on the signal that calls removeEventListener
    // Build: signal.addEventListener('abort', function() { target.removeEventListener(type, handler); })
    // We do this via JS_Eval with captured values to keep it simple.
    JSValue add_fn = JS_GetPropertyStr(ctx, signal, "addEventListener");
    if (JS_IsFunction(ctx, add_fn)) {
        // Create a wrapper function that calls removeEventListener
        // We store target, type, handler as properties on the wrapper for access
        const char* wrapper_src = R"JS(
(function(tgt, evtType, evtHandler) {
    return function() {
        if (tgt && typeof tgt.removeEventListener === 'function') {
            tgt.removeEventListener(evtType, evtHandler);
        }
    };
})
)JS";
        JSValue factory = JS_Eval(ctx, wrapper_src, std::strlen(wrapper_src),
                                   "<signal-wrapper>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsFunction(ctx, factory)) {
            JSValue factory_args[3] = {
                JS_DupValue(ctx, target_val),
                JS_DupValue(ctx, type_val),
                JS_DupValue(ctx, handler_val)
            };
            JSValue wrapper = JS_Call(ctx, factory, JS_UNDEFINED, 3, factory_args);
            JS_FreeValue(ctx, factory_args[0]);
            JS_FreeValue(ctx, factory_args[1]);
            JS_FreeValue(ctx, factory_args[2]);

            if (JS_IsFunction(ctx, wrapper)) {
                JSValue abort_str = JS_NewString(ctx, "abort");
                JSValue add_args[2] = { abort_str, wrapper };
                JSValue add_ret = JS_Call(ctx, add_fn, signal, 2, add_args);
                JS_FreeValue(ctx, add_ret);
                JS_FreeValue(ctx, abort_str);
            }
            JS_FreeValue(ctx, wrapper);
        }
        JS_FreeValue(ctx, factory);
    }
    JS_FreeValue(ctx, add_fn);
    JS_FreeValue(ctx, signal);
    return false;
}

// --- element.addEventListener(type, handler [, capture]) ---
static JSValue js_element_add_event_listener(JSContext* ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    // Check signal option — if already aborted, skip adding
    if (handle_signal_option(ctx, this_val, argv[0], argv[1], argc, argv))
        return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    auto opts = extract_listener_options(ctx, argc, argv);

    // Duplicate the function value so we own a reference
    JSValue handler = JS_DupValue(ctx, argv[1]);
    state->listeners[node][type].push_back({handler, opts.capture, opts.once, opts.passive});
    return JS_UNDEFINED;
}

// --- element.removeEventListener(type, handler [, capture]) ---
static JSValue js_element_remove_event_listener(JSContext* ctx,
                                                 JSValueConst this_val,
                                                 int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    bool use_capture = extract_capture_flag(ctx, argc, argv);

    auto node_it = state->listeners.find(node);
    if (node_it == state->listeners.end()) return JS_UNDEFINED;
    auto type_it = node_it->second.find(type);
    if (type_it == node_it->second.end()) return JS_UNDEFINED;

    auto& entries = type_it->second;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        // Must match BOTH the callback identity AND the capture flag
        if (it->use_capture == use_capture) {
            // Compare JS function identity
            JSValue cmp = JS_NewBool(ctx, false);
            // QuickJS: two JSValues referring to the same function object
            // can be compared with JS_VALUE_GET_PTR for reference equality
            if (JS_VALUE_GET_TAG(it->handler) == JS_VALUE_GET_TAG(argv[1]) &&
                JS_VALUE_GET_PTR(it->handler) == JS_VALUE_GET_PTR(argv[1])) {
                JS_FreeValue(ctx, it->handler);
                entries.erase(it);
                JS_FreeValue(ctx, cmp);
                return JS_UNDEFINED;
            }
            JS_FreeValue(ctx, cmp);
        }
    }
    return JS_UNDEFINED;
}

// --- element.remove() ---
static JSValue js_element_remove(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || !node->parent) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    auto* parent = node->parent;
    for (auto it = parent->children.begin();
         it != parent->children.end(); ++it) {
        if (it->get() == node) {
            auto owned = std::move(*it);
            parent->children.erase(it);
            owned->parent = nullptr;
            state->owned_nodes.push_back(std::move(owned));
            state->modified = true;
            break;
        }
    }
    return JS_UNDEFINED;
}

// --- element.hasAttribute(name) ---
static JSValue js_element_has_attribute(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;

    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return JS_FALSE;
    std::string name_str(name_cstr);
    JS_FreeCString(ctx, name_cstr);

    return JS_NewBool(ctx, has_attr(*node, name_str));
}

// --- element.removeAttribute(name) ---
static JSValue js_element_remove_attribute(JSContext* ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;

    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return JS_UNDEFINED;
    std::string name_str(name_cstr);
    JS_FreeCString(ctx, name_cstr);

    auto& attrs = node->attributes;
    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        if (it->name == name_str) {
            attrs.erase(it);
            auto* state = get_dom_state(ctx);
            if (state) state->modified = true;
            break;
        }
    }
    return JS_UNDEFINED;
}

// --- classList helper methods ---
// These are exposed as __classListAdd, __classListRemove, __classListContains
// on the element prototype. The JS setup script creates a classList proxy object.

static JSValue js_element_classlist_add(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;

    const char* cls_cstr = JS_ToCString(ctx, argv[0]);
    if (!cls_cstr) return JS_UNDEFINED;
    std::string cls(cls_cstr);
    JS_FreeCString(ctx, cls_cstr);

    std::string classes = get_attr(*node, "class");
    // Check if already present
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string::npos) end = classes.size();
        if (classes.substr(pos, end - pos) == cls) return JS_UNDEFINED;
        pos = end + 1;
    }
    if (!classes.empty()) classes += " ";
    classes += cls;
    set_attr(*node, "class", classes);
    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

static JSValue js_element_classlist_remove(JSContext* ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;

    const char* cls_cstr = JS_ToCString(ctx, argv[0]);
    if (!cls_cstr) return JS_UNDEFINED;
    std::string cls(cls_cstr);
    JS_FreeCString(ctx, cls_cstr);

    std::string classes = get_attr(*node, "class");
    std::string result;
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string::npos) end = classes.size();
        std::string token = classes.substr(pos, end - pos);
        if (token != cls && !token.empty()) {
            if (!result.empty()) result += " ";
            result += token;
        }
        pos = end + 1;
    }
    set_attr(*node, "class", result);
    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

static JSValue js_element_classlist_contains(JSContext* ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;

    const char* cls_cstr = JS_ToCString(ctx, argv[0]);
    if (!cls_cstr) return JS_FALSE;
    std::string cls(cls_cstr);
    JS_FreeCString(ctx, cls_cstr);

    std::string classes = get_attr(*node, "class");
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string::npos) end = classes.size();
        if (classes.substr(pos, end - pos) == cls) return JS_TRUE;
        pos = end + 1;
    }
    return JS_FALSE;
}

// --- classList additional helpers ---

// __classListReplace(oldClass, newClass): atomic replace, returns bool
static JSValue js_element_classlist_replace(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_FALSE;

    const char* old_cstr = JS_ToCString(ctx, argv[0]);
    const char* new_cstr = JS_ToCString(ctx, argv[1]);
    if (!old_cstr || !new_cstr) {
        if (old_cstr) JS_FreeCString(ctx, old_cstr);
        if (new_cstr) JS_FreeCString(ctx, new_cstr);
        return JS_FALSE;
    }
    std::string old_cls(old_cstr);
    std::string new_cls(new_cstr);
    JS_FreeCString(ctx, old_cstr);
    JS_FreeCString(ctx, new_cstr);

    std::string classes = get_attr(*node, "class");
    bool found = false;
    std::string result;
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string::npos) end = classes.size();
        std::string token = classes.substr(pos, end - pos);
        if (!token.empty()) {
            if (token == old_cls && !found) {
                // Replace old with new (only first occurrence)
                found = true;
                if (!result.empty()) result += " ";
                result += new_cls;
            } else if (token != new_cls || !found) {
                // Keep other tokens; avoid duplicating new_cls if already inserted
                if (!result.empty()) result += " ";
                result += token;
            }
        }
        pos = end + 1;
    }
    if (!found) return JS_FALSE;
    set_attr(*node, "class", result);
    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_TRUE;
}

// __classListGetAll(): returns a JS Array of all class tokens (live from attribute)
static JSValue js_element_classlist_get_all(JSContext* ctx, JSValueConst this_val,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (!node) return arr;

    std::string classes = get_attr(*node, "class");
    uint32_t idx = 0;
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string::npos) end = classes.size();
        if (end > pos) {
            std::string token = classes.substr(pos, end - pos);
            if (!token.empty()) {
                JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, token.c_str()));
            }
        }
        pos = end + 1;
    }
    return arr;
}

// --- element.id setter ---
static JSValue js_element_set_id(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
        set_attr(*node, "id", str);
        JS_FreeCString(ctx, str);
        auto* state = get_dom_state(ctx);
        if (state) state->modified = true;
    }
    return JS_UNDEFINED;
}

// --- dataset helper methods ---
// __datasetGet(key), __datasetSet(key, value), __datasetHas(key)
// maps key "foo" to attribute "data-foo", camelCase "fooBar" to "data-foo-bar"

static std::string dataset_key_to_attr(const std::string& key) {
    std::string attr = "data-";
    for (char c : key) {
        if (c >= 'A' && c <= 'Z') {
            attr += '-';
            attr += static_cast<char>(c + ('a' - 'A'));
        } else {
            attr += c;
        }
    }
    return attr;
}

static JSValue js_element_dataset_get(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;

    const char* key_cstr = JS_ToCString(ctx, argv[0]);
    if (!key_cstr) return JS_UNDEFINED;
    std::string attr_name = dataset_key_to_attr(key_cstr);
    JS_FreeCString(ctx, key_cstr);

    if (!has_attr(*node, attr_name)) return JS_UNDEFINED;
    return JS_NewString(ctx, get_attr(*node, attr_name).c_str());
}

static JSValue js_element_dataset_set(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* key_cstr = JS_ToCString(ctx, argv[0]);
    const char* val_cstr = JS_ToCString(ctx, argv[1]);
    if (!key_cstr || !val_cstr) {
        if (key_cstr) JS_FreeCString(ctx, key_cstr);
        if (val_cstr) JS_FreeCString(ctx, val_cstr);
        return JS_UNDEFINED;
    }
    std::string attr_name = dataset_key_to_attr(key_cstr);
    set_attr(*node, attr_name, val_cstr);
    JS_FreeCString(ctx, key_cstr);
    JS_FreeCString(ctx, val_cstr);

    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

static JSValue js_element_dataset_has(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;

    const char* key_cstr = JS_ToCString(ctx, argv[0]);
    if (!key_cstr) return JS_FALSE;
    std::string attr_name = dataset_key_to_attr(key_cstr);
    JS_FreeCString(ctx, key_cstr);

    return JS_NewBool(ctx, has_attr(*node, attr_name));
}

// =========================================================================
// Style proxy
//
// Provides element.style.color = "red" by storing into the style attribute.
// A full implementation would parse and manipulate individual CSS properties;
// for now we support a limited set via a proxy object.
// =========================================================================

static void js_style_finalizer(JSRuntime* /*rt*/, JSValue val) {
    // The node is not owned by the style proxy.
    (void)val;
}

static JSClassDef style_class_def = {
    "CSSStyleDeclaration",
    js_style_finalizer,
    nullptr, nullptr, nullptr
};

// Helper: convert camelCase JS property name to CSS kebab-case
// e.g. "backgroundColor" -> "background-color"
static std::string camel_to_kebab(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            result += '-';
            result += static_cast<char>(c + ('a' - 'A'));
        } else {
            result += c;
        }
    }
    return result;
}

// Parse "color: red; font-size: 14px" into a map
static std::unordered_map<std::string, std::string>
parse_style_attr(const std::string& style) {
    std::unordered_map<std::string, std::string> props;
    size_t pos = 0;
    while (pos < style.size()) {
        size_t colon = style.find(':', pos);
        if (colon == std::string::npos) break;
        size_t semi = style.find(';', colon);
        if (semi == std::string::npos) semi = style.size();

        std::string key = style.substr(pos, colon - pos);
        std::string val = style.substr(colon + 1, semi - colon - 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start == std::string::npos) { s.clear(); return; }
            s = s.substr(start, end - start + 1);
        };
        trim(key);
        trim(val);
        if (!key.empty()) props[key] = val;
        pos = semi + 1;
    }
    return props;
}

// Serialize map back to style attribute string
static std::string serialize_style(
        const std::unordered_map<std::string, std::string>& props) {
    std::string result;
    for (auto& [k, v] : props) {
        if (!result.empty()) result += " ";
        result += k + ": " + v + ";";
    }
    return result;
}

// style.__getProperty(name) -- used by JS getter proxy
static JSValue js_style_get_property(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node || argc < 1) return JS_NewString(ctx, "");

    const char* prop_cstr = JS_ToCString(ctx, argv[0]);
    if (!prop_cstr) return JS_NewString(ctx, "");
    std::string prop(prop_cstr);
    JS_FreeCString(ctx, prop_cstr);

    auto props = parse_style_attr(get_attr(*node, "style"));
    std::string css_name = camel_to_kebab(prop);
    auto it = props.find(css_name);
    if (it != props.end()) {
        return JS_NewString(ctx, it->second.c_str());
    }
    return JS_NewString(ctx, "");
}

// Check if a CSS property name matches a transition definition
// Returns true if any transition covers this property (including "all")
static bool has_transition_for_property(
    const std::unordered_map<std::string, std::string>& style_props,
    const std::string& css_property) {
    auto it = style_props.find("transition");
    if (it != style_props.end()) {
        const std::string& tv = it->second;
        // Check if transition value mentions "all" or the specific property
        if (tv.find("all") != std::string::npos) return true;
        if (tv.find(css_property) != std::string::npos) return true;
    }
    auto pit = style_props.find("transition-property");
    if (pit != style_props.end()) {
        if (pit->second.find("all") != std::string::npos) return true;
        if (pit->second.find(css_property) != std::string::npos) return true;
    }
    return false;
}

// style.__setProperty(name, value) -- used by JS setter proxy
static JSValue js_style_set_property(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* prop_cstr = JS_ToCString(ctx, argv[0]);
    const char* val_cstr = JS_ToCString(ctx, argv[1]);
    if (!prop_cstr || !val_cstr) {
        if (prop_cstr) JS_FreeCString(ctx, prop_cstr);
        if (val_cstr) JS_FreeCString(ctx, val_cstr);
        return JS_UNDEFINED;
    }

    std::string prop(prop_cstr);
    std::string value(val_cstr);
    JS_FreeCString(ctx, prop_cstr);
    JS_FreeCString(ctx, val_cstr);

    auto props = parse_style_attr(get_attr(*node, "style"));
    std::string css_name = camel_to_kebab(prop);

    // CSS Transition awareness: if this property has a transition defined,
    // store the previous ("from") value as a data attribute for the render
    // pipeline to pick up on the next render pass.
    if (has_transition_for_property(props, css_name)) {
        auto prev_it = props.find(css_name);
        std::string from_attr = "data-transition-from-" + css_name;
        if (prev_it != props.end()) {
            set_attr(*node, from_attr, prev_it->second);
        } else {
            // No explicit previous value; mark as needing computed default
            set_attr(*node, from_attr, "");
        }
        set_attr(*node, "data-transition-to-" + css_name, value);
    }

    if (value.empty()) {
        props.erase(css_name);
    } else {
        props[css_name] = value;
    }
    set_attr(*node, "style", serialize_style(props));

    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

// style.removeProperty(name) — remove the property, return old value
static JSValue js_style_remove_property(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node || argc < 1) return JS_NewString(ctx, "");

    const char* prop_cstr = JS_ToCString(ctx, argv[0]);
    if (!prop_cstr) return JS_NewString(ctx, "");
    std::string prop(prop_cstr);
    JS_FreeCString(ctx, prop_cstr);

    auto props = parse_style_attr(get_attr(*node, "style"));
    std::string css_name = camel_to_kebab(prop);
    auto it = props.find(css_name);
    std::string old_value;
    if (it != props.end()) {
        old_value = it->second;
        props.erase(it);
        set_attr(*node, "style", serialize_style(props));
        auto* state = get_dom_state(ctx);
        if (state) state->modified = true;
    }
    return JS_NewString(ctx, old_value.c_str());
}

// style.cssText getter — return the full inline style string
static JSValue js_style_get_css_text(JSContext* ctx, JSValueConst this_val,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node) return JS_NewString(ctx, "");
    return JS_NewString(ctx, get_attr(*node, "style").c_str());
}

// style.cssText setter — replace entire inline style
static JSValue js_style_set_css_text(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node || argc < 1) return JS_UNDEFINED;

    const char* val_cstr = JS_ToCString(ctx, argv[0]);
    if (!val_cstr) return JS_UNDEFINED;
    set_attr(*node, "style", val_cstr);
    JS_FreeCString(ctx, val_cstr);

    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

// Create a style proxy for a given element node
static JSValue create_style_proxy(JSContext* ctx,
                                   clever::html::SimpleNode* node) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(style_class_id));
    if (JS_IsException(obj)) return JS_UNDEFINED;
    JS_SetOpaque(obj, node);

    // Add helper methods
    JS_SetPropertyStr(ctx, obj, "getPropertyValue",
        JS_NewCFunction(ctx, js_style_get_property, "getPropertyValue", 1));
    JS_SetPropertyStr(ctx, obj, "setProperty",
        JS_NewCFunction(ctx, js_style_set_property, "setProperty", 2));
    JS_SetPropertyStr(ctx, obj, "removeProperty",
        JS_NewCFunction(ctx, js_style_remove_property, "removeProperty", 1));
    JS_SetPropertyStr(ctx, obj, "__getProperty",
        JS_NewCFunction(ctx, js_style_get_property, "__getProperty", 1));
    JS_SetPropertyStr(ctx, obj, "__setProperty",
        JS_NewCFunction(ctx, js_style_set_property, "__setProperty", 2));

    // cssText as a getter/setter property
    {
        JSAtom css_text_atom = JS_NewAtom(ctx, "cssText");
        JSValue getter = JS_NewCFunction(ctx, js_style_get_css_text, "get cssText", 0);
        JSValue setter = JS_NewCFunction(ctx, js_style_set_css_text, "set cssText", 1);
        JS_DefinePropertyGetSet(ctx, obj, css_text_atom, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, css_text_atom);
    }

    return obj;
}

// --- element.style (getter) ---
static JSValue js_element_get_style(JSContext* ctx, JSValueConst this_val,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return create_style_proxy(ctx, node);
}

// =========================================================================
// document object methods
// =========================================================================

static JSValue js_document_get_element_by_id(JSContext* ctx,
                                              JSValueConst /*this_val*/,
                                              int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;

    auto* found = find_by_id(state->root, id);
    JS_FreeCString(ctx, id);
    return wrap_element(ctx, found);
}

static JSValue js_document_query_selector(JSContext* ctx,
                                           JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;

    const char* sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NULL;

    std::string sel_str(sel);
    JS_FreeCString(ctx, sel);

    auto* found = query_selector_real(state->root, sel_str);
    return wrap_element(ctx, found);
}

static JSValue js_document_query_selector_all(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NewArray(ctx);
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewArray(ctx);

    const char* sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NewArray(ctx);

    std::string sel_str(sel);
    JS_FreeCString(ctx, sel);

    std::vector<clever::html::SimpleNode*> results;
    query_selector_all_real(state->root, sel_str, results);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < results.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             wrap_element(ctx, results[i]));
    }
    return arr;
}

static JSValue js_document_create_element(JSContext* ctx,
                                           JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    const char* tag = JS_ToCString(ctx, argv[0]);
    if (!tag) return JS_NULL;

    auto node = std::make_unique<clever::html::SimpleNode>();
    node->type = clever::html::SimpleNode::Element;
    node->tag_name = tag;
    JS_FreeCString(ctx, tag);

    auto* raw_ptr = node.get();
    state->owned_nodes.push_back(std::move(node));
    return wrap_element(ctx, raw_ptr);
}

static JSValue js_document_create_text_node(JSContext* ctx,
                                             JSValueConst /*this_val*/,
                                             int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    const char* text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_NULL;

    auto node = std::make_unique<clever::html::SimpleNode>();
    node->type = clever::html::SimpleNode::Text;
    node->data = text;
    JS_FreeCString(ctx, text);

    auto* raw_ptr = node.get();
    state->owned_nodes.push_back(std::move(node));
    return wrap_element(ctx, raw_ptr);
}

static JSValue js_document_get_body(JSContext* ctx,
                                     JSValueConst /*this_val*/,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;
    return wrap_element(ctx, state->root->find_element("body"));
}

static JSValue js_document_get_head(JSContext* ctx,
                                     JSValueConst /*this_val*/,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;
    return wrap_element(ctx, state->root->find_element("head"));
}

static JSValue js_document_get_document_element(JSContext* ctx,
                                                 JSValueConst /*this_val*/,
                                                 int /*argc*/,
                                                 JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;
    return wrap_element(ctx, state->root->find_element("html"));
}

static JSValue js_document_get_title(JSContext* ctx,
                                      JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewString(ctx, "");
    if (state->title_set) return JS_NewString(ctx, state->title.c_str());
    auto* title_elem = state->root->find_element("title");
    if (title_elem) {
        return JS_NewString(ctx, title_elem->text_content().c_str());
    }
    return JS_NewString(ctx, "");
}

static JSValue js_document_set_title(JSContext* ctx,
                                      JSValueConst /*this_val*/,
                                      int argc, JSValueConst* argv) {
    auto* state = get_dom_state(ctx);
    if (!state || argc < 1) return JS_UNDEFINED;
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
        state->title = str;
        state->title_set = true;
        state->modified = true;
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
}

// --- document.write(html) ---
static JSValue js_document_write(JSContext* ctx,
                                  JSValueConst /*this_val*/,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_UNDEFINED;

    const char* html_cstr = JS_ToCString(ctx, argv[0]);
    if (!html_cstr) return JS_UNDEFINED;
    std::string html_str(html_cstr);
    JS_FreeCString(ctx, html_cstr);

    // Find or create body
    auto* body = state->root->find_element("body");
    if (!body) {
        auto* html_elem = state->root->find_element("html");
        if (!html_elem) html_elem = state->root;
        auto body_node = std::make_unique<clever::html::SimpleNode>();
        body_node->type = clever::html::SimpleNode::Element;
        body_node->tag_name = "body";
        body_node->parent = html_elem;
        body = body_node.get();
        html_elem->children.push_back(std::move(body_node));
    }

    // Parse the HTML string and move children into body
    auto parsed = clever::html::parse(html_str);
    if (parsed) {
        auto* parsed_body = parsed->find_element("body");
        auto* source = parsed_body ? parsed_body : parsed.get();
        for (auto& child : source->children) {
            child->parent = body;
            body->children.push_back(std::move(child));
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- document.writeln(html) ---
static JSValue js_document_writeln(JSContext* ctx,
                                    JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_UNDEFINED;

    const char* html_cstr = JS_ToCString(ctx, argv[0]);
    if (!html_cstr) return JS_UNDEFINED;
    std::string html_str(html_cstr);
    html_str += "\n"; // writeln appends a newline
    JS_FreeCString(ctx, html_cstr);

    // Find or create body
    auto* body = state->root->find_element("body");
    if (!body) {
        auto* html_elem = state->root->find_element("html");
        if (!html_elem) html_elem = state->root;
        auto body_node = std::make_unique<clever::html::SimpleNode>();
        body_node->type = clever::html::SimpleNode::Element;
        body_node->tag_name = "body";
        body_node->parent = html_elem;
        body = body_node.get();
        html_elem->children.push_back(std::move(body_node));
    }

    // Parse the HTML string and move children into body
    auto parsed = clever::html::parse(html_str);
    if (parsed) {
        auto* parsed_body = parsed->find_element("body");
        auto* source = parsed_body ? parsed_body : parsed.get();
        for (auto& child : source->children) {
            child->parent = body;
            body->children.push_back(std::move(child));
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- document.getElementsByTagName(tag) ---
static JSValue js_document_get_elements_by_tag_name(JSContext* ctx,
                                                     JSValueConst /*this_val*/,
                                                     int argc,
                                                     JSValueConst* argv) {
    if (argc < 1) return JS_NewArray(ctx);
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewArray(ctx);

    const char* tag_cstr = JS_ToCString(ctx, argv[0]);
    if (!tag_cstr) return JS_NewArray(ctx);
    std::string tag(tag_cstr);
    JS_FreeCString(ctx, tag_cstr);

    // Convert to lowercase for case-insensitive matching
    std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

    std::vector<clever::html::SimpleNode*> results;
    find_by_tag(state->root, tag, results);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < results.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             wrap_element(ctx, results[i]));
    }
    return arr;
}

// --- document.getElementsByClassName(cls) ---
static JSValue js_document_get_elements_by_class_name(JSContext* ctx,
                                                       JSValueConst /*this_val*/,
                                                       int argc,
                                                       JSValueConst* argv) {
    if (argc < 1) return JS_NewArray(ctx);
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewArray(ctx);

    const char* cls_cstr = JS_ToCString(ctx, argv[0]);
    if (!cls_cstr) return JS_NewArray(ctx);
    std::string cls(cls_cstr);
    JS_FreeCString(ctx, cls_cstr);

    std::vector<clever::html::SimpleNode*> results;
    find_by_class(state->root, cls, results);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < results.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             wrap_element(ctx, results[i]));
    }
    return arr;
}

// =========================================================================
// document.addEventListener(type, handler) / window.addEventListener(...)
//
// These store listeners keyed on the document root node in DOMState.
// A special sentinel key "__window__" is used for window listeners.
// =========================================================================

static JSValue js_document_add_event_listener(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    // Check signal option — if already aborted, skip adding
    // Use document object as the target for removeEventListener
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue doc_obj = JS_GetPropertyStr(ctx, global, "document");
    bool skip = handle_signal_option(ctx, doc_obj, argv[0], argv[1], argc, argv);
    JS_FreeValue(ctx, doc_obj);
    JS_FreeValue(ctx, global);
    if (skip) return JS_UNDEFINED;

    auto opts = extract_listener_options(ctx, argc, argv);

    // Store listener on the document root node
    JSValue handler = JS_DupValue(ctx, argv[1]);
    state->listeners[state->root][type].push_back({handler, opts.capture, opts.once, opts.passive});
    return JS_UNDEFINED;
}

static JSValue js_document_remove_event_listener(JSContext* ctx,
                                                  JSValueConst /*this_val*/,
                                                  int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    bool use_capture = extract_capture_flag(ctx, argc, argv);

    auto node_it = state->listeners.find(state->root);
    if (node_it == state->listeners.end()) return JS_UNDEFINED;
    auto type_it = node_it->second.find(type);
    if (type_it == node_it->second.end()) return JS_UNDEFINED;

    auto& entries = type_it->second;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->use_capture == use_capture &&
            JS_VALUE_GET_TAG(it->handler) == JS_VALUE_GET_TAG(argv[1]) &&
            JS_VALUE_GET_PTR(it->handler) == JS_VALUE_GET_PTR(argv[1])) {
            JS_FreeValue(ctx, it->handler);
            entries.erase(it);
            return JS_UNDEFINED;
        }
    }
    return JS_UNDEFINED;
}

// We use a special nullptr key to represent "window" listeners that are
// distinct from document listeners.  This is a pragmatic choice since
// SimpleNode* nullptr is never a real node.
static constexpr clever::html::SimpleNode* window_sentinel = nullptr;

static JSValue js_window_add_event_listener(JSContext* ctx,
                                             JSValueConst /*this_val*/,
                                             int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    // Check signal option — if already aborted, skip adding
    JSValue global = JS_GetGlobalObject(ctx);
    bool skip = handle_signal_option(ctx, global, argv[0], argv[1], argc, argv);
    JS_FreeValue(ctx, global);
    if (skip) return JS_UNDEFINED;

    bool use_capture = extract_capture_flag(ctx, argc, argv);

    // Store listener under the window sentinel key
    JSValue handler = JS_DupValue(ctx, argv[1]);
    state->listeners[window_sentinel][type].push_back({handler, use_capture});
    return JS_UNDEFINED;
}

static JSValue js_window_remove_event_listener(JSContext* ctx,
                                                JSValueConst /*this_val*/,
                                                int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return JS_UNDEFINED;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    if (!JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;

    bool use_capture = extract_capture_flag(ctx, argc, argv);

    auto node_it = state->listeners.find(window_sentinel);
    if (node_it == state->listeners.end()) return JS_UNDEFINED;
    auto type_it = node_it->second.find(type);
    if (type_it == node_it->second.end()) return JS_UNDEFINED;

    auto& entries = type_it->second;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->use_capture == use_capture &&
            JS_VALUE_GET_TAG(it->handler) == JS_VALUE_GET_TAG(argv[1]) &&
            JS_VALUE_GET_PTR(it->handler) == JS_VALUE_GET_PTR(argv[1])) {
            JS_FreeValue(ctx, it->handler);
            entries.erase(it);
            return JS_UNDEFINED;
        }
    }
    return JS_UNDEFINED;
}

// =========================================================================
// Helper: scan DOM tree for inline event attributes (onclick, onload, etc.)
// and register them as event listeners.
// =========================================================================

static void scan_inline_event_attributes(JSContext* ctx,
                                          clever::html::SimpleNode* node) {
    if (!node) return;
    auto* state = get_dom_state(ctx);
    if (!state) return;

    // List of HTML inline event attributes we support
    static const char* event_attrs[] = {
        "onclick", "onload", "onchange", "onsubmit", "oninput",
        "onmouseover", "onmouseout", "onmousedown", "onmouseup",
        "onmousemove", "onmouseenter", "onmouseleave",
        "ondblclick", "oncontextmenu",
        "onkeydown", "onkeyup", "onkeypress", "onfocus", "onblur",
        nullptr
    };

    if (node->type == clever::html::SimpleNode::Element) {
        for (int i = 0; event_attrs[i] != nullptr; i++) {
            std::string attr_name(event_attrs[i]);
            std::string code = get_attr(*node, attr_name);
            if (code.empty()) continue;

            // Extract event type from attribute name (strip "on" prefix)
            std::string event_type = attr_name.substr(2);

            // Wrap the inline code in a function expression
            std::string wrapper = "(function(event){" + code + "})";
            JSValue func = JS_Eval(ctx, wrapper.c_str(), wrapper.size(),
                                    "<inline-event>", JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(func)) {
                JSValue exc = JS_GetException(ctx);
                JS_FreeValue(ctx, exc);
                continue;
            }

            if (JS_IsFunction(ctx, func)) {
                state->listeners[node][event_type].push_back({func, false});
            } else {
                JS_FreeValue(ctx, func);
            }
        }
    }

    // Recurse into children
    for (auto& child : node->children) {
        scan_inline_event_attributes(ctx, child.get());
    }
}

// =========================================================================
// Element class definition (class ID, finalizer)
// =========================================================================

static void js_element_finalizer(JSRuntime* /*rt*/, JSValue /*val*/) {
    // SimpleNode is owned by the tree (or DOMState::owned_nodes), not by JS.
    // Do NOT free anything here.
}

static JSClassDef element_class_def = {
    "Element",
    js_element_finalizer,
    nullptr, nullptr, nullptr
};

// =========================================================================
// document.cookie getter/setter
// =========================================================================

// Getter: returns all cookies as "name=value; name2=value2"
static JSValue js_document_get_cookie(JSContext* ctx, JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NewString(ctx, "");

    auto trim = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    };

    std::string document_url;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue location = JS_GetPropertyStr(ctx, global, "location");
    if (JS_IsObject(location)) {
        JSValue href = JS_GetPropertyStr(ctx, location, "href");
        if (JS_IsString(href)) {
            const char* href_cstr = JS_ToCString(ctx, href);
            if (href_cstr) {
                document_url = href_cstr;
                JS_FreeCString(ctx, href_cstr);
            }
        }
        JS_FreeValue(ctx, href);
    }
    JS_FreeValue(ctx, location);
    JS_FreeValue(ctx, global);

    std::map<std::string, std::string> merged = state->cookies;
    if (!document_url.empty()) {
        if (auto parsed = clever::url::parse(document_url); parsed.has_value() && !parsed->host.empty()) {
            std::string cookie_header = clever::net::CookieJar::shared().get_cookie_header(
                parsed->host, parsed->path.empty() ? "/" : parsed->path, parsed->scheme == "https", true, true);
            if (!cookie_header.empty()) {
                std::istringstream stream(cookie_header);
                std::string part;
                while (std::getline(stream, part, ';')) {
                    part = trim(part);
                    if (part.empty()) continue;
                    auto eq = part.find('=');
                    if (eq == std::string::npos) continue;
                    std::string name = trim(part.substr(0, eq));
                    if (name.empty()) continue;
                    if (merged.find(name) != merged.end()) continue;
                    merged[name] = trim(part.substr(eq + 1));
                }
            }
        }
    }

    std::string result;
    for (auto it = merged.begin(); it != merged.end(); ++it) {
        if (it != merged.begin()) result += "; ";
        result += it->first + "=" + it->second;
    }
    return JS_NewString(ctx, result.c_str());
}

static JSValue js_document_set_cookie(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    std::string cookie_str = raw;
    JS_FreeCString(ctx, raw);
    if (cookie_str.empty()) return JS_UNDEFINED;

    auto trim = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    };

    std::string first_part;
    auto semi = cookie_str.find(';');
    if (semi != std::string::npos) {
        first_part = cookie_str.substr(0, semi);
    } else {
        first_part = cookie_str;
    }

    first_part = trim(first_part);
    if (first_part.empty()) return JS_UNDEFINED;

    auto eq = first_part.find('=');
    if (eq == std::string::npos) return JS_UNDEFINED;

    std::string name = trim(first_part.substr(0, eq));
    std::string value = trim(first_part.substr(eq + 1));

    if (name.empty()) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    state->cookies[name] = value;

    std::string document_url;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue location = JS_GetPropertyStr(ctx, global, "location");
    if (JS_IsObject(location)) {
        JSValue href = JS_GetPropertyStr(ctx, location, "href");
        if (JS_IsString(href)) {
            const char* href_cstr = JS_ToCString(ctx, href);
            if (href_cstr) {
                document_url = href_cstr;
                JS_FreeCString(ctx, href_cstr);
            }
        }
        JS_FreeValue(ctx, href);
    }
    JS_FreeValue(ctx, location);
    JS_FreeValue(ctx, global);

    if (auto parsed = clever::url::parse(document_url); parsed.has_value() && !parsed->host.empty()) {
        bool has_domain = false;
        bool has_path = false;
        std::istringstream stream(cookie_str);
        std::string part;
        bool is_first = true;
        while (std::getline(stream, part, ';')) {
            part = trim(part);
            if (part.empty()) continue;
            if (is_first) {
                is_first = false;
                continue;
            }

            auto attr_eq = part.find('=');
            std::string attr_name = trim(attr_eq == std::string::npos ? part : part.substr(0, attr_eq));
            attr_name = to_lower_str(attr_name);
            if (attr_name == "domain") {
                has_domain = true;
            } else if (attr_name == "path") {
                has_path = true;
            }
        }

        std::string jar_cookie = cookie_str;
        if (!has_domain) {
            jar_cookie += "; Domain=" + parsed->host;
        }
        if (!has_path) {
            std::string default_path = parsed->path.empty() ? "/" : parsed->path;
            jar_cookie += "; Path=" + default_path;
        }
        clever::net::CookieJar::shared().set_from_header(jar_cookie, parsed->host);
    }

    return JS_UNDEFINED;
}

// =========================================================================
// Helper: build a plain DOMRect object from {x, y, width, height}.
//
// top/right/bottom/left are derived from x/y/width/height per spec.
// toJSON() is added for structured serialization.
// =========================================================================

static JSValue make_dom_rect(JSContext* ctx, double x, double y,
                              double w, double h) {
    JSValue rect = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rect, "x",      JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, rect, "y",      JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, rect, "width",  JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, rect, "height", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, rect, "top",    JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, rect, "left",   JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, rect, "right",  JS_NewFloat64(ctx, x + w));
    JS_SetPropertyStr(ctx, rect, "bottom", JS_NewFloat64(ctx, y + h));
    static const char* to_json_src =
        "(function() { return { x: this.x, y: this.y, "
        "width: this.width, height: this.height, "
        "top: this.top, right: this.right, "
        "bottom: this.bottom, left: this.left }; })";
    JSValue to_json_fn = JS_Eval(ctx, to_json_src,
                                  std::strlen(to_json_src),
                                  "<dom-rect>", JS_EVAL_TYPE_GLOBAL);
    JS_SetPropertyStr(ctx, rect, "toJSON", to_json_fn);
    return rect;
}

// =========================================================================
// element.getBoundingClientRect()
//
// Returns a DOMRect object with {x, y, width, height, top, right, bottom,
// left} representing the element's border box in viewport coordinates.
// Viewport coordinates = page coordinates minus the current scroll offset.
// When layout geometry has been populated via populate_layout_geometry(),
// returns real computed values.  Otherwise falls back to zeros.
// =========================================================================

static JSValue js_element_get_bounding_client_rect(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);

    double x = 0, y = 0, w = 0, h = 0;
    if (node && state) {
        auto it = state->layout_geometry.find(node);
        if (it != state->layout_geometry.end()) {
            auto& lr = it->second;
            // abs_border_x/y is the absolute page-coordinate position of
            // the border-box top-left edge (precomputed by populate_layout_geometry).
            x = static_cast<double>(lr.abs_border_x);
            y = static_cast<double>(lr.abs_border_y);
            w = static_cast<double>(lr.border_left + lr.padding_left +
                                    lr.width +
                                    lr.padding_right + lr.border_right);
            h = static_cast<double>(lr.border_top + lr.padding_top +
                                    lr.height +
                                    lr.padding_bottom + lr.border_bottom);
        }
    }

    // Subtract viewport scroll offset so result is in viewport coordinates
    // (CSSOM View spec: getBoundingClientRect is viewport-relative).
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue sx_val = JS_GetPropertyStr(ctx, global, "scrollX");
    JSValue sy_val = JS_GetPropertyStr(ctx, global, "scrollY");
    double sx = 0, sy = 0;
    JS_ToFloat64(ctx, &sx, sx_val);
    JS_ToFloat64(ctx, &sy, sy_val);
    JS_FreeValue(ctx, sx_val);
    JS_FreeValue(ctx, sy_val);
    JS_FreeValue(ctx, global);
    x -= sx;
    y -= sy;

    return make_dom_rect(ctx, x, y, w, h);
}

// =========================================================================
// element.getClientRects()
//
// Returns an array-like object (DOMRectList) containing DOMRect entries for
// each CSS box of the element.  For block-level elements this is a single
// rect equal to getBoundingClientRect().  The returned object has:
//   .length, [i], and .item(i)
// matching the DOMRectList interface.
// =========================================================================

static JSValue js_element_get_client_rects(JSContext* ctx,
                                            JSValueConst this_val,
                                            int /*argc*/,
                                            JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);

    // Build a JS Array; wrap it into a DOMRectList-compatible object via
    // DOMRectList constructor if it has been registered.
    JSValue arr = JS_NewArray(ctx);
    uint32_t count = 0;

    if (node && state) {
        auto it = state->layout_geometry.find(node);
        if (it != state->layout_geometry.end()) {
            auto& lr = it->second;
            double x = static_cast<double>(lr.abs_border_x);
            double y = static_cast<double>(lr.abs_border_y);
            double w = static_cast<double>(lr.border_left + lr.padding_left +
                                           lr.width +
                                           lr.padding_right + lr.border_right);
            double h = static_cast<double>(lr.border_top + lr.padding_top +
                                           lr.height +
                                           lr.padding_bottom + lr.border_bottom);

            // Subtract viewport scroll (same as getBoundingClientRect)
            JSValue global = JS_GetGlobalObject(ctx);
            JSValue sx_v = JS_GetPropertyStr(ctx, global, "scrollX");
            JSValue sy_v = JS_GetPropertyStr(ctx, global, "scrollY");
            double sx = 0, sy = 0;
            JS_ToFloat64(ctx, &sx, sx_v);
            JS_ToFloat64(ctx, &sy, sy_v);
            JS_FreeValue(ctx, sx_v);
            JS_FreeValue(ctx, sy_v);
            JS_FreeValue(ctx, global);
            x -= sx;
            y -= sy;

            if (w > 0 || h > 0) {
                JS_SetPropertyUint32(ctx, arr, count++,
                                     make_dom_rect(ctx, x, y, w, h));
            }
        }
    }

    // Try to wrap the array as a DOMRectList (registered in JS)
    JSValue global2 = JS_GetGlobalObject(ctx);
    JSValue DRLCtor = JS_GetPropertyStr(ctx, global2, "DOMRectList");
    JS_FreeValue(ctx, global2);
    if (JS_IsFunction(ctx, DRLCtor)) {
        JSValue list = JS_CallConstructor(ctx, DRLCtor, 1, &arr);
        JS_FreeValue(ctx, DRLCtor);
        JS_FreeValue(ctx, arr);
        if (!JS_IsException(list)) return list;
        JS_FreeValue(ctx, list);
        // Fall through and return plain array on error
        arr = JS_NewArray(ctx);
        return arr;
    }
    JS_FreeValue(ctx, DRLCtor);

    // Fallback: add .item() method to the plain array
    static const char* item_src = "(function(arr) { "
        "arr.item = function(i) { return (i >= 0 && i < arr.length) ? arr[i] : null; }; })";
    JSValue item_fn = JS_Eval(ctx, item_src, std::strlen(item_src),
                               "<client-rects-item>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, item_fn)) {
        JSValue ret = JS_Call(ctx, item_fn, JS_UNDEFINED, 1, &arr);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, item_fn);
    return arr;
}

// =========================================================================
// Dimension getters backed by layout geometry
//
// Magic values: 0=offsetWidth, 1=offsetHeight, 2=offsetTop, 3=offsetLeft
//               4=scrollWidth, 5=scrollHeight, 6=scrollTop, 7=scrollLeft
//               8=clientWidth, 9=clientHeight, 10=clientTop, 11=clientLeft
// =========================================================================

static JSValue js_element_dimension_getter(JSContext* ctx,
                                            JSValueConst this_val,
                                            int /*argc*/,
                                            JSValueConst* /*argv*/,
                                            int magic) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state) return JS_NewFloat64(ctx, 0);

    // For document.documentElement (html) or body, clientWidth/Height
    // should return the viewport dimensions when no layout geometry exists
    if (magic == 8 || magic == 9) {
        if (node == state->root) {
            // document.documentElement
            auto it = state->layout_geometry.find(node);
            if (it == state->layout_geometry.end()) {
                return JS_NewFloat64(ctx, magic == 8 ? state->viewport_width : state->viewport_height);
            }
        }
        // Check for <body> -- first element child of root
        if (state->root) {
            for (auto& child : state->root->children) {
                if (child->type == clever::html::SimpleNode::Type::Element &&
                    child->tag_name == "html") {
                    for (auto& grandchild : child->children) {
                        if (grandchild->type == clever::html::SimpleNode::Type::Element &&
                            grandchild->tag_name == "body" && grandchild.get() == node) {
                            auto it = state->layout_geometry.find(node);
                            if (it == state->layout_geometry.end()) {
                                return JS_NewFloat64(ctx, magic == 8 ? state->viewport_width : state->viewport_height);
                            }
                        }
                    }
                }
            }
        }
    }

    auto it = state->layout_geometry.find(node);
    if (it == state->layout_geometry.end()) return JS_NewFloat64(ctx, 0);

    auto& lr = it->second;
    // offsetWidth/offsetHeight = border box (border + padding + content)
    float border_box_w = lr.border_left + lr.padding_left + lr.width + lr.padding_right + lr.border_right;
    float border_box_h = lr.border_top + lr.padding_top + lr.height + lr.padding_bottom + lr.border_bottom;
    // clientWidth/clientHeight = padding box (padding + content, no border)
    float client_w = lr.padding_left + lr.width + lr.padding_right;
    float client_h = lr.padding_top + lr.height + lr.padding_bottom;

    switch (magic) {
        case 0: return JS_NewFloat64(ctx, border_box_w);     // offsetWidth
        case 1: return JS_NewFloat64(ctx, border_box_h);     // offsetHeight
        case 2: {
            // offsetTop = distance from border-box top to offsetParent border-box top
            float parent_border_y = 0;
            if (lr.parent_dom_node) {
                auto pit = state->layout_geometry.find(lr.parent_dom_node);
                if (pit != state->layout_geometry.end()) {
                    parent_border_y = pit->second.abs_border_y;
                }
            }
            return JS_NewFloat64(ctx, lr.abs_border_y - parent_border_y);  // offsetTop
        }
        case 3: {
            // offsetLeft = distance from border-box left to offsetParent border-box left
            float parent_border_x = 0;
            if (lr.parent_dom_node) {
                auto pit = state->layout_geometry.find(lr.parent_dom_node);
                if (pit != state->layout_geometry.end()) {
                    parent_border_x = pit->second.abs_border_x;
                }
            }
            return JS_NewFloat64(ctx, lr.abs_border_x - parent_border_x);  // offsetLeft
        }
        case 4: {
            // scrollWidth: max of client width and scroll content width
            float sw = lr.is_scroll_container && lr.scroll_content_width > client_w
                       ? lr.scroll_content_width : client_w;
            return JS_NewFloat64(ctx, sw);
        }
        case 5: {
            // scrollHeight: max of client height and scroll content height
            float sh = lr.is_scroll_container && lr.scroll_content_height > client_h
                       ? lr.scroll_content_height : client_h;
            return JS_NewFloat64(ctx, sh);
        }
        case 6: return JS_NewFloat64(ctx, lr.scroll_top);     // scrollTop
        case 7: return JS_NewFloat64(ctx, lr.scroll_left);    // scrollLeft
        case 8: return JS_NewFloat64(ctx, client_w);          // clientWidth
        case 9: return JS_NewFloat64(ctx, client_h);          // clientHeight
        case 10: return JS_NewFloat64(ctx, lr.border_top);    // clientTop (= border-top width)
        case 11: return JS_NewFloat64(ctx, lr.border_left);   // clientLeft (= border-left width)
        default: return JS_NewFloat64(ctx, 0);
    }
}

// =========================================================================
// scrollTop / scrollLeft setters (no-op, since we don't scroll yet)
// =========================================================================

static JSValue js_element_dimension_setter(JSContext* ctx,
                                            JSValueConst this_val,
                                            int argc,
                                            JSValueConst* argv,
                                            int magic) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state || argc < 1) return JS_UNDEFINED;

    double val = 0;
    JS_ToFloat64(ctx, &val, argv[0]);
    if (val < 0) val = 0; // scroll positions cannot be negative

    auto it = state->layout_geometry.find(static_cast<void*>(node));
    if (it != state->layout_geometry.end()) {
        auto& lr = it->second;
        if (magic == 6) {
            // scrollTop setter
            float max_scroll = lr.is_scroll_container
                ? std::max(0.0f, lr.scroll_content_height - lr.height)
                : 0.0f;
            lr.scroll_top = static_cast<float>(std::min(val, static_cast<double>(max_scroll)));
        } else if (magic == 7) {
            // scrollLeft setter
            float max_scroll = lr.is_scroll_container
                ? std::max(0.0f, lr.scroll_content_width - lr.width)
                : 0.0f;
            lr.scroll_left = static_cast<float>(std::min(val, static_cast<double>(max_scroll)));
        }
        // Mark DOM as modified so re-render picks up the scroll change
        state->modified = true;
    }
    return JS_UNDEFINED;
}


// =========================================================================
// window.getComputedStyle(element [, pseudoElement])
//
// Returns a CSSStyleDeclaration-like object.  When layout geometry is
// available (populated by populate_layout_geometry()), dimension-related
// properties return real computed values.  Otherwise falls back to
// inline style attribute values.
// =========================================================================

// Helper: format a float as "Npx" (integer when possible, otherwise 2 decimals)
static std::string format_px(float v) {
    int iv = static_cast<int>(v);
    if (static_cast<float>(iv) == v) {
        return std::to_string(iv) + "px";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2fpx", static_cast<double>(v));
    return buf;
}

// getPropertyValue for computed-style objects: checks layout_geometry first
// =========================================================================
// Helpers for getComputedStyle value formatting
// =========================================================================

// Format ARGB uint32_t as "rgb(r, g, b)" or "rgba(r, g, b, a/255)"
static std::string format_color_argb(uint32_t argb) {
    uint8_t a = (argb >> 24) & 0xFF;
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >>  8) & 0xFF;
    uint8_t b = (argb >>  0) & 0xFF;
    char buf[64];
    if (a == 255) {
        std::snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", r, g, b);
    } else if (a == 0) {
        std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, 0)", r, g, b);
    } else {
        std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.3f)",
                      r, g, b, static_cast<double>(a) / 255.0);
    }
    return buf;
}

// Map display_type int to CSS string
static const char* display_type_to_css(int dt) {
    switch (dt) {
        case 0:  return "block";
        case 1:  return "inline";
        case 2:  return "inline-block";
        case 3:  return "flex";
        case 4:  return "inline-flex";
        case 5:  return "none";
        case 6:  return "list-item";
        case 7:  return "table";
        case 8:  return "table-row";
        case 9:  return "table-cell";
        case 10: return "grid";
        case 11: return "inline-grid";
        default: return "block";
    }
}

// Map position_type int to CSS string
static const char* position_type_to_css(int pt) {
    switch (pt) {
        case 0: return "static";
        case 1: return "relative";
        case 2: return "absolute";
        case 3: return "fixed";
        case 4: return "sticky";
        default: return "static";
    }
}

// Map float_type int to CSS string
static const char* float_type_to_css(int ft) {
    switch (ft) {
        case 0: return "none";
        case 1: return "left";
        case 2: return "right";
        default: return "none";
    }
}

// Map clear_type int to CSS string
static const char* clear_type_to_css(int ct) {
    switch (ct) {
        case 0: return "none";
        case 1: return "left";
        case 2: return "right";
        case 3: return "both";
        default: return "none";
    }
}

// Map overflow int to CSS string (0=visible, 1=hidden, 2=scroll, 3=auto)
static const char* overflow_to_css(int ov) {
    switch (ov) {
        case 0: return "visible";
        case 1: return "hidden";
        case 2: return "scroll";
        case 3: return "auto";
        default: return "visible";
    }
}

// Map text_align int to CSS string
static const char* text_align_to_css(int ta) {
    switch (ta) {
        case 0: return "left";
        case 1: return "center";
        case 2: return "right";
        case 3: return "justify";
        default: return "left";
    }
}

// Map white_space int to CSS string
static const char* white_space_to_css(int ws) {
    switch (ws) {
        case 0: return "normal";
        case 1: return "nowrap";
        case 2: return "pre";
        case 3: return "pre-wrap";
        case 4: return "pre-line";
        case 5: return "break-spaces";
        default: return "normal";
    }
}

// Map word_break int to CSS string
static const char* word_break_to_css(int wb) {
    switch (wb) {
        case 0: return "normal";
        case 1: return "break-all";
        case 2: return "keep-all";
        default: return "normal";
    }
}

// Map overflow_wrap int to CSS string
static const char* overflow_wrap_to_css(int ow) {
    switch (ow) {
        case 0: return "normal";
        case 1: return "break-word";
        case 2: return "anywhere";
        default: return "normal";
    }
}

// Map text_transform int to CSS string
static const char* text_transform_to_css(int tt) {
    switch (tt) {
        case 0: return "none";
        case 1: return "capitalize";
        case 2: return "uppercase";
        case 3: return "lowercase";
        default: return "none";
    }
}

// Map flex_direction int to CSS string
static const char* flex_direction_to_css(int fd) {
    switch (fd) {
        case 0: return "row";
        case 1: return "row-reverse";
        case 2: return "column";
        case 3: return "column-reverse";
        default: return "row";
    }
}

// Map flex_wrap int to CSS string
static const char* flex_wrap_to_css(int fw) {
    switch (fw) {
        case 0: return "nowrap";
        case 1: return "wrap";
        case 2: return "wrap-reverse";
        default: return "nowrap";
    }
}

// Map justify_content int to CSS string
static const char* justify_content_to_css(int jc) {
    switch (jc) {
        case 0: return "flex-start";
        case 1: return "flex-end";
        case 2: return "center";
        case 3: return "space-between";
        case 4: return "space-around";
        case 5: return "space-evenly";
        default: return "flex-start";
    }
}

// Map align_items / align_self int to CSS string
static const char* align_items_to_css(int ai) {
    switch (ai) {
        case 0: return "flex-start";
        case 1: return "flex-end";
        case 2: return "center";
        case 3: return "baseline";
        case 4: return "stretch";
        default: return "stretch";
    }
}

// Map border_style int to CSS string
static const char* border_style_to_css(int bs) {
    switch (bs) {
        case 0: return "none";
        case 1: return "solid";
        case 2: return "dashed";
        case 3: return "dotted";
        case 4: return "double";
        default: return "none";
    }
}

// Map cursor int to CSS string
static const char* cursor_to_css(int c) {
    switch (c) {
        case 0: return "auto";
        case 1: return "default";
        case 2: return "pointer";
        case 3: return "text";
        case 4: return "move";
        case 5: return "not-allowed";
        default: return "auto";
    }
}

// Map user_select int to CSS string
static const char* user_select_to_css(int us) {
    switch (us) {
        case 0: return "auto";
        case 1: return "none";
        case 2: return "text";
        case 3: return "all";
        default: return "auto";
    }
}

// Build text-decoration string from bitmask
static std::string text_decoration_to_css(int bits) {
    if (bits == 0) return "none";
    std::string result;
    if (bits & 1) result += "underline ";
    if (bits & 2) result += "overline ";
    if (bits & 4) result += "line-through ";
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

// Build transform CSS string from vector of Transform structs
static std::string transforms_to_css(const std::vector<clever::css::Transform>& transforms) {
    if (transforms.empty()) return "none";
    std::string result;
    for (const auto& t : transforms) {
        char buf[128];
        switch (t.type) {
            case clever::css::TransformType::Translate:
                std::snprintf(buf, sizeof(buf), "translate(%gpx, %gpx) ",
                              static_cast<double>(t.x), static_cast<double>(t.y));
                break;
            case clever::css::TransformType::Rotate:
                std::snprintf(buf, sizeof(buf), "rotate(%gdeg) ",
                              static_cast<double>(t.angle));
                break;
            case clever::css::TransformType::Scale:
                std::snprintf(buf, sizeof(buf), "scale(%g, %g) ",
                              static_cast<double>(t.x), static_cast<double>(t.y));
                break;
            case clever::css::TransformType::Skew:
                std::snprintf(buf, sizeof(buf), "skew(%gdeg, %gdeg) ",
                              static_cast<double>(t.x), static_cast<double>(t.y));
                break;
            case clever::css::TransformType::Matrix:
                std::snprintf(buf, sizeof(buf),
                              "matrix(%g, %g, %g, %g, %g, %g) ",
                              static_cast<double>(t.m[0]), static_cast<double>(t.m[1]),
                              static_cast<double>(t.m[2]), static_cast<double>(t.m[3]),
                              static_cast<double>(t.m[4]), static_cast<double>(t.m[5]));
                break;
            default:
                buf[0] = '\0';
                break;
        }
        result += buf;
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

// Lookup a single CSS property value from a LayoutRect.
// Returns empty string if the property is not known or not available.
static std::string computed_style_lookup(const DOMState::LayoutRect& rect,
                                          const std::string& css_name) {
    // Box model dimensions (resolved px)
    if (css_name == "width")  return format_px(rect.width);
    if (css_name == "height") return format_px(rect.height);
    if (css_name == "padding-top")    return format_px(rect.padding_top);
    if (css_name == "padding-right")  return format_px(rect.padding_right);
    if (css_name == "padding-bottom") return format_px(rect.padding_bottom);
    if (css_name == "padding-left")   return format_px(rect.padding_left);
    if (css_name == "padding") {
        return format_px(rect.padding_top) + " " + format_px(rect.padding_right) + " " +
               format_px(rect.padding_bottom) + " " + format_px(rect.padding_left);
    }
    if (css_name == "margin-top")    return format_px(rect.margin_top);
    if (css_name == "margin-right")  return format_px(rect.margin_right);
    if (css_name == "margin-bottom") return format_px(rect.margin_bottom);
    if (css_name == "margin-left")   return format_px(rect.margin_left);
    if (css_name == "margin") {
        return format_px(rect.margin_top) + " " + format_px(rect.margin_right) + " " +
               format_px(rect.margin_bottom) + " " + format_px(rect.margin_left);
    }
    if (css_name == "border-top-width")    return format_px(rect.border_top);
    if (css_name == "border-right-width")  return format_px(rect.border_right);
    if (css_name == "border-bottom-width") return format_px(rect.border_bottom);
    if (css_name == "border-left-width")   return format_px(rect.border_left);
    if (css_name == "border-width") {
        return format_px(rect.border_top) + " " + format_px(rect.border_right) + " " +
               format_px(rect.border_bottom) + " " + format_px(rect.border_left);
    }

    // Sizing constraints
    if (css_name == "min-width")  return (rect.min_width_val > 0) ? format_px(rect.min_width_val) : "0px";
    if (css_name == "max-width")  return (rect.max_width_val >= 1e8f) ? "none" : format_px(rect.max_width_val);
    if (css_name == "min-height") return (rect.min_height_val > 0) ? format_px(rect.min_height_val) : "0px";
    if (css_name == "max-height") return (rect.max_height_val >= 1e8f) ? "none" : format_px(rect.max_height_val);

    // Display / position / flow
    if (css_name == "display")  return display_type_to_css(rect.display_type);
    if (css_name == "position") return position_type_to_css(rect.position_type);
    if (css_name == "float")    return float_type_to_css(rect.float_type);
    if (css_name == "clear")    return clear_type_to_css(rect.clear_type);
    if (css_name == "box-sizing") return rect.border_box ? "border-box" : "content-box";

    // Typography
    if (css_name == "font-size")    return format_px(rect.font_size);
    if (css_name == "font-weight")  {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", rect.font_weight);
        return buf;
    }
    if (css_name == "font-style")  return rect.font_italic ? "italic" : "normal";
    if (css_name == "font-family") return rect.font_family.empty() ? "sans-serif" : rect.font_family;
    if (css_name == "line-height") {
        if (rect.line_height_px > 0) return format_px(rect.line_height_px);
        return "normal";
    }

    // Colors
    if (css_name == "color")            return format_color_argb(rect.color);
    if (css_name == "background-color") return format_color_argb(rect.background_color);
    if (css_name == "background-image") {
        if (!rect.bg_image_url.empty() && rect.bg_image_url != "<url>")
            return "url(\"" + rect.bg_image_url + "\")";
        if (rect.bg_image_url == "<url>") return "url()"; // URL unknown from layout
        if (rect.gradient_type == 1)    return "linear-gradient(...)";
        if (rect.gradient_type == 2)    return "radial-gradient(...)";
        return "none";
    }

    // Visual
    if (css_name == "opacity") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.opacity_val));
        return buf;
    }
    if (css_name == "visibility") return rect.visibility_hidden ? "hidden" : "visible";
    if (css_name == "overflow")   return overflow_to_css(rect.overflow_x_val);
    if (css_name == "overflow-x") return overflow_to_css(rect.overflow_x_val);
    if (css_name == "overflow-y") return overflow_to_css(rect.overflow_y_val);
    if (css_name == "z-index")    return rect.z_index_auto ? "auto" : std::to_string(rect.z_index_val);

    // Text properties
    if (css_name == "text-align")      return text_align_to_css(rect.text_align_val);
    if (css_name == "text-decoration") return text_decoration_to_css(rect.text_decoration_bits);
    if (css_name == "white-space")     return white_space_to_css(rect.white_space_val);
    if (css_name == "word-break")      return word_break_to_css(rect.word_break_val);
    if (css_name == "word-wrap" || css_name == "overflow-wrap")
                                       return overflow_wrap_to_css(rect.overflow_wrap_val);
    if (css_name == "text-transform")  return text_transform_to_css(rect.text_transform_val);
    if (css_name == "text-overflow") {
        return (rect.text_overflow_val == 1) ? "ellipsis" : "clip";
    }

    // Flex properties
    if (css_name == "flex-grow") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.flex_grow));
        return buf;
    }
    if (css_name == "flex-shrink") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.flex_shrink));
        return buf;
    }
    if (css_name == "flex-basis") {
        return (rect.flex_basis < 0) ? "auto" : format_px(rect.flex_basis);
    }
    if (css_name == "flex") {
        std::string fb = (rect.flex_basis < 0) ? "auto" : format_px(rect.flex_basis);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g %g %s",
                      static_cast<double>(rect.flex_grow),
                      static_cast<double>(rect.flex_shrink),
                      fb.c_str());
        return buf;
    }
    if (css_name == "flex-direction")  return flex_direction_to_css(rect.flex_direction);
    if (css_name == "flex-wrap")       return flex_wrap_to_css(rect.flex_wrap_val);
    if (css_name == "justify-content") return justify_content_to_css(rect.justify_content_val);
    if (css_name == "align-items")     return align_items_to_css(rect.align_items_val);
    if (css_name == "align-self") {
        if (rect.align_self_val == -1) return "auto";
        return align_items_to_css(rect.align_self_val);
    }

    // Border radius
    if (css_name == "border-radius") {
        // Return shorthand: tl tr br bl
        if (rect.border_radius_tl == rect.border_radius_tr &&
            rect.border_radius_tl == rect.border_radius_br &&
            rect.border_radius_tl == rect.border_radius_bl) {
            return format_px(rect.border_radius_tl);
        }
        return format_px(rect.border_radius_tl) + " " + format_px(rect.border_radius_tr) + " " +
               format_px(rect.border_radius_br) + " " + format_px(rect.border_radius_bl);
    }
    if (css_name == "border-top-left-radius")     return format_px(rect.border_radius_tl);
    if (css_name == "border-top-right-radius")    return format_px(rect.border_radius_tr);
    if (css_name == "border-bottom-left-radius")  return format_px(rect.border_radius_bl);
    if (css_name == "border-bottom-right-radius") return format_px(rect.border_radius_br);

    // Border styles
    if (css_name == "border-top-style")    return border_style_to_css(rect.border_style_top);
    if (css_name == "border-right-style")  return border_style_to_css(rect.border_style_right);
    if (css_name == "border-bottom-style") return border_style_to_css(rect.border_style_bottom);
    if (css_name == "border-left-style")   return border_style_to_css(rect.border_style_left);
    if (css_name == "border-style") {
        return std::string(border_style_to_css(rect.border_style_top)) + " " +
               border_style_to_css(rect.border_style_right) + " " +
               border_style_to_css(rect.border_style_bottom) + " " +
               border_style_to_css(rect.border_style_left);
    }

    // Border colors
    if (css_name == "border-top-color")    return format_color_argb(rect.border_color_top);
    if (css_name == "border-right-color")  return format_color_argb(rect.border_color_right);
    if (css_name == "border-bottom-color") return format_color_argb(rect.border_color_bottom);
    if (css_name == "border-left-color")   return format_color_argb(rect.border_color_left);
    if (css_name == "border-color") {
        return format_color_argb(rect.border_color_top) + " " +
               format_color_argb(rect.border_color_right) + " " +
               format_color_argb(rect.border_color_bottom) + " " +
               format_color_argb(rect.border_color_left);
    }

    // Shorthand border
    if (css_name == "border") {
        return format_px(rect.border_top) + " " +
               border_style_to_css(rect.border_style_top) + " " +
               format_color_argb(rect.border_color_top);
    }

    // CSS transforms
    if (css_name == "transform") return transforms_to_css(rect.transforms);

    // Cursor / pointer-events / user-select
    if (css_name == "cursor")         return cursor_to_css(rect.cursor_val);
    if (css_name == "pointer-events") return (rect.pointer_events == 1) ? "none" : "auto";
    if (css_name == "user-select" || css_name == "-webkit-user-select")
                                       return user_select_to_css(rect.user_select_val);

    return ""; // property not in rect
}

// getPropertyValue for computed-style objects backed by the LayoutRect cache
static JSValue js_computed_style_get_property(JSContext* ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    auto* node = static_cast<clever::html::SimpleNode*>(
        JS_GetOpaque(this_val, style_class_id));
    if (!node || argc < 1) return JS_NewString(ctx, "");

    const char* prop_cstr = JS_ToCString(ctx, argv[0]);
    if (!prop_cstr) return JS_NewString(ctx, "");
    std::string prop(prop_cstr);
    JS_FreeCString(ctx, prop_cstr);

    // Normalize to kebab-case
    std::string css_name = camel_to_kebab(prop);
    if (css_name == "css-float") css_name = "float";

    // Check layout_geometry for layout-resolved properties
    auto* state = get_dom_state(ctx);
    if (state) {
        auto it = state->layout_geometry.find(static_cast<void*>(node));
        if (it != state->layout_geometry.end()) {
            std::string val = computed_style_lookup(it->second, css_name);
            if (!val.empty()) return JS_NewString(ctx, val.c_str());
        }
    }

    // Fall back to inline style attribute
    auto props = parse_style_attr(get_attr(*node, "style"));
    auto it2 = props.find(css_name);
    if (it2 != props.end()) {
        return JS_NewString(ctx, it2->second.c_str());
    }
    return JS_NewString(ctx, "");
}

// =========================================================================
// window.getComputedStyle(element [, pseudoElement])
//
// Returns a CSSStyleDeclaration-like object with layout-backed computed
// values for all important CSS properties.
// =========================================================================
static JSValue js_get_computed_style(JSContext* ctx, JSValueConst /*this_val*/,
                                      int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    // Unwrap the element from the first argument
    auto* node = unwrap_element(ctx, argv[0]);
    if (!node) return JS_NULL;

    // Create a CSSStyleDeclaration-class object backed by the node pointer
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(style_class_id));
    if (JS_IsException(obj)) return JS_NULL;
    JS_SetOpaque(obj, node);

    // Add getPropertyValue and setProperty methods
    JS_SetPropertyStr(ctx, obj, "getPropertyValue",
        JS_NewCFunction(ctx, js_computed_style_get_property, "getPropertyValue", 1));

    // Stub setProperty (computed styles are read-only per spec)
    JS_SetPropertyStr(ctx, obj, "setProperty",
        JS_NewCFunction(ctx, [](JSContext* /*c*/, JSValueConst, int, JSValueConst*) -> JSValue {
            return JS_UNDEFINED;
        }, "setProperty", 3));

    // Helper: kebab-to-camelCase conversion
    auto to_camel = [](const std::string& key) -> std::string {
        if (key.rfind("--", 0) == 0) return key; // CSS variables unchanged
        std::string camel;
        bool next_upper = false;
        for (char c : key) {
            if (c == '-') {
                next_upper = true;
            } else if (next_upper) {
                camel += static_cast<char>(::toupper(c));
                next_upper = false;
            } else {
                camel += c;
            }
        }
        return camel;
    };

    // Helper to set a property (kebab and camelCase)
    auto set_prop = [&](const char* name, const std::string& value) {
        JS_SetPropertyStr(ctx, obj, name, JS_NewString(ctx, value.c_str()));
        std::string camel = to_camel(name);
        if (camel != name) {
            JS_SetPropertyStr(ctx, obj, camel.c_str(), JS_NewString(ctx, value.c_str()));
        }
    };

    // Inline style fallback map (built once, used for anything not in layout cache)
    auto inline_props = parse_style_attr(get_attr(*node, "style"));

    auto* state = get_dom_state(ctx);
    bool has_geometry = false;

    if (state) {
        auto geo_it = state->layout_geometry.find(static_cast<void*>(node));
        if (geo_it != state->layout_geometry.end()) {
            has_geometry = true;
            const auto& rect = geo_it->second;

            // ---- Box model (resolved px from actual layout) ----
            set_prop("width",  format_px(rect.width));
            set_prop("height", format_px(rect.height));
            set_prop("padding-top",    format_px(rect.padding_top));
            set_prop("padding-right",  format_px(rect.padding_right));
            set_prop("padding-bottom", format_px(rect.padding_bottom));
            set_prop("padding-left",   format_px(rect.padding_left));
            set_prop("padding",
                format_px(rect.padding_top) + " " + format_px(rect.padding_right) + " " +
                format_px(rect.padding_bottom) + " " + format_px(rect.padding_left));
            set_prop("margin-top",    format_px(rect.margin_top));
            set_prop("margin-right",  format_px(rect.margin_right));
            set_prop("margin-bottom", format_px(rect.margin_bottom));
            set_prop("margin-left",   format_px(rect.margin_left));
            set_prop("margin",
                format_px(rect.margin_top) + " " + format_px(rect.margin_right) + " " +
                format_px(rect.margin_bottom) + " " + format_px(rect.margin_left));
            set_prop("border-top-width",    format_px(rect.border_top));
            set_prop("border-right-width",  format_px(rect.border_right));
            set_prop("border-bottom-width", format_px(rect.border_bottom));
            set_prop("border-left-width",   format_px(rect.border_left));
            set_prop("border-width",
                format_px(rect.border_top) + " " + format_px(rect.border_right) + " " +
                format_px(rect.border_bottom) + " " + format_px(rect.border_left));

            // ---- Sizing constraints ----
            set_prop("min-width",  (rect.min_width_val > 0) ? format_px(rect.min_width_val) : "0px");
            set_prop("max-width",  (rect.max_width_val >= 1e8f) ? "none" : format_px(rect.max_width_val));
            set_prop("min-height", (rect.min_height_val > 0) ? format_px(rect.min_height_val) : "0px");
            set_prop("max-height", (rect.max_height_val >= 1e8f) ? "none" : format_px(rect.max_height_val));

            // ---- Display / position / flow ----
            set_prop("display",    display_type_to_css(rect.display_type));
            set_prop("position",   position_type_to_css(rect.position_type));
            set_prop("float",      float_type_to_css(rect.float_type));
            set_prop("clear",      clear_type_to_css(rect.clear_type));
            set_prop("box-sizing", rect.border_box ? "border-box" : "content-box");

            // ---- Typography ----
            set_prop("font-size",   format_px(rect.font_size));
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%d", rect.font_weight);
                set_prop("font-weight", buf);
            }
            set_prop("font-style",  rect.font_italic ? "italic" : "normal");
            set_prop("font-family", rect.font_family.empty() ? "sans-serif" : rect.font_family);
            set_prop("line-height", rect.line_height_px > 0 ? format_px(rect.line_height_px) : "normal");

            // ---- Colors ----
            set_prop("color",            format_color_argb(rect.color));
            set_prop("background-color", format_color_argb(rect.background_color));
            {
                std::string bg_img = "none";
                if (!rect.bg_image_url.empty() && rect.bg_image_url != "<url>")
                    bg_img = "url(\"" + rect.bg_image_url + "\")";
                else if (rect.bg_image_url == "<url>")
                    bg_img = "url()"; // URL unknown from layout
                else if (rect.gradient_type == 1)
                    bg_img = "linear-gradient(...)";
                else if (rect.gradient_type == 2)
                    bg_img = "radial-gradient(...)";
                set_prop("background-image", bg_img);
            }

            // ---- Visual ----
            {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.opacity_val));
                set_prop("opacity", buf);
            }
            set_prop("visibility",  rect.visibility_hidden ? "hidden" : "visible");
            set_prop("overflow",    overflow_to_css(rect.overflow_x_val));
            set_prop("overflow-x",  overflow_to_css(rect.overflow_x_val));
            set_prop("overflow-y",  overflow_to_css(rect.overflow_y_val));
            set_prop("z-index",     rect.z_index_auto ? "auto" : std::to_string(rect.z_index_val));

            // ---- Text properties ----
            set_prop("text-align",      text_align_to_css(rect.text_align_val));
            set_prop("text-decoration", text_decoration_to_css(rect.text_decoration_bits));
            set_prop("white-space",     white_space_to_css(rect.white_space_val));
            set_prop("word-break",      word_break_to_css(rect.word_break_val));
            set_prop("word-wrap",       overflow_wrap_to_css(rect.overflow_wrap_val));
            set_prop("overflow-wrap",   overflow_wrap_to_css(rect.overflow_wrap_val));
            set_prop("text-transform",  text_transform_to_css(rect.text_transform_val));
            set_prop("text-overflow",   rect.text_overflow_val == 1 ? "ellipsis" : "clip");

            // ---- Flex properties ----
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.flex_grow));
                set_prop("flex-grow", buf);
                std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(rect.flex_shrink));
                set_prop("flex-shrink", buf);
            }
            set_prop("flex-basis",     rect.flex_basis < 0 ? "auto" : format_px(rect.flex_basis));
            {
                std::string fb = rect.flex_basis < 0 ? "auto" : format_px(rect.flex_basis);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g %g %s",
                              static_cast<double>(rect.flex_grow),
                              static_cast<double>(rect.flex_shrink),
                              fb.c_str());
                set_prop("flex", buf);
            }
            set_prop("flex-direction",  flex_direction_to_css(rect.flex_direction));
            set_prop("flex-wrap",       flex_wrap_to_css(rect.flex_wrap_val));
            set_prop("justify-content", justify_content_to_css(rect.justify_content_val));
            set_prop("align-items",     align_items_to_css(rect.align_items_val));
            set_prop("align-self",      rect.align_self_val == -1 ? "auto" :
                                        align_items_to_css(rect.align_self_val));

            // ---- Border radius ----
            set_prop("border-top-left-radius",     format_px(rect.border_radius_tl));
            set_prop("border-top-right-radius",    format_px(rect.border_radius_tr));
            set_prop("border-bottom-left-radius",  format_px(rect.border_radius_bl));
            set_prop("border-bottom-right-radius", format_px(rect.border_radius_br));
            {
                std::string br_short;
                if (rect.border_radius_tl == rect.border_radius_tr &&
                    rect.border_radius_tl == rect.border_radius_br &&
                    rect.border_radius_tl == rect.border_radius_bl) {
                    br_short = format_px(rect.border_radius_tl);
                } else {
                    br_short = format_px(rect.border_radius_tl) + " " +
                               format_px(rect.border_radius_tr) + " " +
                               format_px(rect.border_radius_br) + " " +
                               format_px(rect.border_radius_bl);
                }
                set_prop("border-radius", br_short);
            }

            // ---- Border styles ----
            set_prop("border-top-style",    border_style_to_css(rect.border_style_top));
            set_prop("border-right-style",  border_style_to_css(rect.border_style_right));
            set_prop("border-bottom-style", border_style_to_css(rect.border_style_bottom));
            set_prop("border-left-style",   border_style_to_css(rect.border_style_left));
            set_prop("border-style",
                std::string(border_style_to_css(rect.border_style_top)) + " " +
                border_style_to_css(rect.border_style_right) + " " +
                border_style_to_css(rect.border_style_bottom) + " " +
                border_style_to_css(rect.border_style_left));

            // ---- Border colors ----
            set_prop("border-top-color",    format_color_argb(rect.border_color_top));
            set_prop("border-right-color",  format_color_argb(rect.border_color_right));
            set_prop("border-bottom-color", format_color_argb(rect.border_color_bottom));
            set_prop("border-left-color",   format_color_argb(rect.border_color_left));
            set_prop("border-color",
                format_color_argb(rect.border_color_top) + " " +
                format_color_argb(rect.border_color_right) + " " +
                format_color_argb(rect.border_color_bottom) + " " +
                format_color_argb(rect.border_color_left));

            // ---- Border shorthand ----
            set_prop("border",
                format_px(rect.border_top) + " " +
                border_style_to_css(rect.border_style_top) + " " +
                format_color_argb(rect.border_color_top));

            // ---- CSS Transforms ----
            set_prop("transform", transforms_to_css(rect.transforms));

            // Stub transition / animation — return "none" unless inline style says otherwise
            set_prop("transition", "none");
            set_prop("animation",  "none");

            // ---- Cursor / pointer-events / user-select ----
            set_prop("cursor",          cursor_to_css(rect.cursor_val));
            set_prop("pointer-events",  rect.pointer_events == 1 ? "none" : "auto");
            set_prop("user-select",     user_select_to_css(rect.user_select_val));
            set_prop("-webkit-user-select", user_select_to_css(rect.user_select_val));
        }
    }

    // Apply any inline style properties not already set from layout geometry.
    // This ensures properties like transition, animation, custom properties, etc.
    // that are set inline but not in the layout cache still appear.
    static const std::unordered_set<std::string> layout_backed_props = {
        "width", "height",
        "padding-top", "padding-right", "padding-bottom", "padding-left", "padding",
        "margin-top", "margin-right", "margin-bottom", "margin-left", "margin",
        "border-top-width", "border-right-width", "border-bottom-width", "border-left-width", "border-width",
        "min-width", "max-width", "min-height", "max-height",
        "display", "position", "float", "clear", "box-sizing",
        "font-size", "font-weight", "font-style", "font-family", "line-height",
        "color", "background-color", "background-image",
        "opacity", "visibility", "overflow", "overflow-x", "overflow-y", "z-index",
        "text-align", "text-decoration", "white-space", "word-break", "word-wrap",
        "overflow-wrap", "text-transform", "text-overflow",
        "flex-grow", "flex-shrink", "flex-basis", "flex",
        "flex-direction", "flex-wrap", "justify-content", "align-items", "align-self",
        "border-radius", "border-top-left-radius", "border-top-right-radius",
        "border-bottom-left-radius", "border-bottom-right-radius",
        "border-top-style", "border-right-style", "border-bottom-style", "border-left-style", "border-style",
        "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", "border-color",
        "border", "transform", "cursor", "pointer-events", "user-select", "-webkit-user-select",
    };

    for (const auto& [key, value] : inline_props) {
        if (has_geometry && layout_backed_props.count(key)) continue;
        set_prop(key.c_str(), value);
    }

    // Set length to count of properties (approximate)
    int prop_count = has_geometry ? static_cast<int>(layout_backed_props.size()) : 0;
    prop_count += static_cast<int>(inline_props.size());
    JS_SetPropertyStr(ctx, obj, "length", JS_NewInt32(ctx, prop_count));

    return obj;
}

// =========================================================================
// element.insertBefore(newNode, referenceNode)
// =========================================================================

// Helper: detach a node from the tree or owned_nodes, returning its unique_ptr.
// Returns empty unique_ptr if not found.
static std::unique_ptr<clever::html::SimpleNode>
detach_node(DOMState* state, clever::html::SimpleNode* node) {
    // Check owned_nodes first (nodes created by JS not yet in tree)
    for (auto it = state->owned_nodes.begin();
         it != state->owned_nodes.end(); ++it) {
        if (it->get() == node) {
            auto owned = std::move(*it);
            state->owned_nodes.erase(it);
            return owned;
        }
    }
    // Otherwise detach from current parent
    if (node->parent) {
        auto* old_parent = node->parent;
        for (auto it = old_parent->children.begin();
             it != old_parent->children.end(); ++it) {
            if (it->get() == node) {
                auto owned = std::move(*it);
                old_parent->children.erase(it);
                owned->parent = nullptr;
                return owned;
            }
        }
    }
    return nullptr;
}

static JSValue js_element_insert_before(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* parent_node = unwrap_element(ctx, this_val);
    auto* new_node = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!parent_node || !new_node) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // If referenceNode is null/undefined, behave like appendChild
    bool ref_is_null = (argc < 2) || JS_IsNull(argv[1]) || JS_IsUndefined(argv[1]);

    if (ref_is_null) {
        // Append: detach new_node, then push_back
        auto owned = detach_node(state, new_node);
        if (!owned) return JS_UNDEFINED;
        owned->parent = parent_node;
        parent_node->children.push_back(std::move(owned));
        state->modified = true;
        return wrap_element(ctx, new_node);
    }

    auto* ref_node = unwrap_element(ctx, argv[1]);
    if (!ref_node) return JS_UNDEFINED;

    // Find referenceNode's index in parent's children
    int ref_idx = -1;
    for (size_t i = 0; i < parent_node->children.size(); i++) {
        if (parent_node->children[i].get() == ref_node) {
            ref_idx = static_cast<int>(i);
            break;
        }
    }
    if (ref_idx < 0) return JS_UNDEFINED; // ref not a child of this parent

    auto owned = detach_node(state, new_node);
    if (!owned) return JS_UNDEFINED;

    // Re-find ref_idx in case detach_node modified the children vector
    // (if new_node was already a child of parent_node before ref_node)
    ref_idx = -1;
    for (size_t i = 0; i < parent_node->children.size(); i++) {
        if (parent_node->children[i].get() == ref_node) {
            ref_idx = static_cast<int>(i);
            break;
        }
    }
    if (ref_idx < 0) return JS_UNDEFINED;

    owned->parent = parent_node;
    parent_node->children.insert(
        parent_node->children.begin() + ref_idx, std::move(owned));
    state->modified = true;
    return wrap_element(ctx, new_node);
}

// =========================================================================
// element.replaceChild(newChild, oldChild)
// =========================================================================

static JSValue js_element_replace_child(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* parent_node = unwrap_element(ctx, this_val);
    auto* new_child = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    auto* old_child = (argc > 1) ? unwrap_element(ctx, argv[1]) : nullptr;
    if (!parent_node || !new_child || !old_child) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // Find oldChild in parent's children
    int old_idx = -1;
    for (size_t i = 0; i < parent_node->children.size(); i++) {
        if (parent_node->children[i].get() == old_child) {
            old_idx = static_cast<int>(i);
            break;
        }
    }
    if (old_idx < 0) return JS_UNDEFINED; // oldChild not found

    // Detach newChild from its current location
    auto new_owned = detach_node(state, new_child);
    if (!new_owned) return JS_UNDEFINED;

    // Re-find old_idx (may have changed if newChild was a sibling before oldChild)
    old_idx = -1;
    for (size_t i = 0; i < parent_node->children.size(); i++) {
        if (parent_node->children[i].get() == old_child) {
            old_idx = static_cast<int>(i);
            break;
        }
    }
    if (old_idx < 0) return JS_UNDEFINED;

    // Replace: take the old child's unique_ptr, put newChild in its place
    auto old_owned = std::move(parent_node->children[static_cast<size_t>(old_idx)]);
    old_owned->parent = nullptr;
    new_owned->parent = parent_node;
    parent_node->children[static_cast<size_t>(old_idx)] = std::move(new_owned);

    // Keep old child alive in owned_nodes so JS references remain valid
    state->owned_nodes.push_back(std::move(old_owned));
    state->modified = true;

    // Return the old child (per DOM spec)
    return wrap_element(ctx, old_child);
}

// =========================================================================
// element.cloneNode(deep)
// =========================================================================

static std::unique_ptr<clever::html::SimpleNode>
clone_node_impl(const clever::html::SimpleNode* source, bool deep) {
    auto clone = std::make_unique<clever::html::SimpleNode>();
    clone->type = source->type;
    clone->tag_name = source->tag_name;
    clone->data = source->data;
    clone->doctype_name = source->doctype_name;
    clone->attributes = source->attributes;
    clone->parent = nullptr;

    if (deep) {
        for (auto& child : source->children) {
            auto child_clone = clone_node_impl(child.get(), true);
            child_clone->parent = clone.get();
            clone->children.push_back(std::move(child_clone));
        }
    }
    return clone;
}

static JSValue js_element_clone_node(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;

    int deep_arg = 0;
    if (argc > 0) {
        deep_arg = JS_ToBool(ctx, argv[0]);
        if (deep_arg < 0) return JS_EXCEPTION;
    }
    const bool deep = deep_arg != 0;

    auto clone = clone_node_impl(node, deep);
    auto* raw_ptr = clone.get();

    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;
    state->owned_nodes.push_back(std::move(clone));

    return wrap_element(ctx, raw_ptr);
}

// =========================================================================
// document.createDocumentFragment()
// =========================================================================

static JSValue js_document_create_document_fragment(JSContext* ctx,
                                                      JSValueConst /*this_val*/,
                                                      int /*argc*/,
                                                      JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    auto frag = std::make_unique<clever::html::SimpleNode>();
    frag->type = clever::html::SimpleNode::Document; // DocumentFragment uses Document type
    frag->tag_name = "#document-fragment";

    auto* raw_ptr = frag.get();
    state->owned_nodes.push_back(std::move(frag));
    return wrap_element(ctx, raw_ptr);
}

// =========================================================================
// element.contains(other)
// =========================================================================

static bool contains_impl(const clever::html::SimpleNode* ancestor,
                           const clever::html::SimpleNode* target) {
    if (!ancestor || !target) return false;

    std::vector<const clever::html::SimpleNode*> to_visit;
    to_visit.push_back(ancestor);
    while (!to_visit.empty()) {
        const auto* current = to_visit.back();
        to_visit.pop_back();
        if (current == target) return true;

        for (auto it = current->children.rbegin();
             it != current->children.rend(); ++it) {
            to_visit.push_back(it->get());
        }
    }
    return false;
}

static JSValue js_element_contains(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* other = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!node) return JS_FALSE;
    if (!other) return JS_FALSE;
    return JS_NewBool(ctx, contains_impl(node, other));
}

// =========================================================================
// element.insertAdjacentHTML(position, html)
// =========================================================================

static JSValue js_element_insert_adjacent_html(JSContext* ctx,
                                                JSValueConst this_val,
                                                int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* pos_cstr = JS_ToCString(ctx, argv[0]);
    const char* html_cstr = JS_ToCString(ctx, argv[1]);
    if (!pos_cstr || !html_cstr) {
        if (pos_cstr) JS_FreeCString(ctx, pos_cstr);
        if (html_cstr) JS_FreeCString(ctx, html_cstr);
        return JS_UNDEFINED;
    }
    std::string position(pos_cstr);
    std::string html_str(html_cstr);
    JS_FreeCString(ctx, pos_cstr);
    JS_FreeCString(ctx, html_cstr);

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // Parse the HTML fragment
    auto parsed = clever::html::parse(html_str);
    if (!parsed) return JS_UNDEFINED;

    auto* body = parsed->find_element("body");
    auto* source = body ? body : parsed.get();

    if (position == "beforebegin") {
        // Insert before this element (needs a parent)
        if (!node->parent) return JS_UNDEFINED;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_UNDEFINED;
        auto& siblings = node->parent->children;
        size_t insert_pos = static_cast<size_t>(idx);
        for (auto it = source->children.begin(); it != source->children.end();) {
            auto& child = *it;
            child->parent = node->parent;
            siblings.insert(siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
                          std::move(child));
            insert_pos++;
            it = source->children.erase(it);
        }
    } else if (position == "afterbegin") {
        // Insert as first children of this element
        size_t insert_pos = 0;
        for (auto it = source->children.begin(); it != source->children.end();) {
            auto& child = *it;
            child->parent = node;
            node->children.insert(
                node->children.begin() + static_cast<ptrdiff_t>(insert_pos),
                std::move(child));
            insert_pos++;
            it = source->children.erase(it);
        }
    } else if (position == "beforeend") {
        // Append as last children of this element
        for (auto& child : source->children) {
            child->parent = node;
            node->children.push_back(std::move(child));
        }
        source->children.clear();
    } else if (position == "afterend") {
        // Insert after this element (needs a parent)
        if (!node->parent) return JS_UNDEFINED;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_UNDEFINED;
        auto& siblings = node->parent->children;
        size_t insert_pos = static_cast<size_t>(idx) + 1;
        for (auto it = source->children.begin(); it != source->children.end();) {
            auto& child = *it;
            child->parent = node->parent;
            siblings.insert(siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
                          std::move(child));
            insert_pos++;
            it = source->children.erase(it);
        }
    }

    state->modified = true;
    return JS_UNDEFINED;
}

// =========================================================================
// element.outerHTML (getter)
// =========================================================================

static std::string serialize_node(const clever::html::SimpleNode* node) {
    if (!node) return "";
    if (node->type == clever::html::SimpleNode::Text) {
        return node->data;
    }
    if (node->type == clever::html::SimpleNode::Comment) {
        return "<!--" + node->data + "-->";
    }
    if (node->type != clever::html::SimpleNode::Element) {
        // For Document/DocumentType, just serialize children
        std::string result;
        for (auto& child : node->children) {
            result += serialize_node(child.get());
        }
        return result;
    }

    // Element type
    std::string result = "<" + node->tag_name;
    for (auto& attr : node->attributes) {
        result += " " + attr.name + "=\"" + attr.value + "\"";
    }
    result += ">";

    // Void elements (self-closing, no end tag)
    static const char* void_tags[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr", nullptr
    };
    for (int i = 0; void_tags[i]; i++) {
        if (node->tag_name == void_tags[i]) return result;
    }

    for (auto& child : node->children) {
        result += serialize_node(child.get());
    }
    result += "</" + node->tag_name + ">";
    return result;
}

static JSValue js_element_get_outer_html(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, serialize_node(node).c_str());
}

// =========================================================================
// element.outerHTML (setter) — replaces element in its parent with parsed HTML
// =========================================================================

static JSValue js_element_set_outer_html(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;
    if (!node->parent) return JS_UNDEFINED;  // Cannot replace root

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_UNDEFINED;

    auto parsed = clever::html::parse(str);
    JS_FreeCString(ctx, str);
    if (!parsed) return JS_UNDEFINED;

    auto* body = parsed->find_element("body");
    auto* source = body ? body : parsed.get();

    auto* parent = node->parent;
    int idx = find_sibling_index(node);
    if (idx < 0) return JS_UNDEFINED;

    // Remove the current element from its parent
    auto& siblings = parent->children;
    siblings.erase(siblings.begin() + idx);

    // Insert parsed children at the same position
    size_t insert_pos = static_cast<size_t>(idx);
    for (auto it = source->children.begin(); it != source->children.end();) {
        auto& child = *it;
        child->parent = parent;
        siblings.insert(siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
                        std::move(child));
        insert_pos++;
        it = source->children.erase(it);
    }

    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

// =========================================================================
// MutationObserver implementation
// =========================================================================

// Helper: check if a node is an ancestor of another
static bool is_ancestor(clever::html::SimpleNode* potential_ancestor,
                        clever::html::SimpleNode* node) {
    auto* current = node->parent;
    while (current) {
        if (current == potential_ancestor) return true;
        current = current->parent;
    }
    return false;
}

// Helper: create a MutationRecord JS object
static JSValue create_mutation_record(JSContext* ctx,
                                      const std::string& type,
                                      clever::html::SimpleNode* target,
                                      const std::vector<clever::html::SimpleNode*>& added_nodes,
                                      const std::vector<clever::html::SimpleNode*>& removed_nodes,
                                      clever::html::SimpleNode* previous_sibling,
                                      clever::html::SimpleNode* next_sibling,
                                      const std::string& attr_name = "",
                                      const std::string& old_value = "") {
    JSValue record = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, record, "type", JS_NewString(ctx, type.c_str()));
    JS_SetPropertyStr(ctx, record, "target", wrap_element(ctx, target));

    // Create addedNodes array
    JSValue added_arr = JS_NewArray(ctx);
    for (size_t i = 0; i < added_nodes.size(); ++i) {
        JS_SetPropertyUint32(ctx, added_arr, static_cast<uint32_t>(i),
                           wrap_element(ctx, added_nodes[i]));
    }
    JS_SetPropertyStr(ctx, record, "addedNodes", added_arr);

    // Create removedNodes array
    JSValue removed_arr = JS_NewArray(ctx);
    for (size_t i = 0; i < removed_nodes.size(); ++i) {
        JS_SetPropertyUint32(ctx, removed_arr, static_cast<uint32_t>(i),
                           wrap_element(ctx, removed_nodes[i]));
    }
    JS_SetPropertyStr(ctx, record, "removedNodes", removed_arr);

    // Siblings
    JS_SetPropertyStr(ctx, record, "previousSibling",
                     previous_sibling ? wrap_element(ctx, previous_sibling) : JS_NULL);
    JS_SetPropertyStr(ctx, record, "nextSibling",
                     next_sibling ? wrap_element(ctx, next_sibling) : JS_NULL);

    // Attribute mutation info
    if (type == "attributes") {
        JS_SetPropertyStr(ctx, record, "attributeName",
                         JS_NewString(ctx, attr_name.c_str()));
        JS_SetPropertyStr(ctx, record, "attributeNamespace", JS_NULL);
        JS_SetPropertyStr(ctx, record, "oldValue",
                         !old_value.empty() ? JS_NewString(ctx, old_value.c_str()) : JS_NULL);
    } else {
        JS_SetPropertyStr(ctx, record, "attributeName", JS_NULL);
        JS_SetPropertyStr(ctx, record, "attributeNamespace", JS_NULL);
        JS_SetPropertyStr(ctx, record, "oldValue", JS_NULL);
    }

    return record;
}

// Helper: notify all mutation observers of a mutation
static void notify_mutation_observers(JSContext* ctx,
                                      DOMState* state,
                                      const std::string& type,
                                      clever::html::SimpleNode* target,
                                      const std::vector<clever::html::SimpleNode*>& added_nodes,
                                      const std::vector<clever::html::SimpleNode*>& removed_nodes,
                                      clever::html::SimpleNode* previous_sibling,
                                      clever::html::SimpleNode* next_sibling,
                                      const std::string& attr_name,
                                      const std::string& old_value) {
    if (!state || !target) return;

    for (auto& entry : state->mutation_observers) {
        bool should_notify = false;

        // Check if this observer is watching the target
        for (auto* observed : entry.observed_targets) {
            if (observed == target) {
                should_notify = true;
                break;
            }
            // Check subtree
            if (entry.watch_subtree && is_ancestor(observed, target)) {
                should_notify = true;
                break;
            }
        }

        if (!should_notify) continue;

        // Check mutation type against observer options
        bool matches = false;
        if (type == "childList" && entry.watch_child_list) {
            matches = true;
        } else if (type == "attributes" && entry.watch_attributes) {
            // Check attribute filter if present
            if (!entry.attribute_filter.empty()) {
                for (const auto& filtered : entry.attribute_filter) {
                    if (filtered == attr_name) {
                        matches = true;
                        break;
                    }
                }
            } else {
                matches = true;
            }
        } else if (type == "characterData" && entry.watch_character_data) {
            matches = true;
        }

        if (!matches) continue;

        // Create mutation record
        JSValue record = create_mutation_record(ctx, type, target,
                                              added_nodes, removed_nodes,
                                              previous_sibling, next_sibling,
                                              attr_name, old_value);

        // Queue for async delivery
        DOMState::PendingMutation pm;
        pm.observer_obj = JS_DupValue(ctx, entry.observer_obj);
        pm.callback = JS_DupValue(ctx, entry.callback);
        pm.mutation_records.push_back(record);
        state->pending_mutations.push_back(std::move(pm));
    }
}

// Helper: flush all pending mutation callbacks
static void flush_mutation_observers(JSContext* ctx, DOMState* state) {
    if (!state) return;

    while (!state->pending_mutations.empty()) {
        auto pm = std::move(state->pending_mutations.front());
        state->pending_mutations.erase(state->pending_mutations.begin());

        // Create array of mutation records for this callback batch
        JSValue records_arr = JS_NewArray(ctx);
        for (size_t i = 0; i < pm.mutation_records.size(); ++i) {
            JS_SetPropertyUint32(ctx, records_arr, static_cast<uint32_t>(i),
                               pm.mutation_records[i]);
        }

        // Call the callback with (records, observer)
        JSValue args[2] = { records_arr, pm.observer_obj };
        JSValue ret = JS_Call(ctx, pm.callback, JS_UNDEFINED, 2, args);
        if (JS_IsException(ret)) {
            JS_FreeValue(ctx, ret);
        }
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, pm.observer_obj);
        JS_FreeValue(ctx, pm.callback);
    }
}

static void js_mutation_observer_finalizer(JSRuntime* rt, JSValue val) {
    (void)rt;
    (void)val;
}

static JSClassDef mutation_observer_class_def = {
    "MutationObserver",
    js_mutation_observer_finalizer,
    nullptr, nullptr, nullptr
};

// MutationObserver.prototype.observe(target, config)
static JSValue js_mutation_observer_observe(JSContext* ctx,
                                             JSValueConst this_val,
                                             int argc,
                                             JSValueConst* argv) {
    auto* target_node = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!target_node || argc < 2) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // Parse options object
    JSValue options = argv[1];
    bool watch_child_list = false, watch_attributes = false, watch_character_data = false;
    bool watch_subtree = false, record_attr_old = false, record_char_old = false;
    std::vector<std::string> attr_filter;

    if (JS_IsObject(options)) {
        JSValue val;

        val = JS_GetPropertyStr(ctx, options, "childList");
        watch_child_list = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "attributes");
        watch_attributes = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "characterData");
        watch_character_data = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "subtree");
        watch_subtree = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "attributeOldValue");
        record_attr_old = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "characterDataOldValue");
        record_char_old = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, options, "attributeFilter");
        if (JS_IsArray(ctx, val)) {
            JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
            uint32_t len = 0;
            JS_ToUint32(ctx, &len, len_val);
            JS_FreeValue(ctx, len_val);
            for (uint32_t i = 0; i < len; ++i) {
                JSValue item = JS_GetPropertyUint32(ctx, val, i);
                const char* str = JS_ToCString(ctx, item);
                if (str) attr_filter.push_back(str);
                JS_FreeCString(ctx, str);
                JS_FreeValue(ctx, item);
            }
        }
        JS_FreeValue(ctx, val);
    }

    // Find or create observer entry for this observer object
    for (auto& entry : state->mutation_observers) {
        if (JS_StrictEq(ctx, entry.observer_obj, this_val)) {
            // Update existing observer
            entry.observed_targets.push_back(target_node);
            entry.watch_child_list = watch_child_list;
            entry.watch_attributes = watch_attributes;
            entry.watch_character_data = watch_character_data;
            entry.watch_subtree = watch_subtree;
            entry.record_attribute_old_value = record_attr_old;
            entry.record_character_data_old_value = record_char_old;
            entry.attribute_filter = attr_filter;

            // Store old attribute values if needed
            if (record_attr_old) {
                auto& old_vals = entry.old_attribute_values[target_node];
                for (const auto& attr : target_node->attributes) {
                    old_vals[attr.name] = attr.value;
                }
            }

            return JS_UNDEFINED;
        }
    }

    // Create new observer entry
    DOMState::MutationObserverEntry new_entry;
    new_entry.observer_obj = JS_DupValue(ctx, this_val);
    new_entry.callback = JS_GetPropertyStr(ctx, this_val, "_callback");
    new_entry.observed_targets.push_back(target_node);
    new_entry.watch_child_list = watch_child_list;
    new_entry.watch_attributes = watch_attributes;
    new_entry.watch_character_data = watch_character_data;
    new_entry.watch_subtree = watch_subtree;
    new_entry.record_attribute_old_value = record_attr_old;
    new_entry.record_character_data_old_value = record_char_old;
    new_entry.attribute_filter = attr_filter;

    if (record_attr_old) {
        auto& old_vals = new_entry.old_attribute_values[target_node];
        for (const auto& attr : target_node->attributes) {
            old_vals[attr.name] = attr.value;
        }
    }

    state->mutation_observers.push_back(std::move(new_entry));
    return JS_UNDEFINED;
}

// MutationObserver.prototype.disconnect()
static JSValue js_mutation_observer_disconnect(JSContext* ctx,
                                                JSValueConst this_val,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    // Remove this observer from the list
    for (auto it = state->mutation_observers.begin();
         it != state->mutation_observers.end(); ++it) {
        if (JS_StrictEq(ctx, it->observer_obj, this_val)) {
            JS_FreeValue(ctx, it->observer_obj);
            JS_FreeValue(ctx, it->callback);
            state->mutation_observers.erase(it);
            break;
        }
    }

    return JS_UNDEFINED;
}

// MutationObserver.prototype.takeRecords()
static JSValue js_mutation_observer_take_records(JSContext* ctx,
                                                  JSValueConst this_val,
                                                  int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NewArray(ctx);

    // Find pending mutations for this observer and return them
    JSValue records_arr = JS_NewArray(ctx);
    uint32_t count = 0;

    auto it = state->pending_mutations.begin();
    while (it != state->pending_mutations.end()) {
        if (JS_StrictEq(ctx, it->observer_obj, this_val)) {
            for (auto& record : it->mutation_records) {
                JS_SetPropertyUint32(ctx, records_arr, count++, JS_DupValue(ctx, record));
                JS_FreeValue(ctx, record);
            }
            JS_FreeValue(ctx, it->observer_obj);
            JS_FreeValue(ctx, it->callback);
            it = state->pending_mutations.erase(it);
        } else {
            ++it;
        }
    }

    return records_arr;
}

// MutationObserver constructor: new MutationObserver(callback)
static JSValue js_mutation_observer_constructor(JSContext* ctx,
                                                 JSValueConst /*new_target*/,
                                                 int argc,
                                                 JSValueConst* argv) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(mutation_observer_class_id));
    if (JS_IsException(obj)) return obj;

    // Store callback (even if not provided, store undefined)
    if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
        JS_SetPropertyStr(ctx, obj, "_callback", JS_DupValue(ctx, argv[0]));
    }

    // Attach methods
    JS_SetPropertyStr(ctx, obj, "observe",
        JS_NewCFunction(ctx, js_mutation_observer_observe, "observe", 2));
    JS_SetPropertyStr(ctx, obj, "disconnect",
        JS_NewCFunction(ctx, js_mutation_observer_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, obj, "takeRecords",
        JS_NewCFunction(ctx, js_mutation_observer_take_records, "takeRecords", 0));

    return obj;
}

// =========================================================================
// document.elementFromPoint(x, y) — real hit-testing via layout_geometry
// =========================================================================

static JSValue js_document_element_from_point(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc,
                                               JSValueConst* argv) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;

    // Parse (x, y) from arguments
    double px = 0, py = 0;
    if (argc >= 1) JS_ToFloat64(ctx, &px, argv[0]);
    if (argc >= 2) JS_ToFloat64(ctx, &py, argv[1]);

    // Walk the layout_geometry map to collect all SimpleNode* whose border box
    // contains (px, py) and that are not filtered out by pointer-events/visibility.
    // We pick the deepest candidate — the one with the most ancestors in the DOM tree.
    clever::html::SimpleNode* best = nullptr;
    int best_depth = -1;

    for (auto& [key, lr] : state->layout_geometry) {
        // key is void* but was stored as SimpleNode*
        auto* snode = static_cast<clever::html::SimpleNode*>(key);
        if (!snode) continue;

        // Skip elements with pointer-events: none
        if (lr.pointer_events == 1) continue;

        // Skip elements with visibility: hidden
        if (lr.visibility_hidden) continue;

        // Compute border-box rectangle from the LayoutRect.
        // Use the precomputed abs_border_x/y (border-box top-left in page coords).
        float box_x = lr.abs_border_x;
        float box_y = lr.abs_border_y;
        float box_w = lr.border_left + lr.padding_left + lr.width
                    + lr.padding_right + lr.border_right;
        float box_h = lr.border_top + lr.padding_top + lr.height
                    + lr.padding_bottom + lr.border_bottom;

        // Skip zero-size elements
        if (box_w <= 0 || box_h <= 0) continue;

        // Hit test
        if (px < box_x || px >= box_x + box_w) continue;
        if (py < box_y || py >= box_y + box_h) continue;

        // Compute depth by walking the parent chain
        int depth = 0;
        for (auto* p = snode->parent; p != nullptr; p = p->parent) {
            depth++;
        }

        if (depth > best_depth) {
            best_depth = depth;
            best = snode;
        }
    }

    if (best) return wrap_element(ctx, best);

    // Fallback: return document.body
    auto* body = state->root->find_element("body");
    return wrap_element(ctx, body);
}

// =========================================================================
// element.getAttributeNames() — returns array of attribute name strings
// =========================================================================

static JSValue js_element_get_attribute_names(JSContext* ctx,
                                               JSValueConst this_val,
                                               int /*argc*/,
                                               JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NewArray(ctx);
    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < node->attributes.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, i,
            JS_NewString(ctx, node->attributes[i].name.c_str()));
    }
    return arr;
}

// =========================================================================
// element.isConnected — walk parent chain to check if in document tree
// =========================================================================

static JSValue js_element_get_is_connected(JSContext* ctx,
                                            JSValueConst this_val,
                                            int /*argc*/,
                                            JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_FALSE;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_FALSE;

    // Walk up the parent chain to see if we reach the root
    auto* current = node;
    while (current) {
        if (current == state->root) return JS_TRUE;
        current = current->parent;
    }
    return JS_FALSE;
}

// =========================================================================
// DOMParser — parseFromString(string, mimeType) -> document-like object
// =========================================================================

static JSValue js_domparser_parse_from_string(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc,
                                               JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    const char* html_str = JS_ToCString(ctx, argv[0]);
    if (!html_str) return JS_NULL;

    // Parse the HTML string into a SimpleNode tree
    auto parsed = clever::html::parse(html_str);
    JS_FreeCString(ctx, html_str);

    if (!parsed) return JS_NULL;

    auto* parsed_root = parsed.get();
    state->owned_nodes.push_back(std::move(parsed));

    // Build a document-like JS object with body, head, documentElement,
    // querySelector, querySelectorAll, getElementById, getElementsByTagName, title
    JSValue doc_obj = JS_NewObject(ctx);

    // Store the parsed root as opaque data for query functions
    // We use a wrapper element to hold the root pointer
    JSValue root_el = wrap_element(ctx, parsed_root);

    // body
    auto* body_node = parsed_root->find_element("body");
    if (body_node) {
        JS_SetPropertyStr(ctx, doc_obj, "body", wrap_element(ctx, body_node));
    } else {
        JS_SetPropertyStr(ctx, doc_obj, "body", JS_NULL);
    }

    // head
    auto* head_node = parsed_root->find_element("head");
    if (head_node) {
        JS_SetPropertyStr(ctx, doc_obj, "head", wrap_element(ctx, head_node));
    } else {
        JS_SetPropertyStr(ctx, doc_obj, "head", JS_NULL);
    }

    // documentElement (the <html> element)
    auto* html_node = parsed_root->find_element("html");
    if (html_node) {
        JS_SetPropertyStr(ctx, doc_obj, "documentElement", wrap_element(ctx, html_node));
    } else {
        JS_SetPropertyStr(ctx, doc_obj, "documentElement", wrap_element(ctx, parsed_root));
    }

    // title
    auto* title_node = parsed_root->find_element("title");
    if (title_node) {
        JS_SetPropertyStr(ctx, doc_obj, "title",
            JS_NewString(ctx, title_node->text_content().c_str()));
    } else {
        JS_SetPropertyStr(ctx, doc_obj, "title", JS_NewString(ctx, ""));
    }

    // Store the root pointer as an internal property for query functions
    JS_SetPropertyStr(ctx, doc_obj, "__parsedRoot", root_el);

    // querySelector / querySelectorAll / getElementById / getElementsByTagName
    // via JS closures that capture the root element
    const char* query_code = R"JS(
(function(doc, rootEl) {
    doc.querySelector = function(sel) {
        if (!rootEl) return null;
        return rootEl.querySelector(sel);
    };
    doc.querySelectorAll = function(sel) {
        if (!rootEl) return [];
        return rootEl.querySelectorAll(sel);
    };
    doc.getElementById = function(id) {
        if (!rootEl) return null;
        // Walk children to find by id
        var all = rootEl.querySelectorAll('#' + id);
        return all.length > 0 ? all[0] : null;
    };
    doc.getElementsByTagName = function(tag) {
        if (!rootEl) return [];
        return rootEl.querySelectorAll(tag);
    };
})
)JS";
    JSValue query_fn = JS_Eval(ctx, query_code, std::strlen(query_code),
                                "<domparser-query>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, query_fn)) {
        JSValue args[2] = { doc_obj, root_el };
        JSValue ret = JS_Call(ctx, query_fn, JS_UNDEFINED, 2, args);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, query_fn);

    return doc_obj;
}

// DOMParser constructor: new DOMParser()
static JSValue js_domparser_constructor(JSContext* ctx,
                                         JSValueConst /*new_target*/,
                                         int /*argc*/,
                                         JSValueConst* /*argv*/) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "parseFromString",
        JS_NewCFunction(ctx, js_domparser_parse_from_string,
                        "parseFromString", 2));
    return obj;
}

// =========================================================================
// No-op element methods (scrollIntoView, scroll, focus, blur, animate, etc.)
//
// Many websites call these methods. Since our renderer is synchronous
// and doesn't have live scrolling or focus management, these are safe no-ops.
// =========================================================================

static JSValue js_element_scroll_into_view(JSContext* /*ctx*/,
                                            JSValueConst /*this_val*/,
                                            int /*argc*/,
                                            JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

static JSValue js_element_scroll_to(JSContext* /*ctx*/,
                                     JSValueConst /*this_val*/,
                                     int /*argc*/,
                                     JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

static JSValue js_element_scroll(JSContext* /*ctx*/,
                                  JSValueConst /*this_val*/,
                                  int /*argc*/,
                                  JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

static JSValue js_element_focus(JSContext* ctx,
                                 JSValueConst this_val,
                                 int /*argc*/,
                                 JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;
    do_focus_element(ctx, state, node);
    return JS_UNDEFINED;
}

static JSValue js_element_blur(JSContext* ctx,
                                JSValueConst this_val,
                                int /*argc*/,
                                JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;
    // Only blur if this element is actually focused
    if (state->focused_element == node) {
        do_blur_element(ctx, state, node);
    }
    return JS_UNDEFINED;
}

static JSValue js_element_get_animations(JSContext* ctx,
                                          JSValueConst /*this_val*/,
                                          int /*argc*/,
                                          JSValueConst* /*argv*/) {
    // Return empty array
    return JS_NewArray(ctx);
}

// animate() — returns a minimal Animation-like object with play/pause/cancel/finish
static JSValue js_animation_noop(JSContext* /*ctx*/,
                                  JSValueConst /*this_val*/,
                                  int /*argc*/,
                                  JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

static JSValue js_element_animate(JSContext* ctx,
                                   JSValueConst /*this_val*/,
                                   int /*argc*/,
                                   JSValueConst* /*argv*/) {
    // Return a minimal Animation-like object
    JSValue anim = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, anim, "play",
        JS_NewCFunction(ctx, js_animation_noop, "play", 0));
    JS_SetPropertyStr(ctx, anim, "pause",
        JS_NewCFunction(ctx, js_animation_noop, "pause", 0));
    JS_SetPropertyStr(ctx, anim, "cancel",
        JS_NewCFunction(ctx, js_animation_noop, "cancel", 0));
    JS_SetPropertyStr(ctx, anim, "finish",
        JS_NewCFunction(ctx, js_animation_noop, "finish", 0));
    JS_SetPropertyStr(ctx, anim, "reverse",
        JS_NewCFunction(ctx, js_animation_noop, "reverse", 0));
    JS_SetPropertyStr(ctx, anim, "playState", JS_NewString(ctx, "finished"));
    JS_SetPropertyStr(ctx, anim, "currentTime", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, anim, "playbackRate", JS_NewFloat64(ctx, 1));
    JS_SetPropertyStr(ctx, anim, "effect", JS_NULL);
    JS_SetPropertyStr(ctx, anim, "timeline", JS_NULL);
    JS_SetPropertyStr(ctx, anim, "onfinish", JS_NULL);
    JS_SetPropertyStr(ctx, anim, "oncancel", JS_NULL);
    JS_SetPropertyStr(ctx, anim, "id", JS_NewString(ctx, ""));
    // finished / ready as resolved promises
    {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue promise_ctor = JS_GetPropertyStr(ctx, global, "Promise");
        if (JS_IsFunction(ctx, promise_ctor)) {
            JSValue resolve_fn = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
            if (JS_IsFunction(ctx, resolve_fn)) {
                JSValue resolved = JS_Call(ctx, resolve_fn, promise_ctor, 0, nullptr);
                JS_SetPropertyStr(ctx, anim, "finished", JS_DupValue(ctx, resolved));
                JS_SetPropertyStr(ctx, anim, "ready", resolved);
            }
            JS_FreeValue(ctx, resolve_fn);
        }
        JS_FreeValue(ctx, promise_ctor);
        JS_FreeValue(ctx, global);
    }
    return anim;
}

// =========================================================================
// Modern DOM manipulation methods: before, after, prepend, append,
// replaceWith, toggleAttribute, insertAdjacentElement
// =========================================================================

// Helper: detach a node from its current parent or owned_nodes list and
// insert it into `target_siblings` at `insert_pos`.  Returns true on success.
static bool detach_and_insert(DOMState* state,
                              clever::html::SimpleNode* elem,
                              clever::html::SimpleNode* new_parent,
                              std::vector<std::unique_ptr<clever::html::SimpleNode>>& target_siblings,
                              size_t insert_pos) {
    if (elem->parent) {
        auto& old_siblings = elem->parent->children;
        for (auto it = old_siblings.begin(); it != old_siblings.end(); ++it) {
            if (it->get() == elem) {
                auto moved = std::move(*it);
                old_siblings.erase(it);
                moved->parent = new_parent;
                target_siblings.insert(
                    target_siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
                    std::move(moved));
                return true;
            }
        }
    } else {
        // Check owned_nodes
        for (auto it = state->owned_nodes.begin(); it != state->owned_nodes.end(); ++it) {
            if (it->get() == elem) {
                auto moved = std::move(*it);
                state->owned_nodes.erase(it);
                moved->parent = new_parent;
                target_siblings.insert(
                    target_siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
                    std::move(moved));
                return true;
            }
        }
    }
    return false;
}

// Helper: insert a text node into target_siblings at insert_pos
static void insert_text_node(clever::html::SimpleNode* parent,
                             std::vector<std::unique_ptr<clever::html::SimpleNode>>& target_siblings,
                             size_t insert_pos, const std::string& text) {
    auto text_node = std::make_unique<clever::html::SimpleNode>();
    text_node->type = clever::html::SimpleNode::Text;
    text_node->data = text;
    text_node->parent = parent;
    target_siblings.insert(
        target_siblings.begin() + static_cast<ptrdiff_t>(insert_pos),
        std::move(text_node));
}

// --- element.before(...nodes) ---
static JSValue js_element_before(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state || !node->parent) return JS_UNDEFINED;

    auto& siblings = node->parent->children;
    size_t idx = 0;
    for (size_t i = 0; i < siblings.size(); i++) {
        if (siblings[i].get() == node) { idx = i; break; }
    }

    for (int i = argc - 1; i >= 0; i--) {
        auto* elem = unwrap_element(ctx, argv[i]);
        if (elem) {
            detach_and_insert(state, elem, node->parent, siblings, idx);
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                insert_text_node(node->parent, siblings, idx, str);
                JS_FreeCString(ctx, str);
            }
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- element.after(...nodes) ---
static JSValue js_element_after(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state || !node->parent) return JS_UNDEFINED;

    auto& siblings = node->parent->children;
    size_t idx = 0;
    for (size_t i = 0; i < siblings.size(); i++) {
        if (siblings[i].get() == node) { idx = i + 1; break; }
    }

    for (int i = argc - 1; i >= 0; i--) {
        auto* elem = unwrap_element(ctx, argv[i]);
        if (elem) {
            detach_and_insert(state, elem, node->parent, siblings, idx);
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                insert_text_node(node->parent, siblings, idx, str);
                JS_FreeCString(ctx, str);
            }
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- element.prepend(...nodes) ---
static JSValue js_element_prepend(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state) return JS_UNDEFINED;

    size_t insert_pos = 0;
    for (int i = 0; i < argc; i++) {
        auto* elem = unwrap_element(ctx, argv[i]);
        if (elem) {
            if (detach_and_insert(state, elem, node, node->children, insert_pos))
                insert_pos++;
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                insert_text_node(node, node->children, insert_pos, str);
                insert_pos++;
                JS_FreeCString(ctx, str);
            }
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- element.append(...nodes) ---
static JSValue js_element_append(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state) return JS_UNDEFINED;

    for (int i = 0; i < argc; i++) {
        auto* elem = unwrap_element(ctx, argv[i]);
        if (elem) {
            detach_and_insert(state, elem, node, node->children, node->children.size());
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                insert_text_node(node, node->children, node->children.size(), str);
                JS_FreeCString(ctx, str);
            }
        }
    }
    state->modified = true;
    return JS_UNDEFINED;
}

// --- element.replaceWith(...nodes) ---
static JSValue js_element_replace_with(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* state = get_dom_state(ctx);
    if (!node || !state || !node->parent) return JS_UNDEFINED;

    auto* parent = node->parent;
    auto& siblings = parent->children;

    // Find this node's index
    size_t idx = 0;
    for (size_t i = 0; i < siblings.size(); i++) {
        if (siblings[i].get() == node) { idx = i; break; }
    }

    // First, insert all replacement nodes after this element
    size_t insert_pos = idx + 1;
    for (int i = 0; i < argc; i++) {
        auto* elem = unwrap_element(ctx, argv[i]);
        if (elem && elem != node) {
            if (detach_and_insert(state, elem, parent, siblings, insert_pos))
                insert_pos++;
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                insert_text_node(parent, siblings, insert_pos, str);
                insert_pos++;
                JS_FreeCString(ctx, str);
            }
        }
    }

    // Now remove the original node (re-find it since indices may have shifted)
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == node) {
            auto owned = std::move(*it);
            siblings.erase(it);
            owned->parent = nullptr;
            state->owned_nodes.push_back(std::move(owned));
            break;
        }
    }

    state->modified = true;
    return JS_UNDEFINED;
}

// --- element.toggleAttribute(name [, force]) ---
static JSValue js_element_toggle_attribute(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    if (argc < 1) return JS_FALSE;
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_FALSE;
    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_FALSE;

    bool has_it = has_attr(*node, std::string(name));
    bool should_have = (argc > 1) ? (JS_ToBool(ctx, argv[1]) != 0) : !has_it;

    if (should_have && !has_it) {
        node->attributes.push_back({std::string(name), ""});
    } else if (!should_have && has_it) {
        node->attributes.erase(
            std::remove_if(node->attributes.begin(), node->attributes.end(),
                [&](const auto& a) { return a.name == name; }),
            node->attributes.end());
    }

    auto* state = get_dom_state(ctx);
    if (state) state->modified = true;

    JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, should_have);
}

// --- element.insertAdjacentElement(position, element) ---
static JSValue js_element_insert_adjacent_element(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int argc,
                                                    JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    const char* pos_cstr = JS_ToCString(ctx, argv[0]);
    if (!pos_cstr) return JS_NULL;
    std::string position(pos_cstr);
    JS_FreeCString(ctx, pos_cstr);

    auto* elem = unwrap_element(ctx, argv[1]);
    if (!elem) return JS_NULL;

    if (position == "beforebegin") {
        if (!node->parent) return JS_NULL;
        auto& siblings = node->parent->children;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_NULL;
        detach_and_insert(state, elem, node->parent, siblings, static_cast<size_t>(idx));
    } else if (position == "afterbegin") {
        detach_and_insert(state, elem, node, node->children, 0);
    } else if (position == "beforeend") {
        detach_and_insert(state, elem, node, node->children, node->children.size());
    } else if (position == "afterend") {
        if (!node->parent) return JS_NULL;
        auto& siblings = node->parent->children;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_NULL;
        detach_and_insert(state, elem, node->parent, siblings, static_cast<size_t>(idx) + 1);
    } else {
        return JS_NULL;
    }

    state->modified = true;
    return wrap_element(ctx, elem);
}

// =========================================================================
// Node.hasChildNodes()
// =========================================================================

static JSValue js_element_has_child_nodes(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_FALSE;
    return JS_NewBool(ctx, !node->children.empty());
}

// =========================================================================
// Node.getRootNode()
// =========================================================================

static JSValue js_element_get_root_node(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;
    auto* current = node;
    while (current->parent) {
        current = current->parent;
    }
    return wrap_element(ctx, current);
}

// =========================================================================
// Node.isSameNode(other)
// =========================================================================

static JSValue js_element_is_same_node(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* other = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!node) return JS_FALSE;
    return JS_NewBool(ctx, node == other);
}

// =========================================================================
// Node.compareDocumentPosition(other)
// =========================================================================

static JSValue js_element_compare_document_position(JSContext* ctx,
                                                      JSValueConst this_val,
                                                      int argc,
                                                      JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* other = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!node || !other) return JS_NewInt32(ctx, 0);
    if (node == other) return JS_NewInt32(ctx, 0);

    // Check if other is a descendant of this (CONTAINS=16 | FOLLOWING=4 = 20)
    if (contains_impl(node, other)) return JS_NewInt32(ctx, 20);
    // Check if other is an ancestor of this (CONTAINED_BY=8 | PRECEDING=2 = 10)
    if (contains_impl(other, node)) return JS_NewInt32(ctx, 10);
    // Otherwise: DISCONNECTED=1 | PRECEDING=2 = 35 (or simplified)
    return JS_NewInt32(ctx, 35);
}

// =========================================================================
// element.attachShadow({mode: 'open'|'closed'})
// =========================================================================

static JSValue js_element_attach_shadow(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    // Check if already has a shadow root
    if (state->shadow_roots.count(node)) {
        // Per spec: throw a NotSupportedError
        return JS_ThrowTypeError(ctx, "Element already has a shadow root");
    }

    // Parse options: {mode: 'open'|'closed'}
    bool is_closed = false;
    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue mode_val = JS_GetPropertyStr(ctx, argv[0], "mode");
        const char* mode_str = JS_ToCString(ctx, mode_val);
        if (mode_str) {
            is_closed = (std::string(mode_str) == "closed");
            JS_FreeCString(ctx, mode_str);
        }
        JS_FreeValue(ctx, mode_val);
    }

    // Create shadow root node (a document-fragment-like node)
    auto shadow = std::make_unique<clever::html::SimpleNode>();
    shadow->type = clever::html::SimpleNode::Document;
    shadow->tag_name = "#shadow-root";
    shadow->parent = node;

    auto* raw_ptr = shadow.get();
    state->owned_nodes.push_back(std::move(shadow));
    state->shadow_roots[node] = raw_ptr;
    if (is_closed) {
        state->closed_shadow_roots.insert(raw_ptr);
    }
    state->modified = true;

    return wrap_element(ctx, raw_ptr);
}

// =========================================================================
// element.shadowRoot (getter) — returns shadow root if mode='open', else null
// =========================================================================

static JSValue js_element_get_shadow_root(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    auto it = state->shadow_roots.find(node);
    if (it == state->shadow_roots.end()) return JS_NULL;

    // If the shadow root is closed, return null
    if (state->closed_shadow_roots.count(it->second)) return JS_NULL;

    return wrap_element(ctx, it->second);
}

// =========================================================================
// Node.normalize() — merge adjacent text nodes, remove empty text nodes
// =========================================================================

static JSValue js_node_normalize(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);

    // Walk children and normalize
    for (auto it = node->children.begin(); it != node->children.end(); ) {
        auto& child = *it;
        if (child->type == clever::html::SimpleNode::Text) {
            // Remove empty text nodes
            if (child->data.empty()) {
                it = node->children.erase(it);
                if (state) state->modified = true;
                continue;
            }
            // Merge with subsequent adjacent text nodes
            auto next_it = std::next(it);
            while (next_it != node->children.end() &&
                   (*next_it)->type == clever::html::SimpleNode::Text) {
                child->data += (*next_it)->data;
                next_it = node->children.erase(next_it);
                if (state) state->modified = true;
            }
            ++it;
        } else {
            ++it;
        }
    }

    return JS_UNDEFINED;
}

// =========================================================================
// Node.isEqualNode(other) — deep comparison
// =========================================================================

static bool is_equal_node_impl(const clever::html::SimpleNode* a,
                                const clever::html::SimpleNode* b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) return false;
    if (a->tag_name != b->tag_name) return false;
    if (a->data != b->data) return false;
    if (a->doctype_name != b->doctype_name) return false;

    // Compare attributes (order-independent)
    if (a->attributes.size() != b->attributes.size()) return false;
    for (const auto& attr_a : a->attributes) {
        bool found = false;
        for (const auto& attr_b : b->attributes) {
            if (attr_a.name == attr_b.name && attr_a.value == attr_b.value) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Compare children recursively
    if (a->children.size() != b->children.size()) return false;
    for (size_t i = 0; i < a->children.size(); i++) {
        if (!is_equal_node_impl(a->children[i].get(), b->children[i].get()))
            return false;
    }
    return true;
}

static JSValue js_node_is_equal_node(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    auto* other = (argc > 0) ? unwrap_element(ctx, argv[0]) : nullptr;
    if (!node) return JS_FALSE;
    if (!other) return JS_NewBool(ctx, false);
    return JS_NewBool(ctx, is_equal_node_impl(node, other));
}

// =========================================================================
// document.adoptNode(node) — adopts a node (in our engine, just returns it)
// =========================================================================

static JSValue js_document_adopt_node(JSContext* ctx,
                                       JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* node = unwrap_element(ctx, argv[0]);
    if (!node) return JS_NULL;

    // Per spec: remove from parent if attached
    if (node->parent) {
        auto* old_parent = node->parent;
        auto* state = get_dom_state(ctx);
        for (auto it = old_parent->children.begin();
             it != old_parent->children.end(); ++it) {
            if (it->get() == node) {
                auto owned = std::move(*it);
                old_parent->children.erase(it);
                owned->parent = nullptr;
                if (state) {
                    state->owned_nodes.push_back(std::move(owned));
                    state->modified = true;
                }
                break;
            }
        }
    }

    return wrap_element(ctx, node);
}

// =========================================================================
// Element.insertAdjacentText(position, text)
// =========================================================================

static JSValue js_element_insert_adjacent_text(JSContext* ctx,
                                                JSValueConst this_val,
                                                int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;

    const char* pos_cstr = JS_ToCString(ctx, argv[0]);
    const char* text_cstr = JS_ToCString(ctx, argv[1]);
    if (!pos_cstr || !text_cstr) {
        if (pos_cstr) JS_FreeCString(ctx, pos_cstr);
        if (text_cstr) JS_FreeCString(ctx, text_cstr);
        return JS_UNDEFINED;
    }
    std::string position(pos_cstr);
    std::string text(text_cstr);
    JS_FreeCString(ctx, pos_cstr);
    JS_FreeCString(ctx, text_cstr);

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    auto text_node = std::make_unique<clever::html::SimpleNode>();
    text_node->type = clever::html::SimpleNode::Text;
    text_node->data = text;

    if (position == "beforebegin") {
        if (!node->parent) return JS_UNDEFINED;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_UNDEFINED;
        text_node->parent = node->parent;
        node->parent->children.insert(
            node->parent->children.begin() + idx,
            std::move(text_node));
    } else if (position == "afterbegin") {
        text_node->parent = node;
        node->children.insert(node->children.begin(), std::move(text_node));
    } else if (position == "beforeend") {
        text_node->parent = node;
        node->children.push_back(std::move(text_node));
    } else if (position == "afterend") {
        if (!node->parent) return JS_UNDEFINED;
        int idx = find_sibling_index(node);
        if (idx < 0) return JS_UNDEFINED;
        text_node->parent = node->parent;
        node->parent->children.insert(
            node->parent->children.begin() + idx + 1,
            std::move(text_node));
    }

    state->modified = true;
    return JS_UNDEFINED;
}

// =========================================================================
// document.createComment(text)
// =========================================================================

static JSValue js_document_create_comment(JSContext* ctx,
                                           JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;

    std::string text;
    if (argc > 0) {
        const char* text_cstr = JS_ToCString(ctx, argv[0]);
        if (text_cstr) {
            text = text_cstr;
            JS_FreeCString(ctx, text_cstr);
        }
    }

    auto node = std::make_unique<clever::html::SimpleNode>();
    node->type = clever::html::SimpleNode::Comment;
    node->data = text;

    auto* raw_ptr = node.get();
    state->owned_nodes.push_back(std::move(node));
    return wrap_element(ctx, raw_ptr);
}

// =========================================================================
// document.importNode(node, deep)
// =========================================================================

static JSValue js_document_import_node(JSContext* ctx,
                                        JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    auto* source = unwrap_element(ctx, argv[0]);
    if (!source) return JS_NULL;

    bool deep = false;
    if (argc > 1) {
        deep = JS_ToBool(ctx, argv[1]);
    }

    auto clone = clone_node_impl(source, deep);
    auto* raw_ptr = clone.get();

    auto* state = get_dom_state(ctx);
    if (!state) return JS_NULL;
    state->owned_nodes.push_back(std::move(clone));
    return wrap_element(ctx, raw_ptr);
}

// =========================================================================
// document.forms / document.images / document.links / document.scripts
// =========================================================================

static JSValue js_document_get_collection_by_tag(JSContext* ctx,
                                                   const std::string& tag) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewArray(ctx);

    std::vector<clever::html::SimpleNode*> results;
    find_by_tag(state->root, tag, results);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < results.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             wrap_element(ctx, results[i]));
    }
    return arr;
}

static JSValue js_document_get_forms(JSContext* ctx,
                                      JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    return js_document_get_collection_by_tag(ctx, "form");
}

static JSValue js_document_get_images(JSContext* ctx,
                                       JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    return js_document_get_collection_by_tag(ctx, "img");
}

static JSValue js_document_get_links(JSContext* ctx,
                                      JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NewArray(ctx);

    std::vector<clever::html::SimpleNode*> a_results;
    std::vector<clever::html::SimpleNode*> area_results;
    find_by_tag(state->root, "a", a_results);
    find_by_tag(state->root, "area", area_results);

    // Only include elements with href attribute
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (auto* el : a_results) {
        if (has_attr(*el, "href")) {
            JS_SetPropertyUint32(ctx, arr, idx++, wrap_element(ctx, el));
        }
    }
    for (auto* el : area_results) {
        if (has_attr(*el, "href")) {
            JS_SetPropertyUint32(ctx, arr, idx++, wrap_element(ctx, el));
        }
    }
    return arr;
}

static JSValue js_document_get_scripts(JSContext* ctx,
                                        JSValueConst /*this_val*/,
                                        int /*argc*/, JSValueConst* /*argv*/) {
    return js_document_get_collection_by_tag(ctx, "script");
}

// =========================================================================
// document.activeElement — returns the focused element, or body if none
// =========================================================================

static JSValue js_document_get_active_element(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int /*argc*/,
                                               JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;
    // Return the focused element if one is tracked
    if (state->focused_element) {
        return wrap_element(ctx, state->focused_element);
    }
    // Fall back to document.body per spec
    auto* body = state->root->find_element("body");
    if (body) return wrap_element(ctx, body);
    return JS_NULL;
}

// =========================================================================
// element.hidden (getter/setter)
// =========================================================================

static JSValue js_element_get_hidden(JSContext* ctx, JSValueConst this_val,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_FALSE;
    return JS_NewBool(ctx, has_attr(*node, "hidden"));
}

static JSValue js_element_set_hidden(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_UNDEFINED;

    bool should_hide = JS_ToBool(ctx, argv[0]);
    auto* state = get_dom_state(ctx);

    if (should_hide) {
        if (!has_attr(*node, "hidden")) {
            set_attr(*node, "hidden", "");
        }
    } else {
        // Remove the hidden attribute
        for (auto it = node->attributes.begin(); it != node->attributes.end(); ++it) {
            if (it->name == "hidden") {
                node->attributes.erase(it);
                break;
            }
        }
    }
    if (state) state->modified = true;
    return JS_UNDEFINED;
}

// =========================================================================
// element.offsetParent
//
// Returns the nearest ancestor element that has a CSS position other than
// static (i.e., relative/absolute/fixed/sticky), or the <body> element if
// no such ancestor exists.  Returns null for fixed-position elements and
// for elements not in the layout tree.
// =========================================================================

static JSValue js_element_get_offset_parent(JSContext* ctx,
                                              JSValueConst this_val,
                                              int /*argc*/,
                                              JSValueConst* /*argv*/) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NULL;
    auto* state = get_dom_state(ctx);
    if (!state || !state->root) return JS_NULL;

    // If the element itself is fixed-position, offsetParent is null
    auto self_it = state->layout_geometry.find(node);
    if (self_it != state->layout_geometry.end() &&
        self_it->second.position_type == 3 /* fixed */) {
        return JS_NULL;
    }

    // Walk up via parent_dom_node chain looking for a positioned ancestor
    if (!state->layout_geometry.empty() && self_it != state->layout_geometry.end()) {
        void* parent_ptr = self_it->second.parent_dom_node;
        while (parent_ptr) {
            auto pit = state->layout_geometry.find(parent_ptr);
            if (pit == state->layout_geometry.end()) break;
            int ptype = pit->second.position_type;
            // position_type 1=relative, 2=absolute, 3=fixed, 4=sticky
            if (ptype != 0) {
                return wrap_element(ctx,
                    static_cast<clever::html::SimpleNode*>(parent_ptr));
            }
            parent_ptr = pit->second.parent_dom_node;
        }
    }

    // Fall back to <body>
    auto* body = state->root->find_element("body");
    if (body) return wrap_element(ctx, body);
    return JS_NULL;
}

// =========================================================================
// IntersectionObserver — real implementation
//
// Tracks observed elements. After layout geometry is populated, computes
// intersection ratios against the viewport and fires callbacks with
// IntersectionObserverEntry objects.
// =========================================================================

// Each IntersectionObserver JS object stores an index into the DOMState
// intersection_observers vector via "_io_index" property.

static void js_intersection_observer_finalizer(JSRuntime* rt, JSValue val) {
    (void)rt;
    (void)val;
}

static JSClassDef intersection_observer_class_def = {
    "IntersectionObserver",
    js_intersection_observer_finalizer,
    nullptr, nullptr, nullptr
};

static JSValue js_intersection_observer_observe(JSContext* ctx,
                                                 JSValueConst this_val,
                                                 int argc,
                                                 JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* elem = unwrap_element(ctx, argv[0]);
    if (!elem) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_io_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->intersection_observers.size())) {
        auto& entry = state->intersection_observers[idx];
        // Don't add duplicates
        for (auto* e : entry.observed_elements) {
            if (e == elem) return JS_UNDEFINED;
        }
        entry.observed_elements.push_back(elem);

        // Fire initial callback with a not-intersecting entry (spec behavior)
        if (JS_IsFunction(ctx, entry.callback)) {
            JSValue entries_arr = JS_NewArray(ctx);
            JSValue init_entry = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, init_entry, "target", wrap_element(ctx, elem));
            JS_SetPropertyStr(ctx, init_entry, "isIntersecting", JS_FALSE);
            JS_SetPropertyStr(ctx, init_entry, "intersectionRatio", JS_NewFloat64(ctx, 0.0));
            JS_SetPropertyStr(ctx, init_entry, "time", JS_NewFloat64(ctx, 0.0));
            JS_SetPropertyStr(ctx, init_entry, "rootBounds", JS_NULL);

            // boundingClientRect
            JSValue bcr = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, bcr, "x", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "y", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "width", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "height", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "top", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "left", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "bottom", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, bcr, "right", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, init_entry, "boundingClientRect", bcr);

            // intersectionRect
            JSValue ir = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, ir, "x", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "y", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "width", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "height", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "top", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "left", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "bottom", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, ir, "right", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, init_entry, "intersectionRect", ir);

            JS_SetPropertyUint32(ctx, entries_arr, 0, init_entry);

            JSValue args[2] = { entries_arr, entry.observer_obj };
            JSValue ret = JS_Call(ctx, entry.callback, JS_UNDEFINED, 2, args);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, entries_arr);
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_intersection_observer_unobserve(JSContext* ctx,
                                                   JSValueConst this_val,
                                                   int argc,
                                                   JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* elem = unwrap_element(ctx, argv[0]);
    if (!elem) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_io_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->intersection_observers.size())) {
        auto& entry = state->intersection_observers[idx];
        entry.observed_elements.erase(
            std::remove(entry.observed_elements.begin(), entry.observed_elements.end(), elem),
            entry.observed_elements.end());
    }
    return JS_UNDEFINED;
}

static JSValue js_intersection_observer_disconnect(JSContext* ctx,
                                                    JSValueConst this_val,
                                                    int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_io_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->intersection_observers.size())) {
        state->intersection_observers[idx].observed_elements.clear();
    }
    return JS_UNDEFINED;
}

static JSValue js_intersection_observer_take_records(JSContext* ctx,
                                                      JSValueConst /*this_val*/,
                                                      int /*argc*/,
                                                      JSValueConst* /*argv*/) {
    return JS_NewArray(ctx);
}

// Parse rootMargin string like "10px 20px 30px 40px" or "10px"
static void parse_root_margin(const std::string& s, float& top, float& right,
                               float& bottom, float& left) {
    top = right = bottom = left = 0;
    std::istringstream iss(s);
    std::vector<float> vals;
    std::string token;
    while (iss >> token) {
        float v = 0;
        try { v = std::stof(token); } catch (...) {}
        vals.push_back(v);
    }
    if (vals.size() >= 1) top = right = bottom = left = vals[0];
    if (vals.size() >= 2) { right = left = vals[1]; }
    if (vals.size() >= 3) { bottom = vals[2]; }
    if (vals.size() >= 4) { left = vals[3]; }
}

static JSValue js_intersection_observer_constructor(JSContext* ctx,
                                                     JSValueConst /*new_target*/,
                                                     int argc,
                                                     JSValueConst* argv) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(intersection_observer_class_id));
    if (JS_IsException(obj)) return obj;

    auto* state = get_dom_state(ctx);

    DOMState::IntersectionObserverEntry io_entry;
    io_entry.observer_obj = JS_DupValue(ctx, obj);
    io_entry.thresholds.push_back(0.0f);  // default threshold

    if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
        io_entry.callback = JS_DupValue(ctx, argv[0]);
        JS_SetPropertyStr(ctx, obj, "_callback", JS_DupValue(ctx, argv[0]));
    } else {
        io_entry.callback = JS_UNDEFINED;
    }

    // Parse options (second argument)
    if (argc > 1 && JS_IsObject(argv[1])) {
        // rootMargin
        JSValue rm = JS_GetPropertyStr(ctx, argv[1], "rootMargin");
        if (JS_IsString(rm)) {
            const char* rm_str = JS_ToCString(ctx, rm);
            if (rm_str) {
                parse_root_margin(rm_str, io_entry.root_margin_top,
                                  io_entry.root_margin_right,
                                  io_entry.root_margin_bottom,
                                  io_entry.root_margin_left);
                JS_FreeCString(ctx, rm_str);
            }
        }
        JS_FreeValue(ctx, rm);

        // threshold (number or array)
        JSValue th = JS_GetPropertyStr(ctx, argv[1], "threshold");
        if (JS_IsNumber(th)) {
            double v = 0;
            JS_ToFloat64(ctx, &v, th);
            io_entry.thresholds.clear();
            io_entry.thresholds.push_back(static_cast<float>(v));
        } else if (JS_IsArray(ctx, th)) {
            io_entry.thresholds.clear();
            JSValue len_val = JS_GetPropertyStr(ctx, th, "length");
            int32_t len = 0;
            JS_ToInt32(ctx, &len, len_val);
            JS_FreeValue(ctx, len_val);
            for (int32_t i = 0; i < len; i++) {
                JSValue item = JS_GetPropertyUint32(ctx, th, i);
                double v = 0;
                JS_ToFloat64(ctx, &v, item);
                io_entry.thresholds.push_back(static_cast<float>(v));
                JS_FreeValue(ctx, item);
            }
        }
        JS_FreeValue(ctx, th);
    }

    int32_t io_index = state ? static_cast<int32_t>(state->intersection_observers.size()) : -1;
    if (state) {
        state->intersection_observers.push_back(std::move(io_entry));
    }

    JS_SetPropertyStr(ctx, obj, "_io_index", JS_NewInt32(ctx, io_index));

    JS_SetPropertyStr(ctx, obj, "observe",
        JS_NewCFunction(ctx, js_intersection_observer_observe, "observe", 1));
    JS_SetPropertyStr(ctx, obj, "unobserve",
        JS_NewCFunction(ctx, js_intersection_observer_unobserve, "unobserve", 1));
    JS_SetPropertyStr(ctx, obj, "disconnect",
        JS_NewCFunction(ctx, js_intersection_observer_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, obj, "takeRecords",
        JS_NewCFunction(ctx, js_intersection_observer_take_records, "takeRecords", 0));

    return obj;
}

// =========================================================================
// ResizeObserver — real implementation
//
// Tracks observed elements. After layout geometry is populated, computes
// content/border box sizes and fires callbacks with ResizeObserverEntry
// objects containing contentRect, contentBoxSize, borderBoxSize, and target.
// =========================================================================

// Each ResizeObserver JS object stores an index into the DOMState
// resize_observers vector via "_ro_index" property.

static void js_resize_observer_finalizer(JSRuntime* rt, JSValue val) {
    (void)rt;
    (void)val;
}

static JSClassDef resize_observer_class_def = {
    "ResizeObserver",
    js_resize_observer_finalizer,
    nullptr, nullptr, nullptr
};

static JSValue js_resize_observer_observe(JSContext* ctx,
                                           JSValueConst this_val,
                                           int argc,
                                           JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* elem = unwrap_element(ctx, argv[0]);
    if (!elem) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_ro_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->resize_observers.size())) {
        auto& entry = state->resize_observers[idx];
        // Don't add duplicates
        for (auto* e : entry.observed_elements) {
            if (e == elem) return JS_UNDEFINED;
        }
        entry.observed_elements.push_back(elem);
    }
    return JS_UNDEFINED;
}

static JSValue js_resize_observer_unobserve(JSContext* ctx,
                                             JSValueConst this_val,
                                             int argc,
                                             JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    auto* elem = unwrap_element(ctx, argv[0]);
    if (!elem) return JS_UNDEFINED;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_ro_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->resize_observers.size())) {
        auto& entry = state->resize_observers[idx];
        entry.observed_elements.erase(
            std::remove(entry.observed_elements.begin(), entry.observed_elements.end(), elem),
            entry.observed_elements.end());
    }
    return JS_UNDEFINED;
}

static JSValue js_resize_observer_disconnect(JSContext* ctx,
                                              JSValueConst this_val,
                                              int /*argc*/,
                                              JSValueConst* /*argv*/) {
    auto* state = get_dom_state(ctx);
    if (!state) return JS_UNDEFINED;

    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_ro_index");
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    if (idx >= 0 && idx < static_cast<int32_t>(state->resize_observers.size())) {
        state->resize_observers[idx].observed_elements.clear();
    }
    return JS_UNDEFINED;
}

static JSValue js_resize_observer_constructor(JSContext* ctx,
                                               JSValueConst /*new_target*/,
                                               int argc,
                                               JSValueConst* argv) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(resize_observer_class_id));
    if (JS_IsException(obj)) return obj;

    auto* state = get_dom_state(ctx);

    DOMState::ResizeObserverEntry ro_entry;
    ro_entry.observer_obj = JS_DupValue(ctx, obj);

    if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
        ro_entry.callback = JS_DupValue(ctx, argv[0]);
        JS_SetPropertyStr(ctx, obj, "_callback", JS_DupValue(ctx, argv[0]));
    } else {
        ro_entry.callback = JS_UNDEFINED;
    }

    int32_t ro_index = state ? static_cast<int32_t>(state->resize_observers.size()) : -1;
    if (state) {
        state->resize_observers.push_back(std::move(ro_entry));
    }

    JS_SetPropertyStr(ctx, obj, "_ro_index", JS_NewInt32(ctx, ro_index));

    JS_SetPropertyStr(ctx, obj, "observe",
        JS_NewCFunction(ctx, js_resize_observer_observe, "observe", 1));
    JS_SetPropertyStr(ctx, obj, "unobserve",
        JS_NewCFunction(ctx, js_resize_observer_unobserve, "unobserve", 1));
    JS_SetPropertyStr(ctx, obj, "disconnect",
        JS_NewCFunction(ctx, js_resize_observer_disconnect, "disconnect", 0));

    return obj;
}

// =========================================================================
// CustomEvent constructor
// =========================================================================

static JSValue js_custom_event_constructor(JSContext* ctx,
                                            JSValueConst /*new_target*/,
                                            int argc,
                                            JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type argument (required)
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type",
            JS_NewString(ctx, ""));
    }

    // Default values
    JS_SetPropertyStr(ctx, event_obj, "detail", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // options argument (optional)
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue detail = JS_GetPropertyStr(ctx, argv[1], "detail");
        if (!JS_IsUndefined(detail)) {
            JS_SetPropertyStr(ctx, event_obj, "detail", detail);
        } else {
            JS_FreeValue(ctx, detail);
        }

        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
            JS_FreeValue(ctx, bubbles);
        } else {
            JS_FreeValue(ctx, bubbles);
        }

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
            JS_FreeValue(ctx, cancelable);
        } else {
            JS_FreeValue(ctx, cancelable);
        }
    }

    // Add preventDefault / stopPropagation methods
    const char* evt_methods = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
        })
    )JS";
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase", JS_NewInt32(ctx, 0));
    JSValue setup_fn = JS_Eval(ctx, evt_methods, std::strlen(evt_methods),
                                "<custom-event-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);

    return event_obj;
}

// =========================================================================
// Helper: attach standard event methods via JS eval (shared by all event ctors)
// =========================================================================

static void attach_event_methods(JSContext* ctx, JSValue event_obj) {
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "timeStamp",
        JS_NewFloat64(ctx, 0));

    const char* evt_methods = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
        })
    )JS";
    JSValue setup_fn = JS_Eval(ctx, evt_methods, std::strlen(evt_methods),
                                "<event-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);
}

// =========================================================================
// Event constructor: new Event(type, {bubbles, cancelable})
// =========================================================================

static JSValue js_event_constructor(JSContext* ctx,
                                     JSValueConst /*new_target*/,
                                     int argc,
                                     JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type argument (required)
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type",
            JS_NewString(ctx, ""));
    }

    // Defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // options argument (optional)
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// KeyboardEvent constructor: new KeyboardEvent(type, options)
// =========================================================================

static JSValue js_keyboard_event_constructor(JSContext* ctx,
                                              JSValueConst /*new_target*/,
                                              int argc,
                                              JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "key", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "code", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "keyCode", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "charCode", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "which", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "shiftKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "altKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "metaKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "repeat", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "location", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "isComposing", JS_FALSE);

    // options
    if (argc > 1 && JS_IsObject(argv[1])) {
        auto read_bool = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewBool(ctx, JS_ToBool(ctx, v)));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_str = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                const char* s = JS_ToCString(ctx, v);
                if (s) {
                    JS_SetPropertyStr(ctx, event_obj, name,
                        JS_NewString(ctx, s));
                    JS_FreeCString(ctx, s);
                }
            }
            JS_FreeValue(ctx, v);
        };
        auto read_int = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                int32_t ival = 0;
                JS_ToInt32(ctx, &ival, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewInt32(ctx, ival));
            }
            JS_FreeValue(ctx, v);
        };

        read_bool("bubbles");
        read_bool("cancelable");
        read_str("key");
        read_str("code");
        read_int("keyCode");
        read_int("charCode");
        read_int("location");
        read_bool("ctrlKey");
        read_bool("shiftKey");
        read_bool("altKey");
        read_bool("metaKey");
        read_bool("repeat");
        read_bool("isComposing");

        // 'which' defaults to keyCode if not explicitly provided
        JSValue which_val = JS_GetPropertyStr(ctx, argv[1], "which");
        if (!JS_IsUndefined(which_val)) {
            int32_t wval = 0;
            JS_ToInt32(ctx, &wval, which_val);
            JS_SetPropertyStr(ctx, event_obj, "which",
                JS_NewInt32(ctx, wval));
        } else {
            // Fall back to keyCode value
            JSValue kc = JS_GetPropertyStr(ctx, event_obj, "keyCode");
            JS_SetPropertyStr(ctx, event_obj, "which",
                JS_DupValue(ctx, kc));
            JS_FreeValue(ctx, kc);
        }
        JS_FreeValue(ctx, which_val);
    }

    // Add getModifierState method
    const char* gms_code = R"JS(
        (function() {
            var evt = this;
            evt.getModifierState = function(key) {
                if (key === 'Control') return evt.ctrlKey;
                if (key === 'Shift') return evt.shiftKey;
                if (key === 'Alt') return evt.altKey;
                if (key === 'Meta') return evt.metaKey;
                return false;
            };
        })
    )JS";
    JSValue gms_fn = JS_Eval(ctx, gms_code, std::strlen(gms_code),
                              "<keyboard-event-gms>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, gms_fn)) {
        JSValue gms_ret = JS_Call(ctx, gms_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, gms_ret);
    }
    JS_FreeValue(ctx, gms_fn);

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// MouseEvent constructor: new MouseEvent(type, options)
// =========================================================================

static JSValue js_mouse_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc,
                                           JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "button", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "buttons", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "shiftKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "altKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "metaKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "relatedTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "movementX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "movementY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "detail", JS_NewInt32(ctx, 0));

    // options
    if (argc > 1 && JS_IsObject(argv[1])) {
        auto read_bool = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewBool(ctx, JS_ToBool(ctx, v)));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_num = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                double dval = 0;
                JS_ToFloat64(ctx, &dval, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewFloat64(ctx, dval));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_int = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                int32_t ival = 0;
                JS_ToInt32(ctx, &ival, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewInt32(ctx, ival));
            }
            JS_FreeValue(ctx, v);
        };

        read_bool("bubbles");
        read_bool("cancelable");
        read_int("button");
        read_int("buttons");
        read_num("clientX");
        read_num("clientY");
        read_num("screenX");
        read_num("screenY");
        read_num("pageX");
        read_num("pageY");
        read_num("offsetX");
        read_num("offsetY");
        read_bool("ctrlKey");
        read_bool("shiftKey");
        read_bool("altKey");
        read_bool("metaKey");
        read_num("movementX");
        read_num("movementY");
        read_int("detail");

        // relatedTarget: pass through JS object reference
        JSValue rt = JS_GetPropertyStr(ctx, argv[1], "relatedTarget");
        if (!JS_IsUndefined(rt)) {
            JS_SetPropertyStr(ctx, event_obj, "relatedTarget",
                JS_DupValue(ctx, rt));
        }
        JS_FreeValue(ctx, rt);
    }

    attach_event_methods(ctx, event_obj);

    // Add getModifierState method (MouseEvent-specific)
    const char* gms_code = R"JS(
        (function() {
            var evt = this;
            evt.getModifierState = function(key) {
                if (key === 'Control') return evt.ctrlKey;
                if (key === 'Shift') return evt.shiftKey;
                if (key === 'Alt') return evt.altKey;
                if (key === 'Meta') return evt.metaKey;
                return false;
            };
        })
    )JS";
    JSValue gms_fn = JS_Eval(ctx, gms_code, std::strlen(gms_code),
                              "<mouse-gms>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, gms_fn)) {
        JSValue gms_ret = JS_Call(ctx, gms_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, gms_ret);
    }
    JS_FreeValue(ctx, gms_fn);

    return event_obj;
}

// =========================================================================
// PointerEvent constructor: new PointerEvent(type, options)
// Extends MouseEvent with pointer-specific properties.
// =========================================================================

static JSValue js_pointer_event_constructor(JSContext* ctx,
                                             JSValueConst /*new_target*/,
                                             int argc,
                                             JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // MouseEvent defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "button", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "buttons", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "shiftKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "altKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "metaKey", JS_FALSE);

    // PointerEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "pointerId", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "width", JS_NewFloat64(ctx, 1));
    JS_SetPropertyStr(ctx, event_obj, "height", JS_NewFloat64(ctx, 1));
    JS_SetPropertyStr(ctx, event_obj, "pressure", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "tangentialPressure", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "tiltX", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "tiltY", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "twist", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pointerType", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "isPrimary", JS_FALSE);

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        auto read_bool = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewBool(ctx, JS_ToBool(ctx, v)));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_num = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                double dval = 0;
                JS_ToFloat64(ctx, &dval, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewFloat64(ctx, dval));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_int = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                int32_t ival = 0;
                JS_ToInt32(ctx, &ival, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewInt32(ctx, ival));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_str = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                const char* s = JS_ToCString(ctx, v);
                if (s) {
                    JS_SetPropertyStr(ctx, event_obj, name,
                        JS_NewString(ctx, s));
                    JS_FreeCString(ctx, s);
                }
            }
            JS_FreeValue(ctx, v);
        };

        // MouseEvent properties
        read_bool("bubbles");
        read_bool("cancelable");
        read_int("button");
        read_int("buttons");
        read_num("clientX");
        read_num("clientY");
        read_num("screenX");
        read_num("screenY");
        read_num("pageX");
        read_num("pageY");
        read_num("offsetX");
        read_num("offsetY");
        read_bool("ctrlKey");
        read_bool("shiftKey");
        read_bool("altKey");
        read_bool("metaKey");

        // PointerEvent-specific properties
        read_int("pointerId");
        read_num("width");
        read_num("height");
        read_num("pressure");
        read_num("tangentialPressure");
        read_int("tiltX");
        read_int("tiltY");
        read_int("twist");
        read_str("pointerType");
        read_bool("isPrimary");
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// FocusEvent constructor: new FocusEvent(type, options)
// Extends Event with relatedTarget property.
// =========================================================================

static JSValue js_focus_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc,
                                           JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // FocusEvent-specific default
    JS_SetPropertyStr(ctx, event_obj, "relatedTarget", JS_NULL);

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue rt = JS_GetPropertyStr(ctx, argv[1], "relatedTarget");
        if (!JS_IsUndefined(rt)) {
            JS_SetPropertyStr(ctx, event_obj, "relatedTarget",
                JS_DupValue(ctx, rt));
        }
        JS_FreeValue(ctx, rt);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// InputEvent constructor: new InputEvent(type, options)
// Extends Event with data, inputType, isComposing properties.
// =========================================================================

static JSValue js_input_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc,
                                           JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // InputEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "data", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "inputType", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "isComposing", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "dataTransfer", JS_NULL);

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue data = JS_GetPropertyStr(ctx, argv[1], "data");
        if (!JS_IsUndefined(data)) {
            const char* s = JS_ToCString(ctx, data);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "data",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, data);

        JSValue inputType = JS_GetPropertyStr(ctx, argv[1], "inputType");
        if (!JS_IsUndefined(inputType)) {
            const char* s = JS_ToCString(ctx, inputType);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "inputType",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, inputType);

        JSValue isComposing = JS_GetPropertyStr(ctx, argv[1], "isComposing");
        if (!JS_IsUndefined(isComposing)) {
            JS_SetPropertyStr(ctx, event_obj, "isComposing",
                JS_NewBool(ctx, JS_ToBool(ctx, isComposing)));
        }
        JS_FreeValue(ctx, isComposing);

        JSValue dataTransfer = JS_GetPropertyStr(ctx, argv[1], "dataTransfer");
        if (!JS_IsUndefined(dataTransfer)) {
            JS_SetPropertyStr(ctx, event_obj, "dataTransfer",
                JS_DupValue(ctx, dataTransfer));
        }
        JS_FreeValue(ctx, dataTransfer);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// initEvent(type, bubbles, cancelable) — static C function for createEvent
// =========================================================================

static JSValue js_init_event(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, this_val, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        }
    }
    if (argc > 1) {
        JS_SetPropertyStr(ctx, this_val, "bubbles",
            JS_NewBool(ctx, JS_ToBool(ctx, argv[1])));
    }
    if (argc > 2) {
        JS_SetPropertyStr(ctx, this_val, "cancelable",
            JS_NewBool(ctx, JS_ToBool(ctx, argv[2])));
    }
    return JS_UNDEFINED;
}

// =========================================================================
// document.createEvent(type)
// =========================================================================

static JSValue js_document_create_event(JSContext* ctx,
                                          JSValueConst /*this_val*/,
                                          int argc, JSValueConst* argv) {
    // The type parameter is usually "Event", "MouseEvent", etc.
    // For now we create a basic event object regardless of type.
    (void)argc;
    (void)argv;

    JSValue event_obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // initEvent(type, bubbles, cancelable)
    JS_SetPropertyStr(ctx, event_obj, "initEvent",
        JS_NewCFunction(ctx, js_init_event, "initEvent", 3));

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// Shared event propagation helpers
// =========================================================================

// Determine if an event type bubbles by default
static bool event_type_bubbles(const std::string& type) {
    // Events that do NOT bubble (per W3C spec)
    static const std::vector<std::string> non_bubbling = {
        "focus", "blur", "load", "unload", "scroll", "resize",
        "mouseenter", "mouseleave",   // mouse enter/leave never bubble
        "pointerenter", "pointerleave"
    };
    for (const auto& nb : non_bubbling) {
        if (type == nb) return false;
    }
    return true;
}

// Check if __stopped is set on the event object
static bool is_event_stopped(JSContext* ctx, JSValue event_obj) {
    JSValue stopped = JS_GetPropertyStr(ctx, event_obj, "__stopped");
    bool result = JS_ToBool(ctx, stopped) != 0;
    JS_FreeValue(ctx, stopped);
    return result;
}

// Check if __immediate_stopped is set on the event object
static bool is_event_immediate_stopped(JSContext* ctx, JSValue event_obj) {
    JSValue stopped = JS_GetPropertyStr(ctx, event_obj, "__immediate_stopped");
    bool result = JS_ToBool(ctx, stopped) != 0;
    JS_FreeValue(ctx, stopped);
    return result;
}

// Build the ancestor chain from target up to root (excluding target itself).
// Returns: [window_sentinel, root(document), html, body, ..., parent]
// i.e., ordered from outermost to innermost (capture order).
static std::vector<clever::html::SimpleNode*> build_ancestor_chain(
        DOMState* /*state*/, clever::html::SimpleNode* target) {
    std::vector<clever::html::SimpleNode*> chain;
    // Walk from target's parent up to root
    clever::html::SimpleNode* current = target->parent;
    while (current) {
        chain.push_back(current);
        current = current->parent;
    }
    // chain is currently [parent, ..., html, root]
    // Reverse to get [root, html, ..., parent] (capture order)
    std::reverse(chain.begin(), chain.end());
    return chain;
}

// Invoke listeners on a specific node for a given event type and phase.
// phase: 1 = CAPTURING_PHASE, 2 = AT_TARGET, 3 = BUBBLING_PHASE
// For capture phase (1): only call listeners with use_capture == true
// For target phase (2): call ALL listeners (both capture and bubble)
// For bubble phase (3): only call listeners with use_capture == false
static void invoke_listeners_on_node(JSContext* ctx, DOMState* state,
                                      clever::html::SimpleNode* node,
                                      const std::string& event_type,
                                      JSValue event_obj, int phase,
                                      JSValue current_target_proxy) {
    // Set currentTarget and eventPhase
    JS_SetPropertyStr(ctx, event_obj, "currentTarget",
        JS_DupValue(ctx, current_target_proxy));
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, phase));

    auto node_it = state->listeners.find(node);
    if (node_it == state->listeners.end()) return;
    auto type_it = node_it->second.find(event_type);
    if (type_it == node_it->second.end()) return;

    // Copy the list in case handlers modify it
    auto entries = type_it->second;
    for (auto& entry : entries) {
        if (is_event_immediate_stopped(ctx, event_obj)) break;

        // Phase filtering
        if (phase == 1 && !entry.use_capture) continue; // capture only
        if (phase == 3 && entry.use_capture) continue;   // bubble only
        // phase == 2 (AT_TARGET): call all listeners

        JSValue result = JS_Call(ctx, entry.handler, current_target_proxy,
                                 1, &event_obj);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, result);

        // Handle {once: true} — remove after first invocation
        if (entry.once) {
            auto& real_entries = type_it->second;
            for (auto it = real_entries.begin(); it != real_entries.end(); ++it) {
                if (JS_VALUE_GET_TAG(it->handler) == JS_VALUE_GET_TAG(entry.handler) &&
                    JS_VALUE_GET_PTR(it->handler) == JS_VALUE_GET_PTR(entry.handler) &&
                    it->use_capture == entry.use_capture) {
                    JS_FreeValue(ctx, it->handler);
                    real_entries.erase(it);
                    break;
                }
            }
        }
    }
}

// Full three-phase event dispatch.
// Returns true if defaultPrevented was set.
static bool dispatch_event_propagated(JSContext* ctx, DOMState* state,
                                       clever::html::SimpleNode* target,
                                       JSValue event_obj,
                                       const std::string& event_type,
                                       bool bubbles) {
    // Set target on the event
    JSValue target_proxy = wrap_element(ctx, target);
    JS_SetPropertyStr(ctx, event_obj, "target",
        JS_DupValue(ctx, target_proxy));

    // Build ancestor chain: [root(document), html, body, ..., parent]
    auto ancestors = build_ancestor_chain(state, target);

    // Build the composedPath array: [target, parent, ..., body, html, root]
    // (i.e., target first, then ancestors in reverse)
    JSValue path_arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, path_arr, 0, JS_DupValue(ctx, target_proxy));
    for (size_t i = 0; i < ancestors.size(); i++) {
        // ancestors are in capture order (root first), we need target-first
        size_t rev_idx = ancestors.size() - 1 - i;
        JSValue anc_proxy = wrap_element(ctx, ancestors[rev_idx]);
        JS_SetPropertyUint32(ctx, path_arr, static_cast<uint32_t>(i + 1),
                             anc_proxy);
    }

    // Store composedPath as a method on the event
    JS_SetPropertyStr(ctx, event_obj, "__composedPathArray", path_arr);

    // --- Phase 1: CAPTURING_PHASE ---
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 1));
    for (auto* ancestor : ancestors) {
        if (is_event_stopped(ctx, event_obj)) break;
        JSValue anc_proxy = wrap_element(ctx, ancestor);
        invoke_listeners_on_node(ctx, state, ancestor, event_type,
                                 event_obj, 1, anc_proxy);
        JS_FreeValue(ctx, anc_proxy);
    }

    // --- Phase 2: AT_TARGET ---
    if (!is_event_stopped(ctx, event_obj)) {
        invoke_listeners_on_node(ctx, state, target, event_type,
                                 event_obj, 2, target_proxy);
    }

    // --- Phase 3: BUBBLING_PHASE ---
    if (bubbles && !is_event_stopped(ctx, event_obj)) {
        JS_SetPropertyStr(ctx, event_obj, "eventPhase",
            JS_NewInt32(ctx, 3));
        // Walk ancestors in reverse (parent first, root last)
        for (int i = static_cast<int>(ancestors.size()) - 1; i >= 0; i--) {
            if (is_event_stopped(ctx, event_obj)) break;
            JSValue anc_proxy = wrap_element(ctx, ancestors[static_cast<size_t>(i)]);
            invoke_listeners_on_node(ctx, state, ancestors[static_cast<size_t>(i)],
                                     event_type, event_obj, 3, anc_proxy);
            JS_FreeValue(ctx, anc_proxy);
        }
    }

    // Set eventPhase to NONE after dispatch
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 0));

    JS_FreeValue(ctx, target_proxy);

    // Check defaultPrevented
    JSValue prevented = JS_GetPropertyStr(ctx, event_obj, "defaultPrevented");
    bool default_prevented = JS_ToBool(ctx, prevented) != 0;
    JS_FreeValue(ctx, prevented);

    return default_prevented;
}

// =========================================================================
// Default actions: after event propagation completes without preventDefault,
// execute the browser's built-in behavior for the element/event combination.
// =========================================================================

static void execute_default_action(JSContext* ctx, DOMState* state,
                                    clever::html::SimpleNode* target,
                                    const std::string& event_type) {
    if (!target || !state) return;

    // Helper: case-insensitive tag comparison (tags are typically lowercase)
    auto tag_lower = [](const std::string& t) -> std::string {
        std::string result = t;
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    };

    std::string tag = tag_lower(target->tag_name);

    if (event_type == "click") {
        // ------------------------------------------------------------------
        // 1. Click on <a href="..."> → navigate via window.location.href
        //    Walk up from target to find the nearest <a> ancestor with href.
        // ------------------------------------------------------------------
        {
            clever::html::SimpleNode* link = target;
            while (link) {
                if (link->type == clever::html::SimpleNode::Element &&
                    tag_lower(link->tag_name) == "a" &&
                    has_attr(*link, "href")) {
                    std::string href = get_attr(*link, "href");
                    // Set window.location.href to trigger navigation
                    JSValue global = JS_GetGlobalObject(ctx);
                    JSValue loc = JS_GetPropertyStr(ctx, global, "location");
                    if (JS_IsObject(loc)) {
                        JS_SetPropertyStr(ctx, loc, "href",
                            JS_NewString(ctx, href.c_str()));
                    }
                    JS_FreeValue(ctx, loc);
                    JS_FreeValue(ctx, global);
                    break;
                }
                link = link->parent;
            }
        }

        // ------------------------------------------------------------------
        // 2. Click on <input type="submit"> or <button type="submit">
        //    → dispatch 'submit' event on closest parent <form>
        //    → if not prevented, collect form data and navigate
        // ------------------------------------------------------------------
        if ((tag == "input" &&
             tag_lower(get_attr(*target, "type")) == "submit") ||
            (tag == "button" &&
             (get_attr(*target, "type").empty() ||
              tag_lower(get_attr(*target, "type")) == "submit"))) {
            // Walk up to find <form>
            clever::html::SimpleNode* form = target->parent;
            while (form) {
                if (form->type == clever::html::SimpleNode::Element &&
                    tag_lower(form->tag_name) == "form") {
                    // Dispatch a 'submit' event on the form.
                    // Build the event object inline (submit bubbles, is cancelable).
                    JSValue submit_evt = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, submit_evt, "type",
                        JS_NewString(ctx, "submit"));
                    JS_SetPropertyStr(ctx, submit_evt, "bubbles", JS_TRUE);
                    JS_SetPropertyStr(ctx, submit_evt, "cancelable", JS_TRUE);
                    JS_SetPropertyStr(ctx, submit_evt, "defaultPrevented", JS_FALSE);
                    JS_SetPropertyStr(ctx, submit_evt, "eventPhase",
                        JS_NewInt32(ctx, 0));
                    JS_SetPropertyStr(ctx, submit_evt, "target", JS_NULL);
                    JS_SetPropertyStr(ctx, submit_evt, "currentTarget", JS_NULL);
                    JS_SetPropertyStr(ctx, submit_evt, "__stopped", JS_FALSE);
                    JS_SetPropertyStr(ctx, submit_evt, "__immediate_stopped", JS_FALSE);

                    // Install preventDefault/stopPropagation methods
                    const char* method_code = R"JS(
                        (function() {
                            var evt = this;
                            evt.preventDefault = function() { evt.defaultPrevented = true; };
                            evt.stopPropagation = function() { evt.__stopped = true; };
                            evt.stopImmediatePropagation = function() {
                                evt.__stopped = true;
                                evt.__immediate_stopped = true;
                            };
                        })
                    )JS";
                    JSValue setup_fn = JS_Eval(ctx, method_code,
                        std::strlen(method_code), "<submit-evt>", JS_EVAL_TYPE_GLOBAL);
                    if (JS_IsFunction(ctx, setup_fn)) {
                        JSValue setup_ret = JS_Call(ctx, setup_fn, submit_evt,
                                                     0, nullptr);
                        JS_FreeValue(ctx, setup_ret);
                    }
                    JS_FreeValue(ctx, setup_fn);

                    dispatch_event_propagated(ctx, state, form, submit_evt,
                                              "submit", true);

                    // Check if preventDefault was called
                    JSValue prevented_val = JS_GetPropertyStr(ctx, submit_evt, "defaultPrevented");
                    bool submit_prevented = JS_ToBool(ctx, prevented_val) != 0;
                    JS_FreeValue(ctx, prevented_val);

                    // If not prevented, collect form data and submit
                    if (!submit_prevented) {
                        std::string action = get_attr(*form, "action");
                        std::string method_str = tag_lower(get_attr(*form, "method"));
                        if (method_str.empty()) method_str = "get";
                        std::string enctype = get_attr(*form, "enctype");

                        // Collect form data: name=value pairs
                        std::vector<std::pair<std::string, std::string>> form_data;

                        std::function<void(clever::html::SimpleNode*)> collect_inputs =
                            [&](clever::html::SimpleNode* node) {
                            if (!node) return;
                            if (node->type == clever::html::SimpleNode::Element) {
                                std::string node_tag = tag_lower(node->tag_name);
                                if (node_tag == "input") {
                                    std::string input_name = get_attr(*node, "name");
                                    if (!input_name.empty() && !has_attr(*node, "disabled")) {
                                        std::string input_type = tag_lower(get_attr(*node, "type"));
                                        if (input_type == "checkbox" || input_type == "radio") {
                                            if (has_attr(*node, "checked")) {
                                                std::string input_value = get_attr(*node, "value");
                                                if (input_value.empty()) input_value = "on";
                                                form_data.push_back({input_name, input_value});
                                            }
                                        } else if (input_type != "submit" && input_type != "image" &&
                                                   input_type != "button" && input_type != "file") {
                                            std::string input_value = get_attr(*node, "value");
                                            form_data.push_back({input_name, input_value});
                                        }
                                    }
                                } else if (node_tag == "textarea") {
                                    std::string textarea_name = get_attr(*node, "name");
                                    if (!textarea_name.empty() && !has_attr(*node, "disabled")) {
                                        std::string textarea_value = node->text_content();
                                        form_data.push_back({textarea_name, textarea_value});
                                    }
                                } else if (node_tag == "select") {
                                    std::string select_name = get_attr(*node, "name");
                                    if (!select_name.empty() && !has_attr(*node, "disabled")) {
                                        for (auto& child : node->children) {
                                            if (child->type == clever::html::SimpleNode::Element &&
                                                tag_lower(child->tag_name) == "option" &&
                                                has_attr(*child, "selected")) {
                                                std::string opt_value = get_attr(*child, "value");
                                                if (opt_value.empty()) {
                                                    opt_value = child->text_content();
                                                }
                                                form_data.push_back({select_name, opt_value});
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            for (auto& child : node->children) {
                                collect_inputs(child.get());
                            }
                        };

                        collect_inputs(form);

                        // Include submitter button's name/value if it has a name
                        std::string submitter_name = get_attr(*target, "name");
                        if (!submitter_name.empty()) {
                            std::string submitter_value = get_attr(*target, "value");
                            if (submitter_value.empty()) submitter_value = "Submit";
                            form_data.push_back({submitter_name, submitter_value});
                        }

                        // URL encode form data
                        auto url_encode_char = [](unsigned char c) -> std::string {
                            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                                (c >= 'a' && c <= 'z') || c == '-' || c == '_' ||
                                c == '.' || c == '~') {
                                return std::string(1, c);
                            }
                            char buf[4];
                            std::snprintf(buf, sizeof(buf), "%%%02X", c);
                            return buf;
                        };

                        std::string encoded_data;
                        for (size_t i = 0; i < form_data.size(); i++) {
                            if (i > 0) encoded_data += "&";
                            for (unsigned char c : form_data[i].first) {
                                encoded_data += url_encode_char(c);
                            }
                            encoded_data += "=";
                            for (unsigned char c : form_data[i].second) {
                                if (c == ' ') {
                                    encoded_data += "+";
                                } else {
                                    encoded_data += url_encode_char(c);
                                }
                            }
                        }

                        // Determine target URL
                        std::string target_url = action;
                        if (target_url.empty()) {
                            JSValue global = JS_GetGlobalObject(ctx);
                            JSValue loc = JS_GetPropertyStr(ctx, global, "location");
                            if (JS_IsObject(loc)) {
                                JSValue href = JS_GetPropertyStr(ctx, loc, "href");
                                const char* href_cstr = JS_ToCString(ctx, href);
                                if (href_cstr) {
                                    target_url = href_cstr;
                                    JS_FreeCString(ctx, href_cstr);
                                }
                                JS_FreeValue(ctx, href);
                            }
                            JS_FreeValue(ctx, loc);
                            JS_FreeValue(ctx, global);
                        }

                        // Navigate based on method
                        JSValue global = JS_GetGlobalObject(ctx);
                        JSValue loc = JS_GetPropertyStr(ctx, global, "location");
                        if (JS_IsObject(loc)) {
                            if (method_str == "get") {
                                // Append query string to URL
                                std::string final_url = target_url;
                                if (!encoded_data.empty()) {
                                    size_t hash_pos = final_url.find('#');
                                    std::string fragment;
                                    if (hash_pos != std::string::npos) {
                                        fragment = final_url.substr(hash_pos);
                                        final_url = final_url.substr(0, hash_pos);
                                    }
                                    final_url += (final_url.find('?') != std::string::npos ? "&" : "?");
                                    final_url += encoded_data;
                                    final_url += fragment;
                                }
                                JS_SetPropertyStr(ctx, loc, "href",
                                    JS_NewString(ctx, final_url.c_str()));
                            } else {
                                // POST: store form data metadata and navigate
                                JS_SetPropertyStr(ctx, loc, "__formMethod",
                                    JS_NewString(ctx, "POST"));
                                JS_SetPropertyStr(ctx, loc, "__formEnctype",
                                    JS_NewString(ctx, enctype.empty() ? "application/x-www-form-urlencoded" : enctype.c_str()));
                                JS_SetPropertyStr(ctx, loc, "__formData",
                                    JS_NewString(ctx, encoded_data.c_str()));
                                JS_SetPropertyStr(ctx, loc, "href",
                                    JS_NewString(ctx, target_url.c_str()));
                            }
                        }
                        JS_FreeValue(ctx, loc);
                        JS_FreeValue(ctx, global);
                    }

                    JS_FreeValue(ctx, submit_evt);
                    break;
                }
                form = form->parent;
            }
        }

        // ------------------------------------------------------------------
        // 3. Click on <input type="checkbox"> → toggle checked attribute
        // ------------------------------------------------------------------
        if (tag == "input" &&
            tag_lower(get_attr(*target, "type")) == "checkbox") {
            if (has_attr(*target, "checked")) {
                remove_attr(*target, "checked");
            } else {
                set_attr(*target, "checked", "");
            }
            state->modified = true;
        }

        // ------------------------------------------------------------------
        // 4. Click on <summary> inside <details> → toggle open attribute
        // ------------------------------------------------------------------
        if (tag == "summary" && target->parent &&
            target->parent->type == clever::html::SimpleNode::Element &&
            tag_lower(target->parent->tag_name) == "details") {
            clever::html::SimpleNode* details = target->parent;
            if (has_attr(*details, "open")) {
                remove_attr(*details, "open");
            } else {
                set_attr(*details, "open", "");
            }
            state->modified = true;
        }
    }

    // ------------------------------------------------------------------
    // 5. Focus/click on <input>/<textarea>/<select> → mark as focused
    //    Sets a __focused attribute so the rendering pipeline can show
    //    focus indicators.
    // ------------------------------------------------------------------
    if (event_type == "focus" || event_type == "click") {
        if (tag == "input" || tag == "textarea" || tag == "select") {
            set_attr(*target, "__focused", "true");
            state->modified = true;
        }
    }

    // On blur, remove focus marker
    if (event_type == "blur") {
        if (has_attr(*target, "__focused")) {
            remove_attr(*target, "__focused");
            state->modified = true;
        }
    }
}

// =========================================================================
// Focus event helper: create a FocusEvent object with relatedTarget
// =========================================================================

static JSValue create_focus_event_object(JSContext* ctx,
                                          const std::string& event_type,
                                          bool bubbles,
                                          clever::html::SimpleNode* related_target) {
    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type",
        JS_NewString(ctx, event_type.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "bubbles",
        JS_NewBool(ctx, bubbles));
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 0)); // NONE
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // FocusEvent-specific: relatedTarget
    if (related_target) {
        JS_SetPropertyStr(ctx, event_obj, "relatedTarget",
            wrap_element(ctx, related_target));
    } else {
        JS_SetPropertyStr(ctx, event_obj, "relatedTarget", JS_NULL);
    }

    // Hidden propagation state
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);

    // Install methods via eval
    const char* method_code = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
        })
    )JS";
    JSValue setup_fn = JS_Eval(ctx, method_code, std::strlen(method_code),
                                "<focus-event-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);

    return event_obj;
}

// =========================================================================
// do_focus_element / do_blur_element — focus management implementation
//
// Focus sequence (per spec):
//   1. If same element is already focused, do nothing.
//   2. Fire "focusout" (bubbles) on old element, with relatedTarget = new element
//   3. Fire "blur" (does not bubble) on old element, with relatedTarget = new element
//   4. Update focused_element and __focused attributes
//   5. Fire "focusin" (bubbles) on new element, with relatedTarget = old element
//   6. Fire "focus" (does not bubble) on new element, with relatedTarget = old element
// =========================================================================

static void do_focus_element(JSContext* ctx, DOMState* state,
                              clever::html::SimpleNode* new_focus,
                              clever::html::SimpleNode* related) {
    if (!state || !new_focus) return;

    // Skip if element is already focused
    if (state->focused_element == new_focus) return;

    clever::html::SimpleNode* old_focus = state->focused_element;

    // --- Step 1: blur the old element ---
    if (old_focus) {
        // Remove __focused marker
        if (has_attr(*old_focus, "__focused")) {
            remove_attr(*old_focus, "__focused");
        }

        // Fire "focusout" (bubbles) on old element
        {
            JSValue evt = create_focus_event_object(ctx, "focusout", true, new_focus);
            dispatch_event_propagated(ctx, state, old_focus, evt, "focusout", true);
            execute_default_action(ctx, state, old_focus, "focusout");
            JS_FreeValue(ctx, evt);
        }

        // Fire "blur" (does not bubble) on old element
        {
            JSValue evt = create_focus_event_object(ctx, "blur", false, new_focus);
            dispatch_event_propagated(ctx, state, old_focus, evt, "blur", false);
            execute_default_action(ctx, state, old_focus, "blur");
            JS_FreeValue(ctx, evt);
        }
    }

    // --- Step 2: update focused element ---
    state->focused_element = new_focus;

    // Set __focused marker on new element
    set_attr(*new_focus, "__focused", "true");
    state->modified = true;

    // --- Step 3: focus the new element ---
    clever::html::SimpleNode* rel = related ? related : old_focus;

    // Fire "focusin" (bubbles) on new element
    {
        JSValue evt = create_focus_event_object(ctx, "focusin", true, rel);
        dispatch_event_propagated(ctx, state, new_focus, evt, "focusin", true);
        execute_default_action(ctx, state, new_focus, "focusin");
        JS_FreeValue(ctx, evt);
    }

    // Fire "focus" (does not bubble) on new element
    {
        JSValue evt = create_focus_event_object(ctx, "focus", false, rel);
        dispatch_event_propagated(ctx, state, new_focus, evt, "focus", false);
        execute_default_action(ctx, state, new_focus, "focus");
        JS_FreeValue(ctx, evt);
    }
}

static void do_blur_element(JSContext* ctx, DOMState* state,
                             clever::html::SimpleNode* element,
                             clever::html::SimpleNode* related) {
    if (!state || !element) return;
    if (state->focused_element != element) return;

    // Remove __focused marker
    if (has_attr(*element, "__focused")) {
        remove_attr(*element, "__focused");
    }

    // Clear the focused element pointer
    state->focused_element = nullptr;
    state->modified = true;

    // Fire "focusout" (bubbles)
    {
        JSValue evt = create_focus_event_object(ctx, "focusout", true, related);
        dispatch_event_propagated(ctx, state, element, evt, "focusout", true);
        execute_default_action(ctx, state, element, "focusout");
        JS_FreeValue(ctx, evt);
    }

    // Fire "blur" (does not bubble)
    {
        JSValue evt = create_focus_event_object(ctx, "blur", false, related);
        dispatch_event_propagated(ctx, state, element, evt, "blur", false);
        execute_default_action(ctx, state, element, "blur");
        JS_FreeValue(ctx, evt);
    }
}

// =========================================================================
// element.dispatchEvent(event) -- invokes handlers registered via
// addEventListener for the given event type on the element.
// Now uses full three-phase propagation.
// Executes default actions if preventDefault() was not called.
// =========================================================================

static JSValue js_element_dispatch_event(JSContext* ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1 || !JS_IsObject(argv[0])) return JS_FALSE;

    auto* state = get_dom_state(ctx);
    if (!state) return JS_FALSE;

    JSValue event_obj = argv[0];

    // Get the event type from the event object
    JSValue type_val = JS_GetPropertyStr(ctx, event_obj, "type");
    const char* type_cstr = JS_ToCString(ctx, type_val);
    JS_FreeValue(ctx, type_val);
    if (!type_cstr) return JS_FALSE;
    std::string event_type(type_cstr);
    JS_FreeCString(ctx, type_cstr);

    // Determine if event bubbles (from event object or default)
    JSValue bubbles_val = JS_GetPropertyStr(ctx, event_obj, "bubbles");
    bool bubbles = JS_ToBool(ctx, bubbles_val) != 0;
    JS_FreeValue(ctx, bubbles_val);

    bool default_prevented = dispatch_event_propagated(
        ctx, state, node, event_obj, event_type, bubbles);

    // Execute default actions if not prevented
    if (!default_prevented) {
        execute_default_action(ctx, state, node, event_type);
    }

    return JS_NewBool(ctx, !default_prevented);
}

// =========================================================================
// Canvas 2D Rendering Context
// =========================================================================

// ---- Canvas gradient data (stored as C++ struct to avoid JSValue lifetime issues) ----
enum class CanvasGradientType { None, Linear, Radial, Conic };

// ---- Canvas pattern data ----
enum class CanvasPatternRepeat { Repeat, RepeatX, RepeatY, NoRepeat };

struct CanvasPattern {
    std::vector<uint8_t> pixels; // RGBA pixel data
    int width = 0;
    int height = 0;
    CanvasPatternRepeat repeat = CanvasPatternRepeat::Repeat;

    bool active() const { return !pixels.empty() && width > 0 && height > 0; }

    // Sample pattern color at canvas position (px, py), returns RGBA packed as R|G|B|A in 4 bytes
    // Returns false (transparent) if outside tile and repetition mode excludes that direction.
    // Returns ARGB uint32 like gradient (A in high byte).
    uint32_t sample(int px, int py) const {
        if (!active()) return 0x00000000u;
        bool tile_x = (repeat == CanvasPatternRepeat::Repeat || repeat == CanvasPatternRepeat::RepeatX);
        bool tile_y = (repeat == CanvasPatternRepeat::Repeat || repeat == CanvasPatternRepeat::RepeatY);

        int tx = px, ty = py;
        if (tile_x) {
            tx = ((px % width) + width) % width;
        } else {
            if (px < 0 || px >= width) return 0x00000000u; // transparent outside
        }
        if (tile_y) {
            ty = ((py % height) + height) % height;
        } else {
            if (py < 0 || py >= height) return 0x00000000u; // transparent outside
        }

        int idx = (ty * width + tx) * 4;
        uint8_t r = pixels[idx];
        uint8_t g = pixels[idx + 1];
        uint8_t b = pixels[idx + 2];
        uint8_t a = pixels[idx + 3];
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
};

struct CanvasColorStop {
    float offset; // 0.0 - 1.0
    uint32_t color; // ARGB
};

struct CanvasGradient {
    CanvasGradientType type = CanvasGradientType::None;
    // Linear: line from (x0,y0) to (x1,y1)
    // Radial: inner circle (x0,y0,r0), outer circle (x1,y1,r1)
    // Conic:  center=(x0,y0), startAngle stored in r0
    float x0 = 0, y0 = 0, r0 = 0;
    float x1 = 0, y1 = 0, r1 = 0;
    std::vector<CanvasColorStop> stops;

    bool active() const { return type != CanvasGradientType::None && !stops.empty(); }

    // Sample gradient color at canvas position (px, py), returns ARGB
    uint32_t sample(float px, float py) const {
        if (stops.empty()) return 0xFF000000;
        float t = 0.0f;
        if (type == CanvasGradientType::Linear) {
            float dx = x1 - x0, dy = y1 - y0;
            float len2 = dx * dx + dy * dy;
            t = (len2 < 1e-10f) ? 0.0f : ((px - x0) * dx + (py - y0) * dy) / len2;
        } else if (type == CanvasGradientType::Radial) {
            float dx = px - x0, dy = py - y0;
            float dist = std::sqrt(dx * dx + dy * dy);
            float denom = r1 - r0;
            t = (std::abs(denom) < 1e-10f) ? ((dist >= r1) ? 1.0f : 0.0f)
                                             : (dist - r0) / denom;
        } else if (type == CanvasGradientType::Conic) {
            float dx = px - x0, dy = py - y0;
            float angle = std::atan2(dy, dx) - r0;
            const float two_pi = 6.283185307f;
            angle = std::fmod(angle, two_pi);
            if (angle < 0.0f) angle += two_pi;
            t = angle / two_pi;
        }
        t = std::max(0.0f, std::min(1.0f, t));

        if (stops.size() == 1) return stops[0].color;
        if (t <= stops.front().offset) return stops.front().color;
        if (t >= stops.back().offset) return stops.back().color;

        for (size_t i = 0; i + 1 < stops.size(); i++) {
            if (t >= stops[i].offset && t <= stops[i + 1].offset) {
                float span = stops[i + 1].offset - stops[i].offset;
                float frac = (span > 1e-10f) ? (t - stops[i].offset) / span : 0.0f;
                frac = std::max(0.0f, std::min(1.0f, frac));
                uint32_t ca = stops[i].color, cb = stops[i + 1].color;
                auto lerp_ch = [&](int shift) -> uint32_t {
                    float va = static_cast<float>((ca >> shift) & 0xFF);
                    float vb = static_cast<float>((cb >> shift) & 0xFF);
                    return static_cast<uint32_t>(va + (vb - va) * frac) & 0xFF;
                };
                return (lerp_ch(24) << 24) | (lerp_ch(16) << 16) | (lerp_ch(8) << 8) | lerp_ch(0);
            }
        }
        return stops.back().color;
    }
};

struct Canvas2DState {
    int width = 300;
    int height = 150;
    std::vector<uint8_t>* buffer = nullptr; // Points to canvas_buffer data
    // Drawing state
    uint32_t fill_color = 0xFF000000;   // ARGB black
    uint32_t stroke_color = 0xFF000000; // ARGB black
    float line_width = 1.0f;
    std::string font = "10px sans-serif";
    int text_align = 0; // 0=start/left, 1=center, 2=right/end
    float global_alpha = 1.0f;
    // Gradient fill/stroke (overrides solid color when active)
    CanvasGradient fill_gradient;
    CanvasGradient stroke_gradient;
    // Pattern fill/stroke (overrides gradient and solid color when active)
    CanvasPattern fill_pattern;
    CanvasPattern stroke_pattern;
    // Canvas shadow state
    uint32_t shadow_color = 0x00000000; // transparent = no shadow
    float shadow_blur = 0.0f;
    float shadow_offset_x = 0.0f;
    float shadow_offset_y = 0.0f;
    // Line style state
    int line_cap = 0;  // 0=butt, 1=round, 2=square
    int line_join = 0; // 0=miter, 1=round, 2=bevel
    float miter_limit = 10.0f;
    std::vector<float> line_dash;   // empty = solid line
    float line_dash_offset = 0.0f;
    // Text state
    int text_baseline = 0; // 0=alphabetic, 1=top, 2=hanging, 3=middle, 4=ideographic, 5=bottom
    // Compositing
    std::string global_composite_op = "source-over";
    bool image_smoothing = true;
    // Path state
    struct PathPoint { float x, y; bool is_move; };
    std::vector<PathPoint> path_points;
    float path_x = 0, path_y = 0; // current point

    // Transform matrix (2D affine: a,b,c,d,e,f)
    // | a c e |   Transforms point (x,y) to (a*x+c*y+e, b*x+d*y+f)
    // | b d f |
    // | 0 0 1 |
    float tx_a = 1, tx_b = 0, tx_c = 0, tx_d = 1, tx_e = 0, tx_f = 0;

    // Clip mask: size = width * height, 0xFF = visible, 0x00 = clipped
    // Allocated lazily on first clip() call to avoid per-canvas overhead
    bool has_clip = false;
    std::vector<uint8_t> clip_mask; // empty means no clip mask allocated

    // State stack for save/restore
    struct SavedState {
        uint32_t fill_color;
        uint32_t stroke_color;
        CanvasGradient fill_gradient;
        CanvasGradient stroke_gradient;
        CanvasPattern fill_pattern;
        CanvasPattern stroke_pattern;
        float line_width;
        float global_alpha;
        std::string font;
        int text_align;
        int text_baseline;
        int line_cap, line_join;
        float miter_limit;
        uint32_t shadow_color;
        float shadow_blur, shadow_offset_x, shadow_offset_y;
        std::string global_composite_op;
        bool image_smoothing;
        float tx_a, tx_b, tx_c, tx_d, tx_e, tx_f;
        // Clip state
        bool has_clip;
        std::vector<uint8_t> clip_mask;
    };
    std::vector<SavedState> state_stack;
};

// ---- Canvas color parsing ----
static uint32_t canvas_parse_color(const std::string& input) {
    std::string s = input;
    // trim whitespace
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    // lowercase
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

    if (s.empty()) return 0xFF000000;

    // Named colors (subset)
    static const std::unordered_map<std::string, uint32_t> named = {
        {"black",   0xFF000000}, {"white",   0xFFFFFFFF}, {"red",     0xFFFF0000},
        {"green",   0xFF008000}, {"blue",    0xFF0000FF}, {"yellow",  0xFFFFFF00},
        {"cyan",    0xFF00FFFF}, {"magenta", 0xFFFF00FF}, {"orange",  0xFFFFA500},
        {"purple",  0xFF800080}, {"pink",    0xFFFFC0CB}, {"brown",   0xFFA52A2A},
        {"gray",    0xFF808080}, {"grey",    0xFF808080}, {"silver",  0xFFC0C0C0},
        {"lime",    0xFF00FF00}, {"navy",    0xFF000080}, {"teal",    0xFF008080},
        {"maroon",  0xFF800000}, {"olive",   0xFF808000}, {"aqua",    0xFF00FFFF},
        {"fuchsia", 0xFFFF00FF}, {"transparent", 0x00000000},
        {"coral",   0xFFFF7F50}, {"tomato",  0xFFFF6347}, {"gold",    0xFFFFD700},
        {"lightgray", 0xFFD3D3D3}, {"lightgrey", 0xFFD3D3D3},
        {"darkgray", 0xFFA9A9A9}, {"darkgrey", 0xFFA9A9A9},
        {"lightblue", 0xFFADD8E6}, {"darkblue", 0xFF00008B},
        {"lightgreen", 0xFF90EE90}, {"darkgreen", 0xFF006400},
        {"darkred",  0xFF8B0000}, {"skyblue", 0xFF87CEEB},
        {"steelblue", 0xFF4682B4}, {"indigo", 0xFF4B0082},
        {"violet",   0xFFEE82EE}, {"chocolate", 0xFFD2691E},
        {"tan",      0xFFD2B48C}, {"wheat",    0xFFF5DEB3},
        {"beige",    0xFFF5F5DC}, {"ivory",    0xFFFFFFF0},
        {"cornflowerblue", 0xFF6495ED}, {"dodgerblue", 0xFF1E90FF},
        {"firebrick", 0xFFB22222}, {"crimson", 0xFFDC143C},
    };
    auto it = named.find(s);
    if (it != named.end()) return it->second;

    // Hex: #RGB, #RRGGBB, #RRGGBBAA
    if (s[0] == '#') {
        std::string hex = s.substr(1);
        if (hex.size() == 3) {
            // #RGB -> #RRGGBB
            uint32_t r = 0, g = 0, b = 0;
            r = std::stoul(hex.substr(0, 1), nullptr, 16); r = r * 16 + r;
            g = std::stoul(hex.substr(1, 1), nullptr, 16); g = g * 16 + g;
            b = std::stoul(hex.substr(2, 1), nullptr, 16); b = b * 16 + b;
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        } else if (hex.size() == 4) {
            // #RGBA -> #RRGGBBAA
            uint32_t r = 0, g = 0, b = 0, a = 0;
            r = std::stoul(hex.substr(0, 1), nullptr, 16); r = r * 16 + r;
            g = std::stoul(hex.substr(1, 1), nullptr, 16); g = g * 16 + g;
            b = std::stoul(hex.substr(2, 1), nullptr, 16); b = b * 16 + b;
            a = std::stoul(hex.substr(3, 1), nullptr, 16); a = a * 16 + a;
            return (a << 24) | (r << 16) | (g << 8) | b;
        } else if (hex.size() == 6) {
            uint32_t val = std::stoul(hex, nullptr, 16);
            return 0xFF000000 | val;
        } else if (hex.size() == 8) {
            uint32_t val = std::stoul(hex, nullptr, 16);
            // val is RRGGBBAA, convert to ARGB
            uint32_t r = (val >> 24) & 0xFF;
            uint32_t g = (val >> 16) & 0xFF;
            uint32_t b = (val >> 8) & 0xFF;
            uint32_t a = val & 0xFF;
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // rgb(r, g, b) / rgba(r, g, b, a)
    if (s.substr(0, 4) == "rgb(" || s.substr(0, 5) == "rgba(") {
        auto paren_start = s.find('(');
        auto paren_end = s.rfind(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos) {
            std::string inner = s.substr(paren_start + 1, paren_end - paren_start - 1);
            // Replace commas and slashes with spaces
            for (auto& c : inner) {
                if (c == ',' || c == '/') c = ' ';
            }
            std::istringstream iss(inner);
            float rf = 0, gf = 0, bf = 0, af = 1.0f;
            iss >> rf >> gf >> bf;
            if (iss >> af) {
                // af might be 0-1 or 0-255
                if (af > 1.0f) af /= 255.0f;
            }
            uint8_t r = static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f));
            uint8_t a = static_cast<uint8_t>(std::clamp(af * 255.0f, 0.0f, 255.0f));
            return (static_cast<uint32_t>(a) << 24) |
                   (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) << 8) |
                   static_cast<uint32_t>(b);
        }
    }

    return 0xFF000000; // default black
}

static std::string canvas_color_to_string(uint32_t argb) {
    uint8_t a = (argb >> 24) & 0xFF;
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >> 8) & 0xFF;
    uint8_t b = argb & 0xFF;
    if (a == 255) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.3g)",
                  r, g, b, a / 255.0);
    return buf;
}

// ---- Pixel buffer operations ----

static void fill_rect_buffer(Canvas2DState* s, int x, int y, int w, int h) {
    if (!s->buffer) return;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(s->width, x + w);
    int y1 = std::min(s->height, y + h);

    if (s->fill_pattern.active()) {
        // Per-pixel pattern fill (tiled image)
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                if (s->has_clip && s->clip_mask[py * s->width + px] == 0x00) continue;
                uint32_t col = s->fill_pattern.sample(px, py);
                uint8_t cr = (col >> 16) & 0xFF;
                uint8_t cg = (col >> 8) & 0xFF;
                uint8_t cb = col & 0xFF;
                uint8_t ca = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
                if (ca == 0) continue; // fully transparent — skip
                int idx = (py * s->width + px) * 4;
                float alpha = ca / 255.0f;
                if (alpha >= 1.0f) {
                    (*s->buffer)[idx]     = cr;
                    (*s->buffer)[idx + 1] = cg;
                    (*s->buffer)[idx + 2] = cb;
                    (*s->buffer)[idx + 3] = 255;
                } else {
                    float inv = 1.0f - alpha;
                    (*s->buffer)[idx]     = static_cast<uint8_t>(cr * alpha + (*s->buffer)[idx]     * inv);
                    (*s->buffer)[idx + 1] = static_cast<uint8_t>(cg * alpha + (*s->buffer)[idx + 1] * inv);
                    (*s->buffer)[idx + 2] = static_cast<uint8_t>(cb * alpha + (*s->buffer)[idx + 2] * inv);
                    (*s->buffer)[idx + 3] = static_cast<uint8_t>(std::min(255.0f, ca * alpha + (*s->buffer)[idx + 3] * inv));
                }
            }
        }
    } else if (s->fill_gradient.active()) {
        // Per-pixel gradient fill
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                if (s->has_clip && s->clip_mask[py * s->width + px] == 0x00) continue;
                uint32_t col = s->fill_gradient.sample(static_cast<float>(px) + 0.5f,
                                                       static_cast<float>(py) + 0.5f);
                uint8_t cr = (col >> 16) & 0xFF;
                uint8_t cg = (col >> 8) & 0xFF;
                uint8_t cb = col & 0xFF;
                uint8_t ca = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
                int idx = (py * s->width + px) * 4;
                (*s->buffer)[idx]     = cr;
                (*s->buffer)[idx + 1] = cg;
                (*s->buffer)[idx + 2] = cb;
                (*s->buffer)[idx + 3] = ca;
            }
        }
    } else {
        uint8_t r = (s->fill_color >> 16) & 0xFF;
        uint8_t g = (s->fill_color >> 8) & 0xFF;
        uint8_t b = s->fill_color & 0xFF;
        uint8_t a = (s->fill_color >> 24) & 0xFF;
        a = static_cast<uint8_t>(a * s->global_alpha);
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                if (s->has_clip && s->clip_mask[py * s->width + px] == 0x00) continue;
                int idx = (py * s->width + px) * 4;
                (*s->buffer)[idx]     = r;
                (*s->buffer)[idx + 1] = g;
                (*s->buffer)[idx + 2] = b;
                (*s->buffer)[idx + 3] = a;
            }
        }
    }
}

static void stroke_rect_buffer(Canvas2DState* s, int x, int y, int w, int h) {
    if (!s->buffer) return;
    uint8_t r = (s->stroke_color >> 16) & 0xFF;
    uint8_t g = (s->stroke_color >> 8) & 0xFF;
    uint8_t b = s->stroke_color & 0xFF;
    uint8_t a = (s->stroke_color >> 24) & 0xFF;
    a = static_cast<uint8_t>(a * s->global_alpha);

    int lw = std::max(1, static_cast<int>(s->line_width));

    auto set_pixel = [&](int px, int py) {
        if (px < 0 || py < 0 || px >= s->width || py >= s->height) return;
        if (s->has_clip && s->clip_mask[py * s->width + px] == 0x00) return;
        int idx = (py * s->width + px) * 4;
        if (s->stroke_pattern.active()) {
            uint32_t col = s->stroke_pattern.sample(px, py);
            uint8_t cr = (col >> 16) & 0xFF;
            uint8_t cg = (col >> 8) & 0xFF;
            uint8_t cb = col & 0xFF;
            uint8_t ca = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
            float alpha = ca / 255.0f;
            if (alpha >= 1.0f) {
                (*s->buffer)[idx]     = cr;
                (*s->buffer)[idx + 1] = cg;
                (*s->buffer)[idx + 2] = cb;
                (*s->buffer)[idx + 3] = 255;
            } else if (alpha > 0.0f) {
                float inv = 1.0f - alpha;
                (*s->buffer)[idx]     = static_cast<uint8_t>(cr * alpha + (*s->buffer)[idx]     * inv);
                (*s->buffer)[idx + 1] = static_cast<uint8_t>(cg * alpha + (*s->buffer)[idx + 1] * inv);
                (*s->buffer)[idx + 2] = static_cast<uint8_t>(cb * alpha + (*s->buffer)[idx + 2] * inv);
                (*s->buffer)[idx + 3] = static_cast<uint8_t>(std::min(255.0f, ca * alpha + (*s->buffer)[idx + 3] * inv));
            }
        } else if (s->stroke_gradient.active()) {
            uint32_t col = s->stroke_gradient.sample(static_cast<float>(px) + 0.5f,
                                                      static_cast<float>(py) + 0.5f);
            (*s->buffer)[idx]     = (col >> 16) & 0xFF;
            (*s->buffer)[idx + 1] = (col >> 8) & 0xFF;
            (*s->buffer)[idx + 2] = col & 0xFF;
            (*s->buffer)[idx + 3] = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
        } else {
            (*s->buffer)[idx]     = r;
            (*s->buffer)[idx + 1] = g;
            (*s->buffer)[idx + 2] = b;
            (*s->buffer)[idx + 3] = a;
        }
    };

    // Top edge
    for (int dy = 0; dy < lw; dy++)
        for (int px = x; px < x + w; px++)
            set_pixel(px, y + dy);
    // Bottom edge
    for (int dy = 0; dy < lw; dy++)
        for (int px = x; px < x + w; px++)
            set_pixel(px, y + h - 1 - dy);
    // Left edge
    for (int dy = 0; dy < lw; dy++)
        for (int py = y; py < y + h; py++)
            set_pixel(x + dy, py);
    // Right edge
    for (int dy = 0; dy < lw; dy++)
        for (int py = y; py < y + h; py++)
            set_pixel(x + w - 1 - dy, py);
}

static void clear_rect_buffer(Canvas2DState* s, int x, int y, int w, int h) {
    if (!s->buffer) return;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(s->width, x + w);
    int y1 = std::min(s->height, y + h);

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            int idx = (py * s->width + px) * 4;
            (*s->buffer)[idx]     = 0;
            (*s->buffer)[idx + 1] = 0;
            (*s->buffer)[idx + 2] = 0;
            (*s->buffer)[idx + 3] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Stroke pixel helper — shared by thick-line and Bresenham paths.
// Handles pattern, gradient, and solid color blending.
// ---------------------------------------------------------------------------
static inline void stroke_set_pixel(Canvas2DState* s,
                                    int px, int py,
                                    uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                    float alpha) {
    if (px < 0 || py < 0 || px >= s->width || py >= s->height) return;
    if (s->has_clip && s->clip_mask[py * s->width + px] == 0x00) return;
    int idx = (py * s->width + px) * 4;
    if (s->stroke_pattern.active()) {
        uint32_t col = s->stroke_pattern.sample(px, py);
        uint8_t cr = (col >> 16) & 0xFF;
        uint8_t cg = (col >> 8) & 0xFF;
        uint8_t cb = col & 0xFF;
        uint8_t ca = static_cast<uint8_t>(((col >> 24) & 0xFF) * alpha);
        float palpha = ca / 255.0f;
        if (palpha >= 1.0f) {
            (*s->buffer)[idx]     = cr;
            (*s->buffer)[idx + 1] = cg;
            (*s->buffer)[idx + 2] = cb;
            (*s->buffer)[idx + 3] = 255;
        } else if (palpha > 0.0f) {
            float inv = 1.0f - palpha;
            (*s->buffer)[idx]     = static_cast<uint8_t>(cr * palpha + (*s->buffer)[idx]     * inv);
            (*s->buffer)[idx + 1] = static_cast<uint8_t>(cg * palpha + (*s->buffer)[idx + 1] * inv);
            (*s->buffer)[idx + 2] = static_cast<uint8_t>(cb * palpha + (*s->buffer)[idx + 2] * inv);
            (*s->buffer)[idx + 3] = static_cast<uint8_t>(std::min(255.0f, ca * palpha + (*s->buffer)[idx + 3] * inv));
        }
    } else if (s->stroke_gradient.active()) {
        uint32_t col = s->stroke_gradient.sample(static_cast<float>(px) + 0.5f,
                                                  static_cast<float>(py) + 0.5f);
        (*s->buffer)[idx]     = (col >> 16) & 0xFF;
        (*s->buffer)[idx + 1] = (col >> 8) & 0xFF;
        (*s->buffer)[idx + 2] = col & 0xFF;
        (*s->buffer)[idx + 3] = static_cast<uint8_t>(((col >> 24) & 0xFF) * alpha);
    } else {
        (*s->buffer)[idx]     = r;
        (*s->buffer)[idx + 1] = g;
        (*s->buffer)[idx + 2] = b;
        (*s->buffer)[idx + 3] = a;
    }
}

// ---------------------------------------------------------------------------
// Paint a filled circle — used for round caps and round joins.
// ---------------------------------------------------------------------------
static void paint_filled_circle(Canvas2DState* s,
                                 float cx, float cy, float radius,
                                 uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                 float alpha) {
    int ix0 = static_cast<int>(std::floor(cx - radius));
    int iy0 = static_cast<int>(std::floor(cy - radius));
    int ix1 = static_cast<int>(std::ceil(cx + radius)) + 1;
    int iy1 = static_cast<int>(std::ceil(cy + radius)) + 1;
    float r2 = radius * radius;
    for (int py = iy0; py < iy1; py++) {
        float fdy = static_cast<float>(py) + 0.5f - cy;
        for (int px = ix0; px < ix1; px++) {
            float fdx = static_cast<float>(px) + 0.5f - cx;
            if (fdx * fdx + fdy * fdy <= r2)
                stroke_set_pixel(s, px, py, r, g, b, a, alpha);
        }
    }
}

// ---------------------------------------------------------------------------
// Thick line rasterizer (line_width > 1).
//
// Algorithm: the stroke of a segment is a rectangle (rotated by the line
// direction) with optional cap decorations.
//
//   cap == 0 (butt)   -- rectangle exactly between endpoints, no extension
//   cap == 1 (round)  -- rectangle + filled semicircles at both endpoints
//   cap == 2 (square) -- rectangle extended by half-width at both endpoints
//
// Per-pixel test: project candidate pixel onto the segment axis and
// perpendicular.  Inside the rectangle if:
//   along-axis in [t_start, t_end]  AND  |perp| <= half_width
// For round caps also accept pixels within radius half_width of either endpoint.
// ---------------------------------------------------------------------------
static void draw_thick_line(Canvas2DState* s,
                             float x0, float y0, float x1, float y1,
                             float lw, int cap,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                             float alpha) {
    float ddx = x1 - x0;
    float ddy = y1 - y0;
    float len = std::sqrt(ddx * ddx + ddy * ddy);
    float half = lw * 0.5f;

    // Degenerate zero-length segment -- draw a dot
    if (len < 0.0001f) {
        if (cap == 1) {
            paint_filled_circle(s, x0, y0, half, r, g, b, a, alpha);
        } else {
            int ix = static_cast<int>(std::floor(x0 - half));
            int iy = static_cast<int>(std::floor(y0 - half));
            int iw = static_cast<int>(std::ceil(lw)) + 1;
            for (int py = iy; py < iy + iw; py++)
                for (int px = ix; px < ix + iw; px++)
                    stroke_set_pixel(s, px, py, r, g, b, a, alpha);
        }
        return;
    }

    // Unit direction along line and perpendicular normal
    float ux = ddx / len, uy = ddy / len;

    // t range along the line axis (in pixel units from p0)
    float t_lo = 0.0f, t_hi = len;
    if (cap == 2) { // square: extend each end by half
        t_lo = -half;
        t_hi = len + half;
    }

    // Bounding box (expanded by half perpendicular + cap extension)
    float ext = (cap == 1) ? half : ((cap == 2) ? half : 0.0f);
    float bx0 = std::min(x0, x1) - half - ext;
    float by0 = std::min(y0, y1) - half - ext;
    float bx1 = std::max(x0, x1) + half + ext;
    float by1 = std::max(y0, y1) + half + ext;

    int ix0 = static_cast<int>(std::floor(bx0));
    int iy0 = static_cast<int>(std::floor(by0));
    int ix1 = static_cast<int>(std::ceil(bx1)) + 1;
    int iy1 = static_cast<int>(std::ceil(by1)) + 1;

    float half2 = half * half;

    for (int py = iy0; py < iy1; py++) {
        float fpy = static_cast<float>(py) + 0.5f;
        float py0 = fpy - y0;
        for (int px = ix0; px < ix1; px++) {
            float fpx = static_cast<float>(px) + 0.5f;
            float px0 = fpx - x0;

            // Project onto segment axis and perpendicular
            float t = px0 * ux + py0 * uy;   // along-line distance from p0
            float d = px0 * (-uy) + py0 * ux; // perpendicular distance

            bool inside = (t >= t_lo && t <= t_hi && std::abs(d) <= half);

            // Round caps: accept pixels within radius of each endpoint
            if (!inside && cap == 1) {
                float dx0 = fpx - x0, dy0 = fpy - y0;
                float dx1 = fpx - x1, dy1 = fpy - y1;
                if (dx0 * dx0 + dy0 * dy0 <= half2) inside = true;
                else if (dx1 * dx1 + dy1 * dy1 <= half2) inside = true;
            }

            if (inside)
                stroke_set_pixel(s, px, py, r, g, b, a, alpha);
        }
    }
}

// Draw a line using Bresenham's algorithm (1-px), or the thick-line rasterizer
// when line_width > 1.  Dashed lines always use Bresenham regardless of width.
static void draw_line_buffer(Canvas2DState* s, int x0, int y0, int x1, int y1,
                             uint32_t color, float alpha) {
    if (!s->buffer) return;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = static_cast<uint8_t>(((color >> 24) & 0xFF) * alpha);

    // Thick-line fast dispatch (solid strokes only)
    if (s->line_width > 1.0f && s->line_dash.empty()) {
        draw_thick_line(s,
                        static_cast<float>(x0), static_cast<float>(y0),
                        static_cast<float>(x1), static_cast<float>(y1),
                        s->line_width, s->line_cap,
                        r, g, b, a, alpha);
        return;
    }

    auto set_pixel = [&](int px, int py) {
        stroke_set_pixel(s, px, py, r, g, b, a, alpha);
    };

    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    // Fast path: no dash pattern — draw solid line
    if (s->line_dash.empty()) {
        while (true) {
            set_pixel(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        return;
    }

    // Dash pattern path.
    // The dash array alternates [dash_len, gap_len, dash_len, gap_len, ...] (always even).
    // lineDashOffset shifts where in the pattern cycle we start.
    const std::vector<float>& dash = s->line_dash;
    float cycle_len = 0.0f;
    for (float v : dash) cycle_len += v;
    if (cycle_len <= 0.0f) {
        // All zeros — effectively solid
        while (true) {
            set_pixel(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        return;
    }

    // Starting position within the dash cycle
    float pos = std::fmod(s->line_dash_offset, cycle_len);
    if (pos < 0.0f) pos += cycle_len;

    // Find which dash segment and remaining distance within that segment we start in
    size_t seg_idx = 0;
    float seg_rem = 0.0f;
    {
        float walked = pos;
        for (size_t i = 0; i < dash.size(); ++i) {
            if (walked < dash[i] || i == dash.size() - 1) {
                seg_idx = i;
                seg_rem = dash[i] - walked;
                break;
            }
            walked -= dash[i];
        }
    }

    // Walk Bresenham pixels; each step advances ~1 unit along the line.
    // Even segment indices (0,2,4,...) are dash (draw), odd are gap (skip).
    while (true) {
        bool do_draw = (seg_idx % 2 == 0);
        if (do_draw) set_pixel(x0, y0);

        if (x0 == x1 && y0 == y1) break;

        // Advance Bresenham one pixel
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }

        // Consume 1 pixel from current dash segment
        seg_rem -= 1.0f;
        while (seg_rem <= 0.0f) {
            seg_idx = (seg_idx + 1) % dash.size();
            seg_rem += dash[seg_idx];
        }
    }
}

// ---- Canvas2D finalizer ----
static void js_canvas2d_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<Canvas2DState*>(
        JS_GetOpaque(val, canvas2d_class_id));
    delete state;
}

static JSClassDef canvas2d_class_def = {
    "CanvasRenderingContext2D",
    js_canvas2d_finalizer,
    nullptr, nullptr, nullptr
};

// ---- Canvas2D methods ----

static JSValue js_canvas2d_fill_rect(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);
    // Apply affine transform to origin point
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    float tx = s->tx_a * fx + s->tx_c * fy + s->tx_e;
    float ty = s->tx_b * fx + s->tx_d * fy + s->tx_f;
    // Scale width/height by transform scale factors (approximation, ignores rotation for rects)
    float sx = std::sqrt(s->tx_a * s->tx_a + s->tx_b * s->tx_b);
    float sy = std::sqrt(s->tx_c * s->tx_c + s->tx_d * s->tx_d);
    fill_rect_buffer(s, static_cast<int>(tx), static_cast<int>(ty),
                     static_cast<int>(w * sx), static_cast<int>(h * sy));
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_stroke_rect(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);
    // Apply affine transform to origin point
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    float tx = s->tx_a * fx + s->tx_c * fy + s->tx_e;
    float ty = s->tx_b * fx + s->tx_d * fy + s->tx_f;
    // Scale width/height by transform scale factors
    float sx = std::sqrt(s->tx_a * s->tx_a + s->tx_b * s->tx_b);
    float sy = std::sqrt(s->tx_c * s->tx_c + s->tx_d * s->tx_d);
    stroke_rect_buffer(s, static_cast<int>(tx), static_cast<int>(ty),
                       static_cast<int>(w * sx), static_cast<int>(h * sy));
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_clear_rect(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);
    // Apply affine transform to origin point
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    float tx = s->tx_a * fx + s->tx_c * fy + s->tx_e;
    float ty = s->tx_b * fx + s->tx_d * fy + s->tx_f;
    // Scale width/height by transform scale factors
    float sx = std::sqrt(s->tx_a * s->tx_a + s->tx_b * s->tx_b);
    float sy = std::sqrt(s->tx_c * s->tx_c + s->tx_d * s->tx_d);
    clear_rect_buffer(s, static_cast<int>(tx), static_cast<int>(ty),
                      static_cast<int>(w * sx), static_cast<int>(h * sy));
    return JS_UNDEFINED;
}

// ---- fillStyle / strokeStyle getters and setters ----
static JSValue js_canvas2d_get_fill_style(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewString(ctx, canvas_color_to_string(s->fill_color).c_str());
}

// Helper: extract CanvasPattern data from a JS pattern object (produced by createPattern)
static bool canvas_load_pattern_from_js(JSContext* ctx, JSValueConst obj, CanvasPattern& out) {
    // Pattern object has: type="pattern", __pixels (JS array), __patWidth, __patHeight, __repeat
    JSValue pw_val = JS_GetPropertyStr(ctx, obj, "__patWidth");
    JSValue ph_val = JS_GetPropertyStr(ctx, obj, "__patHeight");
    JSValue rp_val = JS_GetPropertyStr(ctx, obj, "__repeat");
    JSValue px_val = JS_GetPropertyStr(ctx, obj, "__pixels");

    int32_t pw = 0, ph = 0;
    JS_ToInt32(ctx, &pw, pw_val);
    JS_ToInt32(ctx, &ph, ph_val);
    JS_FreeValue(ctx, pw_val);
    JS_FreeValue(ctx, ph_val);

    if (pw <= 0 || ph <= 0 || !JS_IsArray(ctx, px_val)) {
        JS_FreeValue(ctx, rp_val);
        JS_FreeValue(ctx, px_val);
        return false;
    }

    // Parse repetition
    CanvasPatternRepeat rep = CanvasPatternRepeat::Repeat;
    const char* rp_str = JS_ToCString(ctx, rp_val);
    if (rp_str) {
        std::string rs(rp_str);
        JS_FreeCString(ctx, rp_str);
        if (rs == "repeat-x") rep = CanvasPatternRepeat::RepeatX;
        else if (rs == "repeat-y") rep = CanvasPatternRepeat::RepeatY;
        else if (rs == "no-repeat") rep = CanvasPatternRepeat::NoRepeat;
        // else "repeat" (default)
    }
    JS_FreeValue(ctx, rp_val);

    // Copy pixel data
    int total = pw * ph * 4;
    out.pixels.resize(static_cast<size_t>(total));
    for (int i = 0; i < total; i++) {
        JSValue v = JS_GetPropertyUint32(ctx, px_val, static_cast<uint32_t>(i));
        int32_t bv = 0;
        JS_ToInt32(ctx, &bv, v);
        JS_FreeValue(ctx, v);
        out.pixels[i] = static_cast<uint8_t>(bv < 0 ? 0 : (bv > 255 ? 255 : bv));
    }
    JS_FreeValue(ctx, px_val);

    out.width = pw;
    out.height = ph;
    out.repeat = rep;
    return true;
}

static JSValue js_canvas2d_set_fill_style(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    if (JS_IsObject(argv[0]) && !JS_IsFunction(ctx, argv[0])) {
        // Check for gradient or pattern object (has "type" property)
        JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
        const char* type_str = JS_ToCString(ctx, type_val);
        if (type_str) {
            std::string gtype(type_str);
            JS_FreeCString(ctx, type_str);
            JS_FreeValue(ctx, type_val);
            if (gtype == "pattern") {
                CanvasPattern fill_pat;
                if (canvas_load_pattern_from_js(ctx, argv[0], fill_pat)) {
                    s->fill_pattern = std::move(fill_pat);
                    s->fill_gradient = CanvasGradient{};
                } else {
                    s->fill_pattern = CanvasPattern{};
                }
                return JS_UNDEFINED;
            }
            if (gtype == "linear" || gtype == "radial" || gtype == "conic") {
                s->fill_pattern = CanvasPattern{};
                CanvasGradient grad;
                grad.type = (gtype == "linear") ? CanvasGradientType::Linear
                          : (gtype == "radial") ? CanvasGradientType::Radial
                                                : CanvasGradientType::Conic;
                auto get_f = [&](const char* prop) -> float {
                    JSValue v = JS_GetPropertyStr(ctx, argv[0], prop);
                    double d = 0.0;
                    JS_ToFloat64(ctx, &d, v);
                    JS_FreeValue(ctx, v);
                    return static_cast<float>(d);
                };
                grad.x0 = get_f("x0"); grad.y0 = get_f("y0"); grad.r0 = get_f("r0");
                grad.x1 = get_f("x1"); grad.y1 = get_f("y1"); grad.r1 = get_f("r1");
                // For conic: startAngle is r0, cx/cy may use x0/y0 or cx/cy
                if (gtype == "conic") {
                    grad.r0 = get_f("startAngle");
                    grad.x0 = get_f("cx"); grad.y0 = get_f("cy");
                }
                // Read stops array
                JSValue stops_val = JS_GetPropertyStr(ctx, argv[0], "stops");
                if (JS_IsArray(ctx, stops_val)) {
                    JSValue len_val = JS_GetPropertyStr(ctx, stops_val, "length");
                    int32_t len = 0;
                    JS_ToInt32(ctx, &len, len_val);
                    JS_FreeValue(ctx, len_val);
                    for (int32_t i = 0; i < len; i++) {
                        JSValue stop = JS_GetPropertyUint32(ctx, stops_val, static_cast<uint32_t>(i));
                        JSValue off_v = JS_GetPropertyStr(ctx, stop, "offset");
                        JSValue col_v = JS_GetPropertyStr(ctx, stop, "color");
                        double off = 0.0;
                        JS_ToFloat64(ctx, &off, off_v);
                        const char* col_s = JS_ToCString(ctx, col_v);
                        uint32_t col = col_s ? canvas_parse_color(col_s) : 0xFF000000u;
                        if (col_s) JS_FreeCString(ctx, col_s);
                        JS_FreeValue(ctx, off_v);
                        JS_FreeValue(ctx, col_v);
                        JS_FreeValue(ctx, stop);
                        grad.stops.push_back({static_cast<float>(off), col});
                    }
                    // Sort stops by offset
                    std::sort(grad.stops.begin(), grad.stops.end(),
                              [](const CanvasColorStop& a, const CanvasColorStop& b) {
                                  return a.offset < b.offset;
                              });
                }
                JS_FreeValue(ctx, stops_val);
                s->fill_gradient = std::move(grad);
                return JS_UNDEFINED;
            }
        } else {
            JS_FreeValue(ctx, type_val);
        }
        // Unknown object — clear gradient and pattern
        s->fill_gradient = CanvasGradient{};
        s->fill_pattern = CanvasPattern{};
        return JS_UNDEFINED;
    }
    // String: parse as solid color, clear any gradient/pattern
    s->fill_gradient = CanvasGradient{};
    s->fill_pattern = CanvasPattern{};
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        s->fill_color = canvas_parse_color(cstr);
        JS_FreeCString(ctx, cstr);
    }
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_get_stroke_style(JSContext* ctx, JSValueConst this_val,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewString(ctx, canvas_color_to_string(s->stroke_color).c_str());
}

static JSValue js_canvas2d_set_stroke_style(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    if (JS_IsObject(argv[0]) && !JS_IsFunction(ctx, argv[0])) {
        JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
        const char* type_str = JS_ToCString(ctx, type_val);
        if (type_str) {
            std::string gtype(type_str);
            JS_FreeCString(ctx, type_str);
            JS_FreeValue(ctx, type_val);
            if (gtype == "pattern") {
                CanvasPattern stroke_pat;
                if (canvas_load_pattern_from_js(ctx, argv[0], stroke_pat)) {
                    s->stroke_pattern = std::move(stroke_pat);
                    s->stroke_gradient = CanvasGradient{};
                } else {
                    s->stroke_pattern = CanvasPattern{};
                }
                return JS_UNDEFINED;
            }
            if (gtype == "linear" || gtype == "radial" || gtype == "conic") {
                s->stroke_pattern = CanvasPattern{};
                CanvasGradient grad;
                grad.type = (gtype == "linear") ? CanvasGradientType::Linear
                          : (gtype == "radial") ? CanvasGradientType::Radial
                                                : CanvasGradientType::Conic;
                auto get_f = [&](const char* prop) -> float {
                    JSValue v = JS_GetPropertyStr(ctx, argv[0], prop);
                    double d = 0.0;
                    JS_ToFloat64(ctx, &d, v);
                    JS_FreeValue(ctx, v);
                    return static_cast<float>(d);
                };
                grad.x0 = get_f("x0"); grad.y0 = get_f("y0"); grad.r0 = get_f("r0");
                grad.x1 = get_f("x1"); grad.y1 = get_f("y1"); grad.r1 = get_f("r1");
                if (gtype == "conic") {
                    grad.r0 = get_f("startAngle");
                    grad.x0 = get_f("cx"); grad.y0 = get_f("cy");
                }
                JSValue stops_val = JS_GetPropertyStr(ctx, argv[0], "stops");
                if (JS_IsArray(ctx, stops_val)) {
                    JSValue len_val = JS_GetPropertyStr(ctx, stops_val, "length");
                    int32_t len = 0;
                    JS_ToInt32(ctx, &len, len_val);
                    JS_FreeValue(ctx, len_val);
                    for (int32_t i = 0; i < len; i++) {
                        JSValue stop = JS_GetPropertyUint32(ctx, stops_val, static_cast<uint32_t>(i));
                        JSValue off_v = JS_GetPropertyStr(ctx, stop, "offset");
                        JSValue col_v = JS_GetPropertyStr(ctx, stop, "color");
                        double off = 0.0;
                        JS_ToFloat64(ctx, &off, off_v);
                        const char* col_s = JS_ToCString(ctx, col_v);
                        uint32_t col = col_s ? canvas_parse_color(col_s) : 0xFF000000u;
                        if (col_s) JS_FreeCString(ctx, col_s);
                        JS_FreeValue(ctx, off_v);
                        JS_FreeValue(ctx, col_v);
                        JS_FreeValue(ctx, stop);
                        grad.stops.push_back({static_cast<float>(off), col});
                    }
                    std::sort(grad.stops.begin(), grad.stops.end(),
                              [](const CanvasColorStop& a, const CanvasColorStop& b) {
                                  return a.offset < b.offset;
                              });
                }
                JS_FreeValue(ctx, stops_val);
                s->stroke_gradient = std::move(grad);
                return JS_UNDEFINED;
            }
        } else {
            JS_FreeValue(ctx, type_val);
        }
        // Unknown object — clear gradient and pattern
        s->stroke_gradient = CanvasGradient{};
        s->stroke_pattern = CanvasPattern{};
        return JS_UNDEFINED;
    }
    // String: parse as solid color, clear any gradient/pattern
    s->stroke_gradient = CanvasGradient{};
    s->stroke_pattern = CanvasPattern{};
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        s->stroke_color = canvas_parse_color(cstr);
        JS_FreeCString(ctx, cstr);
    }
    return JS_UNDEFINED;
}

// ---- globalAlpha getter/setter ----
static JSValue js_canvas2d_get_global_alpha(JSContext* ctx, JSValueConst this_val,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewFloat64(ctx, s->global_alpha);
}

static JSValue js_canvas2d_set_global_alpha(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double val;
    JS_ToFloat64(ctx, &val, argv[0]);
    s->global_alpha = static_cast<float>(std::clamp(val, 0.0, 1.0));
    return JS_UNDEFINED;
}

// ---- lineWidth getter/setter ----
static JSValue js_canvas2d_get_line_width(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewFloat64(ctx, s->line_width);
}

static JSValue js_canvas2d_set_line_width(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double val;
    JS_ToFloat64(ctx, &val, argv[0]);
    if (val > 0) s->line_width = static_cast<float>(val);
    return JS_UNDEFINED;
}

// ---- font getter/setter ----
static JSValue js_canvas2d_get_font(JSContext* ctx, JSValueConst this_val,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewString(ctx, s->font.c_str());
}

static JSValue js_canvas2d_set_font(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        s->font = cstr;
        JS_FreeCString(ctx, cstr);
    }
    return JS_UNDEFINED;
}

// ---- textAlign getter/setter ----
static JSValue js_canvas2d_get_text_align(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    const char* align_str = "start";
    switch (s->text_align) {
        case 1: align_str = "center"; break;
        case 2: align_str = "right"; break;
        case 3: align_str = "end"; break;
    }
    return JS_NewString(ctx, align_str);
}

static JSValue js_canvas2d_set_text_align(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        std::string val(cstr);
        JS_FreeCString(ctx, cstr);
        if (val == "start" || val == "left") s->text_align = 0;
        else if (val == "center") s->text_align = 1;
        else if (val == "right") s->text_align = 2;
        else if (val == "end") s->text_align = 3;
    }
    return JS_UNDEFINED;
}

// ---- textBaseline ----
static JSValue js_canvas2d_get_text_baseline(JSContext* ctx, JSValueConst this_val,
                                              int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    const char* names[] = {"alphabetic", "top", "hanging", "middle", "ideographic", "bottom"};
    return JS_NewString(ctx, names[s->text_baseline < 6 ? s->text_baseline : 0]);
}
static JSValue js_canvas2d_set_text_baseline(JSContext* ctx, JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        std::string val(cstr);
        JS_FreeCString(ctx, cstr);
        if (val == "alphabetic") s->text_baseline = 0;
        else if (val == "top") s->text_baseline = 1;
        else if (val == "hanging") s->text_baseline = 2;
        else if (val == "middle") s->text_baseline = 3;
        else if (val == "ideographic") s->text_baseline = 4;
        else if (val == "bottom") s->text_baseline = 5;
    }
    return JS_UNDEFINED;
}

// ---- lineCap ----
static JSValue js_canvas2d_get_line_cap(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    const char* names[] = {"butt", "round", "square"};
    return JS_NewString(ctx, names[s->line_cap < 3 ? s->line_cap : 0]);
}
static JSValue js_canvas2d_set_line_cap(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        std::string val(cstr);
        JS_FreeCString(ctx, cstr);
        if (val == "butt") s->line_cap = 0;
        else if (val == "round") s->line_cap = 1;
        else if (val == "square") s->line_cap = 2;
    }
    return JS_UNDEFINED;
}

// ---- lineJoin ----
static JSValue js_canvas2d_get_line_join(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    const char* names[] = {"miter", "round", "bevel"};
    return JS_NewString(ctx, names[s->line_join < 3 ? s->line_join : 0]);
}
static JSValue js_canvas2d_set_line_join(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        std::string val(cstr);
        JS_FreeCString(ctx, cstr);
        if (val == "miter") s->line_join = 0;
        else if (val == "round") s->line_join = 1;
        else if (val == "bevel") s->line_join = 2;
    }
    return JS_UNDEFINED;
}

// ---- miterLimit ----
static JSValue js_canvas2d_get_miter_limit(JSContext* ctx, JSValueConst this_val,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewFloat64(ctx, s->miter_limit);
}
static JSValue js_canvas2d_set_miter_limit(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    if (v > 0) s->miter_limit = static_cast<float>(v);
    return JS_UNDEFINED;
}

// ---- canvas shadow properties ----
static JSValue js_canvas2d_get_shadow_color(JSContext* ctx, JSValueConst this_val,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    uint8_t r = (s->shadow_color >> 16) & 0xFF;
    uint8_t g = (s->shadow_color >> 8) & 0xFF;
    uint8_t b = s->shadow_color & 0xFF;
    uint8_t a = (s->shadow_color >> 24) & 0xFF;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.2f)", r, g, b, a / 255.0f);
    return JS_NewString(ctx, buf);
}
static JSValue js_canvas2d_set_shadow_color(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        s->shadow_color = canvas_parse_color(cstr);
        JS_FreeCString(ctx, cstr);
    }
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_get_shadow_blur(JSContext* ctx, JSValueConst this_val,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    return s ? JS_NewFloat64(ctx, s->shadow_blur) : JS_UNDEFINED;
}
static JSValue js_canvas2d_set_shadow_blur(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double v; JS_ToFloat64(ctx, &v, argv[0]);
    if (v >= 0) s->shadow_blur = static_cast<float>(v);
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_get_shadow_offset_x(JSContext* ctx, JSValueConst this_val,
                                                int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    return s ? JS_NewFloat64(ctx, s->shadow_offset_x) : JS_UNDEFINED;
}
static JSValue js_canvas2d_set_shadow_offset_x(JSContext* ctx, JSValueConst this_val,
                                                int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double v; JS_ToFloat64(ctx, &v, argv[0]);
    s->shadow_offset_x = static_cast<float>(v);
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_get_shadow_offset_y(JSContext* ctx, JSValueConst this_val,
                                                int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    return s ? JS_NewFloat64(ctx, s->shadow_offset_y) : JS_UNDEFINED;
}
static JSValue js_canvas2d_set_shadow_offset_y(JSContext* ctx, JSValueConst this_val,
                                                int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double v; JS_ToFloat64(ctx, &v, argv[0]);
    s->shadow_offset_y = static_cast<float>(v);
    return JS_UNDEFINED;
}

// ---- globalCompositeOperation ----
static JSValue js_canvas2d_get_global_composite_op(JSContext* ctx, JSValueConst this_val,
                                                    int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    return s ? JS_NewString(ctx, s->global_composite_op.c_str()) : JS_UNDEFINED;
}
static JSValue js_canvas2d_set_global_composite_op(JSContext* ctx, JSValueConst this_val,
                                                    int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    const char* cstr = JS_ToCString(ctx, argv[0]);
    if (cstr) {
        s->global_composite_op = cstr;
        JS_FreeCString(ctx, cstr);
    }
    return JS_UNDEFINED;
}

// ---- imageSmoothingEnabled ----
static JSValue js_canvas2d_get_image_smoothing(JSContext* ctx, JSValueConst this_val,
                                                int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    return s ? JS_NewBool(ctx, s->image_smoothing ? 1 : 0) : JS_UNDEFINED;
}
static JSValue js_canvas2d_set_image_smoothing(JSContext* ctx, JSValueConst this_val,
                                                int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    s->image_smoothing = JS_ToBool(ctx, argv[0]) != 0;
    return JS_UNDEFINED;
}

// ---- Path methods ----
static JSValue js_canvas2d_begin_path(JSContext* /*ctx*/, JSValueConst this_val,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    s->path_points.clear();
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_close_path(JSContext* /*ctx*/, JSValueConst this_val,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    // Close: add line back to the first moveTo point
    if (!s->path_points.empty()) {
        for (auto& pt : s->path_points) {
            if (pt.is_move) {
                s->path_points.push_back({pt.x, pt.y, false});
                s->path_x = pt.x;
                s->path_y = pt.y;
                break;
            }
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_move_to(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_UNDEFINED;
    double x, y;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    s->path_points.push_back({static_cast<float>(x), static_cast<float>(y), true});
    s->path_x = static_cast<float>(x);
    s->path_y = static_cast<float>(y);
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_line_to(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_UNDEFINED;
    double x, y;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    s->path_points.push_back({static_cast<float>(x), static_cast<float>(y), false});
    s->path_x = static_cast<float>(x);
    s->path_y = static_cast<float>(y);
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_rect(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    float fw = static_cast<float>(w), fh = static_cast<float>(h);
    s->path_points.push_back({fx, fy, true});
    s->path_points.push_back({fx + fw, fy, false});
    s->path_points.push_back({fx + fw, fy + fh, false});
    s->path_points.push_back({fx, fy + fh, false});
    s->path_points.push_back({fx, fy, false}); // close
    s->path_x = fx;
    s->path_y = fy;
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_arc(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 5) return JS_UNDEFINED;
    double cx_d, cy_d, radius, start_angle, end_angle;
    JS_ToFloat64(ctx, &cx_d, argv[0]);
    JS_ToFloat64(ctx, &cy_d, argv[1]);
    JS_ToFloat64(ctx, &radius, argv[2]);
    JS_ToFloat64(ctx, &start_angle, argv[3]);
    JS_ToFloat64(ctx, &end_angle, argv[4]);
    bool ccw = false;
    if (argc > 5) ccw = JS_ToBool(ctx, argv[5]) != 0;

    // Approximate arc with line segments
    int num_segments = std::max(16, static_cast<int>(radius * 2));
    if (num_segments > 360) num_segments = 360;

    double angle_range = end_angle - start_angle;
    if (ccw && angle_range > 0) angle_range -= 2 * M_PI;
    if (!ccw && angle_range < 0) angle_range += 2 * M_PI;

    for (int i = 0; i <= num_segments; i++) {
        double t = static_cast<double>(i) / num_segments;
        double angle = start_angle + t * angle_range;
        float px = static_cast<float>(cx_d + radius * std::cos(angle));
        float py = static_cast<float>(cy_d + radius * std::sin(angle));
        s->path_points.push_back({px, py, (i == 0)});
    }
    s->path_x = static_cast<float>(cx_d + radius * std::cos(end_angle));
    s->path_y = static_cast<float>(cy_d + radius * std::sin(end_angle));
    return JS_UNDEFINED;
}

// Simple scanline fill for path (closed polygon fill)
static JSValue js_canvas2d_fill(JSContext* /*ctx*/, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || s->path_points.empty()) return JS_UNDEFINED;

    uint8_t r = (s->fill_color >> 16) & 0xFF;
    uint8_t g = (s->fill_color >> 8) & 0xFF;
    uint8_t b = s->fill_color & 0xFF;
    uint8_t a = static_cast<uint8_t>(((s->fill_color >> 24) & 0xFF) * s->global_alpha);

    // Find bounding box
    float min_y = s->path_points[0].y, max_y = s->path_points[0].y;
    for (auto& pt : s->path_points) {
        if (pt.y < min_y) min_y = pt.y;
        if (pt.y > max_y) max_y = pt.y;
    }
    int iy_start = std::max(0, static_cast<int>(min_y));
    int iy_end = std::min(s->height - 1, static_cast<int>(max_y));

    // Build edges from path points
    struct Edge { float x0, y0, x1, y1; };
    std::vector<Edge> edges;
    float prev_x = 0, prev_y = 0;
    bool have_prev = false;
    for (auto& pt : s->path_points) {
        if (pt.is_move) {
            prev_x = pt.x; prev_y = pt.y;
            have_prev = true;
        } else if (have_prev) {
            edges.push_back({prev_x, prev_y, pt.x, pt.y});
            prev_x = pt.x; prev_y = pt.y;
        }
    }

    // Scanline fill using even-odd rule
    for (int y = iy_start; y <= iy_end; y++) {
        float scan_y = y + 0.5f;
        std::vector<float> intersections;
        for (auto& e : edges) {
            float ey0 = e.y0, ey1 = e.y1;
            if (ey0 == ey1) continue; // horizontal edge
            if (scan_y < std::min(ey0, ey1) || scan_y >= std::max(ey0, ey1)) continue;
            float t = (scan_y - ey0) / (ey1 - ey0);
            float ix = e.x0 + t * (e.x1 - e.x0);
            intersections.push_back(ix);
        }
        std::sort(intersections.begin(), intersections.end());

        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x_start = std::max(0, static_cast<int>(intersections[i]));
            int x_end = std::min(s->width, static_cast<int>(intersections[i + 1]) + 1);
            for (int x = x_start; x < x_end; x++) {
                if (s->has_clip && s->clip_mask[y * s->width + x] == 0x00) continue;
                int idx = (y * s->width + x) * 4;
                if (s->fill_pattern.active()) {
                    uint32_t col = s->fill_pattern.sample(x, y);
                    uint8_t cr = (col >> 16) & 0xFF;
                    uint8_t cg = (col >> 8) & 0xFF;
                    uint8_t cb = col & 0xFF;
                    uint8_t ca = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
                    float palpha = ca / 255.0f;
                    if (palpha >= 1.0f) {
                        (*s->buffer)[idx]     = cr;
                        (*s->buffer)[idx + 1] = cg;
                        (*s->buffer)[idx + 2] = cb;
                        (*s->buffer)[idx + 3] = 255;
                    } else if (palpha > 0.0f) {
                        float inv = 1.0f - palpha;
                        (*s->buffer)[idx]     = static_cast<uint8_t>(cr * palpha + (*s->buffer)[idx]     * inv);
                        (*s->buffer)[idx + 1] = static_cast<uint8_t>(cg * palpha + (*s->buffer)[idx + 1] * inv);
                        (*s->buffer)[idx + 2] = static_cast<uint8_t>(cb * palpha + (*s->buffer)[idx + 2] * inv);
                        (*s->buffer)[idx + 3] = static_cast<uint8_t>(std::min(255.0f, ca * palpha + (*s->buffer)[idx + 3] * inv));
                    }
                } else if (s->fill_gradient.active()) {
                    uint32_t col = s->fill_gradient.sample(static_cast<float>(x) + 0.5f,
                                                           static_cast<float>(y) + 0.5f);
                    (*s->buffer)[idx]     = (col >> 16) & 0xFF;
                    (*s->buffer)[idx + 1] = (col >> 8) & 0xFF;
                    (*s->buffer)[idx + 2] = col & 0xFF;
                    (*s->buffer)[idx + 3] = static_cast<uint8_t>(((col >> 24) & 0xFF) * s->global_alpha);
                } else {
                    (*s->buffer)[idx]     = r;
                    (*s->buffer)[idx + 1] = g;
                    (*s->buffer)[idx + 2] = b;
                    (*s->buffer)[idx + 3] = a;
                }
            }
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_stroke(JSContext* /*ctx*/, JSValueConst this_val,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || s->path_points.empty()) return JS_UNDEFINED;

    uint8_t sr = (s->stroke_color >> 16) & 0xFF;
    uint8_t sg = (s->stroke_color >> 8) & 0xFF;
    uint8_t sb = s->stroke_color & 0xFF;
    uint8_t sa = static_cast<uint8_t>(((s->stroke_color >> 24) & 0xFF) * s->global_alpha);
    float alpha = s->global_alpha;
    float lw = s->line_width;
    bool thick = (lw > 1.0f && s->line_dash.empty());

    // For join rendering we need to know three consecutive points.
    // Collect sub-paths, then iterate over consecutive segment pairs.
    // join_style: 0=miter, 1=round, 2=bevel
    // We implement round and bevel joins by painting a filled circle at
    // each interior join point (covers round and makes bevel approximate).
    // Miter joins are naturally filled by the overlapping thick segments.

    float prev_x = 0, prev_y = 0;
    float pprev_x = 0, pprev_y = 0;  // point before prev (for join detection)
    bool have_prev = false;
    bool have_pprev = false;

    for (auto& pt : s->path_points) {
        if (pt.is_move) {
            prev_x = pt.x; prev_y = pt.y;
            have_prev = true;
            have_pprev = false;
        } else if (have_prev) {
            // Draw the segment
            if (thick) {
                draw_thick_line(s,
                                prev_x, prev_y,
                                pt.x, pt.y,
                                lw, s->line_cap,
                                sr, sg, sb, sa, alpha);
                // Paint a join circle at the shared interior vertex between
                // the previous segment and this one.  This correctly handles
                // round and bevel joins; miter is approximated (no gap).
                if (have_pprev && s->line_join != 0 /* not miter */) {
                    paint_filled_circle(s, prev_x, prev_y, lw * 0.5f,
                                        sr, sg, sb, sa, alpha);
                } else if (have_pprev) {
                    // Miter: painting a small circle fills the inner gap cleanly
                    paint_filled_circle(s, prev_x, prev_y, lw * 0.5f,
                                        sr, sg, sb, sa, alpha);
                }
            } else {
                draw_line_buffer(s, static_cast<int>(prev_x), static_cast<int>(prev_y),
                                static_cast<int>(pt.x), static_cast<int>(pt.y),
                                s->stroke_color, s->global_alpha);
            }
            pprev_x = prev_x; pprev_y = prev_y;
            prev_x = pt.x; prev_y = pt.y;
            have_pprev = true;
        }
    }
    return JS_UNDEFINED;
}

// ---- Text methods ----

// Parse a Canvas 2D font string (e.g. "bold 16px Arial") and extract the
// numeric pixel size, font family, weight and italic flag.
// Falls back to safe defaults if the string cannot be fully parsed.
static void parse_canvas2d_font(const std::string& font_str,
                                 float& out_size,
                                 std::string& out_family,
                                 int& out_weight,
                                 bool& out_italic) {
    out_size   = 10.0f;
    out_family = "sans-serif";
    out_weight = 400;
    out_italic = false;

    std::istringstream iss(font_str);
    std::string tok;
    std::vector<std::string> tokens;
    while (iss >> tok) tokens.push_back(tok);

    bool found_size = false;
    size_t family_start = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        if (!found_size) {
            if (t == "italic" || t == "oblique") { out_italic = true; continue; }
            if (t == "bold")    { out_weight = 700; continue; }
            if (t == "bolder")  { out_weight = 900; continue; }
            if (t == "lighter") { out_weight = 300; continue; }
            if (t == "normal")  { continue; }

            // Numeric weight (e.g. "600")
            {
                bool all_digits = !t.empty();
                for (char c : t) if (c < '0' || c > '9') { all_digits = false; break; }
                if (all_digits && t.size() >= 3) {
                    try { out_weight = std::stoi(t); } catch (...) {}
                    continue;
                }
            }

            // Size token: contains "px", "pt", or "em"
            auto px_pos = t.find("px");
            auto pt_pos = t.find("pt");
            auto em_pos = t.find("em");
            auto sl_pos = t.find('/');
            std::string size_part = (sl_pos != std::string::npos) ? t.substr(0, sl_pos) : t;

            float sz = 0;
            bool parsed_size = false;
            try {
                if (px_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, px_pos));
                    parsed_size = true;
                } else if (pt_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, pt_pos)) * (96.0f / 72.0f);
                    parsed_size = true;
                } else if (em_pos != std::string::npos) {
                    sz = std::stof(size_part.substr(0, em_pos)) * 16.0f;
                    parsed_size = true;
                }
            } catch (...) {}

            if (parsed_size && sz > 0) {
                out_size = sz;
                found_size = true;
                family_start = i + 1;
            }
        }
    }

    if (found_size && family_start < tokens.size()) {
        std::string fam;
        for (size_t i = family_start; i < tokens.size(); ++i) {
            if (i > family_start) fam += ' ';
            fam += tokens[i];
        }
        // Strip surrounding quotes
        if (fam.size() >= 2 &&
            ((fam.front() == '"' && fam.back() == '"') ||
             (fam.front() == '\'' && fam.back() == '\''))) {
            fam = fam.substr(1, fam.size() - 2);
        }
        if (!fam.empty()) out_family = fam;
    }
}

static JSValue js_canvas2d_fill_text(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || argc < 3) return JS_UNDEFINED;

    const char* text_cstr = JS_ToCString(ctx, argv[0]);
    if (!text_cstr) return JS_UNDEFINED;
    double x, y;
    JS_ToFloat64(ctx, &x, argv[1]);
    JS_ToFloat64(ctx, &y, argv[2]);

    // Optional maxWidth parameter (4th argument)
    float max_width = 0.0f;
    if (argc >= 4) {
        double mw = 0;
        JS_ToFloat64(ctx, &mw, argv[3]);
        if (mw > 0) max_width = static_cast<float>(mw);
    }

    std::string txt(text_cstr);
    JS_FreeCString(ctx, text_cstr);

    // Parse font string to extract size, family, weight, italic
    float font_size = 10.0f;
    std::string font_family = "sans-serif";
    int font_weight = 400;
    bool font_italic = false;
    parse_canvas2d_font(s->font, font_size, font_family, font_weight, font_italic);

#ifdef __APPLE__
    // --- Real CoreText rendering via canvas_text_bridge ---
    uint8_t* raw_buf = s->buffer->data();
    clever::paint::canvas_render_text(
        txt,
        static_cast<float>(x),
        static_cast<float>(y),
        font_size,
        font_family,
        font_weight,
        font_italic,
        s->fill_color,
        s->global_alpha,
        s->text_align,
        s->text_baseline,
        raw_buf,
        s->width,
        s->height,
        max_width);
#else
    // --- Fallback: colored rectangles per character position ---
    float char_w = font_size * 0.6f;
    float total_w = char_w * static_cast<float>(txt.size());
    float start_x = static_cast<float>(x);
    if (s->text_align == 1) start_x -= total_w / 2.0f;
    else if (s->text_align == 2 || s->text_align == 3) start_x -= total_w;

    uint8_t r = (s->fill_color >> 16) & 0xFF;
    uint8_t g = (s->fill_color >> 8)  & 0xFF;
    uint8_t b =  s->fill_color        & 0xFF;
    uint8_t a = static_cast<uint8_t>(((s->fill_color >> 24) & 0xFF) * s->global_alpha);
    float text_top = static_cast<float>(y) - font_size * 0.8f;

    for (size_t i = 0; i < txt.size(); i++) {
        if (txt[i] == ' ') continue;
        int cx = static_cast<int>(start_x + static_cast<float>(i) * char_w + char_w * 0.1f);
        int cy = static_cast<int>(text_top);
        int cw = static_cast<int>(char_w * 0.8f);
        int ch = static_cast<int>(font_size);
        for (int py = std::max(0, cy); py < std::min(s->height, cy + ch); py++) {
            for (int px = std::max(0, cx); px < std::min(s->width, cx + cw); px++) {
                int idx = (py * s->width + px) * 4;
                (*s->buffer)[idx]     = r;
                (*s->buffer)[idx + 1] = g;
                (*s->buffer)[idx + 2] = b;
                (*s->buffer)[idx + 3] = a;
            }
        }
    }
#endif

    return JS_UNDEFINED;
}

static JSValue js_canvas2d_stroke_text(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || argc < 3) return JS_UNDEFINED;

    const char* text_cstr = JS_ToCString(ctx, argv[0]);
    if (!text_cstr) return JS_UNDEFINED;
    double x, y;
    JS_ToFloat64(ctx, &x, argv[1]);
    JS_ToFloat64(ctx, &y, argv[2]);

    // Optional maxWidth parameter (4th argument)
    float max_width = 0.0f;
    if (argc >= 4) {
        double mw = 0;
        JS_ToFloat64(ctx, &mw, argv[3]);
        if (mw > 0) max_width = static_cast<float>(mw);
    }

    std::string txt(text_cstr);
    JS_FreeCString(ctx, text_cstr);

    // Parse font string to extract size, family, weight, italic
    float font_size = 10.0f;
    std::string font_family = "sans-serif";
    int font_weight = 400;
    bool font_italic = false;
    parse_canvas2d_font(s->font, font_size, font_family, font_weight, font_italic);

#ifdef __APPLE__
    // --- Real CoreText stroke rendering via canvas_text_bridge ---
    uint8_t* raw_buf = s->buffer->data();
    clever::paint::canvas_render_text_stroke(
        txt,
        static_cast<float>(x),
        static_cast<float>(y),
        font_size,
        font_family,
        font_weight,
        font_italic,
        s->stroke_color,
        s->global_alpha,
        s->text_align,
        s->text_baseline,
        s->line_width,
        raw_buf,
        s->width,
        s->height,
        max_width);
#else
    // --- Fallback: outline rectangles per character position ---
    float char_w = font_size * 0.6f;
    float total_w = char_w * static_cast<float>(txt.size());
    float start_x = static_cast<float>(x);
    if (s->text_align == 1) start_x -= total_w / 2.0f;
    else if (s->text_align == 2 || s->text_align == 3) start_x -= total_w;

    uint8_t r = (s->stroke_color >> 16) & 0xFF;
    uint8_t g = (s->stroke_color >> 8)  & 0xFF;
    uint8_t b =  s->stroke_color        & 0xFF;
    uint8_t a = static_cast<uint8_t>(((s->stroke_color >> 24) & 0xFF) * s->global_alpha);
    float text_top = static_cast<float>(y) - font_size * 0.8f;
    int lw = std::max(1, static_cast<int>(s->line_width));

    auto set_pixel = [&](int px, int py) {
        if (px < 0 || py < 0 || px >= s->width || py >= s->height) return;
        int idx = (py * s->width + px) * 4;
        (*s->buffer)[idx]     = r;
        (*s->buffer)[idx + 1] = g;
        (*s->buffer)[idx + 2] = b;
        (*s->buffer)[idx + 3] = a;
    };

    for (size_t i = 0; i < txt.size(); i++) {
        if (txt[i] == ' ') continue;
        int cx = static_cast<int>(start_x + static_cast<float>(i) * char_w + char_w * 0.1f);
        int cy = static_cast<int>(text_top);
        int cw = static_cast<int>(char_w * 0.8f);
        int ch = static_cast<int>(font_size);
        // Draw only the border of the character box (outline / stroke)
        for (int t = 0; t < lw; t++) {
            for (int px = cx + t; px < cx + cw - t; px++) {
                set_pixel(px, cy + t);
                set_pixel(px, cy + ch - 1 - t);
            }
            for (int py = cy + t; py < cy + ch - t; py++) {
                set_pixel(cx + t, py);
                set_pixel(cx + cw - 1 - t, py);
            }
        }
    }
#endif

    return JS_UNDEFINED;
}

static JSValue js_canvas2d_measure_text(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "width", JS_NewFloat64(ctx, 0));
        return obj;
    }

    const char* text_cstr = JS_ToCString(ctx, argv[0]);
    if (!text_cstr) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "width", JS_NewFloat64(ctx, 0));
        return obj;
    }
    std::string txt(text_cstr);
    JS_FreeCString(ctx, text_cstr);

    float font_size = 10.0f;
    std::string font_family = "sans-serif";
    int font_weight = 400;
    bool font_italic = false;
    if (s) parse_canvas2d_font(s->font, font_size, font_family, font_weight, font_italic);

    float width = 0.0f;
#ifdef __APPLE__
    width = clever::paint::canvas_measure_text(txt, font_size, font_family,
                                                font_weight, font_italic);
    // Fall back to approximation if CoreText returns 0 for some reason
    if (width <= 0.0f && !txt.empty()) {
        width = font_size * 0.6f * static_cast<float>(txt.size());
    }
#else
    width = font_size * 0.6f * static_cast<float>(txt.size());
#endif

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "width", JS_NewFloat64(ctx, static_cast<double>(width)));
    return obj;
}

// ---- save/restore and transforms ----
static JSValue js_canvas2d_save(JSContext* /*ctx*/, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    Canvas2DState::SavedState st;
    st.fill_color = s->fill_color;
    st.stroke_color = s->stroke_color;
    st.fill_gradient = s->fill_gradient;
    st.stroke_gradient = s->stroke_gradient;
    st.fill_pattern = s->fill_pattern;
    st.stroke_pattern = s->stroke_pattern;
    st.line_width = s->line_width;
    st.global_alpha = s->global_alpha;
    st.font = s->font;
    st.text_align = s->text_align;
    st.text_baseline = s->text_baseline;
    st.line_cap = s->line_cap;
    st.line_join = s->line_join;
    st.miter_limit = s->miter_limit;
    st.shadow_color = s->shadow_color;
    st.shadow_blur = s->shadow_blur;
    st.shadow_offset_x = s->shadow_offset_x;
    st.shadow_offset_y = s->shadow_offset_y;
    st.global_composite_op = s->global_composite_op;
    st.image_smoothing = s->image_smoothing;
    st.tx_a = s->tx_a; st.tx_b = s->tx_b; st.tx_c = s->tx_c;
    st.tx_d = s->tx_d; st.tx_e = s->tx_e; st.tx_f = s->tx_f;
    st.has_clip = s->has_clip;
    st.clip_mask = s->clip_mask; // copy current clip mask
    s->state_stack.push_back(std::move(st));
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_restore(JSContext* /*ctx*/, JSValueConst this_val,
                                    int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || s->state_stack.empty()) return JS_UNDEFINED;
    auto& st = s->state_stack.back();
    s->fill_color = st.fill_color;
    s->stroke_color = st.stroke_color;
    s->fill_gradient = st.fill_gradient;
    s->stroke_gradient = st.stroke_gradient;
    s->fill_pattern = st.fill_pattern;
    s->stroke_pattern = st.stroke_pattern;
    s->line_width = st.line_width;
    s->global_alpha = st.global_alpha;
    s->font = std::move(st.font);
    s->text_align = st.text_align;
    s->text_baseline = st.text_baseline;
    s->line_cap = st.line_cap;
    s->line_join = st.line_join;
    s->miter_limit = st.miter_limit;
    s->shadow_color = st.shadow_color;
    s->shadow_blur = st.shadow_blur;
    s->shadow_offset_x = st.shadow_offset_x;
    s->shadow_offset_y = st.shadow_offset_y;
    s->global_composite_op = std::move(st.global_composite_op);
    s->image_smoothing = st.image_smoothing;
    s->tx_a = st.tx_a; s->tx_b = st.tx_b; s->tx_c = st.tx_c;
    s->tx_d = st.tx_d; s->tx_e = st.tx_e; s->tx_f = st.tx_f;
    s->has_clip = st.has_clip;
    s->clip_mask = std::move(st.clip_mask); // restore clip mask
    s->state_stack.pop_back();
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_translate(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_UNDEFINED;
    double tx, ty;
    JS_ToFloat64(ctx, &tx, argv[0]);
    JS_ToFloat64(ctx, &ty, argv[1]);
    // Post-multiply current matrix by translate(tx, ty)
    s->tx_e += s->tx_a * static_cast<float>(tx) + s->tx_c * static_cast<float>(ty);
    s->tx_f += s->tx_b * static_cast<float>(tx) + s->tx_d * static_cast<float>(ty);
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_rotate(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double angle;
    JS_ToFloat64(ctx, &angle, argv[0]);
    float cosv = std::cos(static_cast<float>(angle));
    float sinv = std::sin(static_cast<float>(angle));
    float a = s->tx_a, b = s->tx_b, c = s->tx_c, d = s->tx_d;
    s->tx_a = a * cosv + c * sinv;
    s->tx_b = b * cosv + d * sinv;
    s->tx_c = a * (-sinv) + c * cosv;
    s->tx_d = b * (-sinv) + d * cosv;
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_scale(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_UNDEFINED;
    double sx, sy;
    JS_ToFloat64(ctx, &sx, argv[0]);
    JS_ToFloat64(ctx, &sy, argv[1]);
    s->tx_a *= static_cast<float>(sx);
    s->tx_b *= static_cast<float>(sx);
    s->tx_c *= static_cast<float>(sy);
    s->tx_d *= static_cast<float>(sy);
    return JS_UNDEFINED;
}

// ---- quadraticCurveTo ----
static JSValue js_canvas2d_quadratic_curve_to(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double cpx, cpy, x, y;
    JS_ToFloat64(ctx, &cpx, argv[0]);
    JS_ToFloat64(ctx, &cpy, argv[1]);
    JS_ToFloat64(ctx, &x, argv[2]);
    JS_ToFloat64(ctx, &y, argv[3]);
    // Approximate with line segments (de Casteljau subdivision)
    float sx = s->path_x, sy = s->path_y;
    constexpr int steps = 16;
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float u = 1.0f - t;
        float px = u * u * sx + 2 * u * t * static_cast<float>(cpx) + t * t * static_cast<float>(x);
        float py = u * u * sy + 2 * u * t * static_cast<float>(cpy) + t * t * static_cast<float>(y);
        s->path_points.push_back({px, py, false});
    }
    s->path_x = static_cast<float>(x);
    s->path_y = static_cast<float>(y);
    return JS_UNDEFINED;
}

// ---- bezierCurveTo ----
static JSValue js_canvas2d_bezier_curve_to(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 6) return JS_UNDEFINED;
    double cp1x, cp1y, cp2x, cp2y, x, y;
    JS_ToFloat64(ctx, &cp1x, argv[0]);
    JS_ToFloat64(ctx, &cp1y, argv[1]);
    JS_ToFloat64(ctx, &cp2x, argv[2]);
    JS_ToFloat64(ctx, &cp2y, argv[3]);
    JS_ToFloat64(ctx, &x, argv[4]);
    JS_ToFloat64(ctx, &y, argv[5]);
    float sx = s->path_x, sy = s->path_y;
    constexpr int steps = 20;
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float u = 1.0f - t;
        float px = u*u*u*sx + 3*u*u*t*static_cast<float>(cp1x) +
                   3*u*t*t*static_cast<float>(cp2x) + t*t*t*static_cast<float>(x);
        float py = u*u*u*sy + 3*u*u*t*static_cast<float>(cp1y) +
                   3*u*t*t*static_cast<float>(cp2y) + t*t*t*static_cast<float>(y);
        s->path_points.push_back({px, py, false});
    }
    s->path_x = static_cast<float>(x);
    s->path_y = static_cast<float>(y);
    return JS_UNDEFINED;
}

// ---- arcTo ----
static JSValue js_canvas2d_arc_to(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 5) return JS_UNDEFINED;
    double x1d, y1d, x2d, y2d, rd;
    JS_ToFloat64(ctx, &x1d, argv[0]);
    JS_ToFloat64(ctx, &y1d, argv[1]);
    JS_ToFloat64(ctx, &x2d, argv[2]);
    JS_ToFloat64(ctx, &y2d, argv[3]);
    JS_ToFloat64(ctx, &rd, argv[4]);
    // Simplified: just draw line to (x1,y1) then to (x2,y2) for now
    s->path_points.push_back({static_cast<float>(x1d), static_cast<float>(y1d), false});
    s->path_points.push_back({static_cast<float>(x2d), static_cast<float>(y2d), false});
    s->path_x = static_cast<float>(x2d);
    s->path_y = static_cast<float>(y2d);
    return JS_UNDEFINED;
}

// ---- ellipse ----
static JSValue js_canvas2d_ellipse(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 5) return JS_UNDEFINED;
    double cx, cy, rx, ry, rotation = 0, start_angle = 0, end_angle = 6.2832;
    int ccw = 0;
    JS_ToFloat64(ctx, &cx, argv[0]);
    JS_ToFloat64(ctx, &cy, argv[1]);
    JS_ToFloat64(ctx, &rx, argv[2]);
    JS_ToFloat64(ctx, &ry, argv[3]);
    if (argc > 4) JS_ToFloat64(ctx, &rotation, argv[4]);
    if (argc > 5) JS_ToFloat64(ctx, &start_angle, argv[5]);
    if (argc > 6) JS_ToFloat64(ctx, &end_angle, argv[6]);
    if (argc > 7) ccw = JS_ToBool(ctx, argv[7]);
    // Approximate with line segments
    constexpr int steps = 32;
    double sweep = end_angle - start_angle;
    if (ccw && sweep > 0) sweep -= 2 * 3.14159265358979323846;
    if (!ccw && sweep < 0) sweep += 2 * 3.14159265358979323846;
    for (int i = 0; i <= steps; ++i) {
        double t = start_angle + sweep * i / steps;
        double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
        double px = cx + rx * std::cos(t) * cos_r - ry * std::sin(t) * sin_r;
        double py = cy + rx * std::cos(t) * sin_r + ry * std::sin(t) * cos_r;
        s->path_points.push_back({static_cast<float>(px), static_cast<float>(py),
                                   i == 0});
    }
    s->path_x = s->path_points.back().x;
    s->path_y = s->path_points.back().y;
    return JS_UNDEFINED;
}

// ---- isPointInPath — ray-casting with even-odd or nonzero winding ----
static JSValue js_canvas2d_is_point_in_path(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_NewBool(ctx, 0);

    double test_x, test_y;
    JS_ToFloat64(ctx, &test_x, argv[0]);
    JS_ToFloat64(ctx, &test_y, argv[1]);

    // Optional fill rule: "nonzero" (default) or "evenodd"
    bool use_evenodd = false;
    if (argc >= 3) {
        const char* fr = JS_ToCString(ctx, argv[2]);
        if (fr) {
            if (std::strcmp(fr, "evenodd") == 0) use_evenodd = true;
            JS_FreeCString(ctx, fr);
        }
    }

    // Apply inverse of the current transform to bring the test point into path space.
    // Forward transform: px' = a*x + c*y + e,  py' = b*x + d*y + f
    // Inverse (assuming non-singular):
    //   det = a*d - b*c
    //   inv_a =  d/det, inv_b = -b/det
    //   inv_c = -c/det, inv_d =  a/det
    //   inv_e = (c*f - d*e)/det, inv_f = (b*e - a*f)/det
    double a = s->tx_a, b = s->tx_b, c = s->tx_c, d = s->tx_d, e = s->tx_e, f = s->tx_f;
    double det = a * d - b * c;
    if (std::fabs(det) > 1e-10) {
        double inv_a =  d / det;
        double inv_b = -b / det;
        double inv_c = -c / det;
        double inv_d =  a / det;
        double inv_e = (c * f - d * e) / det;
        double inv_f = (b * e - a * f) / det;
        double nx = inv_a * test_x + inv_c * test_y + inv_e;
        double ny = inv_b * test_x + inv_d * test_y + inv_f;
        test_x = nx;
        test_y = ny;
    }

    // Ray-casting: cast a horizontal ray to +X from (test_x, test_y).
    // Iterate over segments formed by consecutive non-move path points.
    // A new sub-path starts at each point where is_move == true.
    // We implicitly close each sub-path back to its start for the containment test.
    int winding = 0;   // nonzero winding number
    int crossings = 0; // even-odd crossing count

    const auto& pts = s->path_points;
    const size_t n = pts.size();

    // Helper: process one edge (x0,y0) -> (x1,y1)
    auto process_edge = [&](double x0, double y0, double x1, double y1) {
        // Skip horizontal edges
        if (y0 == y1) return;

        double min_y = std::min(y0, y1);
        double max_y = std::max(y0, y1);

        // Ray is at test_y; edge must straddle it (exclusive on the bottom vertex)
        if (test_y < min_y || test_y >= max_y) return;

        // Compute x coordinate of edge at test_y
        double t = (test_y - y0) / (y1 - y0);
        double intersect_x = x0 + t * (x1 - x0);

        if (intersect_x > test_x) {
            ++crossings;
            if (y1 > y0)
                ++winding;  // upward crossing
            else
                --winding;  // downward crossing
        }
    };

    // Walk the path, tracking the start of each sub-path so we can close it.
    double sub_start_x = 0, sub_start_y = 0;
    double prev_x = 0, prev_y = 0;
    bool have_prev = false;

    for (size_t i = 0; i < n; ++i) {
        const auto& pt = pts[i];
        double px = pt.x, py = pt.y;

        if (pt.is_move) {
            // Close the previous sub-path before starting a new one
            if (have_prev) {
                process_edge(prev_x, prev_y, sub_start_x, sub_start_y);
            }
            sub_start_x = px;
            sub_start_y = py;
            prev_x = px;
            prev_y = py;
            have_prev = true;
        } else {
            if (have_prev) {
                process_edge(prev_x, prev_y, px, py);
            }
            prev_x = px;
            prev_y = py;
            have_prev = true;
        }
    }
    // Close the final sub-path
    if (have_prev) {
        process_edge(prev_x, prev_y, sub_start_x, sub_start_y);
    }

    bool inside = use_evenodd ? ((crossings % 2) != 0) : (winding != 0);
    return JS_NewBool(ctx, inside ? 1 : 0);
}

// ---- isPointInStroke — distance-from-path test ----
static JSValue js_canvas2d_is_point_in_stroke(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 2) return JS_NewBool(ctx, 0);

    double test_x, test_y;
    JS_ToFloat64(ctx, &test_x, argv[0]);
    JS_ToFloat64(ctx, &test_y, argv[1]);

    // Apply inverse transform (same as isPointInPath)
    double a = s->tx_a, b = s->tx_b, c = s->tx_c, d = s->tx_d, e = s->tx_e, f = s->tx_f;
    double det = a * d - b * c;
    if (std::fabs(det) > 1e-10) {
        double inv_a =  d / det;
        double inv_b = -b / det;
        double inv_c = -c / det;
        double inv_d =  a / det;
        double inv_e = (c * f - d * e) / det;
        double inv_f = (b * e - a * f) / det;
        double nx = inv_a * test_x + inv_c * test_y + inv_e;
        double ny = inv_b * test_x + inv_d * test_y + inv_f;
        test_x = nx;
        test_y = ny;
    }

    double half_width = s->line_width * 0.5;
    double threshold_sq = half_width * half_width;

    const auto& pts = s->path_points;
    const size_t n = pts.size();

    // Minimum squared distance from point (px,py) to segment (ax,ay)-(bx,by)
    auto seg_dist_sq = [](double px, double py,
                          double ax, double ay,
                          double bx, double by) -> double {
        double dx = bx - ax, dy = by - ay;
        double len_sq = dx * dx + dy * dy;
        if (len_sq < 1e-20) {
            double ex = px - ax, ey = py - ay;
            return ex * ex + ey * ey;
        }
        double t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
        t = std::max(0.0, std::min(1.0, t));
        double cx = ax + t * dx - px;
        double cy = ay + t * dy - py;
        return cx * cx + cy * cy;
    };

    double prev_x = 0, prev_y = 0;
    bool have_prev = false;

    for (size_t i = 0; i < n; ++i) {
        const auto& pt = pts[i];
        double px = pt.x, py = pt.y;

        if (pt.is_move) {
            // Don't draw a segment on a move
            prev_x = px;
            prev_y = py;
            have_prev = true;
        } else {
            if (have_prev) {
                double d_sq = seg_dist_sq(test_x, test_y, prev_x, prev_y, px, py);
                if (d_sq <= threshold_sq) return JS_NewBool(ctx, 1);
            }
            prev_x = px;
            prev_y = py;
            have_prev = true;
        }
    }

    return JS_NewBool(ctx, 0);
}

// ---- setLineDash / getLineDash / lineDashOffset ----
static JSValue js_canvas2d_set_line_dash(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    JSValue arr = argv[0];
    if (!JS_IsArray(ctx, arr)) return JS_UNDEFINED;

    // Get array length
    JSValue len_val = JS_GetPropertyStr(ctx, arr, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    // Read values and validate (no negatives)
    std::vector<float> values;
    values.reserve(static_cast<size_t>(len));
    for (int32_t i = 0; i < len; ++i) {
        JSValue item = JS_GetPropertyUint32(ctx, arr, static_cast<uint32_t>(i));
        double v = 0.0;
        JS_ToFloat64(ctx, &v, item);
        JS_FreeValue(ctx, item);
        if (v < 0.0) return JS_UNDEFINED; // spec: abort if any value is negative
        values.push_back(static_cast<float>(v));
    }

    if (values.empty()) {
        // Empty array clears the dash pattern — back to solid line
        s->line_dash.clear();
        return JS_UNDEFINED;
    }

    // Per spec: if odd number of values, duplicate the array (concat with itself)
    if (values.size() % 2 != 0) {
        std::vector<float> doubled;
        doubled.reserve(values.size() * 2);
        doubled.insert(doubled.end(), values.begin(), values.end());
        doubled.insert(doubled.end(), values.begin(), values.end());
        s->line_dash = std::move(doubled);
    } else {
        s->line_dash = std::move(values);
    }
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_get_line_dash(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    JSValue arr = JS_NewArray(ctx);
    if (!s) return arr;
    for (uint32_t i = 0; i < static_cast<uint32_t>(s->line_dash.size()); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat64(ctx, s->line_dash[i]));
    }
    return arr;
}
// ---- lineDashOffset getter/setter ----
static JSValue js_canvas2d_get_line_dash_offset(JSContext* ctx, JSValueConst this_val,
                                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    return JS_NewFloat64(ctx, s->line_dash_offset);
}
static JSValue js_canvas2d_set_line_dash_offset(JSContext* ctx, JSValueConst this_val,
                                                  int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 1) return JS_UNDEFINED;
    double v = 0.0;
    JS_ToFloat64(ctx, &v, argv[0]);
    s->line_dash_offset = static_cast<float>(v);
    return JS_UNDEFINED;
}

// ---- createLinearGradient ----
static JSValue js_canvas2d_create_linear_gradient(JSContext* ctx, JSValueConst /*this_val*/,
                                                   int argc, JSValueConst* argv) {
    if (argc < 4) return JS_UNDEFINED;
    double x0, y0, x1, y1;
    JS_ToFloat64(ctx, &x0, argv[0]);
    JS_ToFloat64(ctx, &y0, argv[1]);
    JS_ToFloat64(ctx, &x1, argv[2]);
    JS_ToFloat64(ctx, &y1, argv[3]);
    // Return a gradient stub object with addColorStop method
    JSValue grad = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, grad, "type", JS_NewString(ctx, "linear"));
    JS_SetPropertyStr(ctx, grad, "x0", JS_NewFloat64(ctx, x0));
    JS_SetPropertyStr(ctx, grad, "y0", JS_NewFloat64(ctx, y0));
    JS_SetPropertyStr(ctx, grad, "x1", JS_NewFloat64(ctx, x1));
    JS_SetPropertyStr(ctx, grad, "y1", JS_NewFloat64(ctx, y1));
    JSValue stops = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, grad, "stops", stops);
    // addColorStop method
    const char* add_stop_src = "(function() { var g = this; g.addColorStop = function(offset, color) { g.stops.push({offset: offset, color: color}); }; })";
    JSValue add_fn = JS_Eval(ctx, add_stop_src, std::strlen(add_stop_src), "<gradient>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(add_fn)) {
        JS_Call(ctx, add_fn, grad, 0, nullptr);
    }
    JS_FreeValue(ctx, add_fn);
    return grad;
}

// ---- createRadialGradient ----
static JSValue js_canvas2d_create_radial_gradient(JSContext* ctx, JSValueConst /*this_val*/,
                                                   int argc, JSValueConst* argv) {
    if (argc < 6) return JS_UNDEFINED;
    double x0, y0, r0, x1, y1, r1;
    JS_ToFloat64(ctx, &x0, argv[0]);
    JS_ToFloat64(ctx, &y0, argv[1]);
    JS_ToFloat64(ctx, &r0, argv[2]);
    JS_ToFloat64(ctx, &x1, argv[3]);
    JS_ToFloat64(ctx, &y1, argv[4]);
    JS_ToFloat64(ctx, &r1, argv[5]);
    JSValue grad = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, grad, "type", JS_NewString(ctx, "radial"));
    JS_SetPropertyStr(ctx, grad, "x0", JS_NewFloat64(ctx, x0));
    JS_SetPropertyStr(ctx, grad, "y0", JS_NewFloat64(ctx, y0));
    JS_SetPropertyStr(ctx, grad, "r0", JS_NewFloat64(ctx, r0));
    JS_SetPropertyStr(ctx, grad, "x1", JS_NewFloat64(ctx, x1));
    JS_SetPropertyStr(ctx, grad, "y1", JS_NewFloat64(ctx, y1));
    JS_SetPropertyStr(ctx, grad, "r1", JS_NewFloat64(ctx, r1));
    JSValue stops = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, grad, "stops", stops);
    const char* add_stop_src = "(function() { var g = this; g.addColorStop = function(offset, color) { g.stops.push({offset: offset, color: color}); }; })";
    JSValue add_fn = JS_Eval(ctx, add_stop_src, std::strlen(add_stop_src), "<gradient>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(add_fn)) {
        JS_Call(ctx, add_fn, grad, 0, nullptr);
    }
    JS_FreeValue(ctx, add_fn);
    return grad;
}

// ---- createConicGradient ----
static JSValue js_canvas2d_create_conic_gradient(JSContext* ctx, JSValueConst /*this_val*/,
                                                  int argc, JSValueConst* argv) {
    if (argc < 3) return JS_UNDEFINED;
    double startAngle, cx, cy;
    JS_ToFloat64(ctx, &startAngle, argv[0]);
    JS_ToFloat64(ctx, &cx, argv[1]);
    JS_ToFloat64(ctx, &cy, argv[2]);
    JSValue grad = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, grad, "type", JS_NewString(ctx, "conic"));
    JS_SetPropertyStr(ctx, grad, "startAngle", JS_NewFloat64(ctx, startAngle));
    JS_SetPropertyStr(ctx, grad, "cx", JS_NewFloat64(ctx, cx));
    JS_SetPropertyStr(ctx, grad, "cy", JS_NewFloat64(ctx, cy));
    JSValue stops = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, grad, "stops", stops);
    const char* add_stop_src = "(function() { var g = this; g.addColorStop = function(offset, color) { g.stops.push({offset: offset, color: color}); }; })";
    JSValue add_fn = JS_Eval(ctx, add_stop_src, std::strlen(add_stop_src), "<gradient>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(add_fn)) {
        JS_Call(ctx, add_fn, grad, 0, nullptr);
    }
    JS_FreeValue(ctx, add_fn);
    return grad;
}

// ---- createPattern: create a tiling pattern from an image/canvas source ----
// createPattern(source, repetition)
// source: HTMLImageElement (with __pixels RGBA array), HTMLCanvasElement (with __canvas2d_ctx),
//         or ImageData {width, height, data[]}
// repetition: "repeat" | "repeat-x" | "repeat-y" | "no-repeat"
static JSValue js_canvas2d_create_pattern(JSContext* ctx, JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    JSValue source = argv[0];
    int src_w = 0, src_h = 0;
    std::vector<uint8_t> src_pixels;

    // --- Try: HTMLCanvasElement (has __canvas2d_ctx) ---
    JSValue src_ctx_val = JS_GetPropertyStr(ctx, source, "__canvas2d_ctx");
    if (!JS_IsUndefined(src_ctx_val) && !JS_IsNull(src_ctx_val)) {
        auto* src_state = static_cast<Canvas2DState*>(JS_GetOpaque(src_ctx_val, canvas2d_class_id));
        if (src_state && src_state->buffer) {
            JSValue jw = JS_GetPropertyStr(ctx, source, "width");
            JSValue jh = JS_GetPropertyStr(ctx, source, "height");
            int32_t elem_w = 0, elem_h = 0;
            if (!JS_IsUndefined(jw)) JS_ToInt32(ctx, &elem_w, jw);
            if (!JS_IsUndefined(jh)) JS_ToInt32(ctx, &elem_h, jh);
            JS_FreeValue(ctx, jw); JS_FreeValue(ctx, jh);
            src_w = (elem_w > 0 && elem_w <= src_state->width) ? elem_w : src_state->width;
            src_h = (elem_h > 0 && elem_h <= src_state->height) ? elem_h : src_state->height;
            // Copy pixels (RGBA, src_state->width stride)
            src_pixels.resize(static_cast<size_t>(src_w) * src_h * 4);
            for (int row = 0; row < src_h; row++) {
                const uint8_t* src_row = src_state->buffer->data() + row * src_state->width * 4;
                uint8_t* dst_row = src_pixels.data() + row * src_w * 4;
                std::memcpy(dst_row, src_row, static_cast<size_t>(src_w) * 4);
            }
        }
        JS_FreeValue(ctx, src_ctx_val);
    } else {
        JS_FreeValue(ctx, src_ctx_val);

        // --- Try: HTMLImageElement (has __pixels RGBA flat array, naturalWidth/naturalHeight) ---
        JSValue nat_w_val = JS_GetPropertyStr(ctx, source, "naturalWidth");
        JSValue nat_h_val = JS_GetPropertyStr(ctx, source, "naturalHeight");
        JSValue pix_val = JS_GetPropertyStr(ctx, source, "__pixels");
        int32_t nw = 0, nh = 0;
        JS_ToInt32(ctx, &nw, nat_w_val);
        JS_ToInt32(ctx, &nh, nat_h_val);
        JS_FreeValue(ctx, nat_w_val); JS_FreeValue(ctx, nat_h_val);

        if (nw > 0 && nh > 0 && JS_IsArray(ctx, pix_val)) {
            src_w = nw;
            src_h = nh;
            int total = nw * nh * 4;
            src_pixels.resize(static_cast<size_t>(total));
            for (int i = 0; i < total; i++) {
                JSValue v = JS_GetPropertyUint32(ctx, pix_val, static_cast<uint32_t>(i));
                int32_t bv = 0;
                JS_ToInt32(ctx, &bv, v);
                JS_FreeValue(ctx, v);
                src_pixels[i] = static_cast<uint8_t>(bv < 0 ? 0 : (bv > 255 ? 255 : bv));
            }
        } else {
            // --- Try: ImageData {width, height, data[]} ---
            JSValue w_val = JS_GetPropertyStr(ctx, source, "width");
            JSValue h_val = JS_GetPropertyStr(ctx, source, "height");
            JSValue d_val = JS_GetPropertyStr(ctx, source, "data");
            int32_t iw = 0, ih = 0;
            if (!JS_IsUndefined(w_val)) JS_ToInt32(ctx, &iw, w_val);
            if (!JS_IsUndefined(h_val)) JS_ToInt32(ctx, &ih, h_val);
            if (iw > 0 && ih > 0 && JS_IsArray(ctx, d_val)) {
                src_w = iw;
                src_h = ih;
                int total = iw * ih * 4;
                src_pixels.resize(static_cast<size_t>(total));
                for (int i = 0; i < total; i++) {
                    JSValue v = JS_GetPropertyUint32(ctx, d_val, static_cast<uint32_t>(i));
                    int32_t bv = 0;
                    JS_ToInt32(ctx, &bv, v);
                    JS_FreeValue(ctx, v);
                    src_pixels[i] = static_cast<uint8_t>(bv < 0 ? 0 : (bv > 255 ? 255 : bv));
                }
            }
            JS_FreeValue(ctx, w_val);
            JS_FreeValue(ctx, h_val);
            JS_FreeValue(ctx, d_val);
        }
        JS_FreeValue(ctx, pix_val);
    }

    if (src_w <= 0 || src_h <= 0 || src_pixels.empty()) {
        // Per spec: invalid source still returns a CanvasPattern object (with empty pixels)
        // Some browsers return null, but we return an inert pattern object to match test expectations.
        JSValue empty_pat = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, empty_pat, "type", JS_NewString(ctx, "pattern"));
        JS_SetPropertyStr(ctx, empty_pat, "__patWidth", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, empty_pat, "__patHeight", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, empty_pat, "__repeat",
            argc >= 2 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]) ?
            JS_DupValue(ctx, argv[1]) : JS_NewString(ctx, "repeat"));
        JS_SetPropertyStr(ctx, empty_pat, "__pixels", JS_NewArray(ctx));
        const char* set_transform_src2 = "(function() { var p = this; p.setTransform = function() {}; })";
        JSValue fn2 = JS_Eval(ctx, set_transform_src2, std::strlen(set_transform_src2), "<pattern>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(fn2)) { JS_Call(ctx, fn2, empty_pat, 0, nullptr); }
        JS_FreeValue(ctx, fn2);
        return empty_pat;
    }

    // Parse repetition string (default: "repeat")
    std::string rep_str = "repeat";
    if (argc >= 2 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1])) {
        const char* rep_cstr = JS_ToCString(ctx, argv[1]);
        if (rep_cstr) {
            rep_str = rep_cstr;
            JS_FreeCString(ctx, rep_cstr);
        }
    }

    // Build the pattern JS object with embedded pixel data
    // The C++ pattern system reads back: type, __patWidth, __patHeight, __repeat, __pixels
    JSValue pat = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, pat, "type", JS_NewString(ctx, "pattern"));
    JS_SetPropertyStr(ctx, pat, "__patWidth", JS_NewInt32(ctx, src_w));
    JS_SetPropertyStr(ctx, pat, "__patHeight", JS_NewInt32(ctx, src_h));
    JS_SetPropertyStr(ctx, pat, "__repeat", JS_NewString(ctx, rep_str.c_str()));

    // Store pixels as a flat JS array of RGBA bytes
    JSValue pix_arr = JS_NewArray(ctx);
    int total = src_w * src_h * 4;
    for (int i = 0; i < total; i++) {
        JS_SetPropertyUint32(ctx, pix_arr, static_cast<uint32_t>(i),
                             JS_NewInt32(ctx, src_pixels[i]));
    }
    JS_SetPropertyStr(ctx, pat, "__pixels", pix_arr);

    // Provide setTransform stub for API compatibility
    const char* set_transform_src = "(function() { var p = this; p.setTransform = function() {}; })";
    JSValue fn = JS_Eval(ctx, set_transform_src, std::strlen(set_transform_src), "<pattern>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(fn)) {
        JS_Call(ctx, fn, pat, 0, nullptr);
    }
    JS_FreeValue(ctx, fn);

    return pat;
}

// ---- transform / setTransform / resetTransform ----
static JSValue js_canvas2d_transform(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 6) return JS_UNDEFINED;
    double a, b, c, d, e, f;
    JS_ToFloat64(ctx, &a, argv[0]);
    JS_ToFloat64(ctx, &b, argv[1]);
    JS_ToFloat64(ctx, &c, argv[2]);
    JS_ToFloat64(ctx, &d, argv[3]);
    JS_ToFloat64(ctx, &e, argv[4]);
    JS_ToFloat64(ctx, &f, argv[5]);
    float fa = static_cast<float>(a), fb = static_cast<float>(b);
    float fc = static_cast<float>(c), fd = static_cast<float>(d);
    float fe = static_cast<float>(e), ff = static_cast<float>(f);
    float cur_a = s->tx_a, cur_b = s->tx_b, cur_c = s->tx_c;
    float cur_d = s->tx_d, cur_e = s->tx_e, cur_f = s->tx_f;
    s->tx_a = cur_a * fa + cur_c * fb;
    s->tx_b = cur_b * fa + cur_d * fb;
    s->tx_c = cur_a * fc + cur_c * fd;
    s->tx_d = cur_b * fc + cur_d * fd;
    s->tx_e = cur_a * fe + cur_c * ff + cur_e;
    s->tx_f = cur_b * fe + cur_d * ff + cur_f;
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_set_transform(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 6) return JS_UNDEFINED;
    double a, b, c, d, e, f;
    JS_ToFloat64(ctx, &a, argv[0]);
    JS_ToFloat64(ctx, &b, argv[1]);
    JS_ToFloat64(ctx, &c, argv[2]);
    JS_ToFloat64(ctx, &d, argv[3]);
    JS_ToFloat64(ctx, &e, argv[4]);
    JS_ToFloat64(ctx, &f, argv[5]);
    s->tx_a = static_cast<float>(a);
    s->tx_b = static_cast<float>(b);
    s->tx_c = static_cast<float>(c);
    s->tx_d = static_cast<float>(d);
    s->tx_e = static_cast<float>(e);
    s->tx_f = static_cast<float>(f);
    return JS_UNDEFINED;
}
static JSValue js_canvas2d_reset_transform(JSContext* /*ctx*/, JSValueConst this_val,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;
    s->tx_a = 1.0f; s->tx_b = 0.0f;
    s->tx_c = 0.0f; s->tx_d = 1.0f;
    s->tx_e = 0.0f; s->tx_f = 0.0f;
    return JS_UNDEFINED;
}
// ---- clip ----
// Rasterises the current path into a new clip mask using the same
// even-odd scanline algorithm as js_canvas2d_fill, then intersects
// (ANDs) it with any existing clip mask.  Future draw calls skip
// pixels whose clip_mask entry is 0x00.
static JSValue js_canvas2d_clip(JSContext* /*ctx*/, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s) return JS_UNDEFINED;

    int total = s->width * s->height;
    if (total <= 0) return JS_UNDEFINED;

    // Build a fresh mask for this path (default 0x00 = all clipped)
    std::vector<uint8_t> new_mask(total, 0x00);

    if (!s->path_points.empty()) {
        // Build edges from the current path
        struct Edge { float x0, y0, x1, y1; };
        std::vector<Edge> edges;
        float prev_x = 0, prev_y = 0;
        bool have_prev = false;
        for (auto& pt : s->path_points) {
            if (pt.is_move) {
                prev_x = pt.x; prev_y = pt.y;
                have_prev = true;
            } else if (have_prev) {
                edges.push_back({prev_x, prev_y, pt.x, pt.y});
                prev_x = pt.x; prev_y = pt.y;
            }
        }

        // Find bounding box
        float min_y = s->path_points[0].y, max_y = s->path_points[0].y;
        for (auto& pt : s->path_points) {
            if (pt.y < min_y) min_y = pt.y;
            if (pt.y > max_y) max_y = pt.y;
        }
        int iy_start = std::max(0, static_cast<int>(min_y));
        int iy_end   = std::min(s->height - 1, static_cast<int>(max_y));

        // Scanline fill (even-odd rule) — mark visible pixels in new_mask
        for (int y = iy_start; y <= iy_end; y++) {
            float scan_y = y + 0.5f;
            std::vector<float> intersections;
            for (auto& e : edges) {
                float ey0 = e.y0, ey1 = e.y1;
                if (ey0 == ey1) continue;
                if (scan_y < std::min(ey0, ey1) || scan_y >= std::max(ey0, ey1)) continue;
                float t = (scan_y - ey0) / (ey1 - ey0);
                float ix = e.x0 + t * (e.x1 - e.x0);
                intersections.push_back(ix);
            }
            std::sort(intersections.begin(), intersections.end());

            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                int x_start = std::max(0, static_cast<int>(intersections[i]));
                int x_end   = std::min(s->width, static_cast<int>(intersections[i + 1]) + 1);
                for (int x = x_start; x < x_end; x++) {
                    new_mask[y * s->width + x] = 0xFF;
                }
            }
        }
    }
    // (An empty path leaves new_mask all 0x00 — clips everything, per spec.)

    if (s->has_clip) {
        // Intersect: AND the new mask with the existing mask
        for (int i = 0; i < total; i++) {
            s->clip_mask[i] = (s->clip_mask[i] & new_mask[i]);
        }
    } else {
        // First clip: install the new mask
        s->clip_mask = std::move(new_mask);
        s->has_clip = true;
    }
    return JS_UNDEFINED;
}
// ---- roundRect ----
static JSValue js_canvas2d_round_rect(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || argc < 4) return JS_UNDEFINED;
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);
    // For simplicity, just add a rect path (ignoring radii)
    s->path_points.push_back({static_cast<float>(x), static_cast<float>(y), true});
    s->path_points.push_back({static_cast<float>(x + w), static_cast<float>(y), false});
    s->path_points.push_back({static_cast<float>(x + w), static_cast<float>(y + h), false});
    s->path_points.push_back({static_cast<float>(x), static_cast<float>(y + h), false});
    s->path_points.push_back({static_cast<float>(x), static_cast<float>(y), false});
    s->path_x = static_cast<float>(x);
    s->path_y = static_cast<float>(y);
    return JS_UNDEFINED;
}

// ---- drawImage: blit source pixels into canvas buffer ----
// Supports 3 forms:
//   drawImage(source, dx, dy)
//   drawImage(source, dx, dy, dw, dh)
//   drawImage(source, sx, sy, sw, sh, dx, dy, dw, dh)
// Source can be: another canvas element (with __canvas2d_ctx), or an ImageData object.
static JSValue js_canvas2d_draw_image(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || argc < 3) return JS_UNDEFINED;

    // Extract source pixel data (width, height, RGBA buffer pointer)
    int src_w = 0, src_h = 0;
    int src_stride = 0; // actual buffer stride in pixels (may differ from src_w)
    const uint8_t* src_data = nullptr;
    std::vector<uint8_t> temp_buf; // holds data if we need to copy from JS array

    JSValue source = argv[0];

    // Try source as a canvas element (has __canvas2d_ctx with opaque Canvas2DState)
    JSValue src_ctx_val = JS_GetPropertyStr(ctx, source, "__canvas2d_ctx");
    if (!JS_IsUndefined(src_ctx_val) && !JS_IsNull(src_ctx_val)) {
        auto* src_state = static_cast<Canvas2DState*>(JS_GetOpaque(src_ctx_val, canvas2d_class_id));
        if (src_state && src_state->buffer) {
            // The buffer stride is the Canvas2DState width (actual buffer layout)
            src_stride = src_state->width;
            // Use the canvas element's JS width/height properties for logical size
            JSValue jw = JS_GetPropertyStr(ctx, source, "width");
            JSValue jh = JS_GetPropertyStr(ctx, source, "height");
            int32_t elem_w = 0, elem_h = 0;
            if (!JS_IsUndefined(jw)) JS_ToInt32(ctx, &elem_w, jw);
            if (!JS_IsUndefined(jh)) JS_ToInt32(ctx, &elem_h, jh);
            JS_FreeValue(ctx, jw); JS_FreeValue(ctx, jh);
            src_w = (elem_w > 0 && elem_w <= src_state->width) ? elem_w : src_state->width;
            src_h = (elem_h > 0 && elem_h <= src_state->height) ? elem_h : src_state->height;
            src_data = src_state->buffer->data();
        }
        JS_FreeValue(ctx, src_ctx_val);
    } else {
        JS_FreeValue(ctx, src_ctx_val);
        // Try as ImageData: { width, height, data: [...] }
        JSValue w_val = JS_GetPropertyStr(ctx, source, "width");
        JSValue h_val = JS_GetPropertyStr(ctx, source, "height");
        JSValue d_val = JS_GetPropertyStr(ctx, source, "data");
        int32_t iw = 0, ih = 0;
        if (!JS_IsUndefined(w_val)) JS_ToInt32(ctx, &iw, w_val);
        if (!JS_IsUndefined(h_val)) JS_ToInt32(ctx, &ih, h_val);
        if (iw > 0 && ih > 0 && JS_IsArray(ctx, d_val)) {
            src_w = iw;
            src_h = ih;
            src_stride = iw; // ImageData stride matches width
            temp_buf.resize(static_cast<size_t>(iw) * ih * 4, 0);
            for (int i = 0; i < iw * ih * 4; i++) {
                JSValue v = JS_GetPropertyUint32(ctx, d_val, i);
                int32_t byte_val = 0;
                JS_ToInt32(ctx, &byte_val, v);
                JS_FreeValue(ctx, v);
                temp_buf[i] = static_cast<uint8_t>(byte_val < 0 ? 0 : (byte_val > 255 ? 255 : byte_val));
            }
            src_data = temp_buf.data();
        }
        JS_FreeValue(ctx, w_val);
        JS_FreeValue(ctx, h_val);
        JS_FreeValue(ctx, d_val);
    }

    if (!src_data || src_w <= 0 || src_h <= 0) return JS_UNDEFINED;

    // Parse the 3 call forms
    int sx = 0, sy = 0, sw = src_w, sh = src_h;
    double ddx, ddy, ddw, ddh;

    if (argc >= 9) {
        // drawImage(source, sx, sy, sw, sh, dx, dy, dw, dh)
        double dsx, dsy, dsw, dsh;
        JS_ToFloat64(ctx, &dsx, argv[1]); JS_ToFloat64(ctx, &dsy, argv[2]);
        JS_ToFloat64(ctx, &dsw, argv[3]); JS_ToFloat64(ctx, &dsh, argv[4]);
        JS_ToFloat64(ctx, &ddx, argv[5]); JS_ToFloat64(ctx, &ddy, argv[6]);
        JS_ToFloat64(ctx, &ddw, argv[7]); JS_ToFloat64(ctx, &ddh, argv[8]);
        sx = static_cast<int>(dsx); sy = static_cast<int>(dsy);
        sw = static_cast<int>(dsw); sh = static_cast<int>(dsh);
    } else if (argc >= 5) {
        // drawImage(source, dx, dy, dw, dh)
        JS_ToFloat64(ctx, &ddx, argv[1]); JS_ToFloat64(ctx, &ddy, argv[2]);
        JS_ToFloat64(ctx, &ddw, argv[3]); JS_ToFloat64(ctx, &ddh, argv[4]);
    } else {
        // drawImage(source, dx, dy)
        JS_ToFloat64(ctx, &ddx, argv[1]); JS_ToFloat64(ctx, &ddy, argv[2]);
        ddw = static_cast<double>(sw);
        ddh = static_cast<double>(sh);
    }

    int dx = static_cast<int>(ddx), dy = static_cast<int>(ddy);
    int dw = static_cast<int>(ddw), dh = static_cast<int>(ddh);
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return JS_UNDEFINED;
    if (src_stride <= 0) src_stride = src_w; // fallback

    // Blit: nearest-neighbor scaling from source rect to dest rect
    for (int py = 0; py < dh; py++) {
        int target_y = dy + py;
        if (target_y < 0 || target_y >= s->height) continue;
        for (int px = 0; px < dw; px++) {
            int target_x = dx + px;
            if (target_x < 0 || target_x >= s->width) continue;

            // Map destination pixel to source pixel (nearest neighbor)
            int sample_x = sx + (px * sw) / dw;
            int sample_y = sy + (py * sh) / dh;
            if (sample_x < 0 || sample_x >= src_w || sample_y < 0 || sample_y >= src_h) continue;

            // Use src_stride (actual buffer width) for index, not src_w (logical width)
            int src_idx = (sample_y * src_stride + sample_x) * 4;
            int dst_idx = (target_y * s->width + target_x) * 4;

            uint8_t sr = src_data[src_idx], sg = src_data[src_idx + 1];
            uint8_t sb = src_data[src_idx + 2], sa = src_data[src_idx + 3];

            // Alpha composite (source-over)
            float alpha = (sa / 255.0f) * s->global_alpha;
            if (alpha >= 1.0f) {
                (*s->buffer)[dst_idx]     = sr;
                (*s->buffer)[dst_idx + 1] = sg;
                (*s->buffer)[dst_idx + 2] = sb;
                (*s->buffer)[dst_idx + 3] = 255;
            } else if (alpha > 0.0f) {
                float inv = 1.0f - alpha;
                (*s->buffer)[dst_idx]     = static_cast<uint8_t>(sr * alpha + (*s->buffer)[dst_idx] * inv);
                (*s->buffer)[dst_idx + 1] = static_cast<uint8_t>(sg * alpha + (*s->buffer)[dst_idx + 1] * inv);
                (*s->buffer)[dst_idx + 2] = static_cast<uint8_t>(sb * alpha + (*s->buffer)[dst_idx + 2] * inv);
                (*s->buffer)[dst_idx + 3] = static_cast<uint8_t>(
                    std::min(255.0f, sa * alpha + (*s->buffer)[dst_idx + 3] * inv));
            }
        }
    }
    return JS_UNDEFINED;
}

// ---- Pixel manipulation ----
static JSValue js_canvas2d_get_image_data(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || argc < 4) return JS_UNDEFINED;

    double dx, dy, dw, dh;
    JS_ToFloat64(ctx, &dx, argv[0]);
    JS_ToFloat64(ctx, &dy, argv[1]);
    JS_ToFloat64(ctx, &dw, argv[2]);
    JS_ToFloat64(ctx, &dh, argv[3]);

    int x = static_cast<int>(dx), y = static_cast<int>(dy);
    int w = static_cast<int>(dw), h = static_cast<int>(dh);
    if (w <= 0 || h <= 0) return JS_UNDEFINED;

    // Create ImageData object
    JSValue img_data = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, img_data, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, img_data, "height", JS_NewInt32(ctx, h));

    // Create data array (Uint8ClampedArray-like: just a regular array for now)
    JSValue data_arr = JS_NewArray(ctx);

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int src_x = x + px;
            int src_y = y + py;
            int dst_idx = (py * w + px) * 4;
            if (src_x >= 0 && src_x < s->width && src_y >= 0 && src_y < s->height) {
                int src_idx = (src_y * s->width + src_x) * 4;
                JS_SetPropertyUint32(ctx, data_arr, dst_idx,
                    JS_NewInt32(ctx, (*s->buffer)[src_idx]));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 1,
                    JS_NewInt32(ctx, (*s->buffer)[src_idx + 1]));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 2,
                    JS_NewInt32(ctx, (*s->buffer)[src_idx + 2]));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 3,
                    JS_NewInt32(ctx, (*s->buffer)[src_idx + 3]));
            } else {
                JS_SetPropertyUint32(ctx, data_arr, dst_idx, JS_NewInt32(ctx, 0));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 1, JS_NewInt32(ctx, 0));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 2, JS_NewInt32(ctx, 0));
                JS_SetPropertyUint32(ctx, data_arr, dst_idx + 3, JS_NewInt32(ctx, 0));
            }
        }
    }

    JS_SetPropertyStr(ctx, img_data, "data", data_arr);
    return img_data;
}

static JSValue js_canvas2d_put_image_data(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* s = static_cast<Canvas2DState*>(JS_GetOpaque(this_val, canvas2d_class_id));
    if (!s || !s->buffer || argc < 3) return JS_UNDEFINED;

    JSValue img_data = argv[0];
    double dx, dy;
    JS_ToFloat64(ctx, &dx, argv[1]);
    JS_ToFloat64(ctx, &dy, argv[2]);
    int dest_x = static_cast<int>(dx);
    int dest_y = static_cast<int>(dy);

    JSValue w_val = JS_GetPropertyStr(ctx, img_data, "width");
    JSValue h_val = JS_GetPropertyStr(ctx, img_data, "height");
    JSValue data_val = JS_GetPropertyStr(ctx, img_data, "data");

    int w = 0, h = 0;
    JS_ToInt32(ctx, &w, w_val);
    JS_ToInt32(ctx, &h, h_val);
    JS_FreeValue(ctx, w_val);
    JS_FreeValue(ctx, h_val);

    if (w <= 0 || h <= 0 || !JS_IsObject(data_val)) {
        JS_FreeValue(ctx, data_val);
        return JS_UNDEFINED;
    }

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int tx = dest_x + px;
            int ty = dest_y + py;
            if (tx < 0 || tx >= s->width || ty < 0 || ty >= s->height) continue;
            int src_idx = (py * w + px) * 4;
            int dst_idx = (ty * s->width + tx) * 4;

            JSValue rv = JS_GetPropertyUint32(ctx, data_val, src_idx);
            JSValue gv = JS_GetPropertyUint32(ctx, data_val, src_idx + 1);
            JSValue bv = JS_GetPropertyUint32(ctx, data_val, src_idx + 2);
            JSValue av = JS_GetPropertyUint32(ctx, data_val, src_idx + 3);

            int ri, gi, bi, ai;
            JS_ToInt32(ctx, &ri, rv);
            JS_ToInt32(ctx, &gi, gv);
            JS_ToInt32(ctx, &bi, bv);
            JS_ToInt32(ctx, &ai, av);

            JS_FreeValue(ctx, rv);
            JS_FreeValue(ctx, gv);
            JS_FreeValue(ctx, bv);
            JS_FreeValue(ctx, av);

            (*s->buffer)[dst_idx]     = static_cast<uint8_t>(ri);
            (*s->buffer)[dst_idx + 1] = static_cast<uint8_t>(gi);
            (*s->buffer)[dst_idx + 2] = static_cast<uint8_t>(bi);
            (*s->buffer)[dst_idx + 3] = static_cast<uint8_t>(ai);
        }
    }

    JS_FreeValue(ctx, data_val);
    return JS_UNDEFINED;
}

static JSValue js_canvas2d_create_image_data(JSContext* ctx, JSValueConst /*this_val*/,
                                              int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    int w = 0, h = 0;
    JS_ToInt32(ctx, &w, argv[0]);
    JS_ToInt32(ctx, &h, argv[1]);
    if (w <= 0 || h <= 0) return JS_UNDEFINED;

    JSValue img_data = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, img_data, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, img_data, "height", JS_NewInt32(ctx, h));

    JSValue data_arr = JS_NewArray(ctx);
    int total_pixels = w * h * 4;
    for (int i = 0; i < total_pixels; i++) {
        JS_SetPropertyUint32(ctx, data_arr, i, JS_NewInt32(ctx, 0));
    }
    JS_SetPropertyStr(ctx, img_data, "data", data_arr);
    return img_data;
}

// ---- Create a Canvas2D context object with all methods ----
static JSValue create_canvas2d_context(JSContext* ctx, Canvas2DState* state) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(canvas2d_class_id));
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, state);

    // Rectangle methods
    JS_SetPropertyStr(ctx, obj, "fillRect",
        JS_NewCFunction(ctx, js_canvas2d_fill_rect, "fillRect", 4));
    JS_SetPropertyStr(ctx, obj, "strokeRect",
        JS_NewCFunction(ctx, js_canvas2d_stroke_rect, "strokeRect", 4));
    JS_SetPropertyStr(ctx, obj, "clearRect",
        JS_NewCFunction(ctx, js_canvas2d_clear_rect, "clearRect", 4));

    // Path methods
    JS_SetPropertyStr(ctx, obj, "beginPath",
        JS_NewCFunction(ctx, js_canvas2d_begin_path, "beginPath", 0));
    JS_SetPropertyStr(ctx, obj, "closePath",
        JS_NewCFunction(ctx, js_canvas2d_close_path, "closePath", 0));
    JS_SetPropertyStr(ctx, obj, "moveTo",
        JS_NewCFunction(ctx, js_canvas2d_move_to, "moveTo", 2));
    JS_SetPropertyStr(ctx, obj, "lineTo",
        JS_NewCFunction(ctx, js_canvas2d_line_to, "lineTo", 2));
    JS_SetPropertyStr(ctx, obj, "rect",
        JS_NewCFunction(ctx, js_canvas2d_rect, "rect", 4));
    JS_SetPropertyStr(ctx, obj, "arc",
        JS_NewCFunction(ctx, js_canvas2d_arc, "arc", 6));
    JS_SetPropertyStr(ctx, obj, "quadraticCurveTo",
        JS_NewCFunction(ctx, js_canvas2d_quadratic_curve_to, "quadraticCurveTo", 4));
    JS_SetPropertyStr(ctx, obj, "bezierCurveTo",
        JS_NewCFunction(ctx, js_canvas2d_bezier_curve_to, "bezierCurveTo", 6));
    JS_SetPropertyStr(ctx, obj, "arcTo",
        JS_NewCFunction(ctx, js_canvas2d_arc_to, "arcTo", 5));
    JS_SetPropertyStr(ctx, obj, "ellipse",
        JS_NewCFunction(ctx, js_canvas2d_ellipse, "ellipse", 7));
    JS_SetPropertyStr(ctx, obj, "isPointInPath",
        JS_NewCFunction(ctx, js_canvas2d_is_point_in_path, "isPointInPath", 2));
    JS_SetPropertyStr(ctx, obj, "isPointInStroke",
        JS_NewCFunction(ctx, js_canvas2d_is_point_in_stroke, "isPointInStroke", 2));
    JS_SetPropertyStr(ctx, obj, "setLineDash",
        JS_NewCFunction(ctx, js_canvas2d_set_line_dash, "setLineDash", 1));
    JS_SetPropertyStr(ctx, obj, "getLineDash",
        JS_NewCFunction(ctx, js_canvas2d_get_line_dash, "getLineDash", 0));
    JS_SetPropertyStr(ctx, obj, "fill",
        JS_NewCFunction(ctx, js_canvas2d_fill, "fill", 0));
    JS_SetPropertyStr(ctx, obj, "stroke",
        JS_NewCFunction(ctx, js_canvas2d_stroke, "stroke", 0));

    // Gradient/pattern methods
    JS_SetPropertyStr(ctx, obj, "createLinearGradient",
        JS_NewCFunction(ctx, js_canvas2d_create_linear_gradient, "createLinearGradient", 4));
    JS_SetPropertyStr(ctx, obj, "createRadialGradient",
        JS_NewCFunction(ctx, js_canvas2d_create_radial_gradient, "createRadialGradient", 6));
    JS_SetPropertyStr(ctx, obj, "createConicGradient",
        JS_NewCFunction(ctx, js_canvas2d_create_conic_gradient, "createConicGradient", 3));
    JS_SetPropertyStr(ctx, obj, "createPattern",
        JS_NewCFunction(ctx, js_canvas2d_create_pattern, "createPattern", 2));

    // Text methods
    JS_SetPropertyStr(ctx, obj, "fillText",
        JS_NewCFunction(ctx, js_canvas2d_fill_text, "fillText", 3));
    JS_SetPropertyStr(ctx, obj, "strokeText",
        JS_NewCFunction(ctx, js_canvas2d_stroke_text, "strokeText", 3));
    JS_SetPropertyStr(ctx, obj, "measureText",
        JS_NewCFunction(ctx, js_canvas2d_measure_text, "measureText", 1));

    // Transform stubs
    JS_SetPropertyStr(ctx, obj, "save",
        JS_NewCFunction(ctx, js_canvas2d_save, "save", 0));
    JS_SetPropertyStr(ctx, obj, "restore",
        JS_NewCFunction(ctx, js_canvas2d_restore, "restore", 0));
    JS_SetPropertyStr(ctx, obj, "translate",
        JS_NewCFunction(ctx, js_canvas2d_translate, "translate", 2));
    JS_SetPropertyStr(ctx, obj, "rotate",
        JS_NewCFunction(ctx, js_canvas2d_rotate, "rotate", 1));
    JS_SetPropertyStr(ctx, obj, "scale",
        JS_NewCFunction(ctx, js_canvas2d_scale, "scale", 2));
    JS_SetPropertyStr(ctx, obj, "transform",
        JS_NewCFunction(ctx, js_canvas2d_transform, "transform", 6));
    JS_SetPropertyStr(ctx, obj, "setTransform",
        JS_NewCFunction(ctx, js_canvas2d_set_transform, "setTransform", 6));
    JS_SetPropertyStr(ctx, obj, "resetTransform",
        JS_NewCFunction(ctx, js_canvas2d_reset_transform, "resetTransform", 0));
    JS_SetPropertyStr(ctx, obj, "clip",
        JS_NewCFunction(ctx, js_canvas2d_clip, "clip", 0));
    JS_SetPropertyStr(ctx, obj, "roundRect",
        JS_NewCFunction(ctx, js_canvas2d_round_rect, "roundRect", 5));

    // Image methods
    JS_SetPropertyStr(ctx, obj, "drawImage",
        JS_NewCFunction(ctx, js_canvas2d_draw_image, "drawImage", 3));

    // Pixel manipulation
    JS_SetPropertyStr(ctx, obj, "getImageData",
        JS_NewCFunction(ctx, js_canvas2d_get_image_data, "getImageData", 4));
    JS_SetPropertyStr(ctx, obj, "putImageData",
        JS_NewCFunction(ctx, js_canvas2d_put_image_data, "putImageData", 3));
    JS_SetPropertyStr(ctx, obj, "createImageData",
        JS_NewCFunction(ctx, js_canvas2d_create_image_data, "createImageData", 2));

    // Internal getters for properties (wired into getters/setters below)
    JS_SetPropertyStr(ctx, obj, "__getFillStyle",
        JS_NewCFunction(ctx, js_canvas2d_get_fill_style, "__getFillStyle", 0));
    JS_SetPropertyStr(ctx, obj, "__setFillStyle",
        JS_NewCFunction(ctx, js_canvas2d_set_fill_style, "__setFillStyle", 1));
    JS_SetPropertyStr(ctx, obj, "__getStrokeStyle",
        JS_NewCFunction(ctx, js_canvas2d_get_stroke_style, "__getStrokeStyle", 0));
    JS_SetPropertyStr(ctx, obj, "__setStrokeStyle",
        JS_NewCFunction(ctx, js_canvas2d_set_stroke_style, "__setStrokeStyle", 1));
    JS_SetPropertyStr(ctx, obj, "__getGlobalAlpha",
        JS_NewCFunction(ctx, js_canvas2d_get_global_alpha, "__getGlobalAlpha", 0));
    JS_SetPropertyStr(ctx, obj, "__setGlobalAlpha",
        JS_NewCFunction(ctx, js_canvas2d_set_global_alpha, "__setGlobalAlpha", 1));
    JS_SetPropertyStr(ctx, obj, "__getLineWidth",
        JS_NewCFunction(ctx, js_canvas2d_get_line_width, "__getLineWidth", 0));
    JS_SetPropertyStr(ctx, obj, "__setLineWidth",
        JS_NewCFunction(ctx, js_canvas2d_set_line_width, "__setLineWidth", 1));
    JS_SetPropertyStr(ctx, obj, "__getFont",
        JS_NewCFunction(ctx, js_canvas2d_get_font, "__getFont", 0));
    JS_SetPropertyStr(ctx, obj, "__setFont",
        JS_NewCFunction(ctx, js_canvas2d_set_font, "__setFont", 1));
    JS_SetPropertyStr(ctx, obj, "__getTextAlign",
        JS_NewCFunction(ctx, js_canvas2d_get_text_align, "__getTextAlign", 0));
    JS_SetPropertyStr(ctx, obj, "__setTextAlign",
        JS_NewCFunction(ctx, js_canvas2d_set_text_align, "__setTextAlign", 1));
    JS_SetPropertyStr(ctx, obj, "__getTextBaseline",
        JS_NewCFunction(ctx, js_canvas2d_get_text_baseline, "__getTextBaseline", 0));
    JS_SetPropertyStr(ctx, obj, "__setTextBaseline",
        JS_NewCFunction(ctx, js_canvas2d_set_text_baseline, "__setTextBaseline", 1));
    JS_SetPropertyStr(ctx, obj, "__getLineCap",
        JS_NewCFunction(ctx, js_canvas2d_get_line_cap, "__getLineCap", 0));
    JS_SetPropertyStr(ctx, obj, "__setLineCap",
        JS_NewCFunction(ctx, js_canvas2d_set_line_cap, "__setLineCap", 1));
    JS_SetPropertyStr(ctx, obj, "__getLineJoin",
        JS_NewCFunction(ctx, js_canvas2d_get_line_join, "__getLineJoin", 0));
    JS_SetPropertyStr(ctx, obj, "__setLineJoin",
        JS_NewCFunction(ctx, js_canvas2d_set_line_join, "__setLineJoin", 1));
    JS_SetPropertyStr(ctx, obj, "__getMiterLimit",
        JS_NewCFunction(ctx, js_canvas2d_get_miter_limit, "__getMiterLimit", 0));
    JS_SetPropertyStr(ctx, obj, "__setMiterLimit",
        JS_NewCFunction(ctx, js_canvas2d_set_miter_limit, "__setMiterLimit", 1));
    JS_SetPropertyStr(ctx, obj, "__getLineDashOffset",
        JS_NewCFunction(ctx, js_canvas2d_get_line_dash_offset, "__getLineDashOffset", 0));
    JS_SetPropertyStr(ctx, obj, "__setLineDashOffset",
        JS_NewCFunction(ctx, js_canvas2d_set_line_dash_offset, "__setLineDashOffset", 1));
    JS_SetPropertyStr(ctx, obj, "__getShadowColor",
        JS_NewCFunction(ctx, js_canvas2d_get_shadow_color, "__getShadowColor", 0));
    JS_SetPropertyStr(ctx, obj, "__setShadowColor",
        JS_NewCFunction(ctx, js_canvas2d_set_shadow_color, "__setShadowColor", 1));
    JS_SetPropertyStr(ctx, obj, "__getShadowBlur",
        JS_NewCFunction(ctx, js_canvas2d_get_shadow_blur, "__getShadowBlur", 0));
    JS_SetPropertyStr(ctx, obj, "__setShadowBlur",
        JS_NewCFunction(ctx, js_canvas2d_set_shadow_blur, "__setShadowBlur", 1));
    JS_SetPropertyStr(ctx, obj, "__getShadowOffsetX",
        JS_NewCFunction(ctx, js_canvas2d_get_shadow_offset_x, "__getShadowOffsetX", 0));
    JS_SetPropertyStr(ctx, obj, "__setShadowOffsetX",
        JS_NewCFunction(ctx, js_canvas2d_set_shadow_offset_x, "__setShadowOffsetX", 1));
    JS_SetPropertyStr(ctx, obj, "__getShadowOffsetY",
        JS_NewCFunction(ctx, js_canvas2d_get_shadow_offset_y, "__getShadowOffsetY", 0));
    JS_SetPropertyStr(ctx, obj, "__setShadowOffsetY",
        JS_NewCFunction(ctx, js_canvas2d_set_shadow_offset_y, "__setShadowOffsetY", 1));
    JS_SetPropertyStr(ctx, obj, "__getGlobalCompositeOp",
        JS_NewCFunction(ctx, js_canvas2d_get_global_composite_op, "__getGlobalCompositeOp", 0));
    JS_SetPropertyStr(ctx, obj, "__setGlobalCompositeOp",
        JS_NewCFunction(ctx, js_canvas2d_set_global_composite_op, "__setGlobalCompositeOp", 1));
    JS_SetPropertyStr(ctx, obj, "__getImageSmoothing",
        JS_NewCFunction(ctx, js_canvas2d_get_image_smoothing, "__getImageSmoothing", 0));
    JS_SetPropertyStr(ctx, obj, "__setImageSmoothing",
        JS_NewCFunction(ctx, js_canvas2d_set_image_smoothing, "__setImageSmoothing", 1));

    // Wire up getters/setters via eval
    const char* ctx2d_setup = R"JS(
(function(c) {
    Object.defineProperty(c, 'fillStyle', {
        get: function() { return c.__getFillStyle(); },
        set: function(v) { c.__setFillStyle(v); },
        configurable: true
    });
    Object.defineProperty(c, 'strokeStyle', {
        get: function() { return c.__getStrokeStyle(); },
        set: function(v) { c.__setStrokeStyle(v); },
        configurable: true
    });
    Object.defineProperty(c, 'globalAlpha', {
        get: function() { return c.__getGlobalAlpha(); },
        set: function(v) { c.__setGlobalAlpha(v); },
        configurable: true
    });
    Object.defineProperty(c, 'lineWidth', {
        get: function() { return c.__getLineWidth(); },
        set: function(v) { c.__setLineWidth(v); },
        configurable: true
    });
    Object.defineProperty(c, 'font', {
        get: function() { return c.__getFont(); },
        set: function(v) { c.__setFont(v); },
        configurable: true
    });
    Object.defineProperty(c, 'textAlign', {
        get: function() { return c.__getTextAlign(); },
        set: function(v) { c.__setTextAlign(v); },
        configurable: true
    });
    Object.defineProperty(c, 'textBaseline', {
        get: function() { return c.__getTextBaseline(); },
        set: function(v) { c.__setTextBaseline(v); },
        configurable: true
    });
    Object.defineProperty(c, 'lineCap', {
        get: function() { return c.__getLineCap(); },
        set: function(v) { c.__setLineCap(v); },
        configurable: true
    });
    Object.defineProperty(c, 'lineJoin', {
        get: function() { return c.__getLineJoin(); },
        set: function(v) { c.__setLineJoin(v); },
        configurable: true
    });
    Object.defineProperty(c, 'miterLimit', {
        get: function() { return c.__getMiterLimit(); },
        set: function(v) { c.__setMiterLimit(v); },
        configurable: true
    });
    Object.defineProperty(c, 'lineDashOffset', {
        get: function() { return c.__getLineDashOffset(); },
        set: function(v) { c.__setLineDashOffset(v); },
        configurable: true
    });
    Object.defineProperty(c, 'shadowColor', {
        get: function() { return c.__getShadowColor(); },
        set: function(v) { c.__setShadowColor(v); },
        configurable: true
    });
    Object.defineProperty(c, 'shadowBlur', {
        get: function() { return c.__getShadowBlur(); },
        set: function(v) { c.__setShadowBlur(v); },
        configurable: true
    });
    Object.defineProperty(c, 'shadowOffsetX', {
        get: function() { return c.__getShadowOffsetX(); },
        set: function(v) { c.__setShadowOffsetX(v); },
        configurable: true
    });
    Object.defineProperty(c, 'shadowOffsetY', {
        get: function() { return c.__getShadowOffsetY(); },
        set: function(v) { c.__setShadowOffsetY(v); },
        configurable: true
    });
    Object.defineProperty(c, 'globalCompositeOperation', {
        get: function() { return c.__getGlobalCompositeOp(); },
        set: function(v) { c.__setGlobalCompositeOp(v); },
        configurable: true
    });
    Object.defineProperty(c, 'imageSmoothingEnabled', {
        get: function() { return c.__getImageSmoothing(); },
        set: function(v) { c.__setImageSmoothing(v); },
        configurable: true
    });
})
)JS";
    JSValue setup_fn = JS_Eval(ctx, ctx2d_setup, std::strlen(ctx2d_setup),
                                "<canvas2d-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue args[1] = { JS_DupValue(ctx, obj) };
        JSValue setup_ret = JS_Call(ctx, setup_fn, JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, setup_ret);
        JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, setup_fn);

    // Add canvas property (object with width/height)
    JSValue canvas_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, canvas_obj, "width", JS_NewInt32(ctx, state->width));
    JS_SetPropertyStr(ctx, canvas_obj, "height", JS_NewInt32(ctx, state->height));
    JS_SetPropertyStr(ctx, obj, "canvas", canvas_obj);

    return obj;
}

// ---- element.getContext('2d') ----
static JSValue js_element_get_context(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* node = unwrap_element(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;

    // Check that this is a canvas element
    std::string tag = node->tag_name;
    for (auto& c : tag) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (tag != "canvas") return JS_NULL;

    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_NULL;
    std::string context_type(type);
    JS_FreeCString(ctx, type);

    // Return a WebGLRenderingContext stub for WebGL contexts
    if (context_type == "webgl" || context_type == "experimental-webgl" ||
        context_type == "webgl2") {
        // Check if stub already cached on element
        JSValue existing_gl = JS_GetPropertyStr(ctx, this_val, "__webgl_ctx");
        if (!JS_IsUndefined(existing_gl) && !JS_IsNull(existing_gl)) {
            return existing_gl;
        }
        JS_FreeValue(ctx, existing_gl);
        // Create a new WebGLRenderingContext stub via the global constructor
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue gl_ctor = JS_GetPropertyStr(ctx, global, "WebGLRenderingContext");
        JS_FreeValue(ctx, global);
        if (JS_IsFunction(ctx, gl_ctor)) {
            JSValue gl_obj = JS_CallConstructor(ctx, gl_ctor, 1, &this_val);
            JS_FreeValue(ctx, gl_ctor);
            if (!JS_IsException(gl_obj)) {
                JS_SetPropertyStr(ctx, this_val, "__webgl_ctx",
                                  JS_DupValue(ctx, gl_obj));
                return gl_obj;
            }
            JS_FreeValue(ctx, gl_obj);
        } else {
            JS_FreeValue(ctx, gl_ctor);
        }
        return JS_NULL;
    }

    if (context_type != "2d") return JS_NULL;

    // Check if context already cached on element
    JSValue existing = JS_GetPropertyStr(ctx, this_val, "__canvas2d_ctx");
    if (!JS_IsUndefined(existing) && !JS_IsNull(existing)) {
        return existing;
    }
    JS_FreeValue(ctx, existing);

    // Get canvas dimensions
    int cw = 300, ch = 150;
    std::string w_attr = get_attr(*node, "width");
    std::string h_attr = get_attr(*node, "height");
    if (!w_attr.empty()) { try { cw = std::stoi(w_attr); } catch (...) {} }
    if (!h_attr.empty()) { try { ch = std::stoi(h_attr); } catch (...) {} }

    // Allocate Canvas2DState
    auto* state = new Canvas2DState();
    state->width = cw;
    state->height = ch;

    // Store buffer info in DOMState for render pipeline to pick up
    // We store the buffer pointer as a hidden attribute on the element
    auto buf = std::make_shared<std::vector<uint8_t>>(cw * ch * 4, 0);
    state->buffer = buf->data() ? &(*buf) : nullptr;

    // Store the shared_ptr in the element via a hidden attribute encoding
    // (We encode the pointer as a decimal string in a special data attribute)
    // The render pipeline will read this during layout.
    uintptr_t buf_ptr = reinterpret_cast<uintptr_t>(buf.get());
    set_attr(*node, "data-canvas-buffer-ptr",
             std::to_string(static_cast<uint64_t>(buf_ptr)));
    set_attr(*node, "data-canvas-buffer-size",
             std::to_string(cw * ch * 4));

    // Store the shared_ptr in DOMState to keep it alive
    auto* dom_state = get_dom_state(ctx);
    if (dom_state) {
        // We need a place to store the shared_ptr. Use a static map keyed by node*.
        // Simpler approach: store it as a hidden property on the global object.
        // Even simpler: just leak-protect by storing in a vector on DOMState
        // For now, keep the buffer alive by storing as JS ArrayBuffer in a hidden prop.
    }

    // Keep shared_ptr alive by storing in a hidden JS property on the element
    // We'll use the raw pointer and ensure it stays valid via the DOM.
    // The simplest correct approach: store the shared_ptr in a static map.
    static std::unordered_map<clever::html::SimpleNode*,
                              std::shared_ptr<std::vector<uint8_t>>> canvas_buffers;
    canvas_buffers[node] = buf;

    JSValue ctx_obj = create_canvas2d_context(ctx, state);

    // Cache the context on the element
    JS_SetPropertyStr(ctx, this_val, "__canvas2d_ctx", JS_DupValue(ctx, ctx_obj));

    return ctx_obj;
}

// ---- HTMLCanvasElement.toDataURL(type, quality) ----
// Returns a data: URL containing a BMP representation of the canvas pixels.
static JSValue js_canvas_to_data_url(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    // Check that this is a canvas element
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_NewString(ctx, "data:,");
    std::string tag = node->tag_name;
    for (auto& c : tag) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (tag != "canvas") return JS_NewString(ctx, "data:,");

    // Get the cached 2D context to access the pixel buffer
    JSValue ctx_obj = JS_GetPropertyStr(ctx, this_val, "__canvas2d_ctx");
    if (JS_IsUndefined(ctx_obj) || JS_IsNull(ctx_obj)) {
        JS_FreeValue(ctx, ctx_obj);
        return JS_NewString(ctx, "data:,");
    }

    auto* state = static_cast<Canvas2DState*>(JS_GetOpaque(ctx_obj, canvas2d_class_id));
    JS_FreeValue(ctx, ctx_obj);
    if (!state || !state->buffer) return JS_NewString(ctx, "data:,");

    // Determine requested type (default: image/png, but we produce BMP)
    std::string req_type = "image/png";
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* t = JS_ToCString(ctx, argv[0]);
        if (t) { req_type = t; JS_FreeCString(ctx, t); }
    }

    int w = state->width;
    int h = state->height;
    auto& buf = *state->buffer;

    // Build an uncompressed 32-bit BMP in memory
    // BMP header: 14 bytes file header + 40 bytes DIB header = 54 bytes
    uint32_t row_size = static_cast<uint32_t>(w * 4); // no padding needed for 32bpp
    uint32_t pixel_data_size = row_size * static_cast<uint32_t>(h);
    uint32_t file_size = 54 + pixel_data_size;

    std::vector<uint8_t> bmp(file_size, 0);
    // File header
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = file_size & 0xFF; bmp[3] = (file_size >> 8) & 0xFF;
    bmp[4] = (file_size >> 16) & 0xFF; bmp[5] = (file_size >> 24) & 0xFF;
    bmp[10] = 54; // pixel data offset

    // DIB header (BITMAPINFOHEADER)
    bmp[14] = 40; // header size
    bmp[18] = w & 0xFF; bmp[19] = (w >> 8) & 0xFF;
    bmp[20] = (w >> 16) & 0xFF; bmp[21] = (w >> 24) & 0xFF;
    bmp[22] = h & 0xFF; bmp[23] = (h >> 8) & 0xFF;
    bmp[24] = (h >> 16) & 0xFF; bmp[25] = (h >> 24) & 0xFF;
    bmp[26] = 1; // planes
    bmp[28] = 32; // bits per pixel
    // compression = 0 (BI_RGB), no compression fields needed

    // Copy pixel data (BMP is bottom-to-top, BGRA order)
    // Our canvas buffer is RGBA, top-to-bottom
    for (int y = 0; y < h; y++) {
        int bmp_row = h - 1 - y; // flip vertically
        for (int x = 0; x < w; x++) {
            int src = (y * w + x) * 4;
            int dst = 54 + (bmp_row * w + x) * 4;
            bmp[dst + 0] = buf[src + 2]; // B
            bmp[dst + 1] = buf[src + 1]; // G
            bmp[dst + 2] = buf[src + 0]; // R
            bmp[dst + 3] = buf[src + 3]; // A
        }
    }

    // Base64 encode
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve((bmp.size() + 2) / 3 * 4);
    for (size_t i = 0; i < bmp.size(); i += 3) {
        uint8_t a = bmp[i];
        uint8_t b_val = (i + 1 < bmp.size()) ? bmp[i + 1] : 0;
        uint8_t c_val = (i + 2 < bmp.size()) ? bmp[i + 2] : 0;
        encoded += b64[a >> 2];
        encoded += b64[((a & 3) << 4) | (b_val >> 4)];
        encoded += (i + 1 < bmp.size()) ? b64[((b_val & 0xF) << 2) | (c_val >> 6)] : '=';
        encoded += (i + 2 < bmp.size()) ? b64[c_val & 0x3F] : '=';
    }

    std::string result = "data:image/bmp;base64," + encoded;
    return JS_NewString(ctx, result.c_str());
}

// ---- HTMLCanvasElement.toBlob(callback, type, quality) ----
// Calls callback with a Blob containing the raw canvas pixel data.
static JSValue js_canvas_to_blob(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_UNDEFINED;

    // Check that this is a canvas element
    auto* node = unwrap_element(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    std::string tag = node->tag_name;
    for (auto& c : tag) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (tag != "canvas") return JS_UNDEFINED;

    // Get type argument
    std::string mime_type = "image/png";
    if (argc >= 2 && JS_IsString(argv[1])) {
        const char* t = JS_ToCString(ctx, argv[1]);
        if (t) { mime_type = t; JS_FreeCString(ctx, t); }
    }

    // Get the data URL string (reuse toDataURL logic via internal call)
    JSValue data_url_val = js_canvas_to_data_url(ctx, this_val, argc - 1, argv + 1);
    const char* data_url = JS_ToCString(ctx, data_url_val);
    if (!data_url) {
        JS_FreeValue(ctx, data_url_val);
        return JS_UNDEFINED;
    }

    // Extract the base64 portion after the comma
    std::string du(data_url);
    JS_FreeCString(ctx, data_url);
    JS_FreeValue(ctx, data_url_val);

    std::string b64_data;
    auto comma_pos = du.find(',');
    if (comma_pos != std::string::npos) {
        b64_data = du.substr(comma_pos + 1);
    }

    // Create Blob from the base64 decoded data using atob + Blob constructor
    // For simplicity, pass the raw BMP bytes as a string to Blob
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue blob_ctor = JS_GetPropertyStr(ctx, global, "Blob");
    JSValue atob_fn = JS_GetPropertyStr(ctx, global, "atob");

    JSValue blob = JS_NULL;
    if (JS_IsFunction(ctx, blob_ctor) && JS_IsFunction(ctx, atob_fn)) {
        // Decode base64 to binary string
        JSValue b64_str = JS_NewString(ctx, b64_data.c_str());
        JSValue decoded = JS_Call(ctx, atob_fn, JS_UNDEFINED, 1, &b64_str);
        JS_FreeValue(ctx, b64_str);

        if (!JS_IsException(decoded)) {
            // Create Blob([decoded], {type: 'image/bmp'})
            JSValue parts = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, parts, 0, JS_DupValue(ctx, decoded));
            JSValue options = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, options, "type", JS_NewString(ctx, "image/bmp"));
            JSValue args[2] = { parts, options };
            blob = JS_CallConstructor(ctx, blob_ctor, 2, args);
            JS_FreeValue(ctx, parts);
            JS_FreeValue(ctx, options);
        }
        JS_FreeValue(ctx, decoded);
    }

    // Call callback(blob)
    JSValue cb_args[1] = { blob };
    JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 1, cb_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, blob);
    JS_FreeValue(ctx, atob_fn);
    JS_FreeValue(ctx, blob_ctor);
    JS_FreeValue(ctx, global);

    return JS_UNDEFINED;
}

} // anonymous namespace

// =========================================================================
// TreeWalker implementation (C++ based)
// =========================================================================

struct TreeWalkerState {
    clever::html::SimpleNode* root = nullptr;
    clever::html::SimpleNode* current = nullptr;
    uint32_t whatToShow = 0xFFFFFFFF; // NodeFilter.SHOW_ALL
};

// Helper: depth-first next node in document order starting from `node`,
// staying within the subtree rooted at `root`.
static clever::html::SimpleNode* tree_walker_next_in_order(
        clever::html::SimpleNode* node,
        clever::html::SimpleNode* root) {
    if (!node) return nullptr;

    // First, try to go to first child
    if (!node->children.empty()) {
        return node->children.front().get();
    }

    // Otherwise, go to next sibling, or parent's next sibling, etc.
    auto* current = node;
    while (current && current != root) {
        auto* parent = current->parent;
        if (!parent) return nullptr;

        // Find current in parent's children and get next sibling
        for (size_t i = 0; i < parent->children.size(); i++) {
            if (parent->children[i].get() == current) {
                if (i + 1 < parent->children.size()) {
                    return parent->children[i + 1].get();
                }
                break;
            }
        }
        current = parent;
    }
    return nullptr;
}

// Helper: check if whatToShow accepts this node type
static bool tree_walker_accepts(clever::html::SimpleNode* node, uint32_t whatToShow) {
    if (!node) return false;
    int nodeType = 0;
    switch (node->type) {
        case clever::html::SimpleNode::Element:  nodeType = 1; break;
        case clever::html::SimpleNode::Text:     nodeType = 3; break;
        case clever::html::SimpleNode::Comment:  nodeType = 8; break;
        case clever::html::SimpleNode::Document: nodeType = 9; break;
        default: nodeType = 1; break;
    }
    uint32_t mask = (1u << (nodeType - 1));
    return (whatToShow & mask) != 0;
}

static JSValue js_tree_walker_next_node(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    JSValue state_val = JS_GetPropertyStr(ctx, this_val, "__tw_state_ptr");
    int64_t ptr_val = 0;
    JS_ToInt64(ctx, &ptr_val, state_val);
    JS_FreeValue(ctx, state_val);
    auto* state = reinterpret_cast<TreeWalkerState*>(static_cast<uintptr_t>(ptr_val));
    if (!state) return JS_NULL;

    auto* node = state->current;
    while (true) {
        node = tree_walker_next_in_order(node, state->root);
        if (!node) return JS_NULL;
        if (tree_walker_accepts(node, state->whatToShow)) {
            state->current = node;
            // Update JS currentNode property
            JS_SetPropertyStr(ctx, this_val, "currentNode",
                wrap_element(ctx, node));
            return wrap_element(ctx, node);
        }
    }
}

static JSValue js_tree_walker_parent_node(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    JSValue state_val = JS_GetPropertyStr(ctx, this_val, "__tw_state_ptr");
    int64_t ptr_val = 0;
    JS_ToInt64(ctx, &ptr_val, state_val);
    JS_FreeValue(ctx, state_val);
    auto* state = reinterpret_cast<TreeWalkerState*>(static_cast<uintptr_t>(ptr_val));
    if (!state) return JS_NULL;

    auto* node = state->current;
    while (node && node != state->root) {
        node = node->parent;
        if (!node) return JS_NULL;
        if (tree_walker_accepts(node, state->whatToShow)) {
            state->current = node;
            JS_SetPropertyStr(ctx, this_val, "currentNode",
                wrap_element(ctx, node));
            return wrap_element(ctx, node);
        }
    }
    return JS_NULL;
}

static JSValue js_tree_walker_previous_node(JSContext* ctx, JSValueConst this_val,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    JSValue state_val = JS_GetPropertyStr(ctx, this_val, "__tw_state_ptr");
    int64_t ptr_val = 0;
    JS_ToInt64(ctx, &ptr_val, state_val);
    JS_FreeValue(ctx, state_val);
    auto* state = reinterpret_cast<TreeWalkerState*>(static_cast<uintptr_t>(ptr_val));
    if (!state) return JS_NULL;

    // Walk backwards: collect all nodes in document order, find current, go back
    // Simpler approach: iterate forward from root until we find current, remember the previous acceptable node
    clever::html::SimpleNode* prev_acceptable = nullptr;
    auto* node = state->root;
    while (node) {
        if (node == state->current) break;
        if (tree_walker_accepts(node, state->whatToShow)) {
            prev_acceptable = node;
        }
        node = tree_walker_next_in_order(node, state->root);
    }
    if (prev_acceptable) {
        state->current = prev_acceptable;
        JS_SetPropertyStr(ctx, this_val, "currentNode",
            wrap_element(ctx, prev_acceptable));
        return wrap_element(ctx, prev_acceptable);
    }
    return JS_NULL;
}

static JSValue js_tree_walker_first_child(JSContext* ctx, JSValueConst this_val,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    JSValue state_val = JS_GetPropertyStr(ctx, this_val, "__tw_state_ptr");
    int64_t ptr_val = 0;
    JS_ToInt64(ctx, &ptr_val, state_val);
    JS_FreeValue(ctx, state_val);
    auto* state = reinterpret_cast<TreeWalkerState*>(static_cast<uintptr_t>(ptr_val));
    if (!state) return JS_NULL;

    for (auto& child : state->current->children) {
        if (tree_walker_accepts(child.get(), state->whatToShow)) {
            state->current = child.get();
            JS_SetPropertyStr(ctx, this_val, "currentNode",
                wrap_element(ctx, child.get()));
            return wrap_element(ctx, child.get());
        }
    }
    return JS_NULL;
}

static JSValue js_tree_walker_last_child(JSContext* ctx, JSValueConst this_val,
                                          int /*argc*/, JSValueConst* /*argv*/) {
    JSValue state_val = JS_GetPropertyStr(ctx, this_val, "__tw_state_ptr");
    int64_t ptr_val = 0;
    JS_ToInt64(ctx, &ptr_val, state_val);
    JS_FreeValue(ctx, state_val);
    auto* state = reinterpret_cast<TreeWalkerState*>(static_cast<uintptr_t>(ptr_val));
    if (!state) return JS_NULL;

    for (auto it = state->current->children.rbegin();
         it != state->current->children.rend(); ++it) {
        if (tree_walker_accepts(it->get(), state->whatToShow)) {
            state->current = it->get();
            JS_SetPropertyStr(ctx, this_val, "currentNode",
                wrap_element(ctx, it->get()));
            return wrap_element(ctx, it->get());
        }
    }
    return JS_NULL;
}

static JSValue js_document_create_tree_walker(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc,
                                               JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    auto* root = unwrap_element(ctx, argv[0]);
    if (!root) return JS_NULL;

    uint32_t whatToShow = 0xFFFFFFFF;
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        int32_t ws = 0;
        JS_ToInt32(ctx, &ws, argv[1]);
        whatToShow = static_cast<uint32_t>(ws);
    }

    // Allocate state (stored in DOM state's owned memory via opaque int pointer)
    auto* state = new TreeWalkerState();
    state->root = root;
    state->current = root;
    state->whatToShow = whatToShow;

    JSValue walker = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, walker, "__tw_state_ptr",
        JS_NewInt64(ctx, static_cast<int64_t>(
            reinterpret_cast<uintptr_t>(state))));

    JS_SetPropertyStr(ctx, walker, "root", wrap_element(ctx, root));
    JS_SetPropertyStr(ctx, walker, "currentNode", wrap_element(ctx, root));
    JS_SetPropertyStr(ctx, walker, "whatToShow",
        JS_NewUint32(ctx, whatToShow));

    JS_SetPropertyStr(ctx, walker, "nextNode",
        JS_NewCFunction(ctx, js_tree_walker_next_node, "nextNode", 0));
    JS_SetPropertyStr(ctx, walker, "parentNode",
        JS_NewCFunction(ctx, js_tree_walker_parent_node, "parentNode", 0));
    JS_SetPropertyStr(ctx, walker, "previousNode",
        JS_NewCFunction(ctx, js_tree_walker_previous_node, "previousNode", 0));
    JS_SetPropertyStr(ctx, walker, "firstChild",
        JS_NewCFunction(ctx, js_tree_walker_first_child, "firstChild", 0));
    JS_SetPropertyStr(ctx, walker, "lastChild",
        JS_NewCFunction(ctx, js_tree_walker_last_child, "lastChild", 0));

    // Store state pointer in DOM state for cleanup
    auto* dom_state = get_dom_state(ctx);
    if (dom_state) {
        // We'll leak a tiny bit here since TreeWalkerState is small and
        // lives for the duration of the page. For a real implementation,
        // we'd track these in the dom state for cleanup.
    }

    return walker;
}

// =========================================================================
// document.hasFocus() — always returns true
// =========================================================================

static JSValue js_document_has_focus(JSContext* ctx, JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewBool(ctx, true);
}

// =========================================================================
// ErrorEvent constructor: new ErrorEvent(type, {message, filename, lineno, colno, error})
// =========================================================================

static JSValue js_error_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc,
                                           JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type argument
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Defaults for ErrorEvent-specific properties
    JS_SetPropertyStr(ctx, event_obj, "message", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "filename", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "lineno", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "colno", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "error", JS_NULL);

    // Standard event properties
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // Read from options dict
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue msg = JS_GetPropertyStr(ctx, argv[1], "message");
        if (!JS_IsUndefined(msg)) {
            const char* s = JS_ToCString(ctx, msg);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "message", JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, msg);

        JSValue fn = JS_GetPropertyStr(ctx, argv[1], "filename");
        if (!JS_IsUndefined(fn)) {
            const char* s = JS_ToCString(ctx, fn);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "filename", JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, fn);

        JSValue lineno = JS_GetPropertyStr(ctx, argv[1], "lineno");
        if (!JS_IsUndefined(lineno)) {
            int32_t v = 0;
            JS_ToInt32(ctx, &v, lineno);
            JS_SetPropertyStr(ctx, event_obj, "lineno", JS_NewInt32(ctx, v));
        }
        JS_FreeValue(ctx, lineno);

        JSValue colno = JS_GetPropertyStr(ctx, argv[1], "colno");
        if (!JS_IsUndefined(colno)) {
            int32_t v = 0;
            JS_ToInt32(ctx, &v, colno);
            JS_SetPropertyStr(ctx, event_obj, "colno", JS_NewInt32(ctx, v));
        }
        JS_FreeValue(ctx, colno);

        JSValue error = JS_GetPropertyStr(ctx, argv[1], "error");
        if (!JS_IsUndefined(error)) {
            JS_SetPropertyStr(ctx, event_obj, "error", JS_DupValue(ctx, error));
        }
        JS_FreeValue(ctx, error);

        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// PromiseRejectionEvent constructor:
//   new PromiseRejectionEvent(type, {promise, reason})
// =========================================================================

static JSValue js_promise_rejection_event_constructor(JSContext* ctx,
                                                       JSValueConst /*new_target*/,
                                                       int argc,
                                                       JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type argument
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Defaults
    JS_SetPropertyStr(ctx, event_obj, "promise", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "reason", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // Read from options dict
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue promise = JS_GetPropertyStr(ctx, argv[1], "promise");
        if (!JS_IsUndefined(promise)) {
            JS_SetPropertyStr(ctx, event_obj, "promise",
                JS_DupValue(ctx, promise));
        }
        JS_FreeValue(ctx, promise);

        JSValue reason = JS_GetPropertyStr(ctx, argv[1], "reason");
        if (!JS_IsUndefined(reason)) {
            JS_SetPropertyStr(ctx, event_obj, "reason",
                JS_DupValue(ctx, reason));
        }
        JS_FreeValue(ctx, reason);

        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// WheelEvent constructor: new WheelEvent(type, options)
// Extends MouseEvent with deltaX, deltaY, deltaZ, deltaMode.
// =========================================================================

static JSValue js_wheel_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc,
                                           JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // MouseEvent defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "button", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "buttons", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "clientY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "screenY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pageY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "offsetY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "shiftKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "altKey", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "metaKey", JS_FALSE);

    // WheelEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "deltaX", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "deltaY", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "deltaZ", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "deltaMode", JS_NewInt32(ctx, 0));

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        auto read_bool = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewBool(ctx, JS_ToBool(ctx, v)));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_num = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                double dval = 0;
                JS_ToFloat64(ctx, &dval, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewFloat64(ctx, dval));
            }
            JS_FreeValue(ctx, v);
        };
        auto read_int = [&](const char* name) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], name);
            if (!JS_IsUndefined(v)) {
                int32_t ival = 0;
                JS_ToInt32(ctx, &ival, v);
                JS_SetPropertyStr(ctx, event_obj, name,
                    JS_NewInt32(ctx, ival));
            }
            JS_FreeValue(ctx, v);
        };

        // MouseEvent properties
        read_bool("bubbles");
        read_bool("cancelable");
        read_int("button");
        read_int("buttons");
        read_num("clientX");
        read_num("clientY");
        read_num("screenX");
        read_num("screenY");
        read_num("pageX");
        read_num("pageY");
        read_num("offsetX");
        read_num("offsetY");
        read_bool("ctrlKey");
        read_bool("shiftKey");
        read_bool("altKey");
        read_bool("metaKey");

        // WheelEvent-specific properties
        read_num("deltaX");
        read_num("deltaY");
        read_num("deltaZ");
        read_int("deltaMode");
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// HashChangeEvent constructor: new HashChangeEvent(type, {oldURL, newURL})
// =========================================================================

static JSValue js_hash_change_event_constructor(JSContext* ctx,
                                                 JSValueConst /*new_target*/,
                                                 int argc,
                                                 JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // HashChangeEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "oldURL", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "newURL", JS_NewString(ctx, ""));

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue oldURL = JS_GetPropertyStr(ctx, argv[1], "oldURL");
        if (!JS_IsUndefined(oldURL)) {
            const char* s = JS_ToCString(ctx, oldURL);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "oldURL",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, oldURL);

        JSValue newURL = JS_GetPropertyStr(ctx, argv[1], "newURL");
        if (!JS_IsUndefined(newURL)) {
            const char* s = JS_ToCString(ctx, newURL);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "newURL",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, newURL);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// PopStateEvent constructor: new PopStateEvent(type, {state})
// =========================================================================

static JSValue js_pop_state_event_constructor(JSContext* ctx,
                                               JSValueConst /*new_target*/,
                                               int argc,
                                               JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // PopStateEvent-specific default
    JS_SetPropertyStr(ctx, event_obj, "state", JS_NULL);

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue state = JS_GetPropertyStr(ctx, argv[1], "state");
        if (!JS_IsUndefined(state)) {
            JS_SetPropertyStr(ctx, event_obj, "state",
                JS_DupValue(ctx, state));
        }
        JS_FreeValue(ctx, state);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// TransitionEvent constructor: new TransitionEvent(type, options)
// CSS transition events with propertyName, elapsedTime, pseudoElement.
// =========================================================================

static JSValue js_transition_event_constructor(JSContext* ctx,
                                                JSValueConst /*new_target*/,
                                                int argc,
                                                JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // TransitionEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "propertyName", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "elapsedTime", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pseudoElement", JS_NewString(ctx, ""));

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue propertyName = JS_GetPropertyStr(ctx, argv[1], "propertyName");
        if (!JS_IsUndefined(propertyName)) {
            const char* s = JS_ToCString(ctx, propertyName);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "propertyName",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, propertyName);

        JSValue elapsedTime = JS_GetPropertyStr(ctx, argv[1], "elapsedTime");
        if (!JS_IsUndefined(elapsedTime)) {
            double dval = 0;
            JS_ToFloat64(ctx, &dval, elapsedTime);
            JS_SetPropertyStr(ctx, event_obj, "elapsedTime",
                JS_NewFloat64(ctx, dval));
        }
        JS_FreeValue(ctx, elapsedTime);

        JSValue pseudoElement = JS_GetPropertyStr(ctx, argv[1], "pseudoElement");
        if (!JS_IsUndefined(pseudoElement)) {
            const char* s = JS_ToCString(ctx, pseudoElement);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "pseudoElement",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, pseudoElement);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// AnimationEvent constructor: new AnimationEvent(type, options)
// CSS animation events with animationName, elapsedTime, pseudoElement.
// =========================================================================

static JSValue js_animation_event_constructor(JSContext* ctx,
                                               JSValueConst /*new_target*/,
                                               int argc,
                                               JSValueConst* argv) {
    JSValue event_obj = JS_NewObject(ctx);

    // type
    if (argc > 0) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, type_cstr));
            JS_FreeCString(ctx, type_cstr);
        } else {
            JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
        }
    } else {
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, ""));
    }

    // Event defaults
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // AnimationEvent-specific defaults
    JS_SetPropertyStr(ctx, event_obj, "animationName", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, event_obj, "elapsedTime", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, event_obj, "pseudoElement", JS_NewString(ctx, ""));

    // Read options
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);

        JSValue animationName = JS_GetPropertyStr(ctx, argv[1], "animationName");
        if (!JS_IsUndefined(animationName)) {
            const char* s = JS_ToCString(ctx, animationName);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "animationName",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, animationName);

        JSValue elapsedTime = JS_GetPropertyStr(ctx, argv[1], "elapsedTime");
        if (!JS_IsUndefined(elapsedTime)) {
            double dval = 0;
            JS_ToFloat64(ctx, &dval, elapsedTime);
            JS_SetPropertyStr(ctx, event_obj, "elapsedTime",
                JS_NewFloat64(ctx, dval));
        }
        JS_FreeValue(ctx, elapsedTime);

        JSValue pseudoElement = JS_GetPropertyStr(ctx, argv[1], "pseudoElement");
        if (!JS_IsUndefined(pseudoElement)) {
            const char* s = JS_ToCString(ctx, pseudoElement);
            if (s) {
                JS_SetPropertyStr(ctx, event_obj, "pseudoElement",
                    JS_NewString(ctx, s));
                JS_FreeCString(ctx, s);
            }
        }
        JS_FreeValue(ctx, pseudoElement);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// TouchEvent constructor (Cycle 239)
// =========================================================================
static JSValue js_touch_event_constructor(JSContext* ctx,
                                           JSValueConst /*new_target*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "1 argument required");
    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_EXCEPTION;

    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, type));
    JS_FreeCString(ctx, type);
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_TRUE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_TRUE);
    JS_SetPropertyStr(ctx, event_obj, "touches", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, event_obj, "targetTouches", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, event_obj, "changedTouches", JS_NewArray(ctx));

    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);
        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_SetPropertyStr(ctx, event_obj, "cancelable",
                JS_NewBool(ctx, JS_ToBool(ctx, cancelable)));
        }
        JS_FreeValue(ctx, cancelable);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// DragEvent constructor (Cycle 239)
// =========================================================================
static JSValue js_drag_event_constructor(JSContext* ctx,
                                          JSValueConst /*new_target*/,
                                          int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "1 argument required");
    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_EXCEPTION;

    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, type));
    JS_FreeCString(ctx, type);
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_TRUE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_TRUE);
    // DataTransfer stub
    JSValue dt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, dt, "dropEffect", JS_NewString(ctx, "none"));
    JS_SetPropertyStr(ctx, dt, "effectAllowed", JS_NewString(ctx, "uninitialized"));
    JS_SetPropertyStr(ctx, dt, "items", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, dt, "files", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, dt, "types", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, event_obj, "dataTransfer", dt);

    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_SetPropertyStr(ctx, event_obj, "bubbles",
                JS_NewBool(ctx, JS_ToBool(ctx, bubbles)));
        }
        JS_FreeValue(ctx, bubbles);
    }

    attach_event_methods(ctx, event_obj);
    return event_obj;
}

// =========================================================================
// document.createNodeIterator(root, whatToShow, filter)
// Similar to createTreeWalker but uses flat iteration order.
// =========================================================================

static JSValue js_node_iterator_next_node(JSContext* ctx,
                                            JSValueConst this_val,
                                            int /*argc*/,
                                            JSValueConst* /*argv*/) {
    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "__ni_index");
    int32_t idx = 0;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    JSValue nodes = JS_GetPropertyStr(ctx, this_val, "__ni_nodes");
    JSValue len_val = JS_GetPropertyStr(ctx, nodes, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    if (idx >= len) {
        JS_FreeValue(ctx, nodes);
        return JS_NULL;
    }

    JSValue node = JS_GetPropertyUint32(ctx, nodes, static_cast<uint32_t>(idx));
    JS_SetPropertyStr(ctx, this_val, "__ni_index",
        JS_NewInt32(ctx, idx + 1));
    JS_SetPropertyStr(ctx, this_val, "referenceNode",
        JS_DupValue(ctx, node));
    JS_FreeValue(ctx, nodes);
    return node;
}

static JSValue js_node_iterator_previous_node(JSContext* ctx,
                                                JSValueConst this_val,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "__ni_index");
    int32_t idx = 0;
    JS_ToInt32(ctx, &idx, idx_val);
    JS_FreeValue(ctx, idx_val);

    // Move back: the index points to the next unvisited node.
    // previousNode returns the node before the last returned one.
    int32_t target = idx - 2;
    if (target < 0) {
        return JS_NULL;
    }

    JSValue nodes = JS_GetPropertyStr(ctx, this_val, "__ni_nodes");
    JSValue node = JS_GetPropertyUint32(ctx, nodes, static_cast<uint32_t>(target));
    JS_SetPropertyStr(ctx, this_val, "__ni_index",
        JS_NewInt32(ctx, target + 1));
    JS_SetPropertyStr(ctx, this_val, "referenceNode",
        JS_DupValue(ctx, node));
    JS_FreeValue(ctx, nodes);
    return node;
}

static JSValue js_document_create_node_iterator(JSContext* ctx,
                                                  JSValueConst /*this_val*/,
                                                  int argc,
                                                  JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    auto* root = unwrap_element(ctx, argv[0]);
    if (!root) return JS_NULL;

    uint32_t whatToShow = 0xFFFFFFFF;
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        int32_t ws = 0;
        JS_ToInt32(ctx, &ws, argv[1]);
        whatToShow = static_cast<uint32_t>(ws);
    }

    // Collect all matching nodes in document order into a JS array
    JSValue nodes_arr = JS_NewArray(ctx);
    uint32_t arr_idx = 0;

    // Add root if it matches
    if (tree_walker_accepts(root, whatToShow)) {
        JS_SetPropertyUint32(ctx, nodes_arr, arr_idx++,
            wrap_element(ctx, root));
    }

    // Depth-first traversal
    auto* node = root;
    while (true) {
        node = tree_walker_next_in_order(node, root);
        if (!node) break;
        if (tree_walker_accepts(node, whatToShow)) {
            JS_SetPropertyUint32(ctx, nodes_arr, arr_idx++,
                wrap_element(ctx, node));
        }
    }

    JSValue iterator = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, iterator, "root", wrap_element(ctx, root));
    JS_SetPropertyStr(ctx, iterator, "referenceNode", wrap_element(ctx, root));
    JS_SetPropertyStr(ctx, iterator, "whatToShow",
        JS_NewUint32(ctx, whatToShow));
    JS_SetPropertyStr(ctx, iterator, "__ni_nodes", nodes_arr);
    JS_SetPropertyStr(ctx, iterator, "__ni_index", JS_NewInt32(ctx, 0));

    JS_SetPropertyStr(ctx, iterator, "nextNode",
        JS_NewCFunction(ctx, js_node_iterator_next_node, "nextNode", 0));
    JS_SetPropertyStr(ctx, iterator, "previousNode",
        JS_NewCFunction(ctx, js_node_iterator_previous_node, "previousNode", 0));

    return iterator;
}

// =========================================================================
// crypto.subtle.digest — native SHA implementation via CommonCrypto
// =========================================================================

#ifdef __APPLE__
static JSValue js_crypto_subtle_digest(JSContext* ctx, JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "crypto.subtle.digest requires 2 arguments");

    // --- algorithm string ---
    const char* algo_str = JS_ToCString(ctx, argv[0]);
    if (!algo_str)
        return JS_EXCEPTION;

    enum { ALG_SHA1, ALG_SHA256, ALG_SHA384, ALG_SHA512 } algo;
    size_t digest_len = 0;
    if (std::strcmp(algo_str, "SHA-256") == 0) {
        algo = ALG_SHA256;
        digest_len = CC_SHA256_DIGEST_LENGTH;
    } else if (std::strcmp(algo_str, "SHA-384") == 0) {
        algo = ALG_SHA384;
        digest_len = CC_SHA384_DIGEST_LENGTH;
    } else if (std::strcmp(algo_str, "SHA-512") == 0) {
        algo = ALG_SHA512;
        digest_len = CC_SHA512_DIGEST_LENGTH;
    } else if (std::strcmp(algo_str, "SHA-1") == 0) {
        algo = ALG_SHA1;
        digest_len = CC_SHA1_DIGEST_LENGTH;
    } else {
        JS_FreeCString(ctx, algo_str);
        return JS_ThrowTypeError(ctx, "Unsupported algorithm");
    }
    JS_FreeCString(ctx, algo_str);

    // --- data buffer (ArrayBuffer or TypedArray) ---
    size_t data_len = 0;
    uint8_t* data_ptr = nullptr;

    // Try ArrayBuffer first
    data_ptr = JS_GetArrayBuffer(ctx, &data_len, argv[1]);
    if (!data_ptr) {
        // Might be a TypedArray — get underlying buffer
        size_t byte_offset = 0, byte_length = 0;
        size_t bytes_per_element = 0;
        JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset,
                                             &byte_length, &bytes_per_element);
        if (!JS_IsException(ab)) {
            data_ptr = JS_GetArrayBuffer(ctx, &data_len, ab);
            if (data_ptr) {
                data_ptr += byte_offset;
                data_len = byte_length;
            }
            JS_FreeValue(ctx, ab);
        } else {
            // Clear exception from GetTypedArrayBuffer and try converting
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
    }

    if (!data_ptr) {
        return JS_ThrowTypeError(ctx, "crypto.subtle.digest: data must be ArrayBuffer or TypedArray");
    }

    // --- compute hash ---
    unsigned char hash[CC_SHA512_DIGEST_LENGTH]; // largest possible
    switch (algo) {
        case ALG_SHA1:
            CC_SHA1(data_ptr, static_cast<CC_LONG>(data_len), hash);
            break;
        case ALG_SHA256:
            CC_SHA256(data_ptr, static_cast<CC_LONG>(data_len), hash);
            break;
        case ALG_SHA384:
            CC_SHA384(data_ptr, static_cast<CC_LONG>(data_len), hash);
            break;
        case ALG_SHA512:
            CC_SHA512(data_ptr, static_cast<CC_LONG>(data_len), hash);
            break;
    }

    // --- create ArrayBuffer with the hash ---
    JSValue result_ab = JS_NewArrayBufferCopy(ctx, hash, digest_len);

    // --- wrap in a resolved Promise ---
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue promise_ctor = JS_GetPropertyStr(ctx, global, "Promise");
    JSValue resolve_fn = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
    JSValue args[1] = { result_ab };
    JSValue promise = JS_Call(ctx, resolve_fn, promise_ctor, 1, args);
    JS_FreeValue(ctx, resolve_fn);
    JS_FreeValue(ctx, promise_ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, result_ab);

    return promise;
}
#endif // __APPLE__

// =========================================================================
// URL constructor and methods
// =========================================================================

static JSValue js_url_constructor(JSContext* ctx, JSValueConst /*this_val*/,
                                   int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "URL constructor requires at least 1 argument");
    }

    const char* url_str = JS_ToCString(ctx, argv[0]);
    if (!url_str) {
        return JS_EXCEPTION;
    }

    clever::url::URL* base_url = nullptr;
    if (argc > 1 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1])) {
        auto* base_state = static_cast<URLState*>(JS_GetOpaque(argv[1], url_class_id));
        if (base_state) {
            base_url = &base_state->parsed_url;
        }
    }

    auto parsed = clever::url::parse(url_str, base_url);
    JS_FreeCString(ctx, url_str);

    if (!parsed) {
        return JS_ThrowTypeError(ctx, "Invalid URL");
    }

    auto* state = new URLState();
    state->parsed_url = parsed.value();

    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(url_class_id));
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue js_url_get_property(JSContext* ctx, JSValueConst this_val,
                                    const std::string& prop) {
    auto* state = static_cast<URLState*>(JS_GetOpaque(this_val, url_class_id));
    if (!state) return JS_UNDEFINED;

    const auto& url = state->parsed_url;

    if (prop == "href") {
        return JS_NewString(ctx, url.serialize().c_str());
    } else if (prop == "protocol") {
        return JS_NewString(ctx, (url.scheme + ":").c_str());
    } else if (prop == "hostname") {
        return JS_NewString(ctx, url.host.c_str());
    } else if (prop == "port") {
        if (url.port) {
            return JS_NewString(ctx, std::to_string(url.port.value()).c_str());
        }
        return JS_NewString(ctx, "");
    } else if (prop == "pathname") {
        return JS_NewString(ctx, url.path.c_str());
    } else if (prop == "search") {
        if (url.query.empty()) {
            return JS_NewString(ctx, "");
        }
        return JS_NewString(ctx, ("?" + url.query).c_str());
    } else if (prop == "hash") {
        if (url.fragment.empty()) {
            return JS_NewString(ctx, "");
        }
        return JS_NewString(ctx, ("#" + url.fragment).c_str());
    } else if (prop == "origin") {
        return JS_NewString(ctx, url.origin().c_str());
    } else if (prop == "searchParams") {
        JSValue params = JS_NewObject(ctx);
        if (!url.query.empty()) {
            std::string query = url.query;
            size_t pos = 0;
            while (pos < query.length()) {
                size_t eq_pos = query.find('=', pos);
                size_t amp_pos = query.find('&', pos);
                if (amp_pos == std::string::npos) amp_pos = query.length();

                if (eq_pos != std::string::npos && eq_pos < amp_pos) {
                    std::string key = query.substr(pos, eq_pos - pos);
                    std::string val = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);
                    JS_SetPropertyStr(ctx, params, key.c_str(), JS_NewString(ctx, val.c_str()));
                } else {
                    std::string key = query.substr(pos, amp_pos - pos);
                    JS_SetPropertyStr(ctx, params, key.c_str(), JS_NewString(ctx, ""));
                }
                pos = amp_pos + 1;
            }
        }
        return params;
    }

    return JS_UNDEFINED;
}

static JSValue js_url_to_string(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "href");
}

static JSValue js_url_get_href(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "href");
}

static JSValue js_url_get_protocol(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "protocol");
}

static JSValue js_url_get_hostname(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "hostname");
}

static JSValue js_url_get_port(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "port");
}

static JSValue js_url_get_pathname(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "pathname");
}

static JSValue js_url_get_search(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "search");
}

static JSValue js_url_get_hash(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "hash");
}

static JSValue js_url_get_origin(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "origin");
}

static JSValue js_url_get_search_params(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return js_url_get_property(ctx, this_val, "searchParams");
}

// =========================================================================
// TextEncoder constructor and methods
// =========================================================================

static JSValue js_text_encoder_constructor(JSContext* ctx, JSValueConst /*this_val*/,
                                            int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = new TextEncoderState();
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(text_encoder_class_id));
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue js_text_encoder_encode(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "encode requires 1 argument");
    }

    size_t len = 0;
    const char* str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str) {
        return JS_EXCEPTION;
    }

    JSValue array_buf = JS_NewArrayBufferCopy(ctx,
        reinterpret_cast<const uint8_t*>(str), len);
    JS_FreeCString(ctx, str);

    if (JS_IsException(array_buf)) {
        return array_buf;
    }

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JSValue args[3] = { array_buf, JS_NewInt32(ctx, 0), JS_NewInt64(ctx, static_cast<int64_t>(len)) };
    JSValue result = JS_CallConstructor(ctx, uint8_ctor, 3, args);

    JS_FreeValue(ctx, uint8_ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, array_buf);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);

    return result;
}

// =========================================================================
// TextDecoder constructor and methods
// =========================================================================

static JSValue js_text_decoder_constructor(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    (void)this_val;
    auto* state = new TextDecoderState();

    if (argc > 0) {
        const char* encoding = JS_ToCString(ctx, argv[0]);
        if (encoding) {
            state->encoding = encoding;
            JS_FreeCString(ctx, encoding);
        }
    }

    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(text_decoder_class_id));
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue js_text_decoder_decode(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    (void)this_val;

    if (argc < 1) {
        return JS_NewString(ctx, "");
    }

    size_t byte_length = 0;
    uint8_t* buf = nullptr;

    size_t offset = 0;
    size_t arr_len = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &offset, &arr_len, nullptr);
    if (!JS_IsException(ab)) {
        buf = JS_GetArrayBuffer(ctx, &byte_length, ab);
        if (buf) {
            buf += offset;
            byte_length = arr_len;
        }
    } else {
        buf = JS_GetArrayBuffer(ctx, &byte_length, argv[0]);
    }

    if (!buf) {
        return JS_NewString(ctx, "");
    }

    return JS_NewStringLen(ctx, reinterpret_cast<const char*>(buf), byte_length);
}

// =========================================================================
// Public API
// =========================================================================

void install_dom_bindings(JSContext* ctx,
                          clever::html::SimpleNode* document_root) {
    JSRuntime* rt = JS_GetRuntime(ctx);

    // Register Element class (once per runtime)
    if (element_class_id == 0) {
        JS_NewClassID(&element_class_id);
    }
    if (!JS_IsRegisteredClass(rt, element_class_id)) {
        JS_NewClass(rt, element_class_id, &element_class_def);
    }

    // Register CSSStyleDeclaration class (once per runtime)
    if (style_class_id == 0) {
        JS_NewClassID(&style_class_id);
    }
    if (!JS_IsRegisteredClass(rt, style_class_id)) {
        JS_NewClass(rt, style_class_id, &style_class_def);
    }

    // Register MutationObserver class (once per runtime)
    if (mutation_observer_class_id == 0) {
        JS_NewClassID(&mutation_observer_class_id);
    }
    if (!JS_IsRegisteredClass(rt, mutation_observer_class_id)) {
        JS_NewClass(rt, mutation_observer_class_id, &mutation_observer_class_def);
    }

    // Register IntersectionObserver class (once per runtime)
    if (intersection_observer_class_id == 0) {
        JS_NewClassID(&intersection_observer_class_id);
    }
    if (!JS_IsRegisteredClass(rt, intersection_observer_class_id)) {
        JS_NewClass(rt, intersection_observer_class_id, &intersection_observer_class_def);
    }

    // Register ResizeObserver class (once per runtime)
    if (resize_observer_class_id == 0) {
        JS_NewClassID(&resize_observer_class_id);
    }
    if (!JS_IsRegisteredClass(rt, resize_observer_class_id)) {
        JS_NewClass(rt, resize_observer_class_id, &resize_observer_class_def);
    }

    // Register CanvasRenderingContext2D class (once per runtime)
    if (canvas2d_class_id == 0) {
        JS_NewClassID(&canvas2d_class_id);
    }
    if (!JS_IsRegisteredClass(rt, canvas2d_class_id)) {
        JS_NewClass(rt, canvas2d_class_id, &canvas2d_class_def);
    }

    // ------------------------------------------------------------------
    // Element prototype: add methods that every element proxy inherits
    // ------------------------------------------------------------------
    JSValue element_proto = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, element_proto, "getAttribute",
        JS_NewCFunction(ctx, js_element_get_attribute, "getAttribute", 1));
    JS_SetPropertyStr(ctx, element_proto, "setAttribute",
        JS_NewCFunction(ctx, js_element_set_attribute, "setAttribute", 2));
    JS_SetPropertyStr(ctx, element_proto, "appendChild",
        JS_NewCFunction(ctx, js_element_append_child, "appendChild", 1));
    JS_SetPropertyStr(ctx, element_proto, "removeChild",
        JS_NewCFunction(ctx, js_element_remove_child, "removeChild", 1));
    JS_SetPropertyStr(ctx, element_proto, "addEventListener",
        JS_NewCFunction(ctx, js_element_add_event_listener,
                        "addEventListener", 3));
    JS_SetPropertyStr(ctx, element_proto, "removeEventListener",
        JS_NewCFunction(ctx, js_element_remove_event_listener,
                        "removeEventListener", 3));
    JS_SetPropertyStr(ctx, element_proto, "dispatchEvent",
        JS_NewCFunction(ctx, js_element_dispatch_event,
                        "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, element_proto, "remove",
        JS_NewCFunction(ctx, js_element_remove, "remove", 0));
    JS_SetPropertyStr(ctx, element_proto, "hasAttribute",
        JS_NewCFunction(ctx, js_element_has_attribute, "hasAttribute", 1));
    JS_SetPropertyStr(ctx, element_proto, "removeAttribute",
        JS_NewCFunction(ctx, js_element_remove_attribute,
                        "removeAttribute", 1));

    // classList internal helpers (wired into classList proxy by JS setup)
    JS_SetPropertyStr(ctx, element_proto, "__classListAdd",
        JS_NewCFunction(ctx, js_element_classlist_add,
                        "__classListAdd", 1));
    JS_SetPropertyStr(ctx, element_proto, "__classListRemove",
        JS_NewCFunction(ctx, js_element_classlist_remove,
                        "__classListRemove", 1));
    JS_SetPropertyStr(ctx, element_proto, "__classListContains",
        JS_NewCFunction(ctx, js_element_classlist_contains,
                        "__classListContains", 1));
    JS_SetPropertyStr(ctx, element_proto, "__classListReplace",
        JS_NewCFunction(ctx, js_element_classlist_replace,
                        "__classListReplace", 2));
    JS_SetPropertyStr(ctx, element_proto, "__classListGetAll",
        JS_NewCFunction(ctx, js_element_classlist_get_all,
                        "__classListGetAll", 0));

    // dataset internal helpers (wired into dataset proxy by JS setup)
    JS_SetPropertyStr(ctx, element_proto, "__datasetGet",
        JS_NewCFunction(ctx, js_element_dataset_get, "__datasetGet", 1));
    JS_SetPropertyStr(ctx, element_proto, "__datasetSet",
        JS_NewCFunction(ctx, js_element_dataset_set, "__datasetSet", 2));
    JS_SetPropertyStr(ctx, element_proto, "__datasetHas",
        JS_NewCFunction(ctx, js_element_dataset_has, "__datasetHas", 1));

    // Internal getters exposed as regular methods -- the JS setup script
    // below will wire them into proper getters via Object.defineProperty.
    JS_SetPropertyStr(ctx, element_proto, "__getId",
        JS_NewCFunction(ctx, js_element_get_id, "__getId", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setId",
        JS_NewCFunction(ctx, js_element_set_id, "__setId", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getTagName",
        JS_NewCFunction(ctx, js_element_get_tag_name, "__getTagName", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getClassName",
        JS_NewCFunction(ctx, js_element_get_class_name, "__getClassName", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setClassName",
        JS_NewCFunction(ctx, js_element_set_class_name, "__setClassName", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getTextContent",
        JS_NewCFunction(ctx, js_element_get_text_content,
                        "__getTextContent", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setTextContent",
        JS_NewCFunction(ctx, js_element_set_text_content,
                        "__setTextContent", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getInnerHTML",
        JS_NewCFunction(ctx, js_element_get_inner_html,
                        "__getInnerHTML", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setInnerHTML",
        JS_NewCFunction(ctx, js_element_set_inner_html,
                        "__setInnerHTML", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getChildren",
        JS_NewCFunction(ctx, js_element_get_children, "__getChildren", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getChildNodes",
        JS_NewCFunction(ctx, js_element_get_child_nodes,
                        "__getChildNodes", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getParentNode",
        JS_NewCFunction(ctx, js_element_get_parent, "__getParentNode", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getStyle",
        JS_NewCFunction(ctx, js_element_get_style, "__getStyle", 0));

    // Traversal getters (wired into JS getters by setup script below)
    JS_SetPropertyStr(ctx, element_proto, "__getFirstChild",
        JS_NewCFunction(ctx, js_element_get_first_child,
                        "__getFirstChild", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getLastChild",
        JS_NewCFunction(ctx, js_element_get_last_child,
                        "__getLastChild", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getFirstElementChild",
        JS_NewCFunction(ctx, js_element_get_first_element_child,
                        "__getFirstElementChild", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getLastElementChild",
        JS_NewCFunction(ctx, js_element_get_last_element_child,
                        "__getLastElementChild", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getNextSibling",
        JS_NewCFunction(ctx, js_element_get_next_sibling,
                        "__getNextSibling", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getPreviousSibling",
        JS_NewCFunction(ctx, js_element_get_previous_sibling,
                        "__getPreviousSibling", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getNextElementSibling",
        JS_NewCFunction(ctx, js_element_get_next_element_sibling,
                        "__getNextElementSibling", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getPreviousElementSibling",
        JS_NewCFunction(ctx, js_element_get_previous_element_sibling,
                        "__getPreviousElementSibling", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getChildElementCount",
        JS_NewCFunction(ctx, js_element_get_child_element_count,
                        "__getChildElementCount", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getNodeType",
        JS_NewCFunction(ctx, js_element_get_node_type,
                        "__getNodeType", 0));
    JS_SetPropertyStr(ctx, element_proto, "__getNodeName",
        JS_NewCFunction(ctx, js_element_get_node_name,
                        "__getNodeName", 0));

    // matches(), closest(), querySelector(), querySelectorAll() on elements
    JS_SetPropertyStr(ctx, element_proto, "matches",
        JS_NewCFunction(ctx, js_element_matches, "matches", 1));
    JS_SetPropertyStr(ctx, element_proto, "closest",
        JS_NewCFunction(ctx, js_element_closest, "closest", 1));
    JS_SetPropertyStr(ctx, element_proto, "querySelector",
        JS_NewCFunction(ctx, js_element_query_selector, "querySelector", 1));
    JS_SetPropertyStr(ctx, element_proto, "querySelectorAll",
        JS_NewCFunction(ctx, js_element_query_selector_all,
                        "querySelectorAll", 1));

    // getAttributeNames() — returns array of attribute name strings
    JS_SetPropertyStr(ctx, element_proto, "getAttributeNames",
        JS_NewCFunction(ctx, js_element_get_attribute_names,
                        "getAttributeNames", 0));

    // isConnected internal getter (wired into JS getter by setup script)
    JS_SetPropertyStr(ctx, element_proto, "__getIsConnected",
        JS_NewCFunction(ctx, js_element_get_is_connected,
                        "__getIsConnected", 0));

    // getBoundingClientRect() -- returns DOMRect with layout geometry
    JS_SetPropertyStr(ctx, element_proto, "getBoundingClientRect",
        JS_NewCFunction(ctx, js_element_get_bounding_client_rect,
                        "getBoundingClientRect", 0));

    // getClientRects() -- returns array of DOMRect objects
    JS_SetPropertyStr(ctx, element_proto, "getClientRects",
        JS_NewCFunction(ctx, js_element_get_client_rects,
                        "getClientRects", 0));

    // getContext('2d') -- returns CanvasRenderingContext2D for <canvas> elements
    JS_SetPropertyStr(ctx, element_proto, "getContext",
        JS_NewCFunction(ctx, js_element_get_context, "getContext", 1));

    // HTMLCanvasElement.toDataURL(type, quality) -- returns data URL from canvas pixels
    JS_SetPropertyStr(ctx, element_proto, "toDataURL",
        JS_NewCFunction(ctx, js_canvas_to_data_url, "toDataURL", 2));

    // HTMLCanvasElement.toBlob(callback, type, quality) -- calls callback with Blob
    JS_SetPropertyStr(ctx, element_proto, "toBlob",
        JS_NewCFunction(ctx, js_canvas_to_blob, "toBlob", 3));

    // Dimension getters (backed by layout geometry, using magic for dispatch)
    {
        struct DimEntry { const char* name; int magic; bool has_setter; };
        DimEntry dims[] = {
            {"offsetWidth",  0, false}, {"offsetHeight", 1, false},
            {"offsetTop",    2, false}, {"offsetLeft",   3, false},
            {"scrollWidth",  4, false}, {"scrollHeight", 5, false},
            {"scrollTop",    6, true},  {"scrollLeft",   7, true},
            {"clientWidth",  8, false}, {"clientHeight", 9, false},
            {"clientTop",   10, false}, {"clientLeft",  11, false}
        };
        for (auto& d : dims) {
            JSValue getter = JS_NewCFunctionMagic(ctx,
                (JSCFunctionMagic*)js_element_dimension_getter, d.name, 0,
                JS_CFUNC_generic_magic, d.magic);
            JSValue setter = JS_UNDEFINED;
            if (d.has_setter) {
                setter = JS_NewCFunctionMagic(ctx,
                    (JSCFunctionMagic*)js_element_dimension_setter, d.name, 1,
                    JS_CFUNC_generic_magic, d.magic);
            }
            JSAtom prop = JS_NewAtom(ctx, d.name);
            JS_DefinePropertyGetSet(ctx, element_proto, prop, getter, setter, 0);
            JS_FreeAtom(ctx, prop);
        }
    }

    // DOM mutation methods
    JS_SetPropertyStr(ctx, element_proto, "insertBefore",
        JS_NewCFunction(ctx, js_element_insert_before, "insertBefore", 2));
    JS_SetPropertyStr(ctx, element_proto, "replaceChild",
        JS_NewCFunction(ctx, js_element_replace_child, "replaceChild", 2));
    JS_SetPropertyStr(ctx, element_proto, "cloneNode",
        JS_NewCFunction(ctx, js_element_clone_node, "cloneNode", 1));
    JS_SetPropertyStr(ctx, element_proto, "contains",
        JS_NewCFunction(ctx, js_element_contains, "contains", 1));
    JS_SetPropertyStr(ctx, element_proto, "insertAdjacentHTML",
        JS_NewCFunction(ctx, js_element_insert_adjacent_html,
                        "insertAdjacentHTML", 2));

    // No-op element methods (sites call these, safe to ignore in sync renderer)
    JS_SetPropertyStr(ctx, element_proto, "scrollIntoView",
        JS_NewCFunction(ctx, js_element_scroll_into_view, "scrollIntoView", 0));
    JS_SetPropertyStr(ctx, element_proto, "scrollTo",
        JS_NewCFunction(ctx, js_element_scroll_to, "scrollTo", 0));
    JS_SetPropertyStr(ctx, element_proto, "scroll",
        JS_NewCFunction(ctx, js_element_scroll, "scroll", 0));
    JS_SetPropertyStr(ctx, element_proto, "focus",
        JS_NewCFunction(ctx, js_element_focus, "focus", 0));
    JS_SetPropertyStr(ctx, element_proto, "blur",
        JS_NewCFunction(ctx, js_element_blur, "blur", 0));
    JS_SetPropertyStr(ctx, element_proto, "animate",
        JS_NewCFunction(ctx, js_element_animate, "animate", 2));
    JS_SetPropertyStr(ctx, element_proto, "getAnimations",
        JS_NewCFunction(ctx, js_element_get_animations, "getAnimations", 0));

    // outerHTML internal getter/setter (wired into JS getter/setter by setup script)
    JS_SetPropertyStr(ctx, element_proto, "__getOuterHTML",
        JS_NewCFunction(ctx, js_element_get_outer_html,
                        "__getOuterHTML", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setOuterHTML",
        JS_NewCFunction(ctx, js_element_set_outer_html,
                        "__setOuterHTML", 1));

    // matches() aliases — webkitMatchesSelector / msMatchesSelector
    JS_SetPropertyStr(ctx, element_proto, "webkitMatchesSelector",
        JS_NewCFunction(ctx, js_element_matches, "webkitMatchesSelector", 1));
    JS_SetPropertyStr(ctx, element_proto, "msMatchesSelector",
        JS_NewCFunction(ctx, js_element_matches, "msMatchesSelector", 1));

    // Modern DOM manipulation methods (Cycle 220)
    JS_SetPropertyStr(ctx, element_proto, "before",
        JS_NewCFunction(ctx, js_element_before, "before", 1));
    JS_SetPropertyStr(ctx, element_proto, "after",
        JS_NewCFunction(ctx, js_element_after, "after", 1));
    JS_SetPropertyStr(ctx, element_proto, "prepend",
        JS_NewCFunction(ctx, js_element_prepend, "prepend", 1));
    JS_SetPropertyStr(ctx, element_proto, "append",
        JS_NewCFunction(ctx, js_element_append, "append", 1));
    JS_SetPropertyStr(ctx, element_proto, "replaceWith",
        JS_NewCFunction(ctx, js_element_replace_with, "replaceWith", 1));
    JS_SetPropertyStr(ctx, element_proto, "toggleAttribute",
        JS_NewCFunction(ctx, js_element_toggle_attribute, "toggleAttribute", 1));
    JS_SetPropertyStr(ctx, element_proto, "insertAdjacentElement",
        JS_NewCFunction(ctx, js_element_insert_adjacent_element,
                        "insertAdjacentElement", 2));
    JS_SetPropertyStr(ctx, element_proto, "insertAdjacentText",
        JS_NewCFunction(ctx, js_element_insert_adjacent_text,
                        "insertAdjacentText", 2));

    // Node methods
    JS_SetPropertyStr(ctx, element_proto, "hasChildNodes",
        JS_NewCFunction(ctx, js_element_has_child_nodes, "hasChildNodes", 0));
    JS_SetPropertyStr(ctx, element_proto, "getRootNode",
        JS_NewCFunction(ctx, js_element_get_root_node, "getRootNode", 0));
    JS_SetPropertyStr(ctx, element_proto, "isSameNode",
        JS_NewCFunction(ctx, js_element_is_same_node, "isSameNode", 1));
    JS_SetPropertyStr(ctx, element_proto, "compareDocumentPosition",
        JS_NewCFunction(ctx, js_element_compare_document_position,
                        "compareDocumentPosition", 1));

    // Shadow DOM methods
    JS_SetPropertyStr(ctx, element_proto, "attachShadow",
        JS_NewCFunction(ctx, js_element_attach_shadow, "attachShadow", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getShadowRoot",
        JS_NewCFunction(ctx, js_element_get_shadow_root, "__getShadowRoot", 0));

    // Node methods: normalize, isEqualNode
    JS_SetPropertyStr(ctx, element_proto, "normalize",
        JS_NewCFunction(ctx, js_node_normalize, "normalize", 0));
    JS_SetPropertyStr(ctx, element_proto, "isEqualNode",
        JS_NewCFunction(ctx, js_node_is_equal_node, "isEqualNode", 1));

    // HTMLElement property internal getters/setters
    JS_SetPropertyStr(ctx, element_proto, "__getHidden",
        JS_NewCFunction(ctx, js_element_get_hidden, "__getHidden", 0));
    JS_SetPropertyStr(ctx, element_proto, "__setHidden",
        JS_NewCFunction(ctx, js_element_set_hidden, "__setHidden", 1));
    JS_SetPropertyStr(ctx, element_proto, "__getOffsetParent",
        JS_NewCFunction(ctx, js_element_get_offset_parent, "__getOffsetParent", 0));

    JS_SetClassProto(ctx, element_class_id, element_proto);

    // ------------------------------------------------------------------
    // Allocate per-context DOMState and store pointer in global
    // ------------------------------------------------------------------
    auto* state = new DOMState();
    state->root = document_root;
    state->ctx = ctx;

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__dom_state_ptr",
        JS_NewInt64(ctx, static_cast<int64_t>(
            reinterpret_cast<uintptr_t>(state))));

    // ------------------------------------------------------------------
    // Create the document object
    // ------------------------------------------------------------------
    JSValue doc_obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, doc_obj, "getElementById",
        JS_NewCFunction(ctx, js_document_get_element_by_id,
                        "getElementById", 1));
    JS_SetPropertyStr(ctx, doc_obj, "querySelector",
        JS_NewCFunction(ctx, js_document_query_selector,
                        "querySelector", 1));
    JS_SetPropertyStr(ctx, doc_obj, "querySelectorAll",
        JS_NewCFunction(ctx, js_document_query_selector_all,
                        "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createElement",
        JS_NewCFunction(ctx, js_document_create_element,
                        "createElement", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createTextNode",
        JS_NewCFunction(ctx, js_document_create_text_node,
                        "createTextNode", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createDocumentFragment",
        JS_NewCFunction(ctx, js_document_create_document_fragment,
                        "createDocumentFragment", 0));
    JS_SetPropertyStr(ctx, doc_obj, "createComment",
        JS_NewCFunction(ctx, js_document_create_comment,
                        "createComment", 1));
    JS_SetPropertyStr(ctx, doc_obj, "importNode",
        JS_NewCFunction(ctx, js_document_import_node,
                        "importNode", 2));
    JS_SetPropertyStr(ctx, doc_obj, "adoptNode",
        JS_NewCFunction(ctx, js_document_adopt_node,
                        "adoptNode", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createEvent",
        JS_NewCFunction(ctx, js_document_create_event,
                        "createEvent", 1));
    JS_SetPropertyStr(ctx, doc_obj, "write",
        JS_NewCFunction(ctx, js_document_write, "write", 1));
    JS_SetPropertyStr(ctx, doc_obj, "writeln",
        JS_NewCFunction(ctx, js_document_writeln, "writeln", 1));

    // document.createRange() — returns a stub Range object
    {
        const char* createRange_code = R"JS(
(function() {
    return function createRange() {
        var range = {
            collapsed: true,
            startContainer: document,
            endContainer: document,
            startOffset: 0,
            endOffset: 0,
            commonAncestorContainer: document,
            selectNode: function(node) {
                this.startContainer = node;
                this.endContainer = node;
                this.commonAncestorContainer = node;
                this.collapsed = false;
            },
            selectNodeContents: function(node) {
                this.startContainer = node;
                this.endContainer = node;
                this.commonAncestorContainer = node;
                this.collapsed = false;
            },
            setStart: function(node, offset) {
                this.startContainer = node;
                this.startOffset = offset;
                this.collapsed = (this.startContainer === this.endContainer && this.startOffset === this.endOffset);
            },
            setEnd: function(node, offset) {
                this.endContainer = node;
                this.endOffset = offset;
                this.collapsed = (this.startContainer === this.endContainer && this.startOffset === this.endOffset);
            },
            collapse: function(toStart) {
                if (toStart) {
                    this.endContainer = this.startContainer;
                    this.endOffset = this.startOffset;
                } else {
                    this.startContainer = this.endContainer;
                    this.startOffset = this.endOffset;
                }
                this.collapsed = true;
            },
            cloneRange: function() {
                var clone = document.createRange();
                clone.startContainer = this.startContainer;
                clone.endContainer = this.endContainer;
                clone.startOffset = this.startOffset;
                clone.endOffset = this.endOffset;
                clone.commonAncestorContainer = this.commonAncestorContainer;
                clone.collapsed = this.collapsed;
                return clone;
            },
            detach: function() {},
            getBoundingClientRect: function() {
                return { x: 0, y: 0, width: 0, height: 0, top: 0, right: 0, bottom: 0, left: 0 };
            },
            getClientRects: function() { return []; },
            toString: function() { return ''; },
            createContextualFragment: function(html) {
                var frag = document.createDocumentFragment();
                return frag;
            },
            cloneContents: function() {
                return document.createDocumentFragment();
            },
            deleteContents: function() {},
            extractContents: function() {
                return document.createDocumentFragment();
            },
            insertNode: function(node) {},
            surroundContents: function(newParent) {},
            compareBoundaryPoints: function(how, sourceRange) { return 0; }
        };
        return range;
    };
})()
)JS";
        JSValue fn = JS_Eval(ctx, createRange_code, std::strlen(createRange_code),
                              "<createRange>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(fn)) {
            JS_SetPropertyStr(ctx, doc_obj, "createRange", fn);
        } else {
            JS_FreeValue(ctx, fn);
        }
    }

    // document.createTreeWalker(root, whatToShow, filter) — C++ impl
    JS_SetPropertyStr(ctx, doc_obj, "createTreeWalker",
        JS_NewCFunction(ctx, js_document_create_tree_walker,
                        "createTreeWalker", 3));

    // document.createNodeIterator(root, whatToShow, filter) — C++ impl
    JS_SetPropertyStr(ctx, doc_obj, "createNodeIterator",
        JS_NewCFunction(ctx, js_document_create_node_iterator,
                        "createNodeIterator", 3));

    // document.createProcessingInstruction(target, data) — stub
    {
        const char* code = R"JS(
(function() {
    return function createProcessingInstruction(target, data) {
        return { nodeType: 7, target: target, data: data, nodeName: target,
                 nodeValue: data, ownerDocument: document, parentNode: null,
                 textContent: data };
    };
})()
)JS";
        JSValue fn = JS_Eval(ctx, code, std::strlen(code),
                              "<createProcessingInstruction>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(fn)) {
            JS_SetPropertyStr(ctx, doc_obj, "createProcessingInstruction", fn);
        } else {
            JS_FreeValue(ctx, fn);
        }
    }

    // document.createCDATASection(data) — stub
    {
        const char* code = R"JS(
(function() {
    return function createCDATASection(data) {
        return { nodeType: 4, data: data, nodeName: '#cdata-section',
                 nodeValue: data, ownerDocument: document, parentNode: null,
                 textContent: data, length: (data || '').length };
    };
})()
)JS";
        JSValue fn = JS_Eval(ctx, code, std::strlen(code),
                              "<createCDATASection>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(fn)) {
            JS_SetPropertyStr(ctx, doc_obj, "createCDATASection", fn);
        } else {
            JS_FreeValue(ctx, fn);
        }
    }

    // NodeFilter constants on globalThis
    {
        JSValue nf = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nf, "FILTER_ACCEPT", JS_NewInt32(ctx, 1));
        JS_SetPropertyStr(ctx, nf, "FILTER_REJECT", JS_NewInt32(ctx, 2));
        JS_SetPropertyStr(ctx, nf, "FILTER_SKIP", JS_NewInt32(ctx, 3));
        JS_SetPropertyStr(ctx, nf, "SHOW_ALL", JS_NewUint32(ctx, 0xFFFFFFFF));
        JS_SetPropertyStr(ctx, nf, "SHOW_ELEMENT", JS_NewUint32(ctx, 0x1));
        JS_SetPropertyStr(ctx, nf, "SHOW_TEXT", JS_NewUint32(ctx, 0x4));
        JS_SetPropertyStr(ctx, nf, "SHOW_COMMENT", JS_NewUint32(ctx, 0x80));
        JS_SetPropertyStr(ctx, nf, "SHOW_DOCUMENT", JS_NewUint32(ctx, 0x100));
        JS_SetPropertyStr(ctx, global, "NodeFilter", nf);
    }

    JS_SetPropertyStr(ctx, doc_obj, "getElementsByTagName",
        JS_NewCFunction(ctx, js_document_get_elements_by_tag_name,
                        "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, doc_obj, "getElementsByClassName",
        JS_NewCFunction(ctx, js_document_get_elements_by_class_name,
                        "getElementsByClassName", 1));

    // document.elementFromPoint(x, y) — stub that returns body
    JS_SetPropertyStr(ctx, doc_obj, "elementFromPoint",
        JS_NewCFunction(ctx, js_document_element_from_point,
                        "elementFromPoint", 2));

    // document.addEventListener / removeEventListener
    JS_SetPropertyStr(ctx, doc_obj, "addEventListener",
        JS_NewCFunction(ctx, js_document_add_event_listener,
                        "addEventListener", 3));
    JS_SetPropertyStr(ctx, doc_obj, "removeEventListener",
        JS_NewCFunction(ctx, js_document_remove_event_listener,
                        "removeEventListener", 3));

    // Internal getters for document properties (wired into getters below)
    JS_SetPropertyStr(ctx, doc_obj, "__getBody",
        JS_NewCFunction(ctx, js_document_get_body, "__getBody", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getHead",
        JS_NewCFunction(ctx, js_document_get_head, "__getHead", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getDocumentElement",
        JS_NewCFunction(ctx, js_document_get_document_element,
                        "__getDocumentElement", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getTitle",
        JS_NewCFunction(ctx, js_document_get_title, "__getTitle", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__setTitle",
        JS_NewCFunction(ctx, js_document_set_title, "__setTitle", 1));

    // document.cookie internal getter/setter
    JS_SetPropertyStr(ctx, doc_obj, "__getCookie",
        JS_NewCFunction(ctx, js_document_get_cookie, "__getCookie", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__setCookie",
        JS_NewCFunction(ctx, js_document_set_cookie, "__setCookie", 1));

    // ---- document.readyState ----
    JS_SetPropertyStr(ctx, doc_obj, "readyState",
        JS_NewString(ctx, "complete"));

    // ---- document.defaultView ----
    // defaultView should point to the window (globalThis)
    JS_SetPropertyStr(ctx, doc_obj, "defaultView",
        JS_DupValue(ctx, global));

    // ---- document.characterEncoding / document.charset / document.inputEncoding ----
    JS_SetPropertyStr(ctx, doc_obj, "characterEncoding",
        JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, doc_obj, "charset",
        JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, doc_obj, "inputEncoding",
        JS_NewString(ctx, "UTF-8"));

    // ---- document.contentType ----
    JS_SetPropertyStr(ctx, doc_obj, "contentType",
        JS_NewString(ctx, "text/html"));

    // ---- document.currentScript (initially null, set during script execution) ----
    JS_SetPropertyStr(ctx, doc_obj, "currentScript", JS_NULL);

    // ---- document.visibilityState / document.hidden ----
    JS_SetPropertyStr(ctx, doc_obj, "visibilityState",
        JS_NewString(ctx, "visible"));
    JS_SetPropertyStr(ctx, doc_obj, "hidden",
        JS_NewBool(ctx, false));

    // ---- document.hasFocus() ----
    JS_SetPropertyStr(ctx, doc_obj, "hasFocus",
        JS_NewCFunction(ctx, js_document_has_focus, "hasFocus", 0));

    // ---- document.activeElement (internal getter) ----
    JS_SetPropertyStr(ctx, doc_obj, "__getActiveElement",
        JS_NewCFunction(ctx, js_document_get_active_element,
                        "__getActiveElement", 0));

    // ---- document.forms / images / links / scripts (internal getters) ----
    JS_SetPropertyStr(ctx, doc_obj, "__getForms",
        JS_NewCFunction(ctx, js_document_get_forms, "__getForms", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getImages",
        JS_NewCFunction(ctx, js_document_get_images, "__getImages", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getLinks",
        JS_NewCFunction(ctx, js_document_get_links, "__getLinks", 0));
    JS_SetPropertyStr(ctx, doc_obj, "__getScripts",
        JS_NewCFunction(ctx, js_document_get_scripts, "__getScripts", 0));

    // ---- document.implementation ----
    {
        JSValue impl = JS_NewObject(ctx);
        // hasFeature() always returns true per spec (since HTML5)
        const char* has_feature_code = R"JS(
(function() {
    return function hasFeature() { return true; };
})()
)JS";
        JSValue hf_fn = JS_Eval(ctx, has_feature_code, std::strlen(has_feature_code),
                                "<hasFeature>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(hf_fn)) {
            JS_SetPropertyStr(ctx, impl, "hasFeature", hf_fn);
        } else {
            JS_FreeValue(ctx, hf_fn);
        }
        // createHTMLDocument(title) — returns a minimal document-like object
        const char* create_doc_code = R"JS(
(function() {
    return function createHTMLDocument(title) {
        return {
            title: title || '',
            body: null,
            head: null,
            documentElement: null,
            createElement: function(tag) { return null; },
            createTextNode: function(text) { return null; }
        };
    };
})()
)JS";
        JSValue cd_fn = JS_Eval(ctx, create_doc_code, std::strlen(create_doc_code),
                                "<createHTMLDocument>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(cd_fn)) {
            JS_SetPropertyStr(ctx, impl, "createHTMLDocument", cd_fn);
        } else {
            JS_FreeValue(ctx, cd_fn);
        }
        JS_SetPropertyStr(ctx, doc_obj, "implementation", impl);
    }

    JS_SetPropertyStr(ctx, global, "document", doc_obj);

    // ---- document.location as getter/setter that delegates to window.location ----
    {
        const char* doc_loc_code = R"JS(
(function() {
    Object.defineProperty(document, 'location', {
        get: function() { return location; },
        set: function(v) { location.href = String(v); },
        configurable: true,
        enumerable: true
    });
})()
)JS";
        JSValue dl_ret = JS_Eval(ctx, doc_loc_code, std::strlen(doc_loc_code),
                                  "<document-location>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(dl_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, dl_ret);
    }

    // window.addEventListener / removeEventListener (window === globalThis)
    JS_SetPropertyStr(ctx, global, "addEventListener",
        JS_NewCFunction(ctx, js_window_add_event_listener,
                        "addEventListener", 3));
    JS_SetPropertyStr(ctx, global, "removeEventListener",
        JS_NewCFunction(ctx, js_window_remove_event_listener,
                        "removeEventListener", 3));

    // window.getComputedStyle(element [, pseudoElement])
    JS_SetPropertyStr(ctx, global, "getComputedStyle",
        JS_NewCFunction(ctx, js_get_computed_style,
                        "getComputedStyle", 1));

    // ------------------------------------------------------------------
    // Observer stubs (MutationObserver, IntersectionObserver, ResizeObserver)
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "MutationObserver",
        JS_NewCFunction2(ctx, js_mutation_observer_constructor,
                         "MutationObserver", 1,
                         JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, global, "IntersectionObserver",
        JS_NewCFunction2(ctx, js_intersection_observer_constructor,
                         "IntersectionObserver", 1,
                         JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, global, "ResizeObserver",
        JS_NewCFunction2(ctx, js_resize_observer_constructor,
                         "ResizeObserver", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // CustomEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "CustomEvent",
        JS_NewCFunction2(ctx, js_custom_event_constructor,
                         "CustomEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // Event constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "Event",
        JS_NewCFunction2(ctx, js_event_constructor,
                         "Event", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // KeyboardEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "KeyboardEvent",
        JS_NewCFunction2(ctx, js_keyboard_event_constructor,
                         "KeyboardEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // MouseEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "MouseEvent",
        JS_NewCFunction2(ctx, js_mouse_event_constructor,
                         "MouseEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // PointerEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "PointerEvent",
        JS_NewCFunction2(ctx, js_pointer_event_constructor,
                         "PointerEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // FocusEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "FocusEvent",
        JS_NewCFunction2(ctx, js_focus_event_constructor,
                         "FocusEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // InputEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "InputEvent",
        JS_NewCFunction2(ctx, js_input_event_constructor,
                         "InputEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // ErrorEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "ErrorEvent",
        JS_NewCFunction2(ctx, js_error_event_constructor,
                         "ErrorEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // PromiseRejectionEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "PromiseRejectionEvent",
        JS_NewCFunction2(ctx, js_promise_rejection_event_constructor,
                         "PromiseRejectionEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // WheelEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "WheelEvent",
        JS_NewCFunction2(ctx, js_wheel_event_constructor,
                         "WheelEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // HashChangeEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "HashChangeEvent",
        JS_NewCFunction2(ctx, js_hash_change_event_constructor,
                         "HashChangeEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // PopStateEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "PopStateEvent",
        JS_NewCFunction2(ctx, js_pop_state_event_constructor,
                         "PopStateEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // TransitionEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "TransitionEvent",
        JS_NewCFunction2(ctx, js_transition_event_constructor,
                         "TransitionEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // AnimationEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "AnimationEvent",
        JS_NewCFunction2(ctx, js_animation_event_constructor,
                         "AnimationEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // TouchEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "TouchEvent",
        JS_NewCFunction2(ctx, js_touch_event_constructor,
                         "TouchEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // DragEvent constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "DragEvent",
        JS_NewCFunction2(ctx, js_drag_event_constructor,
                         "DragEvent", 1,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // DOMParser constructor
    // ------------------------------------------------------------------
    JS_SetPropertyStr(ctx, global, "DOMParser",
        JS_NewCFunction2(ctx, js_domparser_constructor,
                         "DOMParser", 0,
                         JS_CFUNC_constructor, 0));

    // ------------------------------------------------------------------
    // URL constructor
    // ------------------------------------------------------------------
    if (url_class_id == 0) {
        JS_NewClassID(&url_class_id);
    }
    if (!JS_IsRegisteredClass(rt, url_class_id)) {
        JS_NewClass(rt, url_class_id, &url_class_def);
    }

    JSValue url_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, url_proto, "toString",
        JS_NewCFunction(ctx, js_url_to_string, "toString", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getHref",
        JS_NewCFunction(ctx, js_url_get_href, "__getHref", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getProtocol",
        JS_NewCFunction(ctx, js_url_get_protocol, "__getProtocol", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getHostname",
        JS_NewCFunction(ctx, js_url_get_hostname, "__getHostname", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getPort",
        JS_NewCFunction(ctx, js_url_get_port, "__getPort", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getPathname",
        JS_NewCFunction(ctx, js_url_get_pathname, "__getPathname", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getSearch",
        JS_NewCFunction(ctx, js_url_get_search, "__getSearch", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getHash",
        JS_NewCFunction(ctx, js_url_get_hash, "__getHash", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getOrigin",
        JS_NewCFunction(ctx, js_url_get_origin, "__getOrigin", 0));
    JS_SetPropertyStr(ctx, url_proto, "__getSearchParams",
        JS_NewCFunction(ctx, js_url_get_search_params, "__getSearchParams", 0));

    JSValue url_ctor = JS_NewCFunction2(ctx, js_url_constructor,
                                        "URL", 2,
                                        JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, url_ctor, url_proto);
    JS_SetClassProto(ctx, url_class_id, url_proto);
    JS_SetPropertyStr(ctx, global, "URL", url_ctor);

    // ------------------------------------------------------------------
    // TextEncoder constructor
    // ------------------------------------------------------------------
    if (text_encoder_class_id == 0) {
        JS_NewClassID(&text_encoder_class_id);
    }
    if (!JS_IsRegisteredClass(rt, text_encoder_class_id)) {
        JS_NewClass(rt, text_encoder_class_id, &text_encoder_class_def);
    }

    JSValue encoder_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, encoder_proto, "encode",
        JS_NewCFunction(ctx, js_text_encoder_encode, "encode", 1));

    JSValue encoder_ctor = JS_NewCFunction2(ctx, js_text_encoder_constructor,
                                            "TextEncoder", 0,
                                            JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, encoder_ctor, encoder_proto);
    JS_SetClassProto(ctx, text_encoder_class_id, encoder_proto);
    JS_SetPropertyStr(ctx, global, "TextEncoder", encoder_ctor);

    // ------------------------------------------------------------------
    // TextDecoder constructor
    // ------------------------------------------------------------------
    if (text_decoder_class_id == 0) {
        JS_NewClassID(&text_decoder_class_id);
    }
    if (!JS_IsRegisteredClass(rt, text_decoder_class_id)) {
        JS_NewClass(rt, text_decoder_class_id, &text_decoder_class_def);
    }

    JSValue decoder_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, decoder_proto, "decode",
        JS_NewCFunction(ctx, js_text_decoder_decode, "decode", 1));

    JSValue decoder_ctor = JS_NewCFunction2(ctx, js_text_decoder_constructor,
                                            "TextDecoder", 1,
                                            JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, decoder_ctor, decoder_proto);
    JS_SetClassProto(ctx, text_decoder_class_id, decoder_proto);
    JS_SetPropertyStr(ctx, global, "TextDecoder", decoder_ctor);

    JS_FreeValue(ctx, global);

    // ------------------------------------------------------------------
    // Wire up getters/setters via JavaScript eval
    // ------------------------------------------------------------------
    // Using eval is the cleanest way to set up getters/setters on
    // prototypes and the document object from C.
    const char* setup_script = R"JS(
(function() {
    // ---- document property getters/setters ----
    Object.defineProperty(document, 'body', {
        get: function() { return document.__getBody(); },
        configurable: true
    });
    Object.defineProperty(document, 'head', {
        get: function() { return document.__getHead(); },
        configurable: true
    });
    Object.defineProperty(document, 'documentElement', {
        get: function() { return document.__getDocumentElement(); },
        configurable: true
    });
    Object.defineProperty(document, 'title', {
        get: function() { return document.__getTitle(); },
        set: function(v) { document.__setTitle(v); },
        configurable: true
    });
    Object.defineProperty(document, 'cookie', {
        get: function() { return document.__getCookie(); },
        set: function(v) { document.__setCookie(v); },
        configurable: true
    });
    Object.defineProperty(document, 'activeElement', {
        get: function() { return document.__getActiveElement(); },
        configurable: true
    });
    Object.defineProperty(document, 'forms', {
        get: function() { return document.__getForms(); },
        configurable: true
    });
    Object.defineProperty(document, 'images', {
        get: function() { return document.__getImages(); },
        configurable: true
    });
    Object.defineProperty(document, 'links', {
        get: function() { return document.__getLinks(); },
        configurable: true
    });
    Object.defineProperty(document, 'scripts', {
        get: function() { return document.__getScripts(); },
        configurable: true
    });

    // ---- Element prototype getters/setters ----
    // We retrieve the prototype that was set via JS_SetClassProto.
    // Every Element proxy created by wrap_element() inherits from it.
    // We can obtain a reference by creating a dummy element via
    // document.createElement and reading its __proto__.
    var dummy = document.createElement('__dummy__');
    if (!dummy) return;
    var proto = Object.getPrototypeOf(dummy);
    if (!proto) return;

    Object.defineProperty(proto, 'id', {
        get: function() { return this.__getId(); },
        set: function(v) { this.__setId(String(v)); },
        configurable: true
    });
    Object.defineProperty(proto, 'tagName', {
        get: function() { return this.__getTagName(); },
        configurable: true
    });
    Object.defineProperty(proto, 'nodeName', {
        get: function() { return this.__getNodeName(); },
        configurable: true
    });
    Object.defineProperty(proto, 'className', {
        get: function() { return this.__getClassName(); },
        set: function(v) { this.__setClassName(v); },
        configurable: true
    });
    Object.defineProperty(proto, 'textContent', {
        get: function() { return this.__getTextContent(); },
        set: function(v) { this.__setTextContent(v); },
        configurable: true
    });
    Object.defineProperty(proto, 'innerHTML', {
        get: function() { return this.__getInnerHTML(); },
        set: function(v) { this.__setInnerHTML(v); },
        configurable: true
    });
    Object.defineProperty(proto, 'outerHTML', {
        get: function() { return this.__getOuterHTML(); },
        set: function(v) { this.__setOuterHTML(v); },
        configurable: true
    });
    Object.defineProperty(proto, 'children', {
        get: function() { return this.__getChildren(); },
        configurable: true
    });
    Object.defineProperty(proto, 'childNodes', {
        get: function() { return this.__getChildNodes(); },
        configurable: true
    });
    Object.defineProperty(proto, 'parentNode', {
        get: function() { return this.__getParentNode(); },
        configurable: true
    });
    Object.defineProperty(proto, 'parentElement', {
        get: function() { return this.__getParentNode(); },
        configurable: true
    });
    Object.defineProperty(proto, 'style', {
        get: function() {
            var raw = this.__getStyle();
            if (!raw) return raw;
            return new Proxy(raw, {
                get: function(target, prop, receiver) {
                    if (typeof prop !== 'string') return target[prop];
                    // Return methods bound to target (not the Proxy),
                    // so that C functions get the correct 'this' with opaque data
                    var val = target[prop];
                    if (typeof val === 'function') {
                        return val.bind(target);
                    }
                    if (typeof val === 'number' || val !== undefined) return val;
                    // For unknown properties (CSS camelCase like "display", "color"),
                    // look up the CSS property via __getProperty
                    var fn = target.__getProperty;
                    if (fn) return fn.call(target, prop);
                    return undefined;
                },
                set: function(target, prop, value) {
                    if (typeof prop !== 'string') return false;
                    if (prop === 'cssText') {
                        target.cssText = value;
                        return true;
                    }
                    var fn = target.__setProperty || target.setProperty;
                    if (fn) fn.call(target, prop, String(value));
                    return true;
                }
            });
        },
        configurable: true
    });

    // ---- Traversal properties ----
    Object.defineProperty(proto, 'firstChild', {
        get: function() { return this.__getFirstChild(); },
        configurable: true
    });
    Object.defineProperty(proto, 'lastChild', {
        get: function() { return this.__getLastChild(); },
        configurable: true
    });
    Object.defineProperty(proto, 'firstElementChild', {
        get: function() { return this.__getFirstElementChild(); },
        configurable: true
    });
    Object.defineProperty(proto, 'lastElementChild', {
        get: function() { return this.__getLastElementChild(); },
        configurable: true
    });
    Object.defineProperty(proto, 'nextSibling', {
        get: function() { return this.__getNextSibling(); },
        configurable: true
    });
    Object.defineProperty(proto, 'previousSibling', {
        get: function() { return this.__getPreviousSibling(); },
        configurable: true
    });
    Object.defineProperty(proto, 'nextElementSibling', {
        get: function() { return this.__getNextElementSibling(); },
        configurable: true
    });
    Object.defineProperty(proto, 'previousElementSibling', {
        get: function() { return this.__getPreviousElementSibling(); },
        configurable: true
    });
    Object.defineProperty(proto, 'childElementCount', {
        get: function() { return this.__getChildElementCount(); },
        configurable: true
    });
    Object.defineProperty(proto, 'nodeType', {
        get: function() { return this.__getNodeType(); },
        configurable: true
    });
    Object.defineProperty(proto, 'isConnected', {
        get: function() { return this.__getIsConnected(); },
        configurable: true
    });
    Object.defineProperty(proto, 'hidden', {
        get: function() { return this.__getHidden(); },
        set: function(v) { this.__setHidden(v); },
        configurable: true
    });
    Object.defineProperty(proto, 'offsetParent', {
        get: function() { return this.__getOffsetParent(); },
        configurable: true
    });

    // ---- title / lang / dir (string attribute mappings) ----
    Object.defineProperty(proto, 'title', {
        get: function() { return this.getAttribute('title') || ''; },
        set: function(v) { this.setAttribute('title', String(v)); },
        configurable: true
    });
    Object.defineProperty(proto, 'lang', {
        get: function() { return this.getAttribute('lang') || ''; },
        set: function(v) { this.setAttribute('lang', String(v)); },
        configurable: true
    });
    Object.defineProperty(proto, 'dir', {
        get: function() { return this.getAttribute('dir') || ''; },
        set: function(v) { this.setAttribute('dir', String(v)); },
        configurable: true
    });

    // ---- tabIndex (int attribute mapping) ----
    // Interactive elements default to 0 (natively focusable), others to -1
    Object.defineProperty(proto, 'tabIndex', {
        get: function() {
            var raw = this.getAttribute('tabindex');
            if (raw !== null && raw !== '') {
                var parsed = parseInt(raw, 10);
                return isNaN(parsed) ? -1 : parsed;
            }
            // Natively focusable elements default to 0 when no tabindex attr set
            var tag = (this.__getTagName ? this.__getTagName() : '').toLowerCase();
            var nativeFocusable = ['input', 'button', 'select', 'textarea', 'summary'];
            if (nativeFocusable.indexOf(tag) >= 0) return 0;
            // <a> and <area> with href default to 0
            if ((tag === 'a' || tag === 'area') && this.hasAttribute('href')) return 0;
            // contenteditable elements are focusable
            if (this.hasAttribute('contenteditable')) return 0;
            return -1;
        },
        set: function(v) {
            var parsed = parseInt(v, 10);
            this.setAttribute('tabindex', String(isNaN(parsed) ? -1 : parsed));
        },
        configurable: true
    });

    // ---- draggable (boolean attribute presence mapping) ----
    Object.defineProperty(proto, 'draggable', {
        get: function() { return this.hasAttribute('draggable'); },
        set: function(v) {
            if (v) this.setAttribute('draggable', '');
            else this.removeAttribute('draggable');
        },
        configurable: true
    });

    // ---- contentEditable (string attribute mapping) ----
    Object.defineProperty(proto, 'contentEditable', {
        get: function() { if (!this.hasAttribute('contenteditable')) return 'inherit'; var v = this.getAttribute('contenteditable'); return v || 'inherit'; },
        set: function(v) { this.setAttribute('contenteditable', String(v)); },
        configurable: true
    });

    // ---- shadowRoot getter ----
    Object.defineProperty(proto, 'shadowRoot', {
        get: function() { return this.__getShadowRoot(); },
        configurable: true
    });

    // ---- template.content getter ----
    // For <template> elements, content returns a document fragment
    // containing the element's children
    Object.defineProperty(proto, 'content', {
        get: function() {
            var tag = this.__getTagName();
            if (tag !== 'TEMPLATE') return undefined;
            // Create a document fragment and move children into it
            var frag = document.createDocumentFragment();
            var children = this.__getChildren();
            if (children) {
                for (var i = 0; i < children.length; i++) {
                    frag.appendChild(children[i]);
                }
            }
            return frag;
        },
        configurable: true
    });

    // ---- Dimension getters are now native (installed via C++ JS_DefinePropertyGetSet) ----

    // ---- classList proxy (DOMTokenList-like) ----
    // Returns a live object reflecting the element's class attribute.
    // Changes through classList update the class attribute immediately.
    Object.defineProperty(proto, 'classList', {
        get: function() {
            var self = this;
            // getClasses(): always reads live from the attribute
            var getClasses = function() {
                var cn = self.__getClassName() || '';
                if (cn === '') return [];
                return cn.split(/\s+/).filter(function(c) { return c.length > 0; });
            };
            var cl = {
                // add(cls, ...) — variadic, adds one or more classes
                add: function() {
                    for (var i = 0; i < arguments.length; i++) {
                        self.__classListAdd(String(arguments[i]));
                    }
                },
                // remove(cls, ...) — variadic, removes one or more classes
                remove: function() {
                    for (var i = 0; i < arguments.length; i++) {
                        self.__classListRemove(String(arguments[i]));
                    }
                },
                // contains(cls) — returns boolean
                contains: function(cls) {
                    return self.__classListContains(String(cls));
                },
                // toggle(cls, force?) — toggle, returns boolean (whether class is now present)
                toggle: function(cls, force) {
                    cls = String(cls);
                    if (arguments.length > 1) {
                        // force is explicitly provided
                        if (force) {
                            self.__classListAdd(cls);
                            return true;
                        } else {
                            self.__classListRemove(cls);
                            return false;
                        }
                    }
                    if (self.__classListContains(cls)) {
                        self.__classListRemove(cls);
                        return false;
                    } else {
                        self.__classListAdd(cls);
                        return true;
                    }
                },
                // replace(oldClass, newClass) — atomic replace, returns boolean
                replace: function(oldCls, newCls) {
                    return self.__classListReplace(String(oldCls), String(newCls));
                },
                // item(index) — get class at index, null if out of range
                item: function(index) {
                    var classes = getClasses();
                    index = index >>> 0; // ToUint32
                    return index < classes.length ? classes[index] : null;
                },
                // forEach(callback, thisArg?) — iterate: callback(value, index, list)
                forEach: function(callback, thisArg) {
                    var classes = getClasses();
                    for (var i = 0; i < classes.length; i++) {
                        callback.call(thisArg !== undefined ? thisArg : undefined, classes[i], i, cl);
                    }
                },
                // values() — iterator over class strings
                values: function() {
                    return getClasses()[Symbol.iterator]();
                },
                // keys() — iterator over indices (0, 1, 2, ...)
                keys: function() {
                    var classes = getClasses();
                    var i = 0;
                    return {
                        next: function() {
                            if (i < classes.length) {
                                return { value: i++, done: false };
                            }
                            return { value: undefined, done: true };
                        },
                        [Symbol.iterator]: function() { return this; }
                    };
                },
                // entries() — iterator over [index, class] pairs
                entries: function() {
                    var classes = getClasses();
                    var i = 0;
                    return {
                        next: function() {
                            if (i < classes.length) {
                                var idx = i++;
                                return { value: [idx, classes[idx]], done: false };
                            }
                            return { value: undefined, done: true };
                        },
                        [Symbol.iterator]: function() { return this; }
                    };
                },
                toString: function() {
                    return self.__getClassName() || '';
                }
            };
            // length — live count of classes
            Object.defineProperty(cl, 'length', {
                get: function() { return getClasses().length; },
                enumerable: true,
                configurable: true
            });
            // value — get/set the full className string
            Object.defineProperty(cl, 'value', {
                get: function() { return self.__getClassName() || ''; },
                set: function(v) { self.__setClassName(String(v)); },
                enumerable: true,
                configurable: true
            });
            // Symbol.iterator — make classList directly iterable (for...of)
            cl[Symbol.iterator] = function() {
                return getClasses()[Symbol.iterator]();
            };
            // Index-based access: cl[0], cl[1], etc. via Proxy
            // Wrap in a Proxy so numeric index access works: el.classList[0]
            return new Proxy(cl, {
                get: function(target, prop) {
                    if (typeof prop === 'string') {
                        var n = Number(prop);
                        if (prop !== '' && !isNaN(n) && n >= 0 && n === Math.floor(n)) {
                            var classes = getClasses();
                            return n < classes.length ? classes[n] : undefined;
                        }
                    }
                    return target[prop];
                },
                has: function(target, prop) {
                    if (typeof prop === 'string') {
                        var n = Number(prop);
                        if (prop !== '' && !isNaN(n) && n >= 0 && n === Math.floor(n)) {
                            return n < getClasses().length;
                        }
                    }
                    return prop in target;
                }
            });
        },
        configurable: true
    });

    // ---- dataset proxy ----
    Object.defineProperty(proto, 'dataset', {
        get: function() {
            var self = this;
            return new Proxy({}, {
                get: function(target, prop) {
                    if (typeof prop !== 'string') return undefined;
                    return self.__datasetGet(prop);
                },
                set: function(target, prop, value) {
                    if (typeof prop !== 'string') return false;
                    self.__datasetSet(prop, String(value));
                    return true;
                },
                has: function(target, prop) {
                    if (typeof prop !== 'string') return false;
                    return self.__datasetHas(prop);
                }
            });
        },
        configurable: true
    });

    // ---- Typed HTML element properties ----
    var isInputLikeTag = function(tag) {
        return tag === 'input' || tag === 'textarea' || tag === 'select';
    };

    // .value (get/set string) — input/textarea/select
    Object.defineProperty(proto, 'value', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            return this.getAttribute('value') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('value', String(v));
        },
        configurable: true
    });

    // .type (get/set string) — input-like (default "text" for input)
    Object.defineProperty(proto, 'type', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            if (tag === 'input') return this.getAttribute('type') || 'text';
            return this.getAttribute('type') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('type', String(v));
        },
        configurable: true
    });

    // .name (get/set string) — input/textarea/select
    Object.defineProperty(proto, 'name', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            return this.getAttribute('name') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('name', String(v));
        },
        configurable: true
    });

    // .placeholder (get/set string) — input/textarea/select
    Object.defineProperty(proto, 'placeholder', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            return this.getAttribute('placeholder') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('placeholder', String(v));
        },
        configurable: true
    });

    // .disabled (get/set boolean) — input/textarea/select
    Object.defineProperty(proto, 'disabled', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            return isInputLikeTag(tag) ? this.hasAttribute('disabled') : false;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            if (v) this.setAttribute('disabled', '');
            else this.removeAttribute('disabled');
        },
        configurable: true
    });

    // .checked (get/set boolean) — input/textarea/select
    Object.defineProperty(proto, 'checked', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            return isInputLikeTag(tag) ? this.hasAttribute('checked') : false;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            if (v) this.setAttribute('checked', '');
            else this.removeAttribute('checked');
        },
        configurable: true
    });

    // .readOnly (get/set boolean) — input/textarea/select
    Object.defineProperty(proto, 'readOnly', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            return isInputLikeTag(tag) ? this.hasAttribute('readonly') : false;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            if (v) this.setAttribute('readonly', '');
            else this.removeAttribute('readonly');
        },
        configurable: true
    });

    // .required (get/set boolean) — input/textarea/select
    Object.defineProperty(proto, 'required', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            return isInputLikeTag(tag) ? this.hasAttribute('required') : false;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            if (v) this.setAttribute('required', '');
            else this.removeAttribute('required');
        },
        configurable: true
    });

    // .maxLength (get/set int) — input/textarea/select
    Object.defineProperty(proto, 'maxLength', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return -1;
            var raw = this.getAttribute('maxlength');
            if (raw === null || raw === '') return -1;
            var parsed = parseInt(raw, 10);
            return isNaN(parsed) ? -1 : parsed;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            var parsed = parseInt(v, 10);
            if (isNaN(parsed)) this.removeAttribute('maxlength');
            else this.setAttribute('maxlength', String(parsed));
        },
        configurable: true
    });

    // .min / .max (get/set string) — input/textarea/select
    Object.defineProperty(proto, 'min', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            return this.getAttribute('min') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('min', String(v));
        },
        configurable: true
    });
    Object.defineProperty(proto, 'max', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return '';
            return this.getAttribute('max') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (!isInputLikeTag(tag)) return;
            this.setAttribute('max', String(v));
        },
        configurable: true
    });

    // .href (get/set string) — a/link elements
    Object.defineProperty(proto, 'href', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'a' && tag !== 'link') return '';
            return this.getAttribute('href') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'a' && tag !== 'link') return;
            this.setAttribute('href', String(v));
        },
        configurable: true
    });

    // .src (get/set string) — img
    Object.defineProperty(proto, 'src', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'img') return '';
            return this.getAttribute('src') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'img') return;
            this.setAttribute('src', String(v));
        },
        configurable: true
    });

    // .alt (get/set string) — img
    Object.defineProperty(proto, 'alt', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'img') return '';
            return this.getAttribute('alt') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'img') return;
            this.setAttribute('alt', String(v));
        },
        configurable: true
    });

    // .width / .height (get/set int) — img only (canvas has its own via instance props)
    Object.defineProperty(proto, 'width', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag === 'canvas' || tag === 'video') return this.__elem_width || 0;
            if (tag !== 'img') return undefined;
            var raw = this.getAttribute('width');
            if (raw === null || raw === '') return 0;
            var parsed = parseInt(raw, 10);
            return isNaN(parsed) ? 0 : parsed;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag === 'canvas' || tag === 'video') { this.__elem_width = parseInt(v, 10) || 0; return; }
            if (tag !== 'img') return;
            var parsed = parseInt(v, 10);
            this.setAttribute('width', String(isNaN(parsed) ? 0 : parsed));
        },
        configurable: true
    });
    Object.defineProperty(proto, 'height', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag === 'canvas' || tag === 'video') return this.__elem_height || 0;
            if (tag !== 'img') return undefined;
            var raw = this.getAttribute('height');
            if (raw === null || raw === '') return 0;
            var parsed = parseInt(raw, 10);
            return isNaN(parsed) ? 0 : parsed;
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag === 'canvas' || tag === 'video') { this.__elem_height = parseInt(v, 10) || 0; return; }
            if (tag !== 'img') return;
            var parsed = parseInt(v, 10);
            this.setAttribute('height', String(isNaN(parsed) ? 0 : parsed));
        },
        configurable: true
    });

    // .src (get/set string) — iframe
    var __imgSrcDesc = Object.getOwnPropertyDescriptor(proto, 'src');
    Object.defineProperty(proto, 'src', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'iframe') {
                if (__imgSrcDesc && __imgSrcDesc.get) return __imgSrcDesc.get.call(this);
                return '';
            }
            return this.getAttribute('src') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'iframe') {
                if (__imgSrcDesc && __imgSrcDesc.set) __imgSrcDesc.set.call(this, v);
                return;
            }
            this.setAttribute('src', String(v));
        },
        configurable: true
    });

    // .contentWindow (get) — iframe
    Object.defineProperty(proto, 'contentWindow', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'iframe') return null;
            return null;
        },
        configurable: true
    });

    // .contentDocument (get) — iframe
    Object.defineProperty(proto, 'contentDocument', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'iframe') return null;
            return null;
        },
        configurable: true
    });

    // .action / .method (get/set string) — form
    Object.defineProperty(proto, 'action', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'form') return '';
            return this.getAttribute('action') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'form') return;
            this.setAttribute('action', String(v));
        },
        configurable: true
    });
    Object.defineProperty(proto, 'method', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'form') return '';
            return this.getAttribute('method') || '';
        },
        set: function(v) {
            var tag = this.tagName.toLowerCase();
            if (tag !== 'form') return;
            this.setAttribute('method', String(v));
        },
        configurable: true
    });

    // .selectedIndex (get/set number) — select elements
    Object.defineProperty(proto, 'selectedIndex', {
        get: function() {
            var tag = this.tagName.toLowerCase();
            if (tag === 'select') {
                var idx = this.getAttribute('data-selected-index');
                return idx !== null ? parseInt(idx, 10) : -1;
            }
            return -1;
        },
        set: function(v) {
            this.setAttribute('data-selected-index', String(parseInt(v, 10)));
        },
        configurable: true
    });

    // Clean up the dummy element from owned_nodes -- it will be GC'd
    // by JS, and is also in the owned_nodes list. We leave it there;
    // it's harmless.

    // ---- URL prototype getters ----
    if (typeof URL !== 'undefined' && URL.prototype) {
        Object.defineProperty(URL.prototype, 'href', {
            get: function() { return this.__getHref(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'protocol', {
            get: function() { return this.__getProtocol(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'hostname', {
            get: function() { return this.__getHostname(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'port', {
            get: function() { return this.__getPort(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'pathname', {
            get: function() { return this.__getPathname(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'search', {
            get: function() { return this.__getSearch(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'hash', {
            get: function() { return this.__getHash(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'origin', {
            get: function() { return this.__getOrigin(); },
            configurable: true
        });
        Object.defineProperty(URL.prototype, 'searchParams', {
            get: function() { return this.__getSearchParams(); },
            configurable: true
        });
    }
})();
)JS";

    JSValue ret = JS_Eval(ctx, setup_script, std::strlen(setup_script),
                           "<dom-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        // Silently consume -- DOM setup errors are not fatal
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);

    // ------------------------------------------------------------------
    // navigator object
    // ------------------------------------------------------------------
    {
        JSValue nav = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "userAgent",
            JS_NewString(ctx, "Vibrowser/0.7.0 (Macintosh; like Gecko)"));
        JS_SetPropertyStr(ctx, nav, "appName",
            JS_NewString(ctx, "Vibrowser"));
        JS_SetPropertyStr(ctx, nav, "appVersion",
            JS_NewString(ctx, "0.7.0"));
        JS_SetPropertyStr(ctx, nav, "platform",
            JS_NewString(ctx, "MacIntel"));
        JS_SetPropertyStr(ctx, nav, "language",
            JS_NewString(ctx, "en-US"));
        JSValue langs = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, langs, 0, JS_NewString(ctx, "en-US"));
        JS_SetPropertyUint32(ctx, langs, 1, JS_NewString(ctx, "en"));
        JS_SetPropertyStr(ctx, nav, "languages", langs);
        JS_SetPropertyStr(ctx, nav, "onLine", JS_TRUE);
        JS_SetPropertyStr(ctx, nav, "cookieEnabled", JS_TRUE);
        JS_SetPropertyStr(ctx, nav, "hardwareConcurrency", JS_NewInt32(ctx, 4));
        JS_SetPropertyStr(ctx, nav, "maxTouchPoints", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, nav, "vendor",
            JS_NewString(ctx, "Vibrowser"));
        JS_SetPropertyStr(ctx, nav, "vendorSub", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, nav, "product",
            JS_NewString(ctx, "Gecko"));
        JS_SetPropertyStr(ctx, nav, "productSub",
            JS_NewString(ctx, "20030107"));
        // navigator.clipboard (stub)
        JSValue clipboard = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "clipboard", clipboard);
        // navigator.mediaDevices (stub)
        JSValue media_devices = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "mediaDevices", media_devices);
        // navigator.geolocation (stub)
        JSValue geoloc = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "geolocation", geoloc);
        // navigator.serviceWorker (stub)
        JSValue sw = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "serviceWorker", sw);
        // navigator.permissions (stub)
        JSValue perms = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "permissions", perms);

        JS_SetPropertyStr(ctx, global, "navigator", nav);
    }

    // ------------------------------------------------------------------
    // navigator.sendBeacon and extra navigator APIs
    // ------------------------------------------------------------------
    {
        const char* nav_extra_src = R"JS(
        if (typeof navigator !== 'undefined') {
            navigator.sendBeacon = function(url, data) { return true; };
            navigator.vibrate = function() { return true; };
            navigator.share = function() { return Promise.reject(new DOMException('Share API not supported', 'NotAllowedError')); };
            navigator.canShare = function() { return false; };
            navigator.requestMIDIAccess = function() { return Promise.reject(new DOMException('Web MIDI not supported', 'NotSupportedError')); };
        }
)JS";
        JSValue nav_ret = JS_Eval(ctx, nav_extra_src, std::strlen(nav_extra_src),
                                   "<navigator-extra>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(nav_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, nav_ret);
    }

    // ------------------------------------------------------------------
    // navigator.geolocation methods (stubs)
    // ------------------------------------------------------------------
    {
        const char* geo_src = R"JS(
(function() {
    if (typeof navigator !== 'undefined' && navigator.geolocation) {
        navigator.geolocation.getCurrentPosition = function(success, error) {
            if (typeof error === 'function') {
                error({ code: 1, message: 'Not supported', PERMISSION_DENIED: 1 });
            }
        };
        navigator.geolocation.watchPosition = function(success, error) {
            if (typeof error === 'function') {
                error({ code: 1, message: 'Not supported', PERMISSION_DENIED: 1 });
            }
            return 0;
        };
        navigator.geolocation.clearWatch = function() {};
    }
})();
)JS";
        JSValue geo_ret = JS_Eval(ctx, geo_src, std::strlen(geo_src),
                                   "<geolocation-stubs>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(geo_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, geo_ret);
    }

    // ------------------------------------------------------------------
    // window.location object (basic about:blank fallback; install_window_bindings
    // will override this with the full spec version when a real URL is available)
    // ------------------------------------------------------------------
    {
        JSValue loc = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, loc, "href", JS_NewString(ctx, "about:blank"));
        JS_SetPropertyStr(ctx, loc, "protocol", JS_NewString(ctx, "about:"));
        JS_SetPropertyStr(ctx, loc, "host", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, loc, "hostname", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, loc, "port", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, loc, "pathname", JS_NewString(ctx, "blank"));
        JS_SetPropertyStr(ctx, loc, "search", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, loc, "hash", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, loc, "origin", JS_NewString(ctx, "null"));

        // Add basic methods for spec compliance even on about:blank
        const char* loc_methods_code = R"JS(
(function(loc) {
    loc.assign = function(url) { loc.href = String(url); };
    loc.replace = function(url) { loc.href = String(url); };
    loc.reload = function() {};
    loc.toString = function() { return loc.href; };
})(this)
)JS";
        JSValue mfn = JS_Eval(ctx, loc_methods_code, std::strlen(loc_methods_code),
                               "<location-methods>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsFunction(ctx, mfn)) {
            JS_Call(ctx, mfn, loc, 0, nullptr);
        }
        JS_FreeValue(ctx, mfn);

        JS_SetPropertyStr(ctx, global, "location", loc);
    }

    // ------------------------------------------------------------------
    // window.screen object
    // ------------------------------------------------------------------
    {
        JSValue scr = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, scr, "width", JS_NewInt32(ctx, 1920));
        JS_SetPropertyStr(ctx, scr, "height", JS_NewInt32(ctx, 1080));
        JS_SetPropertyStr(ctx, scr, "availWidth", JS_NewInt32(ctx, 1920));
        JS_SetPropertyStr(ctx, scr, "availHeight", JS_NewInt32(ctx, 1080));
        JS_SetPropertyStr(ctx, scr, "colorDepth", JS_NewInt32(ctx, 24));
        JS_SetPropertyStr(ctx, scr, "pixelDepth", JS_NewInt32(ctx, 24));
        JS_SetPropertyStr(ctx, global, "screen", scr);
    }

    // ------------------------------------------------------------------
    // window.history (stub)
    // ------------------------------------------------------------------
    {
        JSValue hist = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, hist, "length", JS_NewInt32(ctx, 1));
        JS_SetPropertyStr(ctx, global, "history", hist);
    }

    // ------------------------------------------------------------------
    // Blob constructor
    // ------------------------------------------------------------------
    {
        const char* blob_src = R"JS(
(function() {
    globalThis.Blob = function Blob(parts, options) {
        this._parts = parts || [];
        this._options = options || {};
        this.type = this._options.type || '';
        var size = 0;
        for (var i = 0; i < this._parts.length; i++) {
            var p = this._parts[i];
            if (typeof p === 'string') size += p.length;
            else if (p && p.byteLength !== undefined) size += p.byteLength;
            else if (p && p.size !== undefined) size += p.size;
        }
        this.size = size;
    };
    Blob.prototype.slice = function(start, end, type) {
        return new Blob([], {type: type || this.type});
    };
    Blob.prototype.text = function() {
        var str = '';
        for (var i = 0; i < this._parts.length; i++) {
            if (typeof this._parts[i] === 'string') str += this._parts[i];
        }
        return Promise.resolve(str);
    };
    Blob.prototype.arrayBuffer = function() {
        return Promise.resolve(new ArrayBuffer(0));
    };

    // File extends Blob
    globalThis.File = function File(parts, name, options) {
        Blob.call(this, parts, options);
        this.name = name || '';
        this.lastModified = (options && options.lastModified) || Date.now();
    };
    File.prototype = Object.create(Blob.prototype);
    File.prototype.constructor = File;

    // FileReader
    globalThis.FileReader = function FileReader() {
        this.readyState = 0; // EMPTY
        this.result = null;
        this.error = null;
        this._listeners = {};
    };
    FileReader.EMPTY = 0;
    FileReader.LOADING = 1;
    FileReader.DONE = 2;
    FileReader.prototype.addEventListener = function(type, fn) {
        if (!this._listeners[type]) this._listeners[type] = [];
        this._listeners[type].push(fn);
    };
    FileReader.prototype.removeEventListener = function(type, fn) {
        if (!this._listeners[type]) return;
        this._listeners[type] = this._listeners[type].filter(function(f) { return f !== fn; });
    };
    FileReader.prototype._dispatch = function(type) {
        var evt = {type: type, target: this};
        if (typeof this['on' + type] === 'function') this['on' + type](evt);
        var fns = this._listeners[type] || [];
        for (var i = 0; i < fns.length; i++) fns[i](evt);
    };
    FileReader.prototype.readAsText = function(blob) {
        var self = this;
        self.readyState = 1;
        self._dispatch('loadstart');
        if (blob && blob.text) {
            blob.text().then(function(txt) {
                self.result = txt;
                self.readyState = 2;
                self._dispatch('load');
                self._dispatch('loadend');
            });
        } else {
            self.result = '';
            self.readyState = 2;
            self._dispatch('load');
            self._dispatch('loadend');
        }
    };
    FileReader.prototype.readAsDataURL = function(blob) {
        var self = this;
        self.readyState = 1;
        self._dispatch('loadstart');
        self.result = 'data:' + (blob && blob.type || '') + ';base64,';
        self.readyState = 2;
        self._dispatch('load');
        self._dispatch('loadend');
    };
    FileReader.prototype.readAsArrayBuffer = function() {
        var self = this;
        self.readyState = 1;
        self._dispatch('loadstart');
        self.result = new ArrayBuffer(0);
        self.readyState = 2;
        self._dispatch('load');
        self._dispatch('loadend');
    };
    FileReader.prototype.abort = function() {
        this.readyState = 2;
        this._dispatch('abort');
        this._dispatch('loadend');
    };
})();
)JS";
        JSValue ret2 = JS_Eval(ctx, blob_src, std::strlen(blob_src),
                                "<blob-setup>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ret2)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret2);
    }

    // ------------------------------------------------------------------
    // DOMRect / DOMRectReadOnly constructor
    //
    // new DOMRect(x, y, width, height) creates a plain DOMRect object
    // compatible with what getBoundingClientRect() returns.
    // DOMRectReadOnly has the same shape but is conventionally read-only.
    // =========================================================================
    {
        const char* domrect_src = R"JS(
(function() {
    function DOMRect(x, y, width, height) {
        this.x      = (x      !== undefined) ? +x      : 0;
        this.y      = (y      !== undefined) ? +y      : 0;
        this.width  = (width  !== undefined) ? +width  : 0;
        this.height = (height !== undefined) ? +height : 0;
    }
    Object.defineProperty(DOMRect.prototype, 'top', {
        get: function() { return this.height >= 0 ? this.y : this.y + this.height; },
        configurable: true, enumerable: true
    });
    Object.defineProperty(DOMRect.prototype, 'left', {
        get: function() { return this.width >= 0 ? this.x : this.x + this.width; },
        configurable: true, enumerable: true
    });
    Object.defineProperty(DOMRect.prototype, 'right', {
        get: function() { return this.width >= 0 ? this.x + this.width : this.x; },
        configurable: true, enumerable: true
    });
    Object.defineProperty(DOMRect.prototype, 'bottom', {
        get: function() { return this.height >= 0 ? this.y + this.height : this.y; },
        configurable: true, enumerable: true
    });
    DOMRect.prototype.toJSON = function() {
        return { x: this.x, y: this.y, width: this.width, height: this.height,
                 top: this.top, right: this.right, bottom: this.bottom, left: this.left };
    };
    DOMRect.fromRect = function(other) {
        if (!other) other = {};
        return new DOMRect(other.x || 0, other.y || 0, other.width || 0, other.height || 0);
    };
    globalThis.DOMRect = DOMRect;

    // DOMRectReadOnly — same shape, conventionally immutable
    function DOMRectReadOnly(x, y, width, height) {
        DOMRect.call(this, x, y, width, height);
    }
    DOMRectReadOnly.prototype = Object.create(DOMRect.prototype);
    DOMRectReadOnly.prototype.constructor = DOMRectReadOnly;
    DOMRectReadOnly.fromRect = DOMRect.fromRect;
    globalThis.DOMRectReadOnly = DOMRectReadOnly;

    // DOMRectList — array-like list returned by getClientRects()
    function DOMRectList(rects) {
        this._rects = rects || [];
        this.length = this._rects.length;
        for (var i = 0; i < this._rects.length; i++) this[i] = this._rects[i];
    }
    DOMRectList.prototype.item = function(i) {
        return (i >= 0 && i < this.length) ? this[i] : null;
    };
    DOMRectList.prototype[Symbol.iterator] = function() {
        var i = 0, arr = this._rects;
        return { next: function() {
            return i < arr.length ? { value: arr[i++], done: false }
                                  : { done: true };
        }};
    };
    globalThis.DOMRectList = DOMRectList;
})();
)JS";
        JSValue dret = JS_Eval(ctx, domrect_src, std::strlen(domrect_src),
                                "<domrect-setup>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(dret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, dret);
    }

    // ------------------------------------------------------------------
    // Window geometry defaults (do not override values already provided by
    // install_window_bindings for the active viewport).
    // ------------------------------------------------------------------
    JSValue existing_dpr = JS_GetPropertyStr(ctx, global, "devicePixelRatio");
    if (JS_IsUndefined(existing_dpr)) {
        JS_SetPropertyStr(ctx, global, "devicePixelRatio", JS_NewFloat64(ctx, 1.0));
    }
    JS_FreeValue(ctx, existing_dpr);

    JSValue existing_inner_w = JS_GetPropertyStr(ctx, global, "innerWidth");
    if (JS_IsUndefined(existing_inner_w)) {
        JS_SetPropertyStr(ctx, global, "innerWidth", JS_NewInt32(ctx, 1024));
    }
    JS_FreeValue(ctx, existing_inner_w);

    JSValue existing_inner_h = JS_GetPropertyStr(ctx, global, "innerHeight");
    if (JS_IsUndefined(existing_inner_h)) {
        JS_SetPropertyStr(ctx, global, "innerHeight", JS_NewInt32(ctx, 768));
    }
    JS_FreeValue(ctx, existing_inner_h);

    JSValue existing_outer_w = JS_GetPropertyStr(ctx, global, "outerWidth");
    if (JS_IsUndefined(existing_outer_w)) {
        JSValue current_inner_w = JS_GetPropertyStr(ctx, global, "innerWidth");
        JS_SetPropertyStr(ctx, global, "outerWidth", current_inner_w);
    }
    JS_FreeValue(ctx, existing_outer_w);

    JSValue existing_outer_h = JS_GetPropertyStr(ctx, global, "outerHeight");
    if (JS_IsUndefined(existing_outer_h)) {
        JSValue current_inner_h = JS_GetPropertyStr(ctx, global, "innerHeight");
        JS_SetPropertyStr(ctx, global, "outerHeight", current_inner_h);
    }
    JS_FreeValue(ctx, existing_outer_h);

    JS_SetPropertyStr(ctx, global, "scrollX", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "scrollY", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "pageXOffset", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "pageYOffset", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "screenX", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "screenY", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "screenLeft", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "screenTop", JS_NewInt32(ctx, 0));

    // Ensure 'window' alias exists (may already be set by install_window_bindings)
    JSValue existing_window = JS_GetPropertyStr(ctx, global, "window");
    if (JS_IsUndefined(existing_window)) {
        JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));
    }
    JS_FreeValue(ctx, existing_window);

    // Additional window properties
    JS_SetPropertyStr(ctx, global, "origin", JS_NewString(ctx, "null"));
    JS_SetPropertyStr(ctx, global, "name", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, global, "opener", JS_NULL);
    JS_SetPropertyStr(ctx, global, "parent", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "top", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "frameElement", JS_NULL);
    JS_SetPropertyStr(ctx, global, "frames", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "length", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "closed", JS_FALSE);
    JS_SetPropertyStr(ctx, global, "isSecureContext", JS_TRUE);
    JS_SetPropertyStr(ctx, global, "crossOriginIsolated", JS_FALSE);

    // ------------------------------------------------------------------
    // performance object (basic stub)
    // ------------------------------------------------------------------
    {
        JSValue perf = JS_NewObject(ctx);
        // performance.now() — high-resolution timer
        const char* perf_now_src = R"JS(
(function() {
    var start = Date.now();
    globalThis.performance.now = function() { return Date.now() - start; };
    globalThis.performance.timeOrigin = start;
    globalThis.performance.toJSON = function() {
        return { timeOrigin: this.timeOrigin };
    };
    globalThis.performance.getEntries = function() { return []; };
    globalThis.performance.getEntriesByName = function() { return []; };
    globalThis.performance.getEntriesByType = function() { return []; };
    globalThis.performance.mark = function() {};
    globalThis.performance.measure = function() {};
    globalThis.performance.clearMarks = function() {};
    globalThis.performance.clearMeasures = function() {};
    globalThis.performance.timing = {};
    globalThis.performance.navigation = { type: 0, redirectCount: 0 };
})();
)JS";
        JS_SetPropertyStr(ctx, global, "performance", perf);
        JSValue pret = JS_Eval(ctx, perf_now_src, std::strlen(perf_now_src),
                                "<perf-setup>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(pret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, pret);
    }

    // ------------------------------------------------------------------
    // window.matchMedia(query) — returns MediaQueryList stub
    // ------------------------------------------------------------------
    {
        const char* mm_src = R"JS(
(function() {
    globalThis.matchMedia = function(query) {
        var matches = false;
        var viewportWidth = (typeof globalThis.innerWidth === 'number' && globalThis.innerWidth > 0)
            ? globalThis.innerWidth
            : 1024;
        // Simple checks for common queries
        if (query.indexOf('(prefers-color-scheme: light)') !== -1) matches = true;
        if (query.indexOf('(min-width:') !== -1) {
            var m = query.match(/min-width:\s*(\d+)/);
            if (m) matches = parseInt(m[1]) <= viewportWidth;
        }
        if (query.indexOf('(max-width:') !== -1) {
            var m = query.match(/max-width:\s*(\d+)/);
            if (m) matches = parseInt(m[1]) >= viewportWidth;
        }
        return {
            matches: matches,
            media: query,
            onchange: null,
            addListener: function() {},
            removeListener: function() {},
            addEventListener: function() {},
            removeEventListener: function() {},
            dispatchEvent: function() { return false; }
        };
    };
})();
)JS";
        JSValue ret3 = JS_Eval(ctx, mm_src, std::strlen(mm_src),
                                "<matchmedia-setup>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ret3)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret3);
    }

    // ------------------------------------------------------------------
    // window.scrollTo/scrollBy/scroll (no-op stubs)
    // ------------------------------------------------------------------
    {
        const char* scroll_src = R"JS(
(function() {
    if (typeof globalThis.confirm !== 'function') globalThis.confirm = function() { return false; };
    if (typeof globalThis.prompt !== 'function') globalThis.prompt = function() { return null; };
    if (typeof globalThis.print !== 'function') globalThis.print = function() {};
    if (typeof globalThis.focus !== 'function') globalThis.focus = function() {};
    if (typeof globalThis.blur !== 'function') globalThis.blur = function() {};
    if (typeof globalThis.stop !== 'function') globalThis.stop = function() {};
    if (typeof globalThis.find !== 'function') globalThis.find = function() { return false; };
    if (typeof globalThis.open !== 'function') globalThis.open = function() { return null; };
    if (typeof globalThis.close !== 'function') globalThis.close = function() {};
    if (typeof globalThis.postMessage !== 'function') globalThis.postMessage = function() {};
    if (typeof globalThis.requestIdleCallback !== 'function') {
        globalThis.requestIdleCallback = function(fn) {
            fn({timeRemaining: function() { return 50; }, didTimeout: false});
            return 1;
        };
    }
    if (typeof globalThis.cancelIdleCallback !== 'function') globalThis.cancelIdleCallback = function() {};
    if (typeof globalThis.btoa !== 'function') globalThis.btoa = function(str) {
        var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
        var result = '';
        for (var i = 0; i < str.length; i += 3) {
            var a = str.charCodeAt(i);
            var b = i + 1 < str.length ? str.charCodeAt(i + 1) : 0;
            var c = i + 2 < str.length ? str.charCodeAt(i + 2) : 0;
            result += chars[a >> 2];
            result += chars[((a & 3) << 4) | (b >> 4)];
            result += i + 1 < str.length ? chars[((b & 15) << 2) | (c >> 6)] : '=';
            result += i + 2 < str.length ? chars[c & 63] : '=';
        }
        return result;
    };
    if (typeof globalThis.atob !== 'function') globalThis.atob = function(str) {
        var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
        var result = '';
        str = str.replace(/[^A-Za-z0-9+/=]/g, '');
        for (var i = 0; i < str.length; i += 4) {
            var a = chars.indexOf(str[i]);
            var b = chars.indexOf(str[i + 1]);
            var c = chars.indexOf(str[i + 2]);
            var d = chars.indexOf(str[i + 3]);
            result += String.fromCharCode((a << 2) | (b >> 4));
            if (c !== 64) result += String.fromCharCode(((b & 15) << 4) | (c >> 2));
            if (d !== 64) result += String.fromCharCode(((c & 3) << 6) | d);
        }
        return result;
    };
})();
)JS";
        JSValue ret4 = JS_Eval(ctx, scroll_src, std::strlen(scroll_src),
                                "<scroll-setup>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ret4)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret4);
    }

    // ------------------------------------------------------------------
    // CSSStyleSheet + document.styleSheets
    // ------------------------------------------------------------------
    {
        const char* cssom_src = R"JS(
        function CSSStyleSheet() {
            this.cssRules = [];
            this.rules = this.cssRules;
            this.disabled = false;
            this.ownerNode = null;
            this.parentStyleSheet = null;
            this.href = null;
            this.title = null;
            this.type = 'text/css';
            this.media = { length: 0, mediaText: '' };
        }
        CSSStyleSheet.prototype.insertRule = function(rule, index) {
            if (index === undefined) index = 0;
            var ruleObj = { cssText: rule, type: 1 };
            this.cssRules.splice(index, 0, ruleObj);
            this.rules = this.cssRules;
            return index;
        };
        CSSStyleSheet.prototype.deleteRule = function(index) {
            this.cssRules.splice(index, 1);
            this.rules = this.cssRules;
        };
        CSSStyleSheet.prototype.addRule = function(selector, style, index) {
            var rule = selector + '{' + style + '}';
            if (index === undefined) index = this.cssRules.length;
            return this.insertRule(rule, index);
        };
        CSSStyleSheet.prototype.removeRule = function(index) {
            this.deleteRule(index === undefined ? 0 : index);
        };
        CSSStyleSheet.prototype.replace = function(text) {
            this.cssRules = [];
            this.rules = this.cssRules;
            return Promise.resolve(this);
        };
        CSSStyleSheet.prototype.replaceSync = function(text) {
            this.cssRules = [];
            this.rules = this.cssRules;
        };

        // document.styleSheets
        if (typeof document !== 'undefined') {
            Object.defineProperty(document, 'styleSheets', {
                get: function() {
                    var sheets = [];
                    sheets.length = 0;
                    sheets.item = function(i) { return sheets[i] || null; };
                    return sheets;
                },
                configurable: true
            });
            document.adoptedStyleSheets = [];
        }
)JS";
        JSValue cssom_ret = JS_Eval(ctx, cssom_src, std::strlen(cssom_src),
                                     "<cssom>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(cssom_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, cssom_ret);
    }

    // ------------------------------------------------------------------
    // PerformanceObserver stub
    // ------------------------------------------------------------------
    {
        const char* perf_obs_src = R"JS(
        function PerformanceObserver(callback) {
            this._callback = callback;
            this._entryTypes = [];
        }
        PerformanceObserver.prototype.observe = function(options) {
            if (options && options.entryTypes) this._entryTypes = options.entryTypes;
        };
        PerformanceObserver.prototype.disconnect = function() {};
        PerformanceObserver.prototype.takeRecords = function() { return []; };
        PerformanceObserver.supportedEntryTypes = ['mark', 'measure', 'navigation', 'resource', 'paint', 'largest-contentful-paint', 'first-input', 'layout-shift'];
)JS";
        JSValue po_ret = JS_Eval(ctx, perf_obs_src, std::strlen(perf_obs_src),
                                  "<perf-observer>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(po_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, po_ret);
    }

    // ------------------------------------------------------------------
    // DOMException polyfill (needed by AbortController etc.)
    // ------------------------------------------------------------------
    {
        const char* domex_src = R"JS(
if (typeof DOMException === 'undefined') {
    function DOMException(message, name) {
        this.message = message || '';
        this.name = name || 'Error';
        this.code = 0;
    }
    DOMException.prototype = Object.create(Error.prototype);
    DOMException.prototype.constructor = DOMException;
    DOMException.prototype.toString = function() {
        return this.name + ': ' + this.message;
    };
}
)JS";
        JSValue de_ret = JS_Eval(ctx, domex_src, std::strlen(domex_src),
                                  "<domexception>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(de_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, de_ret);
    }

    // ------------------------------------------------------------------
    // AbortController / AbortSignal  (enhanced — full spec coverage)
    // ------------------------------------------------------------------
    {
        const char* abort_src = R"JS(
// ---- AbortSignal ----
function AbortSignal() {
    this.aborted = false;
    this.reason = undefined;
    this.onabort = null;
    this._listeners = []; // array of {fn, once}
}

// Internal helper: fire the abort event on this signal.
AbortSignal.prototype._fire = function() {
    var evt = { type: 'abort', target: this, currentTarget: this,
                bubbles: false, cancelable: false };
    if (typeof this.onabort === 'function') {
        try { this.onabort.call(this, evt); } catch(e) {}
    }
    var ls = this._listeners.slice();
    var toRemove = [];
    for (var i = 0; i < ls.length; i++) {
        try { ls[i].fn.call(this, evt); } catch(e) {}
        if (ls[i].once) toRemove.push(ls[i].fn);
    }
    var self = this;
    toRemove.forEach(function(fn) { self.removeEventListener('abort', fn); });
};

AbortSignal.prototype.addEventListener = function(type, fn, options) {
    if (type !== 'abort' || typeof fn !== 'function') return;
    var once = (options && typeof options === 'object') ? !!options.once : false;
    if (this.aborted) {
        var evt = { type: 'abort', target: this, currentTarget: this,
                    bubbles: false, cancelable: false };
        try { fn.call(this, evt); } catch(e) {}
        if (once) return;
        return; // non-once: already aborted once, no future fires
    }
    for (var i = 0; i < this._listeners.length; i++) {
        if (this._listeners[i].fn === fn && !this._listeners[i].once && !once) return;
    }
    this._listeners.push({ fn: fn, once: once });
};

AbortSignal.prototype.removeEventListener = function(type, fn) {
    if (type !== 'abort') return;
    this._listeners = this._listeners.filter(function(l) { return l.fn !== fn; });
};

AbortSignal.prototype.dispatchEvent = function(evt) {
    if (evt && evt.type === 'abort') this._fire();
    return true;
};

AbortSignal.prototype.throwIfAborted = function() {
    if (this.aborted) {
        throw (this.reason !== undefined ? this.reason
               : new DOMException('The operation was aborted.', 'AbortError'));
    }
};

// AbortSignal.abort(reason?) -- static: return an already-aborted signal
AbortSignal.abort = function(reason) {
    var s = new AbortSignal();
    s.aborted = true;
    s.reason = reason !== undefined ? reason
               : new DOMException('The operation was aborted.', 'AbortError');
    return s;
};

// AbortSignal.timeout(ms) -- static: returns a signal that aborts after ms ms.
// Uses setTimeout so the abort fires through the timer queue.
AbortSignal.timeout = function(ms) {
    var s = new AbortSignal();
    var tid = setTimeout(function() {
        if (!s.aborted) {
            s.aborted = true;
            s.reason = new DOMException('The operation timed out.', 'TimeoutError');
            s._fire();
        }
    }, typeof ms === 'number' ? ms : 0);
    s._timeoutId = tid;
    return s;
};

// AbortSignal.any(signals[]) -- static: aborts when any input signal aborts.
AbortSignal.any = function(signals) {
    var s = new AbortSignal();
    if (!signals || !signals.length) return s;
    for (var i = 0; i < signals.length; i++) {
        if (signals[i] && signals[i].aborted) {
            s.aborted = true;
            s.reason = signals[i].reason;
            return s;
        }
    }
    function onInputAbort(evt) {
        if (!s.aborted) {
            s.aborted = true;
            s.reason = evt.target ? evt.target.reason : undefined;
            s._fire();
        }
    }
    for (var j = 0; j < signals.length; j++) {
        if (signals[j]) signals[j].addEventListener('abort', onInputAbort);
    }
    return s;
};

// ---- AbortController ----
function AbortController() {
    this.signal = new AbortSignal();
}

AbortController.prototype.abort = function(reason) {
    if (!this.signal.aborted) {
        this.signal.aborted = true;
        this.signal.reason = reason !== undefined ? reason
                             : new DOMException('The operation was aborted.', 'AbortError');
        this.signal._fire();
    }
};
)JS";
        JSValue abort_ret = JS_Eval(ctx, abort_src, std::strlen(abort_src),
                                     "<abort>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(abort_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, abort_ret);
    }

    // ------------------------------------------------------------------
    // crypto.getRandomValues / crypto.randomUUID / crypto.subtle
    // ------------------------------------------------------------------
    {
        const char* crypto_src = R"JS(
if (typeof crypto === 'undefined') {
    var crypto = {};
    crypto.getRandomValues = function(arr) {
        for (var i = 0; i < arr.length; i++) arr[i] = Math.floor(Math.random() * 256);
        return arr;
    };
    crypto.randomUUID = function() {
        var h = '0123456789abcdef';
        var s = '';
        for (var i = 0; i < 36; i++) {
            if (i === 8 || i === 13 || i === 18 || i === 23) s += '-';
            else if (i === 14) s += '4';
            else if (i === 19) s += h[(Math.random() * 4 | 0) + 8];
            else s += h[Math.random() * 16 | 0];
        }
        return s;
    };
    crypto.subtle = {};
    globalThis.crypto = crypto;
}
)JS";
        JSValue crypto_ret = JS_Eval(ctx, crypto_src, std::strlen(crypto_src),
                                      "<crypto>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(crypto_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, crypto_ret);

        // Register native crypto.subtle.digest using CommonCrypto
#ifdef __APPLE__
        {
            JSValue crypto_obj = JS_GetPropertyStr(ctx, global, "crypto");
            if (!JS_IsUndefined(crypto_obj) && !JS_IsException(crypto_obj)) {
                JSValue subtle_obj = JS_GetPropertyStr(ctx, crypto_obj, "subtle");
                if (!JS_IsUndefined(subtle_obj) && !JS_IsException(subtle_obj)) {
                    JS_SetPropertyStr(ctx, subtle_obj, "digest",
                        JS_NewCFunction(ctx, js_crypto_subtle_digest, "digest", 2));
                    JS_FreeValue(ctx, subtle_obj);
                }
                JS_FreeValue(ctx, crypto_obj);
            }
        }
#endif

        // crypto.subtle stub methods (encrypt, decrypt, sign, verify, etc.)
        const char* subtle_stubs_src = R"JS(
(function() {
    if (typeof crypto !== 'undefined' && crypto.subtle) {
        var notSupported = function() {
            return Promise.reject(new Error('Not supported'));
        };
        var methods = ['encrypt','decrypt','sign','verify','generateKey',
                       'importKey','exportKey','deriveBits','deriveKey',
                       'wrapKey','unwrapKey'];
        for (var i = 0; i < methods.length; i++) {
            if (!crypto.subtle[methods[i]]) {
                crypto.subtle[methods[i]] = notSupported;
            }
        }
    }
})();
)JS";
        JSValue subtle_stubs_ret = JS_Eval(ctx, subtle_stubs_src,
                                            std::strlen(subtle_stubs_src),
                                            "<crypto-subtle-stubs>",
                                            JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(subtle_stubs_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, subtle_stubs_ret);
    }

    // ------------------------------------------------------------------
    // structuredClone polyfill (JSON round-trip)
    // ------------------------------------------------------------------
    {
        const char* clone_src = R"JS(
if (typeof structuredClone === 'undefined') {
    globalThis.structuredClone = function(obj) {
        return JSON.parse(JSON.stringify(obj));
    };
}
)JS";
        JSValue clone_ret = JS_Eval(ctx, clone_src, std::strlen(clone_src),
                                     "<structuredClone>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(clone_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, clone_ret);
    }

    // ------------------------------------------------------------------
    // navigator.serviceWorker (full stub)
    // ------------------------------------------------------------------
    {
        const char* sw_src = R"JS(
if (typeof navigator !== 'undefined' && navigator && !navigator.serviceWorker.register) {
    navigator.serviceWorker = {
        register: function() { return Promise.resolve({
            installing: null, waiting: null, active: null,
            scope: '/', unregister: function() { return Promise.resolve(true); },
            update: function() { return Promise.resolve(); },
            addEventListener: function() {},
            removeEventListener: function() {}
        }); },
        ready: Promise.resolve({ active: null }),
        controller: null,
        addEventListener: function() {},
        removeEventListener: function() {},
        getRegistrations: function() { return Promise.resolve([]); }
    };
}
)JS";
        JSValue sw_ret = JS_Eval(ctx, sw_src, std::strlen(sw_src),
                                  "<serviceWorker>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(sw_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, sw_ret);
    }

    // ------------------------------------------------------------------
    // BroadcastChannel stub
    // ------------------------------------------------------------------
    {
        const char* bc_src = R"JS(
if (typeof globalThis.BroadcastChannel === 'undefined') {
    globalThis.BroadcastChannel = function(name) {
        this.name = name;
        this.onmessage = null;
        this.onmessageerror = null;
    };
    BroadcastChannel.prototype.postMessage = function() {};
    BroadcastChannel.prototype.close = function() {};
    BroadcastChannel.prototype.addEventListener = function() {};
    BroadcastChannel.prototype.removeEventListener = function() {};
}
)JS";
        JSValue bc_ret = JS_Eval(ctx, bc_src, std::strlen(bc_src),
                                  "<BroadcastChannel>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(bc_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, bc_ret);
    }

    // ------------------------------------------------------------------
    // Notification API stub
    // ------------------------------------------------------------------
    {
        const char* notif_src = R"JS(
if (typeof globalThis.Notification === 'undefined') {
    globalThis.Notification = function(title, options) {
        this.title = title;
        this.body = (options && options.body) || '';
        this.icon = (options && options.icon) || '';
        this.tag = (options && options.tag) || '';
    };
    Notification.permission = 'default';
    Notification.requestPermission = function() { return Promise.resolve('denied'); };
}
)JS";
        JSValue notif_ret = JS_Eval(ctx, notif_src, std::strlen(notif_src),
                                     "<Notification>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(notif_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, notif_ret);
    }

    // ------------------------------------------------------------------
    // Fullscreen API stubs (requestFullscreen, exitFullscreen, etc.)
    // ------------------------------------------------------------------
    {
        const char* fullscreen_src = R"JS(
(function() {
    // Element.prototype.requestFullscreen
    if (typeof Element !== 'undefined' && Element.prototype) {
        if (!Element.prototype.requestFullscreen) {
            Element.prototype.requestFullscreen = function() { return Promise.resolve(); };
        }
        if (!Element.prototype.webkitRequestFullscreen) {
            Element.prototype.webkitRequestFullscreen = Element.prototype.requestFullscreen;
        }
    }
    // document.exitFullscreen and related properties
    if (typeof document !== 'undefined') {
        if (!document.exitFullscreen) {
            document.exitFullscreen = function() { return Promise.resolve(); };
        }
        if (document.fullscreenElement === undefined) {
            document.fullscreenElement = null;
        }
        if (document.fullscreenEnabled === undefined) {
            document.fullscreenEnabled = false;
        }
        if (!document.webkitExitFullscreen) {
            document.webkitExitFullscreen = document.exitFullscreen;
        }
        if (document.webkitFullscreenElement === undefined) {
            document.webkitFullscreenElement = null;
        }
        if (document.webkitFullscreenEnabled === undefined) {
            document.webkitFullscreenEnabled = false;
        }
    }
})();
)JS";
        JSValue fs_ret = JS_Eval(ctx, fullscreen_src, std::strlen(fullscreen_src),
                                  "<fullscreen>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(fs_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, fs_ret);
    }

    // ------------------------------------------------------------------
    // queueMicrotask guard (may already be set by install_window_bindings)
    // ------------------------------------------------------------------
    {
        const char* microtask_src = R"JS(
if (typeof queueMicrotask === 'undefined') {
    globalThis.queueMicrotask = function(fn) { fn(); };
}
)JS";
        JSValue mt_ret = JS_Eval(ctx, microtask_src, std::strlen(microtask_src),
                                  "<queueMicrotask>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(mt_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, mt_ret);
    }

    // ------------------------------------------------------------------
    // Node.lookupPrefix / Node.lookupNamespaceURI stubs
    // ------------------------------------------------------------------
    {
        const char* node_ns_src = R"JS(
        (function() {
            // Add lookupPrefix and lookupNamespaceURI to all node-like objects
            // These return null per spec for HTML documents (no namespace support)
            var origCreateElement = document.createElement;
            if (typeof document !== 'undefined') {
                // Patch onto the element prototype via __clever_element_proto
                var proto = Object.getPrototypeOf(document.createElement('div'));
                if (proto) {
                    proto.lookupPrefix = function(namespace) { return null; };
                    proto.lookupNamespaceURI = function(prefix) { return null; };
                }
                // Also add to document itself
                document.lookupPrefix = function(namespace) { return null; };
                document.lookupNamespaceURI = function(prefix) { return null; };
            }
        })();
)JS";
        JSValue ns_ret = JS_Eval(ctx, node_ns_src, std::strlen(node_ns_src),
                                  "<node-namespace>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ns_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ns_ret);
    }

    // ------------------------------------------------------------------
    // window.getMatchedCSSRules stub (non-standard, WebKit legacy)
    // ------------------------------------------------------------------
    {
        const char* matched_rules_src = R"JS(
        if (typeof window !== 'undefined') {
            window.getMatchedCSSRules = function(element, pseudo) { return []; };
        } else if (typeof globalThis !== 'undefined') {
            globalThis.getMatchedCSSRules = function(element, pseudo) { return []; };
        }
)JS";
        JSValue mr_ret = JS_Eval(ctx, matched_rules_src, std::strlen(matched_rules_src),
                                  "<matched-css-rules>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(mr_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, mr_ret);
    }

    // ------------------------------------------------------------------
    // MessageChannel / MessagePort stubs
    // ------------------------------------------------------------------
    {
        const char* mc_src = R"JS(
        (function() {
            function MessagePort() {
                this.onmessage = null;
                this.onmessageerror = null;
            }
            MessagePort.prototype.postMessage = function(msg) {};
            MessagePort.prototype.start = function() {};
            MessagePort.prototype.close = function() {};
            MessagePort.prototype.addEventListener = function() {};
            MessagePort.prototype.removeEventListener = function() {};

            function MessageChannel() {
                this.port1 = new MessagePort();
                this.port2 = new MessagePort();
            }

            globalThis.MessagePort = MessagePort;
            globalThis.MessageChannel = MessageChannel;
        })();
)JS";
        JSValue mc_ret = JS_Eval(ctx, mc_src, std::strlen(mc_src),
                                  "<message-channel>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(mc_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, mc_ret);
    }

    // ------------------------------------------------------------------
    // CSSRule stub — minimal rule objects in stylesheet.cssRules
    // ------------------------------------------------------------------
    {
        const char* cssrule_src = R"JS(
        (function() {
            // CSSRule constants
            var CSSRule = {
                STYLE_RULE: 1,
                CHARSET_RULE: 2,
                IMPORT_RULE: 3,
                MEDIA_RULE: 4,
                FONT_FACE_RULE: 5,
                PAGE_RULE: 6,
                KEYFRAMES_RULE: 7,
                KEYFRAME_RULE: 8,
                SUPPORTS_RULE: 12,
                NAMESPACE_RULE: 10
            };
            globalThis.CSSRule = CSSRule;

            // Enhance CSSStyleSheet.insertRule to produce proper CSSRule objects
            if (typeof CSSStyleSheet !== 'undefined') {
                var origInsert = CSSStyleSheet.prototype.insertRule;
                CSSStyleSheet.prototype.insertRule = function(ruleText, index) {
                    if (index === undefined) index = 0;
                    // Parse selector from rule text
                    var braceIdx = ruleText.indexOf('{');
                    var selector = braceIdx >= 0 ? ruleText.substring(0, braceIdx).trim() : '';
                    var ruleObj = {
                        type: 1, // STYLE_RULE
                        selectorText: selector,
                        cssText: ruleText,
                        style: {},
                        parentStyleSheet: this,
                        parentRule: null
                    };
                    this.cssRules.splice(index, 0, ruleObj);
                    this.rules = this.cssRules;
                    return index;
                };
            }
        })();
)JS";
        JSValue cr_ret = JS_Eval(ctx, cssrule_src, std::strlen(cssrule_src),
                                  "<cssrule>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(cr_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, cr_ret);
    }

    // ------------------------------------------------------------------
    // Element.slot getter/setter
    // ------------------------------------------------------------------
    {
        const char* slot_src = R"JS(
        (function() {
            if (typeof document !== 'undefined') {
                var proto = Object.getPrototypeOf(document.createElement('div'));
                if (proto) {
                    Object.defineProperty(proto, 'slot', {
                        get: function() {
                            return this.getAttribute('slot') || '';
                        },
                        set: function(val) {
                            this.setAttribute('slot', val);
                        },
                        configurable: true
                    });
                }
            }
        })();
)JS";
        JSValue sl_ret = JS_Eval(ctx, slot_src, std::strlen(slot_src),
                                  "<element-slot>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(sl_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, sl_ret);
    }

    // ------------------------------------------------------------------
    // IndexedDB stub
    // ------------------------------------------------------------------
    {
        const char* indexeddb_src = R"JS(
        (function() {
            if (typeof indexedDB !== 'undefined') return;

            function IDBRequest() {
                this.result = null;
                this.error = null;
                this.source = null;
                this.transaction = null;
                this.readyState = 'pending';
                this.onsuccess = null;
                this.onerror = null;
            }

            function IDBOpenDBRequest() {
                IDBRequest.call(this);
                this.onblocked = null;
                this.onupgradeneeded = null;
            }

            function IDBDatabase(name, version) {
                this.name = name;
                this.version = version || 1;
                this.objectStoreNames = [];
                this.onabort = null;
                this.onerror = null;
                this.onversionchange = null;
            }
            IDBDatabase.prototype.close = function() {};
            IDBDatabase.prototype.createObjectStore = function(name) {
                return { name: name, keyPath: null, indexNames: [],
                    put: function() { return new IDBRequest(); },
                    add: function() { return new IDBRequest(); },
                    get: function() { return new IDBRequest(); },
                    delete: function() { return new IDBRequest(); },
                    clear: function() { return new IDBRequest(); },
                    count: function() { var r = new IDBRequest(); r.result = 0; return r; },
                    createIndex: function() { return {}; },
                    deleteIndex: function() {},
                    getAll: function() { var r = new IDBRequest(); r.result = []; return r; },
                    getAllKeys: function() { var r = new IDBRequest(); r.result = []; return r; },
                    openCursor: function() { return new IDBRequest(); },
                    openKeyCursor: function() { return new IDBRequest(); },
                    index: function() { return { get: function() { return new IDBRequest(); } }; }
                };
            };
            IDBDatabase.prototype.deleteObjectStore = function() {};
            IDBDatabase.prototype.transaction = function(stores, mode) {
                return {
                    objectStore: function(name) { return IDBDatabase.prototype.createObjectStore(name); },
                    abort: function() {},
                    oncomplete: null, onerror: null, onabort: null,
                    mode: mode || 'readonly'
                };
            };

            var _defer = typeof setTimeout === 'function'
                ? function(fn) { setTimeout(fn, 0); }
                : function(fn) { fn(); };

            var idb = {
                open: function(name, version) {
                    var req = new IDBOpenDBRequest();
                    req.readyState = 'done';
                    req.result = new IDBDatabase(name, version);
                    // Fire onsuccess asynchronously if possible
                    _defer(function() {
                        if (req.onupgradeneeded) {
                            req.onupgradeneeded({ target: req, oldVersion: 0, newVersion: version || 1 });
                        }
                        if (req.onsuccess) req.onsuccess({ target: req });
                    });
                    return req;
                },
                deleteDatabase: function(name) {
                    var req = new IDBRequest();
                    req.readyState = 'done';
                    _defer(function() { if (req.onsuccess) req.onsuccess({ target: req }); });
                    return req;
                },
                cmp: function(a, b) { return a < b ? -1 : a > b ? 1 : 0; },
                databases: function() { return Promise.resolve([]); }
            };

            globalThis.indexedDB = idb;
            globalThis.IDBDatabase = IDBDatabase;
            globalThis.IDBRequest = IDBRequest;
            globalThis.IDBOpenDBRequest = IDBOpenDBRequest;
            globalThis.IDBKeyRange = {
                only: function(v) { return { lower: v, upper: v, lowerOpen: false, upperOpen: false, includes: function(k) { return k === v; } }; },
                lowerBound: function(v, open) { return { lower: v, upper: undefined, lowerOpen: !!open, upperOpen: true }; },
                upperBound: function(v, open) { return { lower: undefined, upper: v, lowerOpen: true, upperOpen: !!open }; },
                bound: function(l, u, lo, uo) { return { lower: l, upper: u, lowerOpen: !!lo, upperOpen: !!uo }; }
            };
            globalThis.IDBTransaction = function() {};
            globalThis.IDBObjectStore = function() {};
            globalThis.IDBIndex = function() {};
            globalThis.IDBCursor = function() {};
            globalThis.IDBCursorWithValue = function() {};
        })();
)JS";
        JSValue idb_ret = JS_Eval(ctx, indexeddb_src, std::strlen(indexeddb_src),
                                  "<indexeddb>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(idb_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, idb_ret);
    }

    // ------------------------------------------------------------------
    // Streams API stubs (ReadableStream, WritableStream, TransformStream)
    // ------------------------------------------------------------------
    {
        const char* streams_src = R"JS(
        (function() {
            if (typeof ReadableStream !== 'undefined') return;

            globalThis.ReadableStream = function(underlyingSource, strategy) {
                this.locked = false;
                this._reader = null;
            };
            ReadableStream.prototype.getReader = function() {
                this.locked = true;
                var stream = this;
                return {
                    read: function() { return Promise.resolve({ done: true, value: undefined }); },
                    releaseLock: function() { stream.locked = false; },
                    cancel: function() { return Promise.resolve(); },
                    closed: Promise.resolve()
                };
            };
            ReadableStream.prototype.cancel = function() { return Promise.resolve(); };
            ReadableStream.prototype.pipeTo = function(dest) { return Promise.resolve(); };
            ReadableStream.prototype.pipeThrough = function(transform) { return transform.readable || new ReadableStream(); };
            ReadableStream.prototype.tee = function() { return [new ReadableStream(), new ReadableStream()]; };

            globalThis.WritableStream = function() { this.locked = false; };
            WritableStream.prototype.getWriter = function() {
                this.locked = true;
                var stream = this;
                return {
                    write: function() { return Promise.resolve(); },
                    close: function() { return Promise.resolve(); },
                    abort: function() { return Promise.resolve(); },
                    releaseLock: function() { stream.locked = false; },
                    ready: Promise.resolve(),
                    closed: Promise.resolve(),
                    desiredSize: 1
                };
            };
            WritableStream.prototype.abort = function() { return Promise.resolve(); };
            WritableStream.prototype.close = function() { return Promise.resolve(); };

            globalThis.TransformStream = function() {
                this.readable = new ReadableStream();
                this.writable = new WritableStream();
            };
        })();
)JS";
        JSValue streams_ret = JS_Eval(ctx, streams_src, std::strlen(streams_src),
                                  "<streams>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(streams_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, streams_ret);
    }

    // ------------------------------------------------------------------
    // Cache API stubs (CacheStorage, Cache, globalThis.caches)
    // ------------------------------------------------------------------
    {
        const char* cache_src = R"JS(
(function() {
    if (typeof caches !== 'undefined') return;
    function CacheStorage() {}
    CacheStorage.prototype.open = function(name) { return Promise.resolve(new Cache(name)); };
    CacheStorage.prototype.has = function(name) { return Promise.resolve(false); };
    CacheStorage.prototype.delete = function(name) { return Promise.resolve(false); };
    CacheStorage.prototype.keys = function() { return Promise.resolve([]); };
    CacheStorage.prototype.match = function() { return Promise.resolve(undefined); };

    function Cache(name) { this._name = name; this._entries = []; }
    Cache.prototype.match = function() { return Promise.resolve(undefined); };
    Cache.prototype.matchAll = function() { return Promise.resolve([]); };
    Cache.prototype.add = function() { return Promise.resolve(); };
    Cache.prototype.addAll = function() { return Promise.resolve(); };
    Cache.prototype.put = function() { return Promise.resolve(); };
    Cache.prototype.delete = function() { return Promise.resolve(false); };
    Cache.prototype.keys = function() { return Promise.resolve([]); };

    globalThis.caches = new CacheStorage();
    globalThis.CacheStorage = CacheStorage;
    globalThis.Cache = Cache;
})();
)JS";
        JSValue cache_ret = JS_Eval(ctx, cache_src, std::strlen(cache_src),
                                     "<cache-api>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(cache_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, cache_ret);
    }

    // ------------------------------------------------------------------
    // Web Animations API enhancements (Animation, KeyframeEffect,
    // DocumentTimeline, document.timeline, document.getAnimations)
    // ------------------------------------------------------------------
    {
        const char* anim_src = R"JS(
(function() {
    if (typeof Animation !== 'undefined') return;
    globalThis.Animation = function(effect, timeline) {
        this.effect = effect || null;
        this.timeline = timeline || null;
        this.playState = 'idle';
        this.currentTime = null;
        this.playbackRate = 1;
        this.id = '';
        this.onfinish = null;
        this.oncancel = null;
        this.onremove = null;
        this.finished = Promise.resolve(this);
        this.ready = Promise.resolve(this);
    };
    Animation.prototype.play = function() { this.playState = 'running'; };
    Animation.prototype.pause = function() { this.playState = 'paused'; };
    Animation.prototype.cancel = function() { this.playState = 'idle'; this.currentTime = null; };
    Animation.prototype.finish = function() { this.playState = 'finished'; };
    Animation.prototype.reverse = function() {};
    Animation.prototype.updatePlaybackRate = function(rate) { this.playbackRate = rate; };
    Animation.prototype.commitStyles = function() {};
    Animation.prototype.persist = function() {};

    globalThis.KeyframeEffect = function(target, keyframes, options) {
        this.target = target;
        this.composite = 'replace';
        this.pseudoElement = null;
    };
    KeyframeEffect.prototype.getKeyframes = function() { return []; };
    KeyframeEffect.prototype.setKeyframes = function() {};
    KeyframeEffect.prototype.getComputedTiming = function() {
        return { duration: 0, fill: 'auto', delay: 0, endDelay: 0, direction: 'normal',
                 easing: 'linear', iterations: 1, iterationStart: 0, activeDuration: 0,
                 localTime: null, progress: null, currentIteration: null };
    };

    globalThis.DocumentTimeline = function() { this.currentTime = performance.now(); };

    if (typeof document !== 'undefined' && !document.timeline) {
        document.timeline = new DocumentTimeline();
    }
    if (typeof document !== 'undefined' && !document.getAnimations) {
        document.getAnimations = function() { return []; };
    }
})();
)JS";
        JSValue anim_ret = JS_Eval(ctx, anim_src, std::strlen(anim_src),
                                    "<web-animations>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(anim_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, anim_ret);
    }

    // ------------------------------------------------------------------
    // Intersection Observer V2 enhancements (trackVisibility/delay)
    // ------------------------------------------------------------------
    {
        const char* iov2_src = R"JS(
(function() {
    if (typeof IntersectionObserver === 'undefined') return;
    // Ensure the constructor accepts V2 options (trackVisibility, delay)
    // without throwing. The existing constructor already ignores unknown
    // options, but V2 entries should expose isVisible = false by default.
    // Patch observe() to silently accept trackVisibility in options.
    var origObserve = IntersectionObserver.prototype.observe;
    if (origObserve) {
        IntersectionObserver.prototype.observe = function(target, options) {
            // V2 options (trackVisibility, delay) are accepted but ignored
            return origObserve.call(this, target);
        };
    }
})();
)JS";
        JSValue iov2_ret = JS_Eval(ctx, iov2_src, std::strlen(iov2_src),
                                    "<intersection-observer-v2>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(iov2_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, iov2_ret);
    }

    // ------------------------------------------------------------------
    // PerformanceEntry / PerformanceResourceTiming / PerformanceMark /
    // PerformanceMeasure / PerformanceNavigation / PerformanceNavigationTiming
    // ------------------------------------------------------------------
    {
        const char* perf_entry_src = R"JS(
(function() {
    if (typeof PerformanceEntry !== 'undefined') return;
    globalThis.PerformanceEntry = function() {
        this.name = ''; this.entryType = ''; this.startTime = 0; this.duration = 0;
    };
    PerformanceEntry.prototype.toJSON = function() {
        return { name: this.name, entryType: this.entryType, startTime: this.startTime, duration: this.duration };
    };
    globalThis.PerformanceResourceTiming = function() {
        PerformanceEntry.call(this);
        this.initiatorType = ''; this.nextHopProtocol = '';
        this.workerStart = 0; this.redirectStart = 0; this.redirectEnd = 0;
        this.fetchStart = 0; this.domainLookupStart = 0; this.domainLookupEnd = 0;
        this.connectStart = 0; this.connectEnd = 0; this.secureConnectionStart = 0;
        this.requestStart = 0; this.responseStart = 0; this.responseEnd = 0;
        this.transferSize = 0; this.encodedBodySize = 0; this.decodedBodySize = 0;
    };
    globalThis.PerformanceMark = function(name) {
        PerformanceEntry.call(this);
        this.name = name; this.entryType = 'mark'; this.startTime = performance.now();
    };
    globalThis.PerformanceMeasure = function(name) {
        PerformanceEntry.call(this);
        this.name = name; this.entryType = 'measure';
    };
    globalThis.PerformanceNavigation = function() { this.type = 0; this.redirectCount = 0; };
    globalThis.PerformanceNavigationTiming = function() { PerformanceEntry.call(this); this.entryType = 'navigation'; };
})();
)JS";
        JSValue pe_ret = JS_Eval(ctx, perf_entry_src, std::strlen(perf_entry_src),
                                  "<performance-entry>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(pe_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, pe_ret);
    }

    // ------------------------------------------------------------------
    // HTMLMediaElement / HTMLVideoElement / HTMLAudioElement stubs
    // ------------------------------------------------------------------
    {
        const char* media_src = R"JS(
(function() {
    if (typeof globalThis.HTMLMediaElement !== 'undefined') return;
    function HTMLMediaElement() {
        this.src=''; this.currentSrc=''; this.currentTime=0; this.duration=NaN;
        this.paused=true; this.ended=false; this.muted=false; this.volume=1;
        this.playbackRate=1; this.defaultPlaybackRate=1; this.readyState=0;
        this.networkState=0; this.error=null;
        this.buffered={length:0,start:function(){return 0;},end:function(){return 0;}};
        this.seekable={length:0,start:function(){return 0;},end:function(){return 0;}};
        this.played={length:0,start:function(){return 0;},end:function(){return 0;}};
        this.autoplay=false; this.loop=false; this.controls=false;
        this.preload='auto'; this.crossOrigin=null;
    }
    HTMLMediaElement.prototype.play = function() { return Promise.resolve(); };
    HTMLMediaElement.prototype.pause = function() {};
    HTMLMediaElement.prototype.load = function() {};
    HTMLMediaElement.prototype.canPlayType = function(t) { return ''; };
    HTMLMediaElement.prototype.addTextTrack = function(k,l,lang) {
        return {kind:k,label:l||'',language:lang||'',mode:'disabled',cues:null,addCue:function(){},removeCue:function(){}};
    };
    HTMLMediaElement.prototype.addEventListener = function() {};
    HTMLMediaElement.prototype.removeEventListener = function() {};

    function HTMLVideoElement() {
        HTMLMediaElement.call(this);
        this.width=0; this.height=0; this.videoWidth=0; this.videoHeight=0; this.poster='';
    }
    HTMLVideoElement.prototype = Object.create(HTMLMediaElement.prototype);
    HTMLVideoElement.prototype.constructor = HTMLVideoElement;

    function HTMLAudioElement() {
        HTMLMediaElement.call(this);
    }
    HTMLAudioElement.prototype = Object.create(HTMLMediaElement.prototype);
    HTMLAudioElement.prototype.constructor = HTMLAudioElement;

    globalThis.HTMLMediaElement = HTMLMediaElement;
    globalThis.HTMLVideoElement = HTMLVideoElement;
    globalThis.HTMLAudioElement = HTMLAudioElement;
    globalThis.Audio = HTMLAudioElement;
})();
)JS";
        JSValue media_ret = JS_Eval(ctx, media_src, std::strlen(media_src),
                                     "<media-element>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(media_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, media_ret);

        // ---- Image / HTMLImageElement constructor ----
        static const char* image_src = R"JS(
(function() {
    if (typeof globalThis.Image !== 'undefined') return;
    function HTMLImageElement(width, height) {
        this.tagName = 'IMG';
        this.nodeName = 'IMG';
        this.nodeType = 1;
        this.src = '';
        this.alt = '';
        this.crossOrigin = null;
        this.naturalWidth = 0;
        this.naturalHeight = 0;
        this.complete = false;
        this.loading = 'auto';
        this.decoding = 'auto';
        if (typeof width === 'number') this.width = width;
        else this.width = 0;
        if (typeof height === 'number') this.height = height;
        else this.height = 0;
        this.onload = null;
        this.onerror = null;
        this._listeners = {};
    }
    HTMLImageElement.prototype.addEventListener = function(type, fn) {
        if (!this._listeners[type]) this._listeners[type] = [];
        this._listeners[type].push(fn);
    };
    HTMLImageElement.prototype.removeEventListener = function(type, fn) {
        if (!this._listeners[type]) return;
        this._listeners[type] = this._listeners[type].filter(function(f) { return f !== fn; });
    };
    HTMLImageElement.prototype.decode = function() {
        return Promise.resolve();
    };
    HTMLImageElement.prototype.getAttribute = function(name) {
        return this[name] !== undefined ? String(this[name]) : null;
    };
    HTMLImageElement.prototype.setAttribute = function(name, value) {
        this[name] = value;
    };
    globalThis.HTMLImageElement = HTMLImageElement;
    globalThis.Image = HTMLImageElement;
})();
)JS";
        JSValue image_ret = JS_Eval(ctx, image_src, std::strlen(image_src),
                                      "<image-element>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(image_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, image_ret);
    }

    // ------------------------------------------------------------------
    // Web Audio API stubs (AudioContext)
    // ------------------------------------------------------------------
    {
        const char* audio_ctx_src = R"JS(
(function() {
    if (typeof globalThis.AudioContext !== 'undefined') return;
    function AudioContext() {
        this.state='suspended'; this.sampleRate=44100; this.currentTime=0;
        this.destination={numberOfInputs:1,numberOfOutputs:0,channelCount:2};
        this.listener={positionX:{value:0},positionY:{value:0},positionZ:{value:0}};
    }
    AudioContext.prototype.createGain = function() {
        return {gain:{value:1,setValueAtTime:function(){},linearRampToValueAtTime:function(){},exponentialRampToValueAtTime:function(){}},connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.createOscillator = function() {
        return {frequency:{value:440,setValueAtTime:function(){}},type:'sine',connect:function(){return this;},disconnect:function(){},start:function(){},stop:function(){}};
    };
    AudioContext.prototype.createBufferSource = function() {
        return {buffer:null,loop:false,playbackRate:{value:1},connect:function(){return this;},disconnect:function(){},start:function(){},stop:function(){}};
    };
    AudioContext.prototype.createAnalyser = function() {
        return {fftSize:2048,frequencyBinCount:1024,connect:function(){return this;},disconnect:function(){},getByteFrequencyData:function(){},getFloatFrequencyData:function(){},getByteTimeDomainData:function(){}};
    };
    AudioContext.prototype.createBiquadFilter = function() {
        return {type:'lowpass',frequency:{value:350},Q:{value:1},gain:{value:0},connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.createDynamicsCompressor = function() {
        return {threshold:{value:-24},knee:{value:30},ratio:{value:12},attack:{value:0.003},release:{value:0.25},connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.createDelay = function(max) {
        return {delayTime:{value:0},connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.createConvolver = function() {
        return {buffer:null,normalize:true,connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.createPanner = function() {
        return {panningModel:'equalpower',distanceModel:'inverse',connect:function(){return this;},disconnect:function(){}};
    };
    AudioContext.prototype.decodeAudioData = function(buf) {
        var sr = this.sampleRate;
        return Promise.resolve({duration:0,length:0,numberOfChannels:1,sampleRate:sr,getChannelData:function(){return new Float32Array(0);}});
    };
    AudioContext.prototype.resume = function() { this.state='running'; return Promise.resolve(); };
    AudioContext.prototype.suspend = function() { this.state='suspended'; return Promise.resolve(); };
    AudioContext.prototype.close = function() { this.state='closed'; return Promise.resolve(); };

    globalThis.AudioContext = AudioContext;
    globalThis.webkitAudioContext = AudioContext;
})();
)JS";
        JSValue ac_ret = JS_Eval(ctx, audio_ctx_src, std::strlen(audio_ctx_src),
                                  "<audio-context>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ac_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ac_ret);
    }

    // ------------------------------------------------------------------
    // navigator.locks (Web Locks API)
    // ------------------------------------------------------------------
    {
        const char* locks_src = R"JS(
(function() {
    if (typeof navigator !== 'undefined' && !navigator.locks) {
        navigator.locks = {
            request: function(name, opts, cb) {
                if (typeof opts === 'function') { cb = opts; opts = {}; }
                return Promise.resolve(cb({name:name,mode:(opts&&opts.mode)||'exclusive'}));
            },
            query: function() { return Promise.resolve({held:[],pending:[]}); }
        };
    }
})();
)JS";
        JSValue locks_ret = JS_Eval(ctx, locks_src, std::strlen(locks_src),
                                     "<navigator-locks>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(locks_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, locks_ret);
    }

    // ------------------------------------------------------------------
    // Gamepad API stub
    // ------------------------------------------------------------------
    {
        const char* gamepad_src = R"JS(
(function() {
    if (typeof navigator !== 'undefined' && !navigator.getGamepads) {
        navigator.getGamepads = function() { return [null,null,null,null]; };
    }
})();
)JS";
        JSValue gp_ret = JS_Eval(ctx, gamepad_src, std::strlen(gamepad_src),
                                  "<gamepad>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(gp_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, gp_ret);
    }

    // ------------------------------------------------------------------
    // Credential Management API stub
    // ------------------------------------------------------------------
    {
        const char* cred_src = R"JS(
(function() {
    if (typeof navigator !== 'undefined' && !navigator.credentials) {
        navigator.credentials = {
            get: function(opts) { return Promise.resolve(null); },
            store: function(cred) { return Promise.resolve(cred); },
            create: function(opts) { return Promise.resolve(null); },
            preventSilentAccess: function() { return Promise.resolve(); }
        };
    }
})();
)JS";
        JSValue cred_ret = JS_Eval(ctx, cred_src, std::strlen(cred_src),
                                    "<credentials>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(cred_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, cred_ret);
    }

    // ------------------------------------------------------------------
    // ReportingObserver stub
    // ------------------------------------------------------------------
    {
        const char* reporting_src = R"JS(
(function() {
    if (typeof globalThis.ReportingObserver !== 'undefined') return;
    function ReportingObserver(cb, opts) { this._cb = cb; }
    ReportingObserver.prototype.observe = function() {};
    ReportingObserver.prototype.disconnect = function() {};
    ReportingObserver.prototype.takeRecords = function() { return []; };
    globalThis.ReportingObserver = ReportingObserver;
})();
)JS";
        JSValue ro_ret = JS_Eval(ctx, reporting_src, std::strlen(reporting_src),
                                  "<reporting-observer>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ro_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ro_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 253: Touch Events (Touch, TouchEvent enhanced)
    // ------------------------------------------------------------------
    {
        const char* touch_src = R"JS(
(function(){
if(typeof globalThis.Touch !== 'undefined') return;
function Touch(init) {
    this.identifier = init && init.identifier || 0;
    this.target = init && init.target || null;
    this.screenX = init && init.screenX || 0;
    this.screenY = init && init.screenY || 0;
    this.clientX = init && init.clientX || 0;
    this.clientY = init && init.clientY || 0;
    this.pageX = init && init.pageX || 0;
    this.pageY = init && init.pageY || 0;
    this.radiusX = init && init.radiusX || 0;
    this.radiusY = init && init.radiusY || 0;
    this.rotationAngle = init && init.rotationAngle || 0;
    this.force = init && init.force || 0;
}
globalThis.Touch = Touch;
// Enhanced TouchEvent with modifier keys and touch lists from init
var _OrigTouchEvent = globalThis.TouchEvent;
function EnhancedTouchEvent(type, init) {
    var evt;
    if (_OrigTouchEvent) {
        try { evt = new _OrigTouchEvent(type, init); } catch(e) { evt = {type:type}; }
    } else { evt = {type:type}; }
    evt.touches = init && init.touches || [];
    evt.targetTouches = init && init.targetTouches || [];
    evt.changedTouches = init && init.changedTouches || [];
    evt.altKey = init && init.altKey || false;
    evt.metaKey = init && init.metaKey || false;
    evt.ctrlKey = init && init.ctrlKey || false;
    evt.shiftKey = init && init.shiftKey || false;
    return evt;
}
globalThis.TouchEvent = EnhancedTouchEvent;
})();
)JS";
        JSValue t_ret = JS_Eval(ctx, touch_src, std::strlen(touch_src),
                                 "<cycle253_touch_events>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(t_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, t_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 253: Drag and Drop API (DataTransfer enhanced)
    // ------------------------------------------------------------------
    {
        const char* dnd_src = R"JS(
(function(){
if(typeof globalThis.DataTransfer !== 'undefined') return;
function DataTransferItemList() { this._items = []; this.length = 0; }
DataTransferItemList.prototype.add = function(data, type) {
    var i = {kind:typeof data==='string'?'string':'file',type:type||'',getAsString:function(cb){cb(data)},getAsFile:function(){return null}};
    this._items.push(i); this.length++; return i;
};
DataTransferItemList.prototype.remove = function(idx) { this._items.splice(idx,1); this.length--; };
DataTransferItemList.prototype.clear = function() { this._items=[]; this.length=0; };

function DataTransfer() {
    this.dropEffect = 'none';
    this.effectAllowed = 'uninitialized';
    this.items = new DataTransferItemList();
    this.types = [];
    this.files = [];
    this._data = {};
}
DataTransfer.prototype.setData = function(format, data) {
    this._data[format] = data;
    if(this.types.indexOf(format)===-1) this.types.push(format);
};
DataTransfer.prototype.getData = function(format) { return this._data[format] || ''; };
DataTransfer.prototype.clearData = function(format) {
    if(format){delete this._data[format]; this.types=this.types.filter(function(t){return t!==format});}
    else{this._data={};this.types=[];}
};
DataTransfer.prototype.setDragImage = function(img, x, y) {};
globalThis.DataTransfer = DataTransfer;
globalThis.DataTransferItemList = DataTransferItemList;
})();
)JS";
        JSValue dnd_ret = JS_Eval(ctx, dnd_src, std::strlen(dnd_src),
                                   "<cycle253_drag_drop>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(dnd_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, dnd_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 253: Web Speech API stubs
    // ------------------------------------------------------------------
    {
        const char* speech_src = R"JS(
(function(){
if(typeof globalThis.SpeechRecognition !== 'undefined') return;
function SpeechRecognition() {
    this.lang=''; this.continuous=false; this.interimResults=false;
    this.maxAlternatives=1; this.grammars=null;
    this.onaudiostart=null; this.onsoundstart=null; this.onspeechstart=null;
    this.onspeechend=null; this.onsoundend=null; this.onaudioend=null;
    this.onresult=null; this.onnomatch=null; this.onerror=null;
    this.onstart=null; this.onend=null;
}
SpeechRecognition.prototype.start = function() {};
SpeechRecognition.prototype.stop = function() {};
SpeechRecognition.prototype.abort = function() {};
globalThis.SpeechRecognition = SpeechRecognition;
globalThis.webkitSpeechRecognition = SpeechRecognition;

function SpeechSynthesisUtterance(text) {
    this.text=text||''; this.lang=''; this.voice=null;
    this.volume=1; this.rate=1; this.pitch=1;
    this.onstart=null; this.onend=null; this.onerror=null;
    this.onpause=null; this.onresume=null; this.onboundary=null; this.onmark=null;
}
globalThis.SpeechSynthesisUtterance = SpeechSynthesisUtterance;

globalThis.speechSynthesis = {
    speaking: false,
    pending: false,
    paused: false,
    speak: function(u) {},
    cancel: function() {},
    pause: function() {},
    resume: function() {},
    getVoices: function() { return []; },
    onvoiceschanged: null
};
})();
)JS";
        JSValue sp_ret = JS_Eval(ctx, speech_src, std::strlen(speech_src),
                                  "<cycle253_web_speech>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(sp_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, sp_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 253: Clipboard API (full) + ClipboardItem
    // ------------------------------------------------------------------
    {
        const char* clip_src = R"JS(
(function(){
// Enhance navigator.clipboard with write/read
if(typeof navigator !== 'undefined' && navigator.clipboard) {
    if(!navigator.clipboard.writeText) {
        navigator.clipboard.writeText = function(text) { return Promise.resolve(); };
    }
    if(!navigator.clipboard.readText) {
        navigator.clipboard.readText = function() { return Promise.resolve(''); };
    }
    if(!navigator.clipboard.write) {
        navigator.clipboard.write = function(items) { return Promise.resolve(); };
    }
    if(!navigator.clipboard.read) {
        navigator.clipboard.read = function() { return Promise.resolve([]); };
    }
}
// ClipboardItem constructor
if(typeof globalThis.ClipboardItem === 'undefined') {
    function ClipboardItem(items) {
        this._items = items || {};
        this.types = Object.keys(this._items);
    }
    ClipboardItem.prototype.getType = function(type) {
        var v = this._items[type];
        return Promise.resolve(typeof Blob !== 'undefined' && v instanceof Blob ? v : new Blob([v||''],{type:type}));
    };
    globalThis.ClipboardItem = ClipboardItem;
}
})();
)JS";
        JSValue cl_ret = JS_Eval(ctx, clip_src, std::strlen(clip_src),
                                  "<cycle253_clipboard>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(cl_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, cl_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 254: WebRTC stubs (RTCPeerConnection, MediaStream, MediaStreamTrack)
    // ------------------------------------------------------------------
    {
        const char* webrtc_src = R"JS(
(function(){
if(typeof globalThis.RTCPeerConnection !== 'undefined') return;
class RTCSessionDescription { constructor(init){this.type=init&&init.type||'';this.sdp=init&&init.sdp||'';} toJSON(){return{type:this.type,sdp:this.sdp};} }
class RTCIceCandidate { constructor(init){this.candidate=init&&init.candidate||'';this.sdpMid=init&&init.sdpMid||null;this.sdpMLineIndex=init&&init.sdpMLineIndex||null;this.usernameFragment=init&&init.usernameFragment||null;} toJSON(){return{candidate:this.candidate,sdpMid:this.sdpMid,sdpMLineIndex:this.sdpMLineIndex};} }
class RTCPeerConnection {
  constructor(config) { this.localDescription=null; this.remoteDescription=null; this.signalingState='stable'; this.iceConnectionState='new'; this.iceGatheringState='new'; this.connectionState='new'; this.onicecandidate=null; this.ontrack=null; this.ondatachannel=null; this.onconnectionstatechange=null; this.oniceconnectionstatechange=null; this.onicegatheringstatechange=null; this.onsignalingstatechange=null; this.onnegotiationneeded=null; this._config=config||{}; }
  createOffer(opts) { return Promise.resolve(new RTCSessionDescription({type:'offer',sdp:''})); }
  createAnswer(opts) { return Promise.resolve(new RTCSessionDescription({type:'answer',sdp:''})); }
  setLocalDescription(desc) { this.localDescription=desc; return Promise.resolve(); }
  setRemoteDescription(desc) { this.remoteDescription=desc; return Promise.resolve(); }
  addIceCandidate(c) { return Promise.resolve(); }
  createDataChannel(label,opts) { return {label:label,readyState:'connecting',send:function(){},close:function(){},onopen:null,onmessage:null,onclose:null,onerror:null,bufferedAmount:0}; }
  addTrack(track,stream) { return {track:track,sender:null}; }
  removeTrack(sender) {}
  getStats() { return Promise.resolve(new Map()); }
  getSenders() { return []; }
  getReceivers() { return []; }
  getTransceivers() { return []; }
  close() { this.connectionState='closed'; this.signalingState='closed'; }
}
globalThis.RTCPeerConnection = RTCPeerConnection;
globalThis.RTCSessionDescription = RTCSessionDescription;
globalThis.RTCIceCandidate = RTCIceCandidate;

class MediaStream {
  constructor(tracks) { this.id=Math.random().toString(36).substr(2,9); this._tracks=tracks||[]; this.active=this._tracks.length>0; }
  getTracks() { return this._tracks.slice(); }
  getAudioTracks() { return this._tracks.filter(function(t){return t.kind==='audio';}); }
  getVideoTracks() { return this._tracks.filter(function(t){return t.kind==='video';}); }
  addTrack(t) { this._tracks.push(t); }
  removeTrack(t) { this._tracks=this._tracks.filter(function(x){return x!==t;}); }
  clone() { return new MediaStream(this._tracks.slice()); }
}
globalThis.MediaStream = MediaStream;

class MediaStreamTrack {
  constructor(kind) { this.kind=kind||'audio'; this.id=Math.random().toString(36).substr(2,9); this.label=''; this.enabled=true; this.muted=false; this.readyState='live'; this.onended=null; this.onmute=null; this.onunmute=null; }
  stop() { this.readyState='ended'; }
  clone() { var c=new MediaStreamTrack(this.kind); c.label=this.label; return c; }
  getSettings() { return {}; }
  getCapabilities() { return {}; }
  getConstraints() { return {}; }
  applyConstraints(c) { return Promise.resolve(); }
}
globalThis.MediaStreamTrack = MediaStreamTrack;
})();
)JS";
        JSValue rtc_ret = JS_Eval(ctx, webrtc_src, std::strlen(webrtc_src),
                                  "<cycle254_webrtc>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(rtc_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, rtc_ret);
    }

    // ------------------------------------------------------------------
    // Cycle 254: Payment Request API stub
    // ------------------------------------------------------------------
    {
        const char* pay_src = R"JS(
(function(){
if(typeof globalThis.PaymentRequest !== 'undefined') return;
class PaymentRequest {
  constructor(methods,details,opts) { this.id=Math.random().toString(36).substr(2,9); this._methods=methods; this._details=details; this.onpaymentmethodchange=null; this.onshippingaddresschange=null; this.onshippingoptionchange=null; }
  show() { return Promise.reject(new DOMException('NotSupportedError','Payment not supported')); }
  abort() { return Promise.resolve(); }
  canMakePayment() { return Promise.resolve(false); }
}
globalThis.PaymentRequest = PaymentRequest;
})();
)JS";
        JSValue pay_ret = JS_Eval(ctx, pay_src, std::strlen(pay_src),
                                  "<cycle254_payment>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(pay_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, pay_ret);
    }

    // ------------------------------------------------------------------
    // WebGL stub — WebGLRenderingContext / WebGL2RenderingContext
    // ------------------------------------------------------------------
    {
        const char* webgl_src = R"JS(
(function(){
if(typeof globalThis.WebGLRenderingContext !== 'undefined') return;
var gl_consts = {DEPTH_BUFFER_BIT:256,STENCIL_BUFFER_BIT:1024,COLOR_BUFFER_BIT:16384,POINTS:0,LINES:1,LINE_LOOP:2,LINE_STRIP:3,TRIANGLES:4,TRIANGLE_STRIP:5,TRIANGLE_FAN:6,ZERO:0,ONE:1,SRC_COLOR:768,SRC_ALPHA:770,DST_ALPHA:772,DST_COLOR:774,ONE_MINUS_SRC_COLOR:769,ONE_MINUS_SRC_ALPHA:771,ONE_MINUS_DST_ALPHA:773,ONE_MINUS_DST_COLOR:775,FLOAT:5126,UNSIGNED_BYTE:5121,UNSIGNED_SHORT:5123,ARRAY_BUFFER:34962,ELEMENT_ARRAY_BUFFER:34963,STATIC_DRAW:35044,DYNAMIC_DRAW:35048,FRAGMENT_SHADER:35632,VERTEX_SHADER:35633,COMPILE_STATUS:35713,LINK_STATUS:35714,TEXTURE_2D:3553,TEXTURE0:33984,RGBA:6408,LINEAR:9729,NEAREST:9728,TEXTURE_MIN_FILTER:10241,TEXTURE_MAG_FILTER:10240,TEXTURE_WRAP_S:10242,TEXTURE_WRAP_T:10243,CLAMP_TO_EDGE:33071,REPEAT:10497,COLOR_ATTACHMENT0:36064,FRAMEBUFFER:36160,RENDERBUFFER:36161,DEPTH_COMPONENT16:33189,NO_ERROR:0};
function WebGLRenderingContext(canvas) {
  this.canvas=canvas||null;
  this.drawingBufferWidth=canvas?canvas.width||300:300;
  this.drawingBufferHeight=canvas?canvas.height||150:150;
  var self=this; Object.keys(gl_consts).forEach(function(k){self[k]=gl_consts[k];});
  this._id=0;
}
var p = WebGLRenderingContext.prototype;
p.getExtension=function(n){return null;};
p.getSupportedExtensions=function(){return[];};
p.getParameter=function(pp){if(pp===7938)return'WebGL 1.0';if(pp===7936)return'Vibrowser';if(pp===7937)return'Vibrowser WebGL';return 0;};
p.getShaderPrecisionFormat=function(s,pp){return{rangeMin:127,rangeMax:127,precision:23};};
p.createShader=function(t){return{_type:t,_id:++this._id};};
p.shaderSource=function(s,src){if(s)s._source=src;};
p.compileShader=function(s){if(s)s._compiled=true;};
p.getShaderParameter=function(s,pp){if(pp===35713)return true;return 0;};
p.getShaderInfoLog=function(s){return'';};
p.createProgram=function(){return{_id:++this._id,_shaders:[]};};
p.attachShader=function(pr,s){if(pr&&pr._shaders)pr._shaders.push(s);};
p.linkProgram=function(pr){if(pr)pr._linked=true;};
p.getProgramParameter=function(pr,pname){if(pname===35714)return true;return 0;};
p.getProgramInfoLog=function(pr){return'';};
p.useProgram=function(pr){};
p.getAttribLocation=function(pr,n){return 0;};
p.getUniformLocation=function(pr,n){return{_name:n};};
p.enableVertexAttribArray=function(i){};
p.disableVertexAttribArray=function(i){};
p.vertexAttribPointer=function(i,s,t,n,st,o){};
p.createBuffer=function(){return{_id:++this._id};};
p.bindBuffer=function(t,b){};
p.bufferData=function(t,d,u){};
p.createTexture=function(){return{_id:++this._id};};
p.bindTexture=function(t,tex){};
p.texImage2D=function(){};
p.texParameteri=function(t,pp,v){};
p.activeTexture=function(t){};
p.createFramebuffer=function(){return{_id:++this._id};};
p.bindFramebuffer=function(t,f){};
p.framebufferTexture2D=function(){};
p.createRenderbuffer=function(){return{_id:++this._id};};
p.bindRenderbuffer=function(t,r){};
p.renderbufferStorage=function(){};
p.uniform1i=function(l,v){};
p.uniform1f=function(l,v){};
p.uniform2f=function(l,x,y){};
p.uniform3f=function(l,x,y,z){};
p.uniform4f=function(l,x,y,z,w){};
p.uniformMatrix4fv=function(l,t,v){};
p.viewport=function(x,y,w,h){};
p.clear=function(m){};
p.clearColor=function(r,g,b,a){};
p.clearDepth=function(d){};
p.enable=function(c){};
p.disable=function(c){};
p.blendFunc=function(s,d){};
p.depthFunc=function(f){};
p.cullFace=function(m){};
p.drawArrays=function(m,f,c){};
p.drawElements=function(m,c,t,o){};
p.deleteShader=function(s){};
p.deleteProgram=function(pr){};
p.deleteBuffer=function(b){};
p.deleteTexture=function(t){};
p.deleteFramebuffer=function(f){};
p.deleteRenderbuffer=function(r){};
p.getError=function(){return 0;};
p.flush=function(){};
p.finish=function(){};
p.pixelStorei=function(pp,v){};
p.scissor=function(x,y,w,h){};
p.lineWidth=function(w){};
p.generateMipmap=function(t){};
p.isContextLost=function(){return false;};
Object.keys(gl_consts).forEach(function(k){WebGLRenderingContext[k]=gl_consts[k];});
globalThis.WebGLRenderingContext = WebGLRenderingContext;
globalThis.WebGL2RenderingContext = WebGLRenderingContext;
})();
)JS";
        JSValue webgl_ret = JS_Eval(ctx, webgl_src, std::strlen(webgl_src),
                                    "<webgl-stub>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(webgl_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, webgl_ret);
    }

    // ------------------------------------------------------------------
    // Scan for inline event attributes (onclick, onload, etc.)
    // ------------------------------------------------------------------
    scan_inline_event_attributes(ctx, document_root);

    // ------------------------------------------------------------------
    // Handle autofocus attribute: focus the first element with autofocus
    // ------------------------------------------------------------------
    {
        auto* state = get_dom_state(ctx);
        if (state && document_root) {
            // BFS/DFS to find first element with autofocus attribute
            std::function<clever::html::SimpleNode*(clever::html::SimpleNode*)> find_autofocus =
                [&](clever::html::SimpleNode* node) -> clever::html::SimpleNode* {
                if (!node) return nullptr;
                if (node->type == clever::html::SimpleNode::Element &&
                    has_attr(*node, "autofocus")) {
                    return node;
                }
                for (auto& child : node->children) {
                    auto* result = find_autofocus(child.get());
                    if (result) return result;
                }
                return nullptr;
            };

            auto* autofocus_el = find_autofocus(document_root);
            if (autofocus_el) {
                do_focus_element(ctx, state, autofocus_el, nullptr);
            }
        }
    }
}

std::string get_document_title(JSContext* ctx) {
    auto* state = get_dom_state(ctx);
    if (state && state->title_set) return state->title;
    return "";
}

bool dom_was_modified(JSContext* ctx) {
    auto* state = get_dom_state(ctx);
    return state && state->modified;
}

// Flush pending MutationObserver callbacks
void fire_mutation_observers(JSContext* ctx) {
    auto* state = get_dom_state(ctx);
    flush_mutation_observers(ctx, state);
}

// =========================================================================
// Event dispatch
// =========================================================================

// Helper: create a standard event object with all propagation methods
static JSValue create_event_object(JSContext* ctx,
                                    const std::string& event_type,
                                    bool bubbles, bool cancelable) {
    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type",
        JS_NewString(ctx, event_type.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "bubbles",
        JS_NewBool(ctx, bubbles));
    JS_SetPropertyStr(ctx, event_obj, "cancelable",
        JS_NewBool(ctx, cancelable));
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 0)); // NONE
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // Hidden propagation state
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);

    // Install methods via eval
    const char* method_code = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
        })
    )JS";
    JSValue setup_fn = JS_Eval(ctx, method_code, std::strlen(method_code),
                                "<event-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);

    return event_obj;
}

// Helper: create a MouseEvent object with all standard properties
static JSValue create_mouse_event_object(JSContext* ctx,
                                          const std::string& event_type,
                                          bool bubbles, bool cancelable,
                                          double client_x, double client_y,
                                          double screen_x, double screen_y,
                                          int button, int buttons,
                                          bool ctrl_key, bool shift_key,
                                          bool alt_key, bool meta_key,
                                          double movement_x, double movement_y,
                                          int detail) {
    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type",
        JS_NewString(ctx, event_type.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "bubbles",
        JS_NewBool(ctx, bubbles));
    JS_SetPropertyStr(ctx, event_obj, "cancelable",
        JS_NewBool(ctx, cancelable));
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 0)); // NONE
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

    // MouseEvent-specific properties
    JS_SetPropertyStr(ctx, event_obj, "clientX", JS_NewFloat64(ctx, client_x));
    JS_SetPropertyStr(ctx, event_obj, "clientY", JS_NewFloat64(ctx, client_y));
    JS_SetPropertyStr(ctx, event_obj, "screenX", JS_NewFloat64(ctx, screen_x));
    JS_SetPropertyStr(ctx, event_obj, "screenY", JS_NewFloat64(ctx, screen_y));
    JS_SetPropertyStr(ctx, event_obj, "pageX", JS_NewFloat64(ctx, client_x));
    JS_SetPropertyStr(ctx, event_obj, "pageY", JS_NewFloat64(ctx, client_y));
    JS_SetPropertyStr(ctx, event_obj, "offsetX", JS_NewFloat64(ctx, client_x));
    JS_SetPropertyStr(ctx, event_obj, "offsetY", JS_NewFloat64(ctx, client_y));
    JS_SetPropertyStr(ctx, event_obj, "button", JS_NewInt32(ctx, button));
    JS_SetPropertyStr(ctx, event_obj, "buttons", JS_NewInt32(ctx, buttons));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey", JS_NewBool(ctx, ctrl_key));
    JS_SetPropertyStr(ctx, event_obj, "shiftKey", JS_NewBool(ctx, shift_key));
    JS_SetPropertyStr(ctx, event_obj, "altKey", JS_NewBool(ctx, alt_key));
    JS_SetPropertyStr(ctx, event_obj, "metaKey", JS_NewBool(ctx, meta_key));
    JS_SetPropertyStr(ctx, event_obj, "relatedTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "movementX", JS_NewFloat64(ctx, movement_x));
    JS_SetPropertyStr(ctx, event_obj, "movementY", JS_NewFloat64(ctx, movement_y));
    JS_SetPropertyStr(ctx, event_obj, "detail", JS_NewInt32(ctx, detail));

    // Hidden propagation state
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);

    // Install methods via eval
    const char* method_code = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
            evt.getModifierState = function(key) {
                if (key === 'Control') return evt.ctrlKey;
                if (key === 'Shift') return evt.shiftKey;
                if (key === 'Alt') return evt.altKey;
                if (key === 'Meta') return evt.metaKey;
                return false;
            };
        })
    )JS";
    JSValue setup_fn = JS_Eval(ctx, method_code, std::strlen(method_code),
                                "<mouse-event-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);

    return event_obj;
}

bool dispatch_event(JSContext* ctx, clever::html::SimpleNode* target,
                    const std::string& event_type) {
    auto* state = get_dom_state(ctx);
    if (!state) return false;

    // Determine bubbling behavior
    bool bubbles = event_type_bubbles(event_type);

    // Create a full event object with propagation support
    JSValue event_obj = create_event_object(ctx, event_type, bubbles, true);

    bool default_prevented = dispatch_event_propagated(
        ctx, state, target, event_obj, event_type, bubbles);

    // Execute default actions if not prevented
    if (!default_prevented) {
        execute_default_action(ctx, state, target, event_type);
    }

    JS_FreeValue(ctx, event_obj);

    return default_prevented;
}

bool dispatch_mouse_event(JSContext* ctx, clever::html::SimpleNode* target,
                           const std::string& event_type,
                           double client_x, double client_y,
                           double screen_x, double screen_y,
                           int button, int buttons,
                           bool ctrl_key, bool shift_key,
                           bool alt_key, bool meta_key,
                           int detail) {
    auto* state = get_dom_state(ctx);
    if (!state || !target) return false;

    // Determine bubbling behavior
    bool bubbles = event_type_bubbles(event_type);

    // Create a MouseEvent object with all standard properties
    JSValue event_obj = create_mouse_event_object(
        ctx, event_type, bubbles, true,
        client_x, client_y, screen_x, screen_y,
        button, buttons,
        ctrl_key, shift_key, alt_key, meta_key,
        0.0, 0.0,  // movementX/Y default to 0
        detail);

    bool default_prevented = dispatch_event_propagated(
        ctx, state, target, event_obj, event_type, bubbles);

    // Execute default actions if not prevented
    if (!default_prevented) {
        execute_default_action(ctx, state, target, event_type);
    }

    JS_FreeValue(ctx, event_obj);

    return default_prevented;
}

bool dispatch_keyboard_event(JSContext* ctx, clever::html::SimpleNode* target,
                             const std::string& event_type,
                             const KeyboardEventInit& init) {
    auto* state = get_dom_state(ctx);
    if (!state || !target) return false;

    // Keyboard events always bubble and are cancelable
    bool bubbles = true;
    bool cancelable = true;

    // Create the event object with standard Event properties
    JSValue event_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event_obj, "type",
        JS_NewString(ctx, event_type.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "bubbles",
        JS_NewBool(ctx, bubbles));
    JS_SetPropertyStr(ctx, event_obj, "cancelable",
        JS_NewBool(ctx, cancelable));
    JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "eventPhase",
        JS_NewInt32(ctx, 0)); // NONE
    JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);
    JS_SetPropertyStr(ctx, event_obj, "timeStamp",
        JS_NewFloat64(ctx, 0));

    // KeyboardEvent-specific properties
    JS_SetPropertyStr(ctx, event_obj, "key",
        JS_NewString(ctx, init.key.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "code",
        JS_NewString(ctx, init.code.c_str()));
    JS_SetPropertyStr(ctx, event_obj, "keyCode",
        JS_NewInt32(ctx, init.key_code));
    JS_SetPropertyStr(ctx, event_obj, "charCode",
        JS_NewInt32(ctx, init.char_code));
    JS_SetPropertyStr(ctx, event_obj, "which",
        JS_NewInt32(ctx, init.key_code)); // 'which' defaults to keyCode
    JS_SetPropertyStr(ctx, event_obj, "location",
        JS_NewInt32(ctx, init.location));
    JS_SetPropertyStr(ctx, event_obj, "altKey",
        JS_NewBool(ctx, init.alt_key));
    JS_SetPropertyStr(ctx, event_obj, "ctrlKey",
        JS_NewBool(ctx, init.ctrl_key));
    JS_SetPropertyStr(ctx, event_obj, "metaKey",
        JS_NewBool(ctx, init.meta_key));
    JS_SetPropertyStr(ctx, event_obj, "shiftKey",
        JS_NewBool(ctx, init.shift_key));
    JS_SetPropertyStr(ctx, event_obj, "repeat",
        JS_NewBool(ctx, init.repeat));
    JS_SetPropertyStr(ctx, event_obj, "isComposing",
        JS_NewBool(ctx, init.is_composing));

    // Hidden propagation state
    JS_SetPropertyStr(ctx, event_obj, "__stopped", JS_FALSE);
    JS_SetPropertyStr(ctx, event_obj, "__immediate_stopped", JS_FALSE);

    // Install methods (preventDefault, stopPropagation, getModifierState, etc.)
    const char* method_code = R"JS(
        (function() {
            var evt = this;
            evt.preventDefault = function() { evt.defaultPrevented = true; };
            evt.stopPropagation = function() { evt.__stopped = true; };
            evt.stopImmediatePropagation = function() {
                evt.__stopped = true;
                evt.__immediate_stopped = true;
            };
            evt.composedPath = function() {
                var arr = evt.__composedPathArray;
                if (!arr) return [];
                var result = [];
                for (var i = 0; i < arr.length; i++) result.push(arr[i]);
                return result;
            };
            evt.getModifierState = function(key) {
                if (key === 'Control') return evt.ctrlKey;
                if (key === 'Shift') return evt.shiftKey;
                if (key === 'Alt') return evt.altKey;
                if (key === 'Meta') return evt.metaKey;
                return false;
            };
        })
    )JS";
    JSValue setup_fn = JS_Eval(ctx, method_code, std::strlen(method_code),
                                "<keyboard-event-dispatch-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsFunction(ctx, setup_fn)) {
        JSValue setup_ret = JS_Call(ctx, setup_fn, event_obj, 0, nullptr);
        JS_FreeValue(ctx, setup_ret);
    }
    JS_FreeValue(ctx, setup_fn);

    bool default_prevented = dispatch_event_propagated(
        ctx, state, target, event_obj, event_type, bubbles);

    // Execute default actions if not prevented
    if (!default_prevented) {
        execute_default_action(ctx, state, target, event_type);
    }

    JS_FreeValue(ctx, event_obj);

    return default_prevented;
}

void dispatch_dom_content_loaded(JSContext* ctx) {
    auto* state = get_dom_state(ctx);
    if (!state) return;

    // Fire DOMContentLoaded on the document root
    // DOMContentLoaded bubbles: true
    if (state->root) {
        dispatch_event(ctx, state->root, "DOMContentLoaded");
    }

    // Also fire on "window" listeners (many scripts use
    // window.addEventListener('DOMContentLoaded', ...))
    // Window listeners are stored under the nullptr sentinel key
    auto win_it = state->listeners.find(nullptr);
    if (win_it != state->listeners.end()) {
        auto type_it = win_it->second.find("DOMContentLoaded");
        if (type_it != win_it->second.end()) {
            // Create a minimal event object for window dispatch
            JSValue event_obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, event_obj, "type",
                JS_NewString(ctx, "DOMContentLoaded"));
            JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_FALSE);
            JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_FALSE);
            JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_FALSE);
            JS_SetPropertyStr(ctx, event_obj, "target", JS_NULL);
            JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_NULL);

            JSValue global = JS_GetGlobalObject(ctx);
            auto entries = type_it->second;
            for (auto& entry : entries) {
                JSValue result = JS_Call(ctx, entry.handler, global, 1, &event_obj);
                if (JS_IsException(result)) {
                    JSValue exc = JS_GetException(ctx);
                    JS_FreeValue(ctx, exc);
                }
                JS_FreeValue(ctx, result);
            }
            JS_FreeValue(ctx, global);
            JS_FreeValue(ctx, event_obj);
        }
    }
}

void populate_layout_geometry(JSContext* ctx, void* layout_root_ptr) {
    auto* state = get_dom_state(ctx);
    if (!state || !layout_root_ptr) return;
    state->layout_geometry.clear();

    auto* root = static_cast<clever::layout::LayoutNode*>(layout_root_ptr);

    // Walk the layout tree.  parent_dom_node tracks the closest ancestor
    // SimpleNode* — it may skip anonymous boxes which have no dom_node.
    std::function<void(clever::layout::LayoutNode&, float, float, void*)> walk =
        [&](clever::layout::LayoutNode& node, float abs_x, float abs_y,
            void* parent_dom_node) {
            // nx/ny = absolute position of the border-box top-left corner
            // (geometry.x/y are relative to the content-box of the parent,
            //  geometry.margin.left/top shifts to the margin-box edge, but
            //  since the parent passed content_x/y we only add x+margin).
            float nx = abs_x + node.geometry.x + node.geometry.margin.left;
            float ny = abs_y + node.geometry.y + node.geometry.margin.top;

            // The DOM node pointer that children will see as their parent
            void* this_dom_node = parent_dom_node;

            if (node.dom_node) {
                DOMState::LayoutRect rect;
                // x/y = content-box origin (absolute page coordinates)
                rect.x = nx + node.geometry.border.left + node.geometry.padding.left;
                rect.y = ny + node.geometry.border.top + node.geometry.padding.top;
                rect.width = node.geometry.width;
                rect.height = node.geometry.height;
                rect.border_left = node.geometry.border.left;
                rect.border_top = node.geometry.border.top;
                rect.border_right = node.geometry.border.right;
                rect.border_bottom = node.geometry.border.bottom;
                rect.padding_left = node.geometry.padding.left;
                rect.padding_top = node.geometry.padding.top;
                rect.padding_right = node.geometry.padding.right;
                rect.padding_bottom = node.geometry.padding.bottom;
                rect.margin_left = node.geometry.margin.left;
                rect.margin_top = node.geometry.margin.top;
                rect.margin_right = node.geometry.margin.right;
                rect.margin_bottom = node.geometry.margin.bottom;
                // Precompute absolute border-box origin (top-left of border edge)
                // in page coordinates.  Used by getBoundingClientRect and
                // offsetLeft/offsetTop.
                rect.abs_border_x = nx;
                rect.abs_border_y = ny;
                rect.scroll_top = node.scroll_top;
                rect.scroll_left = node.scroll_left;
                rect.scroll_content_width = node.scroll_content_width;
                rect.scroll_content_height = node.scroll_content_height;
                rect.is_scroll_container = node.is_scroll_container;
                rect.pointer_events = node.pointer_events;
                rect.visibility_hidden = node.visibility_hidden;
                rect.position_type = node.position_type;
                rect.parent_dom_node = parent_dom_node;

                // ---- Computed CSS style fields ----
                // Display type from LayoutNode's DisplayType enum
                {
                    using DT = clever::layout::DisplayType;
                    switch (node.display) {
                        case DT::Block:        rect.display_type = 0;  break;
                        case DT::Inline:       rect.display_type = 1;  break;
                        case DT::InlineBlock:  rect.display_type = 2;  break;
                        case DT::Flex:         rect.display_type = 3;  break;
                        case DT::InlineFlex:   rect.display_type = 4;  break;
                        case DT::None:         rect.display_type = 5;  break;
                        case DT::ListItem:     rect.display_type = 6;  break;
                        case DT::Table:        rect.display_type = 7;  break;
                        case DT::TableRow:     rect.display_type = 8;  break;
                        case DT::TableCell:    rect.display_type = 9;  break;
                        case DT::Grid:         rect.display_type = 10; break;
                        case DT::InlineGrid:   rect.display_type = 11; break;
                        default:               rect.display_type = 0;  break;
                    }
                }
                rect.float_type = node.float_type;
                rect.clear_type = node.clear_type;
                rect.border_box = node.border_box;
                // Sizing constraints
                rect.specified_width = node.specified_width;
                rect.specified_height = node.specified_height;
                rect.min_width_val = node.min_width;
                rect.max_width_val = node.max_width;
                rect.min_height_val = node.min_height;
                rect.max_height_val = node.max_height;
                // Typography
                rect.font_size = node.font_size;
                rect.font_weight = node.font_weight;
                rect.font_italic = node.font_italic;
                rect.font_family = node.font_family;
                // LayoutNode::line_height is the unitless factor (e.g. 1.2)
                rect.line_height_unitless = node.line_height;
                rect.line_height_px = node.line_height * node.font_size;
                // Colors
                rect.color = node.color;
                rect.background_color = node.background_color;
                // Background
                // LayoutNode lacks bg_image_url (that's in ComputedStyle only).
                // We can only detect presence via bg_image_pixels; URL is unknown here.
                if (node.bg_image_pixels && !node.bg_image_pixels->empty()) {
                    rect.bg_image_url = "<url>"; // placeholder: actual URL not in LayoutNode
                } else {
                    rect.bg_image_url = "";
                }
                rect.gradient_type = node.gradient_type;
                // Visual
                rect.opacity_val = node.opacity;
                rect.overflow_x_val = node.overflow;
                rect.overflow_y_val = node.overflow;
                if (clever::layout::is_z_index_auto(node.z_index)) {
                    rect.z_index_auto = true;
                    rect.z_index_val = 0;
                } else {
                    rect.z_index_auto = false;
                    rect.z_index_val = node.z_index;
                }
                // Text
                rect.text_align_val = node.text_align;
                rect.text_decoration_bits = node.text_decoration_bits;
                rect.white_space_val = node.white_space;
                rect.word_break_val = node.word_break;
                rect.overflow_wrap_val = node.overflow_wrap;
                rect.text_transform_val = node.text_transform;
                rect.text_overflow_val = node.text_overflow;
                // Flex
                rect.flex_grow = node.flex_grow;
                rect.flex_shrink = node.flex_shrink;
                rect.flex_basis = node.flex_basis;
                rect.flex_direction = node.flex_direction;
                rect.flex_wrap_val = node.flex_wrap;
                rect.justify_content_val = node.justify_content;
                rect.align_items_val = node.align_items;
                rect.align_self_val = node.align_self;
                // Border radius
                rect.border_radius_tl = node.border_radius_tl;
                rect.border_radius_tr = node.border_radius_tr;
                rect.border_radius_bl = node.border_radius_bl;
                rect.border_radius_br = node.border_radius_br;
                // Border styles and colors
                rect.border_style_top = node.border_style_top;
                rect.border_style_right = node.border_style_right;
                rect.border_style_bottom = node.border_style_bottom;
                rect.border_style_left = node.border_style_left;
                rect.border_color_top = node.border_color_top;
                rect.border_color_right = node.border_color_right;
                rect.border_color_bottom = node.border_color_bottom;
                rect.border_color_left = node.border_color_left;
                // CSS Transforms
                rect.transforms = node.transforms;
                // Cursor / pointer-events / user-select
                rect.cursor_val = node.cursor;
                rect.user_select_val = node.user_select;

                state->layout_geometry[node.dom_node] = rect;
                this_dom_node = node.dom_node;
            }

            float content_x = nx + node.geometry.border.left + node.geometry.padding.left;
            float content_y = ny + node.geometry.border.top + node.geometry.padding.top;

            for (auto& child : node.children) {
                walk(*child, content_x, content_y, this_dom_node);
            }
        };

    walk(*root, 0, 0, nullptr);
}

void fire_intersection_observers(JSContext* ctx, int viewport_w, int viewport_h) {
    auto* state = get_dom_state(ctx);
    if (!state) return;
    state->viewport_width = viewport_w;
    state->viewport_height = viewport_h;

    for (auto& io : state->intersection_observers) {
        if (!JS_IsFunction(ctx, io.callback)) continue;
        if (io.observed_elements.empty()) continue;

        // Build array of IntersectionObserverEntry objects
        JSValue entries = JS_NewArray(ctx);
        int entry_idx = 0;

        for (auto* elem : io.observed_elements) {
            // Look up element geometry
            auto it = state->layout_geometry.find(elem);

            // Compute border-box rect for the element using precomputed abs_border_x/y
            float elem_x = 0, elem_y = 0, elem_w = 0, elem_h = 0;
            if (it != state->layout_geometry.end()) {
                auto& lr = it->second;
                elem_x = lr.abs_border_x;
                elem_y = lr.abs_border_y;
                elem_w = lr.border_left + lr.padding_left + lr.width +
                         lr.padding_right + lr.border_right;
                elem_h = lr.border_top + lr.padding_top + lr.height +
                         lr.padding_bottom + lr.border_bottom;
            }

            // Root rect (viewport + rootMargin)
            float root_x = -io.root_margin_left;
            float root_y = -io.root_margin_top;
            float root_w = static_cast<float>(viewport_w) +
                           io.root_margin_left + io.root_margin_right;
            float root_h = static_cast<float>(viewport_h) +
                           io.root_margin_top + io.root_margin_bottom;

            // Compute intersection rect
            float ix1 = std::max(elem_x, root_x);
            float iy1 = std::max(elem_y, root_y);
            float ix2 = std::min(elem_x + elem_w, root_x + root_w);
            float iy2 = std::min(elem_y + elem_h, root_y + root_h);
            float inter_w = std::max(0.0f, ix2 - ix1);
            float inter_h = std::max(0.0f, iy2 - iy1);

            float intersection_area = inter_w * inter_h;
            float element_area = elem_w * elem_h;
            float ratio = (element_area > 0) ? (intersection_area / element_area) : 0.0f;
            bool is_intersecting = (ratio > 0) || (elem_w > 0 && elem_h > 0 &&
                                   inter_w > 0 && inter_h > 0);

            // Check against thresholds — fire if ratio crosses any threshold
            bool should_fire = false;
            for (float t : io.thresholds) {
                if (ratio >= t) { should_fire = true; break; }
            }
            // Always fire on initial observation (spec behavior)
            should_fire = true;

            if (should_fire) {
                // Create IntersectionObserverEntry
                JSValue entry = JS_NewObject(ctx);

                // boundingClientRect
                JSValue bcr = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, bcr, "x", JS_NewFloat64(ctx, elem_x));
                JS_SetPropertyStr(ctx, bcr, "y", JS_NewFloat64(ctx, elem_y));
                JS_SetPropertyStr(ctx, bcr, "top", JS_NewFloat64(ctx, elem_y));
                JS_SetPropertyStr(ctx, bcr, "left", JS_NewFloat64(ctx, elem_x));
                JS_SetPropertyStr(ctx, bcr, "bottom", JS_NewFloat64(ctx, elem_y + elem_h));
                JS_SetPropertyStr(ctx, bcr, "right", JS_NewFloat64(ctx, elem_x + elem_w));
                JS_SetPropertyStr(ctx, bcr, "width", JS_NewFloat64(ctx, elem_w));
                JS_SetPropertyStr(ctx, bcr, "height", JS_NewFloat64(ctx, elem_h));
                JS_SetPropertyStr(ctx, entry, "boundingClientRect", bcr);

                // intersectionRect
                JSValue ir = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, ir, "x", JS_NewFloat64(ctx, ix1));
                JS_SetPropertyStr(ctx, ir, "y", JS_NewFloat64(ctx, iy1));
                JS_SetPropertyStr(ctx, ir, "top", JS_NewFloat64(ctx, iy1));
                JS_SetPropertyStr(ctx, ir, "left", JS_NewFloat64(ctx, ix1));
                JS_SetPropertyStr(ctx, ir, "bottom", JS_NewFloat64(ctx, iy1 + inter_h));
                JS_SetPropertyStr(ctx, ir, "right", JS_NewFloat64(ctx, ix1 + inter_w));
                JS_SetPropertyStr(ctx, ir, "width", JS_NewFloat64(ctx, inter_w));
                JS_SetPropertyStr(ctx, ir, "height", JS_NewFloat64(ctx, inter_h));
                JS_SetPropertyStr(ctx, entry, "intersectionRect", ir);

                // rootBounds
                JSValue rb = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, rb, "x", JS_NewFloat64(ctx, root_x));
                JS_SetPropertyStr(ctx, rb, "y", JS_NewFloat64(ctx, root_y));
                JS_SetPropertyStr(ctx, rb, "top", JS_NewFloat64(ctx, root_y));
                JS_SetPropertyStr(ctx, rb, "left", JS_NewFloat64(ctx, root_x));
                JS_SetPropertyStr(ctx, rb, "bottom", JS_NewFloat64(ctx, root_y + root_h));
                JS_SetPropertyStr(ctx, rb, "right", JS_NewFloat64(ctx, root_x + root_w));
                JS_SetPropertyStr(ctx, rb, "width", JS_NewFloat64(ctx, root_w));
                JS_SetPropertyStr(ctx, rb, "height", JS_NewFloat64(ctx, root_h));
                JS_SetPropertyStr(ctx, entry, "rootBounds", rb);

                JS_SetPropertyStr(ctx, entry, "intersectionRatio",
                    JS_NewFloat64(ctx, ratio));
                JS_SetPropertyStr(ctx, entry, "isIntersecting",
                    JS_NewBool(ctx, is_intersecting));

                // target — wrap the SimpleNode as an element proxy
                JS_SetPropertyStr(ctx, entry, "target", wrap_element(ctx, elem));

                JS_SetPropertyUint32(ctx, entries, entry_idx++, entry);
            }
        }

        if (entry_idx > 0) {
            // Call the callback with (entries, observer)
            JSValue args[2] = { entries, io.observer_obj };
            JSValue ret = JS_Call(ctx, io.callback, JS_UNDEFINED, 2, args);
            JS_FreeValue(ctx, ret);
        }
        JS_FreeValue(ctx, entries);
    }
}

void fire_resize_observers(JSContext* ctx, int viewport_w, int viewport_h) {
    auto* state = get_dom_state(ctx);
    if (!state) return;
    (void)viewport_w;
    (void)viewport_h;

    double device_pixel_ratio = 1.0;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue dpr_val = JS_GetPropertyStr(ctx, global, "devicePixelRatio");
    if (JS_IsNumber(dpr_val)) {
        JS_ToFloat64(ctx, &device_pixel_ratio, dpr_val);
    }
    JS_FreeValue(ctx, dpr_val);
    JS_FreeValue(ctx, global);
    if (!std::isfinite(device_pixel_ratio) || device_pixel_ratio <= 0.0) {
        device_pixel_ratio = 1.0;
    }

    for (auto& ro : state->resize_observers) {
        if (!JS_IsFunction(ctx, ro.callback)) continue;
        if (ro.observed_elements.empty()) continue;

        // Build array of ResizeObserverEntry objects
        JSValue entries = JS_NewArray(ctx);
        int entry_idx = 0;
        bool has_size_change = false;
        std::unordered_map<clever::html::SimpleNode*, std::pair<float, float>> current_sizes;

        for (auto* elem : ro.observed_elements) {
            // Look up element geometry
            auto it = state->layout_geometry.find(elem);

            // Content box dimensions
            float content_x = 0, content_y = 0, content_w = 0, content_h = 0;
            // Border box dimensions
            float border_w = 0, border_h = 0;

            if (it != state->layout_geometry.end()) {
                auto& lr = it->second;
                content_x = lr.x;
                content_y = lr.y;
                content_w = lr.width;
                content_h = lr.height;
                border_w = lr.border_left + lr.padding_left + lr.width +
                           lr.padding_right + lr.border_right;
                border_h = lr.border_top + lr.padding_top + lr.height +
                           lr.padding_bottom + lr.border_bottom;
            }
            current_sizes[elem] = std::make_pair(border_w, border_h);

            auto prev_it = ro.previous_sizes.find(elem);
            if (prev_it == ro.previous_sizes.end() ||
                prev_it->second.first != border_w ||
                prev_it->second.second != border_h) {
                has_size_change = true;
            }

            // Create ResizeObserverEntry
            JSValue entry = JS_NewObject(ctx);

            // contentRect (DOMRectReadOnly) — content-box in page coordinates
            JSValue cr = make_dom_rect(ctx, content_x, content_y,
                                        content_w, content_h);
            JS_SetPropertyStr(ctx, entry, "contentRect", cr);

            // contentBoxSize (array of one ResizeObserverSize)
            JSValue cbs_arr = JS_NewArray(ctx);
            JSValue cbs = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, cbs, "inlineSize", JS_NewFloat64(ctx, content_w));
            JS_SetPropertyStr(ctx, cbs, "blockSize", JS_NewFloat64(ctx, content_h));
            JS_SetPropertyUint32(ctx, cbs_arr, 0, cbs);
            JS_SetPropertyStr(ctx, entry, "contentBoxSize", cbs_arr);

            // borderBoxSize (array of one ResizeObserverSize)
            JSValue bbs_arr = JS_NewArray(ctx);
            JSValue bbs = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, bbs, "inlineSize", JS_NewFloat64(ctx, border_w));
            JS_SetPropertyStr(ctx, bbs, "blockSize", JS_NewFloat64(ctx, border_h));
            JS_SetPropertyUint32(ctx, bbs_arr, 0, bbs);
            JS_SetPropertyStr(ctx, entry, "borderBoxSize", bbs_arr);

            // devicePixelContentBoxSize (array of one ResizeObserverSize)
            JSValue dpbs_arr = JS_NewArray(ctx);
            JSValue dpbs = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, dpbs, "inlineSize", JS_NewFloat64(ctx, content_w * device_pixel_ratio));
            JS_SetPropertyStr(ctx, dpbs, "blockSize", JS_NewFloat64(ctx, content_h * device_pixel_ratio));
            JS_SetPropertyUint32(ctx, dpbs_arr, 0, dpbs);
            JS_SetPropertyStr(ctx, entry, "devicePixelContentBoxSize", dpbs_arr);

            // target — wrap the SimpleNode as an element proxy
            JS_SetPropertyStr(ctx, entry, "target", wrap_element(ctx, elem));

            JS_SetPropertyUint32(ctx, entries, entry_idx++, entry);
        }

        if (has_size_change && entry_idx > 0) {
            // Call the callback with (entries, observer)
            JSValue args[2] = { entries, ro.observer_obj };
            JSValue ret = JS_Call(ctx, ro.callback, JS_UNDEFINED, 2, args);
            JS_FreeValue(ctx, ret);
            ro.previous_sizes = std::move(current_sizes);
        }
        JS_FreeValue(ctx, entries);
    }
}

void cleanup_dom_bindings(JSContext* ctx) {
    auto* state = get_dom_state(ctx);
    if (!state) return;

    // Free all JS event listener references
    for (auto& [node, type_map] : state->listeners) {
        for (auto& [type, entries] : type_map) {
            for (auto& entry : entries) {
                JS_FreeValue(ctx, entry.handler);
            }
        }
    }

    // Free IntersectionObserver JS values
    for (auto& io : state->intersection_observers) {
        JS_FreeValue(ctx, io.callback);
        JS_FreeValue(ctx, io.observer_obj);
    }
    state->intersection_observers.clear();

    // Free ResizeObserver JS values
    for (auto& ro : state->resize_observers) {
        JS_FreeValue(ctx, ro.callback);
        JS_FreeValue(ctx, ro.observer_obj);
    }
    state->resize_observers.clear();

    delete state;

    // Clear the pointer from the global object so it can't be used again
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__dom_state_ptr", JS_NewInt64(ctx, 0));
    JS_FreeValue(ctx, global);
}

void set_current_script(JSContext* ctx, clever::html::SimpleNode* script_elem) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue doc_obj = JS_GetPropertyStr(ctx, global, "document");
    if (!JS_IsUndefined(doc_obj) && !JS_IsException(doc_obj)) {
        if (script_elem) {
            JS_SetPropertyStr(ctx, doc_obj, "currentScript",
                              wrap_element(ctx, script_elem));
        } else {
            JS_SetPropertyStr(ctx, doc_obj, "currentScript", JS_NULL);
        }
    }
    JS_FreeValue(ctx, doc_obj);
    JS_FreeValue(ctx, global);
}

} // namespace clever::js
