#pragma once

#include <map>
#include <string>
#include <vector>

#include "browser/html/dom.h"

namespace browser::css {

struct Declaration {
  std::string property;
  std::string value;
};

struct Rule {
  std::string selector;
  std::vector<Declaration> declarations;
  int specificity = 0;
};

struct Stylesheet {
  std::vector<Rule> rules;
};

struct StyleWarning {
    std::string message;
    std::string selector;
};

struct ParseCssResult {
    Stylesheet stylesheet;
    std::vector<StyleWarning> warnings;
};

Stylesheet parse_css(const std::string& css);
ParseCssResult parse_css_with_diagnostics(const std::string& css);

std::map<std::string, std::string> parse_inline_style(const std::string& s);

std::map<std::string, std::string> compute_style_for_node(
    const browser::html::Node& node,
    const Stylesheet& stylesheet);

std::map<std::string, std::string> compute_style_for_node(
    const browser::html::Node& node,
    const Stylesheet& stylesheet,
    std::vector<StyleWarning>& warnings);

struct LinkedCssRef {
    std::string href;
    std::string tag;
};

std::vector<LinkedCssRef> extract_linked_css(const browser::html::Node& document);

struct LinkedCssLoadResult {
    Stylesheet merged;
    std::vector<std::string> warnings;
    std::vector<std::string> loaded_urls;
    std::vector<std::string> failed_urls;
};

LinkedCssLoadResult load_linked_css(const browser::html::Node& document,
                                     const std::string& inline_css);

}  // namespace browser::css
