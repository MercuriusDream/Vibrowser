#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace clever::css {

// Forward declaration for calc() expression tree
struct CalcExpr;

enum class Display {
    Block, Inline, InlineBlock, Flex, InlineFlex, None,
    ListItem, Table, TableRow, TableCell, TableHeaderGroup, TableRowGroup,
    Grid, InlineGrid, Contents
};

enum class Position { Static, Relative, Absolute, Fixed, Sticky };
enum class Float { None, Left, Right };
enum class Clear { None, Left, Right, Both };
enum class BoxSizing { ContentBox, BorderBox };
enum class TextAlign { Left, Right, Center, Justify, WebkitCenter };
enum class TextDecoration { None, Underline, Overline, LineThrough };
enum class TextDecorationStyle { Solid, Dashed, Dotted, Wavy, Double };
enum class UserSelect { Auto, None, Text, All };
enum class PointerEvents {
    Auto = 0, None = 1, VisiblePainted = 2, VisibleFill = 3,
    VisibleStroke = 4, Visible = 5, Painted = 6, Fill = 7,
    Stroke = 8, All = 9
};
enum class ListStylePosition { Outside, Inside };
enum class TextTransform { None, Capitalize, Uppercase, Lowercase };
enum class FontStyle { Normal, Italic, Oblique };
enum class FontWeight { Normal = 400, Bold = 700 };
enum class WhiteSpace { Normal, NoWrap, Pre, PreWrap, PreLine, BreakSpaces };
enum class Overflow { Visible, Hidden, Scroll, Auto };
enum class Visibility { Visible, Hidden, Collapse };
enum class FlexDirection { Row, RowReverse, Column, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignItems { FlexStart, FlexEnd, Center, Baseline, Stretch };
enum class Cursor { Auto, Default, Pointer, Text, Move, NotAllowed };
enum class ListStyleType {
    Disc = 0, Circle = 1, Square = 2, Decimal = 3,
    DecimalLeadingZero = 4, LowerRoman = 5, UpperRoman = 6,
    LowerAlpha = 7, UpperAlpha = 8, None = 9,
    LowerGreek = 10, LowerLatin = 11, UpperLatin = 12,
    Armenian = 13, Georgian = 14, CjkDecimal = 15
};
enum class BorderStyle { None, Solid, Dashed, Dotted, Double, Groove, Ridge, Inset, Outset };
enum class TextOverflow { Clip, Ellipsis, Fade };
enum class VerticalAlign { Baseline, Top, Middle, Bottom, TextTop, TextBottom };
enum class Direction { Ltr, Rtl };

// CSS transform function types
enum class TransformType { None, Translate, Rotate, Scale, Skew, Matrix };

struct Transform {
    TransformType type = TransformType::None;
    float x = 0;    // translate: x offset (px), scale: x factor, skew: x angle (deg)
    float y = 0;    // translate: y offset (px), scale: y factor, skew: y angle (deg)
    float angle = 0; // rotate: angle in degrees
    // matrix(a, b, c, d, e, f) parameters
    float m[6] = {1, 0, 0, 1, 0, 0}; // a, b, c, d, e(tx), f(ty)
};

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }

    static Color black() { return {0, 0, 0, 255}; }
    static Color white() { return {255, 255, 255, 255}; }
    static Color transparent() { return {0, 0, 0, 0}; }
};

struct Length {
    enum class Unit { Px, Em, Rem, Percent, Vw, Vh, Auto, Zero, Calc, Ch, Lh, Vmin, Vmax,
                      Cqw, Cqh, Cqi, Cqb, Cqmin, Cqmax };
    float value = 0;
    Unit unit = Unit::Px;
    std::shared_ptr<CalcExpr> calc_expr;  // non-null when unit == Calc

    // Viewport dimensions for vw/vh/vmin/vmax units (set once per render)
    static inline float s_viewport_w = 800;
    static inline float s_viewport_h = 600;
    static void set_viewport(float w, float h) { s_viewport_w = w; s_viewport_h = h; }

