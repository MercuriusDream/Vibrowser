#include <clever/css/parser/selector.h>
#include <clever/css/parser/tokenizer.h>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace clever::css {

namespace {

std::string ascii_lower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool should_cache_function_selector_list(std::string_view pseudo_name) {
    const std::string pseudo = ascii_lower(std::string(pseudo_name));
    return pseudo == "is" || pseudo == "where" || pseudo == "not" || pseudo == "has" ||
           pseudo == "matches" || pseudo == "-webkit-any";
}

std::string normalized_function_selector_cache_argument(std::string_view argument) {
    const auto tokens = CSSTokenizer::tokenize_all(argument);
    std::string normalized;
    normalized.reserve(argument.size());

    bool previous_was_colon = false;
    for (const auto& token : tokens) {
        if (token.type == CSSToken::EndOfFile) {
            break;
        }

        switch (token.type) {
            case CSSToken::Colon:
                normalized += ':';
                previous_was_colon = true;
                break;
            case CSSToken::Function:
                if (previous_was_colon && should_cache_function_selector_list(token.value)) {
                    normalized += ascii_lower(token.value);
                } else {
                    normalized += token.value;
                }
                normalized += '(';
                previous_was_colon = false;
                break;
            case CSSToken::Hash:
                normalized += '#';
                normalized += token.value;
                previous_was_colon = false;
                break;
            case CSSToken::Whitespace:
                normalized += ' ';
                previous_was_colon = false;
                break;
            default:
                normalized += token.value;
                previous_was_colon = false;
                break;
        }
    }

    return normalized;
}

struct FunctionSelectorListCacheEntry {
    std::shared_ptr<const SelectorList> list;
};

RightmostSelectorKey compute_rightmost_match_key(const ComplexSelector& selector);
std::optional<std::string> safe_unambiguous_required_class_key(const ComplexSelector& selector);
std::optional<std::string> safe_terminal_function_selector_class_key(const SimpleSelector& simple);

std::unordered_map<std::string, FunctionSelectorListCacheEntry>& function_selector_list_cache() {
    static std::unordered_map<std::string, FunctionSelectorListCacheEntry> cache;
    return cache;
}

std::string build_compiled_function_selector_list_cache_key(std::string_view pseudo_name,
                                                            std::string_view argument) {
    if (!should_cache_function_selector_list(pseudo_name)) {
        return {};
    }

    std::string cache_key = ascii_lower(std::string(pseudo_name));
    cache_key += '\n';
    cache_key += normalized_function_selector_cache_argument(argument);
    return cache_key;
}

FunctionSelectorListCacheEntry& function_selector_list_cache_entry(std::string_view pseudo_name,
                                                                   std::string_view argument) {
    const std::string cache_key =
        build_compiled_function_selector_list_cache_key(pseudo_name, argument);

    auto& cache = function_selector_list_cache();
    auto it = cache.find(cache_key);
    if (it != cache.end() && it->second.list) {
        return it->second;
    }

    auto compiled_list = std::make_shared<SelectorList>(parse_selector_list(argument));
    auto& entry = cache[cache_key];
    if (!entry.list) {
        entry.list = std::move(compiled_list);
    }
    return entry;
}

Specificity max_specificity_in_list(const SelectorList& list) {
    Specificity max_spec;
    bool has_value = false;
    for (const auto& selector : list.selectors) {
        Specificity current = compute_specificity(selector);
        if (!has_value || max_spec < current) {
            max_spec = current;
            has_value = true;
        }
    }
    return max_spec;
}

bool can_bucket_attribute_selector_by_name(const SimpleSelector& simple) {
    if (simple.type != SimpleSelectorType::Attribute || simple.attr_name.empty()) {
        return false;
    }

    // Name-only bucketing is safe only when the attribute must still exist
    // before any operator or case-flag-specific value match can succeed.
    switch (simple.attr_match) {
        case AttributeMatch::Exists:
        case AttributeMatch::Exact:
        case AttributeMatch::Includes:
        case AttributeMatch::DashMatch:
        case AttributeMatch::Prefix:
        case AttributeMatch::Suffix:
        case AttributeMatch::Substring:
            return true;
    }

    return false;
}

RightmostSelectorKey safe_terminal_function_selector_key(const SimpleSelector& simple) {
    if (simple.type != SimpleSelectorType::PseudoClass) {
        return {};
    }

    const std::string pseudo = ascii_lower(simple.value);
    if (pseudo != "is" && pseudo != "where") {
        return {};
    }

    const SelectorList* inner_list = function_selector_list_program(simple);
    if (!inner_list || inner_list->selectors.empty()) {
        return {};
    }

    RightmostSelectorKey shared_key;
    bool has_shared_key = false;
    for (const auto& branch : inner_list->selectors) {
        const RightmostSelectorKey branch_key = compute_rightmost_match_key(branch);
        if (branch_key.type == RightmostSelectorKeyType::None || branch_key.value.empty()) {
            return {};
        }
        if (!has_shared_key) {
            shared_key = branch_key;
            has_shared_key = true;
            continue;
        }
        if (branch_key.type != shared_key.type || branch_key.value != shared_key.value) {
            return {};
        }
    }

    return shared_key;
}

bool merge_unambiguous_class_key(std::optional<std::string>& class_key,
                                 std::string_view candidate) {
    if (candidate.empty()) {
        return true;
    }
    if (!class_key.has_value()) {
        class_key = std::string(candidate);
        return true;
    }
    return *class_key == candidate;
}

bool can_infer_required_class_from_function_selector(std::string_view pseudo_name) {
    const std::string pseudo = ascii_lower(std::string(pseudo_name));
    return pseudo == "is" || pseudo == "where" || pseudo == "matches" ||
           pseudo == "-webkit-any";
}

std::optional<std::string> safe_unambiguous_required_class_key(const ComplexSelector& selector) {
    if (selector.parts.empty()) {
        return std::nullopt;
    }

    std::optional<std::string> class_key;
    const auto& simple_selectors = selector.parts.back().compound.simple_selectors;
    for (const auto& simple : simple_selectors) {
        if (simple.type != SimpleSelectorType::Class) {
            continue;
        }
        if (!merge_unambiguous_class_key(class_key, simple.value)) {
            return std::nullopt;
        }
    }

    for (const auto& simple : simple_selectors) {
        const auto nested_class_key = safe_terminal_function_selector_class_key(simple);
        if (!nested_class_key.has_value()) {
            continue;
        }
        if (!merge_unambiguous_class_key(class_key, *nested_class_key)) {
            return std::nullopt;
        }
    }

    return class_key;
}

std::optional<std::string> safe_terminal_function_selector_class_key(const SimpleSelector& simple) {
    if (simple.type != SimpleSelectorType::PseudoClass ||
        !can_infer_required_class_from_function_selector(simple.value)) {
        return std::nullopt;
    }

    const SelectorList* inner_list = function_selector_list_program(simple);
    if (!inner_list || inner_list->selectors.empty()) {
        return std::nullopt;
    }

    std::optional<std::string> shared_class_key;
    for (const auto& branch : inner_list->selectors) {
        const auto branch_class_key = safe_unambiguous_required_class_key(branch);
        if (!branch_class_key.has_value()) {
            return std::nullopt;
        }
        if (!merge_unambiguous_class_key(shared_class_key, *branch_class_key)) {
            return std::nullopt;
        }
    }

    return shared_class_key;
}

RightmostSelectorKey compute_rightmost_match_key(const ComplexSelector& selector) {
    if (selector.parts.empty()) {
        return {};
    }

    const auto& simple_selectors = selector.parts.back().compound.simple_selectors;
    for (const auto& simple : simple_selectors) {
        if (simple.type == SimpleSelectorType::Id) {
            return {RightmostSelectorKeyType::Id, simple.value};
        }
    }
    for (const auto& simple : simple_selectors) {
        if (simple.type == SimpleSelectorType::Class) {
            return {RightmostSelectorKeyType::Class, simple.value};
        }
    }
    for (const auto& simple : simple_selectors) {
        if (simple.type == SimpleSelectorType::Type) {
            return {RightmostSelectorKeyType::Type, simple.value};
        }
    }

    for (const auto& simple : simple_selectors) {
        if (can_bucket_attribute_selector_by_name(simple)) {
            return {RightmostSelectorKeyType::Attribute,
                    ascii_lower(simple.attr_name)};
        }
    }

    for (const auto& simple : simple_selectors) {
        const RightmostSelectorKey function_key = safe_terminal_function_selector_key(simple);
        if (function_key.type != RightmostSelectorKeyType::None &&
            !function_key.value.empty()) {
            return function_key;
        }
    }

    for (const auto& simple : simple_selectors) {
        const auto class_key = safe_terminal_function_selector_class_key(simple);
        if (class_key.has_value() && !class_key->empty()) {
            return {RightmostSelectorKeyType::Class, *class_key};
        }
    }

    return {};
}

} // namespace

