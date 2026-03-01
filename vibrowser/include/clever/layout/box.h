#pragma once
#include <clever/css/style/computed_style.h>
#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

namespace clever::layout {

// Sentinel value for "auto" margins. The layout engine resolves auto margins
// into computed values during layout.  Tests and render_pipeline use this
// constant to mark margins as auto before layout runs.
constexpr float MARGIN_AUTO = -1e30f;
inline bool is_margin_auto(float v) { return v <= -1e29f; }
constexpr int Z_INDEX_AUTO = INT_MIN;
inline bool is_z_index_auto(int v) { return v == Z_INDEX_AUTO; }

struct EdgeSizes {
    float top = 0, right = 0, bottom = 0, left = 0;
};

struct BoxGeometry {
    float x = 0, y = 0;           // position relative to containing block
    float width = 0, height = 0;   // content box size
    EdgeSizes margin, border, padding;

    float margin_box_width() const {
        return margin.left + border.left + padding.left + width + padding.right + border.right + margin.right;
    }
    float margin_box_height() const {
        return margin.top + border.top + padding.top + height + padding.bottom + border.bottom + margin.bottom;
    }
    float border_box_width() const { return border.left + padding.left + width + padding.right + border.right; }
    float border_box_height() const { return border.top + padding.top + height + padding.bottom + border.bottom; }
    float content_left() const { return x + margin.left + border.left + padding.left; }
    float content_top() const { return y + margin.top + border.top + padding.top; }
};

enum class LayoutMode {
    Block,
    Inline,
    Flex,
    InlineBlock,
    Grid,
    Table,
    None
};

enum class DisplayType {
    Block, Inline, InlineBlock, Flex, InlineFlex, None,
    ListItem, Table, TableRow, TableCell,
    Grid, InlineGrid
};

struct LayoutNode {
    BoxGeometry geometry;
    LayoutMode mode = LayoutMode::Block;
    DisplayType display = DisplayType::Block;
    std::string tag_name;
    std::string element_id;    // HTML id attribute (for SVG <use> refs)
    std::vector<std::string> css_classes;  // CSS class names (for container query matching)
    std::string text_content;  // for text nodes
    bool is_text = false;
    bool is_svg = false;  // true for SVG container and shape elements
    int svg_type = 0;     // 0=none, 1=rect, 2=circle, 3=ellipse, 4=line, 5=path, 6=text, 7=polygon, 8=polyline
    bool is_svg_group = false;    // true for SVG <g> group elements
    bool svg_has_viewbox = false;  // true if viewBox attribute is set
    float svg_viewbox_x = 0;      // viewBox minX
    float svg_viewbox_y = 0;      // viewBox minY
    float svg_viewbox_w = 0;      // viewBox width
    float svg_viewbox_h = 0;      // viewBox height
    float svg_transform_tx = 0;   // SVG transform translate X
    float svg_transform_ty = 0;   // SVG transform translate Y
    float svg_transform_sx = 1;   // SVG transform scale X
    float svg_transform_sy = 1;   // SVG transform scale Y
    float svg_transform_rotate = 0; // SVG transform rotate (degrees)
    std::vector<float> svg_attrs; // generic SVG attribute storage
    std::vector<std::pair<float,float>> svg_points; // polygon/polyline coordinate pairs
    std::string svg_path_d;       // SVG <path> d attribute string
    bool is_svg_defs = false;    // <defs> container - not rendered
    bool is_svg_use = false;     // <use> reference element
    std::string svg_use_href;    // href reference (e.g., "#myCircle")
    float svg_use_x = 0;        // x position offset for <use>
    float svg_use_y = 0;        // y position offset for <use>
    std::string svg_text_content;  // SVG <text> element text content
    std::string svg_content;     // Serialized SVG XML string for inline SVG containers

    // SVG gradient definitions (stored on SVG container <svg> node)
    struct SvgGradient {
        bool is_radial = false; // false=linear, true=radial
        float x1 = 0, y1 = 0, x2 = 1, y2 = 0; // linear: start/end points (0-1 fractions)
        float cx = 0.5f, cy = 0.5f, r = 0.5f;  // radial: center and radius (0-1 fractions)
        std::vector<std::pair<uint32_t, float>> stops; // {ARGB color, offset 0-1}
    };
    std::map<std::string, SvgGradient> svg_gradient_defs; // keyed by id
    std::string svg_fill_gradient_id; // if fill="url(#id)", the gradient reference
    float svg_text_x = 0;         // SVG <text> x position
    float svg_text_y = 0;         // SVG <text> y position
    float svg_text_dx = 0;        // SVG <text> dx offset
    float svg_text_dy = 0;        // SVG <text> dy offset
    float svg_font_size = 16.0f;  // SVG <text> font-size
    int svg_text_anchor = 0;      // 0=start, 1=middle, 2=end
    int svg_dominant_baseline = 0; // 0=auto, 1=middle, 2=hanging, 3=central, 4=text-top
    std::string svg_font_family;  // SVG text font-family
    int svg_font_weight = 400;    // SVG text font-weight
    bool svg_font_italic = false; // SVG text font-style italic
    uint32_t svg_fill_color = 0xFF000000;   // SVG fill color (ARGB), default black
    bool svg_fill_none = false;              // SVG fill: none
    uint32_t svg_stroke_color = 0xFF000000;  // SVG stroke color (ARGB), default black
    bool svg_stroke_none = true;             // SVG stroke: none (default no stroke)
    float svg_fill_opacity = 1.0f;           // SVG fill-opacity (0.0 to 1.0)
    float svg_stroke_opacity = 1.0f;         // SVG stroke-opacity (0.0 to 1.0)
    std::vector<float> svg_stroke_dasharray; // SVG stroke-dasharray pattern
    float svg_stroke_dashoffset = 0;         // SVG stroke-dashoffset
    int svg_stroke_linecap = 0;              // 0=butt, 1=round, 2=square
    int svg_stroke_linejoin = 0;             // 0=miter, 1=round, 2=bevel
    int fill_rule = 0;                      // 0=nonzero, 1=evenodd
    int clip_rule = 0;                      // 0=nonzero, 1=evenodd
    float stroke_miterlimit = 4.0f;         // SVG stroke-miterlimit (default 4)
    int shape_rendering = 0;                // 0=auto, 1=optimizeSpeed, 2=crispEdges, 3=geometricPrecision
    int vector_effect = 0;                  // 0=none, 1=non-scaling-stroke
    uint32_t stop_color = 0xFF000000;       // SVG stop-color (ARGB, default black)
    float stop_opacity = 1.0f;             // SVG stop-opacity (0.0 to 1.0)
    uint32_t flood_color = 0xFF000000;      // SVG flood-color (ARGB, default black)
    float flood_opacity = 1.0f;            // SVG flood-opacity (0.0 to 1.0)
    uint32_t lighting_color = 0xFFFFFFFF;   // SVG lighting-color (ARGB, default white)
    bool visibility_hidden = false; // CSS visibility: hidden — invisible but takes up space
    bool visibility_collapse = false; // CSS visibility: collapse — collapses table rows/cols

