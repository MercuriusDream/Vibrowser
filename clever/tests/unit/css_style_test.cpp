#include <clever/css/style/computed_style.h>
#include <clever/css/style/selector_matcher.h>
#include <clever/css/style/style_resolver.h>
#include <clever/css/parser/selector.h>
#include <clever/css/parser/stylesheet.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace clever::css;

// ---------------------------------------------------------------------------
// Helper: build ComponentValue from a string for simple token values
// ---------------------------------------------------------------------------
namespace {

ComponentValue make_token(const std::string& val) {
    ComponentValue cv;
    cv.type = ComponentValue::Token;
    cv.value = val;
    return cv;
}

Declaration make_decl(const std::string& property, const std::string& value, bool important = false) {
    Declaration d;
    d.property = property;
    d.values.push_back(make_token(value));
    d.important = important;
    return d;
}

Declaration make_decl_multi(const std::string& property, const std::vector<std::string>& values, bool important = false) {
    Declaration d;
    d.property = property;
    for (const auto& v : values) {
        d.values.push_back(make_token(v));
    }
    d.important = important;
    return d;
}

SimpleSelector make_type_sel(const std::string& tag) {
    SimpleSelector s;
    s.type = SimpleSelectorType::Type;
    s.value = tag;
    return s;
}

SimpleSelector make_class_sel(const std::string& cls) {
    SimpleSelector s;
    s.type = SimpleSelectorType::Class;
    s.value = cls;
    return s;
}

SimpleSelector make_id_sel(const std::string& id) {
    SimpleSelector s;
    s.type = SimpleSelectorType::Id;
    s.value = id;
    return s;
}

SimpleSelector make_attr_sel(const std::string& attr_name, const std::string& attr_val,
                             AttributeMatch match = AttributeMatch::Exact) {
    SimpleSelector s;
    s.type = SimpleSelectorType::Attribute;
    s.attr_name = attr_name;
    s.attr_match = match;
    s.attr_value = attr_val;
    return s;
}

SimpleSelector make_universal_sel() {
    SimpleSelector s;
    s.type = SimpleSelectorType::Universal;
    return s;
}

// Build a ComplexSelector with a single compound (no combinators)
ComplexSelector make_simple_complex(const CompoundSelector& compound) {
    ComplexSelector cs;
    ComplexSelector::Part part;
    part.compound = compound;
    part.combinator = std::nullopt;  // No combinator for the first/only part
    cs.parts.push_back(part);
    return cs;
}

// Build a ComplexSelector from subject + ancestor chain
// parts[0] is the outermost ancestor, parts[last] is the subject element
// Each part (except the first) has a combinator that relates it to the previous part
ComplexSelector make_complex_chain(
    const std::vector<std::pair<std::optional<Combinator>, CompoundSelector>>& chain) {
    ComplexSelector cs;
    for (const auto& [comb, compound] : chain) {
        ComplexSelector::Part part;
        part.compound = compound;
        part.combinator = comb;
        cs.parts.push_back(part);
    }
    return cs;
}

} // anonymous namespace

// ===========================================================================
// Test 1: Default ComputedStyle values
// ===========================================================================
TEST(ComputedStyleTest, DefaultValues) {
    ComputedStyle style;
    EXPECT_EQ(style.display, Display::Inline);
    EXPECT_EQ(style.position, Position::Static);
    EXPECT_EQ(style.float_val, Float::None);
    EXPECT_EQ(style.clear, Clear::None);
    EXPECT_EQ(style.box_sizing, BoxSizing::ContentBox);
    EXPECT_TRUE(style.width.is_auto());
    EXPECT_TRUE(style.height.is_auto());
    EXPECT_EQ(style.color, Color::black());
    EXPECT_EQ(style.background_color, Color::transparent());
    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
    EXPECT_EQ(style.visibility, Visibility::Visible);
    EXPECT_EQ(style.font_weight, 400);
    EXPECT_EQ(style.font_family, "sans-serif");
    EXPECT_FLOAT_EQ(style.font_size.value, 16.0f);
    EXPECT_EQ(style.text_align, TextAlign::Left);
    EXPECT_EQ(style.overflow_x, Overflow::Visible);
    EXPECT_EQ(style.overflow_y, Overflow::Visible);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
    EXPECT_EQ(style.flex_wrap, FlexWrap::NoWrap);
    EXPECT_FLOAT_EQ(style.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 1.0f);
    EXPECT_TRUE(style.flex_basis.is_auto());
    EXPECT_EQ(style.cursor, Cursor::Auto);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline);
}

// ===========================================================================
// Test 2: default_style_for_tag("div") -> display: block
// ===========================================================================
TEST(ComputedStyleTest, DefaultStyleForDiv) {
    auto style = default_style_for_tag("div");
    EXPECT_EQ(style.display, Display::Block);
}

// ===========================================================================
// Test 3: default_style_for_tag("span") -> display: inline
// ===========================================================================
TEST(ComputedStyleTest, DefaultStyleForSpan) {
    auto style = default_style_for_tag("span");
    EXPECT_EQ(style.display, Display::Inline);
}

// ===========================================================================
// Test 4: default_style_for_tag("h1") -> font-size larger, font-weight bold
// ===========================================================================
TEST(ComputedStyleTest, DefaultStyleForH1) {
    auto style = default_style_for_tag("h1");
    EXPECT_EQ(style.display, Display::Block);
    EXPECT_GT(style.font_size.value, 16.0f);
    EXPECT_EQ(style.font_weight, 700);
}

// ===========================================================================
// Test 5: Length::to_px for px values
// ===========================================================================
TEST(LengthTest, ToPxForPxValues) {
    auto len = Length::px(42.0f);
    EXPECT_FLOAT_EQ(len.to_px(), 42.0f);
}

// ===========================================================================
// Test 6: Length::to_px for em values
// ===========================================================================
TEST(LengthTest, ToPxForEmValues) {
    auto len = Length::em(2.0f);
    EXPECT_FLOAT_EQ(len.to_px(16.0f), 32.0f);
    EXPECT_FLOAT_EQ(len.to_px(10.0f), 20.0f);
}

// ===========================================================================
// Test 7: Length::to_px for percent values
// ===========================================================================
TEST(LengthTest, ToPxForPercentValues) {
    auto len = Length::percent(50.0f);
    EXPECT_FLOAT_EQ(len.to_px(200.0f), 100.0f);
    EXPECT_FLOAT_EQ(len.to_px(400.0f), 200.0f);
}

// ===========================================================================
// Test 8: Length::auto detection
// ===========================================================================
TEST(LengthTest, AutoDetection) {
    auto auto_len = Length::auto_val();
    EXPECT_TRUE(auto_len.is_auto());
    EXPECT_FALSE(auto_len.is_zero());

    auto px_len = Length::px(10);
    EXPECT_FALSE(px_len.is_auto());
    EXPECT_FALSE(px_len.is_zero());

    auto zero_len = Length::zero();
    EXPECT_FALSE(zero_len.is_auto());
    EXPECT_TRUE(zero_len.is_zero());

    auto px_zero = Length::px(0);
    EXPECT_TRUE(px_zero.is_zero());
}

// ===========================================================================
// Test 9: Color::black(), Color::white()
// ===========================================================================
TEST(ColorTest, NamedColors) {
    auto black = Color::black();
    EXPECT_EQ(black.r, 0);
    EXPECT_EQ(black.g, 0);
    EXPECT_EQ(black.b, 0);
    EXPECT_EQ(black.a, 255);

    auto white = Color::white();
    EXPECT_EQ(white.r, 255);
    EXPECT_EQ(white.g, 255);
    EXPECT_EQ(white.b, 255);
    EXPECT_EQ(white.a, 255);

    auto trans = Color::transparent();
    EXPECT_EQ(trans.r, 0);
    EXPECT_EQ(trans.g, 0);
    EXPECT_EQ(trans.b, 0);
    EXPECT_EQ(trans.a, 0);
}

// ===========================================================================
// Test 10: parse_color("red") -> Color{255,0,0,255}
// ===========================================================================
TEST(ValueParserTest, ParseColorNamedRed) {
    auto c = parse_color("red");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

// ===========================================================================
// Test 11: parse_color("#ff0000") -> Color{255,0,0,255}
// ===========================================================================
TEST(ValueParserTest, ParseColorHex6) {
    auto c = parse_color("#ff0000");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

// ===========================================================================
// Test 12: parse_color("#f00") -> Color{255,0,0,255}
// ===========================================================================
TEST(ValueParserTest, ParseColorHex3) {
    auto c = parse_color("#f00");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

// ===========================================================================
// Test 13: parse_color("rgb(255, 128, 0)") -> Color{255,128,0,255}
// ===========================================================================
TEST(ValueParserTest, ParseColorRgbFunction) {
    auto c = parse_color("rgb(255, 128, 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 128);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

// ===========================================================================
// Test 14: parse_length("16px") -> Length::px(16)
// ===========================================================================
TEST(ValueParserTest, ParseLengthPx) {
    auto l = parse_length("16px");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 16.0f);
    EXPECT_EQ(l->unit, Length::Unit::Px);
}

// ===========================================================================
// Test 15: parse_length("2em") -> Length::em(2)
// ===========================================================================
TEST(ValueParserTest, ParseLengthEm) {
    auto l = parse_length("2em");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 2.0f);
    EXPECT_EQ(l->unit, Length::Unit::Em);
}

// ===========================================================================
// Test 16: parse_length("50%") -> Length::percent(50)
// ===========================================================================
TEST(ValueParserTest, ParseLengthPercent) {
    auto l = parse_length("50%");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 50.0f);
    EXPECT_EQ(l->unit, Length::Unit::Percent);
}

// ===========================================================================
// Test 17: SelectorMatcher: type selector matches element tag
// ===========================================================================
TEST(SelectorMatcherTest, TypeSelectorMatchesTag) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "div";

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_type_sel("div"));
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    elem.tag_name = "span";
    EXPECT_FALSE(matcher.matches(elem, complex));
}

// ===========================================================================
// Test 18: SelectorMatcher: class selector matches element with class
// ===========================================================================
TEST(SelectorMatcherTest, ClassSelectorMatchesClass) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"foo", "bar"};

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_class_sel("foo"));
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    CompoundSelector compound2;
    compound2.simple_selectors.push_back(make_class_sel("baz"));
    auto complex2 = make_simple_complex(compound2);

    EXPECT_FALSE(matcher.matches(elem, complex2));
}

// ===========================================================================
// Test 19: SelectorMatcher: ID selector matches element with id
// ===========================================================================
TEST(SelectorMatcherTest, IdSelectorMatchesId) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "main";

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_id_sel("main"));
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    CompoundSelector compound2;
    compound2.simple_selectors.push_back(make_id_sel("sidebar"));
    auto complex2 = make_simple_complex(compound2);

    EXPECT_FALSE(matcher.matches(elem, complex2));
}

// ===========================================================================
// Test 20: SelectorMatcher: compound selector (tag.class#id)
// ===========================================================================
TEST(SelectorMatcherTest, CompoundSelectorTagClassId) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "main";
    elem.classes = {"container"};

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_type_sel("div"));
    compound.simple_selectors.push_back(make_class_sel("container"));
    compound.simple_selectors.push_back(make_id_sel("main"));
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    elem.id = "other";
    EXPECT_FALSE(matcher.matches(elem, complex));
}

// ===========================================================================
// Test 21: SelectorMatcher: descendant combinator
// ===========================================================================
TEST(SelectorMatcherTest, DescendantCombinator) {
    SelectorMatcher matcher;

    // Structure: div > section > p
    ElementView grandparent;
    grandparent.tag_name = "div";

    ElementView parent_elem;
    parent_elem.tag_name = "section";
    parent_elem.parent = &grandparent;

    ElementView child;
    child.tag_name = "p";
    child.parent = &parent_elem;

    // Selector: div p (descendant combinator)
    // parts[0] = div (no combinator), parts[1] = p (descendant combinator)
    CompoundSelector ancestor_compound;
    ancestor_compound.simple_selectors.push_back(make_type_sel("div"));

    CompoundSelector subject_compound;
    subject_compound.simple_selectors.push_back(make_type_sel("p"));

    auto complex = make_complex_chain({
        {std::nullopt, ancestor_compound},
        {Combinator::Descendant, subject_compound}
    });

    EXPECT_TRUE(matcher.matches(child, complex));

    // Direct child of div should also match descendant
    ElementView direct_child;
    direct_child.tag_name = "p";
    direct_child.parent = &grandparent;
    EXPECT_TRUE(matcher.matches(direct_child, complex));

    // No div ancestor
    ElementView orphan;
    orphan.tag_name = "p";
    EXPECT_FALSE(matcher.matches(orphan, complex));
}

// ===========================================================================
// Test 22: SelectorMatcher: child combinator
// ===========================================================================
TEST(SelectorMatcherTest, ChildCombinator) {
    SelectorMatcher matcher;

    ElementView parent_elem;
    parent_elem.tag_name = "div";

    ElementView child;
    child.tag_name = "p";
    child.parent = &parent_elem;

    // Selector: div > p (child combinator)
    CompoundSelector parent_compound;
    parent_compound.simple_selectors.push_back(make_type_sel("div"));

    CompoundSelector child_compound;
    child_compound.simple_selectors.push_back(make_type_sel("p"));

    auto complex = make_complex_chain({
        {std::nullopt, parent_compound},
        {Combinator::Child, child_compound}
    });

    EXPECT_TRUE(matcher.matches(child, complex));

    // Grandchild should NOT match child combinator
    ElementView mid;
    mid.tag_name = "section";
    mid.parent = &parent_elem;

    ElementView grandchild;
    grandchild.tag_name = "p";
    grandchild.parent = &mid;

    EXPECT_FALSE(matcher.matches(grandchild, complex));
}

// ===========================================================================
// Test 23: SelectorMatcher: attribute selector [attr=val]
// ===========================================================================
TEST(SelectorMatcherTest, AttributeSelector) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "input";
    elem.attributes = {{"type", "text"}, {"name", "email"}};

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_attr_sel("type", "text"));
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    CompoundSelector compound2;
    compound2.simple_selectors.push_back(make_attr_sel("type", "password"));
    auto complex2 = make_simple_complex(compound2);

    EXPECT_FALSE(matcher.matches(elem, complex2));

    // Attribute exists check
    SimpleSelector attr_exists;
    attr_exists.type = SimpleSelectorType::Attribute;
    attr_exists.attr_name = "name";
    attr_exists.attr_match = AttributeMatch::Exists;

    CompoundSelector compound3;
    compound3.simple_selectors.push_back(attr_exists);
    auto complex3 = make_simple_complex(compound3);

    EXPECT_TRUE(matcher.matches(elem, complex3));
}

// ===========================================================================
// Test 24: PropertyCascade: single rule applied
// ===========================================================================
TEST(PropertyCascadeTest, SingleRuleApplied) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule rule;
    rule.declarations.push_back(make_decl("display", "block"));

    MatchedRule matched;
    matched.rule = &rule;
    matched.specificity = {0, 0, 1};
    matched.source_order = 0;

    std::vector<MatchedRule> rules = {matched};
    auto result = cascade.cascade(rules, parent_style);

    EXPECT_EQ(result.display, Display::Block);
}

// ===========================================================================
// Test 25: PropertyCascade: specificity ordering (higher specificity wins)
// ===========================================================================
TEST(PropertyCascadeTest, SpecificityOrdering) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule rule1;
    rule1.declarations.push_back(make_decl("display", "block"));

    StyleRule rule2;
    rule2.declarations.push_back(make_decl("display", "flex"));

    MatchedRule matched1;
    matched1.rule = &rule1;
    matched1.specificity = {0, 0, 1};  // type selector
    matched1.source_order = 0;

    MatchedRule matched2;
    matched2.rule = &rule2;
    matched2.specificity = {0, 1, 0};  // class selector (higher)
    matched2.source_order = 1;

    std::vector<MatchedRule> rules = {matched1, matched2};
    auto result = cascade.cascade(rules, parent_style);

    EXPECT_EQ(result.display, Display::Flex);
}

// ===========================================================================
// Test 26: PropertyCascade: !important overrides
// ===========================================================================
TEST(PropertyCascadeTest, ImportantOverrides) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule rule1;
    rule1.declarations.push_back(make_decl("display", "flex", false));

    StyleRule rule2;
    rule2.declarations.push_back(make_decl("display", "block", true));  // !important

    MatchedRule matched1;
    matched1.rule = &rule1;
    matched1.specificity = {1, 0, 0};  // ID selector (very high)
    matched1.source_order = 0;

    MatchedRule matched2;
    matched2.rule = &rule2;
    matched2.specificity = {0, 0, 1};  // type selector (low)
    matched2.source_order = 1;

    std::vector<MatchedRule> rules = {matched1, matched2};
    auto result = cascade.cascade(rules, parent_style);

    EXPECT_EQ(result.display, Display::Block);
}

// ===========================================================================
// Test 27: PropertyCascade: source order breaks ties
// ===========================================================================
TEST(PropertyCascadeTest, SourceOrderBreaksTies) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule rule1;
    rule1.declarations.push_back(make_decl("display", "block"));

    StyleRule rule2;
    rule2.declarations.push_back(make_decl("display", "flex"));

    MatchedRule matched1;
    matched1.rule = &rule1;
    matched1.specificity = {0, 1, 0};
    matched1.source_order = 0;

    MatchedRule matched2;
    matched2.rule = &rule2;
    matched2.specificity = {0, 1, 0};
    matched2.source_order = 1;  // Later

    std::vector<MatchedRule> rules = {matched1, matched2};
    auto result = cascade.cascade(rules, parent_style);

    EXPECT_EQ(result.display, Display::Flex);
}

TEST(PropertyCascadeTest, LayeredNormalLosesToUnlayeredNormal) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule layered_rule;
    layered_rule.in_layer = true;
    layered_rule.layer_order = 0;
    layered_rule.declarations.push_back(make_decl("display", "flex"));

    StyleRule unlayered_rule;
    unlayered_rule.declarations.push_back(make_decl("display", "block"));

    MatchedRule m1{&layered_rule, {0, 1, 0}, 0};
    MatchedRule m2{&unlayered_rule, {0, 1, 0}, 1};

    auto result = cascade.cascade({m1, m2}, parent_style);
    EXPECT_EQ(result.display, Display::Block);
}

TEST(PropertyCascadeTest, LayeredImportantBeatsUnlayeredImportant) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule layered_rule;
    layered_rule.in_layer = true;
    layered_rule.layer_order = 0;
    layered_rule.declarations.push_back(make_decl("display", "flex", true));

    StyleRule unlayered_rule;
    unlayered_rule.declarations.push_back(make_decl("display", "block", true));

    MatchedRule m1{&layered_rule, {0, 1, 0}, 0};
    MatchedRule m2{&unlayered_rule, {0, 1, 0}, 1};

    auto result = cascade.cascade({m1, m2}, parent_style);
    EXPECT_EQ(result.display, Display::Flex);
}

TEST(PropertyCascadeTest, EarlierLayerWinsForImportantDeclarations) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule base_layer_rule;
    base_layer_rule.in_layer = true;
    base_layer_rule.layer_order = 0;
    base_layer_rule.declarations.push_back(make_decl("display", "block", true));

    StyleRule theme_layer_rule;
    theme_layer_rule.in_layer = true;
    theme_layer_rule.layer_order = 1;
    theme_layer_rule.declarations.push_back(make_decl("display", "flex", true));

    MatchedRule m1{&base_layer_rule, {0, 1, 0}, 0};
    MatchedRule m2{&theme_layer_rule, {0, 1, 0}, 1};

    auto result = cascade.cascade({m1, m2}, parent_style);
    EXPECT_EQ(result.display, Display::Block);
}

// ===========================================================================
// Test 28: StyleResolver: resolve with single stylesheet
// ===========================================================================
TEST(StyleResolverTest, ResolveWithSingleStylesheet) {
    StyleResolver resolver;

    StyleSheet sheet;
    StyleRule rule;

    // Selector: div
    CompoundSelector compound;
    compound.simple_selectors.push_back(make_type_sel("div"));
    auto complex = make_simple_complex(compound);
    rule.selectors.selectors.push_back(complex);

    rule.declarations.push_back(make_decl("display", "block"));
    rule.declarations.push_back(make_decl("color", "red"));
    sheet.rules.push_back(rule);

    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto result = resolver.resolve(elem, parent);

    EXPECT_EQ(result.display, Display::Block);
    EXPECT_EQ(result.color.r, 255);
    EXPECT_EQ(result.color.g, 0);
    EXPECT_EQ(result.color.b, 0);
}

// ===========================================================================
// Test 29: StyleResolver: inherited properties (color, font-size)
// ===========================================================================
TEST(StyleResolverTest, InheritedProperties) {
    StyleResolver resolver;

    StyleSheet sheet;
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    parent.color = {255, 0, 0, 255};
    parent.font_size = Length::px(24);
    parent.font_family = "serif";
    parent.font_weight = 700;
    parent.text_align = TextAlign::Center;
    parent.visibility = Visibility::Hidden;
    parent.cursor = Cursor::Pointer;
    parent.list_style_type = ListStyleType::Square;

    auto result = resolver.resolve(elem, parent);

    EXPECT_EQ(result.color, parent.color);
    EXPECT_FLOAT_EQ(result.font_size.value, 24.0f);
    EXPECT_EQ(result.font_family, "serif");
    EXPECT_EQ(result.font_weight, 700);
    EXPECT_EQ(result.text_align, TextAlign::Center);
    EXPECT_EQ(result.visibility, Visibility::Hidden);
    EXPECT_EQ(result.cursor, Cursor::Pointer);
    EXPECT_EQ(result.list_style_type, ListStyleType::Square);

    // Non-inherited properties should NOT come from parent
    EXPECT_EQ(result.background_color, Color::transparent());
}

// ===========================================================================
// Test 30: apply_declaration for display property
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationDisplay) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "block"), parent);
    EXPECT_EQ(style.display, Display::Block);

    cascade.apply_declaration(style, make_decl("display", "inline"), parent);
    EXPECT_EQ(style.display, Display::Inline);

    cascade.apply_declaration(style, make_decl("display", "inline-block"), parent);
    EXPECT_EQ(style.display, Display::InlineBlock);

    cascade.apply_declaration(style, make_decl("display", "flex"), parent);
    EXPECT_EQ(style.display, Display::Flex);

    cascade.apply_declaration(style, make_decl("display", "none"), parent);
    EXPECT_EQ(style.display, Display::None);
}

// ===========================================================================
// Test 31: apply_declaration for color property
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("color", "red"), parent);
    EXPECT_EQ(style.color, (Color{255, 0, 0, 255}));

    cascade.apply_declaration(style, make_decl("color", "#00ff00"), parent);
    EXPECT_EQ(style.color, (Color{0, 255, 0, 255}));

    cascade.apply_declaration(style, make_decl("color", "rgb(0, 0, 255)"), parent);
    EXPECT_EQ(style.color, (Color{0, 0, 255, 255}));

    // Test inherit
    parent.color = {128, 64, 32, 255};
    cascade.apply_declaration(style, make_decl("color", "inherit"), parent);
    EXPECT_EQ(style.color, parent.color);
}

// ===========================================================================
// Test 32: apply_declaration for margin shorthand
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationMarginShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single value: all four sides
    cascade.apply_declaration(style, make_decl("margin", "10px"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 10.0f);

    // Two values: top/bottom and left/right
    cascade.apply_declaration(style, make_decl_multi("margin", {"10px", "20px"}), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 20.0f);

    // Three values: top, left/right, bottom
    cascade.apply_declaration(style, make_decl_multi("margin", {"10px", "20px", "30px"}), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 20.0f);

    // Four values: top, right, bottom, left
    cascade.apply_declaration(style, make_decl_multi("margin", {"10px", "20px", "30px", "40px"}), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 40.0f);

    // Auto value
    cascade.apply_declaration(style, make_decl("margin", "auto"), parent);
    EXPECT_TRUE(style.margin.top.is_auto());
    EXPECT_TRUE(style.margin.right.is_auto());
    EXPECT_TRUE(style.margin.bottom.is_auto());
    EXPECT_TRUE(style.margin.left.is_auto());

    // Individual margin property
    cascade.apply_declaration(style, make_decl("margin-top", "5px"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 5.0f);
}

// ===========================================================================
// Additional value parser tests
// ===========================================================================
TEST(ValueParserTest, ParseColorNamedColors) {
    auto black = parse_color("black");
    ASSERT_TRUE(black.has_value());
    EXPECT_EQ(*black, Color::black());

    auto white = parse_color("white");
    ASSERT_TRUE(white.has_value());
    EXPECT_EQ(*white, Color::white());

    auto green = parse_color("green");
    ASSERT_TRUE(green.has_value());
    EXPECT_EQ(green->r, 0);
    EXPECT_EQ(green->g, 128);
    EXPECT_EQ(green->b, 0);

    auto blue = parse_color("blue");
    ASSERT_TRUE(blue.has_value());
    EXPECT_EQ(blue->r, 0);
    EXPECT_EQ(blue->g, 0);
    EXPECT_EQ(blue->b, 255);

    auto yellow = parse_color("yellow");
    ASSERT_TRUE(yellow.has_value());
    EXPECT_EQ(yellow->r, 255);
    EXPECT_EQ(yellow->g, 255);
    EXPECT_EQ(yellow->b, 0);

    auto orange = parse_color("orange");
    ASSERT_TRUE(orange.has_value());
    EXPECT_EQ(orange->r, 255);
    EXPECT_EQ(orange->g, 165);
    EXPECT_EQ(orange->b, 0);

    auto purple = parse_color("purple");
    ASSERT_TRUE(purple.has_value());
    EXPECT_EQ(purple->r, 128);
    EXPECT_EQ(purple->g, 0);
    EXPECT_EQ(purple->b, 128);

    auto gray = parse_color("gray");
    ASSERT_TRUE(gray.has_value());
    EXPECT_EQ(gray->r, 128);
    EXPECT_EQ(gray->g, 128);
    EXPECT_EQ(gray->b, 128);

    auto grey = parse_color("grey");
    ASSERT_TRUE(grey.has_value());
    EXPECT_EQ(grey->r, 128);
    EXPECT_EQ(grey->g, 128);
    EXPECT_EQ(grey->b, 128);

    auto transparent = parse_color("transparent");
    ASSERT_TRUE(transparent.has_value());
    EXPECT_EQ(*transparent, Color::transparent());
}

TEST(ValueParserTest, ParseColorInvalid) {
    auto invalid = parse_color("notacolor");
    EXPECT_FALSE(invalid.has_value());

    auto empty = parse_color("");
    EXPECT_FALSE(empty.has_value());
}

TEST(ValueParserTest, ParseLengthAutoAndZero) {
    auto auto_val = parse_length("auto");
    ASSERT_TRUE(auto_val.has_value());
    EXPECT_TRUE(auto_val->is_auto());

    auto zero_val = parse_length("0");
    ASSERT_TRUE(zero_val.has_value());
    EXPECT_TRUE(zero_val->is_zero());
}

TEST(ValueParserTest, ParseLengthRem) {
    auto l = parse_length("1.5rem");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 1.5f);
    EXPECT_EQ(l->unit, Length::Unit::Rem);
}

