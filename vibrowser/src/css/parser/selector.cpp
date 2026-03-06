#include <clever/css/parser/selector.h>
#include <clever/css/parser/tokenizer.h>
#include <algorithm>
#include <cctype>

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

const SelectorList* cached_function_selector_list(const SimpleSelector& selector) {
    if (selector.parsed_selector_list) {
        return selector.parsed_selector_list.get();
    }
    return nullptr;
}

bool should_cache_function_selector_list(std::string_view pseudo_name) {
    const std::string pseudo = ascii_lower(std::string(pseudo_name));
    return pseudo == "is" || pseudo == "where" || pseudo == "not" || pseudo == "has" ||
           pseudo == "matches" || pseudo == "-webkit-any";
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

} // namespace

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
    Specificity spec;
    for (auto& part : selector.parts) {
        for (auto& ss : part.compound.simple_selectors) {
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
                            const SelectorList* inner_list = cached_function_selector_list(ss);
                            SelectorList parsed_list;
                            if (!inner_list) {
                                parsed_list = parse_selector_list(ss.argument);
                                inner_list = &parsed_list;
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
                    if (should_cache_function_selector_list(ss.value)) {
                        ss.parsed_selector_list =
                            std::make_shared<SelectorList>(parse_selector_list(ss.argument));
                    }
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