    static Length px(float v) { return {v, Unit::Px, nullptr}; }
    static Length em(float v) { return {v, Unit::Em, nullptr}; }
    static Length rem(float v) { return {v, Unit::Rem, nullptr}; }
    static Length percent(float v) { return {v, Unit::Percent, nullptr}; }
    static Length ch(float v) { return {v, Unit::Ch, nullptr}; }
    static Length lh(float v) { return {v, Unit::Lh, nullptr}; }
    static Length vw(float v) { return {v, Unit::Vw, nullptr}; }
    static Length vh(float v) { return {v, Unit::Vh, nullptr}; }
    static Length vmin(float v) { return {v, Unit::Vmin, nullptr}; }
    static Length vmax(float v) { return {v, Unit::Vmax, nullptr}; }
    static Length cqw(float v) { return {v, Unit::Cqw, nullptr}; }
    static Length cqh(float v) { return {v, Unit::Cqh, nullptr}; }
    static Length cqi(float v) { return {v, Unit::Cqi, nullptr}; }
    static Length cqb(float v) { return {v, Unit::Cqb, nullptr}; }
    static Length cqmin(float v) { return {v, Unit::Cqmin, nullptr}; }
    static Length cqmax(float v) { return {v, Unit::Cqmax, nullptr}; }
    static Length auto_val() { return {0, Unit::Auto, nullptr}; }
    static Length zero() { return {0, Unit::Zero, nullptr}; }
    static Length calc(std::shared_ptr<CalcExpr> expr);

    bool is_auto() const { return unit == Unit::Auto; }
    bool is_zero() const { return (unit == Unit::Zero) || (value == 0 && unit != Unit::Auto && unit != Unit::Calc); }

    float to_px(float parent_value = 0, float root_font_size = 16, float line_height = 0) const;
};

// Expression tree node for CSS calc(), min(), max(), clamp()
struct CalcExpr {
    enum class Op { Value, Add, Sub, Mul, Div, Min, Max,
                     Mod, Rem, Abs, Sign,
                     RoundNearest, RoundUp, RoundDown, RoundToZero,
                     Sin, Cos, Tan, Asin, Acos, Atan, Atan2,
                     Sqrt, Pow, Hypot, Exp, Log };
    Op op = Op::Value;
    Length leaf;  // used when op == Value
    std::shared_ptr<CalcExpr> left;
    std::shared_ptr<CalcExpr> right;

    // Evaluate this expression given context values
    float evaluate(float parent_value, float root_font_size, float line_height = 0) const;

    // Factory helpers
    static std::shared_ptr<CalcExpr> make_value(const Length& l) {
        auto e = std::make_shared<CalcExpr>();
        e->op = Op::Value;
        e->leaf = l;
        return e;
    }
    static std::shared_ptr<CalcExpr> make_binary(Op op,
            std::shared_ptr<CalcExpr> lhs,
            std::shared_ptr<CalcExpr> rhs) {
        auto e = std::make_shared<CalcExpr>();
        e->op = op;
        e->left = std::move(lhs);
        e->right = std::move(rhs);
        return e;
    }
    static std::shared_ptr<CalcExpr> make_unary(Op op,
            std::shared_ptr<CalcExpr> arg) {
        auto e = std::make_shared<CalcExpr>();
        e->op = op;
        e->left = std::move(arg);
        return e;
    }
};

struct EdgeSizes {
    Length top, right, bottom, left;
};

struct BorderEdge {
    Length width = Length::zero();
    BorderStyle style = BorderStyle::None;
    Color color = Color::black();
};

// CSS Transition definition: parsed from transition shorthand or longhands
struct TransitionDef {
    std::string property;         // "opacity", "transform", "all", etc.
    float duration_ms = 0;        // duration in milliseconds
    float delay_ms = 0;           // delay in milliseconds
    int timing_function = 0;      // 0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out
                                  // 5=cubic-bezier (custom), 6=steps-end, 7=steps-start
    // Custom cubic-bezier control points (used when timing_function == 5)
    float bezier_x1 = 0, bezier_y1 = 0, bezier_x2 = 1, bezier_y2 = 1;
    // Steps parameters (used when timing_function == 6 or 7)
    int steps_count = 1;
};

