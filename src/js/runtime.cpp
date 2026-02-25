#include "browser/js/runtime.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "browser/css/css_parser.h"

namespace browser::js {
namespace {

std::string trim_copy(std::string_view input) {
  std::size_t first = 0;
  while (first < input.size() &&
         std::isspace(static_cast<unsigned char>(input[first])) != 0) {
    ++first;
  }

  std::size_t last = input.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(input[last - 1])) != 0) {
    --last;
  }

  return std::string(input.substr(first, last - first));
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

bool is_identifier_start_char(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalpha(uch) != 0 || ch == '_';
}

bool is_identifier_continue_char(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) != 0 || ch == '_';
}

void skip_whitespace(std::string_view source, std::size_t& pos) {
  while (pos < source.size() &&
         std::isspace(static_cast<unsigned char>(source[pos])) != 0) {
    ++pos;
  }
}

browser::html::Node* find_first_element_by_tag(browser::html::Node& node,
                                                const std::string& tag_name) {
  if (node.type == browser::html::NodeType::Element && node.tag_name == tag_name) {
    return &node;
  }

  for (const std::unique_ptr<browser::html::Node>& child : node.children) {
    if (!child) {
      continue;
    }
    browser::html::Node* found = find_first_element_by_tag(*child, tag_name);
    if (found != nullptr) {
      return found;
    }
  }

  return nullptr;
}

browser::html::Node* find_first_element_by_id(browser::html::Node& node,
                                               const std::string& id_value) {
  if (node.type == browser::html::NodeType::Element) {
    auto id_it = node.attributes.find("id");
    if (id_it != node.attributes.end() && id_it->second == id_value) {
      return &node;
    }
  }

  for (const std::unique_ptr<browser::html::Node>& child : node.children) {
    if (!child) {
      continue;
    }
    browser::html::Node* found = find_first_element_by_id(*child, id_value);
    if (found != nullptr) {
      return found;
    }
  }

  return nullptr;
}

void set_element_text(browser::html::Node& element, const std::string& text) {
  element.children.clear();
  auto text_node = std::make_unique<browser::html::Node>(browser::html::NodeType::Text);
  text_node->parent = &element;
  text_node->text_content = text;
  element.children.push_back(std::move(text_node));
}

browser::html::Node* ensure_head_element(browser::html::Node& document) {
  browser::html::Node* head = find_first_element_by_tag(document, "head");
  if (head != nullptr) {
    return head;
  }

  browser::html::Node* container = find_first_element_by_tag(document, "html");
  if (container == nullptr) {
    container = &document;
  }

  auto head_node = std::make_unique<browser::html::Node>(browser::html::NodeType::Element, "head");
  head_node->parent = container;
  browser::html::Node* head_ptr = head_node.get();
  container->children.push_back(std::move(head_node));
  return head_ptr;
}

void set_document_title(browser::html::Node& document, const std::string& title) {
  browser::html::Node* title_node = find_first_element_by_tag(document, "title");
  if (title_node == nullptr) {
    browser::html::Node* head = ensure_head_element(document);
    auto new_title = std::make_unique<browser::html::Node>(browser::html::NodeType::Element, "title");
    new_title->parent = head;
    title_node = new_title.get();
    head->children.push_back(std::move(new_title));
  }

  set_element_text(*title_node, title);
}

std::string serialize_inline_style(const std::map<std::string, std::string>& style_map) {
  std::string serialized;
  bool first = true;

  for (const auto& [property, value] : style_map) {
    if (!first) {
      serialized.push_back(' ');
    }
    serialized += property;
    serialized += ": ";
    serialized += value;
    serialized.push_back(';');
    first = false;
  }

  return serialized;
}

bool map_supported_style_property(const std::string& property, std::string& css_property) {
  if (property == "background") {
    css_property = "background";
    return true;
  }
  if (property == "backgroundColor") {
    css_property = "background-color";
    return true;
  }
  if (property == "border") {
    css_property = "border";
    return true;
  }
  if (property == "borderColor") {
    css_property = "border-color";
    return true;
  }
  if (property == "borderWidth") {
    css_property = "border-width";
    return true;
  }
  if (property == "borderStyle") {
    css_property = "border-style";
    return true;
  }
  if (property == "color") {
    css_property = "color";
    return true;
  }
  return false;
}

