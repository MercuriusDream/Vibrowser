#include <clever/css/style/computed_style.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace clever::css {

Length Length::calc(std::shared_ptr<CalcExpr> expr) {
    Length l;
    l.value = 0;
    l.unit = Unit::Calc;
    l.calc_expr = std::move(expr);
    return l;
}

float Length::to_px(float parent_value, float root_font_size, float line_height) const {
    switch (unit) {
        case Unit::Px:
            return value;
        case Unit::Em:
            return value * parent_value;
        case Unit::Rem:
            return value * root_font_size;
        case Unit::Percent:
            return (value / 100.0f) * parent_value;
        case Unit::Vw:
            return (value / 100.0f) * s_viewport_w;
        case Unit::Vh:
            return (value / 100.0f) * s_viewport_h;
        case Unit::Vmin:
            return (value / 100.0f) * std::min(s_viewport_w, s_viewport_h);
        case Unit::Vmax:
            return (value / 100.0f) * std::max(s_viewport_w, s_viewport_h);
        case Unit::Ch:
            // 1ch ≈ advance width of "0" glyph ≈ 0.6 * font-size
            return value * parent_value * 0.6f;
        case Unit::Lh:
            // 1lh = computed line-height of the element
            return value * (line_height > 0 ? line_height : parent_value * 1.2f);
        case Unit::Auto:
            return 0;
        case Unit::Zero:
            return 0;
        case Unit::Calc:
            if (calc_expr) {
                return calc_expr->evaluate(parent_value, root_font_size);
            }
            return 0;
    }
    return 0;
}

float CalcExpr::evaluate(float parent_value, float root_font_size) const {
    switch (op) {
        case Op::Value:
            return leaf.to_px(parent_value, root_font_size);
        case Op::Add: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return l + r;
        }
        case Op::Sub: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return l - r;
        }
        case Op::Mul: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return l * r;
        }
        case Op::Div: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            if (r == 0) return 0;
            return l / r;
        }
        case Op::Min: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return std::min(l, r);
        }
        case Op::Max: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return std::max(l, r);
        }
        case Op::Mod: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            if (r == 0) return 0;
            // CSS mod(): result has sign of divisor
            float result = std::fmod(l, r);
            if ((result > 0 && r < 0) || (result < 0 && r > 0))
                result += r;
            return result;
        }
        case Op::Rem: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            if (r == 0) return 0;
            return std::fmod(l, r); // sign of dividend
        }
        case Op::Abs: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::abs(l);
        }
        case Op::Sign: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            if (l > 0) return 1.0f;
            if (l < 0) return -1.0f;
            return 0.0f;
        }
        case Op::RoundNearest: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 1;
            if (r == 0) return 0;
            return std::round(l / r) * r;
        }
        case Op::RoundUp: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 1;
            if (r == 0) return 0;
            return std::ceil(l / r) * r;
        }
        case Op::RoundDown: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 1;
            if (r == 0) return 0;
            return std::floor(l / r) * r;
        }
        case Op::RoundToZero: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 1;
            if (r == 0) return 0;
            return std::trunc(l / r) * r;
        }
        case Op::Sin: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::sin(l);
        }
        case Op::Cos: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::cos(l);
        }
        case Op::Tan: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::tan(l);
        }
        case Op::Asin: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::asin(std::clamp(l, -1.0f, 1.0f));
        }
        case Op::Acos: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::acos(std::clamp(l, -1.0f, 1.0f));
        }
        case Op::Atan: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::atan(l);
        }
        case Op::Atan2: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return std::atan2(l, r);
        }
        case Op::Sqrt: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return l >= 0 ? std::sqrt(l) : 0;
        }
        case Op::Pow: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return std::pow(l, r);
        }
        case Op::Hypot: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            float r = right ? right->evaluate(parent_value, root_font_size) : 0;
            return std::hypot(l, r);
        }
        case Op::Exp: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return std::exp(l);
        }
        case Op::Log: {
            float l = left ? left->evaluate(parent_value, root_font_size) : 0;
            return l > 0 ? std::log(l) : 0;
        }
    }
    return 0;
}

