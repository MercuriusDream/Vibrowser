#include <clever/paint/software_renderer.h>
#include <clever/paint/text_renderer.h>
#include <stb_image_write.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace clever::paint {

SoftwareRenderer::SoftwareRenderer(int width, int height)
    : width_(width), height_(height), pixels_(width * height * 4, 0),
      text_renderer_(std::make_unique<TextRenderer>()) {
}

SoftwareRenderer::~SoftwareRenderer() = default;

void SoftwareRenderer::render(const DisplayList& list) {
    for (auto& cmd : list.commands()) {
        switch (cmd.type) {
            case PaintCommand::FillRect:
                if (!cmd.gradient_stops.empty()) {
                    if (cmd.gradient_type == 3 || cmd.gradient_type == 6) {
                        draw_conic_gradient_rect(cmd.bounds, cmd.gradient_angle,
                                                  cmd.gradient_stops, cmd.border_radius,
                                                  cmd.gradient_type == 6,
                                                  cmd.border_radius_tl, cmd.border_radius_tr,
                                                  cmd.border_radius_bl, cmd.border_radius_br);
                    } else if (cmd.gradient_type == 2 || cmd.gradient_type == 5) {
                        draw_radial_gradient_rect(cmd.bounds, cmd.radial_shape,
                                                   cmd.gradient_stops, cmd.border_radius,
                                                   cmd.gradient_type == 5,
                                                   cmd.border_radius_tl, cmd.border_radius_tr,
                                                   cmd.border_radius_bl, cmd.border_radius_br);
                    } else {
                        draw_gradient_rect(cmd.bounds, cmd.gradient_angle,
                                           cmd.gradient_stops, cmd.border_radius,
                                           cmd.gradient_type == 4,
                                           cmd.border_radius_tl, cmd.border_radius_tr,
                                           cmd.border_radius_bl, cmd.border_radius_br);
                    }
                } else {
                    draw_filled_rect(cmd.bounds, cmd.color, cmd.border_radius,
                                     cmd.border_radius_tl, cmd.border_radius_tr,
                                     cmd.border_radius_bl, cmd.border_radius_br);
                }
                break;
            case PaintCommand::DrawText: {
                // Expand tabs based on tab_size before rendering
                std::string render_text = cmd.text;
                if (render_text.find('\t') != std::string::npos) {
                    std::string expanded;
                    int ts = (cmd.tab_size > 0) ? cmd.tab_size : 4;
                    for (char c : render_text) {
                        if (c == '\t') {
                            expanded.append(static_cast<size_t>(ts), ' ');
                        } else {
                            expanded += c;
                        }
                    }
                    render_text = std::move(expanded);
                }
                auto render_text_command = [&]() {
                    // Handle word_spacing: draw word-by-word with extra spacing
                    if (cmd.word_spacing != 0) {
                        float char_advance = cmd.font_size * 0.6f + cmd.letter_spacing;
                        float cursor_x = cmd.bounds.x;
                        size_t i = 0;
                        while (i < render_text.size()) {
                            // Find next space
                            size_t sp = render_text.find(' ', i);
                            if (sp == std::string::npos) sp = render_text.size();
                            std::string word = render_text.substr(i, sp - i);
                            if (!word.empty()) {
                                draw_text_simple(word, cursor_x, cmd.bounds.y,
                                                 cmd.font_size, cmd.color, cmd.font_family,
                                                 cmd.font_weight, cmd.font_italic,
                                                 cmd.letter_spacing,
                                                 cmd.font_feature_settings, cmd.font_variation_settings,
                                                 cmd.text_rendering, cmd.font_kerning, cmd.font_optical_sizing);
                                cursor_x += static_cast<float>(word.size()) * char_advance;
                            }
                            if (sp < render_text.size()) {
                                // Space character: advance by char_advance + word_spacing
                                cursor_x += char_advance + cmd.word_spacing;
                            }
                            i = sp + 1;
                            if (sp == render_text.size()) break;
                        }
                    } else {
                        draw_text_simple(render_text, cmd.bounds.x, cmd.bounds.y,
                                         cmd.font_size, cmd.color, cmd.font_family,
                                         cmd.font_weight, cmd.font_italic,
                                         cmd.letter_spacing,
                                         cmd.font_feature_settings, cmd.font_variation_settings,
                                         cmd.text_rendering, cmd.font_kerning, cmd.font_optical_sizing);
                    }
                };

                if (cmd.text_shadow_blur <= 0.0f) {
                    render_text_command();
                    break;
                }

                size_t pixel_bytes = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4;
                if (pixel_bytes == 0) break;

                // Render shadow text into an isolated transparent layer first.
                std::vector<uint8_t> backdrop_pixels;
                backdrop_pixels.swap(pixels_);
                pixels_.assign(pixel_bytes, 0);
                render_text_command();
                std::vector<uint8_t> shadow_layer;
                shadow_layer.swap(pixels_);
                pixels_.swap(backdrop_pixels);

                // Find tight alpha bounds for the rendered shadow layer.
                int min_x = width_;
                int min_y = height_;
                int max_x = -1;
                int max_y = -1;
                for (int py = 0; py < height_; py++) {
                    for (int px = 0; px < width_; px++) {
                        size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4 + 3;
                        if (shadow_layer[idx] > 0) {
                            if (px < min_x) min_x = px;
                            if (px > max_x) max_x = px;
                            if (py < min_y) min_y = py;
                            if (py > max_y) max_y = py;
                        }
                    }
                }
                if (max_x < min_x || max_y < min_y) break;

                float sigma = cmd.text_shadow_blur / 2.0f;
                if (sigma <= 0.01f) {
                    for (int py = min_y; py <= max_y; py++) {
                        for (int px = min_x; px <= max_x; px++) {
                            size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4 + 3;
                            uint8_t alpha = shadow_layer[idx];
                            if (alpha == 0) continue;
                            Color c = cmd.color;
                            c.a = alpha;
                            set_pixel(px, py, c);
                        }
                    }
                    break;
                }

                int radius = static_cast<int>(std::ceil(sigma * 3.0f));
                if (radius < 1) radius = 1;
                if (radius > 128) radius = 128;

                int bx0 = std::max(0, min_x - radius);
                int by0 = std::max(0, min_y - radius);
                int bx1 = std::min(width_ - 1, max_x + radius);
                int by1 = std::min(height_ - 1, max_y + radius);
                int bw = bx1 - bx0 + 1;
                int bh = by1 - by0 + 1;
                if (bw <= 0 || bh <= 0) break;

                std::vector<float> kernel(static_cast<size_t>(radius * 2 + 1), 0.0f);
                float kernel_sum = 0.0f;
                for (int k = -radius; k <= radius; k++) {
                    float value = std::exp(-(static_cast<float>(k * k)) / (2.0f * sigma * sigma));
                    kernel[static_cast<size_t>(k + radius)] = value;
                    kernel_sum += value;
                }
                if (kernel_sum > 0.0f) {
                    for (float& w : kernel) w /= kernel_sum;
                }

                std::vector<uint8_t> src_alpha(static_cast<size_t>(bw * bh), 0);
                for (int py = 0; py < bh; py++) {
                    int sy = by0 + py;
                    for (int px = 0; px < bw; px++) {
                        int sx = bx0 + px;
                        size_t src_idx = (static_cast<size_t>(sy) * static_cast<size_t>(width_) + static_cast<size_t>(sx)) * 4 + 3;
                        src_alpha[static_cast<size_t>(py) * static_cast<size_t>(bw) + static_cast<size_t>(px)] = shadow_layer[src_idx];
                    }
                }

                std::vector<uint8_t> temp_alpha(static_cast<size_t>(bw * bh), 0);
                std::vector<uint8_t> blurred_alpha(static_cast<size_t>(bw * bh), 0);

                // Horizontal Gaussian pass.
                for (int py = 0; py < bh; py++) {
                    for (int px = 0; px < bw; px++) {
                        float accum = 0.0f;
                        float weight_sum = 0.0f;
                        for (int k = -radius; k <= radius; k++) {
                            int sx = px + k;
                            if (sx < 0 || sx >= bw) continue;
                            float weight = kernel[static_cast<size_t>(k + radius)];
                            accum += static_cast<float>(src_alpha[static_cast<size_t>(py) * static_cast<size_t>(bw) + static_cast<size_t>(sx)]) * weight;
                            weight_sum += weight;
                        }
                        float alpha = (weight_sum > 0.0f) ? (accum / weight_sum) : 0.0f;
                        temp_alpha[static_cast<size_t>(py) * static_cast<size_t>(bw) + static_cast<size_t>(px)] =
                            static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
                    }
                }

                // Vertical Gaussian pass.
                for (int py = 0; py < bh; py++) {
                    for (int px = 0; px < bw; px++) {
                        float accum = 0.0f;
                        float weight_sum = 0.0f;
                        for (int k = -radius; k <= radius; k++) {
                            int sy = py + k;
                            if (sy < 0 || sy >= bh) continue;
                            float weight = kernel[static_cast<size_t>(k + radius)];
                            accum += static_cast<float>(temp_alpha[static_cast<size_t>(sy) * static_cast<size_t>(bw) + static_cast<size_t>(px)]) * weight;
                            weight_sum += weight;
                        }
                        float alpha = (weight_sum > 0.0f) ? (accum / weight_sum) : 0.0f;
                        blurred_alpha[static_cast<size_t>(py) * static_cast<size_t>(bw) + static_cast<size_t>(px)] =
                            static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
                    }
                }

                // Composite blurred shadow back onto the main framebuffer.
                for (int py = 0; py < bh; py++) {
                    int dy = by0 + py;
                    for (int px = 0; px < bw; px++) {
                        int dx = bx0 + px;
                        uint8_t alpha = blurred_alpha[static_cast<size_t>(py) * static_cast<size_t>(bw) + static_cast<size_t>(px)];
                        if (alpha == 0) continue;
                        Color c = cmd.color;
                        c.a = alpha;
                        set_pixel(dx, dy, c);
                    }
                }
                break;
            }
            case PaintCommand::DrawImage:
                if (cmd.image) {
                    draw_image(cmd.bounds, *cmd.image, cmd.image_rendering);
                }
                break;
            case PaintCommand::DrawBorder: {
                // border_style: 0=none, 1=solid, 2=dashed, 3=dotted
                if (cmd.border_style == 0) break; // none: skip drawing

                float x = cmd.bounds.x;
                float y = cmd.bounds.y;
                float w = cmd.bounds.width;
                float h = cmd.bounds.height;
                float bt = cmd.border_widths[0];
                float br = cmd.border_widths[1];
                float bb = cmd.border_widths[2];
                float bl = cmd.border_widths[3];

                // Resolve per-corner radii for borders (per-corner takes precedence over uniform)
                auto resolve_border_radii = [&](float r_unif,
                                                float& r_tl, float& r_tr, float& r_bl, float& r_br) {
                    bool has_per = (cmd.border_radius_tl > 0 || cmd.border_radius_tr > 0 ||
                                    cmd.border_radius_bl > 0 || cmd.border_radius_br > 0);
                    if (has_per) {
                        r_tl = cmd.border_radius_tl;
                        r_tr = cmd.border_radius_tr;
                        r_bl = cmd.border_radius_bl;
                        r_br = cmd.border_radius_br;
                    } else {
                        r_tl = r_tr = r_bl = r_br = r_unif;
                    }
                    // Clamp each corner radius to half-dimensions
                    float half_w = w / 2.0f, half_h = h / 2.0f;
                    r_tl = std::min(r_tl, std::min(half_w, half_h));
                    r_tr = std::min(r_tr, std::min(half_w, half_h));
                    r_bl = std::min(r_bl, std::min(half_w, half_h));
                    r_br = std::min(r_br, std::min(half_w, half_h));
                };

                if (cmd.border_style == 1) {
                    // Solid border (original behavior)
                    bool has_any_radius = (cmd.border_radius > 0 || cmd.border_radius_tl > 0 ||
                                           cmd.border_radius_tr > 0 || cmd.border_radius_bl > 0 ||
                                           cmd.border_radius_br > 0);
                    if (has_any_radius) {
                        // Per-corner SDF for rounded border
                        float r_tl, r_tr, r_bl, r_br;
                        resolve_border_radii(cmd.border_radius, r_tl, r_tr, r_bl, r_br);

                        // Outer SDF: per-corner
                        auto sdf_per_corner = [](float fx, float fy, float rx, float ry,
                                                  float rw, float rh,
                                                  float rtl, float rtr, float rbl, float rbr) -> float {
                            // Determine which corner quadrant the point is in
                            float mid_x = rx + rw / 2.0f;
                            float mid_y = ry + rh / 2.0f;
                            float r;
                            float ccx, ccy;
                            if (fx <= mid_x && fy <= mid_y) {
                                // top-left
                                r = rtl; ccx = rx + rtl; ccy = ry + rtl;
                            } else if (fx > mid_x && fy <= mid_y) {
                                // top-right
                                r = rtr; ccx = rx + rw - rtr; ccy = ry + rtr;
                            } else if (fx <= mid_x && fy > mid_y) {
                                // bottom-left
                                r = rbl; ccx = rx + rbl; ccy = ry + rh - rbl;
                            } else {
                                // bottom-right
                                r = rbr; ccx = rx + rw - rbr; ccy = ry + rh - rbr;
                            }
                            if (r > 0) {
                                bool in_corner = (fx < rx + r || fx > rx + rw - r) &&
                                                 (fy < ry + r || fy > ry + rh - r);
                                if (in_corner) {
                                    return std::sqrt((fx - ccx) * (fx - ccx) + (fy - ccy) * (fy - ccy)) - r;
                                }
                            }
                            float dl = rx - fx, dr = fx - (rx + rw);
                            float dt = ry - fy, db = fy - (ry + rh);
                            return std::max({dl, dr, dt, db});
                        };

                        float bw_avg = (bt + br + bb + bl) / 4.0f;
                        float bw_min_val = (bt > 0 || br > 0 || bb > 0 || bl > 0)
                            ? std::max(0.0f, std::min({bt > 0 ? bt : 1e9f,
                                                       br > 0 ? br : 1e9f,
                                                       bb > 0 ? bb : 1e9f,
                                                       bl > 0 ? bl : 1e9f}))
                            : 0.0f;
                        (void)bw_avg;

                        // Inner radii (shrink by min border width on each corner)
                        float ri_tl = std::max(0.0f, r_tl - bw_min_val);
                        float ri_tr = std::max(0.0f, r_tr - bw_min_val);
                        float ri_bl = std::max(0.0f, r_bl - bw_min_val);
                        float ri_br = std::max(0.0f, r_br - bw_min_val);

                        int x0 = std::max(0, static_cast<int>(std::floor(x)));
                        int y0 = std::max(0, static_cast<int>(std::floor(y)));
                        int x1 = std::min(width_, static_cast<int>(std::ceil(x + w)));
                        int y1 = std::min(height_, static_cast<int>(std::ceil(y + h)));

                        float ix = x + bl, iy = y + bt;
                        float iw = w - bl - br, ih = h - bt - bb;

                        for (int py = y0; py < y1; py++) {
                            float fy = static_cast<float>(py) + 0.5f;
                            for (int px = x0; px < x1; px++) {
                                float fx = static_cast<float>(px) + 0.5f;
                                float d_outer = sdf_per_corner(fx, fy, x, y, w, h, r_tl, r_tr, r_bl, r_br);
                                if (d_outer > 0.5f) continue;
                                float d_inner = (iw > 0 && ih > 0)
                                    ? sdf_per_corner(fx, fy, ix, iy, iw, ih, ri_tl, ri_tr, ri_bl, ri_br)
                                    : 1.0f;
                                if (d_inner < -0.5f) continue;
                                float ao = std::min(1.0f, 0.5f - d_outer);
                                float ai = std::min(1.0f, d_inner + 0.5f);
                                float alpha = std::max(0.0f, std::min(ao, ai));
                                Color c = cmd.color;
                                c.a = static_cast<uint8_t>(c.a * alpha);
                                set_pixel(px, py, c);
                            }
                        }
                    } else {
                        if (bt > 0) draw_filled_rect({x, y, w, bt}, cmd.color);
                        if (bb > 0) draw_filled_rect({x, y + h - bb, w, bb}, cmd.color);
                        if (bl > 0) draw_filled_rect({x, y + bt, bl, h - bt - bb}, cmd.color);
                        if (br > 0) draw_filled_rect({x + w - br, y + bt, br, h - bt - bb}, cmd.color);
                    }
                } else if (cmd.border_style == 2 || cmd.border_style == 3) {
                    // Dashed (2) or Dotted (3) border
                    auto draw_dashed_edge = [&](Rect edge_rect, bool horizontal, float bw, int style) {
                        float seg_len = (style == 3) ? bw : 3.0f * bw;
                        if (seg_len < 1.0f) seg_len = 1.0f;
                        float total_len = horizontal ? edge_rect.width : edge_rect.height;
                        float pos = 0;
                        bool draw = true;
                        while (pos < total_len) {
                            float len = std::min(seg_len, total_len - pos);
                            if (draw) {
                                if (horizontal) {
                                    draw_filled_rect({edge_rect.x + pos, edge_rect.y, len, edge_rect.height}, cmd.color);
                                } else {
                                    draw_filled_rect({edge_rect.x, edge_rect.y + pos, edge_rect.width, len}, cmd.color);
                                }
                            }
                            pos += seg_len;
                            draw = !draw;
                        }
                    };

                    if (bt > 0) draw_dashed_edge({x, y, w, bt}, true, bt, cmd.border_style);
                    if (bb > 0) draw_dashed_edge({x, y + h - bb, w, bb}, true, bb, cmd.border_style);
                    if (bl > 0) draw_dashed_edge({x, y + bt, bl, h - bt - bb}, false, bl, cmd.border_style);
                    if (br > 0) draw_dashed_edge({x + w - br, y + bt, br, h - bt - bb}, false, br, cmd.border_style);
                } else if (cmd.border_style == 4) {
                    // Double: two lines with gap between
                    // Each line is 1/3 of border width, with 1/3 gap
                    auto draw_double_edge = [&](Rect edge_rect, bool horizontal, float bw, Color c) {
                        float third = std::max(1.0f, bw / 3.0f);
                        if (horizontal) {
                            draw_filled_rect({edge_rect.x, edge_rect.y, edge_rect.width, third}, c);
                            draw_filled_rect({edge_rect.x, edge_rect.y + bw - third, edge_rect.width, third}, c);
                        } else {
                            draw_filled_rect({edge_rect.x, edge_rect.y, third, edge_rect.height}, c);
                            draw_filled_rect({edge_rect.x + bw - third, edge_rect.y, third, edge_rect.height}, c);
                        }
                    };
                    if (bt > 0) draw_double_edge({x, y, w, bt}, true, bt, cmd.color);
                    if (bb > 0) draw_double_edge({x, y + h - bb, w, bb}, true, bb, cmd.color);
                    if (bl > 0) draw_double_edge({x, y + bt, bl, h - bt - bb}, false, bl, cmd.color);
                    if (br > 0) draw_double_edge({x + w - br, y + bt, br, h - bt - bb}, false, br, cmd.color);
                } else if (cmd.border_style == 5 || cmd.border_style == 6) {
                    // Groove (5): outer half darker, inner half lighter
                    // Ridge (6): outer half lighter, inner half darker (opposite of groove)
                    auto darken = [](Color c, float f) -> Color {
                        return {static_cast<uint8_t>(c.r * f), static_cast<uint8_t>(c.g * f),
                                static_cast<uint8_t>(c.b * f), c.a};
                    };
                    auto lighten = [](Color c, float f) -> Color {
                        return {static_cast<uint8_t>(c.r + (255 - c.r) * f),
                                static_cast<uint8_t>(c.g + (255 - c.g) * f),
                                static_cast<uint8_t>(c.b + (255 - c.b) * f), c.a};
                    };
                    Color dark = darken(cmd.color, 0.4f);
                    Color light = lighten(cmd.color, 0.5f);
                    Color outer = (cmd.border_style == 5) ? dark : light;
                    Color inner = (cmd.border_style == 5) ? light : dark;
                    // Top
                    if (bt > 0) {
                        float half = bt / 2.0f;
                        draw_filled_rect({x, y, w, half}, outer);
                        draw_filled_rect({x, y + half, w, bt - half}, inner);
                    }
                    // Bottom
                    if (bb > 0) {
                        float half = bb / 2.0f;
                        draw_filled_rect({x, y + h - bb, w, half}, inner);
                        draw_filled_rect({x, y + h - half, w, half}, outer);
                    }
                    // Left
                    if (bl > 0) {
                        float half = bl / 2.0f;
                        draw_filled_rect({x, y + bt, half, h - bt - bb}, outer);
                        draw_filled_rect({x + half, y + bt, bl - half, h - bt - bb}, inner);
                    }
                    // Right
                    if (br > 0) {
                        float half = br / 2.0f;
                        draw_filled_rect({x + w - br, y + bt, half, h - bt - bb}, inner);
                        draw_filled_rect({x + w - half, y + bt, half, h - bt - bb}, outer);
                    }
                } else if (cmd.border_style == 7 || cmd.border_style == 8) {
                    // Inset (7): top/left dark, bottom/right light
                    // Outset (8): top/left light, bottom/right dark
                    auto darken = [](Color c, float f) -> Color {
                        return {static_cast<uint8_t>(c.r * f), static_cast<uint8_t>(c.g * f),
                                static_cast<uint8_t>(c.b * f), c.a};
                    };
                    auto lighten = [](Color c, float f) -> Color {
                        return {static_cast<uint8_t>(c.r + (255 - c.r) * f),
                                static_cast<uint8_t>(c.g + (255 - c.g) * f),
                                static_cast<uint8_t>(c.b + (255 - c.b) * f), c.a};
                    };
                    Color dark = darken(cmd.color, 0.4f);
                    Color light = lighten(cmd.color, 0.5f);
                    Color tl_color = (cmd.border_style == 7) ? dark : light;
                    Color br_color = (cmd.border_style == 7) ? light : dark;
                    if (bt > 0) draw_filled_rect({x, y, w, bt}, tl_color);
                    if (bl > 0) draw_filled_rect({x, y + bt, bl, h - bt - bb}, tl_color);
                    if (bb > 0) draw_filled_rect({x, y + h - bb, w, bb}, br_color);
                    if (br > 0) draw_filled_rect({x + w - br, y + bt, br, h - bt - bb}, br_color);
                }
                break;
            }
            case PaintCommand::FillBoxShadow:
                draw_box_shadow(cmd.bounds, cmd.element_rect, cmd.color,
                                cmd.blur_radius, cmd.border_radius,
                                cmd.border_radius_tl, cmd.border_radius_tr,
                                cmd.border_radius_bl, cmd.border_radius_br);
                break;
            case PaintCommand::PushClip:
                if (clip_stack_.empty()) {
                    clip_stack_.push(cmd.bounds);
                } else {
                    const Rect& current = clip_stack_.top();
                    Rect next;
                    next.x = std::max(current.x, cmd.bounds.x);
                    next.y = std::max(current.y, cmd.bounds.y);
                    float right = std::min(current.x + current.width, cmd.bounds.x + cmd.bounds.width);
                    float bottom = std::min(current.y + current.height, cmd.bounds.y + cmd.bounds.height);
                    next.width = std::max(0.0f, right - next.x);
                    next.height = std::max(0.0f, bottom - next.y);
                    clip_stack_.push(next);
                }
                break;
            case PaintCommand::PopClip:
                if (!clip_stack_.empty()) clip_stack_.pop();
                break;
            case PaintCommand::ApplyTransform: {
                // Save current transform on stack
                transform_stack_.push(current_transform_);
                // Build the new incremental transform
                AffineTransform t = AffineTransform::identity();
                if (cmd.transform_type == 1) {
                    // Translate
                    t.tx = cmd.transform_x;
                    t.ty = cmd.transform_y;
                } else if (cmd.transform_type == 2) {
                    // Rotate around origin
                    float rad = cmd.transform_angle * 3.14159265f / 180.0f;
                    float cos_a = std::cos(rad);
                    float sin_a = std::sin(rad);
                    float ox = cmd.transform_origin_x;
                    float oy = cmd.transform_origin_y;
                    // Translate to origin, rotate, translate back
                    // T(ox,oy) * R(angle) * T(-ox,-oy)
                    t.a = cos_a;  t.b = -sin_a;
                    t.c = sin_a;  t.d =  cos_a;
                    t.tx = ox - cos_a * ox + sin_a * oy;
                    t.ty = oy - sin_a * ox - cos_a * oy;
                } else if (cmd.transform_type == 3) {
                    // Scale around origin
                    float sx = cmd.transform_x;
                    float sy = cmd.transform_y;
                    float ox = cmd.transform_origin_x;
                    float oy = cmd.transform_origin_y;
                    // T(ox,oy) * S(sx,sy) * T(-ox,-oy)
                    t.a = sx; t.d = sy;
                    t.tx = ox - sx * ox;
                    t.ty = oy - sy * oy;
                } else if (cmd.transform_type == 4) {
                    // Skew around origin
                    float ax_rad = cmd.transform_x * 3.14159265f / 180.0f;
                    float ay_rad = cmd.transform_y * 3.14159265f / 180.0f;
                    float ox = cmd.transform_origin_x;
                    float oy = cmd.transform_origin_y;
                    float tan_ax = std::tan(ax_rad);
                    float tan_ay = std::tan(ay_rad);
                    // T(ox,oy) * Skew(ax,ay) * T(-ox,-oy)
                    // Skew matrix: [1 tan(ax); tan(ay) 1]
                    t.a = 1;       t.b = tan_ax;
                    t.c = tan_ay;  t.d = 1;
                    t.tx = ox - ox - tan_ax * oy;
                    t.ty = oy - tan_ay * ox - oy;
                } else if (cmd.transform_type == 5) {
                    // Direct matrix(a, b, c, d, e, f) — CSS matrix values
                    t.a = cmd.transform_x;      // a
                    t.b = cmd.transform_y;      // b
                    t.c = cmd.transform_angle;  // c
                    t.d = cmd.transform_origin_x; // d
                    t.tx = cmd.transform_origin_y; // e (tx)
                    t.ty = cmd.transform_extra;    // f (ty)
                }
                // Concatenate: new = current * incremental
                current_transform_ = current_transform_ * t;
                break;
            }
            case PaintCommand::ResetTransform:
                if (!transform_stack_.empty()) {
                    current_transform_ = transform_stack_.top();
                    transform_stack_.pop();
                } else {
                    current_transform_ = AffineTransform::identity();
                }
                break;
            case PaintCommand::ApplyFilter:
                if (cmd.filter_type == 10) {
                    apply_drop_shadow_to_region(cmd.bounds, cmd.filter_value,
                                                cmd.drop_shadow_ox, cmd.drop_shadow_oy,
                                                cmd.drop_shadow_color);
                } else {
                    apply_filter_to_region(cmd.bounds, cmd.filter_type, cmd.filter_value);
                }
                break;
            case PaintCommand::ApplyBackdropFilter:
                apply_backdrop_filter_to_region(cmd.bounds, cmd.filter_type, cmd.filter_value);
                break;
            case PaintCommand::DrawEllipse: {
                // Fill the ellipse
                if (cmd.color.a > 0) {
                    draw_filled_ellipse(cmd.center_x, cmd.center_y,
                                        cmd.radius_x, cmd.radius_y, cmd.color);
                }
                // Stroke the ellipse if stroke is visible
                uint8_t stroke_a = static_cast<uint8_t>(cmd.border_widths[3]);
                if (stroke_a > 0 && cmd.stroke_width > 0) {
                    Color stroke_color = {
                        static_cast<uint8_t>(cmd.border_widths[0]),
                        static_cast<uint8_t>(cmd.border_widths[1]),
                        static_cast<uint8_t>(cmd.border_widths[2]),
                        stroke_a
                    };
                    draw_stroked_ellipse(cmd.center_x, cmd.center_y,
                                         cmd.radius_x, cmd.radius_y,
                                         stroke_color, cmd.stroke_width);
                }
                break;
            }
            case PaintCommand::DrawLine:
                draw_line_segment(cmd.line_x1, cmd.line_y1,
                                  cmd.line_x2, cmd.line_y2,
                                  cmd.color, cmd.stroke_width);
                break;
            case PaintCommand::ApplyClipPath:
                apply_clip_path_mask(cmd.bounds, cmd.clip_path_type, cmd.clip_path_values);
                break;
            case PaintCommand::SaveBackdrop: {
                // Save a snapshot of the current pixel region for later blend-mode compositing
                int bx0 = std::max(0, static_cast<int>(std::floor(cmd.bounds.x)));
                int by0 = std::max(0, static_cast<int>(std::floor(cmd.bounds.y)));
                int bx1 = std::min(width_, static_cast<int>(std::ceil(cmd.bounds.x + cmd.bounds.width)));
                int by1 = std::min(height_, static_cast<int>(std::ceil(cmd.bounds.y + cmd.bounds.height)));
                int bw = bx1 - bx0;
                int bh = by1 - by0;
                BackdropSnapshot snap;
                snap.bounds = cmd.bounds;
                if (bw > 0 && bh > 0) {
                    snap.pixels.resize(static_cast<size_t>(bw * bh * 4));
                    for (int py = by0; py < by1; py++) {
                        for (int px = bx0; px < bx1; px++) {
                            int src_idx = (py * width_ + px) * 4;
                            int dst_idx = ((py - by0) * bw + (px - bx0)) * 4;
                            snap.pixels[dst_idx]     = pixels_[src_idx];
                            snap.pixels[dst_idx + 1] = pixels_[src_idx + 1];
                            snap.pixels[dst_idx + 2] = pixels_[src_idx + 2];
                            snap.pixels[dst_idx + 3] = pixels_[src_idx + 3];
                        }
                    }
                }
                backdrop_stack_.push(std::move(snap));

                // Clear the region to transparent so we can detect which pixels
                // the element actually paints (alpha > 0 = painted)
                for (int py = by0; py < by1; py++) {
                    for (int px = bx0; px < bx1; px++) {
                        int idx = (py * width_ + px) * 4;
                        pixels_[idx]     = 0;
                        pixels_[idx + 1] = 0;
                        pixels_[idx + 2] = 0;
                        pixels_[idx + 3] = 0;
                    }
                }
                break;
            }
            case PaintCommand::ApplyBlendMode:
                apply_blend_mode_to_region(cmd.bounds, cmd.blend_mode);
                break;
            case PaintCommand::ApplyMaskGradient:
                apply_mask_gradient_to_region(cmd.bounds, cmd.gradient_angle,
                                              cmd.gradient_stops);
                break;
        }
    }
}

