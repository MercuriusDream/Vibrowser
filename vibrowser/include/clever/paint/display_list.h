#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace clever::paint {

struct Rect {
    float x, y, width, height;
    bool contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

struct Color {
    uint8_t r, g, b, a;
    static Color from_argb(uint32_t argb) {
        return {
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF),
            static_cast<uint8_t>((argb >> 24) & 0xFF)
        };
    }
    uint32_t to_argb() const {
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) | b;
    }
};

struct ImageData {
    std::vector<uint8_t> pixels; // RGBA, row-major
    int width = 0;
    int height = 0;
};

struct LinkRegion {
    Rect bounds;
    std::string href;
    std::string target; // target attribute (e.g., "_blank")
};

struct CursorRegion {
    Rect bounds;
    int cursor_type; // 0=auto, 1=default, 2=pointer, 3=text, 4=move, 5=not-allowed
};

// A clickable region for form submission (submit buttons)
struct FormSubmitRegion {
    Rect bounds;
    int form_index; // index into RenderResult::forms
};

// A clickable region for <summary> elements to toggle <details> open/closed
struct DetailsToggleRegion {
    Rect bounds;
    int details_id; // unique ID for the <details> element (DOM tree position)
};

// A clickable region for <select> dropdown elements
struct SelectClickRegion {
    Rect bounds;
    std::vector<std::string> options; // option text values
    int selected_index;               // currently selected option index
    std::string name;                 // form field name
};

// A region mapping a rendered element to its DOM node for JS event dispatch.
// dom_node is a void* pointing to the clever::html::SimpleNode that was rendered
// at this position. Used for hit-testing click events against DOM elements.
struct ElementRegion {
    Rect bounds;
    void* dom_node = nullptr; // SimpleNode* cast to void* to avoid header dependency
};

struct PaintCommand {
    enum Type {
        FillRect, DrawText, DrawBorder, DrawImage, PushClip, PopClip,
        FillBoxShadow, ApplyTransform, ResetTransform, ApplyFilter,
        ApplyBackdropFilter,
        DrawEllipse, DrawLine,
        ApplyClipPath,
        SaveBackdrop, ApplyBlendMode,
        ApplyMaskGradient
    };
    Type type;
    Rect bounds;
    Color color;
    std::string text;
    float font_size = 16.0f;
    int font_weight = 400;
    bool font_italic = false;
    float letter_spacing = 0;
    float word_spacing = 0;
    int tab_size = 4;
    std::string font_family;  // e.g. "monospace", "sans-serif"
    int text_decoration = 0;  // 0=none, 1=underline, 2=line-through
    float border_radius = 0;            // corner radius
    float border_widths[4] = {0,0,0,0}; // top, right, bottom, left
    int border_style = 1;               // 0=none, 1=solid, 2=dashed, 3=dotted
    std::shared_ptr<ImageData> image;    // for DrawImage

    // Gradient data (for FillRect with gradient)
    int gradient_type = 0; // 0=none, 1=linear, 2=radial, 3=conic, 4=repeating-linear, 5=repeating-radial, 6=repeating-conic
    float gradient_angle = 0;
    int radial_shape = 0; // 0=ellipse, 1=circle (radial only)
    std::vector<std::pair<uint32_t, float>> gradient_stops; // {argb, position_0_to_1}

    // Box shadow data (for FillBoxShadow)
    float blur_radius = 0;     // Gaussian blur radius
    Rect element_rect = {0, 0, 0, 0}; // The original element rect (before blur expansion)

    // Text shadow data (for DrawText commands)
    float text_shadow_offset_x = 0;
    float text_shadow_offset_y = 0;
    float text_shadow_blur = 0;
    uint32_t text_shadow_color = 0x00000000; // ARGB, transparent = no shadow

    // Transform data (for ApplyTransform)
    int transform_type = 0; // 0=none, 1=translate, 2=rotate, 3=scale, 4=skew, 5=matrix
    float transform_x = 0;  // translate x, scale x, matrix a
    float transform_y = 0;  // translate y, scale y, matrix b
    float transform_angle = 0; // rotate angle, matrix c
    float transform_origin_x = 0; // origin x, matrix d
    float transform_origin_y = 0; // origin y, matrix e(tx)
    float transform_extra = 0; // matrix f(ty)

    // Filter data (for ApplyFilter)
    // filter_type: 1=grayscale, 2=sepia, 3=brightness, 4=contrast,
    //              5=invert, 6=saturate, 7=opacity, 8=hue-rotate, 9=blur, 10=drop-shadow
    int filter_type = 0;
    float filter_value = 0; // amount (0-1 for most, degrees for hue-rotate, px for blur)
    // drop-shadow extra data (for filter_type=10)
    float drop_shadow_ox = 0;
    float drop_shadow_oy = 0;
    uint32_t drop_shadow_color = 0xFF000000;

    // Ellipse data (for DrawEllipse)
    float center_x = 0, center_y = 0;
    float radius_x = 0, radius_y = 0;

    // Line data (for DrawLine)
    float line_x1 = 0, line_y1 = 0;
    float line_x2 = 0, line_y2 = 0;
    float stroke_width = 1.0f;

    // Clip-path data (for ApplyClipPath)
    // clip_path_type: 0=none, 1=circle, 2=ellipse, 3=inset, 4=polygon
    int clip_path_type = 0;
    std::vector<float> clip_path_values;