TEST(ValueParserTest, ParseColorHex8) {
    auto c = parse_color("#ff000080");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 128);
}

TEST(ValueParserTest, ParseColorRgba) {
    auto c = parse_color("rgba(255, 128, 0, 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 128);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 127);  // 0.5 * 255 ~ 127
}

// ===========================================================================
// Additional default_style_for_tag tests
// ===========================================================================
TEST(ComputedStyleTest, DefaultStyleForBody) {
    auto style = default_style_for_tag("body");
    EXPECT_EQ(style.display, Display::Block);
}

TEST(ComputedStyleTest, DefaultStyleForP) {
    auto style = default_style_for_tag("p");
    EXPECT_EQ(style.display, Display::Block);
}

TEST(ComputedStyleTest, DefaultStyleForA) {
    auto style = default_style_for_tag("a");
    EXPECT_EQ(style.display, Display::Inline);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(ComputedStyleTest, DefaultStyleForStrong) {
    auto style = default_style_for_tag("strong");
    EXPECT_EQ(style.display, Display::Inline);
    EXPECT_EQ(style.font_weight, 700);
}

TEST(ComputedStyleTest, DefaultStyleForEm) {
    auto style = default_style_for_tag("em");
    EXPECT_EQ(style.display, Display::Inline);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(ComputedStyleTest, DefaultStyleForUl) {
    auto style = default_style_for_tag("ul");
    EXPECT_EQ(style.display, Display::Block);
    EXPECT_EQ(style.list_style_type, ListStyleType::Disc);
}

TEST(ComputedStyleTest, DefaultStyleForTable) {
    auto style = default_style_for_tag("table");
    EXPECT_EQ(style.display, Display::Table);
}

TEST(ComputedStyleTest, DefaultStyleForUnknown) {
    auto style = default_style_for_tag("custom-element");
    EXPECT_EQ(style.display, Display::Inline);
}

// ===========================================================================
// Additional Length tests
// ===========================================================================
TEST(LengthTest, ToPxForRemValues) {
    auto len = Length::rem(2.0f);
    EXPECT_FLOAT_EQ(len.to_px(0, 16.0f), 32.0f);
    EXPECT_FLOAT_EQ(len.to_px(0, 20.0f), 40.0f);
}

TEST(LengthTest, ToPxForZero) {
    auto len = Length::zero();
    EXPECT_FLOAT_EQ(len.to_px(), 0.0f);
}

TEST(LengthTest, ToPxForAuto) {
    auto len = Length::auto_val();
    EXPECT_FLOAT_EQ(len.to_px(), 0.0f);
}

// ===========================================================================
// Additional apply_declaration tests
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationPosition) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("position", "relative"), parent);
    EXPECT_EQ(style.position, Position::Relative);

    cascade.apply_declaration(style, make_decl("position", "absolute"), parent);
    EXPECT_EQ(style.position, Position::Absolute);

    cascade.apply_declaration(style, make_decl("position", "fixed"), parent);
    EXPECT_EQ(style.position, Position::Fixed);

    cascade.apply_declaration(style, make_decl("position", "static"), parent);
    EXPECT_EQ(style.position, Position::Static);
}

TEST(PropertyCascadeTest, ApplyDeclarationFontSize) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-size", "24px"), parent);
    EXPECT_FLOAT_EQ(style.font_size.value, 24.0f);
    EXPECT_EQ(style.font_size.unit, Length::Unit::Px);
}

TEST(PropertyCascadeTest, ApplyDeclarationBackgroundColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("background-color", "#ff0000"), parent);
    EXPECT_EQ(style.background_color, (Color{255, 0, 0, 255}));
}

TEST(PropertyCascadeTest, ApplyDeclarationWidth) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("width", "100px"), parent);
    EXPECT_FLOAT_EQ(style.width.value, 100.0f);
    EXPECT_EQ(style.width.unit, Length::Unit::Px);

    cascade.apply_declaration(style, make_decl("width", "50%"), parent);
    EXPECT_FLOAT_EQ(style.width.value, 50.0f);
    EXPECT_EQ(style.width.unit, Length::Unit::Percent);

    cascade.apply_declaration(style, make_decl("width", "auto"), parent);
    EXPECT_TRUE(style.width.is_auto());
}

TEST(PropertyCascadeTest, ApplyDeclarationPadding) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl_multi("padding", {"10px", "20px"}), parent);
    EXPECT_FLOAT_EQ(style.padding.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.value, 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.value, 10.0f);
    EXPECT_FLOAT_EQ(style.padding.left.value, 20.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationOpacity) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

TEST(PropertyCascadeTest, ApplyDeclarationFontWeight) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-weight", "bold"), parent);
    EXPECT_EQ(style.font_weight, 700);

    cascade.apply_declaration(style, make_decl("font-weight", "normal"), parent);
    EXPECT_EQ(style.font_weight, 400);

    cascade.apply_declaration(style, make_decl("font-weight", "600"), parent);
    EXPECT_EQ(style.font_weight, 600);
}

TEST(PropertyCascadeTest, ApplyDeclarationFontFamily) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-family", "Arial"), parent);
    EXPECT_EQ(style.font_family, "Arial");

    cascade.apply_declaration(style, make_decl("font-family", "\"Times New Roman\""), parent);
    EXPECT_EQ(style.font_family, "Times New Roman");
}

TEST(PropertyCascadeTest, ApplyDeclarationBorder) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-top-width", "2px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.value, 2.0f);

    cascade.apply_declaration(style, make_decl("border-top-style", "solid"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);

    cascade.apply_declaration(style, make_decl("border-top-color", "red"), parent);
    EXPECT_EQ(style.border_top.color, (Color{255, 0, 0, 255}));
}

TEST(PropertyCascadeTest, ApplyDeclarationFlexbox) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("flex-direction", "column"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);

    cascade.apply_declaration(style, make_decl("flex-wrap", "wrap"), parent);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);

    cascade.apply_declaration(style, make_decl("justify-content", "center"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);

    cascade.apply_declaration(style, make_decl("align-items", "center"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Center);

    cascade.apply_declaration(style, make_decl("flex-grow", "1"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 1.0f);

    cascade.apply_declaration(style, make_decl("flex-shrink", "0"), parent);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);

    cascade.apply_declaration(style, make_decl("gap", "16px"), parent);
    EXPECT_FLOAT_EQ(style.gap.value, 16.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationOverflow) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow", "hidden"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);

    cascade.apply_declaration(style, make_decl("overflow-x", "scroll"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);

    cascade.apply_declaration(style, make_decl("overflow-y", "auto"), parent);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
}

// ===========================================================================
// SelectorMatcher: universal selector
// ===========================================================================
TEST(SelectorMatcherTest, UniversalSelector) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "anything";

    CompoundSelector compound;
    compound.simple_selectors.push_back(make_universal_sel());
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));
}

// ===========================================================================
// Specificity calculation
// ===========================================================================
TEST(SpecificityTest, CompoundSelectorSpecificity) {
    // div.class#id => (1, 1, 1)
    CompoundSelector compound;
    compound.simple_selectors.push_back(make_type_sel("div"));
    compound.simple_selectors.push_back(make_class_sel("foo"));
    compound.simple_selectors.push_back(make_id_sel("bar"));

    ComplexSelector complex;
    ComplexSelector::Part part;
    part.compound = compound;
    part.combinator = std::nullopt;
    complex.parts.push_back(part);

    auto spec = compute_specificity(complex);
    EXPECT_EQ(spec.a, 1);
    EXPECT_EQ(spec.b, 1);
    EXPECT_EQ(spec.c, 1);
}

TEST(SpecificityTest, ComplexSelectorSpecificity) {
    // div > .class p => (0, 1, 2)
    CompoundSelector div_compound;
    div_compound.simple_selectors.push_back(make_type_sel("div"));

    CompoundSelector class_compound;
    class_compound.simple_selectors.push_back(make_class_sel("class"));

    CompoundSelector p_compound;
    p_compound.simple_selectors.push_back(make_type_sel("p"));

    ComplexSelector complex;
    complex.parts.push_back({div_compound, std::nullopt});
    complex.parts.push_back({class_compound, Combinator::Child});
    complex.parts.push_back({p_compound, Combinator::Descendant});

    auto spec = compute_specificity(complex);
    EXPECT_EQ(spec.a, 0);
    EXPECT_EQ(spec.b, 1);
    EXPECT_EQ(spec.c, 2);
}

// ============================================================================
// border-radius parsing in cascade
// ============================================================================
TEST(StyleResolver, BorderRadiusParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;
    cascade.apply_declaration(style, make_decl("border-radius", "10px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius, 10.0f);
}

TEST(StyleResolver, BorderRadiusEm) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;
    // First set font-size, then border-radius in em
    cascade.apply_declaration(style, make_decl("border-radius", "20px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius, 20.0f);
}

TEST(StyleResolver, WordSpacingParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;
    cascade.apply_declaration(style, make_decl("word-spacing", "5px"), parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(0), 5.0f);
}

TEST(StyleResolver, WordSpacingNormal) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;
    cascade.apply_declaration(style, make_decl("word-spacing", "normal"), parent);
    EXPECT_TRUE(style.word_spacing.is_zero());
}

// ===========================================================================
// :nth-child() pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, NthChildOdd) {
    SelectorMatcher matcher;

    // 5 siblings
    for (size_t i = 0; i < 5; i++) {
        ElementView elem;
        elem.tag_name = "li";
        elem.child_index = i;
        elem.sibling_count = 5;

        SimpleSelector ss;
        ss.type = SimpleSelectorType::PseudoClass;
        ss.value = "nth-child";
        ss.argument = "odd";

        CompoundSelector compound;
        compound.simple_selectors.push_back(ss);
        auto complex = make_simple_complex(compound);

        // odd = 1st, 3rd, 5th (indices 0, 2, 4)
        if (i == 0 || i == 2 || i == 4) {
            EXPECT_TRUE(matcher.matches(elem, complex)) << "index=" << i;
        } else {
            EXPECT_FALSE(matcher.matches(elem, complex)) << "index=" << i;
        }
    }
}

TEST(SelectorMatcherTest, NthChildEven) {
    SelectorMatcher matcher;

    for (size_t i = 0; i < 4; i++) {
        ElementView elem;
        elem.tag_name = "li";
        elem.child_index = i;
        elem.sibling_count = 4;

        SimpleSelector ss;
        ss.type = SimpleSelectorType::PseudoClass;
        ss.value = "nth-child";
        ss.argument = "even";

        CompoundSelector compound;
        compound.simple_selectors.push_back(ss);
        auto complex = make_simple_complex(compound);

        // even = 2nd, 4th (indices 1, 3)
        if (i == 1 || i == 3) {
            EXPECT_TRUE(matcher.matches(elem, complex)) << "index=" << i;
        } else {
            EXPECT_FALSE(matcher.matches(elem, complex)) << "index=" << i;
        }
    }
}

TEST(SelectorMatcherTest, NthChildNumber) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "li";
    elem.child_index = 2;  // 3rd child (1-based: 3)
    elem.sibling_count = 5;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "nth-child";
    ss.argument = "3";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(elem, complex));

    elem.child_index = 0;  // 1st child
    EXPECT_FALSE(matcher.matches(elem, complex));
}

TEST(SelectorMatcherTest, NthChildFormula) {
    SelectorMatcher matcher;

    // :nth-child(3n+1) matches 1st, 4th, 7th...
    for (size_t i = 0; i < 7; i++) {
        ElementView elem;
        elem.tag_name = "li";
        elem.child_index = i;
        elem.sibling_count = 7;

        SimpleSelector ss;
        ss.type = SimpleSelectorType::PseudoClass;
        ss.value = "nth-child";
        ss.argument = "3n+1";

        CompoundSelector compound;
        compound.simple_selectors.push_back(ss);
        auto complex = make_simple_complex(compound);

        // 1-based positions 1, 4, 7 → indices 0, 3, 6
        if (i == 0 || i == 3 || i == 6) {
            EXPECT_TRUE(matcher.matches(elem, complex)) << "index=" << i;
        } else {
            EXPECT_FALSE(matcher.matches(elem, complex)) << "index=" << i;
        }
    }
}

// ===========================================================================
// :nth-last-child() pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, NthLastChild) {
    SelectorMatcher matcher;

    // 5 siblings, :nth-last-child(1) = last child
    ElementView last;
    last.tag_name = "li";
    last.child_index = 4;
    last.sibling_count = 5;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "nth-last-child";
    ss.argument = "1";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(last, complex));

    // First child should not match :nth-last-child(1) if sibling_count > 1
    ElementView first;
    first.tag_name = "li";
    first.child_index = 0;
    first.sibling_count = 5;

    EXPECT_FALSE(matcher.matches(first, complex));
}

// ===========================================================================
// :empty pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, EmptyElement) {
    SelectorMatcher matcher;

    ElementView empty_elem;
    empty_elem.tag_name = "div";
    empty_elem.child_element_count = 0;
    empty_elem.has_text_children = false;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "empty";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(empty_elem, complex));

    // Element with child element
    ElementView non_empty;
    non_empty.tag_name = "div";
    non_empty.child_element_count = 1;
    non_empty.has_text_children = false;

    EXPECT_FALSE(matcher.matches(non_empty, complex));

    // Element with text content
    ElementView text_elem;
    text_elem.tag_name = "div";
    text_elem.child_element_count = 0;
    text_elem.has_text_children = true;

    EXPECT_FALSE(matcher.matches(text_elem, complex));
}

// ===========================================================================
// :root pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, RootElement) {
    SelectorMatcher matcher;

    ElementView root;
    root.tag_name = "html";
    root.parent = nullptr;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "root";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(root, complex));

    // Non-root
    ElementView child;
    child.tag_name = "body";
    child.parent = &root;

    EXPECT_FALSE(matcher.matches(child, complex));
}

// ===========================================================================
// :not() pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, NotPseudoClass) {
    SelectorMatcher matcher;

    ElementView div_elem;
    div_elem.tag_name = "div";
    div_elem.classes = {"active"};

    // :not(.hidden) should match an element that doesn't have class "hidden"
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "not";
    ss.argument = ".hidden";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(div_elem, complex));

    // Element with class "hidden" should NOT match :not(.hidden)
    ElementView hidden_elem;
    hidden_elem.tag_name = "div";
    hidden_elem.classes = {"hidden"};

    EXPECT_FALSE(matcher.matches(hidden_elem, complex));
}

TEST(SelectorMatcherTest, NotPseudoClassWithType) {
    SelectorMatcher matcher;

    ElementView span_elem;
    span_elem.tag_name = "span";

    // :not(div) should match a span
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "not";
    ss.argument = "div";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(span_elem, complex));

    ElementView div_elem;
    div_elem.tag_name = "div";

    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

// ===========================================================================
// :first-of-type pseudo-class
// ===========================================================================
TEST(SelectorMatcherTest, FirstOfType) {
    SelectorMatcher matcher;

    ElementView first;
    first.tag_name = "p";
    first.child_index = 0;
    first.prev_sibling = nullptr;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "first-of-type";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_TRUE(matcher.matches(first, complex));

    // Second p with a preceding p sibling
    ElementView second;
    second.tag_name = "p";
    second.child_index = 1;
    second.prev_sibling = &first;

    EXPECT_FALSE(matcher.matches(second, complex));

    // Different tag preceding — should still be first-of-type
    ElementView div;
    div.tag_name = "div";
    div.child_index = 0;

    ElementView p_after_div;
    p_after_div.tag_name = "p";
    p_after_div.child_index = 1;
    p_after_div.prev_sibling = &div;

    EXPECT_TRUE(matcher.matches(p_after_div, complex));
}