void SoftwareRenderer::clear(const Color& color) {
    for (int i = 0; i < width_ * height_; i++) {
        pixels_[i * 4 + 0] = color.r;
        pixels_[i * 4 + 1] = color.g;
        pixels_[i * 4 + 2] = color.b;
        pixels_[i * 4 + 3] = color.a;
    }
}

Color SoftwareRenderer::get_pixel(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return {0, 0, 0, 0};
    }
    int idx = (y * width_ + x) * 4;
    return {pixels_[idx], pixels_[idx + 1], pixels_[idx + 2], pixels_[idx + 3]};
}

void SoftwareRenderer::set_pixel(int x, int y, const Color& color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;

    // Check clip stack
    if (!clip_stack_.empty()) {
        const auto& clip = clip_stack_.top();
        float px = static_cast<float>(x);
        float py = static_cast<float>(y);
        if (px < clip.x || px >= clip.x + clip.width ||
            py < clip.y || py >= clip.y + clip.height) {
            return;
        }
    }

    int idx = (y * width_ + x) * 4;

    if (color.a == 255) {
        // Fully opaque: direct write
        pixels_[idx + 0] = color.r;
        pixels_[idx + 1] = color.g;
        pixels_[idx + 2] = color.b;
        pixels_[idx + 3] = 255;
    } else if (color.a == 0) {
        // Fully transparent: no-op
        return;
    } else {
        // Alpha blending: result = (src * src_a + dst * (255 - src_a)) / 255
        uint8_t dst_r = pixels_[idx + 0];
        uint8_t dst_g = pixels_[idx + 1];
        uint8_t dst_b = pixels_[idx + 2];
        uint8_t dst_a = pixels_[idx + 3];

        uint16_t src_a = color.a;
        uint16_t inv_a = 255 - src_a;

        pixels_[idx + 0] = static_cast<uint8_t>((color.r * src_a + dst_r * inv_a) / 255);
        pixels_[idx + 1] = static_cast<uint8_t>((color.g * src_a + dst_g * inv_a) / 255);
        pixels_[idx + 2] = static_cast<uint8_t>((color.b * src_a + dst_b * inv_a) / 255);
        pixels_[idx + 3] = static_cast<uint8_t>(std::min(255u, static_cast<unsigned>(src_a) + dst_a));
    }
}