std::string compiled_function_selector_list_cache_key(std::string_view pseudo_name,
                                                      std::string_view argument) {
    return build_compiled_function_selector_list_cache_key(pseudo_name, argument);
}

std::shared_ptr<const SelectorList> compiled_function_selector_list_for_key(std::string_view cache_key) {
    if (cache_key.empty()) {
        return {};
    }

    auto& cache = function_selector_list_cache();
    auto it = cache.find(std::string(cache_key));
    if (it == cache.end()) {
        return {};
    }
    return it->second.list;
}

std::shared_ptr<const SelectorList> compile_function_selector_list(std::string_view pseudo_name,
                                                                   std::string_view argument) {
    if (compiled_function_selector_list_cache_key(pseudo_name, argument).empty()) {
        return {};
    }
    return function_selector_list_cache_entry(pseudo_name, argument).list;
}

std::shared_ptr<const SelectorList> attach_compiled_function_selector_list(SimpleSelector& selector) {
    if (selector.parsed_selector_list) {
        return selector.parsed_selector_list;
    }

    if (selector.compiled_selector_list_cache_key.empty()) {
        selector.compiled_selector_list_cache_key =
            compiled_function_selector_list_cache_key(selector.value, selector.argument);
    }
    if (selector.compiled_selector_list_cache_key.empty()) {
        return {};
    }

    selector.parsed_selector_list =
        compiled_function_selector_list_for_key(selector.compiled_selector_list_cache_key);
    if (!selector.parsed_selector_list) {
        selector.parsed_selector_list =
            compile_function_selector_list(selector.value, selector.argument);
    }
    return selector.parsed_selector_list;
}

