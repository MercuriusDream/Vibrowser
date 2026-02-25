#include <clever/paint/display_list.h>

namespace clever::paint {

void DisplayList::fill_rect(const Rect& rect, const Color& color) {
    PaintCommand cmd;
    cmd.type = PaintCommand::FillRect;
    cmd.bounds = rect;
    cmd.color = color;
    commands_.push_back(cmd);
}

void DisplayList::fill_rounded_rect(const Rect& rect, const Color& color, float radius) {
    PaintCommand cmd;
    cmd.type = PaintCommand::FillRect;
    cmd.bounds = rect;
    cmd.color = color;
    cmd.border_radius = radius;
    commands_.push_back(cmd);
}

void DisplayList::fill_box_shadow(const Rect& shadow_rect, const Rect& element_rect,
                                   const Color& color, float blur_radius, float border_radius) {
    PaintCommand cmd;
    cmd.type = PaintCommand::FillBoxShadow;
    cmd.bounds = shadow_rect;
    cmd.element_rect = element_rect;
    cmd.color = color;
    cmd.blur_radius = blur_radius;
    cmd.border_radius = border_radius;
    commands_.push_back(cmd);
}

void DisplayList::draw_text(const std::string& text, float x, float y, float font_size,
                            const Color& color, const std::string& font_family,
                            int font_weight, bool font_italic,
                            float letter_spacing, float word_spacing, int tab_size) {
    PaintCommand cmd;
    cmd.type = PaintCommand::DrawText;
    cmd.text = text;
    cmd.bounds.x = x;
    cmd.bounds.y = y;
    // Approximate text bounds: each char is font_size * 0.6 wide + letter_spacing
    cmd.bounds.width = static_cast<float>(text.size()) * (font_size * 0.6f + letter_spacing);
    cmd.bounds.height = font_size;
    cmd.font_size = font_size;
    cmd.font_weight = font_weight;
    cmd.font_italic = font_italic;
    cmd.letter_spacing = letter_spacing;
    cmd.word_spacing = word_spacing;
    cmd.tab_size = tab_size;
    cmd.font_family = font_family;
    cmd.color = color;
    commands_.push_back(cmd);
}

void DisplayList::draw_border(const Rect& rect, const Color& color,
                              float top, float right, float bottom, float left,
                              float border_radius, int border_style) {
    PaintCommand cmd;
    cmd.type = PaintCommand::DrawBorder;
    cmd.bounds = rect;
    cmd.color = color;
    cmd.border_widths[0] = top;
    cmd.border_widths[1] = right;
    cmd.border_widths[2] = bottom;
    cmd.border_widths[3] = left;
    cmd.border_radius = border_radius;
    cmd.border_style = border_style;
    commands_.push_back(cmd);
}

void DisplayList::push_clip(const Rect& clip_rect) {
    PaintCommand cmd;
    cmd.type = PaintCommand::PushClip;
    cmd.bounds = clip_rect;
    commands_.push_back(cmd);
}

void DisplayList::pop_clip() {
    PaintCommand cmd;
    cmd.type = PaintCommand::PopClip;
    commands_.push_back(cmd);
}

void DisplayList::add_link(const Rect& bounds, const std::string& href, const std::string& target) {
    links_.push_back({bounds, href, target});
}

void DisplayList::add_cursor_region(const Rect& bounds, int cursor_type) {
    cursor_regions_.push_back({bounds, cursor_type});
}

void DisplayList::add_form_submit_region(const Rect& bounds, int form_index) {
    form_submit_regions_.push_back({bounds, form_index});
}

void DisplayList::add_details_toggle_region(const Rect& bounds, int details_id) {
    details_toggle_regions_.push_back({bounds, details_id});
}

void DisplayList::add_select_click_region(const Rect& bounds, const std::vector<std::string>& options,
                                           int selected_index, const std::string& name) {
    select_click_regions_.push_back({bounds, options, selected_index, name});
}

void DisplayList::fill_gradient(const Rect& rect, float angle,
                                const std::vector<std::pair<uint32_t, float>>& stops,
                                float border_radius,
                                int gradient_type, int radial_shape) {
    PaintCommand cmd;
    cmd.type = PaintCommand::FillRect;
    cmd.bounds = rect;
    cmd.color = {0, 0, 0, 0}; // unused when gradient_stops is set
    cmd.border_radius = border_radius;
    cmd.gradient_type = gradient_type;
    cmd.gradient_angle = angle;
    cmd.radial_shape = radial_shape;
    cmd.gradient_stops = stops;
    commands_.push_back(std::move(cmd));
}

void DisplayList::draw_image(const Rect& dest, std::shared_ptr<ImageData> image, int image_rendering) {
    PaintCommand cmd;
    cmd.type = PaintCommand::DrawImage;
    cmd.bounds = dest;
    cmd.image = std::move(image);
    cmd.image_rendering = image_rendering;
    commands_.push_back(std::move(cmd));
}

void DisplayList::set_last_font_features(const std::string& features, const std::string& variations) {
    if (!commands_.empty()) {
        commands_.back().font_feature_settings = features;
        commands_.back().font_variation_settings = variations;
    }
}

void DisplayList::set_last_text_hints(int text_rendering, int font_kerning, int font_optical_sizing) {
    if (!commands_.empty()) {
        commands_.back().text_rendering = text_rendering;
        commands_.back().font_kerning = font_kerning;
        commands_.back().font_optical_sizing = font_optical_sizing;
    }
}

void DisplayList::push_translate(float tx, float ty) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyTransform;
    cmd.transform_type = 1; // translate
    cmd.transform_x = tx;
    cmd.transform_y = ty;
    commands_.push_back(cmd);
}

