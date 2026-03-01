#include "browser/css/css_parser.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace browser::css {
namespace {

constexpr int kIdSpecificity = 100;
constexpr int kClassSpecificity = 10;
constexpr int kPseudoClassSpecificity = 10;
constexpr int kTagSpecificity = 1;
constexpr int kInlineSpecificity = 1000;

enum class PseudoClass {
  FirstChild,
  LastChild,
  FirstOfType,
  LastOfType,
  OnlyChild,
  Root,
  Empty,
  NthChild,
  NthOfType,
  NthLastChild,
  NthLastOfType,
  Not,
};

enum class NthChildPattern {
  ExactIndex,
  Odd,
  Even,
};

struct ParsedPseudoClass {
  PseudoClass type = PseudoClass::Root;
  NthChildPattern nth_child_pattern = NthChildPattern::ExactIndex;
  int nth_child_index = 0;
  std::string negated_selector;
};

enum class AttributeOperator {
  Exists,
  Exact,
  ClassContainsToken,
  Prefix,
  Suffix,
  ContainsSubstring,
};

struct ParsedAttributeSelector {
  std::string name;
  std::string value;
  AttributeOperator op = AttributeOperator::Exact;
};

enum class Combinator {
  Descendant,
  Child,
  AdjacentSibling,
  GeneralSibling,
};

struct CompoundSelector {
  std::string tag;
  std::vector<std::string> ids;
  std::vector<std::string> classes;
  std::vector<ParsedAttributeSelector> attribute_selectors;
  std::vector<ParsedPseudoClass> pseudo_classes;
  bool has_universal = false;
};

struct ParsedSelector {
  std::vector<CompoundSelector> compounds;
  std::vector<Combinator> combinators;
};

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string trim_copy(std::string_view s) {
  size_t begin = 0;
  while (begin < s.size() && is_space(s[begin])) {
    ++begin;
  }

  size_t end = s.size();
  while (end > begin && is_space(s[end - 1])) {
    --end;
  }

  return std::string(s.substr(begin, end - begin));
}

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool is_identifier_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
}

std::string parse_identifier(std::string_view source, size_t* cursor) {
  const size_t start = *cursor;
  while (*cursor < source.size() && is_identifier_char(source[*cursor])) {
    ++(*cursor);
  }
  if (start == *cursor) {
    return {};
  }
  return std::string(source.substr(start, *cursor - start));
}

void skip_spaces(std::string_view source, size_t* cursor) {
  while (*cursor < source.size() && is_space(source[*cursor])) {
    ++(*cursor);
  }
}

bool is_empty_compound_selector(const CompoundSelector& selector) {
  return !selector.has_universal &&
         selector.tag.empty() &&
         selector.ids.empty() &&
         selector.classes.empty() &&
         selector.attribute_selectors.empty() &&
         selector.pseudo_classes.empty();
}

bool parse_pseudo_class(std::string_view name, PseudoClass* pseudo_class) {
  const std::string lowered = to_lower_ascii(std::string(name));
  if (lowered == "first-child") {
    *pseudo_class = PseudoClass::FirstChild;
    return true;
  }
  if (lowered == "last-child") {
    *pseudo_class = PseudoClass::LastChild;
    return true;
  }
  if (lowered == "first-of-type") {
    *pseudo_class = PseudoClass::FirstOfType;
    return true;
  }
  if (lowered == "last-of-type") {
    *pseudo_class = PseudoClass::LastOfType;
    return true;
  }
  if (lowered == "only-child") {
    *pseudo_class = PseudoClass::OnlyChild;
    return true;
  }
  if (lowered == "root") {
    *pseudo_class = PseudoClass::Root;
    return true;
  }
  if (lowered == "empty") {
    *pseudo_class = PseudoClass::Empty;
    return true;
  }
  return false;
}

