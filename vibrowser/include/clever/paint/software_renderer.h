#pragma once
#include <clever/paint/display_list.h>
#include <cstdint>
#include <memory>
#include <string>
#include <stack>
#include <vector>

namespace clever::paint {

// Forward declaration
class TextRenderer;

// 2D affine transform matrix: [a b tx; c d ty; 0 0 1]
struct AffineTransform {
    float a = 1, b = 0, tx = 0;
    float c = 0, d = 1, ty = 0;

    // Identity
    static AffineTransform identity() { return {1, 0, 0, 0, 1, 0}; }

    // Apply this transform to a point (px, py) -> (ox, oy)
    void apply(float px, float py, float& ox, float& oy) const {
        ox = a * px + b * py + tx;
        oy = c * px + d * py + ty;
    }

    // Apply the inverse transform to map screen coords back to local coords
    void apply_inverse(float px, float py, float& ox, float& oy) const {
        float det = a * d - b * c;
        if (det == 0) { ox = px; oy = py; return; }
        float inv_det = 1.0f / det;
        float ia =  d * inv_det;
        float ib = -b * inv_det;
        float ic = -c * inv_det;
        float id =  a * inv_det;
        float itx = -(ia * tx + ib * ty);
        float ity = -(ic * tx + id * ty);
        ox = ia * px + ib * py + itx;
        oy = ic * px + id * py + ity;
    }

    // Concatenate: this * other
    AffineTransform operator*(const AffineTransform& o) const {
        AffineTransform r;
        r.a  = a * o.a  + b * o.c;
        r.b  = a * o.b  + b * o.d;
        r.tx = a * o.tx + b * o.ty + tx;
        r.c  = c * o.a  + d * o.c;
        r.d  = c * o.b  + d * o.d;
        r.ty = c * o.tx + d * o.ty + ty;
        return r;
    }

    bool is_identity() const {
        return a == 1 && b == 0 && tx == 0 && c == 0 && d == 1 && ty == 0;
    }
};

class SoftwareRenderer {
public:
    SoftwareRenderer(int width, int height);
    ~SoftwareRenderer();

    // Render a display list to the pixel buffer
    void render(const DisplayList& list);

    // Clear with a color
    void clear(const Color& color);

    // Get pixel at position
    Color get_pixel(int x, int y) const;

    // Set pixel at position (with alpha blending)
    void set_pixel(int x, int y, const Color& color);

    // Get dimensions
    int width() const { return width_; }
    int height() const { return height_; }

    // Save as PPM image
    bool save_ppm(const std::string& filename) const;

    // Save as PNG image (uses stb_image_write)
    bool save_png(const std::string& filename) const;

    // Get raw pixel buffer (RGBA)
    const std::vector<uint8_t>& pixels() const { return pixels_; }

private:
    int width_, height_;
    std::vector<uint8_t> pixels_;  // RGBA, row-major
    std::unique_ptr<TextRenderer> text_renderer_;
    std::stack<Rect> clip_stack_;
    std::stack<AffineTransform> transform_stack_;
    AffineTransform current_transform_ = AffineTransform::identity();

    // Set pixel with transform applied (maps through current_transform_)
    void set_pixel_transformed(float fx, float fy, const Color& color);

    void draw_filled_rect(const Rect& rect, const Color& color, float border_radius = 0,
                          float r_tl = 0, float r_tr = 0, float r_bl = 0, float r_br = 0);
    void draw_box_shadow(const Rect& shadow_rect, const Rect& element_rect,
                         const Color& color, float blur_radius, float border_radius,
                         float r_tl = 0, float r_tr = 0, float r_bl = 0, float r_br = 0);
    void draw_gradient_rect(const Rect& rect, float angle,
                            const std::vector<std::pair<uint32_t, float>>& stops,
                            float border_radius = 0, bool repeating = false,
                            float r_tl = 0, float r_tr = 0, float r_bl = 0, float r_br = 0);
    void draw_radial_gradient_rect(const Rect& rect, int radial_shape,
                                    const std::vector<std::pair<uint32_t, float>>& stops,
                                    float border_radius = 0, bool repeating = false,
                                    float r_tl = 0, float r_tr = 0, float r_bl = 0, float r_br = 0);
    void draw_conic_gradient_rect(const Rect& rect, float from_angle,
                                   const std::vector<std::pair<uint32_t, float>>& stops,
                                   float border_radius = 0, bool repeating = false,
                                   float r_tl = 0, float r_tr = 0, float r_bl = 0, float r_br = 0);
    void draw_image(const Rect& dest, const ImageData& image, int image_rendering = 0);
    void draw_text_simple(const std::string& text, float x, float y, float font_size, const Color& color,
                          const std::string& font_family = "", int font_weight = 400, bool font_italic = false,
                          float letter_spacing = 0,
                          const std::string& font_feature_settings = "",
                          const std::string& font_variation_settings = "",
                          int text_rendering = 0, int font_kerning = 0,
                          int font_optical_sizing = 0);
    void draw_char_bitmap(char c, int x, int y, int char_width, int char_height, const Color& color);
    void draw_filled_ellipse(float cx, float cy, float rx, float ry, const Color& color);
    void draw_stroked_ellipse(float cx, float cy, float rx, float ry, const Color& color, float stroke_width);
    void draw_line_segment(float x1, float y1, float x2, float y2, const Color& color, float stroke_width);
    void apply_filter_to_region(const Rect& bounds, int filter_type, float filter_value);
    void apply_drop_shadow_to_region(const Rect& bounds, float blur_radius,
                                      float offset_x, float offset_y, uint32_t shadow_color);
    void apply_backdrop_filter_to_region(const Rect& bounds, int filter_type, float filter_value);
    void apply_clip_path_mask(const Rect& bounds, int clip_type, const std::vector<float>& values);
    void apply_blend_mode_to_region(const Rect& bounds, int blend_mode);
    void apply_mask_gradient_to_region(const Rect& bounds, float angle,
                                       const std::vector<std::pair<uint32_t, float>>& stops);

    // Stack of saved backdrop pixel snapshots for mix-blend-mode
    struct BackdropSnapshot {
        Rect bounds;
        std::vector<uint8_t> pixels; // RGBA snapshot of the region
    };
    std::stack<BackdropSnapshot> backdrop_stack_;
};

} // namespace clever::paint
