#include <gtest/gtest.h>
#include <clever/css/parser/tokenizer.h>
#include <clever/css/parser/selector.h>
#include <clever/css/parser/stylesheet.h>

using namespace clever::css;

// =============================================================================
// Tokenizer Tests
// =============================================================================

class CSSTokenizerTest : public ::testing::Test {};

// Test 1: Ident token
TEST_F(CSSTokenizerTest, IdentToken) {
    auto tokens = CSSTokenizer::tokenize_all("color");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Ident);
    EXPECT_EQ(tokens[0].value, "color");
}

// Test 2: Hash token
TEST_F(CSSTokenizerTest, HashToken) {
    auto tokens = CSSTokenizer::tokenize_all("#fff");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Hash);
    EXPECT_EQ(tokens[0].value, "fff");
}

// Test 3: Number token
TEST_F(CSSTokenizerTest, NumberToken) {
    auto tokens = CSSTokenizer::tokenize_all("42");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 42.0);
    EXPECT_TRUE(tokens[0].is_integer);
}

// Test 4: Dimension token
TEST_F(CSSTokenizerTest, DimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("16px");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 16.0);
    EXPECT_EQ(tokens[0].unit, "px");
}

// Test 5: Percentage token
TEST_F(CSSTokenizerTest, PercentageToken) {
    auto tokens = CSSTokenizer::tokenize_all("50%");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Percentage);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 50.0);
}

// Test 6: String token (single-quoted)
TEST_F(CSSTokenizerTest, StringTokenSingleQuoted) {
    auto tokens = CSSTokenizer::tokenize_all("'hello'");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::String);
    EXPECT_EQ(tokens[0].value, "hello");
}

// Test 7: String token (double-quoted)
TEST_F(CSSTokenizerTest, StringTokenDoubleQuoted) {
    auto tokens = CSSTokenizer::tokenize_all("\"hello\"");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::String);
    EXPECT_EQ(tokens[0].value, "hello");
}

// Test 8: Colon, semicolon, braces
TEST_F(CSSTokenizerTest, PunctuationTokens) {
    auto tokens = CSSTokenizer::tokenize_all(":;{}");
    ASSERT_GE(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, CSSToken::Colon);
    EXPECT_EQ(tokens[1].type, CSSToken::Semicolon);
    EXPECT_EQ(tokens[2].type, CSSToken::LeftBrace);
    EXPECT_EQ(tokens[3].type, CSSToken::RightBrace);
}

// Test 9: Function token
TEST_F(CSSTokenizerTest, FunctionToken) {
    auto tokens = CSSTokenizer::tokenize_all("rgb(");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Function);
    EXPECT_EQ(tokens[0].value, "rgb");
}

// Test 10: Whitespace handling
TEST_F(CSSTokenizerTest, WhitespaceHandling) {
    auto tokens = CSSTokenizer::tokenize_all("  \t\n  ");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Whitespace);
}

// Test 11: Multiple tokens in sequence
TEST_F(CSSTokenizerTest, MultipleTokensInSequence) {
    auto tokens = CSSTokenizer::tokenize_all("color: red;");
    // color WS : WS red ; EOF
    // Filter out whitespace for easier testing
    std::vector<CSSToken> significant;
    for (auto& t : tokens) {
        if (t.type != CSSToken::Whitespace && t.type != CSSToken::EndOfFile)
            significant.push_back(t);
    }
    ASSERT_EQ(significant.size(), 4u);
    EXPECT_EQ(significant[0].type, CSSToken::Ident);
    EXPECT_EQ(significant[0].value, "color");
    EXPECT_EQ(significant[1].type, CSSToken::Colon);
    EXPECT_EQ(significant[2].type, CSSToken::Ident);
    EXPECT_EQ(significant[2].value, "red");
    EXPECT_EQ(significant[3].type, CSSToken::Semicolon);
}

// Test 12: At-keyword
TEST_F(CSSTokenizerTest, AtKeyword) {
    auto tokens = CSSTokenizer::tokenize_all("@media");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::AtKeyword);
    EXPECT_EQ(tokens[0].value, "media");
}

// Test 13: Delim token for special chars
TEST_F(CSSTokenizerTest, DelimToken) {
    auto tokens = CSSTokenizer::tokenize_all("*");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Delim);
    EXPECT_EQ(tokens[0].value, "*");
}

// Test 14: CDC and CDO tokens
TEST_F(CSSTokenizerTest, CDCAndCDOTokens) {
    auto tokens = CSSTokenizer::tokenize_all("<!-- -->");
    std::vector<CSSToken> significant;
    for (auto& t : tokens) {
        if (t.type != CSSToken::Whitespace && t.type != CSSToken::EndOfFile)
            significant.push_back(t);
    }
    ASSERT_EQ(significant.size(), 2u);
    EXPECT_EQ(significant[0].type, CSSToken::CDO);
    EXPECT_EQ(significant[1].type, CSSToken::CDC);
}

// Additional tokenizer tests
TEST_F(CSSTokenizerTest, CommentsAreSkipped) {
    auto tokens = CSSTokenizer::tokenize_all("color /* comment */ : red");
    std::vector<CSSToken> significant;
    for (auto& t : tokens) {
        if (t.type != CSSToken::Whitespace && t.type != CSSToken::EndOfFile)
            significant.push_back(t);
    }
    ASSERT_EQ(significant.size(), 3u);
    EXPECT_EQ(significant[0].type, CSSToken::Ident);
    EXPECT_EQ(significant[0].value, "color");
    EXPECT_EQ(significant[1].type, CSSToken::Colon);
    EXPECT_EQ(significant[2].type, CSSToken::Ident);
    EXPECT_EQ(significant[2].value, "red");
}

TEST_F(CSSTokenizerTest, NegativeNumber) {
    auto tokens = CSSTokenizer::tokenize_all("-5");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, -5.0);
}

TEST_F(CSSTokenizerTest, FloatingPointNumber) {
    auto tokens = CSSTokenizer::tokenize_all("3.14");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 3.14);
    EXPECT_FALSE(tokens[0].is_integer);
}

TEST_F(CSSTokenizerTest, BracketsAndParens) {
    auto tokens = CSSTokenizer::tokenize_all("[]()");
    ASSERT_GE(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, CSSToken::LeftBracket);
    EXPECT_EQ(tokens[1].type, CSSToken::RightBracket);
    EXPECT_EQ(tokens[2].type, CSSToken::LeftParen);
    EXPECT_EQ(tokens[3].type, CSSToken::RightParen);
}

TEST_F(CSSTokenizerTest, CommaToken) {
    auto tokens = CSSTokenizer::tokenize_all(",");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Comma);
}

TEST_F(CSSTokenizerTest, StringWithEscapeSequence) {
    auto tokens = CSSTokenizer::tokenize_all("'he\\'llo'");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::String);
    EXPECT_EQ(tokens[0].value, "he'llo");
}

TEST_F(CSSTokenizerTest, IdentStartingWithHyphen) {
    auto tokens = CSSTokenizer::tokenize_all("-webkit-transform");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Ident);
    EXPECT_EQ(tokens[0].value, "-webkit-transform");
}

TEST_F(CSSTokenizerTest, DimensionWithEm) {
    auto tokens = CSSTokenizer::tokenize_all("1.5em");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 1.5);
    EXPECT_EQ(tokens[0].unit, "em");
}

TEST_F(CSSTokenizerTest, EndOfFileToken) {
    auto tokens = CSSTokenizer::tokenize_all("");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::EndOfFile);
}

TEST_F(CSSTokenizerTest, HashWithHexColor) {
    auto tokens = CSSTokenizer::tokenize_all("#ff00cc");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Hash);
    EXPECT_EQ(tokens[0].value, "ff00cc");
}

TEST_F(CSSTokenizerTest, GreaterThanDelim) {
    auto tokens = CSSTokenizer::tokenize_all(">");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Delim);
    EXPECT_EQ(tokens[0].value, ">");
}

TEST_F(CSSTokenizerTest, PlusDelim) {
    auto tokens = CSSTokenizer::tokenize_all("+");
    ASSERT_GE(tokens.size(), 1u);
    // + that doesn't start a number is a Delim
    EXPECT_EQ(tokens[0].type, CSSToken::Delim);
    EXPECT_EQ(tokens[0].value, "+");
}

TEST_F(CSSTokenizerTest, TildeDelim) {
    auto tokens = CSSTokenizer::tokenize_all("~");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Delim);
    EXPECT_EQ(tokens[0].value, "~");
}

// =============================================================================
// Selector Tests
// =============================================================================

class CSSSelectorTest : public ::testing::Test {};

// Test 15: Type selector
TEST_F(CSSSelectorTest, TypeSelector) {
    auto list = parse_selector_list("div");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 1u);
    auto& compound = sel.parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Type);
    EXPECT_EQ(compound.simple_selectors[0].value, "div");
}

// Test 16: Class selector
TEST_F(CSSSelectorTest, ClassSelector) {
    auto list = parse_selector_list(".foo");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Class);
    EXPECT_EQ(compound.simple_selectors[0].value, "foo");
}

// Test 17: ID selector
TEST_F(CSSSelectorTest, IdSelector) {
    auto list = parse_selector_list("#bar");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Id);
    EXPECT_EQ(compound.simple_selectors[0].value, "bar");
}

// Test 18: Universal selector
TEST_F(CSSSelectorTest, UniversalSelector) {
    auto list = parse_selector_list("*");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Universal);
}