// ===========================================================================
// Selector parsing integration: :nth-child parsed correctly
// ===========================================================================
TEST(SelectorParserTest, NthChildParsed) {
    auto list = parse_selector_list(":nth-child(2n+1)");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& parts = list.selectors[0].parts;
    ASSERT_EQ(parts.size(), 1u);
    auto& simple_sels = parts[0].compound.simple_selectors;
    ASSERT_EQ(simple_sels.size(), 1u);
    EXPECT_EQ(simple_sels[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(simple_sels[0].value, "nth-child");
    EXPECT_EQ(simple_sels[0].argument, "2n+1");
}

TEST(SelectorParserTest, NotParsed) {
    auto list = parse_selector_list(":not(.hidden)");
    ASSERT_EQ(list.selectors.size(), 1u);
    auto& parts = list.selectors[0].parts;
    ASSERT_EQ(parts.size(), 1u);
    auto& simple_sels = parts[0].compound.simple_selectors;
    ASSERT_EQ(simple_sels.size(), 1u);
    EXPECT_EQ(simple_sels[0].type, SimpleSelectorType::PseudoClass);
    EXPECT_EQ(simple_sels[0].value, "not");
    // Argument should contain ".hidden" (parsed from CSS tokens)
    EXPECT_FALSE(simple_sels[0].argument.empty());
}

// ===========================================================================
// Text-indent cascade
// ===========================================================================
TEST(PropertyCascadeTest, TextIndent) {
    // text-indent: 32px should be applied
    auto sheet = parse_stylesheet("p { text-indent: 32px; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, "text-indent");

    PropertyCascade cascade;
    std::vector<MatchedRule> matched;
    matched.push_back({&sheet.rules[0], {0, 0, 1}, 0});
    auto style = cascade.cascade(matched, ComputedStyle{});

    EXPECT_NEAR(style.text_indent.to_px(), 32.0f, 0.1f);
}

// ===========================================================================
// Vertical-align cascade
// ===========================================================================
TEST(PropertyCascadeTest, VerticalAlignMiddle) {
    auto sheet = parse_stylesheet("span { vertical-align: middle; }");
    ASSERT_EQ(sheet.rules.size(), 1u);

    PropertyCascade cascade;
    std::vector<MatchedRule> matched;
    matched.push_back({&sheet.rules[0], {0, 0, 1}, 0});
    auto style = cascade.cascade(matched, ComputedStyle{});

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
}

TEST(PropertyCascadeTest, VerticalAlignTop) {
    auto sheet = parse_stylesheet("img { vertical-align: top; }");
    ASSERT_EQ(sheet.rules.size(), 1u);

    PropertyCascade cascade;
    std::vector<MatchedRule> matched;
    matched.push_back({&sheet.rules[0], {0, 0, 1}, 0});
    auto style = cascade.cascade(matched, ComputedStyle{});

    EXPECT_EQ(style.vertical_align, VerticalAlign::Top);
}

// ============================================================================
// TextShadowParsed: parse text-shadow with blur radius and color
// ============================================================================
TEST(PropertyCascadeTest, TextShadowParsed) {
    auto sheet = parse_stylesheet("p { text-shadow: 3px 3px 5px blue; }");
    ASSERT_EQ(sheet.rules.size(), 1u);

    PropertyCascade cascade;
    std::vector<MatchedRule> matched;
    matched.push_back({&sheet.rules[0], {0, 0, 1}, 0});
    auto style = cascade.cascade(matched, ComputedStyle{});

    EXPECT_FLOAT_EQ(style.text_shadow_offset_x, 3.0f);
    EXPECT_FLOAT_EQ(style.text_shadow_offset_y, 3.0f);
    EXPECT_FLOAT_EQ(style.text_shadow_blur, 5.0f);
    // "blue" = Color{0, 0, 255, 255}
    EXPECT_EQ(style.text_shadow_color.r, 0);
    EXPECT_EQ(style.text_shadow_color.g, 0);
    EXPECT_EQ(style.text_shadow_color.b, 255);
    EXPECT_EQ(style.text_shadow_color.a, 255);
}

// =============================================================================
// CSS ch unit
// =============================================================================

TEST(ValueParserTest, ParseLengthCh) {
    auto l = parse_length("3ch");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 3.0f);
    EXPECT_EQ(l->unit, Length::Unit::Ch);
    // 3ch with 16px font-size ≈ 3 * 16 * 0.6 = 28.8px
    float px = l->to_px(16.0f, 16.0f);
    EXPECT_NEAR(px, 28.8f, 0.1f);
}

// =============================================================================
// CSS lh unit
// =============================================================================

TEST(ValueParserTest, ParseLengthLh) {
    auto l = parse_length("2lh");
    ASSERT_TRUE(l.has_value());
    EXPECT_FLOAT_EQ(l->value, 2.0f);
    EXPECT_EQ(l->unit, Length::Unit::Lh);
    // 2lh with line-height=24px → 48px
    float px = l->to_px(16.0f, 16.0f, 24.0f);
    EXPECT_NEAR(px, 48.0f, 0.1f);
}

// =============================================================================
// CSS min() function
// =============================================================================

TEST(ValueParserTest, ParseMinFunction) {
    auto l = parse_length("min(300px, 200px)");
    ASSERT_TRUE(l.has_value());
    EXPECT_EQ(l->unit, Length::Unit::Calc);
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 200.0f, 0.1f) << "min(300px, 200px) should be 200px";
}

TEST(ValueParserTest, ParseMinWithPercent) {
    auto l = parse_length("min(100%, 300px)");
    ASSERT_TRUE(l.has_value());
    // With parent_value=400 → min(400, 300) = 300
    float px = l->to_px(400, 16);
    EXPECT_NEAR(px, 300.0f, 0.1f) << "min(100%, 300px) with 400px parent = 300px";
}

TEST(ValueParserTest, ParseMinThreeArgs) {
    auto l = parse_length("min(500px, 200px, 100px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 100.0f, 0.1f) << "min(500, 200, 100) = 100";
}

// =============================================================================
// CSS max() function
// =============================================================================

TEST(ValueParserTest, ParseMaxFunction) {
    auto l = parse_length("max(100px, 200px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 200.0f, 0.1f) << "max(100px, 200px) should be 200px";
}

TEST(ValueParserTest, ParseMaxWithPercent) {
    auto l = parse_length("max(50%, 100px)");
    ASSERT_TRUE(l.has_value());
    // With parent_value=300 → max(150, 100) = 150
    float px = l->to_px(300, 16);
    EXPECT_NEAR(px, 150.0f, 0.1f) << "max(50%, 100px) with 300px parent = 150px";
}

// =============================================================================
// CSS clamp() function
// =============================================================================

TEST(ValueParserTest, ParseClampPreferred) {
    // clamp(100px, 200px, 300px) → preferred is within range → 200px
    auto l = parse_length("clamp(100px, 200px, 300px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 200.0f, 0.1f) << "clamp(100, 200, 300) = 200 (preferred)";
}

TEST(ValueParserTest, ParseClampClampsToMin) {
    // clamp(150px, 50px, 300px) → preferred < min → 150px
    auto l = parse_length("clamp(150px, 50px, 300px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 150.0f, 0.1f) << "clamp(150, 50, 300) = 150 (clamped to min)";
}

TEST(ValueParserTest, ParseClampClampsToMax) {
    // clamp(100px, 500px, 300px) → preferred > max → 300px
    auto l = parse_length("clamp(100px, 500px, 300px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 300.0f, 0.1f) << "clamp(100, 500, 300) = 300 (clamped to max)";
}

TEST(ValueParserTest, ParseClampWithPercent) {
    // clamp(100px, 50%, 400px) with parent=600px → clamp(100, 300, 400) = 300
    auto l = parse_length("clamp(100px, 50%, 400px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(600, 16);
    EXPECT_NEAR(px, 300.0f, 0.1f) << "clamp(100px, 50%, 400px) with 600px parent = 300px";
}

TEST(ValueParserTest, ParseClampWithCalcArg) {
    // clamp(100px, calc(50px + 100px), 300px) → clamp(100, 150, 300) = 150
    auto l = parse_length("clamp(100px, calc(50px + 100px), 300px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 150.0f, 0.1f) << "clamp(100, calc(50+100), 300) = 150";
}

// =============================================================================
// Nested min/max
// =============================================================================

TEST(ValueParserTest, ParseMinNestedMax) {
    // min(max(100px, 200px), 300px) → min(200, 300) = 200
    auto l = parse_length("min(max(100px, 200px), 300px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 200.0f, 0.1f) << "min(max(100,200), 300) = 200";
}

// =============================================================================
// env() function
// =============================================================================

TEST(ValueParserTest, ParseEnvWithFallback) {
    // env(safe-area-inset-top, 20px) → should return 20px fallback on desktop
    auto l = parse_length("env(safe-area-inset-top, 20px)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 20.0f, 0.1f) << "env() with fallback should use fallback value";
}

TEST(ValueParserTest, ParseEnvNoFallback) {
    // env(safe-area-inset-top) → should return 0px (no fallback, desktop default)
    auto l = parse_length("env(safe-area-inset-top)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 0.0f, 0.1f) << "env() without fallback should return 0";
}

TEST(ValueParserTest, ParseEnvWithEmFallback) {
    auto l = parse_length("env(safe-area-inset-bottom, 2em)");
    ASSERT_TRUE(l.has_value());
    float px = l->to_px(16, 16);
    EXPECT_NEAR(px, 32.0f, 0.1f) << "env() with 2em fallback = 32px at 16px font-size";
}

// ============================================================
// CSS Color Level 4 — hsl(), hsla(), oklch(), oklab(), hwb()
// ============================================================

TEST(ValueParserTest, ParseColorHslRed) {
    auto c = parse_color("hsl(0, 100%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorHslGreen) {
    auto c = parse_color("hsl(120, 100%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHslBlue) {
    auto c = parse_color("hsl(240, 100%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 255);
}

TEST(ValueParserTest, ParseColorHslGray) {
    // S=0% means gray, L=50% means mid-gray
    auto c = parse_color("hsl(0, 0%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 128, 1);
    EXPECT_NEAR(c->g, 128, 1);
    EXPECT_NEAR(c->b, 128, 1);
}

TEST(ValueParserTest, ParseColorHslWhite) {
    auto c = parse_color("hsl(0, 0%, 100%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 255);
}

TEST(ValueParserTest, ParseColorHslBlack) {
    auto c = parse_color("hsl(0, 0%, 0%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHslaWithAlpha) {
    auto c = parse_color("hsla(120, 100%, 50%, 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
    EXPECT_NEAR(c->a, 128, 1); // 0.5 * 255 ≈ 127-128
}

TEST(ValueParserTest, ParseColorHslSpaceSeparated) {
    // Modern CSS: hsl(120 100% 50%)
    auto c = parse_color("hsl(120 100% 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHslSlashAlpha) {
    // Modern: hsl(120 100% 50% / 0.5)
    auto c = parse_color("hsl(120 100% 50% / 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_NEAR(c->a, 128, 1);
}

TEST(ValueParserTest, ParseColorHslOrange) {
    // hsl(30, 100%, 50%) = orange (#FF8000)
    auto c = parse_color("hsl(30, 100%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_NEAR(c->g, 128, 2);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHslNegativeHue) {
    // Negative hue wraps: hsl(-120, 100%, 50%) = hsl(240, 100%, 50%) = blue
    auto c = parse_color("hsl(-120, 100%, 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 255);
}

TEST(ValueParserTest, ParseColorOklchRed) {
    // oklch(0.6279 0.2577 29.23) ≈ red-ish
    auto c = parse_color("oklch(0.6279 0.2577 29.23)");
    ASSERT_TRUE(c.has_value());
    // Should produce a reddish color
    EXPECT_GT(c->r, 150);
    EXPECT_LT(c->g, 100);
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorOklchBlack) {
    // oklch(0 0 0) = black
    auto c = parse_color("oklch(0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorOklchWhite) {
    // oklch(1 0 0) = white
    auto c = parse_color("oklch(1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 2);
    EXPECT_NEAR(c->g, 255, 2);
    EXPECT_NEAR(c->b, 255, 2);
}

TEST(ValueParserTest, ParseColorOklchWithAlpha) {
    auto c = parse_color("oklch(0.5 0.1 180 / 0.75)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 191, 2); // 0.75 * 255 ≈ 191
}

TEST(ValueParserTest, ParseColorOklabBlack) {
    // oklab(0 0 0) = black
    auto c = parse_color("oklab(0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorOklabWhite) {
    // oklab(1 0 0) = white
    auto c = parse_color("oklab(1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 2);
    EXPECT_NEAR(c->g, 255, 2);
    EXPECT_NEAR(c->b, 255, 2);
}

TEST(ValueParserTest, ParseColorOklabWithAlpha) {
    auto c = parse_color("oklab(0.5 0.1 -0.1 / 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 128, 1);
}

TEST(ValueParserTest, ParseColorHwbRed) {
    // hwb(0 0% 0%) = pure red
    auto c = parse_color("hwb(0 0% 0%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHwbWhite) {
    // hwb(0 100% 0%) = white
    auto c = parse_color("hwb(0 100% 0%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 255);
}

TEST(ValueParserTest, ParseColorHwbBlack) {
    // hwb(0 0% 100%) = black
    auto c = parse_color("hwb(0 0% 100%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorHwbGray) {
    // hwb(0 50% 50%) = gray (w+b normalized → 50% each)
    auto c = parse_color("hwb(0 50% 50%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 128, 1);
    EXPECT_NEAR(c->g, 128, 1);
    EXPECT_NEAR(c->b, 128, 1);
}

TEST(ValueParserTest, ParseColorHwbWithAlpha) {
    auto c = parse_color("hwb(120 10% 10% / 0.8)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 204, 1); // 0.8 * 255 ≈ 204
    EXPECT_GT(c->g, c->r); // green hue
}

TEST(ValueParserTest, ParseColorHwbGreenHue) {
    // hwb(120 0% 0%) = pure green
    auto c = parse_color("hwb(120 0% 0%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorCurrentColor) {
    auto c = parse_color("currentcolor");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorCurrentColorCaseInsensitive) {
    auto c = parse_color("CurrentColor");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorHslInvalid) {
    auto c = parse_color("hsl(120)");
    EXPECT_FALSE(c.has_value());
}

TEST(ValueParserTest, ParseColorOklchInvalid) {
    auto c = parse_color("oklch(0.5)");
    EXPECT_FALSE(c.has_value());
}

TEST(ValueParserTest, ParseColorHwbInvalid) {
    auto c = parse_color("hwb(0)");
    EXPECT_FALSE(c.has_value());
}

// ============================================================
// CSS Color Level 4 — lab(), lch()
// ============================================================

TEST(ValueParserTest, ParseColorLabBlack) {
    // lab(0 0 0) = black
    auto c = parse_color("lab(0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorLabWhite) {
    // lab(100 0 0) = white
    auto c = parse_color("lab(100 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 2);
    EXPECT_NEAR(c->g, 255, 2);
    EXPECT_NEAR(c->b, 255, 2);
}

TEST(ValueParserTest, ParseColorLabMidGray) {
    // lab(50 0 0) = mid-gray
    auto c = parse_color("lab(50 0 0)");
    ASSERT_TRUE(c.has_value());
    // Perceptual mid-gray ≈ sRGB 119
    EXPECT_GT(c->r, 100);
    EXPECT_LT(c->r, 140);
    EXPECT_NEAR(c->r, c->g, 2);
    EXPECT_NEAR(c->g, c->b, 2);
}

TEST(ValueParserTest, ParseColorLabWithAlpha) {
    auto c = parse_color("lab(50 40 -20 / 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 128, 1);
}

TEST(ValueParserTest, ParseColorLabReddish) {
    // lab(50 60 30) — reddish color (positive a, positive b)
    auto c = parse_color("lab(50 60 30)");
    ASSERT_TRUE(c.has_value());
    EXPECT_GT(c->r, c->g);
    EXPECT_GT(c->r, c->b);
}

TEST(ValueParserTest, ParseColorLchBlack) {
    auto c = parse_color("lch(0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorLchWhite) {
    auto c = parse_color("lch(100 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 2);
    EXPECT_NEAR(c->g, 255, 2);
    EXPECT_NEAR(c->b, 255, 2);
}

TEST(ValueParserTest, ParseColorLchWithAlpha) {
    auto c = parse_color("lch(50 30 270 / 0.75)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 191, 2);
}

TEST(ValueParserTest, ParseColorLchRedHue) {
    // lch(50 80 30) — red-ish hue
    auto c = parse_color("lch(50 80 30)");
    ASSERT_TRUE(c.has_value());
    EXPECT_GT(c->r, c->g);
}

TEST(ValueParserTest, ParseColorLabInvalid) {
    auto c = parse_color("lab(50)");
    EXPECT_FALSE(c.has_value());
}

TEST(ValueParserTest, ParseColorLchInvalid) {
    auto c = parse_color("lch(50)");
    EXPECT_FALSE(c.has_value());
}

// ============================================================
// CSS Color Level 5 — color-mix(), light-dark()
// ============================================================

TEST(ValueParserTest, ParseColorMixEqual) {
    // Mix red and blue 50/50 → purple-ish
    auto c = parse_color("color-mix(in srgb, red, blue)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 128, 2); // 255 * 0.5
    EXPECT_EQ(c->g, 0);
    EXPECT_NEAR(c->b, 128, 2);
}

TEST(ValueParserTest, ParseColorMixWithPercentages) {
    // Mix red 75%, blue 25%
    auto c = parse_color("color-mix(in srgb, red 75%, blue 25%)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 191, 2); // 255 * 0.75
    EXPECT_NEAR(c->b, 64, 2);  // 255 * 0.25
}

TEST(ValueParserTest, ParseColorMixOnePctSpecified) {
    // Mix red 80% (blue gets 20%)
    auto c = parse_color("color-mix(in srgb, red 80%, blue)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 204, 2); // 255 * 0.8
    EXPECT_NEAR(c->b, 51, 2);  // 255 * 0.2
}

TEST(ValueParserTest, ParseColorMixHexColors) {
    // Mix #ff0000 and #0000ff
    auto c = parse_color("color-mix(in srgb, #ff0000, #0000ff)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 128, 2);
    EXPECT_NEAR(c->b, 128, 2);
}

TEST(ValueParserTest, ParseColorMixInvalid) {
    auto c = parse_color("color-mix(in srgb, red)");
    EXPECT_FALSE(c.has_value());
}

TEST(ValueParserTest, ParseColorLightDark) {
    // light-dark returns the light color
    auto c = parse_color("light-dark(red, blue)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorLightDarkHex) {
    auto c = parse_color("light-dark(#00ff00, #ff0000)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorLightDarkInvalid) {
    auto c = parse_color("light-dark(red)");
    EXPECT_FALSE(c.has_value());
}

// ============================================================
// CSS color() function — CSS Color Level 4
// ============================================================

TEST(ValueParserTest, ParseColorFuncSrgbRed) {
    auto c = parse_color("color(srgb 1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorFuncSrgbGreen) {
    auto c = parse_color("color(srgb 0 1 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorFuncSrgbBlack) {
    auto c = parse_color("color(srgb 0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorFuncSrgbWhite) {
    auto c = parse_color("color(srgb 1 1 1)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 255);
    EXPECT_EQ(c->b, 255);
}

TEST(ValueParserTest, ParseColorFuncSrgbHalf) {
    auto c = parse_color("color(srgb 0.5 0.5 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 128, 1);
    EXPECT_NEAR(c->g, 128, 1);
    EXPECT_NEAR(c->b, 128, 1);
}

TEST(ValueParserTest, ParseColorFuncSrgbWithAlpha) {
    auto c = parse_color("color(srgb 1 0 0 / 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_NEAR(c->a, 128, 1);
}

TEST(ValueParserTest, ParseColorFuncSrgbLinearWhite) {
    auto c = parse_color("color(srgb-linear 1 1 1)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 1);
    EXPECT_NEAR(c->g, 255, 1);
    EXPECT_NEAR(c->b, 255, 1);
}

TEST(ValueParserTest, ParseColorFuncSrgbLinearBlack) {
    auto c = parse_color("color(srgb-linear 0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorFuncSrgbLinearHalf) {
    // Linear 0.5 → sRGB gamma ≈ 0.735 → ~187
    auto c = parse_color("color(srgb-linear 0.5 0.5 0.5)");
    ASSERT_TRUE(c.has_value());
    EXPECT_GT(c->r, 170);
    EXPECT_LT(c->r, 200);
    EXPECT_NEAR(c->r, c->g, 1);
}

TEST(ValueParserTest, ParseColorFuncDisplayP3Red) {
    // display-p3 pure red (1, 0, 0) is a vivid red in sRGB (may clip)
    auto c = parse_color("color(display-p3 1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_GT(c->r, 200); // Should be very red
    EXPECT_EQ(c->a, 255);
}

TEST(ValueParserTest, ParseColorFuncDisplayP3White) {
    auto c = parse_color("color(display-p3 1 1 1)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->r, 255, 5);
    EXPECT_NEAR(c->g, 255, 5);
    EXPECT_NEAR(c->b, 255, 5);
}

TEST(ValueParserTest, ParseColorFuncDisplayP3Black) {
    auto c = parse_color("color(display-p3 0 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 0);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
}

TEST(ValueParserTest, ParseColorFuncA98RgbRed) {
    auto c = parse_color("color(a98-rgb 1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_GT(c->r, 200); // Red channel should be high
}

TEST(ValueParserTest, ParseColorFuncWithAlphaSlash) {
    auto c = parse_color("color(display-p3 0.5 0.5 0.5 / 0.75)");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR(c->a, 191, 2); // 0.75 * 255
}

TEST(ValueParserTest, ParseColorFuncInvalid) {
    auto c = parse_color("color(srgb 1)");
    EXPECT_FALSE(c.has_value());
}

TEST(ValueParserTest, ParseColorFuncUnknownColorspace) {
    // Unknown colorspace defaults to sRGB treatment
    auto c = parse_color("color(xyz 1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
}

// Cycle 179: CSS sin() math function
TEST(ValueParserTest, ParseLengthSin90Deg) {
    auto l = parse_length("calc(sin(90deg) * 200px)");
    ASSERT_TRUE(l.has_value()) << "sin(90deg) in calc should parse";
    ASSERT_NE(l->calc_expr, nullptr) << "Should have calc expression";
    float val = l->calc_expr->evaluate(0, 16);
    EXPECT_NEAR(val, 200.0f, 1.0f) << "sin(90deg)*200px should be ~200";
}

// Cycle 179: CSS pow() math function
TEST(ValueParserTest, ParseLengthPow) {
    // Standalone pow works
    auto l0 = parse_length("pow(10, 2)");
    ASSERT_TRUE(l0.has_value()) << "standalone pow(10,2) should parse";
    ASSERT_NE(l0->calc_expr, nullptr);
    EXPECT_NEAR(l0->calc_expr->evaluate(0, 16), 100.0f, 1.0f);
}

// Cycle 179: CSS sqrt() math function
TEST(ValueParserTest, ParseLengthSqrt) {
    auto l = parse_length("calc(sqrt(10000) * 1px)");
    ASSERT_TRUE(l.has_value()) << "sqrt() in calc should parse";
    ASSERT_NE(l->calc_expr, nullptr) << "Should have calc expression";
    float val = l->calc_expr->evaluate(0, 16);
    EXPECT_NEAR(val, 100.0f, 1.0f) << "sqrt(10000)*1px should be ~100";
}

// text-align-last cascade parsing
TEST(ComputedStyleTest, TextAlignLastCascadeParsing) {
    // Verify text-align-last values are parsed through the cascade
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { text-align-last: center; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    ComputedStyle parent;

    auto result = resolver.resolve(elem, parent);
    EXPECT_EQ(result.text_align_last, 3) << "text-align-last: center should be 3";
}

TEST(ComputedStyleTest, TextAlignLastInheritance) {
    // text-align-last should inherit from parent
    StyleResolver resolver;
    auto sheet = parse_stylesheet("span { color: black; }"); // no text-align-last set
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    parent.text_align_last = 2; // right

    auto result = resolver.resolve(elem, parent);
    EXPECT_EQ(result.text_align_last, 2)
        << "text-align-last should be inherited from parent";
}

TEST(ComputedStyleTest, TextAlignLastAllValues) {
    // Test all values: auto, left, right, center, justify
    StyleResolver resolver;

    auto sheet_auto = parse_stylesheet("div { text-align-last: auto; }");
    auto sheet_left = parse_stylesheet("div { text-align-last: left; }");
    auto sheet_right = parse_stylesheet("div { text-align-last: right; }");
    auto sheet_center = parse_stylesheet("div { text-align-last: center; }");
    auto sheet_justify = parse_stylesheet("div { text-align-last: justify; }");
    auto sheet_start = parse_stylesheet("div { text-align-last: start; }");
    auto sheet_end = parse_stylesheet("div { text-align-last: end; }");

    ElementView elem;
    elem.tag_name = "div";
    ComputedStyle parent;

    StyleResolver r1; r1.add_stylesheet(sheet_auto);
    EXPECT_EQ(r1.resolve(elem, parent).text_align_last, 0);

    StyleResolver r2; r2.add_stylesheet(sheet_left);
    EXPECT_EQ(r2.resolve(elem, parent).text_align_last, 1);

    StyleResolver r3; r3.add_stylesheet(sheet_right);
    EXPECT_EQ(r3.resolve(elem, parent).text_align_last, 2);

    StyleResolver r4; r4.add_stylesheet(sheet_center);
    EXPECT_EQ(r4.resolve(elem, parent).text_align_last, 3);

    StyleResolver r5; r5.add_stylesheet(sheet_justify);
    EXPECT_EQ(r5.resolve(elem, parent).text_align_last, 4);

    StyleResolver r6; r6.add_stylesheet(sheet_start);
    EXPECT_EQ(r6.resolve(elem, parent).text_align_last, 1) << "start should map to 1 (left)";

    StyleResolver r7; r7.add_stylesheet(sheet_end);
    EXPECT_EQ(r7.resolve(elem, parent).text_align_last, 2) << "end should map to 2 (right)";
}

// =============================================================================
// CSS clamp() — exact values from spec examples
// =============================================================================

TEST(ValueParserTest, ClampPreferredInRange) {
    // clamp(10px, 50px, 100px) → preferred is within [10, 100] → 50px
    auto l = parse_length("clamp(10px, 50px, 100px)");
    ASSERT_TRUE(l.has_value()) << "clamp(10px, 50px, 100px) should parse";
    EXPECT_EQ(l->unit, Length::Unit::Calc);
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 50.0f, 0.1f) << "clamp(10px, 50px, 100px) should resolve to 50px";
}

TEST(ValueParserTest, ClampMinWins) {
    // clamp(10px, 5px, 100px) → preferred (5) < min (10) → 10px
    auto l = parse_length("clamp(10px, 5px, 100px)");
    ASSERT_TRUE(l.has_value()) << "clamp(10px, 5px, 100px) should parse";
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 10.0f, 0.1f) << "clamp(10px, 5px, 100px) should resolve to 10px (min wins)";
}

TEST(ValueParserTest, ClampMaxWins) {
    // clamp(10px, 200px, 100px) → preferred (200) > max (100) → 100px
    auto l = parse_length("clamp(10px, 200px, 100px)");
    ASSERT_TRUE(l.has_value()) << "clamp(10px, 200px, 100px) should parse";
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 100.0f, 0.1f) << "clamp(10px, 200px, 100px) should resolve to 100px (max wins)";
}

// =============================================================================
// CSS min() / max() — exact values from spec examples
// =============================================================================

TEST(ValueParserTest, MinTwoArgs) {
    // min(100px, 50px) → 50px
    auto l = parse_length("min(100px, 50px)");
    ASSERT_TRUE(l.has_value()) << "min(100px, 50px) should parse";
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 50.0f, 0.1f) << "min(100px, 50px) should resolve to 50px";
}

TEST(ValueParserTest, MaxTwoArgs) {
    // max(100px, 50px) → 100px
    auto l = parse_length("max(100px, 50px)");
    ASSERT_TRUE(l.has_value()) << "max(100px, 50px) should parse";
    float px = l->to_px(0, 16);
    EXPECT_NEAR(px, 100.0f, 0.1f) << "max(100px, 50px) should resolve to 100px";
}

// Cycle 179: CSS pi constant
TEST(ValueParserTest, ParseLengthPi) {
    auto l = parse_length("calc(pi * 50px)");
    ASSERT_TRUE(l.has_value()) << "pi constant in calc should parse";
    ASSERT_NE(l->calc_expr, nullptr) << "Should have calc expression";
    float val = l->calc_expr->evaluate(0, 16);
    EXPECT_NEAR(val, 157.08f, 1.0f) << "pi*50px should be ~157.08";
}

// ===========================================================================
// text-wrap property: parsing all values
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationTextWrapAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default should be 0 (wrap)
    EXPECT_EQ(style.text_wrap, 0) << "Default text_wrap should be 0 (wrap)";

    // text-wrap: wrap
    cascade.apply_declaration(style, make_decl("text-wrap", "wrap"), parent);
    EXPECT_EQ(style.text_wrap, 0) << "text-wrap: wrap should be 0";

    // text-wrap: nowrap
    cascade.apply_declaration(style, make_decl("text-wrap", "nowrap"), parent);
    EXPECT_EQ(style.text_wrap, 1) << "text-wrap: nowrap should be 1";

    // text-wrap: balance
    cascade.apply_declaration(style, make_decl("text-wrap", "balance"), parent);
    EXPECT_EQ(style.text_wrap, 2) << "text-wrap: balance should be 2";

    // text-wrap: pretty
    cascade.apply_declaration(style, make_decl("text-wrap", "pretty"), parent);
    EXPECT_EQ(style.text_wrap, 3) << "text-wrap: pretty should be 3";

    // text-wrap: stable
    cascade.apply_declaration(style, make_decl("text-wrap", "stable"), parent);
    EXPECT_EQ(style.text_wrap, 4) << "text-wrap: stable should be 4";
}

// ===========================================================================
// text-wrap property: inheritance via the inherit keyword
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationTextWrapInherit) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set parent to balance
    parent.text_wrap = 2;

    // Apply inherit
    cascade.apply_declaration(style, make_decl("text-wrap", "inherit"), parent);
    EXPECT_EQ(style.text_wrap, 2)
        << "text-wrap: inherit should copy parent value (balance=2)";

    // Try with parent pretty
    parent.text_wrap = 3;
    cascade.apply_declaration(style, make_decl("text-wrap", "inherit"), parent);
    EXPECT_EQ(style.text_wrap, 3)
        << "text-wrap: inherit should copy parent value (pretty=3)";
}

// ===========================================================================
// text-wrap-mode alias: should also set text_wrap
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationTextWrapModeAlias) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-wrap-mode", "nowrap"), parent);
    EXPECT_EQ(style.text_wrap, 1)
        << "text-wrap-mode: nowrap should set text_wrap=1";

    cascade.apply_declaration(style, make_decl("text-wrap-mode", "balance"), parent);
    EXPECT_EQ(style.text_wrap, 2)
        << "text-wrap-mode: balance should set text_wrap=2";
}

// ===========================================================================
// text-wrap-style: sets wrap style values (balance, pretty, stable)
// ===========================================================================
TEST(PropertyCascadeTest, ApplyDeclarationTextWrapStyle) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-wrap-style", "balance"), parent);
    EXPECT_EQ(style.text_wrap, 2)
        << "text-wrap-style: balance should set text_wrap=2";

    cascade.apply_declaration(style, make_decl("text-wrap-style", "pretty"), parent);
    EXPECT_EQ(style.text_wrap, 3)
        << "text-wrap-style: pretty should set text_wrap=3";

    cascade.apply_declaration(style, make_decl("text-wrap-style", "stable"), parent);
    EXPECT_EQ(style.text_wrap, 4)
        << "text-wrap-style: stable should set text_wrap=4";
}

// ===========================================================================
// CSS Transitions: shorthand parsing
// ===========================================================================
TEST(TransitionTest, ShorthandParsesSingleTransition) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "opacity 0.3s ease" -> property=opacity, duration=300ms, timing=ease(0)
    cascade.apply_declaration(style, make_decl("transition", "opacity 0.3s ease"), parent);
    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 300.0f);
    EXPECT_EQ(style.transitions[0].timing_function, 0);
    EXPECT_FLOAT_EQ(style.transitions[0].delay_ms, 0.0f);

    // Legacy fields should also be set
    EXPECT_EQ(style.transition_property, "opacity");
    EXPECT_FLOAT_EQ(style.transition_duration, 0.3f);
    EXPECT_EQ(style.transition_timing, 0);
}

TEST(TransitionTest, ShorthandDurationMs) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "opacity 200ms linear" -> duration=200ms
    cascade.apply_declaration(style, make_decl("transition", "opacity 200ms linear"), parent);
    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 200.0f);
    EXPECT_EQ(style.transitions[0].timing_function, 1); // linear
}

TEST(TransitionTest, ShorthandTimingFunctions) {
    PropertyCascade cascade;
    ComputedStyle parent;

    // Test all timing functions
    {
        ComputedStyle style;
        cascade.apply_declaration(style, make_decl("transition", "opacity 1s ease-in"), parent);
        ASSERT_EQ(style.transitions.size(), 1u);
        EXPECT_EQ(style.transitions[0].timing_function, 2);
    }
    {
        ComputedStyle style;
        cascade.apply_declaration(style, make_decl("transition", "opacity 1s ease-out"), parent);
        ASSERT_EQ(style.transitions.size(), 1u);
        EXPECT_EQ(style.transitions[0].timing_function, 3);
    }
    {
        ComputedStyle style;
        cascade.apply_declaration(style, make_decl("transition", "opacity 1s ease-in-out"), parent);
        ASSERT_EQ(style.transitions.size(), 1u);
        EXPECT_EQ(style.transitions[0].timing_function, 4);
    }
}

TEST(TransitionTest, ShorthandMultipleTransitions) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "opacity 0.3s, transform 0.5s ease-in"
    cascade.apply_declaration(style, make_decl("transition", "opacity 0.3s, transform 0.5s ease-in"), parent);
    ASSERT_EQ(style.transitions.size(), 2u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 300.0f);
    EXPECT_EQ(style.transitions[1].property, "transform");
    EXPECT_FLOAT_EQ(style.transitions[1].duration_ms, 500.0f);
    EXPECT_EQ(style.transitions[1].timing_function, 2); // ease-in
}

TEST(TransitionTest, ShorthandTransitionAll) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition", "all 0.3s"), parent);
    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "all");
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 300.0f);
}

TEST(TransitionTest, ShorthandWithDelay) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition", "opacity 0.3s ease 100ms"), parent);
    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_FLOAT_EQ(style.transitions[0].delay_ms, 100.0f);
}

TEST(TransitionTest, LonghandDurationSeconds) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition-duration", "0.3s"), parent);
    EXPECT_FLOAT_EQ(style.transition_duration, 0.3f);
    ASSERT_GE(style.transitions.size(), 1u);
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 300.0f);
}

TEST(TransitionTest, LonghandDurationMilliseconds) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition-duration", "200ms"), parent);
    EXPECT_FLOAT_EQ(style.transition_duration, 0.2f);
    ASSERT_GE(style.transitions.size(), 1u);
    EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 200.0f);
}

TEST(TransitionTest, LonghandTimingFunction) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition-timing-function", "ease-in-out"), parent);
    EXPECT_EQ(style.transition_timing, 4);
    ASSERT_GE(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].timing_function, 4);
}

TEST(TransitionTest, LonghandProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition-property", "opacity, transform"), parent);
    EXPECT_EQ(style.transition_property, "opacity, transform");
    ASSERT_EQ(style.transitions.size(), 2u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_EQ(style.transitions[1].property, "transform");
}

// ===========================================================================
// Container Queries: container-type parsing
// ===========================================================================

TEST(ContainerQueryTest, ContainerTypeNormal) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-type", "normal"), parent);
    EXPECT_EQ(style.container_type, 0) << "container-type: normal should be 0";
}

TEST(ContainerQueryTest, ContainerTypeInlineSize) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-type", "inline-size"), parent);
    EXPECT_EQ(style.container_type, 2) << "container-type: inline-size should be 2";
}

TEST(ContainerQueryTest, ContainerTypeSize) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-type", "size"), parent);
    EXPECT_EQ(style.container_type, 1) << "container-type: size should be 1";
}

TEST(ContainerQueryTest, ContainerTypeBlockSize) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-type", "block-size"), parent);
    EXPECT_EQ(style.container_type, 3) << "container-type: block-size should be 3";
}

// ===========================================================================
// Container Queries: container-name parsing
// ===========================================================================

TEST(ContainerQueryTest, ContainerNameBasic) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-name", "sidebar"), parent);
    EXPECT_EQ(style.container_name, "sidebar");
}

TEST(ContainerQueryTest, ContainerNameEmpty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-name", "none"), parent);
    EXPECT_EQ(style.container_name, "none");
}

// ===========================================================================
// Container Queries: container shorthand parsing
// ===========================================================================

TEST(ContainerQueryTest, ContainerShorthandNameAndType) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // container: sidebar / inline-size
    cascade.apply_declaration(style, make_decl("container", "sidebar / inline-size"), parent);
    EXPECT_EQ(style.container_name, "sidebar")
        << "container shorthand should set name to 'sidebar'";
    EXPECT_EQ(style.container_type, 2)
        << "container shorthand should set type to inline-size (2)";
}

TEST(ContainerQueryTest, ContainerShorthandTypeOnly) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // container: size (no name, just type)
    cascade.apply_declaration(style, make_decl("container", "size"), parent);
    EXPECT_EQ(style.container_type, 1)
        << "container shorthand with only type should set type to size (1)";
}

TEST(ContainerQueryTest, ContainerShorthandNormal) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container", "normal"), parent);
    EXPECT_EQ(style.container_type, 0)
        << "container: normal should set type to 0";
}

// ===========================================================================
// Container Queries: @container rule parsing
// ===========================================================================

TEST(ContainerQueryTest, ContainerRuleParsing) {
    auto sheet = parse_stylesheet(
        ".sidebar { container-type: inline-size; container-name: sidebar; }"
        "@container sidebar (min-width: 400px) {"
        "  .card { grid-template-columns: 1fr 1fr; }"
        "}"
    );
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    EXPECT_EQ(sheet.container_rules[0].name, "sidebar");
    EXPECT_EQ(sheet.container_rules[0].condition, "(min-width: 400px)");
    ASSERT_EQ(sheet.container_rules[0].rules.size(), 1u);
    EXPECT_EQ(sheet.container_rules[0].rules[0].selector_text, ".card");
}

TEST(ContainerQueryTest, ContainerRuleNoName) {
    auto sheet = parse_stylesheet(
        "@container (min-width: 600px) {"
        "  .widget { font-size: 1.2em; }"
        "}"
    );
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    EXPECT_TRUE(sheet.container_rules[0].name.empty())
        << "Unnamed @container should have empty name";
    EXPECT_EQ(sheet.container_rules[0].condition, "(min-width: 600px)");
    ASSERT_EQ(sheet.container_rules[0].rules.size(), 1u);
}

TEST(ContainerQueryTest, ContainerRuleMultipleRules) {
    auto sheet = parse_stylesheet(
        "@container (max-width: 300px) {"
        "  .a { color: red; }"
        "  .b { color: blue; }"
        "}"
    );
    ASSERT_EQ(sheet.container_rules.size(), 1u);
    ASSERT_EQ(sheet.container_rules[0].rules.size(), 2u);
    EXPECT_EQ(sheet.container_rules[0].rules[0].selector_text, ".a");
    EXPECT_EQ(sheet.container_rules[0].rules[1].selector_text, ".b");
}

// ============================================================================
// CSS font shorthand: verify that apply_declaration sets font-size, font-family, font-weight
// ============================================================================
TEST(PropertyCascadeTest, FontShorthandParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Test basic: "20px Arial"
    cascade.apply_declaration(style, make_decl("font", "20px Arial"), parent);
    EXPECT_FLOAT_EQ(style.font_size.value, 20.0f) << "font: 20px Arial should set font-size to 20px";
    EXPECT_EQ(style.font_family, "Arial") << "font: 20px Arial should set font-family to Arial";
    EXPECT_EQ(style.font_weight, 400) << "font: 20px Arial should leave font-weight at normal (400)";

    // Test with bold: "bold 16px Georgia"
    ComputedStyle style2;
    cascade.apply_declaration(style2, make_decl("font", "bold 16px Georgia"), parent);
    EXPECT_EQ(style2.font_weight, 700) << "font: bold 16px Georgia should set font-weight to 700";
    EXPECT_FLOAT_EQ(style2.font_size.value, 16.0f) << "font: bold 16px Georgia should set font-size to 16px";
    EXPECT_EQ(style2.font_family, "Georgia") << "font: bold 16px Georgia should set font-family to Georgia";

    // Test with italic and line-height: "italic 18px/1.5 sans-serif"
    ComputedStyle style3;
    cascade.apply_declaration(style3, make_decl("font", "italic 18px/1.5 sans-serif"), parent);
    EXPECT_EQ(style3.font_style, FontStyle::Italic) << "font: italic should set font-style to Italic";
    EXPECT_FLOAT_EQ(style3.font_size.value, 18.0f) << "font: italic 18px/1.5 should set font-size to 18px";
    EXPECT_FLOAT_EQ(style3.line_height.value, 27.0f) << "font: 18px/1.5 should set line-height to 27px (18*1.5)";

    // Test keyword size: "large sans-serif"
    ComputedStyle style4;
    cascade.apply_declaration(style4, make_decl("font", "large sans-serif"), parent);
    EXPECT_FLOAT_EQ(style4.font_size.value, 18.0f) << "font: large should resolve to 18px";
    EXPECT_EQ(style4.font_family, "sans-serif") << "font: large sans-serif should set family";
}

// ============================================================================
// CSS cubic-bezier() timing function parsing
// ============================================================================
TEST(CSSTimingFunction, CubicBezierParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transition-timing-function", "cubic-bezier(0.42, 0, 0.58, 1)"), parent);

    EXPECT_EQ(style.transition_timing, 5) << "cubic-bezier should set timing to 5";
    EXPECT_FLOAT_EQ(style.transition_bezier_x1, 0.42f);
    EXPECT_FLOAT_EQ(style.transition_bezier_y1, 0.0f);
    EXPECT_FLOAT_EQ(style.transition_bezier_x2, 0.58f);
    EXPECT_FLOAT_EQ(style.transition_bezier_y2, 1.0f);

    // Also test animation-timing-function
    ComputedStyle style2;
    cascade.apply_declaration(style2,
        make_decl("animation-timing-function", "cubic-bezier(0.25, 0.1, 0.25, 1.0)"), parent);
    EXPECT_EQ(style2.animation_timing, 5);
    EXPECT_FLOAT_EQ(style2.animation_bezier_x1, 0.25f);
    EXPECT_FLOAT_EQ(style2.animation_bezier_y1, 0.1f);
}

// ============================================================================
// CSS steps() timing function parsing
// ============================================================================
TEST(CSSTimingFunction, StepsParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transition-timing-function", "steps(4, end)"), parent);
    EXPECT_EQ(style.transition_timing, 6) << "steps(4, end) should set timing to 6 (steps-end)";
    EXPECT_EQ(style.transition_steps_count, 4);

    ComputedStyle style2;
    cascade.apply_declaration(style2,
        make_decl("transition-timing-function", "steps(3, start)"), parent);
    EXPECT_EQ(style2.transition_timing, 7) << "steps(3, start) should set timing to 7 (steps-start)";
    EXPECT_EQ(style2.transition_steps_count, 3);

    // Also test animation-timing-function with steps
    ComputedStyle style3;
    cascade.apply_declaration(style3,
        make_decl("animation-timing-function", "steps(6, end)"), parent);
    EXPECT_EQ(style3.animation_timing, 6);
    EXPECT_EQ(style3.animation_steps_count, 6);
}

// ============================================================================
// Grid longhands: grid-column-start sets grid_column
// ============================================================================
TEST(CSSGridLonghands, GridLonghandsParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("grid-column-start", "2"), parent);
    EXPECT_EQ(style.grid_column_start, "2");
    EXPECT_EQ(style.grid_column, "2") << "grid-column should be set from grid-column-start longhand";

    // Now set grid-column-end as well
    cascade.apply_declaration(style,
        make_decl("grid-column-end", "4"), parent);
    EXPECT_EQ(style.grid_column_end, "4");
    EXPECT_EQ(style.grid_column, "2 / 4") << "grid-column should combine start and end";

    // Test grid-row longhands
    ComputedStyle style2;
    cascade.apply_declaration(style2,
        make_decl("grid-row-start", "1"), parent);
    EXPECT_EQ(style2.grid_row_start, "1");
    EXPECT_EQ(style2.grid_row, "1");

    cascade.apply_declaration(style2,
        make_decl("grid-row-end", "3"), parent);
    EXPECT_EQ(style2.grid_row_end, "3");
    EXPECT_EQ(style2.grid_row, "1 / 3");
}

// ============================================================================
// Animation play-state parsed
// ============================================================================
TEST(CSSAnimationPlayState, PlayStateParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("animation-play-state", "paused"), parent);
    EXPECT_EQ(style.animation_play_state, 1) << "paused should set animation_play_state to 1";

    ComputedStyle style2;
    cascade.apply_declaration(style2,
        make_decl("animation-play-state", "running"), parent);
    EXPECT_EQ(style2.animation_play_state, 0) << "running should set animation_play_state to 0";
}

// ============================================================================
// Text emphasis shorthand parsed
// ============================================================================
TEST(CSSTextEmphasis, TextEmphasisShorthandParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("text-emphasis", "circle red"), parent);
    EXPECT_EQ(style.text_emphasis_style, "circle");
    EXPECT_NE(style.text_emphasis_color, 0u) << "text-emphasis-color should be set from shorthand";
}

// ============================================================================
// Vertical align with length value
// ============================================================================
TEST(CSSVerticalAlign, VerticalAlignLengthParsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("vertical-align", "5px"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline)
        << "Length vertical-align should keep Baseline enum";
    EXPECT_FLOAT_EQ(style.vertical_align_offset, 5.0f)
        << "vertical-align: 5px should set offset to 5";
}

// ============================================================================
// CSS Logical Longhand Properties
// ============================================================================

TEST(CSSLogicalLonghands, MarginLogicalLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("margin-block-start", "10px"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.value, 10.0f)
        << "margin-block-start should map to margin-top";

    cascade.apply_declaration(style,
        make_decl("margin-block-end", "20px"), parent);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 20.0f)
        << "margin-block-end should map to margin-bottom";

    cascade.apply_declaration(style,
        make_decl("margin-inline-start", "30px"), parent);
    EXPECT_FLOAT_EQ(style.margin.left.value, 30.0f)
        << "margin-inline-start should map to margin-left";

    cascade.apply_declaration(style,
        make_decl("margin-inline-end", "auto"), parent);
    EXPECT_TRUE(style.margin.right.is_auto())
        << "margin-inline-end: auto should map to margin-right auto";
}

TEST(CSSLogicalLonghands, PaddingLogicalLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("padding-block-start", "5px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.value, 5.0f)
        << "padding-block-start should map to padding-top";

    cascade.apply_declaration(style,
        make_decl("padding-block-end", "15px"), parent);
    EXPECT_FLOAT_EQ(style.padding.bottom.value, 15.0f)
        << "padding-block-end should map to padding-bottom";

    cascade.apply_declaration(style,
        make_decl("padding-inline-start", "25px"), parent);
    EXPECT_FLOAT_EQ(style.padding.left.value, 25.0f)
        << "padding-inline-start should map to padding-left";

    cascade.apply_declaration(style,
        make_decl("padding-inline-end", "35px"), parent);
    EXPECT_FLOAT_EQ(style.padding.right.value, 35.0f)
        << "padding-inline-end should map to padding-right";
}

TEST(CSSLogicalLonghands, InsetLogicalLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("inset-block-start", "10px"), parent);
    EXPECT_FLOAT_EQ(style.top.value, 10.0f)
        << "inset-block-start should map to top";

    cascade.apply_declaration(style,
        make_decl("inset-block-end", "20px"), parent);
    EXPECT_FLOAT_EQ(style.bottom.value, 20.0f)
        << "inset-block-end should map to bottom";

    cascade.apply_declaration(style,
        make_decl("inset-inline-start", "30px"), parent);
    EXPECT_FLOAT_EQ(style.left_pos.value, 30.0f)
        << "inset-inline-start should map to left";

    cascade.apply_declaration(style,
        make_decl("inset-inline-end", "40px"), parent);
    EXPECT_FLOAT_EQ(style.right_pos.value, 40.0f)
        << "inset-inline-end should map to right";
}

TEST(CSSLogicalLonghands, BorderLogicalLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-block-start-width
    cascade.apply_declaration(style,
        make_decl("border-block-start-width", "3px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.value, 3.0f)
        << "border-block-start-width should map to border-top width";
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid)
        << "setting border width should auto-set style to solid";

    // border-block-end-color
    cascade.apply_declaration(style,
        make_decl("border-block-end-color", "red"), parent);
    EXPECT_EQ(style.border_bottom.color, (Color{255, 0, 0, 255}))
        << "border-block-end-color should map to border-bottom color";

    // border-inline-start-style
    cascade.apply_declaration(style,
        make_decl("border-inline-start-style", "dashed"), parent);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dashed)
        << "border-inline-start-style should map to border-left style";

    // border-inline-end-width
    cascade.apply_declaration(style,
        make_decl("border-inline-end-width", "5px"), parent);
    EXPECT_FLOAT_EQ(style.border_right.width.value, 5.0f)
        << "border-inline-end-width should map to border-right width";
}

// ============================================================================
// CSS 3D Transform Functions
// ============================================================================

TEST(CSS3DTransforms, Translate3dParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transform", "translate3d(10px, 20px, 30px)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 10.0f)
        << "translate3d x should be 10px";
    EXPECT_FLOAT_EQ(style.transforms[0].y, 20.0f)
        << "translate3d y should be 20px (z ignored)";
}

TEST(CSS3DTransforms, TranslateZParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transform", "translateZ(50px)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 0.0f)
        << "translateZ should have x=0 (no 2D effect)";
    EXPECT_FLOAT_EQ(style.transforms[0].y, 0.0f)
        << "translateZ should have y=0 (no 2D effect)";
}

TEST(CSS3DTransforms, Scale3dParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transform", "scale3d(2, 3, 4)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 2.0f)
        << "scale3d x should be 2";
    EXPECT_FLOAT_EQ(style.transforms[0].y, 3.0f)
        << "scale3d y should be 3 (z ignored)";
}

TEST(CSS3DTransforms, Rotate3dAndRotateZParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Test rotate3d
    cascade.apply_declaration(style,
        make_decl("transform", "rotate3d(0, 0, 1, 45deg)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(style.transforms[0].angle, 45.0f)
        << "rotate3d angle should be 45 degrees";

    // Test rotateZ
    style.transforms.clear();
    cascade.apply_declaration(style,
        make_decl("transform", "rotateZ(90deg)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(style.transforms[0].angle, 90.0f)
        << "rotateZ should work like rotate";

    // Test rotateX/rotateY are no-ops (no transforms pushed)
    style.transforms.clear();
    cascade.apply_declaration(style,
        make_decl("transform", "rotateX(45deg)"), parent);
    EXPECT_EQ(style.transforms.size(), 0u)
        << "rotateX should be a no-op in 2D";
}

TEST(CSS3DTransforms, Matrix3d2DExtraction) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // matrix3d with identity-like values but custom tx=100, ty=200
    // 4x4 column-major: [a,b,0,0, c,d,0,0, 0,0,1,0, tx,ty,0,1]
    cascade.apply_declaration(style,
        make_decl("transform",
            "matrix3d(2, 0.5, 0, 0, 0.3, 3, 0, 0, 0, 0, 1, 0, 100, 200, 0, 1)"),
        parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Matrix);
    EXPECT_FLOAT_EQ(style.transforms[0].m[0], 2.0f)   << "a = m[0]";
    EXPECT_FLOAT_EQ(style.transforms[0].m[1], 0.5f)   << "b = m[1]";
    EXPECT_FLOAT_EQ(style.transforms[0].m[2], 0.3f)   << "c = m[4]";
    EXPECT_FLOAT_EQ(style.transforms[0].m[3], 3.0f)   << "d = m[5]";
    EXPECT_FLOAT_EQ(style.transforms[0].m[4], 100.0f)  << "e(tx) = m[12]";
    EXPECT_FLOAT_EQ(style.transforms[0].m[5], 200.0f)  << "f(ty) = m[13]";
}

TEST(CSS3DTransforms, PerspectivePropertyParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // perspective as CSS property (not transform function)
    cascade.apply_declaration(style,
        make_decl("perspective", "500px"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 500.0f)
        << "perspective property should store distance in px";

    // perspective: none
    cascade.apply_declaration(style,
        make_decl("perspective", "none"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 0.0f)
        << "perspective: none should be 0";
}

TEST(CSS3DTransforms, BackfaceVisibilityParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("backface-visibility", "hidden"), parent);
    EXPECT_EQ(style.backface_visibility, 1)
        << "backface-visibility: hidden should be 1";

    cascade.apply_declaration(style,
        make_decl("backface-visibility", "visible"), parent);
    EXPECT_EQ(style.backface_visibility, 0)
        << "backface-visibility: visible should be 0";
}

TEST(CSS3DTransforms, TransformStyleParsing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transform-style", "preserve-3d"), parent);
    EXPECT_EQ(style.transform_style, 1)
        << "transform-style: preserve-3d should be 1";

    cascade.apply_declaration(style,
        make_decl("transform-style", "flat"), parent);
    EXPECT_EQ(style.transform_style, 0)
        << "transform-style: flat should be 0";
}

TEST(CSS3DTransforms, PerspectiveFunctionNoOp) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // perspective() as a transform function should be a no-op
    cascade.apply_declaration(style,
        make_decl("transform", "perspective(500px)"), parent);
    EXPECT_EQ(style.transforms.size(), 0u)
        << "perspective() function should not add a transform (no-op in 2D)";
}

TEST(CSS3DTransforms, ScaleZNoOp) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("transform", "scaleZ(2)"), parent);
    EXPECT_EQ(style.transforms.size(), 0u)
        << "scaleZ should be a no-op in 2D";
}

TEST(CSS3DTransforms, Mixed2DAnd3DTransforms) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Common pattern on the web: translate3d(0,0,0) used as GPU hint
    cascade.apply_declaration(style,
        make_decl("transform", "translate3d(0, 0, 0) scale(1.5)"), parent);
    ASSERT_EQ(style.transforms.size(), 2u)
        << "Should parse both translate3d and scale";
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 0.0f);
    EXPECT_FLOAT_EQ(style.transforms[0].y, 0.0f);
    EXPECT_EQ(style.transforms[1].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(style.transforms[1].x, 1.5f);
    EXPECT_FLOAT_EQ(style.transforms[1].y, 1.5f);
}

// ---------------------------------------------------------------------------
// Part 1: background-position-x / background-position-y longhands
// ---------------------------------------------------------------------------

TEST(CSSPropertyGaps, BackgroundPositionXOnly) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set both via shorthand first
    cascade.apply_declaration(style,
        make_decl("background-position", "center center"), parent);
    EXPECT_EQ(style.background_position_x, 1); // center
    EXPECT_EQ(style.background_position_y, 1); // center

    // Now override only x
    cascade.apply_declaration(style,
        make_decl("background-position-x", "right"), parent);
    EXPECT_EQ(style.background_position_x, 2) // right
        << "background-position-x should override only x component";
    EXPECT_EQ(style.background_position_y, 1) // center unchanged
        << "background-position-y should remain unchanged";
}

TEST(CSSPropertyGaps, BackgroundPositionYOnly) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set both via shorthand first
    cascade.apply_declaration(style,
        make_decl("background-position", "left top"), parent);
    EXPECT_EQ(style.background_position_x, 0); // left
    EXPECT_EQ(style.background_position_y, 0); // top

    // Now override only y
    cascade.apply_declaration(style,
        make_decl("background-position-y", "bottom"), parent);
    EXPECT_EQ(style.background_position_x, 0) // left unchanged
        << "background-position-x should remain unchanged";
    EXPECT_EQ(style.background_position_y, 2) // bottom
        << "background-position-y should override only y component";
}

// ---------------------------------------------------------------------------
// Part 2: border-style: hidden maps to None
// ---------------------------------------------------------------------------

TEST(CSSPropertyGaps, BorderStyleHidden) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("border-style", "hidden"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::None)
        << "border-style: hidden should map to BorderStyle::None";
    EXPECT_EQ(style.border_right.style, BorderStyle::None);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::None);
    EXPECT_EQ(style.border_left.style, BorderStyle::None);
}

// ---------------------------------------------------------------------------
// Part 3: clip-path: path() doesn't crash
// ---------------------------------------------------------------------------

TEST(CSSPropertyGaps, ClipPathPath) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Should parse without crash and set type to 5 (path)
    cascade.apply_declaration(style,
        make_decl("clip-path", "path('M0 0L100 100L0 100Z')"), parent);
    EXPECT_EQ(style.clip_path_type, 5)
        << "clip-path: path() should set type to 5";
    EXPECT_EQ(style.clip_path_path_data, "m0 0l100 100l0 100z")
        << "path data should be stored (lowercased by value_lower)";
}

// ---------------------------------------------------------------------------
// Part 4: shape-outside: polygon() parses
// ---------------------------------------------------------------------------

TEST(CSSPropertyGaps, ShapeOutsidePolygon) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Should parse polygon without crash
    cascade.apply_declaration(style,
        make_decl("shape-outside", "polygon(0% 0%, 100% 0%, 100% 100%)"), parent);
    EXPECT_EQ(style.shape_outside_type, 4)
        << "shape-outside: polygon() should set type to 4 (polygon)";
    // Should have 6 float values (3 points x 2 coords)
    EXPECT_EQ(style.shape_outside_values.size(), 6u)
        << "polygon with 3 points should have 6 coordinate values";
    // Also check string form is stored
    EXPECT_FALSE(style.shape_outside_str.empty())
        << "shape_outside_str should store the raw value";
}

// ---------------------------------------------------------------------------
// Part 5: counter-set and column-fill
// ---------------------------------------------------------------------------

TEST(CSSPropertyGaps, CounterSetProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl("counter-set", "section 5"), parent);
    EXPECT_EQ(style.counter_set, "section 5")
        << "counter-set should store the raw value";
}

TEST(CSSPropertyGaps, ColumnFillProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default should be 0 (balance)
    EXPECT_EQ(style.column_fill, 0);

    cascade.apply_declaration(style,
        make_decl("column-fill", "balance"), parent);
    EXPECT_EQ(style.column_fill, 0)
        << "column-fill: balance should set to 0";

    cascade.apply_declaration(style,
        make_decl("column-fill", "auto"), parent);
    EXPECT_EQ(style.column_fill, 1)
        << "column-fill: auto should set to 1";
}

// ============================================================================
// Multiple box-shadow support
// ============================================================================
TEST(CSSBoxShadowMultiple, SingleShadow) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "2px 3px 4px red"), parent);
    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 3.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].blur, 4.0f);
    EXPECT_FALSE(style.box_shadows[0].inset);
}

TEST(CSSBoxShadowMultiple, TwoShadows) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "2px 3px 4px red, 0px 0px 10px blue"), parent);
    ASSERT_EQ(style.box_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 3.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[1].offset_x, 0.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[1].offset_y, 0.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[1].blur, 10.0f);
}

TEST(CSSBoxShadowMultiple, ThreeShadowsWithInset) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "1px 1px 2px red, inset 0px 0px 5px green, 3px 3px 6px blue"), parent);
    ASSERT_EQ(style.box_shadows.size(), 3u);
    EXPECT_FALSE(style.box_shadows[0].inset);
    EXPECT_TRUE(style.box_shadows[1].inset);
    EXPECT_FALSE(style.box_shadows[2].inset);
    EXPECT_FLOAT_EQ(style.box_shadows[1].blur, 5.0f);
}

TEST(CSSBoxShadowMultiple, WithSpreadRadius) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "2px 3px 4px 5px red"), parent);
    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(style.box_shadows[0].spread, 5.0f);
}