void SoftwareRenderer::set_pixel_transformed(float fx, float fy, const Color& color) {
    if (current_transform_.is_identity()) {
        set_pixel(static_cast<int>(fx), static_cast<int>(fy), color);
        return;
    }
    float sx, sy;
    current_transform_.apply(fx, fy, sx, sy);
    set_pixel(static_cast<int>(sx), static_cast<int>(sy), color);
}

bool SoftwareRenderer::save_ppm(const std::string& filename) const {
    FILE* f = fopen(filename.c_str(), "wb");
    if (!f) return false;

    fprintf(f, "P6\n%d %d\n255\n", width_, height_);

    for (int i = 0; i < width_ * height_; i++) {
        uint8_t rgb[3] = {pixels_[i * 4], pixels_[i * 4 + 1], pixels_[i * 4 + 2]};
        fwrite(rgb, 1, 3, f);
    }

    fclose(f);
    return true;
}

bool SoftwareRenderer::save_png(const std::string& filename) const {
    return stbi_write_png(filename.c_str(), width_, height_, 4, pixels_.data(), width_ * 4) != 0;
}

void SoftwareRenderer::draw_filled_rect(const Rect& rect, const Color& color, float border_radius,
                                         float r_tl, float r_tr, float r_bl, float r_br) {
    // Resolve effective per-corner radii. Per-corner values take precedence over the uniform radius.
    bool has_per_corner = (r_tl > 0 || r_tr > 0 || r_bl > 0 || r_br > 0);
    float half_min = std::min(rect.width, rect.height) / 2.0f;
    float eff_tl, eff_tr, eff_bl, eff_br;
    if (has_per_corner) {
        eff_tl = std::min(r_tl, half_min);
        eff_tr = std::min(r_tr, half_min);
        eff_bl = std::min(r_bl, half_min);
        eff_br = std::min(r_br, half_min);
    } else {
        float r = std::min(border_radius, half_min);
        eff_tl = eff_tr = eff_bl = eff_br = r;
    }
    bool has_radius = (eff_tl > 0 || eff_tr > 0 || eff_bl > 0 || eff_br > 0);

    // Corner center coordinates
    float cx_tl = rect.x + eff_tl,       cy_tl = rect.y + eff_tl;
    float cx_tr = rect.x + rect.width - eff_tr,  cy_tr = rect.y + eff_tr;
    float cx_bl = rect.x + eff_bl,       cy_bl = rect.y + rect.height - eff_bl;
    float cx_br = rect.x + rect.width - eff_br,  cy_br = rect.y + rect.height - eff_br;

    // Helper: per-corner rounded rect test, returns (inside, aa_alpha)
    // Returns false if outside, true if inside (aa_alpha is 0..1 for anti-aliasing edge)
    auto corner_test = [&](float px, float py) -> std::pair<bool, float> {
        if (!has_radius) return {true, 1.0f};
        float dx = 0, dy = 0;
        float r = 0;
        bool in_corner_zone = false;

        if (px < cx_tl && py < cy_tl) {
            dx = px - cx_tl; dy = py - cy_tl; r = eff_tl; in_corner_zone = true;
        } else if (px > cx_tr && py < cy_tr) {
            dx = px - cx_tr; dy = py - cy_tr; r = eff_tr; in_corner_zone = true;
        } else if (px < cx_bl && py > cy_bl) {
            dx = px - cx_bl; dy = py - cy_bl; r = eff_bl; in_corner_zone = true;
        } else if (px > cx_br && py > cy_br) {
            dx = px - cx_br; dy = py - cy_br; r = eff_br; in_corner_zone = true;
        }

        if (in_corner_zone) {
            if (r <= 0) return {false, 0.0f}; // corner has zero radius — sharp corner, outside if in zone
            float dist2 = dx * dx + dy * dy;
            if (dist2 > r * r) return {false, 0.0f};
            float dist = std::sqrt(dist2);
            if (dist > r - 1.0f) {
                return {true, std::max(0.0f, std::min(1.0f, r - dist))};
            }
        }
        return {true, 1.0f};
    };

    // When a non-identity transform is active, we need to:
    // 1. Compute the bounding box of the transformed rect in screen space
    // 2. For each screen pixel in that bounding box, inverse-transform to local coords
    // 3. Check if the local coords are inside the rect (with optional border-radius)
    if (!current_transform_.is_identity()) {
        // Transform all four corners of the rect to screen space
        float corners[4][2] = {
            {rect.x, rect.y},
            {rect.x + rect.width, rect.y},
            {rect.x + rect.width, rect.y + rect.height},
            {rect.x, rect.y + rect.height}
        };
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        for (auto& corner : corners) {
            float sx, sy;
            current_transform_.apply(corner[0], corner[1], sx, sy);
            min_x = std::min(min_x, sx);
            min_y = std::min(min_y, sy);
            max_x = std::max(max_x, sx);
            max_y = std::max(max_y, sy);
        }

        int x0 = std::max(0, static_cast<int>(std::floor(min_x)));
        int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
        int x1 = std::min(width_, static_cast<int>(std::ceil(max_x)));
        int y1 = std::min(height_, static_cast<int>(std::ceil(max_y)));

        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                float screen_x = static_cast<float>(x) + 0.5f;
                float screen_y = static_cast<float>(y) + 0.5f;
                // Inverse-transform to local coords
                float lx, ly;
                current_transform_.apply_inverse(screen_x, screen_y, lx, ly);

                // Check if local coord is inside the rect
                if (lx < rect.x || lx >= rect.x + rect.width ||
                    ly < rect.y || ly >= rect.y + rect.height) {
                    continue;
                }

                auto [inside, aa] = corner_test(lx, ly);
                if (!inside) continue;
                if (aa < 1.0f) {
                    Color aa_color = {color.r, color.g, color.b,
                                      static_cast<uint8_t>(color.a * aa)};
                    set_pixel(x, y, aa_color);
                    continue;
                }
                set_pixel(x, y, color);
            }
        }
        return;
    }

    // Non-transformed path
    int x0 = std::max(0, static_cast<int>(std::floor(rect.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(rect.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(rect.x + rect.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(rect.y + rect.height)));

    if (!has_radius) {
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                set_pixel(x, y, color);
            }
        }
        return;
    }

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;
            auto [inside, aa] = corner_test(px, py);
            if (!inside) continue;
            if (aa < 1.0f) {
                Color aa_color = {color.r, color.g, color.b,
                                  static_cast<uint8_t>(color.a * aa)};
                set_pixel(x, y, aa_color);
                continue;
            }
            set_pixel(x, y, color);
        }
    }
}