bool parse_positive_integer(std::string_view source, size_t* cursor, int* out) {
  const size_t start = *cursor;
  while (*cursor < source.size() && std::isdigit(static_cast<unsigned char>(source[*cursor])) != 0) {
    ++(*cursor);
  }
  if (start == *cursor) {
    return false;
  }

  int value = 0;
  for (size_t i = start; i < *cursor; ++i) {
    const int digit = source[i] - '0';
    if (value > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }

  if (value <= 0) {
    return false;
  }

  *out = value;
  return true;
}

bool parse_parenthesized_argument(std::string_view source, size_t* cursor, std::string* out) {
  if (*cursor >= source.size() || source[*cursor] != '(') {
    return false;
  }

  ++(*cursor);
  const size_t argument_start = *cursor;

  int paren_depth = 1;
  int bracket_depth = 0;
  char attribute_quote = '\0';

  while (*cursor < source.size()) {
    const char current = source[*cursor];

    if (attribute_quote != '\0') {
      if (current == attribute_quote) {
        attribute_quote = '\0';
      }
      ++(*cursor);
      continue;
    }

    if (bracket_depth > 0 && (current == '"' || current == '\'')) {
      attribute_quote = current;
      ++(*cursor);
      continue;
    }

    if (current == '[') {
      ++bracket_depth;
      ++(*cursor);
      continue;
    }

    if (current == ']') {
      if (bracket_depth <= 0) {
        return false;
      }
      --bracket_depth;
      ++(*cursor);
      continue;
    }

    if (current == '(') {
      ++paren_depth;
      ++(*cursor);
      continue;
    }

    if (current == ')') {
      --paren_depth;
      if (paren_depth == 0) {
        if (bracket_depth != 0) {
          return false;
        }
        *out = trim_copy(source.substr(argument_start, *cursor - argument_start));
        ++(*cursor);
        return true;
      }
      ++(*cursor);
      continue;
    }

    ++(*cursor);
  }

  return false;
}

bool parse_nth_pattern_expression(std::string_view expression, ParsedPseudoClass* out) {
  size_t cursor = 0;
  skip_spaces(expression, &cursor);

  int nth_index = 0;
  if (parse_positive_integer(expression, &cursor, &nth_index)) {
    out->nth_child_pattern = NthChildPattern::ExactIndex;
    out->nth_child_index = nth_index;
  } else {
    const std::string nth_alias = to_lower_ascii(parse_identifier(expression, &cursor));
    if (nth_alias == "odd") {
      out->nth_child_pattern = NthChildPattern::Odd;
      out->nth_child_index = 0;
    } else if (nth_alias == "even") {
      out->nth_child_pattern = NthChildPattern::Even;
      out->nth_child_index = 0;
    } else {
      return false;
    }
  }

  skip_spaces(expression, &cursor);
  return cursor == expression.size();
}

bool parse_nth_pattern_argument(
    std::string_view source,
    size_t* cursor,
    ParsedPseudoClass* out) {
  std::string nth_argument;
  if (!parse_parenthesized_argument(source, cursor, &nth_argument)) {
    return false;
  }
  return parse_nth_pattern_expression(nth_argument, out);
}

bool parse_attribute_selector(
    std::string_view source,
    size_t* cursor,
    ParsedAttributeSelector* out) {
  skip_spaces(source, cursor);
  std::string name = to_lower_ascii(parse_identifier(source, cursor));
  if (name.empty()) {
    return false;
  }

  skip_spaces(source, cursor);
  if (*cursor >= source.size()) {
    return false;
  }

  if (source[*cursor] == ']') {
    ++(*cursor);
    out->name = std::move(name);
    out->value.clear();
    out->op = AttributeOperator::Exists;
    return true;
  }

  AttributeOperator op = AttributeOperator::Exact;
  if (source[*cursor] == '=') {
    if (name != "id" && name != "class") {
      return false;
    }
    ++(*cursor);
  } else if (source[*cursor] == '~') {
    if (name != "class") {
      return false;
    }
    ++(*cursor);
    if (*cursor >= source.size() || source[*cursor] != '=') {
      return false;
    }
    ++(*cursor);
    op = AttributeOperator::ClassContainsToken;
  } else if (source[*cursor] == '^' || source[*cursor] == '$' || source[*cursor] == '*') {
    const char operator_prefix = source[*cursor];
    ++(*cursor);
    if (*cursor >= source.size() || source[*cursor] != '=') {
      return false;
    }
    ++(*cursor);
    if (operator_prefix == '^') {
      op = AttributeOperator::Prefix;
    } else if (operator_prefix == '$') {
      op = AttributeOperator::Suffix;
    } else {
      op = AttributeOperator::ContainsSubstring;
    }
  } else {
    return false;
  }

  skip_spaces(source, cursor);
  if (*cursor >= source.size()) {
    return false;
  }

  std::string value;
  if (source[*cursor] == '"' || source[*cursor] == '\'') {
    const char quote = source[*cursor];
    ++(*cursor);

    const size_t value_start = *cursor;
    while (*cursor < source.size() && source[*cursor] != quote) {
      ++(*cursor);
    }
    if (*cursor >= source.size()) {
      return false;
    }

    value = std::string(source.substr(value_start, *cursor - value_start));
    ++(*cursor);
  } else {
    value = parse_identifier(source, cursor);
    if (value.empty()) {
      return false;
    }
  }

  skip_spaces(source, cursor);
  if (*cursor >= source.size() || source[*cursor] != ']') {
    return false;
  }
  ++(*cursor);

  out->name = std::move(name);
  out->value = std::move(value);
  out->op = op;
  return true;
}

bool parse_compound_selector(std::string_view source, CompoundSelector* out) {
  CompoundSelector selector;
  size_t cursor = 0;

  if (cursor < source.size() && source[cursor] == '*') {
    selector.has_universal = true;
    ++cursor;
  } else if (cursor < source.size() &&
             source[cursor] != '#' &&
             source[cursor] != '.' &&
             source[cursor] != ':' &&
             source[cursor] != '[') {
    std::string tag = parse_identifier(source, &cursor);
    if (tag.empty()) {
      return false;
    }
    selector.tag = to_lower_ascii(std::move(tag));
  }

  while (cursor < source.size()) {
    const char kind = source[cursor];
    ++cursor;

    if (kind == '#') {
      std::string id = parse_identifier(source, &cursor);
      if (id.empty()) {
        return false;
      }
      selector.ids.push_back(std::move(id));
      continue;
    }

    if (kind == '.') {
      std::string class_name = parse_identifier(source, &cursor);
      if (class_name.empty()) {
        return false;
      }
      selector.classes.push_back(std::move(class_name));
      continue;
    }

    if (kind == '[') {
      ParsedAttributeSelector attribute_selector;
      if (!parse_attribute_selector(source, &cursor, &attribute_selector)) {
        return false;
      }
      selector.attribute_selectors.push_back(std::move(attribute_selector));
      continue;
    }

    if (kind == ':') {
      const std::string pseudo_name = parse_identifier(source, &cursor);
      if (pseudo_name.empty()) {
        return false;
      }

      ParsedPseudoClass parsed_pseudo;
      const std::string lowered_pseudo_name = to_lower_ascii(pseudo_name);
      if (lowered_pseudo_name == "nth-child" ||
          lowered_pseudo_name == "nth-of-type" ||
          lowered_pseudo_name == "nth-last-child" ||
          lowered_pseudo_name == "nth-last-of-type") {
        if (!parse_nth_pattern_argument(source, &cursor, &parsed_pseudo)) {
          return false;
        }
        if (lowered_pseudo_name == "nth-child") {
          parsed_pseudo.type = PseudoClass::NthChild;
        } else if (lowered_pseudo_name == "nth-of-type") {
          parsed_pseudo.type = PseudoClass::NthOfType;
        } else if (lowered_pseudo_name == "nth-last-child") {
          parsed_pseudo.type = PseudoClass::NthLastChild;
        } else {
          parsed_pseudo.type = PseudoClass::NthLastOfType;
        }
      } else if (lowered_pseudo_name == "not") {
        std::string negated_selector;
        if (!parse_parenthesized_argument(source, &cursor, &negated_selector)) {
          return false;
        }
        if (negated_selector.empty()) {
          return false;
        }

        CompoundSelector negated_compound;
        if (!parse_compound_selector(negated_selector, &negated_compound)) {
          return false;
        }

        parsed_pseudo.type = PseudoClass::Not;
        parsed_pseudo.negated_selector = std::move(negated_selector);
      } else {
        PseudoClass pseudo_class;
        if (!parse_pseudo_class(pseudo_name, &pseudo_class)) {
          return false;
        }
        parsed_pseudo.type = pseudo_class;
      }

      selector.pseudo_classes.push_back(std::move(parsed_pseudo));
      continue;
    }

    return false;
  }

  if (is_empty_compound_selector(selector)) {
    return false;
  }

  *out = std::move(selector);
  return true;
}

bool parse_selector(std::string_view source, ParsedSelector* out) {
  ParsedSelector selector;
  size_t cursor = 0;

  auto parse_next_compound = [&source, &cursor](CompoundSelector* compound) -> bool {
    const size_t start = cursor;
    int paren_depth = 0;
    int bracket_depth = 0;
    char attribute_quote = '\0';
    while (cursor < source.size()) {
      const char current = source[cursor];

      if (attribute_quote != '\0') {
        if (current == attribute_quote) {
          attribute_quote = '\0';
        }
        ++cursor;
        continue;
      }

      if (bracket_depth > 0 && (current == '"' || current == '\'')) {
        attribute_quote = current;
        ++cursor;
        continue;
      }

      if (current == '(') {
        ++paren_depth;
        ++cursor;
        continue;
      }

      if (current == ')') {
        if (paren_depth <= 0) {
          return false;
        }
        --paren_depth;
        ++cursor;
        continue;
      }

      if (current == '[') {
        ++bracket_depth;
        ++cursor;
        continue;
      }

      if (current == ']') {
        if (bracket_depth <= 0) {
          return false;
        }
        --bracket_depth;
        ++cursor;
        continue;
      }

      if (paren_depth == 0 &&
          bracket_depth == 0 &&
          (is_space(current) || current == '>' || current == '+' || current == '~')) {
        break;
      }

      ++cursor;
    }

    if (paren_depth != 0 || bracket_depth != 0 || attribute_quote != '\0') {
      return false;
    }

    if (start == cursor) {
      return false;
    }

    return parse_compound_selector(source.substr(start, cursor - start), compound);
  };

  while (cursor < source.size() && is_space(source[cursor])) {
    ++cursor;
  }

  CompoundSelector first_compound;
  if (!parse_next_compound(&first_compound)) {
    return false;
  }
  selector.compounds.push_back(std::move(first_compound));

  while (cursor < source.size()) {
    bool saw_space = false;
    while (cursor < source.size() && is_space(source[cursor])) {
      saw_space = true;
      ++cursor;
    }
    if (cursor >= source.size()) {
      break;
    }

    Combinator combinator = Combinator::Descendant;
    if (source[cursor] == '>' || source[cursor] == '+' || source[cursor] == '~') {
      if (source[cursor] == '>') {
        combinator = Combinator::Child;
      } else if (source[cursor] == '+') {
        combinator = Combinator::AdjacentSibling;
      } else {
        combinator = Combinator::GeneralSibling;
      }
      ++cursor;

      while (cursor < source.size() && is_space(source[cursor])) {
        ++cursor;
      }
      if (cursor >= source.size()) {
        return false;
      }
    } else if (!saw_space) {
      return false;
    }

    CompoundSelector compound;
    if (!parse_next_compound(&compound)) {
      return false;
    }
    selector.combinators.push_back(combinator);
    selector.compounds.push_back(std::move(compound));
  }

  if (selector.compounds.empty() ||
      selector.combinators.size() + 1 != selector.compounds.size()) {
    return false;
  }

  *out = std::move(selector);
  return true;
}

std::vector<std::string> split_selector_list(std::string_view source) {
  std::vector<std::string> selectors;
  size_t cursor = 0;
  while (cursor <= source.size()) {
    const size_t comma = source.find(',', cursor);
    const std::string_view raw_selector =
        comma == std::string_view::npos ? source.substr(cursor) : source.substr(cursor, comma - cursor);
    std::string selector = trim_copy(raw_selector);
    if (!selector.empty()) {
      selectors.push_back(std::move(selector));
    }

    if (comma == std::string_view::npos) {
      break;
    }
    cursor = comma + 1;
  }
  return selectors;
}

int compute_specificity(const std::string& selector) {
  ParsedSelector parsed_selector;
  if (!parse_selector(selector, &parsed_selector)) {
    return 0;
  }

  int specificity = 0;
  for (const auto& compound : parsed_selector.compounds) {
    specificity += static_cast<int>(compound.ids.size()) * kIdSpecificity;
    specificity += static_cast<int>(compound.classes.size()) * kClassSpecificity;
    specificity += static_cast<int>(compound.attribute_selectors.size()) * kClassSpecificity;
    specificity += static_cast<int>(compound.pseudo_classes.size()) * kPseudoClassSpecificity;
    if (!compound.tag.empty()) {
      specificity += kTagSpecificity;
    }
  }
  return specificity;
}

std::vector<Declaration> parse_declarations(std::string_view block) {
  std::vector<Declaration> declarations;

  size_t cursor = 0;
  while (cursor < block.size()) {
    const size_t semi = block.find(';', cursor);
    const std::string_view chunk =
        semi == std::string_view::npos ? block.substr(cursor) : block.substr(cursor, semi - cursor);

    const size_t colon = chunk.find(':');
    if (colon != std::string_view::npos) {
      std::string property = to_lower_ascii(trim_copy(chunk.substr(0, colon)));
      std::string value = trim_copy(chunk.substr(colon + 1));
      if (!property.empty()) {
        declarations.push_back({std::move(property), std::move(value)});
      }
    }

    if (semi == std::string_view::npos) {
      break;
    }
    cursor = semi + 1;
  }

  return declarations;
}

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
std::string value_to_string(const T& value) {
  using Decayed = std::decay_t<T>;

  if constexpr (is_optional<Decayed>::value) {
    if (value.has_value()) {
      return value_to_string(*value);
    }
    return {};
  } else if constexpr (std::is_same_v<Decayed, std::string>) {
    return value;
  } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
    return std::string(value);
  } else if constexpr (std::is_same_v<Decayed, const char*> || std::is_same_v<Decayed, char*>) {
    return value == nullptr ? std::string() : std::string(value);
  } else if constexpr (std::is_convertible_v<Decayed, std::string>) {
    return static_cast<std::string>(value);
  } else {
    return {};
  }
}