const SelectorList* function_selector_list_program(const SimpleSelector& selector) {
    if (selector.parsed_selector_list) {
        return selector.parsed_selector_list.get();
    }

    const std::string cache_key = selector.compiled_selector_list_cache_key.empty()
                                      ? compiled_function_selector_list_cache_key(
                                            selector.value,
                                            selector.argument)
                                      : selector.compiled_selector_list_cache_key;
    if (cache_key.empty()) {
        return nullptr;
    }

    if (auto compiled_list = compiled_function_selector_list_for_key(cache_key)) {
        return compiled_list.get();
    }

    if (auto compiled_list = compile_function_selector_list(selector.value, selector.argument)) {
        return compiled_list.get();
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Specificity
// ---------------------------------------------------------------------------

bool Specificity::operator<(const Specificity& other) const {
    if (a != other.a) return a < other.a;
    if (b != other.b) return b < other.b;
    return c < other.c;
}

bool Specificity::operator==(const Specificity& other) const {
    return a == other.a && b == other.b && c == other.c;
}

Specificity compute_specificity(const ComplexSelector& selector) {
    if (selector.precomputed_specificity.has_value()) {
        return *selector.precomputed_specificity;
    }

    Specificity spec;
    for (const auto& part : selector.parts) {
        for (const auto& ss : part.compound.simple_selectors) {
            switch (ss.type) {
                case SimpleSelectorType::Id:
                    spec.a++;
                    break;
                case SimpleSelectorType::Class:
                case SimpleSelectorType::Attribute:
                    spec.b++;
                    break;
                case SimpleSelectorType::PseudoClass:
                    // :where() contributes 0.
                    // :is()/:not() and aliases contribute their argument specificity.
                    {
                        const std::string pseudo = ascii_lower(ss.value);
                        if (pseudo == "where") {
                            break;
                        }
                        if (pseudo == "is" || pseudo == "not" || pseudo == "has" ||
                            pseudo == "matches" || pseudo == "-webkit-any") {
                            const SelectorList* inner_list = function_selector_list_program(ss);
                            if (!inner_list) {
                                break;
                            }
                            Specificity inner = max_specificity_in_list(*inner_list);
                            spec.a += inner.a;
                            spec.b += inner.b;
                            spec.c += inner.c;
                            break;
                        }
                        spec.b++;
                    }
                    break;
                case SimpleSelectorType::Type:
                case SimpleSelectorType::PseudoElement:
                    spec.c++;
                    break;
                case SimpleSelectorType::Universal:
                    // Universal selector does not contribute to specificity
                    break;
            }
        }
    }
    return spec;
}

// ---------------------------------------------------------------------------
// Selector Parser
// ---------------------------------------------------------------------------

// Internal helper class for parsing selectors from a token stream
class SelectorParser {
public:
    explicit SelectorParser(std::vector<CSSToken> tokens)
        : tokens_(std::move(tokens)), pos_(0) {}

    SelectorList parse();

private:
    std::vector<CSSToken> tokens_;
    size_t pos_;

    const CSSToken& current() const;
    const CSSToken& peek_token() const;
    bool at_end() const;
    void advance();
    void skip_whitespace();

    ComplexSelector parse_complex_selector();
    CompoundSelector parse_compound_selector();
    SimpleSelector parse_simple_selector();
    SimpleSelector parse_attribute_selector();
    std::optional<Combinator> try_parse_combinator();
};

const CSSToken& SelectorParser::current() const {
    if (pos_ < tokens_.size()) {
        return tokens_[pos_];
    }
    static CSSToken eof{CSSToken::EndOfFile, "", 0, "", false};
    return eof;
}

const CSSToken& SelectorParser::peek_token() const {
    return current();
}

bool SelectorParser::at_end() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == CSSToken::EndOfFile;
}

void SelectorParser::advance() {
    if (pos_ < tokens_.size()) {
        ++pos_;
    }
}

void SelectorParser::skip_whitespace() {
    while (!at_end() && current().type == CSSToken::Whitespace) {
        advance();
    }
}

SelectorList SelectorParser::parse() {
    SelectorList list;
    skip_whitespace();

    if (at_end()) {
        return list;
    }

    ComplexSelector first = parse_complex_selector();
    if (!first.parts.empty()) {
        first.precomputed_specificity = compute_specificity(first);
        list.selectors.push_back(std::move(first));
    }

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;
        if (current().type == CSSToken::Comma) {
                advance(); // skip comma
                skip_whitespace();
                if (!at_end()) {
                    ComplexSelector next = parse_complex_selector();
                    if (!next.parts.empty()) {
                        next.precomputed_specificity = compute_specificity(next);
                        list.selectors.push_back(std::move(next));
                    }
                }
            } else {
                break;
        }
    }

    return list;
}