void SoftwareRenderer::draw_box_shadow(const Rect& shadow_rect, const Rect& element_rect,
                                        const Color& color, float blur_radius,
                                        float border_radius,
                                        float r_tl, float r_tr, float r_bl, float r_br) {
    // Gaussian blur for box-shadow using signed distance field approach.
    // For each pixel in the expanded shadow_rect, compute the distance from
    // the element_rect boundary, then apply Gaussian falloff.
    //
    // The CSS blur radius maps to the Gaussian sigma as: sigma = blur_radius / 2.
    // This follows the CSS spec where the blur radius is approximately 2*sigma.

    int x0 = std::max(0, static_cast<int>(std::floor(shadow_rect.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(shadow_rect.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(shadow_rect.x + shadow_rect.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(shadow_rect.y + shadow_rect.height)));

    float sigma = blur_radius / 2.0f;
    if (sigma < 0.5f) sigma = 0.5f;

    // Precompute 1 / (sigma * sqrt(2)) for the Gaussian CDF (erf approximation)
    float inv_sigma_sqrt2 = 1.0f / (sigma * 1.41421356f);

    // Element rect edges
    float el_left = element_rect.x;
    float el_top = element_rect.y;
    float el_right = element_rect.x + element_rect.width;
    float el_bottom = element_rect.y + element_rect.height;

    // Resolve per-corner radii: per-corner takes precedence over uniform
    bool has_per = (r_tl > 0 || r_tr > 0 || r_bl > 0 || r_br > 0);
    float half_min = std::min(element_rect.width, element_rect.height) / 2.0f;
    float eff_tl, eff_tr, eff_bl, eff_br;
    if (has_per) {
        eff_tl = std::min(r_tl, half_min);
        eff_tr = std::min(r_tr, half_min);
        eff_bl = std::min(r_bl, half_min);
        eff_br = std::min(r_br, half_min);
    } else {
        float r = std::min(border_radius, half_min);
        eff_tl = eff_tr = eff_bl = eff_br = r;
    }
    bool has_radius = (eff_tl > 0 || eff_tr > 0 || eff_bl > 0 || eff_br > 0);

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;

            // Compute signed distance from the element rect.
            // Negative = inside the rect, positive = outside.
            float dist;
            if (has_radius) {
                // Per-corner rounded rect SDF
                // Determine which corner quadrant to use
                float mid_x = el_left + element_rect.width / 2.0f;
                float mid_y = el_top + element_rect.height / 2.0f;
                float r_corner;
                float ccx, ccy;
                if (px <= mid_x && py <= mid_y) {
                    r_corner = eff_tl; ccx = el_left + eff_tl; ccy = el_top + eff_tl;
                } else if (px > mid_x && py <= mid_y) {
                    r_corner = eff_tr; ccx = el_right - eff_tr; ccy = el_top + eff_tr;
                } else if (px <= mid_x && py > mid_y) {
                    r_corner = eff_bl; ccx = el_left + eff_bl; ccy = el_bottom - eff_bl;
                } else {
                    r_corner = eff_br; ccx = el_right - eff_br; ccy = el_bottom - eff_br;
                }

                bool in_corner = (px < el_left + r_corner || px > el_right - r_corner) &&
                                 (py < el_top + r_corner || py > el_bottom - r_corner);
                if (in_corner && r_corner > 0) {
                    float dx = px - ccx, dy = py - ccy;
                    dist = std::sqrt(dx * dx + dy * dy) - r_corner;
                } else {
                    // Distance to nearest edge of the non-rounded rect
                    float d_left = el_left - px;
                    float d_right = px - el_right;
                    float d_top = el_top - py;
                    float d_bottom = py - el_bottom;
                    dist = std::max({d_left, d_right, d_top, d_bottom});
                }
            } else {
                // Simple rectangle signed distance
                float d_left = el_left - px;
                float d_right = px - el_right;
                float d_top = el_top - py;
                float d_bottom = py - el_bottom;
                dist = std::max({d_left, d_right, d_top, d_bottom});
            }

            // Compute Gaussian-blurred alpha using the error function (erf).
            // For a box with Gaussian blur, the coverage at distance d is:
            //   coverage = 0.5 * erfc(d / (sigma * sqrt(2)))
            // which equals 0.5 * (1 - erf(d / (sigma * sqrt(2))))
            //
            // When dist < 0 (inside), coverage is > 0.5 (approaches 1.0)
            // When dist > 0 (outside), coverage is < 0.5 (approaches 0.0)

            float t = dist * inv_sigma_sqrt2;

            // Fast erf approximation (Abramowitz and Stegun, maximum error ~1.5e-7)
            float abs_t = std::fabs(t);
            float erf_val;
            if (abs_t > 3.7f) {
                // For large values, erf approaches +/-1
                erf_val = (t > 0) ? 1.0f : -1.0f;
            } else {
                // Approximation: erf(x) = 1 - (a1*p + a2*p^2 + a3*p^3) * exp(-x^2)
                // where p = 1/(1 + 0.47047*|x|)
                float p = 1.0f / (1.0f + 0.3275911f * abs_t);
                float exp_val = std::exp(-abs_t * abs_t);
                erf_val = 1.0f - (0.254829592f * p
                                  - 0.284496736f * p * p
                                  + 1.421413741f * p * p * p
                                  - 1.453152027f * p * p * p * p
                                  + 1.061405429f * p * p * p * p * p) * exp_val;
                if (t < 0) erf_val = -erf_val;
            }

            float coverage = 0.5f * (1.0f - erf_val);

            if (coverage < 0.004f) continue; // Skip nearly transparent pixels

            uint8_t alpha = static_cast<uint8_t>(
                std::min(255.0f, static_cast<float>(color.a) * coverage));

            if (alpha == 0) continue;

            set_pixel(x, y, {color.r, color.g, color.b, alpha});
        }
    }
}

void SoftwareRenderer::draw_gradient_rect(const Rect& rect, float angle,
                                          const std::vector<std::pair<uint32_t, float>>& stops,
                                          float border_radius, bool repeating,
                                          float r_tl, float r_tr, float r_bl, float r_br) {
    if (stops.size() < 2) return;

    int x0 = std::max(0, static_cast<int>(std::floor(rect.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(rect.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(rect.x + rect.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(rect.y + rect.height)));

    // Convert angle to radians (CSS: 0=to top, 90=to right, 180=to bottom)
    float rad = (angle - 90.0f) * 3.14159265f / 180.0f;
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);

    // Center of the rect
    float cx = rect.x + rect.width / 2.0f;
    float cy = rect.y + rect.height / 2.0f;

    // The gradient line length is the projection of the rect diagonal
    float half_w = rect.width / 2.0f;
    float half_h = rect.height / 2.0f;
    float gradient_length = std::abs(half_w * cos_a) + std::abs(half_h * sin_a);
    if (gradient_length < 0.001f) gradient_length = 1.0f;

    // Resolve per-corner radii
    bool has_per = (r_tl > 0 || r_tr > 0 || r_bl > 0 || r_br > 0);
    float half_min = std::min(rect.width, rect.height) / 2.0f;
    float eff_tl, eff_tr, eff_bl, eff_br;
    if (has_per) {
        eff_tl = std::min(r_tl, half_min);
        eff_tr = std::min(r_tr, half_min);
        eff_bl = std::min(r_bl, half_min);
        eff_br = std::min(r_br, half_min);
    } else {
        float r = std::min(border_radius, half_min);
        eff_tl = eff_tr = eff_bl = eff_br = r;
    }
    bool has_radius = (eff_tl > 0 || eff_tr > 0 || eff_bl > 0 || eff_br > 0);

    // Corner centers
    float ccx_tl = rect.x + eff_tl,              ccy_tl = rect.y + eff_tl;
    float ccx_tr = rect.x + rect.width - eff_tr,  ccy_tr = rect.y + eff_tr;
    float ccx_bl = rect.x + eff_bl,              ccy_bl = rect.y + rect.height - eff_bl;
    float ccx_br = rect.x + rect.width - eff_br,  ccy_br = rect.y + rect.height - eff_br;

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;

            // Per-corner rounded clip check
            float aa_factor = 1.0f;
            if (has_radius) {
                float dx = 0, dy = 0, r = 0;
                bool in_corner_zone = false;
                if (px < ccx_tl && py < ccy_tl) {
                    dx = px - ccx_tl; dy = py - ccy_tl; r = eff_tl; in_corner_zone = true;
                } else if (px > ccx_tr && py < ccy_tr) {
                    dx = px - ccx_tr; dy = py - ccy_tr; r = eff_tr; in_corner_zone = true;
                } else if (px < ccx_bl && py > ccy_bl) {
                    dx = px - ccx_bl; dy = py - ccy_bl; r = eff_bl; in_corner_zone = true;
                } else if (px > ccx_br && py > ccy_br) {
                    dx = px - ccx_br; dy = py - ccy_br; r = eff_br; in_corner_zone = true;
                }
                if (in_corner_zone) {
                    if (r <= 0) continue;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 > r * r) continue;
                    float dist = std::sqrt(dist2);
                    if (dist > r - 1.0f) aa_factor = std::max(0.0f, r - dist);
                }
            }

            // Project pixel onto gradient line
            float rel_x = px - cx;
            float rel_y = py - cy;
            float proj = (rel_x * cos_a + rel_y * sin_a) / gradient_length;
            float t = (proj + 1.0f) / 2.0f; // normalize to [0, 1]
            if (repeating && stops.size() >= 2) {
                float first = stops.front().second;
                float last = stops.back().second;
                float range = last - first;
                if (range > 0.001f) {
                    t = first + std::fmod(t - first, range);
                    if (t < first) t += range;
                }
            } else {
                t = std::max(0.0f, std::min(1.0f, t));
            }

            // Interpolate between color stops
            // Find the two surrounding stops
            size_t idx = 0;
            for (size_t i = 1; i < stops.size(); i++) {
                if (t <= stops[i].second) { idx = i - 1; break; }
                if (i == stops.size() - 1) idx = i - 1;
            }
            float t0 = stops[idx].second;
            float t1 = stops[idx + 1].second;
            float local_t = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
            local_t = std::max(0.0f, std::min(1.0f, local_t));

            uint32_t c0 = stops[idx].first;
            uint32_t c1 = stops[idx + 1].first;
            auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
                return static_cast<uint8_t>(a + (b - a) * t);
            };
            Color col;
            col.a = lerp((c0 >> 24) & 0xFF, (c1 >> 24) & 0xFF, local_t);
            col.r = lerp((c0 >> 16) & 0xFF, (c1 >> 16) & 0xFF, local_t);
            col.g = lerp((c0 >> 8) & 0xFF, (c1 >> 8) & 0xFF, local_t);
            col.b = lerp(c0 & 0xFF, c1 & 0xFF, local_t);

            if (aa_factor < 1.0f) {
                col.a = static_cast<uint8_t>(col.a * aa_factor);
            }

            set_pixel(x, y, col);
        }
    }
}

