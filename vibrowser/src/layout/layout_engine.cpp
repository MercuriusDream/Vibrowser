#include <clever/layout/layout_engine.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

namespace clever::layout {

namespace {

float collapse_vertical_margins(float first, float second) {
    if (first >= 0 && second >= 0) return std::max(first, second);
    if (first < 0 && second < 0) return std::min(first, second);
    return first + second;
}

bool is_empty_block_for_margin_collapse(const LayoutNode& node) {
    if (node.mode != LayoutMode::Block || node.display == DisplayType::InlineBlock) return false;
    if (node.specified_height >= 0 || node.min_height > 0) return false;
    if (node.geometry.border.top != 0 || node.geometry.border.right != 0 ||
        node.geometry.border.bottom != 0 || node.geometry.border.left != 0) {
        return false;
    }
    if (node.geometry.padding.top != 0 || node.geometry.padding.right != 0 ||
        node.geometry.padding.bottom != 0 || node.geometry.padding.left != 0) {
        return false;
    }
    if (node.is_text && !node.text_content.empty()) return false;

    for (const auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
        if (child->position_type == 2 || child->position_type == 3) continue;
        if (child->float_type != 0) continue;
        return false;
    }

    return std::abs(node.geometry.height) < 0.0001f;
}

std::string collapse_whitespace(const std::string& text, int white_space, bool white_space_pre) {
    if (text.empty()) return text;
    if (white_space_pre || white_space == 2 || white_space == 3 || white_space == 5) {
        return text;
    }
    std::string result;
    result.reserve(text.size());
    if (white_space == 0 || white_space == 1) {
        bool in_space = false;
        for (char c : text) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!in_space) {
                    result.push_back(' ');
                    in_space = true;
                }
            } else {
                result.push_back(c);
                in_space = false;
            }
        }
        return result;
    }
    if (white_space == 4) {
        bool at_line_start = true;
        bool previous_space = false;
        for (char c : text) {
            if (c == '\n') {
                result.push_back('\n');
                at_line_start = true;
                previous_space = false;
            } else if (c == '\t' || c == ' ') {
                if (!at_line_start && !previous_space) {
                    result.push_back(' ');
                    previous_space = true;
                }
            } else {
                result.push_back(c);
                at_line_start = false;
                previous_space = false;
            }
        }
        return result;
    }
    return text;
}

} // namespace

// ---------------------------------------------------------------------------
// Text measurement helpers (delegate to platform callback or fallback to 0.6f)
// ---------------------------------------------------------------------------

float LayoutEngine::measure_text(const std::string& text, float font_size,
                                  const std::string& font_family, int font_weight,
                                  bool is_italic, float letter_spacing) const {
    if (text_measurer_ && !text.empty()) {
        return text_measurer_(text, font_size, font_family, font_weight, is_italic, letter_spacing);
    }
    // Fallback: approximate
    float char_w = font_size * 0.6f + letter_spacing;
    return static_cast<float>(text.size()) * char_w;
}

float LayoutEngine::avg_char_width(float font_size, const std::string& font_family,
                                    int font_weight, bool is_italic, bool is_monospace,
                                    float letter_spacing) const {
    if (text_measurer_) {
        // Measure a representative string to get average char width
        float measured;
        if (is_monospace) {
            measured = text_measurer_("M", font_size, font_family, font_weight, is_italic, letter_spacing);
        } else {
            // Measure lowercase alphabet + space to get average proportional char width
            static const std::string sample = "abcdefghijklmnopqrstuvwxyz ";
            measured = text_measurer_(sample, font_size, font_family, font_weight, is_italic, letter_spacing) / 27.0f;
        }
        // Clamp to minimum 1.0 to prevent division-by-zero in layout calculations
        return std::max(1.0f, measured);
    }
    // Fallback: approximate
    float result = font_size * (is_monospace ? 0.65f : 0.6f) + letter_spacing;
    // Clamp to minimum 1.0 to prevent division-by-zero in layout calculations
    return std::max(1.0f, result);
}

// ---------------------------------------------------------------------------

// Measure intrinsic content width for min-content/max-content sizing
static float measure_intrinsic_width(const LayoutNode& node, bool max_content,
                                      const TextMeasureFn* measurer, int depth = 0) {
    if (depth > 256) return 0;
    float width = 0;
    if (node.specified_width >= 0) {
        width = std::max(width, node.specified_width);
    }
    if (node.is_text && !node.text_content.empty()) {
        if (max_content) {
            // max-content: no wrapping, full text width
            if (measurer && *measurer) {
                width = (*measurer)(node.text_content, node.font_size, node.font_family,
                                    node.font_weight, node.font_italic, node.letter_spacing);
            } else {
                float char_w = node.font_size * 0.6f;
                width = static_cast<float>(node.text_content.size()) * char_w;
            }
        } else {
            // min-content: longest word — measure each word individually
            float longest_word = 0;
            if (measurer && *measurer) {
                std::string cur_word;
                for (char c : node.text_content) {
                    if (c == ' ' || c == '\t' || c == '\n') {
                        if (!cur_word.empty()) {
                            float w = (*measurer)(cur_word, node.font_size, node.font_family,
                                                  node.font_weight, node.font_italic, node.letter_spacing);
                            longest_word = std::max(longest_word, w);
                            cur_word.clear();
                        }
                    } else {
                        cur_word += c;
                    }
                }
                if (!cur_word.empty()) {
                    float w = (*measurer)(cur_word, node.font_size, node.font_family,
                                          node.font_weight, node.font_italic, node.letter_spacing);
                    longest_word = std::max(longest_word, w);
                }
            } else {
                float char_w = node.font_size * 0.6f;
                float cur_word = 0;
                for (char c : node.text_content) {
                    if (c == ' ' || c == '\t' || c == '\n') {
                        longest_word = std::max(longest_word, cur_word);
                        cur_word = 0;
                    } else {
                        cur_word += char_w;
                    }
                }
                longest_word = std::max(longest_word, cur_word);
            }
            width = longest_word;
        }
    }
    // Recurse into children and accumulate
    float children_width = 0;
    if (node.contain == 3) {
        if (node.contain_intrinsic_width > 0) {
            children_width = node.contain_intrinsic_width;
        }
    } else {
        for (auto& child : node.children) {
            if (child->content_visibility == 1) continue;
            float cw = measure_intrinsic_width(*child, max_content, measurer, depth + 1);
            if (child->mode == LayoutMode::Inline || child->display == DisplayType::Inline ||
                child->display == DisplayType::InlineBlock) {
                children_width += cw; // inline: sum widths
            } else {
                children_width = std::max(children_width, cw); // block: max width
            }
        }
    }
    float padding_border = node.geometry.padding.left + node.geometry.padding.right +
                           node.geometry.border.left + node.geometry.border.right;
    return std::max(width, children_width) + padding_border;
}

// Measure intrinsic content height for min-content/max-content sizing
// For height, min-content and max-content both resolve to the natural content height
// (the height needed to display content without overflow).
// The difference only matters when content could reflow vertically, which is rare.
static float measure_intrinsic_height(const LayoutNode& node, bool max_content,
                                       const TextMeasureFn* measurer, int depth = 0) {
    if (depth > 256) return 0;
    float height = 0;
    if (node.is_text && !node.text_content.empty()) {
        float line_h = node.font_size * 1.2f; // approximate line height
        if (max_content) {
            // max-content height: single line of text
            height = line_h;
        } else {
            // min-content height: text wrapped to its min-content width (longest word)
            // Count the number of lines needed when wrapped to longest-word width
            float char_w;
            if (measurer && *measurer) {
                // Use average char width for wrapping calculations
                static const std::string sample = "abcdefghijklmnopqrstuvwxyz ";
                char_w = (*measurer)(sample, node.font_size, node.font_family,
                                     node.font_weight, node.font_italic, node.letter_spacing) / 27.0f;
            } else {
                char_w = node.font_size * 0.6f;
            }
            float longest_word = 0;
            float cur_word = 0;
            for (char c : node.text_content) {
                if (c == ' ' || c == '\t' || c == '\n') {
                    longest_word = std::max(longest_word, cur_word);
                    cur_word = 0;
                } else {
                    cur_word += char_w;
                }
            }
            longest_word = std::max(longest_word, cur_word);
            // Count words to approximate lines when wrapped at longest-word width
            float total_text_w = static_cast<float>(node.text_content.size()) * char_w;
            if (longest_word > 0) {
                int lines = std::max(1, static_cast<int>(std::ceil(total_text_w / longest_word)));
                height = static_cast<float>(lines) * line_h;
            } else {
                height = line_h;
            }
        }
    }
    // Recurse into children and accumulate
    float children_height = 0;
    for (auto& child : node.children) {
        float ch = measure_intrinsic_height(*child, max_content, measurer, depth + 1);
        if (child->mode == LayoutMode::Inline || child->display == DisplayType::Inline ||
            child->display == DisplayType::InlineBlock) {
            children_height = std::max(children_height, ch); // inline: max height (same line)
        } else {
            children_height += ch; // block: sum heights (stacked)
        }
    }
    float padding_border = node.geometry.padding.top + node.geometry.padding.bottom +
                           node.geometry.border.top + node.geometry.border.bottom;
    return std::max(height, children_height) + padding_border;
}

void LayoutEngine::compute(LayoutNode& root, float viewport_width, float viewport_height) {
    viewport_width_ = viewport_width;
    viewport_height_ = viewport_height;
    root.geometry.x = 0;
    root.geometry.y = 0;

    // Resolve intrinsic sizing keywords for root: -2=min-content, -3=max-content, -4=fit-content
    if (root.specified_width <= -2 && root.specified_width >= -4) {
        int kw = static_cast<int>(root.specified_width);
        const TextMeasureFn* mp = text_measurer_ ? &text_measurer_ : nullptr;
        float min_w = measure_intrinsic_width(root, false, mp);
        float max_w = measure_intrinsic_width(root, true, mp);
        if (kw == -2) {
            root.specified_width = min_w;
        } else if (kw == -3) {
            root.specified_width = max_w;
        } else { // -4 = fit-content
            root.specified_width = std::max(min_w, std::min(viewport_width, max_w));
        }
    }

    // Resolve deferred calc/percent min/max for root
    if (root.css_min_width.has_value()) {
        root.min_width = root.css_min_width->to_px(viewport_width);
    }
    if (root.css_max_width.has_value()) {
        root.max_width = root.css_max_width->to_px(viewport_width);
    }

    // Root width: specified (capped at viewport), or viewport, with min/max
    float w;
    if (root.specified_width >= 0) {
        w = std::min(root.specified_width, viewport_width);
    } else {
        w = viewport_width;
    }
    w = std::max(w, root.min_width);
    w = std::min(w, root.max_width);
    // Also cap max_width at viewport
    w = std::min(w, viewport_width);
    root.geometry.width = w;

    switch (root.mode) {
        case LayoutMode::Block:
        case LayoutMode::InlineBlock:
            layout_block(root, viewport_width);
            break;
        case LayoutMode::Flex:
            layout_flex(root, viewport_width);
            break;
        case LayoutMode::Grid:
            layout_grid(root, viewport_width);
            break;
        case LayoutMode::Table:
            layout_table(root, viewport_width);
            break;
        case LayoutMode::Inline:
            layout_inline(root, viewport_width);
            break;
        case LayoutMode::None:
            root.geometry.width = 0;
            root.geometry.height = 0;
            return;
        default:
            layout_block(root, viewport_width);
            break;
    }
}

float LayoutEngine::compute_width(LayoutNode& node, float containing_width) {
    // For a block element:
    //   If specified_width is set, use it.
    //   Otherwise, width = containing_width - horizontal margins.
    //   (padding and border do NOT reduce the content width; they are inside the box)

    // Resolve deferred calc/percent width now that we know containing_width
    if (node.css_width.has_value()) {
        float resolved = node.css_width->to_px(containing_width);
        node.specified_width = resolved;
    }

    // Resolve deferred calc/percent min/max-width
    if (node.css_min_width.has_value()) {
        node.min_width = node.css_min_width->to_px(containing_width);
    }
    if (node.css_max_width.has_value()) {
        node.max_width = node.css_max_width->to_px(containing_width);
    }

    // contain: size (3) or strict (1) — use contain-intrinsic-size if no explicit width
    if ((node.contain == 3 || node.contain == 1) && node.specified_width < 0 && node.contain_intrinsic_width > 0) {
        node.specified_width = node.contain_intrinsic_width;
    }

    // Resolve intrinsic sizing keywords: -2=min-content, -3=max-content, -4=fit-content
    if (node.specified_width <= -2 && node.specified_width >= -4) {
        int kw = static_cast<int>(node.specified_width);
        const TextMeasureFn* mp = text_measurer_ ? &text_measurer_ : nullptr;
        float min_w = measure_intrinsic_width(node, false, mp);
        float max_w = measure_intrinsic_width(node, true, mp);
        if (kw == -2) {
            node.specified_width = min_w;
        } else if (kw == -3) {
            node.specified_width = max_w;
        } else { // -4 = fit-content
            node.specified_width = std::max(min_w, std::min(containing_width, max_w));
        }
    }

    float w;
    if (node.specified_width >= 0) {
        w = node.specified_width;
        // border-box: specified width includes padding and border.
        // In this engine, geometry.width is used as the outer content dimension
        // (content_w = width - padding - border for children), so for border-box
        // we keep specified_width as-is — layout_block's content_w calculation
        // will naturally give children the correct content area.
        // No adjustment needed here; it matches the engine convention.
    } else if (node.specified_width < 0 && node.specified_height >= 0 && node.aspect_ratio > 0) {
        // Width is auto, height is set, and aspect-ratio exists: compute width from height
        w = node.specified_height * node.aspect_ratio;
    } else {
        float horiz_margins = 0;
        // Only subtract non-auto margins
        if (!is_margin_auto(node.geometry.margin.left)) horiz_margins += node.geometry.margin.left;
        if (!is_margin_auto(node.geometry.margin.right)) horiz_margins += node.geometry.margin.right;
        w = containing_width - horiz_margins;
    }
    w = std::max(w, node.min_width);
    w = std::min(w, node.max_width);
    return std::max(w, 0.0f);
}

float LayoutEngine::compute_height(LayoutNode& node, float containing_height) {
    // Determine the containing block's height for percentage resolution.
    // In CSS, percentage heights only resolve when the containing block has
    // a definite (explicitly set) height. If containing_height is not provided
    // (<0), try to get it from the parent node or the viewport.
    float cb_height = containing_height;
    if (cb_height < 0) {
        // Walk up to the parent: if it has a definite height, use it
        if (node.parent && node.parent->specified_height >= 0) {
            cb_height = node.parent->specified_height;
        } else if (!node.parent) {
            // Root element: resolve against viewport height
            cb_height = viewport_height_;
        } else {
            // Parent has auto height: percentage heights resolve to auto (0)
            cb_height = 0;
        }
    }

    // Resolve deferred calc/percent height
    if (node.css_height.has_value()) {
        float resolved = node.css_height->to_px(cb_height);
        node.specified_height = resolved;
    }

    // Resolve deferred calc/percent min/max-height
    if (node.css_min_height.has_value()) {
        node.min_height = node.css_min_height->to_px(cb_height);
    }
    if (node.css_max_height.has_value()) {
        node.max_height = node.css_max_height->to_px(cb_height);
    }

    // Resolve intrinsic sizing keywords: -2=min-content, -3=max-content, -4=fit-content
    if (node.specified_height <= -2 && node.specified_height >= -4) {
        int kw = static_cast<int>(node.specified_height);
        const TextMeasureFn* mhp = text_measurer_ ? &text_measurer_ : nullptr;
        float min_h = measure_intrinsic_height(node, false, mhp);
        float max_h = measure_intrinsic_height(node, true, mhp);
        if (kw == -2) {
            node.specified_height = min_h;
        } else if (kw == -3) {
            node.specified_height = max_h;
        } else { // -4 = fit-content
            // For height, fit-content is essentially max-content since
            // height doesn't have an "available space" constraint like width does
            node.specified_height = max_h;
        }
    }

    if (node.specified_height >= 0) {
        float h = node.specified_height;
        // border-box: specified height includes padding and border
        // No adjustment needed — matches engine convention where geometry.height
        // is the outer content dimension
        h = std::max(h, node.min_height);
        h = std::min(h, node.max_height);
        return h;
    }
    return -1; // signal: compute from children
}