// CSS @keyframes animation step (parsed from @keyframes rule body)
struct KeyframeStep {
    float offset = 0.0f;          // 0.0 = from, 1.0 = to, 0.5 = 50%, etc.
    std::map<std::string, std::string> properties; // CSS property -> value
};

// CSS @keyframes animation definition
struct KeyframeAnimation {
    std::string name;
    std::vector<KeyframeStep> steps;
};

struct ComputedStyle {
    // Display & Position
    Display display = Display::Inline;
    Position position = Position::Static;
    Float float_val = Float::None;
    Clear clear = Clear::None;
    BoxSizing box_sizing = BoxSizing::ContentBox;
    bool is_flow_root = false; // display: flow-root — always establishes BFC

    // Sizing
    // width_keyword/height_keyword: 0=normal, -2=min-content, -3=max-content, -4=fit-content
    int width_keyword = 0;
    int height_keyword = 0;
    Length width = Length::auto_val();
    Length height = Length::auto_val();
    Length min_width = Length::zero();
    Length max_width = Length::px(std::numeric_limits<float>::max());
    Length min_height = Length::zero();
    Length max_height = Length::px(std::numeric_limits<float>::max());
    float aspect_ratio = 0; // 0 = none, >0 = width/height ratio
    bool aspect_ratio_is_auto = false; // true for "auto" or "auto <ratio>"

    // Margin, Padding
    EdgeSizes margin = {Length::zero(), Length::zero(), Length::zero(), Length::zero()};
    EdgeSizes padding = {Length::zero(), Length::zero(), Length::zero(), Length::zero()};

    // Border
    BorderEdge border_top, border_right, border_bottom, border_left;

    // Positioning
    Length top = Length::auto_val();
    Length right_pos = Length::auto_val();
    Length bottom = Length::auto_val();
    Length left_pos = Length::auto_val();
    int z_index = 0;

    // Text
    Color color = Color::black();
    std::string font_family = "sans-serif";
    Length font_size = Length::px(16);
    int font_weight = 400;
    FontStyle font_style = FontStyle::Normal;
    Length line_height = Length::px(1.2f * 16);
    float line_height_unitless = 1.2f; // 0 = not unitless (explicit px/em/%), >0 = unitless factor
    TextAlign text_align = TextAlign::Left;
    int text_align_last = 0; // 0=auto, 1=start/left, 2=end/right, 3=center, 4=justify
    Direction direction = Direction::Ltr;
    TextDecoration text_decoration = TextDecoration::None;
    int text_decoration_bits = 0; // bitmask: 1=underline, 2=overline, 4=line-through
    Color text_decoration_color = {0, 0, 0, 0}; // {0,0,0,0} = use currentColor
    TextDecorationStyle text_decoration_style = TextDecorationStyle::Solid;
    float text_decoration_thickness = 0; // 0 = auto (1px)
    TextTransform text_transform = TextTransform::None;
    WhiteSpace white_space = WhiteSpace::Normal;
    Length letter_spacing = Length::zero();
    Length word_spacing = Length::zero();

    // Text indent
    Length text_indent = Length::zero();

    // Border radius
    float border_radius = 0;
    float border_radius_tl = 0; // top-left
    float border_radius_tr = 0; // top-right
    float border_radius_bl = 0; // bottom-left
    float border_radius_br = 0; // bottom-right

    // Box shadow (legacy single shadow fields kept for backward compat)
    float shadow_offset_x = 0, shadow_offset_y = 0, shadow_blur = 0;
    float shadow_spread = 0;
    Color shadow_color = Color::transparent();
    bool shadow_inset = false;

    // Multiple box-shadow support
    struct BoxShadowEntry {
        float offset_x = 0;
        float offset_y = 0;
        float blur = 0;
        float spread = 0;
        Color color = Color::transparent();
        bool inset = false;
    };
    std::vector<BoxShadowEntry> box_shadows;

    // Text shadow
    float text_shadow_offset_x = 0, text_shadow_offset_y = 0, text_shadow_blur = 0;
    Color text_shadow_color = Color::transparent();

