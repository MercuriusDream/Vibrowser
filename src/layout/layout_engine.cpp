#include "browser/layout/layout_engine.h"

#include "browser/css/css_parser.h"
#include "browser/html/dom.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace browser::layout {
namespace {

std::string trim_copy(const std::string& input) {
  std::size_t first = 0;
  while (first < input.size() &&
         std::isspace(static_cast<unsigned char>(input[first])) != 0) {
    ++first;
  }

  if (first == input.size()) {
    return {};
  }

  std::size_t last = input.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(input[last - 1])) != 0) {
    --last;
  }

  return input.substr(first, last - first);
}

std::string lower_copy(const std::string& input) {
  std::string out = input;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

bool ends_with(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

int parse_css_px(const std::string& raw, int fallback) {
  std::string value = lower_copy(trim_copy(raw));
  if (value.empty()) {
    return fallback;
  }

  if (ends_with(value, "px")) {
    value = trim_copy(value.substr(0, value.size() - 2));
  }

  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size()) {
      return fallback;
    }
    return static_cast<int>(std::round(parsed));
  } catch (...) {
    return fallback;
  }
}

std::vector<int> parse_length_list(const std::string& raw) {
  std::vector<int> out;
  std::istringstream stream(raw);
  std::string token;
  while (stream >> token) {
    out.push_back(parse_css_px(token, 0));
  }
  return out;
}

std::string style_value(const std::map<std::string, std::string>& style,
                        const std::string& key,
                        const std::string& fallback = {}) {
  const auto it = style.find(lower_copy(key));
  if (it == style.end()) {
    return fallback;
  }
  return it->second;
}

std::string apply_text_transform(const std::string& text,
                                 const std::map<std::string, std::string>& style) {
  const std::string transform =
      lower_copy(trim_copy(style_value(style, "text-transform")));
  if (transform == "uppercase") {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
      return static_cast<char>(std::toupper(ch));
    });
    return out;
  }
  if (transform == "lowercase") {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    return out;
  }
  if (transform == "capitalize") {
    std::string out = text;
    bool at_word_start = true;
    for (char& ch : out) {
      const unsigned char uch = static_cast<unsigned char>(ch);
      if (std::isalnum(uch) != 0) {
        if (at_word_start && std::isalpha(uch) != 0) {
          ch = static_cast<char>(std::toupper(uch));
        }
        at_word_start = false;
      } else {
        at_word_start = true;
      }
    }
    return out;
  }
  return text;
}

bool is_text_node(const browser::html::Node& node) {
  return node.type == browser::html::NodeType::Text;
}

std::string node_tag_name(const browser::html::Node& node) {
  return lower_copy(trim_copy(node.tag_name));
}

std::string node_text_value(const browser::html::Node& node) {
  return node.text_content;
}

struct InternalLayoutNode {
  bool is_text = false;
  std::string tag;
  std::string text;
  std::map<std::string, std::string> style;
  std::vector<InternalLayoutNode> children;
};

InternalLayoutNode build_layout_tree(const browser::html::Node& dom_node,
                                     const browser::css::Stylesheet& sheet) {
  InternalLayoutNode node;
  node.is_text = is_text_node(dom_node);
  node.tag = node.is_text ? "#text" : node_tag_name(dom_node);
  if (node.tag.empty()) {
    node.tag = node.is_text ? "#text" : "div";
  }

  if (node.is_text) {
    node.text = node_text_value(dom_node);
  }

  node.style = browser::css::compute_style_for_node(dom_node, sheet);

  for (const auto& child : dom_node.children) {
    if (!child) {
      continue;
    }

    InternalLayoutNode child_node = build_layout_tree(*child, sheet);
    if (style_value(child_node.style, "display") == "none") {
      continue;
    }
    if (child_node.is_text && trim_copy(child_node.text).empty()) {
      continue;
    }
    node.children.push_back(std::move(child_node));
  }

  return node;
}

struct BoxEdges {
  int top = 0;
  int right = 0;
  int bottom = 0;
  int left = 0;
};

BoxEdges edges_from_style(const std::map<std::string, std::string>& style,
                          const std::string& key) {
  BoxEdges edges;
  const std::vector<int> shorthand = parse_length_list(style_value(style, key));

  if (shorthand.size() == 1) {
    edges.top = shorthand[0];
    edges.right = shorthand[0];
    edges.bottom = shorthand[0];
    edges.left = shorthand[0];
  } else if (shorthand.size() == 2) {
    edges.top = shorthand[0];
    edges.bottom = shorthand[0];
    edges.left = shorthand[1];
    edges.right = shorthand[1];
  } else if (shorthand.size() == 3) {
    edges.top = shorthand[0];
    edges.left = shorthand[1];
    edges.right = shorthand[1];
    edges.bottom = shorthand[2];
  } else if (shorthand.size() >= 4) {
    edges.top = shorthand[0];
    edges.right = shorthand[1];
    edges.bottom = shorthand[2];
    edges.left = shorthand[3];
  }

  edges.top = parse_css_px(style_value(style, key + "-top"), edges.top);
  edges.right = parse_css_px(style_value(style, key + "-right"), edges.right);
  edges.bottom = parse_css_px(style_value(style, key + "-bottom"), edges.bottom);
  edges.left = parse_css_px(style_value(style, key + "-left"), edges.left);

  return edges;
}