bool normalize_identifier_style_property(const std::string& property,
                                         std::string& css_property) {
  if (property.empty() || !is_identifier_start_char(property.front())) {
    return false;
  }

  std::string normalized;
  normalized.reserve(property.size() + 4);

  for (const char ch : property) {
    if (!is_identifier_continue_char(ch)) {
      return false;
    }

    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isupper(uch) != 0) {
      if (!normalized.empty()) {
        normalized.push_back('-');
      }
      normalized.push_back(static_cast<char>(std::tolower(uch)));
      continue;
    }

    normalized.push_back(ch);
  }

  css_property = std::move(normalized);
  return true;
}

bool normalize_supported_style_property(const std::string& property,
                                        std::string& css_property) {
  if (map_supported_style_property(property, css_property)) {
    // Keep getElementById/querySelector/body style aliases normalized identically.
    if (property == "background") {
      return css_property == "background";
    }
    if (property == "backgroundColor") {
      return css_property == "background-color";
    }
    if (property == "border") {
      return css_property == "border";
    }
    if (property == "borderColor") {
      return css_property == "border-color";
    }
    if (property == "borderWidth") {
      return css_property == "border-width";
    }
    if (property == "borderStyle") {
      return css_property == "border-style";
    }
    return true;
  }

  return normalize_identifier_style_property(property, css_property);
}

void set_inline_style_property(browser::html::Node& element,
                               const std::string& property,
                               const std::string& value) {
  std::map<std::string, std::string> style_map;
  auto current_style_it = element.attributes.find("style");
  if (current_style_it != element.attributes.end()) {
    style_map = browser::css::parse_inline_style(current_style_it->second);
  }

  style_map[property] = value;
  element.attributes["style"] = serialize_inline_style(style_map);
}

void set_inline_style_string(browser::html::Node& element, const std::string& style_text) {
  const std::map<std::string, std::string> style_map = browser::css::parse_inline_style(style_text);
  element.attributes["style"] = serialize_inline_style(style_map);
}

void set_element_attribute(browser::html::Node& element,
                           std::string_view attribute_name,
                           const std::string& attribute_value) {
  if (attribute_name == "style") {
    set_inline_style_string(element, attribute_value);
    return;
  }

  // Keep .id and setAttribute("id", ...) updates identical across selector paths.
  if (attribute_name == "id") {
    element.attributes["id"] = attribute_value;
    return;
  }

  element.attributes[std::string(attribute_name)] = attribute_value;
}

bool parse_string_literal(std::string_view source,
                          std::size_t& pos,
                          std::string& out_value,
                          std::string& err) {
  skip_whitespace(source, pos);
  if (pos >= source.size()) {
    err = "Expected string literal";
    return false;
  }

  const char quote = source[pos];
  if (quote != '"' && quote != '\'') {
    err = "Expected quoted string literal";
    return false;
  }
  ++pos;

  std::string value;
  while (pos < source.size()) {
    const char ch = source[pos];
    if (ch == '\\') {
      if (pos + 1 >= source.size()) {
        err = "Invalid escape sequence";
        return false;
      }
      value.push_back(source[pos + 1]);
      pos += 2;
      continue;
    }
    if (ch == quote) {
      ++pos;
      out_value = std::move(value);
      return true;
    }
    value.push_back(ch);
    ++pos;
  }

  err = "Unterminated string literal";
  return false;
}

bool parse_member_identifier(std::string_view source,
                             std::size_t& pos,
                             std::string& identifier) {
  if (pos >= source.size() || !is_identifier_start_char(source[pos])) {
    identifier.clear();
    return false;
  }

  const std::size_t identifier_start = pos;
  ++pos;
  while (pos < source.size() && is_identifier_continue_char(source[pos])) {
    ++pos;
  }

  identifier = std::string(source.substr(identifier_start, pos - identifier_start));
  return true;
}

bool parse_string_assignment(std::string_view statement,
                             std::size_t& pos,
                             std::string_view assignment_name,
                             std::string& value,
                             std::string& err) {
  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '=') {
    err = std::string(assignment_name) + " is missing '='";
    return false;
  }
  ++pos;

  if (!parse_string_literal(statement, pos, value, err)) {
    return false;
  }

  skip_whitespace(statement, pos);
  if (pos != statement.size()) {
    err = "Unexpected trailing characters in " + std::string(assignment_name);
    return false;
  }

  return true;
}