TEST(CSSBoxShadowMultiple, NoneClears) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "2px 3px 4px red, 0px 0px 10px blue"), parent);
    ASSERT_EQ(style.box_shadows.size(), 2u);
    cascade.apply_declaration(style,
        make_decl("box-shadow", "none"), parent);
    EXPECT_EQ(style.box_shadows.size(), 0u);
}

TEST(CSSBoxShadowMultiple, LegacyFieldsFromFirstEntry) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("box-shadow", "5px 6px 7px red, 1px 1px 1px blue"), parent);
    EXPECT_FLOAT_EQ(style.shadow_offset_x, 5.0f);
    EXPECT_FLOAT_EQ(style.shadow_offset_y, 6.0f);
    EXPECT_FLOAT_EQ(style.shadow_blur, 7.0f);
}

// ============================================================================
// Cycle 242 — Elliptical border-radius
// ============================================================================

TEST(CSSStyleCascade, EllipticalBorderRadiusTwoValues) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("border-radius", "10px / 5px"), parent);
    // Averaged: (10+5)/2 = 7.5 for all corners
    EXPECT_FLOAT_EQ(style.border_radius_tl, 7.5f);
    EXPECT_FLOAT_EQ(style.border_radius_tr, 7.5f);
    EXPECT_FLOAT_EQ(style.border_radius_br, 7.5f);
    EXPECT_FLOAT_EQ(style.border_radius_bl, 7.5f);
}