void LayoutEngine::layout_block(LayoutNode& node, float containing_width) {
    if (node.display == DisplayType::None || node.mode == LayoutMode::None) {
        node.geometry.width = 0;
        node.geometry.height = 0;
        return;
    }

    // Determine if this node establishes a new Block Formatting Context (BFC).
    // A BFC contains floats, prevents parent-child margin collapsing across the
    // boundary, and includes floated descendants in its height.
    node.establishes_bfc =
        node.parent == nullptr ||                   // root element
        node.tag_name == "html" ||                  // root element by tag
        node.float_type != 0 ||                     // floated elements
        node.position_type >= 2 ||                  // absolute (2) or fixed (3)
        node.display == DisplayType::InlineBlock ||
        node.display == DisplayType::Flex ||
        node.display == DisplayType::InlineFlex ||
        node.display == DisplayType::Grid ||
        node.display == DisplayType::InlineGrid ||
        node.display == DisplayType::Table ||
        node.display == DisplayType::TableCell ||
        node.overflow >= 1 ||                       // hidden(1), scroll(2), auto(3)
        node.is_flow_root ||                        // display: flow-root
        node.contain == 1 ||                        // strict
        node.contain == 4 ||                        // layout
        node.contain == 6;                          // paint

    // contain: size (3) or strict (1) — use contain-intrinsic-size for auto dimensions
    if (node.contain == 3 || node.contain == 1) {
        if (node.specified_height < 0 && node.contain_intrinsic_height > 0) {
            node.specified_height = node.contain_intrinsic_height;
        }
    }

    // Resolve deferred percentage/calc margins against containing block width
    // (CSS spec: percentage margins resolve against containing block's WIDTH, even for top/bottom)
    if (node.css_margin_top.has_value())
        node.geometry.margin.top = node.css_margin_top->to_px(containing_width);
    if (node.css_margin_right.has_value())
        node.geometry.margin.right = node.css_margin_right->to_px(containing_width);
    if (node.css_margin_bottom.has_value())
        node.geometry.margin.bottom = node.css_margin_bottom->to_px(containing_width);
    if (node.css_margin_left.has_value())
        node.geometry.margin.left = node.css_margin_left->to_px(containing_width);

    // Resolve deferred percentage/calc padding against containing block width
    if (node.css_padding_top.has_value())
        node.geometry.padding.top = node.css_padding_top->to_px(containing_width);
    if (node.css_padding_right.has_value())
        node.geometry.padding.right = node.css_padding_right->to_px(containing_width);
    if (node.css_padding_bottom.has_value())
        node.geometry.padding.bottom = node.css_padding_bottom->to_px(containing_width);
    if (node.css_padding_left.has_value())
        node.geometry.padding.left = node.css_padding_left->to_px(containing_width);

    // Compute width FIRST (resolves css_width percentage) — needed before auto margins
    if (node.parent != nullptr) {
        node.geometry.width = compute_width(node, containing_width);
    }

    // Resolve auto margins for centering (AFTER width is resolved)
    bool auto_left = is_margin_auto(node.geometry.margin.left);
    bool auto_right = is_margin_auto(node.geometry.margin.right);
    if (auto_left || auto_right) {
        float remaining = containing_width - node.geometry.width;
        if (!auto_left) remaining -= node.geometry.margin.left;
        if (!auto_right) remaining -= node.geometry.margin.right;
        if (remaining < 0) remaining = 0;
        if (auto_left && auto_right) {
            node.geometry.margin.left = remaining / 2.0f;
            node.geometry.margin.right = remaining / 2.0f;
        } else if (auto_left) {
            node.geometry.margin.left = remaining;
        } else {
            node.geometry.margin.right = remaining;
        }
    }

    // Set x from left margin
    if (node.parent != nullptr) {
        node.geometry.x = node.geometry.margin.left;
    }

    // Content area width for children = width - padding - border
    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;
    if (content_w < 0) content_w = 0;

    // Determine children layout mode (skip absolute/fixed — they're out of flow)
    bool has_inline_children = false;
    bool has_block_children = false;
    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
        if (child->content_visibility == 1) continue;
        if (child->position_type == 2 || child->position_type == 3) continue;
        bool inline_like = child->is_text
            || child->mode == LayoutMode::Inline
            || child->mode == LayoutMode::InlineBlock
            || child->display == DisplayType::Inline
            || child->display == DisplayType::InlineBlock
            || child->display == DisplayType::InlineFlex
            || child->display == DisplayType::InlineGrid;
        if (inline_like) {
            has_inline_children = true;
        } else {
            has_block_children = true;
        }
    }

    float children_height = 0;

    if (has_inline_children && !has_block_children) {
        // Pure inline formatting context
        position_inline_children(node, content_w);
        // Compute children height from inline layout
        for (auto& child : node.children) {
            if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
            if (child->content_visibility == 1) continue;
            if (child->position_type == 2 || child->position_type == 3) continue;
            float bottom = child->geometry.y + child->geometry.margin_box_height();
            if (bottom > children_height) children_height = bottom;
        }
    } else if (has_inline_children && has_block_children) {
        // CSS 2.1 §9.2.1.1: Mixed block/inline content.
        // Wrap consecutive inline children into anonymous block boxes so that
        // each inline run gets its own inline formatting context, and block
        // children remain in the normal block flow.
        wrap_anonymous_blocks(node);
        // After wrapping, all in-flow children are block-level.
        position_block_children(node);
        for (auto& child : node.children) {
            if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
            if (child->content_visibility == 1) continue;
            if (child->position_type == 2 || child->position_type == 3) continue;
            if (child->float_type != 0) {
                if (node.establishes_bfc) {
                    float float_bottom = child->geometry.y + child->geometry.border_box_height()
                                         + child->geometry.margin.bottom;
                    children_height = std::max(children_height, float_bottom);
                }
                continue;
            }
            children_height += child->geometry.margin_box_height();
        }
    } else {
        // Pure block formatting context (no inline children)
        position_block_children(node);
        for (auto& child : node.children) {
            if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
            if (child->content_visibility == 1) continue;
            if (child->position_type == 2 || child->position_type == 3) continue;
            // Floats are out of normal flow — only include them in height if
            // the parent establishes a BFC (which contains its floats)
            if (child->float_type != 0) {
                if (node.establishes_bfc) {
                    float float_bottom = child->geometry.y + child->geometry.border_box_height()
                                         + child->geometry.margin.bottom;
                    children_height = std::max(children_height, float_bottom);
                }
                continue; // floats don't contribute to normal-flow height sum
            }
            children_height += child->geometry.margin_box_height();
        }
    }

    // Apply CSS multi-column layout (column-count / column-width)
    if (int ncols = [&]() {
            float gap = node.column_gap_val;
            if (gap < 0) gap = 0;

            int cols_from_width = 0;
            if (node.column_width > 0) {
                cols_from_width = static_cast<int>(std::floor((content_w + gap) / (node.column_width + gap)));
                if (cols_from_width < 1) cols_from_width = 1;
            }

            int cols_from_count = 0;
            if (node.column_count > 0) {
                cols_from_count = node.column_count;
            }

            if (cols_from_width > 0 && cols_from_count > 0) return std::min(cols_from_width, cols_from_count);
            if (cols_from_width > 0) return cols_from_width;
            return cols_from_count;
        }(); ncols > 1 && content_w > 0 && children_height > 0) {
        float gap = node.column_gap_val;
        if (gap < 0) gap = 0;

        float col_w = (content_w - gap * static_cast<float>(ncols - 1)) / static_cast<float>(ncols);
        if (node.column_width > 0) {
            col_w = node.column_width;
            float used_w = col_w * static_cast<float>(ncols) + gap * static_cast<float>(ncols - 1);
            if (used_w < content_w) {
                col_w += (content_w - used_w) / static_cast<float>(ncols);
            }
        }
        if (col_w < 10) col_w = 10; // minimum column width

        // Collect visible children
        std::vector<LayoutNode*> visible_children;
        for (auto& child : node.children) {
            if (child->display == DisplayType::None || child->mode == LayoutMode::None) continue;
            if (child->content_visibility == 1) continue;
            if (child->position_type == 2 || child->position_type == 3) continue;
            visible_children.push_back(child.get());
        }

        // Calculate total height of non-spanning children for target
        float non_span_height = 0;
        for (auto* child : visible_children) {
            if (child->column_span != 1) {
                non_span_height += child->geometry.margin_box_height();
            }
        }

        float target_col_h = non_span_height / static_cast<float>(ncols);
        if (target_col_h < 20) target_col_h = non_span_height;

        int current_col = 0;
        float col_y = 0;
        float max_col_h = 0;
        float total_y = 0; // tracks overall y position for spanning elements

        for (auto* child : visible_children) {
            float child_h = child->geometry.margin_box_height();

            // column-span: all — span full width, break out of column flow
            if (child->column_span == 1) {
                // Finalize current column segment
                if (col_y > max_col_h) max_col_h = col_y;
                total_y += max_col_h;

                // Position spanning element at full width
                child->geometry.x = child->geometry.margin.left;
                child->geometry.y = total_y + child->geometry.margin.top;
                child->geometry.width = content_w;

                total_y += child_h;

                // Reset column flow for content after spanning element
                current_col = 0;
                col_y = 0;
                max_col_h = 0;
                continue;
            }

            // break-before: column (value 4) — force a column break before this element
            if (child->break_before == 4 && current_col < ncols - 1 && col_y > 0) {
                if (col_y > max_col_h) max_col_h = col_y;
                current_col++;
                col_y = 0;
            }

            // Move to next column if this child would exceed target
            if (col_y > 0 && col_y + child_h > target_col_h * 1.1f && current_col < ncols - 1) {
                // orphans/widows check: estimate line count in this child
                int min_orphans = (node.orphans > 0) ? node.orphans : 2;
                int min_widows = (node.widows > 0) ? node.widows : 2;
                float line_h = child->font_size * child->line_height;
                if (line_h > 0 && child_h > line_h * 2) {
                    // Approximate lines that fit before column break
                    float remaining_space = target_col_h * 1.1f - col_y;
                    int lines_before = static_cast<int>(remaining_space / line_h);
                    int total_lines = static_cast<int>(child_h / line_h);
                    int lines_after = total_lines - lines_before;
                    // If splitting would leave too few orphans or widows, push to next column
                    if (lines_before > 0 && lines_before < min_orphans) {
                        // Too few lines at bottom of column — push whole element to next column
                        if (col_y > max_col_h) max_col_h = col_y;
                        current_col++;
                        col_y = 0;
                    } else if (lines_after > 0 && lines_after < min_widows) {
                        // Too few lines would start next column — push to next column
                        if (col_y > max_col_h) max_col_h = col_y;
                        current_col++;
                        col_y = 0;
                    } else {
                        if (col_y > max_col_h) max_col_h = col_y;
                        current_col++;
                        col_y = 0;
                    }
                } else {
                    // break-inside: avoid — push entire element to next column instead of splitting
                    if (col_y > max_col_h) max_col_h = col_y;
                    current_col++;
                    col_y = 0;
                }
            }

            // Position child in current column
            float col_x = static_cast<float>(current_col) * (col_w + gap);
            child->geometry.x = col_x + child->geometry.margin.left;
            child->geometry.y = total_y + col_y + child->geometry.margin.top;

            // Constrain child width to column width
            if (child->geometry.width > col_w) {
                child->geometry.width = col_w - child->geometry.padding.left -
                                        child->geometry.padding.right -
                                        child->geometry.border.left -
                                        child->geometry.border.right;
                if (child->geometry.width < 0) child->geometry.width = 0;
            }

            col_y += child_h;
        }
        if (col_y > max_col_h) max_col_h = col_y;
        total_y += max_col_h;
        children_height = total_y;
    }

    // Apply line-clamp: limit children_height to N lines worth of text.
    // When line_clamp > 0, compute how many lines the text would occupy if
    // word-wrapped to fit content_w, then clamp both children_height and
    // the text child's geometry.
    if (node.line_clamp > 0 && has_inline_children && content_w > 0) {
        for (auto& child : node.children) {
            if (child->content_visibility == 1) continue;
            if (!child->is_text || child->text_content.empty()) continue;

            float char_width = avg_char_width(child->font_size, child->font_family, child->font_weight,
                                              child->font_italic, child->is_monospace, child->letter_spacing);
            if (char_width <= 0) continue;
            float single_line_h = child->font_size * child->line_height;
            if (single_line_h <= 0) continue;

            float text_width = measure_text(child->text_content, child->font_size, child->font_family,
                                            child->font_weight, child->font_italic, child->letter_spacing);
            if (text_width <= content_w) continue; // text fits on one line, no clamping needed

            // Compute how many lines the text would naturally wrap to
            int chars_per_line = std::max(1, static_cast<int>(content_w / char_width));
            int total_lines = static_cast<int>(
                (child->text_content.length() + static_cast<size_t>(chars_per_line) - 1)
                / static_cast<size_t>(chars_per_line));

            if (total_lines <= node.line_clamp) continue; // already within clamp

            // Clamp: set the text child height to line_clamp lines
            float clamped_h = single_line_h * static_cast<float>(node.line_clamp);
            child->geometry.height = clamped_h;
            child->geometry.width = std::min(text_width, content_w);

            // Recompute children_height from the clamped children
            children_height = 0;
            for (auto& c : node.children) {
                if (c->display == DisplayType::None || c->mode == LayoutMode::None) continue;
                if (c->content_visibility == 1) continue;
                if (c->position_type == 2 || c->position_type == 3) continue;
                float bottom = c->geometry.y + c->geometry.margin_box_height();
                if (bottom > children_height) children_height = bottom;
            }
            break; // process only the first text child for line-clamp
        }
    }

    // Compute height
    float h = compute_height(node);
    if (h < 0 && node.aspect_ratio > 0) {
        // Aspect ratio: height = width / ratio
        h = node.geometry.width / node.aspect_ratio;
    }
    if (h < 0) {
        // Height from children + padding + border
        h = children_height
            + node.geometry.padding.top + node.geometry.padding.bottom
            + node.geometry.border.top + node.geometry.border.bottom;
    }
    h = std::max(h, node.min_height);
    h = std::min(h, node.max_height);
    node.geometry.height = h;

    // Position absolute/fixed children now that container dimensions are known
    position_absolute_children(node);

    // Note: apply_positioning is NOT called here.
    // The caller (position_block_children, flex_layout) is responsible for
    // calling apply_positioning after setting the child's position.
}

void LayoutEngine::layout_inline(LayoutNode& node, float containing_width) {
    if (node.display == DisplayType::None || node.mode == LayoutMode::None) {
        node.geometry.width = 0;
        node.geometry.height = 0;
        return;
    }

    if (node.is_text) {
        // <wbr> elements are zero-width break opportunities — skip text measurement
        if (node.is_wbr) {
            node.geometry.width = 0;
            node.geometry.height = 0;
            return;
        }
        // Apply text-transform before measuring
        if (node.text_transform == 2) { // uppercase
            std::transform(node.text_content.begin(), node.text_content.end(),
                           node.text_content.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        } else if (node.text_transform == 3) { // lowercase
            std::transform(node.text_content.begin(), node.text_content.end(),
                           node.text_content.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        } else if (node.text_transform == 1) { // capitalize
            bool cap_next = true;
            for (size_t i = 0; i < node.text_content.size(); i++) {
                if (node.text_content[i] == ' ') {
                    cap_next = true;
                } else if (cap_next) {
                    node.text_content[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(node.text_content[i])));
                    cap_next = false;
                }
            }
        }

        // Apply CSS white-space collapsing before measuring and font-variant processing.
        // Skip for <br> elements — their newline must be preserved regardless of white-space.
        if (node.tag_name != "br") {
            node.text_content = collapse_whitespace(node.text_content, node.white_space, node.white_space_pre);
        }

        // font-variant: small-caps — measure per-character width since
        // originally-lowercase letters render at 80% font size while
        // originally-uppercase letters render at 100%.
        float effective_font_size = node.font_size;
        if (node.font_variant == 1) {
            // Compute a blended effective font size based on the ratio of
            // lowercase vs uppercase characters in the original text.
            int lower_count = 0;
            int upper_count = 0;
            for (char c : node.text_content) {
                if (std::islower(static_cast<unsigned char>(c))) lower_count++;
                else if (std::isupper(static_cast<unsigned char>(c))) upper_count++;
            }
            int total_alpha = lower_count + upper_count;
            if (total_alpha > 0) {
                float lower_ratio = static_cast<float>(lower_count) / static_cast<float>(total_alpha);
                // Blend: lowercase chars use 80% size, uppercase use 100%
                float blend = lower_ratio * 0.8f + (1.0f - lower_ratio) * 1.0f;
                effective_font_size = node.font_size * blend;
            }
            // Transform text to uppercase for display (small-caps shows all caps)
            std::transform(node.text_content.begin(), node.text_content.end(),
                           node.text_content.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        }

        float char_width = avg_char_width(effective_font_size, node.font_family, node.font_weight,
                                          node.font_italic, node.is_monospace, node.letter_spacing);
        float single_line_height = node.font_size * node.line_height;

        if (node.white_space_pre || node.white_space == 4) {
            // Pre-formatted or pre-line text: respect newlines
            // white_space 2 (pre) and 3 (pre-wrap): preserve all whitespace and newlines
            // white_space 4 (pre-line): spaces already collapsed, but newlines preserved
            // For pre-formatted text, measure each line individually if measurer available
            if (text_measurer_) {
                float max_line_width = 0;
                int line_count = 1;
                std::string current_line;
                for (char c : node.text_content) {
                    if (c == '\n') {
                        float lw = current_line.empty() ? 0.0f :
                            measure_text(current_line, effective_font_size, node.font_family,
                                         node.font_weight, node.font_italic, node.letter_spacing);
                        if (node.word_spacing != 0) {
                            for (char sc : current_line) {
                                if (sc == ' ') lw += node.word_spacing;
                            }
                        }
                        max_line_width = std::max(max_line_width, lw);
                        current_line.clear();
                        line_count++;
                    } else if (c == '\t') {
                        // Approximate tab as tab_size spaces
                        for (int ti = 0; ti < node.tab_size; ti++) current_line += ' ';
                    } else {
                        current_line += c;
                    }
                }
                if (!current_line.empty()) {
                    float lw = measure_text(current_line, effective_font_size, node.font_family,
                                            node.font_weight, node.font_italic, node.letter_spacing);
                    if (node.word_spacing != 0) {
                        for (char sc : current_line) {
                            if (sc == ' ') lw += node.word_spacing;
                        }
                    }
                    max_line_width = std::max(max_line_width, lw);
                }
                node.geometry.width = max_line_width;
                node.geometry.height = single_line_height * static_cast<float>(line_count);
            } else {
                float max_line_width = 0;
                int line_count = 1;
                float current_line_width = 0;

                for (char c : node.text_content) {
                    if (c == '\n') {
                        max_line_width = std::max(max_line_width, current_line_width);
                        current_line_width = 0;
                        line_count++;
                    } else if (c == '\t') {
                        current_line_width += (char_width + node.letter_spacing) * node.tab_size;
                    } else {
                        current_line_width += char_width + node.letter_spacing;
                        if (c == ' ') {
                            current_line_width += node.word_spacing;
                        }
                    }
                }
                max_line_width = std::max(max_line_width, current_line_width);

                node.geometry.width = max_line_width;
                node.geometry.height = single_line_height * static_cast<float>(line_count);
            }
        } else {
            // Normal text: use real measurement or fallback
            float text_width = measure_text(node.text_content, effective_font_size, node.font_family,
                                            node.font_weight, node.font_italic, node.letter_spacing);
            // Add word_spacing for each space character
            if (node.word_spacing != 0) {
                for (char c : node.text_content) {
                    if (c == ' ') text_width += node.word_spacing;
                }
            }
            node.geometry.width = text_width;
            node.geometry.height = single_line_height;
        }
        return;
    }

    // Inline element with specified dimensions
    if (node.specified_width >= 0) {
        node.geometry.width = node.specified_width;
    }
    if (node.specified_height >= 0) {
        node.geometry.height = node.specified_height;
    }

    // For inline container elements (span, em, strong, a, etc.) that have
    // children but no explicit dimensions, compute width/height from children.
    if (!node.children.empty() && node.specified_width < 0) {
        float total_w = 0;
        float max_h = 0;
        for (auto& child : node.children) {
            if (child->display == DisplayType::None || child->mode == LayoutMode::None)
                continue;
            layout_inline(*child, containing_width);
            child->geometry.x = total_w;
            child->geometry.y = 0;
            total_w += child->geometry.margin_box_width();
            max_h = std::max(max_h, child->geometry.margin_box_height());
        }
        if (node.geometry.width <= 0) node.geometry.width = total_w;
        if (node.geometry.height <= 0) node.geometry.height = max_h;
    }
}

void LayoutEngine::layout_flex(LayoutNode& node, float containing_width) {
    if (node.display == DisplayType::None || node.mode == LayoutMode::None) {
        node.geometry.width = 0;
        node.geometry.height = 0;
        return;
    }

    // Resolve deferred percentage margins/padding for the flex container
    if (node.css_margin_top.has_value())
        node.geometry.margin.top = node.css_margin_top->to_px(containing_width);
    if (node.css_margin_right.has_value())
        node.geometry.margin.right = node.css_margin_right->to_px(containing_width);
    if (node.css_margin_bottom.has_value())
        node.geometry.margin.bottom = node.css_margin_bottom->to_px(containing_width);
    if (node.css_margin_left.has_value())
        node.geometry.margin.left = node.css_margin_left->to_px(containing_width);
    if (node.css_padding_top.has_value())
        node.geometry.padding.top = node.css_padding_top->to_px(containing_width);
    if (node.css_padding_right.has_value())
        node.geometry.padding.right = node.css_padding_right->to_px(containing_width);
    if (node.css_padding_bottom.has_value())
        node.geometry.padding.bottom = node.css_padding_bottom->to_px(containing_width);
    if (node.css_padding_left.has_value())
        node.geometry.padding.left = node.css_padding_left->to_px(containing_width);
    // Compute width FIRST (resolves css_width percentage) — needed before auto margins
    if (node.parent != nullptr) {
        node.geometry.width = compute_width(node, containing_width);
    }

    // Resolve auto margins for centering (AFTER width is resolved) — same as layout_block
    bool auto_left = is_margin_auto(node.geometry.margin.left);
    bool auto_right = is_margin_auto(node.geometry.margin.right);
    if (auto_left || auto_right) {
        float remaining = containing_width - node.geometry.width;
        if (!auto_left) remaining -= node.geometry.margin.left;
        if (!auto_right) remaining -= node.geometry.margin.right;
        if (remaining < 0) remaining = 0;
        if (auto_left && auto_right) {
            node.geometry.margin.left = remaining / 2.0f;
            node.geometry.margin.right = remaining / 2.0f;
        } else if (auto_left) {
            node.geometry.margin.left = remaining;
        } else {
            node.geometry.margin.right = remaining;
        }
    }

    // Set x from left margin (same as layout_block)
    if (node.parent != nullptr) {
        node.geometry.x = node.geometry.margin.left;
    }

    // Use content width (not border-box) for flex layout — padding/border handled by painter offset
    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;
    if (content_w < 0) content_w = 0;
    flex_layout(node, content_w);
}

void LayoutEngine::layout_children(LayoutNode& node) {
    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;

    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (child->content_visibility == 1) continue;
        switch (child->mode) {
            case LayoutMode::Block:
            case LayoutMode::InlineBlock:
                layout_block(*child, content_w);
                break;
            case LayoutMode::Inline:
                layout_inline(*child, content_w);
                break;
            case LayoutMode::Flex:
                layout_flex(*child, content_w);
                break;
            case LayoutMode::Grid:
                layout_grid(*child, content_w);
                break;
            case LayoutMode::Table:
                layout_table(*child, content_w);
                break;
            default:
                layout_block(*child, content_w);
                break;
        }
    }
}

void LayoutEngine::position_block_children(LayoutNode& node) {
    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;
    if (content_w < 0) content_w = 0;

    // Track active floats
    struct FloatRegion {
        float x, y, width, height;
        int type; // 1=left, 2=right
        int shape_type = 0; // 0=none, 1=circle, 2=ellipse, 3=inset
        std::vector<float> shape_values;
        float shape_margin = 0; // CSS shape-margin: extra space around shape
    };
    std::vector<FloatRegion> floats;

    auto available_at_y = [&](float y, float h, float& left_offset, float& right_offset) {
        left_offset = 0;
        right_offset = 0;
        for (auto& f : floats) {
            if (y + h > f.y && y < f.y + f.height) {
                float effective_width = f.width;

                // Apply shape-outside if present
                if (f.shape_type == 1 && !f.shape_values.empty()) {
                    // circle(radius) — shape_values[0] = radius in px
                    float radius = f.shape_values[0] + f.shape_margin;
                    float cx = f.x + f.width / 2.0f;
                    float cy = f.y + f.height / 2.0f;
                    float mid_y = y + h / 2.0f;
                    float dy = mid_y - cy;
                    if (std::abs(dy) < radius) {
                        float dx = std::sqrt(radius * radius - dy * dy);
                        if (f.type == 1) {
                            // Left float: exclusion extends from float left to cx + dx
                            effective_width = (cx + dx) - f.x;
                        } else {
                            // Right float: exclusion from cx - dx to float right
                            effective_width = (f.x + f.width) - (cx - dx);
                        }
                    } else {
                        effective_width = 0; // Outside circle, no exclusion
                    }
                } else if (f.shape_type == 2 && f.shape_values.size() >= 2) {
                    // ellipse(rx ry) — shape_values[0]=rx, [1]=ry
                    float rx = f.shape_values[0] + f.shape_margin;
                    float ry = f.shape_values[1] + f.shape_margin;
                    float cx = f.x + f.width / 2.0f;
                    float cy = f.y + f.height / 2.0f;
                    float mid_y = y + h / 2.0f;
                    float dy = mid_y - cy;
                    if (ry > 0 && std::abs(dy) < ry) {
                        float t = dy / ry;
                        float dx = rx * std::sqrt(1.0f - t * t);
                        if (f.type == 1) {
                            effective_width = (cx + dx) - f.x;
                        } else {
                            effective_width = (f.x + f.width) - (cx - dx);
                        }
                    } else {
                        effective_width = 0;
                    }
                } else if (f.shape_type == 3 && f.shape_values.size() >= 4) {
                    // inset(top right bottom left) — shape_values[0-3]
                    // shape-margin expands the shape (reduces insets)
                    float sm = f.shape_margin;
                    float inset_top = std::max(0.0f, f.shape_values[0] - sm);
                    float inset_right = std::max(0.0f, f.shape_values[1] - sm);
                    float inset_bottom = std::max(0.0f, f.shape_values[2] - sm);
                    float inset_left = std::max(0.0f, f.shape_values[3] - sm);
                    float shape_top = f.y + inset_top;
                    float shape_bottom = f.y + f.height - inset_bottom;
                    float mid_y = y + h / 2.0f;
                    if (mid_y >= shape_top && mid_y <= shape_bottom) {
                        effective_width = f.width - inset_left - inset_right;
                    } else {
                        effective_width = 0;
                    }
                }

                if (effective_width > 0) {
                    if (f.type == 1) left_offset = std::max(left_offset, f.x + effective_width);
                    if (f.type == 2) right_offset = std::max(right_offset, content_w - (f.x + f.width - effective_width));
                }
            }
        }
    };

    float cursor_y = 0;
    float prev_margin_bottom = 0; // Track previous sibling's bottom margin for collapsing
    bool first_child = true;
    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (child->content_visibility == 1) continue;

        // Skip absolute/fixed from normal flow — they are positioned separately
        if (child->position_type == 2 || child->position_type == 3) {
            continue;
        }

        // Handle clear property
        if (child->clear_type != 0) {
            float clear_y = cursor_y;
            for (auto& f : floats) {
                bool applies = (child->clear_type == 3) ||
                               (child->clear_type == 1 && f.type == 1) ||
                               (child->clear_type == 2 && f.type == 2);
                if (applies) {
                    clear_y = std::max(clear_y, f.y + f.height);
                }
            }
            cursor_y = clear_y;
        }

        // Determine available width considering floats
        float left_off = 0, right_off = 0;
        available_at_y(cursor_y, 20, left_off, right_off); // estimate 20px height
        float avail_w = content_w - left_off - right_off;
        if (avail_w < 0) avail_w = content_w;

        // Layout child
        float layout_width = (child->float_type != 0) ? avail_w : content_w;
        switch (child->mode) {
            case LayoutMode::Block:
                layout_block(*child, layout_width);
                break;
            case LayoutMode::InlineBlock:
                // Inline-block shrink-wraps to content unless explicit width
                if (child->specified_width >= 0) {
                    child->geometry.width = child->specified_width;
                    layout_block(*child, child->specified_width);
                } else {
                    layout_block(*child, layout_width);
                    // Shrink-wrap width to content
                    float max_cw = 0;
                    for (auto& gc : child->children) {
                        if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                        float w = gc->geometry.x + gc->geometry.margin_box_width();
                        max_cw = std::max(max_cw, w);
                    }
                    child->geometry.width = max_cw
                        + child->geometry.padding.left + child->geometry.padding.right
                        + child->geometry.border.left + child->geometry.border.right;
                }
                break;
            case LayoutMode::Inline:
                layout_inline(*child, layout_width);
                break;
            case LayoutMode::Flex:
                layout_flex(*child, layout_width);
                break;
            case LayoutMode::Grid:
                layout_grid(*child, layout_width);
                break;
            case LayoutMode::Table:
                layout_table(*child, layout_width);
                break;
            default:
                layout_block(*child, layout_width);
                break;
        }

        if (child->float_type != 0) {
            // Float element — position beside existing floats
            float child_w = child->geometry.margin_box_width();
            float child_h = child->geometry.margin_box_height();

            if (child->float_type == 1) {
                child->geometry.x = left_off + child->geometry.margin.left;
            } else {
                child->geometry.x = content_w - right_off - child_w + child->geometry.margin.left;
            }
            child->geometry.y = cursor_y + child->geometry.margin.top;

            floats.push_back({
                child->geometry.x,
                cursor_y,
                child_w,
                child_h,
                child->float_type,
                child->shape_outside_type,
                child->shape_outside_values,
                child->shape_margin
            });
            // Floats don't advance cursor_y
        } else {
            // Normal flow — position after considering floats
            available_at_y(cursor_y, child->geometry.margin_box_height(), left_off, right_off);
            child->geometry.x = left_off + child->geometry.margin.left;

            // Apply text-align centering for inline-like children in block context.
            // When a container has mixed block+inline content, inline-like children
            // (inline-block, images) should still respect the parent's text-align.
            if (node.text_align != 0) {
                bool child_inline_like = child->is_text
                    || child->mode == LayoutMode::Inline
                    || child->mode == LayoutMode::InlineBlock
                    || child->display == DisplayType::Inline
                    || child->display == DisplayType::InlineBlock
                    || child->display == DisplayType::InlineFlex
                    || child->display == DisplayType::InlineGrid;
                if (child_inline_like) {
                    float child_w = child->geometry.margin_box_width();
                    float extra = content_w - left_off - right_off - child_w;
                    if (extra > 0) {
                        if (node.text_align == 1 || node.text_align == 4) { // center or -webkit-center
                            child->geometry.x += extra / 2.0f;
                        } else if (node.text_align == 2) { // right
                            child->geometry.x += extra;
                        }
                    }
                }
            }

            // CSS margin collapsing (CSS2.1 Section 8.3.1):
            // When two vertical margins meet, they collapse into a single margin:
            //   - Both positive: take the larger
            //   - Both negative: take the more negative (smaller value)
            //   - Mixed (one positive, one negative): sum them
            // Margins do NOT collapse across BFC boundaries, or when separated
            // by border/padding.
            float top_margin = child->geometry.margin.top;
            float bottom_margin = child->geometry.margin.bottom;

            // Check if this child is block-level and eligible for collapsing
            // (inline-block, floats, and absolutely positioned elements don't collapse)
            bool child_is_block = child->mode == LayoutMode::Block &&
                child->display != DisplayType::InlineBlock &&
                child->float_type == 0;
            bool child_is_empty_block = child_is_block &&
                is_empty_block_for_margin_collapse(*child);

            // Empty block margin collapsing:
            // top and bottom margins collapse into a single carried margin.
            if (child_is_empty_block) {
                top_margin = collapse_vertical_margins(top_margin, bottom_margin);
            }

            // Parent-child top margin collapsing: the first child's top margin
            // collapses with its parent's top margin if:
            //   - this is the first in-flow child
            //   - the parent has no top border or top padding
            //   - the parent does NOT establish a new BFC
            // When collapsing, the child's top margin "escapes" to the parent
            // (i.e., it is not applied as internal spacing inside the parent).
            bool collapse_with_parent = first_child && child_is_block &&
                !node.establishes_bfc &&
                node.geometry.border.top == 0 && node.geometry.padding.top == 0;
            float carried_margin = top_margin;

            if (collapse_with_parent) {
                // The child's top margin collapses with the parent's top margin.
                // Apply the CSS collapsing rules:
                float parent_top = node.geometry.margin.top;
                float collapsed = collapse_vertical_margins(parent_top, top_margin);
                node.geometry.margin.top = collapsed;
                // The child is positioned at cursor_y (0) with no additional
                // internal top margin offset — margin escapes to parent.
                child->geometry.y = cursor_y;
            } else if (!first_child && child_is_block &&
                       (prev_margin_bottom != 0 || top_margin != 0)) {
                // Adjacent sibling margin collapsing:
                // CSS spec: the collapsed margin is computed as follows:
                //   - Both positive: max(prev_bottom, curr_top)
                //   - Both negative: min(prev_bottom, curr_top) (more negative)
                //   - Mixed: prev_bottom + curr_top (sum)
                float collapsed = collapse_vertical_margins(prev_margin_bottom, top_margin);
                // cursor_y already includes prev_margin_bottom, so subtract it
                // and add the collapsed value instead
                cursor_y -= prev_margin_bottom;
                child->geometry.y = cursor_y + collapsed;
                carried_margin = collapsed;
            } else {
                child->geometry.y = cursor_y + top_margin;
            }

            if (child_is_empty_block) {
                // The element contributes a single collapsed margin to adjoining siblings.
                prev_margin_bottom = carried_margin;
                cursor_y = collapse_with_parent
                    ? (child->geometry.y + carried_margin)
                    : child->geometry.y;
            } else {
                cursor_y = child->geometry.y + child->geometry.border_box_height() + bottom_margin;
                prev_margin_bottom = bottom_margin;
            }
            first_child = false;
        }

        apply_positioning(*child);
    }

    // Parent-child bottom margin collapsing: the last in-flow child's bottom
    // margin collapses with its parent's bottom margin if:
    //   - the parent has no bottom border or bottom padding
    //   - the parent has no explicit height or min-height
    //   - the parent does NOT establish a new BFC
    // The collapsed margin follows the same rules as top:
    //   - Both positive: take the larger
    //   - Both negative: take the more negative
    //   - Mixed: sum them
    if (!node.establishes_bfc &&
        node.geometry.border.bottom == 0 && node.geometry.padding.bottom == 0 &&
        node.specified_height < 0 && node.min_height <= 0) {
        // Find last in-flow non-float block-level child
        LayoutNode* last_child = nullptr;
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            auto& c = *it;
            if (c->display == DisplayType::None || c->mode == LayoutMode::None) continue;
            if (c->content_visibility == 1) continue;
            if (c->position_type == 2 || c->position_type == 3) continue;
            if (c->float_type != 0) continue;
            if (c->display == DisplayType::InlineBlock) continue; // inline-block doesn't collapse
            last_child = c.get();
            break;
        }
        if (last_child) {
            float child_bottom = last_child->geometry.margin.bottom;
            float parent_bottom = node.geometry.margin.bottom;
            float collapsed;
            if (child_bottom >= 0 && parent_bottom >= 0) {
                collapsed = std::max(parent_bottom, child_bottom);
            } else if (child_bottom < 0 && parent_bottom < 0) {
                collapsed = std::min(parent_bottom, child_bottom); // more negative
            } else {
                collapsed = parent_bottom + child_bottom; // mixed: sum
            }
            node.geometry.margin.bottom = collapsed;
        }
    }
}