    // Canvas element placeholder
    bool is_canvas = false;
    int canvas_width = 0;
    int canvas_height = 0;
    std::shared_ptr<std::vector<uint8_t>> canvas_buffer; // RGBA pixel data

    // Iframe element placeholder (nested browsing context not yet supported)
    bool is_iframe = false;
    std::string iframe_src;
    std::string lazy_iframe_url = "";  // deferred src for loading="lazy"

    // Noscript element — always rendered (no JS engine)
    bool is_noscript = false;

    // Slot element (<slot>) — Web Components slot distribution placeholder
    bool is_slot = false;
    std::string slot_name;  // the `name` attribute of a <slot>

    // Style properties relevant to layout
    float font_size = 16.0f;
    int font_weight = 400;
    bool font_italic = false;
    std::string font_family;  // e.g. "monospace", "sans-serif"
    bool is_monospace = false; // signals to use monospace font metrics (wider char_width)
    bool is_kbd = false;       // for keyboard input styling (border, background)
    float line_height = 1.2f;
    float opacity = 1.0f;
    int mix_blend_mode = 0; // 0=normal, 1=multiply, 2=screen, 3=overlay, 4=darken, 5=lighten, 6=color-dodge, 7=color-burn, 8=hard-light, 9=soft-light, 10=difference, 11=exclusion
    float letter_spacing = 0;
    float word_spacing = 0;
    int z_index = Z_INDEX_AUTO;

    // Object fit (for <img> elements): 0=fill, 1=contain, 2=cover, 3=none, 4=scale-down
    int object_fit = 0;

    // Object position (for <img> elements)
    float object_position_x = 50.0f; // percentage (0=left, 50=center, 100=right)
    float object_position_y = 50.0f; // percentage (0=top, 50=center, 100=bottom)

    // Computed image rendering dimensions (set by object-fit calculation)
    float rendered_img_x = 0;  // x offset within the element box for the image
    float rendered_img_y = 0;  // y offset within the element box for the image
    float rendered_img_w = 0;  // rendered width of the image
    float rendered_img_h = 0;  // rendered height of the image

    // Image rendering: 0=auto, 1=smooth, 2=high-quality, 3=crisp-edges, 4=pixelated
    int image_rendering = 0;

    // Hanging punctuation: 0=none, 1=first, 2=last, 3=force-end, 4=allow-end, 5=first last
    int hanging_punctuation = 0;

    // Flex properties
    float flex_grow = 0;
    float flex_shrink = 1;
    float flex_basis = -1;  // -1 = auto
    int flex_direction = 0; // 0=row, 1=row-reverse, 2=column, 3=column-reverse
    int flex_wrap = 0;     // 0=nowrap, 1=wrap, 2=wrap-reverse
    int justify_content = 0; // 0=flex-start, 1=flex-end, 2=center, 3=space-between, 4=space-around
    int align_items = 4; // 0=flex-start, 1=flex-end, 2=center, 3=baseline, 4=stretch
    int align_self = -1; // -1=auto, 0-4 same as align_items
    int justify_self = -1; // -1=auto, 0=start, 1=end, 2=center, 3=baseline, 4=stretch
    float gap = 0;
    float row_gap = 0;    // CSS row-gap (set by gap shorthand or row-gap property)
    float column_gap = 0; // CSS column-gap (set by gap shorthand or column-gap property)
    int order = 0; // CSS order property for flex items

    // CSS Grid layout
    std::string grid_template_columns;
    std::string grid_template_rows;
    bool grid_template_columns_is_subgrid = false;
    bool grid_template_rows_is_subgrid = false;
    std::string grid_column; // e.g. "1 / 3"
    std::string grid_row;    // e.g. "1 / 2"
    std::string grid_column_start;
    std::string grid_column_end;
    std::string grid_row_start;
    std::string grid_row_end;
    std::string grid_auto_rows;       // e.g. "100px", "minmax(100px, auto)"
    std::string grid_auto_columns;    // e.g. "1fr", "200px"
    std::string grid_template_areas;  // e.g. "\"header header\" \"sidebar main\""
    std::string grid_area;            // e.g. "header" or "1 / 2 / 3 / 4"
    int grid_auto_flow = 0;   // 0=row, 1=column, 2=row dense, 3=column dense
    int justify_items = 3;   // 0=start, 1=end, 2=center, 3=stretch (default)
    int align_content = 0;   // 0=start, 1=end, 2=center, 3=stretch, 4=space-between, 5=space-around

    // Width/height constraints
    // -1 = auto, -2 = min-content, -3 = max-content, -4 = fit-content
    float specified_width = -1;
    float specified_height = -1;
    float min_width = 0;
    float max_width = 1e9f;
    float min_height = 0;
    float max_height = 1e9f;
    float aspect_ratio = 0; // 0 = none, >0 = width/height ratio
    bool aspect_ratio_is_auto = false; // true for "auto" or "auto <ratio>"

    // Box sizing: false=content-box (default), true=border-box
    bool border_box = false;

    // Deferred CSS Length for calc() expressions that need container context
    std::optional<clever::css::Length> css_width;   // set when width uses calc/percent
    std::optional<clever::css::Length> css_height;  // set when height uses calc/percent
    std::optional<clever::css::Length> css_max_width;  // set when max-width uses percent/calc
    std::optional<clever::css::Length> css_min_width;  // set when min-width uses percent/calc
    std::optional<clever::css::Length> css_max_height;  // set when max-height uses percent/calc
    std::optional<clever::css::Length> css_min_height;  // set when min-height uses percent/calc

    // Deferred CSS Length for percentage margins (resolve against containing block width)
    std::optional<clever::css::Length> css_margin_left;
    std::optional<clever::css::Length> css_margin_right;
    std::optional<clever::css::Length> css_margin_top;
    std::optional<clever::css::Length> css_margin_bottom;

    // Deferred CSS Length for percentage padding (resolve against containing block width)
    std::optional<clever::css::Length> css_padding_left;
    std::optional<clever::css::Length> css_padding_right;
    std::optional<clever::css::Length> css_padding_top;
    std::optional<clever::css::Length> css_padding_bottom;