    // Multiple text-shadow support
    struct TextShadowEntry {
        float offset_x = 0;
        float offset_y = 0;
        float blur = 0;
        Color color = Color::transparent();
    };
    std::vector<TextShadowEntry> text_shadows;

    // Visual
    Color background_color = Color::transparent();
    int gradient_type = 0; // 0=none, 1=linear, 2=radial
    float gradient_angle = 180.0f;
    int radial_shape = 0; // 0=ellipse, 1=circle
    std::vector<std::pair<uint32_t, float>> gradient_stops; // {argb, position_0_to_1}
    std::string bg_image_url; // background-image: url(...)
    int background_size = 0; // 0=auto, 1=cover, 2=contain, 3=explicit (use bg_size_width/height)
    float bg_size_width = 0, bg_size_height = 0; // for explicit pixel/percent sizes
    bool bg_size_width_pct = false;  // true if bg_size_width is a percentage value
    bool bg_size_height_pct = false; // true if bg_size_height is a percentage value
    bool bg_size_height_auto = false; // true if bg-size height is 'auto' (maintain aspect ratio)
    int background_repeat = 0; // 0=repeat, 1=repeat-x, 2=repeat-y, 3=no-repeat
    int background_position_x = 0; // 0=left, 1=center, 2=right (or use bg_position_x_val)
    int background_position_y = 0; // 0=top, 1=center, 2=bottom (or use bg_position_y_val)
    float bg_position_x_val = 0;    // numeric background-position-x value (px or pct)
    float bg_position_y_val = 0;    // numeric background-position-y value (px or pct)
    bool bg_position_x_pct = false; // true if bg_position_x_val is a percentage
    bool bg_position_y_pct = false; // true if bg_position_y_val is a percentage
    float opacity = 1.0f;
    // Mix blend mode: 0=normal, 1=multiply, 2=screen, 3=overlay, 4=darken, 5=lighten, 6=color-dodge, 7=color-burn, 8=hard-light, 9=soft-light, 10=difference, 11=exclusion
    int mix_blend_mode = 0;
    Visibility visibility = Visibility::Visible;
    Overflow overflow_x = Overflow::Visible;
    Overflow overflow_y = Overflow::Visible;

    // Object fit (for <img> elements)
    int object_fit = 0; // 0=fill, 1=contain, 2=cover, 3=none, 4=scale-down

    // Object position (for <img> elements)
    float object_position_x = 50.0f; // percentage, default 50% (center)
    float object_position_y = 50.0f; // percentage, default 50% (center)

    // Image rendering: 0=auto, 1=smooth, 2=high-quality, 3=crisp-edges, 4=pixelated
    int image_rendering = 0;

    // Hanging punctuation: 0=none, 1=first, 2=last, 3=force-end, 4=allow-end, 5=first last
    int hanging_punctuation = 0;

    // Flexbox
    FlexDirection flex_direction = FlexDirection::Row;
    FlexWrap flex_wrap = FlexWrap::NoWrap;
    JustifyContent justify_content = JustifyContent::FlexStart;
    AlignItems align_items = AlignItems::Stretch;
    int align_self = -1; // -1=auto (inherit from parent align-items), 0-4 same as AlignItems
    int justify_self = -1; // -1=auto, 0=start, 1=end, 2=center, 3=baseline, 4=stretch
    float flex_grow = 0;
    float flex_shrink = 1;
    Length flex_basis = Length::auto_val();
    int order = 0;
    Length gap = Length::zero();

    // CSS Grid layout
    std::string grid_template_columns;
    std::string grid_template_rows;
    bool grid_template_columns_is_subgrid = false;
    bool grid_template_rows_is_subgrid = false;
    std::string grid_column; // e.g. "1 / 3"
    std::string grid_row;    // e.g. "1 / 2"
    std::string grid_column_start; // individual longhand, e.g. "2"
    std::string grid_column_end;   // individual longhand, e.g. "4"
    std::string grid_row_start;    // individual longhand, e.g. "1"
    std::string grid_row_end;      // individual longhand, e.g. "3"
    std::string grid_auto_rows;       // e.g. "100px", "minmax(100px, auto)"
    std::string grid_auto_columns;    // e.g. "1fr", "200px"
    std::string grid_template_areas;  // e.g. "\"header header\" \"sidebar main\""
    std::string grid_area;            // e.g. "header" or "1 / 2 / 3 / 4"
    int grid_auto_flow = 0;   // 0=row, 1=column, 2=row dense, 3=column dense
    int justify_items = 3;   // 0=start, 1=end, 2=center, 3=stretch (default)
    int align_content = 0;   // 0=start, 1=end, 2=center, 3=stretch, 4=space-between, 5=space-around