void LayoutEngine::position_inline_children(LayoutNode& node, float containing_width) {
    float cursor_x = 0;
    float cursor_y = 0;
    float line_height = 0;

    // Track line start indices for text-align adjustment
    struct LineInfo {
        size_t start_idx;
        size_t end_idx;
        float width;
        float y;
    };
    std::vector<LineInfo> lines;
    size_t line_start = 0;

    // Collect non-hidden, in-flow children indices
    std::vector<size_t> visible;
    for (size_t i = 0; i < node.children.size(); i++) {
        auto& child = node.children[i];
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (child->content_visibility == 1) continue;
        if (child->position_type == 2 || child->position_type == 3) continue;
        visible.push_back(i);
    }

    // Apply text-indent to first line
    if (node.text_indent != 0) {
        cursor_x = node.text_indent;
    }

    // -----------------------------------------------------------------------
    // Flattened inline wrapping: when inline container elements (span, strong,
    // em, a, etc.) contain text, flatten all children into a continuous stream
    // of word runs so that text wraps naturally across inline element boundaries.
    //
    // Example: <p>Hello <strong>world this is long</strong> end</p>
    // Instead of wrapping the entire <strong> as one box, we extract individual
    // words and wrap them in a single shared line-breaking context.
    // -----------------------------------------------------------------------

    // Helper: check if an inline element is a "simple inline container"
    // (contains only text children or nested simple inline containers,
    //  no specified dimensions, no special features that need box-level layout)
    auto is_simple_inline_container = [](const LayoutNode& n) -> bool {
        // Must be an inline element (not text, not InlineBlock, not a replaced element)
        if (n.is_text) return false;
        if (n.display == DisplayType::InlineBlock) return false;
        if (n.mode != LayoutMode::Inline) return false;
        if (n.specified_width >= 0 || n.specified_height >= 0) return false;
        if (n.float_type != 0) return false;
        if (n.is_svg || n.is_canvas || n.is_iframe) return false;
        if (n.children.empty()) return false;
        // Check that all children are either text or simple inline containers
        for (auto& c : n.children) {
            if (c->is_text) continue;
            if (c->display == DisplayType::InlineBlock) return false;
            if (c->mode != LayoutMode::Inline) return false;
            if (c->specified_width >= 0 || c->specified_height >= 0) return false;
        }
        return true;
    };

    // Check if flattened inline wrapping should be used:
    // - wrapping is not suppressed
    // - at least one visible child is a simple inline container with text
    // - containing_width > 0
    bool nowrap_parent = (node.white_space == 1 || node.white_space == 2);
    bool has_inline_container = false;
    if (!nowrap_parent && containing_width > 0) {
        for (size_t vi = 0; vi < visible.size(); vi++) {
            auto& child = node.children[visible[vi]];
            if (is_simple_inline_container(*child)) {
                has_inline_container = true;
                break;
            }
        }
    }

    if (has_inline_container) {
        // --- Flattened inline wrapping path ---
        //
        // Collect all inline children into a flat list of word runs.
        // Each run is either a word (with styling from its originating element)
        // or a non-text element that we treat as a single box.

        struct WordRun {
            std::string word;        // word text (empty for non-text box runs)
            float width;             // measured or specified width
            float height;            // line height for text, box height for elements
            size_t child_vi;         // index into visible[]
            bool is_space;           // true if this is a word separator space
            bool is_box;             // true if this is a non-flattenable element
            float font_size;         // for line height computation
            float line_ht_mult;      // line_height multiplier
        };
        std::vector<WordRun> runs;

        // Recursive helper: extract word runs from a node and its children.
        // `child_vi` is the top-level visible child index that owns these runs.
        std::function<void(LayoutNode&, size_t)> extract_runs;
        extract_runs = [&](LayoutNode& n, size_t child_vi) {
            if (n.is_text && !n.text_content.empty()) {
                // Split text into words
                const std::string& text = n.text_content;
                float fs = n.font_size;
                float lh = n.line_height;

                // Handle newlines: treat \n as a line break
                size_t start = 0;
                while (start < text.size()) {
                    if (text[start] == '\n') {
                        // Newline: push a special "line break" run
                        WordRun br;
                        br.word = "\n";
                        br.width = 0;
                        br.height = fs * lh;
                        br.child_vi = child_vi;
                        br.is_space = false;
                        br.is_box = false;
                        br.font_size = fs;
                        br.line_ht_mult = lh;
                        runs.push_back(br);
                        start++;
                        continue;
                    }

                    size_t sp = text.find_first_of(" \n", start);
                    if (sp == std::string::npos) sp = text.size();
                    std::string word = text.substr(start, sp - start);
                    if (!word.empty()) {
                        float word_w = measure_text(word, fs, n.font_family,
                                                    n.font_weight, n.font_italic, n.letter_spacing);
                        WordRun wr;
                        wr.word = word;
                        wr.width = word_w;
                        wr.height = fs * lh;
                        wr.child_vi = child_vi;
                        wr.is_space = false;
                        wr.is_box = false;
                        wr.font_size = fs;
                        wr.line_ht_mult = lh;
                        runs.push_back(wr);
                    }
                    if (sp < text.size() && text[sp] == ' ') {
                        float space_w = measure_text(" ", fs, n.font_family,
                                                     n.font_weight, n.font_italic, n.letter_spacing);
                        WordRun sr;
                        sr.word = " ";
                        sr.width = space_w;
                        sr.height = fs * lh;
                        sr.child_vi = child_vi;
                        sr.is_space = true;
                        sr.is_box = false;
                        sr.font_size = fs;
                        sr.line_ht_mult = lh;
                        runs.push_back(sr);
                        sp++;
                    }
                    start = sp;
                }
                return;
            }

            // Non-text inline element: recurse into children
            for (auto& c : n.children) {
                if (c->display == DisplayType::None || c->mode == LayoutMode::None) continue;
                extract_runs(*c, child_vi);
            }
        };

        for (size_t vi = 0; vi < visible.size(); vi++) {
            auto& child = node.children[visible[vi]];

            if (child->is_text || is_simple_inline_container(*child)) {
                // Layout inline to apply text transforms etc.
                layout_inline(*child, containing_width);
                // Extract word runs
                extract_runs(*child, vi);
            } else {
                // Non-flattenable element: treat as a single box
                if (child->mode == LayoutMode::InlineBlock ||
                    child->display == DisplayType::InlineBlock) {
                    // Inline-block uses block formatting internally, but
                    // participates in this inline formatting context as one box.
                    if (child->specified_width >= 0) {
                        child->geometry.width = child->specified_width;
                    }
                    layout_block(*child, child->specified_width >= 0 ? child->specified_width : containing_width);
                    if (child->specified_width < 0) {
                        float max_child_w = 0;
                        for (auto& gc : child->children) {
                            if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                            float w = gc->geometry.margin_box_width();
                            max_child_w = std::max(max_child_w, gc->geometry.x + w);
                        }
                        child->geometry.width = max_child_w
                            + child->geometry.padding.left + child->geometry.padding.right
                            + child->geometry.border.left + child->geometry.border.right;
                    }
                } else {
                    layout_inline(*child, containing_width);
                }
                WordRun br;
                br.word = "";
                br.width = child->geometry.margin_box_width();
                br.height = child->geometry.margin_box_height();
                br.child_vi = vi;
                br.is_space = false;
                br.is_box = true;
                br.font_size = child->font_size;
                br.line_ht_mult = child->line_height;
                runs.push_back(br);
            }
        }

        // Now perform word-level wrapping across all runs.
        // Track per-child: first line y, max x extent, number of lines spanned.
        struct ChildPlacement {
            float first_x = -1;     // x position of first run on first line
            float first_y = -1;     // y position of first line
            float max_x = 0;        // maximum x extent reached
            float min_x = 1e9f;     // minimum x position (for width calculation)
            float last_y = -1;      // y position of last line
            float max_line_h = 0;   // maximum line height
            int line_count = 0;     // number of lines this child spans
            bool placed = false;
        };
        std::vector<ChildPlacement> placements(visible.size());

        // text-wrap: balance — narrow effective_width to distribute lines evenly.
        // Uses the same binary-search approach as the pure-text-node path (Path B).
        float effective_cw = containing_width;
        if (node.text_wrap == 2 && !runs.empty()) {
            // Compute total text width
            float total_run_w = 0;
            for (auto& r : runs) {
                if (r.word != "\n") total_run_w += r.width;
            }
            if (total_run_w > containing_width) {
                // Count lines at a given width using greedy wrapping
                auto count_lines = [&](float w) -> int {
                    int lines_c = 1;
                    float lc = cursor_x;
                    for (auto& r : runs) {
                        if (r.word == "\n") { lines_c++; lc = 0; continue; }
                        if (r.is_space && lc == 0) continue;
                        if (lc > 0 && lc + r.width > w && !r.is_space) {
                            lines_c++; lc = 0;
                            if (r.is_space) continue;
                        }
                        lc += r.width;
                    }
                    return lines_c;
                };
                int greedy_lines = count_lines(containing_width);
                if (greedy_lines > 1) {
                    float lo = total_run_w / static_cast<float>(greedy_lines);
                    float hi = containing_width;
                    for (int iter = 0; iter < 20; iter++) {
                        float mid = (lo + hi) / 2.0f;
                        if (count_lines(mid) <= greedy_lines) hi = mid;
                        else lo = mid;
                    }
                    effective_cw = hi;
                    if (effective_cw > containing_width) effective_cw = containing_width;
                }
            }
        }

        float cx = cursor_x;
        float cy = cursor_y;
        float lh_cur = 0;
        size_t run_line_start = 0;

        for (size_t ri = 0; ri < runs.size(); ri++) {
            auto& run = runs[ri];

            // Handle line break runs
            if (run.word == "\n") {
                lines.push_back({run_line_start, visible.size(), cx, cy}); // approximate
                cx = 0;
                cy += std::max(lh_cur, run.height);
                lh_cur = 0;
                continue;
            }

            // Skip leading spaces at start of line
            if (run.is_space && cx == 0) continue;

            // Check if this run fits on the current line (use effective_cw for balance)
            if (cx > 0 && cx + run.width > effective_cw && !run.is_space) {
                // Wrap to next line
                cx = 0;
                cy += lh_cur;
                lh_cur = 0;
                // Skip spaces at start of new line
                if (run.is_space) continue;
            }

            // Place the run
            auto& pl = placements[run.child_vi];
            if (!pl.placed) {
                pl.first_x = cx;
                pl.first_y = cy;
                pl.last_y = cy;
                pl.min_x = cx;
                pl.line_count = 1;
                pl.placed = true;
            }
            if (cy > pl.last_y) {
                pl.line_count++;
                pl.last_y = cy;
            }
            pl.min_x = std::min(pl.min_x, cx);
            pl.max_x = std::max(pl.max_x, cx + run.width);
            pl.max_line_h = std::max(pl.max_line_h, run.height);

            cx += run.width;
            lh_cur = std::max(lh_cur, run.height);
        }

        // Assign geometry back to each child based on placements
        for (size_t vi = 0; vi < visible.size(); vi++) {
            auto& child = node.children[visible[vi]];
            auto& pl = placements[vi];

            if (!pl.placed) {
                child->geometry.x = 0;
                child->geometry.y = 0;
                child->geometry.width = 0;
                child->geometry.height = 0;
                continue;
            }

            child->geometry.x = pl.first_x;
            child->geometry.y = pl.first_y;

            if (pl.line_count > 1) {
                // Element spans multiple lines:
                // Width = containing_width (it covers the full line on wrapped lines)
                // Height = number of lines * line height
                child->geometry.width = std::max(pl.max_x - pl.min_x, containing_width - pl.first_x);
                child->geometry.height = pl.max_line_h * static_cast<float>(pl.line_count);
            } else {
                // Single line: width = extent of its runs
                child->geometry.width = pl.max_x - pl.first_x;
                child->geometry.height = pl.max_line_h;
            }

            // For inline container children, position their sub-children relatively
            if (is_simple_inline_container(*child)) {
                // Recompute child text nodes' positions relative to the container
                float sub_x = 0;
                for (auto& sub : child->children) {
                    if (sub->display == DisplayType::None || sub->mode == LayoutMode::None) continue;
                    sub->geometry.x = sub_x;
                    sub->geometry.y = 0;
                    sub_x += sub->geometry.margin_box_width();
                }
            }

            apply_positioning(*child);
        }

        // Update final cursor position for container height calculation
        cursor_y = cy;
        line_height = lh_cur;

        // Record line info for text-align (simplified: treat as single line group)
        if (!visible.empty()) {
            lines.push_back({0, visible.size(), cx, cy});
        }

    } else {
    // --- Original (non-flattened) inline wrapping path ---
    // Used when there are no inline container elements with text,
    // or when wrapping is suppressed.

    for (size_t vi = 0; vi < visible.size(); vi++) {
        auto& child = node.children[visible[vi]];

        // Layout child — InlineBlock uses block model internally but participates inline
        if (child->mode == LayoutMode::InlineBlock ||
            child->display == DisplayType::InlineBlock) {
            // Shrink-wrap: use specified width or compute from content
            if (child->specified_width >= 0) {
                child->geometry.width = child->specified_width;
            }
            layout_block(*child, child->specified_width >= 0 ? child->specified_width : containing_width);
            // Shrink-wrap width to content if no explicit width
            if (child->specified_width < 0) {
                float max_child_w = 0;
                for (auto& gc : child->children) {
                    if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                    float w = gc->geometry.margin_box_width();
                    max_child_w = std::max(max_child_w, gc->geometry.x + w);
                }
                child->geometry.width = max_child_w
                    + child->geometry.padding.left + child->geometry.padding.right
                    + child->geometry.border.left + child->geometry.border.right;
            }
        } else {
            layout_inline(*child, containing_width);
        }

        float child_width = child->geometry.margin_box_width();
        float child_height = child->geometry.margin_box_height();

        // word-break: break-all (1) breaks at ANY character boundary unconditionally.
        // overflow-wrap: break-word (1) / anywhere (2) only breaks mid-word when
        // a single word doesn't fit — it should use word-boundary wrapping first,
        // falling back to character-level breaks only for overlong words.
        // The word-boundary wrapping path (below) already handles breaking long
        // words via its count_lines_at_width lambda, so overflow-wrap text
        // correctly enters that path rather than the character-level path.
        bool can_break_word = (child->word_break == 1);

        // word-break: keep-all (value 2) prevents ALL word breaking — text
        // stays on a single line unless explicit line breaks exist (like nowrap
        // for word-breaking purposes). This suppresses both word-boundary and
        // character-level breaking.
        bool keep_all = (child->word_break == 2);

        // Determine if wrapping is suppressed by white-space on the parent node.
        // white_space: 0=normal (wrap), 1=nowrap (no wrap), 2=pre (no wrap),
        //              3=pre-wrap (wrap), 4=pre-line (wrap), 5=break-spaces (wrap)
        bool nowrap = (node.white_space == 1 || node.white_space == 2 || keep_all);

        // Check wrapping (skip if nowrap or pre — no line wrapping allowed)
        if (!nowrap && cursor_x > 0 && cursor_x + child_width > containing_width) {
            // If this is a text node that can break mid-word, try to fit
            // partial characters on current line first, then wrap the rest
            if (can_break_word && child->is_text && !child->text_content.empty()) {
                float char_w = avg_char_width(child->font_size, child->font_family, child->font_weight,
                                              child->font_italic, child->is_monospace, child->letter_spacing);
                float avail = containing_width - cursor_x;
                int chars_on_line = static_cast<int>(avail / char_w);
                if (chars_on_line <= 0) {
                    // No room on current line, wrap to next line
                    lines.push_back({line_start, vi, cursor_x, cursor_y});
                    line_start = vi;
                    cursor_x = 0;
                    cursor_y += line_height;
                    line_height = 0;
                }
            } else {
                lines.push_back({line_start, vi, cursor_x, cursor_y});
                line_start = vi;
                cursor_x = 0;
                cursor_y += line_height;
                line_height = 0;
            }
        }

        // If this is a text node with word-breaking enabled and it's still too wide
        // for the containing width, re-layout it to wrap character-by-character
        // (skip if nowrap — no wrapping allowed)
        if (!nowrap && can_break_word && child->is_text && !child->text_content.empty()) {
            float char_w = avg_char_width(child->font_size, child->font_family, child->font_weight,
                                          child->font_italic, child->is_monospace, child->letter_spacing);
            float single_line_h = child->font_size * child->line_height;
            float total_text_width = measure_text(child->text_content, child->font_size, child->font_family,
                                                  child->font_weight, child->font_italic, child->letter_spacing);
            float avail_first_line = containing_width - cursor_x;

            if (total_text_width > avail_first_line) {
                // Text needs to wrap across multiple lines
                int chars_first = std::max(1, static_cast<int>(avail_first_line / char_w));
                int remaining = static_cast<int>(child->text_content.length()) - chars_first;
                int chars_per_full_line = std::max(1, static_cast<int>(containing_width / char_w));

                int extra_lines = 0;
                if (remaining > 0) {
                    extra_lines = (remaining + chars_per_full_line - 1) / chars_per_full_line;
                }

                int total_lines = 1 + extra_lines;
                child->geometry.width = std::min(total_text_width, containing_width);
                child->geometry.height = single_line_h * static_cast<float>(total_lines);
                child_width = child->geometry.width;
                child_height = child->geometry.height;
            }
        }

        // For normal text (word-break: normal), wrap at word boundaries when
        // the text node is wider than the available containing width.
        // text_wrap: 1 = nowrap (skip wrapping entirely)
        // text_wrap: 2 = balance (use narrower target width for even lines)
        // Also skip if white-space prevents wrapping (nowrap, pre)
        if (!nowrap && !can_break_word && child->is_text && !child->text_content.empty() &&
            containing_width > 0 && child->text_wrap != 1) {
            float char_w = avg_char_width(child->font_size, child->font_family, child->font_weight,
                                          child->font_italic, child->is_monospace, child->letter_spacing);
            float single_line_h = child->font_size * child->line_height;
            float total_text_width = measure_text(child->text_content, child->font_size, child->font_family,
                                                  child->font_weight, child->font_italic, child->letter_spacing);

            // Split text into words (used by both greedy and balanced paths)
            const std::string& text = child->text_content;
            std::vector<std::string> words;
            {
                size_t start = 0;
                while (start < text.size()) {
                    size_t sp = text.find(' ', start);
                    if (sp == std::string::npos) {
                        words.push_back(text.substr(start));
                        break;
                    }
                    words.push_back(text.substr(start, sp - start));
                    start = sp + 1;
                }
            }

            // Lambda: count how many lines the words take at a given width
            auto count_lines_at_width = [&](float w) -> int {
                int lines = 1;
                float lc = cursor_x;
                for (size_t wi = 0; wi < words.size(); wi++) {
                    float word_w = static_cast<float>(words[wi].length()) * char_w;
                    if (word_w > w && words[wi].length() > 1) {
                        float avail = w - lc;
                        int cf = std::max(1, static_cast<int>(avail / char_w));
                        int rem = static_cast<int>(words[wi].length()) - cf;
                        int cpf = std::max(1, static_cast<int>(w / char_w));
                        if (rem > 0) lines += (rem + cpf - 1) / cpf;
                        int llc = rem % cpf;
                        lc = (llc == 0 && rem > 0) ? w : static_cast<float>(llc) * char_w;
                    } else {
                        if (lc + word_w > w && lc > 0) {
                            lines++;
                            lc = 0;
                        }
                        lc += word_w;
                    }
                    if (wi + 1 < words.size()) lc += char_w;
                }
                return lines;
            };

            // For text-wrap: balance, use binary search to find the narrowest
            // width that produces the same number of lines as greedy wrapping.
            float effective_width = containing_width;
            if (child->text_wrap == 2 && total_text_width > containing_width) {
                int greedy_lines = count_lines_at_width(containing_width);
                if (greedy_lines > 1) {
                    // Binary search: find minimum width in [ideal, containing_width]
                    // that still yields greedy_lines lines.
                    float lo = total_text_width / static_cast<float>(greedy_lines);
                    float hi = containing_width;
                    // Ensure lo is at least char_w
                    if (lo < char_w) lo = char_w;
                    for (int iter = 0; iter < 20; iter++) {
                        float mid = (lo + hi) / 2.0f;
                        if (count_lines_at_width(mid) <= greedy_lines) {
                            hi = mid;
                        } else {
                            lo = mid;
                        }
                    }
                    effective_width = hi;
                    if (effective_width > containing_width) effective_width = containing_width;
                }
            }

            // For text-wrap: pretty, avoid orphans (short last lines).
            // If the last line is < 25% of container width, try a slightly narrower
            // effective width that pulls a word from the previous line down.
            if (child->text_wrap == 3 && total_text_width > containing_width) {
                int greedy_lines = count_lines_at_width(containing_width);
                if (greedy_lines >= 2) {
                    // Estimate last line width by simulating greedy wrapping
                    float last_line_w = 0;
                    {
                        float lc = cursor_x;
                        for (size_t wi = 0; wi < words.size(); wi++) {
                            float word_w = static_cast<float>(words[wi].length()) * char_w;
                            if (lc + word_w > containing_width && lc > 0) {
                                lc = 0;
                            }
                            lc += word_w;
                            if (wi + 1 < words.size()) lc += char_w;
                        }
                        last_line_w = lc;
                    }
                    // If last line is less than 25% of container width, try to
                    // redistribute by narrowing the effective width slightly.
                    // This pushes a word from the penultimate line to the last line.
                    if (last_line_w < containing_width * 0.25f && last_line_w > 0) {
                        // Binary search: find narrowest width that keeps same line count
                        // but allows one more line (greedy_lines + 1) to redistribute.
                        float lo = total_text_width / static_cast<float>(greedy_lines + 1);
                        float hi = containing_width;
                        if (lo < char_w) lo = char_w;
                        for (int iter = 0; iter < 20; iter++) {
                            float mid = (lo + hi) / 2.0f;
                            if (count_lines_at_width(mid) <= greedy_lines) {
                                hi = mid;
                            } else {
                                lo = mid;
                            }
                        }
                        // Only use narrower width if it doesn't increase line count
                        if (count_lines_at_width(hi) <= greedy_lines) {
                            effective_width = hi;
                            if (effective_width > containing_width) effective_width = containing_width;
                        }
                    }
                }
            }

            if (total_text_width > effective_width) {
                int total_lines = count_lines_at_width(effective_width);

                child->geometry.width = std::min(total_text_width, effective_width);
                child->geometry.height = single_line_h * static_cast<float>(total_lines);
                child_width = child->geometry.width;
                child_height = child->geometry.height;
            }
        }

        // For pre-wrap (white_space 3) and break-spaces (white_space 5) text:
        // account for word wrapping within each sub-line between \n characters.
        // The layout_inline() already computed dimensions for \n splits, but
        // sub-lines may also need to wrap at the container edge.
        // break-spaces is like pre-wrap but trailing spaces are not hung
        // (they contribute to line width and can cause wrapping).
        if ((node.white_space == 3 || node.white_space == 5) &&
            child->is_text && !child->text_content.empty() &&
            containing_width > 0) {
            float char_w = avg_char_width(child->font_size, child->font_family, child->font_weight,
                                          child->font_italic, child->is_monospace, child->letter_spacing);
            float single_line_h = child->font_size * child->line_height;
            if (char_w > 0 && single_line_h > 0) {
                // Split on newlines, count wrapped lines for each sub-line
                int total_lines = 0;
                float max_w = 0;
                const std::string& text = child->text_content;
                size_t pos = 0;
                while (pos <= text.size()) {
                    size_t nl = text.find('\n', pos);
                    if (nl == std::string::npos) nl = text.size();
                    size_t sub_len = nl - pos;
                    float sub_width = 0;
                    for (size_t i = pos; i < nl; i++) {
                        if (text[i] == '\t')
                            sub_width += (char_w) * child->tab_size;
                        else
                            sub_width += char_w;
                    }
                    if (sub_width > containing_width) {
                        int chars_per_line = std::max(1, static_cast<int>(containing_width / char_w));
                        int sub_lines = (static_cast<int>(sub_len) + chars_per_line - 1) / chars_per_line;
                        if (sub_lines < 1) sub_lines = 1;
                        total_lines += sub_lines;
                        max_w = std::max(max_w, containing_width);
                    } else {
                        total_lines += 1;
                        max_w = std::max(max_w, sub_width);
                    }
                    pos = nl + 1;
                    if (nl == text.size()) break;
                }
                child->geometry.width = max_w;
                child->geometry.height = single_line_h * static_cast<float>(total_lines);
                child_width = child->geometry.width;
                child_height = child->geometry.height;
            }
        }

        child->geometry.x = cursor_x;
        child->geometry.y = cursor_y;

        cursor_x += child_width;
        line_height = std::max(line_height, child_height);

        apply_positioning(*child);
    }
    } // end else (non-flattened path)

    // Record last line
    if (line_start < visible.size()) {
        lines.push_back({line_start, visible.size(), cursor_x, cursor_y});
    }

    // Apply text-align adjustment (including text-align-last for final line)
    if ((node.text_align != 0 || node.text_align_last != 0) && containing_width > 0) {
        for (size_t li = 0; li < lines.size(); li++) {
            auto& line = lines[li];
            float extra = containing_width - line.width;
            if (extra <= 0) continue;

            bool is_last_line = (li == lines.size() - 1);

            // Determine effective alignment for this line.
            // text_align_last: 0=auto, 1=left/start, 2=right/end, 3=center, 4=justify
            // For the last line, text_align_last overrides text_align when non-auto.
            int eff_align = node.text_align; // 0=left, 1=center, 2=right, 3=justify
            if (is_last_line && node.text_align_last != 0) {
                // Map text_align_last values to the text_align integer scheme:
                // text_align_last 1=left -> 0, 2=right -> 2, 3=center -> 1, 4=justify -> 3
                switch (node.text_align_last) {
                    case 1: eff_align = 0; break; // left/start
                    case 2: eff_align = 2; break; // right/end
                    case 3: eff_align = 1; break; // center
                    case 4: eff_align = 3; break; // justify
                    default: break;
                }
            }

            float offset = 0;
            if (eff_align == 1 || eff_align == 4) { // center or -webkit-center
                offset = extra / 2.0f;
            } else if (eff_align == 2) { // right
                offset = extra;
            } else if (eff_align == 3) { // justify
                // Distribute extra space between words on this line.
                // Don't justify the last line unless text-align-last says justify.
                bool justify_this_line = !is_last_line || node.text_align_last == 4;
                size_t item_count = line.end_idx - line.start_idx;
                if (justify_this_line && item_count > 1) {
                    float gap = extra / static_cast<float>(item_count - 1);
                    for (size_t vi = line.start_idx; vi < line.end_idx; vi++) {
                        float idx_in_line = static_cast<float>(vi - line.start_idx);
                        node.children[visible[vi]]->geometry.x += gap * idx_in_line;
                    }
                    continue; // skip the offset-based shift below
                }
                // Last line with justify but text-align-last=auto: left-align (offset = 0)
            }

            if (offset > 0) {
                for (size_t vi = line.start_idx; vi < line.end_idx; vi++) {
                    node.children[visible[vi]]->geometry.x += offset;
                }
            }
        }
    }

    // Apply vertical-align within each line
    for (auto& line : lines) {
        // Compute line height for this line
        float max_h = 0;
        for (size_t vi = line.start_idx; vi < line.end_idx; vi++) {
            auto& child = node.children[visible[vi]];
            max_h = std::max(max_h, child->geometry.margin_box_height());
        }

        for (size_t vi = line.start_idx; vi < line.end_idx; vi++) {
            auto& child = node.children[visible[vi]];
            float child_h = child->geometry.margin_box_height();
            float va_offset = 0;

            switch (child->vertical_align) {
                case 1: // top
                    va_offset = 0;
                    break;
                case 2: // middle
                    va_offset = (max_h - child_h) / 2.0f;
                    break;
                case 3: // bottom
                    va_offset = max_h - child_h;
                    break;
                case 4: // text-top — align top with parent's text top
                    va_offset = 0;
                    break;
                case 5: // text-bottom — align bottom with parent's text bottom
                    va_offset = max_h - child_h;
                    break;
                default: { // baseline (0) — align child baseline with line baseline
                    // For same-height elements, just bottom-align (equivalent to baseline)
                    if (std::abs(max_h - child_h) < 0.5f) {
                        va_offset = 0; // same height — no offset needed
                    } else {
                        // Mixed heights: align baselines
                        // For text: baseline = ascent (80% of font size from top)
                        // For replaced elements: baseline = bottom edge
                        float line_baseline = max_h * 0.8f;
                        float child_baseline = child_h; // replaced elements: bottom = baseline
                        if (child->is_text || child->display == clever::layout::DisplayType::Inline) {
                            child_baseline = child->font_size > 0 ? child->font_size * 0.8f : child_h;
                        }
                        va_offset = line_baseline - child_baseline;
                        if (va_offset < 0) va_offset = 0;
                        if (va_offset > max_h - child_h) va_offset = max_h - child_h;
                    }
                    break;
                }
            }

            if (va_offset > 0) {
                child->geometry.y += va_offset;
            }
        }
    }
}