    // Colors (for paint)
    uint32_t color = 0xFF000000;       // ARGB
    uint32_t background_color = 0x00000000; // ARGB, transparent
    uint32_t border_color = 0xFF000000; // ARGB, default black
    uint32_t border_color_top = 0xFF000000;    // per-side border color (ARGB)
    uint32_t border_color_right = 0xFF000000;
    uint32_t border_color_bottom = 0xFF000000;
    uint32_t border_color_left = 0xFF000000;

    // Image data (for <img> elements with decoded images)
    std::shared_ptr<std::vector<uint8_t>> image_pixels; // RGBA, row-major
    int image_width = 0;
    int image_height = 0;

    // Alt text for broken/missing images (non-empty when image failed to load)
    std::string img_alt_text;

    // Deferred image URL for loading="lazy" (cleared once fetched)
    std::string lazy_img_url = "";

    // Background image data (for CSS background-image: url())
    std::shared_ptr<std::vector<uint8_t>> bg_image_pixels; // RGBA, row-major
    int bg_image_width = 0;
    int bg_image_height = 0;

    // Background image layout properties
    int background_size = 0; // 0=auto, 1=cover, 2=contain, 3=explicit
    float bg_size_width = 0, bg_size_height = 0; // explicit sizes: pixels or percentage (see _pct flags)
    bool bg_size_width_pct = false;  // true => bg_size_width is a percentage value (0..100)
    bool bg_size_height_pct = false; // true => bg_size_height is a percentage value (0..100)
    bool bg_size_height_auto = false; // true => height is auto (preserve aspect ratio for explicit width)
    int background_repeat = 0; // 0=repeat, 1=repeat-x, 2=repeat-y, 3=no-repeat, 4=space, 5=round
    float bg_position_x = 0, bg_position_y = 0; // position: pixels or percentage (see _pct flags)
    bool bg_position_x_pct = false; // true => bg_position_x is a percentage value (0..100)
    bool bg_position_y_pct = false; // true => bg_position_y is a percentage value (0..100)

    // Text transform: 0=none, 1=capitalize, 2=uppercase, 3=lowercase
    int text_transform = 0;

    // Text decoration: 0=none, 1=underline, 2=line-through, 3=overline
    int text_decoration = 0;
    int text_decoration_bits = 0; // bitmask: 1=underline, 2=overline, 4=line-through
    uint32_t text_decoration_color = 0; // 0 = use text color (currentColor)
    int text_decoration_style = 0; // 0=solid, 1=dashed, 2=dotted, 3=wavy, 4=double
    float text_decoration_thickness = 0; // 0 = auto (1px)

    // Border collapse: true = collapse table cell borders
    bool border_collapse = false;

    // Table layout: 0=auto, 1=fixed
    int table_layout = 0;

    // HTML cellpadding/cellspacing/border attributes (for propagation to td/th children)
    float table_cellpadding = -1; // -1 = not set (use CSS padding)
    float table_cellspacing = -1; // -1 = not set (use border-spacing)
    int table_border = -1;        // HTML border attribute: -1=not set, 0=no borders, >0=border width
    std::string table_rules;       // HTML rules attribute: none, groups, rows, cols, all

    // Border spacing (CSS border-spacing, in px; 0 when border-collapse: collapse)
    float border_spacing = 2.0f; // CSS default is user-agent dependent, 2px is common
    float border_spacing_v = 0;  // vertical border-spacing (if different from horizontal)

    // Colspan/Rowspan (HTML attributes on table cells)
    int colspan = 1;
    int rowspan = 1;

    // Caption side: 0=top, 1=bottom
    int caption_side = 0;

    // Empty cells: 0=show, 1=hide
    int empty_cells = 0;

    // List style position: 0=outside, 1=inside
    int list_style_position = 0;

    // Pointer events: 0=auto, 1=none, 2=visible-painted, 3=visible-fill, 4=visible-stroke, 5=visible, 6=painted, 7=fill, 8=stroke, 9=all
    int pointer_events = 0;

    // User select: 0=auto, 1=none, 2=text, 3=all
    int user_select = 0;

    // Tab size (number of spaces for pre-formatted text)
    int tab_size = 4;

    // Border style: 0=none, 1=solid, 2=dashed, 3=dotted
    int border_style = 0;
    int border_style_top = 0;    // per-side border style
    int border_style_right = 0;
    int border_style_bottom = 0;
    int border_style_left = 0;

    // Border radius (single value for all corners)
    float border_radius = 0;
    float border_radius_tl = 0; // top-left
    float border_radius_tr = 0; // top-right
    float border_radius_bl = 0; // bottom-left
    float border_radius_br = 0; // bottom-right

    // Box shadow: offset_x, offset_y, blur_radius, color (ARGB)
    float shadow_offset_x = 0, shadow_offset_y = 0, shadow_blur = 0;
    float shadow_spread = 0;
    uint32_t shadow_color = 0x00000000; // transparent = no shadow
    bool shadow_inset = false;

    // Multiple box-shadow support
    struct BoxShadowEntry {
        float offset_x = 0;
        float offset_y = 0;
        float blur = 0;
        float spread = 0;
        uint32_t color = 0xFF000000;
        bool inset = false;
    };
    std::vector<BoxShadowEntry> box_shadows;

    // Text shadow: offset_x, offset_y, blur_radius, color (ARGB)
    float text_shadow_offset_x = 0, text_shadow_offset_y = 0, text_shadow_blur = 0;
    uint32_t text_shadow_color = 0x00000000; // transparent = no text shadow

    // Multiple text-shadow support
    struct TextShadowEntry {
        float offset_x = 0;
        float offset_y = 0;
        float blur = 0;
        uint32_t color = 0xFF000000;
    };
    std::vector<TextShadowEntry> text_shadows;

    // Overflow: 0=visible, 1=hidden, 2=scroll, 3=auto
    int overflow = 0;

    // Scroll container tracking
    bool is_scroll_container = false;  // true when overflow != visible (hidden/scroll/auto)
    float scroll_top = 0;             // vertical scroll offset (pixels scrolled from top)
    float scroll_left = 0;            // horizontal scroll offset (pixels scrolled from left)
    float scroll_content_width = 0;   // total width of child content (for scrollbar thumb sizing)
    float scroll_content_height = 0;  // total height of child content (for scrollbar thumb sizing)

    // Overflow scroll indicators (visual cue that content is clipped)
    bool overflow_indicator_bottom = false;  // content overflows bottom
    bool overflow_indicator_right = false;   // content overflows right