bool has_assignment_operator(std::string_view statement, std::size_t pos) {
  skip_whitespace(statement, pos);
  return pos < statement.size() && statement[pos] == '=';
}

bool parse_set_attribute_call(std::string_view statement,
                              std::size_t& pos,
                              std::string_view operation_name,
                              std::string& attribute_name,
                              std::string& attribute_value,
                              std::string& err) {
  constexpr std::string_view kSetAttributePrefix = ".setAttribute";
  if (statement.size() - pos < kSetAttributePrefix.size() ||
      statement.compare(pos, kSetAttributePrefix.size(), kSetAttributePrefix) != 0) {
    return false;
  }
  pos += kSetAttributePrefix.size();

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '(') {
    err = std::string(operation_name) + ".setAttribute call is missing '('";
    return true;
  }
  ++pos;

  if (!parse_string_literal(statement, pos, attribute_name, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ',') {
    err = std::string(operation_name) + ".setAttribute call is missing ','";
    return true;
  }
  ++pos;

  if (!parse_string_literal(statement, pos, attribute_value, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ')') {
    err = std::string(operation_name) + ".setAttribute call is missing ')'";
    return true;
  }
  ++pos;

  skip_whitespace(statement, pos);
  if (pos != statement.size()) {
    err = "Unexpected trailing characters in " + std::string(operation_name) + ".setAttribute call";
    return true;
  }

  return true;
}

bool parse_remove_attribute_call(std::string_view statement,
                                 std::size_t& pos,
                                 std::string_view operation_name,
                                 std::string& attribute_name,
                                 std::string& err) {
  constexpr std::string_view kRemoveAttributePrefix = ".removeAttribute";
  if (statement.size() - pos < kRemoveAttributePrefix.size() ||
      statement.compare(pos, kRemoveAttributePrefix.size(), kRemoveAttributePrefix) != 0) {
    return false;
  }
  pos += kRemoveAttributePrefix.size();

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '(') {
    err = std::string(operation_name) + ".removeAttribute call is missing '('";
    return true;
  }
  ++pos;

  if (!parse_string_literal(statement, pos, attribute_name, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ')') {
    err = std::string(operation_name) + ".removeAttribute call is missing ')'";
    return true;
  }
  ++pos;

  skip_whitespace(statement, pos);
  if (pos != statement.size()) {
    err = "Unexpected trailing characters in " + std::string(operation_name) +
          ".removeAttribute call";
    return true;
  }

  return true;
}

std::string_view parse_supported_text_property(std::string_view statement,
                                               std::size_t& pos) {
  constexpr std::string_view kInnerTextProperty = ".innerText";
  constexpr std::string_view kTextContentProperty = ".textContent";
  if (statement.size() - pos >= kInnerTextProperty.size() &&
      statement.compare(pos, kInnerTextProperty.size(), kInnerTextProperty) == 0) {
    pos += kInnerTextProperty.size();
    return "innerText";
  }
  if (statement.size() - pos >= kTextContentProperty.size() &&
      statement.compare(pos, kTextContentProperty.size(), kTextContentProperty) == 0) {
    pos += kTextContentProperty.size();
    return "textContent";
  }
  return {};
}

bool parse_document_title_statement(std::string_view statement,
                                    browser::html::Node& document,
                                    std::string& err) {
  constexpr std::string_view kPrefix = "document.title";
  if (!starts_with(statement, kPrefix)) {
    return false;
  }

  std::size_t pos = kPrefix.size();
  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '=') {
    err = "document.title assignment is missing '='";
    return true;
  }
  ++pos;

  std::string value;
  if (!parse_string_literal(statement, pos, value, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos != statement.size()) {
    err = "Unexpected trailing characters in document.title assignment";
    return true;
  }

  set_document_title(document, value);
  return true;
}

bool parse_document_body_statement(std::string_view statement,
                                   browser::html::Node& document,
                                   std::string& err) {
  constexpr std::string_view kPrefix = "document.body";
  if (!starts_with(statement, kPrefix)) {
    return false;
  }

  std::size_t pos = kPrefix.size();
  skip_whitespace(statement, pos);

  browser::html::Node* body = find_first_element_by_tag(document, "body");
  if (body == nullptr) {
    err = "document.body is not available";
    return true;
  }

  constexpr std::string_view kStylePropertyPrefix = ".style.";
  if (statement.size() - pos >= kStylePropertyPrefix.size() &&
      statement.compare(pos, kStylePropertyPrefix.size(), kStylePropertyPrefix) == 0) {
    pos += kStylePropertyPrefix.size();

    std::string property;
    if (!parse_member_identifier(statement, pos, property)) {
      err = "document.body.style assignment is missing property name";
      return true;
    }

    std::string value;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.body.style assignment",
                                 value,
                                 err)) {
      return true;
    }

    std::string css_property;
    if (!normalize_supported_style_property(property, css_property)) {
      err = "Unsupported document.body.style property: " + property;
      return true;
    }

    set_inline_style_property(*body, css_property, value);
    return true;
  }

  constexpr std::string_view kStylePrefix = ".style";
  if (statement.size() - pos >= kStylePrefix.size() &&
      statement.compare(pos, kStylePrefix.size(), kStylePrefix) == 0 &&
      has_assignment_operator(statement, pos + kStylePrefix.size())) {
    pos += kStylePrefix.size();

    std::string style_text;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.body.style assignment",
                                 style_text,
                                 err)) {
      return true;
    }

    set_inline_style_string(*body, style_text);
    return true;
  }

  constexpr std::string_view kClassNamePrefix = ".className";
  if (statement.size() - pos >= kClassNamePrefix.size() &&
      statement.compare(pos, kClassNamePrefix.size(), kClassNamePrefix) == 0 &&
      has_assignment_operator(statement, pos + kClassNamePrefix.size())) {
    pos += kClassNamePrefix.size();

    std::string class_name;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.body.className assignment",
                                 class_name,
                                 err)) {
      return true;
    }

    body->attributes["class"] = class_name;
    return true;
  }

  constexpr std::string_view kIdPrefix = ".id";
  if (statement.size() - pos >= kIdPrefix.size() &&
      statement.compare(pos, kIdPrefix.size(), kIdPrefix) == 0 &&
      has_assignment_operator(statement, pos + kIdPrefix.size())) {
    pos += kIdPrefix.size();

    std::string assigned_id;
    if (!parse_string_assignment(statement, pos, "document.body.id assignment", assigned_id, err)) {
      return true;
    }

    set_element_attribute(*body, "id", assigned_id);
    return true;
  }

  std::string attribute_name;
  std::string attribute_value;
  if (parse_set_attribute_call(
          statement, pos, "document.body", attribute_name, attribute_value, err)) {
    if (!err.empty()) {
      return true;
    }

    set_element_attribute(*body, attribute_name, attribute_value);
    return true;
  }

  if (parse_remove_attribute_call(statement, pos, "document.body", attribute_name, err)) {
    if (!err.empty()) {
      return true;
    }

    if (attribute_name == "style") {
      body->attributes.erase("style");
    } else {
      body->attributes.erase(attribute_name);
    }
    return true;
  }

  if (statement.size() - pos >= kStylePrefix.size() &&
      statement.compare(pos, kStylePrefix.size(), kStylePrefix) == 0) {
    err = "Unsupported document.body.style operation";
    return true;
  }

  err = "Unsupported document.body operation";
  return true;

}