template <typename T>
bool value_has_presence(const T& value) {
  using Decayed = std::decay_t<T>;

  if constexpr (is_optional<Decayed>::value) {
    return value.has_value();
  } else if constexpr (std::is_same_v<Decayed, std::string>) {
    return !trim_copy(value).empty();
  } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
    return !trim_copy(value).empty();
  } else if constexpr (std::is_same_v<Decayed, const char*> || std::is_same_v<Decayed, char*>) {
    return value != nullptr && !trim_copy(std::string_view(value)).empty();
  } else if constexpr (std::is_convertible_v<Decayed, std::string>) {
    return !trim_copy(static_cast<std::string>(value)).empty();
  } else {
    return false;
  }
}

template <typename T, typename = void>
struct has_member_attributes : std::false_type {};

template <typename T>
struct has_member_attributes<T, std::void_t<decltype(std::declval<const T&>().attributes)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_attrs : std::false_type {};

template <typename T>
struct has_member_attrs<T, std::void_t<decltype(std::declval<const T&>().attrs)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_tag_name : std::false_type {};

template <typename T>
struct has_member_tag_name<T, std::void_t<decltype(std::declval<const T&>().tag_name)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_tag : std::false_type {};

template <typename T>
struct has_member_tag<T, std::void_t<decltype(std::declval<const T&>().tag)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_name : std::false_type {};