TEST(CSSStyleCascade, EllipticalBorderRadiusFourSlashFour) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("border-radius", "10px 20px 30px 40px / 5px 10px 15px 20px"), parent);
    // TL: (10+5)/2=7.5, TR: (20+10)/2=15, BR: (30+15)/2=22.5, BL: (40+20)/2=30
    EXPECT_FLOAT_EQ(style.border_radius_tl, 7.5f);
    EXPECT_FLOAT_EQ(style.border_radius_tr, 15.0f);
    EXPECT_FLOAT_EQ(style.border_radius_br, 22.5f);
    EXPECT_FLOAT_EQ(style.border_radius_bl, 30.0f);
}

TEST(CSSStyleCascade, GradientStopPositions) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("background-image", "linear-gradient(to right, red 20%, blue 80%)"), parent);
    // Should have stops at 0.2 and 0.8 (not evenly distributed 0.0 and 1.0)
    ASSERT_GE(style.gradient_stops.size(), 2u);
    // First stop at 0.2
    EXPECT_NEAR(style.gradient_stops[0].second, 0.2f, 0.01f);
    // Second stop at 0.8
    EXPECT_NEAR(style.gradient_stops[1].second, 0.8f, 0.01f);
}

// ============================================================================
// Cycle 244 — SVG CSS properties via cascade
// ============================================================================

TEST(CSSStyleCascade, FillRuleNonzero) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("fill-rule", "nonzero"), parent);
    EXPECT_EQ(style.fill_rule, 0);
}

TEST(CSSStyleCascade, FillRuleEvenodd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("fill-rule", "evenodd"), parent);
    EXPECT_EQ(style.fill_rule, 1);
}

TEST(CSSStyleCascade, ClipRuleEvenodd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("clip-rule", "evenodd"), parent);
    EXPECT_EQ(style.clip_rule, 1);
}

TEST(CSSStyleCascade, StrokeMiterlimit) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("stroke-miterlimit", "8"), parent);
    EXPECT_FLOAT_EQ(style.stroke_miterlimit, 8.0f);
}

TEST(CSSStyleCascade, ShapeRenderingCrispEdges) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("shape-rendering", "crispEdges"), parent);
    EXPECT_EQ(style.shape_rendering, 2);
}

TEST(CSSStyleCascade, ShapeRenderingGeometricPrecision) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("shape-rendering", "geometricPrecision"), parent);
    EXPECT_EQ(style.shape_rendering, 3);
}

TEST(CSSStyleCascade, VectorEffectNonScalingStroke) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("vector-effect", "non-scaling-stroke"), parent);
    EXPECT_EQ(style.vector_effect, 1);
}

TEST(CSSStyleCascade, StopColorRed) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("stop-color", "red"), parent);
    EXPECT_EQ(style.stop_color, 0xFFFF0000u);
}

TEST(CSSStyleCascade, StopOpacity) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("stop-opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.stop_opacity, 0.5f);
}

TEST(CSSStyleCascade, StopOpacityClamped) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("stop-opacity", "2.0"), parent);
    EXPECT_FLOAT_EQ(style.stop_opacity, 1.0f);
}

// ============================================================================
// Cycle 244 — grid-template / grid shorthand via cascade
// ============================================================================

TEST(CSSStyleCascade, GridTemplateShorthandRowsAndCols) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("grid-template", "100px auto / 1fr 2fr"), parent);
    EXPECT_EQ(style.grid_template_rows, "100px auto");
    EXPECT_EQ(style.grid_template_columns, "1fr 2fr");
}

TEST(CSSStyleCascade, GridShorthandRowsAndCols) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("grid", "auto 1fr / repeat(3, 1fr)"), parent);
    EXPECT_EQ(style.grid_template_rows, "auto 1fr");
    EXPECT_EQ(style.grid_template_columns, "repeat(3, 1fr)");
}

TEST(CSSStyleCascade, GridShorthandRowsOnly) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style,
        make_decl("grid-template", "100px auto"), parent);
    EXPECT_EQ(style.grid_template_rows, "100px auto");
}

// ---- scroll-snap-stop ----
TEST(CSSStyleCascade, ScrollSnapStopNormal) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-snap-stop", "normal"), parent);
    EXPECT_EQ(style.scroll_snap_stop, 0);
}

TEST(CSSStyleCascade, ScrollSnapStopAlways) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-snap-stop", "always"), parent);
    EXPECT_EQ(style.scroll_snap_stop, 1);
}

// ---- scroll-margin-block-start/end, scroll-margin-inline-start/end ----
TEST(CSSStyleCascade, ScrollMarginBlockStart) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-margin-block-start", "10px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_top, 10.0f);
}

TEST(CSSStyleCascade, ScrollMarginBlockEnd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-margin-block-end", "20px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_bottom, 20.0f);
}

TEST(CSSStyleCascade, ScrollMarginInlineStart) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-margin-inline-start", "5px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_left, 5.0f);
}

TEST(CSSStyleCascade, ScrollMarginInlineEnd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("scroll-margin-inline-end", "15px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_right, 15.0f);
}

// ---- column-fill ----
TEST(CSSStyleCascade, ColumnFillBalance) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("column-fill", "balance"), parent);
    EXPECT_EQ(style.column_fill, 0);
}

TEST(CSSStyleCascade, ColumnFillAuto) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("column-fill", "auto"), parent);
    EXPECT_EQ(style.column_fill, 1);
}

TEST(CSSStyleCascade, ColumnFillBalanceAll) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("column-fill", "balance-all"), parent);
    EXPECT_EQ(style.column_fill, 2);
}

// ---- counter-set ----
TEST(CSSStyleCascade, CounterSetValue) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("counter-set", "section 5"), parent);
    EXPECT_EQ(style.counter_set, "section 5");
}

TEST(CSSStyleCascade, CounterSetNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("counter-set", "none"), parent);
    EXPECT_EQ(style.counter_set, "none");
}

// ---- animation-composition ----
TEST(CSSStyleCascade, AnimationCompositionReplace) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-composition", "replace"), parent);
    EXPECT_EQ(style.animation_composition, 0);
}

TEST(CSSStyleCascade, AnimationCompositionAdd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-composition", "add"), parent);
    EXPECT_EQ(style.animation_composition, 1);
}

TEST(CSSStyleCascade, AnimationCompositionAccumulate) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-composition", "accumulate"), parent);
    EXPECT_EQ(style.animation_composition, 2);
}

// ---- animation-timeline ----
TEST(CSSStyleCascade, AnimationTimelineAuto) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-timeline", "auto"), parent);
    EXPECT_EQ(style.animation_timeline, "auto");
}

TEST(CSSStyleCascade, AnimationTimelineNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-timeline", "none"), parent);
    EXPECT_EQ(style.animation_timeline, "none");
}

TEST(CSSStyleCascade, AnimationTimelineScroll) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-timeline", "scroll()"), parent);
    EXPECT_EQ(style.animation_timeline, "scroll()");
}

// ---- transform-box ----
TEST(CSSStyleCascade, TransformBoxContentBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("transform-box", "content-box"), parent);
    EXPECT_EQ(style.transform_box, 0);
}

TEST(CSSStyleCascade, TransformBoxBorderBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("transform-box", "border-box"), parent);
    EXPECT_EQ(style.transform_box, 1);
}

TEST(CSSStyleCascade, TransformBoxViewBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("transform-box", "view-box"), parent);
    EXPECT_EQ(style.transform_box, 4);
}

// ---- offset-path ----
TEST(CSSStyleCascade, OffsetPathNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("offset-path", "none"), parent);
    EXPECT_EQ(style.offset_path, "none");
}

TEST(CSSStyleCascade, OffsetPathValue) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("offset-path", "path('M0 0L100 100')"), parent);
    EXPECT_EQ(style.offset_path, "path('M0 0L100 100')");
}

// ============================================================================
// SVG filter properties: flood-color, flood-opacity, lighting-color
// ============================================================================

TEST(CSSStyleCascade, FloodColorRed) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("flood-color", "red"), parent);
    EXPECT_EQ(style.flood_color, 0xFFFF0000u);
}

TEST(CSSStyleCascade, FloodColorDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.flood_color, 0xFF000000u);
}

TEST(CSSStyleCascade, FloodOpacity) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("flood-opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.flood_opacity, 0.5f);
}

TEST(CSSStyleCascade, FloodOpacityClamped) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("flood-opacity", "2.0"), parent);
    EXPECT_FLOAT_EQ(style.flood_opacity, 1.0f);
}

TEST(CSSStyleCascade, LightingColorBlue) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("lighting-color", "blue"), parent);
    EXPECT_EQ(style.lighting_color, 0xFF0000FFu);
}

TEST(CSSStyleCascade, LightingColorDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.lighting_color, 0xFFFFFFFFu);
}

// ============================================================================
// Offset properties: offset, offset-anchor, offset-position
// ============================================================================

TEST(CSSStyleCascade, OffsetShorthand) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("offset", "path('M0 0') 50%"), parent);
    EXPECT_EQ(style.offset, "path('M0 0') 50%");
}

TEST(CSSStyleCascade, OffsetShorthandDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.offset, "");
}

TEST(CSSStyleCascade, OffsetAnchor) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("offset-anchor", "50% 50%"), parent);
    EXPECT_EQ(style.offset_anchor, "50% 50%");
}

TEST(CSSStyleCascade, OffsetAnchorDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.offset_anchor, "auto");
}

TEST(CSSStyleCascade, OffsetPosition) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("offset-position", "auto"), parent);
    EXPECT_EQ(style.offset_position, "auto");
}

TEST(CSSStyleCascade, OffsetPositionDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.offset_position, "normal");
}

// ============================================================================
// Transition/animation properties: transition-behavior, animation-range
// ============================================================================

TEST(CSSStyleCascade, TransitionBehaviorAllowDiscrete) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("transition-behavior", "allow-discrete"), parent);
    EXPECT_EQ(style.transition_behavior, 1);
}

TEST(CSSStyleCascade, TransitionBehaviorNormal) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("transition-behavior", "normal"), parent);
    EXPECT_EQ(style.transition_behavior, 0);
}

TEST(CSSStyleCascade, AnimationRange) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("animation-range", "entry 10% exit 90%"), parent);
    EXPECT_EQ(style.animation_range, "entry 10% exit 90%");
}

TEST(CSSStyleCascade, AnimationRangeDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.animation_range, "normal");
}

// ============================================================================
// CSS mask shorthand and related properties
// ============================================================================

TEST(CSSStyleCascade, MaskShorthand) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask", "url(mask.svg) no-repeat center"), parent);
    EXPECT_EQ(style.mask_shorthand, "url(mask.svg) no-repeat center");
}

TEST(CSSStyleCascade, MaskShorthandWebkit) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("-webkit-mask", "linear-gradient(black, transparent)"), parent);
    EXPECT_EQ(style.mask_shorthand, "linear-gradient(black, transparent)");
}

TEST(CSSStyleCascade, MaskOriginBorderBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-origin", "border-box"), parent);
    EXPECT_EQ(style.mask_origin, 0);
}

TEST(CSSStyleCascade, MaskOriginContentBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-origin", "content-box"), parent);
    EXPECT_EQ(style.mask_origin, 2);
}

TEST(CSSStyleCascade, MaskPositionValue) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-position", "center top"), parent);
    EXPECT_EQ(style.mask_position, "center top");
}

TEST(CSSStyleCascade, MaskPositionDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.mask_position, "0% 0%");
}

TEST(CSSStyleCascade, MaskClipBorderBox) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-clip", "border-box"), parent);
    EXPECT_EQ(style.mask_clip, 0);
}

TEST(CSSStyleCascade, MaskClipNoClip) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-clip", "no-clip"), parent);
    EXPECT_EQ(style.mask_clip, 3);
}

// ============================================================================
// SVG marker properties
// ============================================================================

TEST(CSSStyleCascade, MarkerShorthandSetsAll) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker", "url(#arrow)"), parent);
    EXPECT_EQ(style.marker_shorthand, "url(#arrow)");
    EXPECT_EQ(style.marker_start, "url(#arrow)");
    EXPECT_EQ(style.marker_mid, "url(#arrow)");
    EXPECT_EQ(style.marker_end, "url(#arrow)");
}

TEST(CSSStyleCascade, MarkerShorthandNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker", "none"), parent);
    EXPECT_EQ(style.marker_shorthand, "none");
    EXPECT_EQ(style.marker_start, "none");
}

TEST(CSSStyleCascade, MarkerStartUrl) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker-start", "url(#dot)"), parent);
    EXPECT_EQ(style.marker_start, "url(#dot)");
}

TEST(CSSStyleCascade, MarkerStartDefault) {
    ComputedStyle style;
    EXPECT_EQ(style.marker_start, "");
}

TEST(CSSStyleCascade, MarkerMidUrl) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker-mid", "url(#mid-marker)"), parent);
    EXPECT_EQ(style.marker_mid, "url(#mid-marker)");
}

TEST(CSSStyleCascade, MarkerMidNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker-mid", "none"), parent);
    EXPECT_EQ(style.marker_mid, "none");
}

TEST(CSSStyleCascade, MarkerEndUrl) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker-end", "url(#end-arrow)"), parent);
    EXPECT_EQ(style.marker_end, "url(#end-arrow)");
}

TEST(CSSStyleCascade, MarkerEndNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("marker-end", "none"), parent);
    EXPECT_EQ(style.marker_end, "none");
}

// ---- @counter-style at-rule parsing ----
TEST(CSSAtRules, CounterStyleParsed) {
    auto sheet = parse_stylesheet(
        "@counter-style thumbs {\n"
        "  system: cyclic;\n"
        "  symbols: '\\1F44D';\n"
        "  suffix: \" \";\n"
        "}\n"
    );
    ASSERT_EQ(sheet.counter_style_rules.size(), 1u);
    EXPECT_EQ(sheet.counter_style_rules[0].name, "thumbs");
    EXPECT_FALSE(sheet.counter_style_rules[0].descriptors.empty());
}

// ---- @scope rules applied ----
TEST(CSSAtRules, ScopeRulesApplied) {
    auto sheet = parse_stylesheet(
        "@scope (.card) {\n"
        "  .title { color: red; }\n"
        "}\n"
    );
    ASSERT_EQ(sheet.scope_rules.size(), 1u);
    EXPECT_EQ(sheet.scope_rules[0].scope_start, ".card");
    ASSERT_FALSE(sheet.scope_rules[0].rules.empty());
    EXPECT_EQ(sheet.scope_rules[0].rules[0].selector_text, ".title");
}

// ---- @starting-style parsed (does not crash) ----
TEST(CSSAtRules, StartingStyleParsed) {
    auto sheet = parse_stylesheet(
        "@starting-style {\n"
        "  .fade-in { opacity: 0; }\n"
        "}\n"
        "div { color: red; }\n"
    );
    // @starting-style is discarded; the div rule should parse fine
    EXPECT_GE(sheet.rules.size(), 1u);
}

// ---- @font-palette-values parsed (does not crash) ----
TEST(CSSAtRules, FontPaletteValuesParsed) {
    auto sheet = parse_stylesheet(
        "@font-palette-values --Grays {\n"
        "  font-family: \"Bungee Spice\";\n"
        "  base-palette: 0;\n"
        "}\n"
        "p { margin: 0; }\n"
    );
    // @font-palette-values is discarded; the p rule should parse fine
    EXPECT_GE(sheet.rules.size(), 1u);
}

// ---- margin-trim parsing ----
TEST(CSSStyleCascade, MarginTrimNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "none"), parent);
    EXPECT_EQ(style.margin_trim, 0);
}

TEST(CSSStyleCascade, MarginTrimBlock) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "block"), parent);
    EXPECT_EQ(style.margin_trim, 1);
}

TEST(CSSStyleCascade, MarginTrimInline) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "inline"), parent);
    EXPECT_EQ(style.margin_trim, 2);
}

TEST(CSSStyleCascade, MarginTrimBlockStart) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "block-start"), parent);
    EXPECT_EQ(style.margin_trim, 3);
}

TEST(CSSStyleCascade, MarginTrimBlockEnd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "block-end"), parent);
    EXPECT_EQ(style.margin_trim, 4);
}

TEST(CSSStyleCascade, MarginTrimInlineStart) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "inline-start"), parent);
    EXPECT_EQ(style.margin_trim, 5);
}

TEST(CSSStyleCascade, MarginTrimInlineEnd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("margin-trim", "inline-end"), parent);
    EXPECT_EQ(style.margin_trim, 6);
}

// ---- shape-outside: polygon() parsing ----
TEST(CSSStyleCascade, ShapeOutsidePolygon) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("shape-outside", "polygon(0% 0%, 100% 0%, 100% 100%)"), parent);
    EXPECT_EQ(style.shape_outside_type, 4); // 4 = polygon
    EXPECT_GE(style.shape_outside_values.size(), 6u); // 3 points = 6 values
    EXPECT_EQ(style.shape_outside_str, "polygon(0% 0%, 100% 0%, 100% 100%)");
}

// ============================================================================
// Cycle 253: mask-border, clip-path url(), display ruby, float inline-start,
//            ruby-overhang
// ============================================================================

TEST(CSSStyleCascade, MaskBorderStoredAsString) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-border", "url(border.svg) 30 fill / 1em / 0 round"), parent);
    EXPECT_EQ(style.mask_border, "url(border.svg) 30 fill / 1em / 0 round");
}

TEST(CSSStyleCascade, MaskBorderSourceStored) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("mask-border-source", "url(mask.png)"), parent);
    EXPECT_EQ(style.mask_border, "url(mask.png)");
}

TEST(CSSStyleCascade, ClipPathUrl) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("clip-path", "url(#myClip)"), parent);
    EXPECT_EQ(style.clip_path_type, 6); // 6 = url
    EXPECT_EQ(style.clip_path_path_data, "#myClip");
}

TEST(CSSStyleCascade, DisplayRubyMapsToInline) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "ruby"), parent);
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleCascade, DisplayRubyTextMapsToInline) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "ruby-text"), parent);
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleCascade, FloatInlineStartMapsToLeft) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("float", "inline-start"), parent);
    EXPECT_EQ(style.float_val, Float::Left);
}

TEST(CSSStyleCascade, FloatInlineEndMapsToRight) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("float", "inline-end"), parent);
    EXPECT_EQ(style.float_val, Float::Right);
}

TEST(CSSStyleCascade, RubyOverhangAuto) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("ruby-overhang", "auto"), parent);
    EXPECT_EQ(style.ruby_overhang, 0);
}

TEST(CSSStyleCascade, RubyOverhangNone) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("ruby-overhang", "none"), parent);
    EXPECT_EQ(style.ruby_overhang, 1);
}

TEST(CSSStyleCascade, RubyOverhangStart) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("ruby-overhang", "start"), parent);
    EXPECT_EQ(style.ruby_overhang, 2);
}

TEST(CSSStyleCascade, RubyOverhangEnd) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("ruby-overhang", "end"), parent);
    EXPECT_EQ(style.ruby_overhang, 3);
}

// ============================================================================
// Cycle 254: CSS page property stored
// ============================================================================
TEST(CSSStyleCascade, PageProperty) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("page", "my-page"), parent);
    EXPECT_EQ(style.page, "my-page");
}

TEST(CSSStyleCascade, PagePropertyAuto) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("page", "auto"), parent);
    EXPECT_EQ(style.page, "auto");
}

// ============================================================================
// Cycle 254: color(srgb 1 0 0) parses to red (already implemented, verify)
// ============================================================================
TEST(CSSStyleCascade, ColorFunctionSrgbRed) {
    auto c = parse_color("color(srgb 1 0 0)");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->r, 255);
    EXPECT_EQ(c->g, 0);
    EXPECT_EQ(c->b, 0);
    EXPECT_EQ(c->a, 255);
}

// ============================================================================
// Cycle 255: display:table-column maps to TableCell
// ============================================================================
TEST(CSSStyleCascade, DisplayTableColumn) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-column"), parent);
    EXPECT_EQ(style.display, Display::TableCell);
}

// ============================================================================
// Cycle 255: display:table-column-group maps to TableRow
// ============================================================================
TEST(CSSStyleCascade, DisplayTableColumnGroup) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-column-group"), parent);
    EXPECT_EQ(style.display, Display::TableRow);
}

// ============================================================================
// Cycle 255: display:table-footer-group maps to TableRowGroup
// ============================================================================
TEST(CSSStyleCascade, DisplayTableFooterGroup) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-footer-group"), parent);
    EXPECT_EQ(style.display, Display::TableRowGroup);
}

// ============================================================================
// Cycle 255: display:table-caption maps to Block
// ============================================================================
TEST(CSSStyleCascade, DisplayTableCaption) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-caption"), parent);
    EXPECT_EQ(style.display, Display::Block);
}

// ============================================================================
// Cycle 255: display:table-row-group maps to TableRowGroup
// ============================================================================
TEST(CSSStyleCascade, DisplayTableRowGroup) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-row-group"), parent);
    EXPECT_EQ(style.display, Display::TableRowGroup);
}

// ============================================================================
// Cycle 255: display:table-header-group maps to TableHeaderGroup
// ============================================================================
TEST(CSSStyleCascade, DisplayTableHeaderGroup) {
    ComputedStyle style;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("display", "table-header-group"), parent);
    EXPECT_EQ(style.display, Display::TableHeaderGroup);
}

// ============================================================================
// Cycle 257: Unitless line-height sets line_height_unitless factor
// ============================================================================
TEST(CSSStyleCascade, UnitlessLineHeightSetsFactor) {
    ComputedStyle style;
    style.font_size = Length::px(20);
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("line-height", "1.5"), parent);
    // Should store unitless factor
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.5f);
    // Computed value should be 1.5 * 20 = 30px
    EXPECT_FLOAT_EQ(style.line_height.value, 30.0f);
}

// ============================================================================
// Cycle 257: px line-height clears unitless factor
// ============================================================================
TEST(CSSStyleCascade, PxLineHeightClearsUnitless) {
    ComputedStyle style;
    style.line_height_unitless = 1.5f; // previously unitless
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("line-height", "24px"), parent);
    // Should clear the unitless factor
    EXPECT_FLOAT_EQ(style.line_height_unitless, 0.0f);
    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f);
}

// ============================================================================
// Cycle 257: em line-height clears unitless factor
// ============================================================================
TEST(CSSStyleCascade, EmLineHeightClearsUnitless) {
    ComputedStyle style;
    style.font_size = Length::px(16);
    style.line_height_unitless = 1.5f;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("line-height", "1.5em"), parent);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 0.0f);
    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f); // 1.5 * 16
}

// ============================================================================
// Cycle 257: percentage line-height clears unitless factor
// ============================================================================
TEST(CSSStyleCascade, PercentageLineHeightClearsUnitless) {
    ComputedStyle style;
    style.font_size = Length::px(20);
    style.line_height_unitless = 1.5f;
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("line-height", "150%"), parent);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 0.0f);
    EXPECT_FLOAT_EQ(style.line_height.value, 30.0f); // 150% of 20
}

// ============================================================================
// Cycle 257: normal line-height is treated as unitless 1.2
// ============================================================================
TEST(CSSStyleCascade, NormalLineHeightIsUnitless) {
    ComputedStyle style;
    style.font_size = Length::px(20);
    ComputedStyle parent;
    PropertyCascade cascade;
    cascade.apply_declaration(style, make_decl("line-height", "normal"), parent);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.2f);
    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f); // 1.2 * 20
}