bool parse_get_element_by_id_statement(std::string_view statement,
                                       browser::html::Node& document,
                                       std::string& err) {
  constexpr std::string_view kPrefix = "document.getElementById";
  if (!starts_with(statement, kPrefix)) {
    return false;
  }

  std::size_t pos = kPrefix.size();
  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '(') {
    err = "document.getElementById call is missing '('";
    return true;
  }
  ++pos;

  std::string element_id;
  if (!parse_string_literal(statement, pos, element_id, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ')') {
    err = "document.getElementById call is missing ')'";
    return true;
  }
  ++pos;

  skip_whitespace(statement, pos);

  constexpr std::string_view kStylePropertyPrefix = ".style.";
  if (statement.size() - pos >= kStylePropertyPrefix.size() &&
      statement.compare(pos, kStylePropertyPrefix.size(), kStylePropertyPrefix) == 0) {
    pos += kStylePropertyPrefix.size();

    std::string property;
    if (!parse_member_identifier(statement, pos, property)) {
      err = "document.getElementById(...).style assignment is missing property name";
      return true;
    }

    std::string value;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.getElementById(...).style assignment",
                                 value,
                                 err)) {
      return true;
    }

    std::string css_property;
    if (!normalize_supported_style_property(property, css_property)) {
      err = "Unsupported document.getElementById(...).style property: " + property;
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    set_inline_style_property(*element, css_property, value);
    return true;
  }

  constexpr std::string_view kStylePrefix = ".style";
  if (statement.size() - pos >= kStylePrefix.size() &&
      statement.compare(pos, kStylePrefix.size(), kStylePrefix) == 0 &&
      has_assignment_operator(statement, pos + kStylePrefix.size())) {
    pos += kStylePrefix.size();

    std::string style_text;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.getElementById(...).style assignment",
                                 style_text,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    set_inline_style_string(*element, style_text);
    return true;
  }

  constexpr std::string_view kClassNamePrefix = ".className";
  if (statement.size() - pos >= kClassNamePrefix.size() &&
      statement.compare(pos, kClassNamePrefix.size(), kClassNamePrefix) == 0 &&
      has_assignment_operator(statement, pos + kClassNamePrefix.size())) {
    pos += kClassNamePrefix.size();

    std::string class_name;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.getElementById(...).className assignment",
                                 class_name,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    element->attributes["class"] = class_name;
    return true;
  }

  constexpr std::string_view kIdPrefix = ".id";
  if (statement.size() - pos >= kIdPrefix.size() &&
      statement.compare(pos, kIdPrefix.size(), kIdPrefix) == 0 &&
      has_assignment_operator(statement, pos + kIdPrefix.size())) {
    pos += kIdPrefix.size();

    std::string assigned_id;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.getElementById(...).id assignment",
                                 assigned_id,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    set_element_attribute(*element, "id", assigned_id);
    return true;
  }

  std::string attribute_name;
  std::string attribute_value;
  if (parse_set_attribute_call(
          statement, pos, "document.getElementById(...)", attribute_name, attribute_value, err)) {
    if (!err.empty()) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    set_element_attribute(*element, attribute_name, attribute_value);
    return true;
  }

  if (parse_remove_attribute_call(
          statement, pos, "document.getElementById(...)", attribute_name, err)) {
    if (!err.empty()) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    if (attribute_name == "style") {
      element->attributes.erase("style");
    } else {
      element->attributes.erase(attribute_name);
    }
    return true;
  }

  const std::string_view text_property_name = parse_supported_text_property(statement, pos);

  if (!text_property_name.empty()) {
    const std::string text_operation =
        "document.getElementById(...)." + std::string(text_property_name) + " assignment";

    std::string value;
    if (!parse_string_assignment(statement, pos, text_operation, value, err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.getElementById could not find element: " + element_id;
      return true;
    }

    set_element_text(*element, value);
    return true;
  }

  err = "Unsupported document.getElementById operation";
  return true;
}