    // List
    ListStyleType list_style_type = ListStyleType::Disc;
    ListStylePosition list_style_position = ListStylePosition::Outside;
    std::string list_style_image; // URL for list marker image, empty = none

    // Border collapse (for tables)
    bool border_collapse = false; // false=separate, true=collapse

    // Border spacing (CSS border-spacing, in px; two-value: horizontal vertical)
    float border_spacing = 2.0f;   // horizontal spacing (CSS default ~2px)
    float border_spacing_v = 0.0f; // vertical spacing (0 = use horizontal value)

    // Table layout: 0=auto, 1=fixed
    int table_layout = 0;

    // Caption side: 0=top, 1=bottom
    int caption_side = 0;

    // Empty cells: 0=show, 1=hide
    int empty_cells = 0;

    // Pointer events
    PointerEvents pointer_events = PointerEvents::Auto;

    // User select
    UserSelect user_select = UserSelect::Auto;

    // Tab size (number of spaces)
    int tab_size = 4;

    // Cursor
    Cursor cursor = Cursor::Auto;

    // Text overflow
    TextOverflow text_overflow = TextOverflow::Clip;

    // Word break and overflow wrap
    int word_break = 0;    // 0=normal, 1=break-all, 2=keep-all
    int overflow_wrap = 0; // 0=normal, 1=break-word, 2=anywhere

    // Text wrap: 0=wrap, 1=nowrap, 2=balance, 3=pretty, 4=stable
    int text_wrap = 0;

    // White space collapse: 0=collapse, 1=preserve, 2=preserve-breaks, 3=break-spaces
    int white_space_collapse = 0;
    // Vertical align
    VerticalAlign vertical_align = VerticalAlign::Baseline;
    float vertical_align_offset = 0; // offset in px for length/percentage vertical-align values

    // Outline (does NOT affect layout, drawn outside border edge)
    Length outline_width = Length::zero();
    BorderStyle outline_style = BorderStyle::None;
    Color outline_color = Color::black();
    Length outline_offset = Length::zero();

    // Content (for ::before / ::after pseudo-elements)
    // Empty string means not set; "none" means content: none (suppress generation)
    std::string content;

    // When content uses attr(), store the attribute name here for runtime resolution
    std::string content_attr_name;

    // CSS Transforms
    std::vector<Transform> transforms;

    // CSS Filters: {type, value}
    // type: 1=grayscale, 2=sepia, 3=brightness, 4=contrast, 5=invert,
    //       6=saturate, 7=opacity-filter, 8=hue-rotate, 9=blur, 10=drop-shadow
    std::vector<std::pair<int, float>> filters;

    // CSS Backdrop Filters: same format as filters, applied to backdrop behind element
    std::vector<std::pair<int, float>> backdrop_filters;

    // CSS filter: drop-shadow() params (stored separately since filter only has type+value)
    float drop_shadow_ox = 0;
    float drop_shadow_oy = 0;
    uint32_t drop_shadow_color = 0xFF000000;

    // Resize: 0=none, 1=both, 2=horizontal, 3=vertical
    int resize = 0;

    // CSS isolation: 0=auto, 1=isolate
    int isolation = 0;
    // CSS contain: 0=none, 1=strict, 2=content, 3=size, 4=layout, 5=style, 6=paint
    int contain = 0;

    // CSS contain-intrinsic-size
    float contain_intrinsic_width = 0;
    float contain_intrinsic_height = 0;