ComplexSelector SelectorParser::parse_complex_selector() {
    ComplexSelector result;

    // Parse the first compound selector
    skip_whitespace();
    auto leading_combinator = try_parse_combinator();

    CompoundSelector first = parse_compound_selector();
    if (first.simple_selectors.empty()) {
        return result;
    }
    ComplexSelector::Part first_part;
    first_part.compound = std::move(first);
    first_part.combinator = leading_combinator;
    result.parts.push_back(std::move(first_part));

    while (!at_end()) {
        // Check for combinator or end
        auto maybe_comb = try_parse_combinator();
        if (!maybe_comb.has_value()) {
            break;
        }

        skip_whitespace();
        if (at_end() || current().type == CSSToken::Comma) {
            result.parts.clear();
            break;
        }

        CompoundSelector next = parse_compound_selector();
        if (next.simple_selectors.empty()) {
            result.parts.clear();
            break;
        }
        ComplexSelector::Part part;
        part.compound = std::move(next);
        part.combinator = maybe_comb;
        result.parts.push_back(std::move(part));
    }

    result.rightmost_match_key = compute_rightmost_match_key(result);
    return result;
}

std::optional<Combinator> SelectorParser::try_parse_combinator() {
    // Save position
    size_t saved = pos_;

    bool had_whitespace = false;
    if (!at_end() && current().type == CSSToken::Whitespace) {
        had_whitespace = true;
        skip_whitespace();
    }

    if (at_end() || current().type == CSSToken::Comma ||
        current().type == CSSToken::LeftBrace) {
        pos_ = saved;
        return std::nullopt;
    }

    // Check for explicit combinators: >, +, ~
    if (current().type == CSSToken::Delim) {
        if (current().value == ">") {
            advance();
            skip_whitespace();
            return Combinator::Child;
        }
        if (current().value == "+") {
            advance();
            skip_whitespace();
            return Combinator::NextSibling;
        }
        if (current().value == "~") {
            advance();
            skip_whitespace();
            return Combinator::SubsequentSibling;
        }
    }

    // If there was whitespace and the next token starts a compound selector,
    // it's a descendant combinator
    if (had_whitespace) {
        // Check if the next token could start a compound selector
        auto& tok = current();
        if (tok.type == CSSToken::Ident || tok.type == CSSToken::Hash ||
            tok.type == CSSToken::LeftBracket || tok.type == CSSToken::Colon ||
            (tok.type == CSSToken::Delim &&
             (tok.value == "." || tok.value == "*"))) {
            return Combinator::Descendant;
        }
    }

    pos_ = saved;
    return std::nullopt;
}