bool parse_query_selector_statement(std::string_view statement,
                                    browser::html::Node& document,
                                    std::string& err) {
  constexpr std::string_view kPrefix = "document.querySelector";
  if (!starts_with(statement, kPrefix)) {
    return false;
  }

  std::size_t pos = kPrefix.size();
  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '(') {
    err = "document.querySelector call is missing '('";
    return true;
  }
  ++pos;

  std::string selector;
  if (!parse_string_literal(statement, pos, selector, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ')') {
    err = "document.querySelector call is missing ')'";
    return true;
  }
  ++pos;

  if (selector.size() < 2 || selector[0] != '#') {
    err = "Unsupported document.querySelector selector (only '#id' is supported): " + selector;
    return true;
  }

  const std::string element_id = selector.substr(1);
  if (element_id.empty()) {
    err = "Unsupported document.querySelector selector (only '#id' is supported): " + selector;
    return true;
  }
  for (char ch : element_id) {
    if (std::isalnum(static_cast<unsigned char>(ch)) == 0 && ch != '_' && ch != '-') {
      err = "Unsupported document.querySelector selector (only '#id' is supported): " + selector;
      return true;
    }
  }

  skip_whitespace(statement, pos);

  constexpr std::string_view kStylePropertyPrefix = ".style.";
  if (statement.size() - pos >= kStylePropertyPrefix.size() &&
      statement.compare(pos, kStylePropertyPrefix.size(), kStylePropertyPrefix) == 0) {
    pos += kStylePropertyPrefix.size();

    std::string property;
    if (!parse_member_identifier(statement, pos, property)) {
      err = "document.querySelector(...).style assignment is missing property name";
      return true;
    }

    std::string css_property;
    if (!normalize_supported_style_property(property, css_property)) {
      err = "Unsupported document.querySelector(...).style property: " + property;
      return true;
    }

    std::string value;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.querySelector(...).style assignment",
                                 value,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    set_inline_style_property(*element, css_property, value);
    return true;
  }

  constexpr std::string_view kStylePrefix = ".style";
  if (statement.size() - pos >= kStylePrefix.size() &&
      statement.compare(pos, kStylePrefix.size(), kStylePrefix) == 0 &&
      has_assignment_operator(statement, pos + kStylePrefix.size())) {
    pos += kStylePrefix.size();

    std::string style_text;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.querySelector(...).style assignment",
                                 style_text,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    set_inline_style_string(*element, style_text);
    return true;
  }

  constexpr std::string_view kClassNamePrefix = ".className";
  if (statement.size() - pos >= kClassNamePrefix.size() &&
      statement.compare(pos, kClassNamePrefix.size(), kClassNamePrefix) == 0 &&
      has_assignment_operator(statement, pos + kClassNamePrefix.size())) {
    pos += kClassNamePrefix.size();

    std::string class_name;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.querySelector(...).className assignment",
                                 class_name,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    element->attributes["class"] = class_name;
    return true;
  }

  constexpr std::string_view kIdPrefix = ".id";
  if (statement.size() - pos >= kIdPrefix.size() &&
      statement.compare(pos, kIdPrefix.size(), kIdPrefix) == 0 &&
      has_assignment_operator(statement, pos + kIdPrefix.size())) {
    pos += kIdPrefix.size();

    std::string assigned_id;
    if (!parse_string_assignment(statement,
                                 pos,
                                 "document.querySelector(...).id assignment",
                                 assigned_id,
                                 err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    set_element_attribute(*element, "id", assigned_id);
    return true;
  }

  std::string attribute_name;
  std::string attribute_value;
  if (parse_set_attribute_call(
          statement, pos, "document.querySelector(...)", attribute_name, attribute_value, err)) {
    if (!err.empty()) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    set_element_attribute(*element, attribute_name, attribute_value);
    return true;
  }

  if (parse_remove_attribute_call(
          statement, pos, "document.querySelector(...)", attribute_name, err)) {
    if (!err.empty()) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    if (attribute_name == "style") {
      element->attributes.erase("style");
    } else {
      element->attributes.erase(attribute_name);
    }
    return true;
  }

  const std::string_view text_property_name = parse_supported_text_property(statement, pos);

  if (!text_property_name.empty()) {
    const std::string text_operation =
        "document.querySelector(...)." + std::string(text_property_name) + " assignment";

    std::string value;
    if (!parse_string_assignment(statement, pos, text_operation, value, err)) {
      return true;
    }

    browser::html::Node* element = find_first_element_by_id(document, element_id);
    if (element == nullptr) {
      err = "document.querySelector could not find element: " + selector;
      return true;
    }

    set_element_text(*element, value);
    return true;
  }

  err = "Unsupported document.querySelector operation";
  return true;
}

bool parse_console_log_statement(std::string_view statement, std::string& err) {
  constexpr std::string_view kPrefix = "console.log";
  if (!starts_with(statement, kPrefix)) {
    return false;
  }

  std::size_t pos = kPrefix.size();
  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != '(') {
    err = "console.log call is missing '('";
    return true;
  }
  ++pos;

  std::string message;
  if (!parse_string_literal(statement, pos, message, err)) {
    return true;
  }

  skip_whitespace(statement, pos);
  if (pos >= statement.size() || statement[pos] != ')') {
    err = "console.log call is missing ')'";
    return true;
  }
  ++pos;

  skip_whitespace(statement, pos);
  if (pos != statement.size()) {
    err = "Unexpected trailing characters in console.log call";
    return true;
  }

  std::cerr << message << '\n';
  return true;
}

std::vector<std::string> split_statements(const std::string& script_source) {
  std::vector<std::string> statements;
  std::string current;
  char quote = '\0';
  bool escaping = false;

  const auto flush_statement = [&statements, &current]() {
    const std::string trimmed = trim_copy(current);
    if (!trimmed.empty()) {
      statements.push_back(trimmed);
    }
    current.clear();
  };

  for (char ch : script_source) {
    if (quote != '\0') {
      current.push_back(ch);
      if (escaping) {
        escaping = false;
        continue;
      }
      if (ch == '\\') {
        escaping = true;
        continue;
      }
      if (ch == quote) {
        quote = '\0';
      }
      continue;
    }

    if (ch == '"' || ch == '\'') {
      quote = ch;
      current.push_back(ch);
      continue;
    }

    if (ch == ';' || ch == '\n' || ch == '\r') {
      flush_statement();
      continue;
    }

    current.push_back(ch);
  }

  flush_statement();
  return statements;
}

}  // namespace