void DisplayList::push_rotate(float angle_deg, float origin_x, float origin_y) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyTransform;
    cmd.transform_type = 2; // rotate
    cmd.transform_angle = angle_deg;
    cmd.transform_origin_x = origin_x;
    cmd.transform_origin_y = origin_y;
    commands_.push_back(cmd);
}

void DisplayList::push_scale(float sx, float sy, float origin_x, float origin_y) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyTransform;
    cmd.transform_type = 3; // scale
    cmd.transform_x = sx;
    cmd.transform_y = sy;
    cmd.transform_origin_x = origin_x;
    cmd.transform_origin_y = origin_y;
    commands_.push_back(cmd);
}

void DisplayList::push_skew(float ax_deg, float ay_deg, float origin_x, float origin_y) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyTransform;
    cmd.transform_type = 4; // skew
    cmd.transform_x = ax_deg;
    cmd.transform_y = ay_deg;
    cmd.transform_origin_x = origin_x;
    cmd.transform_origin_y = origin_y;
    commands_.push_back(cmd);
}

void DisplayList::push_matrix(float a, float b, float c, float d, float e, float f) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyTransform;
    cmd.transform_type = 5; // matrix
    cmd.transform_x = a;
    cmd.transform_y = b;
    cmd.transform_angle = c;
    cmd.transform_origin_x = d;
    cmd.transform_origin_y = e;
    cmd.transform_extra = f;
    commands_.push_back(cmd);
}

void DisplayList::pop_transform() {
    PaintCommand cmd;
    cmd.type = PaintCommand::ResetTransform;
    commands_.push_back(cmd);
}

void DisplayList::apply_filter(const Rect& bounds, int filter_type, float filter_value) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyFilter;
    cmd.bounds = bounds;
    cmd.filter_type = filter_type;
    cmd.filter_value = filter_value;
    commands_.push_back(cmd);
}

void DisplayList::apply_drop_shadow(const Rect& bounds, float blur_radius,
                                     float offset_x, float offset_y, uint32_t shadow_color) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyFilter;
    cmd.bounds = bounds;
    cmd.filter_type = 10;
    cmd.filter_value = blur_radius;
    cmd.drop_shadow_ox = offset_x;
    cmd.drop_shadow_oy = offset_y;
    cmd.drop_shadow_color = shadow_color;
    commands_.push_back(cmd);
}

void DisplayList::apply_backdrop_filter(const Rect& bounds, int filter_type, float filter_value) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyBackdropFilter;
    cmd.bounds = bounds;
    cmd.filter_type = filter_type;
    cmd.filter_value = filter_value;
    commands_.push_back(cmd);
}

void DisplayList::draw_ellipse(float cx, float cy, float rx, float ry,
                                const Color& fill_color, const Color& stroke_color, float stroke_width) {
    PaintCommand cmd;
    cmd.type = PaintCommand::DrawEllipse;
    cmd.center_x = cx;
    cmd.center_y = cy;
    cmd.radius_x = rx;
    cmd.radius_y = ry;
    cmd.color = fill_color;
    cmd.bounds = {cx - rx, cy - ry, rx * 2, ry * 2};
    cmd.stroke_width = stroke_width;
    // Store stroke color in border_widths as a hack (reuse fields)
    cmd.border_widths[0] = static_cast<float>(stroke_color.r);
    cmd.border_widths[1] = static_cast<float>(stroke_color.g);
    cmd.border_widths[2] = static_cast<float>(stroke_color.b);
    cmd.border_widths[3] = static_cast<float>(stroke_color.a);
    commands_.push_back(cmd);
}

void DisplayList::apply_clip_path(const Rect& bounds, int type, const std::vector<float>& values) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyClipPath;
    cmd.bounds = bounds;
    cmd.clip_path_type = type;
    cmd.clip_path_values = values;
    commands_.push_back(std::move(cmd));
}

void DisplayList::save_backdrop(const Rect& bounds) {
    PaintCommand cmd;
    cmd.type = PaintCommand::SaveBackdrop;
    cmd.bounds = bounds;
    commands_.push_back(cmd);
}

void DisplayList::apply_blend_mode(const Rect& bounds, int blend_mode) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyBlendMode;
    cmd.bounds = bounds;
    cmd.blend_mode = blend_mode;
    commands_.push_back(cmd);
}

void DisplayList::apply_mask_gradient(const Rect& bounds, float angle,
                                      const std::vector<std::pair<uint32_t, float>>& stops) {
    PaintCommand cmd;
    cmd.type = PaintCommand::ApplyMaskGradient;
    cmd.bounds = bounds;
    cmd.gradient_angle = angle;
    cmd.gradient_stops = stops;
    commands_.push_back(cmd);
}

void DisplayList::draw_line(float x1, float y1, float x2, float y2,
                             const Color& color, float stroke_width) {
    PaintCommand cmd;
    cmd.type = PaintCommand::DrawLine;
    cmd.line_x1 = x1;
    cmd.line_y1 = y1;
    cmd.line_x2 = x2;
    cmd.line_y2 = y2;
    cmd.color = color;
    cmd.stroke_width = stroke_width;
    // Set bounds for clipping
    cmd.bounds = {std::min(x1, x2), std::min(y1, y2),
                  std::abs(x2 - x1), std::abs(y2 - y1)};
    commands_.push_back(cmd);
}

} // namespace clever::paint