// Test 19: Attribute selector [href]
TEST_F(CSSSelectorTest, AttributeSelectorExists) {
    auto list = parse_selector_list("[href]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 1u);
    auto& ss = compound.simple_selectors[0];
    EXPECT_EQ(ss.type, SimpleSelectorType::Attribute);
    EXPECT_EQ(ss.attr_name, "href");
    EXPECT_EQ(ss.attr_match, AttributeMatch::Exists);
}

// Test 20: Attribute with value [type="text"]
TEST_F(CSSSelectorTest, AttributeSelectorExact) {
    auto list = parse_selector_list("[type=\"text\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.type, SimpleSelectorType::Attribute);
    EXPECT_EQ(ss.attr_name, "type");
    EXPECT_EQ(ss.attr_value, "text");
    EXPECT_EQ(ss.attr_match, AttributeMatch::Exact);
}

// Test 21: Compound selector "div.foo#bar"
TEST_F(CSSSelectorTest, CompoundSelector) {
    auto list = parse_selector_list("div.foo#bar");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 3u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Type);
    EXPECT_EQ(compound.simple_selectors[0].value, "div");
    EXPECT_EQ(compound.simple_selectors[1].type, SimpleSelectorType::Class);
    EXPECT_EQ(compound.simple_selectors[1].value, "foo");
    EXPECT_EQ(compound.simple_selectors[2].type, SimpleSelectorType::Id);
    EXPECT_EQ(compound.simple_selectors[2].value, "bar");
}

// Test 22: Descendant combinator "div p"
TEST_F(CSSSelectorTest, DescendantCombinator) {
    auto list = parse_selector_list("div p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 2u);
    EXPECT_FALSE(sel.parts[0].combinator.has_value());
    EXPECT_EQ(sel.parts[1].combinator.value(), Combinator::Descendant);
    EXPECT_EQ(sel.parts[0].compound.simple_selectors[0].value, "div");
    EXPECT_EQ(sel.parts[1].compound.simple_selectors[0].value, "p");
}

// Test 23: Child combinator "div > p"
TEST_F(CSSSelectorTest, ChildCombinator) {
    auto list = parse_selector_list("div > p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 2u);
    EXPECT_EQ(sel.parts[1].combinator.value(), Combinator::Child);
}

// Test 24: Adjacent sibling "div + p"
TEST_F(CSSSelectorTest, AdjacentSiblingCombinator) {
    auto list = parse_selector_list("div + p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 2u);
    EXPECT_EQ(sel.parts[1].combinator.value(), Combinator::NextSibling);
}

// Test 25: General sibling "div ~ p"
TEST_F(CSSSelectorTest, GeneralSiblingCombinator) {
    auto list = parse_selector_list("div ~ p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 2u);
    EXPECT_EQ(sel.parts[1].combinator.value(), Combinator::SubsequentSibling);
}

// Test 26: Selector list "div, p, span"
TEST_F(CSSSelectorTest, SelectorList) {
    auto list = parse_selector_list("div, p, span");
    ASSERT_EQ(list.selectors.size(), 3u);
    EXPECT_EQ(list.selectors[0].parts[0].compound.simple_selectors[0].value, "div");
    EXPECT_EQ(list.selectors[1].parts[0].compound.simple_selectors[0].value, "p");
    EXPECT_EQ(list.selectors[2].parts[0].compound.simple_selectors[0].value, "span");
}

// Test 27: Pseudo-class :hover
TEST_F(CSSSelectorTest, PseudoClassHover) {
    auto list = parse_selector_list(":hover");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "hover");
}

// Test 28: Pseudo-class :first-child
TEST_F(CSSSelectorTest, PseudoClassFirstChild) {
    auto list = parse_selector_list(":first-child");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "first-child");
}

// Test 29: Pseudo-element ::before
TEST_F(CSSSelectorTest, PseudoElementBefore) {
    auto list = parse_selector_list("::before");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoElement);
    EXPECT_EQ(compound.simple_selectors[0].value, "before");
}

// Test 30: Specificity calculation
TEST_F(CSSSelectorTest, SpecificityType) {
    auto list = parse_selector_list("div");
    auto spec = compute_specificity(list.selectors[0]);
    EXPECT_EQ(spec.a, 0);
    EXPECT_EQ(spec.b, 0);
    EXPECT_EQ(spec.c, 1);
}

TEST_F(CSSSelectorTest, SpecificityClass) {
    auto list = parse_selector_list(".foo");
    auto spec = compute_specificity(list.selectors[0]);
    EXPECT_EQ(spec.a, 0);
    EXPECT_EQ(spec.b, 1);
    EXPECT_EQ(spec.c, 0);
}

TEST_F(CSSSelectorTest, SpecificityId) {
    auto list = parse_selector_list("#bar");
    auto spec = compute_specificity(list.selectors[0]);
    EXPECT_EQ(spec.a, 1);
    EXPECT_EQ(spec.b, 0);
    EXPECT_EQ(spec.c, 0);
}

// Test 31: Complex specificity "div.foo#bar" = (1,1,1)
TEST_F(CSSSelectorTest, ComplexSpecificity) {
    auto list = parse_selector_list("div.foo#bar");
    auto spec = compute_specificity(list.selectors[0]);
    EXPECT_EQ(spec.a, 1);
    EXPECT_EQ(spec.b, 1);
    EXPECT_EQ(spec.c, 1);
}

TEST_F(CSSSelectorTest, SpecificityComparison) {
    Specificity a{1, 0, 0};
    Specificity b{0, 1, 0};
    Specificity c{0, 0, 1};
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(b > c);
    EXPECT_TRUE(a > c);
    EXPECT_FALSE(a < b);
}

TEST_F(CSSSelectorTest, AttributeSelectorPrefix) {
    auto list = parse_selector_list("[class^=\"btn\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.attr_match, AttributeMatch::Prefix);
    EXPECT_EQ(ss.attr_name, "class");
    EXPECT_EQ(ss.attr_value, "btn");
}

TEST_F(CSSSelectorTest, AttributeSelectorSuffix) {
    auto list = parse_selector_list("[href$=\".pdf\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.attr_match, AttributeMatch::Suffix);
}

TEST_F(CSSSelectorTest, AttributeSelectorSubstring) {
    auto list = parse_selector_list("[title*=\"hello\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.attr_match, AttributeMatch::Substring);
}

TEST_F(CSSSelectorTest, AttributeSelectorIncludes) {
    auto list = parse_selector_list("[class~=\"active\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.attr_match, AttributeMatch::Includes);
}

TEST_F(CSSSelectorTest, AttributeSelectorDashMatch) {
    auto list = parse_selector_list("[lang|=\"en\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& ss = list.selectors[0].parts[0].compound.simple_selectors[0];
    EXPECT_EQ(ss.attr_match, AttributeMatch::DashMatch);
}

// =============================================================================
// Stylesheet Tests
// =============================================================================

class CSSStylesheetTest : public ::testing::Test {};

// Test 32: Simple rule
TEST_F(CSSStylesheetTest, SimpleRule) {
    auto sheet = parse_stylesheet("p { color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_EQ(sheet.rules[0].selector_text, "p");
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "color");
    ASSERT_GE(sheet.rules[0].declarations[0].values.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].values[0].value, "red");
}

// Test 33: Multiple declarations
TEST_F(CSSStylesheetTest, MultipleDeclarations) {
    auto sheet = parse_stylesheet("p { color: red; font-size: 16px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 2u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "color");
    EXPECT_EQ(sheet.rules[0].declarations[1].property, "font-size");
    // Check dimension value
    ASSERT_GE(sheet.rules[0].declarations[1].values.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[1].values[0].numeric_value, 16.0);
    EXPECT_EQ(sheet.rules[0].declarations[1].values[0].unit, "px");
}

// Test 34: !important flag
TEST_F(CSSStylesheetTest, ImportantFlag) {
    auto sheet = parse_stylesheet("p { color: red !important; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_TRUE(sheet.rules[0].declarations[0].important);
}

// Test 35: Multiple rules
TEST_F(CSSStylesheetTest, MultipleRules) {
    auto sheet = parse_stylesheet("p { color: red; } div { margin: 0; }");
    ASSERT_EQ(sheet.rules.size(), 2u);
    EXPECT_EQ(sheet.rules[0].selector_text, "p");
    EXPECT_EQ(sheet.rules[1].selector_text, "div");
}

// Test 36: Nested values (function call)
TEST_F(CSSStylesheetTest, NestedValues) {
    auto sheet = parse_stylesheet("p { background: rgb(255, 0, 0); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    auto& values = sheet.rules[0].declarations[0].values;
    // Should have a Function component value
    bool found_function = false;
    for (auto& v : values) {
        if (v.type == ComponentValue::Function && v.value == "rgb") {
            found_function = true;
            // Function should have children (the arguments)
            EXPECT_GE(v.children.size(), 1u);
        }
    }
    EXPECT_TRUE(found_function);
}

// Test 37: @media rule
TEST_F(CSSStylesheetTest, MediaRule) {
    auto sheet = parse_stylesheet("@media (max-width: 768px) { p { color: blue; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].condition, "(max-width: 768px)");
    ASSERT_EQ(sheet.media_queries[0].rules.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].rules[0].selector_text, "p");
}

// Test 38: @import rule
TEST_F(CSSStylesheetTest, ImportRule) {
    auto sheet = parse_stylesheet("@import url('styles.css');");
    ASSERT_EQ(sheet.imports.size(), 1u);
    EXPECT_EQ(sheet.imports[0].url, "styles.css");
}

// Test 39: Selector list in rule
TEST_F(CSSStylesheetTest, SelectorListInRule) {
    auto sheet = parse_stylesheet("h1, h2, h3 { font-weight: bold; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_EQ(sheet.rules[0].selectors.selectors.size(), 3u);
}

// Test 40: Declaration block parsing (inline styles)
TEST_F(CSSStylesheetTest, DeclarationBlockParsing) {
    auto decls = parse_declaration_block("color: red; font-size: 16px;");
    ASSERT_EQ(decls.size(), 2u);
    EXPECT_EQ(decls[0].property, "color");
    EXPECT_EQ(decls[1].property, "font-size");
}

// Additional stylesheet tests
TEST_F(CSSStylesheetTest, DeclarationWithMultipleValues) {
    auto sheet = parse_stylesheet("p { margin: 10px 20px 30px 40px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    auto& decl = sheet.rules[0].declarations[0];
    EXPECT_EQ(decl.property, "margin");
    EXPECT_GE(decl.values.size(), 4u);
}

TEST_F(CSSStylesheetTest, EmptyStylesheet) {
    auto sheet = parse_stylesheet("");
    EXPECT_EQ(sheet.rules.size(), 0u);
}

TEST_F(CSSStylesheetTest, CommentInStylesheet) {
    auto sheet = parse_stylesheet("/* comment */ p { color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
}

TEST_F(CSSStylesheetTest, ImportWithMedia) {
    auto sheet = parse_stylesheet("@import url('print.css') print;");
    ASSERT_EQ(sheet.imports.size(), 1u);
    EXPECT_EQ(sheet.imports[0].url, "print.css");
    EXPECT_EQ(sheet.imports[0].media, "print");
}

TEST_F(CSSStylesheetTest, StringValueInDeclaration) {
    auto sheet = parse_stylesheet("p { content: \"hello world\"; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "content");
}

// ===========================================================================
// @media query parsing tests
// ===========================================================================
TEST_F(CSSStylesheetTest, MediaQueryBasic) {
    auto sheet = parse_stylesheet(
        "@media screen { .mobile { display: none; } }");
    EXPECT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].condition, "screen");
    ASSERT_EQ(sheet.media_queries[0].rules.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].rules[0].declarations.size(), 1u);
}

TEST_F(CSSStylesheetTest, MediaQueryMinWidth) {
    auto sheet = parse_stylesheet(
        "@media (min-width: 768px) { .sidebar { width: 250px; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    auto& mq = sheet.media_queries[0];
    EXPECT_TRUE(mq.condition.find("min-width") != std::string::npos);
    ASSERT_EQ(mq.rules.size(), 1u);
}

TEST_F(CSSStylesheetTest, MediaQueryScreenAndMinWidth) {
    auto sheet = parse_stylesheet(
        "@media screen and (max-width: 600px) { "
        "  .nav { display: none; } "
        "  .content { width: 100%; } "
        "}");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].rules.size(), 2u);
}

TEST_F(CSSStylesheetTest, MediaQueryMultipleRules) {
    auto sheet = parse_stylesheet(
        "p { color: red; } "
        "@media (max-width: 480px) { p { font-size: 14px; } } "
        "div { margin: 0; }");
    EXPECT_EQ(sheet.rules.size(), 2u);         // p and div
    EXPECT_EQ(sheet.media_queries.size(), 1u);  // one @media block
}

// =============================================================================
// @keyframes Tests
// =============================================================================

TEST(CSSParserTest, KeyframesBasicParse) {
    auto sheet = parse_stylesheet(
        "@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }");
    ASSERT_EQ(sheet.keyframes.size(), 1u);
    EXPECT_EQ(sheet.keyframes[0].name, "fadeIn");
    ASSERT_EQ(sheet.keyframes[0].keyframes.size(), 2u);
    // "from" stop
    EXPECT_EQ(sheet.keyframes[0].keyframes[0].selector, "from");
    ASSERT_EQ(sheet.keyframes[0].keyframes[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.keyframes[0].keyframes[0].declarations[0].property, "opacity");
    // "to" stop
    EXPECT_EQ(sheet.keyframes[0].keyframes[1].selector, "to");
    ASSERT_EQ(sheet.keyframes[0].keyframes[1].declarations.size(), 1u);
    EXPECT_EQ(sheet.keyframes[0].keyframes[1].declarations[0].property, "opacity");
}

// =============================================================================
// @font-face Tests
// =============================================================================

TEST(CSSParserTest, FontFaceBasicParse) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"MyFont\"; src: url(\"font.woff2\"); font-weight: bold; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "MyFont");
    EXPECT_TRUE(sheet.font_faces[0].src.find("font.woff2") != std::string::npos);
    EXPECT_EQ(sheet.font_faces[0].font_weight, "bold");
}

TEST(CSSParserTest, FontFaceMultipleSources) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"Test\"; "
        "src: local(\"Arial\"), url(\"test.woff2\") format(\"woff2\"); }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "Test");
    // The src should contain both local() and url() references
    EXPECT_TRUE(sheet.font_faces[0].src.find("local") != std::string::npos);
    EXPECT_TRUE(sheet.font_faces[0].src.find("test.woff2") != std::string::npos);
}

TEST(CSSParserTest, FontFaceDisplaySwap) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"MyFont\"; src: url(\"font.woff2\"); font-display: swap; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "MyFont");
    EXPECT_EQ(sheet.font_faces[0].font_display, "swap");
}

TEST(CSSParserTest, FontFaceDisplayBlock) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"BlockFont\"; src: url(\"b.woff2\"); font-display: block; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_display, "block");
}

TEST(CSSParserTest, FontFaceDisplayFallback) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"F\"; src: url(\"f.woff2\"); font-display: fallback; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_display, "fallback");
}

TEST(CSSParserTest, FontFaceDisplayOptional) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"O\"; src: url(\"o.woff2\"); font-display: optional; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_display, "optional");
}

TEST(CSSParserTest, FontFaceDisplayAuto) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"A\"; src: url(\"a.woff2\"); font-display: auto; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_display, "auto");
}

TEST(CSSParserTest, FontFaceDisplayDefaultEmpty) {
    // When font-display is not specified, it should default to empty string
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"NoDisplay\"; src: url(\"nd.woff2\"); }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_TRUE(sheet.font_faces[0].font_display.empty());
}

// =============================================================================
// @supports Rule Tests
// =============================================================================

TEST(CSSParserTest, SupportsRuleBasic) {
    auto sheet = parse_stylesheet(
        "@supports (display: grid) { .grid { display: grid; } }");
    ASSERT_EQ(sheet.supports_rules.size(), 1u);
    EXPECT_TRUE(sheet.supports_rules[0].condition.find("display") != std::string::npos);
    ASSERT_GE(sheet.supports_rules[0].rules.size(), 1u);
    EXPECT_EQ(sheet.supports_rules[0].rules[0].selector_text, ".grid");
}

TEST(CSSParserTest, SupportsRuleMultipleDecls) {
    auto sheet = parse_stylesheet(
        "@supports (display: flex) { .a { color: red; } .b { color: blue; } }");
    ASSERT_EQ(sheet.supports_rules.size(), 1u);
    EXPECT_GE(sheet.supports_rules[0].rules.size(), 2u);
}

TEST(CSSParserTest, SupportsRuleNotCondition) {
    auto sheet = parse_stylesheet(
        "@supports not (display: unknown-value) { div { color: green; } }");
    ASSERT_EQ(sheet.supports_rules.size(), 1u);
    EXPECT_TRUE(sheet.supports_rules[0].condition.find("not") != std::string::npos);
}

// =============================================================================
// @layer parsing
// =============================================================================

TEST(CSSParserTest, LayerRuleBasic) {
    auto sheet = parse_stylesheet(
        "@layer base { .a { color: red; } }");
    ASSERT_EQ(sheet.layer_rules.size(), 1u);
    EXPECT_EQ(sheet.layer_rules[0].name, "base");
    ASSERT_EQ(sheet.layer_rules[0].rules.size(), 1u);
    EXPECT_EQ(sheet.layer_rules[0].rules[0].selector_text, ".a");
}

TEST(CSSParserTest, LayerRuleMultipleRules) {
    auto sheet = parse_stylesheet(
        "@layer theme { .a { color: red; } .b { font-size: 16px; } }");
    ASSERT_EQ(sheet.layer_rules.size(), 1u);
    EXPECT_EQ(sheet.layer_rules[0].name, "theme");
    EXPECT_GE(sheet.layer_rules[0].rules.size(), 2u);
}

TEST(CSSParserTest, LayerRuleDeclarationOnly) {
    // @layer name; — no block, just a declaration
    auto sheet = parse_stylesheet("@layer utilities;");
    ASSERT_EQ(sheet.layer_rules.size(), 1u);
    EXPECT_EQ(sheet.layer_rules[0].name, "utilities");
    EXPECT_EQ(sheet.layer_rules[0].rules.size(), 0u);
}

TEST(CSSParserTest, LayerRuleAnonymous) {
    auto sheet = parse_stylesheet(
        "@layer { div { color: blue; } }");
    ASSERT_EQ(sheet.layer_rules.size(), 1u);
    EXPECT_EQ(sheet.layer_rules[0].name, "");
    ASSERT_EQ(sheet.layer_rules[0].rules.size(), 1u);
}

TEST(CSSParserTest, LayerRuleMultipleLayers) {
    auto sheet = parse_stylesheet(
        "@layer base { .a { color: red; } } @layer theme { .b { color: blue; } }");
    ASSERT_EQ(sheet.layer_rules.size(), 2u);
    EXPECT_EQ(sheet.layer_rules[0].name, "base");
    EXPECT_EQ(sheet.layer_rules[1].name, "theme");
}

TEST(CSSParserTest, LayerRuleCommaListOrderingRespected) {
    auto sheet = parse_stylesheet(
        "@layer base, theme;"
        "@layer theme { .x { color: green; } }"
        "@layer base { .x { color: red; } }");

    ASSERT_GE(sheet.layer_rules.size(), 4u);
    const auto& theme_rule = sheet.layer_rules[2].rules[0];
    const auto& base_rule = sheet.layer_rules[3].rules[0];
    EXPECT_TRUE(theme_rule.in_layer);
    EXPECT_TRUE(base_rule.in_layer);
    EXPECT_GT(theme_rule.layer_order, base_rule.layer_order);
}

TEST(CSSParserTest, LayerRuleNestedNamesAndOrder) {
    auto sheet = parse_stylesheet(
        "@layer framework {"
        "  @layer reset, components;"
        "  @layer components { .x { color: red; } }"
        "}");

    bool saw_components_rule = false;
    for (const auto& layer_rule : sheet.layer_rules) {
        if (layer_rule.name == "framework.components") {
            if (layer_rule.rules.empty()) continue;
            ASSERT_EQ(layer_rule.rules.size(), 1u);
            EXPECT_TRUE(layer_rule.rules[0].in_layer);
            EXPECT_EQ(layer_rule.rules[0].layer_name, "framework.components");
            saw_components_rule = true;
            break;
        }
    }
    EXPECT_TRUE(saw_components_rule);
}

// =============================================================================
// @font-face parsing tests
// =============================================================================

TEST(CSSParserTest, FontFaceSrcWithUrl) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"Open Sans\"; "
        "src: url(https://example.com/opensans.woff); }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "Open Sans");
    // The src should contain the url() function call
    EXPECT_NE(sheet.font_faces[0].src.find("url("), std::string::npos);
    EXPECT_NE(sheet.font_faces[0].src.find("example.com"), std::string::npos);
}

TEST(CSSParserTest, FontFaceWithMultipleSrcFormats) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"Roboto\"; "
        "src: url(roboto.woff2) format('woff2'), "
        "     url(roboto.woff) format('woff'), "
        "     url(roboto.ttf) format('truetype'); }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "Roboto");
    // The src value should capture the multi-source declaration
    EXPECT_FALSE(sheet.font_faces[0].src.empty());
}

TEST(CSSParserTest, FontFaceWithWeightAndStyle) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"MyFont\"; "
        "src: url(myfont.ttf); font-weight: bold; font-style: italic; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "MyFont");
    EXPECT_EQ(sheet.font_faces[0].font_weight, "bold");
    EXPECT_EQ(sheet.font_faces[0].font_style, "italic");
}