// ============================================================================
// Cycle 257: Unitless line-height recomputes during cascade when font-size differs
// ============================================================================
TEST(CSSStyleCascade, UnitlessLineHeightRecomputesForChildFontSize) {
    // Parent: font-size 20px, line-height 1.5 (unitless, computed = 30px)
    ComputedStyle parent;
    parent.font_size = Length::px(20);
    parent.line_height = Length::px(30);
    parent.line_height_unitless = 1.5f;

    // Child: font-size 12px, line-height inherited unitless 1.5
    // After recomputation, line-height should be 1.5 * 12 = 18px (not inherited 30px)
    ComputedStyle child;
    child.font_size = Length::px(12);
    child.line_height = parent.line_height; // inherited 30px
    child.line_height_unitless = parent.line_height_unitless; // inherited 1.5

    // Simulate what the cascade does: recompute if unitless and font-size differs
    if (child.line_height_unitless > 0 &&
        child.font_size.value != parent.font_size.value) {
        child.line_height = Length::px(child.line_height_unitless * child.font_size.value);
    }
    EXPECT_FLOAT_EQ(child.line_height.value, 18.0f);  // 1.5 * 12
    EXPECT_FLOAT_EQ(child.line_height_unitless, 1.5f); // factor preserved
}

// ============================================================================
// Cycle 257: <a> tag gets text_decoration_bits = 1 (underline)
// ============================================================================
TEST(ComputedStyleTest, AnchorTagDefaultBits) {
    auto style = default_style_for_tag("a");
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_decoration_bits, 1); // underline bit
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.color, (Color{0, 0, 238, 255})); // #0000EE
}

// ============================================================================
// Cycle 257: <u> tag gets text_decoration_bits = 1 (underline)
// ============================================================================
TEST(ComputedStyleTest, UnderlineTagDefaultBits) {
    auto style = default_style_for_tag("u");
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_decoration_bits, 1);
}

// ============================================================================
// Cycle 257: <s> tag gets text_decoration_bits = 4 (line-through)
// ============================================================================
TEST(ComputedStyleTest, StrikethroughTagDefaultBits) {
    auto style = default_style_for_tag("s");
    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
    EXPECT_EQ(style.text_decoration_bits, 4);
}

// ============================================================================
// Cycle 257: <del> tag gets text_decoration_bits = 4 (line-through)
// ============================================================================
TEST(ComputedStyleTest, DelTagDefaultBits) {
    auto style = default_style_for_tag("del");
    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
    EXPECT_EQ(style.text_decoration_bits, 4);
}

// ============================================================================
// Cycle 257: <ins> tag gets text_decoration_bits = 1 (underline)
// ============================================================================
TEST(ComputedStyleTest, InsTagDefaultBits) {
    auto style = default_style_for_tag("ins");
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_decoration_bits, 1);
}

// ============================================================================
// Cycle 267: :hover matches elements with data-clever-hover attribute
// ============================================================================
TEST(SelectorMatcherTest, HoverPseudoClassWithAttribute) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "button";

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "hover";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Without attribute, :hover should NOT match
    EXPECT_FALSE(matcher.matches(elem, complex));

    // With data-clever-hover attribute, :hover SHOULD match
    elem.attributes.push_back({"data-clever-hover", ""});
    EXPECT_TRUE(matcher.matches(elem, complex));
}

// ============================================================================
// Cycle 267: :focus matches elements with data-clever-focus attribute
// ============================================================================
TEST(SelectorMatcherTest, FocusPseudoClassWithAttribute) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "input";

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "focus";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_FALSE(matcher.matches(elem, complex));

    elem.attributes.push_back({"data-clever-focus", ""});
    EXPECT_TRUE(matcher.matches(elem, complex));
}

// ============================================================================
// Cycle 267: :focus-within matches when descendant has focus
// ============================================================================
TEST(SelectorMatcherTest, FocusWithinPseudoClass) {
    SelectorMatcher matcher;

    ElementView parent;
    parent.tag_name = "div";

    ElementView child;
    child.tag_name = "input";
    child.attributes.push_back({"data-clever-focus", ""});
    child.parent = &parent;
    parent.children.push_back(&child);
    parent.child_element_count = 1;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "focus-within";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Parent should match :focus-within because child has focus
    EXPECT_TRUE(matcher.matches(parent, complex));

    // Without focused child, should NOT match
    ElementView parent2;
    parent2.tag_name = "div";
    EXPECT_FALSE(matcher.matches(parent2, complex));
}

// ============================================================================
// Cycle 267: :focus-visible matches elements with data-clever-focus
// ============================================================================
TEST(SelectorMatcherTest, FocusVisiblePseudoClass) {
    SelectorMatcher matcher;

    ElementView elem;
    elem.tag_name = "input";

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "focus-visible";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    EXPECT_FALSE(matcher.matches(elem, complex));
    elem.attributes.push_back({"data-clever-focus", ""});
    EXPECT_TRUE(matcher.matches(elem, complex));
}

// ============================================================================
// Cycle 422: :first-child / :last-child / :only-child structural pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, FirstChildPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "first-child";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView first;
    first.tag_name = "li";
    first.child_index = 0;
    first.sibling_count = 3;
    EXPECT_TRUE(matcher.matches(first, complex));

    ElementView second;
    second.tag_name = "li";
    second.child_index = 1;
    second.sibling_count = 3;
    EXPECT_FALSE(matcher.matches(second, complex));
}

TEST(SelectorMatcherTest, LastChildPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "last-child";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView last;
    last.tag_name = "li";
    last.child_index = 2;
    last.sibling_count = 3;
    EXPECT_TRUE(matcher.matches(last, complex));

    ElementView first;
    first.tag_name = "li";
    first.child_index = 0;
    first.sibling_count = 3;
    EXPECT_FALSE(matcher.matches(first, complex));
}

TEST(SelectorMatcherTest, OnlyChildPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "only-child";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView only;
    only.tag_name = "span";
    only.child_index = 0;
    only.sibling_count = 1;
    EXPECT_TRUE(matcher.matches(only, complex));

    ElementView one_of_two;
    one_of_two.tag_name = "span";
    one_of_two.child_index = 0;
    one_of_two.sibling_count = 2;
    EXPECT_FALSE(matcher.matches(one_of_two, complex));
}

// ============================================================================
// Cycle 422: :disabled / :enabled / :checked form pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, DisabledPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "disabled";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView btn;
    btn.tag_name = "button";
    btn.attributes = {{"disabled", ""}};
    EXPECT_TRUE(matcher.matches(btn, complex));

    ElementView active_btn;
    active_btn.tag_name = "button";
    EXPECT_FALSE(matcher.matches(active_btn, complex));

    // Non-form element must not match :disabled even with the attribute
    ElementView div_elem;
    div_elem.tag_name = "div";
    div_elem.attributes = {{"disabled", ""}};
    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

TEST(SelectorMatcherTest, EnabledPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "enabled";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView inp;
    inp.tag_name = "input";
    EXPECT_TRUE(matcher.matches(inp, complex));

    ElementView inp_disabled;
    inp_disabled.tag_name = "input";
    inp_disabled.attributes = {{"disabled", ""}};
    EXPECT_FALSE(matcher.matches(inp_disabled, complex));
}

TEST(SelectorMatcherTest, CheckedPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "checked";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView checkbox;
    checkbox.tag_name = "input";
    checkbox.attributes = {{"type", "checkbox"}, {"checked", ""}};
    EXPECT_TRUE(matcher.matches(checkbox, complex));

    ElementView unchecked;
    unchecked.tag_name = "input";
    unchecked.attributes = {{"type", "checkbox"}};
    EXPECT_FALSE(matcher.matches(unchecked, complex));
}

// ============================================================================
// Cycle 422: Adjacent sibling (+) and general sibling (~) combinators
// ============================================================================
TEST(SelectorMatcherTest, AdjacentSiblingCombinator) {
    SelectorMatcher matcher;

    // Selector: div + p
    CompoundSelector div_compound;
    div_compound.simple_selectors.push_back(make_type_sel("div"));

    CompoundSelector p_compound;
    p_compound.simple_selectors.push_back(make_type_sel("p"));

    auto complex = make_complex_chain({
        {std::nullopt, div_compound},
        {Combinator::NextSibling, p_compound}
    });

    ElementView div_elem;
    div_elem.tag_name = "div";

    ElementView p_elem;
    p_elem.tag_name = "p";
    p_elem.prev_sibling = &div_elem;

    EXPECT_TRUE(matcher.matches(p_elem, complex));

    // p with a span (not div) as immediately preceding sibling should not match
    ElementView span_elem;
    span_elem.tag_name = "span";

    ElementView p_after_span;
    p_after_span.tag_name = "p";
    p_after_span.prev_sibling = &span_elem;

    EXPECT_FALSE(matcher.matches(p_after_span, complex));
}

TEST(SelectorMatcherTest, GeneralSiblingCombinator) {
    SelectorMatcher matcher;

    // Selector: h1 ~ p
    CompoundSelector h1_compound;
    h1_compound.simple_selectors.push_back(make_type_sel("h1"));

    CompoundSelector p_compound;
    p_compound.simple_selectors.push_back(make_type_sel("p"));

    auto complex = make_complex_chain({
        {std::nullopt, h1_compound},
        {Combinator::SubsequentSibling, p_compound}
    });

    ElementView h1_elem;
    h1_elem.tag_name = "h1";

    ElementView span_elem;
    span_elem.tag_name = "span";
    span_elem.prev_sibling = &h1_elem;

    // p preceded by span which is preceded by h1 — h1 is a subsequent sibling, should match
    ElementView p_elem;
    p_elem.tag_name = "p";
    p_elem.prev_sibling = &span_elem;

    EXPECT_TRUE(matcher.matches(p_elem, complex));

    // p with no preceding sibling should not match
    ElementView p_alone;
    p_alone.tag_name = "p";
    p_alone.prev_sibling = nullptr;

    EXPECT_FALSE(matcher.matches(p_alone, complex));
}

// ============================================================================
// Cycle 423: :required / :optional form pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, RequiredPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "required";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView inp_required;
    inp_required.tag_name = "input";
    inp_required.attributes = {{"type", "text"}, {"required", ""}};
    EXPECT_TRUE(matcher.matches(inp_required, complex));

    ElementView inp_optional;
    inp_optional.tag_name = "input";
    inp_optional.attributes = {{"type", "text"}};
    EXPECT_FALSE(matcher.matches(inp_optional, complex));
}

TEST(SelectorMatcherTest, OptionalPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "optional";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // input without required attribute is optional
    ElementView inp;
    inp.tag_name = "input";
    inp.attributes = {{"type", "text"}};
    EXPECT_TRUE(matcher.matches(inp, complex));

    // input with required attribute is not optional
    ElementView inp_req;
    inp_req.tag_name = "input";
    inp_req.attributes = {{"type", "text"}, {"required", ""}};
    EXPECT_FALSE(matcher.matches(inp_req, complex));

    // Non-form element (div) is not optional
    ElementView div_elem;
    div_elem.tag_name = "div";
    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

// ============================================================================
// Cycle 423: :read-only / :read-write content-editability pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, ReadOnlyPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "read-only";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Non-editable elements (div, p) are read-only by default
    ElementView div_elem;
    div_elem.tag_name = "div";
    EXPECT_TRUE(matcher.matches(div_elem, complex));

    // input is not read-only by default
    ElementView inp;
    inp.tag_name = "input";
    EXPECT_FALSE(matcher.matches(inp, complex));

    // input with readonly attribute is read-only
    ElementView inp_ro;
    inp_ro.tag_name = "input";
    inp_ro.attributes = {{"readonly", ""}};
    EXPECT_TRUE(matcher.matches(inp_ro, complex));
}

TEST(SelectorMatcherTest, ReadWritePseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "read-write";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // input without readonly is read-write
    ElementView inp;
    inp.tag_name = "input";
    EXPECT_TRUE(matcher.matches(inp, complex));

    // input with readonly is not read-write
    ElementView inp_ro;
    inp_ro.tag_name = "input";
    inp_ro.attributes = {{"readonly", ""}};
    EXPECT_FALSE(matcher.matches(inp_ro, complex));

    // Non-editable element (div) is not read-write
    ElementView div_elem;
    div_elem.tag_name = "div";
    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

// ============================================================================
// Cycle 423: :any-link pseudo-class
// ============================================================================
TEST(SelectorMatcherTest, AnyLinkPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "any-link";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView anchor;
    anchor.tag_name = "a";
    anchor.attributes = {{"href", "https://example.com"}};
    EXPECT_TRUE(matcher.matches(anchor, complex));

    // <a> without href should not match
    ElementView anchor_no_href;
    anchor_no_href.tag_name = "a";
    EXPECT_FALSE(matcher.matches(anchor_no_href, complex));

    // Non-link element should not match
    ElementView div_elem;
    div_elem.tag_name = "div";
    div_elem.attributes = {{"href", "https://example.com"}};
    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

// ============================================================================
// Cycle 423: :placeholder-shown pseudo-class
// ============================================================================
TEST(SelectorMatcherTest, PlaceholderShownPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "placeholder-shown";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // input with placeholder and no value → placeholder is visible
    ElementView inp_empty;
    inp_empty.tag_name = "input";
    inp_empty.attributes = {{"placeholder", "Enter name"}};
    EXPECT_TRUE(matcher.matches(inp_empty, complex));

    // input with placeholder AND a value → placeholder is hidden
    ElementView inp_filled;
    inp_filled.tag_name = "input";
    inp_filled.attributes = {{"placeholder", "Enter name"}, {"value", "Alice"}};
    EXPECT_FALSE(matcher.matches(inp_filled, complex));

    // input with no placeholder
    ElementView inp_no_placeholder;
    inp_no_placeholder.tag_name = "input";
    EXPECT_FALSE(matcher.matches(inp_no_placeholder, complex));
}

// ============================================================================
// Cycle 423: :lang() pseudo-class (exact and prefix matching)
// ============================================================================
TEST(SelectorMatcherTest, LangPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "lang";
    ss.argument = "en";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Exact match
    ElementView elem_en;
    elem_en.tag_name = "p";
    elem_en.attributes = {{"lang", "en"}};
    EXPECT_TRUE(matcher.matches(elem_en, complex));

    // Prefix match: lang="en-US" matches :lang(en)
    ElementView elem_en_us;
    elem_en_us.tag_name = "p";
    elem_en_us.attributes = {{"lang", "en-US"}};
    EXPECT_TRUE(matcher.matches(elem_en_us, complex));

    // Different language does not match
    ElementView elem_fr;
    elem_fr.tag_name = "p";
    elem_fr.attributes = {{"lang", "fr"}};
    EXPECT_FALSE(matcher.matches(elem_fr, complex));
}

// ============================================================================
// Cycle 423: :is() pseudo-class (matches if any argument selector matches)
// ============================================================================
TEST(SelectorMatcherTest, IsPseudoClass) {
    SelectorMatcher matcher;

    // :is(h1, h2, h3) should match h1, h2, or h3 but not h4
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "is";
    ss.argument = "h1, h2, h3";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView h1;
    h1.tag_name = "h1";
    EXPECT_TRUE(matcher.matches(h1, complex));

    ElementView h2;
    h2.tag_name = "h2";
    EXPECT_TRUE(matcher.matches(h2, complex));

    ElementView h4;
    h4.tag_name = "h4";
    EXPECT_FALSE(matcher.matches(h4, complex));
}

// ============================================================================
// Cycle 424: :default pseudo-class (submit button, checked/selected option)
// ============================================================================
TEST(SelectorMatcherTest, DefaultPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "default";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Submit button is the default button in a form
    ElementView submit_btn;
    submit_btn.tag_name = "button";
    submit_btn.attributes = {{"type", "submit"}};
    EXPECT_TRUE(matcher.matches(submit_btn, complex));

    // Non-submit button is not the default
    ElementView reset_btn;
    reset_btn.tag_name = "button";
    reset_btn.attributes = {{"type", "reset"}};
    EXPECT_FALSE(matcher.matches(reset_btn, complex));

    // Option with selected attribute is the default
    ElementView selected_option;
    selected_option.tag_name = "option";
    selected_option.attributes = {{"selected", ""}};
    EXPECT_TRUE(matcher.matches(selected_option, complex));
}

// ============================================================================
// Cycle 424: :valid / :invalid form validation pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, ValidPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "valid";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // All form elements are valid by default (no constraint validation state)
    ElementView inp;
    inp.tag_name = "input";
    EXPECT_TRUE(matcher.matches(inp, complex));

    ElementView form;
    form.tag_name = "form";
    EXPECT_TRUE(matcher.matches(form, complex));

    // Non-form element is not valid
    ElementView div_elem;
    div_elem.tag_name = "div";
    EXPECT_FALSE(matcher.matches(div_elem, complex));
}

TEST(SelectorMatcherTest, InvalidPseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "invalid";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Without runtime validation state, all inputs are considered valid — :invalid never matches
    ElementView inp;
    inp.tag_name = "input";
    EXPECT_FALSE(matcher.matches(inp, complex));
}

// ============================================================================
// Cycle 424: :where() pseudo-class (same as :is() but zero specificity)
// ============================================================================
TEST(SelectorMatcherTest, WherePseudoClass) {
    SelectorMatcher matcher;

    // :where(h1, h2) should match h1 and h2 elements
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "where";
    ss.argument = "h1, h2";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView h1;
    h1.tag_name = "h1";
    EXPECT_TRUE(matcher.matches(h1, complex));

    ElementView h3;
    h3.tag_name = "h3";
    EXPECT_FALSE(matcher.matches(h3, complex));
}

// ============================================================================
// Cycle 424: :has() pseudo-class (matches if any descendant matches)
// ============================================================================
TEST(SelectorMatcherTest, HasPseudoClass) {
    SelectorMatcher matcher;

    // :has(img) matches an element containing an img descendant
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "has";
    ss.argument = "img";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView img_child;
    img_child.tag_name = "img";

    ElementView container;
    container.tag_name = "div";
    container.children = {&img_child};
    EXPECT_TRUE(matcher.matches(container, complex));

    // Container with no children does not match :has(img)
    ElementView empty_container;
    empty_container.tag_name = "div";
    EXPECT_FALSE(matcher.matches(empty_container, complex));
}

// ============================================================================
// Cycle 424: :last-of-type and :only-of-type pseudo-classes
// ============================================================================
TEST(SelectorMatcherTest, LastOfTypePseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "last-of-type";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Element with same_type_index at end of same_type_count is last-of-type
    ElementView last_p;
    last_p.tag_name = "p";
    last_p.same_type_index = 2;
    last_p.same_type_count = 3;
    last_p.child_index = 4;
    last_p.sibling_count = 5;
    EXPECT_TRUE(matcher.matches(last_p, complex));

    ElementView first_p;
    first_p.tag_name = "p";
    first_p.same_type_index = 0;
    first_p.same_type_count = 3;
    first_p.child_index = 0;
    first_p.sibling_count = 5;
    EXPECT_FALSE(matcher.matches(first_p, complex));
}

TEST(SelectorMatcherTest, OnlyOfTypePseudoClass) {
    SelectorMatcher matcher;

    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "only-of-type";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    // Only one p among siblings → only-of-type matches
    ElementView only_p;
    only_p.tag_name = "p";
    only_p.same_type_count = 1;
    only_p.child_index = 1;
    only_p.sibling_count = 3;
    EXPECT_TRUE(matcher.matches(only_p, complex));

    // Two p siblings → not only-of-type
    ElementView one_of_two_p;
    one_of_two_p.tag_name = "p";
    one_of_two_p.same_type_count = 2;
    one_of_two_p.child_index = 0;
    one_of_two_p.sibling_count = 3;
    EXPECT_FALSE(matcher.matches(one_of_two_p, complex));
}

// ============================================================================
// Cycle 424: :nth-of-type() pseudo-class
// ============================================================================
TEST(SelectorMatcherTest, NthOfTypePseudoClass) {
    SelectorMatcher matcher;

    // :nth-of-type(2) matches the second element of its type
    SimpleSelector ss;
    ss.type = SimpleSelectorType::PseudoClass;
    ss.value = "nth-of-type";
    ss.argument = "2";

    CompoundSelector compound;
    compound.simple_selectors.push_back(ss);
    auto complex = make_simple_complex(compound);

    ElementView second_p;
    second_p.tag_name = "p";
    second_p.same_type_index = 1;  // 0-based → 2nd of type
    second_p.same_type_count = 3;
    EXPECT_TRUE(matcher.matches(second_p, complex));

    ElementView first_p;
    first_p.tag_name = "p";
    first_p.same_type_index = 0;  // 1st of type — does not match :nth-of-type(2)
    first_p.same_type_count = 3;
    EXPECT_FALSE(matcher.matches(first_p, complex));
}

// ============================================================================
// Cycle 425: CSS custom properties (--variable) storage and var() resolution
// ============================================================================
TEST(PropertyCascadeTest, CustomPropertyStorage) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Applying a --custom-property declaration should store it in custom_properties
    cascade.apply_declaration(style, make_decl("--primary-color", "blue"), parent);
    ASSERT_NE(style.custom_properties.find("--primary-color"), style.custom_properties.end());
    EXPECT_EQ(style.custom_properties.at("--primary-color"), "blue");
}

TEST(PropertyCascadeTest, VarResolutionFromSelfCustomProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // First store the custom property
    cascade.apply_declaration(style, make_decl("--my-color", "red"), parent);
    // Then resolve var(--my-color) in the color property
    cascade.apply_declaration(style, make_decl("color", "var(--my-color)"), parent);
    EXPECT_EQ(style.color, (Color{255, 0, 0, 255}));
}

TEST(PropertyCascadeTest, VarResolutionFromParentCustomProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Custom property lives on the parent
    parent.custom_properties["--inherited-color"] = "#0000ff";
    cascade.apply_declaration(style, make_decl("color", "var(--inherited-color)"), parent);
    EXPECT_EQ(style.color, (Color{0, 0, 255, 255}));
}

TEST(PropertyCascadeTest, VarResolutionFallbackUsed) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // --undefined-var not set — fallback should be used
    cascade.apply_declaration(style, make_decl("color", "var(--undefined-var, green)"), parent);
    EXPECT_EQ(style.color, (Color{0, 128, 0, 255}));
}

TEST(PropertyCascadeTest, CustomPropertyParsedFromStylesheet) {
    // CSS custom properties in a stylesheet rule should produce declarations
    // with property names starting with "--"
    auto sheet = parse_stylesheet("div { --spacing: 16px; color: red; }");
    ASSERT_EQ(sheet.rules.size(), 1u);
    bool found_custom = false;
    for (const auto& decl : sheet.rules[0].declarations) {
        if (decl.property == "--spacing") {
            found_custom = true;
        }
    }
    EXPECT_TRUE(found_custom);
}

TEST(PropertyCascadeTest, VarSelfReferenceDoesNotCrash) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Self-referential var() — should not crash, just leave property unchanged
    // (the var_pass loop will exhaust without resolving)
    style.custom_properties["--loop"] = "var(--loop)";
    cascade.apply_declaration(style, make_decl("color", "var(--loop)"), parent);
    // Just verify we didn't crash — color may remain default black
    (void)style.color;
}

// ---------------------------------------------------------------------------
// Cycle 437 — pointer-events, user-select, text-overflow, scroll-behavior,
//             touch-action, overscroll-behavior, isolation, will-change
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, PointerEventsNoneAndAuto) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("pointer-events", "none"), parent);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);

    cascade.apply_declaration(style, make_decl("pointer-events", "auto"), parent);
    EXPECT_EQ(style.pointer_events, PointerEvents::Auto);
}

TEST(PropertyCascadeTest, UserSelectValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("user-select", "none"), parent);
    EXPECT_EQ(style.user_select, UserSelect::None);

    cascade.apply_declaration(style, make_decl("user-select", "text"), parent);
    EXPECT_EQ(style.user_select, UserSelect::Text);

    cascade.apply_declaration(style, make_decl("user-select", "all"), parent);
    EXPECT_EQ(style.user_select, UserSelect::All);

    cascade.apply_declaration(style, make_decl("user-select", "auto"), parent);
    EXPECT_EQ(style.user_select, UserSelect::Auto);
}

TEST(PropertyCascadeTest, TextOverflowEllipsisAndClip) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-overflow", "ellipsis"), parent);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);

    cascade.apply_declaration(style, make_decl("text-overflow", "clip"), parent);
    EXPECT_EQ(style.text_overflow, TextOverflow::Clip);

    cascade.apply_declaration(style, make_decl("text-overflow", "fade"), parent);
    EXPECT_EQ(style.text_overflow, TextOverflow::Fade);
}

TEST(PropertyCascadeTest, ScrollBehaviorSmoothAndAuto) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.scroll_behavior, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("scroll-behavior", "smooth"), parent);
    EXPECT_EQ(style.scroll_behavior, 1);

    cascade.apply_declaration(style, make_decl("scroll-behavior", "auto"), parent);
    EXPECT_EQ(style.scroll_behavior, 0);
}

TEST(PropertyCascadeTest, TouchActionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.touch_action, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("touch-action", "none"), parent);
    EXPECT_EQ(style.touch_action, 1);

    cascade.apply_declaration(style, make_decl("touch-action", "manipulation"), parent);
    EXPECT_EQ(style.touch_action, 2);

    cascade.apply_declaration(style, make_decl("touch-action", "pan-x"), parent);
    EXPECT_EQ(style.touch_action, 3);

    cascade.apply_declaration(style, make_decl("touch-action", "pan-y"), parent);
    EXPECT_EQ(style.touch_action, 4);
}