ScriptResult execute_script(browser::html::Node& document, const std::string& script_source) {
  const std::vector<std::string> statements = split_statements(script_source);
  if (statements.empty()) {
    return {true, "Empty script"};
  }

  for (std::size_t i = 0; i < statements.size(); ++i) {
    const std::string& statement = statements[i];
    std::string err;

    const bool matched =
        parse_document_title_statement(statement, document, err) ||
        parse_document_body_statement(statement, document, err) ||
        parse_get_element_by_id_statement(statement, document, err) ||
        parse_query_selector_statement(statement, document, err) ||
        parse_console_log_statement(statement, err);

    if (!matched) {
      return {false, "Unsupported script statement " + std::to_string(i + 1) + ": " + statement};
    }

    if (!err.empty()) {
      return {false, "Script statement " + std::to_string(i + 1) + " failed: " + err};
    }
  }

  return {true, "OK"};
}

namespace {

const browser::html::Node* find_first_element_by_id_const(
    const browser::html::Node& node, const std::string& id_value) {
    if (node.type == browser::html::NodeType::Element) {
        auto id_it = node.attributes.find("id");
        if (id_it != node.attributes.end() && id_it->second == id_value) {
            return &node;
        }
    }
    for (const auto& child : node.children) {
        if (!child) continue;
        const auto* found = find_first_element_by_id_const(*child, id_value);
        if (found) return found;
    }
    return nullptr;
}

const browser::html::Node* find_first_element_by_tag_const(
    const browser::html::Node& node, const std::string& tag) {
    if (node.type == browser::html::NodeType::Element && node.tag_name == tag) {
        return &node;
    }
    for (const auto& child : node.children) {
        if (!child) continue;
        const auto* found = find_first_element_by_tag_const(*child, tag);
        if (found) return found;
    }
    return nullptr;
}

void find_all_elements_by_tag_const(
    const browser::html::Node& node, const std::string& tag,
    std::vector<const browser::html::Node*>& results) {
    if (node.type == browser::html::NodeType::Element && node.tag_name == tag) {
        results.push_back(&node);
    }
    for (const auto& child : node.children) {
        if (!child) continue;
        find_all_elements_by_tag_const(*child, tag, results);
    }
}

void find_all_elements_by_class_const(
    const browser::html::Node& node, const std::string& class_name,
    std::vector<const browser::html::Node*>& results) {
    if (node.type == browser::html::NodeType::Element) {
        auto it = node.attributes.find("class");
        if (it != node.attributes.end() && it->second == class_name) {
            results.push_back(&node);
        }
    }
    for (const auto& child : node.children) {
        if (!child) continue;
        find_all_elements_by_class_const(*child, class_name, results);
    }
}

std::string collect_text_content(const browser::html::Node& node) {
    if (node.type == browser::html::NodeType::Text) {
        return node.text_content;
    }
    std::string result;
    for (const auto& child : node.children) {
        if (!child) continue;
        result += collect_text_content(*child);
    }
    return result;
}

BridgeElement node_to_bridge_element(const browser::html::Node& node) {
    BridgeElement elem;
    elem.found = true;
    elem.tag_name = node.tag_name;
    elem.text_content = collect_text_content(node);
    elem.attributes = node.attributes;
    elem.child_count = node.children.size();
    return elem;
}

}  // namespace