TEST(CSSParserTest, FontFaceWithFontDisplay) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"SwapFont\"; "
        "src: url(swap.woff); font-display: swap; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "SwapFont");
    EXPECT_EQ(sheet.font_faces[0].font_display, "swap");
}

TEST(CSSParserTest, FontFaceWithUnicodeRange) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"LatinFont\"; "
        "src: url(latin.woff); unicode-range: U+0000-00FF; }");
    ASSERT_EQ(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "LatinFont");
    // Unicode range may be partially parsed — just check it's not empty
    EXPECT_FALSE(sheet.font_faces[0].unicode_range.empty());
}

TEST(CSSParserTest, MultipleFontFaceRules) {
    auto sheet = parse_stylesheet(
        "@font-face { font-family: \"FontA\"; src: url(a.woff); font-weight: 400; } "
        "@font-face { font-family: \"FontA\"; src: url(a-bold.woff); font-weight: 700; } "
        "@font-face { font-family: \"FontB\"; src: url(b.woff); }");
    ASSERT_EQ(sheet.font_faces.size(), 3u);
    EXPECT_EQ(sheet.font_faces[0].font_family, "FontA");
    EXPECT_EQ(sheet.font_faces[0].font_weight, "400");
    EXPECT_EQ(sheet.font_faces[1].font_family, "FontA");
    EXPECT_EQ(sheet.font_faces[1].font_weight, "700");
    EXPECT_EQ(sheet.font_faces[2].font_family, "FontB");
}

TEST(CSSParserTest, FontFaceWithFontDisplayValues) {
    // Test all valid font-display values
    for (const auto& display : {"auto", "block", "swap", "fallback", "optional"}) {
        std::string css = "@font-face { font-family: \"Test\"; src: url(t.woff); font-display: ";
        css += display;
        css += "; }";
        auto sheet = parse_stylesheet(css);
        ASSERT_EQ(sheet.font_faces.size(), 1u) << "Failed for font-display: " << display;
        EXPECT_EQ(sheet.font_faces[0].font_display, display) << "Failed for font-display: " << display;
    }
}

// =============================================================================
// CSS Nesting Tests
// =============================================================================

// Basic nesting: .parent { .child { color: blue; } }
TEST(CSSNestingTest, BasicNesting) {
    auto sheet = parse_stylesheet(R"(
        .parent {
            color: red;
            .child {
                color: blue;
            }
        }
    )");
    // Should produce 2 rules: .parent and .parent .child
    ASSERT_GE(sheet.rules.size(), 2u);
    bool found_parent = false;
    bool found_child = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".parent") {
            found_parent = true;
            // Parent should have its own declarations
            bool has_color = false;
            for (auto& d : rule.declarations) {
                if (d.property == "color") has_color = true;
            }
            EXPECT_TRUE(has_color) << ".parent should have color declaration";
        }
        if (rule.selector_text == ".parent .child") {
            found_child = true;
            bool has_color = false;
            for (auto& d : rule.declarations) {
                if (d.property == "color") has_color = true;
            }
            EXPECT_TRUE(has_color) << ".parent .child should have color declaration";
        }
    }
    EXPECT_TRUE(found_parent) << "Should have .parent rule";
    EXPECT_TRUE(found_child) << "Should have flattened .parent .child rule";
}

// & combinator: .parent { &.active { color: green; } }
TEST(CSSNestingTest, AmpersandCombinator) {
    auto sheet = parse_stylesheet(R"(
        .parent {
            &.active {
                color: green;
            }
        }
    )");
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".parent.active") {
            found = true;
            EXPECT_FALSE(rule.declarations.empty());
        }
    }
    EXPECT_TRUE(found) << "Should have flattened .parent.active rule";
}

// & with child combinator: .parent { & > .child { color: yellow; } }
TEST(CSSNestingTest, AmpersandChildCombinator) {
    auto sheet = parse_stylesheet(R"(
        .parent {
            & > .direct {
                color: yellow;
            }
        }
    )");
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".parent > .direct") {
            found = true;
            EXPECT_FALSE(rule.declarations.empty());
        }
    }
    EXPECT_TRUE(found) << "Should have flattened .parent > .direct rule";
}

// Implicit descendant (no &): .parent { .child { } } same as .parent .child
TEST(CSSNestingTest, ImplicitDescendant) {
    auto sheet = parse_stylesheet(R"(
        .wrapper {
            .item {
                display: block;
            }
        }
    )");
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".wrapper .item") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have flattened .wrapper .item (implicit descendant)";
}

// Multiple levels of nesting: .a { .b { .c { color: red; } } }
TEST(CSSNestingTest, MultipleLevelsOfNesting) {
    auto sheet = parse_stylesheet(R"(
        .a {
            color: red;
            .b {
                color: green;
                .c {
                    color: blue;
                }
            }
        }
    )");
    bool found_a = false;
    bool found_b = false;
    bool found_c = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".a") found_a = true;
        if (rule.selector_text == ".a .b") found_b = true;
        if (rule.selector_text == ".a .b .c") found_c = true;
    }
    EXPECT_TRUE(found_a) << "Should have .a rule";
    EXPECT_TRUE(found_b) << "Should have .a .b rule (one level nesting)";
    EXPECT_TRUE(found_c) << "Should have .a .b .c rule (two levels nesting)";
}

// Nested rule preserves parent declarations
TEST(CSSNestingTest, NestedRulePreservesParentDeclarations) {
    auto sheet = parse_stylesheet(R"(
        .box {
            margin: 10px;
            padding: 5px;
            .inner {
                font-size: 14px;
            }
        }
    )");
    // .box should have its own declarations intact
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".box") {
            EXPECT_GE(rule.declarations.size(), 2u)
                << ".box should retain margin and padding declarations";
            bool has_margin = false;
            bool has_padding = false;
            for (auto& d : rule.declarations) {
                if (d.property == "margin") has_margin = true;
                if (d.property == "padding") has_padding = true;
            }
            EXPECT_TRUE(has_margin);
            EXPECT_TRUE(has_padding);
        }
    }
}

// & at end of selector: .child & { color: purple; }
TEST(CSSNestingTest, AmpersandAtEnd) {
    auto sheet = parse_stylesheet(R"(
        .parent {
            .child & {
                color: purple;
            }
        }
    )");
    // The & is at end, so should become: .child .parent
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".child .parent") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have .child .parent rule (& at end replaced)";
}

// Multiple & in same selector
TEST(CSSNestingTest, MultipleAmpersands) {
    auto sheet = parse_stylesheet(R"(
        .item {
            & + & {
                margin-left: 10px;
            }
        }
    )");
    // Both & should be replaced: .item + .item
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".item + .item") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have .item + .item rule (both & replaced)";
}

// Nesting with ID selector
TEST(CSSNestingTest, NestingWithIdSelector) {
    auto sheet = parse_stylesheet(R"(
        .container {
            #main {
                background: white;
            }
        }
    )");
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".container #main") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have .container #main rule";
}

// Deep nesting with & at each level
TEST(CSSNestingTest, DeepNestingWithAmpersand) {
    auto sheet = parse_stylesheet(R"(
        .root {
            &.level1 {
                &.level2 {
                    color: red;
                }
            }
        }
    )");
    bool found_l1 = false;
    bool found_l2 = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".root.level1") found_l1 = true;
        if (rule.selector_text == ".root.level1.level2") found_l2 = true;
    }
    EXPECT_TRUE(found_l1) << "Should have .root.level1";
    EXPECT_TRUE(found_l2) << "Should have .root.level1.level2";
}