    // Image rendering hint (for DrawImage)
    // 0=auto (bilinear), 1=smooth, 2=high-quality, 3=crisp-edges (nearest), 4=pixelated (nearest)
    int image_rendering = 0;

    // OpenType font features (for DrawText) — e.g., "smcp" 1, "liga" 0
    std::string font_feature_settings;
    // Variable font axes (for DrawText) — e.g., "wght" 600, "wdth" 75
    std::string font_variation_settings;

    // Text rendering hint: 0=auto, 1=optimizeSpeed, 2=optimizeLegibility, 3=geometricPrecision
    int text_rendering = 0;
    // Font kerning: 0=auto, 1=normal, 2=none
    int font_kerning = 0;
    // Font optical sizing: 0=auto, 1=none
    int font_optical_sizing = 0;

    // Blend mode data (for ApplyBlendMode / SaveBackdrop)
    // blend_mode: 0=normal, 1=multiply, 2=screen, 3=overlay,
    //             4=darken, 5=lighten, 6=color-dodge, 7=color-burn
    int blend_mode = 0;

    // Mask gradient data (for ApplyMaskGradient)
    // Uses gradient_angle and gradient_stops fields from above
    int mask_direction = 0; // 0=to bottom (default), 1=to right, 2=to top, 3=to left
};

class DisplayList {
public:
    void fill_rect(const Rect& rect, const Color& color);
    void fill_rounded_rect(const Rect& rect, const Color& color, float radius);
    void fill_box_shadow(const Rect& shadow_rect, const Rect& element_rect,
                         const Color& color, float blur_radius, float border_radius);
    void fill_gradient(const Rect& rect, float angle,
                       const std::vector<std::pair<uint32_t, float>>& stops,
                       float border_radius = 0,
                       int gradient_type = 1, int radial_shape = 0);
    void draw_text(const std::string& text, float x, float y, float font_size, const Color& color,
                   const std::string& font_family = "", int font_weight = 400, bool font_italic = false,
                   float letter_spacing = 0, float word_spacing = 0, int tab_size = 4,
                   float text_shadow_blur = 0);
    void draw_border(const Rect& rect, const Color& color, float top, float right, float bottom, float left,
                     float border_radius = 0, int border_style = 1);
    void draw_image(const Rect& dest, std::shared_ptr<ImageData> image, int image_rendering = 0);
    void set_last_font_features(const std::string& features, const std::string& variations);
    void set_last_text_hints(int text_rendering, int font_kerning, int font_optical_sizing);
    void push_clip(const Rect& clip_rect);
    void pop_clip();
    void push_translate(float tx, float ty);
    void push_rotate(float angle_deg, float origin_x, float origin_y);
    void push_scale(float sx, float sy, float origin_x, float origin_y);
    void push_skew(float ax_deg, float ay_deg, float origin_x, float origin_y);
    void push_matrix(float a, float b, float c, float d, float e, float f);
    void pop_transform();
    void apply_filter(const Rect& bounds, int filter_type, float filter_value);
    void apply_drop_shadow(const Rect& bounds, float blur_radius,
                           float offset_x, float offset_y, uint32_t shadow_color);
    void apply_backdrop_filter(const Rect& bounds, int filter_type, float filter_value);
    void draw_ellipse(float cx, float cy, float rx, float ry,
                      const Color& fill_color, const Color& stroke_color, float stroke_width);
    void draw_line(float x1, float y1, float x2, float y2,
                   const Color& color, float stroke_width);
    void apply_clip_path(const Rect& bounds, int type, const std::vector<float>& values);
    void save_backdrop(const Rect& bounds);
    void apply_blend_mode(const Rect& bounds, int blend_mode);
    void apply_mask_gradient(const Rect& bounds, float angle,
                             const std::vector<std::pair<uint32_t, float>>& stops);
    void add_link(const Rect& bounds, const std::string& href, const std::string& target = "");
    void add_cursor_region(const Rect& bounds, int cursor_type);
    void add_form_submit_region(const Rect& bounds, int form_index);
    void add_details_toggle_region(const Rect& bounds, int details_id);
    void add_select_click_region(const Rect& bounds, const std::vector<std::string>& options,
                                 int selected_index, const std::string& name);

    const std::vector<PaintCommand>& commands() const { return commands_; }
    const std::vector<LinkRegion>& links() const { return links_; }
    const std::vector<CursorRegion>& cursor_regions() const { return cursor_regions_; }
    const std::vector<FormSubmitRegion>& form_submit_regions() const { return form_submit_regions_; }
    const std::vector<DetailsToggleRegion>& details_toggle_regions() const { return details_toggle_regions_; }
    const std::vector<SelectClickRegion>& select_click_regions() const { return select_click_regions_; }
    size_t size() const { return commands_.size(); }
    void clear() { commands_.clear(); links_.clear(); cursor_regions_.clear(); form_submit_regions_.clear(); details_toggle_regions_.clear(); select_click_regions_.clear(); }

private:
    std::vector<PaintCommand> commands_;
    std::vector<LinkRegion> links_;
    std::vector<CursorRegion> cursor_regions_;
    std::vector<FormSubmitRegion> form_submit_regions_;
    std::vector<DetailsToggleRegion> details_toggle_regions_;
    std::vector<SelectClickRegion> select_click_regions_;
};

} // namespace clever::paint