struct TextMetrics {
  int char_width = 8;
  int line_height = 18;
  int max_chars = 1;
};

TextMetrics compute_text_metrics(const std::map<std::string, std::string>& style,
                                 int content_width) {
  const int font_size = std::max(1, parse_css_px(style_value(style, "font-size"), 16));
  const int line_height = std::max(
      1, parse_css_px(style_value(style, "line-height"),
                      static_cast<int>(std::round(static_cast<double>(font_size) * 1.2))));
  const int char_width = std::max(1, font_size / 2);
  const int max_chars = std::max(1, content_width / char_width);

  TextMetrics metrics;
  metrics.char_width = char_width;
  metrics.line_height = line_height;
  metrics.max_chars = max_chars;
  return metrics;
}

std::vector<std::string> split_paragraphs(const std::string& text) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : text) {
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  parts.push_back(current);
  return parts;
}

std::vector<std::string> wrap_paragraph(const std::string& paragraph,
                                        int max_chars) {
  std::vector<std::string> lines;
  std::istringstream words(paragraph);
  std::string word;
  std::string current;

  while (words >> word) {
    while (static_cast<int>(word.size()) > max_chars) {
      if (!current.empty()) {
        lines.push_back(current);
        current.clear();
      }
      lines.push_back(word.substr(0, static_cast<std::size_t>(max_chars)));
      word.erase(0, static_cast<std::size_t>(max_chars));
    }

    if (current.empty()) {
      current = word;
    } else if (static_cast<int>(current.size() + 1 + word.size()) <= max_chars) {
      current.append(" ").append(word);
    } else {
      lines.push_back(current);
      current = word;
    }
  }

  if (!current.empty()) {
    lines.push_back(current);
  }

  return lines;
}

std::vector<std::string> wrap_text_lines(const std::string& text, int max_chars) {
  std::vector<std::string> lines;
  if (max_chars <= 0) {
    return lines;
  }

  if (trim_copy(text).empty()) {
    return lines;
  }

  const std::vector<std::string> paragraphs = split_paragraphs(text);
  for (const std::string& paragraph : paragraphs) {
    if (trim_copy(paragraph).empty()) {
      lines.push_back("");
      continue;
    }

    std::vector<std::string> wrapped = wrap_paragraph(paragraph, max_chars);
    lines.insert(lines.end(), wrapped.begin(), wrapped.end());
  }

  return lines;
}

LayoutBox layout_node_box(const InternalLayoutNode& node,
                          int x,
                          int y,
                          int width,
                          int viewport_width);

LayoutBox layout_text_box(const InternalLayoutNode& node,
                          int x,
                          int y,
                          int width) {
  const std::string transformed_text = apply_text_transform(node.text, node.style);

  LayoutBox box;
  box.x = x;
  box.y = y;
  box.width = std::max(0, width);
  box.tag = node.tag;
  box.text = transformed_text;
  box.style = node.style;

  const BoxEdges padding = edges_from_style(node.style, "padding");
  const int content_x = box.x + padding.left;
  const int content_y = box.y + padding.top;
  const int content_width = std::max(0, box.width - padding.left - padding.right);

  const TextMetrics metrics = compute_text_metrics(node.style, std::max(1, content_width));
  const std::vector<std::string> lines =
      wrap_text_lines(transformed_text, metrics.max_chars);
  const std::string text_align = lower_copy(trim_copy(style_value(node.style, "text-align")));

  int cursor_y = content_y;
  for (const std::string& line : lines) {
    LayoutBox line_box;
    line_box.y = cursor_y;
    line_box.width =
        std::min(content_width, static_cast<int>(line.size()) * metrics.char_width);
    int line_offset_x = 0;
    if (text_align == "center") {
      line_offset_x = std::max(0, (content_width - line_box.width) / 2);
    } else if (text_align == "right" || text_align == "end") {
      line_offset_x = std::max(0, content_width - line_box.width);
    } else if (text_align == "left" || text_align == "start" ||
               text_align == "justify") {
      // Full justify is not implemented yet; keep justify aligned like left.
      line_offset_x = 0;
    }
    line_box.x = content_x + line_offset_x;
    line_box.height = metrics.line_height;
    line_box.tag = "#line";
    line_box.text = line;
    line_box.style = node.style;
    box.children.push_back(std::move(line_box));
    cursor_y += metrics.line_height;
  }

  int content_height = static_cast<int>(lines.size()) * metrics.line_height;
  content_height = parse_css_px(style_value(node.style, "height"), content_height);
  const int min_height = parse_css_px(style_value(node.style, "min-height"), 0);
  content_height = std::max(content_height, min_height);
  const int max_height = parse_css_px(style_value(node.style, "max-height"), -1);
  if (max_height >= 0) {
    content_height = std::min(content_height, max_height);
  }

  box.height = std::max(0, padding.top + content_height + padding.bottom);
  const int forced_width = parse_css_px(style_value(node.style, "width"), -1);
  if (forced_width >= 0) {
    box.width = forced_width;
  }

  return box;
}

