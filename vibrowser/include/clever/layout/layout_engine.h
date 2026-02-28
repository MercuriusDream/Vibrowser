#pragma once
#include <clever/layout/box.h>
#include <functional>

namespace clever::layout {

// Callback type for measuring text width using platform font APIs.
// Parameters: text, font_size, font_family, font_weight, is_italic, letter_spacing
// Returns: width in pixels
using TextMeasureFn = std::function<float(const std::string& text, float font_size,
                                          const std::string& font_family, int font_weight,
                                          bool is_italic, float letter_spacing)>;

class LayoutEngine {
public:
    // Compute layout for the tree rooted at node
    void compute(LayoutNode& root, float viewport_width, float viewport_height);

    // Set the text measurement function (injected by render pipeline)
    void set_text_measurer(TextMeasureFn fn) { text_measurer_ = std::move(fn); }

private:
    void layout_block(LayoutNode& node, float containing_width);
    void layout_inline(LayoutNode& node, float containing_width);
    void layout_flex(LayoutNode& node, float containing_width);
    void layout_grid(LayoutNode& node, float containing_width);
    void layout_table(LayoutNode& node, float containing_width);
    void layout_children(LayoutNode& node);

    float compute_width(LayoutNode& node, float containing_width);
    float compute_height(LayoutNode& node, float containing_height = -1);

    // Block layout: position children vertically
    void position_block_children(LayoutNode& node);

    // Inline layout: position children horizontally with wrapping
    void position_inline_children(LayoutNode& node, float containing_width);

    // Flex layout: flex algorithm
    void flex_layout(LayoutNode& node, float containing_width);

    // Apply relative/absolute positioning offsets
    void apply_positioning(LayoutNode& node);

    // Position absolute/fixed children after normal flow
    void position_absolute_children(LayoutNode& node);

    // CSS 2.1 §9.2.1.1: wrap consecutive inline children of a mixed
    // block/inline container into anonymous block boxes so that the parent
    // only has block-level children and can use position_block_children.
    void wrap_anonymous_blocks(LayoutNode& node);

    // Helper: measure a string's pixel width using real fonts or fallback
    float measure_text(const std::string& text, float font_size, const std::string& font_family,
                       int font_weight, bool is_italic, float letter_spacing) const;

    // Helper: get average character width for a font
    float avg_char_width(float font_size, const std::string& font_family,
                         int font_weight, bool is_italic, bool is_monospace,
                         float letter_spacing) const;

    float viewport_width_ = 0;
    float viewport_height_ = 0;
    TextMeasureFn text_measurer_;
};

} // namespace clever::layout
