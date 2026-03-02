#include <clever/paint/painter.h>
#include <clever/layout/box.h>
#include <clever/css/style/style_resolver.h>
#include <clever/paint/text_renderer.h>
#include <clever/paint/image_fetch.h>
#include <nanosvg.h>
#include <nanosvgrast.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <sstream>

namespace clever::paint {

struct DecodedImage {
    std::shared_ptr<std::vector<uint8_t>> pixels;
    int width = 0;
    int height = 0;
};

static inline uint8_t clamp_color_channel(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

static inline uint32_t lighten_color(uint32_t argb, int amount) {
    Color base = Color::from_argb(argb);
    return (static_cast<uint32_t>(base.a) << 24) |
           (static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.r) + amount)) << 16) |
           (static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.g) + amount)) << 8) |
           static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.b) + amount));
}

static inline uint32_t darken_color(uint32_t argb, int amount) {
    Color base = Color::from_argb(argb);
    return (static_cast<uint32_t>(base.a) << 24) |
           (static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.r) - amount)) << 16) |
           (static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.g) - amount)) << 8) |
           static_cast<uint32_t>(clamp_color_channel(static_cast<int>(base.b) - amount));
}

static inline bool is_dark_color(uint32_t argb) {
    Color base = Color::from_argb(argb);
    int luma = (299 * base.r + 587 * base.g + 114 * base.b) / 1000;
    return luma < 128;
}

// Helper: rasterize SVG string to bitmap using nanosvg
static DecodedImage decode_svg_image(const std::string& svg_data, float target_width = 0) {
    DecodedImage result;
    std::string svg_copy = svg_data;
    NSVGimage* svg = nsvgParse(svg_copy.data(), "px", 96.0f);
    if (!svg) return result;

    if (svg->width <= 0 || svg->height <= 0) {
        nsvgDelete(svg);
        return result;
    }

    float scale = 1.0f;
    if (target_width > 0 && svg->width > 0) {
        scale = target_width / svg->width;
    }

    int w = static_cast<int>(svg->width * scale);
    int h = static_cast<int>(svg->height * scale);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        if (w > 4096 || h > 4096) {
            float max_dim = std::max(svg->width, svg->height);
            scale = 4096.0f / max_dim;
            w = static_cast<int>(svg->width * scale);
            h = static_cast<int>(svg->height * scale);
        }
        if (w <= 0 || h <= 0) {
            nsvgDelete(svg);
            return result;
        }
    }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) {
        nsvgDelete(svg);
        return result;
    }

    auto pixels = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(w) * h * 4, 0);
    nsvgRasterize(rast, svg, 0, 0, scale, pixels->data(), w, h, w * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);

    result.width = w;
    result.height = h;
    result.pixels = pixels;
    return result;
}

DisplayList Painter::paint(const clever::layout::LayoutNode& root, float viewport_height,
                           float viewport_width, float viewport_scroll_y, float viewport_scroll_x) {
    viewport_height_ = viewport_height;
    viewport_width_ = viewport_width;
    viewport_scroll_y_ = viewport_scroll_y;
    viewport_scroll_x_ = viewport_scroll_x;
    DisplayList list;
    paint_node(root, list, 0.0f, 0.0f);
    return list;
}