    // Text align: 0=left, 1=center, 2=right, 3=justify
    int text_align = 0;
    int text_align_last = 0; // 0=auto, 1=start/left, 2=end/right, 3=center, 4=justify

    // Text indent (first line of block, in px)
    float text_indent = 0;

    // Vertical align: 0=baseline, 1=top, 2=middle, 3=bottom, 4=text-top, 5=text-bottom,
    //                 6=sub, 7=super, 8=length/percentage offset from baseline
    int vertical_align = 0;
    // Pixel offset for length/percentage vertical-align (used when vertical_align == 8).
    // Positive = shift element down from baseline, negative = shift up.
    float vertical_align_offset = 0;

    // White-space handling
    // white_space: 0=normal, 1=nowrap, 2=pre, 3=pre-wrap, 4=pre-line, 5=break-spaces
    int white_space = 0;
    bool white_space_pre = false; // true = preserve whitespace and newlines (pre, pre-wrap, break-spaces)
    bool white_space_nowrap = false; // true = no wrapping (white-space: nowrap, pre)

    // Text overflow: 0=clip, 1=ellipsis, 2=fade
    int text_overflow = 0;

    // Word break: 0=normal, 1=break-all, 2=keep-all
    int word_break = 0;
    // Overflow wrap: 0=normal, 1=break-word, 2=anywhere
    int overflow_wrap = 0;
    // Text wrap: 0=wrap, 1=nowrap, 2=balance, 3=pretty, 4=stable
    int text_wrap = 0;

    // White space collapse: 0=collapse, 1=preserve, 2=preserve-breaks, 3=break-spaces
    int white_space_collapse = 0;
    // Link target (for <a> elements and their descendants)
    std::string link_href;
    std::string link_target; // target attribute (e.g., "_blank")
    // Form submit: index into RenderResult::forms (-1 = not a submit button)
    int form_index = -1;

    // Gradient: type (0=none, 1=linear, 2=radial), angle in degrees (0=to top, 90=to right, 180=to bottom)
    // Color stops: {color_argb, position_0_to_1}
    int gradient_type = 0; // 0=none, 1=linear, 2=radial
    float gradient_angle = 180.0f; // default: top-to-bottom (linear only)
    int radial_shape = 0; // 0=ellipse, 1=circle (radial only)
    std::vector<std::pair<uint32_t, float>> gradient_stops; // empty = no gradient

    // Outline (does NOT affect layout, drawn outside border edge)
    float outline_width = 0;
    uint32_t outline_color = 0xFF000000; // ARGB, default black
    int outline_style = 0; // 0=none, 1=solid, 2=dashed, 3=dotted, 4=double, 5=groove, 6=ridge, 7=inset, 8=outset
    float outline_offset = 0; // extra space between border and outline

    // Float: 0=none, 1=left, 2=right
    int float_type = 0;

    // Clear: 0=none, 1=left, 2=right, 3=both
    int clear_type = 0;

    // Position
    int position_type = 0; // 0=static, 1=relative, 2=absolute, 3=fixed, 4=sticky
    float pos_top = 0, pos_right = 0, pos_bottom = 0, pos_left = 0;
    bool pos_top_set = false, pos_right_set = false, pos_bottom_set = false, pos_left_set = false;

    // Sticky positioning: element stays in normal flow but adjusts position during paint
    // based on scroll offset within its constraint box.
    float sticky_original_y = 0;  // original normal flow Y position (set during layout)
    float sticky_original_x = 0;  // original normal flow X position (set during layout)

    // CSS Transforms
    std::vector<clever::css::Transform> transforms;

    // CSS Filters: {type, value}
    std::vector<std::pair<int, float>> filters;

    // CSS filter: drop-shadow() extra params
    float drop_shadow_ox = 0;
    float drop_shadow_oy = 0;
    uint32_t drop_shadow_color = 0xFF000000;

    // CSS Backdrop Filters: same format as filters, applied to backdrop behind element
    std::vector<std::pair<int, float>> backdrop_filters;

    // Resize: 0=none, 1=both, 2=horizontal, 3=vertical
    int resize = 0;

    int isolation = 0; // 0=auto, 1=isolate
    int contain = 0;   // 0=none, 1=strict, 2=content, 3=size, 4=layout, 5=style, 6=paint

    // Block Formatting Context: set during layout.
    // An element that establishes a new BFC contains its floats, prevents
    // margin collapsing across the boundary, and includes floated children
    // in its height calculation.
    bool establishes_bfc = false;

    // True when CSS display: flow-root is specified — always establishes a BFC
    bool is_flow_root = false;

    // CSS contain-intrinsic-size
    float contain_intrinsic_width = 0;
    float contain_intrinsic_height = 0;

    // Clip path: 0=none, 1=circle, 2=ellipse, 3=inset, 4=polygon, 5=path
    int clip_path_type = 0;
    std::vector<float> clip_path_values;
    std::string clip_path_path_data; // SVG path data string for path()

    // Shape outside: 0=none, 1=circle, 2=ellipse, 3=inset, 4=polygon,
    //                5=margin-box, 6=border-box, 7=padding-box, 8=content-box
    int shape_outside_type = 0;
    std::vector<float> shape_outside_values;

    // Direction: 0=ltr, 1=rtl
    int direction = 0;

    // Line clamp (-1 = unlimited, >0 = max lines)
    int line_clamp = -1;

    // CSS Multi-column layout
    int column_count = -1;
    int column_fill = 0; // 0=balance, 1=auto, 2=balance-all
    float column_width = -1.0f; // -1=auto
    float column_gap_val = 0;
    float column_rule_width = 0;
    uint32_t column_rule_color = 0xFF000000;
    int column_rule_style = 0;

    // Caret & accent colors (ARGB; 0 = auto)
    uint32_t caret_color = 0;  // 0 = auto
    uint32_t accent_color = 0; // 0 = auto

    // Picture element (<picture>)
    bool is_picture = false;  // true for <picture> elements
    std::string picture_srcset; // selected source URL from <picture>/<source>

    // Media elements (<video>, <audio>)
    int media_type = 0;  // 0=none, 1=video, 2=audio
    std::string media_src;

    // Textarea element (<textarea>)
    bool is_textarea = false;
    int textarea_rows = 2;          // rows attribute (default 2)
    int textarea_cols = 20;         // cols attribute (default 20)
    bool textarea_has_content = false; // true when textarea has actual text (not placeholder)

    // Text input (<input type="text/password/email/search/url/tel/number">)
    bool is_text_input = false;
    bool is_password_input = false; // true for type="password" — render bullets