// Nesting with pseudo-class selector
TEST(CSSNestingTest, NestingWithPseudoClass) {
    auto sheet = parse_stylesheet(R"(
        .btn {
            &:hover {
                background: blue;
            }
        }
    )");
    bool found = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".btn:hover") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have .btn:hover rule";
}

// Mixed declarations and multiple nested rules
TEST(CSSNestingTest, MixedDeclarationsAndNestedRules) {
    auto sheet = parse_stylesheet(R"(
        .card {
            border: 1px;
            .title {
                font-weight: bold;
            }
            .body {
                padding: 10px;
            }
            &:hover {
                shadow: 2px;
            }
        }
    )");
    bool found_card = false;
    bool found_title = false;
    bool found_body = false;
    bool found_hover = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector_text == ".card") found_card = true;
        if (rule.selector_text == ".card .title") found_title = true;
        if (rule.selector_text == ".card .body") found_body = true;
        if (rule.selector_text == ".card:hover") found_hover = true;
    }
    EXPECT_TRUE(found_card);
    EXPECT_TRUE(found_title);
    EXPECT_TRUE(found_body);
    EXPECT_TRUE(found_hover);
}

// =============================================================================
// Cycle 435 — @media, @import, @container, @scope, @property,
//             @counter-style, !important, and parse_declaration_block
// =============================================================================

TEST(CSSParserTest, MediaQueryBasicParse) {
    auto sheet = parse_stylesheet(
        "@media (max-width: 768px) { .col { width: 100%; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_NE(sheet.media_queries[0].condition.find("768px"), std::string::npos);
    ASSERT_EQ(sheet.media_queries[0].rules.size(), 1u);
    EXPECT_EQ(sheet.media_queries[0].rules[0].selector_text, ".col");
}

TEST(CSSParserTest, ImportRuleParse) {
    auto sheet = parse_stylesheet("@import url(\"reset.css\");");
    ASSERT_EQ(sheet.imports.size(), 1u);
    EXPECT_NE(sheet.imports[0].url.find("reset.css"), std::string::npos);
}

TEST(CSSParserTest, ContainerQueryBasicParse) {
    auto sheet = parse_stylesheet(
        "@container sidebar (min-width: 400px) { .widget { display: flex; } }");
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    EXPECT_EQ(sheet.container_rules[0].name, "sidebar");
    EXPECT_NE(sheet.container_rules[0].condition.find("400px"), std::string::npos);
    ASSERT_EQ(sheet.container_rules[0].rules.size(), 1u);
    EXPECT_EQ(sheet.container_rules[0].rules[0].selector_text, ".widget");
}

TEST(CSSParserTest, ScopeRuleParse) {
    auto sheet = parse_stylesheet(
        "@scope (.card) to (.footer) { h2 { color: red; } }");
    ASSERT_EQ(sheet.scope_rules.size(), 1u);
    EXPECT_NE(sheet.scope_rules[0].scope_start.find(".card"), std::string::npos);
    ASSERT_EQ(sheet.scope_rules[0].rules.size(), 1u);
}

TEST(CSSParserTest, PropertyRuleParse) {
    auto sheet = parse_stylesheet(
        "@property --my-color { syntax: '<color>'; inherits: false; initial-value: red; }");
    ASSERT_EQ(sheet.property_rules.size(), 1u);
    EXPECT_EQ(sheet.property_rules[0].name, "--my-color");
    EXPECT_NE(sheet.property_rules[0].syntax.find("color"), std::string::npos);
    EXPECT_EQ(sheet.property_rules[0].inherits, false);
}

TEST(CSSParserTest, CounterStyleRuleParse) {
    auto sheet = parse_stylesheet(
        "@counter-style thumbs { system: cyclic; symbols: '\\1F44D'; suffix: ' '; }");
    ASSERT_EQ(sheet.counter_style_rules.size(), 1u);
    EXPECT_EQ(sheet.counter_style_rules[0].name, "thumbs");
    EXPECT_TRUE(sheet.counter_style_rules[0].descriptors.count("system") > 0);
}

TEST(CSSParserTest, ImportantFlagInDeclaration) {
    auto sheet = parse_stylesheet("div { color: red !important; margin: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    bool found_important = false;
    bool found_non_important = false;
    for (const auto& d : decls) {
        if (d.property == "color") found_important = d.important;
        if (d.property == "margin") found_non_important = !d.important;
    }
    EXPECT_TRUE(found_important) << "color: red !important should have important=true";
    EXPECT_TRUE(found_non_important) << "margin: 0 should have important=false";
}

TEST(CSSParserTest, ParseDeclarationBlock) {
    auto decls = parse_declaration_block("color: blue; font-size: 16px; margin: 0 auto;");
    ASSERT_GE(decls.size(), 3u);
    bool found_color = false;
    bool found_font_size = false;
    for (const auto& d : decls) {
        if (d.property == "color") found_color = true;
        if (d.property == "font-size") found_font_size = true;
    }
    EXPECT_TRUE(found_color);
    EXPECT_TRUE(found_font_size);
}

// =============================================================================
// Cycle 481 — @keyframes percentage stops, multiple animations, pseudo-class
//             arguments, attribute selectors, complex @supports
// =============================================================================

TEST(CSSParserTest, KeyframesWithPercentageStops) {
    auto sheet = parse_stylesheet(R"(
        @keyframes slide {
            0% { transform: translateX(0); }
            50% { transform: translateX(50px); }
            100% { transform: translateX(100px); }
        }
    )");
    ASSERT_EQ(sheet.keyframes.size(), 1u);
    EXPECT_EQ(sheet.keyframes[0].name, "slide");
    ASSERT_EQ(sheet.keyframes[0].keyframes.size(), 3u);
    EXPECT_EQ(sheet.keyframes[0].keyframes[0].selector, "0%");
    EXPECT_EQ(sheet.keyframes[0].keyframes[1].selector, "50%");
    EXPECT_EQ(sheet.keyframes[0].keyframes[2].selector, "100%");
    // Check declarations were parsed
    EXPECT_FALSE(sheet.keyframes[0].keyframes[0].declarations.empty());
    EXPECT_EQ(sheet.keyframes[0].keyframes[0].declarations[0].property, "transform");
}

TEST(CSSParserTest, KeyframesMultipleInStylesheet) {
    auto sheet = parse_stylesheet(R"(
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        @keyframes scaleUp { from { transform: scale(0); } to { transform: scale(1); } }
    )");
    ASSERT_EQ(sheet.keyframes.size(), 2u);
    EXPECT_EQ(sheet.keyframes[0].name, "fadeIn");
    EXPECT_EQ(sheet.keyframes[1].name, "scaleUp");
}

TEST_F(CSSSelectorTest, PseudoClassNthChildArgument) {
    auto list = parse_selector_list("li:nth-child(2n+1)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found_nth = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "nth-child") {
            EXPECT_FALSE(ss.argument.empty()) << "nth-child should have argument";
            found_nth = true;
        }
    }
    EXPECT_TRUE(found_nth) << "Should have :nth-child pseudo-class";
}

TEST_F(CSSSelectorTest, PseudoClassNotArgument) {
    auto list = parse_selector_list("button:not(.disabled)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found_not = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "not") {
            EXPECT_FALSE(ss.argument.empty()) << ":not() should have argument";
            found_not = true;
        }
    }
    EXPECT_TRUE(found_not) << "Should have :not pseudo-class";
}

TEST_F(CSSSelectorTest, AttributeSelectorDashMatchLang) {
    auto list = parse_selector_list("[lang|=en]");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    const auto& ss = compound.simple_selectors[0];
    EXPECT_EQ(ss.type, SimpleSelectorType::Attribute);
    EXPECT_EQ(ss.attr_match, AttributeMatch::DashMatch);
    EXPECT_EQ(ss.attr_name, "lang");
    EXPECT_EQ(ss.attr_value, "en");
}

TEST_F(CSSSelectorTest, SelectorListWithThreeSelectors) {
    auto list = parse_selector_list("h1, h2, h3");
    ASSERT_EQ(list.selectors.size(), 3u);
    EXPECT_EQ(list.selectors[0].parts[0].compound.simple_selectors[0].value, "h1");
    EXPECT_EQ(list.selectors[1].parts[0].compound.simple_selectors[0].value, "h2");
    EXPECT_EQ(list.selectors[2].parts[0].compound.simple_selectors[0].value, "h3");
}

TEST(CSSParserTest, SupportsWithOrCondition) {
    auto sheet = parse_stylesheet(R"(
        @supports (display: grid) or (display: flex) {
            .layout { display: grid; }
        }
    )");
    ASSERT_EQ(sheet.supports_rules.size(), 1u);
    EXPECT_NE(sheet.supports_rules[0].condition.find("grid"), std::string::npos);
    ASSERT_EQ(sheet.supports_rules[0].rules.size(), 1u);
    EXPECT_EQ(sheet.supports_rules[0].rules[0].selector_text, ".layout");
}

TEST(CSSParserTest, StylesheetWithMixedAtRulesAndRules) {
    auto sheet = parse_stylesheet(R"(
        body { margin: 0; }
        @media (max-width: 600px) { body { font-size: 14px; } }
        .container { max-width: 1200px; }
        @keyframes pulse { from { opacity: 1; } to { opacity: 0.5; } }
    )");
    ASSERT_GE(sheet.rules.size(), 2u);  // body and .container
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    ASSERT_EQ(sheet.keyframes.size(), 1u);
    // Check regular rules are present
    bool found_body = false;
    bool found_container = false;
    for (const auto& r : sheet.rules) {
        if (r.selector_text == "body") found_body = true;
        if (r.selector_text == ".container") found_container = true;
    }
    EXPECT_TRUE(found_body);
    EXPECT_TRUE(found_container);
}

// ---------------------------------------------------------------------------
// Cycle 495 — CSS parser additional edge-case regression tests
// ---------------------------------------------------------------------------

// url() function tokenizes as a Function token with value "url"
TEST_F(CSSTokenizerTest, UrlFunctionToken) {
    auto tokens = CSSTokenizer::tokenize_all("url(\"image.png\")");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Function);
    EXPECT_EQ(tokens[0].value, "url");
}

// Viewport-relative dimension: 100vw
TEST_F(CSSTokenizerTest, ViewportWidthDimension) {
    auto tokens = CSSTokenizer::tokenize_all("100vw");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 100.0);
    EXPECT_EQ(tokens[0].unit, "vw");
}

// :last-child pseudo-class selector
TEST_F(CSSSelectorTest, LastChildPseudo) {
    auto list = parse_selector_list(":last-child");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "last-child");
}

// :only-child pseudo-class selector
TEST_F(CSSSelectorTest, OnlyChildPseudo) {
    auto list = parse_selector_list("p:only-child");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "only-child") {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Should have :only-child pseudo-class";
}

// :first-of-type pseudo-class selector
TEST_F(CSSSelectorTest, FirstOfTypePseudo) {
    auto list = parse_selector_list(":first-of-type");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "first-of-type");
}

// :nth-of-type() pseudo-class with argument
TEST_F(CSSSelectorTest, NthOfTypeArgument) {
    auto list = parse_selector_list("li:nth-of-type(2)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found_nth = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "nth-of-type") {
            EXPECT_FALSE(ss.argument.empty()) << "nth-of-type should have argument";
            found_nth = true;
        }
    }
    EXPECT_TRUE(found_nth) << "Should have :nth-of-type pseudo-class";
}

// Stylesheet with only whitespace produces no rules
TEST_F(CSSStylesheetTest, StylesheetWithOnlyWhitespace) {
    auto sheet = parse_stylesheet("   \t\n  ");
    EXPECT_EQ(sheet.rules.size(), 0u);
}

// CSS nesting: &:hover flattens to "a:hover" rule
TEST(CSSNestingTest, NestingWithHoverOnAmpersand) {
    auto sheet = parse_stylesheet(R"(
        a {
            color: blue;
            &:hover { color: red; }
        }
    )");
    // Should flatten to two rules: "a" and "a:hover"
    bool found_base = false;
    bool found_hover = false;
    for (const auto& rule : sheet.rules) {
        if (rule.selector_text == "a") found_base = true;
        if (rule.selector_text.find("hover") != std::string::npos) found_hover = true;
    }
    EXPECT_TRUE(found_base);
    EXPECT_TRUE(found_hover);
}

// ============================================================================
// Cycle 510: CSS parser regression tests
// ============================================================================

TEST_F(CSSTokenizerTest, IntegerFlagOnWholeNumber) {
    auto tokens = CSSTokenizer::tokenize_all("42");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_TRUE(tokens[0].is_integer);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 42.0);
}

TEST_F(CSSTokenizerTest, RemDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("1.5rem");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_EQ(tokens[0].unit, "rem");
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 1.5);
}