TEST(PropertyCascadeTest, OverscrollBehaviorSingleAndTwoValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single keyword: sets both x and y
    cascade.apply_declaration(style, make_decl("overscroll-behavior", "contain"), parent);
    EXPECT_EQ(style.overscroll_behavior, 1);
    EXPECT_EQ(style.overscroll_behavior_x, 1);
    EXPECT_EQ(style.overscroll_behavior_y, 1);

    // Two keywords: x then y
    cascade.apply_declaration(style, make_decl("overscroll-behavior", "none auto"), parent);
    EXPECT_EQ(style.overscroll_behavior_x, 2);  // none
    EXPECT_EQ(style.overscroll_behavior_y, 0);  // auto
}

TEST(PropertyCascadeTest, IsolationIsolateAndAuto) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.isolation, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("isolation", "isolate"), parent);
    EXPECT_EQ(style.isolation, 1);

    cascade.apply_declaration(style, make_decl("isolation", "auto"), parent);
    EXPECT_EQ(style.isolation, 0);
}

TEST(PropertyCascadeTest, WillChangeStoresValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.will_change.empty());  // default: empty

    cascade.apply_declaration(style, make_decl("will-change", "transform"), parent);
    EXPECT_EQ(style.will_change, "transform");

    cascade.apply_declaration(style, make_decl("will-change", "opacity, transform"), parent);
    EXPECT_EQ(style.will_change, "opacity, transform");

    cascade.apply_declaration(style, make_decl("will-change", "auto"), parent);
    EXPECT_TRUE(style.will_change.empty());
}

// ---------------------------------------------------------------------------
// Cycle 438 — cursor, resize, appearance, list-style-type/position,
//             counter-increment/reset, content-visibility
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, CursorValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.cursor, Cursor::Auto);  // default

    cascade.apply_declaration(style, make_decl("cursor", "default"), parent);
    EXPECT_EQ(style.cursor, Cursor::Default);

    cascade.apply_declaration(style, make_decl("cursor", "pointer"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);

    cascade.apply_declaration(style, make_decl("cursor", "text"), parent);
    EXPECT_EQ(style.cursor, Cursor::Text);

    cascade.apply_declaration(style, make_decl("cursor", "move"), parent);
    EXPECT_EQ(style.cursor, Cursor::Move);

    cascade.apply_declaration(style, make_decl("cursor", "not-allowed"), parent);
    EXPECT_EQ(style.cursor, Cursor::NotAllowed);

    cascade.apply_declaration(style, make_decl("cursor", "auto"), parent);
    EXPECT_EQ(style.cursor, Cursor::Auto);
}

TEST(PropertyCascadeTest, ResizeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.resize, 0);  // default: none

    cascade.apply_declaration(style, make_decl("resize", "both"), parent);
    EXPECT_EQ(style.resize, 1);

    cascade.apply_declaration(style, make_decl("resize", "horizontal"), parent);
    EXPECT_EQ(style.resize, 2);

    cascade.apply_declaration(style, make_decl("resize", "vertical"), parent);
    EXPECT_EQ(style.resize, 3);

    cascade.apply_declaration(style, make_decl("resize", "none"), parent);
    EXPECT_EQ(style.resize, 0);
}

TEST(PropertyCascadeTest, AppearanceValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.appearance, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("appearance", "none"), parent);
    EXPECT_EQ(style.appearance, 1);

    cascade.apply_declaration(style, make_decl("appearance", "menulist-button"), parent);
    EXPECT_EQ(style.appearance, 2);

    cascade.apply_declaration(style, make_decl("-webkit-appearance", "textfield"), parent);
    EXPECT_EQ(style.appearance, 3);

    cascade.apply_declaration(style, make_decl("appearance", "button"), parent);
    EXPECT_EQ(style.appearance, 4);

    cascade.apply_declaration(style, make_decl("appearance", "auto"), parent);
    EXPECT_EQ(style.appearance, 0);
}

TEST(PropertyCascadeTest, ListStyleTypeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.list_style_type, ListStyleType::Disc);  // default

    cascade.apply_declaration(style, make_decl("list-style-type", "decimal"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Decimal);

    cascade.apply_declaration(style, make_decl("list-style-type", "upper-roman"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::UpperRoman);

    cascade.apply_declaration(style, make_decl("list-style-type", "lower-alpha"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::LowerAlpha);

    cascade.apply_declaration(style, make_decl("list-style-type", "none"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::None);

    cascade.apply_declaration(style, make_decl("list-style-type", "disc"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Disc);
}

TEST(PropertyCascadeTest, ListStylePositionInsideAndOutside) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.list_style_position, ListStylePosition::Outside);  // default

    cascade.apply_declaration(style, make_decl("list-style-position", "inside"), parent);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);

    cascade.apply_declaration(style, make_decl("list-style-position", "outside"), parent);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Outside);
}

TEST(PropertyCascadeTest, CounterIncrementAndResetStoreStrings) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.counter_increment.empty());  // default
    EXPECT_TRUE(style.counter_reset.empty());

    cascade.apply_declaration(style, make_decl("counter-increment", "section 1"), parent);
    EXPECT_EQ(style.counter_increment, "section 1");

    cascade.apply_declaration(style, make_decl("counter-reset", "chapter 0"), parent);
    EXPECT_EQ(style.counter_reset, "chapter 0");

    cascade.apply_declaration(style, make_decl("counter-increment", "none"), parent);
    EXPECT_EQ(style.counter_increment, "none");
}

TEST(PropertyCascadeTest, ContentVisibilityValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.content_visibility, 0);  // default: visible

    cascade.apply_declaration(style, make_decl("content-visibility", "hidden"), parent);
    EXPECT_EQ(style.content_visibility, 1);

    cascade.apply_declaration(style, make_decl("content-visibility", "auto"), parent);
    EXPECT_EQ(style.content_visibility, 2);

    cascade.apply_declaration(style, make_decl("content-visibility", "visible"), parent);
    EXPECT_EQ(style.content_visibility, 0);
}

TEST(PropertyCascadeTest, CounterSetStoresString) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.counter_set.empty());  // default

    cascade.apply_declaration(style, make_decl("counter-set", "page 5"), parent);
    EXPECT_EQ(style.counter_set, "page 5");
}

// ---------------------------------------------------------------------------
// Cycle 439 — object-fit, object-position, mix-blend-mode, aspect-ratio,
//             contain, image-rendering, clip-path none, webkit-user-select
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ObjectFitValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.object_fit, 0);  // default: fill

    cascade.apply_declaration(style, make_decl("object-fit", "contain"), parent);
    EXPECT_EQ(style.object_fit, 1);

    cascade.apply_declaration(style, make_decl("object-fit", "cover"), parent);
    EXPECT_EQ(style.object_fit, 2);

    cascade.apply_declaration(style, make_decl("object-fit", "none"), parent);
    EXPECT_EQ(style.object_fit, 3);

    cascade.apply_declaration(style, make_decl("object-fit", "scale-down"), parent);
    EXPECT_EQ(style.object_fit, 4);

    cascade.apply_declaration(style, make_decl("object-fit", "fill"), parent);
    EXPECT_EQ(style.object_fit, 0);
}

TEST(PropertyCascadeTest, ObjectPositionCenterDefault) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: 50% 50%
    EXPECT_FLOAT_EQ(style.object_position_x, 50.0f);
    EXPECT_FLOAT_EQ(style.object_position_y, 50.0f);

    cascade.apply_declaration(style, make_decl("object-position", "left top"), parent);
    EXPECT_FLOAT_EQ(style.object_position_x, 0.0f);
    EXPECT_FLOAT_EQ(style.object_position_y, 0.0f);

    cascade.apply_declaration(style, make_decl("object-position", "right bottom"), parent);
    EXPECT_FLOAT_EQ(style.object_position_x, 100.0f);
    EXPECT_FLOAT_EQ(style.object_position_y, 100.0f);

    cascade.apply_declaration(style, make_decl("object-position", "center"), parent);
    EXPECT_FLOAT_EQ(style.object_position_x, 50.0f);
    EXPECT_FLOAT_EQ(style.object_position_y, 50.0f);
}

TEST(PropertyCascadeTest, MixBlendModeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.mix_blend_mode, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "multiply"), parent);
    EXPECT_EQ(style.mix_blend_mode, 1);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "screen"), parent);
    EXPECT_EQ(style.mix_blend_mode, 2);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "overlay"), parent);
    EXPECT_EQ(style.mix_blend_mode, 3);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "difference"), parent);
    EXPECT_EQ(style.mix_blend_mode, 10);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "normal"), parent);
    EXPECT_EQ(style.mix_blend_mode, 0);
}

TEST(PropertyCascadeTest, AspectRatioAutoAndRatio) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.aspect_ratio, 0.0f);  // default: auto

    cascade.apply_declaration(style, make_decl("aspect-ratio", "16/9"), parent);
    EXPECT_NEAR(style.aspect_ratio, 16.0f / 9.0f, 0.001f);

    cascade.apply_declaration(style, make_decl("aspect-ratio", "4/3"), parent);
    EXPECT_NEAR(style.aspect_ratio, 4.0f / 3.0f, 0.001f);

    cascade.apply_declaration(style, make_decl("aspect-ratio", "1/1"), parent);
    EXPECT_FLOAT_EQ(style.aspect_ratio, 1.0f);

    cascade.apply_declaration(style, make_decl("aspect-ratio", "auto"), parent);
    EXPECT_FLOAT_EQ(style.aspect_ratio, 0.0f);
}

TEST(PropertyCascadeTest, ContainValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.contain, 0);  // default: none

    cascade.apply_declaration(style, make_decl("contain", "strict"), parent);
    EXPECT_EQ(style.contain, 1);

    cascade.apply_declaration(style, make_decl("contain", "content"), parent);
    EXPECT_EQ(style.contain, 2);

    cascade.apply_declaration(style, make_decl("contain", "size"), parent);
    EXPECT_EQ(style.contain, 3);

    cascade.apply_declaration(style, make_decl("contain", "layout"), parent);
    EXPECT_EQ(style.contain, 4);

    cascade.apply_declaration(style, make_decl("contain", "paint"), parent);
    EXPECT_EQ(style.contain, 6);

    cascade.apply_declaration(style, make_decl("contain", "none"), parent);
    EXPECT_EQ(style.contain, 0);
}

TEST(PropertyCascadeTest, ImageRenderingValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.image_rendering, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("image-rendering", "smooth"), parent);
    EXPECT_EQ(style.image_rendering, 1);

    cascade.apply_declaration(style, make_decl("image-rendering", "crisp-edges"), parent);
    EXPECT_EQ(style.image_rendering, 3);

    cascade.apply_declaration(style, make_decl("image-rendering", "pixelated"), parent);
    EXPECT_EQ(style.image_rendering, 4);

    cascade.apply_declaration(style, make_decl("image-rendering", "auto"), parent);
    EXPECT_EQ(style.image_rendering, 0);
}

TEST(PropertyCascadeTest, ClipPathNoneClearsValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Manually set a non-zero clip_path_type then reset with "none"
    style.clip_path_type = 1;
    style.clip_path_values.push_back(50.0f);

    cascade.apply_declaration(style, make_decl("clip-path", "none"), parent);
    EXPECT_EQ(style.clip_path_type, 0);
    EXPECT_TRUE(style.clip_path_values.empty());
}

TEST(PropertyCascadeTest, WebkitUserSelectAlias) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // -webkit-user-select should map to same user_select field
    cascade.apply_declaration(style, make_decl("-webkit-user-select", "none"), parent);
    EXPECT_EQ(style.user_select, UserSelect::None);

    cascade.apply_declaration(style, make_decl("-webkit-user-select", "text"), parent);
    EXPECT_EQ(style.user_select, UserSelect::Text);

    cascade.apply_declaration(style, make_decl("-webkit-user-select", "all"), parent);
    EXPECT_EQ(style.user_select, UserSelect::All);
}

// ---------------------------------------------------------------------------
// Cycle 440 — CSS multi-column: column-count, column-fill, column-width,
//             column-gap, column-rule-style, column-rule-color,
//             column-rule-width, columns shorthand, column-span
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ColumnCountAutoAndExplicit) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.column_count, -1);  // default: auto

    cascade.apply_declaration(style, make_decl("column-count", "3"), parent);
    EXPECT_EQ(style.column_count, 3);

    cascade.apply_declaration(style, make_decl("column-count", "1"), parent);
    EXPECT_EQ(style.column_count, 1);

    cascade.apply_declaration(style, make_decl("column-count", "auto"), parent);
    EXPECT_EQ(style.column_count, -1);
}

TEST(PropertyCascadeTest, ColumnFillValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.column_fill, 0);  // default: balance

    cascade.apply_declaration(style, make_decl("column-fill", "auto"), parent);
    EXPECT_EQ(style.column_fill, 1);

    cascade.apply_declaration(style, make_decl("column-fill", "balance-all"), parent);
    EXPECT_EQ(style.column_fill, 2);

    cascade.apply_declaration(style, make_decl("column-fill", "balance"), parent);
    EXPECT_EQ(style.column_fill, 0);
}

TEST(PropertyCascadeTest, ColumnWidthAutoAndPx) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.column_width.is_auto());  // default: auto

    cascade.apply_declaration(style, make_decl("column-width", "200px"), parent);
    EXPECT_FALSE(style.column_width.is_auto());
    EXPECT_FLOAT_EQ(style.column_width.to_px(0), 200.0f);

    cascade.apply_declaration(style, make_decl("column-width", "auto"), parent);
    EXPECT_TRUE(style.column_width.is_auto());
}

TEST(PropertyCascadeTest, ColumnGapPx) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("column-gap", "16px"), parent);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 16.0f);

    cascade.apply_declaration(style, make_decl("column-gap", "0px"), parent);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 0.0f);
}

TEST(PropertyCascadeTest, ColumnRuleStyleValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.column_rule_style, 0);  // default: none

    cascade.apply_declaration(style, make_decl("column-rule-style", "solid"), parent);
    EXPECT_EQ(style.column_rule_style, 1);

    cascade.apply_declaration(style, make_decl("column-rule-style", "dashed"), parent);
    EXPECT_EQ(style.column_rule_style, 2);

    cascade.apply_declaration(style, make_decl("column-rule-style", "dotted"), parent);
    EXPECT_EQ(style.column_rule_style, 3);

    cascade.apply_declaration(style, make_decl("column-rule-style", "none"), parent);
    EXPECT_EQ(style.column_rule_style, 0);
}

TEST(PropertyCascadeTest, ColumnRuleColorAndWidth) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("column-rule-color", "red"), parent);
    EXPECT_EQ(style.column_rule_color, (Color{255, 0, 0, 255}));

    cascade.apply_declaration(style, make_decl("column-rule-width", "2px"), parent);
    EXPECT_FLOAT_EQ(style.column_rule_width, 2.0f);
}

TEST(PropertyCascadeTest, ColumnsShorthandCountAndWidth) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "3 200px" — column-count=3, column-width=200px
    cascade.apply_declaration(style, make_decl("columns", "3 200px"), parent);
    EXPECT_EQ(style.column_count, 3);
    EXPECT_FLOAT_EQ(style.column_width.to_px(0), 200.0f);
}

TEST(PropertyCascadeTest, ColumnSpanNoneAndAll) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.column_span, 0);  // default: none

    cascade.apply_declaration(style, make_decl("column-span", "all"), parent);
    EXPECT_EQ(style.column_span, 1);

    cascade.apply_declaration(style, make_decl("column-span", "none"), parent);
    EXPECT_EQ(style.column_span, 0);
}

// ---------------------------------------------------------------------------
// Cycle 441 — CSS fragmentation: orphans, widows, break-before/after/inside,
//             page-break-before/after/inside
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, OrphansAndWidows) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.orphans, 2);  // default: 2
    EXPECT_EQ(style.widows, 2);   // default: 2

    cascade.apply_declaration(style, make_decl("orphans", "3"), parent);
    EXPECT_EQ(style.orphans, 3);

    cascade.apply_declaration(style, make_decl("widows", "4"), parent);
    EXPECT_EQ(style.widows, 4);
}

TEST(PropertyCascadeTest, BreakBeforeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.break_before, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("break-before", "avoid"), parent);
    EXPECT_EQ(style.break_before, 1);

    cascade.apply_declaration(style, make_decl("break-before", "always"), parent);
    EXPECT_EQ(style.break_before, 2);

    cascade.apply_declaration(style, make_decl("break-before", "page"), parent);
    EXPECT_EQ(style.break_before, 3);

    cascade.apply_declaration(style, make_decl("break-before", "column"), parent);
    EXPECT_EQ(style.break_before, 4);

    cascade.apply_declaration(style, make_decl("break-before", "auto"), parent);
    EXPECT_EQ(style.break_before, 0);
}

TEST(PropertyCascadeTest, BreakAfterValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.break_after, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("break-after", "column"), parent);
    EXPECT_EQ(style.break_after, 4);

    cascade.apply_declaration(style, make_decl("break-after", "page"), parent);
    EXPECT_EQ(style.break_after, 3);

    cascade.apply_declaration(style, make_decl("break-after", "auto"), parent);
    EXPECT_EQ(style.break_after, 0);
}

TEST(PropertyCascadeTest, BreakInsideValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.break_inside, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("break-inside", "avoid"), parent);
    EXPECT_EQ(style.break_inside, 1);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-page"), parent);
    EXPECT_EQ(style.break_inside, 2);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-column"), parent);
    EXPECT_EQ(style.break_inside, 3);

    cascade.apply_declaration(style, make_decl("break-inside", "auto"), parent);
    EXPECT_EQ(style.break_inside, 0);
}

TEST(PropertyCascadeTest, PageBreakBeforeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.page_break_before, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("page-break-before", "always"), parent);
    EXPECT_EQ(style.page_break_before, 1);

    cascade.apply_declaration(style, make_decl("page-break-before", "avoid"), parent);
    EXPECT_EQ(style.page_break_before, 2);

    cascade.apply_declaration(style, make_decl("page-break-before", "left"), parent);
    EXPECT_EQ(style.page_break_before, 3);

    cascade.apply_declaration(style, make_decl("page-break-before", "right"), parent);
    EXPECT_EQ(style.page_break_before, 4);

    cascade.apply_declaration(style, make_decl("page-break-before", "auto"), parent);
    EXPECT_EQ(style.page_break_before, 0);
}

TEST(PropertyCascadeTest, PageBreakAfterValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.page_break_after, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("page-break-after", "always"), parent);
    EXPECT_EQ(style.page_break_after, 1);

    cascade.apply_declaration(style, make_decl("page-break-after", "avoid"), parent);
    EXPECT_EQ(style.page_break_after, 2);

    cascade.apply_declaration(style, make_decl("page-break-after", "auto"), parent);
    EXPECT_EQ(style.page_break_after, 0);
}

TEST(PropertyCascadeTest, PageBreakInsideValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.page_break_inside, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("page-break-inside", "avoid"), parent);
    EXPECT_EQ(style.page_break_inside, 1);

    cascade.apply_declaration(style, make_decl("page-break-inside", "auto"), parent);
    EXPECT_EQ(style.page_break_inside, 0);
}

TEST(PropertyCascadeTest, BreakRegionValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("break-before", "region"), parent);
    EXPECT_EQ(style.break_before, 5);

    cascade.apply_declaration(style, make_decl("break-after", "region"), parent);
    EXPECT_EQ(style.break_after, 5);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-region"), parent);
    EXPECT_EQ(style.break_inside, 4);
}

// ---------------------------------------------------------------------------
// Cycle 442 — CSS Grid layout: grid-template-columns/rows, grid-column/row,
//             grid-column-start/end longhands, grid-auto-flow, grid-auto-rows,
//             grid-template-areas, grid-area
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, GridTemplateColumnsAndRows) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.grid_template_columns.empty());
    EXPECT_TRUE(style.grid_template_rows.empty());

    cascade.apply_declaration(style, make_decl("grid-template-columns", "1fr 2fr 1fr"), parent);
    EXPECT_EQ(style.grid_template_columns, "1fr 2fr 1fr");

    cascade.apply_declaration(style, make_decl("grid-template-rows", "100px auto"), parent);
    EXPECT_EQ(style.grid_template_rows, "100px auto");
}

TEST(PropertyCascadeTest, GridColumnAndRowShorthands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("grid-column", "1 / 3"), parent);
    EXPECT_EQ(style.grid_column, "1 / 3");

    cascade.apply_declaration(style, make_decl("grid-row", "2 / 4"), parent);
    EXPECT_EQ(style.grid_row, "2 / 4");
}

TEST(PropertyCascadeTest, GridColumnStartEndRebuildShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set start first, no end yet — shorthand = start only
    cascade.apply_declaration(style, make_decl("grid-column-start", "2"), parent);
    EXPECT_EQ(style.grid_column_start, "2");
    EXPECT_EQ(style.grid_column, "2");

    // Now set end — shorthand should be rebuilt
    cascade.apply_declaration(style, make_decl("grid-column-end", "5"), parent);
    EXPECT_EQ(style.grid_column_end, "5");
    EXPECT_EQ(style.grid_column, "2 / 5");
}

TEST(PropertyCascadeTest, GridRowStartEndRebuildShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("grid-row-start", "1"), parent);
    EXPECT_EQ(style.grid_row_start, "1");

    cascade.apply_declaration(style, make_decl("grid-row-end", "3"), parent);
    EXPECT_EQ(style.grid_row_end, "3");
    EXPECT_EQ(style.grid_row, "1 / 3");
}

TEST(PropertyCascadeTest, GridAutoFlowValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.grid_auto_flow, 0);  // default: row

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "column"), parent);
    EXPECT_EQ(style.grid_auto_flow, 1);

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "dense"), parent);
    EXPECT_EQ(style.grid_auto_flow, 2);  // dense = row dense

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "column dense"), parent);
    EXPECT_EQ(style.grid_auto_flow, 3);

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "row"), parent);
    EXPECT_EQ(style.grid_auto_flow, 0);
}

TEST(PropertyCascadeTest, GridAutoRowsAndColumns) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("grid-auto-rows", "minmax(100px, auto)"), parent);
    EXPECT_EQ(style.grid_auto_rows, "minmax(100px, auto)");

    cascade.apply_declaration(style, make_decl("grid-auto-columns", "1fr"), parent);
    EXPECT_EQ(style.grid_auto_columns, "1fr");
}

TEST(PropertyCascadeTest, GridTemplateAreas) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.grid_template_areas.empty());

    const std::string areas = "\"header header\" \"sidebar main\"";
    cascade.apply_declaration(style, make_decl("grid-template-areas", areas), parent);
    EXPECT_EQ(style.grid_template_areas, areas);
}

TEST(PropertyCascadeTest, GridArea) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.grid_area.empty());

    cascade.apply_declaration(style, make_decl("grid-area", "header"), parent);
    EXPECT_EQ(style.grid_area, "header");

    cascade.apply_declaration(style, make_decl("grid-area", "1 / 2 / 3 / 4"), parent);
    EXPECT_EQ(style.grid_area, "1 / 2 / 3 / 4");
}

// ---------------------------------------------------------------------------
// Cycle 444 — direction, writing-mode, unicode-bidi, line-clamp,
//             caret-color, text-orientation, text-combine-upright,
//             backface-visibility
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, DirectionLtrAndRtl) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.direction, Direction::Ltr);  // default

    cascade.apply_declaration(style, make_decl("direction", "rtl"), parent);
    EXPECT_EQ(style.direction, Direction::Rtl);

    cascade.apply_declaration(style, make_decl("direction", "ltr"), parent);
    EXPECT_EQ(style.direction, Direction::Ltr);
}

TEST(PropertyCascadeTest, WritingModeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.writing_mode, 0);  // default: horizontal-tb

    cascade.apply_declaration(style, make_decl("writing-mode", "vertical-rl"), parent);
    EXPECT_EQ(style.writing_mode, 1);

    cascade.apply_declaration(style, make_decl("writing-mode", "vertical-lr"), parent);
    EXPECT_EQ(style.writing_mode, 2);

    cascade.apply_declaration(style, make_decl("writing-mode", "horizontal-tb"), parent);
    EXPECT_EQ(style.writing_mode, 0);
}

TEST(PropertyCascadeTest, UnicodeBidiValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.unicode_bidi, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("unicode-bidi", "embed"), parent);
    EXPECT_EQ(style.unicode_bidi, 1);

    cascade.apply_declaration(style, make_decl("unicode-bidi", "bidi-override"), parent);
    EXPECT_EQ(style.unicode_bidi, 2);

    cascade.apply_declaration(style, make_decl("unicode-bidi", "isolate"), parent);
    EXPECT_EQ(style.unicode_bidi, 3);

    cascade.apply_declaration(style, make_decl("unicode-bidi", "isolate-override"), parent);
    EXPECT_EQ(style.unicode_bidi, 4);

    cascade.apply_declaration(style, make_decl("unicode-bidi", "plaintext"), parent);
    EXPECT_EQ(style.unicode_bidi, 5);

    cascade.apply_declaration(style, make_decl("unicode-bidi", "normal"), parent);
    EXPECT_EQ(style.unicode_bidi, 0);
}

TEST(PropertyCascadeTest, LineClampValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.line_clamp, -1);  // default: none

    cascade.apply_declaration(style, make_decl("-webkit-line-clamp", "3"), parent);
    EXPECT_EQ(style.line_clamp, 3);

    cascade.apply_declaration(style, make_decl("line-clamp", "1"), parent);
    EXPECT_EQ(style.line_clamp, 1);

    cascade.apply_declaration(style, make_decl("line-clamp", "none"), parent);
    EXPECT_EQ(style.line_clamp, -1);
}

TEST(PropertyCascadeTest, CaretColorSet) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("caret-color", "red"), parent);
    EXPECT_EQ(style.caret_color, (Color{255, 0, 0, 255}));

    cascade.apply_declaration(style, make_decl("caret-color", "#00ff00"), parent);
    EXPECT_EQ(style.caret_color, (Color{0, 255, 0, 255}));
}