    // Button input (<input type="submit/reset/button"> and <button>)
    bool is_button_input = false;

    // Checkbox and radio input
    bool is_checkbox = false;
    bool is_radio = false;
    bool is_checked = false;
    bool loading_lazy = false;

    // Range input (<input type="range">)
    bool is_range_input = false;
    int input_range_min = 0;
    int input_range_max = 100;
    int input_range_value = 50;

    // Color input (<input type="color">)
    bool is_color_input = false;
    uint32_t color_input_value = 0xFF000000; // ARGB, default black

    // Scroll behavior: 0=auto, 1=smooth
    int scroll_behavior = 0;

    // Scroll snap type: 0=none, 1=x, 2=y, 3=both
    int scroll_snap_type_axis = 0;
    // Scroll snap type strictness: 0=none, 1=mandatory, 2=proximity
    int scroll_snap_type_strictness = 0;

    // Scroll snap align: 0=none, 1=start, 2=center, 3=end
    int scroll_snap_align_x = 0;
    int scroll_snap_align_y = 0;

    // Scroll snap stop: 0=normal, 1=always
    int scroll_snap_stop = 0;

    // Placeholder styling (::placeholder pseudo-element)
    uint32_t placeholder_color = 0xFF757575; // ARGB, default gray (#757575)
    float placeholder_font_size = 0;         // 0 = inherit from parent
    bool placeholder_italic = false;

    // Input/textarea placeholder and value text (for paint inspection/testing)
    std::string placeholder_text;  // the placeholder attribute value
    std::string input_value;       // the value attribute (or textarea text content)

    // Writing mode: 0=horizontal-tb, 1=vertical-rl, 2=vertical-lr
    int writing_mode = 0;

    // CSS Counters (stored as raw string values for now)
    std::string counter_increment;
    std::string counter_reset;
    std::string counter_set;

    // CSS appearance / -webkit-appearance: 0=auto, 1=none, 2=menulist-button, 3=textfield, 4=button
    int appearance = 0;

    // CSS touch-action: 0=auto, 1=none, 2=pan-x, 3=pan-y, 4=pan-x pan-y, 5=manipulation, 6=pinch-zoom
    int touch_action = 0;

    // CSS will-change: "auto" stored as empty string, otherwise the property name(s)
    std::string will_change;

    // Select element (<select>)
    bool is_select_element = false;
    std::string select_display_text; // text of the currently selected <option>
    std::vector<std::string> select_options; // text for each <option> (dropdown mode)
    std::vector<bool> select_option_disabled; // per-option disabled flag (parallel to select_options)
    int select_selected_index = 0; // index of the selected option
    std::string select_name; // form field name attribute
    bool select_is_multiple = false; // true if <select multiple>
    int select_visible_rows = 1; // number of visible rows (from size attr or multiple default)

    // Optgroup element (<optgroup> inside <select>)
    bool is_optgroup = false;
    std::string optgroup_label;  // the label attribute of <optgroup>
    bool optgroup_disabled = false;

    // Option element disabled flag (for listbox child nodes)
    bool is_option_disabled = false;

    // Datalist element
    bool is_datalist = false;
    std::vector<std::string> datalist_options; // option values for autocomplete
    std::string datalist_id; // id for linking input[list] to datalist

    // Details/summary elements
    bool is_summary = false;   // true for <summary> elements
    bool details_open = false;  // whether parent <details> has `open` attribute
    int details_id = -1;       // unique ID for the parent <details> element (-1 = none)

    // Dialog element (<dialog>)
    bool is_dialog = false;
    bool dialog_open = false; // true when `open` attribute is present
    bool dialog_modal = false; // true for modal dialogs (::backdrop rendering)

    // Abbreviation element (<abbr>, <acronym>)
    bool is_abbr = false;
    std::string title_text; // stores the title attribute for tooltip

    bool is_sub = false;   // <sub> subscript
    bool is_sup = false;   // <sup> superscript
    float vertical_offset = 0; // px offset: positive=down (sub), negative=up (sup)

    bool is_wbr = false;   // <wbr> word break opportunity
    bool is_ins = false;   // <ins> inserted text (underline)
    bool is_del = false;   // <del>/<s>/<strike> deleted text (strikethrough)
    bool is_mark = false;  // <mark> highlighted text
    bool is_cite = false;  // <cite> citation (italic)
    bool is_q = false;     // <q> inline quotation
    bool is_bdi = false;   // <bdi> bidirectional isolation
    bool is_bdo = false;   // <bdo> bidirectional override
    bool is_time = false;  // <time> date/time element
    std::string datetime_attr; // <time datetime="..."> machine-readable date

    bool is_dfn = false;   // <dfn> definition element (italic)
    bool is_data = false;  // <data> machine-readable data
    std::string data_value; // <data value="..."> machine-readable value

    bool is_label = false;   // <label> form label element
    std::string label_for;   // <label for="..."> the "for" attribute value linking to an input ID

    bool is_output = false;  // <output> calculation result
    std::string output_for;  // <output for="..."> space-separated list of IDs

    // Ruby annotation elements (<ruby>, <rt>, <rp>)
    bool is_ruby = false;      // <ruby> container
    bool is_ruby_text = false;  // <rt> annotation text
    bool is_ruby_paren = false; // <rp> hidden parenthesis

    // Image map elements (<map>, <area>)
    bool is_map = false;
    std::string map_name;
    bool is_area = false;
    std::string area_shape;   // rect, circle, poly
    std::string area_coords;
    std::string area_href;

    // Progress element (<progress>)
    bool is_progress = false;
    bool progress_indeterminate = false; // true when no value attribute
    float progress_value = 0;   // normalized 0-1 value
    float progress_max = 1;     // max attribute value

    // Meter element (<meter>)
    bool is_meter = false;
    float meter_value = 0;
    float meter_min = 0;
    float meter_max = 1;
    float meter_low = 0;
    float meter_high = 1;
    float meter_optimum = 0.5f;
    uint32_t meter_bar_color = 0xFF4CAF50; // computed bar color based on value vs thresholds

    // Table column group elements (<colgroup>, <col>)
    bool is_colgroup = false;
    bool is_col = false;
    int col_span = 1;  // <col span="N"> — how many columns this col represents
    std::vector<float> col_widths; // collected column widths from col elements, stored on table node

    // Marquee element (<marquee>)
    bool is_address = false;  // <address> contact info (italic block)

    bool is_figure = false;      // <figure> self-contained content
    bool is_figcaption = false;  // <figcaption> caption for <figure>