void LayoutEngine::flex_layout(LayoutNode& node, float containing_width) {
    bool is_row = (node.flex_direction == 0 || node.flex_direction == 1);
    bool is_reverse = (node.flex_direction == 1 || node.flex_direction == 3);

    // For flex-direction: row, the main axis gap is column-gap.
    // For flex-direction: column, the main axis gap is row-gap.
    // The CSS "gap" shorthand sets both node.gap (row-gap) and node.column_gap_val (column-gap).
    float main_gap = is_row ? node.column_gap_val : node.gap;
    float cross_gap = is_row ? node.gap : node.column_gap_val;

    if (node.css_height.has_value()) {
        float cb_height = viewport_height_;
        if (node.parent && node.parent->specified_height >= 0) {
            cb_height = node.parent->specified_height;
        }
        node.specified_height = node.css_height->to_px(cb_height);
    }

    // Main-axis sizing:
    // - row/row-reverse: main axis is width, always definite from containing width.
    // - column/column-reverse: main axis is height and may be indefinite (auto).
    float main_size = 0.0f;
    bool has_definite_main_size = false;
    if (is_row) {
        main_size = containing_width;
        has_definite_main_size = true;
    } else if (node.specified_height >= 0) {
        main_size = node.specified_height;
        has_definite_main_size = true;
    }

    // Collect flex items
    struct FlexItem {
        LayoutNode* child;
        float basis;
    };
    std::vector<FlexItem> items;
    float total_basis = 0;

    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        // Skip absolute/fixed from flex flow
        if (child->position_type == 2 || child->position_type == 3) continue;

        // Resolve deferred percentage/calc values for flex items
        if (child->css_width.has_value()) {
            // `containing_width` here is the flex container's content width.
            // Percentage widths on flex items must resolve against this value.
            child->specified_width = child->css_width->to_px(containing_width);
        }
        if (child->css_height.has_value()) {
            // Percentage heights on flex items resolve only when the flex container
            // has a definite height.
            float cb_height = node.specified_height;
            if (cb_height >= 0) {
                child->specified_height = child->css_height->to_px(cb_height);
            }
        }
        if (child->css_min_width.has_value()) {
            child->min_width = child->css_min_width->to_px(containing_width);
        }
        if (child->css_max_width.has_value()) {
            child->max_width = child->css_max_width->to_px(containing_width);
        }
        if (child->css_min_height.has_value()) {
            float cb_height = node.specified_height;
            if (cb_height >= 0) {
                child->min_height = child->css_min_height->to_px(cb_height);
            }
        }
        if (child->css_max_height.has_value()) {
            float cb_height = node.specified_height;
            if (cb_height >= 0) {
                child->max_height = child->css_max_height->to_px(cb_height);
            }
        }
        // Resolve deferred percentage margins/padding for flex items
        if (child->css_margin_top.has_value())
            child->geometry.margin.top = child->css_margin_top->to_px(containing_width);
        if (child->css_margin_right.has_value())
            child->geometry.margin.right = child->css_margin_right->to_px(containing_width);
        if (child->css_margin_bottom.has_value())
            child->geometry.margin.bottom = child->css_margin_bottom->to_px(containing_width);
        if (child->css_margin_left.has_value())
            child->geometry.margin.left = child->css_margin_left->to_px(containing_width);
        if (child->css_padding_top.has_value())
            child->geometry.padding.top = child->css_padding_top->to_px(containing_width);
        if (child->css_padding_right.has_value())
            child->geometry.padding.right = child->css_padding_right->to_px(containing_width);
        if (child->css_padding_bottom.has_value())
            child->geometry.padding.bottom = child->css_padding_bottom->to_px(containing_width);
        if (child->css_padding_left.has_value())
            child->geometry.padding.left = child->css_padding_left->to_px(containing_width);
        // Note: auto margins on flex items (sentinel -1) are preserved here.
        // They are resolved later in the justify-content phase, per CSS Flexbox spec 8.1.

        float basis;
        if (child->flex_basis >= 0) {
            basis = child->flex_basis;
        } else {
            // flex-basis:auto -> use the item's main-size property when definite.
            // For row flex items, a resolved percentage `width` is already in
            // `specified_width` from the block above and is used as basis.
            if (is_row && child->specified_width >= 0) {
                basis = child->specified_width;
            } else if (!is_row && child->specified_height >= 0) {
                basis = child->specified_height;
            } else {
                // Avoid using stale geometry from prior layout passes.
                // With auto main-size and auto flex-basis, fall back to intrinsic size.
                const TextMeasureFn* mp = text_measurer_ ? &text_measurer_ : nullptr;
                if (is_row) {
                    float intrinsic_w = measure_intrinsic_width(*child, true, mp);
                    intrinsic_w -= child->geometry.padding.left + child->geometry.padding.right
                                 + child->geometry.border.left + child->geometry.border.right;
                    basis = intrinsic_w;
                } else {
                    float intrinsic_h = measure_intrinsic_height(*child, true, mp);
                    intrinsic_h -= child->geometry.padding.top + child->geometry.padding.bottom
                                 + child->geometry.border.top + child->geometry.border.bottom;
                    basis = intrinsic_h;
                }
                if (basis < 0) basis = 0;
            }
        }

        items.push_back({child.get(), basis});
        total_basis += basis;
    }

    // Sort items by CSS order property (stable sort preserves DOM order for equal values)
    bool has_order = false;
    for (auto& item : items) {
        if (item.child->order != 0) { has_order = true; break; }
    }
    if (has_order) {
        std::stable_sort(items.begin(), items.end(),
            [](const FlexItem& a, const FlexItem& b) {
                return a.child->order < b.child->order;
            });
    }

    // Reverse item order for row-reverse / column-reverse
    if (is_reverse && !items.empty()) {
        std::reverse(items.begin(), items.end());
    }

    float total_gap = 0;
    if (items.size() > 1) {
        total_gap = main_gap * static_cast<float>(items.size() - 1);
    }

    // Split items into flex lines
    struct FlexLine {
        size_t start = 0, end = 0;
        float total_basis = 0;
        float max_cross = 0;
    };
    std::vector<FlexLine> lines;

    if (node.flex_wrap != 0 && !items.empty() && has_definite_main_size) {
        // Wrap mode: start new line when items exceed main_size
        FlexLine current_line;
        current_line.start = 0;
        float line_basis = 0;
        for (size_t i = 0; i < items.size(); i++) {
            float item_size = items[i].basis;
            if (i > current_line.start && line_basis + item_size > main_size) {
                current_line.end = i;
                current_line.total_basis = line_basis;
                lines.push_back(current_line);
                current_line = {};
                current_line.start = i;
                line_basis = 0;
            }
            line_basis += item_size;
            if (i > current_line.start) line_basis += main_gap;
        }
        current_line.end = items.size();
        current_line.total_basis = line_basis;
        lines.push_back(current_line);
    } else {
        // No wrap: single line
        FlexLine single;
        single.start = 0;
        single.end = items.size();
        single.total_basis = total_basis + total_gap;
        lines.push_back(single);
    }

    // Process each flex line
    float cross_cursor = 0;
    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        auto& line = lines[line_index];
        // Compute line total basis and gap
        float line_total_gap = 0;
        size_t line_count = line.end - line.start;
        if (line_count > 1) {
            line_total_gap = main_gap * static_cast<float>(line_count - 1);
        }

        float line_total_basis = 0;
        for (size_t i = line.start; i < line.end; i++) {
            line_total_basis += items[i].basis;
        }

        float remaining = 0.0f;
        if (has_definite_main_size) {
            remaining = main_size - line_total_basis - line_total_gap;
        }

        // Flex grow / shrink within this line
        if (has_definite_main_size && remaining > 0) {
            float total_grow = 0;
            for (size_t i = line.start; i < line.end; i++) {
                total_grow += items[i].child->flex_grow;
            }
            if (total_grow > 0) {
                for (size_t i = line.start; i < line.end; i++) {
                    items[i].basis += (items[i].child->flex_grow / total_grow) * remaining;
                }
            }
        } else if (has_definite_main_size && remaining < 0 && node.flex_wrap == 0) {
            float total_shrink_weighted = 0;
            for (size_t i = line.start; i < line.end; i++) {
                total_shrink_weighted += items[i].child->flex_shrink * items[i].basis;
            }
            if (total_shrink_weighted > 0) {
                float overflow = -remaining;
                for (size_t i = line.start; i < line.end; i++) {
                    float shrink_amount = (items[i].child->flex_shrink * items[i].basis / total_shrink_weighted) * overflow;
                    items[i].basis -= shrink_amount;
                    if (items[i].basis < 0) items[i].basis = 0;
                }
            }
        }

        // Apply min/max on each item in this line
        // Resolve deferred min/max constraints in the main axis.
        for (size_t i = line.start; i < line.end; i++) {
            if (is_row) {
                if (items[i].child->css_min_width.has_value()) {
                    items[i].child->min_width = items[i].child->css_min_width->to_px(containing_width);
                }
                if (items[i].child->css_max_width.has_value()) {
                    items[i].child->max_width = items[i].child->css_max_width->to_px(containing_width);
                }
                items[i].basis = std::max(items[i].basis, items[i].child->min_width);
                items[i].basis = std::min(items[i].basis, items[i].child->max_width);
            } else {
                float cb_height = node.specified_height;
                if (cb_height >= 0 && items[i].child->css_min_height.has_value()) {
                    items[i].child->min_height = items[i].child->css_min_height->to_px(cb_height);
                }
                if (cb_height >= 0 && items[i].child->css_max_height.has_value()) {
                    items[i].child->max_height = items[i].child->css_max_height->to_px(cb_height);
                }
                items[i].basis = std::max(items[i].basis, items[i].child->min_height);
                items[i].basis = std::min(items[i].basis, items[i].child->max_height);
            }
        }

        // Compute total used main size for this line
        float total_used = line_total_gap;
        for (size_t i = line.start; i < line.end; i++) {
            total_used += items[i].basis;
        }

        // Per CSS Flexbox spec 8.1: auto margins on main axis absorb free space
        // BEFORE justify-content is applied
        float remaining_space = has_definite_main_size ? (main_size - total_used) : 0.0f;
        if (remaining_space < 0) remaining_space = 0;

        if (remaining_space > 0) {
            // Count auto margins on the main axis
            int auto_margin_count = 0;
            for (size_t i = line.start; i < line.end; i++) {
                auto* child = items[i].child;
                if (is_row) {
                    if (is_margin_auto(child->geometry.margin.left)) auto_margin_count++;
                    if (is_margin_auto(child->geometry.margin.right)) auto_margin_count++;
                } else {
                    if (is_margin_auto(child->geometry.margin.top)) auto_margin_count++;
                    if (is_margin_auto(child->geometry.margin.bottom)) auto_margin_count++;
                }
            }
            if (auto_margin_count > 0) {
                float per_auto = remaining_space / static_cast<float>(auto_margin_count);
                for (size_t i = line.start; i < line.end; i++) {
                    auto* child = items[i].child;
                    if (is_row) {
                        if (is_margin_auto(child->geometry.margin.left)) child->geometry.margin.left = per_auto;
                        if (is_margin_auto(child->geometry.margin.right)) child->geometry.margin.right = per_auto;
                    } else {
                        if (is_margin_auto(child->geometry.margin.top)) child->geometry.margin.top = per_auto;
                        if (is_margin_auto(child->geometry.margin.bottom)) child->geometry.margin.bottom = per_auto;
                    }
                }
                remaining_space = 0; // auto margins consumed all free space
            }
        }

        // Zero out only MAIN-axis auto margins that weren't resolved above.
        // Cross-axis auto margins (top/bottom in row, left/right in column)
        // are handled later in the cross-axis positioning phase.
        for (size_t i = line.start; i < line.end; i++) {
            auto* child = items[i].child;
            if (is_row) {
                if (is_margin_auto(child->geometry.margin.left)) child->geometry.margin.left = 0;
                if (is_margin_auto(child->geometry.margin.right)) child->geometry.margin.right = 0;
            } else {
                if (is_margin_auto(child->geometry.margin.top)) child->geometry.margin.top = 0;
                if (is_margin_auto(child->geometry.margin.bottom)) child->geometry.margin.bottom = 0;
            }
        }

        // Justify-content for this line
        float cursor_main = 0;
        float gap_between = main_gap;

        switch (node.justify_content) {
            case 0: cursor_main = 0; break;
            case 1: cursor_main = remaining_space; break;
            case 2: cursor_main = remaining_space / 2.0f; break;
            case 3: // space-between
                if (line_count > 1) {
                    gap_between = main_gap + remaining_space / static_cast<float>(line_count - 1);
                }
                cursor_main = 0;
                break;
            case 4: // space-around
                if (line_count > 0) {
                    float space_unit = remaining_space / static_cast<float>(line_count);
                    cursor_main = space_unit / 2.0f;
                    gap_between = main_gap + space_unit;
                }
                break;
            case 5: // space-evenly
                if (line_count > 0) {
                    float space_unit = remaining_space / static_cast<float>(line_count + 1);
                    cursor_main = space_unit;
                    gap_between = main_gap + space_unit;
                }
                break;
            default: break;
        }

        // Position items along main axis
        line.max_cross = 0;
        for (size_t i = line.start; i < line.end; ++i) {
            auto& item = items[i];
            auto* child = item.child;

            if (is_row) {
                child->geometry.width = item.basis;
                if (child->max_width < std::numeric_limits<float>::max()) {
                    child->geometry.width = std::min(child->geometry.width, child->max_width);
                }
                child->geometry.width = std::max(child->geometry.width, child->min_width);
                if (child->specified_height >= 0) {
                    child->geometry.height = child->specified_height;
                } else {
                    float child_content_w = child->geometry.width
                        - child->geometry.padding.left - child->geometry.padding.right
                        - child->geometry.border.left - child->geometry.border.right;
                    if (child_content_w < 0) child_content_w = 0;
                    position_block_children(*child);
                    float ch = 0;
                    for (auto& gc : child->children) {
                        if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                        ch += gc->geometry.margin_box_height();
                    }
                    child->geometry.height = ch
                        + child->geometry.padding.top + child->geometry.padding.bottom
                        + child->geometry.border.top + child->geometry.border.bottom;
                }
                if (child->max_height < std::numeric_limits<float>::max()) {
                    child->geometry.height = std::min(child->geometry.height, child->max_height);
                }
                child->geometry.height = std::max(child->geometry.height, child->min_height);
                child->geometry.x = cursor_main;
                child->geometry.y = cross_cursor;
                line.max_cross = std::max(line.max_cross, child->geometry.margin_box_height());
            } else {
                child->geometry.height = item.basis;
                if (child->max_height < std::numeric_limits<float>::max()) {
                    child->geometry.height = std::min(child->geometry.height, child->max_height);
                }
                child->geometry.height = std::max(child->geometry.height, child->min_height);
                if (child->specified_width >= 0) {
                    child->geometry.width = child->specified_width;
                } else {
                    int eff_align = (child->align_self >= 0) ? child->align_self : node.align_items;
                    if (eff_align == 4) {
                        child->geometry.width = containing_width;
                    } else {
                        const TextMeasureFn* mp = text_measurer_ ? &text_measurer_ : nullptr;
                        float intrinsic_w = measure_intrinsic_width(*child, true, mp);
                        intrinsic_w += child->geometry.padding.left + child->geometry.padding.right
                                    + child->geometry.border.left + child->geometry.border.right;
                        child->geometry.width = std::min(intrinsic_w, containing_width);
                    }
                }
                if (child->max_width < std::numeric_limits<float>::max()) {
                    child->geometry.width = std::min(child->geometry.width, child->max_width);
                }
                child->geometry.width = std::max(child->geometry.width, child->min_width);
                child->geometry.y = cursor_main;
                child->geometry.x = cross_cursor;
                line.max_cross = std::max(line.max_cross, child->geometry.margin_box_width());
            }

            cursor_main += item.basis;
            if (i + 1 < line.end) {
                cursor_main += gap_between;
            }
        }

        // Cross-axis align-items for this line
        // For single-line flex with specified container size, use container cross
        float line_cross = line.max_cross;
        if (lines.size() == 1) {
            if (is_row && node.specified_height >= 0) {
                line_cross = node.specified_height;
            } else if (!is_row) {
                line_cross = containing_width;
            }
        }
        for (size_t i = line.start; i < line.end; i++) {
            auto* child = items[i].child;
            float child_cross = is_row ? child->geometry.height : child->geometry.width;

            // In flexbox, auto margins on the cross-axis take priority over
            // align-items/align-self. Distribute remaining cross-axis space
            // to auto margins before falling through to alignment.
            bool auto_margin_top = (is_row && is_margin_auto(child->geometry.margin.top));
            bool auto_margin_bottom = (is_row && is_margin_auto(child->geometry.margin.bottom));
            bool auto_margin_left = (!is_row && is_margin_auto(child->geometry.margin.left));
            bool auto_margin_right = (!is_row && is_margin_auto(child->geometry.margin.right));
            bool has_cross_auto_margin = (is_row ? (auto_margin_top || auto_margin_bottom)
                                                  : (auto_margin_left || auto_margin_right));

            if (has_cross_auto_margin) {
                float remaining_cross = line_cross - child_cross;
                if (remaining_cross < 0) remaining_cross = 0;
                if (is_row) {
                    if (auto_margin_top && auto_margin_bottom) {
                        child->geometry.margin.top = remaining_cross / 2.0f;
                        child->geometry.margin.bottom = remaining_cross / 2.0f;
                        child->geometry.y = cross_cursor + child->geometry.margin.top;
                    } else if (auto_margin_top) {
                        child->geometry.margin.top = remaining_cross;
                        child->geometry.margin.bottom = 0;
                        child->geometry.y = cross_cursor + remaining_cross;
                    } else {
                        child->geometry.margin.top = 0;
                        child->geometry.margin.bottom = remaining_cross;
                        // y stays at cross_cursor (flex-start)
                    }
                } else {
                    if (auto_margin_left && auto_margin_right) {
                        child->geometry.margin.left = remaining_cross / 2.0f;
                        child->geometry.margin.right = remaining_cross / 2.0f;
                        child->geometry.x = cross_cursor + child->geometry.margin.left;
                    } else if (auto_margin_left) {
                        child->geometry.margin.left = remaining_cross;
                        child->geometry.margin.right = 0;
                        child->geometry.x = cross_cursor + remaining_cross;
                    } else {
                        child->geometry.margin.left = 0;
                        child->geometry.margin.right = remaining_cross;
                    }
                }
            } else {
            // Use align-self if set, otherwise fall back to parent's align-items
            int align = (child->align_self >= 0) ? child->align_self : node.align_items;
            switch (align) {
                case 0: break; // flex-start
                case 1: // flex-end
                    if (is_row) child->geometry.y = cross_cursor + line_cross - child_cross;
                    else child->geometry.x = cross_cursor + line_cross - child_cross;
                    break;
                case 2: // center
                    if (is_row) child->geometry.y = cross_cursor + (line_cross - child_cross) / 2.0f;
                    else child->geometry.x = cross_cursor + (line_cross - child_cross) / 2.0f;
                    break;
                case 3: break; // baseline (treat as flex-start)
                case 4: // stretch
                    if (is_row && child->specified_height < 0) {
                        child->geometry.height = line_cross;
                    } else if (!is_row && child->specified_width < 0) {
                        child->geometry.width = line_cross;
                    }
                    break;
                default: break;
            }
            } // end else (no cross-axis auto margin)

            apply_positioning(*child);
        }

        cross_cursor += line_cross;
        if (line_index + 1 < lines.size()) {
            cross_cursor += cross_gap;
        }
    }

    // Container height
    if (node.specified_height >= 0) {
        node.geometry.height = node.specified_height;
    } else {
        if (is_row) {
            node.geometry.height = cross_cursor;
        } else {
            float total_main = 0;
            for (auto& item : items) {
                total_main += item.basis;
            }
            total_main += total_gap;
            node.geometry.height = total_main;
        }
    }

    // Resolve deferred percentage/calc min-height and max-height for flex containers.
    // This is critical for patterns like min-height: 100vh on flex containers.
    {
        float cb_height = viewport_height_;
        if (node.parent && node.parent->specified_height >= 0) {
            cb_height = node.parent->specified_height;
        }
        if (node.css_min_height.has_value()) {
            node.min_height = node.css_min_height->to_px(cb_height);
        }
        if (node.css_max_height.has_value()) {
            node.max_height = node.css_max_height->to_px(cb_height);
        }
    }
    node.geometry.height = std::max(node.geometry.height, node.min_height);
    node.geometry.height = std::min(node.geometry.height, node.max_height);

    // align-content: distribute extra cross-axis space between flex lines (multi-line)
    if (lines.size() > 1 && node.align_content != 0 && is_row && node.specified_height >= 0) {
        float total_line_cross = 0;
        for (auto& line : lines) total_line_cross += line.max_cross;
        float total_cross_gap = cross_gap * static_cast<float>(lines.size() - 1);
        float extra = node.specified_height - total_line_cross - total_cross_gap;
        if (extra > 0) {
            float offset = 0;
            float gap = 0;
            size_t n_lines = lines.size();
            switch (node.align_content) {
                case 1: offset = extra; break; // end
                case 2: offset = extra / 2.0f; break; // center
                case 3: // stretch — divide extra evenly among lines
                    break;
                case 4: // space-between
                    if (n_lines > 1) gap = extra / static_cast<float>(n_lines - 1);
                    break;
                case 5: // space-around
                    gap = extra / static_cast<float>(n_lines);
                    offset = gap / 2.0f;
                    break;
                default: break;
            }
            // Shift all items in each line by accumulated offset
            float cumulative = offset;
            for (size_t li = 0; li < lines.size(); li++) {
                if (li > 0) cumulative += gap;
                if (cumulative != 0) {
                    for (size_t i = lines[li].start; i < lines[li].end; i++) {
                        items[i].child->geometry.y += cumulative;
                    }
                }
            }
        }
    }

    // Position absolute/fixed children
    position_absolute_children(node);
}