void Painter::paint_node(const clever::layout::LayoutNode& node, DisplayList& list,
                          float offset_x, float offset_y) {
    // Guard against extremely deep paint trees causing stack overflow.
    // paint_node is a large function (~5000 lines); each frame is several KB.
    static constexpr int kMaxPaintDepth = 256;
    static thread_local int paint_depth = 0;
    if (paint_depth >= kMaxPaintDepth) return;
    struct PaintDepthGuard { PaintDepthGuard() { ++paint_depth; } ~PaintDepthGuard() { --paint_depth; } } pdg;

    // Skip display:none nodes entirely (no space, no painting)
    if (node.display == clever::layout::DisplayType::None) return;

    // visibility:collapse fully collapses table rows/columns
    if (node.visibility_collapse &&
        (node.tag_name == "tr" || node.tag_name == "TR" ||
         node.tag_name == "col" || node.tag_name == "COL" ||
         node.tag_name == "colgroup" || node.tag_name == "COLGROUP")) {
        return;
    }

    // content-visibility: auto — skip painting if entirely off-screen
    if (node.content_visibility == 2) {
        float abs_y = offset_y + node.geometry.y;
        float bottom = abs_y + node.geometry.border_box_height();
        // Skip if element is entirely below the rendered viewport
        if (abs_y > viewport_height_ && viewport_height_ > 0) return;
        // Skip if element is entirely above the viewport (negative y with no overlap)
        if (bottom < 0) return;
    }

    // SVG <defs> — don't paint definition elements (they're referenced by <use>)
    if (node.is_svg_defs) return;

    // CSS empty-cells: hide — skip painting empty table cells (no background/borders)
    // Table cells have tag_name "td" or "th" (display is Block in our engine)
    if (node.empty_cells == 1 && (node.tag_name == "td" || node.tag_name == "th" ||
                                   node.tag_name == "TD" || node.tag_name == "TH")) {
        // Check if cell is truly empty (no visible text children)
        bool cell_empty = true;
        for (const auto& child : node.children) {
            if (child->is_text && !child->text_content.empty()) {
                // Check if text is non-whitespace
                bool has_content = false;
                for (char c : child->text_content) {
                    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                        has_content = true;
                        break;
                    }
                }
                if (has_content) { cell_empty = false; break; }
            } else if (!child->is_text && child->display != clever::layout::DisplayType::None) {
                cell_empty = false;
                break;
            }
        }
        if (cell_empty) return;
    }

    // Compute absolute position for this node
    const auto& geom = node.geometry;
    float abs_x = offset_x + geom.x;
    float abs_y = offset_y + geom.y;

    // backface-visibility: hidden — skip painting if element is rotated past 90 degrees
    // (i.e. its backface is showing). Check both transform list and individual CSS rotate.
    if (node.backface_visibility == 1) {
        float total_rotation = 0;
        for (const auto& t : node.transforms) {
            if (t.type == clever::css::TransformType::Rotate) {
                total_rotation += t.angle;
            }
        }
        if (!node.css_rotate.empty() && node.css_rotate != "none") {
            float angle = 0;
            std::string val = node.css_rotate;
            if (val.size() > 3 && val.substr(val.size()-3) == "deg") {
                try { angle = std::stof(val.substr(0, val.size()-3)); } catch (...) {}
            } else if (val.size() > 4 && val.substr(val.size()-4) == "turn") {
                try { angle = std::stof(val.substr(0, val.size()-4)) * 360.0f; } catch (...) {}
            } else if (val.size() > 3 && val.substr(val.size()-3) == "rad") {
                try { angle = std::stof(val.substr(0, val.size()-3)) * 180.0f / 3.14159265f; } catch (...) {}
            } else {
                try { angle = std::stof(val); } catch (...) {}
            }
            total_rotation += angle;
        }
        // Normalize to 0-360 range
        float normalized = std::fmod(std::fabs(total_rotation), 360.0f);
        if (normalized > 90.0f && normalized < 270.0f) {
            return; // Backface is showing — skip painting
        }
    }

    // Apply CSS offset-path translation (before transforms)
    bool has_offset = (node.offset_path != "none" && !node.offset_path.empty() &&
                       node.offset_distance != 0);

    // Apply CSS transforms if present
    bool has_transforms = !node.transforms.empty();
    // Also check for individual translate/rotate/scale properties
    bool has_css_translate = (!node.css_translate.empty() && node.css_translate != "none");
    bool has_css_rotate = (!node.css_rotate.empty() && node.css_rotate != "none");
    bool has_css_scale = (!node.css_scale.empty() && node.css_scale != "none");
    bool has_individual_transforms = has_css_translate || has_css_rotate || has_css_scale;
    float active_perspective = (node.parent ? node.parent->perspective : 0.0f);
    float perspective_z_offset = 0.0f;

    int transform_count = 0;
    // Resolve CSS transform-origin against the element's border-box.
    // The Length value may be a percentage (resolved against element's own size)
    // or an absolute length (px, em, etc.) as offset from element's top-left.
    float border_box_w = geom.border_box_width();
    float border_box_h = geom.border_box_height();
    float origin_x, origin_y;
    {
        using Unit = clever::css::Length::Unit;
        const auto& lx = node.transform_origin_x_len;
        const auto& ly = node.transform_origin_y_len;
        if (lx.unit == Unit::Percent) {
            origin_x = abs_x + border_box_w * (lx.value / 100.0f);
        } else {
            // Absolute length: offset from the element's top-left corner
            origin_x = abs_x + lx.to_px(border_box_w);
        }
        if (ly.unit == Unit::Percent) {
            origin_y = abs_y + border_box_h * (ly.value / 100.0f);
        } else {
            origin_y = abs_y + ly.to_px(border_box_h);
        }
    }

    // CSS offset-path: translate element along a path
    if (has_offset) {
        // Parse offset-path: path("...") — extract the SVG path data
        // Simple implementation: support "path(\"M x,y L x2,y2\")" for line segments
        // and "circle(Rpx)" for circular paths
        std::string path_str = node.offset_path;
        float dist = node.offset_distance; // 0-100% mapped to 0.0-1.0 or px value
        float tx = 0, ty = 0;

        // Check for circle(Rpx) or circle(R%)
        auto circle_pos = path_str.find("circle(");
        if (circle_pos != std::string::npos) {
            auto end = path_str.find(')', circle_pos);
            if (end != std::string::npos) {
                std::string r_str = path_str.substr(circle_pos + 7, end - circle_pos - 7);
                float radius = 0;
                try { radius = std::stof(r_str); } catch (...) {}
                // offset-distance as angle around circle (percentage of circumference)
                float angle = dist / 100.0f * 2.0f * 3.14159265f;
                tx = radius * std::cos(angle);
                ty = radius * std::sin(angle);
            }
        } else {
            // path("...") — parse simple M/L commands for a line segment
            auto path_data_start = path_str.find('"');
            auto path_data_end = path_str.rfind('"');
            if (path_data_start != std::string::npos && path_data_end > path_data_start) {
                std::string svg_path = path_str.substr(path_data_start + 1,
                                                        path_data_end - path_data_start - 1);
                // Parse "M x1,y1 L x2,y2" — get start and end points
                float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                std::istringstream pss(svg_path);
                char cmd;
                if (pss >> cmd && (cmd == 'M' || cmd == 'm')) {
                    char sep;
                    pss >> x1;
                    if (pss.peek() == ',' || pss.peek() == ' ') pss >> sep;
                    pss >> y1;
                    if (pss >> cmd && (cmd == 'L' || cmd == 'l')) {
                        pss >> x2;
                        if (pss.peek() == ',' || pss.peek() == ' ') pss >> sep;
                        pss >> y2;
                    }
                }
                // Interpolate along the line based on offset-distance
                float t = dist / 100.0f;
                tx = x1 + (x2 - x1) * t;
                ty = y1 + (y2 - y1) * t;
            }
        }
        if (tx != 0 || ty != 0) {
            list.push_translate(tx, ty);
            transform_count++;
        }
    }

    // Individual CSS properties apply in order: translate → rotate → scale
    if (has_individual_transforms) {
        if (has_css_translate) {
            // Parse "Xpx Ypx" or "Xpx" or "X% Y%"
            std::istringstream iss(node.css_translate);
            std::string xpart, ypart;
            iss >> xpart;
            iss >> ypart;
            auto px = clever::css::parse_length(xpart);
            float tx = px ? px->to_px() : 0;
            float ty = 0;
            if (!ypart.empty()) {
                auto py = clever::css::parse_length(ypart);
                ty = py ? py->to_px() : 0;
            }
            list.push_translate(tx, ty);
            transform_count++;
        }
        if (has_css_rotate) {
            // Parse angle: "45deg", "0.5turn", "1rad"
            float angle = 0;
            std::string val = node.css_rotate;
            if (val.size() > 3 && val.substr(val.size()-3) == "deg") {
                try { angle = std::stof(val.substr(0, val.size()-3)); } catch (...) {}
            } else if (val.size() > 4 && val.substr(val.size()-4) == "turn") {
                try { angle = std::stof(val.substr(0, val.size()-4)) * 360.0f; } catch (...) {}
            } else if (val.size() > 3 && val.substr(val.size()-3) == "rad") {
                try { angle = std::stof(val.substr(0, val.size()-3)) * 180.0f / 3.14159265f; } catch (...) {}
            } else if (val.size() > 4 && val.substr(val.size()-4) == "grad") {
                try { angle = std::stof(val.substr(0, val.size()-4)) * 0.9f; } catch (...) {}
            } else {
                try { angle = std::stof(val); } catch (...) {}
            }
            list.push_rotate(angle, origin_x, origin_y);
            transform_count++;
        }
        if (has_css_scale) {
            float sx = 1, sy = 1;
            std::istringstream iss(node.css_scale);
            std::string xpart, ypart;
            iss >> xpart;
            iss >> ypart;
            try { sx = std::stof(xpart); } catch (...) {}
            sy = ypart.empty() ? sx : 0;
            if (!ypart.empty()) {
                try { sy = std::stof(ypart); } catch (...) {}
            }
            list.push_scale(sx, sy, origin_x, origin_y);
            transform_count++;
        }
    }

    if (has_transforms) {
        auto resolve_translate_value = [&](const clever::css::Transform& t, bool is_x) -> float {
            if (is_x && t.x_length.unit == clever::css::Length::Unit::Percent) {
                // Per CSS spec: translateX percentage is relative to the element's own border-box width.
                // Use border_box_width() which includes content + padding + border.
                // Fall back to geometry.width if border_box_width is 0.
                float bw = geom.border_box_width();
                if (bw <= 0) bw = node.geometry.width;
                return (t.x_length.value / 100.0f) * bw;
            }
            if (!is_x && t.y_length.unit == clever::css::Length::Unit::Percent) {
                float bh = geom.border_box_height();
                if (bh <= 0) bh = node.geometry.height;
                return (t.y_length.value / 100.0f) * bh;
            }
            return is_x ? t.x : t.y;
        };
        for (const auto& t : node.transforms) {
            const float eps = 0.00001f;
            switch (t.type) {
                case clever::css::TransformType::Translate:
                    if (t.is_3d && t.z != 0.0f) perspective_z_offset += t.z;
                    {
                        float tx = resolve_translate_value(t, true);
                        float ty = resolve_translate_value(t, false);
                        list.push_translate(tx, ty);
                    }
                    transform_count++;
                    break;
                case clever::css::TransformType::Rotate:
                    if (!t.is_3d) {
                        list.push_rotate(t.angle, origin_x, origin_y);
                        break;
                    }
                    if (std::fabs(t.axis_x) <= eps && std::fabs(t.axis_y) <= eps) {
                        list.push_rotate(t.angle, origin_x, origin_y);
                        break;
                    }
                    if (std::fabs(t.axis_x - 1.0f) <= eps && std::fabs(t.axis_y) <= eps && std::fabs(t.axis_z) <= eps) {
                        float rx = 1.0f - (1.0f - std::fabs(std::cos(t.angle * 3.14159265f / 180.0f))) * 0.2f;
                        list.push_scale(1.0f, rx, origin_x, origin_y);
                        break;
                    }
                    if (std::fabs(t.axis_y - 1.0f) <= eps && std::fabs(t.axis_x) <= eps && std::fabs(t.axis_z) <= eps) {
                        float ry = 1.0f - (1.0f - std::fabs(std::cos(t.angle * 3.14159265f / 180.0f))) * 0.2f;
                        list.push_scale(ry, 1.0f, origin_x, origin_y);
                        break;
                    }
                    if (std::fabs(t.axis_z) <= eps) {
                        list.push_rotate(t.angle, origin_x, origin_y);
                        break;
                    }
                    if (std::fabs(t.axis_x) > std::fabs(t.axis_y)) {
                        float ry = 1.0f - (1.0f - std::fabs(std::cos(t.angle * 3.14159265f / 180.0f))) * 0.2f;
                        list.push_scale(1.0f, ry, origin_x, origin_y);
                    } else {
                        float rx = 1.0f - (1.0f - std::fabs(std::cos(t.angle * 3.14159265f / 180.0f))) * 0.2f;
                        list.push_scale(rx, 1.0f, origin_x, origin_y);
                    }
                    if (t.axis_z != 0.0f) {
                        float nz = t.axis_z / std::sqrt(t.axis_x * t.axis_x + t.axis_y * t.axis_y + t.axis_z * t.axis_z);
                        if (std::fabs(nz) > eps) {
                            list.push_rotate(t.angle * nz, origin_x, origin_y);
                        }
                    }
                    transform_count++;
                    break;
                case clever::css::TransformType::Scale:
                    if (!t.is_3d || t.z_scale == 1.0f) {
                        list.push_scale(t.x, t.y, origin_x, origin_y);
                    } else {
                        float z_factor = 1.0f + (t.z_scale - 1.0f) * 0.08f;
                        perspective_z_offset += (t.z_scale - 1.0f) * 20.0f;
                        list.push_scale(t.x * z_factor, t.y * z_factor, origin_x, origin_y);
                    }
                    transform_count++;
                    break;
                case clever::css::TransformType::Skew:
                    list.push_skew(t.x, t.y, origin_x, origin_y);
                    transform_count++;
                    break;
                case clever::css::TransformType::Matrix: {
                    if (t.is_3d && std::fabs(t.m4[11]) > 1e-6f &&
                        std::fabs(t.m4[0] - 1.0f) <= 1e-5f && std::fabs(t.m4[1]) <= 1e-5f &&
                        std::fabs(t.m4[2]) <= 1e-5f && std::fabs(t.m4[3]) <= 1e-5f &&
                        std::fabs(t.m4[4]) <= 1e-5f && std::fabs(t.m4[5] - 1.0f) <= 1e-5f &&
                        std::fabs(t.m4[6]) <= 1e-5f && std::fabs(t.m4[7]) <= 1e-5f &&
                        std::fabs(t.m4[8]) <= 1e-5f && std::fabs(t.m4[9]) <= 1e-5f &&
                        std::fabs(t.m4[10] - 1.0f) <= 1e-5f && std::fabs(t.m4[12]) <= 1e-5f &&
                        std::fabs(t.m4[13]) <= 1e-5f && std::fabs(t.m4[15] - 1.0f) <= 1e-5f) {
                        float local_perspective = -1.0f / t.m4[11];
                        if (local_perspective > 0.0f) active_perspective = local_perspective;
                        break;
                    }
                    // CSS matrix(a,b,c,d,e,f) with transform-origin (ox,oy):
                    // Effective = T(ox,oy) * M * T(-ox,-oy)
                    // Adjusted e' = e + ox*(1 - a) - c*oy
                    // Adjusted f' = f - b*ox + oy*(1 - d)
                    float a = t.is_3d ? t.m4[0] : t.m[0];
                    float b = t.is_3d ? t.m4[1] : t.m[1];
                    float c = t.is_3d ? t.m4[4] : t.m[2];
                    float d = t.is_3d ? t.m4[5] : t.m[3];
                    float e = (t.is_3d ? t.m4[12] : t.m[4]) + origin_x * (1.0f - a) - c * origin_y;
                    float f = (t.is_3d ? t.m4[13] : t.m[5]) - b * origin_x + origin_y * (1.0f - d);
                    list.push_matrix(a, b, c, d, e, f);
                    transform_count++;
                    break;
                }
                case clever::css::TransformType::None:
                    break;
            }
        }
    }

    // CSS perspective: parent's perspective creates foreshortening on transformed children
    // Use perspective depth where available to derive projected scale.
    if (active_perspective > 0.0f && transform_count > 0 && perspective_z_offset != 0.0f) {
        float perspective = active_perspective + perspective_z_offset;
        if (perspective < 1.0f) perspective = 1.0f;
        float factor = active_perspective / perspective;
        if (factor < 0.1f) factor = 0.1f;
        if (factor > 1.6f) factor = 1.6f;
        list.push_scale(factor, factor, origin_x, origin_y);
        transform_count++;
    }

    // Save backdrop for mix-blend-mode before any painting of this node
    bool has_blend_mode = (node.mix_blend_mode != 0);
    if (has_blend_mode) {
        Rect blend_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        list.save_backdrop(blend_bounds);
    }

    // For visibility:hidden, skip painting this node's visuals but still
    // recurse into children (they may override with visibility:visible)
    // and still take up layout space.
    if (!node.visibility_hidden) {
        // Apply CSS backdrop-filter to existing backdrop pixels BEFORE painting this element
        if (!node.backdrop_filters.empty()) {
            Rect backdrop_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
            for (auto& [ftype, fval] : node.backdrop_filters) {
                list.apply_backdrop_filter(backdrop_bounds, ftype, fval);
            }
        }

        // Apply clip-path clipping before painting shadows/background
        if (node.clip_path_type != 0) {
            Rect clip_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
            list.apply_clip_path(clip_bounds, node.clip_path_type, node.clip_path_values);
        }

        // Paint outer box shadows before background (render in reverse: last shadow first)
        if (!node.box_shadows.empty()) {
            for (int si = static_cast<int>(node.box_shadows.size()) - 1; si >= 0; --si) {
                auto& bs = node.box_shadows[static_cast<size_t>(si)];
                if (bs.color == 0x00000000 || bs.inset) continue;
                uint32_t sc = bs.color;
                uint8_t sa = static_cast<uint8_t>((sc >> 24) & 0xFF);
                uint8_t sr = static_cast<uint8_t>((sc >> 16) & 0xFF);
                uint8_t sg = static_cast<uint8_t>((sc >> 8) & 0xFF);
                uint8_t sb_c = static_cast<uint8_t>(sc & 0xFF);
                float spread = bs.spread;
                float shadow_x = abs_x + bs.offset_x - spread;
                float shadow_y = abs_y + bs.offset_y - spread;
                float w = geom.border_box_width() + spread * 2;
                float h = geom.border_box_height() + spread * 2;
                float blur = bs.blur;
                // Resolve per-corner radii for shadow shape
                bool shadow_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                                       node.border_radius_bl > 0 || node.border_radius_br > 0);
                float s_tl = shadow_has_per ? node.border_radius_tl : node.border_radius;
                float s_tr = shadow_has_per ? node.border_radius_tr : node.border_radius;
                float s_bl = shadow_has_per ? node.border_radius_bl : node.border_radius;
                float s_br = shadow_has_per ? node.border_radius_br : node.border_radius;
                if (blur > 0) {
                    float expand = blur * 3.0f;
                    Rect shadow_rect = {shadow_x - expand, shadow_y - expand,
                                        w + expand * 2, h + expand * 2};
                    Rect element_rect = {shadow_x, shadow_y, w, h};
                    list.fill_box_shadow(shadow_rect, element_rect,
                                         {sr, sg, sb_c, sa}, blur, s_tl, s_tr, s_bl, s_br);
                } else {
                    Rect shadow_rect = {shadow_x, shadow_y, w, h};
                    bool any_radius = (s_tl > 0 || s_tr > 0 || s_bl > 0 || s_br > 0);
                    if (any_radius) {
                        list.fill_rounded_rect(shadow_rect, {sr, sg, sb_c, sa}, s_tl, s_tr, s_bl, s_br);
                    } else {
                        list.fill_rect(shadow_rect, {sr, sg, sb_c, sa});
                    }
                }
            }
        } else if (node.shadow_color != 0x00000000 && !node.shadow_inset) {
            // Legacy single shadow fallback
            uint32_t sc = node.shadow_color;
            uint8_t sa = static_cast<uint8_t>((sc >> 24) & 0xFF);
            uint8_t sr = static_cast<uint8_t>((sc >> 16) & 0xFF);
            uint8_t sg = static_cast<uint8_t>((sc >> 8) & 0xFF);
            uint8_t sb_c = static_cast<uint8_t>(sc & 0xFF);
            float spread = node.shadow_spread;
            float shadow_x = abs_x + node.shadow_offset_x - spread;
            float shadow_y = abs_y + node.shadow_offset_y - spread;
            float w = geom.border_box_width() + spread * 2;
            float h = geom.border_box_height() + spread * 2;
            float blur = node.shadow_blur;
            bool leg_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                                node.border_radius_bl > 0 || node.border_radius_br > 0);
            float l_tl = leg_has_per ? node.border_radius_tl : node.border_radius;
            float l_tr = leg_has_per ? node.border_radius_tr : node.border_radius;
            float l_bl = leg_has_per ? node.border_radius_bl : node.border_radius;
            float l_br = leg_has_per ? node.border_radius_br : node.border_radius;
            if (blur > 0) {
                float expand = blur * 3.0f;
                Rect shadow_rect = {shadow_x - expand, shadow_y - expand,
                                    w + expand * 2, h + expand * 2};
                Rect element_rect = {shadow_x, shadow_y, w, h};
                list.fill_box_shadow(shadow_rect, element_rect,
                                     {sr, sg, sb_c, sa}, blur, l_tl, l_tr, l_bl, l_br);
            } else {
                Rect shadow_rect = {shadow_x, shadow_y, w, h};
                bool any_radius = (l_tl > 0 || l_tr > 0 || l_bl > 0 || l_br > 0);
                if (any_radius) {
                    list.fill_rounded_rect(shadow_rect, {sr, sg, sb_c, sa}, l_tl, l_tr, l_bl, l_br);
                } else {
                    list.fill_rect(shadow_rect, {sr, sg, sb_c, sa});
                }
            }
        }

        // Paint this node's background first
        paint_background(node, list, abs_x, abs_y);

        // Paint inset box shadows (after background, before content) — render in reverse
        // Resolve per-corner radii for inset shadow clipping
        bool inset_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                               node.border_radius_bl > 0 || node.border_radius_br > 0);
        float i_tl = inset_has_per ? node.border_radius_tl : node.border_radius;
        float i_tr = inset_has_per ? node.border_radius_tr : node.border_radius;
        float i_bl = inset_has_per ? node.border_radius_bl : node.border_radius;
        float i_br = inset_has_per ? node.border_radius_br : node.border_radius;
        Rect element_box = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};

        if (!node.box_shadows.empty()) {
            for (int si = static_cast<int>(node.box_shadows.size()) - 1; si >= 0; --si) {
                auto& bs = node.box_shadows[static_cast<size_t>(si)];
                if (bs.color == 0x00000000 || !bs.inset) continue;
                uint32_t sc = bs.color;
                uint8_t sa = static_cast<uint8_t>((sc >> 24) & 0xFF);
                uint8_t sr_c = static_cast<uint8_t>((sc >> 16) & 0xFF);
                uint8_t sg_c = static_cast<uint8_t>((sc >> 8) & 0xFF);
                uint8_t sb_c = static_cast<uint8_t>(sc & 0xFF);
                list.fill_inset_shadow(element_box, {sr_c, sg_c, sb_c, sa},
                                       bs.blur, bs.offset_x, bs.offset_y, bs.spread,
                                       i_tl, i_tr, i_bl, i_br);
            }
        } else if (node.shadow_color != 0x00000000 && node.shadow_inset) {
            // Legacy single inset shadow fallback
            uint32_t sc = node.shadow_color;
            uint8_t sa = static_cast<uint8_t>((sc >> 24) & 0xFF);
            uint8_t sr_c = static_cast<uint8_t>((sc >> 16) & 0xFF);
            uint8_t sg_c = static_cast<uint8_t>((sc >> 8) & 0xFF);
            uint8_t sb_c = static_cast<uint8_t>(sc & 0xFF);
            list.fill_inset_shadow(element_box, {sr_c, sg_c, sb_c, sa},
                                   node.shadow_blur, node.shadow_offset_x, node.shadow_offset_y,
                                   node.shadow_spread, i_tl, i_tr, i_bl, i_br);
        }

        // Paint inline SVG containers (rasterized via nanosvg) — only when no child layout nodes
        // (i.e., image-sourced SVGs). Inline DOM SVGs with children use native shape painting.
        if (node.is_svg && node.svg_type == 0 && !node.svg_content.empty() && node.children.empty()) {
            auto decoded = decode_svg_image(node.svg_content,
                                           node.geometry.width > 0 ? node.geometry.width : 300);
            if (decoded.pixels) {
                auto img = std::make_shared<ImageData>();
                img->pixels = *decoded.pixels;
                img->width = decoded.width;
                img->height = decoded.height;
                Rect dest = {abs_x, abs_y, static_cast<float>(decoded.width), static_cast<float>(decoded.height)};
                list.draw_image(dest, std::move(img));
            }
            return;
        }

        // Paint SVG shape elements
        if (node.is_svg && node.svg_type > 0) {
            paint_svg_shape(node, list, abs_x, abs_y);
        }

        // SVG <use> — find referenced element by ID and paint it at <use> position
        if (node.is_svg_use && !node.svg_use_href.empty()) {
            std::string ref_id = node.svg_use_href;
            if (!ref_id.empty() && ref_id[0] == '#') ref_id = ref_id.substr(1);
            // Walk the tree to find the referenced element
            const clever::layout::LayoutNode* ref_node = nullptr;
            std::function<void(const clever::layout::LayoutNode&)> find_by_id =
                [&](const clever::layout::LayoutNode& n) {
                    if (ref_node) return;
                    if (n.element_id == ref_id) { ref_node = &n; return; }
                    for (auto& c : n.children) find_by_id(*c);
                };
            // Walk from root (find root by going up through parents)
            const clever::layout::LayoutNode* root = &node;
            while (root->parent) root = root->parent;
            find_by_id(*root);
            if (ref_node) {
                // Paint the referenced element at the <use> position
                float use_x = abs_x + node.svg_use_x;
                float use_y = abs_y + node.svg_use_y;
                // Paint the ref node's SVG shape
                if (ref_node->is_svg && ref_node->svg_type > 0) {
                    paint_svg_shape(*ref_node, list, use_x, use_y);
                }
                // Also paint the ref node's children
                for (auto& child : ref_node->children) {
                    paint_node(*child, list, use_x, use_y);
                }
            }
        }

        // Record link region if this node is inside a link
        if (!node.link_href.empty()) {
            list.add_link(
                {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()},
                node.link_href, node.link_target);
        }

        // Record form submit region if this node is a submit button
        if (node.form_index >= 0) {
            list.add_form_submit_region(
                {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()},
                node.form_index);
        }

        // Record cursor region if this node has a non-auto cursor
        if (node.cursor != 0) {
            list.add_cursor_region(
                {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()},
                node.cursor);
        }

        // Record details toggle region for <summary> elements
        if (node.is_summary && node.details_id >= 0) {
            list.add_details_toggle_region(
                {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()},
                node.details_id);
        }

        // Paint borders
        paint_borders(node, list, abs_x, abs_y);

        // Paint column rules between columns
        if (node.column_count > 1 && node.column_rule_width > 0) {
            float content_x = abs_x + geom.border.left + geom.padding.left;
            float content_w = geom.width - geom.padding.left - geom.padding.right
                              - geom.border.left - geom.border.right;
            float gap = node.column_gap_val;
            float col_w = (content_w - gap * static_cast<float>(node.column_count - 1))
                          / static_cast<float>(node.column_count);
            float content_h = geom.height - geom.padding.top - geom.padding.bottom
                              - geom.border.top - geom.border.bottom;
            float rule_y = abs_y + geom.border.top + geom.padding.top;

            uint32_t rc = node.column_rule_color;
            Color rule_color = Color::from_argb(rc);

            for (int i = 1; i < node.column_count; i++) {
                float rule_x = content_x + static_cast<float>(i) * (col_w + gap)
                               - gap / 2.0f - node.column_rule_width / 2.0f;
                int rs = node.column_rule_style; // 0=none, 1=solid, 2=dashed, 3=dotted
                if (rs == 0) continue; // none — skip
                if (rs == 3) {
                    // Dotted: draw circles along the rule
                    float dot_r = node.column_rule_width / 2.0f;
                    float dot_spacing = node.column_rule_width * 2.0f;
                    float cx = rule_x + dot_r;
                    for (float dy = dot_r; dy < content_h; dy += dot_spacing) {
                        list.fill_rect({cx - dot_r, rule_y + dy - dot_r,
                                        dot_r * 2, dot_r * 2}, rule_color);
                    }
                } else if (rs == 2) {
                    // Dashed: draw segments along the rule
                    float dash_len = node.column_rule_width * 3.0f;
                    float gap_len = node.column_rule_width * 2.0f;
                    for (float dy = 0; dy < content_h; dy += dash_len + gap_len) {
                        float seg_h = std::min(dash_len, content_h - dy);
                        list.fill_rect({rule_x, rule_y + dy,
                                        node.column_rule_width, seg_h}, rule_color);
                    }
                } else if (rs == 4) {
                    // Double: two thin lines with a gap
                    float line_w = node.column_rule_width / 3.0f;
                    float gap = line_w;
                    float left_x = rule_x;
                    float right_x = rule_x + line_w + gap;
                    list.fill_rect({left_x, rule_y, line_w, content_h}, rule_color);
                    list.fill_rect({right_x, rule_y, line_w, content_h}, rule_color);
                } else if (rs == 5 || rs == 6) {
                    // Groove (5): lighter left / darker right
                    // Ridge (6): darker left / lighter right
                    float half_w = node.column_rule_width / 2.0f;
                    float left_alpha = static_cast<float>(rule_color.a) * 0.75f;
                    float right_alpha = static_cast<float>(rule_color.a) * 0.75f;
                    float left_target = (rs == 5) ? 255.0f : 0.0f;
                    float right_target = (rs == 5) ? 0.0f : 255.0f;
                    float left_r = (static_cast<float>(rule_color.r) * 0.5f) + (left_target * 0.5f);
                    float left_g = (static_cast<float>(rule_color.g) * 0.5f) + (left_target * 0.5f);
                    float left_b = (static_cast<float>(rule_color.b) * 0.5f) + (left_target * 0.5f);
                    float right_r = (static_cast<float>(rule_color.r) * 0.5f) + (right_target * 0.5f);
                    float right_g = (static_cast<float>(rule_color.g) * 0.5f) + (right_target * 0.5f);
                    float right_b = (static_cast<float>(rule_color.b) * 0.5f) + (right_target * 0.5f);
                    Color left_color = {
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, left_r))),
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, left_g))),
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, left_b))),
                        static_cast<uint8_t>(std::max(0.0f, left_alpha))
                    };
                    Color right_color = {
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, right_r))),
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, right_g))),
                        static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, right_b))),
                        static_cast<uint8_t>(std::max(0.0f, right_alpha))
                    };
                    list.fill_rect({rule_x, rule_y, half_w, content_h}, left_color);
                    list.fill_rect({rule_x + half_w, rule_y,
                                    node.column_rule_width - half_w, content_h}, right_color);
                } else {
                    // Solid (1) or any other — draw as solid rect
                    list.fill_rect({rule_x, rule_y, node.column_rule_width, content_h}, rule_color);
                }
            }
        }

        // Paint outline (outside the border box, does not affect layout)
        paint_outline(node, list, abs_x, abs_y);

        // Paint resize grip if element has resize property
        if (node.resize > 0) {
            float bw = geom.border_box_width();
            float bh = geom.border_box_height();
            float grip_size = 12.0f;
            float gx = abs_x + bw - grip_size;
            float gy = abs_y + bh - grip_size;
            Color grip_color = {0x99, 0x99, 0x99, 0xFF};
            // Draw 3 diagonal lines (classic resize grip)
            for (int i = 0; i < 3; i++) {
                float offset = static_cast<float>(i) * 4.0f;
                float x1 = gx + grip_size - offset;
                float y1 = gy + grip_size;
                float x2 = gx + grip_size;
                float y2 = gy + grip_size - offset;
                // Approximate diagonal line with small rects
                int steps = static_cast<int>(offset + 1);
                for (int s = 0; s <= steps; s++) {
                    float t = (steps > 0) ? static_cast<float>(s) / static_cast<float>(steps) : 0;
                    float px = x1 + (x2 - x1) * t;
                    float py = y1 + (y2 - y1) * t;
                    list.fill_rect({px - 0.5f, py - 0.5f, 1.5f, 1.5f}, grip_color);
                }
            }
        }

        // Paint image if this node has decoded image data
        const bool should_lazy_skip =
            node.loading_lazy &&
            (abs_y > (viewport_scroll_y_ + (2.0f * viewport_height_)));

        if (should_lazy_skip) {
            Color placeholder_color = {0xE5, 0xE5, 0xE5, 0xFF};
            list.fill_rect({abs_x, abs_y, geom.border_box_width(), geom.border_box_height()}, placeholder_color);
        } else if (node.image_pixels && !node.image_pixels->empty()) {
            auto img = std::make_shared<ImageData>();
            img->pixels = *node.image_pixels;
            img->width = node.image_width;
            img->height = node.image_height;

            float box_w = geom.border_box_width();
            float box_h = geom.border_box_height();
            float img_w = static_cast<float>(node.image_width);
            float img_h = static_cast<float>(node.image_height);
            Rect dest = {abs_x, abs_y, box_w, box_h};

            // Use precomputed rendered_img_* if available (from object-fit + object-position)
            if (node.rendered_img_w > 0 && node.rendered_img_h > 0) {
                dest.x = abs_x + node.rendered_img_x;
                dest.y = abs_y + node.rendered_img_y;
                dest.width = node.rendered_img_w;
                dest.height = node.rendered_img_h;
            } else if (node.object_fit != 0 && img_w > 0 && img_h > 0) {
                float draw_w = box_w, draw_h = box_h;
                if (node.object_fit == 1) {
                    // contain: scale to fit, maintain aspect ratio
                    float scale = std::min(box_w / img_w, box_h / img_h);
                    draw_w = img_w * scale;
                    draw_h = img_h * scale;
                } else if (node.object_fit == 2) {
                    // cover: scale to fill, maintain aspect ratio
                    float scale = std::max(box_w / img_w, box_h / img_h);
                    draw_w = img_w * scale;
                    draw_h = img_h * scale;
                } else if (node.object_fit == 3) {
                    // none: natural size
                    draw_w = img_w;
                    draw_h = img_h;
                } else if (node.object_fit == 4) {
                    // scale-down: smaller of none or contain
                    float scale = std::min(box_w / img_w, box_h / img_h);
                    if (scale < 1.0f) {
                        draw_w = img_w * scale;
                        draw_h = img_h * scale;
                    } else {
                        draw_w = img_w;
                        draw_h = img_h;
                    }
                }
                // Position using object-position percentages (default 50% 50% = center)
                float pos_x = node.object_position_x; // 0-100%
                float pos_y = node.object_position_y;
                dest.x = abs_x + (box_w - draw_w) * (pos_x / 100.0f);
                dest.y = abs_y + (box_h - draw_h) * (pos_y / 100.0f);
                dest.width = draw_w;
                dest.height = draw_h;
            }

            // image-orientation: flip (2) — horizontal mirror
            if (node.image_orientation == 2 && node.image_orientation_explicit &&
                img->width > 0 && img->height > 0) {
                int w = img->width;
                int h = img->height;
                auto& px = img->pixels;
                for (int row = 0; row < h; row++) {
                    for (int col = 0; col < w / 2; col++) {
                        int left = (row * w + col) * 4;
                        int right = (row * w + (w - 1 - col)) * 4;
                        std::swap(px[left], px[right]);
                        std::swap(px[left + 1], px[right + 1]);
                        std::swap(px[left + 2], px[right + 2]);
                        std::swap(px[left + 3], px[right + 3]);
                    }
                }
            }

            bool clip_image = false;
            if (node.object_fit == 2) {
                clip_image = true;
            } else if ((node.object_fit == 3 || node.object_fit == 4) &&
                       (dest.width > geom.width || dest.height > geom.height)) {
                clip_image = true;
            }

            if (clip_image) {
                list.push_clip({
                    abs_x + geom.border.left,
                    abs_y + geom.border.top,
                    geom.width + geom.padding.left + geom.padding.right,
                    geom.height + geom.padding.top + geom.padding.bottom
                });
            }

            list.draw_image(dest, std::move(img), node.image_rendering);

            if (clip_image) {
                list.pop_clip();
            }
        }

        // Paint broken image indicator when img has no image data but has alt text
        if (!node.loading_lazy &&
            !node.img_alt_text.empty() && (!node.image_pixels || node.image_pixels->empty())) {
            // Draw a small broken image icon (gray box outline) in the top-left padding area
            float icon_size = 16.0f;
            float icon_x = abs_x + geom.border.left + 4;
            float icon_y = abs_y + geom.border.top + 4;
            // Outer box of the icon
            Color icon_color = {0x99, 0x99, 0x99, 0xFF};
            list.draw_border({icon_x, icon_y, icon_size, icon_size},
                             icon_color, 1, 1, 1, 1, 0, 1);
            // Simple mountain shape inside: a small triangle (approximated with lines)
            list.draw_line(icon_x + 3, icon_y + icon_size - 4,
                           icon_x + icon_size * 0.5f, icon_y + 5,
                           icon_color, 1.0f);
            list.draw_line(icon_x + icon_size * 0.5f, icon_y + 5,
                           icon_x + icon_size - 3, icon_y + icon_size - 4,
                           icon_color, 1.0f);
            // Small circle in top-right (sun indicator)
            float sun_r = 2.0f;
            float sun_cx = icon_x + icon_size - 5;
            float sun_cy = icon_y + 5;
            list.fill_rect({sun_cx - sun_r, sun_cy - sun_r, sun_r * 2, sun_r * 2}, icon_color);
        }

        // Paint dialog backdrop if this is an open <dialog>
        if (node.is_dialog && node.dialog_open) {
            // Draw semi-transparent black backdrop overlay behind the dialog
            list.fill_rect({0, 0, 10000, 10000},
                           {0, 0, 0, 64}); // black at ~25% opacity (64/255)
        }

        // Paint marquee if this is <marquee>
        if (node.is_marquee) {
            paint_marquee(node, list, abs_x, abs_y);
        }

        // Paint ruby annotation if this is <rt> (ruby text)
        if (node.is_ruby_text) {
            paint_ruby_annotation(node, list, abs_x, abs_y);
        }

        // Paint legend background gap if this is <legend> inside a <fieldset>
        if (node.is_legend) {
            // Draw a background behind the legend to "cut" through the fieldset top border
            // Use parent's background color if available, otherwise white
            float lx = abs_x;
            float ly = abs_y;
            float lw = geom.border_box_width();
            float lh = geom.border_box_height();
            Color legend_bg = {255, 255, 255, 255};
            if (node.parent && node.parent->parent) {
                // Use the grandparent's (or page) background color
                uint32_t bg = node.parent->parent->background_color;
                if (bg != 0) {
                    legend_bg = {
                        static_cast<uint8_t>((bg >> 16) & 0xFF),
                        static_cast<uint8_t>((bg >> 8) & 0xFF),
                        static_cast<uint8_t>(bg & 0xFF),
                        static_cast<uint8_t>((bg >> 24) & 0xFF)
                    };
                }
            }
            list.fill_rect({lx - 2, ly, lw + 4, lh}, legend_bg);
        }

        // Paint canvas placeholder if this is <canvas>
        if (node.is_canvas) {
            paint_canvas_placeholder(node, list, abs_x, abs_y);
        }

        // Paint media placeholder if this is <video> or <audio>
        if (node.media_type > 0) {
            paint_media_placeholder(node, list, abs_x, abs_y);
        }

        // Paint iframe placeholder if this is <iframe>
        if (node.is_iframe) {
            paint_iframe_placeholder(node, list, abs_x, abs_y);
        }

        // Paint native text input box decoration (background + inset border)
        if (node.is_text_input && node.appearance != 1) {
            paint_text_input(node, list, abs_x, abs_y);
        }

        // Paint textarea decoration (background + border + scrollbar indicator + resize handle)
        if (node.is_textarea && node.appearance != 1) {
            paint_textarea(node, list, abs_x, abs_y);
        }

        // Paint native button decoration (gradient background + raised border)
        if (node.is_button_input && node.appearance != 1) {
            paint_button_input(node, list, abs_x, abs_y);
        }

        // Paint range input slider if this is <input type="range">
        if (node.is_range_input && node.appearance != 1) {
            paint_range_input(node, list, abs_x, abs_y);
        }

        // Paint color input swatch if this is <input type="color">
        if (node.is_color_input) {
            paint_color_input(node, list, abs_x, abs_y);
        }

        // Paint checkbox or radio input (skip if appearance:none)
        if (node.is_checkbox && node.appearance != 1) {
            paint_checkbox(node, list, abs_x, abs_y);
        }
        if (node.is_radio && node.appearance != 1) {
            paint_radio(node, list, abs_x, abs_y);
        }

        // Paint select element with dropdown arrow (skip if appearance:none)
        if (node.is_select_element && node.appearance != 1) {
            paint_select_element(node, list, abs_x, abs_y);
        }

        // Paint caret for input/textarea elements that have caret-color set
        if (node.caret_color != 0 &&
            (node.tag_name == "input" || node.tag_name == "textarea")) {
            paint_caret(node, list, abs_x, abs_y);
        }

         // List markers are rendered via marker text nodes created in render_pipeline
         // for inside-positioned markers. paint_list_marker() handles outside markers
         // (drawn to the left of the content box) and list-style-image markers.
         if (node.is_list_item &&
             (node.list_style_type != 9 || !node.list_style_image.empty())) {
             // list_style_position == 1 is inside. render_pipeline prepends a marker
             // text node as the first child for inside markers — detect it to avoid
             // double-painting. For outside (0) or image markers, always call the painter.
             bool has_inside_marker_node = false;
             if (node.list_style_position == 1 &&
                 !node.children.empty() && node.children[0]->is_text) {
                 const auto& txt = node.children[0]->text_content;
                 // Bullet/circle/square start with 0xE2 (UTF-8 3-byte sequence).
                 // Numeric/alpha markers have form "N. " (ends with space).
                 if (!txt.empty() &&
                     (static_cast<unsigned char>(txt[0]) == 0xE2u ||
                      (txt.size() >= 2 && txt.back() == ' ' &&
                       (std::isdigit(static_cast<unsigned char>(txt[0])) ||
                        std::isalpha(static_cast<unsigned char>(txt[0])))))) {
                     has_inside_marker_node = true;
                 }
             }
             if (!has_inside_marker_node) {
                 float content_x = abs_x + geom.border.left + geom.padding.left;
                 float content_y = abs_y + geom.border.top + geom.padding.top;
                 paint_list_marker(node, list, content_x, content_y);
             }
         }

        // Paint quotation marks for <q> inline quotation elements
        if (node.is_q) {
            uint32_t tc = node.color;
            Color q_color = {
                static_cast<uint8_t>((tc >> 16) & 0xFF),
                static_cast<uint8_t>((tc >> 8) & 0xFF),
                static_cast<uint8_t>(tc & 0xFF),
                static_cast<uint8_t>((tc >> 24) & 0xFF)
            };
            float content_x = abs_x + geom.border.left + geom.padding.left;
            float content_y = abs_y + geom.border.top + geom.padding.top;
            // Draw opening left double quotation mark before content
            list.draw_text("\xe2\x80\x9c", content_x - node.font_size * 0.6f, content_y,
                           node.font_size, q_color);
            // Draw closing right double quotation mark after content
            float end_x = content_x + geom.width;
            list.draw_text("\xe2\x80\x9d", end_x, content_y,
                           node.font_size, q_color);
        }

        // Paint text if this is a text node
        paint_text(node, list, abs_x, abs_y);
    }

    // Compute the content area offset for children
    float child_offset_x = abs_x + geom.border.left + geom.padding.left;
    float child_offset_y = abs_y + geom.border.top + geom.padding.top;

    // Apply scroll offset for scroll containers (overflow: hidden/scroll/auto)
    // Children are shifted by -scroll_top/-scroll_left so that scrolled content
    // appears in the correct position within the clipped viewport.
    if (node.is_scroll_container) {
        child_offset_x -= node.scroll_left;
        child_offset_y -= node.scroll_top;
    }

    // Apply SVG <g> group transform offset to children
    if (node.is_svg_group) {
        child_offset_x += node.svg_transform_tx;
        child_offset_y += node.svg_transform_ty;
    }

    // Apply SVG group scale: multiply child coordinates
    // (Scale is applied by modifying SVG attrs in paint_svg_shape via viewBox-like scaling)
    // For now, we store the group scale and it gets picked up by viewBox computation

    // Push clip rect if overflow is hidden or contain includes paint
    // contain: 1=strict (all), 2=content (layout+style+paint), 6=paint
    bool contain_paint = (node.contain == 1 || node.contain == 2 || node.contain == 6);
    bool clipping = (node.overflow >= 1 || contain_paint); // 1=hidden, 2=scroll, 3=auto
    if (clipping) {
        // CSS overflow clips to the padding box (content + padding, excluding border).
        list.push_clip({
            abs_x + geom.border.left,
            abs_y + geom.border.top,
            geom.padding.left + geom.width + geom.padding.right,
            geom.padding.top + geom.height + geom.padding.bottom
        });
    }

    // CSS Stacking Context: properly separate stacking context children from
    // normal flow children. Per CSS spec, a new stacking context is created when:
    // - Root element (<html>)
    // - Positioned (abs/rel/fixed/sticky) with z-index != auto
    // - Flex/grid item with z-index != auto
    // - opacity < 1
    // - Has CSS transforms
    // - Has CSS filters
    // - mix-blend-mode != normal
    // - isolation: isolate
    // - will-change specifying opacity, transform, or filter
    const bool parent_is_flex_grid = (node.display == clever::layout::DisplayType::Flex ||
                                     node.display == clever::layout::DisplayType::InlineFlex ||
                                     node.display == clever::layout::DisplayType::Grid ||
                                     node.display == clever::layout::DisplayType::InlineGrid);
    auto creates_stacking_context = [&](const clever::layout::LayoutNode& child) -> bool {
        // Root element
        if (child.tag_name == "html" || child.tag_name == "HTML") return true;
        // Positioned element with explicit z-index (including explicit 0)
        if (!clever::layout::is_z_index_auto(child.z_index) && child.position_type >= 1) return true;
        // Flex/grid item with explicit z-index creates a stacking context.
        if (!clever::layout::is_z_index_auto(child.z_index) && parent_is_flex_grid) return true;
        // Opacity < 1
        if (child.opacity < 1.0f) return true;
        // CSS transforms
        if (!child.transforms.empty()) return true;
        // CSS filters
        if (!child.filters.empty()) return true;
        // mix-blend-mode != normal
        if (child.mix_blend_mode != 0) return true;
        // isolation: isolate
        if (child.isolation == 1) return true;
        // contain: paint creates stacking context
        if (child.contain == 1 || child.contain == 2 || child.contain == 6) return true;
        // will-change for stacking-context-creating properties
        if (!child.will_change.empty() && child.will_change != "auto") {
            if (child.will_change.find("opacity") != std::string::npos ||
                child.will_change.find("transform") != std::string::npos ||
                child.will_change.find("filter") != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    // Separate children into stacking context participants and normal flow
    std::vector<const clever::layout::LayoutNode*> stacking_negative; // z-index < 0
    std::vector<const clever::layout::LayoutNode*> stacking_non_negative; // z-index >= 0
    std::vector<const clever::layout::LayoutNode*> normal_flow; // DOM order

    for (auto& child : node.children) {
        auto* c = child.get();
        if (creates_stacking_context(*c)) {
            if (c->z_index < 0) {
                stacking_negative.push_back(c);
            } else {
                stacking_non_negative.push_back(c);
            }
        } else {
            normal_flow.push_back(c);
        }
    }

    // Sort stacking context children by z-index (stable sort preserves DOM order for ties)
    if (stacking_negative.size() > 1) {
        std::stable_sort(stacking_negative.begin(), stacking_negative.end(),
            [](const clever::layout::LayoutNode* a, const clever::layout::LayoutNode* b) {
                return a->z_index < b->z_index;
            });
    }
    if (stacking_non_negative.size() > 1) {
        std::stable_sort(stacking_non_negative.begin(), stacking_non_negative.end(),
            [](const clever::layout::LayoutNode* a, const clever::layout::LayoutNode* b) {
                return a->z_index < b->z_index;
            });
    }

    // Helper lambda to paint a single child node with proper offset
    auto paint_child = [&](const clever::layout::LayoutNode* child) {
        // position:fixed elements are positioned relative to the viewport (0,0),
        // not their parent's content area. Their geometry.x/y already contain
        // viewport-relative coordinates from layout.
        if (child->position_type == 3) {
            paint_node(*child, list, 0.0f, 0.0f);
        } else if (child->position_type == 4) {
            // Sticky element sticks to edges within its nearest scroll container ancestor.
            float sticky_offset_x = 0.0f, sticky_offset_y = 0.0f;
            const auto& child_geom = child->geometry;
            const float child_box_w = child_geom.border_box_width();
            const float child_box_h = child_geom.border_box_height();
            const float normal_x = child->sticky_original_x;
            const float normal_y = child->sticky_original_y;
            const float child_abs_x = child_offset_x + normal_x;
            const float child_abs_y = child_offset_y + normal_y;
            float sc_content_x = 0.0f;
            float sc_content_y = 0.0f;
            float sc_scroll_x = viewport_scroll_x_;
            float sc_scroll_y = viewport_scroll_y_;
            float sc_w = viewport_width_ > 0.0f ? viewport_width_ : 1e9f;
            float sc_h = viewport_height_ > 0.0f ? viewport_height_ : 1e9f;
            auto* sticky_child = const_cast<clever::layout::LayoutNode*>(child);

            // Find the effective scroll container: first check the direct parent (node),
            // then walk up the ancestor chain via parent pointers.
            const clever::layout::LayoutNode* sc = nullptr;
            if (node.is_scroll_container) {
                sc = &node;
            } else {
                const clever::layout::LayoutNode* ancestor = node.parent;
                while (ancestor != nullptr) {
                    if (ancestor->is_scroll_container) {
                        sc = ancestor;
                        break;
                    }
                    ancestor = ancestor->parent;
                }
            }

            if (sc != nullptr) {
                // Find the scroll container's content origin and scroll offsets in paint
                // coordinates. These values are reused for stick calculations below.
                float cur_content_y = child_offset_y;
                float cur_content_x = child_offset_x;
                const clever::layout::LayoutNode* cur = &node;
                while (cur != sc && cur != nullptr) {
                    float parent_content_y = cur_content_y
                        - cur->geometry.y
                        - cur->geometry.border.top
                        - cur->geometry.padding.top;
                    float parent_content_x = cur_content_x
                        - cur->geometry.x
                        - cur->geometry.border.left
                        - cur->geometry.padding.left;
                    cur_content_y = parent_content_y;
                    cur_content_x = parent_content_x;
                    cur = cur->parent;
                }
                sc_content_x = cur_content_x;
                sc_content_y = cur_content_y;
                sc_scroll_x = sc->scroll_left;
                sc_scroll_y = sc->scroll_top;
                sc_w = sc->geometry.width;
                sc_h = sc->geometry.height;

                if (sc_w <= 0.0f) sc_w = 1e9f;
                if (sc_h <= 0.0f) sc_h = 1e9f;
            }

            // Convert sticky normal-flow position to scroll-container content coordinates.
            const float normal_in_sc_x = child_abs_x - sc_content_x;
            const float normal_in_sc_y = child_abs_y - sc_content_y;
            const float container_top = sc_scroll_y;
            const float container_left = sc_scroll_x;
            const float container_bottom = sc_scroll_y + sc_h;
            const float container_right = sc_scroll_x + sc_w;
            const float container_limit_x = container_right - child_box_w;
            const float container_limit_y = container_bottom - child_box_h;

            float stuck_x = normal_in_sc_x;
            float stuck_y = normal_in_sc_y;

            // Keep element within horizontal container bounds.
            if (child_box_w >= sc_w) {
                stuck_x = container_left;
            } else {
                if (child->pos_left_set) {
                    const float sticky_left = sc_scroll_x + child->pos_left;
                    if (stuck_x < sticky_left) {
                        stuck_x = sticky_left;
                    }
                } else if (child->pos_right_set) {
                    const float sticky_right = sc_scroll_x + sc_w - child_box_w - child->pos_right;
                    if (stuck_x > sticky_right) {
                        stuck_x = sticky_right;
                    }
                }

                if (stuck_x < normal_in_sc_x) {
                    stuck_x = normal_in_sc_x;
                }
                if (stuck_x > container_limit_x) {
                    stuck_x = container_limit_x;
                }
            }

            // Keep element within vertical container bounds.
            if (child_box_h >= sc_h) {
                stuck_y = container_top;
            } else {
                if (child->pos_top_set) {
                    const float sticky_top = sc_scroll_y + child->pos_top;
                    if (stuck_y < sticky_top) {
                        stuck_y = sticky_top;
                    }
                } else if (child->pos_bottom_set) {
                    const float sticky_bottom = sc_scroll_y + sc_h - child_box_h - child->pos_bottom;
                    if (stuck_y > sticky_bottom) {
                        stuck_y = sticky_bottom;
                    }
                }
                if (stuck_y < normal_in_sc_y) {
                    stuck_y = normal_in_sc_y;
                }
                if (stuck_y > container_limit_y) {
                    stuck_y = container_limit_y;
                }
            }

            sticky_offset_x = stuck_x - normal_in_sc_x;
            sticky_offset_y = stuck_y - normal_in_sc_y;

            // Cache sticky constraint bounds for this paint pass.
            sticky_child->sticky_container_top = container_top;
            sticky_child->sticky_container_bottom = container_bottom;
            sticky_child->sticky_container_left = container_left;
            sticky_child->sticky_container_right = container_right;
            sticky_child->sticky_container_width = sc_w;
            sticky_child->sticky_container_height = sc_h;
            sticky_child->sticky_max_top = sc_scroll_y + (child->pos_top_set ? child->pos_top : 0.0f);
            sticky_child->sticky_max_bottom = sc_scroll_y + sc_h - child_box_h - (child->pos_bottom_set ? child->pos_bottom : 0.0f);

            paint_node(*child, list, child_offset_x + sticky_offset_x, child_offset_y + sticky_offset_y);
        } else {
            paint_node(*child, list, child_offset_x, child_offset_y);
        }
    };

    // Paint in CSS stacking order:
    // 1. Child stacking contexts with negative z-index (sorted by z-index)
    // 2. Normal flow children (DOM order) — non-positioned, non-stacking-context
    // 3. Child stacking contexts with z-index >= 0 (sorted by z-index, DOM order for ties)
    // content-visibility: hidden — paint the element's own box (background/border)
    // but skip painting all child content
    if (node.content_visibility != 1) {
        // Phase 1: negative z-index stacking contexts
        for (auto* child : stacking_negative) {
            paint_child(child);
        }
        // Phase 2: normal flow children in DOM order
        for (auto* child : normal_flow) {
            paint_child(child);
        }
        // Phase 3: non-negative z-index stacking contexts (z-index 0 and positive)
        for (auto* child : stacking_non_negative) {
            paint_child(child);
        }
    }

    // Apply clip-path masking after element + children are painted
    if (node.clip_path_type > 0) {
        Rect clip_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        list.apply_clip_path(clip_bounds, node.clip_path_type, node.clip_path_values);
    }

    // Apply mask-image: linear-gradient() — modulates alpha of the region
    if (!node.mask_image.empty()) {
        float elem_w = geom.border_box_width();
        float elem_h = geom.border_box_height();

        // ---- mask-origin: determine the positioning reference box ----
        // 0=border-box (abs_x/abs_y), 1=padding-box (default), 2=content-box
        float origin_x = abs_x;
        float origin_y = abs_y;
        float origin_w = elem_w;
        float origin_h = elem_h;
        if (node.mask_origin == 1) {
            // padding-box: shift inward by border
            origin_x += geom.border.left;
            origin_y += geom.border.top;
            origin_w = geom.padding.left + geom.width + geom.padding.right;
            origin_h = geom.padding.top + geom.height + geom.padding.bottom;
        } else if (node.mask_origin == 2) {
            // content-box: shift inward by border + padding
            origin_x += geom.border.left + geom.padding.left;
            origin_y += geom.border.top + geom.padding.top;
            origin_w = geom.width;
            origin_h = geom.height;
        }

        // ---- mask-clip: determine the visible region ----
        // 0=border-box, 1=padding-box, 2=content-box, 3=no-clip
        bool mask_clipping = (node.mask_clip != 3);
        Rect clip_rect = {abs_x, abs_y, elem_w, elem_h}; // border-box by default
        if (node.mask_clip == 1) {
            // padding-box
            clip_rect = {
                abs_x + geom.border.left,
                abs_y + geom.border.top,
                geom.padding.left + geom.width + geom.padding.right,
                geom.padding.top + geom.height + geom.padding.bottom
            };
        } else if (node.mask_clip == 2) {
            // content-box
            clip_rect = {
                abs_x + geom.border.left + geom.padding.left,
                abs_y + geom.border.top + geom.padding.top,
                geom.width,
                geom.height
            };
        }
        if (mask_clipping) {
            list.push_clip(clip_rect);
        }

        // ---- mask-size: compute tile dimensions ----
        float tile_w = origin_w;
        float tile_h = origin_h;
        if (node.mask_size == 3 && node.mask_size_width > 0 && node.mask_size_height > 0) {
            tile_w = node.mask_size_width;
            tile_h = node.mask_size_height;
        } else if (node.mask_size == 1) {
            // cover: scale uniformly to cover origin box (gradient treated as square)
            float s = std::max(origin_w, origin_h);
            tile_w = s;
            tile_h = s;
        } else if (node.mask_size == 2) {
            // contain: scale uniformly to fit within origin box
            float s = std::min(origin_w, origin_h);
            tile_w = s;
            tile_h = s;
        }

        // ---- Parse "linear-gradient(...)" into angle + stops ----
        auto mask_str = node.mask_image;
        auto lg_pos = mask_str.find("linear-gradient(");
        if (lg_pos != std::string::npos) {
            auto paren_start = mask_str.find('(', lg_pos);
            auto paren_end = mask_str.rfind(')');
            if (paren_start != std::string::npos && paren_end != std::string::npos && paren_end > paren_start) {
                std::string args = mask_str.substr(paren_start + 1, paren_end - paren_start - 1);
                float angle = 180.0f; // default: to bottom
                std::vector<std::pair<uint32_t, float>> stops;

                // Split by comma
                std::vector<std::string> parts;
                size_t start = 0;
                int paren_depth = 0;
                for (size_t i = 0; i < args.size(); i++) {
                    if (args[i] == '(') paren_depth++;
                    else if (args[i] == ')') paren_depth--;
                    else if (args[i] == ',' && paren_depth == 0) {
                        parts.push_back(args.substr(start, i - start));
                        start = i + 1;
                    }
                }
                parts.push_back(args.substr(start));

                // Trim parts
                size_t stop_start = 0;
                for (auto& p : parts) {
                    while (!p.empty() && p.front() == ' ') p.erase(p.begin());
                    while (!p.empty() && p.back() == ' ') p.pop_back();
                }

                // Check if first part is direction
                if (!parts.empty()) {
                    auto& first = parts[0];
                    if (first == "to top") { angle = 0; stop_start = 1; }
                    else if (first == "to right") { angle = 90; stop_start = 1; }
                    else if (first == "to bottom") { angle = 180; stop_start = 1; }
                    else if (first == "to left") { angle = 270; stop_start = 1; }
                    else if (first.find("deg") != std::string::npos) {
                        try { angle = std::stof(first); } catch (...) {}
                        stop_start = 1;
                    }
                }

                // Parse color stops
                for (size_t i = stop_start; i < parts.size(); i++) {
                    auto& p = parts[i];
                    // Split into color + optional position
                    // Find last space that separates color from position
                    auto last_space = p.rfind(' ');
                    std::string color_str = p;
                    float position = -1;
                    if (last_space != std::string::npos) {
                        std::string maybe_pos = p.substr(last_space + 1);
                        if (maybe_pos.back() == '%') {
                            try {
                                position = std::stof(maybe_pos) / 100.0f;
                                color_str = p.substr(0, last_space);
                            } catch (...) {}
                        }
                    }
                    // Parse color — handle "transparent" as 0x00000000
                    uint32_t argb = 0xFF000000;
                    if (color_str == "transparent") {
                        argb = 0x00000000;
                    } else if (color_str == "black") {
                        argb = 0xFF000000;
                    } else if (color_str == "white") {
                        argb = 0xFFFFFFFF;
                    } else {
                        auto c = clever::css::parse_color(color_str);
                        if (c) {
                            argb = (static_cast<uint32_t>(c->a) << 24) |
                                   (static_cast<uint32_t>(c->r) << 16) |
                                   (static_cast<uint32_t>(c->g) << 8) |
                                   static_cast<uint32_t>(c->b);
                        }
                    }
                    stops.push_back({argb, position});
                }

                // Auto-distribute positions
                if (!stops.empty()) {
                    if (stops.front().second < 0) stops.front().second = 0.0f;
                    if (stops.back().second < 0) stops.back().second = 1.0f;
                    for (size_t i = 1; i + 1 < stops.size(); i++) {
                        if (stops[i].second < 0) {
                            // Find next defined stop
                            size_t next = i + 1;
                            while (next < stops.size() && stops[next].second < 0) next++;
                            float prev_pos = stops[i - 1].second;
                            float next_pos = stops[next].second;
                            size_t span = next - i + 1;
                            for (size_t j = i; j < next; j++) {
                                stops[j].second = prev_pos + (next_pos - prev_pos) * (j - i + 1) / span;
                            }
                        }
                    }
                }

                if (stops.size() >= 2) {
                    // ---- mask-repeat: tile gradient tiles across the origin box ----
                    // 0=repeat, 1=repeat-x, 2=repeat-y, 3=no-repeat
                    // For gradients tiling produces multiple apply_mask_gradient calls
                    // covering adjacent tile positions; the clip rect above limits visibility.
                    bool tile_x = (node.mask_repeat == 0 || node.mask_repeat == 1);
                    bool tile_y = (node.mask_repeat == 0 || node.mask_repeat == 2);

                    if (!tile_x && !tile_y) {
                        // no-repeat: paint a single tile at origin position
                        Rect mask_bounds = {origin_x, origin_y, tile_w, tile_h};
                        list.apply_mask_gradient(mask_bounds, angle, stops);
                    } else {
                        // Tile across the clip region (union of origin box and clip rect)
                        float region_x = clip_rect.x;
                        float region_y = clip_rect.y;
                        float region_w = clip_rect.width;
                        float region_h = clip_rect.height;

                        // Guard against zero tile size to prevent infinite loop
                        float step_x = (tile_w > 0) ? tile_w : origin_w;
                        float step_y = (tile_h > 0) ? tile_h : origin_h;
                        if (step_x <= 0) step_x = 1;
                        if (step_y <= 0) step_y = 1;

                        // Starting tile position aligned to origin_x/origin_y
                        float start_x = origin_x;
                        float start_y = origin_y;
                        if (tile_x) {
                            // Walk backwards to cover region left edge
                            while (start_x > region_x) start_x -= step_x;
                        }
                        if (tile_y) {
                            while (start_y > region_y) start_y -= step_y;
                        }

                        float end_x = tile_x ? (region_x + region_w) : (origin_x + step_x);
                        float end_y = tile_y ? (region_y + region_h) : (origin_y + step_y);

                        for (float ty = start_y; ty < end_y; ty += step_y) {
                            for (float tx = start_x; tx < end_x; tx += step_x) {
                                Rect mask_bounds = {tx, ty, tile_w, tile_h};
                                list.apply_mask_gradient(mask_bounds, angle, stops);
                            }
                            if (!tile_x) break;
                        }
                    }
                }
            }
        }

        if (mask_clipping) {
            list.pop_clip();
        }
    }

    // Apply CSS filters after element + children are painted
    if (!node.filters.empty()) {
        Rect filter_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        for (auto& [ftype, fval] : node.filters) {
            if (ftype == 10) {
                // drop-shadow: pass extra params
                list.apply_drop_shadow(filter_bounds, fval,
                                       node.drop_shadow_ox, node.drop_shadow_oy,
                                       node.drop_shadow_color);
            } else {
                list.apply_filter(filter_bounds, ftype, fval);
            }
        }
    }

    // Apply mix-blend-mode: blend the element's pixels with the saved backdrop
    if (has_blend_mode) {
        Rect blend_bounds = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        list.apply_blend_mode(blend_bounds, node.mix_blend_mode);
    }

    // Paint overflow scroll indicators after children (on top of content)
    if (node.overflow >= 2 && (node.overflow_indicator_bottom || node.overflow_indicator_right)) {
        paint_overflow_indicator(node, list, abs_x, abs_y);
    }

    // Paint scrollbars for scroll containers (overlaid on top of content)
    if (node.is_scroll_container && node.overflow >= 2) {
        paint_scrollbar(node, list, abs_x, abs_y);
    }

    if (clipping) {
        list.pop_clip();
    }

    // Pop all transforms in reverse order
    for (int i = 0; i < transform_count; i++) {
        list.pop_transform();
    }
}

void Painter::paint_background(const clever::layout::LayoutNode& node, DisplayList& list,
                                float abs_x, float abs_y) {
    const auto& geom = node.geometry;
    float w = geom.border_box_width();
    float h = geom.border_box_height();
    Rect rect = {abs_x, abs_y, w > 0 ? w : geom.width, h > 0 ? h : geom.height};

    // Determine the origin box for background-position calculations
    // background-origin: 0=padding-box (default), 1=border-box, 2=content-box
    float origin_x = abs_x;
    float origin_y = abs_y;
    float origin_w = rect.width;
    float origin_h = rect.height;

    if (node.background_origin == 1) {
        // border-box: origin at the border edge (no adjustment needed)
        // origin is already at abs_x, abs_y
    } else if (node.background_origin == 2) {
        // content-box: origin inside border and padding
        origin_x += geom.border.left + geom.padding.left;
        origin_y += geom.border.top + geom.padding.top;
        origin_w -= geom.border.left + geom.border.right + geom.padding.left + geom.padding.right;
        origin_h -= geom.border.top + geom.border.bottom + geom.padding.top + geom.padding.bottom;
    } else {
        // padding-box (default, 0): origin inside border but at padding edge
        origin_x += geom.border.left;
        origin_y += geom.border.top;
        origin_w -= geom.border.left + geom.border.right;
        origin_h -= geom.border.top + geom.border.bottom;
    }

    // Apply background-clip: 0=border-box (default), 1=padding-box, 2=content-box, 3=text
    if (node.background_clip == 1) {
        // padding-box: inset by border widths
        rect.x += geom.border.left;
        rect.y += geom.border.top;
        rect.width -= geom.border.left + geom.border.right;
        rect.height -= geom.border.top + geom.border.bottom;
    } else if (node.background_clip == 2) {
        // content-box: inset by border + padding
        rect.x += geom.border.left + geom.padding.left;
        rect.y += geom.border.top + geom.padding.top;
        rect.width -= geom.border.left + geom.border.right + geom.padding.left + geom.padding.right;
        rect.height -= geom.border.top + geom.border.bottom + geom.padding.top + geom.padding.bottom;
    } else if (node.background_clip == 3) {
        // text: background visible only where text is drawn
        // TODO(background-clip:text): Requires clipping to glyph bounds during text rendering phase.
        // Not yet implemented — treating as border-box for now.
    }
    if (rect.width <= 0 || rect.height <= 0) return;

    // Gradient background — with optional background-blend-mode
    if (!node.gradient_stops.empty()) {
        if (node.background_blend_mode != 0 && node.background_color != 0) {
            // Paint background color first, then blend gradient on top
            uint32_t bgc = node.background_color;
            Color bg_color = {
                static_cast<uint8_t>((bgc >> 16) & 0xFF),
                static_cast<uint8_t>((bgc >> 8) & 0xFF),
                static_cast<uint8_t>(bgc & 0xFF),
                static_cast<uint8_t>((bgc >> 24) & 0xFF)
            };
            {
                bool bg_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                                   node.border_radius_bl > 0 || node.border_radius_br > 0);
                if (bg_has_per) {
                    list.fill_rounded_rect(rect, bg_color,
                                           node.border_radius_tl, node.border_radius_tr,
                                           node.border_radius_bl, node.border_radius_br);
                } else if (node.border_radius > 0) {
                    list.fill_rounded_rect(rect, bg_color, node.border_radius);
                } else {
                    list.fill_rect(rect, bg_color);
                }
            }
            list.save_backdrop(rect);
            {
                bool grad_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                                     node.border_radius_bl > 0 || node.border_radius_br > 0);
                if (grad_has_per) {
                    list.fill_gradient(rect, node.gradient_angle, node.gradient_stops,
                                       node.border_radius_tl, node.border_radius_tr,
                                       node.border_radius_bl, node.border_radius_br,
                                       node.gradient_type, node.radial_shape);
                } else {
                    list.fill_gradient(rect, node.gradient_angle, node.gradient_stops,
                                       node.border_radius, node.gradient_type, node.radial_shape);
                }
            }
            list.apply_blend_mode(rect, node.background_blend_mode);
        } else {
            bool grad_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                                 node.border_radius_bl > 0 || node.border_radius_br > 0);
            if (grad_has_per) {
                list.fill_gradient(rect, node.gradient_angle, node.gradient_stops,
                                   node.border_radius_tl, node.border_radius_tr,
                                   node.border_radius_bl, node.border_radius_br,
                                   node.gradient_type, node.radial_shape);
            } else {
                list.fill_gradient(rect, node.gradient_angle, node.gradient_stops,
                                   node.border_radius, node.gradient_type, node.radial_shape);
            }
        }
        return;
    }

    // Background image (CSS background-image: url(...))
    if (node.bg_image_pixels && !node.bg_image_pixels->empty()) {
        auto img = std::make_shared<ImageData>();
        img->pixels = *node.bg_image_pixels;
        img->width = node.bg_image_width;
        img->height = node.bg_image_height;

        float img_w = static_cast<float>(node.bg_image_width);
        float img_h = static_cast<float>(node.bg_image_height);
        float origin_elem_w = origin_w;
        float origin_elem_h = origin_h;

        // Determine drawn image size based on background-size
        float draw_w = img_w;
        float draw_h = img_h;

        if (node.background_size == 1 && img_w > 0 && img_h > 0) {
            // cover: scale to fill origin box, maintaining aspect ratio (may clip)
            float scale = std::max(origin_elem_w / img_w, origin_elem_h / img_h);
            draw_w = img_w * scale;
            draw_h = img_h * scale;
        } else if (node.background_size == 2 && img_w > 0 && img_h > 0) {
            // contain: scale to fit entirely within origin box (may leave gaps)
            float scale = std::min(origin_elem_w / img_w, origin_elem_h / img_h);
            draw_w = img_w * scale;
            draw_h = img_h * scale;
        } else if (node.background_size == 3) {
            // explicit size — resolve width and height independently
            if (node.bg_size_width_pct) {
                draw_w = origin_elem_w * node.bg_size_width / 100.0f;
            } else if (node.bg_size_width > 0) {
                draw_w = node.bg_size_width;
            } else {
                draw_w = img_w; // auto
            }
            if (node.bg_size_height_auto) {
                draw_h = (img_w > 0) ? draw_w * img_h / img_w : img_h;
            } else if (node.bg_size_height_pct) {
                draw_h = origin_elem_h * node.bg_size_height / 100.0f;
            } else if (node.bg_size_height > 0) {
                draw_h = node.bg_size_height;
            } else {
                draw_h = img_h; // auto
            }
        }
        // else background_size == 0 (auto): use natural size

        // Resolve background-position relative to the origin box.
        // CSS spec: a percentage P positions the image so that the point at P% within
        // the image aligns with the point at P% within the container:
        //   offset = (container_size - image_size) * P / 100
        float pos_x = 0, pos_y = 0;
        if (node.bg_attachment == 1) {
            // fixed: position relative to viewport, not element
            float vw = clever::css::Length::s_viewport_w;
            float vh = (viewport_height_ > 0) ? viewport_height_ : clever::css::Length::s_viewport_h;
            if (node.bg_position_x_pct)
                pos_x = (vw - draw_w) * node.bg_position_x / 100.0f - origin_x;
            else
                pos_x = node.bg_position_x - origin_x;
            if (node.bg_position_y_pct)
                pos_y = (vh - draw_h) * node.bg_position_y / 100.0f - origin_y;
            else
                pos_y = node.bg_position_y - origin_y;
        } else {
            // scroll (0) or local (2): position relative to origin box
            if (node.bg_position_x_pct)
                pos_x = (origin_elem_w - draw_w) * node.bg_position_x / 100.0f;
            else
                pos_x = node.bg_position_x;
            if (node.bg_position_y_pct)
                pos_y = (origin_elem_h - draw_h) * node.bg_position_y / 100.0f;
            else
                pos_y = node.bg_position_y;
            // local (2): background scrolls with the element's own content.
            if (node.bg_attachment == 2 && node.is_scroll_container) {
                pos_x -= node.scroll_left;
                pos_y -= node.scroll_top;
            }
        }

        // Apply clip to ensure all background image modes respect the background-clip boundary.
        list.push_clip(rect);

        // Draw based on background-repeat, clipped to the clipping box (rect)
        if (node.background_repeat == 3) {
            // no-repeat: draw once at computed position
            list.draw_image({origin_x + pos_x, origin_y + pos_y, draw_w, draw_h}, img);
        } else if (node.background_repeat == 1) {
            // repeat-x: tile horizontally only
            if (draw_w > 0) {
                float start_x = origin_x + pos_x;
                if (start_x > rect.x)
                    start_x -= std::ceil((start_x - rect.x) / draw_w) * draw_w;
                else
                    start_x -= std::floor((rect.x - start_x) / draw_w) * draw_w;
                for (float tx = start_x; tx < rect.x + rect.width; tx += draw_w)
                    list.draw_image({tx, origin_y + pos_y, draw_w, draw_h}, img);
            }
        } else if (node.background_repeat == 2) {
            // repeat-y: tile vertically only
            if (draw_h > 0) {
                float start_y = origin_y + pos_y;
                if (start_y > rect.y)
                    start_y -= std::ceil((start_y - rect.y) / draw_h) * draw_h;
                else
                    start_y -= std::floor((rect.y - start_y) / draw_h) * draw_h;
                for (float ty = start_y; ty < rect.y + rect.height; ty += draw_h)
                    list.draw_image({origin_x + pos_x, ty, draw_w, draw_h}, img);
            }
        } else if (node.background_repeat == 4 && draw_w > 0 && draw_h > 0) {
            // space: distribute tiles evenly with equal spacing, no clipping
            int n_x = std::max(1, static_cast<int>(std::floor(origin_elem_w / draw_w)));
            int n_y = std::max(1, static_cast<int>(std::floor(origin_elem_h / draw_h)));
            float gap_x = (n_x > 1) ? (origin_elem_w - n_x * draw_w) / (n_x - 1) : 0.0f;
            float gap_y = (n_y > 1) ? (origin_elem_h - n_y * draw_h) / (n_y - 1) : 0.0f;
            float off_x = (n_x == 1) ? (origin_elem_w - draw_w) / 2.0f : 0.0f;
            float off_y = (n_y == 1) ? (origin_elem_h - draw_h) / 2.0f : 0.0f;
            for (int iy = 0; iy < n_y; ++iy) {
                float ty = origin_y + off_y + iy * (draw_h + gap_y);
                for (int ix = 0; ix < n_x; ++ix) {
                    float tx = origin_x + off_x + ix * (draw_w + gap_x);
                    list.draw_image({tx, ty, draw_w, draw_h}, img);
                }
            }
        } else if (node.background_repeat == 5 && draw_w > 0 && draw_h > 0) {
            // round: scale image so it tiles a whole number of times
            int n_x = std::max(1, static_cast<int>(std::round(origin_elem_w / draw_w)));
            int n_y = std::max(1, static_cast<int>(std::round(origin_elem_h / draw_h)));
            float tile_w = origin_elem_w / n_x;
            float tile_h = origin_elem_h / n_y;
            for (int iy = 0; iy < n_y; ++iy)
                for (int ix = 0; ix < n_x; ++ix)
                    list.draw_image({origin_x + ix * tile_w, origin_y + iy * tile_h, tile_w, tile_h}, img);
        } else {
            // repeat (default, 0): tile in both directions within clipping box
            if (draw_w > 0 && draw_h > 0) {
                float start_x = origin_x + pos_x;
                float start_y = origin_y + pos_y;
                if (start_x > rect.x)
                    start_x -= std::ceil((start_x - rect.x) / draw_w) * draw_w;
                else
                    start_x -= std::floor((rect.x - start_x) / draw_w) * draw_w;
                if (start_y > rect.y)
                    start_y -= std::ceil((start_y - rect.y) / draw_h) * draw_h;
                else
                    start_y -= std::floor((rect.y - start_y) / draw_h) * draw_h;
                for (float ty = start_y; ty < rect.y + rect.height; ty += draw_h) {
                    for (float tx = start_x; tx < rect.x + rect.width; tx += draw_w) {
                        list.draw_image({tx, ty, draw_w, draw_h}, img);
                    }
                }
            }
        }

        list.pop_clip();
        return;
    }

    // Extract ARGB color
    uint32_t bg = node.background_color;
    uint8_t a = static_cast<uint8_t>((bg >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((bg >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((bg >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(bg & 0xFF);

    // Apply opacity
    if (node.opacity < 1.0f) {
        a = static_cast<uint8_t>(a * node.opacity);
    }

    // Skip transparent backgrounds
    if (a == 0) return;

    // Use per-corner radius if any set, otherwise generic
    bool bg_has_per = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                       node.border_radius_bl > 0 || node.border_radius_br > 0);
    if (bg_has_per) {
        list.fill_rounded_rect(rect, {r, g, b, a},
                               node.border_radius_tl, node.border_radius_tr,
                               node.border_radius_bl, node.border_radius_br);
    } else if (node.border_radius > 0) {
        list.fill_rounded_rect(rect, {r, g, b, a}, node.border_radius);
    } else {
        list.fill_rect(rect, {r, g, b, a});
    }
}

void Painter::paint_borders(const clever::layout::LayoutNode& node, DisplayList& list,
                             float abs_x, float abs_y) {
    auto geom = node.geometry;

    // CSS border-collapse: collapse — merge shared borders between adjacent cells
    // Per CSS 2.1 §17.6.2: the wider border wins; if equal width, style precedence applies
    if (node.border_collapse &&
        (node.tag_name == "td" || node.tag_name == "th" ||
         node.tag_name == "TD" || node.tag_name == "TH")) {

        clever::layout::LayoutNode* right_neighbor = nullptr;
        clever::layout::LayoutNode* bottom_neighbor = nullptr;
        int cell_index = -1;

        if (node.parent) {
            auto& siblings = node.parent->children;
            for (size_t i = 0; i < siblings.size(); i++) {
                if (siblings[i].get() == &node) {
                    cell_index = static_cast<int>(i);
                    if (i + 1 < siblings.size()) {
                        right_neighbor = siblings[i + 1].get();
                    }
                    break;
                }
            }

            if (node.parent->parent && right_neighbor) {
                auto& rows = node.parent->parent->children;
                for (size_t i = 0; i < rows.size(); i++) {
                    if (rows[i].get() == node.parent && i + 1 < rows.size()) {
                        clever::layout::LayoutNode* next_row = rows[i + 1].get();
                        int cell_count = 0;
                        for (auto& cell : next_row->children) {
                            if (cell_count == cell_index) {
                                bottom_neighbor = cell.get();
                                break;
                            }
                            if (cell->display != clever::layout::DisplayType::None && cell->mode != clever::layout::LayoutMode::None) {
                                cell_count++;
                            }
                        }
                        break;
                    }
                }
            }
        }

        // Merge right border with right neighbor's left border
        if (right_neighbor) {
            float this_right_width = geom.border.right;
            float neighbor_left_width = right_neighbor->geometry.border.left;

            // Winner is the wider border; if equal, use style precedence
            bool this_wins = this_right_width > neighbor_left_width;
            if (this_right_width == neighbor_left_width) {
                int this_style = node.border_style_right;
                int neighbor_style = right_neighbor->border_style_left;
                // Style precedence: hidden=1, none=0, double=6, solid=2, dashed=3, dotted=4
                int precedence_this = (this_style == 1) ? 10 : (this_style == 6) ? 8 : (this_style == 2) ? 5 : (this_style == 3) ? 4 : (this_style == 4) ? 3 : 0;
                int precedence_neighbor = (neighbor_style == 1) ? 10 : (neighbor_style == 6) ? 8 : (neighbor_style == 2) ? 5 : (neighbor_style == 3) ? 4 : (neighbor_style == 4) ? 3 : 0;
                this_wins = precedence_this >= precedence_neighbor;
            }

            // Only this cell draws the merged right border; neighbor suppresses its left
            if (!this_wins) {
                geom.border.right = neighbor_left_width;
            }
        }

        // Merge bottom border with bottom neighbor's top border
        if (bottom_neighbor) {
            float this_bottom_width = geom.border.bottom;
            float neighbor_top_width = bottom_neighbor->geometry.border.top;

            bool this_wins = this_bottom_width > neighbor_top_width;
            if (this_bottom_width == neighbor_top_width) {
                int this_style = node.border_style_bottom;
                int neighbor_style = bottom_neighbor->border_style_top;
                int precedence_this = (this_style == 1) ? 10 : (this_style == 6) ? 8 : (this_style == 2) ? 5 : (this_style == 3) ? 4 : (this_style == 4) ? 3 : 0;
                int precedence_neighbor = (neighbor_style == 1) ? 10 : (neighbor_style == 6) ? 8 : (neighbor_style == 2) ? 5 : (neighbor_style == 3) ? 4 : (neighbor_style == 4) ? 3 : 0;
                this_wins = precedence_this >= precedence_neighbor;
            }

            if (!this_wins) {
                geom.border.bottom = neighbor_top_width;
            }
        }
    }

    // Only paint if there are actual borders
    if (geom.border.top <= 0 && geom.border.right <= 0 &&
        geom.border.bottom <= 0 && geom.border.left <= 0) {
        return;
    }

    // CSS border-image: replaces normal borders with gradient or image
    if (node.border_image_gradient_type > 0 && !node.border_image_gradient_stops.empty()) {
        Rect border_box = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        float bt = geom.border.top, br_w = geom.border.right;
        float bb = geom.border.bottom, bl_w = geom.border.left;

        const auto& stops = node.border_image_gradient_stops;
        int grad_type = node.border_image_gradient_type; // 1=linear, 2=radial
        float angle = node.border_image_gradient_angle;
        int radial_shape = node.border_image_radial_shape;

        // Draw gradient in border regions (top, right, bottom, left bands)
        if (bt > 0) list.fill_gradient({abs_x, abs_y, border_box.width, bt}, angle, stops, 0, grad_type, radial_shape);
        if (bb > 0) list.fill_gradient({abs_x, abs_y + border_box.height - bb, border_box.width, bb}, angle, stops, 0, grad_type, radial_shape);
        if (bl_w > 0) list.fill_gradient({abs_x, abs_y + bt, bl_w, border_box.height - bt - bb}, angle, stops, 0, grad_type, radial_shape);
        if (br_w > 0) list.fill_gradient({abs_x + border_box.width - br_w, abs_y + bt, br_w, border_box.height - bt - bb}, angle, stops, 0, grad_type, radial_shape);
        return; // border-image replaces normal borders
    }

    // CSS border-image with image pixels: 9-part slicing
    if (node.border_image_pixels && !node.border_image_pixels->empty()) {
        Rect border_box = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};
        float bt = geom.border.top, br_w = geom.border.right;
        float bb = geom.border.bottom, bl_w = geom.border.left;

        int img_w = node.border_image_img_width;
        int img_h = node.border_image_img_height;
        float slice_pct = node.border_image_slice / 100.0f;
        float slice_px_w = slice_pct * img_w;
        float slice_px_h = slice_pct * img_h;
        int repeat_mode = node.border_image_repeat; // 0=stretch, 1=repeat, 2=round, 3=space

        if (img_w <= 0 || img_h <= 0) {
            // Invalid image dimensions, fall through to normal borders
        } else {
            // Helper lambda to extract a rectangular region from source pixels
            auto extract_region = [&](int src_x, int src_y, int src_w, int src_h) -> std::shared_ptr<ImageData> {
                auto result = std::make_shared<ImageData>();
                result->width = src_w;
                result->height = src_h;
                result->pixels.resize(src_w * src_h * 4, 0);

                const auto& src = *node.border_image_pixels;
                for (int dy = 0; dy < src_h; dy++) {
                    for (int dx = 0; dx < src_w; dx++) {
                        int src_idx = (src_y + dy) * img_w * 4 + (src_x + dx) * 4;
                        int dst_idx = dy * src_w * 4 + dx * 4;
                        if (src_idx + 3 < static_cast<int>(src.size())) {
                            result->pixels[dst_idx] = src[src_idx];
                            result->pixels[dst_idx + 1] = src[src_idx + 1];
                            result->pixels[dst_idx + 2] = src[src_idx + 2];
                            result->pixels[dst_idx + 3] = src[src_idx + 3];
                        }
                    }
                }
                return result;
            };

            // Clamp slice values to image bounds
            int slice_x = static_cast<int>(std::max(0.0f, std::min(slice_px_w, static_cast<float>(img_w))));
            int slice_y = static_cast<int>(std::max(0.0f, std::min(slice_px_h, static_cast<float>(img_h))));

            // 4 corners (always stretch)
            if (bt > 0 && bl_w > 0) {
                auto img = extract_region(0, 0, slice_x, slice_y);
                if (img->width > 0 && img->height > 0) {
                    list.draw_image({abs_x, abs_y, bl_w, bt}, img);
                }
            }
            if (bt > 0 && br_w > 0 && slice_x < img_w) {
                auto img = extract_region(img_w - slice_x, 0, slice_x, slice_y);
                if (img->width > 0 && img->height > 0) {
                    list.draw_image({abs_x + border_box.width - br_w, abs_y, br_w, bt}, img);
                }
            }
            if (bb > 0 && bl_w > 0 && slice_y < img_h) {
                auto img = extract_region(0, img_h - slice_y, slice_x, slice_y);
                if (img->width > 0 && img->height > 0) {
                    list.draw_image({abs_x, abs_y + border_box.height - bb, bl_w, bb}, img);
                }
            }
            if (bb > 0 && br_w > 0 && slice_x < img_w && slice_y < img_h) {
                auto img = extract_region(img_w - slice_x, img_h - slice_y, slice_x, slice_y);
                if (img->width > 0 && img->height > 0) {
                    list.draw_image({abs_x + border_box.width - br_w, abs_y + border_box.height - bb, br_w, bb}, img);
                }
            }

            // 4 edges (use repeat mode)
            // Top edge
            if (bt > 0 && slice_x < img_w) {
                auto img = extract_region(slice_x, 0, img_w - 2 * slice_x, slice_y);
                if (img->width > 0 && img->height > 0 && border_box.width - bl_w - br_w > 0) {
                    float edge_w = border_box.width - bl_w - br_w;
                    if (repeat_mode == 0) { // stretch
                        list.draw_image({abs_x + bl_w, abs_y, edge_w, bt}, img);
                    } else if (repeat_mode == 1) { // repeat
                        float x = abs_x + bl_w;
                        while (x < abs_x + bl_w + edge_w) {
                            float w = std::min(img->width * bt / img->height, abs_x + bl_w + edge_w - x);
                            list.draw_image({x, abs_y, w, bt}, img);
                            x += w;
                        }
                    } else {
                        list.draw_image({abs_x + bl_w, abs_y, edge_w, bt}, img);
                    }
                }
            }
            // Bottom edge
            if (bb > 0 && slice_x < img_w && slice_y < img_h) {
                auto img = extract_region(slice_x, img_h - slice_y, img_w - 2 * slice_x, slice_y);
                if (img->width > 0 && img->height > 0 && border_box.width - bl_w - br_w > 0) {
                    float edge_w = border_box.width - bl_w - br_w;
                    if (repeat_mode == 0) { // stretch
                        list.draw_image({abs_x + bl_w, abs_y + border_box.height - bb, edge_w, bb}, img);
                    } else if (repeat_mode == 1) { // repeat
                        float x = abs_x + bl_w;
                        while (x < abs_x + bl_w + edge_w) {
                            float w = std::min(img->width * bb / img->height, abs_x + bl_w + edge_w - x);
                            list.draw_image({x, abs_y + border_box.height - bb, w, bb}, img);
                            x += w;
                        }
                    } else {
                        list.draw_image({abs_x + bl_w, abs_y + border_box.height - bb, edge_w, bb}, img);
                    }
                }
            }
            // Left edge
            if (bl_w > 0 && slice_y < img_h) {
                auto img = extract_region(0, slice_y, slice_x, img_h - 2 * slice_y);
                if (img->width > 0 && img->height > 0 && border_box.height - bt - bb > 0) {
                    float edge_h = border_box.height - bt - bb;
                    if (repeat_mode == 0) { // stretch
                        list.draw_image({abs_x, abs_y + bt, bl_w, edge_h}, img);
                    } else if (repeat_mode == 1) { // repeat
                        float y = abs_y + bt;
                        while (y < abs_y + bt + edge_h) {
                            float h = std::min(img->height * bl_w / img->width, abs_y + bt + edge_h - y);
                            list.draw_image({abs_x, y, bl_w, h}, img);
                            y += h;
                        }
                    } else {
                        list.draw_image({abs_x, abs_y + bt, bl_w, edge_h}, img);
                    }
                }
            }
            // Right edge
            if (br_w > 0 && slice_x < img_w && slice_y < img_h) {
                auto img = extract_region(img_w - slice_x, slice_y, slice_x, img_h - 2 * slice_y);
                if (img->width > 0 && img->height > 0 && border_box.height - bt - bb > 0) {
                    float edge_h = border_box.height - bt - bb;
                    if (repeat_mode == 0) { // stretch
                        list.draw_image({abs_x + border_box.width - br_w, abs_y + bt, br_w, edge_h}, img);
                    } else if (repeat_mode == 1) { // repeat
                        float y = abs_y + bt;
                        while (y < abs_y + bt + edge_h) {
                            float h = std::min(img->height * br_w / img->width, abs_y + bt + edge_h - y);
                            list.draw_image({abs_x + border_box.width - br_w, y, br_w, h}, img);
                            y += h;
                        }
                    } else {
                        list.draw_image({abs_x + border_box.width - br_w, abs_y + bt, br_w, edge_h}, img);
                    }
                }
            }

            // Center fill (only if fill keyword is present)
            if (node.border_image_slice_fill && border_box.width - bl_w - br_w > 0 && border_box.height - bt - bb > 0 && slice_x < img_w && slice_y < img_h) {
                auto img = extract_region(slice_x, slice_y, img_w - 2 * slice_x, img_h - 2 * slice_y);
                if (img->width > 0 && img->height > 0) {
                    list.draw_image({abs_x + bl_w, abs_y + bt, border_box.width - bl_w - br_w, border_box.height - bt - bb}, img);
                }
            }

            return; // border-image replaces normal borders
        }
    }

    auto extract_color = [&](uint32_t bc) -> Color {
        uint8_t a = static_cast<uint8_t>((bc >> 24) & 0xFF);
        uint8_t r = static_cast<uint8_t>((bc >> 16) & 0xFF);
        uint8_t g = static_cast<uint8_t>((bc >> 8) & 0xFF);
        uint8_t b = static_cast<uint8_t>(bc & 0xFF);
        if (node.opacity < 1.0f) a = static_cast<uint8_t>(a * node.opacity);
        return {r, g, b, a};
    };

    Rect border_box = {abs_x, abs_y, geom.border_box_width(), geom.border_box_height()};

    // Check if all sides have the same color and style (common case — draw as one command)
    bool same_color = (node.border_color_top == node.border_color_right &&
                       node.border_color_top == node.border_color_bottom &&
                       node.border_color_top == node.border_color_left);
    bool same_style = (node.border_style_top == node.border_style_right &&
                       node.border_style_top == node.border_style_bottom &&
                       node.border_style_top == node.border_style_left);

    // Use per-side colors if they differ from the generic border_color,
    // or fall back to the generic color
    uint32_t ct = node.border_color_top;
    uint32_t cr_c = node.border_color_right;
    uint32_t cb_c = node.border_color_bottom;
    uint32_t cl_c = node.border_color_left;
    // If per-side colors are all default (0xFF000000) but generic border_color is set,
    // use the generic color for all sides
    if (same_color && ct == 0xFF000000 && node.border_color != 0xFF000000) {
        ct = cr_c = cb_c = cl_c = node.border_color;
        same_color = true;
    }

    // Use per-corner border radius if any non-zero, else fall back to generic
    bool has_per_corner = (node.border_radius_tl > 0 || node.border_radius_tr > 0 ||
                           node.border_radius_bl > 0 || node.border_radius_br > 0);

    if (same_color && same_style) {
        // All sides same color and style — single draw_border command
        Color color = extract_color(ct);
        int style = node.border_style_top;
        if (style == 0) return;
        if (has_per_corner) {
            list.draw_border(border_box, color,
                             geom.border.top, geom.border.right,
                             geom.border.bottom, geom.border.left,
                             node.border_radius_tl, node.border_radius_tr,
                             node.border_radius_bl, node.border_radius_br,
                             style);
        } else {
            list.draw_border(border_box, color,
                             geom.border.top, geom.border.right,
                             geom.border.bottom, geom.border.left,
                             node.border_radius, style);
        }
    } else {
        // Per-side rendering: draw each side as a separate border with only that side's width
        // Top border
        if (geom.border.top > 0 && node.border_style_top != 0) {
            if (has_per_corner) {
                list.draw_border(border_box, extract_color(ct),
                                 geom.border.top, 0, 0, 0,
                                 node.border_radius_tl, node.border_radius_tr,
                                 node.border_radius_bl, node.border_radius_br,
                                 node.border_style_top);
            } else {
                list.draw_border(border_box, extract_color(ct),
                                 geom.border.top, 0, 0, 0,
                                 node.border_radius, node.border_style_top);
            }
        }
        // Right border
        if (geom.border.right > 0 && node.border_style_right != 0) {
            if (has_per_corner) {
                list.draw_border(border_box, extract_color(cr_c),
                                 0, geom.border.right, 0, 0,
                                 node.border_radius_tl, node.border_radius_tr,
                                 node.border_radius_bl, node.border_radius_br,
                                 node.border_style_right);
            } else {
                list.draw_border(border_box, extract_color(cr_c),
                                 0, geom.border.right, 0, 0,
                                 node.border_radius, node.border_style_right);
            }
        }
        // Bottom border
        if (geom.border.bottom > 0 && node.border_style_bottom != 0) {
            if (has_per_corner) {
                list.draw_border(border_box, extract_color(cb_c),
                                 0, 0, geom.border.bottom, 0,
                                 node.border_radius_tl, node.border_radius_tr,
                                 node.border_radius_bl, node.border_radius_br,
                                 node.border_style_bottom);
            } else {
                list.draw_border(border_box, extract_color(cb_c),
                                 0, 0, geom.border.bottom, 0,
                                 node.border_radius, node.border_style_bottom);
            }
        }
        // Left border
        if (geom.border.left > 0 && node.border_style_left != 0) {
            if (has_per_corner) {
                list.draw_border(border_box, extract_color(cl_c),
                                 0, 0, 0, geom.border.left,
                                 node.border_radius_tl, node.border_radius_tr,
                                 node.border_radius_bl, node.border_radius_br,
                                 node.border_style_left);
            } else {
                list.draw_border(border_box, extract_color(cl_c),
                                 0, 0, 0, geom.border.left,
                                 node.border_radius, node.border_style_left);
            }
        }
    }
}

void Painter::paint_text(const clever::layout::LayoutNode& node, DisplayList& list,
                          float abs_x, float abs_y) {
    if (!node.is_text || node.text_content.empty()) return;

    // Extract text color from ARGB
    uint32_t tc = node.color;
    uint8_t a = static_cast<uint8_t>((tc >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((tc >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((tc >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(tc & 0xFF);

    if (node.opacity < 1.0f) {
        a = static_cast<uint8_t>(a * node.opacity);
    }

    const auto& geom = node.geometry;
    std::string text_to_render = node.text_content;

    // CSS text-transform: uppercase/lowercase/capitalize
    // 0=none, 1=capitalize, 2=uppercase, 3=lowercase
    if (node.text_transform == 2) {
        // uppercase
        std::transform(text_to_render.begin(), text_to_render.end(),
                       text_to_render.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    } else if (node.text_transform == 3) {
        // lowercase
        std::transform(text_to_render.begin(), text_to_render.end(),
                       text_to_render.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    } else if (node.text_transform == 1) {
        // capitalize — uppercase first letter of each word
        bool capitalize_next = true;
        for (size_t i = 0; i < text_to_render.size(); i++) {
            if (std::isspace(static_cast<unsigned char>(text_to_render[i]))) {
                capitalize_next = true;
            } else if (capitalize_next) {
                text_to_render[i] = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(text_to_render[i])));
                capitalize_next = false;
            }
        }
    }

    // CSS font-synthesis: control synthetic bold/italic
    // Bitmask: 1=weight, 2=style, 4=small-caps; default 7 (all enabled)
    int eff_weight = node.font_weight;
    bool eff_italic = node.font_italic;
    if (node.font_synthesis != 7) { // not default "all enabled"
        if (!(node.font_synthesis & 1) && eff_weight > 400) {
            eff_weight = 400; // suppress synthetic bold
        }
        if (!(node.font_synthesis & 2)) {
            eff_italic = false; // suppress synthetic italic
        }
    }

    // font-variant / font-variant-caps: small-caps rendering
    // Transform lowercase to uppercase and use reduced font size.
    // font_variant == 1: legacy "small-caps"
    // font_variant_caps: 1=small-caps, 2=all-small-caps, 3=petite-caps,
    //                    4=all-petite-caps, 5=unicase, 6=titling-caps
    float effective_font_size = node.font_size;
    bool do_small_caps = (node.font_variant == 1 || node.font_variant_caps == 1 ||
                          node.font_variant_caps == 2 || node.font_variant_caps == 3 ||
                          node.font_variant_caps == 4);
    if (do_small_caps) {
        std::transform(text_to_render.begin(), text_to_render.end(),
                       text_to_render.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        // all-small-caps/all-petite-caps: ALL letters at reduced size
        // small-caps/petite-caps: only originally-lowercase letters smaller
        // (simplified: reduce all to 80% since we can't track original case)
        float scale = (node.font_variant_caps == 3 || node.font_variant_caps == 4) ? 0.75f : 0.8f;
        effective_font_size = node.font_size * scale;
    } else if (node.font_variant_caps == 5) {
        // unicase: uppercase stays as-is, lowercase becomes small-caps size
        effective_font_size = node.font_size * 0.85f;
    } else if (node.font_variant_caps == 6) {
        // titling-caps: slightly larger capitals, no size reduction
        effective_font_size = node.font_size;
    }

    // font-size-adjust: scale font size to match desired x-height ratio
    // Typical sans-serif x-height ratio ~0.56, serif ~0.52
    if (node.font_size_adjust > 0) {
        float assumed_ratio = 0.56f; // approximate default aspect ratio
        effective_font_size = effective_font_size * (node.font_size_adjust / assumed_ratio);
    }

    // Check if parent clips overflow + white-space:nowrap, then handle text-overflow mode.
    bool needs_fade = false;
    float fade_region_x = 0, fade_region_y = 0, fade_region_w = 0, fade_region_h = 0;
    const auto* overflow_parent = node.parent;
    const bool parent_text_overflow_ellipsis = overflow_parent && overflow_parent->text_overflow == 1;
    const bool node_text_overflow_ellipsis = node.text_overflow == 1;
    if (overflow_parent && (overflow_parent->overflow == 1 || overflow_parent->overflow == 2 || overflow_parent->overflow == 3) &&
        overflow_parent->white_space_nowrap &&
        (overflow_parent->overflow == 1 || overflow_parent->overflow_indicator_right || overflow_parent->overflow_indicator_bottom || node_text_overflow_ellipsis)) {
        static TextRenderer s_text_measurer;
        const std::string ellipsis_str = "\xE2\x80\xA6"; // U+2026 HORIZONTAL ELLIPSIS
        // Dual-mode support:
        // 1) legacy behavior: text-overflow set on the overflow container (parent)
        // 2) modern behavior: text-overflow set on the text node itself (child)
        const int text_overflow_mode = node_text_overflow_ellipsis ? node.text_overflow : overflow_parent->text_overflow;
        const bool use_parent_text_overflow = !node_text_overflow_ellipsis && overflow_parent->text_overflow != 0;
        const float container_width = std::max(0.0f,
            use_parent_text_overflow
                ? overflow_parent->geometry.width - overflow_parent->geometry.padding.left - overflow_parent->geometry.padding.right
                    - overflow_parent->geometry.border.left - overflow_parent->geometry.border.right
                : node.geometry.width - node.geometry.padding.left - node.geometry.padding.right
                    - node.geometry.border.left - node.geometry.border.right);

        auto utf8_char_len = [](unsigned char lead) -> size_t {
            if ((lead & 0x80) == 0) return 1;
            if ((lead & 0xE0) == 0xC0) return 2;
            if ((lead & 0xF0) == 0xE0) return 3;
            if ((lead & 0xF8) == 0xF0) return 4;
            return 1;
        };

        if (text_overflow_mode == 1) {
            // text-overflow: ellipsis — use actual font metrics for accurate truncation.
            const float text_width = s_text_measurer.measure_text_width(
                text_to_render, effective_font_size, node.font_family,
                eff_weight, eff_italic, node.letter_spacing);
            if (text_width > container_width && container_width > 0.0f) {
                const float ellipsis_width = s_text_measurer.measure_text_width(
                    ellipsis_str, effective_font_size, node.font_family,
                    eff_weight, eff_italic, node.letter_spacing);
                const float available_width = std::max(0.0f, container_width - ellipsis_width);

                // Binary search for the number of UTF-8 characters that fit.
                // Work with byte offsets while preserving UTF-8 boundaries.
                std::vector<size_t> char_ends; // byte offsets after each UTF-8 char
                for (size_t i = 0; i < text_to_render.size(); ) {
                    size_t char_len = utf8_char_len(static_cast<unsigned char>(text_to_render[i]));
                    i += char_len;
                    if (i > text_to_render.size()) i = text_to_render.size();
                    char_ends.push_back(i);
                }

                if (!char_ends.empty()) {
                    // Binary search: find largest prefix that fits in available_width.
                    size_t lo = 0, hi = char_ends.size();
                    while (lo < hi) {
                        size_t mid = lo + (hi - lo + 1) / 2;
                        size_t cut_bytes = char_ends[mid - 1];
                        const float w = s_text_measurer.measure_text_width(
                            text_to_render.substr(0, cut_bytes), effective_font_size, node.font_family,
                            eff_weight, eff_italic, node.letter_spacing);
                        if (w <= available_width) lo = mid;
                        else hi = mid - 1;
                    }
                    const size_t cut_bytes = (lo > 0) ? char_ends[lo - 1] : 0;
                    text_to_render = text_to_render.substr(0, cut_bytes) + ellipsis_str;
                }
            }
        } else if (text_overflow_mode == 2) {
            // text-overflow: fade — apply a trailing gradient fade when content overflows.
            const float text_width = s_text_measurer.measure_text_width(
                text_to_render, effective_font_size, node.font_family,
                eff_weight, eff_italic, node.letter_spacing);
            if (text_width > container_width && container_width > 0.0f) {
                needs_fade = true;
                // Fade covers the last 25% of the container (or up to 3em, whichever is smaller).
                float fade_len = std::min(container_width * 0.25f, effective_font_size * 3.0f);
                if (fade_len < effective_font_size) fade_len = effective_font_size; // minimum 1em

                // The fade region starts at container_end - fade_len.
                float parent_abs_x = abs_x - node.geometry.x; // approximate parent's abs_x
                fade_region_x = parent_abs_x + overflow_parent->geometry.padding.left
                              + overflow_parent->geometry.border.left
                              + container_width - fade_len;
                fade_region_y = abs_y;
                fade_region_w = fade_len;
                fade_region_h = effective_font_size * node.line_height;
            }
        } else {
            // text-overflow: clip (0) or unknown value — no synthetic overflow treatment.
        }
    }

    // Apply text-indent: offset first line of text in nearest block-level ancestor
    float text_x = abs_x;
    const auto* indent_block = [&node]() -> const clever::layout::LayoutNode* {
        for (const auto* ancestor = node.parent; ancestor; ancestor = ancestor->parent) {
            if (ancestor->display == clever::layout::DisplayType::Block ||
                ancestor->display == clever::layout::DisplayType::ListItem ||
                ancestor->display == clever::layout::DisplayType::Flex ||
                ancestor->display == clever::layout::DisplayType::Grid ||
                ancestor->display == clever::layout::DisplayType::Table ||
                ancestor->display == clever::layout::DisplayType::TableCell) {
                return ancestor;
            }
        }
        return nullptr;
    }();
    if (indent_block && indent_block->text_indent != 0) {
        const auto* first_text = [&indent_block]() -> const clever::layout::LayoutNode* {
            std::vector<const clever::layout::LayoutNode*> stack;
            stack.push_back(indent_block);
            while (!stack.empty()) {
                const auto* current = stack.back();
                stack.pop_back();
                if (current->is_text && !current->text_content.empty()) return current;
                for (auto it = current->children.rbegin(); it != current->children.rend(); ++it) {
                    if (it->get()) stack.push_back(it->get());
                }
            }
            return nullptr;
        }();
        if (first_text) {
            const auto absolute_y = [](const clever::layout::LayoutNode* n) -> float {
                float y = 0.0f;
                while (n) {
                    y += n->geometry.y;
                    n = n->parent;
                }
                return y;
            };
            if (std::fabs(absolute_y(first_text) - absolute_y(&node)) <= 0.5f) {
                text_x += indent_block->text_indent;
            }
        }
    }

    // Apply sub/sup vertical offset
    float text_y = abs_y;
    if (node.vertical_offset != 0) {
        text_y += node.vertical_offset;
    }

    // Apply hanging-punctuation
    if (node.parent && node.parent->hanging_punctuation > 0 && !text_to_render.empty()) {
        int hp = node.parent->hanging_punctuation;
        float char_w = effective_font_size * 0.6f + node.letter_spacing;
        // Check if first char is hanging punctuation
        auto is_hanging_punct = [](char c) -> bool {
            return c == '"' || c == '\'' || c == '(' || c == '[' || c == '{' ||
                   c == '\xE2'; // UTF-8 lead byte for smart quotes
        };
        // "first" (1) or "first last" (5): hang first punctuation outside start margin
        if ((hp == 1 || hp == 5) && is_hanging_punct(text_to_render[0])) {
            text_x -= char_w;
        }
    }

    // Draw -webkit-text-stroke (before shadows and main text fill)
    if (node.text_stroke_width > 0) {
        uint32_t sc = node.text_stroke_color;
        uint8_t sa = static_cast<uint8_t>((sc >> 24) & 0xFF);
        uint8_t sr = static_cast<uint8_t>((sc >> 16) & 0xFF);
        uint8_t sg = static_cast<uint8_t>((sc >> 8) & 0xFF);
        uint8_t sb = static_cast<uint8_t>(sc & 0xFF);
        Color stroke_col = {sr, sg, sb, sa};
        float sw = node.text_stroke_width;
        // Draw text at offsets to simulate stroke
        for (float dx = -sw; dx <= sw; dx += std::max(0.5f, sw * 0.5f)) {
            for (float dy = -sw; dy <= sw; dy += std::max(0.5f, sw * 0.5f)) {
                if (dx == 0 && dy == 0) continue;
                if (dx * dx + dy * dy > sw * sw * 1.1f) continue; // circular
                list.draw_text(text_to_render, text_x + dx, text_y + dy, effective_font_size,
                               stroke_col, node.font_family, eff_weight, eff_italic,
                               node.letter_spacing);
            }
        }
    }

    // Override fill color if -webkit-text-fill-color is set
    if (node.text_fill_color != 0) {
        uint32_t fc = node.text_fill_color;
        a = static_cast<uint8_t>((fc >> 24) & 0xFF);
        r = static_cast<uint8_t>((fc >> 16) & 0xFF);
        g = static_cast<uint8_t>((fc >> 8) & 0xFF);
        b = static_cast<uint8_t>(fc & 0xFF);
    }

    // Draw text shadows before main text (if present)
    if (!node.text_shadows.empty()) {
        // Render in reverse order: last shadow painted first, first shadow on top
        for (int si = static_cast<int>(node.text_shadows.size()) - 1; si >= 0; --si) {
            auto& ts = node.text_shadows[static_cast<size_t>(si)];
            if (ts.color != 0x00000000) {
                uint32_t tsc = ts.color;
                uint8_t tsa = static_cast<uint8_t>((tsc >> 24) & 0xFF);
                uint8_t tsr = static_cast<uint8_t>((tsc >> 16) & 0xFF);
                uint8_t tsg = static_cast<uint8_t>((tsc >> 8) & 0xFF);
                uint8_t tsb = static_cast<uint8_t>(tsc & 0xFF);
                float shadow_x = abs_x + ts.offset_x;
                float shadow_y = text_y + ts.offset_y;
                list.draw_text(text_to_render, shadow_x, shadow_y, effective_font_size,
                               {tsr, tsg, tsb, tsa},
                               node.font_family, eff_weight, eff_italic,
                               node.letter_spacing, 0, 4, ts.blur);
            }
        }
    } else if (node.text_shadow_color != 0x00000000) {
        // Fallback: single text shadow (legacy path)
        uint32_t tsc = node.text_shadow_color;
        uint8_t tsa = static_cast<uint8_t>((tsc >> 24) & 0xFF);
        uint8_t tsr = static_cast<uint8_t>((tsc >> 16) & 0xFF);
        uint8_t tsg = static_cast<uint8_t>((tsc >> 8) & 0xFF);
        uint8_t tsb = static_cast<uint8_t>(tsc & 0xFF);
        float shadow_x = abs_x + node.text_shadow_offset_x;
        float shadow_y = text_y + node.text_shadow_offset_y;
        list.draw_text(text_to_render, shadow_x, shadow_y, effective_font_size,
                       {tsr, tsg, tsb, tsa},
                       node.font_family, eff_weight, eff_italic,
                       node.letter_spacing, 0, 4, node.text_shadow_blur);
    }

    // Determine line-clamp parameters from parent
    int line_clamp = -1;
    bool parent_has_ellipsis = false;
    if (node.parent) {
        line_clamp = node.parent->line_clamp;
        parent_has_ellipsis = (node.parent->text_overflow == 1);
    }

    // white-space: pre (2) / pre-wrap (3) / pre-line (4) / break-spaces (5) — render explicit newlines.
    // For pre mode, split on \n and draw each line separately using the node's line_height
    // (instead of delegating to CoreText which uses a hardcoded 1.2x multiplier).
    bool prewrap_handled = false;
    if (node.is_text && !text_to_render.empty() &&
        (node.white_space == 2 || node.white_space == 3 ||
         node.white_space == 4 || node.white_space == 5) &&
        text_to_render.find('\n') != std::string::npos) {
        float line_h = node.font_size * node.line_height;
        float draw_y = text_y;
        size_t start = 0;
        while (start <= text_to_render.size()) {
            size_t nl = text_to_render.find('\n', start);
            std::string line;
            if (nl == std::string::npos) {
                line = text_to_render.substr(start);
                start = text_to_render.size() + 1; // signal end of iteration
            } else {
                line = text_to_render.substr(start, nl - start);
                start = nl + 1;
            }
            // pre-line (4) collapses spaces within lines but preserves newlines
            if (node.white_space == 4 && !line.empty()) {
                std::string collapsed;
                bool prev_space = false;
                for (char c : line) {
                    if (c == ' ' || c == '\t') {
                        if (!prev_space) { collapsed += ' '; prev_space = true; }
                    } else {
                        collapsed += c;
                        prev_space = false;
                    }
                }
                line = collapsed;
            }
            if (!line.empty()) {
                // Pass node.tab_size so the renderer expands \t correctly per CSS tab-size
                list.draw_text(line, text_x, draw_y, effective_font_size, {r, g, b, a},
                               node.font_family, eff_weight, eff_italic,
                               node.letter_spacing, node.word_spacing, node.tab_size);
            }
            draw_y += line_h;
            if (start > text_to_render.size()) break;
        }
        prewrap_handled = true;
    }

    // Check if word-break wrapping is needed for rendering
    // line-break: anywhere (4) allows breaking at any character position
    bool can_break_word = (node.word_break == 1 || node.overflow_wrap >= 1 || node.line_break == 4);
    float char_w = effective_font_size * 0.6f + node.letter_spacing;
    float single_text_width = static_cast<float>(text_to_render.length()) * char_w;

    // Use the parent's content width as the wrapping container.
    // The text node's own geom.width reflects the measured text width (which may
    // use real font metrics), so comparing against it would cause a mismatch
    // with the 0.6f approximation used for single_text_width.
    float container_w = geom.width;
    if (node.parent) {
        float parent_content_w = node.parent->geometry.width
            - node.parent->geometry.padding.left - node.parent->geometry.padding.right
            - node.parent->geometry.border.left - node.parent->geometry.border.right;
        if (parent_content_w > 0) {
            container_w = parent_content_w;
        }
    }

    // For line-clamp: use the parent's content width as the wrapping container,
    // since the text node's own width may be the full unwrapped text width
    bool line_clamp_active = (line_clamp > 0 && node.parent &&
                              node.parent->overflow == 1);
    if (line_clamp_active && node.parent) {
        float parent_content_w = node.parent->geometry.width
            - node.parent->geometry.padding.left - node.parent->geometry.padding.right
            - node.parent->geometry.border.left - node.parent->geometry.border.right;
        if (parent_content_w > 0) {
            container_w = parent_content_w;
        }
    }
    bool needs_wrap = (can_break_word && node.parent && container_w > 0 && single_text_width > container_w);
    // Also enable wrapping if line_clamp is active and text overflows the container
    if (line_clamp_active && container_w > 0 && single_text_width > container_w) {
        needs_wrap = true;
    }

    // CSS writing-mode: vertical-rl (1), vertical-lr (2), sideways-rl (3), sideways-lr (4)
    // Draw each character stacked vertically and rotate baseline flow.
    bool vertical_handled = false;
    bool vertical_rotated = false;
    float vertical_deco_len = 0.0f;
    int wm = node.parent ? node.parent->writing_mode : node.writing_mode;
    if (wm == 1 || wm == 2 || wm == 3 || wm == 4) {
        const float rotate_angle = (wm == 1 || wm == 3) ? 90.0f : -90.0f;
        list.push_rotate(rotate_angle, text_x, text_y);
        vertical_rotated = true;

        float draw_x = text_x;
        float draw_y = text_y;
        float step = effective_font_size * node.line_height;
        int stack_count = 0;
        // Walk through text, drawing one character (or UTF-8 sequence) at a time
        size_t i = 0;
        while (i < text_to_render.size()) {
            // Determine UTF-8 character length
            unsigned char ch = static_cast<unsigned char>(text_to_render[i]);
            size_t clen = 1;
            if (ch >= 0xF0) clen = 4;
            else if (ch >= 0xE0) clen = 3;
            else if (ch >= 0xC0) clen = 2;
            if (i + clen > text_to_render.size()) clen = text_to_render.size() - i;
            std::string one_char = text_to_render.substr(i, clen);
            list.draw_text(one_char, draw_x, draw_y, effective_font_size, {r, g, b, a},
                           node.font_family, eff_weight, eff_italic,
                           node.letter_spacing);
            draw_y += step;
            i += clen;
            ++stack_count;
        }
        vertical_deco_len = static_cast<float>(stack_count) * step;
        vertical_handled = true;
    }

    if (vertical_handled) {
        // Already rendered vertically — skip to decoration
    } else if (prewrap_handled) {
        // Already rendered via pre-wrap/pre-line newline splitting — skip to decoration
    } else if (needs_wrap) {
        // Draw text in wrapped lines character-by-character
        float line_h = node.font_size * node.line_height;
        int chars_per_line = std::max(1, static_cast<int>(container_w / char_w));
        float draw_y = text_y;
        size_t pos = 0;
        int line_num = 0;

        // Compute total number of lines for line-clamp check
        int total_lines = 0;
        if (line_clamp_active) {
            size_t text_len = text_to_render.length();
            total_lines = static_cast<int>((text_len + static_cast<size_t>(chars_per_line) - 1) / static_cast<size_t>(chars_per_line));
        }

        while (pos < text_to_render.length()) {
            line_num++;
            size_t end = std::min(pos + static_cast<size_t>(chars_per_line), text_to_render.length());
            std::string line_text = text_to_render.substr(pos, end - pos);

            // Apply line-clamp: stop after N lines
            if (line_clamp_active && line_num >= line_clamp) {
                // This is the last allowed line
                if (total_lines > line_clamp && parent_has_ellipsis) {
                    // Truncate the line and append ellipsis
                    float ellipsis_w = char_w; // one character width for ellipsis
                    float available_w = container_w - ellipsis_w;
                    int max_chars_on_line = std::max(0, static_cast<int>(available_w / char_w));
                    if (max_chars_on_line < static_cast<int>(line_text.size())) {
                        line_text = line_text.substr(0, static_cast<size_t>(max_chars_on_line)) + "\xE2\x80\xA6";
                    } else {
                        line_text += "\xE2\x80\xA6";
                    }
                }
                list.draw_text(line_text, abs_x, draw_y, effective_font_size, {r, g, b, a},
                               node.font_family, eff_weight, eff_italic,
                               node.letter_spacing);
                break; // stop drawing more lines
            }

            list.draw_text(line_text, abs_x, draw_y, effective_font_size, {r, g, b, a},
                           node.font_family, eff_weight, eff_italic,
                           node.letter_spacing);
            draw_y += line_h;
            pos = end;
        }
    } else if (!can_break_word && node.is_text && container_w > 0 &&
               single_text_width > container_w &&
               node.white_space != 1 && node.white_space != 2 &&
               node.word_break != 2 && node.text_wrap != 1) {
        // Word-boundary wrapping: break text at spaces (not character-level)
        float line_h = node.font_size * node.line_height;
        float draw_y = text_y;
        float space_w = char_w;

        // Split into words at spaces
        std::vector<std::string> words;
        {
            size_t start = 0;
            while (start < text_to_render.size()) {
                size_t sp = text_to_render.find(' ', start);
                if (sp == std::string::npos) {
                    words.push_back(text_to_render.substr(start));
                    break;
                }
                words.push_back(text_to_render.substr(start, sp - start));
                start = sp + 1;
            }
        }

        // Get text-align from parent (0=left, 1=center, 2=right, 3=justify)
        int text_align = node.parent ? node.parent->text_align : 0;
        // CSS direction: RTL default text alignment is right
        int text_dir = node.parent ? node.parent->direction : node.direction;
        if (text_dir == 1 && text_align == 0) {
            text_align = 2; // RTL default: right-aligned
        }
        // Get text-justify from parent (0=auto, 1=inter-word, 2=inter-character, 3=none)
        int text_justify = node.parent ? node.parent->text_justify : 0;
        // Get text-align-last from parent (0=auto, 1=left, 2=right, 3=center, 4=justify)
        int text_align_last = node.parent ? node.parent->text_align_last : 0;

        // Helper to flush a line with text-align offset
        auto flush_line = [&](const std::string& line, float line_w, bool is_last) {
            float draw_x = abs_x;
            // For the last line of justified text, use text-align-last
            bool do_justify = (text_align == 3 && !is_last && text_justify != 3);
            if (is_last && text_align == 3 && text_align_last == 4) {
                do_justify = true; // text-align-last: justify — justify last line too
            }
            if (do_justify && line_w < container_w) {
                // text-justify: inter-character — distribute extra space between ALL characters
                if (text_justify == 2 && line.length() > 1) {
                    float extra_per_char = (container_w - line_w) / static_cast<float>(line.length() - 1);
                    float cx = abs_x;
                    for (size_t ci = 0; ci < line.length(); ci++) {
                        std::string ch(1, line[ci]);
                        list.draw_text(ch, cx, draw_y, effective_font_size, {r, g, b, a},
                                       node.font_family, eff_weight, eff_italic,
                                       node.letter_spacing);
                        cx += char_w + extra_per_char;
                    }
                    draw_y += line_h;
                    return;
                }
                // text-justify: auto or inter-word — distribute extra space between words
                int space_count = 0;
                for (char c : line) { if (c == ' ') space_count++; }
                if (space_count > 0) {
                    float extra_space = (container_w - line_w) / static_cast<float>(space_count);
                    // Render word by word with extra spacing
                    float wx = abs_x;
                    size_t start = 0;
                    while (start < line.size()) {
                        size_t sp = line.find(' ', start);
                        std::string word;
                        if (sp == std::string::npos) {
                            word = line.substr(start);
                            list.draw_text(word, wx, draw_y, effective_font_size, {r, g, b, a},
                                           node.font_family, eff_weight, eff_italic,
                                           node.letter_spacing);
                            break;
                        }
                        word = line.substr(start, sp - start);
                        list.draw_text(word, wx, draw_y, effective_font_size, {r, g, b, a},
                                       node.font_family, eff_weight, eff_italic,
                                       node.letter_spacing);
                        wx += static_cast<float>(word.length()) * char_w + space_w + extra_space;
                        start = sp + 1;
                    }
                    draw_y += line_h;
                    return;
                }
            }
            // Determine effective alignment for this line
            int eff_align = text_align;
            if (is_last && text_align == 3 && text_align_last > 0) {
                // text-align-last overrides last line alignment in justified text
                // 1=left, 2=right, 3=center (4=justify handled above)
                if (text_align_last == 1) eff_align = 0;
                else if (text_align_last == 2) eff_align = 2;
                else if (text_align_last == 3) eff_align = 1; // center
            }
            if (eff_align == 1) { // center
                draw_x += (container_w - line_w) / 2.0f;
            } else if (eff_align == 2) { // right
                draw_x += container_w - line_w;
            }
            list.draw_text(line, draw_x, draw_y, effective_font_size, {r, g, b, a},
                           node.font_family, eff_weight, eff_italic,
                           node.letter_spacing);
            draw_y += line_h;
        };

        // Count lines at a given width using greedy wrapping
        auto count_lines_at_width = [&](float max_w) -> int {
            int lines = 1;
            float cur_w = 0;
            for (size_t wi = 0; wi < words.size(); wi++) {
                float word_w = static_cast<float>(words[wi].length()) * char_w;
                if (words[wi].empty()) continue;
                if (cur_w == 0) {
                    cur_w = word_w;
                } else if (cur_w + space_w + word_w <= max_w) {
                    cur_w += space_w + word_w;
                } else {
                    lines++;
                    cur_w = word_w;
                }
            }
            return lines;
        };

        // Determine effective wrapping width
        float wrap_width = container_w;

        if (node.text_wrap == 2 && words.size() > 1) {
            // text-wrap: balance — find minimum width that keeps same line count
            int target_lines = count_lines_at_width(container_w);
            if (target_lines > 1) {
                // Binary search for minimum width with same line count
                float lo = 0;
                float hi = container_w;
                for (int iter = 0; iter < 20; iter++) {
                    float mid = (lo + hi) / 2.0f;
                    if (count_lines_at_width(mid) <= target_lines) {
                        hi = mid;
                    } else {
                        lo = mid;
                    }
                }
                wrap_width = hi;
            }
        } else if (node.text_wrap == 3 && words.size() > 2) {
            // text-wrap: pretty — avoid widows (single words on last line)
            // Check if last line would have only 1 word at full width
            int total_lines = count_lines_at_width(container_w);
            if (total_lines > 1) {
                // Simulate wrapping to find last line word count
                float test_w = 0;
                int last_line_words = 0;
                for (size_t i = 0; i < words.size(); i++) {
                    float ww = static_cast<float>(words[i].length()) * char_w;
                    if (test_w + ww + (last_line_words > 0 ? space_w : 0) > container_w && last_line_words > 0) {
                        test_w = ww;
                        last_line_words = 1;
                    } else {
                        test_w += ww + (last_line_words > 0 ? space_w : 0);
                        last_line_words++;
                    }
                }
                // If last line has only 1 word, reduce wrap width slightly to push
                // a second word down to the last line
                if (last_line_words == 1) {
                    float reduced = container_w * 0.92f;
                    if (count_lines_at_width(reduced) == total_lines) {
                        wrap_width = reduced;
                    }
                }
            }
        }

        // Greedily fit words onto lines at wrap_width, render each line
        std::string current_line;
        float current_width = 0;

        for (size_t wi = 0; wi < words.size(); wi++) {
            float word_w = static_cast<float>(words[wi].length()) * char_w;
            if (words[wi].empty()) {
                if (!current_line.empty()) current_width += space_w;
                continue;
            }

            if (current_line.empty()) {
                current_line = words[wi];
                current_width = word_w;
            } else if (current_width + space_w + word_w <= wrap_width) {
                current_line += " " + words[wi];
                current_width += space_w + word_w;
            } else {
                // Word doesn't fit — check if we can hyphenate (hyphens: auto)
                // line-break: strict (3) prevents automatic hyphenation
                int hyphens_val = node.parent ? node.parent->hyphens : node.hyphens;
                if (hyphens_val == 2 && node.line_break != 3 && words[wi].length() > 4) {
                    // Try to split the word: find best split point (min 2 chars each side)
                    float remaining = wrap_width - current_width - space_w - char_w; // -char_w for hyphen
                    if (remaining > char_w * 2) {
                        int max_chars = static_cast<int>(remaining / char_w);
                        if (max_chars >= 2 && max_chars < static_cast<int>(words[wi].length()) - 1) {
                            // Split at max_chars, prefer vowel boundaries
                            int split = max_chars;
                            // Try to find a better split point (after a vowel)
                            for (int si = max_chars; si >= 2; si--) {
                                char c = words[wi][static_cast<size_t>(si) - 1];
                                if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
                                    c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
                                    split = si;
                                    break;
                                }
                            }
                            std::string first = words[wi].substr(0, static_cast<size_t>(split)) + "-";
                            std::string rest = words[wi].substr(static_cast<size_t>(split));
                            if (!current_line.empty()) {
                                current_line += " " + first;
                            } else {
                                current_line = first;
                            }
                            current_width += space_w + static_cast<float>(first.length()) * char_w;
                            flush_line(current_line, current_width, false);
                            current_line = rest;
                            current_width = static_cast<float>(rest.length()) * char_w;
                            continue;
                        }
                    }
                }
                flush_line(current_line, current_width, false);
                current_line = words[wi];
                current_width = word_w;
            }
        }
        if (!current_line.empty()) {
            flush_line(current_line, current_width, true);
        }
    } else {
        // Single-line rendering (possibly with line-clamp:1 acting as single-line ellipsis)
        if (line_clamp_active && line_clamp == 1 && parent_has_ellipsis &&
            single_text_width > container_w && container_w > 0) {
            float ellipsis_w = char_w;
            float available_w = container_w - ellipsis_w;
            if (available_w < 0) available_w = 0;
            int max_chars = static_cast<int>(available_w / char_w);
            if (max_chars < 0) max_chars = 0;
            if (max_chars < static_cast<int>(text_to_render.size())) {
                text_to_render = text_to_render.substr(0, static_cast<size_t>(max_chars)) + "\xE2\x80\xA6";
            }
        }

        // Apply text-indent to the drawing x position
        float draw_start_x = text_x;

        // ::first-line rendering: apply special styling to the first line of text
        // The pipeline sets has_first_line on the first text node itself
        if (node.has_first_line) {
            // Use ::first-line styling for the whole text (single line = first line)
            float fl_size = (node.first_line_font_size > 0)
                ? node.first_line_font_size : effective_font_size;
            uint8_t fl_r = r, fl_g = g, fl_b = b, fl_a = a;
            if (node.first_line_color != 0) {
                fl_a = static_cast<uint8_t>((node.first_line_color >> 24) & 0xFF);
                fl_r = static_cast<uint8_t>((node.first_line_color >> 16) & 0xFF);
                fl_g = static_cast<uint8_t>((node.first_line_color >> 8) & 0xFF);
                fl_b = static_cast<uint8_t>(node.first_line_color & 0xFF);
            }
            int fl_weight = node.first_line_bold ? 700 : node.font_weight;
            bool fl_italic = node.first_line_italic || node.font_italic;
            list.draw_text(text_to_render, draw_start_x, text_y, fl_size, {fl_r, fl_g, fl_b, fl_a},
                           node.font_family, fl_weight, fl_italic, node.letter_spacing);
        }
        // CSS initial-letter (drop cap): enlarge first letter to span multiple lines
        else if (node.parent && node.parent->initial_letter_size > 0 && text_to_render.size() > 0) {
            // initial_letter_size = number of lines the first letter should span
            float line_h = node.font_size * node.line_height;
            float drop_size = node.parent->initial_letter_size * line_h;

            // Find the first non-whitespace character
            size_t skip = 0;
            while (skip < text_to_render.size() && text_to_render[skip] == ' ') skip++;
            if (skip < text_to_render.size()) {
                // UTF-8 first char length
                size_t first_char_len = 1;
                unsigned char c0 = static_cast<unsigned char>(text_to_render[skip]);
                if (c0 >= 0xC0 && c0 < 0xE0) first_char_len = 2;
                else if (c0 >= 0xE0 && c0 < 0xF0) first_char_len = 3;
                else if (c0 >= 0xF0) first_char_len = 4;
                if (skip + first_char_len > text_to_render.size())
                    first_char_len = text_to_render.size() - skip;

                std::string first_letter = text_to_render.substr(skip, first_char_len);
                std::string rest = text_to_render.substr(skip + first_char_len);

                // Draw the drop cap at enlarged size
                float drop_char_w = drop_size * 0.6f;
                list.draw_text(first_letter, draw_start_x, text_y, drop_size, {r, g, b, a},
                               node.font_family, eff_weight, eff_italic,
                               node.letter_spacing);

                // Draw the rest of the text after the drop cap
                if (!rest.empty()) {
                    list.draw_text(rest, draw_start_x + drop_char_w, text_y,
                                   effective_font_size, {r, g, b, a},
                                   node.font_family, eff_weight, eff_italic,
                                   node.letter_spacing);
                }
            } else {
                list.draw_text(text_to_render, draw_start_x, text_y, effective_font_size,
                               {r, g, b, a}, node.font_family, node.font_weight,
                               node.font_italic, node.letter_spacing);
            }
        }
        // ::first-letter rendering: draw first character with special styling
        else if (node.has_first_letter && text_to_render.size() > 1) {
            // Determine first letter byte length (handle UTF-8 multi-byte)
            size_t first_char_len = 1;
            unsigned char c0 = static_cast<unsigned char>(text_to_render[0]);
            if (c0 >= 0xC0 && c0 < 0xE0) first_char_len = 2;
            else if (c0 >= 0xE0 && c0 < 0xF0) first_char_len = 3;
            else if (c0 >= 0xF0) first_char_len = 4;
            if (first_char_len > text_to_render.size()) first_char_len = text_to_render.size();

            // Skip leading whitespace to find the actual first letter
            size_t skip = 0;
            while (skip < text_to_render.size() && text_to_render[skip] == ' ') skip++;

            if (skip < text_to_render.size()) {
                // Recalculate first char len from the actual letter position
                c0 = static_cast<unsigned char>(text_to_render[skip]);
                first_char_len = 1;
                if (c0 >= 0xC0 && c0 < 0xE0) first_char_len = 2;
                else if (c0 >= 0xE0 && c0 < 0xF0) first_char_len = 3;
                else if (c0 >= 0xF0) first_char_len = 4;
                if (skip + first_char_len > text_to_render.size()) first_char_len = text_to_render.size() - skip;

                std::string first_letter = text_to_render.substr(skip, first_char_len);
                std::string rest = text_to_render.substr(skip + first_char_len);

                // First letter style
                float fl_font_size = (node.first_letter_font_size > 0) ? node.first_letter_font_size : node.font_size;
                uint8_t fl_r = r, fl_g = g, fl_b = b, fl_a = a;
                if (node.first_letter_color != 0) {
                    fl_a = static_cast<uint8_t>((node.first_letter_color >> 24) & 0xFF);
                    fl_r = static_cast<uint8_t>((node.first_letter_color >> 16) & 0xFF);
                    fl_g = static_cast<uint8_t>((node.first_letter_color >> 8) & 0xFF);
                    fl_b = static_cast<uint8_t>(node.first_letter_color & 0xFF);
                }
                int fl_weight = node.first_letter_bold ? 700 : node.font_weight;

                // Draw any leading whitespace at normal size
                float draw_x = draw_start_x;
                if (skip > 0) {
                    std::string leading_space = text_to_render.substr(0, skip);
                    list.draw_text(leading_space, draw_x, text_y, effective_font_size, {r, g, b, a},
                                   node.font_family, eff_weight, eff_italic,
                                   node.letter_spacing);
                    draw_x += static_cast<float>(skip) * char_w;
                }

                // Draw the first letter at the special size/color
                list.draw_text(first_letter, draw_x, text_y, fl_font_size, {fl_r, fl_g, fl_b, fl_a},
                               node.font_family, fl_weight, node.font_italic,
                               node.letter_spacing);
                float fl_char_w = fl_font_size * 0.6f + node.letter_spacing;
                draw_x += fl_char_w;

                // Draw the rest of the text at normal size
                if (!rest.empty()) {
                    list.draw_text(rest, draw_x, text_y, effective_font_size, {r, g, b, a},
                                   node.font_family, eff_weight, eff_italic,
                                   node.letter_spacing);
                }
            } else {
                // All whitespace, just draw normally
                list.draw_text(text_to_render, draw_start_x, text_y, effective_font_size, {r, g, b, a},
                               node.font_family, eff_weight, eff_italic,
                               node.letter_spacing, node.word_spacing, node.tab_size);
            }
        } else {
            list.draw_text(text_to_render, draw_start_x, text_y, effective_font_size, {r, g, b, a},
                           node.font_family, eff_weight, eff_italic,
                           node.letter_spacing, node.word_spacing, node.tab_size);
        }

        // Build effective font features from font_feature_settings + font_variant_numeric
        std::string effective_features;
        for (size_t fi = 0; fi < node.font_feature_settings.size(); ++fi) {
            if (fi > 0) effective_features += ", ";
            effective_features += "\"" + node.font_feature_settings[fi].first + "\" " +
                                  std::to_string(node.font_feature_settings[fi].second);
        }
        if (node.font_variant_numeric != 0) {
            // Map font_variant_numeric to OpenType feature tags
            const char* tag = nullptr;
            switch (node.font_variant_numeric) {
                case 1: tag = "\"ordn\" 1"; break; // ordinal
                case 2: tag = "\"zero\" 1"; break; // slashed-zero
                case 3: tag = "\"lnum\" 1"; break; // lining-nums
                case 4: tag = "\"onum\" 1"; break; // oldstyle-nums
                case 5: tag = "\"pnum\" 1"; break; // proportional-nums
                case 6: tag = "\"tnum\" 1"; break; // tabular-nums
                default: break;
            }
            if (tag) {
                if (!effective_features.empty()) effective_features += ", ";
                effective_features += tag;
            }
        }

        // Map font_variant_caps to OpenType feature tags
        // 1=small-caps, 2=all-small-caps, 3=petite-caps, 4=all-petite-caps, 5=unicase, 6=titling-caps
        if (node.font_variant_caps != 0) {
            const char* tag = nullptr;
            switch (node.font_variant_caps) {
                case 1: tag = "\"smcp\" 1"; break;
                case 2: tag = "\"smcp\" 1, \"c2sc\" 1"; break;
                case 3: tag = "\"pcap\" 1"; break;
                case 4: tag = "\"pcap\" 1, \"c2pc\" 1"; break;
                case 5: tag = "\"unic\" 1"; break;
                case 6: tag = "\"titl\" 1"; break;
                default: break;
            }
            if (tag) {
                if (!effective_features.empty()) effective_features += ", ";
                effective_features += tag;
            }
        }

        // Map font_variant_ligatures to OpenType feature tags
        // 0=normal (default, ligatures on), 1=none (all off), 2=common-ligatures,
        // 3=no-common-ligatures, 4=discretionary-ligatures, 5=no-discretionary-ligatures
        if (node.font_variant_ligatures == 1) {
            // none: disable all ligatures
            if (!effective_features.empty()) effective_features += ", ";
            effective_features += "\"liga\" 0, \"clig\" 0, \"dlig\" 0, \"hlig\" 0";
        } else if (node.font_variant_ligatures == 3) {
            // no-common-ligatures: disable standard + contextual
            if (!effective_features.empty()) effective_features += ", ";
            effective_features += "\"liga\" 0, \"clig\" 0";
        } else if (node.font_variant_ligatures == 4) {
            // discretionary-ligatures: enable discretionary
            if (!effective_features.empty()) effective_features += ", ";
            effective_features += "\"dlig\" 1";
        } else if (node.font_variant_ligatures == 5) {
            // no-discretionary-ligatures: explicitly disable
            if (!effective_features.empty()) effective_features += ", ";
            effective_features += "\"dlig\" 0";
        }

        // Map font_variant_east_asian to OpenType feature tags
        if (node.font_variant_east_asian != 0) {
            const char* tag = nullptr;
            switch (node.font_variant_east_asian) {
                case 1: tag = "\"jp78\" 1"; break;
                case 2: tag = "\"jp83\" 1"; break;
                case 3: tag = "\"jp90\" 1"; break;
                case 4: tag = "\"jp04\" 1"; break;
                case 5: tag = "\"smpl\" 1"; break;
                case 6: tag = "\"trad\" 1"; break;
                case 7: tag = "\"fwid\" 1"; break;
                case 8: tag = "\"pwid\" 1"; break;
                case 9: tag = "\"ruby\" 1"; break;
                default: break;
            }
            if (tag) {
                if (!effective_features.empty()) effective_features += ", ";
                effective_features += tag;
            }
        }

        // Map font_variant_position to OpenType feature tags
        if (node.font_variant_position != 0) {
            const char* tag = nullptr;
            switch (node.font_variant_position) {
                case 1: tag = "\"subs\" 1"; break;
                case 2: tag = "\"sups\" 1"; break;
                default: break;
            }
            if (tag) {
                if (!effective_features.empty()) effective_features += ", ";
                effective_features += tag;
            }
        }

        // Map font_variant_alternates to OpenType feature tags
        if (node.font_variant_alternates != 0) {
            const char* tag = nullptr;
            switch (node.font_variant_alternates) {
                case 1: tag = "\"hist\" 1"; break;
                case 2: tag = "\"swsh\" 1"; break;
                case 3: tag = "\"ornm\" 1"; break;
                case 4: tag = "\"nalt\" 1"; break;
                case 5: tag = "\"salt\" 1"; break;
                case 6: tag = "\"calt\" 1"; break;
                default: break;
            }
            if (tag) {
                if (!effective_features.empty()) effective_features += ", ";
                effective_features += tag;
            }
        }

        // Build effective font variations from font_variation_settings + font_stretch
        std::string effective_variations = node.font_variation_settings;
        if (node.font_stretch != 5 && node.font_stretch >= 1 && node.font_stretch <= 9) {
            // Map font_stretch 1-9 to CSS font-stretch percentage (wdth axis)
            // 1=50%, 2=62.5%, 3=75%, 4=87.5%, 5=100%, 6=112.5%, 7=125%, 8=150%, 9=200%
            static const float wdth_map[] = {0, 50, 62.5f, 75, 87.5f, 100, 112.5f, 125, 150, 200};
            float wdth = wdth_map[node.font_stretch];
            if (!effective_variations.empty()) effective_variations += ", ";
            effective_variations += "\"wdth\" " + std::to_string(static_cast<int>(wdth));
        }

        // Apply font-feature-settings / font-variation-settings to last draw_text command
        if (!effective_features.empty() || !effective_variations.empty()) {
            list.set_last_font_features(effective_features, effective_variations);
        }

        // Apply text rendering hints (text-rendering, font-kerning, font-optical-sizing)
        if (node.text_rendering != 0 || node.font_kerning != 0 || node.font_optical_sizing != 0) {
            list.set_last_text_hints(node.text_rendering, node.font_kerning, node.font_optical_sizing);
        }
    }

    // Apply text-overflow: fade mask gradient if needed
    // This draws a gradient mask over the trailing portion of the text, fading from
    // fully opaque to fully transparent (left-to-right), creating a smooth fade-out effect.
    if (needs_fade && fade_region_w > 0) {
        // Gradient from fully opaque (alpha=255) on the left to fully transparent (alpha=0) on the right
        // Using mask_direction=1 (to right) with stops: opaque at 0%, transparent at 100%
        std::vector<std::pair<uint32_t, float>> fade_stops = {
            {0xFF000000, 0.0f},  // fully opaque at start
            {0x00000000, 1.0f}   // fully transparent at end
        };
        Rect fade_bounds = {fade_region_x, fade_region_y, fade_region_w, fade_region_h};
        list.apply_mask_gradient(fade_bounds, 90.0f, fade_stops); // 90 degrees = left-to-right
    }

    // Draw text decoration (underline / overline / line-through).
    // Bits: 1=underline, 2=overline, 4=line-through.
    int deco_bits = node.text_decoration_bits;
    if (node.text_decoration == 0) {
        deco_bits = 0;
    } else if (deco_bits == 0) {
        // text_decoration value mapping:
        // 0=none, 1=underline, 2=overline, 3=line-through.
        // Values >3 are treated as a bitmask combination.
        if (node.text_decoration == 1) deco_bits = 1;
        else if (node.text_decoration == 2) deco_bits = 2;
        else if (node.text_decoration == 3) deco_bits = 4;
        else if (node.text_decoration > 3) deco_bits = node.text_decoration;
    }
    deco_bits &= 0x7;

    if (deco_bits != 0) {
        uint8_t dr = r, dg = g, db = b, da = a;
        if (node.text_decoration_color != 0) {
            da = static_cast<uint8_t>((node.text_decoration_color >> 24) & 0xFF);
            dr = static_cast<uint8_t>((node.text_decoration_color >> 16) & 0xFF);
            dg = static_cast<uint8_t>((node.text_decoration_color >> 8) & 0xFF);
            db = static_cast<uint8_t>(node.text_decoration_color & 0xFF);
        }

        const float thickness = node.text_decoration_thickness > 0.0f
                                    ? node.text_decoration_thickness
                                    : 1.0f;
        const float deco_x = text_x;
        float deco_w = geom.width;
        if (deco_w <= 0) {
            if (vertical_rotated && vertical_deco_len > 0.0f) {
                deco_w = vertical_deco_len;
            } else {
                deco_w = static_cast<float>(text_to_render.size()) *
                         (node.font_size * 0.6f + node.letter_spacing);
            }
        }
        if (deco_w > 0) {
            auto draw_deco_line = [&](float line_y) {
                const int style = node.text_decoration_style;
                if (style == 1) { // double
                    const float line_gap = thickness + 1.0f;
                    list.fill_rect({deco_x, line_y, deco_w, thickness}, {dr, dg, db, da});
                    list.fill_rect({deco_x, line_y + line_gap, deco_w, thickness}, {dr, dg, db, da});
                } else if (style == 2) { // dotted
                    float x_pos = deco_x;
                    const float end_x = deco_x + deco_w;
                    const float dot_size = thickness;
                    const float dot_step = thickness * 3.0f;
                    while (x_pos < end_x) {
                        list.fill_rect({x_pos, line_y, dot_size, dot_size}, {dr, dg, db, da});
                        x_pos += dot_step;
                    }
                } else if (style == 3) { // dashed
                    float x_pos = deco_x;
                    const float end_x = deco_x + deco_w;
                    const float dash_base_w = thickness * 4.0f;
                    const float dash_gap = thickness * 2.0f;
                    while (x_pos < end_x) {
                        float dash_w = std::min(dash_base_w, end_x - x_pos);
                        list.fill_rect({x_pos, line_y, dash_w, thickness}, {dr, dg, db, da});
                        x_pos += dash_base_w + dash_gap;
                    }
                } else if (style == 4) { // wavy
                    const float amplitude = 1.5f * thickness;
                    const float wavelength = 8.0f * thickness;
                    const float step = 1.0f;
                    float prev_x = deco_x;
                    float prev_y = line_y;
                    for (float dx = step; dx <= deco_w; dx += step) {
                        float cur_x = deco_x + std::min(dx, deco_w);
                        float phase = (2.0f * static_cast<float>(M_PI) * dx) / wavelength;
                        float cur_y = line_y + amplitude * std::sin(phase);
                        list.draw_line(prev_x, prev_y, cur_x, cur_y, {dr, dg, db, da}, thickness);
                        prev_x = cur_x;
                        prev_y = cur_y;
                    }
                } else { // solid
                    list.fill_rect({deco_x, line_y, deco_w, thickness}, {dr, dg, db, da});
                }
            };

            const float baseline_y = text_y + node.font_size;
        if (deco_bits & 1) {
                float underline_y = node.text_underline_position == 1
                    ? baseline_y + node.font_size * 0.25f
                    : baseline_y + 2.0f;
                if (node.text_underline_offset != 0.0f) {
                    underline_y += node.text_underline_offset;
                }
                draw_deco_line(underline_y);
            }
            if (deco_bits & 2) draw_deco_line(text_y);                      // overline
            if (deco_bits & 4) draw_deco_line(text_y + node.font_size * 0.5f); // line-through
        }
    }
    if (vertical_rotated) {
        list.pop_transform();
    }
    // Draw text-emphasis marks above/below each character
    if (node.text_emphasis_style != "none" && !node.text_emphasis_style.empty() &&
        !text_to_render.empty()) {
        // Determine emphasis mark character
        std::string mark;
        const std::string& es = node.text_emphasis_style;
        if (es == "dot" || es == "filled dot") mark = "\xE2\x80\xA2"; // U+2022 bullet
        else if (es == "circle" || es == "filled circle") mark = "\xE2\x97\x8F"; // U+25CF
        else if (es == "open dot" || es == "open circle") mark = "\xE2\x97\x8B"; // U+25CB
        else if (es == "double-circle" || es == "filled double-circle") mark = "\xE2\x97\x89"; // U+25C9
        else if (es == "triangle" || es == "filled triangle") mark = "\xE2\x96\xB2"; // U+25B2
        else if (es == "open triangle") mark = "\xE2\x96\xB3"; // U+25B3
        else if (es == "sesame" || es == "filled sesame") mark = "\xEF\xB8\xB0"; // U+FE30
        else if (es == "open sesame") mark = "\xEF\xB8\xB1"; // U+FE31
        else if (es.size() > 0 && es[0] != 'f' && es[0] != 'o') mark = es; // custom character

        if (!mark.empty()) {
            // Emphasis color: 0 means inherit text color
            uint8_t er = r, eg = g, eb = b, ea = a;
            if (node.text_emphasis_color != 0) {
                ea = (node.text_emphasis_color >> 24) & 0xFF;
                er = (node.text_emphasis_color >> 16) & 0xFF;
                eg = (node.text_emphasis_color >> 8) & 0xFF;
                eb = node.text_emphasis_color & 0xFF;
            }

            // Draw mark above each character (emphasis goes above for horizontal text)
            float mark_size = effective_font_size * 0.5f;
            float mark_y = abs_y - mark_size * 0.8f; // above the text
            float x_pos = abs_x;
            for (size_t i = 0; i < text_to_render.size(); i++) {
                if (text_to_render[i] != ' ') {
                    list.draw_text(mark, x_pos, mark_y, mark_size, {er, eg, eb, ea},
                                   node.font_family, node.font_weight, false, 0);
                }
                x_pos += char_w;
            }
        }
    }
}

void Painter::paint_outline(const clever::layout::LayoutNode& node, DisplayList& list,
                             float abs_x, float abs_y) {
    // Only paint if there is a visible outline
    if (node.outline_style == 0 || node.outline_width <= 0) return;

    const auto& geom = node.geometry;

    // Extract outline color from ARGB
    uint32_t oc = node.outline_color;
    uint8_t a = static_cast<uint8_t>((oc >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((oc >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((oc >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(oc & 0xFF);

    if (node.opacity < 1.0f) {
        a = static_cast<uint8_t>(a * node.opacity);
    }

    float ow = node.outline_width;
    // outline-offset adds spacing between border edge and outline; keep this path outward-only.
    float offset = std::max(0.0f, node.outline_offset);

    // Border box geometry
    float bw = geom.border_box_width();
    float bh = geom.border_box_height();

    // Inner edge of outline ring: border box expanded by outline-offset.
    float ix = abs_x - offset;
    float iy = abs_y - offset;
    float inner_w = bw + 2.0f * offset;
    float inner_h = bh + 2.0f * offset;

    // Outer edge of outline ring: inner edge expanded by outline-width.
    float ox = ix - ow;
    float oy = iy - ow;
    float outer_w = inner_w + 2.0f * ow;
    float outer_h = inner_h + 2.0f * ow;

    Color color = {r, g, b, a};

    auto draw_solid_outline = [&](const Color& c) {
        list.fill_rect({ox, oy, outer_w, ow}, c);                     // top
        list.fill_rect({ox, oy + outer_h - ow, outer_w, ow}, c);      // bottom
        list.fill_rect({ox, iy, ow, inner_h}, c);                     // left
        list.fill_rect({ix + inner_w, iy, ow, inner_h}, c);           // right
    };

    // Helper to draw a dashed edge
    auto draw_dashed_edge = [&](float ex, float ey, float ew, float eh, bool horizontal) {
        float dash_len = std::max(ow * 2.5f, 1.0f);
        float gap_len = std::max(ow * 1.5f, 1.0f);
        float pos = 0;
        float total = horizontal ? ew : eh;
        while (pos < total) {
            float seg = std::min(dash_len, total - pos);
            if (horizontal)
                list.fill_rect({ex + pos, ey, seg, eh}, color);
            else
                list.fill_rect({ex, ey + pos, ew, seg}, color);
            pos += dash_len + gap_len;
        }
    };

    // Helper to draw a dotted edge
    auto draw_dotted_edge = [&](float ex, float ey, float ew, float eh, bool horizontal) {
        float dot = std::max(ow * 1.2f, 1.0f);
        float gap = std::max(ow * 0.8f, 1.0f);
        float pos = 0;
        float total = horizontal ? ew : eh;
        while (pos < total) {
            if (horizontal)
                list.fill_rect({ex + pos, ey, dot, eh}, color);
            else
                list.fill_rect({ex, ey + pos, ew, dot}, color);
            pos += dot + gap;
        }
    };

    if (node.outline_style == 2) {
        // Dashed outline
        draw_dashed_edge(ox, oy, outer_w, ow, true);      // top
        draw_dashed_edge(ox, oy + outer_h - ow, outer_w, ow, true); // bottom
        draw_dashed_edge(ox, iy, ow, inner_h, false); // left
        draw_dashed_edge(ix + inner_w, iy, ow, inner_h, false); // right
    } else if (node.outline_style == 3) {
        // Dotted outline
        draw_dotted_edge(ox, oy, outer_w, ow, true);      // top
        draw_dotted_edge(ox, oy + outer_h - ow, outer_w, ow, true); // bottom
        draw_dotted_edge(ox, iy, ow, inner_h, false); // left
        draw_dotted_edge(ix + inner_w, iy, ow, inner_h, false); // right
    } else if (node.outline_style == 4) {
        // Double outline: two thinner outlines with gap between
        if (ow < 2.0f) {
            draw_solid_outline(color);
            return;
        }
        float band = std::max(ow / 3.0f, 1.0f);
        band = std::min(band, ow * 0.5f);

        // Outer band
        list.fill_rect({ox, oy, outer_w, band}, color);                    // top
        list.fill_rect({ox, oy + outer_h - band, outer_w, band}, color);   // bottom
        list.fill_rect({ox, oy + band, band, outer_h - 2.0f * band}, color); // left
        list.fill_rect({ox + outer_w - band, oy + band, band, outer_h - 2.0f * band}, color); // right

        // Inner band
        float iox = ix - band;
        float ioy = iy - band;
        float iow = inner_w + 2.0f * band;
        float ioh = inner_h + 2.0f * band;
        list.fill_rect({iox, ioy, iow, band}, color);                    // top
        list.fill_rect({iox, ioy + ioh - band, iow, band}, color);       // bottom
        list.fill_rect({iox, ioy + band, band, ioh - 2.0f * band}, color); // left
        list.fill_rect({iox + iow - band, ioy + band, band, ioh - 2.0f * band}, color); // right
    } else if (node.outline_style == 5 || node.outline_style == 6) {
        // Groove (5) or Ridge (6) — 3D effect simulating beveled edges
        // Groove: appears recessed (top/left darker, bottom/right lighter)
        // Ridge: appears raised (top/left lighter, bottom/right darker)
        float half = ow / 2.0f;
        float inner_half = ow - half;
        uint8_t dark_r = static_cast<uint8_t>(r * 0.4f);
        uint8_t dark_g = static_cast<uint8_t>(g * 0.4f);
        uint8_t dark_b = static_cast<uint8_t>(b * 0.4f);
        uint8_t light_r = static_cast<uint8_t>(std::min(255, r + (255 - r) / 2));
        uint8_t light_g = static_cast<uint8_t>(std::min(255, g + (255 - g) / 2));
        uint8_t light_b = static_cast<uint8_t>(std::min(255, b + (255 - b) / 2));
        Color dark = {dark_r, dark_g, dark_b, a};
        Color light = {light_r, light_g, light_b, a};

        // For groove: darker edges create inset appearance; for ridge: lighter edges create raised appearance
        Color tl_outer = (node.outline_style == 5) ? dark : light;   // top/left outer (groove=dark, ridge=light)
        Color br_outer = (node.outline_style == 5) ? light : dark;   // bottom/right outer (groove=light, ridge=dark)
        Color tl_inner = (node.outline_style == 5) ? light : dark;   // top/left inner (groove=light, ridge=dark)
        Color br_inner = (node.outline_style == 5) ? dark : light;   // bottom/right inner (groove=dark, ridge=light)

        // Outer half: creates outer beveled edge
        list.fill_rect({ox, oy, outer_w, half}, tl_outer);                  // top
        list.fill_rect({ox, oy + outer_h - half, outer_w, half}, br_outer); // bottom
        list.fill_rect({ox, oy + half, half, outer_h - 2.0f * half}, tl_outer);       // left
        list.fill_rect({ox + outer_w - half, oy + half, half, outer_h - 2.0f * half}, br_outer); // right

        // Inner half: creates inner beveled edge
        float iox = ix - inner_half;
        float ioy = iy - inner_half;
        float iow = inner_w + 2.0f * inner_half;
        float ioh = inner_h + 2.0f * inner_half;
        list.fill_rect({iox, ioy, iow, inner_half}, tl_inner);  // top
        list.fill_rect({iox, ioy + ioh - inner_half, iow, inner_half}, br_inner); // bottom
        list.fill_rect({iox, ioy + inner_half, inner_half, ioh - 2.0f * inner_half}, tl_inner);          // left
        list.fill_rect({iox + iow - inner_half, ioy + inner_half, inner_half, ioh - 2.0f * inner_half}, br_inner);       // right
    } else if (node.outline_style == 7 || node.outline_style == 8) {
        // Inset (7) or Outset (8) — uniform 3D-like effect
        uint8_t dark_r = static_cast<uint8_t>(r * 0.5f);
        uint8_t dark_g = static_cast<uint8_t>(g * 0.5f);
        uint8_t dark_b = static_cast<uint8_t>(b * 0.5f);
        uint8_t light_r = static_cast<uint8_t>(std::min(255, r + (255 - r) / 2));
        uint8_t light_g = static_cast<uint8_t>(std::min(255, g + (255 - g) / 2));
        uint8_t light_b = static_cast<uint8_t>(std::min(255, b + (255 - b) / 2));
        Color dark = {dark_r, dark_g, dark_b, a};
        Color light = {light_r, light_g, light_b, a};

        // Inset: top/left dark, bottom/right light
        // Outset: top/left light, bottom/right dark
        Color tl = (node.outline_style == 7) ? dark : light;
        Color br = (node.outline_style == 7) ? light : dark;

        list.fill_rect({ox, oy, outer_w, ow}, tl);              // top
        list.fill_rect({ox, oy + outer_h - ow, outer_w, ow}, br);    // bottom
        list.fill_rect({ox, iy, ow, inner_h}, tl);         // left
        list.fill_rect({ix + inner_w, iy, ow, inner_h}, br); // right
    } else {
        // Solid outline (default)
        draw_solid_outline(color);
    }
}

void Painter::paint_caret(const clever::layout::LayoutNode& node, DisplayList& list,
                           float abs_x, float abs_y) {
    // Draw a 1px-wide vertical caret line inside the content box using caret-color.
    // The caret is positioned at the left edge of the content area (after padding/border).
    const auto& geom = node.geometry;
    uint32_t cc = node.caret_color;
    uint8_t a = static_cast<uint8_t>((cc >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((cc >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((cc >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(cc & 0xFF);

    if (a == 0) return; // fully transparent, nothing to draw

    // Content box starts after border + padding
    float content_x = abs_x + geom.border.left + geom.padding.left;
    float content_y = abs_y + geom.border.top + geom.padding.top;
    float content_h = geom.height; // content height (excludes padding/border)

    // Caret is 1px wide, full content height, at left edge of content area
    float caret_width = 1.0f;
    list.fill_rect({content_x, content_y, caret_width, content_h}, {r, g, b, a});
}

void Painter::paint_text_input(const clever::layout::LayoutNode& node, DisplayList& list,
                               float abs_x, float abs_y) {
    if (!node.is_text_input) return;
    if (node.appearance == 1) return; // appearance: none — skip native rendering

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    Rect box_rect = {abs_x, abs_y, box_w, box_h};

    bool dark = (node.color_scheme == 2);

    // Background fill (white / dark background)
    Color bg_color = dark ? Color{0x1E, 0x1E, 0x1E, 0xFF} : Color{0xFF, 0xFF, 0xFF, 0xFF};
    if (node.background_color != 0x00000000 && node.background_color != 0xFF000000) {
        bg_color = Color::from_argb(node.background_color);
    }
    // Use a small border-radius for modern look (3px)
    float radius = node.border_radius > 0 ? node.border_radius : 3.0f;
    list.fill_rounded_rect(box_rect, bg_color, radius);

    // Draw inset-style border: top/left slightly darker, bottom/right slightly lighter
    // giving a subtle sunken appearance like native text fields
    Color border_outer = dark ? Color{0x44, 0x44, 0x44, 0xFF} : Color{0x8A, 0x8A, 0x8A, 0xFF};
    Color border_inner = dark ? Color{0x2E, 0x2E, 0x2E, 0xFF} : Color{0xC0, 0xC0, 0xC0, 0xFF};

    // If CSS border-color is explicitly set, use it instead
    if (node.border_color != 0 && node.border_color != 0xFF000000) {
        border_outer = Color::from_argb(node.border_color);
        border_inner = border_outer;
    }

    float bw_top    = geom.border.top    > 0 ? geom.border.top    : 1.0f;
    float bw_right  = geom.border.right  > 0 ? geom.border.right  : 1.0f;
    float bw_bottom = geom.border.bottom > 0 ? geom.border.bottom : 1.0f;
    float bw_left   = geom.border.left   > 0 ? geom.border.left   : 1.0f;

    // Top/left border segments (darker = sunken shadow effect)
    list.fill_rect({abs_x, abs_y, box_w, bw_top}, border_outer);
    list.fill_rect({abs_x, abs_y, bw_left, box_h}, border_outer);
    // Bottom/right border segments (lighter = sunken highlight)
    list.fill_rect({abs_x, abs_y + box_h - bw_bottom, box_w, bw_bottom}, border_inner);
    list.fill_rect({abs_x + box_w - bw_right, abs_y, bw_right, box_h}, border_inner);

    // The text child (value or placeholder) is painted via the normal child recursion.
    // This method only provides the native background + border decoration.
}

void Painter::paint_textarea(const clever::layout::LayoutNode& node, DisplayList& list,
                             float abs_x, float abs_y) {
    if (!node.is_textarea) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    Rect box_rect = {abs_x, abs_y, box_w, box_h};

    bool dark = (node.color_scheme == 2);

    // Background fill
    Color bg_color = dark ? Color{0x1E, 0x1E, 0x1E, 0xFF} : Color{0xFF, 0xFF, 0xFF, 0xFF};
    if (node.background_color != 0x00000000 && node.background_color != 0xFF000000) {
        bg_color = Color::from_argb(node.background_color);
    }
    float radius = node.border_radius > 0 ? node.border_radius : 3.0f;
    list.fill_rounded_rect(box_rect, bg_color, radius);

    // Inset border: top/left slightly darker to give sunken effect
    Color border_outer = dark ? Color{0x44, 0x44, 0x44, 0xFF} : Color{0x76, 0x76, 0x76, 0xFF};
    Color border_inner = dark ? Color{0x2E, 0x2E, 0x2E, 0xFF} : Color{0xAA, 0xAA, 0xAA, 0xFF};
    if (node.border_color != 0 && node.border_color != 0xFF000000) {
        border_outer = Color::from_argb(node.border_color);
        border_inner = border_outer;
    }

    float bw = 1.0f;
    list.fill_rect({abs_x,               abs_y,               box_w, bw   }, border_outer);
    list.fill_rect({abs_x,               abs_y,               bw,    box_h}, border_outer);
    list.fill_rect({abs_x,               abs_y + box_h - bw,  box_w, bw   }, border_inner);
    list.fill_rect({abs_x + box_w - bw,  abs_y,               bw,    box_h}, border_inner);

    // Resize handle: small diagonal dots in the bottom-right corner indicating resize
    float rh = 8.0f;
    float rx = abs_x + box_w - 1.0f;
    float ry = abs_y + box_h - 1.0f;
    Color handle_color = dark ? Color{0x44, 0x44, 0x44, 0xFF} : Color{0xAA, 0xAA, 0xAA, 0xFF};
    for (int i = 2; i <= static_cast<int>(rh); i += 3) {
        float fi = static_cast<float>(i);
        list.fill_rect({rx - fi, ry - 1.5f, 1.5f, 1.5f}, handle_color);
        list.fill_rect({rx - 1.5f, ry - fi, 1.5f, 1.5f}, handle_color);
    }

    // Scrollbar indicator on right edge when content likely overflows
    if (node.textarea_has_content && box_h > 30.0f) {
        float sb_w = 6.0f;
        float sb_x = abs_x + box_w - sb_w - 1.0f;
        float sb_y = abs_y + 1.0f;
        float sb_h = box_h - 2.0f;
        Color track_color = dark ? Color{0x2A, 0x2A, 0x2A, 0xFF} : Color{0xF0, 0xF0, 0xF0, 0xFF};
        list.fill_rect({sb_x, sb_y, sb_w, sb_h}, track_color);
        Color thumb_color = dark ? Color{0x55, 0x55, 0x55, 0xFF} : Color{0xC0, 0xC0, 0xC0, 0xFF};
        float thumb_h = std::max(20.0f, sb_h * 0.4f);
        list.fill_rounded_rect({sb_x + 1.0f, sb_y + 1.0f, sb_w - 2.0f, thumb_h - 2.0f},
                               thumb_color, 2.0f);
    }

    // The text content/placeholder child is painted via the normal child recursion.
}

void Painter::paint_button_input(const clever::layout::LayoutNode& node, DisplayList& list,
                                 float abs_x, float abs_y) {
    if (!node.is_button_input) return;
    if (node.appearance == 1) return; // appearance: none — skip native rendering

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    bool dark = (node.color_scheme == 2);

    // Use CSS background-color if set, otherwise system button gray
    Color bg_top, bg_bot;
    if (node.background_color != 0x00000000) {
        Color base = Color::from_argb(node.background_color);
        // Create a subtle vertical gradient: slightly lighter at top, base at bottom
        bg_top = dark
            ? Color{static_cast<uint8_t>(std::min(255, base.r + 20)),
                    static_cast<uint8_t>(std::min(255, base.g + 20)),
                    static_cast<uint8_t>(std::min(255, base.b + 20)),
                    base.a}
            : Color{static_cast<uint8_t>(std::min(255, base.r + 20)),
                    static_cast<uint8_t>(std::min(255, base.g + 20)),
                    static_cast<uint8_t>(std::min(255, base.b + 20)),
                    base.a};
        bg_bot = base;
    } else if (dark) {
        bg_top = Color{0x40, 0x40, 0x40, 0xFF};
        bg_bot = Color{0x2E, 0x2E, 0x2E, 0xFF};
    } else {
        bg_top = Color{0xF0, 0xF0, 0xF0, 0xFF};
        bg_bot = Color{0xD8, 0xD8, 0xD8, 0xFF};
    }

    float radius = node.border_radius > 0 ? node.border_radius : 4.0f;

    // Draw vertical gradient approximated as two halves
    float half_h = box_h / 2.0f;
    // Top half
    list.fill_rounded_rect({abs_x, abs_y, box_w, half_h + 1.0f}, bg_top,
                            radius, radius, 0.0f, 0.0f);
    // Bottom half
    list.fill_rounded_rect({abs_x, abs_y + half_h, box_w, box_h - half_h}, bg_bot,
                            0.0f, 0.0f, radius, radius);

    // Raised border: top/left lighter (highlight), bottom/right darker (shadow)
    Color border_tl = dark ? Color{0x66, 0x66, 0x66, 0xFF} : Color{0xC8, 0xC8, 0xC8, 0xFF};
    Color border_br = dark ? Color{0x22, 0x22, 0x22, 0xFF} : Color{0x88, 0x88, 0x88, 0xFF};

    // If CSS border-color is set, use it uniformly
    if (node.border_color != 0 && node.border_color != 0xFF000000) {
        border_tl = Color::from_argb(node.border_color);
        border_br = border_tl;
    }

    float bw_top    = geom.border.top    > 0 ? geom.border.top    : 1.0f;
    float bw_right  = geom.border.right  > 0 ? geom.border.right  : 1.0f;
    float bw_bottom = geom.border.bottom > 0 ? geom.border.bottom : 1.0f;
    float bw_left   = geom.border.left   > 0 ? geom.border.left   : 1.0f;

    // Top/left border (lighter — raised highlight)
    list.fill_rect({abs_x, abs_y, box_w, bw_top}, border_tl);
    list.fill_rect({abs_x, abs_y, bw_left, box_h}, border_tl);
    // Bottom/right border (darker — raised shadow)
    list.fill_rect({abs_x, abs_y + box_h - bw_bottom, box_w, bw_bottom}, border_br);
    list.fill_rect({abs_x + box_w - bw_right, abs_y, bw_right, box_h}, border_br);

    // The text child (button label) is painted via the normal child recursion.
    // This method only provides the native button background + border decoration.
}

void Painter::paint_range_input(const clever::layout::LayoutNode& node, DisplayList& list,
                                 float abs_x, float abs_y) {
    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    // Track dimensions
    float track_height = 4.0f;
    float track_y = abs_y + (box_h - track_height) / 2.0f;
    float thumb_radius = 8.0f;

    // Compute thumb position along the track
    int range = node.input_range_max - node.input_range_min;
    float ratio = (range > 0)
        ? static_cast<float>(node.input_range_value - node.input_range_min) / static_cast<float>(range)
        : 0.0f;
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    // Usable track width (inset by thumb radius so thumb doesn't overflow)
    float track_left = abs_x + thumb_radius;
    float track_right = abs_x + box_w - thumb_radius;
    float usable_width = track_right - track_left;
    float thumb_cx = track_left + usable_width * ratio;
    float thumb_cy = abs_y + box_h / 2.0f;

    // Colors: track background and accent — use dark theme if color-scheme: dark
    bool dark_range = (node.color_scheme == 2);
    Color track_bg = dark_range ? Color{0x33, 0x33, 0x33, 0xFF} : Color{0xDD, 0xDD, 0xDD, 0xFF};
    uint32_t accent_argb = node.accent_color != 0 ? node.accent_color : 0xFF007AFFu;
    Color accent_fill = Color::from_argb(accent_argb);
    // Use a darker shade for thumb emphasis by default.
    Color thumb_fill = Color::from_argb(darken_color(accent_argb, 24));

    // Draw track background (full width, rounded)
    float track_radius = track_height / 2.0f;
    list.fill_rounded_rect({abs_x, track_y, box_w, track_height}, track_bg, track_radius);

    // Draw filled portion of track (from left edge to thumb center)
    float filled_width = thumb_cx - abs_x;
    if (filled_width > 0) {
        list.fill_rounded_rect({abs_x, track_y, filled_width, track_height}, accent_fill, track_radius);
    }

    // Draw thumb as a circle (rounded rect with radius = half its size)
    float thumb_size = thumb_radius * 2.0f;
    Rect thumb_rect = {thumb_cx - thumb_radius, thumb_cy - thumb_radius, thumb_size, thumb_size};
    list.fill_rounded_rect(thumb_rect, thumb_fill, thumb_radius);
}

void Painter::paint_color_input(const clever::layout::LayoutNode& node, DisplayList& list,
                                 float abs_x, float abs_y) {
    if (!node.is_color_input) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    // Draw rounded rectangle border (#767676)
    float border_radius = 3.0f;
    Color border_color = {0x76, 0x76, 0x76, 0xFF};
    Rect outer_rect = {abs_x, abs_y, box_w, box_h};
    list.fill_rounded_rect(outer_rect, border_color, border_radius);

    // Fill the interior with the color swatch, inset by ~3px from the border
    float inset = 3.0f;
    Rect swatch_rect = {abs_x + inset, abs_y + inset,
                        box_w - inset * 2.0f, box_h - inset * 2.0f};

    // Extract ARGB color from color_input_value
    uint32_t cv = node.color_input_value;
    uint8_t ca = static_cast<uint8_t>((cv >> 24) & 0xFF);
    uint8_t cr = static_cast<uint8_t>((cv >> 16) & 0xFF);
    uint8_t cg = static_cast<uint8_t>((cv >> 8) & 0xFF);
    uint8_t cb = static_cast<uint8_t>(cv & 0xFF);

    Color swatch_color = {cr, cg, cb, ca};
    float inner_radius = std::max(0.0f, border_radius - 1.0f);
    list.fill_rounded_rect(swatch_rect, swatch_color, inner_radius);
}

void Painter::paint_checkbox(const clever::layout::LayoutNode& node, DisplayList& list,
                              float abs_x, float abs_y) {
    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    float size = std::min(box_w, box_h);

    // Center the checkbox in its box
    float cx = abs_x + (box_w - size) / 2.0f;
    float cy = abs_y + (box_h - size) / 2.0f;

    uint32_t ac = node.accent_color != 0 ? node.accent_color : 0xFF007AFFu;
    Color accent = Color::from_argb(ac);
    Color accent_hover = Color::from_argb(lighten_color(ac, 30));
    Color accent_active = Color::from_argb(darken_color(ac, 28));
    bool use_hover = false;
    bool use_active = false;
    Color fill = accent;
    if (use_active) fill = accent_active;
    else if (use_hover) fill = accent_hover;
    Color mark = is_dark_color(ac) ? Color{0xFF, 0xFF, 0xFF, 0xFF} : Color{0x00, 0x00, 0x00, 0xFF};

    if (node.is_checked) {
        // Filled rounded rect with accent color
        list.fill_rounded_rect({cx, cy, size, size}, fill, 2.0f);
        // Draw check mark with enough contrast — simplified as two lines
        float inset = size * 0.2f;
        // Check mark: short line from lower-left to bottom-center, then long line to upper-right
        float x1 = cx + inset;
        float y1 = cy + size * 0.5f;
        float x2 = cx + size * 0.4f;
        float y2 = cy + size - inset;
        float x3 = cx + size - inset;
        float y3 = cy + inset;
        // Approximate with small rects along the check mark path
        float stroke = std::max(1.5f, size * 0.12f);
        // Left leg: (x1,y1) to (x2,y2)
        int steps = static_cast<int>(size * 0.5f);
        for (int i = 0; i <= steps; i++) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float px = x1 + (x2 - x1) * t;
            float py = y1 + (y2 - y1) * t;
            list.fill_rect({px - stroke / 2, py - stroke / 2, stroke, stroke}, mark);
        }
        // Right leg: (x2,y2) to (x3,y3)
        steps = static_cast<int>(size * 0.8f);
        for (int i = 0; i <= steps; i++) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float px = x2 + (x3 - x2) * t;
            float py = y2 + (y3 - y2) * t;
            list.fill_rect({px - stroke / 2, py - stroke / 2, stroke, stroke}, mark);
        }
    } else {
        // Unchecked: border only — use dark theme if color-scheme: dark
        bool dark = (node.color_scheme == 2);
        Color border_col = dark ? Color{0x55, 0x55, 0x55, 0xFF} : Color{0x76, 0x76, 0x76, 0xFF};
        list.fill_rounded_rect({cx, cy, size, size}, border_col, 2.0f);
        float inset = 1.5f;
        Color bg = dark ? Color{0x1E, 0x1E, 0x1E, 0xFF} : Color{0xFF, 0xFF, 0xFF, 0xFF};
        list.fill_rounded_rect({cx + inset, cy + inset, size - 2 * inset, size - 2 * inset}, bg, 1.0f);
    }
}

void Painter::paint_radio(const clever::layout::LayoutNode& node, DisplayList& list,
                           float abs_x, float abs_y) {
    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    float size = std::min(box_w, box_h);
    float radius = size / 2.0f;

    // Center
    float cx = abs_x + box_w / 2.0f;
    float cy = abs_y + box_h / 2.0f;

    uint32_t ac = node.accent_color != 0 ? node.accent_color : 0xFF007AFFu;
    Color accent = Color::from_argb(ac);
    Color accent_hover = Color::from_argb(lighten_color(ac, 30));
    Color accent_active = Color::from_argb(darken_color(ac, 28));
    bool use_hover = false;
    bool use_active = false;
    Color outer_fill = accent;
    if (use_active) outer_fill = accent_active;
    else if (use_hover) outer_fill = accent_hover;
    Color dot = is_dark_color(ac) ? Color{0xFF, 0xFF, 0xFF, 0xFF} : Color{0x00, 0x00, 0x00, 0xFF};

    if (node.is_checked) {
        // Outer circle with accent color
        Rect outer = {cx - radius, cy - radius, size, size};
        list.fill_rounded_rect(outer, outer_fill, radius);
        // Inner white dot
        float inner_r = radius * 0.4f;
        Rect inner = {cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2};
        list.fill_rounded_rect(inner, dot, inner_r);
    } else {
        // Unchecked: circle border — use dark theme if color-scheme: dark
        bool dark = (node.color_scheme == 2);
        Color border_col = dark ? Color{0x55, 0x55, 0x55, 0xFF} : Color{0x76, 0x76, 0x76, 0xFF};
        Rect outer = {cx - radius, cy - radius, size, size};
        list.fill_rounded_rect(outer, border_col, radius);
        float inset = 1.5f;
        Rect inner = {cx - radius + inset, cy - radius + inset, size - 2 * inset, size - 2 * inset};
        Color bg = dark ? Color{0x1E, 0x1E, 0x1E, 0xFF} : Color{0xFF, 0xFF, 0xFF, 0xFF};
        list.fill_rounded_rect(inner, bg, radius - inset);
    }
}

void Painter::paint_svg_shape(const clever::layout::LayoutNode& node, DisplayList& list,
                               float abs_x, float abs_y) {
    if (!node.is_svg || node.svg_type == 0) return;

    // Compute viewBox scale from nearest SVG container ancestor
    float vb_sx = 1.0f, vb_sy = 1.0f, vb_ox = 0, vb_oy = 0;
    {
        const clever::layout::LayoutNode* p = node.parent;
        while (p) {
            if (p->is_svg && p->svg_type == 0 && p->svg_has_viewbox) {
                float vp_w = p->geometry.width > 0 ? p->geometry.width : p->specified_width;
                float vp_h = p->geometry.height > 0 ? p->geometry.height : p->specified_height;
                if (p->svg_viewbox_w > 0 && p->svg_viewbox_h > 0) {
                    vb_sx = vp_w / p->svg_viewbox_w;
                    vb_sy = vp_h / p->svg_viewbox_h;
                    vb_ox = -p->svg_viewbox_x * vb_sx;
                    vb_oy = -p->svg_viewbox_y * vb_sy;
                }
                break;
            }
            p = p->parent;
        }
    }

    // Accumulate group scale transforms from parent <g> elements
    {
        const clever::layout::LayoutNode* p = node.parent;
        while (p) {
            if (p->is_svg_group) {
                vb_sx *= p->svg_transform_sx;
                vb_sy *= p->svg_transform_sy;
            }
            if (p->is_svg && p->svg_type == 0) break; // stop at SVG root
            p = p->parent;
        }
    }

    // Extract fill color from svg_fill_color (not background_color which is for HTML)
    Color fill_color = {0, 0, 0, 0}; // default: no fill
    if (!node.svg_fill_none) {
        uint32_t bg = node.svg_fill_color;
        fill_color = Color::from_argb(bg);
        if (node.svg_fill_opacity < 1.0f)
            fill_color.a = static_cast<uint8_t>(fill_color.a * node.svg_fill_opacity);
    }

    // Check for gradient fill reference
    const clever::layout::LayoutNode::SvgGradient* fill_gradient = nullptr;
    if (!node.svg_fill_gradient_id.empty()) {
        // Walk up to find SVG root with gradient defs
        const clever::layout::LayoutNode* p = &node;
        while (p) {
            auto it = p->svg_gradient_defs.find(node.svg_fill_gradient_id);
            if (it != p->svg_gradient_defs.end()) {
                fill_gradient = &it->second;
                break;
            }
            p = p->parent;
        }
    }

    // Helper: fill a rect with either solid color or gradient
    auto fill_shape_rect = [&](const Rect& rect) {
        if (fill_gradient && fill_gradient->stops.size() >= 2) {
            if (!fill_gradient->is_radial) {
                // Linear gradient: compute angle from (x1,y1) to (x2,y2)
                float dx = fill_gradient->x2 - fill_gradient->x1;
                float dy = fill_gradient->y2 - fill_gradient->y1;
                float angle_deg = std::atan2(dy, dx) * 180.0f / static_cast<float>(M_PI) + 90.0f;
                list.fill_gradient(rect, angle_deg, fill_gradient->stops, 0, 1, 0);
            } else {
                // Radial gradient
                list.fill_gradient(rect, 0, fill_gradient->stops, 0, 2, 0);
            }
        } else if (fill_color.a > 0) {
            list.fill_rect(rect, fill_color);
        }
    };

    // Extract stroke color from svg_stroke_color (not border_color which is for HTML)
    Color stroke_color = {0, 0, 0, 0}; // default: no stroke
    if (!node.svg_stroke_none) {
        uint32_t sc = node.svg_stroke_color;
        stroke_color = Color::from_argb(sc);
        if (node.svg_stroke_opacity < 1.0f)
            stroke_color.a = static_cast<uint8_t>(stroke_color.a * node.svg_stroke_opacity);
    }

    // CSS paint-order: determines fill/stroke rendering order
    // Default SVG order: fill → stroke → markers
    // "stroke" or "stroke fill" means stroke first, then fill on top
    bool stroke_first = false;
    if (!node.paint_order.empty() && node.paint_order != "normal") {
        auto po = node.paint_order;
        auto fill_pos = po.find("fill");
        auto stroke_pos = po.find("stroke");
        if (stroke_pos != std::string::npos && (fill_pos == std::string::npos || stroke_pos < fill_pos)) {
            stroke_first = true;
        }
    }

    const auto& attrs = node.svg_attrs;
    const auto& dasharray = node.svg_stroke_dasharray;
    float dashoffset = node.svg_stroke_dashoffset;

    // Helper: draw a line with optional dash pattern
    auto draw_dashed_line = [&](float x0, float y0, float x1, float y1,
                                Color color, float sw) {
        if (dasharray.empty()) {
            list.draw_line(x0, y0, x1, y1, color, sw);
            return;
        }
        // Compute total dash pattern length
        float total_dash = 0;
        for (float d : dasharray) total_dash += d;
        if (total_dash <= 0) { list.draw_line(x0, y0, x1, y1, color, sw); return; }

        float dx = x1 - x0, dy = y1 - y0;
        float seg_len = std::sqrt(dx * dx + dy * dy);
        if (seg_len <= 0) return;
        float ux = dx / seg_len, uy = dy / seg_len;

        // Apply dashoffset
        float pos = -std::fmod(dashoffset, total_dash);
        if (pos < 0) pos += total_dash;

        size_t dash_idx = 0;
        bool drawing = true; // odd indices are gaps
        while (pos < seg_len) {
            float dash_len = dasharray[dash_idx % dasharray.size()];
            float end_pos = std::min(pos + dash_len, seg_len);
            if (drawing && end_pos > std::max(pos, 0.0f)) {
                float start = std::max(pos, 0.0f);
                list.draw_line(x0 + start * ux, y0 + start * uy,
                               x0 + end_pos * ux, y0 + end_pos * uy,
                               color, sw);
            }
            pos = end_pos;
            dash_idx++;
            drawing = !drawing;
        }
    };

    switch (node.svg_type) {
        case 1: { // rect
            if (attrs.size() < 5) break;
            float rx = attrs[0] * vb_sx + vb_ox;
            float ry = attrs[1] * vb_sy + vb_oy;
            float rw = attrs[2] * vb_sx;
            float rh = attrs[3] * vb_sy;
            float sw = attrs[4] * std::min(vb_sx, vb_sy);

            Rect rect = {abs_x + rx, abs_y + ry, rw, rh};
            auto do_fill = [&]() { fill_shape_rect(rect); };
            auto do_stroke = [&]() {
                if (stroke_color.a > 0 && sw > 0)
                    list.draw_border(rect, stroke_color, sw, sw, sw, sw);
            };
            if (stroke_first) { do_stroke(); do_fill(); }
            else { do_fill(); do_stroke(); }
            break;
        }
        case 2: { // circle
            if (attrs.size() < 4) break;
            float cx = attrs[0] * vb_sx + vb_ox;
            float cy = attrs[1] * vb_sy + vb_oy;
            float r_x = attrs[2] * vb_sx;
            float r_y = attrs[2] * vb_sy;
            float sw = attrs[3] * std::min(vb_sx, vb_sy);

            // Apply fill (solid or gradient)
            if (fill_gradient && fill_gradient->stops.size() >= 2) {
                // Gradient fill: create bounding rect for gradient
                Rect grad_rect = {abs_x + cx - r_x, abs_y + cy - r_y, r_x * 2, r_y * 2};
                if (!fill_gradient->is_radial) {
                    // Linear gradient
                    float dx = fill_gradient->x2 - fill_gradient->x1;
                    float dy = fill_gradient->y2 - fill_gradient->y1;
                    float angle_deg = std::atan2(dy, dx) * 180.0f / static_cast<float>(M_PI) + 90.0f;
                    list.fill_gradient(grad_rect, angle_deg, fill_gradient->stops, 0, 1, 0);
                } else {
                    // Radial gradient
                    list.fill_gradient(grad_rect, 0, fill_gradient->stops, 0, 2, 0);
                }
            } else if (fill_color.a > 0) {
                // Solid fill: use draw_ellipse with both fill and stroke
                list.draw_ellipse(abs_x + cx, abs_y + cy, r_x, r_y, fill_color, {0, 0, 0, 0}, 0);
            }

            // Apply stroke on top if present
            if (stroke_color.a > 0 && sw > 0) {
                list.draw_ellipse(abs_x + cx, abs_y + cy, r_x, r_y, {0, 0, 0, 0}, stroke_color, sw);
            }
            break;
        }
        case 3: { // ellipse
            if (attrs.size() < 5) break;
            float cx = attrs[0] * vb_sx + vb_ox;
            float cy = attrs[1] * vb_sy + vb_oy;
            float erx = attrs[2] * vb_sx;
            float ery = attrs[3] * vb_sy;
            float sw = attrs[4] * std::min(vb_sx, vb_sy);

            // Apply fill (solid or gradient)
            if (fill_gradient && fill_gradient->stops.size() >= 2) {
                // Gradient fill: create bounding rect for gradient
                Rect grad_rect = {abs_x + cx - erx, abs_y + cy - ery, erx * 2, ery * 2};
                if (!fill_gradient->is_radial) {
                    // Linear gradient
                    float dx = fill_gradient->x2 - fill_gradient->x1;
                    float dy = fill_gradient->y2 - fill_gradient->y1;
                    float angle_deg = std::atan2(dy, dx) * 180.0f / static_cast<float>(M_PI) + 90.0f;
                    list.fill_gradient(grad_rect, angle_deg, fill_gradient->stops, 0, 1, 0);
                } else {
                    // Radial gradient
                    list.fill_gradient(grad_rect, 0, fill_gradient->stops, 0, 2, 0);
                }
            } else if (fill_color.a > 0) {
                // Solid fill: use draw_ellipse with both fill and stroke
                list.draw_ellipse(abs_x + cx, abs_y + cy, erx, ery, fill_color, {0, 0, 0, 0}, 0);
            }

            // Apply stroke on top if present
            if (stroke_color.a > 0 && sw > 0) {
                list.draw_ellipse(abs_x + cx, abs_y + cy, erx, ery, {0, 0, 0, 0}, stroke_color, sw);
            }
            break;
        }
        case 4: { // line
            if (attrs.size() < 5) break;
            float x1 = attrs[0] * vb_sx + vb_ox;
            float y1 = attrs[1] * vb_sy + vb_oy;
            float x2 = attrs[2] * vb_sx + vb_ox;
            float y2 = attrs[3] * vb_sy + vb_oy;
            float sw = attrs[4] * std::min(vb_sx, vb_sy);

            if (stroke_color.a > 0 && sw > 0) {
                draw_dashed_line(abs_x + x1, abs_y + y1,
                                 abs_x + x2, abs_y + y2,
                                 stroke_color, sw);
            }
            break;
        }
        case 5: { // path — supports M,L,H,V,C,S,Q,T,A,Z (and lowercase)
            if (node.svg_path_d.empty()) break;
            float stroke_w = (attrs.size() >= 1) ? attrs[0] * std::min(vb_sx, vb_sy) : 1.0f;

            // Collect vertices for scanline fill
            // Each subpath is a separate contour; all contours contribute to fill.
            std::vector<std::vector<std::pair<float,float>>> subpaths;
            std::vector<std::pair<float,float>> current_subpath;
            auto add_fill_pt = [&](float x, float y) {
                current_subpath.push_back({x, y});
            };

            // --- SVG path tokenizer: handles commas, optional whitespace, negative numbers ---
            const std::string& d = node.svg_path_d;
            size_t pos = 0;
            auto skip_ws_comma = [&]() {
                while (pos < d.size() && (d[pos] == ' ' || d[pos] == '\t' ||
                       d[pos] == '\n' || d[pos] == '\r' || d[pos] == ','))
                    ++pos;
            };
            auto peek_number = [&]() -> bool {
                skip_ws_comma();
                if (pos >= d.size()) return false;
                char c = d[pos];
                return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
            };
            auto read_float = [&](float& out) -> bool {
                skip_ws_comma();
                if (pos >= d.size()) return false;
                size_t start = pos;
                if (d[pos] == '-' || d[pos] == '+') ++pos;
                bool has_dot = false;
                while (pos < d.size()) {
                    if (d[pos] >= '0' && d[pos] <= '9') { ++pos; continue; }
                    if (d[pos] == '.' && !has_dot) { has_dot = true; ++pos; continue; }
                    break;
                }
                // Handle exponent (e.g. 1e-5)
                if (pos < d.size() && (d[pos] == 'e' || d[pos] == 'E')) {
                    ++pos;
                    if (pos < d.size() && (d[pos] == '-' || d[pos] == '+')) ++pos;
                    while (pos < d.size() && d[pos] >= '0' && d[pos] <= '9') ++pos;
                }
                if (pos == start) return false;
                try { out = std::stof(d.substr(start, pos - start)); } catch (...) { return false; }
                return true;
            };
            auto read_flag = [&](int& out) -> bool {
                skip_ws_comma();
                if (pos >= d.size()) return false;
                if (d[pos] == '0' || d[pos] == '1') {
                    out = d[pos] - '0';
                    ++pos;
                    return true;
                }
                return false;
            };
            auto read_cmd = [&](char& out) -> bool {
                skip_ws_comma();
                if (pos >= d.size()) return false;
                char c = d[pos];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    out = c;
                    ++pos;
                    return true;
                }
                return false;
            };

            // Collect stroke segments for deferred drawing (fill must come first)
            struct StrokeSeg { float x0, y0, x1, y1; };
            std::vector<StrokeSeg> stroke_segs;

            // Helper: record a line segment for both fill and stroke
            // Applies viewBox transform to coordinates
            auto draw_seg = [&](float x0, float y0, float x1, float y1) {
                float tx0 = x0 * vb_sx + vb_ox, ty0 = y0 * vb_sy + vb_oy;
                float tx1 = x1 * vb_sx + vb_ox, ty1 = y1 * vb_sy + vb_oy;
                add_fill_pt(tx1, ty1);
                stroke_segs.push_back({tx0, ty0, tx1, ty1});
            };

            // Flatten cubic Bezier (p0, p1, p2, p3) into line segments
            constexpr int CURVE_SEGMENTS = 20;
            auto flatten_cubic = [&](float p0x, float p0y, float p1x, float p1y,
                                     float p2x, float p2y, float p3x, float p3y) {
                float px = p0x, py = p0y;
                for (int i = 1; i <= CURVE_SEGMENTS; ++i) {
                    float t = static_cast<float>(i) / CURVE_SEGMENTS;
                    float u = 1.0f - t;
                    float nx = u*u*u*p0x + 3*u*u*t*p1x + 3*u*t*t*p2x + t*t*t*p3x;
                    float ny = u*u*u*p0y + 3*u*u*t*p1y + 3*u*t*t*p2y + t*t*t*p3y;
                    draw_seg(px, py, nx, ny);
                    px = nx; py = ny;
                }
            };

            // Flatten quadratic Bezier (p0, p1, p2)
            auto flatten_quad = [&](float p0x, float p0y, float p1x, float p1y,
                                    float p2x, float p2y) {
                float px = p0x, py = p0y;
                for (int i = 1; i <= CURVE_SEGMENTS; ++i) {
                    float t = static_cast<float>(i) / CURVE_SEGMENTS;
                    float u = 1.0f - t;
                    float nx = u*u*p0x + 2*u*t*p1x + t*t*p2x;
                    float ny = u*u*p0y + 2*u*t*p1y + t*t*p2y;
                    draw_seg(px, py, nx, ny);
                    px = nx; py = ny;
                }
            };

            // Arc helpers
            auto deg2rad = [](float dg) -> float { return dg * 3.14159265358979f / 180.0f; };
            // Flatten elliptical arc (SVG endpoint-to-center parameterization)
            auto flatten_arc = [&](float ax0, float ay0,
                                   float rx, float ry, float phi,
                                   int large_arc, int sweep,
                                   float ax1, float ay1) {
                if (rx == 0 || ry == 0) { draw_seg(ax0, ay0, ax1, ay1); return; }
                rx = std::fabs(rx); ry = std::fabs(ry);
                float cos_phi = std::cos(deg2rad(phi));
                float sin_phi = std::sin(deg2rad(phi));

                // Step 1: compute (x1', y1')
                float dx = (ax0 - ax1) / 2.0f;
                float dy = (ay0 - ay1) / 2.0f;
                float x1p = cos_phi * dx + sin_phi * dy;
                float y1p = -sin_phi * dx + cos_phi * dy;

                // Step 2: ensure radii are large enough
                float x1p2 = x1p * x1p, y1p2 = y1p * y1p;
                float rx2 = rx * rx, ry2 = ry * ry;
                float lambda = x1p2 / rx2 + y1p2 / ry2;
                if (lambda > 1.0f) {
                    float sl = std::sqrt(lambda);
                    rx *= sl; ry *= sl;
                    rx2 = rx * rx; ry2 = ry * ry;
                }

                // Step 3: compute center (cx', cy')
                float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
                float den = rx2 * y1p2 + ry2 * x1p2;
                float sq = (den > 0) ? std::sqrt(std::max(0.0f, num / den)) : 0.0f;
                if (large_arc == sweep) sq = -sq;
                float cxp = sq * (rx * y1p / ry);
                float cyp = sq * (-(ry * x1p / rx));

                // Step 4: compute center (cx, cy)
                float acx = cos_phi * cxp - sin_phi * cyp + (ax0 + ax1) / 2.0f;
                float acy = sin_phi * cxp + cos_phi * cyp + (ay0 + ay1) / 2.0f;

                // Step 5: compute theta1, dtheta using atan2
                auto angle_between = [](float ux, float uy, float vx, float vy) -> float {
                    float dot = ux * vx + uy * vy;
                    float len = std::sqrt((ux*ux + uy*uy) * (vx*vx + vy*vy));
                    if (len == 0) return 0;
                    float cos_a = std::max(-1.0f, std::min(1.0f, dot / len));
                    float a = std::acos(cos_a);
                    if (ux * vy - uy * vx < 0) a = -a;
                    return a;
                };
                float theta1 = angle_between(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
                float dtheta = angle_between((x1p - cxp) / rx, (y1p - cyp) / ry,
                                             (-x1p - cxp) / rx, (-y1p - cyp) / ry);
                // Ensure correct sweep direction
                if (sweep == 0 && dtheta > 0) dtheta -= 2.0f * 3.14159265358979f;
                if (sweep == 1 && dtheta < 0) dtheta += 2.0f * 3.14159265358979f;

                // Sample points along the arc
                int n_segs = std::max(4, static_cast<int>(std::fabs(dtheta) / (3.14159265358979f / 10.0f)));
                float ppx = ax0, ppy = ay0;
                for (int i = 1; i <= n_segs; ++i) {
                    float t = theta1 + dtheta * static_cast<float>(i) / n_segs;
                    float ex = cos_phi * rx * std::cos(t) - sin_phi * ry * std::sin(t) + acx;
                    float ey = sin_phi * rx * std::cos(t) + cos_phi * ry * std::sin(t) + acy;
                    draw_seg(ppx, ppy, ex, ey);
                    ppx = ex; ppy = ey;
                }
            };

            float start_x = 0, start_y = 0;
            float cur_x = 0, cur_y = 0;
            // Control point tracking for smooth curves (S/T commands)
            float last_cp2_x = 0, last_cp2_y = 0; // last cubic control point 2
            float last_qp_x = 0, last_qp_y = 0;   // last quadratic control point
            char last_cmd = 0;

            char cmd = 0;
            while (read_cmd(cmd)) {
                // Process the command, with implicit repeats for coordinate pairs
                while (true) {
                    if (cmd == 'M' || cmd == 'm') {
                        float x, y;
                        if (!read_float(x) || !read_float(y)) break;
                        if (cmd == 'm') { x += cur_x; y += cur_y; }
                        // Start new subpath for fill
                        if (!current_subpath.empty())
                            subpaths.push_back(std::move(current_subpath));
                        current_subpath.clear();
                        add_fill_pt(x * vb_sx + vb_ox, y * vb_sy + vb_oy);
                        start_x = cur_x = x;
                        start_y = cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        // Subsequent coordinates after M are treated as L
                        if (peek_number()) {
                            cmd = (cmd == 'M') ? 'L' : 'l';
                            continue;
                        }
                        break;
                    } else if (cmd == 'L' || cmd == 'l') {
                        float x, y;
                        if (!read_float(x) || !read_float(y)) break;
                        if (cmd == 'l') { x += cur_x; y += cur_y; }
                        draw_seg(cur_x, cur_y, x, y);
                        cur_x = x; cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'H' || cmd == 'h') {
                        float x;
                        if (!read_float(x)) break;
                        if (cmd == 'h') x += cur_x;
                        draw_seg(cur_x, cur_y, x, cur_y);
                        cur_x = x;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'V' || cmd == 'v') {
                        float y;
                        if (!read_float(y)) break;
                        if (cmd == 'v') y += cur_y;
                        draw_seg(cur_x, cur_y, cur_x, y);
                        cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'C' || cmd == 'c') {
                        float x1, y1, x2, y2, x, y;
                        if (!read_float(x1) || !read_float(y1) ||
                            !read_float(x2) || !read_float(y2) ||
                            !read_float(x) || !read_float(y)) break;
                        if (cmd == 'c') {
                            x1 += cur_x; y1 += cur_y;
                            x2 += cur_x; y2 += cur_y;
                            x += cur_x; y += cur_y;
                        }
                        flatten_cubic(cur_x, cur_y, x1, y1, x2, y2, x, y);
                        last_cp2_x = x2; last_cp2_y = y2;
                        cur_x = x; cur_y = y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'S' || cmd == 's') {
                        float x2, y2, x, y;
                        if (!read_float(x2) || !read_float(y2) ||
                            !read_float(x) || !read_float(y)) break;
                        if (cmd == 's') {
                            x2 += cur_x; y2 += cur_y;
                            x += cur_x; y += cur_y;
                        }
                        // Reflect last cubic control point
                        float x1 = cur_x, y1 = cur_y;
                        if (last_cmd == 'C' || last_cmd == 'c' ||
                            last_cmd == 'S' || last_cmd == 's') {
                            x1 = 2 * cur_x - last_cp2_x;
                            y1 = 2 * cur_y - last_cp2_y;
                        }
                        flatten_cubic(cur_x, cur_y, x1, y1, x2, y2, x, y);
                        last_cp2_x = x2; last_cp2_y = y2;
                        cur_x = x; cur_y = y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'Q' || cmd == 'q') {
                        float x1, y1, x, y;
                        if (!read_float(x1) || !read_float(y1) ||
                            !read_float(x) || !read_float(y)) break;
                        if (cmd == 'q') {
                            x1 += cur_x; y1 += cur_y;
                            x += cur_x; y += cur_y;
                        }
                        flatten_quad(cur_x, cur_y, x1, y1, x, y);
                        last_qp_x = x1; last_qp_y = y1;
                        cur_x = x; cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'T' || cmd == 't') {
                        float x, y;
                        if (!read_float(x) || !read_float(y)) break;
                        if (cmd == 't') { x += cur_x; y += cur_y; }
                        // Reflect last quadratic control point
                        float x1 = cur_x, y1 = cur_y;
                        if (last_cmd == 'Q' || last_cmd == 'q' ||
                            last_cmd == 'T' || last_cmd == 't') {
                            x1 = 2 * cur_x - last_qp_x;
                            y1 = 2 * cur_y - last_qp_y;
                        }
                        flatten_quad(cur_x, cur_y, x1, y1, x, y);
                        last_qp_x = x1; last_qp_y = y1;
                        cur_x = x; cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'A' || cmd == 'a') {
                        float arx, ary, rotation, x, y;
                        int large_arc, sweep;
                        if (!read_float(arx) || !read_float(ary) ||
                            !read_float(rotation) ||
                            !read_flag(large_arc) || !read_flag(sweep) ||
                            !read_float(x) || !read_float(y)) break;
                        if (cmd == 'a') { x += cur_x; y += cur_y; }
                        flatten_arc(cur_x, cur_y, arx, ary, rotation,
                                    large_arc, sweep, x, y);
                        cur_x = x; cur_y = y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        if (peek_number()) continue;
                        break;
                    } else if (cmd == 'Z' || cmd == 'z') {
                        draw_seg(cur_x, cur_y, start_x, start_y);
                        cur_x = start_x;
                        cur_y = start_y;
                        last_cp2_x = cur_x; last_cp2_y = cur_y;
                        last_qp_x = cur_x; last_qp_y = cur_y;
                        last_cmd = cmd;
                        break;
                    } else {
                        // Unknown command — skip
                        break;
                    }
                }
            }

            // Finalize last subpath
            if (!current_subpath.empty())
                subpaths.push_back(std::move(current_subpath));

            // --- Phase 1: Fill (SVG default: fill painted before stroke) ---
            if (!node.svg_fill_none && fill_color.a > 0 && !subpaths.empty()) {
                // Combine all subpath edges for even-odd scanline fill
                // Collect all edges from all subpaths
                struct Edge { float x0, y0, x1, y1; };
                std::vector<Edge> edges;
                for (const auto& sp : subpaths) {
                    if (sp.size() < 3) continue;
                    for (size_t i = 0; i < sp.size(); ++i) {
                        size_t j = (i + 1) % sp.size();
                        edges.push_back({sp[i].first, sp[i].second,
                                        sp[j].first, sp[j].second});
                    }
                }

                if (!edges.empty()) {
                    // Find bounding box
                    float min_y = edges[0].y0, max_y = edges[0].y0;
                    for (const auto& e : edges) {
                        min_y = std::min({min_y, e.y0, e.y1});
                        max_y = std::max({max_y, e.y0, e.y1});
                    }
                    int iy_min = static_cast<int>(std::floor(min_y));
                    int iy_max = static_cast<int>(std::ceil(max_y));

                    // Scanline fill (even-odd rule)
                    for (int iy = iy_min; iy <= iy_max; ++iy) {
                        float scan_y = static_cast<float>(iy) + 0.5f;
                        std::vector<float> x_intersections;
                        for (const auto& e : edges) {
                            float y0 = e.y0, y1 = e.y1;
                            if ((y0 <= scan_y && y1 > scan_y) ||
                                (y1 <= scan_y && y0 > scan_y)) {
                                float t = (scan_y - y0) / (y1 - y0);
                                float xi = e.x0 + t * (e.x1 - e.x0);
                                x_intersections.push_back(xi);
                            }
                        }
                        std::sort(x_intersections.begin(), x_intersections.end());
                        for (size_t k = 0; k + 1 < x_intersections.size(); k += 2) {
                            Rect fill_span = {abs_x + x_intersections[k],
                                              abs_y + scan_y - 0.5f,
                                              x_intersections[k + 1] - x_intersections[k],
                                              1.0f};
                            list.fill_rect(fill_span, fill_color);
                        }
                    }
                }
            }

            // --- Phase 2: Stroke (deferred segments) ---
            if (stroke_color.a > 0 && stroke_w > 0) {
                for (const auto& seg : stroke_segs) {
                    draw_dashed_line(abs_x + seg.x0, abs_y + seg.y0,
                                     abs_x + seg.x1, abs_y + seg.y1,
                                     stroke_color, stroke_w);
                }
            }

            break;
        }
        case 6: { // text
            if (node.svg_text_content.empty()) break;

            // Use fill color for text (SVG text is filled, not stroked by default)
            Color text_color = fill_color;
            if (text_color.a == 0) {
                // If fill is transparent/none, fall back to black
                text_color = {0, 0, 0, 255};
            }

            float tx = node.svg_text_x * vb_sx + vb_ox + node.svg_text_dx * vb_sx;
            float ty = node.svg_text_y * vb_sy + vb_oy + node.svg_text_dy * vb_sy;
            float fs = node.svg_font_size * std::min(vb_sx, vb_sy);

            // text-anchor: adjust x position based on approximate text width
            if (node.svg_text_anchor != 0) {
                // Better approximation: use 0.5 for monospace, 0.55 for proportional fonts
                float char_width_ratio = node.svg_font_family.find("monospace") != std::string::npos ? 0.5f : 0.55f;
                float approx_w = node.svg_text_content.size() * fs * char_width_ratio;
                if (node.svg_text_anchor == 1) tx -= approx_w * 0.5f; // middle
                else if (node.svg_text_anchor == 2) tx -= approx_w;   // end
            }

            // SVG text y interpretation depends on dominant-baseline.
            // draw_text expects top-left corner.
            float ascent_offset = fs * 0.8f;
            float baseline_adj = ascent_offset; // default: y = alphabetic baseline
            if (node.svg_dominant_baseline == 1) // middle
                baseline_adj = fs * 0.4f;
            else if (node.svg_dominant_baseline == 2) // hanging
                baseline_adj = fs * 0.1f;
            else if (node.svg_dominant_baseline == 3) // central
                baseline_adj = fs * 0.5f;
            else if (node.svg_dominant_baseline == 4) // text-top
                baseline_adj = 0;
            list.draw_text(node.svg_text_content,
                           abs_x + tx,
                           abs_y + ty - baseline_adj,
                           fs, text_color,
                           node.svg_font_family,
                           node.svg_font_weight,
                           node.svg_font_italic);
            break;
        }
        case 9: { // tspan — rendered like text but inherits parent positioning
            if (node.svg_text_content.empty()) break;

            Color text_color = fill_color;
            if (text_color.a == 0) text_color = {0, 0, 0, 255};

            // tspan uses its own x/y if set, otherwise uses dx/dy as offset
            float tx = node.svg_text_dx * vb_sx;
            float ty = node.svg_text_dy * vb_sy;
            if (node.svg_text_x != 0 || node.svg_text_y != 0) {
                tx = node.svg_text_x * vb_sx + vb_ox + node.svg_text_dx * vb_sx;
                ty = node.svg_text_y * vb_sy + vb_oy + node.svg_text_dy * vb_sy;
            }
            float fs = node.svg_font_size * std::min(vb_sx, vb_sy);
            float ascent_offset = fs * 0.8f;

            list.draw_text(node.svg_text_content,
                           abs_x + tx,
                           abs_y + ty - ascent_offset,
                           fs, text_color,
                           node.svg_font_family,
                           node.svg_font_weight,
                           node.svg_font_italic);
            break;
        }
        case 7:   // polygon (closed shape — fill + stroke)
        case 8: { // polyline (open shape — stroke only by default)
            const auto& raw_pts = node.svg_points;
            if (raw_pts.size() < 2) break;
            float sw = (attrs.size() >= 1) ? attrs[0] * std::min(vb_sx, vb_sy) : 1.0f;
            bool is_polygon = (node.svg_type == 7);

            // Apply viewBox transform to points
            std::vector<std::pair<float,float>> pts;
            pts.reserve(raw_pts.size());
            for (const auto& p : raw_pts) {
                pts.push_back({p.first * vb_sx + vb_ox, p.second * vb_sy + vb_oy});
            }

            // --- Fill (polygon only) using scanline algorithm ---
            if (is_polygon && fill_color.a > 0 && pts.size() >= 3) {
                // Find bounding box
                float min_y = pts[0].second, max_y = pts[0].second;
                for (const auto& p : pts) {
                    if (p.second < min_y) min_y = p.second;
                    if (p.second > max_y) max_y = p.second;
                }
                int iy_min = static_cast<int>(std::floor(min_y));
                int iy_max = static_cast<int>(std::ceil(max_y));

                // Scanline fill
                for (int iy = iy_min; iy <= iy_max; ++iy) {
                    float scan_y = static_cast<float>(iy) + 0.5f;
                    std::vector<float> x_intersections;
                    size_t n = pts.size();
                    for (size_t i = 0; i < n; ++i) {
                        size_t j = (i + 1) % n;
                        float y0 = pts[i].second, y1 = pts[j].second;
                        if ((y0 <= scan_y && y1 > scan_y) || (y1 <= scan_y && y0 > scan_y)) {
                            float t = (scan_y - y0) / (y1 - y0);
                            float xi = pts[i].first + t * (pts[j].first - pts[i].first);
                            x_intersections.push_back(xi);
                        }
                    }
                    std::sort(x_intersections.begin(), x_intersections.end());
                    // Fill between pairs of intersections
                    for (size_t k = 0; k + 1 < x_intersections.size(); k += 2) {
                        float x_left = x_intersections[k];
                        float x_right = x_intersections[k + 1];
                        Rect fill_span = {abs_x + x_left, abs_y + scan_y - 0.5f,
                                          x_right - x_left, 1.0f};
                        list.fill_rect(fill_span, fill_color);
                    }
                }
            }

            // --- Stroke: draw line segments connecting successive points ---
            if (stroke_color.a > 0 && sw > 0) {
                for (size_t i = 0; i + 1 < pts.size(); ++i) {
                    draw_dashed_line(abs_x + pts[i].first, abs_y + pts[i].second,
                                     abs_x + pts[i + 1].first, abs_y + pts[i + 1].second,
                                     stroke_color, sw);
                }
                // Close the shape for polygon
                if (is_polygon && pts.size() >= 3) {
                    draw_dashed_line(abs_x + pts.back().first, abs_y + pts.back().second,
                                     abs_x + pts[0].first, abs_y + pts[0].second,
                                     stroke_color, sw);
                }
            }
            break;
        }
        default:
            break;
    }
}

void Painter::paint_canvas_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                        float abs_x, float abs_y) {
    if (!node.is_canvas) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    // If we have a canvas_buffer with pixel data, blit it as an image
    if (node.canvas_buffer && !node.canvas_buffer->empty() &&
        node.canvas_width > 0 && node.canvas_height > 0) {
        auto img = std::make_shared<ImageData>();
        img->pixels = *node.canvas_buffer;
        img->width = node.canvas_width;
        img->height = node.canvas_height;
        Rect dest = {abs_x, abs_y, box_w, box_h};
        list.draw_image(dest, std::move(img));
        return;
    }

    // Fallback: draw placeholder text
    // Gray color for placeholder text (#999999)
    Color label_color = {0x99, 0x99, 0x99, 0xFF};

    // Draw "Canvas" label centered in the upper portion
    float label_font_size = 14.0f;
    float label_text_w = 6.0f * 6.0f * (label_font_size / 16.0f); // approximate width of "Canvas"
    float label_x = abs_x + (box_w - label_text_w) / 2.0f;
    float label_y = abs_y + box_h / 2.0f - label_font_size;
    list.draw_text("Canvas", label_x, label_y, label_font_size, label_color);

    // Draw dimensions text like "300 x 150" centered below the label
    std::string dims = std::to_string(node.canvas_width) + " x " + std::to_string(node.canvas_height);
    float dims_font_size = 11.0f;
    float dims_text_w = static_cast<float>(dims.size()) * 6.0f * (dims_font_size / 16.0f);
    float dims_x = abs_x + (box_w - dims_text_w) / 2.0f;
    float dims_y = label_y + label_font_size + 4.0f;
    list.draw_text(dims, dims_x, dims_y, dims_font_size, label_color);
}

void Painter::paint_media_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                       float abs_x, float abs_y) {
    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    if (node.media_type == 1) {
        // === Video placeholder ===
        // Dark background is already painted by paint_background

        // Draw a centered play button: white circle outline with play triangle
        float center_x = abs_x + box_w / 2.0f;
        float center_y = abs_y + box_h / 2.0f;
        float btn_radius = std::min(box_w, box_h) * 0.15f;
        if (btn_radius < 12.0f) btn_radius = 12.0f;
        if (btn_radius > 40.0f) btn_radius = 40.0f;

        // Draw circle outline (approximated with 4 border edges)
        Color white = {255, 255, 255, 180};
        float ring_w = 2.0f;
        Rect circle_rect = {center_x - btn_radius, center_y - btn_radius,
                            btn_radius * 2.0f, btn_radius * 2.0f};
        list.draw_border(circle_rect, white, ring_w, ring_w, ring_w, ring_w);

        // Draw play triangle (approximated with horizontal slices forming a right-pointing arrow)
        float tri_h = btn_radius * 0.9f;
        float tri_w = btn_radius * 0.7f;
        float tri_left = center_x - tri_w * 0.3f;
        float tri_top = center_y - tri_h / 2.0f;
        int num_slices = static_cast<int>(tri_h);
        if (num_slices < 4) num_slices = 4;
        float slice_h = tri_h / static_cast<float>(num_slices);
        for (int i = 0; i < num_slices; i++) {
            float frac = static_cast<float>(i) / static_cast<float>(num_slices - 1);
            float half = 1.0f - std::abs(2.0f * frac - 1.0f);
            float sw = tri_w * half;
            if (sw < 1.0f) sw = 1.0f;
            float sy = tri_top + slice_h * static_cast<float>(i);
            list.fill_rect({tri_left, sy, sw, slice_h + 0.5f}, white);
        }

        // Draw "Video" label below the play button
        float label_y = center_y + btn_radius + 8.0f;
        if (label_y + 14.0f < abs_y + box_h) {
            Color label_color = {255, 255, 255, 160};
            float label_font = 12.0f;
            float label_x = center_x - 15.0f;
            list.draw_text("Video", label_x, label_y, label_font, label_color);
        }

    } else if (node.media_type == 2) {
        // === Audio placeholder ===
        // Dark gray background is already painted by paint_background

        // Draw play button on the left side
        float btn_size = std::min(box_h * 0.6f, 18.0f);
        float btn_x = abs_x + 8.0f;
        float btn_y = abs_y + (box_h - btn_size) / 2.0f;
        Color white = {255, 255, 255, 200};

        // Play triangle (small, pointing right)
        int tri_slices = static_cast<int>(btn_size);
        if (tri_slices < 4) tri_slices = 4;
        float tri_slice_h = btn_size / static_cast<float>(tri_slices);
        for (int i = 0; i < tri_slices; i++) {
            float frac = static_cast<float>(i) / static_cast<float>(tri_slices - 1);
            float half = 1.0f - std::abs(2.0f * frac - 1.0f);
            float sw = btn_size * 0.6f * half;
            if (sw < 1.0f) sw = 1.0f;
            float sy = btn_y + tri_slice_h * static_cast<float>(i);
            list.fill_rect({btn_x, sy, sw, tri_slice_h + 0.5f}, white);
        }

        // Draw simulated progress bar (gray line across the middle)
        float bar_left = btn_x + btn_size + 8.0f;
        float bar_right = abs_x + box_w - 8.0f;
        float bar_h = 3.0f;
        float bar_y = abs_y + (box_h - bar_h) / 2.0f;
        if (bar_right > bar_left) {
            Color bar_bg = {100, 100, 100, 200};
            list.fill_rect({bar_left, bar_y, bar_right - bar_left, bar_h}, bar_bg);
        }

        // Draw "Audio" label to the right of the progress bar
        float label_x = bar_right - 35.0f;
        float label_y = abs_y + 2.0f;
        if (box_h >= 20.0f && label_x > bar_left) {
            Color label_color = {255, 255, 255, 120};
            list.draw_text("Audio", label_x, label_y, 10.0f, label_color);
        }
    }
}

void Painter::paint_iframe_placeholder(const clever::layout::LayoutNode& node, DisplayList& list,
                                        float abs_x, float abs_y) {
    if (!node.is_iframe) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    float center_x = abs_x + box_w / 2.0f;
    float center_y = abs_y + box_h / 2.0f;

    // Gray color for placeholder content
    Color icon_color = {0x99, 0x99, 0x99, 0xFF};
    Color label_color = {0xAA, 0xAA, 0xAA, 0xFF};

    // Draw a simplified browser window icon centered above the label
    // Icon: small rectangle with an address bar line at the top
    float icon_w = 32.0f;
    float icon_h = 24.0f;
    float icon_x = center_x - icon_w / 2.0f;
    float icon_y = center_y - icon_h - 12.0f;

    // Browser window outline (top, bottom, left, right borders)
    list.fill_rect({icon_x, icon_y, icon_w, 1.0f}, icon_color);                     // top
    list.fill_rect({icon_x, icon_y + icon_h - 1.0f, icon_w, 1.0f}, icon_color);     // bottom
    list.fill_rect({icon_x, icon_y, 1.0f, icon_h}, icon_color);                     // left
    list.fill_rect({icon_x + icon_w - 1.0f, icon_y, 1.0f, icon_h}, icon_color);     // right

    // Address bar line near top of the icon rectangle
    float bar_y = icon_y + 5.0f;
    list.fill_rect({icon_x + 3.0f, bar_y, icon_w - 6.0f, 2.0f}, icon_color);

    // Draw "iframe" label text centered below the icon
    float label_font_size = 13.0f;
    float label_text_w = 6.0f * 6.0f * (label_font_size / 16.0f); // approximate width of "iframe"
    float label_x = center_x - label_text_w / 2.0f;
    float label_y = center_y + 2.0f;
    list.draw_text("iframe", label_x, label_y, label_font_size, label_color);

    // Draw the src URL text below the label (truncated if too long)
    if (!node.iframe_src.empty()) {
        std::string url_text = node.iframe_src;
        float url_font_size = 10.0f;
        float char_w = 6.0f * (url_font_size / 16.0f);
        float max_text_w = box_w - 20.0f; // 10px padding each side
        int max_chars = static_cast<int>(max_text_w / char_w);
        if (max_chars < 3) max_chars = 3;
        if (static_cast<int>(url_text.size()) > max_chars) {
            url_text = url_text.substr(0, static_cast<size_t>(max_chars - 3)) + "...";
        }
        float url_text_w = static_cast<float>(url_text.size()) * char_w;
        float url_x = center_x - url_text_w / 2.0f;
        float url_y = label_y + label_font_size + 4.0f;
        list.draw_text(url_text, url_x, url_y, url_font_size, label_color);
    }
}

void Painter::paint_select_element(const clever::layout::LayoutNode& node, DisplayList& list,
                                    float abs_x, float abs_y) {
    if (!node.is_select_element) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();
    Rect bg_rect = {abs_x, abs_y, box_w, box_h};

    // Use CSS background-color if set, otherwise default (dark if color-scheme: dark)
    bool dark_sel = (node.color_scheme == 2);
    Color bg_color = dark_sel ? Color{0x1E, 0x1E, 0x1E, 0xFF} : Color{0xF8, 0xF8, 0xF8, 0xFF};
    if (node.background_color != 0) {
        uint32_t bg = node.background_color;
        bg_color = {
            static_cast<uint8_t>((bg >> 16) & 0xFF),
            static_cast<uint8_t>((bg >> 8) & 0xFF),
            static_cast<uint8_t>(bg & 0xFF),
            static_cast<uint8_t>((bg >> 24) & 0xFF)
        };
    }

    // Draw background with rounded corners
    float radius = node.border_radius > 0 ? node.border_radius : 4.0f;
    list.fill_rounded_rect(bg_rect, bg_color, radius);

    // Use CSS border-color if set, otherwise default (dark if color-scheme: dark)
    Color border_color = dark_sel ? Color{0x55, 0x55, 0x55, 0xFF} : Color{0x76, 0x76, 0x76, 0xFF};
    if (node.border_color != 0 && node.border_color != 0xFF000000) {
        uint32_t bc = node.border_color;
        border_color = {
            static_cast<uint8_t>((bc >> 16) & 0xFF),
            static_cast<uint8_t>((bc >> 8) & 0xFF),
            static_cast<uint8_t>(bc & 0xFF),
            static_cast<uint8_t>((bc >> 24) & 0xFF)
        };
    }
    list.draw_border(bg_rect, border_color, 1.0f, 1.0f, 1.0f, 1.0f);

    // In listbox mode (multiple or size > 1), children are rendered directly
    // — skip dropdown text and arrow
    bool is_listbox = node.select_is_multiple || node.select_visible_rows > 1;
    if (is_listbox) {
    }

    if (!is_listbox) {
    // Draw the selected option text on the left side
    if (!node.select_display_text.empty()) {
        float text_x = abs_x + geom.border.left + geom.padding.left;
        float text_y = abs_y + geom.border.top + geom.padding.top;
        // Use CSS color if set, otherwise dark gray (or light for dark mode)
        Color text_color = dark_sel ? Color{0xE0, 0xE0, 0xE0, 0xFF} : Color{0x33, 0x33, 0x33, 0xFF};
        if (node.color != 0 && node.color != 0xFF000000) {
            uint32_t tc = node.color;
            text_color = {
                static_cast<uint8_t>((tc >> 16) & 0xFF),
                static_cast<uint8_t>((tc >> 8) & 0xFF),
                static_cast<uint8_t>(tc & 0xFF),
                static_cast<uint8_t>((tc >> 24) & 0xFF)
            };
        }
        float fs = node.font_size > 0 ? node.font_size : 13.0f;
        list.draw_text(node.select_display_text, text_x, text_y, fs, text_color);
    }

    // Draw dropdown arrow — V-shaped chevron on the right side (20px wide area)
    float arrow_area_w = 20.0f;
    float arrow_x = abs_x + box_w - arrow_area_w - geom.border.right;
    float arrow_center_y = abs_y + box_h / 2.0f;

    // Separator line between text and arrow
    Color sep_color = dark_sel ? Color{0x44, 0x44, 0x44, 0xFF} : Color{0xCC, 0xCC, 0xCC, 0xFF};
    list.fill_rect({arrow_x - 1.0f, abs_y + 4.0f, 1.0f, box_h - 8.0f}, sep_color);

    // Draw chevron (V shape) — two diagonal lines meeting at center bottom
    Color arrow_color = dark_sel ? Color{0xAA, 0xAA, 0xAA, 0xFF} : Color{0x55, 0x55, 0x55, 0xFF};
    float chev_w = 8.0f;
    float chev_h = 4.0f;
    float chev_x = arrow_x + (arrow_area_w - chev_w) / 2.0f;
    float chev_top = arrow_center_y - chev_h / 2.0f;
    // Left stroke of V (top-left to center-bottom)
    for (int i = 0; i <= static_cast<int>(chev_h); i++) {
        float frac = static_cast<float>(i) / chev_h;
        float px = chev_x + frac * (chev_w / 2.0f);
        float py = chev_top + static_cast<float>(i);
        list.fill_rect({px, py, 1.5f, 1.0f}, arrow_color);
    }
    // Right stroke of V (top-right to center-bottom)
    for (int i = 0; i <= static_cast<int>(chev_h); i++) {
        float frac = static_cast<float>(i) / chev_h;
        float px = chev_x + chev_w - frac * (chev_w / 2.0f) - 1.5f;
        float py = chev_top + static_cast<float>(i);
        list.fill_rect({px, py, 1.5f, 1.0f}, arrow_color);
    }
    }

    // Register a clickable region for the dropdown if there are options
    if (!node.select_options.empty()) {
        list.add_select_click_region(bg_rect, node.select_options,
                                     node.select_selected_index, node.select_name);
    }
}

void Painter::paint_marquee(const clever::layout::LayoutNode& node, DisplayList& list,
                            float abs_x, float abs_y) {
    if (!node.is_marquee) return;

    const auto& geom = node.geometry;
    float box_w = geom.border_box_width();
    float box_h = geom.border_box_height();

    // Draw background bar (default light blue #E8F4FD, or bgcolor if specified)
    Color bg;
    if (node.marquee_bg_color != 0) {
        bg.a = static_cast<uint8_t>((node.marquee_bg_color >> 24) & 0xFF);
        bg.r = static_cast<uint8_t>((node.marquee_bg_color >> 16) & 0xFF);
        bg.g = static_cast<uint8_t>((node.marquee_bg_color >> 8) & 0xFF);
        bg.b = static_cast<uint8_t>(node.marquee_bg_color & 0xFF);
    } else {
        bg = {0xE8, 0xF4, 0xFD, 0xFF}; // default light blue
    }
    list.fill_rect({abs_x, abs_y, box_w, box_h}, bg);

    // Draw directional arrows as text hints at the edges
    Color arrow_color = {0x88, 0xAA, 0xCC, 0xFF}; // muted blue
    float arrow_font_size = node.font_size;
    float arrow_y = abs_y + (box_h - arrow_font_size) / 2.0f;

    std::string left_arrow, right_arrow;
    switch (node.marquee_direction) {
        case 0: // left
            left_arrow = "<<";
            right_arrow = "<<";
            break;
        case 1: // right
            left_arrow = ">>";
            right_arrow = ">>";
            break;
        case 2: // up
            left_arrow = "^^";
            right_arrow = "^^";
            break;
        case 3: // down
            left_arrow = "vv";
            right_arrow = "vv";
            break;
        default:
            left_arrow = "<<";
            right_arrow = "<<";
            break;
    }

    // Draw arrows at left and right edges
    float char_w = 6.0f * (arrow_font_size / 16.0f);
    float left_x = abs_x + 4.0f;
    float right_x = abs_x + box_w - (static_cast<float>(right_arrow.size()) * char_w) - 4.0f;

    list.draw_text(left_arrow, left_x, arrow_y, arrow_font_size, arrow_color);
    list.draw_text(right_arrow, right_x, arrow_y, arrow_font_size, arrow_color);
}

void Painter::paint_ruby_annotation(const clever::layout::LayoutNode& node, DisplayList& list,
                                     float abs_x, float abs_y) {
    if (!node.is_ruby_text) return;

    // Ruby text (<rt>) is drawn above or below the base text line.
    // The font size is already reduced (50% of parent) in render_pipeline.
    // ruby_position: 0=over (above), 1=under (below), 2=inter-character
    float annotation_offset_y;
    int rp = node.ruby_position;
    if (rp == 0 && node.parent) rp = node.parent->ruby_position;
    if (rp == 1) {
        // Under: place below the base text (parent font size determines baseline)
        float parent_size = node.parent ? node.parent->font_size : node.font_size * 2;
        annotation_offset_y = parent_size * 0.2f;
    } else {
        // Over (default): shift upward by annotation font size
        annotation_offset_y = -node.font_size;
    }

    // Collect text content from child text nodes
    std::string annotation_text;
    for (auto& child : node.children) {
        if (child->is_text && !child->text_content.empty()) {
            annotation_text += child->text_content;
        }
    }

    if (annotation_text.empty()) return;

    // Draw annotation text above the node position, centered
    Color text_color;
    text_color.a = static_cast<uint8_t>((node.color >> 24) & 0xFF);
    text_color.r = static_cast<uint8_t>((node.color >> 16) & 0xFF);
    text_color.g = static_cast<uint8_t>((node.color >> 8) & 0xFF);
    text_color.b = static_cast<uint8_t>(node.color & 0xFF);

    list.draw_text(annotation_text, abs_x, abs_y + annotation_offset_y,
                   node.font_size, text_color);
}

void Painter::paint_list_marker(const clever::layout::LayoutNode& node, DisplayList& list,
                                 float abs_x, float abs_y) {
    if (!node.list_style_image.empty()) {
        float marker_font_size = (node.marker_font_size > 0) ? node.marker_font_size : node.font_size;
        float marker_width = marker_font_size * 0.8f;
        auto marker_img = fetch_image_for_js(node.list_style_image);
        if (marker_img.success() && marker_width > 0 && marker_img.width > 0 && marker_img.height > 0) {
            auto img = std::make_shared<ImageData>();
            img->pixels = *marker_img.pixels;
            img->width = marker_img.width;
            img->height = marker_img.height;

            float marker_height = marker_width * static_cast<float>(marker_img.height) /
                                 static_cast<float>(marker_img.width);
            float marker_x = (node.list_style_position == 1)
                ? abs_x + 2.0f
                : abs_x - marker_width - 8.0f;
            float marker_y = abs_y + marker_font_size * 0.35f - (marker_height * 0.5f);
            list.draw_image({marker_x, marker_y, marker_width, marker_height}, std::move(img));
            return;
        }
    }

    if (node.list_style_type == 9) return; // None

    // Use ::marker color if set, otherwise fall back to text color
    uint32_t tc = (node.marker_color != 0) ? node.marker_color : node.color;
    uint8_t a = static_cast<uint8_t>((tc >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((tc >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((tc >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(tc & 0xFF);
    Color color = {r, g, b, a};

    // Use ::marker font-size if set, otherwise fall back to element font-size
    float effective_font_size = (node.marker_font_size > 0) ? node.marker_font_size : node.font_size;
    float marker_size = effective_font_size * 0.35f;
    // Outside (default): marker to the left of content; Inside: at content start
    float marker_x = (node.list_style_position == 1)
        ? abs_x + 2.0f
        : abs_x - marker_size - 8.0f;
    float marker_y = abs_y + effective_font_size * 0.35f;

    if (node.list_style_type == 0) { // disc - filled circle
        int cx = static_cast<int>(marker_x);
        int cy = static_cast<int>(marker_y);
        int radius = static_cast<int>(marker_size / 2);
        if (radius < 1) radius = 1;
        for (int dy = -radius; dy <= radius; dy++) {
            int dx = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
            list.fill_rect({static_cast<float>(cx - dx), static_cast<float>(cy + dy),
                           static_cast<float>(2 * dx), 1.0f}, color);
        }
    } else if (node.list_style_type == 1) { // circle - hollow
        int cx = static_cast<int>(marker_x);
        int cy = static_cast<int>(marker_y);
        int radius = static_cast<int>(marker_size / 2);
        if (radius < 1) radius = 1;
        for (int angle = 0; angle < 360; angle++) {
            float rad = static_cast<float>(angle) * 3.14159f / 180.0f;
            int px = cx + static_cast<int>(static_cast<float>(radius) * std::cos(rad));
            int py = cy + static_cast<int>(static_cast<float>(radius) * std::sin(rad));
            list.fill_rect({static_cast<float>(px), static_cast<float>(py), 1.0f, 1.0f}, color);
        }
    } else if (node.list_style_type == 2) { // square - filled
        int sx = static_cast<int>(marker_x - marker_size / 2);
        int sy = static_cast<int>(marker_y - marker_size / 2);
        int size = static_cast<int>(marker_size);
        if (size < 2) size = 2;
        list.fill_rect({static_cast<float>(sx), static_cast<float>(sy),
                        static_cast<float>(size), static_cast<float>(size)}, color);
    } else {
        // Text-based markers: decimal, decimal-leading-zero, roman, alpha, greek, latin
        std::string marker_str;
        int idx = node.list_item_index;
        auto to_roman = [](int n, bool upper) -> std::string {
            const std::pair<int,const char*> table_lower[] = {
                {1000,"m"},{900,"cm"},{500,"d"},{400,"cd"},{100,"c"},{90,"xc"},
                {50,"l"},{40,"xl"},{10,"x"},{9,"ix"},{5,"v"},{4,"iv"},{1,"i"}
            };
            const std::pair<int,const char*> table_upper[] = {
                {1000,"M"},{900,"CM"},{500,"D"},{400,"CD"},{100,"C"},{90,"XC"},
                {50,"L"},{40,"XL"},{10,"X"},{9,"IX"},{5,"V"},{4,"IV"},{1,"I"}
            };
            auto& table = upper ? table_upper : table_lower;
            std::string r;
            for (auto& [val, sym] : table) {
                while (n >= val) { r += sym; n -= val; }
            }
            return r;
        };

        auto to_utf8 = [](uint32_t cp) -> std::string {
            std::string out;
            if (cp <= 0x7F) {
                out.push_back(static_cast<char>(cp));
            } else if (cp <= 0x7FF) {
                out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp <= 0xFFFF) {
                out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            return out;
        };

        auto utf8_len = [](const std::string& text) -> size_t {
            size_t len = 0;
            for (unsigned char c : text) {
                if ((c & 0xC0) != 0x80) ++len;
            }
            return len;
        };

        // Helper: spreadsheet-style alpha encoding (1=a, 27=aa, 28=ab...)
        auto to_alpha = [](int n, bool upper) -> std::string {
            if (n <= 0) return std::to_string(n);
            std::string r;
            while (n > 0) {
                int rem = (n - 1) % 26;
                char c = upper ? static_cast<char>('A' + rem) : static_cast<char>('a' + rem);
                r = c + r;
                n = (n - 1) / 26;
            }
            return r;
        };

        switch (node.list_style_type) {
            case 3: // decimal
                marker_str = std::to_string(idx) + ".";
                break;
            case 4: // decimal-leading-zero
                if (idx < 10) marker_str = "0" + std::to_string(idx) + ".";
                else marker_str = std::to_string(idx) + ".";
                break;
            case 5: // lower-roman
                marker_str = to_roman(idx, false) + ".";
                break;
            case 6: // upper-roman
                marker_str = to_roman(idx, true) + ".";
                break;
            case 7:  // lower-alpha
            case 11: // lower-latin (alias)
                marker_str = to_alpha(idx, false) + ".";
                break;
            case 8:  // upper-alpha
            case 12: // upper-latin (alias)
                marker_str = to_alpha(idx, true) + ".";
                break;
            case 10: { // lower-greek
                static const char* greek[] = {
                    "\xCE\xB1","\xCE\xB2","\xCE\xB3","\xCE\xB4","\xCE\xB5",
                    "\xCE\xB6","\xCE\xB7","\xCE\xB8","\xCE\xB9","\xCE\xBA",
                    "\xCE\xBB","\xCE\xBC","\xCE\xBD","\xCE\xBE","\xCE\xBF",
                    "\xCF\x80","\xCF\x81","\xCF\x83","\xCF\x84","\xCF\x85",
                    "\xCF\x86","\xCF\x87","\xCF\x88","\xCF\x89"
                };
                marker_str = std::string(greek[(idx - 1) % 24]) + ".";
                break;
            }
            case 13: { // armenian
                if (idx <= 0) {
                    marker_str = std::to_string(idx) + ".";
                    break;
                }
                // Armenian letters: 37 markers, then wrap
                uint32_t cp = 0x0561 + static_cast<uint32_t>((idx - 1) % 37);
                marker_str = to_utf8(cp) + ".";
                break;
            }
            case 14: { // georgian
                if (idx <= 0) {
                    marker_str = std::to_string(idx) + ".";
                    break;
                }
                // Georgian letters: ა to ჰ (33 markers), then wrap
                uint32_t cp = 0x10D0 + static_cast<uint32_t>((idx - 1) % 33);
                marker_str = to_utf8(cp) + ".";
                break;
            }
            case 15: { // cjk-decimal
                static const char* digits[] = {
                    "\xE3\x80\x87", // 〇
                    "\xE4\xB8\x80", // 一
                    "\xE4\xBA\x8C", // 二
                    "\xE4\xB8\x89", // 三
                    "\xE5\x9B\x9B", // 四
                    "\xE4\xBA\x94", // 五
                    "\xE5\x85\xAD", // 六
                    "\xE4\xB8\x83", // 七
                    "\xE5\x85\xAB", // 八
                    "\xE4\xB9\x9D", // 九
                    "\xE5\x8D\x81"  // 十
                };
                if (idx >= 0 && idx <= 10) marker_str = std::string(digits[idx]) + ".";
                else marker_str = std::to_string(idx) + ".";
                break;
            }
            default:
                marker_str = std::to_string(idx) + "."; // fallback decimal
                break;
        }
        float text_x = (node.list_style_position == 1)
            ? abs_x + 2.0f
            : abs_x - (static_cast<float>(utf8_len(marker_str)) * effective_font_size * 0.6f) - 4.0f;
        list.draw_text(marker_str, text_x, abs_y, effective_font_size, color);
    }
}

void Painter::paint_overflow_indicator(const clever::layout::LayoutNode& node, DisplayList& list,
                                       float abs_x, float abs_y) {
    const auto& geom = node.geometry;

    // Determine scrollbar colors (CSS scrollbar-color)
    Color thumb_color = {0x88, 0x88, 0x88, 0xCC}; // default gray thumb
    Color track_color = {0xEE, 0xEE, 0xEE, 0xFF}; // default light track
    if (node.scrollbar_thumb_color != 0) {
        uint32_t tc = node.scrollbar_thumb_color;
        thumb_color = {
            static_cast<uint8_t>((tc >> 16) & 0xFF),
            static_cast<uint8_t>((tc >> 8) & 0xFF),
            static_cast<uint8_t>(tc & 0xFF),
            static_cast<uint8_t>((tc >> 24) & 0xFF)
        };
    }
    if (node.scrollbar_track_color != 0) {
        uint32_t tc = node.scrollbar_track_color;
        track_color = {
            static_cast<uint8_t>((tc >> 16) & 0xFF),
            static_cast<uint8_t>((tc >> 8) & 0xFF),
            static_cast<uint8_t>(tc & 0xFF),
            static_cast<uint8_t>((tc >> 24) & 0xFF)
        };
    }

    // Scrollbar dimensions: 0=auto (12px), 1=thin (8px), 2=none (no scrollbar)
    float sb_width = 12.0f;
    if (node.scrollbar_width == 1) sb_width = 8.0f;
    else if (node.scrollbar_width == 2) return; // scrollbar-width: none

    float content_x = abs_x + geom.border.left;
    float content_y = abs_y + geom.border.top;
    float box_w = geom.width + geom.padding.left + geom.padding.right;
    float box_h = geom.height + geom.padding.top + geom.padding.bottom;

    if (node.overflow_indicator_bottom) {
        // Draw vertical scrollbar on the right edge
        float sb_x = content_x + box_w - sb_width;
        float sb_y = content_y;
        float sb_h = box_h;

        // Track
        list.fill_rect({sb_x, sb_y, sb_width, sb_h}, track_color);

        // Compute thumb size and position based on actual content-to-viewport ratio
        float viewport_h = geom.height;
        float content_h = node.scroll_content_height;
        float thumb_h, thumb_y;
        if (content_h > viewport_h && content_h > 0) {
            // Thumb height proportional to viewport/content ratio
            float ratio = viewport_h / content_h;
            thumb_h = std::max(20.0f, (sb_h - 4.0f) * ratio);
            // Thumb position based on scroll_top
            float max_scroll = content_h - viewport_h;
            float scroll_frac = (max_scroll > 0) ? (node.scroll_top / max_scroll) : 0.0f;
            float track_range = sb_h - 4.0f - thumb_h; // available track space
            thumb_y = sb_y + 2.0f + scroll_frac * track_range;
        } else {
            // No actual overflow or unknown content size — default thumb
            thumb_h = std::max(20.0f, sb_h * 0.3f);
            thumb_y = sb_y + 2.0f;
        }
        float thumb_radius = sb_width * 0.3f;
        list.fill_rounded_rect({sb_x + 2.0f, thumb_y, sb_width - 4.0f, thumb_h},
                               thumb_color, thumb_radius);
    }

    if (node.overflow_indicator_right) {
        // Draw horizontal scrollbar on the bottom edge
        float sb_x = content_x;
        float sb_y = content_y + box_h - sb_width;
        float sb_w = box_w;

        // Track
        list.fill_rect({sb_x, sb_y, sb_w, sb_width}, track_color);

        // Compute thumb size and position based on actual content-to-viewport ratio
        float viewport_w = geom.width;
        float content_w = node.scroll_content_width;
        float thumb_w, thumb_x;
        if (content_w > viewport_w && content_w > 0) {
            // Thumb width proportional to viewport/content ratio
            float ratio = viewport_w / content_w;
            thumb_w = std::max(20.0f, (sb_w - 4.0f) * ratio);
            // Thumb position based on scroll_left
            float max_scroll = content_w - viewport_w;
            float scroll_frac = (max_scroll > 0) ? (node.scroll_left / max_scroll) : 0.0f;
            float track_range = sb_w - 4.0f - thumb_w;
            thumb_x = sb_x + 2.0f + scroll_frac * track_range;
        } else {
            // No actual overflow or unknown content size — default thumb
            thumb_w = std::max(20.0f, sb_w * 0.3f);
            thumb_x = sb_x + 2.0f;
        }
        float thumb_radius = sb_width * 0.3f;
        list.fill_rounded_rect({thumb_x, sb_y + 2.0f, thumb_w, sb_width - 4.0f},
                               thumb_color, thumb_radius);
    }
}

void Painter::paint_scrollbar(const clever::layout::LayoutNode& node, DisplayList& list,
                              float abs_x, float abs_y) {
    if (!node.is_scroll_container || node.overflow < 2) return;
    if (node.scrollbar_width == 2) return; // scrollbar-width: none

    // Only paint scrollbars if content actually overflows
    if (!node.overflow_indicator_bottom && !node.overflow_indicator_right) return;

    const auto& geom = node.geometry;

    // Determine scrollbar colors (CSS scrollbar-color)
    Color thumb_color = {0x88, 0x88, 0x88, 0xCC}; // default gray thumb, semi-transparent
    Color track_color = {0xEE, 0xEE, 0xEE, 0xFF}; // default light track, opaque
    if (node.scrollbar_thumb_color != 0) {
        uint32_t tc = node.scrollbar_thumb_color;
        thumb_color = {
            static_cast<uint8_t>((tc >> 16) & 0xFF),
            static_cast<uint8_t>((tc >> 8) & 0xFF),
            static_cast<uint8_t>(tc & 0xFF),
            static_cast<uint8_t>((tc >> 24) & 0xFF)
        };
    }
    if (node.scrollbar_track_color != 0) {
        uint32_t tc = node.scrollbar_track_color;
        track_color = {
            static_cast<uint8_t>((tc >> 16) & 0xFF),
            static_cast<uint8_t>((tc >> 8) & 0xFF),
            static_cast<uint8_t>(tc & 0xFF),
            static_cast<uint8_t>((tc >> 24) & 0xFF)
        };
    }

    // Scrollbar dimensions: 0=auto (12px), 1=thin (8px), 2=none (no scrollbar)
    float sb_width = 12.0f;
    if (node.scrollbar_width == 1) sb_width = 8.0f;

    float content_x = abs_x + geom.border.left;
    float content_y = abs_y + geom.border.top;
    float box_w = geom.width + geom.padding.left + geom.padding.right;
    float box_h = geom.height + geom.padding.top + geom.padding.bottom;

    // Vertical scrollbar (right edge)
    if (node.overflow_indicator_bottom) {
        float sb_x = content_x + box_w - sb_width;
        float sb_y = content_y;
        float sb_h = box_h;

        // Track
        list.fill_rect({sb_x, sb_y, sb_width, sb_h}, track_color);

        // Compute thumb size and position based on actual content-to-viewport ratio
        float viewport_h = geom.height;
        float content_h = node.scroll_content_height;
        float thumb_h, thumb_y;
        if (content_h > viewport_h && content_h > 0) {
            // Thumb height proportional to viewport/content ratio
            float ratio = viewport_h / content_h;
            thumb_h = std::max(20.0f, (sb_h - 4.0f) * ratio);
            // Thumb position based on scroll_top
            float max_scroll = content_h - viewport_h;
            float scroll_frac = (max_scroll > 0) ? (node.scroll_top / max_scroll) : 0.0f;
            float track_range = sb_h - 4.0f - thumb_h;
            thumb_y = sb_y + 2.0f + scroll_frac * track_range;
        } else {
            // No actual overflow or unknown content size — default thumb
            thumb_h = std::max(20.0f, sb_h * 0.3f);
            thumb_y = sb_y + 2.0f;
        }
        float thumb_radius = sb_width * 0.3f;
        list.fill_rounded_rect({sb_x + 2.0f, thumb_y, sb_width - 4.0f, thumb_h},
                               thumb_color, thumb_radius);
    }

    // Horizontal scrollbar (bottom edge)
    if (node.overflow_indicator_right) {
        float sb_x = content_x;
        float sb_w = box_w;
        float sb_y = content_y + box_h - sb_width;

        // Track
        list.fill_rect({sb_x, sb_y, sb_w, sb_width}, track_color);

        // Compute thumb size and position based on actual content-to-viewport ratio
        float viewport_w = geom.width;
        float content_w = node.scroll_content_width;
        float thumb_w, thumb_x;
        if (content_w > viewport_w && content_w > 0) {
            // Thumb width proportional to viewport/content ratio
            float ratio = viewport_w / content_w;
            thumb_w = std::max(20.0f, (sb_w - 4.0f) * ratio);
            // Thumb position based on scroll_left
            float max_scroll = content_w - viewport_w;
            float scroll_frac = (max_scroll > 0) ? (node.scroll_left / max_scroll) : 0.0f;
            float track_range = sb_w - 4.0f - thumb_w;
            thumb_x = sb_x + 2.0f + scroll_frac * track_range;
        } else {
            // No actual overflow or unknown content size — default thumb
            thumb_w = std::max(20.0f, sb_w * 0.3f);
            thumb_x = sb_x + 2.0f;
        }
        float thumb_radius = sb_width * 0.3f;
        list.fill_rounded_rect({thumb_x, sb_y + 2.0f, thumb_w, sb_width - 4.0f},
                               thumb_color, thumb_radius);
    }

    // Scrollbar corner (bottom-right intersection)
    if (node.overflow_indicator_bottom && node.overflow_indicator_right) {
        float corner_x = content_x + box_w - sb_width;
        float corner_y = content_y + box_h - sb_width;
        list.fill_rect({corner_x, corner_y, sb_width, sb_width}, track_color);
    }
}

} // namespace clever::paint