    bool display_contents = false; // display: contents — no box, children pass through
    bool is_marquee = false;
    int marquee_direction = 0;   // 0=left, 1=right, 2=up, 3=down
    uint32_t marquee_bg_color = 0; // 0 = use default light blue

    // Fieldset/legend elements
    bool is_fieldset = false;   // <fieldset> form group
    bool is_legend = false;     // <legend> caption for fieldset

    // List item properties
    bool is_list_item = false;
    int list_style_type = 0; // 0=disc, 1=circle, 2=square, 3=decimal, 4=decimal-leading-zero, 5=lower-roman, 6=upper-roman, 7=lower-alpha, 8=upper-alpha, 9=none, 10=lower-greek, 11=lower-latin, 12=upper-latin
    int list_item_index = 1; // 1-based index for ordered lists
    std::string list_style_image; // URL for list marker image, empty = none

    // ::marker pseudo-element support
    uint32_t marker_color = 0;       // 0 = use text color
    float marker_font_size = 0;      // 0 = use parent font size

    // ::first-letter pseudo-element support
    bool has_first_letter = false;  // Element has ::first-letter styling
    float first_letter_font_size = 0;  // 0 = inherit
    uint32_t first_letter_color = 0;   // 0 = inherit
    bool first_letter_bold = false;

    // ::first-line pseudo-element support
    bool has_first_line = false;       // Element has ::first-line styling
    float first_line_font_size = 0;    // 0 = inherit
    uint32_t first_line_color = 0;     // 0 = inherit
    bool first_line_bold = false;
    bool first_line_italic = false;

    // ::selection pseudo-element support
    uint32_t selection_color = 0;      // 0 = use default white text
    uint32_t selection_bg_color = 0;   // 0 = use default system blue highlight

    // Hyphens: 0=none, 1=manual (default), 2=auto
    int hyphens = 1;

    // Text justify: 0=auto (default), 1=inter-word, 2=inter-character, 3=none
    int text_justify = 0;

    // Text underline offset (px)
    float text_underline_offset = 0;

    // Text underline position: 0=auto, 1=under, 2=left, 3=right
    int text_underline_position = 0;

    // Font variant: 0=normal, 1=small-caps
    int font_variant = 0;

    // Font variant caps: 0=normal, 1=small-caps, 2=all-small-caps, 3=petite-caps, 4=all-petite-caps, 5=unicase, 6=titling-caps
    int font_variant_caps = 0;

    // Font variant numeric: 0=normal, 1=ordinal, 2=slashed-zero, 3=lining-nums, 4=oldstyle-nums, 5=proportional-nums, 6=tabular-nums
    int font_variant_numeric = 0;

    // Font feature settings (OpenType feature tags)
    std::vector<std::pair<std::string, int>> font_feature_settings;

    // Font variation settings (OpenType variable font axes)
    std::string font_variation_settings;

    // Font optical sizing: 0=auto (default), 1=none
    int font_optical_sizing = 0;

    // Print color adjust / -webkit-print-color-adjust: 0=economy (default), 1=exact
    int print_color_adjust = 0;

    // Font kerning: 0=auto (default), 1=normal, 2=none
    int font_kerning = 0;

    // Font variant ligatures: 0=normal, 1=none, 2=common-ligatures, 3=no-common-ligatures,
    //                         4=discretionary-ligatures, 5=no-discretionary-ligatures
    int font_variant_ligatures = 0;

    // Font variant east-asian: 0=normal, 1=jis78, 2=jis83, 3=jis90, 4=jis04, 5=simplified, 6=traditional, 7=full-width, 8=proportional-width, 9=ruby
    int font_variant_east_asian = 0;

    // Font variant position: 0=normal, 1=sub, 2=super
    int font_variant_position = 0;

    // Font language override: empty string = normal, otherwise a quoted string value
    std::string font_language_override;

    // Font size adjust: 0=none (default), positive=custom aspect value
    float font_size_adjust = 0;

    // Font stretch: 1=ultra-condensed, 2=extra-condensed, 3=condensed, 4=semi-condensed,
    //               5=normal (default), 6=semi-expanded, 7=expanded, 8=extra-expanded, 9=ultra-expanded
    int font_stretch = 5;

    // Text decoration skip ink: 0=auto, 1=none, 2=all
    int text_decoration_skip_ink = 0;

    // Text decoration skip: 0=none, 1=objects, 2=spaces, 3=ink, 4=edges, 5=box-decoration
    int text_decoration_skip = 0;

    // CSS Transitions
    std::string transition_property = "all";
    float transition_duration = 0;  // seconds
    int transition_timing = 0;      // 0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out
                                    // 5=cubic-bezier (custom), 6=steps-end, 7=steps-start
    float transition_delay = 0;     // seconds
    float transition_bezier_x1 = 0, transition_bezier_y1 = 0;
    float transition_bezier_x2 = 1, transition_bezier_y2 = 1;
    int transition_steps_count = 1;

    // CSS Animations
    std::string animation_name;
    float animation_duration = 0;
    int animation_timing = 0;       // 0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out
                                    // 5=cubic-bezier (custom), 6=steps-end, 7=steps-start
    float animation_delay = 0;
    float animation_bezier_x1 = 0, animation_bezier_y1 = 0;
    float animation_bezier_x2 = 1, animation_bezier_y2 = 1;
    int animation_steps_count = 1;
    float animation_iteration_count = 1; // -1 = infinite
    int animation_direction = 0;    // 0=normal, 1=reverse, 2=alternate, 3=alternate-reverse
    int animation_fill_mode = 0;    // 0=none, 1=forwards, 2=backwards, 3=both

    // CSS animation-composition: 0=replace, 1=add, 2=accumulate
    int animation_composition = 0;

    // CSS animation-timeline: "auto" (default), "none", "scroll()", "view()", or custom name
    std::string animation_timeline = "auto";

    // Border image
    std::string border_image_source;      // url or gradient, empty = none
    float border_image_slice = 100;       // percentage, default 100%
    bool border_image_slice_fill = false;  // whether "fill" keyword is present
    float border_image_width_val = 1;     // multiplier, default 1
    float border_image_outset = 0;        // px
    int border_image_repeat = 0;          // 0=stretch, 1=repeat, 2=round, 3=space
    // Pre-decoded border-image URL pixels (from render_pipeline, when source is url())
    std::shared_ptr<std::vector<uint8_t>> border_image_pixels; // RGBA, row-major
    int border_image_img_width = 0;
    int border_image_img_height = 0;
    // Pre-parsed border-image gradient (from render_pipeline)
    int border_image_gradient_type = 0;   // 0=none, 1=linear, 2=radial
    float border_image_gradient_angle = 0;
    int border_image_radial_shape = 0;
    std::vector<std::pair<uint32_t, float>> border_image_gradient_stops;