QueryResult query_by_id(const browser::html::Node& document, const std::string& id) {
    if (id.empty()) {
        return {false, "Empty id", {}};
    }

    const auto* node = find_first_element_by_id_const(document, id);
    if (!node) {
        return {true, "Not found", {}};
    }

    QueryResult result;
    result.ok = true;
    result.message = "OK";
    result.elements.push_back(node_to_bridge_element(*node));
    return result;
}

QueryResult query_selector(const browser::html::Node& document, const std::string& selector) {
    if (selector.empty()) {
        return {false, "Empty selector", {}};
    }

    const browser::html::Node* node = nullptr;

    if (selector[0] == '#' && selector.size() > 1) {
        node = find_first_element_by_id_const(document, selector.substr(1));
    } else if (selector[0] == '.') {
        // Class selector - find first
        std::vector<const browser::html::Node*> results;
        find_all_elements_by_class_const(document, selector.substr(1), results);
        if (!results.empty()) node = results[0];
    } else {
        // Tag selector
        node = find_first_element_by_tag_const(document, selector);
    }

    if (!node) {
        return {true, "Not found", {}};
    }

    QueryResult result;
    result.ok = true;
    result.message = "OK";
    result.elements.push_back(node_to_bridge_element(*node));
    return result;
}

QueryResult query_selector_all(const browser::html::Node& document, const std::string& selector) {
    if (selector.empty()) {
        return {false, "Empty selector", {}};
    }

    std::vector<const browser::html::Node*> nodes;

    if (selector[0] == '#' && selector.size() > 1) {
        const auto* node = find_first_element_by_id_const(document, selector.substr(1));
        if (node) nodes.push_back(node);
    } else if (selector[0] == '.') {
        find_all_elements_by_class_const(document, selector.substr(1), nodes);
    } else {
        find_all_elements_by_tag_const(document, selector, nodes);
    }

    QueryResult result;
    result.ok = true;
    result.message = "OK";
    for (const auto* node : nodes) {
        result.elements.push_back(node_to_bridge_element(*node));
    }
    return result;
}