void LayoutEngine::wrap_anonymous_blocks(LayoutNode& node) {
    // CSS 2.1 §9.2.1.1: When a block container has both block-level and
    // inline-level children, wrap consecutive runs of inline children into
    // anonymous block boxes so that the parent ends up with only block-level
    // children.
    std::vector<std::unique_ptr<LayoutNode>> new_children;
    std::vector<std::unique_ptr<LayoutNode>> inline_run;

    auto flush_inline_run = [&]() {
        if (inline_run.empty()) return;
        auto anon = std::make_unique<LayoutNode>();
        anon->is_anonymous = true;
        anon->mode = LayoutMode::Block;
        anon->display = DisplayType::Block;
        anon->parent = &node;
        // Inherit text properties from parent
        anon->text_align = node.text_align;
        anon->font_size = node.font_size;
        anon->font_family = node.font_family;
        anon->font_weight = node.font_weight;
        anon->font_italic = node.font_italic;
        anon->color = node.color;
        anon->line_height = node.line_height;
        anon->letter_spacing = node.letter_spacing;
        anon->word_spacing = node.word_spacing;
        anon->white_space = node.white_space;
        anon->direction = node.direction;
        // Move all inline children into the anonymous block
        for (auto& ic : inline_run) {
            ic->parent = anon.get();
            anon->children.push_back(std::move(ic));
        }
        inline_run.clear();
        new_children.push_back(std::move(anon));
    };

    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            // Preserve hidden children in-place
            new_children.push_back(std::move(child));
            continue;
        }
        if (child->position_type == 2 || child->position_type == 3) {
            // Absolutely-positioned children are out of flow; keep in-place
            new_children.push_back(std::move(child));
            continue;
        }
        bool inline_like = child->is_text
            || child->mode == LayoutMode::Inline
            || child->mode == LayoutMode::InlineBlock
            || child->display == DisplayType::Inline
            || child->display == DisplayType::InlineBlock
            || child->display == DisplayType::InlineFlex
            || child->display == DisplayType::InlineGrid;
        if (inline_like) {
            inline_run.push_back(std::move(child));
        } else {
            flush_inline_run();
            new_children.push_back(std::move(child));
        }
    }
    flush_inline_run();
    node.children = std::move(new_children);
}