ComputedStyle default_style_for_tag(const std::string& tag) {
    ComputedStyle style;

    // Block-level elements
    static const std::unordered_set<std::string> block_elements = {
        "html", "body", "div", "section", "article", "aside", "nav", "header",
        "footer", "main", "p", "blockquote", "pre", "figure", "figcaption",
        "address", "details", "summary", "dialog", "dd", "dt", "dl",
        "fieldset", "form", "hr", "noscript", "search", "menu",
        "h1", "h2", "h3", "h4", "h5", "h6",
        "ul", "ol", "li"
    };

    // Inline elements
    static const std::unordered_set<std::string> inline_elements = {
        "span", "a", "em", "strong", "i", "b", "u", "s", "small", "big",
        "sub", "sup", "abbr", "acronym", "cite", "code", "kbd", "mark", "q",
        "samp", "var", "time", "label", "br", "wbr", "img",
        "ruby", "rt", "rp", "ins", "del", "strike",
        "bdi", "bdo", "dfn", "data", "output"
    };

    if (block_elements.count(tag)) {
        style.display = Display::Block;
    } else if (inline_elements.count(tag)) {
        style.display = Display::Inline;
    } else if (tag == "table") {
        style.display = Display::Table;
    } else if (tag == "tr") {
        style.display = Display::TableRow;
    } else if (tag == "td" || tag == "th") {
        style.display = Display::TableCell;
    } else if (tag == "thead") {
        style.display = Display::TableHeaderGroup;
    } else if (tag == "tbody" || tag == "tfoot") {
        style.display = Display::TableRowGroup;
    } else if (tag == "math") {
        style.display = Display::InlineBlock;
    } else {
        // Unknown elements default to inline
        style.display = Display::Inline;
    }

    // Heading font sizes (relative to default 16px)
    if (tag == "h1") {
        style.font_size = Length::px(32.0f);   // 2em
        style.font_weight = 700;
    } else if (tag == "h2") {
        style.font_size = Length::px(24.0f);   // 1.5em
        style.font_weight = 700;
    } else if (tag == "h3") {
        style.font_size = Length::px(18.72f);  // 1.17em
        style.font_weight = 700;
    } else if (tag == "h4") {
        style.font_size = Length::px(16.0f);   // 1em
        style.font_weight = 700;
    } else if (tag == "h5") {
        style.font_size = Length::px(13.28f);  // 0.83em
        style.font_weight = 700;
    } else if (tag == "h6") {
        style.font_size = Length::px(10.72f);  // 0.67em
        style.font_weight = 700;
    }

    // Bold elements
    if (tag == "strong" || tag == "b" || tag == "th") {
        style.font_weight = 700;
    }

    // Italic elements
    if (tag == "em" || tag == "i" || tag == "cite" || tag == "var" || tag == "dfn") {
        style.font_style = FontStyle::Italic;
    }

    // Anchor elements
    if (tag == "a") {
        style.text_decoration = TextDecoration::Underline;
        style.text_decoration_bits = 1; // underline bit
        style.cursor = Cursor::Pointer;
        style.color = {0, 0, 238, 255};  // Default link blue
    }

    // Underline/strikethrough
    if (tag == "u" || tag == "ins") {
        style.text_decoration = TextDecoration::Underline;
        style.text_decoration_bits = 1; // underline bit
    }
    if (tag == "s" || tag == "del" || tag == "strike") {
        style.text_decoration = TextDecoration::LineThrough;
        style.text_decoration_bits = 4; // line-through bit
    }

    // List items
    if (tag == "li") {
        style.display = Display::ListItem;
    }
    if (tag == "ul" || tag == "menu") {
        style.list_style_type = ListStyleType::Disc;
    }
    if (tag == "ol") {
        style.list_style_type = ListStyleType::Decimal;
    }

    // Monospace elements
    if (tag == "code" || tag == "kbd" || tag == "samp" || tag == "pre") {
        style.font_family = "monospace";
    }
    if (tag == "pre") {
        style.white_space = WhiteSpace::Pre;
    }

    // Small element
    if (tag == "small") {
        style.font_size = Length::px(13.0f);
    }

    // Subscript and superscript
    if (tag == "sub" || tag == "sup") {
        style.font_size = Length::px(12.0f);
        style.vertical_align = (tag == "sub") ? VerticalAlign::Bottom : VerticalAlign::Top;
    }

    // Hidden elements
    if (tag == "head" || tag == "meta" || tag == "link" || tag == "script" ||
        tag == "style" || tag == "title") {
        style.display = Display::None;
    }

    // Colgroup/col: not inline or block — treated specially in render pipeline
    // They default to Inline from the fallback, which is fine since render_pipeline
    // overrides their display to None after creating their nodes in the tree.

    // Horizontal rule
    if (tag == "hr") {
        style.border_top.style = BorderStyle::Solid;
        style.border_top.width = Length::px(1);
        style.border_top.color = Color{128, 128, 128, 255};
    }

    return style;
}

} // namespace clever::css