CompoundSelector SelectorParser::parse_compound_selector() {
    CompoundSelector compound;

    while (!at_end()) {
        auto& tok = current();

        // Type selector or universal selector
        if (tok.type == CSSToken::Ident) {
            SimpleSelector ss;
            ss.type = SimpleSelectorType::Type;
            ss.value = tok.value;
            compound.simple_selectors.push_back(std::move(ss));
            advance();
            continue;
        }

        // Universal selector
        if (tok.type == CSSToken::Delim && tok.value == "*") {
            SimpleSelector ss;
            ss.type = SimpleSelectorType::Universal;
            ss.value = "*";
            compound.simple_selectors.push_back(std::move(ss));
            advance();
            continue;
        }

        // Class selector: .name
        if (tok.type == CSSToken::Delim && tok.value == ".") {
            advance();
            if (!at_end() && current().type == CSSToken::Ident) {
                SimpleSelector ss;
                ss.type = SimpleSelectorType::Class;
                ss.value = current().value;
                compound.simple_selectors.push_back(std::move(ss));
                advance();
            }
            continue;
        }

        // ID selector: #name (Hash token)
        if (tok.type == CSSToken::Hash) {
            SimpleSelector ss;
            ss.type = SimpleSelectorType::Id;
            ss.value = tok.value;
            compound.simple_selectors.push_back(std::move(ss));
            advance();
            continue;
        }

        // Attribute selector: [...]
        if (tok.type == CSSToken::LeftBracket) {
            compound.simple_selectors.push_back(parse_attribute_selector());
            continue;
        }

        // Pseudo-element: ::name or pseudo-class: :name
        if (tok.type == CSSToken::Colon) {
            advance();
            if (!at_end() && current().type == CSSToken::Colon) {
                // Pseudo-element ::
                advance();
                if (!at_end() && current().type == CSSToken::Ident) {
                    SimpleSelector ss;
                    ss.type = SimpleSelectorType::PseudoElement;
                    ss.value = current().value;
                    compound.simple_selectors.push_back(std::move(ss));
                    advance();
                }
            } else if (!at_end() && (current().type == CSSToken::Ident ||
                                      current().type == CSSToken::Function)) {
                // Check for single-colon pseudo-element legacy syntax
                // :before and :after should be treated as pseudo-elements
                if (current().type == CSSToken::Ident &&
                    (current().value == "before" || current().value == "after")) {
                    SimpleSelector ss;
                    ss.type = SimpleSelectorType::PseudoElement;
                    ss.value = current().value;
                    compound.simple_selectors.push_back(std::move(ss));
                    advance();
                } else if (current().type == CSSToken::Function) {
                    SimpleSelector ss;
                    ss.type = SimpleSelectorType::PseudoClass;
                    ss.value = current().value;
                    advance();
                    // Consume function arguments until ')'
                    std::string args;
                    int depth = 1;
                    while (!at_end() && depth > 0) {
                        if (current().type == CSSToken::LeftParen) {
                            depth++;
                            args += "(";
                        } else if (current().type == CSSToken::RightParen) {
                            depth--;
                            if (depth > 0) args += ")";
                        } else if (current().type == CSSToken::Function) {
                            // Function token value is name without '('
                            depth++;
                            args += current().value + "(";
                        } else if (current().type == CSSToken::Hash) {
                            args += "#" + current().value;
                        } else {
                            args += current().value;
                        }
                        advance();
                    }
                    ss.argument = args;
                    attach_compiled_function_selector_list(ss);
                    compound.simple_selectors.push_back(std::move(ss));
                } else {
                    SimpleSelector ss;
                    ss.type = SimpleSelectorType::PseudoClass;
                    ss.value = current().value;
                    compound.simple_selectors.push_back(std::move(ss));
                    advance();
                }
            }
            continue;
        }

        // Can't parse more simple selectors
        break;
    }

    return compound;
}