void LayoutEngine::apply_positioning(LayoutNode& node) {
    if (node.position_type == 1) { // relative only
        if (node.pos_top_set) node.geometry.y += node.pos_top;
        else if (node.pos_bottom_set) node.geometry.y -= node.pos_bottom;
        if (node.pos_left_set) node.geometry.x += node.pos_left;
        else if (node.pos_right_set) node.geometry.x -= node.pos_right;
    } else if (node.position_type == 4) { // sticky
        // Store the normal flow position for paint-time sticky calculations
        node.sticky_original_y = node.geometry.y;
        node.sticky_original_x = node.geometry.x;
        // No layout-time offset; sticky adjustments happen at paint time based on scroll
    }
    // position_type == 4 (sticky): element stays in normal flow position.
    // The top/left/right/bottom offsets are used at paint time to clamp
    // the element's position within its constraint box (nearest scroll container or viewport).
}

void LayoutEngine::position_absolute_children(LayoutNode& node) {
    // Absolute positioning:
    // containing block is nearest positioned ancestor (or root).
    bool is_root = (node.parent == nullptr);
    bool is_abs_containing_block = (node.position_type != 0 || is_root);
    if (!is_abs_containing_block) return;

    struct AbsInfo {
        LayoutNode* child;
        float parent_offset_x;
        float parent_offset_y;
    };
    std::vector<AbsInfo> abs_children;

    std::function<void(LayoutNode&, float, float)> collect_abs =
        [&](LayoutNode& parent, float offset_x, float offset_y) {
        for (auto& c : parent.children) {
            if (c->display == DisplayType::None || c->mode == LayoutMode::None) continue;
            if (c->position_type == 2) {
                // Absolute child belongs to this containing block.
                abs_children.push_back({c.get(), offset_x, offset_y});
                // Don't recurse; absolute elements form their own containing block.
                continue;
            }
            if (c->position_type != 0) {
                // Positioned descendants establish their own absolute containing block.
                continue;
            }
            // Static child — recurse, accumulating geometric offset
            collect_abs(*c, offset_x + c->geometry.x, offset_y + c->geometry.y);
        }
    };
    collect_abs(node, 0, 0);

    // Fixed positioning:
    // always positioned relative to viewport, never nearest positioned ancestor.
    std::vector<LayoutNode*> fixed_children;
    if (is_root) {
        std::function<void(LayoutNode&)> collect_fixed = [&](LayoutNode& parent) {
            for (auto& c : parent.children) {
                if (c->display == DisplayType::None || c->mode == LayoutMode::None) continue;
                if (c->position_type == 3) fixed_children.push_back(c.get());
                // Keep walking through all descendants; fixed is viewport-relative.
                collect_fixed(*c);
            }
        };
        collect_fixed(node);
    }

    auto layout_out_of_flow = [&](LayoutNode& child, float ref_w, float ref_h, float ox, float oy) {
        // Compute width for positioning context.
        if (child.specified_width >= 0) {
            child.geometry.width = child.specified_width;
        } else if (child.pos_left_set && child.pos_right_set) {
            child.geometry.width = ref_w - child.pos_left - child.pos_right;
        } else {
            child.geometry.width = ref_w;
        }

        switch (child.mode) {
            case LayoutMode::Block:
            case LayoutMode::InlineBlock:
                layout_block(child, child.geometry.width);
                break;
            case LayoutMode::Inline:
                layout_inline(child, child.geometry.width);
                break;
            case LayoutMode::Flex:
                layout_flex(child, child.geometry.width);
                break;
            case LayoutMode::Grid:
                layout_grid(child, child.geometry.width);
                break;
            case LayoutMode::Table:
                layout_table(child, child.geometry.width);
                break;
            default:
                layout_block(child, child.geometry.width);
                break;
        }

        if (child.pos_left_set) {
            child.geometry.x = child.pos_left - ox;
        } else if (child.pos_right_set) {
            child.geometry.x = ref_w - child.geometry.margin_box_width() - child.pos_right
                + child.geometry.margin.left - ox;
        } else {
            child.geometry.x = -ox;
        }

        if (child.pos_top_set) {
            child.geometry.y = child.pos_top - oy;
        } else if (child.pos_bottom_set) {
            child.geometry.y = ref_h - child.geometry.margin_box_height() - child.pos_bottom
                + child.geometry.margin.top - oy;
        } else {
            child.geometry.y = -oy;
        }
    };

    float container_w = node.geometry.width;
    float container_h = node.geometry.height;

    for (auto& [child, parent_ox, parent_oy] : abs_children) {
        layout_out_of_flow(*child, container_w, container_h, parent_ox, parent_oy);
    }

    for (auto* fixed_child : fixed_children) {
        layout_out_of_flow(*fixed_child, viewport_width_, viewport_height_, 0.0f, 0.0f);
    }
}