    // CSS color-scheme: 0=normal, 1=light, 2=dark, 3=light dark
    int color_scheme = 0;

    // CSS Container Queries: container-type and container-name
    int container_type = 0;       // 0=normal, 1=size, 2=inline-size, 3=block-size
    std::string container_name;
    static inline float s_container_width = 0;
    static inline float s_container_height = 0;

    // CSS forced-color-adjust: 0=auto, 1=none, 2=preserve-parent-color
    int forced_color_adjust = 0;

    // CSS Math properties (MathML integration)
    int math_style = 0;  // 0=normal, 1=compact
    int math_depth = 0;  // nesting depth, integer

    // CSS content-visibility: 0=visible, 1=hidden, 2=auto
    int content_visibility = 0;

    // CSS overscroll-behavior: 0=auto, 1=contain, 2=none
    int overscroll_behavior = 0;
    int overscroll_behavior_x = 0;
    int overscroll_behavior_y = 0;

    // CSS paint-order: "normal", "fill", "stroke", "markers", or combinations like "fill stroke markers"
    std::string paint_order = "normal";

    // CSS initial-letter: drop cap / raised cap
    float initial_letter_size = 0;  // 0=normal, >0=number of lines for drop cap
    int initial_letter_sink = 0;    // how many lines to sink, 0=same as size

    // Text emphasis style: string value ("none", "filled dot", "open circle", "filled sesame", etc.)
    std::string text_emphasis_style = "none";
    uint32_t text_emphasis_color = 0; // ARGB, 0 = inherit/currentColor

    // -webkit-text-stroke
    float text_stroke_width = 0;
    uint32_t text_stroke_color = 0xFF000000; // ARGB, default black
    uint32_t text_fill_color = 0; // ARGB, 0 = use node.color

    // Line break: 0=auto, 1=loose, 2=normal, 3=strict, 4=anywhere
    int line_break = 0;

    // Orphans and widows (minimum lines at bottom/top of page/column break)
    int orphans = 2;
    int widows = 2;

    // CSS column-span: 0=none, 1=all
    int column_span = 0;

    // CSS break-before/break-after: 0=auto, 1=avoid, 2=always, 3=page, 4=column, 5=region
    int break_before = 0;
    int break_after = 0;

    // CSS break-inside: 0=auto, 1=avoid, 2=avoid-page, 3=avoid-column, 4=avoid-region
    int break_inside = 0;

    // CSS unicode-bidi: 0=normal, 1=embed, 2=bidi-override, 3=isolate, 4=isolate-override, 5=plaintext
    int unicode_bidi = 0;

    // Scroll margin (px)
    float scroll_margin_top = 0, scroll_margin_right = 0, scroll_margin_bottom = 0, scroll_margin_left = 0;

    // Scroll padding (px)
    float scroll_padding_top = 0, scroll_padding_right = 0, scroll_padding_bottom = 0, scroll_padding_left = 0;

    // Text rendering: 0=auto, 1=optimizeSpeed, 2=optimizeLegibility, 3=geometricPrecision
    int text_rendering = 0;

    // Ruby align: 0=space-around, 1=start, 2=center, 3=space-between
    int ruby_align = 0;

    // Ruby position: 0=over, 1=under, 2=inter-character
    int ruby_position = 0;

    // Ruby overhang: 0=auto, 1=none, 2=start, 3=end
    int ruby_overhang = 0;

    // Text combine upright: 0=none, 1=all, 2=digits
    int text_combine_upright = 0;

    // Text orientation: 0=mixed, 1=upright, 2=sideways
    int text_orientation = 0;

    // CSS cursor: 0=auto, 1=default, 2=pointer, 3=text, 4=move, 5=not-allowed
    int cursor = 0;

    // CSS backface-visibility: 0=visible, 1=hidden
    int backface_visibility = 0;

    // CSS overflow-anchor: 0=auto, 1=none
    int overflow_anchor = 0;
    float overflow_clip_margin = 0.0f;

    // CSS perspective: 0=none, >0=length in px
    float perspective = 0;

    // CSS transform-style: 0=flat, 1=preserve-3d
    int transform_style = 0;

    // CSS transform-box: 0=content-box, 1=border-box, 2=fill-box, 3=stroke-box, 4=view-box
    int transform_box = 1;  // default border-box for HTML elements

    // CSS transform-origin (percentage): default 50% 50%
    float transform_origin_x = 50.0f;
    float transform_origin_y = 50.0f;
    // CSS transform-origin as Length values (for px/em resolution against element box)
    clever::css::Length transform_origin_x_len = clever::css::Length::percent(50.0f);
    clever::css::Length transform_origin_y_len = clever::css::Length::percent(50.0f);
    // CSS transform-origin z-component: default 0px
    float transform_origin_z = 0.0f;

    // CSS perspective-origin (percentage): default 50% 50%
    float perspective_origin_x = 50.0f;
    float perspective_origin_y = 50.0f;
    // CSS perspective-origin as Length values
    clever::css::Length perspective_origin_x_len = clever::css::Length::percent(50.0f);
    clever::css::Length perspective_origin_y_len = clever::css::Length::percent(50.0f);

    // CSS scrollbar-color: 0 = auto, non-zero = explicit ARGB color
    uint32_t scrollbar_thumb_color = 0;
    uint32_t scrollbar_track_color = 0;

    // CSS scrollbar-width: 0=auto, 1=thin, 2=none
    int scrollbar_width = 0;

    // CSS scrollbar-gutter: 0=auto, 1=stable, 2=stable both-edges
    int scrollbar_gutter = 0;

    // CSS Mask properties
    std::string mask_image;      // url or gradient, empty = none
    int mask_size = 0;           // 0=auto, 1=cover, 2=contain, 3=explicit
    float mask_size_width = 0, mask_size_height = 0; // for explicit pixel sizes
    int mask_repeat = 0;         // 0=repeat, 1=repeat-x, 2=repeat-y, 3=no-repeat, 4=space, 5=round

    // CSS Shape Outside (string form), shape-margin, shape-image-threshold
    std::string shape_outside_str; // e.g. "circle(50%)", "ellipse()", "inset(10px)", "polygon(...)", empty = none
    float shape_margin = 0;         // shape-margin in px
    float shape_image_threshold = 0; // shape-image-threshold 0.0-1.0