LayoutBox layout_block_box(const InternalLayoutNode& node,
                           int x,
                           int y,
                           int width,
                           int viewport_width) {
  LayoutBox box;
  box.x = x;
  box.y = y;
  box.width = std::max(0, width);
  box.tag = node.tag;
  box.style = node.style;

  const BoxEdges padding = edges_from_style(node.style, "padding");
  const int content_x = box.x + padding.left;
  const int content_y = box.y + padding.top;
  const int content_width =
      std::max(0, box.width - padding.left - padding.right);

  int cursor_y = content_y;
  for (const InternalLayoutNode& child : node.children) {
    const BoxEdges margin = edges_from_style(child.style, "margin");
    const int child_x = content_x + margin.left;
    const int child_y = cursor_y + margin.top;
    const int child_width =
        std::max(0, content_width - margin.left - margin.right);

    LayoutBox child_box =
        layout_node_box(child, child_x, child_y, child_width, viewport_width);
    cursor_y = child_y + child_box.height + margin.bottom;
    box.children.push_back(std::move(child_box));
  }

  int content_height = std::max(0, cursor_y - content_y);
  content_height = parse_css_px(style_value(node.style, "height"), content_height);
  const int min_height = parse_css_px(style_value(node.style, "min-height"), -1);
  if (min_height >= 0) {
    content_height = std::max(content_height, min_height);
  }
  const int max_height = parse_css_px(style_value(node.style, "max-height"), -1);
  if (max_height >= 0) {
    content_height = std::min(content_height, max_height);
  }

  box.height = std::max(0, padding.top + content_height + padding.bottom);
  const int forced_width = parse_css_px(style_value(node.style, "width"), -1);
  if (forced_width >= 0) {
    box.width = forced_width;
  }
  const int max_width = parse_css_px(style_value(node.style, "max-width"), -1);
  if (max_width >= 0) {
    box.width = std::min(box.width, max_width);
  }
  const int min_width = parse_css_px(style_value(node.style, "min-width"), -1);
  if (min_width >= 0) {
    box.width = std::max(box.width, min_width);
  }

  (void)viewport_width;
  return box;
}

LayoutBox layout_node_box(const InternalLayoutNode& node,
                          int x,
                          int y,
                          int width,
                          int viewport_width) {
  if (style_value(node.style, "display") == "none") {
    LayoutBox hidden;
    hidden.x = x;
    hidden.y = y;
    hidden.width = 0;
    hidden.height = 0;
    hidden.tag = node.tag;
    hidden.style = node.style;
    return hidden;
  }

  if (node.is_text) {
    return layout_text_box(node, x, y, width);
  }
  return layout_block_box(node, x, y, width, viewport_width);
}

}  // namespace

LayoutBox layout_document(const browser::html::Node& root,
                          const browser::css::Stylesheet& sheet,
                          int viewport_width) {
  const int safe_width = std::max(0, viewport_width);
  InternalLayoutNode layout_root = build_layout_tree(root, sheet);
  LayoutBox root_box = layout_node_box(layout_root, 0, 0, safe_width, safe_width);
  root_box.x = 0;
  root_box.y = 0;
  root_box.width = safe_width;
  return root_box;
}

std::string serialize_layout(const LayoutBox& box) {
    std::string out;
    out += "{";
    if (!box.tag.empty()) {
        out += "tag:" + box.tag;
    } else if (!box.text.empty()) {
        out += "text:\"" + box.text + "\"";
    }
    out += " x:" + std::to_string(box.x);
    out += " y:" + std::to_string(box.y);
    out += " w:" + std::to_string(box.width);
    out += " h:" + std::to_string(box.height);
    for (const auto& child : box.children) {
        out += serialize_layout(child);
    }
    out += "}";
    return out;
}

}  // namespace browser::layout