void LayoutEngine::layout_grid(LayoutNode& node, float containing_width) {
    if (node.display == DisplayType::None || node.mode == LayoutMode::None) {
        node.geometry.width = 0;
        node.geometry.height = 0;
        return;
    }

    // Resolve deferred percentage margins/padding
    if (node.css_margin_top.has_value())
        node.geometry.margin.top = node.css_margin_top->to_px(containing_width);
    if (node.css_margin_right.has_value())
        node.geometry.margin.right = node.css_margin_right->to_px(containing_width);
    if (node.css_margin_bottom.has_value())
        node.geometry.margin.bottom = node.css_margin_bottom->to_px(containing_width);
    if (node.css_margin_left.has_value())
        node.geometry.margin.left = node.css_margin_left->to_px(containing_width);
    if (node.css_padding_top.has_value())
        node.geometry.padding.top = node.css_padding_top->to_px(containing_width);
    if (node.css_padding_right.has_value())
        node.geometry.padding.right = node.css_padding_right->to_px(containing_width);
    if (node.css_padding_bottom.has_value())
        node.geometry.padding.bottom = node.css_padding_bottom->to_px(containing_width);
    if (node.css_padding_left.has_value())
        node.geometry.padding.left = node.css_padding_left->to_px(containing_width);
    if (is_margin_auto(node.geometry.margin.left)) node.geometry.margin.left = 0;
    if (is_margin_auto(node.geometry.margin.right)) node.geometry.margin.right = 0;

    // Compute width (non-root)
    if (node.parent != nullptr) {
        node.geometry.width = compute_width(node, containing_width);
    }

    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;
    if (content_w < 0) content_w = 0;

    // Grid gap: use proper grid gap fields, falling back to legacy gap fields for compat.
    // In production, render_pipeline sets both column_gap and column_gap_val to the same value,
    // and both row_gap and gap. In tests, only one set may be populated.
    float col_gap = node.column_gap > 0 ? node.column_gap : node.column_gap_val;
    float rg = node.row_gap > 0 ? node.row_gap : node.gap;

    // -----------------------------------------------------------------------
    // Helper: expand repeat() in a track template string
    // -----------------------------------------------------------------------
    auto expand_repeat = [&](const std::string& tmpl, float avail_size) -> std::string {
        std::string expanded;
        size_t pos = 0;
        while (pos < tmpl.size()) {
            size_t rep = tmpl.find("repeat(", pos);
            if (rep == std::string::npos) {
                expanded += tmpl.substr(pos);
                break;
            }
            expanded += tmpl.substr(pos, rep - pos);
            size_t paren_start = rep + 7;
            size_t comma = tmpl.find(',', paren_start);
            if (comma == std::string::npos) {
                expanded += tmpl.substr(pos);
                break;
            }
            std::string count_str = tmpl.substr(paren_start, comma - paren_start);
            while (!count_str.empty() && count_str.front() == ' ') count_str.erase(count_str.begin());
            while (!count_str.empty() && count_str.back() == ' ') count_str.pop_back();

            size_t depth = 1;
            size_t end_pos = comma + 1;
            while (end_pos < tmpl.size() && depth > 0) {
                if (tmpl[end_pos] == '(') depth++;
                else if (tmpl[end_pos] == ')') depth--;
                if (depth > 0) end_pos++;
            }
            std::string pattern = tmpl.substr(comma + 1, end_pos - comma - 1);
            while (!pattern.empty() && pattern.front() == ' ') pattern.erase(pattern.begin());
            while (!pattern.empty() && pattern.back() == ' ') pattern.pop_back();

            int count = 0;
            if (count_str == "auto-fill" || count_str == "auto-fit") {
                float track_size = 0;
                if (pattern.find("px") != std::string::npos) {
                    try { track_size = std::stof(pattern); } catch (...) {}
                } else if (pattern.find('%') != std::string::npos) {
                    float pct = 0;
                    try { pct = std::stof(pattern); } catch (...) {}
                    track_size = avail_size * pct / 100.0f;
                } else if (pattern.find("minmax(") != std::string::npos) {
                    size_t mmp = pattern.find("minmax(") + 7;
                    size_t mmc = pattern.find(',', mmp);
                    if (mmc != std::string::npos) {
                        std::string min_str = pattern.substr(mmp, mmc - mmp);
                        while (!min_str.empty() && min_str.front() == ' ') min_str.erase(min_str.begin());
                        if (min_str.find("px") != std::string::npos) {
                            try { track_size = std::stof(min_str); } catch (...) {}
                        }
                    }
                }
                float g = col_gap;
                if (track_size > 0) {
                    count = static_cast<int>((avail_size + g) / (track_size + g));
                    if (count < 1) count = 1;
                } else {
                    count = 1;
                }
            } else {
                try { count = std::stoi(count_str); }
                catch (...) { count = 1; }
            }

            for (int i = 0; i < count; i++) {
                if (!expanded.empty() && expanded.back() != ' ') expanded += ' ';
                expanded += pattern;
            }
            pos = end_pos + 1;
        }
        return expanded;
    };

    // -----------------------------------------------------------------------
    // Helper: tokenize a track template, keeping minmax() as single tokens
    // -----------------------------------------------------------------------
    auto tokenize_tracks = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> tokens;
        size_t ti = 0;
        while (ti < s.size()) {
            while (ti < s.size() && s[ti] == ' ') ti++;
            if (ti >= s.size()) break;
            if (ti + 7 <= s.size() && s.substr(ti, 7) == "minmax(") {
                size_t start = ti;
                int d = 0;
                while (ti < s.size()) {
                    if (s[ti] == '(') d++;
                    else if (s[ti] == ')') { d--; if (d == 0) { ti++; break; } }
                    ti++;
                }
                tokens.push_back(s.substr(start, ti - start));
            } else {
                size_t start = ti;
                while (ti < s.size() && s[ti] != ' ') ti++;
                tokens.push_back(s.substr(start, ti - start));
            }
        }
        return tokens;
    };

    // -----------------------------------------------------------------------
    // TrackSpec: describes a single grid track (column or row)
    // -----------------------------------------------------------------------
    struct TrackSpec { float value; bool is_fr; float min_value; };

    // -----------------------------------------------------------------------
    // Helper: parse track tokens into TrackSpec vector
    // -----------------------------------------------------------------------
    auto parse_track_specs = [&](const std::vector<std::string>& tokens, float avail) {
        std::vector<TrackSpec> specs;
        for (const auto& tk : tokens) {
            if (tk.size() >= 7 && tk.substr(0, 7) == "minmax(") {
                size_t mmp = 7;
                size_t mmc = tk.find(',', mmp);
                if (mmc != std::string::npos) {
                    std::string min_str = tk.substr(mmp, mmc - mmp);
                    std::string max_str = tk.substr(mmc + 1);
                    if (!max_str.empty() && max_str.back() == ')') max_str.pop_back();
                    while (!min_str.empty() && min_str.front() == ' ') min_str.erase(min_str.begin());
                    while (!min_str.empty() && min_str.back() == ' ') min_str.pop_back();
                    while (!max_str.empty() && max_str.front() == ' ') max_str.erase(max_str.begin());
                    while (!max_str.empty() && max_str.back() == ' ') max_str.pop_back();

                    float min_val = 0;
                    if (min_str.find("px") != std::string::npos) {
                        try { min_val = std::stof(min_str); } catch (...) {}
                    } else if (min_str.find('%') != std::string::npos) {
                        float pct = 0;
                        try { pct = std::stof(min_str); } catch (...) {}
                        min_val = avail * pct / 100.0f;
                    }

                    if (max_str.find("fr") != std::string::npos) {
                        float fr = 0;
                        try { fr = std::stof(max_str); } catch (...) { fr = 1; }
                        specs.push_back({fr, true, min_val});
                    } else if (max_str.find("px") != std::string::npos) {
                        float px = 0;
                        try { px = std::stof(max_str); } catch (...) {}
                        specs.push_back({std::max(px, min_val), false, min_val});
                    } else if (max_str.find('%') != std::string::npos) {
                        float pct = 0;
                        try { pct = std::stof(max_str); } catch (...) {}
                        float px = avail * pct / 100.0f;
                        specs.push_back({std::max(px, min_val), false, min_val});
                    } else if (max_str == "auto") {
                        specs.push_back({1.0f, true, min_val});
                    } else {
                        specs.push_back({min_val, false, min_val});
                    }
                } else {
                    specs.push_back({1.0f, true, 0.0f});
                }
            } else if (tk.find("fr") != std::string::npos) {
                float fr = 0;
                try { fr = std::stof(tk); } catch (...) { fr = 1; }
                specs.push_back({fr, true, 0.0f});
            } else if (tk.find("px") != std::string::npos) {
                float px = 0;
                try { px = std::stof(tk); } catch (...) {}
                specs.push_back({px, false, 0.0f});
            } else if (tk.find('%') != std::string::npos) {
                float pct = 0;
                try { pct = std::stof(tk); } catch (...) {}
                specs.push_back({avail * pct / 100.0f, false, 0.0f});
            } else if (tk == "auto") {
                specs.push_back({1.0f, true, 0.0f});
            } else {
                float val = 0;
                try { val = std::stof(tk); } catch (...) {}
                specs.push_back({val, false, 0.0f});
            }
        }
        return specs;
    };

    // -----------------------------------------------------------------------
    // Helper: resolve track sizes from specs, distributing fr units
    // -----------------------------------------------------------------------
    auto resolve_tracks = [](const std::vector<TrackSpec>& specs, float avail, float gap_size) {
        std::vector<float> sizes;
        float total_fr = 0;
        float fixed_total = 0;
        for (const auto& s : specs) {
            if (s.is_fr) total_fr += s.value;
            else fixed_total += s.value;
        }
        float gaps_total = specs.size() > 1 ? (static_cast<float>(specs.size()) - 1.0f) * gap_size : 0;
        float fr_space = avail - fixed_total - gaps_total;
        if (fr_space < 0) fr_space = 0;
        float fr_size = total_fr > 0 ? fr_space / total_fr : 0;
        for (const auto& s : specs) {
            float w = s.is_fr ? s.value * fr_size : s.value;
            if (s.min_value > 0 && w < s.min_value) w = s.min_value;
            sizes.push_back(w);
        }
        return sizes;
    };

    // -----------------------------------------------------------------------
    // Step 1: Parse grid-template-columns
    // -----------------------------------------------------------------------
    std::vector<float> col_widths;
    std::string cols = node.grid_template_columns;
    if (!cols.empty()) {
        std::string expanded = expand_repeat(cols, content_w);
        auto tokens = tokenize_tracks(expanded);
        auto specs = parse_track_specs(tokens, content_w);
        col_widths = resolve_tracks(specs, content_w, col_gap);
    }

    if (col_widths.empty()) {
        // If grid-auto-columns is set and no explicit template, use auto columns
        if (!node.grid_auto_columns.empty()) {
            float auto_w = 0;
            try {
                std::string ac = node.grid_auto_columns;
                if (ac.find("px") != std::string::npos) {
                    auto_w = std::stof(ac);
                } else if (ac.find('%') != std::string::npos) {
                    float pct = std::stof(ac);
                    auto_w = content_w * pct / 100.0f;
                } else if (ac.find("fr") != std::string::npos) {
                    auto_w = content_w;
                }
            } catch (...) {}
            if (auto_w > 0) {
                float g = col_gap;
                int auto_count = std::max(1, static_cast<int>((content_w + g) / (auto_w + g)));
                for (int i = 0; i < auto_count; i++) col_widths.push_back(auto_w);
            }
        }
        if (col_widths.empty()) {
            col_widths.push_back(content_w); // single column fallback
        }
    }

    int num_cols = static_cast<int>(col_widths.size());

    // -----------------------------------------------------------------------
    // Step 2: Parse grid-template-rows (full parsing: px, fr, %, auto, minmax, repeat)
    // -----------------------------------------------------------------------
    std::vector<TrackSpec> row_specs;
    std::string rows_tmpl = node.grid_template_rows;
    if (!rows_tmpl.empty()) {
        std::string expanded = expand_repeat(rows_tmpl, 0);
        auto tokens = tokenize_tracks(expanded);
        row_specs = parse_track_specs(tokens, 0);
    }

    // -----------------------------------------------------------------------
    // Step 3: Collect in-flow children
    // -----------------------------------------------------------------------
    std::vector<LayoutNode*> grid_items;
    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (child->position_type == 2 || child->position_type == 3) continue;
        grid_items.push_back(child.get());
    }

    // -----------------------------------------------------------------------
    // Helper: parse a grid-column or grid-row value into (start_line, span)
    // Returns {-1, 1} if auto-placement should be used.
    // Supports: "N", "N / M", "N / span M", "span M / N", "span M"
    // Grid lines are 1-based; returned start is 0-based index.
    // -----------------------------------------------------------------------
    auto parse_line_spec = [](const std::string& spec, int max_tracks) -> std::pair<int, int> {
        if (spec.empty()) return {-1, 1};

        auto slash = spec.find('/');
        if (slash != std::string::npos) {
            std::string start_str = spec.substr(0, slash);
            std::string end_str = spec.substr(slash + 1);
            while (!start_str.empty() && start_str.front() == ' ') start_str.erase(start_str.begin());
            while (!start_str.empty() && start_str.back() == ' ') start_str.pop_back();
            while (!end_str.empty() && end_str.front() == ' ') end_str.erase(end_str.begin());
            while (!end_str.empty() && end_str.back() == ' ') end_str.pop_back();

            // Handle "span N" in end position: e.g. "1 / span 2"
            if (end_str.size() > 5 && end_str.substr(0, 5) == "span ") {
                int span_val = 1;
                try { span_val = std::stoi(end_str.substr(5)); } catch (...) {}
                if (span_val < 1) span_val = 1;
                if (start_str == "auto") return {-1, span_val};
                try {
                    int start = std::stoi(start_str);
                    if (start >= 1) return {start - 1, span_val};
                } catch (...) {}
                return {-1, span_val};
            }

            // Handle "span N" in start position: e.g. "span 2 / 4"
            if (start_str.size() > 5 && start_str.substr(0, 5) == "span ") {
                int span_val = 1;
                try { span_val = std::stoi(start_str.substr(5)); } catch (...) {}
                if (span_val < 1) span_val = 1;
                try {
                    int end_line = std::stoi(end_str);
                    if (end_line >= 1) {
                        int s = end_line - 1 - span_val;
                        if (s < 0) s = 0;
                        return {s, span_val};
                    }
                } catch (...) {}
                return {-1, span_val};
            }

            // Normal "start / end" (both line numbers, 1-based)
            if (start_str == "auto" && end_str == "auto") return {-1, 1};
            try {
                int start = (start_str == "auto") ? -1 : std::stoi(start_str);
                int end_line = (end_str == "auto") ? -1 : std::stoi(end_str);
                // Handle negative line numbers (count from end)
                if (start < 0 && start != -1) start = max_tracks + 1 + start + 1;
                if (end_line < 0 && end_line != -1) end_line = max_tracks + 1 + end_line + 1;
                if (start >= 1 && end_line > start) {
                    return {start - 1, end_line - start};
                }
                if (start >= 1 && end_line == -1) return {start - 1, 1};
            } catch (...) {}
            return {-1, 1};
        }

        // No slash: single value
        if (spec.size() > 5 && spec.substr(0, 5) == "span ") {
            int span_val = 1;
            try { span_val = std::stoi(spec.substr(5)); } catch (...) {}
            return {-1, std::max(1, span_val)};
        }

        if (spec != "auto") {
            try {
                int line = std::stoi(spec);
                if (line >= 1) return {line - 1, 1};
            } catch (...) {}
        }
        return {-1, 1};
    };

    // -----------------------------------------------------------------------
    // Step 4: Parse grid-template-areas
    // -----------------------------------------------------------------------
    struct AreaRect { int row_start; int col_start; int row_end; int col_end; };
    std::unordered_map<std::string, AreaRect> area_map;
    if (!node.grid_template_areas.empty()) {
        std::vector<std::vector<std::string>> area_grid;
        std::string areas = node.grid_template_areas;
        size_t qpos = 0;
        while (qpos < areas.size()) {
            size_t q1 = areas.find('"', qpos);
            if (q1 == std::string::npos) q1 = areas.find('\'', qpos);
            if (q1 == std::string::npos) break;
            char q_char = areas[q1];
            size_t q2 = areas.find(q_char, q1 + 1);
            if (q2 == std::string::npos) break;
            std::string row_str = areas.substr(q1 + 1, q2 - q1 - 1);
            std::vector<std::string> row_cells;
            std::istringstream rss(row_str);
            std::string cell;
            while (rss >> cell) {
                row_cells.push_back(cell);
            }
            area_grid.push_back(std::move(row_cells));
            qpos = q2 + 1;
        }

        // Build area_map: for each named area, find its bounding rectangle
        for (int r = 0; r < static_cast<int>(area_grid.size()); r++) {
            for (int c = 0; c < static_cast<int>(area_grid[r].size()); c++) {
                const auto& name = area_grid[r][c];
                if (name == ".") continue; // '.' means empty cell
                auto it = area_map.find(name);
                if (it == area_map.end()) {
                    area_map[name] = {r, c, r + 1, c + 1};
                } else {
                    it->second.row_start = std::min(it->second.row_start, r);
                    it->second.col_start = std::min(it->second.col_start, c);
                    it->second.row_end = std::max(it->second.row_end, r + 1);
                    it->second.col_end = std::max(it->second.col_end, c + 1);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Step 5: Determine each item's grid position using a 2D occupation grid
    // -----------------------------------------------------------------------
    struct GridPlacement {
        int col_start = -1;
        int row_start = -1;
        int col_span = 1;
        int row_span = 1;
    };
    std::vector<GridPlacement> placements(grid_items.size());

    // First pass: resolve grid-area and explicit grid-column / grid-row
    for (size_t idx = 0; idx < grid_items.size(); idx++) {
        auto* child = grid_items[idx];
        auto& pl = placements[idx];

        if (!child->grid_area.empty() && !area_map.empty()) {
            auto ait = area_map.find(child->grid_area);
            if (ait != area_map.end()) {
                auto& ar = ait->second;
                child->grid_column = std::to_string(ar.col_start + 1) + " / " + std::to_string(ar.col_end + 1);
                child->grid_row = std::to_string(ar.row_start + 1) + " / " + std::to_string(ar.row_end + 1);
            }
        }

        auto col_result = parse_line_spec(child->grid_column, num_cols);
        pl.col_start = col_result.first;
        pl.col_span = col_result.second;

        int est_rows = static_cast<int>(row_specs.size());
        if (est_rows < 1) est_rows = static_cast<int>((grid_items.size() + num_cols - 1) / num_cols);
        auto row_result = parse_line_spec(child->grid_row, est_rows);
        pl.row_start = row_result.first;
        pl.row_span = row_result.second;
    }

    int num_rows = static_cast<int>(row_specs.size());
    for (const auto& pl : placements) {
        if (pl.row_start >= 0) {
            num_rows = std::max(num_rows, pl.row_start + pl.row_span);
        }
    }

    bool column_flow = (node.grid_auto_flow == 1 || node.grid_auto_flow == 3);

    int estimated_rows = std::max(num_rows, static_cast<int>((grid_items.size() + num_cols - 1) / num_cols));
    estimated_rows = std::max(estimated_rows, 1);
    std::vector<bool> occupied(estimated_rows * num_cols, false);

    auto is_occupied = [&](int r, int c) -> bool {
        if (r < 0 || c < 0 || c >= num_cols) return true;
        if (r >= static_cast<int>(occupied.size()) / num_cols) return false;
        return occupied[r * num_cols + c];
    };

    auto mark_occupied = [&](int r_start, int c_start, int r_span, int c_span) {
        int needed_rows = r_start + r_span;
        int current_rows = static_cast<int>(occupied.size()) / num_cols;
        if (needed_rows > current_rows) {
            occupied.resize(needed_rows * num_cols, false);
        }
        for (int r = r_start; r < r_start + r_span; r++) {
            for (int c = c_start; c < c_start + c_span && c < num_cols; c++) {
                occupied[r * num_cols + c] = true;
            }
        }
    };

    auto can_place = [&](int r_start, int c_start, int r_span, int c_span) -> bool {
        if (c_start + c_span > num_cols) return false;
        for (int r = r_start; r < r_start + r_span; r++) {
            for (int c = c_start; c < c_start + c_span; c++) {
                if (is_occupied(r, c)) return false;
            }
        }
        return true;
    };

    // Place explicitly-positioned items first
    for (size_t idx = 0; idx < grid_items.size(); idx++) {
        auto& pl = placements[idx];
        if (pl.col_start >= 0 && pl.row_start >= 0) {
            mark_occupied(pl.row_start, pl.col_start, pl.row_span, pl.col_span);
        }
    }

    // Auto-place remaining items
    int auto_cursor_row = 0;
    int auto_cursor_col = 0;

    for (size_t idx = 0; idx < grid_items.size(); idx++) {
        auto& pl = placements[idx];
        if (pl.col_start >= 0 && pl.row_start >= 0) continue;

        if (pl.col_start >= 0) {
            for (int r = 0; ; r++) {
                if (can_place(r, pl.col_start, pl.row_span, pl.col_span)) {
                    pl.row_start = r;
                    break;
                }
                if (r > 10000) { pl.row_start = 0; break; }
            }
        } else if (pl.row_start >= 0) {
            for (int c = 0; c < num_cols; c++) {
                if (can_place(pl.row_start, c, pl.row_span, pl.col_span)) {
                    pl.col_start = c;
                    break;
                }
            }
            if (pl.col_start < 0) pl.col_start = 0;
        } else {
            if (column_flow) {
                for (int iter = 0; iter < 100000; iter++) {
                    if (can_place(auto_cursor_row, auto_cursor_col, pl.row_span, pl.col_span)) {
                        pl.row_start = auto_cursor_row;
                        pl.col_start = auto_cursor_col;
                        break;
                    }
                    auto_cursor_row++;
                    int max_auto = static_cast<int>(occupied.size()) / num_cols + pl.row_span + 1;
                    if (auto_cursor_row >= max_auto) {
                        auto_cursor_row = 0;
                        auto_cursor_col++;
                        if (auto_cursor_col >= num_cols) {
                            pl.row_start = 0;
                            pl.col_start = 0;
                            break;
                        }
                    }
                }
                if (pl.row_start < 0) { pl.row_start = 0; pl.col_start = 0; }
            } else {
                for (int iter = 0; iter < 100000; iter++) {
                    if (auto_cursor_col + pl.col_span <= num_cols &&
                        can_place(auto_cursor_row, auto_cursor_col, pl.row_span, pl.col_span)) {
                        pl.row_start = auto_cursor_row;
                        pl.col_start = auto_cursor_col;
                        break;
                    }
                    auto_cursor_col++;
                    if (auto_cursor_col + pl.col_span > num_cols) {
                        auto_cursor_col = 0;
                        auto_cursor_row++;
                    }
                }
                if (pl.row_start < 0) { pl.row_start = 0; pl.col_start = 0; }
            }
        }

        if (pl.col_start + pl.col_span > num_cols) {
            pl.col_start = std::max(0, num_cols - pl.col_span);
        }

        mark_occupied(pl.row_start, pl.col_start, pl.row_span, pl.col_span);

        if (!column_flow) {
            auto_cursor_col = pl.col_start + pl.col_span;
            auto_cursor_row = pl.row_start;
            if (auto_cursor_col >= num_cols) {
                auto_cursor_col = 0;
                auto_cursor_row++;
            }
        } else {
            auto_cursor_row = pl.row_start + pl.row_span;
            auto_cursor_col = pl.col_start;
        }
    }

    // -----------------------------------------------------------------------
    // Step 6: Determine final row count
    // -----------------------------------------------------------------------
    num_rows = 0;
    for (const auto& pl : placements) {
        num_rows = std::max(num_rows, pl.row_start + pl.row_span);
    }
    if (num_rows < 1 && !grid_items.empty()) num_rows = 1;

    // -----------------------------------------------------------------------
    // Step 7: Layout each child and collect per-row content heights
    // -----------------------------------------------------------------------
    float col_x_start = node.geometry.padding.left + node.geometry.border.left;

    std::vector<float> row_content_heights(num_rows, 0.0f);

    for (size_t idx = 0; idx < grid_items.size(); idx++) {
        auto* child = grid_items[idx];
        auto& pl = placements[idx];

        float cell_width = 0;
        for (int c = pl.col_start; c < pl.col_start + pl.col_span && c < num_cols; c++) {
            cell_width += col_widths[c];
            if (c > pl.col_start) cell_width += col_gap;
        }

        {
            int justify = node.justify_items;
            if (child->justify_self >= 0) justify = child->justify_self;
            if (justify != 3 && child->specified_width > 0) {
                child->geometry.width = child->specified_width;
            } else {
                child->geometry.width = cell_width;
            }
        }

        switch (child->mode) {
            case LayoutMode::Block:
            case LayoutMode::InlineBlock:
                layout_block(*child, cell_width);
                break;
            case LayoutMode::Inline:
                layout_inline(*child, cell_width);
                break;
            case LayoutMode::Flex:
                layout_flex(*child, cell_width);
                break;
            case LayoutMode::Grid:
                layout_grid(*child, cell_width);
                break;
            default:
                layout_block(*child, cell_width);
                break;
        }

        float child_h = child->geometry.margin_box_height();
        float per_row_h = child_h / static_cast<float>(pl.row_span);
        for (int r = pl.row_start; r < pl.row_start + pl.row_span && r < num_rows; r++) {
            row_content_heights[r] = std::max(row_content_heights[r], per_row_h);
        }
    }

    // -----------------------------------------------------------------------
    // Step 8: Resolve row heights from row_specs + content heights + grid-auto-rows
    // -----------------------------------------------------------------------
    std::vector<float> row_heights(num_rows, 0.0f);

    float grid_auto_row_h = 0;
    if (!node.grid_auto_rows.empty()) {
        try {
            std::string ar = node.grid_auto_rows;
            if (ar.find("px") != std::string::npos) {
                grid_auto_row_h = std::stof(ar);
            } else if (ar.find('%') != std::string::npos) {
                float pct = std::stof(ar);
                grid_auto_row_h = node.geometry.height * pct / 100.0f;
            }
        } catch (...) {}
    }

    float row_fr_total = 0;
    float row_fixed_total = 0;
    for (int r = 0; r < num_rows; r++) {
        if (r < static_cast<int>(row_specs.size())) {
            auto& s = row_specs[r];
            if (s.is_fr) {
                row_fr_total += s.value;
                row_heights[r] = std::max(row_content_heights[r], s.min_value);
            } else {
                row_heights[r] = std::max(s.value, row_content_heights[r]);
                row_fixed_total += row_heights[r];
            }
        } else {
            if (grid_auto_row_h > 0) {
                row_heights[r] = std::max(grid_auto_row_h, row_content_heights[r]);
            } else {
                row_heights[r] = row_content_heights[r];
            }
            row_fixed_total += row_heights[r];
        }
    }

    if (row_fr_total > 0 && node.specified_height >= 0) {
        float container_content_h = node.specified_height
            - node.geometry.padding.top - node.geometry.padding.bottom
            - node.geometry.border.top - node.geometry.border.bottom;
        float row_gaps_total = num_rows > 1 ? (static_cast<float>(num_rows) - 1.0f) * rg : 0;
        float fr_space = container_content_h - row_fixed_total - row_gaps_total;
        if (fr_space < 0) fr_space = 0;
        float fr_size = fr_space / row_fr_total;

        for (int r = 0; r < num_rows && r < static_cast<int>(row_specs.size()); r++) {
            if (row_specs[r].is_fr) {
                float fr_h = row_specs[r].value * fr_size;
                row_heights[r] = std::max(row_heights[r], fr_h);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Step 9: Position items based on their grid placement
    // -----------------------------------------------------------------------
    std::vector<float> row_y_positions(num_rows);
    {
        float y = node.geometry.padding.top + node.geometry.border.top;
        for (int r = 0; r < num_rows; r++) {
            row_y_positions[r] = y;
            y += row_heights[r];
            if (r < num_rows - 1) y += rg;
        }
    }

    for (size_t idx = 0; idx < grid_items.size(); idx++) {
        auto* child = grid_items[idx];
        auto& pl = placements[idx];

        float col_x = col_x_start;
        for (int c = 0; c < pl.col_start; c++) {
            col_x += col_widths[c] + col_gap;
        }

        float cell_width = 0;
        for (int c = pl.col_start; c < pl.col_start + pl.col_span && c < num_cols; c++) {
            cell_width += col_widths[c];
            if (c > pl.col_start) cell_width += col_gap;
        }

        float cell_height = 0;
        for (int r = pl.row_start; r < pl.row_start + pl.row_span && r < num_rows; r++) {
            cell_height += row_heights[r];
            if (r > pl.row_start) cell_height += rg;
        }

        float child_h = child->geometry.margin_box_height();
        int align = node.align_items;
        if (child->align_self >= 0) align = child->align_self;
        if (align == 4 && child->specified_height < 0) {
            float stretch_h = cell_height
                - child->geometry.margin.top - child->geometry.margin.bottom
                - child->geometry.border.top - child->geometry.border.bottom
                - child->geometry.padding.top - child->geometry.padding.bottom;
            if (stretch_h > child->geometry.height) {
                child->geometry.height = stretch_h;
            }
            child_h = child->geometry.margin_box_height();
        }

        child->geometry.x = col_x;

        float child_actual_w = child->geometry.margin_box_width();
        if (child_actual_w < cell_width) {
            int justify = node.justify_items;
            if (child->justify_self >= 0) justify = child->justify_self;
            if (justify == 2) {
                child->geometry.x = col_x + (cell_width - child_actual_w) / 2.0f;
            } else if (justify == 1) {
                child->geometry.x = col_x + cell_width - child_actual_w;
            }
        }

        float row_y_pos = (pl.row_start < num_rows) ? row_y_positions[pl.row_start] : 0;
        child->geometry.y = row_y_pos;

        if (child_h < cell_height) {
            if (align == 2) {
                child->geometry.y = row_y_pos + (cell_height - child_h) / 2.0f;
            } else if (align == 1) {
                child->geometry.y = row_y_pos + cell_height - child_h;
            }
        }

        apply_positioning(*child);
    }

    // -----------------------------------------------------------------------
    // Step 10: Compute container height
    // -----------------------------------------------------------------------
    float total_content_height = 0;
    for (int r = 0; r < num_rows; r++) {
        total_content_height += row_heights[r];
        if (r < num_rows - 1) total_content_height += rg;
    }
    float total_height = total_content_height
        + node.geometry.padding.top + node.geometry.padding.bottom
        + node.geometry.border.top + node.geometry.border.bottom;

    if (node.specified_height >= 0) {
        node.geometry.height = node.specified_height;
    } else {
        node.geometry.height = std::max(node.geometry.height, total_height);
    }
    node.geometry.height = std::max(node.geometry.height, node.min_height);
    node.geometry.height = std::min(node.geometry.height, node.max_height);

    // -----------------------------------------------------------------------
    // Step 11: align-content -- distribute remaining vertical space among rows
    // -----------------------------------------------------------------------
    if (node.align_content > 0 && num_rows > 0) {
        float container_content_h = node.geometry.height
            - node.geometry.padding.top - node.geometry.padding.bottom
            - node.geometry.border.top - node.geometry.border.bottom;
        if (total_content_height < container_content_h) {
            float extra = container_content_h - total_content_height;
            float offset = 0;
            float inter_row_extra = 0;
            if (node.align_content == 1) {
                offset = extra;
            } else if (node.align_content == 2) {
                offset = extra / 2.0f;
            } else if (node.align_content == 4 && num_rows > 1) {
                inter_row_extra = extra / static_cast<float>(num_rows - 1);
            } else if (node.align_content == 5 && num_rows > 0) {
                inter_row_extra = extra / static_cast<float>(num_rows);
                offset = inter_row_extra / 2.0f;
            }
            if (offset > 0 || inter_row_extra > 0) {
                for (size_t idx = 0; idx < grid_items.size(); idx++) {
                    auto& pl = placements[idx];
                    float shift = offset + static_cast<float>(pl.row_start) * inter_row_extra;
                    grid_items[idx]->geometry.y += shift;
                }
            }
        }
    }

    // Position absolute/fixed children
    position_absolute_children(node);
}

void LayoutEngine::layout_table(LayoutNode& node, float containing_width) {
    if (node.display == DisplayType::None || node.mode == LayoutMode::None) {
        node.geometry.width = 0;
        node.geometry.height = 0;
        return;
    }

    // Resolve deferred percentage margins/padding
    if (node.css_margin_top.has_value())
        node.geometry.margin.top = node.css_margin_top->to_px(containing_width);
    if (node.css_margin_right.has_value())
        node.geometry.margin.right = node.css_margin_right->to_px(containing_width);
    if (node.css_margin_bottom.has_value())
        node.geometry.margin.bottom = node.css_margin_bottom->to_px(containing_width);
    if (node.css_margin_left.has_value())
        node.geometry.margin.left = node.css_margin_left->to_px(containing_width);
    if (node.css_padding_top.has_value())
        node.geometry.padding.top = node.css_padding_top->to_px(containing_width);
    if (node.css_padding_right.has_value())
        node.geometry.padding.right = node.css_padding_right->to_px(containing_width);
    if (node.css_padding_bottom.has_value())
        node.geometry.padding.bottom = node.css_padding_bottom->to_px(containing_width);
    if (node.css_padding_left.has_value())
        node.geometry.padding.left = node.css_padding_left->to_px(containing_width);
    if (is_margin_auto(node.geometry.margin.left)) node.geometry.margin.left = 0;
    if (is_margin_auto(node.geometry.margin.right)) node.geometry.margin.right = 0;

    // Compute table width
    if (node.parent != nullptr) {
        node.geometry.width = compute_width(node, containing_width);
    }

    float content_w = node.geometry.width
        - node.geometry.padding.left - node.geometry.padding.right
        - node.geometry.border.left - node.geometry.border.right;
    if (content_w < 0) content_w = 0;

    // Determine border-spacing: 0 when border-collapse is true
    // Horizontal and vertical spacing (CSS border-spacing two-value syntax)
    float h_spacing = node.border_collapse ? 0.0f : node.border_spacing;
    float v_spacing = node.border_collapse ? 0.0f
        : ((node.border_spacing_v > 0) ? node.border_spacing_v : node.border_spacing);

    // Collect table rows, flattening thead/tbody/tfoot wrappers
    // Also separate out <caption> elements for separate layout
    std::vector<LayoutNode*> rows;
    std::vector<LayoutNode*> captions;
    auto is_table_section_tag = [](const std::string& tag) {
        if (tag.size() < 5) return false;
        std::string t;
        t.reserve(tag.size());
        for (char c : tag) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return t == "thead" || t == "tbody" || t == "tfoot";
    };
    auto is_caption = [](const std::string& tag) {
        if (tag.size() != 7) return false;
        std::string t;
        t.reserve(tag.size());
        for (char c : tag) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return t == "caption";
    };
    auto is_table_row_tag = [](const std::string& tag) {
        if (tag.size() != 2) return false;
        std::string t;
        t.reserve(tag.size());
        for (char c : tag) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return t == "tr";
    };
    auto is_table_cell_tag = [](const std::string& tag) {
        if (tag.size() != 2) return false;
        std::string t;
        t.reserve(tag.size());
        for (char c : tag) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return t == "td" || t == "th";
    };
    auto is_table_row = [&](const LayoutNode& candidate) {
        return candidate.display == DisplayType::TableRow || is_table_row_tag(candidate.tag_name);
    };
    auto is_table_cell = [&](const LayoutNode& candidate) {
        return candidate.display == DisplayType::TableCell || is_table_cell_tag(candidate.tag_name);
    };
    auto is_table_section = [&](const LayoutNode& candidate) {
        if (is_table_section_tag(candidate.tag_name)) return true;
        if (candidate.display == DisplayType::TableRow || candidate.display == DisplayType::TableCell) return false;
        for (const auto& grandchild : candidate.children) {
            if (grandchild->display == DisplayType::None || grandchild->mode == LayoutMode::None) continue;
            if (grandchild->position_type == 2 || grandchild->position_type == 3) continue;
            if (is_table_row(*grandchild)) return true;
        }
        return false;
    };
    for (auto& child : node.children) {
        if (child->display == DisplayType::None || child->mode == LayoutMode::None) {
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (child->position_type == 2 || child->position_type == 3) continue;
        if (is_caption(child->tag_name)) {
            captions.push_back(child.get());
            continue;
        }
        if (is_table_section(*child)) {
            // Flatten: collect tr children from thead/tbody/tfoot
            for (auto& grandchild : child->children) {
                if (grandchild->display == DisplayType::None || grandchild->mode == LayoutMode::None) {
                    grandchild->geometry.width = 0;
                    grandchild->geometry.height = 0;
                    continue;
                }
                if (grandchild->position_type == 2 || grandchild->position_type == 3) continue;
                if (is_table_row(*grandchild)) {
                    rows.push_back(grandchild.get());
                }
            }
            // Zero out the section wrapper dimensions (it's transparent)
            child->geometry.width = 0;
            child->geometry.height = 0;
            continue;
        }
        if (is_table_row(*child)) {
            rows.push_back(child.get());
        }
    }

    // ---- Layout captions with caption_side=top before the table rows ----
    float caption_top_height = 0;
    float caption_bottom_height = 0;
    for (auto* cap : captions) {
        layout_block(*cap, content_w);
        cap->geometry.x = node.geometry.padding.left + node.geometry.border.left;
        if (cap->caption_side == 0) { // top
            cap->geometry.y = node.geometry.padding.top + node.geometry.border.top + caption_top_height;
            caption_top_height += cap->geometry.margin_box_height();
        }
        // bottom captions positioned later
    }

    if (rows.empty()) {
        // Layout bottom captions even if no rows
        float bottom_y = node.geometry.padding.top + node.geometry.border.top + caption_top_height;
        for (auto* cap : captions) {
            if (cap->caption_side == 1) { // bottom
                cap->geometry.y = bottom_y + caption_bottom_height;
                caption_bottom_height += cap->geometry.margin_box_height();
            }
        }
        float h = compute_height(node);
        node.geometry.height = (h >= 0) ? h : (node.geometry.padding.top + node.geometry.padding.bottom
            + node.geometry.border.top + node.geometry.border.bottom
            + caption_top_height + caption_bottom_height);
        return;
    }

    // Determine column count across ALL rows (including rowspan occupancy).
    // Relying only on the first row undercounts columns on pages that start
    // with a short/header row, which can collapse later cells to zero width.
    LayoutNode* first_row = rows[0];
    std::vector<LayoutNode*> first_row_cells;
    for (auto& cell : first_row->children) {
        if (cell->display == DisplayType::None || cell->mode == LayoutMode::None) continue;
        if (cell->position_type == 2 || cell->position_type == 3) continue;
        if (!is_table_cell(*cell)) continue;
        first_row_cells.push_back(cell.get());
    }

    int num_cols = 0;
    std::vector<std::vector<uint8_t>> scan_occupancy;
    for (size_t ri = 0; ri < rows.size(); ri++) {
        if (scan_occupancy.size() <= ri) {
            scan_occupancy.resize(ri + 1);
        }
        if (scan_occupancy[ri].size() < static_cast<size_t>(num_cols)) {
            scan_occupancy[ri].resize(static_cast<size_t>(num_cols), 0);
        }

        int col_idx = 0;
        for (auto& cell : rows[ri]->children) {
            if (cell->display == DisplayType::None || cell->mode == LayoutMode::None) continue;
            if (cell->position_type == 2 || cell->position_type == 3) continue;
            if (!is_table_cell(*cell)) continue;

            while (col_idx < num_cols &&
                   scan_occupancy[ri][static_cast<size_t>(col_idx)] != 0) {
                col_idx++;
            }

            int cspan = std::max(1, cell->colspan);
            int rspan = std::max(1, cell->rowspan);
            int needed_cols = col_idx + cspan;
            if (needed_cols > num_cols) {
                num_cols = needed_cols;
                for (auto& occ_row : scan_occupancy) {
                    occ_row.resize(static_cast<size_t>(num_cols), 0);
                }
            }

            if (rspan > 1) {
                for (int dr = 1; dr < rspan; dr++) {
                    size_t rr = ri + static_cast<size_t>(dr);
                    if (scan_occupancy.size() <= rr) {
                        scan_occupancy.resize(rr + 1);
                    }
                    if (scan_occupancy[rr].size() < static_cast<size_t>(num_cols)) {
                        scan_occupancy[rr].resize(static_cast<size_t>(num_cols), 0);
                    }
                    for (int dc = 0; dc < cspan && (col_idx + dc) < num_cols; dc++) {
                        scan_occupancy[rr][static_cast<size_t>(col_idx + dc)] = 1;
                    }
                }
            }
            col_idx += cspan;
        }
    }
    if (num_cols == 0) num_cols = 1;

    // CSS spec: border-spacing applies between cells and at the edges of the table
    float total_h_spacing = h_spacing * static_cast<float>(num_cols + 1);
    float available_for_cols = content_w - total_h_spacing;
    if (available_for_cols < 0) available_for_cols = 0;

    std::vector<float> col_widths(static_cast<size_t>(num_cols), -1.0f); // -1 = auto

    // ---- Seed column widths from <col> elements (colgroup/col) ----
    if (!node.col_widths.empty()) {
        for (size_t ci = 0; ci < node.col_widths.size() && ci < static_cast<size_t>(num_cols); ci++) {
            if (node.col_widths[ci] >= 0) {
                col_widths[ci] = node.col_widths[ci];
            }
        }
    }

    bool is_fixed_layout = (node.table_layout == 1);

    const TextMeasureFn* intrinsic_measurer = text_measurer_ ? &text_measurer_ : nullptr;

    if (is_fixed_layout) {
        // ---- Fixed table layout (CSS 2.1 Section 17.5.2.1) ----
        // Column widths are determined from the first row only.
        int col_idx = 0;
        for (auto* cell : first_row_cells) {
            int span = cell->colspan;
            if (col_idx >= num_cols) break;
            float width_hint = -1.0f;
            if (cell->specified_width >= 0) {
                width_hint = cell->specified_width;
            } else if (cell->css_width.has_value()) {
                width_hint = cell->css_width->to_px(available_for_cols);
            }

            if (width_hint >= 0 && span == 1) {
                col_widths[static_cast<size_t>(col_idx)] = width_hint;
            } else if (width_hint >= 0 && span > 1) {
                float per_col = width_hint / static_cast<float>(span);
                for (int s = 0; s < span && (col_idx + s) < num_cols; s++) {
                    col_widths[static_cast<size_t>(col_idx + s)] = per_col;
                }
            }
            col_idx += span;
        }
    } else {
        // ---- Auto table layout (CSS 2.1 Section 17.5.2.2) ----
        // Scan ALL rows: use maximum explicit/intrinsic width per column.
        for (auto* row : rows) {
            int col_idx = 0;
            for (auto& cell : row->children) {
                if (cell->display == DisplayType::None || cell->mode == LayoutMode::None) continue;
                if (cell->position_type == 2 || cell->position_type == 3) continue;
                if (!is_table_cell(*cell)) continue;
                int span = cell->colspan;
                if (span < 1) span = 1;

                float width_hint = -1.0f;
                if (cell->specified_width >= 0) {
                    width_hint = cell->specified_width;
                } else if (cell->css_width.has_value()) {
                    width_hint = cell->css_width->to_px(available_for_cols);
                } else {
                    width_hint = measure_intrinsic_width(*cell, true, intrinsic_measurer);
                }

                if (width_hint > 0 && span == 1 && col_idx < num_cols) {
                    float cur = col_widths[static_cast<size_t>(col_idx)];
                    if (cur < 0 || width_hint > cur) {
                        col_widths[static_cast<size_t>(col_idx)] = width_hint;
                    }
                } else if (width_hint > 0 && span > 1) {
                    float per_col = width_hint / static_cast<float>(span);
                    for (int s = 0; s < span && (col_idx + s) < num_cols; s++) {
                        float cur = col_widths[static_cast<size_t>(col_idx + s)];
                        if (cur < 0 || per_col > cur) {
                            col_widths[static_cast<size_t>(col_idx + s)] = per_col;
                        }
                    }
                }
                col_idx += span;
            }
        }
    }

    // Track which columns have explicit cell widths vs intrinsic (auto) widths.
    // We need this to decide which columns to expand when filling the table width.
    std::vector<bool> col_has_explicit_width(static_cast<size_t>(num_cols), false);
    if (!is_fixed_layout) {
        for (auto* row : rows) {
            int col_idx = 0;
            for (auto& cell : row->children) {
                if (cell->display == DisplayType::None || cell->mode == LayoutMode::None) continue;
                if (cell->position_type == 2 || cell->position_type == 3) continue;
                if (!is_table_cell(*cell)) continue;
                int span = cell->colspan;
                if (span < 1) span = 1;
                if (span == 1 && col_idx < num_cols &&
                    (cell->specified_width >= 0 || cell->css_width.has_value())) {
                    col_has_explicit_width[static_cast<size_t>(col_idx)] = true;
                }
                col_idx += span;
            }
        }
    }

    // Distribute remaining space to auto columns equally
    float used_width = 0;
    int auto_cols = 0;
    for (int i = 0; i < num_cols; i++) {
        if (col_widths[static_cast<size_t>(i)] >= 0) {
            used_width += col_widths[static_cast<size_t>(i)];
        } else {
            auto_cols++;
        }
    }

    float remaining = available_for_cols - used_width;
    if (remaining < 0) remaining = 0;
    float auto_col_width = (auto_cols > 0) ? remaining / static_cast<float>(auto_cols) : 0.0f;

    for (int i = 0; i < num_cols; i++) {
        if (col_widths[static_cast<size_t>(i)] < 0) {
            col_widths[static_cast<size_t>(i)] = auto_col_width;
        }
    }

    // When the table has an explicit width and columns don't fill it,
    // expand columns without explicit widths to fill the available space.
    // Columns with explicit cell widths (specified_width or css_width) keep their size.
    bool table_has_explicit_width = (node.specified_width >= 0.0f) || node.css_width.has_value();
    if (table_has_explicit_width && num_cols > 0) {
        float total = 0;
        for (int i = 0; i < num_cols; i++) total += col_widths[static_cast<size_t>(i)];
        float shortfall = available_for_cols - total;
        if (shortfall > 0.1f) {
            // Count columns eligible for expansion (no explicit cell width)
            float expandable_width = 0;
            int expandable_count = 0;
            for (int i = 0; i < num_cols; i++) {
                if (!col_has_explicit_width[static_cast<size_t>(i)]) {
                    expandable_width += col_widths[static_cast<size_t>(i)];
                    expandable_count++;
                }
            }
            if (expandable_count > 0 && expandable_width > 0) {
                // Proportionally expand non-explicit columns
                float scale = (expandable_width + shortfall) / expandable_width;
                for (int i = 0; i < num_cols; i++) {
                    if (!col_has_explicit_width[static_cast<size_t>(i)]) {
                        col_widths[static_cast<size_t>(i)] *= scale;
                    }
                }
            } else if (expandable_count > 0) {
                // All expandable columns have zero width; distribute equally
                float per_col = shortfall / static_cast<float>(expandable_count);
                for (int i = 0; i < num_cols; i++) {
                    if (!col_has_explicit_width[static_cast<size_t>(i)]) {
                        col_widths[static_cast<size_t>(i)] += per_col;
                    }
                }
            } else {
                // ALL columns have explicit widths — distribute equally to all
                float per_col = shortfall / static_cast<float>(num_cols);
                for (int i = 0; i < num_cols; i++) {
                    col_widths[static_cast<size_t>(i)] += per_col;
                }
            }
        }
    }

    // ---- Build cell occupancy grid for rowspan support ----
    // Grid tracks which (row, col) slots are occupied by a rowspanned cell
    // from a previous row. Value: pointer to the cell that occupies that slot (or nullptr).
    int num_rows_total = static_cast<int>(rows.size());
    // occupancy[row][col] = cell pointer if occupied by a rowspan from a prior row
    std::vector<std::vector<LayoutNode*>> occupancy(
        static_cast<size_t>(num_rows_total),
        std::vector<LayoutNode*>(static_cast<size_t>(num_cols), nullptr));

    // First pass: scan all rows and mark occupancy from rowspan cells
    for (size_t ri = 0; ri < rows.size(); ri++) {
        auto* row = rows[ri];
        int cell_col = 0;
        for (auto& cell_ptr : row->children) {
            if (cell_ptr->display == DisplayType::None || cell_ptr->mode == LayoutMode::None) continue;
            if (cell_ptr->position_type == 2 || cell_ptr->position_type == 3) continue;
            if (!is_table_cell(*cell_ptr)) continue;
            // Skip columns already occupied by rowspan from a prior row
            while (cell_col < num_cols && occupancy[ri][static_cast<size_t>(cell_col)] != nullptr) {
                cell_col++;
            }
            int cspan = cell_ptr->colspan;
            if (cspan < 1) cspan = 1;
            int rspan = cell_ptr->rowspan;
            if (rspan < 1) rspan = 1;
            // Mark occupancy for rowspan > 1
            for (int dr = 1; dr < rspan && (static_cast<int>(ri) + dr) < num_rows_total; dr++) {
                for (int dc = 0; dc < cspan && (cell_col + dc) < num_cols; dc++) {
                    occupancy[static_cast<size_t>(static_cast<int>(ri) + dr)]
                             [static_cast<size_t>(cell_col + dc)] = cell_ptr.get();
                }
            }
            cell_col += cspan;
        }
    }

    // ---- Lay out each row ----
    // Start with vertical edge spacing before first row, plus top caption height
    float cursor_y = node.geometry.padding.top + node.geometry.border.top + caption_top_height + v_spacing;

    // Track row y-positions and heights for rowspan height adjustment
    std::vector<float> row_y_positions(static_cast<size_t>(num_rows_total), 0);
    std::vector<float> row_heights(static_cast<size_t>(num_rows_total), 0);

    // Track cells that have rowspan > 1 for later height adjustment
    struct RowspanInfo {
        LayoutNode* cell;
        int start_row;
        int rspan;
    };
    std::vector<RowspanInfo> rowspan_cells;

    for (size_t ri = 0; ri < rows.size(); ri++) {
        auto* row = rows[ri];

        // visibility: collapse — skip row layout but preserve column widths
        if (row->visibility_collapse) {
            row->geometry.height = 0;
            row->geometry.y = cursor_y;
            row_y_positions[ri] = cursor_y;
            row_heights[ri] = 0;
            for (auto& cell : row->children) {
                cell->geometry.width = 0;
                cell->geometry.height = 0;
            }
            continue;
        }

        // Add vertical spacing before rows (except the first — edge spacing already added)
        if (ri > 0) {
            cursor_y += v_spacing;
        }

        row_y_positions[ri] = cursor_y;

        // Collect cells in this row
        std::vector<LayoutNode*> cells;
        for (auto& cell : row->children) {
            if (cell->display == DisplayType::None || cell->mode == LayoutMode::None) {
                cell->geometry.width = 0;
                cell->geometry.height = 0;
                continue;
            }
            if (cell->position_type == 2 || cell->position_type == 3) continue;
            if (!is_table_cell(*cell)) continue;
            cells.push_back(cell.get());
        }

        // Ensure row and cells inherit line-height from table (parent)
        // This allows table-level line-height to affect text spacing in cells
        row->line_height = node.line_height;
        for (auto* cell : cells) {
            cell->line_height = row->line_height;
        }

        // Position cells in this row — start with horizontal edge spacing
        float cursor_x = node.geometry.padding.left + node.geometry.border.left + h_spacing;
        int cell_col = 0;
        float row_height = 0;

        for (auto* cell : cells) {
            // Skip columns occupied by rowspan from a prior row
            while (cell_col < num_cols && occupancy[ri][static_cast<size_t>(cell_col)] != nullptr) {
                // Advance cursor_x past the occupied column
                cursor_x += col_widths[static_cast<size_t>(cell_col)] + h_spacing;
                cell_col++;
            }

            int span = cell->colspan;
            if (span < 1) span = 1;

            // Track rowspan cells
            if (cell->rowspan > 1) {
                rowspan_cells.push_back({cell, static_cast<int>(ri), cell->rowspan});
            }

            // Compute cell width from column widths
            float cell_w = 0;
            for (int s = 0; s < span && (cell_col + s) < num_cols; s++) {
                cell_w += col_widths[static_cast<size_t>(cell_col + s)];
                if (s > 0) cell_w += h_spacing; // include spacing between spanned columns
            }

            cell->geometry.width = cell_w;
            cell->geometry.x = cursor_x;

            // Layout cell contents
            float cell_content_w = cell_w
                - cell->geometry.padding.left - cell->geometry.padding.right
                - cell->geometry.border.left - cell->geometry.border.right;
            if (cell_content_w < 0) cell_content_w = 0;

            // Layout cell's children
            bool cell_has_inline = false;
            bool cell_has_block = false;
            for (auto& gc : cell->children) {
                if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                if (gc->position_type == 2 || gc->position_type == 3) continue;
                if (gc->mode == LayoutMode::Inline || gc->is_text) {
                    cell_has_inline = true;
                } else {
                    cell_has_block = true;
                }
            }

            float cell_children_height = 0;
            if (cell_has_inline && !cell_has_block) {
                // Layout inline children within the cell
                for (auto& gc : cell->children) {
                    if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                    // Propagate cell's line-height to inline children to ensure correct text spacing
                    if (gc->is_text || gc->display == DisplayType::Inline || gc->display == DisplayType::InlineBlock) {
                        gc->line_height = cell->line_height;
                    }
                    layout_inline(*gc, cell_content_w);
                }
                position_inline_children(*cell, cell_content_w);
                for (auto& gc : cell->children) {
                    if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                    if (gc->position_type == 2 || gc->position_type == 3) continue;
                    float bottom = gc->geometry.y + gc->geometry.margin_box_height();
                    if (bottom > cell_children_height) cell_children_height = bottom;
                }
            } else {
                // Layout block children within the cell
                for (auto& gc : cell->children) {
                    if (gc->display == DisplayType::None || gc->mode == LayoutMode::None) continue;
                    if (gc->position_type == 2 || gc->position_type == 3) continue;
                    // Propagate cell's line-height to children to ensure correct text spacing
                    if (gc->is_text || gc->display == DisplayType::Inline || gc->display == DisplayType::InlineBlock) {
                        gc->line_height = cell->line_height;
                    }
                    switch (gc->mode) {
                        case LayoutMode::Block:
                        case LayoutMode::InlineBlock:
                            layout_block(*gc, cell_content_w);
                            break;
                        case LayoutMode::Inline:
                            layout_inline(*gc, cell_content_w);
                            break;
                        case LayoutMode::Flex:
                            layout_flex(*gc, cell_content_w);
                            break;
                        case LayoutMode::Grid:
                            layout_grid(*gc, cell_content_w);
                            break;
                        case LayoutMode::Table:
                            layout_table(*gc, cell_content_w);
                            break;
                        default:
                            layout_block(*gc, cell_content_w);
                            break;
                    }
                    cell_children_height += gc->geometry.margin_box_height();
                }
            }

            // Compute cell height (for non-rowspan cells, this determines row height)
            float ch = compute_height(*cell);
            if (ch < 0) {
                ch = cell_children_height
                    + cell->geometry.padding.top + cell->geometry.padding.bottom
                    + cell->geometry.border.top + cell->geometry.border.bottom;
            }
            cell->geometry.height = ch;

            // Only count non-rowspan cells toward row height (rowspan cells span multiple rows)
            if (cell->rowspan <= 1) {
                row_height = std::max(row_height, cell->geometry.margin_box_height());
            }

            // Advance cursor_x to next column
            cursor_x += cell_w;
            if (cell_col + span < num_cols) {
                cursor_x += h_spacing;
            }
            cell_col += span;
        }

        // Set row geometry
        row->geometry.x = node.geometry.padding.left + node.geometry.border.left;
        row->geometry.y = cursor_y;
        row->geometry.width = content_w;
        row->geometry.height = row_height;
        row_heights[ri] = row_height;

        // Adjust cell y positions relative to the row, applying vertical-align
        for (auto* cell : cells) {
            if (cell->rowspan > 1) {
                cell->geometry.y = 0; // rowspan cells start at top of their first row
                continue;
            }
            float cell_h = cell->geometry.margin_box_height();
            float va_offset = 0;
            // vertical_align: 1=middle, 2=bottom in table-cell layout path
            switch (cell->vertical_align) {
                case 1: // middle
                    va_offset = (row_height - cell_h) / 2.0f;
                    break;
                case 2: // bottom
                case 3: // bottom (compatibility with existing enum mapping)
                    va_offset = row_height - cell_h;
                    break;
                default: // top/baseline/text-top — treat as top for table cells
                    va_offset = 0;
                    break;
            }
            if (va_offset < 0) va_offset = 0;
            cell->geometry.y = va_offset;
        }

        cursor_y += row_height;
    }

    // ---- Rowspan height adjustment ----
    // For cells with rowspan > 1, compute total spanned height and extend cell height
    for (auto& rsi : rowspan_cells) {
        int end_row = std::min(rsi.start_row + rsi.rspan, num_rows_total);
        float total_spanned_h = 0;
        for (int r = rsi.start_row; r < end_row; r++) {
            total_spanned_h += row_heights[static_cast<size_t>(r)];
            if (r > rsi.start_row) total_spanned_h += v_spacing;
        }
        // Extend cell height to cover spanned rows
        if (total_spanned_h > rsi.cell->geometry.height) {
            rsi.cell->geometry.height = total_spanned_h;
        }
    }

    // Add vertical edge spacing after last row
    cursor_y += v_spacing;

    // ---- Layout bottom captions ----
    for (auto* cap : captions) {
        if (cap->caption_side == 1) { // bottom
            cap->geometry.y = cursor_y + caption_bottom_height;
            caption_bottom_height += cap->geometry.margin_box_height();
        }
    }
    cursor_y += caption_bottom_height;

    // Compute table height
    float total_content_height = cursor_y
        - (node.geometry.padding.top + node.geometry.border.top);
    float total_height = total_content_height
        + node.geometry.padding.top + node.geometry.padding.bottom
        + node.geometry.border.top + node.geometry.border.bottom;

    float h = compute_height(node);
    if (h >= 0) {
        node.geometry.height = h;
    } else {
        node.geometry.height = total_height;
    }
    node.geometry.height = std::max(node.geometry.height, node.min_height);
    node.geometry.height = std::min(node.geometry.height, node.max_height);

    // Position absolute/fixed children
    position_absolute_children(node);
}

} // namespace clever::layout
