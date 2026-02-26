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