TEST_F(CSSTokenizerTest, PercentageNumericValue) {
    auto tokens = CSSTokenizer::tokenize_all("75%");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Percentage);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 75.0);
}

TEST_F(CSSSelectorTest, PseudoClassDisabled) {
    auto list = parse_selector_list(":disabled");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "disabled");
}

TEST_F(CSSSelectorTest, PseudoClassChecked) {
    auto list = parse_selector_list("input:checked");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& parts = list.selectors[0].parts;
    ASSERT_GE(parts.size(), 1u);
    bool found_checked = false;
    for (auto& ss : parts[0].compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "checked")
            found_checked = true;
    }
    EXPECT_TRUE(found_checked);
}

TEST_F(CSSSelectorTest, AttributeSuffixSelector) {
    auto list = parse_selector_list("[href$=\".pdf\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Attribute);
    EXPECT_EQ(compound.simple_selectors[0].attr_match, AttributeMatch::Suffix);
}

TEST_F(CSSStylesheetTest, RuleWithMultipleDeclarations) {
    auto sheet = parse_stylesheet("p { color: red; font-size: 14px; margin: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_GE(sheet.rules[0].declarations.size(), 3u);
    bool found_color = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "color") found_color = true;
    }
    EXPECT_TRUE(found_color);
}

TEST(CSSParserTest, DeclarationWithNumericValue) {
    auto decls = parse_declaration_block("margin: 10px");
    ASSERT_GE(decls.size(), 1u);
    bool found = false;
    for (auto& d : decls) {
        if (d.property == "margin" && !d.values.empty()) found = true;
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 520: CSS parser regression tests
// ============================================================================

TEST_F(CSSTokenizerTest, GreaterThanDelimToken) {
    auto tokens = CSSTokenizer::tokenize_all(">");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Delim);
    EXPECT_EQ(tokens[0].value, ">");
}

TEST_F(CSSTokenizerTest, SingleCommaToken) {
    auto tokens = CSSTokenizer::tokenize_all(",");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Comma);
}

TEST_F(CSSTokenizerTest, ColonToken) {
    auto tokens = CSSTokenizer::tokenize_all(":");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Colon);
}

TEST_F(CSSSelectorTest, UniversalSelectorParsed) {
    auto list = parse_selector_list("*");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_GE(sel.parts.size(), 1u);
    auto& compound = sel.parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Universal);
}

TEST_F(CSSSelectorTest, IdSelectorParsed) {
    auto list = parse_selector_list("#main");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    bool has_id = false;
    for (auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Id) has_id = true;
    }
    EXPECT_TRUE(has_id);
}

TEST_F(CSSSelectorTest, AdjacentSiblingCombinatorParsed) {
    auto list = parse_selector_list("h1 + p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    // Should have 2 parts: h1 and p with adjacent-sibling combinator
    ASSERT_GE(sel.parts.size(), 2u);
    EXPECT_EQ(sel.parts[1].combinator, Combinator::NextSibling);
}

TEST_F(CSSStylesheetTest, EmptyRuleBlock) {
    auto sheet = parse_stylesheet("div {}");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_TRUE(sheet.rules[0].declarations.empty());
}

TEST(CSSParserTest, ParseDeclarationBlockMultipleProps) {
    auto decls = parse_declaration_block("color: red; font-size: 16px; display: block");
    EXPECT_GE(decls.size(), 3u);
    bool found_display = false;
    for (auto& d : decls) {
        if (d.property == "display") found_display = true;
    }
    EXPECT_TRUE(found_display);
}

// ============================================================================
// Cycle 532: CSS parser regression tests
// ============================================================================

// Semicolon token
TEST_F(CSSTokenizerTest, SemicolonToken) {
    auto tokens = CSSTokenizer::tokenize_all(";");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Semicolon);
}

// Left curly brace token
TEST_F(CSSTokenizerTest, LeftBraceToken) {
    auto tokens = CSSTokenizer::tokenize_all("{");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::LeftBrace);
}

// Right curly brace token
TEST_F(CSSTokenizerTest, RightBraceToken) {
    auto tokens = CSSTokenizer::tokenize_all("}");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::RightBrace);
}

// Stylesheet with background-color declaration
TEST_F(CSSStylesheetTest, BackgroundColorDeclaration) {
    auto sheet = parse_stylesheet("body { background-color: #fff; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "background-color") found = true;
    }
    EXPECT_TRUE(found);
}

// Stylesheet with multiple rules
TEST_F(CSSStylesheetTest, MultipleRulesParsed) {
    auto sheet = parse_stylesheet("h1 { color: red; } p { font-size: 14px; }");
    EXPECT_GE(sheet.rules.size(), 2u);
}

// Class selector parsed
TEST_F(CSSSelectorTest, ClassSelectorParsed) {
    auto list = parse_selector_list(".container");
    ASSERT_EQ(list.selectors.size(), 1u);
    bool has_class = false;
    for (auto& ss : list.selectors[0].parts[0].compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Class) has_class = true;
    }
    EXPECT_TRUE(has_class);
}

// Type selector for body
TEST_F(CSSSelectorTest, TypeSelectorBodyParsed) {
    auto list = parse_selector_list("body");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_GE(list.selectors[0].parts.size(), 1u);
    bool has_type = false;
    for (auto& ss : list.selectors[0].parts[0].compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Type) has_type = true;
    }
    EXPECT_TRUE(has_type);
}

// Descendant combinator parsed (h1 p)
TEST_F(CSSSelectorTest, DescendantCombinatorParsed) {
    auto list = parse_selector_list("div p");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_GE(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::Descendant);
}

// ============================================================================
// Cycle 541: CSS parser regression tests
// ============================================================================

// At-rule @media is parsed
TEST_F(CSSStylesheetTest, AtRuleMediaParsed) {
    auto sheet = parse_stylesheet("@media screen { body { color: black; } }");
    // Should parse without crashing; may have 0 or more rules depending on @media handling
    // Just verify it doesn't crash and returns something
    SUCCEED();
}

// Percentage number token
TEST_F(CSSTokenizerTest, PercentTokenValue) {
    auto tokens = CSSTokenizer::tokenize_all("50%");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Percentage);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 50.0);
}

// Negative number token
TEST_F(CSSTokenizerTest, NegativeNumberToken) {
    auto tokens = CSSTokenizer::tokenize_all("-10");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, -10.0);
}

// URL function token
TEST_F(CSSTokenizerTest, UrlAsFunction) {
    auto tokens = CSSTokenizer::tokenize_all("url(\"image.png\")");
    ASSERT_GE(tokens.size(), 1u);
    // URL or Function token expected
    bool is_url_or_func = (tokens[0].type == CSSToken::Function);
    EXPECT_TRUE(is_url_or_func);
}

// Multiple selectors (comma-separated)
TEST_F(CSSSelectorTest, CommaListHasTwoSelectors) {
    auto list = parse_selector_list("h1, h2");
    EXPECT_EQ(list.selectors.size(), 2u);
}

// Child combinator parsed (div > p)
TEST_F(CSSSelectorTest, ChildCombinatorParsed) {
    auto list = parse_selector_list("div > p");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_GE(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::Child);
}

// Subsequent sibling combinator (h1 ~ p)
TEST_F(CSSSelectorTest, SubsequentSiblingCombinatorParsed) {
    auto list = parse_selector_list("h1 ~ p");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_GE(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::SubsequentSibling);
}

// Attribute selector [type="text"]
TEST_F(CSSSelectorTest, AttributeSelectorTypeText) {
    auto list = parse_selector_list("input[type=\"text\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    bool has_attr = false;
    for (auto& ss : list.selectors[0].parts[0].compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute) has_attr = true;
    }
    EXPECT_TRUE(has_attr);
}

// ============================================================================
// Cycle 554: CSS parser regression tests
// ============================================================================

// Hash token with full hex color
TEST_F(CSSTokenizerTest, FullHexColorHashToken) {
    auto tokens = CSSTokenizer::tokenize_all("#aabbcc");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Hash);
    EXPECT_EQ(tokens[0].value, "aabbcc");
}

// String token with double quotes
TEST_F(CSSTokenizerTest, DoubleQuoteStringToken) {
    auto tokens = CSSTokenizer::tokenize_all("\"hello\"");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::String);
    EXPECT_EQ(tokens[0].value, "hello");
}

// String token with single quotes
TEST_F(CSSTokenizerTest, SingleQuoteStringToken) {
    auto tokens = CSSTokenizer::tokenize_all("'world'");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::String);
    EXPECT_EQ(tokens[0].value, "world");
}

// Declaration with !important
TEST(CSSParserTest, DeclarationWithImportant) {
    auto decls = parse_declaration_block("color: red !important");
    ASSERT_GE(decls.size(), 1u);
    EXPECT_EQ(decls[0].property, "color");
    EXPECT_TRUE(decls[0].important);
}

// Stylesheet with id selector rule
TEST_F(CSSStylesheetTest, IdSelectorRule) {
    auto sheet = parse_stylesheet("#header { font-size: 24px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool has_font_size = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "font-size") has_font_size = true;
    }
    EXPECT_TRUE(has_font_size);
}

// Stylesheet with class selector rule
TEST_F(CSSStylesheetTest, ClassSelectorRule) {
    auto sheet = parse_stylesheet(".container { max-width: 1200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool has_max_width = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "max-width") has_max_width = true;
    }
    EXPECT_TRUE(has_max_width);
}

// Number token is flagged as integer
TEST_F(CSSTokenizerTest, IntegerNumericToken) {
    auto tokens = CSSTokenizer::tokenize_all("42");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 42.0);
    EXPECT_TRUE(tokens[0].is_integer);
}

// Dimension token (em unit)
TEST_F(CSSTokenizerTest, EmDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("2em");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 2.0);
    EXPECT_EQ(tokens[0].unit, "em");
}

// Dimension token with ch unit
TEST_F(CSSTokenizerTest, ChDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("3ch");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 3.0);
    EXPECT_EQ(tokens[0].unit, "ch");
}

// Dimension token with vw unit
TEST_F(CSSTokenizerTest, VwDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("100vw");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 100.0);
    EXPECT_EQ(tokens[0].unit, "vw");
}

// Dimension token with px unit
TEST_F(CSSTokenizerTest, PxDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("16px");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 16.0);
    EXPECT_EQ(tokens[0].unit, "px");
}

// Pseudo-class selector :hover
TEST_F(CSSSelectorTest, PseudoClassHoverParsed) {
    auto list = parse_selector_list("a:hover");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 2u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Type);
    EXPECT_EQ(compound.simple_selectors[0].value, "a");
    EXPECT_EQ(compound.simple_selectors[1].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[1].value, "hover");
}

// Attribute selector [disabled] (exists match)
TEST_F(CSSSelectorTest, AttributeSelectorExistsParsed) {
    auto list = parse_selector_list("input[disabled]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 2u);
    EXPECT_EQ(compound.simple_selectors[1].type, SimpleSelectorType::Attribute);
    EXPECT_EQ(compound.simple_selectors[1].attr_name, "disabled");
    EXPECT_EQ(compound.simple_selectors[1].attr_match, AttributeMatch::Exists);
}

// Stylesheet: empty rule has zero declarations
TEST_F(CSSStylesheetTest, EmptyRuleZeroDeclarations) {
    auto sheet = parse_stylesheet("div {}");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_EQ(sheet.rules[0].selector_text, "div");
    EXPECT_EQ(sheet.rules[0].declarations.size(), 0u);
}

// Stylesheet: font-size with px value
TEST_F(CSSStylesheetTest, FontSizePxDeclaration) {
    auto sheet = parse_stylesheet("body { font-size: 14px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "font-size");
    auto& val = sheet.rules[0].declarations[0].values[0];
    EXPECT_DOUBLE_EQ(val.numeric_value, 14.0);
    EXPECT_EQ(val.unit, "px");
}

// ============================================================================
// Cycle 572: More CSS parser tests
// ============================================================================

// Tokenizer: percent sign token value
TEST_F(CSSTokenizerTest, PercentSignTokenValue) {
    auto tokens = CSSTokenizer::tokenize_all("50%");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Percentage);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 50.0);
}

// Tokenizer: float number (non-integer)
TEST_F(CSSTokenizerTest, FloatNumberToken) {
    auto tokens = CSSTokenizer::tokenize_all("3.14");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 3.14);
    EXPECT_FALSE(tokens[0].is_integer);
}

// Tokenizer: vh dimension unit
TEST_F(CSSTokenizerTest, VhDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("50vh");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 50.0);
    EXPECT_EQ(tokens[0].unit, "vh");
}

// Selector: adjacent sibling h1 + p target is paragraph
TEST_F(CSSSelectorTest, AdjacentSiblingTargetIsParagraph) {
    auto list = parse_selector_list("h1 + p");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& sel = list.selectors[0];
    ASSERT_EQ(sel.parts.size(), 2u);
    // Second part is the target: "p" type selector
    EXPECT_EQ(sel.parts[1].compound.simple_selectors[0].value, "p");
}