    // CSS dominant-baseline: 0=auto, 1=text-bottom, 2=alphabetic, 3=ideographic,
    //                       4=middle, 5=central, 6=mathematical, 7=hanging, 8=text-top
    int dominant_baseline = 0;

    // Font synthesis bitmask: 0=none, 1=weight, 2=style, 4=small-caps; default 7 (all enabled)
    int font_synthesis = 7;

    // Font variant alternates: 0=normal, 1=historical-forms
    int font_variant_alternates = 0;

    // CSS initial-letter (float size, 0=normal): how many lines the initial letter spans
    float initial_letter = 0;
    // CSS initial-letter-align: 0=auto, 1=border-box, 2=alphabetic
    int initial_letter_align = 0;

    // CSS quotes: stores quote pairs as a string, empty = auto
    std::string quotes;

    // Page break properties (CSS 2.1 legacy paged media)
    // page-break-before/after: 0=auto, 1=always, 2=avoid, 3=left, 4=right
    int page_break_before = 0;
    int page_break_after = 0;

    int background_clip = 0;       // 0=border-box, 1=padding-box, 2=content-box, 3=text
    int background_origin = 0;     // 0=padding-box, 1=border-box, 2=content-box
    int background_blend_mode = 0; // 0=normal, 1=multiply, 2=screen, 3=overlay, 4=darken, 5=lighten
    int bg_attachment = 0; // 0=scroll, 1=fixed, 2=local
    // page-break-inside: 0=auto, 1=avoid
    int page_break_inside = 0;

    // CSS page property (paged media named page)
    std::string page;

    // Image orientation: 0=from-image (default), 1=none, 2=flip
    int image_orientation = 0;
    // True when image-orientation: flip/none/from-image is set on this element itself.
    bool image_orientation_explicit = false;

    // Font smoothing / -webkit-font-smoothing: 0=auto, 1=none, 2=antialiased, 3=subpixel-antialiased
    int font_smooth = 0;

    // Text size adjust / -webkit-text-size-adjust: "auto", "none", or percentage string (e.g. "100%")
    std::string text_size_adjust = "auto";

    // Logical border radius (CSS Logical Properties)
    float border_start_start_radius = 0; // maps to top-left in LTR horizontal-tb
    float border_start_end_radius = 0;   // maps to top-right in LTR horizontal-tb
    float border_end_start_radius = 0;   // maps to bottom-left in LTR horizontal-tb
    float border_end_end_radius = 0;     // maps to bottom-right in LTR horizontal-tb

    // CSS offset-path: "none", "path('M0 0L100 100')", etc.
    std::string offset_path = "none";

    // CSS offset-distance: float px/percentage, default 0
    float offset_distance = 0.0f;

    // CSS offset-rotate: "auto", "0deg", "auto 45deg", etc.
    std::string offset_rotate = "auto";

    // CSS offset shorthand: stores the whole string
    std::string offset;

    // CSS offset-anchor: "auto", "50% 50%", etc.
    std::string offset_anchor = "auto";

    // CSS offset-position: "normal", "auto", "50% 50%", etc.
    std::string offset_position = "normal";

    // CSS transition-behavior: 0=normal, 1=allow-discrete
    int transition_behavior = 0;

    // CSS animation-range: "normal", "entry", "exit", etc.
    std::string animation_range = "normal";

    // CSS individual transform properties (CSS Transforms Level 2)
    std::string css_rotate = "none";     // e.g. "45deg", "x 30deg", "none"
    std::string css_scale = "none";      // e.g. "1.5", "2 3", "none"
    std::string css_translate = "none";  // e.g. "10px 20px", "50%", "none"

    // CSS color-interpolation: 0=auto, 1=sRGB, 2=linearRGB
    int color_interpolation = 0;

    // CSS mask-composite: 0=add, 1=subtract, 2=intersect, 3=exclude
    int mask_composite = 0;

    // CSS mask-mode: 0=match-source, 1=alpha, 2=luminance
    int mask_mode = 0;

    // CSS mask shorthand: stores the whole string
    std::string mask_shorthand;

    // CSS mask-origin: 0=border-box, 1=padding-box, 2=content-box
    int mask_origin = 0;

    // CSS mask-position: string value (like background-position)
    std::string mask_position = "0% 0%";

    // CSS mask-clip: 0=border-box, 1=padding-box, 2=content-box, 3=no-clip
    int mask_clip = 0;

    // CSS mask-border (stores full value as raw string)
    std::string mask_border;

    // SVG marker shorthand: stores the whole string
    std::string marker_shorthand;

    // SVG marker-start: url() reference or "none"
    std::string marker_start;

    // SVG marker-mid: url() reference or "none"
    std::string marker_mid;

    // SVG marker-end: url() reference or "none"
    std::string marker_end;

    // Font palette: "normal" (default), "light", "dark", or custom string
    std::string font_palette = "normal";

    // CSS overflow-block: 0=visible, 1=hidden, 2=scroll, 3=auto, 4=clip
    int overflow_block = 0;

    // CSS overflow-inline: 0=visible, 1=hidden, 2=scroll, 3=auto, 4=clip
    int overflow_inline = 0;

    // CSS box-decoration-break: 0=slice, 1=clone
    int box_decoration_break = 0;

    // CSS margin-trim: 0=none, 1=block, 2=inline, 3=block-start, 4=block-end, 5=inline-start, 6=inline-end
    int margin_trim = 0;

    // CSS all shorthand: "initial"/"inherit"/"unset"/"revert", "" = not set
    std::string css_all = "";

    // CSS Custom Properties (CSS Variables) — inherited from ComputedStyle
    std::unordered_map<std::string, std::string> custom_properties;

    // Back-pointer to source DOM node (SimpleNode*) for JS geometry queries
    void* dom_node = nullptr;

    // True for synthetic anonymous block boxes created by the layout engine
    // to wrap consecutive inline children inside a mixed block/inline container.
    // Anonymous blocks have no tag_name and transparent background (CSS 2.1 §9.2.1.1).
    bool is_anonymous = false;

    // Sticky constraint caching for paint-time calculations
    float sticky_container_top = 0;
    float sticky_container_bottom = 0;
    float sticky_container_left = 0;
    float sticky_container_right = 0;
    float sticky_container_width = 0;
    float sticky_container_height = 0;
    float sticky_max_top = 0;
    float sticky_max_bottom = 0;

    // Tree
    LayoutNode* parent = nullptr;
    std::vector<std::unique_ptr<LayoutNode>> children;

    LayoutNode* append_child(std::unique_ptr<LayoutNode> child);
};

} // namespace clever::layout