    // Clip path: 0=none, 1=circle, 2=ellipse, 3=inset, 4=polygon, 5=path
    int clip_path_type = 0;
    // For circle: clip_path_values[0] = radius (px or %)
    // For inset: clip_path_values = {top, right, bottom, left} in px
    // For ellipse: clip_path_values = {rx, ry} in px
    std::vector<float> clip_path_values;
    std::string clip_path_path_data; // SVG path data string for path()

    // Shape outside: 0=none, 1=circle, 2=ellipse, 3=inset, 4=polygon,
    //                5=margin-box, 6=border-box, 7=padding-box, 8=content-box
    int shape_outside_type = 0;
    std::vector<float> shape_outside_values;

    // Line clamp (-1 = unlimited)
    int line_clamp = -1;

    // CSS Multi-column layout
    int column_count = -1; // -1=auto, >0=explicit count
    int column_fill = 0;  // 0=balance, 1=auto, 2=balance-all
    Length column_width = Length::auto_val(); // column width
    Length column_gap_val = Length::px(0); // gap between columns (renamed to avoid conflict with flex gap)
    float column_rule_width = 0; // rule line width
    Color column_rule_color = Color::black();
    int column_rule_style = 0; // 0=none, 1=solid, 2=dashed, 3=dotted

    // Caret & accent colors
    Color caret_color = {0, 0, 0, 0};  // {0,0,0,0} = auto (use currentColor)
    uint32_t accent_color = 0; // ARGB, 0 = auto (browser default)

    // Scroll behavior: 0=auto, 1=smooth
    int scroll_behavior = 0;

    // CSS scroll-snap-type axis: 0=none, 1=x/inline, 2=y/block, 3=both
    int scroll_snap_type_axis = 0;
    // CSS scroll-snap-type strictness: 0=auto default, 1=mandatory, 2=proximity
    int scroll_snap_type_strictness = 0;

    // CSS scroll-snap-align: 0=none, 1=start, 2=center, 3=end
    int scroll_snap_align_x = 0;
    int scroll_snap_align_y = 0;

    // Scroll snap stop: 0=normal, 1=always
    int scroll_snap_stop = 0;

    // Placeholder pseudo-element color: {0,0,0,0} = auto/default gray
    Color placeholder_color = {0, 0, 0, 0};

    // Writing mode: 0=horizontal-tb, 1=vertical-rl, 2=vertical-lr
    int writing_mode = 0;

    // CSS Counters (stored as raw string values for now)
    std::string counter_increment; // e.g. "section 1"
    std::string counter_reset;     // e.g. "section 0"
    std::string counter_set;       // e.g. "section 5"

    // CSS appearance / -webkit-appearance: 0=auto, 1=none, 2=menulist-button, 3=textfield, 4=button
    int appearance = 0;

    // CSS touch-action: 0=auto, 1=none, 2=pan-x (left/right), 3=pan-y (up/down),
    //                   4=pan-x pan-y, 5=manipulation, 6=pinch-zoom
    int touch_action = 0;

    // CSS will-change: "auto" stored as empty string, otherwise the property name(s)
    std::string will_change;

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

    // Font feature settings (OpenType feature tags), e.g. ("liga", 1), ("kern", 1)
    std::vector<std::pair<std::string, int>> font_feature_settings;

    // Font variation settings (OpenType variable font axes, e.g. ""wght" 700")
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

    int font_display = 0;

    // Font stretch: 1=ultra-condensed, 2=extra-condensed, 3=condensed, 4=semi-condensed,
    //               5=normal (default), 6=semi-expanded, 7=expanded, 8=extra-expanded, 9=ultra-expanded
    int font_stretch = 5;

    // Text decoration skip ink: 0=auto, 1=none, 2=all
    int text_decoration_skip_ink = 0;

    // Text decoration skip: 0=none, 1=objects, 2=spaces, 3=ink, 4=edges, 5=box-decoration
    int text_decoration_skip = 0;

    // CSS Transitions (legacy scalar fields for backward compat)
    std::string transition_property = "all";
    float transition_duration = 0;  // seconds
    int transition_timing = 0;      // 0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out
                                    // 5=cubic-bezier (custom), 6=steps-end, 7=steps-start
    float transition_delay = 0;     // seconds
    // Custom cubic-bezier control points for transition (timing == 5)
    float transition_bezier_x1 = 0, transition_bezier_y1 = 0;
    float transition_bezier_x2 = 1, transition_bezier_y2 = 1;
    // Steps parameters for transition (timing == 6 or 7)
    int transition_steps_count = 1;