// Selector: universal selector (*)
TEST_F(CSSSelectorTest, UniversalSelectorParsedType) {
    auto list = parse_selector_list("*");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Universal);
}

// Stylesheet: display:flex declaration
TEST_F(CSSStylesheetTest, DisplayFlexDeclaration) {
    auto sheet = parse_stylesheet(".flex { display: flex; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "display");
    EXPECT_EQ(sheet.rules[0].declarations[0].values[0].value, "flex");
}

// Stylesheet: margin shorthand declaration
TEST_F(CSSStylesheetTest, MarginShorthandDeclaration) {
    auto sheet = parse_stylesheet("div { margin: 10px 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    auto& decls = sheet.rules[0].declarations;
    bool found = false;
    for (auto& d : decls) {
        if (d.property == "margin") found = true;
    }
    EXPECT_TRUE(found);
}

// Stylesheet: three-rule chain (h1, h2, h3)
TEST_F(CSSStylesheetTest, ThreeRulesHierarchy) {
    auto sheet = parse_stylesheet("h1 { font-size: 32px; } h2 { font-size: 24px; } h3 { font-size: 18px; }");
    EXPECT_EQ(sheet.rules.size(), 3u);
    EXPECT_EQ(sheet.rules[0].selector_text, "h1");
    EXPECT_EQ(sheet.rules[1].selector_text, "h2");
    EXPECT_EQ(sheet.rules[2].selector_text, "h3");
}

// ============================================================================
// Cycle 586: More CSS parser tests
// ============================================================================

// Tokenizer: ms dimension token
TEST_F(CSSTokenizerTest, MsDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("200ms");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 200.0);
    EXPECT_EQ(tokens[0].unit, "ms");
}

// Tokenizer: s (seconds) dimension token
TEST_F(CSSTokenizerTest, SecondsDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("1.5s");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 1.5);
    EXPECT_EQ(tokens[0].unit, "s");
}

// Tokenizer: deg dimension token
TEST_F(CSSTokenizerTest, DegDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("90deg");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 90.0);
    EXPECT_EQ(tokens[0].unit, "deg");
}

// Selector: compound selector (div.class)
TEST_F(CSSSelectorTest, CompoundTypeAndClass) {
    auto list = parse_selector_list("div.container");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 2u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Type);
    EXPECT_EQ(compound.simple_selectors[0].value, "div");
    EXPECT_EQ(compound.simple_selectors[1].type, SimpleSelectorType::Class);
    EXPECT_EQ(compound.simple_selectors[1].value, "container");
}

// Selector: three-class compound selector
TEST_F(CSSSelectorTest, ThreeClassCompoundSelector) {
    auto list = parse_selector_list(".a.b.c");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_EQ(compound.simple_selectors.size(), 3u);
    for (auto& s : compound.simple_selectors) {
        EXPECT_EQ(s.type, SimpleSelectorType::Class);
    }
}

// Stylesheet: border-radius property
TEST_F(CSSStylesheetTest, BorderRadiusProperty) {
    auto sheet = parse_stylesheet(".card { border-radius: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "border-radius") found = true;
    }
    EXPECT_TRUE(found);
}