SimpleSelector SelectorParser::parse_attribute_selector() {
    SimpleSelector ss;
    ss.type = SimpleSelectorType::Attribute;

    advance(); // skip '['
    skip_whitespace();

    // Attribute name
    if (!at_end() && current().type == CSSToken::Ident) {
        ss.attr_name = current().value;
        advance();
    }

    skip_whitespace();

    // Check for match operator or just ']'
    if (!at_end() && current().type == CSSToken::RightBracket) {
        ss.attr_match = AttributeMatch::Exists;
        advance(); // skip ']'
        return ss;
    }

    // Parse match operator
    if (!at_end()) {
        // Check for two-character operators like ~=, |=, ^=, $=, *=
        if (current().type == CSSToken::Delim) {
            std::string op = current().value;
            if (op == "~" || op == "|" || op == "^" || op == "$" || op == "*") {
                advance();
                // Expect '='
                if (!at_end() && current().type == CSSToken::Delim &&
                    current().value == "=") {
                    advance();
                    if (op == "~") ss.attr_match = AttributeMatch::Includes;
                    else if (op == "|") ss.attr_match = AttributeMatch::DashMatch;
                    else if (op == "^") ss.attr_match = AttributeMatch::Prefix;
                    else if (op == "$") ss.attr_match = AttributeMatch::Suffix;
                    else if (op == "*") ss.attr_match = AttributeMatch::Substring;
                }
            } else if (op == "=") {
                ss.attr_match = AttributeMatch::Exact;
                advance();
            }
        }
    }

    skip_whitespace();

    // Parse attribute value (string, ident, or number-like token)
    if (!at_end()) {
        if (current().type == CSSToken::String) {
            ss.attr_value = current().value;
            advance();
        } else if (current().type == CSSToken::Ident) {
            ss.attr_value = current().value;
            advance();
        } else if (current().type == CSSToken::Number ||
                   current().type == CSSToken::Dimension ||
                   current().type == CSSToken::Percentage) {
            ss.attr_value = current().value;
            advance();
        }
    }

    skip_whitespace();

    if (!at_end() && current().type == CSSToken::Ident) {
        const std::string flag = ascii_lower(current().value);
        if (flag == "i" || flag == "s") {
            ss.argument = flag;
            advance();
            skip_whitespace();
        }
    }

    // Skip to closing ']'
    while (!at_end() && current().type != CSSToken::RightBracket) {
        advance();
    }
    if (!at_end()) {
        advance(); // skip ']'
    }

    return ss;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

SelectorList parse_selector_list(std::string_view input) {
    auto tokens = CSSTokenizer::tokenize_all(input);
    SelectorParser parser(std::move(tokens));
    return parser.parse();
}

} // namespace clever::css