    // Parsed transition definitions (supports multiple comma-separated transitions)
    std::vector<TransitionDef> transitions;

    // CSS Animations
    std::string animation_name;
    float animation_duration = 0;
    int animation_timing = 0;       // 0=ease, 1=linear, 2=ease-in, 3=ease-out, 4=ease-in-out
                                    // 5=cubic-bezier (custom), 6=steps-end, 7=steps-start
    float animation_delay = 0;
    // Custom cubic-bezier control points for animation (timing == 5)
    float animation_bezier_x1 = 0, animation_bezier_y1 = 0;
    float animation_bezier_x2 = 1, animation_bezier_y2 = 1;
    // Steps parameters for animation (timing == 6 or 7)
    int animation_steps_count = 1;
    float animation_iteration_count = 1; // -1 = infinite
    int animation_direction = 0;    // 0=normal, 1=reverse, 2=alternate, 3=alternate-reverse
    int animation_fill_mode = 0;    // 0=none, 1=forwards, 2=backwards, 3=both
    int animation_play_state = 0;   // 0=running, 1=paused

    // CSS animation-composition: 0=replace, 1=add, 2=accumulate
    int animation_composition = 0;

    // CSS animation-timeline: "auto" (default), "none", "scroll()", "view()", or custom name
    std::string animation_timeline = "auto";
    int animation_timeline_type = 0;  // 0=auto, 1=none, 2=scroll(), 3=view()
    int animation_timeline_axis = 0;  // 0=block, 1=inline, 2=x, 3=y
    std::string animation_timeline_raw;  // For scroll()/view() parameters

    // Border image
    std::string border_image_source;      // url or gradient, empty = none
    float border_image_slice = 100;       // percentage, default 100%
    bool border_image_slice_fill = false;  // whether "fill" keyword is present
    float border_image_width_val = 1;     // multiplier, default 1
    float border_image_outset = 0;        // px
    int border_image_repeat = 0;          // 0=stretch, 1=repeat, 2=round, 3=space

    // CSS color-scheme: 0=normal, 1=light, 2=dark, 3=light dark
    int color_scheme = 0;

    // CSS Container Queries: container-type and container-name
    int container_type = 0;       // 0=normal, 1=size, 2=inline-size, 3=block-size
    std::string container_name;

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
    // Text emphasis position: 0=over right (default), 1=under right, 2=over left, 3=under left
    int text_emphasis_position = 0;

    // -webkit-text-stroke
    float text_stroke_width = 0;
    Color text_stroke_color;  // default black

    // -webkit-text-fill-color (overrides color for text fill)
    Color text_fill_color = Color::transparent();  // a=0 means use 'color'

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
    // When unit is Percent, use transform_origin_x/y percentage floats above.
    // When unit is Px (or other absolute), resolve as absolute offset from the element's top-left.
    Length transform_origin_x_len = Length::percent(50.0f);
    Length transform_origin_y_len = Length::percent(50.0f);
    // CSS transform-origin z-component (3D): default 0 (px)
    float transform_origin_z = 0.0f;

    // CSS perspective-origin (percentage): default 50% 50%
    float perspective_origin_x = 50.0f;
    float perspective_origin_y = 50.0f;
    // CSS perspective-origin as Length values
    Length perspective_origin_x_len = Length::percent(50.0f);
    Length perspective_origin_y_len = Length::percent(50.0f);