MutationResult set_attribute_by_id(browser::html::Node& document,
                                    const std::string& id,
                                    const std::string& attribute,
                                    const std::string& value) {
    if (id.empty()) return {false, "Empty id"};
    if (attribute.empty()) return {false, "Empty attribute name"};

    browser::html::Node* element = find_first_element_by_id(document, id);
    if (!element) return {false, "Element not found: " + id};

    set_element_attribute(*element, attribute, value);
    return {true, "OK"};
}

MutationResult remove_attribute_by_id(browser::html::Node& document,
                                       const std::string& id,
                                       const std::string& attribute) {
    if (id.empty()) return {false, "Empty id"};
    if (attribute.empty()) return {false, "Empty attribute name"};

    browser::html::Node* element = find_first_element_by_id(document, id);
    if (!element) return {false, "Element not found: " + id};

    element->attributes.erase(attribute);
    return {true, "OK"};
}

MutationResult set_style_by_id(browser::html::Node& document,
                                const std::string& id,
                                const std::string& property,
                                const std::string& value) {
    if (id.empty()) return {false, "Empty id"};
    if (property.empty()) return {false, "Empty style property"};

    browser::html::Node* element = find_first_element_by_id(document, id);
    if (!element) return {false, "Element not found: " + id};

    std::string css_property;
    if (!normalize_supported_style_property(property, css_property)) {
        return {false, "Unsupported style property: " + property};
    }

    set_inline_style_property(*element, css_property, value);
    return {true, "OK"};
}

MutationResult set_text_by_id(browser::html::Node& document,
                               const std::string& id,
                               const std::string& text) {
    if (id.empty()) return {false, "Empty id"};

    browser::html::Node* element = find_first_element_by_id(document, id);
    if (!element) return {false, "Element not found: " + id};

    set_element_text(*element, text);
    return {true, "OK"};
}

const char* event_type_name(EventType type) {
    switch (type) {
        case EventType::Click: return "click";
        case EventType::Input: return "input";
        case EventType::Change: return "change";
    }
    return "unknown";
}

void EventRegistry::add_listener(const std::string& target_id, EventType type, EventHandler handler) {
    bindings_.push_back({target_id, type, std::move(handler)});
}

MutationResult EventRegistry::dispatch(browser::html::Node& document, const DomEvent& event) const {
    bool any_handled = false;

    for (const auto& binding : bindings_) {
        if (binding.target_id == event.target_id && binding.type == event.type) {
            binding.handler(document, event);
            any_handled = true;
        }
    }

    if (!any_handled) {
        return {true, "No handler for event"};
    }

    return {true, "OK"};
}

std::size_t EventRegistry::listener_count() const {
    return bindings_.size();
}

void EventRegistry::clear() {
    bindings_.clear();
}

}  // namespace browser::js
