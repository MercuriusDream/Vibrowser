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