    // SVG fill / stroke / opacity (CSS properties for SVG elements)
    uint32_t svg_fill_color = 0xFF000000;   // ARGB, default black
    bool svg_fill_none = false;
    uint32_t svg_stroke_color = 0xFF000000;  // ARGB, default black
    bool svg_stroke_none = true;             // default: no stroke
    float svg_fill_opacity = 1.0f;
    float svg_stroke_opacity = 1.0f;
    float svg_stroke_width = 0;              // 0 = not set
    int svg_stroke_linecap = 0;              // 0=butt, 1=round, 2=square
    int svg_stroke_linejoin = 0;             // 0=miter, 1=round, 2=bevel
    std::string svg_stroke_dasharray_str;    // raw dash pattern string
    int svg_text_anchor = 0;                 // 0=start, 1=middle, 2=end
    int fill_rule = 0;                       // 0=nonzero, 1=evenodd
    int clip_rule = 0;                       // 0=nonzero, 1=evenodd
    float stroke_miterlimit = 4.0f;          // SVG stroke-miterlimit (default 4)
    int shape_rendering = 0;                 // 0=auto, 1=optimizeSpeed, 2=crispEdges, 3=geometricPrecision
    int vector_effect = 0;                   // 0=none, 1=non-scaling-stroke
    uint32_t stop_color = 0xFF000000;        // SVG stop-color (ARGB, default black)
    float stop_opacity = 1.0f;              // SVG stop-opacity (0.0 to 1.0)
    uint32_t flood_color = 0xFF000000;       // SVG flood-color (ARGB, default black)
    float flood_opacity = 1.0f;             // SVG flood-opacity (0.0 to 1.0)
    uint32_t lighting_color = 0xFFFFFFFF;    // SVG lighting-color (ARGB, default white)

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

    // Page break properties (CSS 2.1 legacy paged media)
    // page-break-before/after: 0=auto, 1=always, 2=avoid, 3=left, 4=right
    int page_break_before = 0;
    int page_break_after = 0;
    // page-break-inside: 0=auto, 1=avoid
    int page_break_inside = 0;

    // CSS quotes: stores quote pairs as a string (e.g. "\"\xc2\xab\" \"\xc2\xbb\""), empty = auto

    int background_clip = 0;       // 0=border-box, 1=padding-box, 2=content-box, 3=text
    int background_origin = 0;     // 0=padding-box, 1=border-box, 2=content-box
    int background_blend_mode = 0; // 0=normal, 1=multiply, 2=screen, 3=overlay, 4=darken, 5=lighten
    int background_attachment = 0; // 0=scroll, 1=fixed, 2=local
    std::string quotes;

    // Image orientation: 0=from-image (default), 1=none, 2=flip
    int image_orientation = 0;
    // True when this element explicitly sets image-orientation (not inherited).
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
    Length animation_range_start = Length::percent(0);
    float animation_range_start_offset = 0.0f;  // 0.0-1.0 for percentage offsets
    Length animation_range_end = Length::percent(100);
    float animation_range_end_offset = 1.0f;  // 0.0-1.0 for percentage offsets

    // CSS View Transitions
    std::string view_transition_name;  // empty = none

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

    // CSS box-decoration-break / -webkit-box-decoration-break: 0=slice, 1=clone
    int box_decoration_break = 0;

    // CSS all shorthand: "initial"/"inherit"/"unset"/"revert", "" = not set
    std::string css_all = "";
    // CSS ::selection pseudo-element support
    Color selection_color = Color::black();
    Color selection_background_color = Color::transparent();
    std::vector<TextShadowEntry> selection_text_shadows;

    // CSS margin-trim: 0=none, 1=block, 2=inline, 3=block-start, 4=block-end, 5=inline-start, 6=inline-end
    int margin_trim = 0;

    // CSS page property (paged media named page)
    std::string page;

    // CSS Custom Properties (CSS Variables)
    std::unordered_map<std::string, std::string> custom_properties;
};

// Keyframe rule: a single stop in a @keyframes definition (resolved to 0.0-1.0 offset)
struct KeyframeStop {
    float offset;                           // 0.0 = from (0%), 1.0 = to (100%)
    ComputedStyle style;                    // resolved style at this stop
    std::vector<std::pair<std::string,std::string>> declarations; // raw property:value pairs
};

// A complete @keyframes definition with a name and a list of stops
struct KeyframesDefinition {
    std::string name;
    std::vector<KeyframeStop> rules;
};

// Get default computed style for an element tag
ComputedStyle default_style_for_tag(const std::string& tag);

} // namespace clever::css