void SoftwareRenderer::draw_radial_gradient_rect(const Rect& rect, int radial_shape,
                                                  const std::vector<std::pair<uint32_t, float>>& stops,
                                                  float border_radius, bool repeating,
                                                  float r_tl, float r_tr, float r_bl, float r_br) {
    if (stops.size() < 2) return;

    int x0 = std::max(0, static_cast<int>(std::floor(rect.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(rect.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(rect.x + rect.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(rect.y + rect.height)));

    // Center of the rect
    float cx = rect.x + rect.width / 2.0f;
    float cy = rect.y + rect.height / 2.0f;

    // For ellipse: half-widths for normalization
    float half_w = rect.width / 2.0f;
    float half_h = rect.height / 2.0f;

    // For circle: use the smaller half-dimension as the radius
    float radius = std::min(half_w, half_h);

    // Resolve per-corner radii
    bool has_per = (r_tl > 0 || r_tr > 0 || r_bl > 0 || r_br > 0);
    float half_min = std::min(rect.width, rect.height) / 2.0f;
    float eff_tl, eff_tr, eff_bl, eff_br;
    if (has_per) {
        eff_tl = std::min(r_tl, half_min);
        eff_tr = std::min(r_tr, half_min);
        eff_bl = std::min(r_bl, half_min);
        eff_br = std::min(r_br, half_min);
    } else {
        float r = std::min(border_radius, half_min);
        eff_tl = eff_tr = eff_bl = eff_br = r;
    }
    bool has_radius = (eff_tl > 0 || eff_tr > 0 || eff_bl > 0 || eff_br > 0);

    // Corner centers
    float ccx_tl = rect.x + eff_tl,              ccy_tl = rect.y + eff_tl;
    float ccx_tr = rect.x + rect.width - eff_tr,  ccy_tr = rect.y + eff_tr;
    float ccx_bl = rect.x + eff_bl,              ccy_bl = rect.y + rect.height - eff_bl;
    float ccx_br = rect.x + rect.width - eff_br,  ccy_br = rect.y + rect.height - eff_br;

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;

            // Per-corner rounded clip check
            float aa_factor = 1.0f;
            if (has_radius) {
                float dx = 0, dy = 0, r = 0;
                bool in_corner_zone = false;
                if (px < ccx_tl && py < ccy_tl) {
                    dx = px - ccx_tl; dy = py - ccy_tl; r = eff_tl; in_corner_zone = true;
                } else if (px > ccx_tr && py < ccy_tr) {
                    dx = px - ccx_tr; dy = py - ccy_tr; r = eff_tr; in_corner_zone = true;
                } else if (px < ccx_bl && py > ccy_bl) {
                    dx = px - ccx_bl; dy = py - ccy_bl; r = eff_bl; in_corner_zone = true;
                } else if (px > ccx_br && py > ccy_br) {
                    dx = px - ccx_br; dy = py - ccy_br; r = eff_br; in_corner_zone = true;
                }
                if (in_corner_zone) {
                    if (r <= 0) continue;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 > r * r) continue;
                    float dist = std::sqrt(dist2);
                    if (dist > r - 1.0f) aa_factor = std::max(0.0f, r - dist);
                }
            }

            // Calculate normalized distance from center
            float rel_x = px - cx;
            float rel_y = py - cy;
            float t;
            if (radial_shape == 1) {
                // Circle: uniform scaling based on min(half_w, half_h)
                float dist = std::sqrt(rel_x * rel_x + rel_y * rel_y);
                t = (radius > 0.001f) ? dist / radius : 0.0f;
            } else {
                // Ellipse: scale x and y independently
                float nx = (half_w > 0.001f) ? (rel_x / half_w) : 0.0f;
                float ny = (half_h > 0.001f) ? (rel_y / half_h) : 0.0f;
                t = std::sqrt(nx * nx + ny * ny);
            }
            if (repeating && stops.size() >= 2) {
                float first = stops.front().second;
                float last = stops.back().second;
                float range = last - first;
                if (range > 0.001f) {
                    t = first + std::fmod(t - first, range);
                    if (t < first) t += range;
                }
            } else {
                t = std::max(0.0f, std::min(1.0f, t));
            }

            // Interpolate between color stops
            size_t idx = 0;
            for (size_t i = 1; i < stops.size(); i++) {
                if (t <= stops[i].second) { idx = i - 1; break; }
                if (i == stops.size() - 1) idx = i - 1;
            }
            float t0 = stops[idx].second;
            float t1 = stops[idx + 1].second;
            float local_t = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
            local_t = std::max(0.0f, std::min(1.0f, local_t));

            uint32_t c0 = stops[idx].first;
            uint32_t c1 = stops[idx + 1].first;
            auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
                return static_cast<uint8_t>(a + (b - a) * t);
            };
            Color col;
            col.a = lerp((c0 >> 24) & 0xFF, (c1 >> 24) & 0xFF, local_t);
            col.r = lerp((c0 >> 16) & 0xFF, (c1 >> 16) & 0xFF, local_t);
            col.g = lerp((c0 >> 8) & 0xFF, (c1 >> 8) & 0xFF, local_t);
            col.b = lerp(c0 & 0xFF, c1 & 0xFF, local_t);

            if (aa_factor < 1.0f) {
                col.a = static_cast<uint8_t>(col.a * aa_factor);
            }

            set_pixel(x, y, col);
        }
    }
}

void SoftwareRenderer::draw_conic_gradient_rect(const Rect& rect, float from_angle,
                                                  const std::vector<std::pair<uint32_t, float>>& stops,
                                                  float border_radius, bool repeating,
                                                  float r_tl, float r_tr, float r_bl, float r_br) {
    if (stops.size() < 2) return;

    int x0 = std::max(0, static_cast<int>(std::floor(rect.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(rect.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(rect.x + rect.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(rect.y + rect.height)));

    float cx = rect.x + rect.width / 2.0f;
    float cy = rect.y + rect.height / 2.0f;

    // from_angle is in degrees, convert to radians
    float from_rad = from_angle * static_cast<float>(M_PI) / 180.0f;

    // Resolve per-corner radii
    bool has_per = (r_tl > 0 || r_tr > 0 || r_bl > 0 || r_br > 0);
    float half_min = std::min(rect.width, rect.height) / 2.0f;
    float eff_tl, eff_tr, eff_bl, eff_br;
    if (has_per) {
        eff_tl = std::min(r_tl, half_min);
        eff_tr = std::min(r_tr, half_min);
        eff_bl = std::min(r_bl, half_min);
        eff_br = std::min(r_br, half_min);
    } else {
        float r = std::min(border_radius, half_min);
        eff_tl = eff_tr = eff_bl = eff_br = r;
    }
    bool has_radius = (eff_tl > 0 || eff_tr > 0 || eff_bl > 0 || eff_br > 0);

    // Corner centers
    float ccx_tl = rect.x + eff_tl,              ccy_tl = rect.y + eff_tl;
    float ccx_tr = rect.x + rect.width - eff_tr,  ccy_tr = rect.y + eff_tr;
    float ccx_bl = rect.x + eff_bl,              ccy_bl = rect.y + rect.height - eff_bl;
    float ccx_br = rect.x + rect.width - eff_br,  ccy_br = rect.y + rect.height - eff_br;

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;

            // Per-corner rounded clip check
            float aa_factor = 1.0f;
            if (has_radius) {
                float dx = 0, dy = 0, r = 0;
                bool in_corner_zone = false;
                if (px < ccx_tl && py < ccy_tl) {
                    dx = px - ccx_tl; dy = py - ccy_tl; r = eff_tl; in_corner_zone = true;
                } else if (px > ccx_tr && py < ccy_tr) {
                    dx = px - ccx_tr; dy = py - ccy_tr; r = eff_tr; in_corner_zone = true;
                } else if (px < ccx_bl && py > ccy_bl) {
                    dx = px - ccx_bl; dy = py - ccy_bl; r = eff_bl; in_corner_zone = true;
                } else if (px > ccx_br && py > ccy_br) {
                    dx = px - ccx_br; dy = py - ccy_br; r = eff_br; in_corner_zone = true;
                }
                if (in_corner_zone) {
                    if (r <= 0) continue;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 > r * r) continue;
                    float dist = std::sqrt(dist2);
                    if (dist > r - 1.0f) aa_factor = std::max(0.0f, r - dist);
                }
            }

            // Calculate angle from center, normalized to [0, 1]
            float rel_x = px - cx;
            float rel_y = py - cy;
            float angle = std::atan2(rel_y, rel_x) - from_rad + static_cast<float>(M_PI) / 2.0f;
            // Normalize to [0, 2*PI]
            while (angle < 0) angle += 2.0f * static_cast<float>(M_PI);
            while (angle >= 2.0f * static_cast<float>(M_PI)) angle -= 2.0f * static_cast<float>(M_PI);
            float t = angle / (2.0f * static_cast<float>(M_PI));
            if (repeating && stops.size() >= 2) {
                float first = stops.front().second;
                float last = stops.back().second;
                float range = last - first;
                if (range > 0.001f) {
                    t = first + std::fmod(t - first, range);
                    if (t < first) t += range;
                }
            }

            // Interpolate between color stops
            size_t idx = 0;
            for (size_t i = 1; i < stops.size(); i++) {
                if (t <= stops[i].second) { idx = i - 1; break; }
                if (i == stops.size() - 1) idx = i - 1;
            }
            float t0 = stops[idx].second;
            float t1 = stops[idx + 1].second;
            float local_t = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
            local_t = std::max(0.0f, std::min(1.0f, local_t));

            uint32_t c0 = stops[idx].first;
            uint32_t c1 = stops[idx + 1].first;
            auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
                return static_cast<uint8_t>(a + (b - a) * t);
            };
            Color col;
            col.a = lerp((c0 >> 24) & 0xFF, (c1 >> 24) & 0xFF, local_t);
            col.r = lerp((c0 >> 16) & 0xFF, (c1 >> 16) & 0xFF, local_t);
            col.g = lerp((c0 >> 8) & 0xFF, (c1 >> 8) & 0xFF, local_t);
            col.b = lerp(c0 & 0xFF, c1 & 0xFF, local_t);

            if (aa_factor < 1.0f) {
                col.a = static_cast<uint8_t>(col.a * aa_factor);
            }

            set_pixel(x, y, col);
        }
    }
}

void SoftwareRenderer::draw_image(const Rect& dest, const ImageData& image, int image_rendering) {
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) return;

    int dx0 = std::max(0, static_cast<int>(std::floor(dest.x)));
    int dy0 = std::max(0, static_cast<int>(std::floor(dest.y)));
    int dx1 = std::min(width_, static_cast<int>(std::ceil(dest.x + dest.width)));
    int dy1 = std::min(height_, static_cast<int>(std::ceil(dest.y + dest.height)));

    float scale_x = static_cast<float>(image.width) / dest.width;
    float scale_y = static_cast<float>(image.height) / dest.height;

    // crisp-edges (3) or pixelated (4): use nearest-neighbor sampling
    bool nearest = (image_rendering == 3 || image_rendering == 4);

    for (int dy = dy0; dy < dy1; dy++) {
        float sy = (dy - dest.y) * scale_y;
        sy = std::clamp(sy, 0.0f, static_cast<float>(image.height - 1));

        for (int dx = dx0; dx < dx1; dx++) {
            float sx = (dx - dest.x) * scale_x;
            sx = std::clamp(sx, 0.0f, static_cast<float>(image.width - 1));

            if (nearest) {
                // Nearest-neighbor: snap to closest source pixel
                int px = std::clamp(static_cast<int>(sx), 0, image.width - 1);
                int py = std::clamp(static_cast<int>(sy), 0, image.height - 1);
                int idx = (py * image.width + px) * 4;
                Color pixel = {image.pixels[idx], image.pixels[idx+1],
                               image.pixels[idx+2], image.pixels[idx+3]};
                set_pixel(dx, dy, pixel);
            } else {
                // Bilinear interpolation for smooth scaling
                int x0 = static_cast<int>(sx);
                int y0 = static_cast<int>(sy);
                int x1 = std::min(x0 + 1, image.width - 1);
                int y1 = std::min(y0 + 1, image.height - 1);
                x0 = std::min(x0, image.width - 1);
                y0 = std::min(y0, image.height - 1);

                float fx = sx - static_cast<float>(x0);
                float fy = sy - static_cast<float>(y0);

                auto sample = [&](int spx, int spy) -> Color {
                    int idx = (spy * image.width + spx) * 4;
                    return {image.pixels[idx], image.pixels[idx+1],
                            image.pixels[idx+2], image.pixels[idx+3]};
                };

                Color c00 = sample(x0, y0);
                Color c10 = sample(x1, y0);
                Color c01 = sample(x0, y1);
                Color c11 = sample(x1, y1);

                auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
                    return static_cast<uint8_t>(a + (b - a) * t);
                };

                Color pixel = {
                    lerp(lerp(c00.r, c10.r, fx), lerp(c01.r, c11.r, fx), fy),
                    lerp(lerp(c00.g, c10.g, fx), lerp(c01.g, c11.g, fx), fy),
                    lerp(lerp(c00.b, c10.b, fx), lerp(c01.b, c11.b, fx), fy),
                    lerp(lerp(c00.a, c10.a, fx), lerp(c01.a, c11.a, fx), fy)
                };
                set_pixel(dx, dy, pixel);
            }
        }
    }
}