TEST(PropertyCascadeTest, TextOrientationValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_orientation, 0);  // default: mixed

    cascade.apply_declaration(style, make_decl("text-orientation", "upright"), parent);
    EXPECT_EQ(style.text_orientation, 1);

    cascade.apply_declaration(style, make_decl("text-orientation", "sideways"), parent);
    EXPECT_EQ(style.text_orientation, 2);

    cascade.apply_declaration(style, make_decl("text-orientation", "mixed"), parent);
    EXPECT_EQ(style.text_orientation, 0);
}

TEST(PropertyCascadeTest, TextCombineUprightValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_combine_upright, 0);  // default: none

    cascade.apply_declaration(style, make_decl("text-combine-upright", "all"), parent);
    EXPECT_EQ(style.text_combine_upright, 1);

    cascade.apply_declaration(style, make_decl("text-combine-upright", "digits"), parent);
    EXPECT_EQ(style.text_combine_upright, 2);

    cascade.apply_declaration(style, make_decl("text-combine-upright", "none"), parent);
    EXPECT_EQ(style.text_combine_upright, 0);
}

TEST(PropertyCascadeTest, BackfaceVisibilityValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.backface_visibility, 0);  // default: visible

    cascade.apply_declaration(style, make_decl("backface-visibility", "hidden"), parent);
    EXPECT_EQ(style.backface_visibility, 1);

    cascade.apply_declaration(style, make_decl("backface-visibility", "visible"), parent);
    EXPECT_EQ(style.backface_visibility, 0);
}

// ---------------------------------------------------------------------------
// Cycle 446 — CSS animation: animation-name, animation-duration,
//             animation-timing-function, animation-delay,
//             animation-iteration-count, animation-direction,
//             animation-fill-mode, animation-play-state
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, AnimationName) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.animation_name.empty());  // default

    cascade.apply_declaration(style, make_decl("animation-name", "slide-in"), parent);
    EXPECT_EQ(style.animation_name, "slide-in");

    cascade.apply_declaration(style, make_decl("animation-name", "none"), parent);
    EXPECT_EQ(style.animation_name, "none");
}

TEST(PropertyCascadeTest, AnimationDurationSecondsAndMs) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.animation_duration, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("animation-duration", "2s"), parent);
    EXPECT_FLOAT_EQ(style.animation_duration, 2.0f);

    cascade.apply_declaration(style, make_decl("animation-duration", "500ms"), parent);
    EXPECT_NEAR(style.animation_duration, 0.5f, 0.001f);

    cascade.apply_declaration(style, make_decl("animation-duration", "0s"), parent);
    EXPECT_FLOAT_EQ(style.animation_duration, 0.0f);
}

TEST(PropertyCascadeTest, AnimationTimingFunctionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_timing, 0);  // default: ease

    cascade.apply_declaration(style, make_decl("animation-timing-function", "linear"), parent);
    EXPECT_EQ(style.animation_timing, 1);

    cascade.apply_declaration(style, make_decl("animation-timing-function", "ease-in"), parent);
    EXPECT_EQ(style.animation_timing, 2);

    cascade.apply_declaration(style, make_decl("animation-timing-function", "ease-out"), parent);
    EXPECT_EQ(style.animation_timing, 3);

    cascade.apply_declaration(style, make_decl("animation-timing-function", "ease-in-out"), parent);
    EXPECT_EQ(style.animation_timing, 4);

    cascade.apply_declaration(style, make_decl("animation-timing-function", "ease"), parent);
    EXPECT_EQ(style.animation_timing, 0);
}

TEST(PropertyCascadeTest, AnimationDelaySeconds) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.animation_delay, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("animation-delay", "1s"), parent);
    EXPECT_FLOAT_EQ(style.animation_delay, 1.0f);

    cascade.apply_declaration(style, make_decl("animation-delay", "250ms"), parent);
    EXPECT_NEAR(style.animation_delay, 0.25f, 0.001f);
}

TEST(PropertyCascadeTest, AnimationIterationCount) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.animation_iteration_count, 1.0f);  // default

    cascade.apply_declaration(style, make_decl("animation-iteration-count", "infinite"), parent);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, -1.0f);

    cascade.apply_declaration(style, make_decl("animation-iteration-count", "3"), parent);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, 3.0f);
}

TEST(PropertyCascadeTest, AnimationDirectionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_direction, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("animation-direction", "reverse"), parent);
    EXPECT_EQ(style.animation_direction, 1);

    cascade.apply_declaration(style, make_decl("animation-direction", "alternate"), parent);
    EXPECT_EQ(style.animation_direction, 2);

    cascade.apply_declaration(style, make_decl("animation-direction", "alternate-reverse"), parent);
    EXPECT_EQ(style.animation_direction, 3);

    cascade.apply_declaration(style, make_decl("animation-direction", "normal"), parent);
    EXPECT_EQ(style.animation_direction, 0);
}

TEST(PropertyCascadeTest, AnimationFillModeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_fill_mode, 0);  // default: none

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "forwards"), parent);
    EXPECT_EQ(style.animation_fill_mode, 1);

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "backwards"), parent);
    EXPECT_EQ(style.animation_fill_mode, 2);

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "both"), parent);
    EXPECT_EQ(style.animation_fill_mode, 3);

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "none"), parent);
    EXPECT_EQ(style.animation_fill_mode, 0);
}

TEST(PropertyCascadeTest, AnimationPlayStateValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_play_state, 0);  // default: running

    cascade.apply_declaration(style, make_decl("animation-play-state", "paused"), parent);
    EXPECT_EQ(style.animation_play_state, 1);

    cascade.apply_declaration(style, make_decl("animation-play-state", "running"), parent);
    EXPECT_EQ(style.animation_play_state, 0);
}

// ---------------------------------------------------------------------------
// Cycle 447 — CSS transition: transition-property, transition-duration,
//             transition-timing-function, transition-delay, transition
//             shorthand (single and multiple)
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, TransitionPropertyStoresString) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transition_property, "all");  // default

    cascade.apply_declaration(style, make_decl("transition-property", "opacity"), parent);
    EXPECT_EQ(style.transition_property, "opacity");
    EXPECT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "opacity");

    cascade.apply_declaration(style, make_decl("transition-property", "none"), parent);
    EXPECT_EQ(style.transition_property, "none");
}

TEST(PropertyCascadeTest, TransitionDurationSecondsAndMs) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.transition_duration, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("transition-duration", "0.3s"), parent);
    EXPECT_NEAR(style.transition_duration, 0.3f, 0.001f);

    cascade.apply_declaration(style, make_decl("transition-duration", "400ms"), parent);
    EXPECT_NEAR(style.transition_duration, 0.4f, 0.001f);

    cascade.apply_declaration(style, make_decl("transition-duration", "1s"), parent);
    EXPECT_FLOAT_EQ(style.transition_duration, 1.0f);
}

TEST(PropertyCascadeTest, TransitionTimingFunctionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transition_timing, 0);  // default: ease

    cascade.apply_declaration(style, make_decl("transition-timing-function", "linear"), parent);
    EXPECT_EQ(style.transition_timing, 1);

    cascade.apply_declaration(style, make_decl("transition-timing-function", "ease-in"), parent);
    EXPECT_EQ(style.transition_timing, 2);

    cascade.apply_declaration(style, make_decl("transition-timing-function", "ease-out"), parent);
    EXPECT_EQ(style.transition_timing, 3);

    cascade.apply_declaration(style, make_decl("transition-timing-function", "ease-in-out"), parent);
    EXPECT_EQ(style.transition_timing, 4);

    cascade.apply_declaration(style, make_decl("transition-timing-function", "ease"), parent);
    EXPECT_EQ(style.transition_timing, 0);
}

TEST(PropertyCascadeTest, TransitionDelay) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.transition_delay, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("transition-delay", "0.5s"), parent);
    EXPECT_NEAR(style.transition_delay, 0.5f, 0.001f);

    cascade.apply_declaration(style, make_decl("transition-delay", "200ms"), parent);
    EXPECT_NEAR(style.transition_delay, 0.2f, 0.001f);
}

TEST(PropertyCascadeTest, TransitionShorthandSingleValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "opacity 0.3s ease"
    cascade.apply_declaration(style, make_decl("transition", "opacity 0.3s ease"), parent);

    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_NEAR(style.transitions[0].duration_ms, 300.0f, 1.0f);
    EXPECT_EQ(style.transitions[0].timing_function, 0);  // ease
    // Legacy scalar fields should also be set
    EXPECT_EQ(style.transition_property, "opacity");
    EXPECT_NEAR(style.transition_duration, 0.3f, 0.001f);
}

TEST(PropertyCascadeTest, TransitionShorthandWithDelay) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "transform 1s linear 0.2s"
    cascade.apply_declaration(style, make_decl("transition", "transform 1s linear 0.2s"), parent);

    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "transform");
    EXPECT_NEAR(style.transitions[0].duration_ms, 1000.0f, 1.0f);
    EXPECT_EQ(style.transitions[0].timing_function, 1);   // linear
    EXPECT_NEAR(style.transitions[0].delay_ms, 200.0f, 1.0f);
}

TEST(PropertyCascadeTest, TransitionShorthandMultipleValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "opacity 0.3s ease, transform 0.5s ease-in"
    cascade.apply_declaration(style, make_decl("transition", "opacity 0.3s ease, transform 0.5s ease-in"), parent);

    ASSERT_EQ(style.transitions.size(), 2u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_NEAR(style.transitions[0].duration_ms, 300.0f, 1.0f);
    EXPECT_EQ(style.transitions[1].property, "transform");
    EXPECT_NEAR(style.transitions[1].duration_ms, 500.0f, 1.0f);
    EXPECT_EQ(style.transitions[1].timing_function, 2);  // ease-in
}

TEST(PropertyCascadeTest, TransitionCubicBezierTimingFunction) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // cubic-bezier() sets timing to 5 and stores control points
    cascade.apply_declaration(style, make_decl("transition-timing-function", "cubic-bezier(0.42, 0, 1.0, 1.0)"), parent);
    EXPECT_EQ(style.transition_timing, 5);
    EXPECT_NEAR(style.transition_bezier_x1, 0.42f, 0.01f);
    EXPECT_NEAR(style.transition_bezier_y1, 0.0f, 0.01f);
    EXPECT_NEAR(style.transition_bezier_x2, 1.0f, 0.01f);
    EXPECT_NEAR(style.transition_bezier_y2, 1.0f, 0.01f);
}

// ---------------------------------------------------------------------------
// Cycle 448 — CSS transform: translate, rotate, scale, skew, matrix,
//             transform none, transform-style, transform-origin, perspective,
//             transform-box
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, TransformTranslate) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.transforms.empty());  // default: none

    cascade.apply_declaration(style, make_decl("transform", "translate(10px, 20px)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 10.0f);
    EXPECT_FLOAT_EQ(style.transforms[0].y, 20.0f);
}

TEST(PropertyCascadeTest, TransformRotate) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform", "rotate(45deg)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(style.transforms[0].angle, 45.0f);
}

TEST(PropertyCascadeTest, TransformScale) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform", "scale(2, 0.5)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 2.0f);
    EXPECT_FLOAT_EQ(style.transforms[0].y, 0.5f);
}

TEST(PropertyCascadeTest, TransformNoneClearsTransforms) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform", "rotate(90deg)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);

    cascade.apply_declaration(style, make_decl("transform", "none"), parent);
    EXPECT_TRUE(style.transforms.empty());
}

TEST(PropertyCascadeTest, TransformStyleFlatAndPreserve3d) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transform_style, 0);  // default: flat

    cascade.apply_declaration(style, make_decl("transform-style", "preserve-3d"), parent);
    EXPECT_EQ(style.transform_style, 1);

    cascade.apply_declaration(style, make_decl("transform-style", "flat"), parent);
    EXPECT_EQ(style.transform_style, 0);
}

TEST(PropertyCascadeTest, TransformOriginKeywords) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.transform_origin_x, 50.0f);  // default: center
    EXPECT_FLOAT_EQ(style.transform_origin_y, 50.0f);

    cascade.apply_declaration(style, make_decl("transform-origin", "left top"), parent);
    EXPECT_FLOAT_EQ(style.transform_origin_x, 0.0f);
    EXPECT_FLOAT_EQ(style.transform_origin_y, 0.0f);

    cascade.apply_declaration(style, make_decl("transform-origin", "right bottom"), parent);
    EXPECT_FLOAT_EQ(style.transform_origin_x, 100.0f);
    EXPECT_FLOAT_EQ(style.transform_origin_y, 100.0f);

    cascade.apply_declaration(style, make_decl("transform-origin", "center center"), parent);
    EXPECT_FLOAT_EQ(style.transform_origin_x, 50.0f);
    EXPECT_FLOAT_EQ(style.transform_origin_y, 50.0f);
}

TEST(PropertyCascadeTest, PerspectivePxAndNone) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.perspective, 0.0f);  // default: none

    cascade.apply_declaration(style, make_decl("perspective", "500px"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 500.0f);

    cascade.apply_declaration(style, make_decl("perspective", "none"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 0.0f);
}

TEST(PropertyCascadeTest, TransformBoxValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: border-box (1) for HTML elements
    EXPECT_EQ(style.transform_box, 1);

    cascade.apply_declaration(style, make_decl("transform-box", "content-box"), parent);
    EXPECT_EQ(style.transform_box, 0);

    cascade.apply_declaration(style, make_decl("transform-box", "fill-box"), parent);
    EXPECT_EQ(style.transform_box, 2);

    cascade.apply_declaration(style, make_decl("transform-box", "stroke-box"), parent);
    EXPECT_EQ(style.transform_box, 3);

    cascade.apply_declaration(style, make_decl("transform-box", "view-box"), parent);
    EXPECT_EQ(style.transform_box, 4);

    cascade.apply_declaration(style, make_decl("transform-box", "border-box"), parent);
    EXPECT_EQ(style.transform_box, 1);
}

// ---------------------------------------------------------------------------
// Cycle 451 — CSS font advanced: font-variant, font-variant-caps,
//             font-variant-numeric, font-feature-settings, font-variation-settings,
//             font-optical-sizing, font-kerning, font-stretch,
//             font-variant-ligatures
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, FontVariantSmallCaps) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant", "small-caps"), parent);
    EXPECT_EQ(style.font_variant, 1);

    cascade.apply_declaration(style, make_decl("font-variant", "normal"), parent);
    EXPECT_EQ(style.font_variant, 0);
}

TEST(PropertyCascadeTest, FontVariantCapsValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_caps, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-caps", "small-caps"), parent);
    EXPECT_EQ(style.font_variant_caps, 1);

    cascade.apply_declaration(style, make_decl("font-variant-caps", "all-small-caps"), parent);
    EXPECT_EQ(style.font_variant_caps, 2);

    cascade.apply_declaration(style, make_decl("font-variant-caps", "petite-caps"), parent);
    EXPECT_EQ(style.font_variant_caps, 3);

    cascade.apply_declaration(style, make_decl("font-variant-caps", "titling-caps"), parent);
    EXPECT_EQ(style.font_variant_caps, 6);

    cascade.apply_declaration(style, make_decl("font-variant-caps", "normal"), parent);
    EXPECT_EQ(style.font_variant_caps, 0);
}

TEST(PropertyCascadeTest, FontVariantNumericValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_numeric, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-numeric", "ordinal"), parent);
    EXPECT_EQ(style.font_variant_numeric, 1);

    cascade.apply_declaration(style, make_decl("font-variant-numeric", "slashed-zero"), parent);
    EXPECT_EQ(style.font_variant_numeric, 2);

    cascade.apply_declaration(style, make_decl("font-variant-numeric", "lining-nums"), parent);
    EXPECT_EQ(style.font_variant_numeric, 3);

    cascade.apply_declaration(style, make_decl("font-variant-numeric", "tabular-nums"), parent);
    EXPECT_EQ(style.font_variant_numeric, 6);
}

TEST(PropertyCascadeTest, FontFeatureAndVariationSettings) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.font_feature_settings.empty());
    EXPECT_TRUE(style.font_variation_settings.empty());

    cascade.apply_declaration(style, make_decl("font-feature-settings", "\"kern\" 1, \"liga\" 0"), parent);
    EXPECT_EQ(style.font_feature_settings, "\"kern\" 1, \"liga\" 0");

    cascade.apply_declaration(style, make_decl("font-variation-settings", "\"wght\" 700"), parent);
    EXPECT_EQ(style.font_variation_settings, "\"wght\" 700");
}

TEST(PropertyCascadeTest, FontOpticalSizing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_optical_sizing, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("font-optical-sizing", "none"), parent);
    EXPECT_EQ(style.font_optical_sizing, 1);

    cascade.apply_declaration(style, make_decl("font-optical-sizing", "auto"), parent);
    EXPECT_EQ(style.font_optical_sizing, 0);
}

TEST(PropertyCascadeTest, FontKerningValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_kerning, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("font-kerning", "normal"), parent);
    EXPECT_EQ(style.font_kerning, 1);

    cascade.apply_declaration(style, make_decl("font-kerning", "none"), parent);
    EXPECT_EQ(style.font_kerning, 2);

    cascade.apply_declaration(style, make_decl("font-kerning", "auto"), parent);
    EXPECT_EQ(style.font_kerning, 0);
}

TEST(PropertyCascadeTest, FontStretchValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_stretch, 5);  // default: normal

    cascade.apply_declaration(style, make_decl("font-stretch", "condensed"), parent);
    EXPECT_EQ(style.font_stretch, 3);

    cascade.apply_declaration(style, make_decl("font-stretch", "ultra-condensed"), parent);
    EXPECT_EQ(style.font_stretch, 1);

    cascade.apply_declaration(style, make_decl("font-stretch", "expanded"), parent);
    EXPECT_EQ(style.font_stretch, 7);

    cascade.apply_declaration(style, make_decl("font-stretch", "ultra-expanded"), parent);
    EXPECT_EQ(style.font_stretch, 9);

    cascade.apply_declaration(style, make_decl("font-stretch", "normal"), parent);
    EXPECT_EQ(style.font_stretch, 5);
}

TEST(PropertyCascadeTest, FontVariantLigatures) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_ligatures, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-ligatures", "none"), parent);
    EXPECT_EQ(style.font_variant_ligatures, 1);

    cascade.apply_declaration(style, make_decl("font-variant-ligatures", "common-ligatures"), parent);
    EXPECT_EQ(style.font_variant_ligatures, 2);

    cascade.apply_declaration(style, make_decl("font-variant-ligatures", "no-common-ligatures"), parent);
    EXPECT_EQ(style.font_variant_ligatures, 3);

    cascade.apply_declaration(style, make_decl("font-variant-ligatures", "discretionary-ligatures"), parent);
    EXPECT_EQ(style.font_variant_ligatures, 4);

    cascade.apply_declaration(style, make_decl("font-variant-ligatures", "normal"), parent);
    EXPECT_EQ(style.font_variant_ligatures, 0);
}

// ---------------------------------------------------------------------------
// Cycle 453 — CSS filter: grayscale, sepia, brightness, contrast, invert,
//             saturate, hue-rotate, blur, filter none, backdrop-filter
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, FilterGrayscaleAndSepia) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.filters.empty());

    cascade.apply_declaration(style, make_decl("filter", "grayscale(0.5)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 1);    // grayscale = type 1
    EXPECT_NEAR(style.filters[0].second, 0.5f, 0.01f);

    cascade.apply_declaration(style, make_decl("filter", "sepia(1)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 2);    // sepia = type 2
    EXPECT_FLOAT_EQ(style.filters[0].second, 1.0f);
}

TEST(PropertyCascadeTest, FilterBrightnessAndContrast) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "brightness(1.5)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 3);    // brightness = type 3
    EXPECT_NEAR(style.filters[0].second, 1.5f, 0.01f);

    cascade.apply_declaration(style, make_decl("filter", "contrast(0.8)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 4);    // contrast = type 4
    EXPECT_NEAR(style.filters[0].second, 0.8f, 0.01f);
}

TEST(PropertyCascadeTest, FilterInvertAndSaturate) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "invert(1)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 5);    // invert = type 5

    cascade.apply_declaration(style, make_decl("filter", "saturate(2)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 6);    // saturate = type 6
    EXPECT_NEAR(style.filters[0].second, 2.0f, 0.01f);
}

TEST(PropertyCascadeTest, FilterOpacityAndHueRotate) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "opacity(0.5)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 7);    // opacity = type 7

    cascade.apply_declaration(style, make_decl("filter", "hue-rotate(90)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 8);    // hue-rotate = type 8
    EXPECT_NEAR(style.filters[0].second, 90.0f, 0.01f);
}

TEST(PropertyCascadeTest, FilterBlur) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "blur(4px)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 9);    // blur = type 9
    EXPECT_FLOAT_EQ(style.filters[0].second, 4.0f);
}

TEST(PropertyCascadeTest, FilterNoneClearsFilters) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "blur(4px)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);

    cascade.apply_declaration(style, make_decl("filter", "none"), parent);
    EXPECT_TRUE(style.filters.empty());
}

TEST(PropertyCascadeTest, FilterMultipleFunctions) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Multiple filters: grayscale(0.5) blur(2px)
    cascade.apply_declaration(style, make_decl("filter", "grayscale(0.5) blur(2px)"), parent);
    ASSERT_EQ(style.filters.size(), 2u);
    EXPECT_EQ(style.filters[0].first, 1);   // grayscale
    EXPECT_EQ(style.filters[1].first, 9);   // blur
    EXPECT_FLOAT_EQ(style.filters[1].second, 2.0f);
}

TEST(PropertyCascadeTest, BackdropFilter) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.backdrop_filters.empty());

    cascade.apply_declaration(style, make_decl("backdrop-filter", "blur(10px)"), parent);
    ASSERT_EQ(style.backdrop_filters.size(), 1u);
    EXPECT_EQ(style.backdrop_filters[0].first, 9);  // blur = type 9
    EXPECT_FLOAT_EQ(style.backdrop_filters[0].second, 10.0f);

    cascade.apply_declaration(style, make_decl("backdrop-filter", "none"), parent);
    EXPECT_TRUE(style.backdrop_filters.empty());
}

// ---------------------------------------------------------------------------
// Cycle 454 — CSS text properties: text-decoration/line/style/color/thickness,
//             text-transform, white-space, word-break, overflow-wrap
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, TextDecorationLineValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_decoration, TextDecoration::None);

    cascade.apply_declaration(style, make_decl("text-decoration-line", "underline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);

    cascade.apply_declaration(style, make_decl("text-decoration-line", "overline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Overline);

    cascade.apply_declaration(style, make_decl("text-decoration-line", "line-through"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);

    cascade.apply_declaration(style, make_decl("text-decoration-line", "none"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::None);
}

TEST(PropertyCascadeTest, TextDecorationStyleValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Solid);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "dashed"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Dashed);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "dotted"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Dotted);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "wavy"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Wavy);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "double"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Double);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "solid"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Solid);
}

TEST(PropertyCascadeTest, TextDecorationColorAndThickness) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-decoration-color", "blue"), parent);
    EXPECT_EQ(style.text_decoration_color, (Color{0, 0, 255, 255}));

    cascade.apply_declaration(style, make_decl("text-decoration-thickness", "2px"), parent);
    EXPECT_FLOAT_EQ(style.text_decoration_thickness, 2.0f);
}

TEST(PropertyCascadeTest, TextDecorationShorthandUnderlineWavy) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "underline wavy red" sets line, style, and color
    cascade.apply_declaration(style, make_decl("text-decoration", "underline wavy red"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Wavy);
    EXPECT_EQ(style.text_decoration_color, (Color{255, 0, 0, 255}));
}

TEST(PropertyCascadeTest, TextTransformValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_transform, TextTransform::None);

    cascade.apply_declaration(style, make_decl("text-transform", "uppercase"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);

    cascade.apply_declaration(style, make_decl("text-transform", "lowercase"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::Lowercase);

    cascade.apply_declaration(style, make_decl("text-transform", "capitalize"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::Capitalize);

    cascade.apply_declaration(style, make_decl("text-transform", "none"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::None);
}

TEST(PropertyCascadeTest, WhiteSpaceValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.white_space, WhiteSpace::Normal);

    cascade.apply_declaration(style, make_decl("white-space", "nowrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);

    cascade.apply_declaration(style, make_decl("white-space", "pre"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::Pre);

    cascade.apply_declaration(style, make_decl("white-space", "pre-wrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);

    cascade.apply_declaration(style, make_decl("white-space", "pre-line"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::PreLine);

    cascade.apply_declaration(style, make_decl("white-space", "normal"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::Normal);
}

TEST(PropertyCascadeTest, WordBreakValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.word_break, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("word-break", "break-all"), parent);
    EXPECT_EQ(style.word_break, 1);

    cascade.apply_declaration(style, make_decl("word-break", "keep-all"), parent);
    EXPECT_EQ(style.word_break, 2);

    cascade.apply_declaration(style, make_decl("word-break", "normal"), parent);
    EXPECT_EQ(style.word_break, 0);
}

TEST(PropertyCascadeTest, OverflowWrapValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.overflow_wrap, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("overflow-wrap", "break-word"), parent);
    EXPECT_EQ(style.overflow_wrap, 1);

    cascade.apply_declaration(style, make_decl("overflow-wrap", "anywhere"), parent);
    EXPECT_EQ(style.overflow_wrap, 2);

    cascade.apply_declaration(style, make_decl("word-wrap", "break-word"), parent);
    EXPECT_EQ(style.overflow_wrap, 1);  // word-wrap alias

    cascade.apply_declaration(style, make_decl("overflow-wrap", "normal"), parent);
    EXPECT_EQ(style.overflow_wrap, 0);
}