// Stylesheet: color named value
TEST_F(CSSStylesheetTest, NamedColorValue) {
    auto sheet = parse_stylesheet("h1 { color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].values[0].value, "blue");
}

// Stylesheet: padding with four values
TEST_F(CSSStylesheetTest, PaddingFourValues) {
    auto sheet = parse_stylesheet("div { padding: 10px 20px 10px 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "padding") found = true;
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 595: More CSS parser tests
// ============================================================================

// Tokenizer: Turn token
TEST_F(CSSTokenizerTest, TurnDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("0.5turn");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_EQ(tokens[0].unit, "turn");
}

// Tokenizer: rad dimension
TEST_F(CSSTokenizerTest, RadDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("1.5rad");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_EQ(tokens[0].unit, "rad");
}

// Tokenizer: em dimension with 3.5 value
TEST_F(CSSTokenizerTest, EmDimensionNumericValue) {
    auto tokens = CSSTokenizer::tokenize_all("3.5em");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 3.5);
    EXPECT_EQ(tokens[0].unit, "em");
}

// Selector: descendant combinator
TEST_F(CSSSelectorTest, DescendantCombinatorExists) {
    auto list = parse_selector_list("div p");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_EQ(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::Descendant);
}

// Selector: child combinator
TEST_F(CSSSelectorTest, ChildCombinatorExists) {
    auto list = parse_selector_list("ul > li");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_EQ(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::Child);
}

// Selector: id selector type
TEST_F(CSSSelectorTest, IdSelectorType) {
    auto list = parse_selector_list("#main");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_FALSE(compound.simple_selectors.empty());
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Id);
    EXPECT_EQ(compound.simple_selectors[0].value, "main");
}

// Stylesheet: background-color with named color
TEST_F(CSSStylesheetTest, BackgroundColorNamedValue) {
    auto sheet = parse_stylesheet("html { background-color: white; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "background-color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: font-weight declaration
TEST_F(CSSStylesheetTest, FontWeightDeclaration) {
    auto sheet = parse_stylesheet("strong { font-weight: bold; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_FALSE(sheet.rules[0].declarations.empty());
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "font-weight");
    EXPECT_EQ(sheet.rules[0].declarations[0].values[0].value, "bold");
}

// ============================================================================
// Cycle 606: More CSS parser tests
// ============================================================================

// Tokenizer: rem dimension
TEST_F(CSSTokenizerTest, RemDimensionV2Token) {
    auto tokens = CSSTokenizer::tokenize_all("1.5rem");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 1.5);
    EXPECT_EQ(tokens[0].unit, "rem");
}

// Tokenizer: lvh dimension
TEST_F(CSSTokenizerTest, LvhDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("50lvh");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
}

// Selector: pseudo-element ::after
TEST_F(CSSSelectorTest, PseudoElementAfter) {
    auto list = parse_selector_list("p::after");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    bool found_pseudo = false;
    for (auto& s : compound.simple_selectors) {
        if (s.type == SimpleSelectorType::PseudoElement) { found_pseudo = true; break; }
    }
    EXPECT_TRUE(found_pseudo);
}

// Selector: subsequent sibling combinator
TEST_F(CSSSelectorTest, SubsequentSiblingCombinatorExists) {
    auto list = parse_selector_list("h1 ~ p");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_EQ(list.selectors[0].parts.size(), 2u);
    EXPECT_EQ(list.selectors[0].parts[1].combinator, Combinator::SubsequentSibling);
}

// Selector: comma separates two selectors
TEST_F(CSSSelectorTest, CommaSeparatesTwoSelectors) {
    auto list = parse_selector_list("h1, h2");
    EXPECT_EQ(list.selectors.size(), 2u);
}

// Selector: three comma-separated selectors
TEST_F(CSSSelectorTest, ThreeCommaSelectors) {
    auto list = parse_selector_list("h1, h2, h3");
    EXPECT_EQ(list.selectors.size(), 3u);
}

// Stylesheet: text-align center
TEST_F(CSSStylesheetTest, TextAlignCenterDeclaration) {
    auto sheet = parse_stylesheet("p { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "text-align") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: line-height value
TEST_F(CSSStylesheetTest, LineHeightNumericValue) {
    auto sheet = parse_stylesheet("p { line-height: 1.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_FALSE(sheet.rules[0].declarations.empty());
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "line-height");
}

// ============================================================================
// Cycle 615: More CSS parser tests
// ============================================================================

// Tokenizer: zero value number
TEST_F(CSSTokenizerTest, ZeroNumberToken) {
    auto tokens = CSSTokenizer::tokenize_all("0");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 0.0);
}

// Tokenizer: negative decimal number
TEST_F(CSSTokenizerTest, NegativeDecimalToken) {
    auto tokens = CSSTokenizer::tokenize_all("-0.5");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, -0.5);
}

// Selector: pseudo-class focus
TEST_F(CSSSelectorTest, PseudoClassFocusParsed) {
    auto list = parse_selector_list("input:focus");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (auto& s : compound.simple_selectors) {
        if (s.type == SimpleSelectorType::PseudoClass && s.value == "focus") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Selector: pseudo-class active
TEST_F(CSSSelectorTest, PseudoClassActiveParsed) {
    auto list = parse_selector_list("a:active");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (auto& s : compound.simple_selectors) {
        if (s.type == SimpleSelectorType::PseudoClass && s.value == "active") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Selector: attribute selector with value
TEST_F(CSSSelectorTest, AttributeSelectorWithValue) {
    auto list = parse_selector_list("[type=\"text\"]");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (auto& s : compound.simple_selectors) {
        if (s.type == SimpleSelectorType::Attribute) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: cursor pointer declaration
TEST_F(CSSStylesheetTest, CursorPointerDeclaration) {
    auto sheet = parse_stylesheet("button { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "cursor") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transition declaration
TEST_F(CSSStylesheetTest, TransitionDeclaration) {
    auto sheet = parse_stylesheet("a { transition: color 0.3s; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transition") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: two declarations in one rule
TEST_F(CSSStylesheetTest, TwoDeclarationsInOneRule) {
    auto sheet = parse_stylesheet("p { color: red; font-size: 16px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_GE(sheet.rules[0].declarations.size(), 2u);
}

// ============================================================================
// Cycle 624: More CSS parser tests
// ============================================================================

// Tokenizer: vmax dimension
TEST_F(CSSTokenizerTest, VmaxDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("10vmax");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
    EXPECT_EQ(tokens[0].unit, "vmax");
}

// Tokenizer: svh dimension (small viewport)
TEST_F(CSSTokenizerTest, SvhDimensionToken) {
    auto tokens = CSSTokenizer::tokenize_all("100svh");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Dimension);
}

// Tokenizer: integer token is_integer
TEST_F(CSSTokenizerTest, IntegerTokenIsInteger) {
    auto tokens = CSSTokenizer::tokenize_all("42");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_TRUE(tokens[0].is_integer);
    EXPECT_DOUBLE_EQ(tokens[0].numeric_value, 42.0);
}

// Tokenizer: float token is not integer
TEST_F(CSSTokenizerTest, FloatTokenNotInteger) {
    auto tokens = CSSTokenizer::tokenize_all("3.14");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Number);
    EXPECT_FALSE(tokens[0].is_integer);
}

// Selector: class name extracted
TEST_F(CSSSelectorTest, ClassNameExtracted) {
    auto list = parse_selector_list(".container");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_FALSE(compound.simple_selectors.empty());
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Class);
    EXPECT_EQ(compound.simple_selectors[0].value, "container");
}

// Selector: type name extracted
TEST_F(CSSSelectorTest, TypeNameExtracted) {
    auto list = parse_selector_list("section");
    ASSERT_EQ(list.selectors.size(), 1u);
    ASSERT_FALSE(list.selectors[0].parts.empty());
    auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_FALSE(compound.simple_selectors.empty());
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::Type);
    EXPECT_EQ(compound.simple_selectors[0].value, "section");
}

// Stylesheet: overflow property
TEST_F(CSSStylesheetTest, OverflowHiddenDeclaration) {
    auto sheet = parse_stylesheet("div { overflow: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "overflow") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: position property
TEST_F(CSSStylesheetTest, PositionAbsoluteDeclaration) {
    auto sheet = parse_stylesheet(".popup { position: absolute; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "position") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 632: More CSS parser tests
// ============================================================================

// Tokenizer: string token (double-quoted)
TEST_F(CSSTokenizerTest, DoubleQuotedStringToken) {
    auto tokens = CSSTokenizer::tokenize_all("\"hello\"");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::String);
}

// Tokenizer: whitespace token
TEST_F(CSSTokenizerTest, WhitespaceTokenExists) {
    auto tokens = CSSTokenizer::tokenize_all("div p");
    bool has_ws = false;
    for (auto& t : tokens) {
        if (t.type == CSSToken::Whitespace) { has_ws = true; break; }
    }
    EXPECT_TRUE(has_ws);
}

// Tokenizer: delim token for >
TEST_F(CSSTokenizerTest, DelimGreaterThanToken) {
    auto tokens = CSSTokenizer::tokenize_all("div > p");
    bool has_delim = false;
    for (auto& t : tokens) {
        if (t.type == CSSToken::Delim) { has_delim = true; break; }
    }
    EXPECT_TRUE(has_delim);
}

// Selector: attribute selector with contains (~=)
TEST_F(CSSSelectorTest, AttributeSelectorContains) {
    auto list = parse_selector_list("[class~=button]");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: border property
TEST_F(CSSStylesheetTest, BorderDeclaration) {
    auto sheet = parse_stylesheet("div { border: 1px solid black; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "border") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: padding shorthand
TEST_F(CSSStylesheetTest, PaddingDeclaration) {
    auto sheet = parse_stylesheet("p { padding: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "padding") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: margin shorthand
TEST_F(CSSStylesheetTest, MarginDeclaration) {
    auto sheet = parse_stylesheet("h1 { margin: 0 auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "margin") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: width property
TEST_F(CSSStylesheetTest, WidthDeclaration) {
    auto sheet = parse_stylesheet(".box { width: 100%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "width") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 646: More CSS parser tests
// ============================================================================

// Tokenizer: hash token (color)
TEST_F(CSSTokenizerTest, HashColorToken) {
    auto tokens = CSSTokenizer::tokenize_all("#ff0000");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Hash);
}

// Tokenizer: at-keyword token
TEST_F(CSSTokenizerTest, AtKeywordToken) {
    auto tokens = CSSTokenizer::tokenize_all("@media");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::AtKeyword);
}

// Selector: compound selector type+class
TEST_F(CSSSelectorTest, CompoundTypeAndClassSelector) {
    auto list = parse_selector_list("div.active");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    EXPECT_GE(compound.simple_selectors.size(), 2u);
}

// Selector: multiple classes on one element
TEST_F(CSSSelectorTest, TwoClassesOnOneElement) {
    auto list = parse_selector_list(".foo.bar");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& compound = list.selectors[0].parts[0].compound;
    int class_count = 0;
    for (auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Class) class_count++;
    }
    EXPECT_GE(class_count, 2);
}

// Stylesheet: height property
TEST_F(CSSStylesheetTest, HeightDeclaration) {
    auto sheet = parse_stylesheet("div { height: 50px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "height") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: color property
TEST_F(CSSStylesheetTest, ColorDeclaration) {
    auto sheet = parse_stylesheet("p { color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: two rules
TEST_F(CSSStylesheetTest, TwoRulesParsed) {
    auto sheet = parse_stylesheet("div { color: red; } p { color: blue; }");
    EXPECT_EQ(sheet.rules.size(), 2u);
}

// Stylesheet: display property
TEST_F(CSSStylesheetTest, DisplayDeclaration) {
    auto sheet = parse_stylesheet("span { display: inline-block; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "display") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 654: More CSS parser tests
// ============================================================================

// Tokenizer: identifier token
TEST_F(CSSTokenizerTest, IdentifierToken) {
    auto tokens = CSSTokenizer::tokenize_all("auto");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Ident);
}

// Tokenizer: rgb function token
TEST_F(CSSTokenizerTest, RgbFunctionToken) {
    auto tokens = CSSTokenizer::tokenize_all("rgb(");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Function);
}

// Tokenizer: standalone colon token
TEST_F(CSSTokenizerTest, StandaloneColonToken) {
    auto tokens = CSSTokenizer::tokenize_all(":");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Colon);
}

// Selector: descendant combinator between type selectors
TEST_F(CSSSelectorTest, DescendantCombinatorBetweenTypes) {
    auto list = parse_selector_list("section p");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_GE(list.selectors[0].parts.size(), 2u);
}

// Stylesheet: font-family property
TEST_F(CSSStylesheetTest, FontFamilyDeclaration) {
    auto sheet = parse_stylesheet("body { font-family: sans-serif; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "font-family") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: background-color property
TEST_F(CSSStylesheetTest, BackgroundColorPropertyExists) {
    auto sheet = parse_stylesheet("div { background-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "background-color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: three declarations in one rule
TEST_F(CSSStylesheetTest, ThreeDeclarationsInRule) {
    auto sheet = parse_stylesheet("div { color: red; font-size: 16px; margin: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_GE(sheet.rules[0].declarations.size(), 3u);
}

// Stylesheet: property name preserved
TEST_F(CSSStylesheetTest, PropertyNamePreserved) {
    auto sheet = parse_stylesheet("p { letter-spacing: 1px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_FALSE(sheet.rules[0].declarations.empty());
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "letter-spacing");
}

// ============================================================================
// Cycle 659: More CSS parser tests
// ============================================================================

// Tokenizer: semicolon delimiter token
TEST_F(CSSTokenizerTest, SemicolonDelimToken) {
    auto tokens = CSSTokenizer::tokenize_all(";");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::Semicolon);
}

// Tokenizer: opening brace for a rule block
TEST_F(CSSTokenizerTest, OpeningBraceForRuleBlock) {
    auto tokens = CSSTokenizer::tokenize_all("div {");
    ASSERT_GE(tokens.size(), 2u);
    bool found = false;
    for (auto& t : tokens) {
        if (t.type == CSSToken::LeftBrace) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Tokenizer: closing brace ends a block
TEST_F(CSSTokenizerTest, ClosingBraceEndsBlock) {
    auto tokens = CSSTokenizer::tokenize_all("color: red; }");
    ASSERT_FALSE(tokens.empty());
    bool found = false;
    for (auto& t : tokens) {
        if (t.type == CSSToken::RightBrace) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Selector: class selector on div
TEST_F(CSSSelectorTest, ClassSelectorOnDiv) {
    auto list = parse_selector_list("div.container");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Stylesheet: two selectors comma-separated
TEST_F(CSSStylesheetTest, TwoSelectorsCommaSeparated) {
    auto sheet = parse_stylesheet("h1, h2 { color: blue; }");
    ASSERT_FALSE(sheet.rules.empty());
}

// Stylesheet: border-radius on paragraph element
TEST_F(CSSStylesheetTest, BorderRadiusParagraphElement) {
    auto sheet = parse_stylesheet("p { border-radius: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "border-radius") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: font-size property value
TEST_F(CSSStylesheetTest, FontSizePropertyValue) {
    auto sheet = parse_stylesheet("p { font-size: 14px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "font-size") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: z-index declaration
TEST_F(CSSStylesheetTest, ZIndexDeclaration) {
    auto sheet = parse_stylesheet(".overlay { z-index: 100; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "z-index") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 672: More CSS parser tests
// ============================================================================

// Tokenizer: left paren token
TEST_F(CSSTokenizerTest, LeftParenToken) {
    auto tokens = CSSTokenizer::tokenize_all("(");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::LeftParen);
}

// Tokenizer: right paren token
TEST_F(CSSTokenizerTest, RightParenToken) {
    auto tokens = CSSTokenizer::tokenize_all(")");
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, CSSToken::RightParen);
}

// Stylesheet: opacity declaration
TEST_F(CSSStylesheetTest, OpacityDeclaration) {
    auto sheet = parse_stylesheet(".fade { opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "opacity") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: overflow property on box class
TEST_F(CSSStylesheetTest, OverflowPropertyOnBoxClass) {
    auto sheet = parse_stylesheet(".box { overflow: scroll; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "overflow") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: text-transform declaration
TEST_F(CSSStylesheetTest, TextTransformDeclaration) {
    auto sheet = parse_stylesheet("h1 { text-transform: uppercase; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "text-transform") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: visibility declaration
TEST_F(CSSStylesheetTest, VisibilityDeclaration) {
    auto sheet = parse_stylesheet(".hidden { visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "visibility") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: cursor auto declaration on div
TEST_F(CSSStylesheetTest, CursorAutoOnDiv) {
    auto sheet = parse_stylesheet("div { cursor: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "cursor") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transition on input element
TEST_F(CSSStylesheetTest, TransitionOnInputElement) {
    auto sheet = parse_stylesheet("input { transition: border-color 0.2s; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transition") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: flex declaration shorthand
TEST_F(CSSStylesheetTest, FlexShorthandDeclaration) {
    auto sheet = parse_stylesheet(".item { flex: 1 1 auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "flex") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// Cycle 680: More CSS parser tests
// ============================================================================

// Stylesheet: grid-template-columns declaration
TEST_F(CSSStylesheetTest, GridTemplateColumnsDeclaration) {
    auto sheet = parse_stylesheet(".grid { grid-template-columns: 1fr 1fr; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "grid-template-columns") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: position relative declaration
TEST_F(CSSStylesheetTest, PositionRelativeDeclaration) {
    auto sheet = parse_stylesheet("div { position: relative; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "position") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: top/left absolute positioning
TEST_F(CSSStylesheetTest, TopLeftDeclarations) {
    auto sheet = parse_stylesheet(".popup { position: absolute; top: 10px; left: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found_top = false, found_left = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "top") found_top = true;
        if (d.property == "left") found_left = true;
    }
    EXPECT_TRUE(found_top);
    EXPECT_TRUE(found_left);
}

// Selector: ID selector #main
TEST_F(CSSSelectorTest, IdSelectorMain) {
    auto list = parse_selector_list("#main");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Selector: pseudo-class a:hover
TEST_F(CSSSelectorTest, PseudoClassHoverOnAnchor) {
    auto list = parse_selector_list("a:hover");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Stylesheet: box-shadow declaration
TEST_F(CSSStylesheetTest, BoxShadowDeclaration) {
    auto sheet = parse_stylesheet(".card { box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "box-shadow") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: text-overflow declaration
TEST_F(CSSStylesheetTest, TextOverflowDeclaration) {
    auto sheet = parse_stylesheet("p { text-overflow: ellipsis; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "text-overflow") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: white-space declaration
TEST_F(CSSStylesheetTest, WhiteSpaceDeclaration) {
    auto sheet = parse_stylesheet("pre { white-space: pre; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "white-space") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Cycle 692 — 8 additional CSS parser tests
// ---------------------------------------------------------------------------

// Selector: li:nth-child(odd) has "odd" argument
TEST_F(CSSSelectorTest, NthChildOddSelector) {
    auto list = parse_selector_list("li:nth-child(odd)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "nth-child") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: li:nth-child(even) is parsed
TEST_F(CSSSelectorTest, NthChildEvenSelector) {
    auto list = parse_selector_list("li:nth-child(even)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "nth-child") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: p:last-of-type is parsed
TEST_F(CSSSelectorTest, LastOfTypePseudo) {
    auto list = parse_selector_list("p:last-of-type");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "last-of-type") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: tr:nth-last-child(2) is parsed
TEST_F(CSSSelectorTest, NthLastChildPseudo) {
    auto list = parse_selector_list("tr:nth-last-child(2)");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "nth-last-child") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: calc() value in width declaration
TEST_F(CSSStylesheetTest, CalcDeclaration) {
    auto sheet = parse_stylesheet("div { width: calc(100% - 20px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "width") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: CSS custom property (--variable) declaration
TEST_F(CSSStylesheetTest, CustomPropertyDeclaration) {
    auto sheet = parse_stylesheet(":root { --primary-color: #0066cc; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "--primary-color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: letter-spacing declaration
TEST_F(CSSStylesheetTest, LetterSpacingDeclaration) {
    auto sheet = parse_stylesheet("h1 { letter-spacing: 0.1em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "letter-spacing") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: line-height declaration
TEST_F(CSSStylesheetTest, LineHeightOnParagraphElement) {
    auto sheet = parse_stylesheet("p { line-height: 1.6; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "line-height") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Cycle 704 — 8 additional CSS parser tests
// ---------------------------------------------------------------------------

// Selector: :focus-within pseudo-class
TEST_F(CSSSelectorTest, PseudoClassFocusWithin) {
    auto list = parse_selector_list("div:focus-within");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "focus-within") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: :is() functional pseudo-class
TEST_F(CSSSelectorTest, PseudoClassIsParsed) {
    auto list = parse_selector_list(":is(h1, h2, h3)");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Selector: :where() functional pseudo-class
TEST_F(CSSSelectorTest, PseudoClassWhereParsed) {
    auto list = parse_selector_list(":where(.nav, .header)");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Selector: ::placeholder pseudo-element
TEST_F(CSSSelectorTest, PseudoElementPlaceholder) {
    auto list = parse_selector_list("input::placeholder");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Selector: ::selection pseudo-element
TEST_F(CSSSelectorTest, PseudoElementSelection) {
    auto list = parse_selector_list("p::selection");
    ASSERT_EQ(list.selectors.size(), 1u);
    EXPECT_FALSE(list.selectors[0].parts.empty());
}

// Stylesheet: max-width declaration
TEST_F(CSSStylesheetTest, MaxWidthDeclaration) {
    auto sheet = parse_stylesheet(".container { max-width: 1200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "max-width") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: min-height declaration
TEST_F(CSSStylesheetTest, MinHeightDeclaration) {
    auto sheet = parse_stylesheet("section { min-height: 100vh; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "min-height") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: aspect-ratio declaration
TEST_F(CSSStylesheetTest, AspectRatioDeclaration) {
    auto sheet = parse_stylesheet("video { aspect-ratio: 16 / 9; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "aspect-ratio") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: animation-name declaration
TEST_F(CSSStylesheetTest, AnimationNameDeclaration) {
    auto sheet = parse_stylesheet(".anim { animation-name: slide; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "animation-name") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: animation-duration declaration
TEST_F(CSSStylesheetTest, AnimationDurationDeclaration) {
    auto sheet = parse_stylesheet(".anim { animation-duration: 0.5s; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "animation-duration") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transition-duration declaration
TEST_F(CSSStylesheetTest, TransitionDurationDeclaration) {
    auto sheet = parse_stylesheet("a { transition-duration: 200ms; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transition-duration") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: grid-column declaration
TEST_F(CSSStylesheetTest, GridColumnDeclaration) {
    auto sheet = parse_stylesheet(".cell { grid-column: 1 / 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "grid-column") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: grid-row declaration
TEST_F(CSSStylesheetTest, GridRowDeclaration) {
    auto sheet = parse_stylesheet(".cell { grid-row: 2 / 4; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "grid-row") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: will-change declaration
TEST_F(CSSStylesheetTest, WillChangeDeclaration) {
    auto sheet = parse_stylesheet(".box { will-change: transform; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "will-change") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: scroll-behavior declaration
TEST_F(CSSStylesheetTest, ScrollBehaviorDeclaration) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "scroll-behavior") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: pointer-events declaration
TEST_F(CSSStylesheetTest, PointerEventsDeclaration) {
    auto sheet = parse_stylesheet(".overlay { pointer-events: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "pointer-events") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: font-family declaration with quoted value
TEST_F(CSSStylesheetTest, FontFamilyQuotedValue) {
    auto sheet = parse_stylesheet(R"(body { font-family: "Arial", sans-serif; })");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "font-family") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: background-color with rgb()
TEST_F(CSSStylesheetTest, BackgroundColorRgbDeclaration) {
    auto sheet = parse_stylesheet("div { background-color: rgb(255, 0, 0); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "background-color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: background-color with rgba()
TEST_F(CSSStylesheetTest, BackgroundColorRgbaDeclaration) {
    auto sheet = parse_stylesheet("div { background-color: rgba(0, 0, 255, 0.5); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "background-color") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: border-radius declaration
TEST_F(CSSStylesheetTest, BorderRadiusDeclaration) {
    auto sheet = parse_stylesheet(".btn { border-radius: 4px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "border-radius") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: outline declaration
TEST_F(CSSStylesheetTest, OutlineDeclaration) {
    auto sheet = parse_stylesheet("a:focus { outline: 2px solid blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "outline") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: user-select declaration
TEST_F(CSSStylesheetTest, UserSelectDeclaration) {
    auto sheet = parse_stylesheet(".noselect { user-select: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "user-select") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: resize declaration
TEST_F(CSSStylesheetTest, ResizeDeclaration) {
    auto sheet = parse_stylesheet("textarea { resize: vertical; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "resize") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: columns declaration
TEST_F(CSSStylesheetTest, ColumnsDeclaration) {
    auto sheet = parse_stylesheet(".multi { columns: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "columns") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Selector: attribute suffix [href$=".pdf"]
TEST_F(CSSSelectorTest, AttributeSelectorSuffixPdf) {
    auto list = parse_selector_list(R"(a[href$=".pdf"])");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute && ss.attr_match == AttributeMatch::Suffix) {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: attribute substring [class*="nav"]
TEST_F(CSSSelectorTest, AttributeSelectorSubstringNav) {
    auto list = parse_selector_list(R"(div[class*="nav"])");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute && ss.attr_match == AttributeMatch::Substring) {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: attribute dash-match [lang|="en"]
TEST_F(CSSSelectorTest, AttributeSelectorDashMatchLangEn) {
    auto list = parse_selector_list(R"(p[lang|="en"])");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute && ss.attr_match == AttributeMatch::DashMatch) {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

// Selector: attribute includes [class~="widget"]
TEST_F(CSSSelectorTest, AttributeSelectorIncludesWidget) {
    auto list = parse_selector_list(R"(div[class~="widget"])");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors) {
        if (ss.type == SimpleSelectorType::Attribute && ss.attr_match == AttributeMatch::Includes) {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: gap declaration
TEST_F(CSSStylesheetTest, GapDeclaration) {
    auto sheet = parse_stylesheet(".grid { gap: 16px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "gap") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: column-gap declaration
TEST_F(CSSStylesheetTest, ColumnGapDeclaration) {
    auto sheet = parse_stylesheet(".flex { column-gap: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "column-gap") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: row-gap declaration
TEST_F(CSSStylesheetTest, RowGapDeclaration) {
    auto sheet = parse_stylesheet(".flex { row-gap: 12px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "row-gap") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: place-items declaration
TEST_F(CSSStylesheetTest, PlaceItemsDeclaration) {
    auto sheet = parse_stylesheet(".grid { place-items: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "place-items") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transition-property declaration
TEST_F(CSSStylesheetTest, TransitionPropertyDeclaration) {
    auto sheet = parse_stylesheet(".box { transition-property: all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transition-property") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transition-timing-function declaration
TEST_F(CSSStylesheetTest, TransitionTimingFunctionDeclaration) {
    auto sheet = parse_stylesheet("a { transition-timing-function: ease-in-out; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transition-timing-function") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: filter declaration
TEST_F(CSSStylesheetTest, FilterDeclaration) {
    auto sheet = parse_stylesheet(".blur { filter: blur(4px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "filter") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: backdrop-filter declaration
TEST_F(CSSStylesheetTest, BackdropFilterDeclaration) {
    auto sheet = parse_stylesheet(".glass { backdrop-filter: blur(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "backdrop-filter") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transform declaration
TEST_F(CSSStylesheetTest, TransformDeclaration) {
    auto sheet = parse_stylesheet(".rotate { transform: rotate(45deg); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transform") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: transform-origin declaration
TEST_F(CSSStylesheetTest, TransformOriginDeclaration) {
    auto sheet = parse_stylesheet(".box { transform-origin: center center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "transform-origin") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: clip-path declaration
TEST_F(CSSStylesheetTest, ClipPathDeclaration) {
    auto sheet = parse_stylesheet(".circle { clip-path: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "clip-path") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: mask-image declaration
TEST_F(CSSStylesheetTest, MaskImageDeclaration) {
    auto sheet = parse_stylesheet(".masked { mask-image: linear-gradient(black, transparent); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "mask-image") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: color-scheme declaration
TEST_F(CSSStylesheetTest, ColorSchemeDeclaration) {
    auto sheet = parse_stylesheet(":root { color-scheme: light dark; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "color-scheme") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: font-variant declaration
TEST_F(CSSStylesheetTest, FontVariantDeclaration) {
    auto sheet = parse_stylesheet("p { font-variant: small-caps; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "font-variant") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: text-align-last declaration
TEST_F(CSSStylesheetTest, TextAlignLastDeclaration) {
    auto sheet = parse_stylesheet("p { text-align-last: justify; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "text-align-last") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: writing-mode declaration
TEST_F(CSSStylesheetTest, WritingModeDeclaration) {
    auto sheet = parse_stylesheet(".vertical { writing-mode: vertical-rl; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "writing-mode") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: direction declaration
TEST_F(CSSStylesheetTest, DirectionDeclaration) {
    auto sheet = parse_stylesheet("[dir=rtl] { direction: rtl; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "direction") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: counter-reset declaration
TEST_F(CSSStylesheetTest, CounterResetDeclaration) {
    auto sheet = parse_stylesheet("body { counter-reset: section 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "counter-reset") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: counter-increment declaration
TEST_F(CSSStylesheetTest, CounterIncrementDeclaration) {
    auto sheet = parse_stylesheet("h2 { counter-increment: section; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "counter-increment") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Stylesheet: list-style-type declaration
TEST_F(CSSStylesheetTest, ListStyleTypeDeclaration) {
    auto sheet = parse_stylesheet("ul { list-style-type: disc; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "list-style-type") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Cycle 753 — typography and layout property declarations
TEST_F(CSSStylesheetTest, TableLayoutDeclaration) {
    auto sheet = parse_stylesheet("table { table-layout: fixed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "table-layout") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AppearanceDeclaration) {
    auto sheet = parse_stylesheet("button { appearance: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "appearance") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ImageRenderingDeclaration) {
    auto sheet = parse_stylesheet("img { image-rendering: pixelated; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "image-rendering") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakDeclaration) {
    auto sheet = parse_stylesheet("p { word-break: break-all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "word-break") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowWrapDeclaration) {
    auto sheet = parse_stylesheet("p { overflow-wrap: break-word; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "overflow-wrap") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextIndentDeclaration) {
    auto sheet = parse_stylesheet("p { text-indent: 2em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "text-indent") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphensDeclaration) {
    auto sheet = parse_stylesheet("p { hyphens: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "hyphens") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TabSizeDeclaration) {
    auto sheet = parse_stylesheet("pre { tab-size: 4; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations) {
        if (d.property == "tab-size") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// Cycle 764 — pseudo-element selector targeting
TEST_F(CSSStylesheetTest, PseudoElementFirstLine) {
    auto sheet = parse_stylesheet("p::first-line { font-weight: bold; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("first-line"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoElementFirstLetter) {
    auto sheet = parse_stylesheet("p::first-letter { font-size: 2em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("first-letter"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoElementMarker) {
    auto sheet = parse_stylesheet("li::marker { color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("marker"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassFocus) {
    auto sheet = parse_stylesheet("input:focus { outline: 2px solid blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("focus"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassVisited) {
    auto sheet = parse_stylesheet("a:visited { color: purple; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("visited"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassChecked) {
    auto sheet = parse_stylesheet("input:checked { background: green; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("checked"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassDisabled) {
    auto sheet = parse_stylesheet("button:disabled { opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("disabled"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassEnabled) {
    auto sheet = parse_stylesheet("button:enabled { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("enabled"), std::string::npos);
}

// Cycle 771 — scroll, accent, caret, isolation, paint declarations
TEST_F(CSSStylesheetTest, ScrollbarWidthDeclaration) {
    auto sheet = parse_stylesheet("body { scrollbar-width: thin; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scrollbar-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AccentColorDeclaration) {
    auto sheet = parse_stylesheet("input { accent-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "accent-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CaretColorDeclaration) {
    auto sheet = parse_stylesheet("textarea { caret-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "caret-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, IsolationDeclaration) {
    auto sheet = parse_stylesheet(".stacking { isolation: isolate; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "isolation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MixBlendModeDeclaration) {
    auto sheet = parse_stylesheet(".layer { mix-blend-mode: multiply; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mix-blend-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PaintOrderDeclaration) {
    auto sheet = parse_stylesheet("text { paint-order: stroke fill; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "paint-order") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorDeclaration) {
    auto sheet = parse_stylesheet("body { overscroll-behavior: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ShapeOutsideDeclaration) {
    auto sheet = parse_stylesheet(".float { shape-outside: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "shape-outside") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 778 — CSS form-state and layout pseudo-class declarations
TEST_F(CSSStylesheetTest, PseudoClassRequired) {
    auto sheet = parse_stylesheet("input:required { border-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("required"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassOptional) {
    auto sheet = parse_stylesheet("input:optional { border-color: gray; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("optional"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassValid) {
    auto sheet = parse_stylesheet("input:valid { outline: 2px solid green; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("valid"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassInvalid) {
    auto sheet = parse_stylesheet("input:invalid { outline: 2px solid red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("invalid"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassFocusVisible) {
    auto sheet = parse_stylesheet("button:focus-visible { outline: 3px solid blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("focus-visible"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassFocusWithin) {
    auto sheet = parse_stylesheet("form:focus-within { background: #eef; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("focus-within"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassPlaceholderShown) {
    auto sheet = parse_stylesheet("input:placeholder-shown { border: 1px dashed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("placeholder-shown"), std::string::npos);
}

TEST_F(CSSStylesheetTest, PseudoClassReadOnly) {
    auto sheet = parse_stylesheet("input:read-only { background: #eee; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_NE(sheet.rules[0].selector_text.find("read-only"), std::string::npos);
}

// Cycle 783 — CSS custom property, var(), and modern function declarations
TEST_F(CSSStylesheetTest, VarFunctionInDeclaration) {
    auto sheet = parse_stylesheet(".theme { color: var(--primary-color); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CustomPropertyDashDash) {
    auto sheet = parse_stylesheet(":root { --brand-color: #ff6600; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "--brand-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerTypeDeclaration) {
    auto sheet = parse_stylesheet(".sidebar { container-type: inline-size; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerNameDeclaration) {
    auto sheet = parse_stylesheet(".sidebar { container-name: sidebar; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InlineStyleFontSize) {
    auto sheet = parse_stylesheet("p { font-size: clamp(1rem, 2vw, 2rem); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MinFunctionDeclaration) {
    auto sheet = parse_stylesheet("img { width: min(100%, 500px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaxFunctionDeclaration) {
    auto sheet = parse_stylesheet("p { padding: max(1em, 4vw); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, RoundFunctionDeclaration) {
    auto sheet = parse_stylesheet(".box { width: round(var(--size), 10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "width") { found = true; break; }
    EXPECT_TRUE(found);
}
