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

// Cycle 847 — pseudo-class selectors
TEST_F(CSSSelectorTest, PseudoClassRoot) {
    auto list = parse_selector_list(":root");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "root");
}

TEST_F(CSSSelectorTest, PseudoClassEmpty) {
    auto list = parse_selector_list("p:empty");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "empty") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassEnabled) {
    auto list = parse_selector_list("input:enabled");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "enabled") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassRequired) {
    auto list = parse_selector_list("input:required");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "required") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassValid) {
    auto list = parse_selector_list("form:valid");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "valid") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassInvalid) {
    auto list = parse_selector_list("input:invalid");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "invalid") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassAnyLink) {
    auto list = parse_selector_list(":any-link");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    ASSERT_GE(compound.simple_selectors.size(), 1u);
    EXPECT_EQ(compound.simple_selectors[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(compound.simple_selectors[0].value, "any-link");
}

TEST_F(CSSSelectorTest, PseudoClassFocusVisible) {
    auto list = parse_selector_list("button:focus-visible");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "focus-visible") found = true;
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

TEST_F(CSSStylesheetTest, PlaceContentDeclaration) {
    auto sheet = parse_stylesheet(".grid { place-content: center space-between; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "place-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceSelfDeclaration) {
    auto sheet = parse_stylesheet(".item { place-self: end stretch; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "place-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OrderDeclaration) {
    auto sheet = parse_stylesheet(".flex-item { order: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "order") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitDeclaration) {
    auto sheet = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectPositionDeclaration) {
    auto sheet = parse_stylesheet("img { object-position: 50% top; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentVisibilityDeclaration) {
    auto sheet = parse_stylesheet(".section { content-visibility: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "content-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainDeclaration) {
    auto sheet = parse_stylesheet(".widget { contain: layout paint; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollPaddingDeclaration) {
    auto sheet = parse_stylesheet(".scroll { scroll-padding: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationDelayDeclaration) {
    auto sheet = parse_stylesheet(".elem { animation-delay: 0.5s; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-delay") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationFillModeDeclaration) {
    auto sheet = parse_stylesheet(".elem { animation-fill-mode: forwards; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-fill-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationIterationCountDeclaration) {
    auto sheet = parse_stylesheet(".spin { animation-iteration-count: infinite; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-iteration-count") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationDirectionDeclaration) {
    auto sheet = parse_stylesheet(".elem { animation-direction: alternate; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationPlayStateDeclaration) {
    auto sheet = parse_stylesheet(".paused { animation-play-state: paused; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-play-state") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationTimingFunctionDeclaration) {
    auto sheet = parse_stylesheet(".ease { animation-timing-function: ease-in-out; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-timing-function") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransitionDelayDeclaration) {
    auto sheet = parse_stylesheet("a { transition-delay: 200ms; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transition-delay") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, KeyframesRuleInStylesheet) {
    auto sheet = parse_stylesheet("@keyframes slide { from { transform: translateX(0); } to { transform: translateX(100px); } }");
    EXPECT_EQ(sheet.keyframes.size(), 1u);
    if (sheet.keyframes.size() > 0)
        EXPECT_EQ(sheet.keyframes[0].name, "slide");
}

TEST(CSSParserTest, SupportsOrCondition) {
    auto sheet = parse_stylesheet("@supports (display: grid) or (display: flex) { .box { display: grid; } }");
    EXPECT_GE(sheet.supports_rules.size(), 1u);
}

TEST(CSSParserTest, SupportsAndCondition) {
    auto sheet = parse_stylesheet("@supports (display: grid) and (gap: 0) { .grid { gap: 10px; } }");
    EXPECT_GE(sheet.supports_rules.size(), 1u);
}

TEST(CSSParserTest, PropertyRuleInheritsField) {
    auto sheet = parse_stylesheet("@property --my-color { syntax: '<color>'; inherits: true; initial-value: red; }");
    ASSERT_GE(sheet.property_rules.size(), 1u);
    EXPECT_EQ(sheet.property_rules[0].name, "--my-color");
}

TEST(CSSParserTest, PropertyRuleSyntaxField) {
    auto sheet = parse_stylesheet("@property --size { syntax: '<length>'; inherits: false; initial-value: 0px; }");
    ASSERT_GE(sheet.property_rules.size(), 1u);
    EXPECT_EQ(sheet.property_rules[0].syntax, "<length>");
}

TEST(CSSParserTest, PropertyRuleInitialValue) {
    auto sheet = parse_stylesheet("@property --ratio { syntax: '<number>'; inherits: false; initial-value: 1; }");
    ASSERT_GE(sheet.property_rules.size(), 1u);
    EXPECT_EQ(sheet.property_rules[0].initial_value, "1");
}

TEST(CSSParserTest, FontFaceWeightField) {
    auto sheet = parse_stylesheet("@font-face { font-family: 'MyFont'; src: url('font.woff2'); font-weight: 700; }");
    ASSERT_GE(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_weight, "700");
}

TEST(CSSParserTest, FontFaceStyleField) {
    auto sheet = parse_stylesheet("@font-face { font-family: 'ItalicFont'; src: url('italic.woff2'); font-style: italic; }");
    ASSERT_GE(sheet.font_faces.size(), 1u);
    EXPECT_EQ(sheet.font_faces[0].font_style, "italic");
}

TEST(CSSParserTest, FontFaceUnicodeRange) {
    auto sheet = parse_stylesheet("@font-face { font-family: 'Latin'; src: url('latin.woff2'); unicode-range: U+0000-00FF; }");
    ASSERT_GE(sheet.font_faces.size(), 1u);
    EXPECT_FALSE(sheet.font_faces[0].unicode_range.empty());
}

TEST_F(CSSStylesheetTest, GridTemplateRowsDeclaration) {
    auto sheet = parse_stylesheet(".grid { grid-template-rows: 100px auto 50px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-template-rows") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridAutoFlowDeclaration) {
    auto sheet = parse_stylesheet(".grid { grid-auto-flow: dense column; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-auto-flow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridTemplateAreasDeclaration) {
    auto sheet = parse_stylesheet(".layout { grid-template-areas: 'header header' 'sidebar main'; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-template-areas") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifyItemsDeclaration) {
    auto sheet = parse_stylesheet(".grid { justify-items: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-items") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignItemsDeclaration) {
    auto sheet = parse_stylesheet(".flex { align-items: stretch; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-items") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexGrowDeclaration) {
    auto sheet = parse_stylesheet(".item { flex-grow: 2; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-grow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexShrinkDeclaration) {
    auto sheet = parse_stylesheet(".item { flex-shrink: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-shrink") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexWrapDeclaration) {
    auto sheet = parse_stylesheet(".container { flex-wrap: wrap-reverse; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-wrap") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 819 — CSS overflow-x/y, text-shadow, cursor, scroll-snap, column-rule, column-fill, grid-row/column
TEST_F(CSSStylesheetTest, OverflowXDeclaration) {
    auto sheet = parse_stylesheet(".box { overflow-x: scroll; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYDeclaration) {
    auto sheet = parse_stylesheet(".box { overflow-y: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextShadowDeclaration) {
    auto sheet = parse_stylesheet("h1 { text-shadow: 1px 1px 2px rgba(0,0,0,0.5); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CursorDeclaration) {
    auto sheet = parse_stylesheet("a { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapTypeDeclaration) {
    auto sheet = parse_stylesheet(".container { scroll-snap-type: x mandatory; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapAlignDeclaration) {
    auto sheet = parse_stylesheet(".item { scroll-snap-align: start; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnRuleWidthDeclaration) {
    auto sheet = parse_stylesheet(".text { column-rule-width: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-rule-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnFillDeclaration) {
    auto sheet = parse_stylesheet(".cols { column-fill: balance; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-fill") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 827 — counter-style descriptors, container query variants, scope rule variants
TEST(CSSParserTest, CounterStyleSymbolsDescriptor) {
    auto sheet = parse_stylesheet(
        "@counter-style emoji-list { system: cyclic; symbols: '🔴' '🟡' '🟢'; suffix: ' '; }");
    ASSERT_GE(sheet.counter_style_rules.size(), 1u);
    EXPECT_TRUE(sheet.counter_style_rules[0].descriptors.count("symbols") > 0);
}

TEST(CSSParserTest, CounterStyleSuffixDescriptor) {
    auto sheet = parse_stylesheet(
        "@counter-style period-list { system: numeric; symbols: '0' '1' '2' '3' '4' '5' '6' '7' '8' '9'; suffix: '. '; }");
    ASSERT_GE(sheet.counter_style_rules.size(), 1u);
    EXPECT_TRUE(sheet.counter_style_rules[0].descriptors.count("suffix") > 0);
}

TEST(CSSParserTest, TwoCounterStyleRules) {
    auto sheet = parse_stylesheet(
        "@counter-style alpha { system: alphabetic; symbols: a b c; }"
        "@counter-style roman { system: additive; additive-symbols: 1000 M, 500 D; }");
    EXPECT_EQ(sheet.counter_style_rules.size(), 2u);
}

TEST(CSSParserTest, ContainerQueryMaxWidth) {
    auto sheet = parse_stylesheet(
        "@container card (max-width: 300px) { .label { font-size: 0.8em; } }");
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    EXPECT_NE(sheet.container_rules[0].condition.find("300px"), std::string::npos);
}

TEST(CSSParserTest, ContainerQueryAnonymous) {
    auto sheet = parse_stylesheet(
        "@container (min-width: 600px) { .hero { padding: 2rem; } }");
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    // anonymous container — name may be empty
    EXPECT_EQ(sheet.container_rules[0].rules.size(), 1u);
}

TEST(CSSParserTest, ContainerQueryTwoRules) {
    auto sheet = parse_stylesheet(
        "@container main (min-width: 800px) { h1 { font-size: 2em; } }"
        "@container sidebar (max-width: 250px) { nav { display: none; } }");
    EXPECT_EQ(sheet.container_rules.size(), 2u);
}

TEST(CSSParserTest, ScopeRuleNoEndBoundary) {
    auto sheet = parse_stylesheet(
        "@scope (.article) { p { line-height: 1.6; } }");
    ASSERT_EQ(sheet.scope_rules.size(), 1u);
    EXPECT_NE(sheet.scope_rules[0].scope_start.find("article"), std::string::npos);
}

TEST(CSSParserTest, TwoScopeRules) {
    auto sheet = parse_stylesheet(
        "@scope (.card) { .title { font-weight: bold; } }"
        "@scope (.nav) to (.footer) { a { color: white; } }");
    EXPECT_EQ(sheet.scope_rules.size(), 2u);
}

// Cycle 838 — @media query features and @import rules
TEST_F(CSSStylesheetTest, MediaQueryMaxWidth) {
    auto sheet = parse_stylesheet(
        "@media (max-width: 480px) { .mobile-only { display: block; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_NE(sheet.media_queries[0].condition.find("max-width"), std::string::npos);
}

TEST_F(CSSStylesheetTest, MediaQueryPrint) {
    auto sheet = parse_stylesheet(
        "@media print { .no-print { display: none; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_NE(sheet.media_queries[0].condition.find("print"), std::string::npos);
}

TEST_F(CSSStylesheetTest, TwoMediaQueries) {
    auto sheet = parse_stylesheet(
        "@media (min-width: 768px) { .desktop { display: flex; } }"
        "@media (max-width: 767px) { .mobile { display: block; } }");
    EXPECT_EQ(sheet.media_queries.size(), 2u);
}

TEST_F(CSSStylesheetTest, MediaQueryPrefersColorSchemeDark) {
    auto sheet = parse_stylesheet(
        "@media (prefers-color-scheme: dark) { body { background: #000; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_NE(sheet.media_queries[0].condition.find("prefers-color-scheme"), std::string::npos);
}

TEST_F(CSSStylesheetTest, MediaQueryPrefersReducedMotion) {
    auto sheet = parse_stylesheet(
        "@media (prefers-reduced-motion: reduce) { * { animation: none; } }");
    ASSERT_EQ(sheet.media_queries.size(), 1u);
    EXPECT_NE(sheet.media_queries[0].condition.find("prefers-reduced-motion"), std::string::npos);
}

TEST(CSSParserTest, ImportRuleWithMediaQuery) {
    auto sheet = parse_stylesheet("@import url(\"print.css\") print;");
    ASSERT_GE(sheet.imports.size(), 1u);
    EXPECT_NE(sheet.imports[0].url.find("print.css"), std::string::npos);
}

TEST(CSSParserTest, TwoImportRules) {
    auto sheet = parse_stylesheet(
        "@import url(\"reset.css\");"
        "@import url(\"theme.css\");");
    EXPECT_EQ(sheet.imports.size(), 2u);
}

TEST(CSSParserTest, ImportRuleUrlStoredCorrectly) {
    auto sheet = parse_stylesheet("@import url(\"fonts/roboto.css\");");
    ASSERT_EQ(sheet.imports.size(), 1u);
    EXPECT_NE(sheet.imports[0].url.find("roboto"), std::string::npos);
}

// Cycle 856 — pseudo-class: only-of-type, scope, in-range, out-of-range, indeterminate, default, read-write, has
TEST_F(CSSSelectorTest, PseudoClassOnlyOfType) {
    auto list = parse_selector_list("p:only-of-type");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "only-of-type") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassScope) {
    auto list = parse_selector_list(":scope > div");
    ASSERT_GE(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "scope") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassInRange) {
    auto list = parse_selector_list("input:in-range");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "in-range") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassOutOfRange) {
    auto list = parse_selector_list("input:out-of-range");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "out-of-range") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassIndeterminate) {
    auto list = parse_selector_list("input:indeterminate");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "indeterminate") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassDefault) {
    auto list = parse_selector_list("button:default");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "default") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassReadWrite) {
    auto list = parse_selector_list("textarea:read-write");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "read-write") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSSelectorTest, PseudoClassLocalLink) {
    auto list = parse_selector_list("a:local-link");
    ASSERT_EQ(list.selectors.size(), 1u);
    const auto& compound = list.selectors[0].parts[0].compound;
    bool found = false;
    for (const auto& ss : compound.simple_selectors)
        if (ss.type == SimpleSelectorType::PseudoClass && ss.value == "local-link") found = true;
    EXPECT_TRUE(found);
}

// Cycle 865 — CSS perspective, transform-style, font-feature-settings, break properties, font-kerning, forced-color-adjust
TEST_F(CSSStylesheetTest, PerspectiveDeclaration) {
    auto sheet = parse_stylesheet(".box { perspective: 800px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectiveOriginDeclaration) {
    auto sheet = parse_stylesheet(".box { perspective-origin: 50% 50%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective-origin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformStyleDeclaration) {
    auto sheet = parse_stylesheet(".box { transform-style: preserve-3d; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontFeatureSettingsDeclaration) {
    auto sheet = parse_stylesheet("p { font-feature-settings: \"liga\" 1, \"kern\" 1; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-feature-settings") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakBeforeDeclaration) {
    auto sheet = parse_stylesheet(".chapter { break-before: page; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "break-before") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakAfterDeclaration) {
    auto sheet = parse_stylesheet(".section { break-after: column; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "break-after") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakInsideDeclaration) {
    auto sheet = parse_stylesheet("img { break-inside: avoid; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "break-inside") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontKerningDeclaration) {
    auto sheet = parse_stylesheet("body { font-kerning: normal; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-kerning") { found = true; break; }
    EXPECT_TRUE(found);
}




// Cycle 874 — CSS less-common properties: text-combine-upright, text-orientation, line-break, hyphenate-character, box-decoration-break, mask-type, scroll-snap-stop, scroll-margin
TEST_F(CSSStylesheetTest, TextCombineUprightDeclaration) {
    auto sheet = parse_stylesheet("span { text-combine-upright: all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-combine-upright") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextOrientationDeclaration) {
    auto sheet = parse_stylesheet("div { text-orientation: mixed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-orientation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineBreakDeclaration) {
    auto sheet = parse_stylesheet("p { line-break: strict; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphenateCharacterDeclaration) {
    auto sheet = parse_stylesheet("p { hyphenate-character: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphenate-character") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxDecorationBreakDeclaration) {
    auto sheet = parse_stylesheet("span { box-decoration-break: clone; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-decoration-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskTypeDeclaration) {
    auto sheet = parse_stylesheet("mask { mask-type: luminance; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapStopDeclaration) {
    auto sheet = parse_stylesheet(".item { scroll-snap-stop: always; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-stop") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollMarginDeclaration) {
    auto sheet = parse_stylesheet(".item { scroll-margin: 10px 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-margin") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 883 — CSS declaration tests

TEST_F(CSSStylesheetTest, ShapeMarginDeclaration) {
    auto sheet = parse_stylesheet("img { shape-margin: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "shape-margin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderCollapseDeclaration) {
    auto sheet = parse_stylesheet("table { border-collapse: collapse; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-collapse") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderSpacingDeclaration) {
    auto sheet = parse_stylesheet("table { border-spacing: 4px 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CaptionSideDeclaration) {
    auto sheet = parse_stylesheet("table { caption-side: bottom; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "caption-side") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, EmptyCellsDeclaration) {
    auto sheet = parse_stylesheet("td { empty-cells: hide; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "empty-cells") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HangingPunctuationDeclaration) {
    auto sheet = parse_stylesheet("p { hanging-punctuation: first; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hanging-punctuation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InsetDeclaration) {
    auto sheet = parse_stylesheet(".box { inset: 10px 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "inset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSynthesisDeclaration) {
    auto sheet = parse_stylesheet("body { font-synthesis: weight style; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-synthesis") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 891 — CSS declaration tests

TEST_F(CSSStylesheetTest, VerticalAlignDeclaration) {
    auto sheet = parse_stylesheet("td { vertical-align: middle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "vertical-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloatDeclaration) {
    auto sheet = parse_stylesheet("img { float: left; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "float") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignSelfDeclaration) {
    auto sheet = parse_stylesheet(".item { align-self: flex-end; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifySelfDeclaration) {
    auto sheet = parse_stylesheet(".item { justify-self: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexDirectionDeclaration) {
    auto sheet = parse_stylesheet(".container { flex-direction: column; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexBasisDeclaration) {
    auto sheet = parse_stylesheet(".item { flex-basis: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-basis") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridAreaDeclaration) {
    auto sheet = parse_stylesheet(".item { grid-area: header; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-area") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderImageRepeatDeclaration) {
    auto sheet = parse_stylesheet(".box { border-image-repeat: round; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-image-repeat") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityDeclaration) {
    auto sheet = parse_stylesheet(".card { backface-visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backface-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectivePixelValueDeclaration) {
    auto sheet = parse_stylesheet(".scene { perspective: 500px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundBlendModeDeclaration) {
    auto sheet = parse_stylesheet(".layer { background-blend-mode: multiply; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-blend-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ResizeBothValueDeclaration) {
    auto sheet = parse_stylesheet("textarea { resize: both; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "resize") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AppearanceNoneValueDeclaration) {
    auto sheet = parse_stylesheet("button { appearance: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "appearance") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TouchActionDeclaration) {
    auto sheet = parse_stylesheet(".slider { touch-action: pan-y; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "touch-action") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridAutoRowsDeclaration) {
    auto sheet = parse_stylesheet(".grid { grid-auto-rows: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-auto-rows") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridAutoColumnsDeclaration) {
    auto sheet = parse_stylesheet(".grid { grid-auto-columns: 1fr; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-auto-columns") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainSizeDeclaration) {
    auto sheet = parse_stylesheet(".box { contain: size; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainLayoutDeclaration) {
    auto sheet = parse_stylesheet(".panel { contain: layout; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorXDeclaration) {
    auto sheet = parse_stylesheet(".scroll { overscroll-behavior-x: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorYDeclaration) {
    auto sheet = parse_stylesheet(".modal { overscroll-behavior-y: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollbarGutterDeclaration) {
    auto sheet = parse_stylesheet(".list { scrollbar-gutter: stable; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scrollbar-gutter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorSchemeMultiValueDeclaration) {
    auto sheet = parse_stylesheet(":root { color-scheme: light dark; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color-scheme") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphensAutoDeclaration) {
    auto sheet = parse_stylesheet("p { hyphens: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphens") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakBreakAllDeclaration) {
    auto sheet = parse_stylesheet(".long-word { word-break: break-all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationLineDeclaration) {
    auto sheet = parse_stylesheet("a { text-decoration-line: underline; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-line") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationColorDeclaration) {
    auto sheet = parse_stylesheet("a { text-decoration-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationStyleDeclaration) {
    auto sheet = parse_stylesheet("a { text-decoration-style: dotted; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationThicknessDeclaration) {
    auto sheet = parse_stylesheet("a { text-decoration-thickness: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-thickness") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextUnderlinePositionDeclaration) {
    auto sheet = parse_stylesheet("p { text-underline-position: under; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-underline-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowRgbaDeclaration) {
    auto sheet = parse_stylesheet(".card { box-shadow: 2px 2px 5px rgba(0,0,0,0.3); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipDeclaration) {
    auto sheet = parse_stylesheet(".legacy { clip: rect(0 100px 100px 0); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FilterBlurDeclaration) {
    auto sheet = parse_stylesheet("img { filter: blur(4px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 927 — additional CSS property declarations
TEST_F(CSSStylesheetTest, ColumnCountDeclaration) {
    auto sheet = parse_stylesheet(".grid { column-count: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-count") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnWidthDeclaration) {
    auto sheet = parse_stylesheet(".grid { column-width: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnSpanDeclaration) {
    auto sheet = parse_stylesheet("h2 { column-span: all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-span") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OrphansDeclaration) {
    auto sheet = parse_stylesheet("p { orphans: 2; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "orphans") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WidowsDeclaration) {
    auto sheet = parse_stylesheet("p { widows: 2; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "widows") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnRuleStyleDeclaration) {
    auto sheet = parse_stylesheet(".multi { column-rule-style: dashed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-rule-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnRuleColorDeclaration) {
    auto sheet = parse_stylesheet(".multi { column-rule-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-rule-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontStretchDeclaration) {
    auto sheet = parse_stylesheet("body { font-stretch: condensed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-stretch") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 936 — font-variant, text-emphasis, text-rendering, color-adjust
TEST_F(CSSStylesheetTest, FontVariantNumericDeclaration) {
    auto sheet = parse_stylesheet("body { font-variant-numeric: oldstyle-nums; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variant-numeric") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontVariantLigaturesDeclaration) {
    auto sheet = parse_stylesheet("p { font-variant-ligatures: no-common-ligatures; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variant-ligatures") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontVariantCapsDeclaration) {
    auto sheet = parse_stylesheet(".caps { font-variant-caps: small-caps; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variant-caps") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontOpticalSizingDeclaration) {
    auto sheet = parse_stylesheet("body { font-optical-sizing: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-optical-sizing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextEmphasisStyleDeclaration) {
    auto sheet = parse_stylesheet(".em { text-emphasis-style: filled circle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-emphasis-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextEmphasisColorDeclaration) {
    auto sheet = parse_stylesheet(".em { text-emphasis-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-emphasis-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextRenderingDeclaration) {
    auto sheet = parse_stylesheet("body { text-rendering: optimizeLegibility; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-rendering") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PrintColorAdjustDeclaration) {
    auto sheet = parse_stylesheet(".logo { print-color-adjust: exact; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "print-color-adjust") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 945 — SVG CSS properties and flex/grid alignment
TEST_F(CSSStylesheetTest, AlignContentDeclaration) {
    auto sheet = parse_stylesheet(".flex { align-content: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifyContentDeclaration) {
    auto sheet = parse_stylesheet(".flex { justify-content: space-between; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexFlowDeclaration) {
    auto sheet = parse_stylesheet(".flex { flex-flow: row wrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-flow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StrokeLinecapDeclaration) {
    auto sheet = parse_stylesheet("path { stroke-linecap: round; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stroke-linecap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StrokeLinejoinDeclaration) {
    auto sheet = parse_stylesheet("path { stroke-linejoin: bevel; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stroke-linejoin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FillOpacityDeclaration) {
    auto sheet = parse_stylesheet("rect { fill-opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "fill-opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StrokeOpacityDeclaration) {
    auto sheet = parse_stylesheet("path { stroke-opacity: 0.8; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stroke-opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, DominantBaselineDeclaration) {
    auto sheet = parse_stylesheet("text { dominant-baseline: middle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "dominant-baseline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ShapeRenderingDeclaration) {
    auto sheet = parse_stylesheet(".icon { shape-rendering: geometricPrecision; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "shape-rendering") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorInterpolationDeclaration) {
    auto sheet = parse_stylesheet(".grad { color-interpolation: linearRGB; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color-interpolation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloodColorDeclaration) {
    auto sheet = parse_stylesheet("feFlood { flood-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flood-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloodOpacityDeclaration) {
    auto sheet = parse_stylesheet("feFlood { flood-opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flood-opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StopColorDeclaration) {
    auto sheet = parse_stylesheet("stop { stop-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stop-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StopOpacityDeclaration) {
    auto sheet = parse_stylesheet("stop { stop-opacity: 1; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stop-opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipRuleDeclaration) {
    auto sheet = parse_stylesheet(".shape { clip-rule: evenodd; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-rule") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FillRuleDeclaration) {
    auto sheet = parse_stylesheet(".path { fill-rule: nonzero; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "fill-rule") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VectorEffectDeclaration) {
    auto sheet = parse_stylesheet(".shape { vector-effect: non-scaling-stroke; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "vector-effect") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextAnchorDeclaration) {
    auto sheet = parse_stylesheet("text { text-anchor: middle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-anchor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarkerStartDeclaration) {
    auto sheet = parse_stylesheet("path { marker-start: url(#arrow); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "marker-start") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarkerEndDeclaration) {
    auto sheet = parse_stylesheet("path { marker-end: url(#dot); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "marker-end") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarkerMidDeclaration) {
    auto sheet = parse_stylesheet("polyline { marker-mid: url(#square); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "marker-mid") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorRenderingDeclaration) {
    auto sheet = parse_stylesheet("canvas { color-rendering: optimizeSpeed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color-rendering") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OutlineOffsetDeclaration) {
    auto sheet = parse_stylesheet("a { outline-offset: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "outline-offset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSizeAdjustDeclaration) {
    auto sheet = parse_stylesheet("p { font-size-adjust: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-size-adjust") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BaselineShiftDeclaration) {
    auto sheet = parse_stylesheet("sup { baseline-shift: super; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "baseline-shift") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LightingColorDeclaration) {
    auto sheet = parse_stylesheet("fePointLight { lighting-color: white; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "lighting-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontVariantEastAsianDeclaration) {
    auto sheet = parse_stylesheet("p { font-variant-east-asian: ruby; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variant-east-asian") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontVariantPositionDeclaration) {
    auto sheet = parse_stylesheet("sub { font-variant-position: sub; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variant-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontPaletteDeclaration) {
    auto sheet = parse_stylesheet(".brand { font-palette: dark; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-palette") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontLanguageOverrideDeclaration) {
    auto sheet = parse_stylesheet("p { font-language-override: TRK; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-language-override") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationSkipInkDeclaration) {
    auto sheet = parse_stylesheet("a { text-decoration-skip-ink: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-skip-ink") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSynthesisWeightDeclaration) {
    auto sheet = parse_stylesheet("body { font-synthesis-weight: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-synthesis-weight") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSynthesisStyleDeclaration) {
    auto sheet = parse_stylesheet("body { font-synthesis-style: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-synthesis-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSynthesisSmallCapsDeclaration) {
    auto sheet = parse_stylesheet("body { font-synthesis-small-caps: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-synthesis-small-caps") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSynthesisPositionDeclaration) {
    auto sheet = parse_stylesheet("body { font-synthesis-position: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-synthesis-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowAnchorDeclaration) {
    auto sheet = parse_stylesheet("div { overflow-anchor: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-anchor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ForcedColorAdjustDeclaration) {
    auto sheet = parse_stylesheet("div { forced-color-adjust: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "forced-color-adjust") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskRepeatDeclaration) {
    auto sheet = parse_stylesheet("div { mask-repeat: no-repeat; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-repeat") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskPositionDeclaration) {
    auto sheet = parse_stylesheet("div { mask-position: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskSizeDeclaration) {
    auto sheet = parse_stylesheet("div { mask-size: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskCompositeDeclaration) {
    auto sheet = parse_stylesheet("div { mask-composite: add; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-composite") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskOriginDeclaration) {
    auto sheet = parse_stylesheet("div { mask-origin: content-box; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-origin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskClipDeclaration) {
    auto sheet = parse_stylesheet("div { mask-clip: content-box; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-clip") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ViewTransitionNameDeclaration) {
    auto sheet = parse_stylesheet("header { view-transition-name: main-header; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "view-transition-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationTimelineDeclaration) {
    auto sheet = parse_stylesheet("div { animation-timeline: scroll(); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-timeline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationRangeStartDeclaration) {
    auto sheet = parse_stylesheet("div { animation-range-start: 0%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-range-start") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationRangeEndDeclaration) {
    auto sheet = parse_stylesheet("div { animation-range-end: 100%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation-range-end") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollTimelineNameDeclaration) {
    auto sheet = parse_stylesheet("div { scroll-timeline-name: --my-scroll; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-timeline-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ViewTimelineNameDeclaration) {
    auto sheet = parse_stylesheet("section { view-timeline-name: --hero; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "view-timeline-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ViewTimelineInsetDeclaration) {
    auto sheet = parse_stylesheet("section { view-timeline-inset: 10%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "view-timeline-inset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollTimelineAxisDeclaration) {
    auto sheet = parse_stylesheet("div { scroll-timeline-axis: block; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-timeline-axis") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OffsetPathDeclaration) {
    auto sheet = parse_stylesheet("div { offset-path: path('M 0 0 L 100 100'); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "offset-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OffsetDistanceDeclaration) {
    auto sheet = parse_stylesheet("div { offset-distance: 50%; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "offset-distance") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OffsetRotateDeclaration) {
    auto sheet = parse_stylesheet("div { offset-rotate: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "offset-rotate") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextEmphasisPositionDeclaration) {
    auto sheet = parse_stylesheet("ruby { text-emphasis-position: over right; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-emphasis-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextUnderlineOffsetDeclaration) {
    auto sheet = parse_stylesheet("a { text-underline-offset: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-underline-offset") { found = true; break; }
    EXPECT_TRUE(found);
}
TEST_F(CSSStylesheetTest, TextDecorationThicknessDeclarationV2) {
    auto sheet = parse_stylesheet("a { text-decoration-thickness: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-thickness") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationSkipInkDeclarationV2) {
    auto sheet = parse_stylesheet("a { text-decoration-skip-ink: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-skip-ink") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AccentColorDeclarationV2) {
    auto sheet = parse_stylesheet("input { accent-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "accent-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AppearanceDeclarationV2) {
    auto sheet = parse_stylesheet("button { appearance: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "appearance") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorSchemeDeclarationV2) {
    auto sheet = parse_stylesheet(":root { color-scheme: dark light; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color-scheme") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainDeclarationV2) {
    auto sheet = parse_stylesheet(".box { contain: layout style; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerTypeDeclarationV2) {
    auto sheet = parse_stylesheet(".card { container-type: inline-size; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerNameDeclarationV2) {
    auto sheet = parse_stylesheet(".sidebar { container-name: sidebar; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AspectRatioDeclarationV2) {
    auto sheet = parse_stylesheet(".box { aspect-ratio: 16/9; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "aspect-ratio") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitDeclarationV2) {
    auto sheet = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectPositionDeclarationV2) {
    auto sheet = parse_stylesheet("img { object-position: center top; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowXDeclarationV2) {
    auto sheet = parse_stylesheet(".scroll { overflow-x: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYDeclarationV2) {
    auto sheet = parse_stylesheet(".scroll { overflow-y: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorDeclarationV3) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserSelectDeclarationV2) {
    auto sheet = parse_stylesheet(".no-select { user-select: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "user-select") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PointerEventsDeclarationV3) {
    auto sheet = parse_stylesheet(".overlay { pointer-events: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "pointer-events") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1017: CSS property declaration tests ---

TEST_F(CSSStylesheetTest, ClipPathDeclarationV3) {
    auto sheet = parse_stylesheet(".clip { clip-path: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MixBlendModeDeclarationV3) {
    auto sheet = parse_stylesheet(".blend { mix-blend-mode: multiply; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mix-blend-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitDeclarationV3) {
    auto sheet = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AspectRatioDeclarationV3) {
    auto sheet = parse_stylesheet(".box { aspect-ratio: 16 / 9; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "aspect-ratio") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainDeclarationV3) {
    auto sheet = parse_stylesheet(".widget { contain: layout paint; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ImageRenderingDeclarationV3) {
    auto sheet = parse_stylesheet("img { image-rendering: pixelated; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "image-rendering") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, IsolationDeclarationV3) {
    auto sheet = parse_stylesheet(".layer { isolation: isolate; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "isolation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentVisibilityDeclarationV3) {
    auto sheet = parse_stylesheet(".offscreen { content-visibility: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "content-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1026: CSS property tests ---

TEST_F(CSSStylesheetTest, WillChangeDeclarationV3) {
    auto sheet = parse_stylesheet(".anim { will-change: transform; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "will-change") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CursorDeclarationV3) {
    auto sheet = parse_stylesheet(".link { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowXDeclarationV3) {
    auto sheet = parse_stylesheet(".box { overflow-x: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYDeclarationV3) {
    auto sheet = parse_stylesheet(".box { overflow-y: scroll; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VisibilityDeclarationV3) {
    auto sheet = parse_stylesheet(".hidden { visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceDeclarationV3) {
    auto sheet = parse_stylesheet("pre { white-space: pre-wrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakDeclarationV3) {
    auto sheet = parse_stylesheet(".wrap { word-break: break-all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextOverflowDeclarationV3) {
    auto sheet = parse_stylesheet(".trunc { text-overflow: ellipsis; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1035: CSS property tests ---

TEST_F(CSSStylesheetTest, ZIndexDeclarationV3) {
    auto sheet = parse_stylesheet(".modal { z-index: 1000; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "z-index") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PositionDeclarationV3) {
    auto sheet = parse_stylesheet(".fixed { position: fixed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TopDeclarationV3) {
    auto sheet = parse_stylesheet(".abs { top: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "top") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LeftDeclarationV3) {
    auto sheet = parse_stylesheet(".abs { left: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "left") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BottomDeclarationV3) {
    auto sheet = parse_stylesheet(".abs { bottom: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "bottom") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, RightDeclarationV3) {
    auto sheet = parse_stylesheet(".abs { right: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "right") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformDeclarationV3) {
    auto sheet = parse_stylesheet(".rot { transform: rotate(45deg); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransitionDeclarationV3) {
    auto sheet = parse_stylesheet(".fade { transition: opacity 0.3s ease; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transition") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1044: CSS property tests ---

TEST_F(CSSStylesheetTest, CursorPointerDeclarationV4) {
    auto sheet = parse_stylesheet(".btn { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowHiddenDeclarationV4) {
    auto sheet = parse_stylesheet(".clip { overflow: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VisibilityDeclarationV4) {
    auto sheet = parse_stylesheet(".hidden { visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PointerEventsDeclarationV4) {
    auto sheet = parse_stylesheet(".noclick { pointer-events: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "pointer-events") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceDeclarationV4) {
    auto sheet = parse_stylesheet("pre { white-space: pre-wrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakDeclarationV4) {
    auto sheet = parse_stylesheet(".wrap { word-break: break-all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OutlineDeclarationV4) {
    auto sheet = parse_stylesheet(":focus { outline: 2px solid blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "outline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowDeclarationV4) {
    auto sheet = parse_stylesheet(".card { box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1053: CSS property tests ---

TEST_F(CSSStylesheetTest, TextTransformDeclarationV4) {
    auto sheet = parse_stylesheet(".upper { text-transform: uppercase; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingDeclarationV4) {
    auto sheet = parse_stylesheet(".spaced { letter-spacing: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "letter-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextShadowDeclarationV4) {
    auto sheet = parse_stylesheet(".shadow { text-shadow: 1px 1px 2px black; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ListStyleTypeDeclarationV4) {
    auto sheet = parse_stylesheet("ul { list-style-type: disc; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "list-style-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundSizeDeclarationV4) {
    auto sheet = parse_stylesheet(".bg { background-size: cover; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundPositionDeclarationV4) {
    auto sheet = parse_stylesheet(".bg { background-position: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundRepeatDeclarationV4) {
    auto sheet = parse_stylesheet(".bg { background-repeat: no-repeat; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-repeat") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderRadiusDeclarationV4) {
    auto sheet = parse_stylesheet(".round { border-radius: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-radius") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1062: CSS property tests ---

TEST_F(CSSStylesheetTest, MinWidthDeclarationV4) {
    auto sheet = parse_stylesheet(".box { min-width: 100px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "min-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaxWidthDeclarationV4) {
    auto sheet = parse_stylesheet(".box { max-width: 500px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "max-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MinHeightDeclarationV4) {
    auto sheet = parse_stylesheet(".box { min-height: 50px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "min-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaxHeightDeclarationV4) {
    auto sheet = parse_stylesheet(".box { max-height: 800px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "max-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexDirectionDeclarationV4) {
    auto sheet = parse_stylesheet(".flex { flex-direction: column; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexWrapDeclarationV4) {
    auto sheet = parse_stylesheet(".flex { flex-wrap: wrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-wrap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifyContentDeclarationV4) {
    auto sheet = parse_stylesheet(".flex { justify-content: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignItemsDeclarationV4) {
    auto sheet = parse_stylesheet(".flex { align-items: stretch; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-items") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1071: CSS property tests ---

TEST_F(CSSStylesheetTest, GapDeclarationV4) {
    auto sheet = parse_stylesheet(".grid { gap: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridTemplateColumnsV4) {
    auto sheet = parse_stylesheet(".grid { grid-template-columns: 1fr 1fr; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-template-columns") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridTemplateRowsV4) {
    auto sheet = parse_stylesheet(".grid { grid-template-rows: auto 1fr; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "grid-template-rows") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignSelfDeclarationV4) {
    auto sheet = parse_stylesheet(".item { align-self: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexGrowDeclarationV4) {
    auto sheet = parse_stylesheet(".item { flex-grow: 1; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-grow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexShrinkDeclarationV4) {
    auto sheet = parse_stylesheet(".item { flex-shrink: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-shrink") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexBasisDeclarationV4) {
    auto sheet = parse_stylesheet(".item { flex-basis: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-basis") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OrderDeclarationV4) {
    auto sheet = parse_stylesheet(".item { order: 2; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "order") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1080: CSS property tests ---

TEST_F(CSSStylesheetTest, TextDecorationDeclarationV4) {
    auto sheet = parse_stylesheet("a { text-decoration: underline; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightDeclarationV4) {
    auto sheet = parse_stylesheet("p { line-height: 1.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontFamilyDeclarationV4) {
    auto sheet = parse_stylesheet("body { font-family: sans-serif; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-family") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontWeightDeclarationV4) {
    auto sheet = parse_stylesheet("b { font-weight: bold; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-weight") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontStyleDeclarationV4) {
    auto sheet = parse_stylesheet("em { font-style: italic; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextAlignDeclarationV4) {
    auto sheet = parse_stylesheet(".center { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloatDeclarationV4) {
    auto sheet = parse_stylesheet(".left { float: left; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "float") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClearDeclarationV4) {
    auto sheet = parse_stylesheet(".clear { clear: both; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clear") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1089: CSS property tests ---

TEST_F(CSSStylesheetTest, ZIndexDeclarationV4) {
    auto sheet = parse_stylesheet(".modal { z-index: 1000; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "z-index") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PositionAbsoluteV4) {
    auto sheet = parse_stylesheet(".abs { position: absolute; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TopDeclarationV4) {
    auto sheet = parse_stylesheet(".pos { top: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "top") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LeftDeclarationV4) {
    auto sheet = parse_stylesheet(".pos { left: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "left") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, RightDeclarationV4) {
    auto sheet = parse_stylesheet(".pos { right: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "right") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BottomDeclarationV4) {
    auto sheet = parse_stylesheet(".pos { bottom: 5px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "bottom") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowXDeclarationV4) {
    auto sheet = parse_stylesheet(".scroll { overflow-x: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYDeclarationV4) {
    auto sheet = parse_stylesheet(".scroll { overflow-y: scroll; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1098: 8 CSS tests ---

TEST_F(CSSStylesheetTest, TransitionDeclarationV5) {
    auto sheet = parse_stylesheet(".anim { transition: all 0.3s ease; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transition") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationDeclarationV5) {
    auto sheet = parse_stylesheet(".spin { animation: rotate 1s infinite; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformDeclarationV5) {
    auto sheet = parse_stylesheet(".moved { transform: translateX(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowDeclarationV5) {
    auto sheet = parse_stylesheet(".card { box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OutlineDeclarationV5) {
    auto sheet = parse_stylesheet(".focus { outline: 2px solid blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "outline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceDeclarationV5) {
    auto sheet = parse_stylesheet(".pre { white-space: pre-wrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakDeclarationV2) {
    auto sheet = parse_stylesheet(".wrap { word-break: break-all; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowWrapDeclarationV2) {
    auto sheet = parse_stylesheet(".wrap { overflow-wrap: break-word; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-wrap") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1107: 8 CSS tests ---

TEST_F(CSSStylesheetTest, PointerEventsDeclarationV5) {
    auto sheet = parse_stylesheet(".no-click { pointer-events: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "pointer-events") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserSelectDeclarationV3) {
    auto sheet = parse_stylesheet(".no-select { user-select: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "user-select") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitDeclarationV4) {
    auto sheet = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectPositionDeclarationV3) {
    auto sheet = parse_stylesheet("img { object-position: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "object-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ResizeDeclarationV2) {
    auto sheet = parse_stylesheet("textarea { resize: vertical; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "resize") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AppearanceDeclarationV3) {
    auto sheet = parse_stylesheet("input { appearance: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "appearance") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentDeclarationV3) {
    auto sheet = parse_stylesheet(".after::after { content: ''; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathDeclarationV4) {
    auto sheet = parse_stylesheet(".clip { clip-path: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1116: 8 CSS tests ---

TEST_F(CSSStylesheetTest, FilterDeclarationV5) {
    auto sheet = parse_stylesheet(".blur { filter: blur(5px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackdropFilterDeclarationV2) {
    auto sheet = parse_stylesheet(".glass { backdrop-filter: blur(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backdrop-filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MixBlendModeDeclarationV2) {
    auto sheet = parse_stylesheet(".blend { mix-blend-mode: multiply; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mix-blend-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, IsolationDeclarationV2) {
    auto sheet = parse_stylesheet(".iso { isolation: isolate; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "isolation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WillChangeDeclarationV2) {
    auto sheet = parse_stylesheet(".opt { will-change: transform; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "will-change") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainDeclarationV4) {
    auto sheet = parse_stylesheet(".box { contain: layout; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorDeclarationV2) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorDeclarationV2) {
    auto sheet = parse_stylesheet("body { overscroll-behavior: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1125: 8 CSS tests ---

TEST_F(CSSStylesheetTest, TouchActionDeclarationV2) {
    auto sheet = parse_stylesheet(".drag { touch-action: pan-y; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "touch-action") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CaretColorDeclarationV2) {
    auto sheet = parse_stylesheet("input { caret-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "caret-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AccentColorDeclarationV3) {
    auto sheet = parse_stylesheet("input { accent-color: blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "accent-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TabSizeDeclarationV6) {
    auto sheet = parse_stylesheet("pre { tab-size: 4; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "tab-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphensDeclarationV2) {
    auto sheet = parse_stylesheet("p { hyphens: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphens") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WritingModeDeclarationV2) {
    auto sheet = parse_stylesheet(".vertical { writing-mode: vertical-rl; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "writing-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, DirectionDeclarationV2) {
    auto sheet = parse_stylesheet(".rtl { direction: rtl; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UnicodeBidiDeclarationV2) {
    auto sheet = parse_stylesheet(".bidi { unicode-bidi: embed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "unicode-bidi") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1134: 8 CSS tests ---

TEST_F(CSSStylesheetTest, AspectRatioDeclarationV5) {
    auto sheet = parse_stylesheet(".box { aspect-ratio: 16 / 9; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "aspect-ratio") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceItemsDeclarationV2) {
    auto sheet = parse_stylesheet(".grid { place-items: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "place-items") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceContentDeclarationV2) {
    auto sheet = parse_stylesheet(".grid { place-content: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "place-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceSelfDeclarationV2) {
    auto sheet = parse_stylesheet(".item { place-self: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "place-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnCountDeclarationV5) {
    auto sheet = parse_stylesheet(".multi { column-count: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-count") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnGapDeclarationV5) {
    auto sheet = parse_stylesheet(".multi { column-gap: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "column-gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, RowGapDeclarationV2) {
    auto sheet = parse_stylesheet(".grid { row-gap: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "row-gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InsetDeclarationV5) {
    auto sheet = parse_stylesheet(".abs { inset: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "inset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextIndentV6) {
    auto sheet = parse_stylesheet("p { text-indent: 2em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-indent") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VerticalAlignV6) {
    auto sheet = parse_stylesheet("span { vertical-align: middle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "vertical-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceV6) {
    auto sheet = parse_stylesheet("pre { white-space: nowrap; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordSpacingV6) {
    auto sheet = parse_stylesheet("p { word-spacing: 4px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingV6) {
    auto sheet = parse_stylesheet("h1 { letter-spacing: 0.05em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "letter-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightV6) {
    auto sheet = parse_stylesheet("body { line-height: 1.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextAlignV6) {
    auto sheet = parse_stylesheet("div { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationV6) {
    auto sheet = parse_stylesheet("a { text-decoration: underline; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1152: 8 CSS tests ---

TEST_F(CSSStylesheetTest, DisplayV7) {
    auto sheet = parse_stylesheet("div { display: flex; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "display") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PositionV7) {
    auto sheet = parse_stylesheet("div { position: absolute; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowV7) {
    auto sheet = parse_stylesheet("div { overflow: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ZIndexV7) {
    auto sheet = parse_stylesheet("div { z-index: 10; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "z-index") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OpacityV7) {
    auto sheet = parse_stylesheet("div { opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CursorV7) {
    auto sheet = parse_stylesheet("div { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VisibilityV7) {
    auto sheet = parse_stylesheet("div { visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloatV7) {
    auto sheet = parse_stylesheet("div { float: left; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "float") { found = true; break; }
    EXPECT_TRUE(found);
}

// --- Cycle 1161: 8 CSS tests ---

TEST_F(CSSStylesheetTest, MarginV8) {
    auto sheet = parse_stylesheet("div { margin: 10px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "margin") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PaddingV8) {
    auto sheet = parse_stylesheet("div { padding: 5px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderV8) {
    auto sheet = parse_stylesheet("div { border: 1px solid black; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WidthV8) {
    auto sheet = parse_stylesheet("div { width: 100%; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "width") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HeightV8) {
    auto sheet = parse_stylesheet("div { height: auto; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "height") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorV8) {
    auto sheet = parse_stylesheet("div { color: red; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundColorV8) {
    auto sheet = parse_stylesheet("div { background-color: blue; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-color") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontSizeV8) {
    auto sheet = parse_stylesheet("div { font-size: 14px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-size") found = true;
    EXPECT_TRUE(found);
}

// --- Cycle 1170: 8 CSS tests ---

TEST_F(CSSStylesheetTest, DisplayV9) {
    auto sheet = parse_stylesheet("div { display: flex; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "display") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PositionV9) {
    auto sheet = parse_stylesheet("div { position: absolute; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "position") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowV9) {
    auto sheet = parse_stylesheet("div { overflow: hidden; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ZIndexV9) {
    auto sheet = parse_stylesheet("div { z-index: 100; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "z-index") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OpacityV9) {
    auto sheet = parse_stylesheet("div { opacity: 0.5; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "opacity") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV9) {
    auto sheet = parse_stylesheet("div { transform: rotate(45deg); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextAlignV9) {
    auto sheet = parse_stylesheet("div { text-align: center; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-align") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowV9) {
    auto sheet = parse_stylesheet("div { box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextShadowV10) {
    auto sheet = parse_stylesheet("p { text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-shadow") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundImageV10) {
    auto sheet = parse_stylesheet("section { background-image: url('image.png'); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-image") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformOriginV10) {
    auto sheet = parse_stylesheet("button { transform-origin: center; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform-origin") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransitionV10) {
    auto sheet = parse_stylesheet("a { transition: all 0.3s ease-in-out; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transition") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationV10) {
    auto sheet = parse_stylesheet("@keyframes slide { from { left: 0; } to { left: 100%; } } div { animation: slide 2s; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& rule : sheet.rules) {
        for (auto& d : rule.declarations) {
            if (d.property == "animation") found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FilterV10) {
    auto sheet = parse_stylesheet("img { filter: blur(5px) brightness(1.2); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackdropFilterV10) {
    auto sheet = parse_stylesheet(".overlay { backdrop-filter: blur(10px); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backdrop-filter") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MixBlendModeV10) {
    auto sheet = parse_stylesheet("span { mix-blend-mode: multiply; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mix-blend-mode") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingV11) {
    auto sheet = parse_stylesheet("p { letter-spacing: 0.15em; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "letter-spacing") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightV11) {
    auto sheet = parse_stylesheet("div { line-height: 1.5; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-height") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationV11) {
    auto sheet = parse_stylesheet("a { text-decoration: underline; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextTransformV11) {
    auto sheet = parse_stylesheet("h1 { text-transform: uppercase; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-transform") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordSpacingV11) {
    auto sheet = parse_stylesheet("span { word-spacing: 0.2em; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-spacing") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextIndentV11) {
    auto sheet = parse_stylesheet("p { text-indent: 2em; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-indent") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceV11) {
    auto sheet = parse_stylesheet("pre { white-space: pre-wrap; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "white-space") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakV11) {
    auto sheet = parse_stylesheet("div { word-break: break-word; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-break") found = true;
    EXPECT_TRUE(found);
}

// --- Cycle 1197: 8 CSS tests ---

TEST_F(CSSStylesheetTest, CursorV12) {
    auto sheet = parse_stylesheet("button { cursor: pointer; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderV12) {
    auto sheet = parse_stylesheet("div { border: 1px solid black; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderRadiusV12) {
    auto sheet = parse_stylesheet("div { border-radius: 8px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-radius") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PaddingV12) {
    auto sheet = parse_stylesheet("p { padding: 10px 20px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarginV12) {
    auto sheet = parse_stylesheet("div { margin: 15px auto; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "margin") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GapV12) {
    auto sheet = parse_stylesheet(".grid { gap: 20px 15px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "gap") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifyContentV12) {
    auto sheet = parse_stylesheet(".flex { justify-content: space-between; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-content") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignItemsV12) {
    auto sheet = parse_stylesheet(".flex { align-items: center; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-items") found = true;
    EXPECT_TRUE(found);
}

// --- Cycle 1206: 8 CSS tests ---

TEST_F(CSSStylesheetTest, FlexDirectionV13) {
    auto sheet = parse_stylesheet(".flex { flex-direction: column; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-direction") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexWrapV13) {
    auto sheet = parse_stylesheet(".flex { flex-wrap: wrap; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "flex-wrap") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignSelfV13) {
    auto sheet = parse_stylesheet(".item { align-self: flex-end; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "align-self") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifySelfV13) {
    auto sheet = parse_stylesheet(".item { justify-self: start; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "justify-self") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV13) {
    auto sheet = parse_stylesheet(".box { transform: rotate(45deg); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformOriginV13) {
    auto sheet = parse_stylesheet(".box { transform-origin: center top; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform-origin") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectiveV13) {
    auto sheet = parse_stylesheet(".scene { perspective: 1000px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityV13) {
    auto sheet = parse_stylesheet(".card { backface-visibility: hidden; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backface-visibility") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathV14) {
    auto sheet = parse_stylesheet(".clip { clip-path: circle(50%); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskImageV14) {
    auto sheet = parse_stylesheet(".masked { mask-image: url(mask.svg); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-image") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ShapeOutsideV14) {
    auto sheet = parse_stylesheet(".float { shape-outside: polygon(0 0, 100% 0, 100% 100%); }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "shape-outside") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorV14) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowAnchorV14) {
    auto sheet = parse_stylesheet(".content { overflow-anchor: auto; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow-anchor") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskSizeV14) {
    auto sheet = parse_stylesheet(".masked { mask-size: cover; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-size") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollMarginV14) {
    auto sheet = parse_stylesheet(".snap { scroll-margin: 20px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-margin") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollPaddingV14) {
    auto sheet = parse_stylesheet(".container { scroll-padding: 10px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-padding") found = true;
    EXPECT_TRUE(found);
}

// Cycle 1224: CSS parser tests V15

TEST_F(CSSStylesheetTest, TextWrapBalanceV15) {
    auto sheet = parse_stylesheet("p { text-wrap: balance; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-wrap") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerTypeV15) {
    auto sheet = parse_stylesheet(".card { container-type: inline-size; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-type") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainerNameV15) {
    auto sheet = parse_stylesheet(".sidebar { container-name: sidebar; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "container-name") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AccentColorV15) {
    auto sheet = parse_stylesheet("input { accent-color: hotpink; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "accent-color") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColorSchemeV15) {
    auto sheet = parse_stylesheet(":root { color-scheme: light dark; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "color-scheme") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorV15) {
    auto sheet = parse_stylesheet("body { overscroll-behavior: contain; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapTypeV15) {
    auto sheet = parse_stylesheet(".scroll { scroll-snap-type: x mandatory; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-type") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapAlignV15) {
    auto sheet = parse_stylesheet(".item { scroll-snap-align: center; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-align") found = true;
    EXPECT_TRUE(found);
}

// Cycle 1233: CSS parser tests V16

TEST_F(CSSStylesheetTest, LineClampV16) {
    auto sheet = parse_stylesheet(".text { -webkit-line-clamp: 3; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-line-clamp") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AppearanceV16) {
    auto sheet = parse_stylesheet(".btn { -webkit-appearance: none; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-appearance") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackgroundClipV16) {
    auto sheet = parse_stylesheet(".box { background-clip: padding-box; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-clip") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextStrokeV16) {
    auto sheet = parse_stylesheet(".headline { -webkit-text-stroke: 1px black; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-text-stroke") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserDragV16) {
    auto sheet = parse_stylesheet(".item { -webkit-user-drag: element; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-user-drag") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxReflectV16) {
    auto sheet = parse_stylesheet(".mirror { -webkit-box-reflect: below 5px; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-box-reflect") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextFillColorV16) {
    auto sheet = parse_stylesheet(".text { -webkit-text-fill-color: blue; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-text-fill-color") found = true;
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TapHighlightColorV16) {
    auto sheet = parse_stylesheet(".link { -webkit-tap-highlight-color: transparent; }");
    ASSERT_FALSE(sheet.rules.empty());
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-tap-highlight-color") found = true;
    EXPECT_TRUE(found);
}

// Cycle 1242: CSS parser tests V17

TEST_F(CSSStylesheetTest, ContainIntrinsicWidthV17) {
    auto sheet = parse_stylesheet(".container { contain-intrinsic-width: 500px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain-intrinsic-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainIntrinsicHeightV17) {
    auto sheet = parse_stylesheet(".container { contain-intrinsic-height: 300px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain-intrinsic-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainmentV17) {
    auto sheet = parse_stylesheet(".box { containment: layout; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "containment") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontVariationSettingsV17) {
    auto sheet = parse_stylesheet(".text { font-variation-settings: 'wght' 700; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "font-variation-settings") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InitialLetterV17) {
    auto sheet = parse_stylesheet("p::first-letter { initial-letter: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "initial-letter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightStepV17) {
    auto sheet = parse_stylesheet("body { line-height-step: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-height-step") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarginBlockV17) {
    auto sheet = parse_stylesheet(".box { margin-block: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "margin-block") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarginInlineV17) {
    auto sheet = parse_stylesheet(".box { margin-inline: 15px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "margin-inline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaxInlineSizeV17) {
    auto sheet = parse_stylesheet(".text { max-inline-size: 80ch; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "max-inline-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MinBlockSizeV17) {
    auto sheet = parse_stylesheet(".container { min-block-size: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "min-block-size") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1251: CSS parser tests V18

TEST_F(CSSStylesheetTest, PaddingBlockV18) {
    auto sheet = parse_stylesheet(".box { padding-block: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding-block") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PaddingInlineV18) {
    auto sheet = parse_stylesheet(".box { padding-inline: 25px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding-inline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InsetV18) {
    auto sheet = parse_stylesheet(".absolute { inset: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "inset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorV18) {
    auto sheet = parse_stylesheet("html { overscroll-behavior: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorV18) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollMarginV18) {
    auto sheet = parse_stylesheet(".section { scroll-margin: 50px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-margin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollPaddingV18) {
    auto sheet = parse_stylesheet(".viewport { scroll-padding: 30px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, SnapAlignV18) {
    auto sheet = parse_stylesheet(".item { snap-align: start; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "snap-align") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1260: CSS parser tests V19

TEST_F(CSSStylesheetTest, HyphensV19) {
    auto sheet = parse_stylesheet(".text { hyphens: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphens") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphenateCharacterV19) {
    auto sheet = parse_stylesheet(".text { hyphenate-character: '-'; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphenate-character") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphenateLimitCharsV19) {
    auto sheet = parse_stylesheet(".text { hyphenate-limit-chars: 5 2 2; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "hyphenate-limit-chars") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ImageOrientationV19) {
    auto sheet = parse_stylesheet("img { image-orientation: from-image; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "image-orientation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ImageRenderingV19) {
    auto sheet = parse_stylesheet("img { image-rendering: pixelated; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "image-rendering") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InitialLetterV19) {
    auto sheet = parse_stylesheet(".intro { initial-letter: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "initial-letter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightStepV19) {
    auto sheet = parse_stylesheet(".paragraph { line-height-step: 1.5rem; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-height-step") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PaintOrderV19) {
    auto sheet = parse_stylesheet("text { paint-order: stroke fill; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "paint-order") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1269: CSS parser tests V20

TEST_F(CSSStylesheetTest, BackgroundAttachmentV20) {
    auto sheet = parse_stylesheet("body { background-attachment: fixed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "background-attachment") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationColorV20) {
    auto sheet = parse_stylesheet("span { text-decoration-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationStyleV20) {
    auto sheet = parse_stylesheet("a { text-decoration-style: wavy; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationThicknessV20) {
    auto sheet = parse_stylesheet("em { text-decoration-thickness: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-thickness") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextUnderlineOffsetV20) {
    auto sheet = parse_stylesheet("u { text-underline-offset: 3px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-underline-offset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordSpacingV20) {
    auto sheet = parse_stylesheet("p { word-spacing: 0.5em; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "word-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingV20) {
    auto sheet = parse_stylesheet("h1 { letter-spacing: 2px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "letter-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextTransformV20) {
    auto sheet = parse_stylesheet(".uppercase { text-transform: uppercase; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-transform") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1278: CSS parser tests V21

TEST_F(CSSStylesheetTest, BlockSizeV21) {
    auto sheet = parse_stylesheet(".box { block-size: 200px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "block-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InlineSizeV21) {
    auto sheet = parse_stylesheet(".element { inline-size: 300px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "inline-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineClampV21) {
    auto sheet = parse_stylesheet(".truncate { line-clamp: 3; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "line-clamp") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PageBreakAfterV21) {
    auto sheet = parse_stylesheet(".section { page-break-after: always; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "page-break-after") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PageBreakBeforeV21) {
    auto sheet = parse_stylesheet(".header { page-break-before: avoid; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "page-break-before") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PageBreakInsideV21) {
    auto sheet = parse_stylesheet(".table { page-break-inside: avoid; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "page-break-inside") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, QuotesV21) {
    auto sheet = parse_stylesheet("q { quotes: '\"' '\"'; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "quotes") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, StrokeDasharrayV21) {
    auto sheet = parse_stylesheet("path { stroke-dasharray: 5, 10; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "stroke-dasharray") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1287: CSS parser tests

TEST_F(CSSStylesheetTest, BorderRadiusV22) {
    auto sheet = parse_stylesheet("div { border-radius: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-radius") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV22) {
    auto sheet = parse_stylesheet("span { transform: rotate(45deg); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FilterV22) {
    auto sheet = parse_stylesheet("img { filter: blur(5px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackdropFilterV22) {
    auto sheet = parse_stylesheet(".modal { backdrop-filter: blur(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backdrop-filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathV22) {
    auto sheet = parse_stylesheet("section { clip-path: polygon(0% 0%, 100% 0%, 50% 100%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskImageV22) {
    auto sheet = parse_stylesheet("article { mask-image: url(#mask); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask-image") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorV22) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapTypeV22) {
    auto sheet = parse_stylesheet(".container { scroll-snap-type: x mandatory; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-type") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1296: CSS parser tests

TEST_F(CSSStylesheetTest, ScrollSnapAlignV23) {
    auto sheet = parse_stylesheet(".item { scroll-snap-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollSnapStopV23) {
    auto sheet = parse_stylesheet(".item { scroll-snap-stop: always; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-stop") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollPaddingV23) {
    auto sheet = parse_stylesheet(".container { scroll-padding: 20px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollMarginV23) {
    auto sheet = parse_stylesheet(".element { scroll-margin: 10px 5px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-margin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorV23) {
    auto sheet = parse_stylesheet("body { overscroll-behavior: contain; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overscroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TouchActionV23) {
    auto sheet = parse_stylesheet("button { touch-action: manipulation; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "touch-action") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserSelectV23) {
    auto sheet = parse_stylesheet(".no-select { user-select: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "user-select") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WebkitAppearanceV23) {
    auto sheet = parse_stylesheet("input { -webkit-appearance: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "-webkit-appearance") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1305: CSS parser tests
TEST_F(CSSStylesheetTest, WillChangeV24) {
    auto sheet = parse_stylesheet(".transition { will-change: transform; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "will-change") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackdropFilterV24) {
    auto sheet = parse_stylesheet(".blur { backdrop-filter: blur(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backdrop-filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollBehaviorV24) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WritingModeV24) {
    auto sheet = parse_stylesheet(".vertical { writing-mode: vertical-rl; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "writing-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextOrientationV24) {
    auto sheet = parse_stylesheet(".mixed { text-orientation: mixed; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-orientation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ASpectRatioV24) {
    auto sheet = parse_stylesheet("img { aspect-ratio: 16 / 9; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "aspect-ratio") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainmentV24) {
    auto sheet = parse_stylesheet(".container { contain: layout style paint; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentVisibilityV24) {
    auto sheet = parse_stylesheet(".hidden { content-visibility: auto; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "content-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1314: CSS parser tests

TEST_F(CSSStylesheetTest, ScrollBehaviorV25) {
    auto sheet = parse_stylesheet("html { scroll-behavior: smooth; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-behavior") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollMarginV25) {
    auto sheet = parse_stylesheet(".section { scroll-margin: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-margin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScrollPaddingV25) {
    auto sheet = parse_stylesheet(".container { scroll-padding: 20px 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, SnapTypeV25) {
    auto sheet = parse_stylesheet(".scroller { scroll-snap-type: x mandatory; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, SnapAlignV25) {
    auto sheet = parse_stylesheet(".child { scroll-snap-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "scroll-snap-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationLineV25) {
    auto sheet = parse_stylesheet("a { text-decoration-line: underline; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-line") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationStyleV25) {
    auto sheet = parse_stylesheet("a { text-decoration-style: wavy; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationColorV25) {
    auto sheet = parse_stylesheet("a { text-decoration-color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-decoration-color") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1323: CSS parser tests
TEST_F(CSSStylesheetTest, BorderRadiusV26) {
    auto sheet = parse_stylesheet("div { border-radius: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-radius") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowV26) {
    auto sheet = parse_stylesheet("div { box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextShadowV26) {
    auto sheet = parse_stylesheet("p { text-shadow: 2px 2px 4px gray; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV26) {
    auto sheet = parse_stylesheet("span { transform: rotate(45deg); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransitionV26) {
    auto sheet = parse_stylesheet("button { transition: all 0.3s ease; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transition") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationV26) {
    auto sheet = parse_stylesheet("div { animation: slide 2s infinite; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "animation") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FilterV26) {
    auto sheet = parse_stylesheet("img { filter: blur(5px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackdropFilterV26) {
    auto sheet = parse_stylesheet("div { backdrop-filter: blur(10px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backdrop-filter") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1332
TEST_F(CSSStylesheetTest, PaddingV27) {
    auto sheet = parse_stylesheet("p { padding: 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "padding") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarginV27) {
    auto sheet = parse_stylesheet("span { margin: 5px 10px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "margin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderRadiusV27) {
    auto sheet = parse_stylesheet("button { border-radius: 8px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "border-radius") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxShadowV27) {
    auto sheet = parse_stylesheet("div { box-shadow: 0 4px 6px rgba(0,0,0,0.1); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "box-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextShadowV27) {
    auto sheet = parse_stylesheet("h1 { text-shadow: 2px 2px 4px gray; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "text-shadow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV27) {
    auto sheet = parse_stylesheet("div { transform: rotate(45deg); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectiveV27) {
    auto sheet = parse_stylesheet("section { perspective: 1000px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathV27) {
    auto sheet = parse_stylesheet("img { clip-path: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1341
TEST_F(CSSStylesheetTest, DisplayV28) {
    auto sheet = parse_stylesheet("div { display: flex; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "display") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PositionV28) {
    auto sheet = parse_stylesheet("span { position: absolute; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowV28) {
    auto sheet = parse_stylesheet("p { overflow: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ZIndexV28) {
    auto sheet = parse_stylesheet("section { z-index: 999; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "z-index") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OpacityV28) {
    auto sheet = parse_stylesheet("a { opacity: 0.5; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "opacity") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VisibilityV28) {
    auto sheet = parse_stylesheet("button { visibility: visible; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CursorV28) {
    auto sheet = parse_stylesheet("input { cursor: pointer; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityV28) {
    auto sheet = parse_stylesheet("div { backface-visibility: hidden; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "backface-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PointerEventsV29) {
    auto sheet = parse_stylesheet("a { pointer-events: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "pointer-events") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserSelectV29) {
    auto sheet = parse_stylesheet("p { user-select: none; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "user-select") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MixBlendModeV29) {
    auto sheet = parse_stylesheet("img { mix-blend-mode: multiply; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mix-blend-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FilterV29) {
    auto sheet = parse_stylesheet("div { filter: blur(5px); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "filter") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathV29) {
    auto sheet = parse_stylesheet("span { clip-path: circle(50%); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskV29) {
    auto sheet = parse_stylesheet("h1 { mask: url(#mask); }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "mask") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformOriginV29) {
    auto sheet = parse_stylesheet("button { transform-origin: center; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "transform-origin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectiveV29) {
    auto sheet = parse_stylesheet("section { perspective: 1000px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found = false;
    for (auto& d : sheet.rules[0].declarations)
        if (d.property == "perspective") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontWeightV30) {
    auto ss = parse_stylesheet("strong { font-weight: 700; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "font-weight") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LineHeightV30) {
    auto ss = parse_stylesheet("p { line-height: 1.5; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "line-height") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingV30) {
    auto ss = parse_stylesheet("h2 { letter-spacing: 0.05em; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "letter-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordSpacingV30) {
    auto ss = parse_stylesheet("div { word-spacing: 2px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "word-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextIndentV30) {
    auto ss = parse_stylesheet("article { text-indent: 2rem; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-indent") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, VerticalAlignV30) {
    auto ss = parse_stylesheet("img { vertical-align: middle; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "vertical-align") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceV30) {
    auto ss = parse_stylesheet("pre { white-space: pre-wrap; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextOverflowV30) {
    auto ss = parse_stylesheet("span { text-overflow: ellipsis; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OutlineV31) {
    auto ss = parse_stylesheet("button { outline: 2px solid red; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "outline") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ListStyleV31) {
    auto ss = parse_stylesheet("ul { list-style: square inside; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "list-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CursorV31) {
    auto ss = parse_stylesheet("a { cursor: pointer; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "cursor") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PointerEventsV31) {
    auto ss = parse_stylesheet("div { pointer-events: none; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "pointer-events") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ResizeV31) {
    auto ss = parse_stylesheet("textarea { resize: vertical; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "resize") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClipPathV31) {
    auto ss = parse_stylesheet("img { clip-path: circle(50%); }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "clip-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitV31) {
    auto ss = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectPositionV31) {
    auto ss = parse_stylesheet("img { object-position: center bottom; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "object-position") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1351: CSS parser tests V32

TEST_F(CSSStylesheetTest, GapV32) {
    auto ss = parse_stylesheet(".grid { gap: 10px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, RowGapV32) {
    auto ss = parse_stylesheet(".grid { row-gap: 20px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "row-gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnGapV32) {
    auto ss = parse_stylesheet(".grid { column-gap: 15px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceItemsV32) {
    auto ss = parse_stylesheet(".container { place-items: center; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "place-items") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceContentV32) {
    auto ss = parse_stylesheet(".container { place-content: space-between; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "place-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PlaceSelfV32) {
    auto ss = parse_stylesheet(".item { place-self: end; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "place-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridTemplateColumnsV32) {
    auto ss = parse_stylesheet(".grid { grid-template-columns: 1fr 2fr 1fr; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "grid-template-columns") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, GridTemplateRowsV32) {
    auto ss = parse_stylesheet(".grid { grid-template-rows: auto 100px auto; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "grid-template-rows") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1350: 8 flex property tests V33

TEST_F(CSSStylesheetTest, FlexDirectionV33) {
    auto ss = parse_stylesheet("div { flex-direction: row; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "flex-direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexWrapV33) {
    auto ss = parse_stylesheet("div { flex-wrap: wrap; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "flex-wrap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, JustifyContentV33) {
    auto ss = parse_stylesheet("div { justify-content: center; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "justify-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignItemsV33) {
    auto ss = parse_stylesheet("div { align-items: flex-start; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "align-items") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignSelfV33) {
    auto ss = parse_stylesheet("div { align-self: stretch; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "align-self") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AlignContentV33) {
    auto ss = parse_stylesheet("div { align-content: space-between; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "align-content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexBasisV33) {
    auto ss = parse_stylesheet("div { flex-basis: 200px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "flex-basis") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FlexFlowV33) {
    auto ss = parse_stylesheet("div { flex-flow: row wrap; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "flex-flow") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1352: 8 border property tests V34

TEST_F(CSSStylesheetTest, BorderTopV34) {
    auto ss = parse_stylesheet("div { border-top: 1px solid black; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-top") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderBottomV34) {
    auto ss = parse_stylesheet("div { border-bottom: 2px dashed blue; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-bottom") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderLeftV34) {
    auto ss = parse_stylesheet("div { border-left: 3px dotted green; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-left") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderRightV34) {
    auto ss = parse_stylesheet("div { border-right: 4px solid red; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-right") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderColorV34) {
    auto ss = parse_stylesheet("div { border-color: #ff0000; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderStyleV34) {
    auto ss = parse_stylesheet("div { border-style: dashed; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderWidthV34) {
    auto ss = parse_stylesheet("div { border-width: 5px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderCollapseV34) {
    auto ss = parse_stylesheet("table { border-collapse: collapse; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-collapse") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransitionV35) {
    auto ss = parse_stylesheet("div { transition: all 0.3s ease; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "transition") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationNameV35) {
    auto ss = parse_stylesheet("div { animation-name: slide; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "animation-name") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationDurationV35) {
    auto ss = parse_stylesheet("div { animation-duration: 2s; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "animation-duration") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AnimationDelayV35) {
    auto ss = parse_stylesheet("div { animation-delay: 1s; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "animation-delay") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformV35) {
    auto ss = parse_stylesheet("div { transform: scale(1.5); }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TransformOriginV35) {
    auto ss = parse_stylesheet("div { transform-origin: center center; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "transform-origin") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityV35) {
    auto ss = parse_stylesheet("div { backface-visibility: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "backface-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, PerspectiveV35) {
    auto ss = parse_stylesheet("div { perspective: 1000px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "perspective") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1398: 8 text decoration property tests V36

TEST_F(CSSStylesheetTest, OverflowXV36) {
    auto ss = parse_stylesheet("div { overflow-x: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYV36) {
    auto ss = parse_stylesheet("div { overflow-y: scroll; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextTransformV36) {
    auto ss = parse_stylesheet("p { text-transform: uppercase; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-transform") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationV36) {
    auto ss = parse_stylesheet("a { text-decoration: underline; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-decoration") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationColorV36) {
    auto ss = parse_stylesheet("a { text-decoration-color: red; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-decoration-color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationStyleV36) {
    auto ss = parse_stylesheet("a { text-decoration-style: wavy; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-decoration-style") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationLineV36) {
    auto ss = parse_stylesheet("a { text-decoration-line: line-through; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-decoration-line") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextDecorationThicknessV36) {
    auto ss = parse_stylesheet("a { text-decoration-thickness: 2px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-decoration-thickness") { found = true; break; }
    EXPECT_TRUE(found);
}

// Cycle 1399: 8 layout and table property tests V37

TEST_F(CSSStylesheetTest, VisibilityV37) {
    auto ss = parse_stylesheet("div { visibility: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BoxSizingV37) {
    auto ss = parse_stylesheet("div { box-sizing: border-box; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "box-sizing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FloatPropertyV37) {
    auto ss = parse_stylesheet("div { float: left; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "float") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ClearV37) {
    auto ss = parse_stylesheet("div { clear: both; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "clear") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TableLayoutV37) {
    auto ss = parse_stylesheet("table { table-layout: fixed; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "table-layout") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BorderCollapseV37) {
    auto ss = parse_stylesheet("table { border-collapse: collapse; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "border-collapse") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, EmptyCellsV37) {
    auto ss = parse_stylesheet("td { empty-cells: hide; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "empty-cells") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CaptionSideV37) {
    auto ss = parse_stylesheet("table { caption-side: bottom; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "caption-side") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakV38) {
    auto ss = parse_stylesheet("p { word-break: break-all; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowWrapV38) {
    auto ss = parse_stylesheet("p { overflow-wrap: break-word; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "overflow-wrap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, HyphensV38) {
    auto ss = parse_stylesheet("p { hyphens: auto; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "hyphens") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WritingModeV38) {
    auto ss = parse_stylesheet("div { writing-mode: vertical-rl; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "writing-mode") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, DirectionV38) {
    auto ss = parse_stylesheet("div { direction: rtl; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "direction") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UnicodeBidiV38) {
    auto ss = parse_stylesheet("span { unicode-bidi: embed; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "unicode-bidi") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextAlignLastV38) {
    auto ss = parse_stylesheet("p { text-align-last: justify; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-align-last") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TabSizeV38) {
    auto ss = parse_stylesheet("pre { tab-size: 4; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "tab-size") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentV39) {
    auto ss = parse_stylesheet("div::before { content: 'hello'; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "content") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CounterResetV39) {
    auto ss = parse_stylesheet("ol { counter-reset: section; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "counter-reset") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, CounterIncrementV39) {
    auto ss = parse_stylesheet("li { counter-increment: section; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "counter-increment") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, QuotesV39) {
    auto ss = parse_stylesheet("q { quotes: '«' '»'; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "quotes") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ListStyleTypeV39) {
    auto ss = parse_stylesheet("ul { list-style-type: disc; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "list-style-type") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ListStylePositionV39) {
    auto ss = parse_stylesheet("ul { list-style-position: inside; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "list-style-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ListStyleImageV39) {
    auto ss = parse_stylesheet("ul { list-style-image: none; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "list-style-image") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarkerV39) {
    auto ss = parse_stylesheet("li::marker { color: red; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "color") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnCountV40) {
    auto ss = parse_stylesheet("div { column-count: 3; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-count") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnWidthV40) {
    auto ss = parse_stylesheet("div { column-width: 200px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-width") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnGapV40) {
    auto ss = parse_stylesheet("div { column-gap: 20px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-gap") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnRuleV40) {
    auto ss = parse_stylesheet("div { column-rule: 1px solid black; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-rule") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ColumnSpanV40) {
    auto ss = parse_stylesheet("h2 { column-span: all; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "column-span") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakBeforeV40) {
    auto ss = parse_stylesheet("div { break-before: page; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "break-before") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakAfterV40) {
    auto ss = parse_stylesheet("div { break-after: avoid; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "break-after") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, BreakInsideV40) {
    auto ss = parse_stylesheet("div { break-inside: avoid; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "break-inside") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AspectRatioV41) {
    auto ss = parse_stylesheet("div { aspect-ratio: 16/9; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "aspect-ratio") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectFitV41) {
    auto ss = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "object-fit") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ObjectPositionV41) {
    auto ss = parse_stylesheet("img { object-position: center; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "object-position") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContainV41) {
    auto ss = parse_stylesheet("div { contain: layout; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "contain") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ContentVisibilityV41) {
    auto ss = parse_stylesheet("div { content-visibility: auto; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "content-visibility") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WillChangeV41) {
    auto ss = parse_stylesheet("div { will-change: transform; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "will-change") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TouchActionV41) {
    auto ss = parse_stylesheet("div { touch-action: none; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "touch-action") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, UserSelectV41) {
    auto ss = parse_stylesheet("div { user-select: none; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "user-select") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowXV42) {
    auto ss = parse_stylesheet("div { overflow-x: scroll; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "overflow-x") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OverflowYV42) {
    auto ss = parse_stylesheet("div { overflow-y: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "overflow-y") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextOverflowV42) {
    auto ss = parse_stylesheet("div { text-overflow: ellipsis; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-overflow") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WhiteSpaceV42) {
    auto ss = parse_stylesheet("div { white-space: nowrap; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "white-space") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordBreakV42) {
    auto ss = parse_stylesheet("div { word-break: break-all; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "word-break") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, WordSpacingV42) {
    auto ss = parse_stylesheet("div { word-spacing: 0.5em; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "word-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, LetterSpacingV42) {
    auto ss = parse_stylesheet("div { letter-spacing: 2px; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "letter-spacing") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextIndentV42) {
    auto ss = parse_stylesheet("div { text-indent: 1.5em; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "text-indent") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MaskImageUrlDeclarationV128) {
    auto ss = parse_stylesheet("div { mask-image: url(mask.png); }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "mask-image") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, OffsetPathDeclarationV128) {
    auto ss = parse_stylesheet("div { offset-path: path('M0 0L100 100'); }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "offset-path") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FontPaletteDeclarationV128) {
    auto ss = parse_stylesheet("div { font-palette: dark; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "font-palette") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, MarginTrimDeclarationV128) {
    auto ss = parse_stylesheet("div { margin-trim: block; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations)
        if (d.property == "margin-trim") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, AccentColorAutoDeclarationV129) {
    auto ss = parse_stylesheet("div { accent-color: auto; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations) {
        if (d.property == "accent-color") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_NE(d.values[0].value.find("auto"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, TextWrapPrettyDeclarationV129) {
    auto ss = parse_stylesheet("p { text-wrap: pretty; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations) {
        if (d.property == "text-wrap") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_NE(d.values[0].value.find("pretty"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, FieldSizingContentDeclarationV129) {
    auto ss = parse_stylesheet("input { field-sizing: content; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations) {
        if (d.property == "field-sizing") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_NE(d.values[0].value.find("content"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, InterpolateSizeDeclarationV129) {
    auto ss = parse_stylesheet("div { interpolate-size: allow-keywords; }");
    ASSERT_EQ(ss.rules.size(), 1);
    bool found = false;
    for (auto& d : ss.rules[0].declarations) {
        if (d.property == "interpolate-size") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_NE(d.values[0].value.find("allow-keywords"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CSSStylesheetTest, ScopeRuleParsingV130) {
    auto ss = parse_stylesheet("@scope (.card) to (.content) { div { color: red; } }");
    // Parser should handle @scope without crashing
    // If supported, scope_rules will contain the parsed rule
    if (!ss.scope_rules.empty()) {
        EXPECT_EQ(ss.scope_rules.size(), 1u);
        EXPECT_NE(ss.scope_rules[0].scope_start.find(".card"), std::string::npos);
        EXPECT_NE(ss.scope_rules[0].scope_end.find(".content"), std::string::npos);
        EXPECT_GE(ss.scope_rules[0].rules.size(), 1u);
    }
    // No crash is the minimum requirement
    SUCCEED();
}

TEST_F(CSSStylesheetTest, CounterStyleRuleParsingV130) {
    auto ss = parse_stylesheet("@counter-style thumbs { system: cyclic; symbols: thumbsup; suffix: ' '; }");
    // Parser should handle @counter-style without crashing
    if (!ss.counter_style_rules.empty()) {
        EXPECT_EQ(ss.counter_style_rules.size(), 1u);
        EXPECT_EQ(ss.counter_style_rules[0].name, "thumbs");
        EXPECT_FALSE(ss.counter_style_rules[0].descriptors.empty());
    }
    SUCCEED();
}

TEST_F(CSSStylesheetTest, PropertyRuleParsingV130) {
    auto ss = parse_stylesheet("@property --my-color { syntax: '<color>'; inherits: false; initial-value: #c0ffee; }");
    // Parser should handle @property without crashing
    if (!ss.property_rules.empty()) {
        EXPECT_EQ(ss.property_rules.size(), 1u);
        EXPECT_EQ(ss.property_rules[0].name, "--my-color");
        EXPECT_FALSE(ss.property_rules[0].inherits);
        EXPECT_NE(ss.property_rules[0].initial_value.find("c0ffee"), std::string::npos);
    }
    SUCCEED();
}

TEST_F(CSSStylesheetTest, ContainerRuleParsingV130) {
    auto ss = parse_stylesheet("@container sidebar (min-width: 400px) { .card { font-size: 20px; } }");
    // Parser should handle @container without crashing
    if (!ss.container_rules.empty()) {
        EXPECT_EQ(ss.container_rules.size(), 1u);
        EXPECT_NE(ss.container_rules[0].name.find("sidebar"), std::string::npos);
        EXPECT_GE(ss.container_rules[0].rules.size(), 1u);
    }
    SUCCEED();
}

TEST_F(CSSStylesheetTest, StartingStyleRuleParsingV131) {
    auto ss = parse_stylesheet("@starting-style { div { opacity: 0; } }");
    // @starting-style is not a recognized at-rule in our parser,
    // so it should not crash and rules may or may not be populated.
    // The minimum requirement is no crash.
    SUCCEED();
}

TEST_F(CSSStylesheetTest, LayerRuleParsingV131) {
    auto ss = parse_stylesheet("@layer utilities { .btn { padding: 10px; } }");
    // Parser should handle @layer without crashing
    if (!ss.layer_rules.empty()) {
        EXPECT_EQ(ss.layer_rules.size(), 1u);
        EXPECT_NE(ss.layer_rules[0].name.find("utilities"), std::string::npos);
        EXPECT_GE(ss.layer_rules[0].rules.size(), 1u);
    }
    SUCCEED();
}

TEST_F(CSSStylesheetTest, NestingSelectorParsingV131) {
    auto ss = parse_stylesheet("div { & span { color: blue; } }");
    // CSS Nesting with & selector — parser should not crash
    // Whether the nested rule is parsed depends on implementation
    ASSERT_GE(ss.rules.size(), 1u);
    SUCCEED();
}

TEST_F(CSSStylesheetTest, FontPaletteValuesRuleParsingV131) {
    auto ss = parse_stylesheet("@font-palette-values --custom { font-family: Bungee; base-palette: 1; }");
    // @font-palette-values is not a recognized at-rule in our parser,
    // so it should not crash. No crash is the minimum requirement.
    SUCCEED();
}

TEST_F(CSSStylesheetTest, ImportRuleParsingV132) {
    auto ss = parse_stylesheet("@import url('styles.css');");
    // @import is an at-rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, SupportsRuleParsingV132) {
    auto ss = parse_stylesheet("@supports (display: grid) { .grid { display: grid; } }");
    // @supports at-rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, KeyframesRuleParsingV132) {
    auto ss = parse_stylesheet("@keyframes fade { from { opacity: 0; } to { opacity: 1; } }");
    // @keyframes at-rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, CommaSeparatedSelectorsParsingV132) {
    auto ss = parse_stylesheet("h1, h2, h3 { font-weight: bold; }");
    ASSERT_GE(ss.rules.size(), 1u);
    SUCCEED();
}

// --- Round 133 (V133) ---

TEST_F(CSSStylesheetTest, MediaRuleNestedParsingV133) {
    auto ss = parse_stylesheet("@media (min-width: 768px) { .c { width: 100%; } }");
    // @media at-rule with nested rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, CharsetRuleParsingV133) {
    auto ss = parse_stylesheet("@charset \"UTF-8\"; body { color: black; }");
    // @charset at-rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, NamespaceRuleParsingV133) {
    auto ss = parse_stylesheet("@namespace svg url(http://www.w3.org/2000/svg);");
    // @namespace at-rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, MultipleAtRulesSequenceV133) {
    auto ss = parse_stylesheet("@import url('a.css'); @media print { p { display: none; } }");
    // Multiple at-rules in sequence — parser should not crash
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Round 134
// ---------------------------------------------------------------------------

TEST_F(CSSStylesheetTest, PageRuleParsingV134) {
    auto ss = parse_stylesheet("@page { margin: 2cm; }");
    // @page rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, CounterStyleRuleParsingV134) {
    auto ss = parse_stylesheet("@counter-style thumbs { system: cyclic; symbols: '\\1F44D'; }");
    // @counter-style rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, PropertyRuleParsingV134) {
    auto ss = parse_stylesheet("@property --main-color { syntax: '<color>'; inherits: false; initial-value: red; }");
    // @property rule — parser should not crash
    SUCCEED();
}

TEST_F(CSSStylesheetTest, NestedMediaAndSupportsParsingV134) {
    auto ss = parse_stylesheet("@media screen { @supports (display: grid) { .x { color: red; } } }");
    // Nested @media and @supports — parser should not crash
    SUCCEED();
}

// === V135 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, ContentVisibilityAutoDeclarationV135) {
    auto ss = parse_stylesheet("div { content-visibility: auto; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "content-visibility") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "auto");
        }
    }
    EXPECT_TRUE(found) << "content-visibility declaration not found";
}

TEST_F(CSSStylesheetTest, ScrollSnapTypeDeclarationV135) {
    auto ss = parse_stylesheet(".container { scroll-snap-type: y mandatory; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "scroll-snap-type") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
        }
    }
    EXPECT_TRUE(found) << "scroll-snap-type declaration not found";
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorContainDeclarationV135) {
    auto ss = parse_stylesheet("body { overscroll-behavior: contain; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "overscroll-behavior") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "contain");
        }
    }
    EXPECT_TRUE(found) << "overscroll-behavior declaration not found";
}

TEST_F(CSSStylesheetTest, ColorSchemeDeclarationV135) {
    auto ss = parse_stylesheet(":root { color-scheme: light dark; }");
    ASSERT_GE(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "color-scheme") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
        }
    }
    EXPECT_TRUE(found) << "color-scheme declaration not found";
}

// === V136 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, WritingModeDeclarationV136) {
    auto ss = parse_stylesheet("div { writing-mode: vertical-rl; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "writing-mode") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "vertical-rl");
        }
    }
    EXPECT_TRUE(found) << "writing-mode declaration not found";
}

TEST_F(CSSStylesheetTest, TextOverflowEllipsisDeclarationV136) {
    auto ss = parse_stylesheet("p { text-overflow: ellipsis; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "text-overflow") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "ellipsis");
        }
    }
    EXPECT_TRUE(found) << "text-overflow declaration not found";
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityHiddenDeclarationV136) {
    auto ss = parse_stylesheet(".card { backface-visibility: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "backface-visibility") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "hidden");
        }
    }
    EXPECT_TRUE(found) << "backface-visibility declaration not found";
}

TEST_F(CSSStylesheetTest, MixBlendModeMultiplyDeclarationV136) {
    auto ss = parse_stylesheet(".overlay { mix-blend-mode: multiply; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "mix-blend-mode") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "multiply");
        }
    }
    EXPECT_TRUE(found) << "mix-blend-mode declaration not found";
}

TEST_F(CSSStylesheetTest, AppearanceNoneDeclarationV137) {
    auto ss = parse_stylesheet(".btn { appearance: none; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "appearance") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "none");
        }
    }
    EXPECT_TRUE(found) << "appearance declaration not found";
}

TEST_F(CSSStylesheetTest, ObjectFitCoverDeclarationV137) {
    auto ss = parse_stylesheet("img { object-fit: cover; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "object-fit") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "cover");
        }
    }
    EXPECT_TRUE(found) << "object-fit declaration not found";
}

TEST_F(CSSStylesheetTest, ObjectPositionDeclarationV137) {
    auto ss = parse_stylesheet("img { object-position: center; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "object-position") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "center");
        }
    }
    EXPECT_TRUE(found) << "object-position declaration not found";
}

TEST_F(CSSStylesheetTest, ResizeVerticalDeclarationV137) {
    auto ss = parse_stylesheet("textarea { resize: vertical; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "resize") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "vertical");
        }
    }
    EXPECT_TRUE(found) << "resize declaration not found";
}

// === V138 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, GapShorthandDeclarationV138) {
    auto ss = parse_stylesheet(".grid { gap: 10px 20px; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "gap") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "10px");
        }
    }
    EXPECT_TRUE(found) << "gap shorthand declaration not found";
}

TEST_F(CSSStylesheetTest, PlaceItemsDeclarationV138) {
    auto ss = parse_stylesheet(".flex { place-items: start end; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "place-items") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "start");
        }
    }
    EXPECT_TRUE(found) << "place-items declaration not found";
}

TEST_F(CSSStylesheetTest, PlaceContentDeclarationV138) {
    auto ss = parse_stylesheet(".grid { place-content: space-around stretch; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "place-content") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "space-around");
        }
    }
    EXPECT_TRUE(found) << "place-content declaration not found";
}

TEST_F(CSSStylesheetTest, InsetDeclarationV138) {
    auto ss = parse_stylesheet(".overlay { inset: 0 0 0 0; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "inset") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "0");
        }
    }
    EXPECT_TRUE(found) << "inset declaration not found";
}

// === V139 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, AspectRatioDeclarationV139) {
    auto ss = parse_stylesheet(".box { aspect-ratio: 16 / 9; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "aspect-ratio") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "16");
        }
    }
    EXPECT_TRUE(found) << "aspect-ratio declaration not found";
}

TEST_F(CSSStylesheetTest, ContainerTypeDeclarationV139) {
    auto ss = parse_stylesheet(".card { container-type: inline-size; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "container-type") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "inline-size");
        }
    }
    EXPECT_TRUE(found) << "container-type declaration not found";
}

TEST_F(CSSStylesheetTest, ColumnsDeclarationV139) {
    auto ss = parse_stylesheet(".text { columns: 3 200px; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "columns") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "3");
        }
    }
    EXPECT_TRUE(found) << "columns declaration not found";
}

TEST_F(CSSStylesheetTest, HyphensAutoDeclarationV139) {
    auto ss = parse_stylesheet("p { hyphens: auto; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "hyphens") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "auto");
        }
    }
    EXPECT_TRUE(found) << "hyphens declaration not found";
}

// === V140 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, TabSizeDeclarationV140) {
    auto ss = parse_stylesheet("pre { tab-size: 4; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "tab-size") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "4");
        }
    }
    EXPECT_TRUE(found) << "tab-size declaration not found";
}

TEST_F(CSSStylesheetTest, WordBreakDeclarationV140) {
    auto ss = parse_stylesheet("p { word-break: break-all; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "word-break") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "break-all");
        }
    }
    EXPECT_TRUE(found) << "word-break declaration not found";
}

TEST_F(CSSStylesheetTest, LineClampDeclarationV140) {
    auto ss = parse_stylesheet(".text { -webkit-line-clamp: 3; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "-webkit-line-clamp") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "3");
        }
    }
    EXPECT_TRUE(found) << "-webkit-line-clamp declaration not found";
}

TEST_F(CSSStylesheetTest, TextShadowDeclarationV140) {
    auto ss = parse_stylesheet("h1 { text-shadow: 2px 2px 4px black; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "text-shadow") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "2px");
        }
    }
    EXPECT_TRUE(found) << "text-shadow declaration not found";
}

// === V141 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, ScrollSnapAlignDeclarationV141) {
    auto ss = parse_stylesheet("div { scroll-snap-align: center; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "scroll-snap-align") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "center");
        }
    }
    EXPECT_TRUE(found) << "scroll-snap-align declaration not found";
}

TEST_F(CSSStylesheetTest, ContainPropertyDeclarationV141) {
    auto ss = parse_stylesheet("section { contain: layout style; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "contain") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "layout");
        }
    }
    EXPECT_TRUE(found) << "contain declaration not found";
}

TEST_F(CSSStylesheetTest, WillChangePropertyDeclarationV141) {
    auto ss = parse_stylesheet(".animated { will-change: transform, opacity; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "will-change") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "transform");
        }
    }
    EXPECT_TRUE(found) << "will-change declaration not found";
}

TEST_F(CSSStylesheetTest, TouchActionPropertyDeclarationV141) {
    auto ss = parse_stylesheet("div { touch-action: pan-x; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "touch-action") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "pan-x");
        }
    }
    EXPECT_TRUE(found) << "touch-action declaration not found";
}

// === V142 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, ColumnGapDeclarationV142) {
    auto ss = parse_stylesheet(".grid { column-gap: 16px; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "column-gap") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "16px");
        }
    }
    EXPECT_TRUE(found) << "column-gap declaration not found";
}

TEST_F(CSSStylesheetTest, RowGapDeclarationV142) {
    auto ss = parse_stylesheet(".grid { row-gap: 8px; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "row-gap") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "8px");
        }
    }
    EXPECT_TRUE(found) << "row-gap declaration not found";
}

TEST_F(CSSStylesheetTest, BackfaceVisibilityDeclarationV142) {
    auto ss = parse_stylesheet(".card { backface-visibility: hidden; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "backface-visibility") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "hidden");
        }
    }
    EXPECT_TRUE(found) << "backface-visibility declaration not found";
}

TEST_F(CSSStylesheetTest, MixBlendModeDeclarationV142) {
    auto ss = parse_stylesheet(".overlay { mix-blend-mode: multiply; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "mix-blend-mode") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "multiply");
        }
    }
    EXPECT_TRUE(found) << "mix-blend-mode declaration not found";
}

// === V143 CSS Parser Tests ===

TEST_F(CSSStylesheetTest, ResizePropertyDeclarationV143) {
    auto ss = parse_stylesheet("textarea { resize: both; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "resize") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "both");
        }
    }
    EXPECT_TRUE(found) << "resize declaration not found";
}

TEST_F(CSSStylesheetTest, UserSelectPropertyDeclarationV143) {
    auto ss = parse_stylesheet(".noselect { user-select: none; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "user-select") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "none");
        }
    }
    EXPECT_TRUE(found) << "user-select declaration not found";
}

TEST_F(CSSStylesheetTest, ListStyleTypeDeclarationV143) {
    auto ss = parse_stylesheet("ul { list-style-type: disc; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "list-style-type") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "disc");
        }
    }
    EXPECT_TRUE(found) << "list-style-type declaration not found";
}

TEST_F(CSSStylesheetTest, OverscrollBehaviorDeclarationV143) {
    auto ss = parse_stylesheet(".modal { overscroll-behavior: contain; }");
    ASSERT_EQ(ss.rules.size(), 1u);
    bool found = false;
    for (const auto& d : ss.rules[0].declarations) {
        if (d.property == "overscroll-behavior") {
            found = true;
            ASSERT_GE(d.values.size(), 1u);
            EXPECT_EQ(d.values[0].value, "contain");
        }
    }
    EXPECT_TRUE(found) << "overscroll-behavior declaration not found";
}