template <typename T>
struct has_member_name<T, std::void_t<decltype(std::declval<const T&>().name)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_id : std::false_type {};

template <typename T>
struct has_member_id<T, std::void_t<decltype(std::declval<const T&>().id)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_class_name : std::false_type {};

template <typename T>
struct has_member_class_name<T, std::void_t<decltype(std::declval<const T&>().class_name)>> : std::true_type {};

template <typename T, typename = void>
struct has_member_style : std::false_type {};

template <typename T>
struct has_member_style<T, std::void_t<decltype(std::declval<const T&>().style)>> : std::true_type {};

template <typename T, typename = void>
struct has_method_get_attribute : std::false_type {};

template <typename T>
struct has_method_get_attribute<
    T,
    std::void_t<decltype(std::declval<const T&>().get_attribute(std::declval<const std::string&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_method_attribute : std::false_type {};

template <typename T>
struct has_method_attribute<T, std::void_t<decltype(std::declval<const T&>().attribute(std::declval<const std::string&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_method_attr : std::false_type {};

template <typename T>
struct has_method_attr<T, std::void_t<decltype(std::declval<const T&>().attr(std::declval<const std::string&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_method_tag_name : std::false_type {};

template <typename T>
struct has_method_tag_name<T, std::void_t<decltype(std::declval<const T&>().tag_name())>> : std::true_type {};

template <typename T, typename = void>
struct has_method_tag : std::false_type {};

template <typename T>
struct has_method_tag<T, std::void_t<decltype(std::declval<const T&>().tag())>> : std::true_type {};

template <typename T, typename = void>
struct has_find_for_string_key : std::false_type {};

template <typename T>
struct has_find_for_string_key<
    T,
    std::void_t<
        decltype(std::declval<const T&>().find(std::declval<const std::string&>())),
        decltype(std::declval<const T&>().end())>> : std::true_type {};

template <typename Container>
std::string lookup_attribute(const Container& attrs, const std::string& key) {
  if constexpr (has_find_for_string_key<Container>::value) {
    const auto it = attrs.find(key);
    if (it != attrs.end()) {
      return value_to_string(it->second);
    }
  }
  return {};
}

template <typename Container>
bool lookup_attribute_exists(const Container& attrs, const std::string& key) {
  if constexpr (has_find_for_string_key<Container>::value) {
    return attrs.find(key) != attrs.end();
  }
  return false;
}

template <typename Node>
std::string get_attribute_value(const Node& node, const std::string& key) {
  if constexpr (has_method_get_attribute<Node>::value) {
    const std::string value = trim_copy(value_to_string(node.get_attribute(key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_attribute<Node>::value) {
    const std::string value = trim_copy(value_to_string(node.attribute(key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_method_attr<Node>::value) {
    const std::string value = trim_copy(value_to_string(node.attr(key)));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_attributes<Node>::value) {
    const std::string value = trim_copy(lookup_attribute(node.attributes, key));
    if (!value.empty()) {
      return value;
    }
  }

  if constexpr (has_member_attrs<Node>::value) {
    const std::string value = trim_copy(lookup_attribute(node.attrs, key));
    if (!value.empty()) {
      return value;
    }
  }

  if (key == "id") {
    if constexpr (has_member_id<Node>::value) {
      return trim_copy(value_to_string(node.id));
    }
  }

  if (key == "class") {
    if constexpr (has_member_class_name<Node>::value) {
      return trim_copy(value_to_string(node.class_name));
    }
  }

  if (key == "style") {
    if constexpr (has_member_style<Node>::value) {
      return trim_copy(value_to_string(node.style));
    }
  }

  return {};
}

template <typename Node>
bool has_attribute(const Node& node, const std::string& key) {
  if constexpr (has_member_attributes<Node>::value) {
    if (lookup_attribute_exists(node.attributes, key)) {
      return true;
    }
  }

  if constexpr (has_member_attrs<Node>::value) {
    if (lookup_attribute_exists(node.attrs, key)) {
      return true;
    }
  }

  if constexpr (has_method_get_attribute<Node>::value) {
    if (value_has_presence(node.get_attribute(key))) {
      return true;
    }
  }

  if constexpr (has_method_attribute<Node>::value) {
    if (value_has_presence(node.attribute(key))) {
      return true;
    }
  }

  if constexpr (has_method_attr<Node>::value) {
    if (value_has_presence(node.attr(key))) {
      return true;
    }
  }

  if (key == "id") {
    if constexpr (has_member_id<Node>::value) {
      if (value_has_presence(node.id)) {
        return true;
      }
    }
  }

  if (key == "class") {
    if constexpr (has_member_class_name<Node>::value) {
      if (value_has_presence(node.class_name)) {
        return true;
      }
    }
  }

  if (key == "style") {
    if constexpr (has_member_style<Node>::value) {
      if (value_has_presence(node.style)) {
        return true;
      }
    }
  }

  return false;
}

template <typename Node>
std::string get_node_tag(const Node& node) {
  if constexpr (has_member_tag_name<Node>::value) {
    return to_lower_ascii(trim_copy(value_to_string(node.tag_name)));
  }

  if constexpr (has_member_tag<Node>::value) {
    return to_lower_ascii(trim_copy(value_to_string(node.tag)));
  }

  if constexpr (has_method_tag_name<Node>::value) {
    return to_lower_ascii(trim_copy(value_to_string(node.tag_name())));
  }

  if constexpr (has_method_tag<Node>::value) {
    return to_lower_ascii(trim_copy(value_to_string(node.tag())));
  }

  if constexpr (has_member_name<Node>::value) {
    return to_lower_ascii(trim_copy(value_to_string(node.name)));
  }

  return {};
}

bool has_class_token(const std::string& class_attribute, std::string_view token) {
  size_t cursor = 0;
  while (cursor < class_attribute.size()) {
    while (cursor < class_attribute.size() && is_space(class_attribute[cursor])) {
      ++cursor;
    }

    const size_t start = cursor;
    while (cursor < class_attribute.size() && !is_space(class_attribute[cursor])) {
      ++cursor;
    }

    if (start < cursor && class_attribute.compare(start, cursor - start, token) == 0) {
      return true;
    }
  }

  return false;
}

bool is_whitespace_only(std::string_view text) {
  for (char c : text) {
    if (!is_space(c)) {
      return false;
    }
  }
  return true;
}

template <typename Node>
bool is_first_element_child(const Node& node) {
  if (node.parent == nullptr) {
    return false;
  }

  for (const auto& sibling : node.parent->children) {
    if (sibling != nullptr && sibling->type == browser::html::NodeType::Element) {
      return sibling.get() == &node;
    }
  }
  return false;
}

template <typename Node>
bool is_last_element_child(const Node& node) {
  if (node.parent == nullptr) {
    return false;
  }

  for (auto it = node.parent->children.rbegin(); it != node.parent->children.rend(); ++it) {
    if (*it != nullptr && (*it)->type == browser::html::NodeType::Element) {
      return it->get() == &node;
    }
  }
  return false;
}

template <typename Node>
bool is_empty_element(const Node& node) {
  for (const auto& child : node.children) {
    if (child == nullptr) {
      continue;
    }

    if (child->type == browser::html::NodeType::Element) {
      return false;
    }

    if (child->type == browser::html::NodeType::Text &&
        !is_whitespace_only(child->text_content)) {
      return false;
    }
  }

  return true;
}

template <typename Node>
bool is_first_of_type(const Node& node) {
  if (node.parent == nullptr) {
    return false;
  }

  const std::string node_tag = get_node_tag(node);
  if (node_tag.empty()) {
    return false;
  }

  for (const auto& sibling : node.parent->children) {
    if (sibling == nullptr || sibling->type != browser::html::NodeType::Element) {
      continue;
    }

    const std::string sibling_tag = get_node_tag(*sibling);
    if (sibling_tag.empty() || sibling_tag != node_tag) {
      continue;
    }

    return sibling.get() == &node;
  }

  return false;
}

template <typename Node>
bool is_last_of_type(const Node& node) {
  if (node.parent == nullptr) {
    return false;
  }

  const std::string node_tag = get_node_tag(node);
  if (node_tag.empty()) {
    return false;
  }

  for (auto it = node.parent->children.rbegin(); it != node.parent->children.rend(); ++it) {
    if (*it == nullptr || (*it)->type != browser::html::NodeType::Element) {
      continue;
    }

    const std::string sibling_tag = get_node_tag(**it);
    if (sibling_tag.empty() || sibling_tag != node_tag) {
      continue;
    }

    return it->get() == &node;
  }

  return false;
}

template <typename Node>
bool is_only_element_child(const Node& node) {
  if (node.parent == nullptr) {
    return false;
  }

  int element_children = 0;
  bool found_node = false;
  for (const auto& sibling : node.parent->children) {
    if (sibling != nullptr && sibling->type == browser::html::NodeType::Element) {
      ++element_children;
      if (sibling.get() == &node) {
        found_node = true;
      }
    }
  }

  return found_node && element_children == 1;
}

template <typename Node>
bool is_nth_element_child(
    const Node& node,
    const ParsedPseudoClass& pseudo_class,
    bool same_tag_only,
    bool count_from_end) {
  if (node.parent == nullptr) {
    return false;
  }

  if (pseudo_class.nth_child_pattern == NthChildPattern::ExactIndex &&
      pseudo_class.nth_child_index <= 0) {
    return false;
  }

  auto matches_pattern = [&pseudo_class](int index) {
    switch (pseudo_class.nth_child_pattern) {
      case NthChildPattern::ExactIndex:
        return index == pseudo_class.nth_child_index;
      case NthChildPattern::Odd:
        return (index % 2) == 1;
      case NthChildPattern::Even:
        return (index % 2) == 0;
    }
    return false;
  };

  std::string node_tag;
  if (same_tag_only) {
    node_tag = get_node_tag(node);
    if (node_tag.empty()) {
      return false;
    }
  }

  int element_index = 0;
  if (count_from_end) {
    for (auto it = node.parent->children.rbegin(); it != node.parent->children.rend(); ++it) {
      if (*it != nullptr && (*it)->type == browser::html::NodeType::Element) {
        if (same_tag_only) {
          const std::string sibling_tag = get_node_tag(**it);
          if (sibling_tag.empty() || sibling_tag != node_tag) {
            continue;
          }
        }

        ++element_index;
        if (it->get() == &node) {
          return matches_pattern(element_index);
        }
      }
    }
  } else {
    for (const auto& sibling : node.parent->children) {
      if (sibling != nullptr && sibling->type == browser::html::NodeType::Element) {
        if (same_tag_only) {
          const std::string sibling_tag = get_node_tag(*sibling);
          if (sibling_tag.empty() || sibling_tag != node_tag) {
            continue;
          }
        }

        ++element_index;
        if (sibling.get() == &node) {
          return matches_pattern(element_index);
        }
      }
    }
  }

  return false;
}

template <typename Node>
bool compound_matches_node(const CompoundSelector& selector, const Node& node);

template <typename Node>
bool pseudo_class_matches(const ParsedPseudoClass& pseudo_class, const Node& node) {
  if (node.type != browser::html::NodeType::Element) {
    return false;
  }

  switch (pseudo_class.type) {
    case PseudoClass::FirstChild:
      return is_first_element_child(node);
    case PseudoClass::LastChild:
      return is_last_element_child(node);
    case PseudoClass::FirstOfType:
      return is_first_of_type(node);
    case PseudoClass::LastOfType:
      return is_last_of_type(node);
    case PseudoClass::OnlyChild:
      return is_only_element_child(node);
    case PseudoClass::Root:
      return node.parent == nullptr || node.parent->type == browser::html::NodeType::Document;
    case PseudoClass::Empty:
      return is_empty_element(node);
    case PseudoClass::NthChild:
      return is_nth_element_child(node, pseudo_class, false, false);
    case PseudoClass::NthOfType:
      return is_nth_element_child(node, pseudo_class, true, false);
    case PseudoClass::NthLastChild:
      return is_nth_element_child(node, pseudo_class, false, true);
    case PseudoClass::NthLastOfType:
      return is_nth_element_child(node, pseudo_class, true, true);
    case PseudoClass::Not: {
      if (pseudo_class.negated_selector.empty()) {
        return false;
      }
      CompoundSelector negated_compound;
      if (!parse_compound_selector(pseudo_class.negated_selector, &negated_compound)) {
        return false;
      }
      return !compound_matches_node(negated_compound, node);
    }
  }

  return false;
}

template <typename Node>
const Node* previous_element_sibling(const Node& node) {
  if (node.parent == nullptr) {
    return nullptr;
  }

  const auto& siblings = node.parent->children;
  const Node* previous = nullptr;
  for (const auto& sibling : siblings) {
    if (sibling.get() == &node) {
      return previous;
    }
    if (sibling != nullptr && sibling->type == browser::html::NodeType::Element) {
      previous = sibling.get();
    }
  }

  return nullptr;
}

template <typename Node>
bool compound_matches_node(const CompoundSelector& selector, const Node& node) {
  if (node.type != browser::html::NodeType::Element) {
    return false;
  }

  if (is_empty_compound_selector(selector)) {
    return false;
  }

  if (!selector.tag.empty()) {
    const std::string tag = get_node_tag(node);
    if (tag.empty() || tag != selector.tag) {
      return false;
    }
  }

  if (!selector.ids.empty()) {
    const std::string id = get_attribute_value(node, "id");
    if (id.empty()) {
      return false;
    }
    for (const auto& selector_id : selector.ids) {
      if (id != selector_id) {
        return false;
      }
    }
  }

  if (!selector.classes.empty()) {
    const std::string class_attribute = get_attribute_value(node, "class");
    if (class_attribute.empty()) {
      return false;
    }
    for (const auto& class_name : selector.classes) {
      if (!has_class_token(class_attribute, class_name)) {
        return false;
      }
    }
  }

  for (const auto& attribute_selector : selector.attribute_selectors) {
    const std::string attribute_value = get_attribute_value(node, attribute_selector.name);
    switch (attribute_selector.op) {
      case AttributeOperator::Exists:
        if (!has_attribute(node, attribute_selector.name)) {
          return false;
        }
        break;
      case AttributeOperator::Exact:
        if (attribute_value != attribute_selector.value) {
          return false;
        }
        break;
      case AttributeOperator::ClassContainsToken:
        if (!has_class_token(attribute_value, attribute_selector.value)) {
          return false;
        }
        break;
      case AttributeOperator::Prefix:
        if (attribute_value.size() < attribute_selector.value.size() ||
            attribute_value.compare(0, attribute_selector.value.size(), attribute_selector.value) != 0) {
          return false;
        }
        break;
      case AttributeOperator::Suffix:
        if (attribute_value.size() < attribute_selector.value.size() ||
            attribute_value.compare(
                attribute_value.size() - attribute_selector.value.size(),
                attribute_selector.value.size(),
                attribute_selector.value) != 0) {
          return false;
        }
        break;
      case AttributeOperator::ContainsSubstring:
        if (attribute_value.find(attribute_selector.value) == std::string::npos) {
          return false;
        }
        break;
    }
  }

  for (const ParsedPseudoClass& pseudo_class : selector.pseudo_classes) {
    if (!pseudo_class_matches(pseudo_class, node)) {
      return false;
    }
  }

  return true;
}

template <typename Node>
bool parsed_selector_matches_node(const ParsedSelector& selector, const Node& node) {
  if (selector.compounds.empty()) {
    return false;
  }

  if (selector.combinators.size() + 1 != selector.compounds.size()) {
    return false;
  }

  if (!compound_matches_node(selector.compounds.back(), node)) {
    return false;
  }

  const Node* current = &node;
  for (size_t index = selector.compounds.size() - 1; index-- > 0;) {
    const CompoundSelector& lhs_selector = selector.compounds[index];
    const Combinator combinator = selector.combinators[index];

    if (combinator == Combinator::Descendant) {
      const Node* ancestor = current->parent;
      bool matched = false;
      while (ancestor != nullptr) {
        if (ancestor->type == browser::html::NodeType::Element &&
            compound_matches_node(lhs_selector, *ancestor)) {
          matched = true;
          current = ancestor;
          break;
        }
        ancestor = ancestor->parent;
      }
      if (!matched) {
        return false;
      }
      continue;
    }

    if (combinator == Combinator::Child) {
      const Node* parent = current->parent;
      if (parent == nullptr ||
          parent->type != browser::html::NodeType::Element ||
          !compound_matches_node(lhs_selector, *parent)) {
        return false;
      }
      current = parent;
      continue;
    }

    if (combinator == Combinator::AdjacentSibling) {
      const Node* sibling = previous_element_sibling(*current);
      if (sibling == nullptr || !compound_matches_node(lhs_selector, *sibling)) {
        return false;
      }
      current = sibling;
      continue;
    }

    if (combinator == Combinator::GeneralSibling) {
      const Node* sibling = previous_element_sibling(*current);
      bool matched = false;
      while (sibling != nullptr) {
        if (compound_matches_node(lhs_selector, *sibling)) {
          matched = true;
          current = sibling;
          break;
        }
        sibling = previous_element_sibling(*sibling);
      }
      if (!matched) {
        return false;
      }
      continue;
    }

    return false;
  }

  return true;
}

template <typename Node>
bool selector_matches_node(const std::string& selector, const Node& node) {
  ParsedSelector parsed_selector;
  if (!parse_selector(selector, &parsed_selector)) {
    return false;
  }
  return parsed_selector_matches_node(parsed_selector, node);
}

// Strip @import rules from CSS text and collect the imported URLs.
// Per CSS spec, @import must appear before any other rules. We scan
// the entire text to be tolerant of malformed stylesheets.
//
// Recognised forms:
//   @import "url";
//   @import 'url';
//   @import url("url");
//   @import url('url');
//   @import url(bare-url);
//   Any of the above may be followed by a media query before the semicolon.
//
// Returns the CSS text with all @import statements removed, and appends
// the extracted import URLs to |out_import_urls| (if non-null).
std::string strip_css_imports(const std::string& css,
                               std::vector<std::string>* out_import_urls) {
  std::string result;
  result.reserve(css.size());
  size_t i = 0;
  const size_t n = css.size();

  auto skip_ws = [&]() {
    while (i < n && is_space(css[i])) ++i;
  };

  // Helper: consume a quoted string ("..." or '...'), return the inner text.
  auto consume_quoted = [&](char q) -> std::string {
    ++i;  // skip opening quote
    std::string inner;
    while (i < n && css[i] != q) {
      inner += css[i++];
    }
    if (i < n) ++i;  // skip closing quote
    return inner;
  };

  // Helper: consume url(...) and return the URL text.
  auto consume_url_fn = [&]() -> std::string {
    // Assumes "url(" has just been consumed
    skip_ws();
    std::string url;
    if (i < n && (css[i] == '"' || css[i] == '\'')) {
      url = consume_quoted(css[i]);
    } else {
      // bare url: collect until ')' (no whitespace inside)
      while (i < n && css[i] != ')' && !is_space(css[i])) {
        url += css[i++];
      }
    }
    skip_ws();
    if (i < n && css[i] == ')') ++i;  // skip ')'
    return url;
  };

  while (i < n) {
    // Skip comments (/* ... */)
    if (i + 1 < n && css[i] == '/' && css[i + 1] == '*') {
      i += 2;
      while (i + 1 < n && !(css[i] == '*' && css[i + 1] == '/')) ++i;
      if (i + 1 < n) i += 2;
      continue;
    }

    // Check for @import
    if (css[i] == '@') {
      // Peek ahead for "import"
      const size_t at_pos = i;
      ++i;
      skip_ws();
      // Collect the at-keyword name
      size_t kw_start = i;
      while (i < n && (std::isalnum(static_cast<unsigned char>(css[i])) || css[i] == '-')) ++i;
      std::string kw = css.substr(kw_start, i - kw_start);
      // Lower-case comparison
      std::string kw_lower;
      for (char c : kw) kw_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      if (kw_lower == "import") {
        // Parse the URL — supports url("..."), url('...'), url(bare),
        // or a bare string literal "..." / '...'
        skip_ws();
        std::string url;
        if (i + 3 < n && css.substr(i, 4) == "url(") {
          i += 4;
          url = consume_url_fn();
        } else if (i < n && (css[i] == '"' || css[i] == '\'')) {
          url = consume_quoted(css[i]);
        }
        // Skip to the terminating semicolon (consume optional media query)
        while (i < n && css[i] != ';') ++i;
        if (i < n) ++i;  // consume ';'

        if (!url.empty() && out_import_urls) {
          out_import_urls->push_back(url);
        }
        // Replace the @import statement with whitespace (preserve line numbers)
        // by simply not copying it to result
        continue;
      } else {
        // Not @import — copy the '@' and keyword we consumed back to result
        // We already advanced i past the keyword; just append what we read.
        result += css.substr(at_pos, i - at_pos);
        continue;
      }
    }

    result += css[i++];
  }

  return result;
}

}  // namespace

Stylesheet parse_css(const std::string& css) {
  Stylesheet stylesheet;
  const std::string stripped = strip_css_imports(css, nullptr);
  const std::string_view source(stripped);

  size_t cursor = 0;
  while (cursor < source.size()) {
    const size_t open_brace = source.find('{', cursor);
    if (open_brace == std::string_view::npos) {
      break;
    }

    const size_t close_brace = source.find('}', open_brace + 1);
    if (close_brace == std::string_view::npos) {
      break;
    }

    const std::vector<std::string> selectors = split_selector_list(source.substr(cursor, open_brace - cursor));
    const std::vector<Declaration> declarations =
        parse_declarations(source.substr(open_brace + 1, close_brace - open_brace - 1));

    for (const std::string& selector : selectors) {
      Rule rule;
      rule.selector = selector;
      rule.declarations = declarations;
      rule.specificity = compute_specificity(rule.selector);
      stylesheet.rules.push_back(std::move(rule));
    }

    cursor = close_brace + 1;
  }

  return stylesheet;
}

std::map<std::string, std::string> parse_inline_style(const std::string& s) {
  std::map<std::string, std::string> inline_style;
  for (const auto& declaration : parse_declarations(s)) {
    inline_style[declaration.property] = declaration.value;
  }
  return inline_style;
}

std::map<std::string, std::string> compute_style_for_node(
    const browser::html::Node& node,
    const Stylesheet& stylesheet) {
  if (node.type != browser::html::NodeType::Element) {
    return {};
  }

  struct Winner {
    int specificity = -1;
    size_t source_order = 0;
    std::string value;
  };

  std::map<std::string, Winner> winners;
  size_t source_order = 0;

  for (const auto& rule : stylesheet.rules) {
    if (!selector_matches_node(rule.selector, node)) {
      continue;
    }

    for (const auto& declaration : rule.declarations) {
      if (declaration.property.empty()) {
        continue;
      }

      const auto it = winners.find(declaration.property);
      const bool should_override =
          it == winners.end() ||
          rule.specificity > it->second.specificity ||
          (rule.specificity == it->second.specificity && source_order >= it->second.source_order);

      if (should_override) {
        winners[declaration.property] = Winner{rule.specificity, source_order, declaration.value};
      }
      ++source_order;
    }
  }

  const auto inline_style = parse_inline_style(get_attribute_value(node, "style"));
  for (const auto& [property, value] : inline_style) {
    winners[property] = Winner{kInlineSpecificity, source_order, value};
    ++source_order;
  }

  std::map<std::string, std::string> computed;
  for (const auto& [property, winner] : winners) {
    computed[property] = winner.value;
  }

  return computed;
}

ParseCssResult parse_css_with_diagnostics(const std::string& css) {
  ParseCssResult result;
  // Strip @import rules (not fetchable in this context) before normal parsing
  const std::string stripped = strip_css_imports(css, nullptr);
  const std::string_view source(stripped);

  size_t cursor = 0;
  while (cursor < source.size()) {
    const size_t open_brace = source.find('{', cursor);
    if (open_brace == std::string_view::npos) {
      break;
    }

    const size_t close_brace = source.find('}', open_brace + 1);
    if (close_brace == std::string_view::npos) {
      break;
    }

    const std::vector<std::string> selectors = split_selector_list(source.substr(cursor, open_brace - cursor));
    const std::vector<Declaration> declarations =
        parse_declarations(source.substr(open_brace + 1, close_brace - open_brace - 1));

    for (const std::string& selector : selectors) {
      ParsedSelector parsed;
      if (!parse_selector(selector, &parsed)) {
        StyleWarning warning;
        warning.message = "Unsupported selector skipped";
        warning.selector = selector;
        result.warnings.push_back(std::move(warning));
        continue;
      }

      Rule rule;
      rule.selector = selector;
      rule.declarations = declarations;
      rule.specificity = compute_specificity(rule.selector);
      result.stylesheet.rules.push_back(std::move(rule));
    }

    cursor = close_brace + 1;
  }

  return result;
}

std::map<std::string, std::string> compute_style_for_node(
    const browser::html::Node& node,
    const Stylesheet& stylesheet,
    std::vector<StyleWarning>& warnings) {
  if (node.type != browser::html::NodeType::Element) {
    return {};
  }

  struct Winner {
    int specificity = -1;
    size_t source_order = 0;
    std::string value;
  };

  std::map<std::string, Winner> winners;
  size_t source_order = 0;

  for (const auto& rule : stylesheet.rules) {
    ParsedSelector parsed;
    if (!parse_selector(rule.selector, &parsed)) {
      StyleWarning warning;
      warning.message = "Selector match failed (unsupported syntax)";
      warning.selector = rule.selector;
      warnings.push_back(std::move(warning));
      continue;
    }

    if (!parsed_selector_matches_node(parsed, node)) {
      continue;
    }

    for (const auto& declaration : rule.declarations) {
      if (declaration.property.empty()) {
        continue;
      }

      const auto it = winners.find(declaration.property);
      const bool should_override =
          it == winners.end() ||
          rule.specificity > it->second.specificity ||
          (rule.specificity == it->second.specificity && source_order >= it->second.source_order);

      if (should_override) {
        winners[declaration.property] = Winner{rule.specificity, source_order, declaration.value};
      }
      ++source_order;
    }
  }

  const auto inline_style = parse_inline_style(get_attribute_value(node, "style"));
  for (const auto& [property, value] : inline_style) {
    winners[property] = Winner{kInlineSpecificity, source_order, value};
    ++source_order;
  }

  std::map<std::string, std::string> computed;
  for (const auto& [property, winner] : winners) {
    computed[property] = winner.value;
  }

  return computed;
}

namespace {

void collect_linked_css_refs(const browser::html::Node& node,
                              std::vector<LinkedCssRef>& refs) {
    if (node.type == browser::html::NodeType::Element) {
        if (node.tag_name == "link") {
            auto rel_it = node.attributes.find("rel");
            auto href_it = node.attributes.find("href");
            if (rel_it != node.attributes.end() &&
                href_it != node.attributes.end()) {
                std::string rel_lower;
                for (char c : rel_it->second) {
                    rel_lower += static_cast<char>(
                        std::tolower(static_cast<unsigned char>(c)));
                }
                if (rel_lower == "stylesheet") {
                    refs.push_back({href_it->second, "link"});
                }
            }
        } else if (node.tag_name == "style") {
            // Inline <style> blocks — collect text content
            for (const auto& child : node.children) {
                if (child && child->type == browser::html::NodeType::Text &&
                    !child->text_content.empty()) {
                    refs.push_back({child->text_content, "style"});
                }
            }
        }
    }

    for (const auto& child : node.children) {
        if (child) {
            collect_linked_css_refs(*child, refs);
        }
    }
}

}  // namespace

std::vector<LinkedCssRef> extract_linked_css(const browser::html::Node& document) {
    std::vector<LinkedCssRef> refs;
    collect_linked_css_refs(document, refs);
    return refs;
}

LinkedCssLoadResult load_linked_css(const browser::html::Node& document,
                                     const std::string& inline_css) {
    LinkedCssLoadResult result;

    // Extract @import URLs from the caller-supplied inline CSS first
    std::vector<std::string> import_urls;
    std::string combined_css = strip_css_imports(inline_css, &import_urls);

    auto refs = extract_linked_css(document);
    for (const auto& ref : refs) {
        if (ref.tag == "style") {
            // Inline style block content: strip @import rules and collect their URLs
            std::vector<std::string> block_imports;
            std::string stripped_block = strip_css_imports(ref.href, &block_imports);
            for (auto& url : block_imports) {
                import_urls.push_back(std::move(url));
            }
            if (!combined_css.empty()) {
                combined_css += "\n";
            }
            combined_css += stripped_block;
            result.loaded_urls.push_back("<style>");
        } else {
            // External link — in this implementation, we note it as a
            // reference that would need fetching. Since we don't have
            // a fetch context here, we record it as a failed load with
            // a deterministic fallback warning.
            result.warnings.push_back(
                "Linked CSS not loaded (no fetch context): " + ref.href);
            result.failed_urls.push_back(ref.href);
        }
    }

    // Record @import URLs as failed loads (no fetch context in this layer)
    for (const auto& url : import_urls) {
        result.warnings.push_back(
            "CSS @import not loaded (no fetch context): " + url);
        result.failed_urls.push_back(url);
    }

    result.merged = parse_css(combined_css);
    return result;
}

}  // namespace browser::css
