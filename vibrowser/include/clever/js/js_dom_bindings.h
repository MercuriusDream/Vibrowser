#pragma once
#include <clever/html/tree_builder.h>
#include <string>

struct JSContext;
struct JSRuntime;

namespace clever::js {

// Install DOM bindings into a QuickJS context.
// The SimpleNode tree must outlive the context.
void install_dom_bindings(JSContext* ctx, clever::html::SimpleNode* document_root);

// Collect document.title if set via JS
std::string get_document_title(JSContext* ctx);

// Collect modifications made by JS to the DOM
// Returns true if DOM was modified (so re-render is needed)
bool dom_was_modified(JSContext* ctx);

// Clean up DOM state allocated for the context.
// Call this before freeing the JSContext.
void cleanup_dom_bindings(JSContext* ctx);

// Dispatch an event to an element, returns true if default was prevented
bool dispatch_event(JSContext* ctx, clever::html::SimpleNode* target,
                    const std::string& event_type);

// Dispatch a MouseEvent with full coordinate/modifier properties.
// Returns true if event.preventDefault() was called.
bool dispatch_mouse_event(JSContext* ctx, clever::html::SimpleNode* target,
                           const std::string& event_type,
                           double client_x, double client_y,
                           double screen_x, double screen_y,
                           int button, int buttons,
                           bool ctrl_key, bool shift_key,
                           bool alt_key, bool meta_key,
                           int detail = 0);

// Keyboard event properties for dispatch_keyboard_event
struct KeyboardEventInit {
    std::string key;          // "a", "Enter", "ArrowUp", etc.
    std::string code;         // "KeyA", "Enter", "ArrowUp", etc.
    int key_code = 0;         // legacy keyCode
    int char_code = 0;        // legacy charCode
    int location = 0;         // 0=standard, 1=left, 2=right, 3=numpad
    bool alt_key = false;
    bool ctrl_key = false;
    bool meta_key = false;
    bool shift_key = false;
    bool repeat = false;
    bool is_composing = false;
};

// Dispatch a keyboard event (keydown/keyup/keypress) to an element.
// Creates a full KeyboardEvent object with all standard properties.
// Returns true if event.preventDefault() was called.
bool dispatch_keyboard_event(JSContext* ctx, clever::html::SimpleNode* target,
                             const std::string& event_type,
                             const KeyboardEventInit& init);

// Fire DOMContentLoaded event on document
void dispatch_dom_content_loaded(JSContext* ctx);

// Populate layout geometry cache from a completed layout tree.
// The layout_root_ptr should be a LayoutNode* cast to void*.
// This enables getBoundingClientRect() and dimension properties to return real values.
void populate_layout_geometry(JSContext* ctx, void* layout_root_ptr);

// Fire IntersectionObserver callbacks using cached layout geometry.
// Call after populate_layout_geometry(). Computes intersection against viewport.
void fire_intersection_observers(JSContext* ctx, int viewport_w, int viewport_h);

// Fire ResizeObserver callbacks using cached layout geometry.
// Call after populate_layout_geometry(). Creates ResizeObserverEntry objects
// with contentRect, contentBoxSize, borderBoxSize, and target.
void fire_resize_observers(JSContext* ctx, int viewport_w, int viewport_h);

// Set document.currentScript to the given element (or null if nullptr).
// Call before evaluating each <script> and set to nullptr after.
void set_current_script(JSContext* ctx, clever::html::SimpleNode* script_elem);

} // namespace clever::js