void SoftwareRenderer::draw_text_simple(const std::string& text, float x, float y,
                                         float font_size, const Color& color,
                                         const std::string& font_family,
                                         int font_weight, bool font_italic,
                                         float letter_spacing,
                                         const std::string& font_feature_settings,
                                         const std::string& font_variation_settings,
                                         int text_rendering, int font_kerning,
                                         int font_optical_sizing) {
    if (text.empty()) return;

    // When a non-identity transform is active, apply it to the text position
    // For translate-only transforms, we can offset the coords and use CoreText.
    // For rotate/scale, we fall back to bitmap rendering with transform.
    bool use_coretext = (text_renderer_ != nullptr);

    if (!current_transform_.is_identity()) {
        // Check if it's a simple translation
        bool is_translation = (current_transform_.a == 1 && current_transform_.b == 0 &&
                               current_transform_.c == 0 && current_transform_.d == 1);
        if (is_translation && use_coretext) {
            // Apply translation offset and use CoreText
            float tx = x + current_transform_.tx;
            float ty = y + current_transform_.ty;
            text_renderer_->render_text(text, tx, ty, font_size, color,
                                         pixels_.data(), width_, height_, font_family,
                                         font_weight, font_italic, letter_spacing,
                                         font_feature_settings, font_variation_settings,
                                         text_rendering, font_kerning, font_optical_sizing);
            return;
        }

        // For non-trivial transforms, use bitmap text with transform applied
        int char_width = static_cast<int>(font_size * 0.6f);
        int char_height = static_cast<int>(font_size);
        if (char_width < 1) char_width = 1;
        if (char_height < 1) char_height = 1;

        float advance = static_cast<float>(char_width) + letter_spacing;
        float cursor_x = x;
        for (char c : text) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                cursor_x += advance;
                continue;
            }
            // For each character, render a small rect in local coords
            // then transform each pixel to screen coords
            int margin = std::max(1, char_width / 8);
            for (int py = margin; py < char_height - margin; py++) {
                for (int px = margin; px < char_width - margin; px++) {
                    float lx = cursor_x + static_cast<float>(px);
                    float ly = y + static_cast<float>(py);
                    float sx, sy;
                    current_transform_.apply(lx, ly, sx, sy);
                    set_pixel(static_cast<int>(sx), static_cast<int>(sy), color);
                }
            }
            cursor_x += advance;
        }
        return;
    }

    // Use CoreText-based text renderer for proper glyph rendering
    if (use_coretext) {
        text_renderer_->render_text(text, x, y, font_size, color,
                                     pixels_.data(), width_, height_, font_family,
                                     font_weight, font_italic, letter_spacing,
                                     font_feature_settings, font_variation_settings,
                                     text_rendering, font_kerning, font_optical_sizing);
        return;
    }

    // Fallback: simple bitmap text (each character is a filled rectangle)
    int char_width = static_cast<int>(font_size * 0.6f);
    int char_height = static_cast<int>(font_size);

    if (char_width < 1) char_width = 1;
    if (char_height < 1) char_height = 1;

    float advance = static_cast<float>(char_width) + letter_spacing;
    float cursor_x = x;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            cursor_x += advance;
            continue;
        }
        draw_char_bitmap(c, static_cast<int>(cursor_x), static_cast<int>(y),
                         char_width, char_height, color);
        cursor_x += advance;
    }
}

void SoftwareRenderer::draw_char_bitmap(char /*c*/, int x, int y,
                                         int char_width, int char_height,
                                         const Color& color) {
    // Simple block rendering: draw a filled rectangle for each character
    // with a small margin so characters are distinguishable
    int margin = std::max(1, char_width / 8);
    int ix0 = std::max(0, x + margin);
    int iy0 = std::max(0, y + margin);
    int ix1 = std::min(width_, x + char_width - margin);
    int iy1 = std::min(height_, y + char_height - margin);

    for (int py = iy0; py < iy1; py++) {
        for (int px = ix0; px < ix1; px++) {
            set_pixel(px, py, color);
        }
    }
}

void SoftwareRenderer::apply_filter_to_region(const Rect& bounds, int filter_type, float value) {
    int x0 = std::max(0, static_cast<int>(bounds.x));
    int y0 = std::max(0, static_cast<int>(bounds.y));
    int x1 = std::min(width_, static_cast<int>(bounds.x + bounds.width));
    int y1 = std::min(height_, static_cast<int>(bounds.y + bounds.height));

    // Blur (type 9) — two-pass box blur, handled separately from per-pixel filters
    if (filter_type == 9) {
        int radius = static_cast<int>(value);
        if (radius <= 0) return;
        if (radius > 50) radius = 50; // clamp to reasonable max

        int rw = x1 - x0; // region width
        int rh = y1 - y0; // region height
        if (rw <= 0 || rh <= 0) return;

        // Temporary buffer for intermediate results (same size as region, RGBA)
        std::vector<uint8_t> temp(rw * rh * 4, 0);

        // --- Horizontal pass: read from pixels_, write to temp ---
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
                int count = 0;
                for (int kx = -radius; kx <= radius; kx++) {
                    int sx = px + kx;
                    // Clamp source x to region bounds
                    if (sx < x0) sx = x0;
                    if (sx >= x1) sx = x1 - 1;
                    int src_idx = (py * width_ + sx) * 4;
                    sum_r += pixels_[src_idx];
                    sum_g += pixels_[src_idx + 1];
                    sum_b += pixels_[src_idx + 2];
                    sum_a += pixels_[src_idx + 3];
                    count++;
                }
                int dst_idx = ((py - y0) * rw + (px - x0)) * 4;
                temp[dst_idx]     = static_cast<uint8_t>(sum_r / count);
                temp[dst_idx + 1] = static_cast<uint8_t>(sum_g / count);
                temp[dst_idx + 2] = static_cast<uint8_t>(sum_b / count);
                temp[dst_idx + 3] = static_cast<uint8_t>(sum_a / count);
            }
        }

        // --- Vertical pass: read from temp, write back to pixels_ ---
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
                int count = 0;
                for (int ky = -radius; ky <= radius; ky++) {
                    int sy = py + ky;
                    // Clamp source y to region bounds
                    if (sy < y0) sy = y0;
                    if (sy >= y1) sy = y1 - 1;
                    int src_idx = ((sy - y0) * rw + (px - x0)) * 4;
                    sum_r += temp[src_idx];
                    sum_g += temp[src_idx + 1];
                    sum_b += temp[src_idx + 2];
                    sum_a += temp[src_idx + 3];
                    count++;
                }
                int dst_idx = (py * width_ + px) * 4;
                pixels_[dst_idx]     = static_cast<uint8_t>(sum_r / count);
                pixels_[dst_idx + 1] = static_cast<uint8_t>(sum_g / count);
                pixels_[dst_idx + 2] = static_cast<uint8_t>(sum_b / count);
                pixels_[dst_idx + 3] = static_cast<uint8_t>(sum_a / count);
            }
        }

        return; // blur done
    }

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            int idx = (py * width_ + px) * 4;
            float r = pixels_[idx] / 255.0f;
            float g = pixels_[idx + 1] / 255.0f;
            float b = pixels_[idx + 2] / 255.0f;
            float a = pixels_[idx + 3] / 255.0f;

            switch (filter_type) {
                case 1: { // grayscale
                    float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    float amt = std::clamp(value, 0.0f, 1.0f);
                    r = r + (gray - r) * amt;
                    g = g + (gray - g) * amt;
                    b = b + (gray - b) * amt;
                    break;
                }
                case 2: { // sepia
                    float amt = std::clamp(value, 0.0f, 1.0f);
                    float sr = std::min(1.0f, 0.393f * r + 0.769f * g + 0.189f * b);
                    float sg = std::min(1.0f, 0.349f * r + 0.686f * g + 0.168f * b);
                    float sb = std::min(1.0f, 0.272f * r + 0.534f * g + 0.131f * b);
                    r = r + (sr - r) * amt;
                    g = g + (sg - g) * amt;
                    b = b + (sb - b) * amt;
                    break;
                }
                case 3: { // brightness
                    r *= value;
                    g *= value;
                    b *= value;
                    break;
                }
                case 4: { // contrast
                    r = (r - 0.5f) * value + 0.5f;
                    g = (g - 0.5f) * value + 0.5f;
                    b = (b - 0.5f) * value + 0.5f;
                    break;
                }
                case 5: { // invert
                    float amt = std::clamp(value, 0.0f, 1.0f);
                    r = r + ((1.0f - r) - r) * amt;
                    g = g + ((1.0f - g) - g) * amt;
                    b = b + ((1.0f - b) - b) * amt;
                    break;
                }
                case 6: { // saturate
                    float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    r = gray + (r - gray) * value;
                    g = gray + (g - gray) * value;
                    b = gray + (b - gray) * value;
                    break;
                }
                case 7: { // opacity
                    a *= std::clamp(value, 0.0f, 1.0f);
                    break;
                }
                case 8: { // hue-rotate (value in degrees)
                    float angle_rad = value * 3.14159265f / 180.0f;
                    float cos_a = std::cos(angle_rad);
                    float sin_a = std::sin(angle_rad);
                    // Hue rotation matrix (from W3C filter spec)
                    float nr = r * (0.213f + cos_a * 0.787f - sin_a * 0.213f)
                             + g * (0.715f - cos_a * 0.715f - sin_a * 0.715f)
                             + b * (0.072f - cos_a * 0.072f + sin_a * 0.928f);
                    float ng = r * (0.213f - cos_a * 0.213f + sin_a * 0.143f)
                             + g * (0.715f + cos_a * 0.285f + sin_a * 0.140f)
                             + b * (0.072f - cos_a * 0.072f - sin_a * 0.283f);
                    float nb = r * (0.213f - cos_a * 0.213f - sin_a * 0.787f)
                             + g * (0.715f - cos_a * 0.715f + sin_a * 0.715f)
                             + b * (0.072f + cos_a * 0.928f + sin_a * 0.072f);
                    r = nr; g = ng; b = nb;
                    break;
                }
                default:
                    break;
            }

            r = std::clamp(r, 0.0f, 1.0f);
            g = std::clamp(g, 0.0f, 1.0f);
            b = std::clamp(b, 0.0f, 1.0f);
            a = std::clamp(a, 0.0f, 1.0f);

            pixels_[idx]     = static_cast<uint8_t>(r * 255.0f);
            pixels_[idx + 1] = static_cast<uint8_t>(g * 255.0f);
            pixels_[idx + 2] = static_cast<uint8_t>(b * 255.0f);
            pixels_[idx + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
}

