#pragma once

#include <map>
#include <string>
#include <vector>

namespace browser::html {
struct Node;
}

namespace browser::css {
struct Stylesheet;
}

namespace browser::layout {

struct LayoutBox {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::string tag;
  std::string text;
  std::map<std::string, std::string> style;
  std::vector<LayoutBox> children;
};

LayoutBox layout_document(const browser::html::Node& root,
                          const browser::css::Stylesheet& sheet,
                          int viewport_width);

std::string serialize_layout(const LayoutBox& box);

}  // namespace browser::layout
