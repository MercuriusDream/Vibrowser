#pragma once
#include <clever/paint/display_list.h>

namespace clever::layout { struct LayoutNode; }

namespace clever::paint {

class Painter {
public:
    DisplayList paint(const clever::layout::LayoutNode& root,
                      float viewport_height = 0,
                      float viewport_width = 0,
                      float viewport_scroll_y = 0,
                      float viewport_scroll_x = 0);

private:
    float viewport_height_ = 0;
    float viewport_width_ = 0;
    float viewport_scroll_y_ = 0;  // current viewport scroll offset (Y), for sticky positioning
    float viewport_scroll_x_ = 0;  // current viewport scroll offset (X), for sticky positioning
    void paint_node(const clever::layout::LayoutNode& node, DisplayList& list,
                    float offset_x, float offset_y);
    void paint_background(const clever::layout::LayoutNode& node, DisplayList& list,
                          float abs_x, float abs_y);
    void paint_borders(const clever::layout::LayoutNode& node, DisplayList& list,
                       float abs_x, float abs_y);
    void paint_text(const clever::layout::LayoutNode& node, DisplayList& list,
                    float abs_x, float abs_y);
    void paint_outline(const clever::layout::LayoutNode& node, DisplayList& list,
                       float abs_x, float abs_y);
    void paint_text_input(const clever::layout::LayoutNode& node, DisplayList& list,
                          float abs_x, float abs_y);
    void paint_button_input(const clever::layout::LayoutNode& node, DisplayList& list,
                            float abs_x, float abs_y);
    void paint_range_input(const clever::layout::LayoutNode& node, DisplayList& list,
                           float abs_x, float abs_y);
    void paint_color_input(const clever::layout::LayoutNode& node, DisplayList& list,
                           float abs_x, float abs_y);
    void paint_checkbox(const clever::layout::LayoutNode& node, DisplayList& list,
                        float abs_x, float abs_y);
    void paint_radio(const clever::layout::LayoutNode& node, DisplayList& list,
                     float abs_x, float abs_y);
    void paint_select_element(const clever::layout::LayoutNode& node, DisplayList& list,
                              float abs_x, float abs_y);
    void paint_svg_shape(const clever::layout::LayoutNode& node, DisplayList& list,
                         float abs_x, float abs_y);
    void paint_canvas_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                  float abs_x, float abs_y);
    void paint_media_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                 float abs_x, float abs_y);
    void paint_iframe_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                  float abs_x, float abs_y);
    void paint_marquee(const clever::layout::LayoutNode& node, DisplayList& list,
                       float abs_x, float abs_y);
    void paint_ruby_annotation(const clever::layout::LayoutNode& node, DisplayList& list,
                               float abs_x, float abs_y);
    void paint_list_marker(const clever::layout::LayoutNode& node, DisplayList& list,
                           float abs_x, float abs_y);
    void paint_overflow_indicator(const clever::layout::LayoutNode& node, DisplayList& list,
                                  float abs_x, float abs_y);
    void paint_caret(const clever::layout::LayoutNode& node, DisplayList& list,
                     float abs_x, float abs_y);
};

} // namespace clever::paint