void SoftwareRenderer::apply_drop_shadow_to_region(const Rect& bounds, float blur_radius,
                                                     float offset_x, float offset_y,
                                                     uint32_t shadow_color) {
    int radius = static_cast<int>(blur_radius);
    if (radius < 0) radius = 0;
    if (radius > 50) radius = 50;

    // Expanded region to include shadow offset + blur
    int margin = radius + static_cast<int>(std::max(std::abs(offset_x), std::abs(offset_y))) + 2;
    int ex0 = std::max(0, static_cast<int>(std::floor(bounds.x)) - margin);
    int ey0 = std::max(0, static_cast<int>(std::floor(bounds.y)) - margin);
    int ex1 = std::min(width_, static_cast<int>(std::ceil(bounds.x + bounds.width)) + margin);
    int ey1 = std::min(height_, static_cast<int>(std::ceil(bounds.y + bounds.height)) + margin);
    int ew = ex1 - ex0;
    int eh = ey1 - ey0;
    if (ew <= 0 || eh <= 0) return;

    // Extract shadow color components
    uint8_t sc_a = static_cast<uint8_t>((shadow_color >> 24) & 0xFF);
    uint8_t sc_r = static_cast<uint8_t>((shadow_color >> 16) & 0xFF);
    uint8_t sc_g = static_cast<uint8_t>((shadow_color >> 8) & 0xFF);
    uint8_t sc_b = static_cast<uint8_t>(shadow_color & 0xFF);

    // Step 1: Create alpha silhouette of the element, offset by (offset_x, offset_y)
    std::vector<uint8_t> shadow_alpha(ew * eh, 0);
    int ox = static_cast<int>(offset_x);
    int oy = static_cast<int>(offset_y);

    int bx0 = std::max(0, static_cast<int>(std::floor(bounds.x)));
    int by0 = std::max(0, static_cast<int>(std::floor(bounds.y)));
    int bx1 = std::min(width_, static_cast<int>(std::ceil(bounds.x + bounds.width)));
    int by1 = std::min(height_, static_cast<int>(std::ceil(bounds.y + bounds.height)));

    for (int py = by0; py < by1; py++) {
        for (int px = bx0; px < bx1; px++) {
            int src_idx = (py * width_ + px) * 4;
            uint8_t alpha = pixels_[src_idx + 3];
            if (alpha == 0) continue;
            int sx = px + ox - ex0;
            int sy = py + oy - ey0;
            if (sx >= 0 && sx < ew && sy >= 0 && sy < eh) {
                shadow_alpha[sy * ew + sx] = alpha;
            }
        }
    }

    // Step 2: Blur the alpha channel if blur_radius > 0
    if (radius > 0) {
        std::vector<uint8_t> temp(ew * eh, 0);
        // Horizontal pass
        for (int py = 0; py < eh; py++) {
            for (int px = 0; px < ew; px++) {
                int sum = 0, count = 0;
                for (int kx = -radius; kx <= radius; kx++) {
                    int sx = std::clamp(px + kx, 0, ew - 1);
                    sum += shadow_alpha[py * ew + sx];
                    count++;
                }
                temp[py * ew + px] = static_cast<uint8_t>(sum / count);
            }
        }
        // Vertical pass
        for (int py = 0; py < eh; py++) {
            for (int px = 0; px < ew; px++) {
                int sum = 0, count = 0;
                for (int ky = -radius; ky <= radius; ky++) {
                    int sy = std::clamp(py + ky, 0, eh - 1);
                    sum += temp[sy * ew + px];
                    count++;
                }
                shadow_alpha[py * ew + px] = static_cast<uint8_t>(sum / count);
            }
        }
    }

    // Step 3: Composite shadow behind existing pixels (source-over with shadow under)
    for (int py = ey0; py < ey1; py++) {
        for (int px = ex0; px < ex1; px++) {
            int sa_idx = (py - ey0) * ew + (px - ex0);
            uint8_t sa = shadow_alpha[sa_idx];
            if (sa == 0) continue;

            // Shadow pixel: shadow_color with alpha modulated by silhouette
            float shadow_a = (sc_a / 255.0f) * (sa / 255.0f);
            if (shadow_a < 0.004f) continue;

            int idx = (py * width_ + px) * 4;
            float dst_r = pixels_[idx] / 255.0f;
            float dst_g = pixels_[idx + 1] / 255.0f;
            float dst_b = pixels_[idx + 2] / 255.0f;
            float dst_a = pixels_[idx + 3] / 255.0f;

            // Source-over compositing: shadow behind existing
            // result = existing_over_shadow
            float src_r = sc_r / 255.0f;
            float src_g = sc_g / 255.0f;
            float src_b = sc_b / 255.0f;
            float out_a = dst_a + shadow_a * (1.0f - dst_a);
            if (out_a > 0.0f) {
                float out_r = (dst_r * dst_a + src_r * shadow_a * (1.0f - dst_a)) / out_a;
                float out_g = (dst_g * dst_a + src_g * shadow_a * (1.0f - dst_a)) / out_a;
                float out_b = (dst_b * dst_a + src_b * shadow_a * (1.0f - dst_a)) / out_a;
                pixels_[idx]     = static_cast<uint8_t>(std::clamp(out_r, 0.0f, 1.0f) * 255.0f);
                pixels_[idx + 1] = static_cast<uint8_t>(std::clamp(out_g, 0.0f, 1.0f) * 255.0f);
                pixels_[idx + 2] = static_cast<uint8_t>(std::clamp(out_b, 0.0f, 1.0f) * 255.0f);
                pixels_[idx + 3] = static_cast<uint8_t>(std::clamp(out_a, 0.0f, 1.0f) * 255.0f);
            }
        }
    }
}

void SoftwareRenderer::apply_backdrop_filter_to_region(const Rect& bounds, int filter_type, float filter_value) {
    // CSS backdrop-filter: filters the existing backdrop pixels in the target region
    // BEFORE the element itself is painted on top. The display list emits this command
    // prior to the element's own FillRect/DrawText commands, so we:
    //   1. Save a snapshot of the current backdrop pixels in the region
    //   2. Apply the filter (blur, grayscale, etc.) to those pixels in-place
    //   3. The element then paints on top in subsequent commands
    //
    // Step 1: Save backdrop snapshot (for potential future compositing needs)
    int x0 = std::max(0, static_cast<int>(bounds.x));
    int y0 = std::max(0, static_cast<int>(bounds.y));
    int x1 = std::min(width_, static_cast<int>(bounds.x + bounds.width));
    int y1 = std::min(height_, static_cast<int>(bounds.y + bounds.height));
    int rw = x1 - x0;
    int rh = y1 - y0;
    if (rw <= 0 || rh <= 0) return;

    // Save backdrop pixels before filtering (snapshot for compositing)
    std::vector<uint8_t> backdrop_snapshot(rw * rh * 4);
    for (int py = y0; py < y1; py++) {
        int src_offset = (py * width_ + x0) * 4;
        int dst_offset = ((py - y0) * rw) * 4;
        std::copy(pixels_.begin() + src_offset,
                  pixels_.begin() + src_offset + rw * 4,
                  backdrop_snapshot.begin() + dst_offset);
    }

    // Step 2: Apply the filter to the backdrop region in-place
    apply_filter_to_region(bounds, filter_type, filter_value);

    // Step 3: The element paints on top in subsequent display list commands
    // (no action needed here — the filtered backdrop is now in the pixel buffer)
}

void SoftwareRenderer::draw_filled_ellipse(float cx, float cy, float rx, float ry,
                                            const Color& color) {
    if (rx <= 0 || ry <= 0) return;

    int x0 = std::max(0, static_cast<int>(std::floor(cx - rx)));
    int y0 = std::max(0, static_cast<int>(std::floor(cy - ry)));
    int x1 = std::min(width_ - 1, static_cast<int>(std::ceil(cx + rx)));
    int y1 = std::min(height_ - 1, static_cast<int>(std::ceil(cy + ry)));

    float inv_rx2 = 1.0f / (rx * rx);
    float inv_ry2 = 1.0f / (ry * ry);

    for (int py = y0; py <= y1; py++) {
        float fy = static_cast<float>(py) + 0.5f - cy;
        for (int px = x0; px <= x1; px++) {
            float fx = static_cast<float>(px) + 0.5f - cx;
            float d = fx * fx * inv_rx2 + fy * fy * inv_ry2;
            if (d <= 1.0f) {
                // Anti-aliasing at the edge
                float alpha = 1.0f;
                if (d > 0.9f) {
                    alpha = (1.0f - d) * 10.0f; // smooth edge
                    alpha = std::max(0.0f, std::min(1.0f, alpha));
                }
                Color c = color;
                c.a = static_cast<uint8_t>(c.a * alpha);
                set_pixel(px, py, c);
            }
        }
    }
}

void SoftwareRenderer::draw_stroked_ellipse(float cx, float cy, float rx, float ry,
                                              const Color& color, float stroke_width) {
    if (rx <= 0 || ry <= 0) return;

    float half_sw = stroke_width / 2.0f;
    float outer_rx = rx + half_sw;
    float outer_ry = ry + half_sw;
    float inner_rx = std::max(0.0f, rx - half_sw);
    float inner_ry = std::max(0.0f, ry - half_sw);

    int x0 = std::max(0, static_cast<int>(std::floor(cx - outer_rx)));
    int y0 = std::max(0, static_cast<int>(std::floor(cy - outer_ry)));
    int x1 = std::min(width_ - 1, static_cast<int>(std::ceil(cx + outer_rx)));
    int y1 = std::min(height_ - 1, static_cast<int>(std::ceil(cy + outer_ry)));

    float inv_orx2 = 1.0f / (outer_rx * outer_rx);
    float inv_ory2 = 1.0f / (outer_ry * outer_ry);
    float inv_irx2 = (inner_rx > 0) ? 1.0f / (inner_rx * inner_rx) : 0;
    float inv_iry2 = (inner_ry > 0) ? 1.0f / (inner_ry * inner_ry) : 0;

    for (int py = y0; py <= y1; py++) {
        float fy = static_cast<float>(py) + 0.5f - cy;
        for (int px = x0; px <= x1; px++) {
            float fx = static_cast<float>(px) + 0.5f - cx;
            float d_outer = fx * fx * inv_orx2 + fy * fy * inv_ory2;
            if (d_outer > 1.0f) continue;
            float d_inner = (inner_rx > 0 && inner_ry > 0)
                ? fx * fx * inv_irx2 + fy * fy * inv_iry2
                : 0.0f;
            if (d_inner < 1.0f && inner_rx > 0 && inner_ry > 0) continue;
            set_pixel(px, py, color);
        }
    }
}

void SoftwareRenderer::draw_line_segment(float x1, float y1, float x2, float y2,
                                          const Color& color, float stroke_width) {
    // DDA line drawing with thickness
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return;

    float half_sw = stroke_width / 2.0f;

    // Perpendicular direction for thickness
    float nx = -dy / length;
    float ny = dx / length;

    // Compute bounding box of the thick line
    float min_x = std::min({x1 + nx * half_sw, x1 - nx * half_sw,
                             x2 + nx * half_sw, x2 - nx * half_sw});
    float max_x = std::max({x1 + nx * half_sw, x1 - nx * half_sw,
                             x2 + nx * half_sw, x2 - nx * half_sw});
    float min_y = std::min({y1 + ny * half_sw, y1 - ny * half_sw,
                             y2 + ny * half_sw, y2 - ny * half_sw});
    float max_y = std::max({y1 + ny * half_sw, y1 - ny * half_sw,
                             y2 + ny * half_sw, y2 - ny * half_sw});

    int px0 = std::max(0, static_cast<int>(std::floor(min_x)));
    int py0 = std::max(0, static_cast<int>(std::floor(min_y)));
    int px1 = std::min(width_ - 1, static_cast<int>(std::ceil(max_x)));
    int py1 = std::min(height_ - 1, static_cast<int>(std::ceil(max_y)));

    for (int py = py0; py <= py1; py++) {
        float fy = static_cast<float>(py) + 0.5f;
        for (int px = px0; px <= px1; px++) {
            float fx = static_cast<float>(px) + 0.5f;
            // Distance from point to line segment
            float t = ((fx - x1) * dx + (fy - y1) * dy) / (length * length);
            t = std::max(0.0f, std::min(1.0f, t));
            float closest_x = x1 + t * dx;
            float closest_y = y1 + t * dy;
            float dist = std::sqrt((fx - closest_x) * (fx - closest_x) +
                                    (fy - closest_y) * (fy - closest_y));
            if (dist <= half_sw) {
                set_pixel(px, py, color);
            }
        }
    }
}

void SoftwareRenderer::apply_clip_path_mask(const Rect& bounds, int clip_type,
                                             const std::vector<float>& values) {
    int x0 = std::max(0, static_cast<int>(std::floor(bounds.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(bounds.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(bounds.x + bounds.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(bounds.y + bounds.height)));

    // Center of the element's bounding box (used as default center for circle/ellipse)
    float cx = bounds.x + bounds.width / 2.0f;
    float cy = bounds.y + bounds.height / 2.0f;

    for (int py = y0; py < y1; py++) {
        float fy = static_cast<float>(py) + 0.5f;
        for (int px = x0; px < x1; px++) {
            float fx = static_cast<float>(px) + 0.5f;
            bool inside = false;

            switch (clip_type) {
                case 1: { // circle(r at cx cy)
                    // values: {radius} or {radius, at_x%, at_y%}
                    float r_pct = (values.size() >= 1) ? values[0] : 50.0f;
                    // Radius: percentage of half the smaller dimension
                    float r = r_pct / 100.0f * std::min(bounds.width, bounds.height) / 2.0f;
                    // Center: percentage position or default center
                    float ccx = cx, ccy = cy;
                    if (values.size() >= 3) {
                        ccx = bounds.x + values[1] / 100.0f * bounds.width;
                        ccy = bounds.y + values[2] / 100.0f * bounds.height;
                    }
                    float dx = fx - ccx;
                    float dy = fy - ccy;
                    inside = (dx * dx + dy * dy) <= (r * r);
                    break;
                }
                case 2: { // ellipse(rx ry at cx cy)
                    // values: {rx%, ry%} or {rx%, ry%, at_x%, at_y%}
                    float rx_pct = (values.size() >= 1) ? values[0] : 50.0f;
                    float ry_pct = (values.size() >= 2) ? values[1] : 50.0f;
                    float rx = rx_pct / 100.0f * bounds.width / 2.0f;
                    float ry = ry_pct / 100.0f * bounds.height / 2.0f;
                    float ecx = cx, ecy = cy;
                    if (values.size() >= 4) {
                        ecx = bounds.x + values[2] / 100.0f * bounds.width;
                        ecy = bounds.y + values[3] / 100.0f * bounds.height;
                    }
                    if (rx > 0 && ry > 0) {
                        float dx = fx - ecx;
                        float dy = fy - ecy;
                        inside = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0f;
                    }
                    break;
                }
                case 3: { // inset(top right bottom left)
                    // values: {top, right, bottom, left}
                    float top = (values.size() >= 1) ? values[0] : 0;
                    float right = (values.size() >= 2) ? values[1] : 0;
                    float bottom = (values.size() >= 3) ? values[2] : 0;
                    float left = (values.size() >= 4) ? values[3] : 0;
                    float inset_x0 = bounds.x + left;
                    float inset_y0 = bounds.y + top;
                    float inset_x1 = bounds.x + bounds.width - right;
                    float inset_y1 = bounds.y + bounds.height - bottom;
                    inside = (fx >= inset_x0 && fx <= inset_x1 &&
                              fy >= inset_y0 && fy <= inset_y1);
                    break;
                }
                case 4: { // polygon(x1 y1 x2 y2 ...)
                    // Point-in-polygon using ray casting algorithm
                    // values are pairs: negative = percentage, positive = pixel offset from bounds origin
                    size_t n = values.size() / 2;
                    if (n < 3) break;
                    int crossings = 0;
                    for (size_t i = 0; i < n; i++) {
                        size_t j = (i + 1) % n;
                        float xv_i = values[i * 2], yv_i = values[i * 2 + 1];
                        float xv_j = values[j * 2], yv_j = values[j * 2 + 1];
                        // Negative = percentage, resolve against bounds
                        float xi = (xv_i < 0) ? bounds.x + (-xv_i / 100.0f) * bounds.width : xv_i + bounds.x;
                        float yi = (yv_i < 0) ? bounds.y + (-yv_i / 100.0f) * bounds.height : yv_i + bounds.y;
                        float xj = (xv_j < 0) ? bounds.x + (-xv_j / 100.0f) * bounds.width : xv_j + bounds.x;
                        float yj = (yv_j < 0) ? bounds.y + (-yv_j / 100.0f) * bounds.height : yv_j + bounds.y;
                        if ((yi <= fy && yj > fy) || (yj <= fy && yi > fy)) {
                            float t = (fy - yi) / (yj - yi);
                            float intersect_x = xi + t * (xj - xi);
                            if (fx < intersect_x) {
                                crossings++;
                            }
                        }
                    }
                    inside = (crossings % 2) == 1;
                    break;
                }
                default:
                    inside = true; // Unknown type: don't clip
                    break;
            }

            if (!inside) {
                // Set pixel to transparent (mask it out)
                int idx = (py * width_ + px) * 4;
                pixels_[idx] = 0;
                pixels_[idx + 1] = 0;
                pixels_[idx + 2] = 0;
                pixels_[idx + 3] = 0;
            }
        }
    }
}

void SoftwareRenderer::apply_blend_mode_to_region(const Rect& bounds, int blend_mode) {
    if (backdrop_stack_.empty()) return;

    auto snap = std::move(backdrop_stack_.top());
    backdrop_stack_.pop();

    int x0 = std::max(0, static_cast<int>(std::floor(bounds.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(bounds.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(bounds.x + bounds.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(bounds.y + bounds.height)));

    // Backdrop region bounds (from the snapshot)
    int bx0 = std::max(0, static_cast<int>(std::floor(snap.bounds.x)));
    int by0 = std::max(0, static_cast<int>(std::floor(snap.bounds.y)));
    int bx1 = std::min(width_, static_cast<int>(std::ceil(snap.bounds.x + snap.bounds.width)));
    int bw = bx1 - bx0;

    if (bw <= 0 || snap.pixels.empty()) return;

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            int cur_idx = (py * width_ + px) * 4;

            // Get the backdrop (saved) pixel
            int snap_local_x = px - bx0;
            int snap_local_y = py - by0;
            if (snap_local_x < 0 || snap_local_y < 0 || snap_local_x >= bw)
                continue;
            int snap_idx = (snap_local_y * bw + snap_local_x) * 4;
            if (snap_idx < 0 || static_cast<size_t>(snap_idx + 3) >= snap.pixels.size())
                continue;

            // dst = backdrop (what was there before the element)
            uint8_t dst_r = snap.pixels[snap_idx];
            uint8_t dst_g = snap.pixels[snap_idx + 1];
            uint8_t dst_b = snap.pixels[snap_idx + 2];
            uint8_t dst_a = snap.pixels[snap_idx + 3];

            // src = current (element painted on top)
            uint8_t src_r = pixels_[cur_idx];
            uint8_t src_g = pixels_[cur_idx + 1];
            uint8_t src_b = pixels_[cur_idx + 2];
            uint8_t src_a = pixels_[cur_idx + 3];

            // If the element didn't paint here (alpha=0 on the cleared canvas),
            // restore the backdrop pixel and skip blending
            if (src_a == 0) {
                pixels_[cur_idx]     = dst_r;
                pixels_[cur_idx + 1] = dst_g;
                pixels_[cur_idx + 2] = dst_b;
                pixels_[cur_idx + 3] = dst_a;
                continue;
            }

            uint8_t blend_r = src_r, blend_g = src_g, blend_b = src_b;

            // Apply blend formula per channel (R, G, B)
            auto blend_channel = [&](uint8_t s, uint8_t d) -> uint8_t {
                switch (blend_mode) {
                    case 1: // multiply
                        return static_cast<uint8_t>((static_cast<int>(s) * d) / 255);
                    case 2: // screen
                        return static_cast<uint8_t>(s + d - (static_cast<int>(s) * d) / 255);
                    case 3: // overlay
                        return d < 128
                            ? static_cast<uint8_t>((2 * static_cast<int>(s) * d) / 255)
                            : static_cast<uint8_t>(255 - (2 * static_cast<int>(255 - s) * (255 - d)) / 255);
                    case 4: // darken
                        return std::min(s, d);
                    case 5: // lighten
                        return std::max(s, d);
                    case 6: // color-dodge
                        if (d == 0) return 0;
                        if (s >= 255) return 255;
                        return static_cast<uint8_t>(std::min(255, static_cast<int>(d) * 255 / (255 - s)));
                    case 7: // color-burn
                        if (d == 255) return 255;
                        if (s == 0) return 0;
                        return static_cast<uint8_t>(std::max(0, 255 - static_cast<int>(255 - d) * 255 / s));
                    case 8: // hard-light (overlay with s/d swapped)
                        return s < 128
                            ? static_cast<uint8_t>((2 * static_cast<int>(s) * d) / 255)
                            : static_cast<uint8_t>(255 - (2 * static_cast<int>(255 - s) * (255 - d)) / 255);
                    case 9: { // soft-light (W3C formula)
                        float sf = s / 255.0f;
                        float df = d / 255.0f;
                        float r;
                        if (sf <= 0.5f) {
                            r = df - (1.0f - 2.0f * sf) * df * (1.0f - df);
                        } else {
                            float g = (df <= 0.25f)
                                ? ((16.0f * df - 12.0f) * df + 4.0f) * df
                                : std::sqrt(df);
                            r = df + (2.0f * sf - 1.0f) * (g - df);
                        }
                        return static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                    }
                    case 10: // difference
                        return static_cast<uint8_t>(std::abs(static_cast<int>(s) - static_cast<int>(d)));
                    case 11: // exclusion
                        return static_cast<uint8_t>(s + d - (2 * static_cast<int>(s) * d) / 255);
                    default: // normal (0) or unknown
                        return s;
                }
            };

            blend_r = blend_channel(src_r, dst_r);
            blend_g = blend_channel(src_g, dst_g);
            blend_b = blend_channel(src_b, dst_b);

            // Composite the blended color back onto the backdrop using the source alpha
            // result = blended * src_a + backdrop * (1 - src_a)
            if (src_a == 255) {
                pixels_[cur_idx]     = blend_r;
                pixels_[cur_idx + 1] = blend_g;
                pixels_[cur_idx + 2] = blend_b;
                // Keep the alpha as the composited alpha
                pixels_[cur_idx + 3] = static_cast<uint8_t>(
                    std::min(255u, static_cast<unsigned>(src_a) + dst_a));
            } else if (src_a > 0) {
                uint16_t sa = src_a;
                uint16_t inv_a = 255 - sa;
                pixels_[cur_idx]     = static_cast<uint8_t>((blend_r * sa + dst_r * inv_a) / 255);
                pixels_[cur_idx + 1] = static_cast<uint8_t>((blend_g * sa + dst_g * inv_a) / 255);
                pixels_[cur_idx + 2] = static_cast<uint8_t>((blend_b * sa + dst_b * inv_a) / 255);
                pixels_[cur_idx + 3] = static_cast<uint8_t>(
                    std::min(255u, static_cast<unsigned>(sa) + dst_a));
            } else {
                // src_a == 0: restore backdrop
                pixels_[cur_idx]     = dst_r;
                pixels_[cur_idx + 1] = dst_g;
                pixels_[cur_idx + 2] = dst_b;
                pixels_[cur_idx + 3] = dst_a;
            }
        }
    }
}

void SoftwareRenderer::apply_mask_gradient_to_region(
    const Rect& bounds, float angle,
    const std::vector<std::pair<uint32_t, float>>& stops) {
    if (stops.size() < 2) return;

    int x0 = std::max(0, static_cast<int>(std::floor(bounds.x)));
    int y0 = std::max(0, static_cast<int>(std::floor(bounds.y)));
    int x1 = std::min(width_, static_cast<int>(std::ceil(bounds.x + bounds.width)));
    int y1 = std::min(height_, static_cast<int>(std::ceil(bounds.y + bounds.height)));

    // Gradient direction vector from angle (CSS: 0=to top, 90=to right, 180=to bottom)
    float rad = (angle - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    float dx = std::cos(rad);
    float dy = std::sin(rad);

    // Project corners to find gradient line extent
    float cx = bounds.x + bounds.width / 2.0f;
    float cy = bounds.y + bounds.height / 2.0f;

    float half_w = bounds.width / 2.0f;
    float half_h = bounds.height / 2.0f;
    float extent = std::abs(dx) * half_w + std::abs(dy) * half_h;
    if (extent < 0.001f) return;

    for (int y = y0; y < y1; y++) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x < x1; x++) {
            float px = static_cast<float>(x) + 0.5f;

            // Project point onto gradient line
            float rel_x = px - cx;
            float rel_y = py - cy;
            float proj = (rel_x * dx + rel_y * dy) / extent;
            float t = (proj + 1.0f) / 2.0f; // map [-1, 1] to [0, 1]
            t = std::max(0.0f, std::min(1.0f, t));

            // Interpolate alpha from gradient stops
            // For mask-image, we use the alpha channel of the gradient color as the mask
            uint8_t mask_alpha = 0;
            if (t <= stops.front().second) {
                mask_alpha = static_cast<uint8_t>((stops.front().first >> 24) & 0xFF);
            } else if (t >= stops.back().second) {
                mask_alpha = static_cast<uint8_t>((stops.back().first >> 24) & 0xFF);
            } else {
                for (size_t i = 0; i + 1 < stops.size(); i++) {
                    if (t >= stops[i].second && t <= stops[i + 1].second) {
                        float range = stops[i + 1].second - stops[i].second;
                        float local_t = (range > 0.001f) ? (t - stops[i].second) / range : 0.0f;
                        uint8_t a0 = static_cast<uint8_t>((stops[i].first >> 24) & 0xFF);
                        uint8_t a1 = static_cast<uint8_t>((stops[i + 1].first >> 24) & 0xFF);
                        mask_alpha = static_cast<uint8_t>(a0 + local_t * (a1 - a0));
                        break;
                    }
                }
            }

            // Apply mask: multiply pixel alpha by mask_alpha
            int idx = (y * width_ + x) * 4;
            if (idx + 3 < static_cast<int>(pixels_.size())) {
                uint8_t cur_a = pixels_[idx + 3];
                pixels_[idx + 3] = static_cast<uint8_t>((cur_a * mask_alpha) / 255);
                // Pre-multiply RGB by mask factor too
                if (mask_alpha < 255) {
                    pixels_[idx]     = static_cast<uint8_t>((pixels_[idx] * mask_alpha) / 255);
                    pixels_[idx + 1] = static_cast<uint8_t>((pixels_[idx + 1] * mask_alpha) / 255);
                    pixels_[idx + 2] = static_cast<uint8_t>((pixels_[idx + 2] * mask_alpha) / 255);
                }
            }
        }
    }
}

} // namespace clever::paint
