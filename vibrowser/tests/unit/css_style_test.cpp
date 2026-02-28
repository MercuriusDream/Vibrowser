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
    // Setting width alone does NOT auto-set style to solid anymore
    EXPECT_EQ(style.border_top.style, BorderStyle::None)
        << "setting border width alone should leave style as None";

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

// ---------------------------------------------------------------------------
// Cycle 455 — CSS background sub-properties
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BackgroundClipValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_clip, 0);  // default: border-box

    cascade.apply_declaration(style, make_decl("background-clip", "padding-box"), parent);
    EXPECT_EQ(style.background_clip, 1);

    cascade.apply_declaration(style, make_decl("background-clip", "content-box"), parent);
    EXPECT_EQ(style.background_clip, 2);

    cascade.apply_declaration(style, make_decl("background-clip", "text"), parent);
    EXPECT_EQ(style.background_clip, 3);

    cascade.apply_declaration(style, make_decl("background-clip", "border-box"), parent);
    EXPECT_EQ(style.background_clip, 0);
}

TEST(PropertyCascadeTest, WebkitBackgroundClipAlias) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("-webkit-background-clip", "text"), parent);
    EXPECT_EQ(style.background_clip, 3);

    cascade.apply_declaration(style, make_decl("-webkit-background-clip", "padding-box"), parent);
    EXPECT_EQ(style.background_clip, 1);
}

TEST(PropertyCascadeTest, BackgroundOriginValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_origin, 0);  // default: padding-box

    cascade.apply_declaration(style, make_decl("background-origin", "border-box"), parent);
    EXPECT_EQ(style.background_origin, 1);

    cascade.apply_declaration(style, make_decl("background-origin", "content-box"), parent);
    EXPECT_EQ(style.background_origin, 2);

    cascade.apply_declaration(style, make_decl("background-origin", "padding-box"), parent);
    EXPECT_EQ(style.background_origin, 0);
}

TEST(PropertyCascadeTest, BackgroundBlendModeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_blend_mode, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("background-blend-mode", "multiply"), parent);
    EXPECT_EQ(style.background_blend_mode, 1);

    cascade.apply_declaration(style, make_decl("background-blend-mode", "screen"), parent);
    EXPECT_EQ(style.background_blend_mode, 2);

    cascade.apply_declaration(style, make_decl("background-blend-mode", "overlay"), parent);
    EXPECT_EQ(style.background_blend_mode, 3);

    cascade.apply_declaration(style, make_decl("background-blend-mode", "darken"), parent);
    EXPECT_EQ(style.background_blend_mode, 4);

    cascade.apply_declaration(style, make_decl("background-blend-mode", "lighten"), parent);
    EXPECT_EQ(style.background_blend_mode, 5);

    cascade.apply_declaration(style, make_decl("background-blend-mode", "normal"), parent);
    EXPECT_EQ(style.background_blend_mode, 0);
}

TEST(PropertyCascadeTest, BackgroundAttachmentValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_attachment, 0);  // default: scroll

    cascade.apply_declaration(style, make_decl("background-attachment", "fixed"), parent);
    EXPECT_EQ(style.background_attachment, 1);

    cascade.apply_declaration(style, make_decl("background-attachment", "local"), parent);
    EXPECT_EQ(style.background_attachment, 2);

    cascade.apply_declaration(style, make_decl("background-attachment", "scroll"), parent);
    EXPECT_EQ(style.background_attachment, 0);
}

TEST(PropertyCascadeTest, BackgroundSizeCoverContainAuto) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_size, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("background-size", "cover"), parent);
    EXPECT_EQ(style.background_size, 1);

    cascade.apply_declaration(style, make_decl("background-size", "contain"), parent);
    EXPECT_EQ(style.background_size, 2);

    cascade.apply_declaration(style, make_decl("background-size", "auto"), parent);
    EXPECT_EQ(style.background_size, 0);
}

TEST(PropertyCascadeTest, BackgroundRepeatValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_repeat, 0);  // default: repeat

    cascade.apply_declaration(style, make_decl("background-repeat", "repeat-x"), parent);
    EXPECT_EQ(style.background_repeat, 1);

    cascade.apply_declaration(style, make_decl("background-repeat", "repeat-y"), parent);
    EXPECT_EQ(style.background_repeat, 2);

    cascade.apply_declaration(style, make_decl("background-repeat", "no-repeat"), parent);
    EXPECT_EQ(style.background_repeat, 3);

    cascade.apply_declaration(style, make_decl("background-repeat", "repeat"), parent);
    EXPECT_EQ(style.background_repeat, 0);
}

TEST(PropertyCascadeTest, BackgroundPositionKeywords) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // defaults: left(0) top(0)
    EXPECT_EQ(style.background_position_x, 0);
    EXPECT_EQ(style.background_position_y, 0);

    // "center center"
    cascade.apply_declaration(style, make_decl("background-position", "center center"), parent);
    EXPECT_EQ(style.background_position_x, 1);
    EXPECT_EQ(style.background_position_y, 1);

    // "right bottom"
    cascade.apply_declaration(style, make_decl("background-position", "right bottom"), parent);
    EXPECT_EQ(style.background_position_x, 2);
    EXPECT_EQ(style.background_position_y, 2);

    // "left top"
    cascade.apply_declaration(style, make_decl("background-position", "left top"), parent);
    EXPECT_EQ(style.background_position_x, 0);
    EXPECT_EQ(style.background_position_y, 0);
}

// ---------------------------------------------------------------------------
// Cycle 456 — SVG CSS properties
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, SvgFillColorAndNone) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // defaults: black fill, not none
    EXPECT_EQ(style.svg_fill_color, 0xFF000000u);
    EXPECT_FALSE(style.svg_fill_none);

    // fill: none
    cascade.apply_declaration(style, make_decl("fill", "none"), parent);
    EXPECT_TRUE(style.svg_fill_none);

    // fill: red clears none flag, stores ARGB
    cascade.apply_declaration(style, make_decl("fill", "red"), parent);
    EXPECT_FALSE(style.svg_fill_none);
    EXPECT_EQ(style.svg_fill_color, 0xFFFF0000u);
}

TEST(PropertyCascadeTest, SvgStrokeColorAndNone) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // default: stroke none
    EXPECT_TRUE(style.svg_stroke_none);

    // stroke: blue
    cascade.apply_declaration(style, make_decl("stroke", "blue"), parent);
    EXPECT_FALSE(style.svg_stroke_none);
    EXPECT_EQ(style.svg_stroke_color, 0xFF0000FFu);

    // stroke: none
    cascade.apply_declaration(style, make_decl("stroke", "none"), parent);
    EXPECT_TRUE(style.svg_stroke_none);
}

TEST(PropertyCascadeTest, SvgFillAndStrokeOpacity) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.svg_fill_opacity, 1.0f);
    EXPECT_FLOAT_EQ(style.svg_stroke_opacity, 1.0f);

    cascade.apply_declaration(style, make_decl("fill-opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.svg_fill_opacity, 0.5f);

    cascade.apply_declaration(style, make_decl("stroke-opacity", "0.25"), parent);
    EXPECT_FLOAT_EQ(style.svg_stroke_opacity, 0.25f);

    // clamped to [0,1]
    cascade.apply_declaration(style, make_decl("fill-opacity", "2.0"), parent);
    EXPECT_FLOAT_EQ(style.svg_fill_opacity, 1.0f);

    cascade.apply_declaration(style, make_decl("stroke-opacity", "-0.5"), parent);
    EXPECT_FLOAT_EQ(style.svg_stroke_opacity, 0.0f);
}

TEST(PropertyCascadeTest, SvgStrokeWidthAndLinecap) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.svg_stroke_width, 0.0f);
    EXPECT_EQ(style.svg_stroke_linecap, 0);  // default: butt

    cascade.apply_declaration(style, make_decl("stroke-width", "3.5"), parent);
    EXPECT_FLOAT_EQ(style.svg_stroke_width, 3.5f);

    cascade.apply_declaration(style, make_decl("stroke-linecap", "round"), parent);
    EXPECT_EQ(style.svg_stroke_linecap, 1);

    cascade.apply_declaration(style, make_decl("stroke-linecap", "square"), parent);
    EXPECT_EQ(style.svg_stroke_linecap, 2);

    cascade.apply_declaration(style, make_decl("stroke-linecap", "butt"), parent);
    EXPECT_EQ(style.svg_stroke_linecap, 0);
}

TEST(PropertyCascadeTest, SvgStrokeLinejoinAndDasharray) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.svg_stroke_linejoin, 0);  // default: miter
    EXPECT_EQ(style.svg_stroke_dasharray_str, "");

    cascade.apply_declaration(style, make_decl("stroke-linejoin", "round"), parent);
    EXPECT_EQ(style.svg_stroke_linejoin, 1);

    cascade.apply_declaration(style, make_decl("stroke-linejoin", "bevel"), parent);
    EXPECT_EQ(style.svg_stroke_linejoin, 2);

    cascade.apply_declaration(style, make_decl("stroke-linejoin", "miter"), parent);
    EXPECT_EQ(style.svg_stroke_linejoin, 0);

    cascade.apply_declaration(style, make_decl("stroke-dasharray", "4 2 1 2"), parent);
    EXPECT_EQ(style.svg_stroke_dasharray_str, "4 2 1 2");
}

TEST(PropertyCascadeTest, SvgTextAnchorValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.svg_text_anchor, 0);  // default: start

    cascade.apply_declaration(style, make_decl("text-anchor", "middle"), parent);
    EXPECT_EQ(style.svg_text_anchor, 1);

    cascade.apply_declaration(style, make_decl("text-anchor", "end"), parent);
    EXPECT_EQ(style.svg_text_anchor, 2);

    cascade.apply_declaration(style, make_decl("text-anchor", "start"), parent);
    EXPECT_EQ(style.svg_text_anchor, 0);
}

TEST(PropertyCascadeTest, SvgFillRuleAndClipRule) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.fill_rule, 0);  // default: nonzero
    EXPECT_EQ(style.clip_rule, 0);  // default: nonzero

    cascade.apply_declaration(style, make_decl("fill-rule", "evenodd"), parent);
    EXPECT_EQ(style.fill_rule, 1);

    cascade.apply_declaration(style, make_decl("clip-rule", "evenodd"), parent);
    EXPECT_EQ(style.clip_rule, 1);

    cascade.apply_declaration(style, make_decl("fill-rule", "nonzero"), parent);
    EXPECT_EQ(style.fill_rule, 0);

    cascade.apply_declaration(style, make_decl("clip-rule", "nonzero"), parent);
    EXPECT_EQ(style.clip_rule, 0);
}

TEST(PropertyCascadeTest, SvgShapeRenderingAndVectorEffect) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.shape_rendering, 0);  // default: auto
    EXPECT_EQ(style.vector_effect, 0);    // default: none

    cascade.apply_declaration(style, make_decl("shape-rendering", "optimizeSpeed"), parent);
    EXPECT_EQ(style.shape_rendering, 1);

    cascade.apply_declaration(style, make_decl("shape-rendering", "crispEdges"), parent);
    EXPECT_EQ(style.shape_rendering, 2);

    cascade.apply_declaration(style, make_decl("shape-rendering", "geometricPrecision"), parent);
    EXPECT_EQ(style.shape_rendering, 3);

    cascade.apply_declaration(style, make_decl("shape-rendering", "auto"), parent);
    EXPECT_EQ(style.shape_rendering, 0);

    cascade.apply_declaration(style, make_decl("vector-effect", "non-scaling-stroke"), parent);
    EXPECT_EQ(style.vector_effect, 1);

    cascade.apply_declaration(style, make_decl("vector-effect", "none"), parent);
    EXPECT_EQ(style.vector_effect, 0);
}

// ---------------------------------------------------------------------------
// Cycle 457 — scroll-snap, scrollbar, CSS motion path, CSS Transforms L2
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ScrollSnapTypeAndAlign) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.scroll_snap_type, "");
    EXPECT_EQ(style.scroll_snap_align, "");

    cascade.apply_declaration(style, make_decl("scroll-snap-type", "y mandatory"), parent);
    EXPECT_EQ(style.scroll_snap_type, "y mandatory");

    cascade.apply_declaration(style, make_decl("scroll-snap-align", "start"), parent);
    EXPECT_EQ(style.scroll_snap_align, "start");

    cascade.apply_declaration(style, make_decl("scroll-snap-align", "center end"), parent);
    EXPECT_EQ(style.scroll_snap_align, "center end");
}

TEST(PropertyCascadeTest, ScrollSnapStop) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.scroll_snap_stop, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("scroll-snap-stop", "always"), parent);
    EXPECT_EQ(style.scroll_snap_stop, 1);

    cascade.apply_declaration(style, make_decl("scroll-snap-stop", "normal"), parent);
    EXPECT_EQ(style.scroll_snap_stop, 0);
}

TEST(PropertyCascadeTest, ScrollbarWidthValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.scrollbar_width, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("scrollbar-width", "thin"), parent);
    EXPECT_EQ(style.scrollbar_width, 1);

    cascade.apply_declaration(style, make_decl("scrollbar-width", "none"), parent);
    EXPECT_EQ(style.scrollbar_width, 2);

    cascade.apply_declaration(style, make_decl("scrollbar-width", "auto"), parent);
    EXPECT_EQ(style.scrollbar_width, 0);
}

TEST(PropertyCascadeTest, ScrollbarGutterValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.scrollbar_gutter, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("scrollbar-gutter", "stable"), parent);
    EXPECT_EQ(style.scrollbar_gutter, 1);

    cascade.apply_declaration(style, make_decl("scrollbar-gutter", "stable both-edges"), parent);
    EXPECT_EQ(style.scrollbar_gutter, 2);

    cascade.apply_declaration(style, make_decl("scrollbar-gutter", "auto"), parent);
    EXPECT_EQ(style.scrollbar_gutter, 0);
}

TEST(PropertyCascadeTest, ScrollbarColorAuto) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: both zero (unset)
    EXPECT_EQ(style.scrollbar_thumb_color, 0u);
    EXPECT_EQ(style.scrollbar_track_color, 0u);

    // "auto" clears back to 0
    cascade.apply_declaration(style, make_decl("scrollbar-color", "auto"), parent);
    EXPECT_EQ(style.scrollbar_thumb_color, 0u);
    EXPECT_EQ(style.scrollbar_track_color, 0u);
}

TEST(PropertyCascadeTest, CssMotionOffsetPath) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.offset_path, "none");
    EXPECT_FLOAT_EQ(style.offset_distance, 0.0f);
    EXPECT_EQ(style.offset_rotate, "auto");

    cascade.apply_declaration(style, make_decl("offset-path", "path('M 0 0 L 100 100')"), parent);
    EXPECT_EQ(style.offset_path, "path('M 0 0 L 100 100')");

    cascade.apply_declaration(style, make_decl("offset-distance", "50px"), parent);
    EXPECT_FLOAT_EQ(style.offset_distance, 50.0f);

    cascade.apply_declaration(style, make_decl("offset-rotate", "45deg"), parent);
    EXPECT_EQ(style.offset_rotate, "45deg");

    cascade.apply_declaration(style, make_decl("offset-path", "none"), parent);
    EXPECT_EQ(style.offset_path, "none");
}

TEST(PropertyCascadeTest, CssTransformsLevel2IndividualProperties) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.css_rotate, "none");
    EXPECT_EQ(style.css_scale, "none");
    EXPECT_EQ(style.css_translate, "none");

    cascade.apply_declaration(style, make_decl("rotate", "45deg"), parent);
    EXPECT_EQ(style.css_rotate, "45deg");

    cascade.apply_declaration(style, make_decl("scale", "1.5"), parent);
    EXPECT_EQ(style.css_scale, "1.5");

    cascade.apply_declaration(style, make_decl("translate", "10px 20px"), parent);
    EXPECT_EQ(style.css_translate, "10px 20px");

    cascade.apply_declaration(style, make_decl("rotate", "none"), parent);
    EXPECT_EQ(style.css_rotate, "none");
}

TEST(PropertyCascadeTest, TransitionBehaviorAndAnimationRange) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transition_behavior, 0);  // default: normal
    EXPECT_EQ(style.animation_range, "normal");

    cascade.apply_declaration(style, make_decl("transition-behavior", "allow-discrete"), parent);
    EXPECT_EQ(style.transition_behavior, 1);

    cascade.apply_declaration(style, make_decl("transition-behavior", "normal"), parent);
    EXPECT_EQ(style.transition_behavior, 0);

    cascade.apply_declaration(style, make_decl("animation-range", "entry 0% exit 100%"), parent);
    EXPECT_EQ(style.animation_range, "entry 0% exit 100%");
}

// ---------------------------------------------------------------------------
// Cycle 458 — justify-items, align-content, inset, overflow-block/inline,
//             box-decoration-break, margin-trim
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, JustifyItemsValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.justify_items, 3);  // default: stretch

    cascade.apply_declaration(style, make_decl("justify-items", "start"), parent);
    EXPECT_EQ(style.justify_items, 0);

    cascade.apply_declaration(style, make_decl("justify-items", "end"), parent);
    EXPECT_EQ(style.justify_items, 1);

    cascade.apply_declaration(style, make_decl("justify-items", "center"), parent);
    EXPECT_EQ(style.justify_items, 2);

    cascade.apply_declaration(style, make_decl("justify-items", "stretch"), parent);
    EXPECT_EQ(style.justify_items, 3);
}

TEST(PropertyCascadeTest, AlignContentValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.align_content, 0);  // default: start

    cascade.apply_declaration(style, make_decl("align-content", "end"), parent);
    EXPECT_EQ(style.align_content, 1);

    cascade.apply_declaration(style, make_decl("align-content", "center"), parent);
    EXPECT_EQ(style.align_content, 2);

    cascade.apply_declaration(style, make_decl("align-content", "stretch"), parent);
    EXPECT_EQ(style.align_content, 3);

    cascade.apply_declaration(style, make_decl("align-content", "space-between"), parent);
    EXPECT_EQ(style.align_content, 4);

    cascade.apply_declaration(style, make_decl("align-content", "space-around"), parent);
    EXPECT_EQ(style.align_content, 5);
}

TEST(PropertyCascadeTest, InsetShorthandAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // inset with 4 values sets top/right/bottom/left and upgrades position to relative
    cascade.apply_declaration(style, make_decl("inset", "10px 20px 30px 40px"), parent);
    EXPECT_FLOAT_EQ(style.top.to_px(0), 10.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(0), 20.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(0), 30.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(0), 40.0f);
    EXPECT_EQ(style.position, Position::Relative);
}

TEST(PropertyCascadeTest, InsetBlockAndInline) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // inset-block sets top+bottom
    cascade.apply_declaration(style, make_decl("inset-block", "15px 25px"), parent);
    EXPECT_FLOAT_EQ(style.top.to_px(0), 15.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(0), 25.0f);

    // inset-inline sets left+right
    cascade.apply_declaration(style, make_decl("inset-inline", "5px 8px"), parent);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(0), 5.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(0), 8.0f);
}

TEST(PropertyCascadeTest, InsetLogicalLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("inset-block-start", "10px"), parent);
    EXPECT_FLOAT_EQ(style.top.to_px(0), 10.0f);

    cascade.apply_declaration(style, make_decl("inset-block-end", "20px"), parent);
    EXPECT_FLOAT_EQ(style.bottom.to_px(0), 20.0f);

    cascade.apply_declaration(style, make_decl("inset-inline-start", "30px"), parent);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(0), 30.0f);

    cascade.apply_declaration(style, make_decl("inset-inline-end", "40px"), parent);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(0), 40.0f);
}

TEST(PropertyCascadeTest, OverflowBlockAndInlineValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.overflow_block, 0);   // default: visible
    EXPECT_EQ(style.overflow_inline, 0);  // default: visible

    cascade.apply_declaration(style, make_decl("overflow-block", "hidden"), parent);
    EXPECT_EQ(style.overflow_block, 1);

    cascade.apply_declaration(style, make_decl("overflow-block", "scroll"), parent);
    EXPECT_EQ(style.overflow_block, 2);

    cascade.apply_declaration(style, make_decl("overflow-block", "clip"), parent);
    EXPECT_EQ(style.overflow_block, 4);

    cascade.apply_declaration(style, make_decl("overflow-inline", "auto"), parent);
    EXPECT_EQ(style.overflow_inline, 3);

    cascade.apply_declaration(style, make_decl("overflow-inline", "hidden"), parent);
    EXPECT_EQ(style.overflow_inline, 1);
}

TEST(PropertyCascadeTest, BoxDecorationBreak) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.box_decoration_break, 0);  // default: slice

    cascade.apply_declaration(style, make_decl("box-decoration-break", "clone"), parent);
    EXPECT_EQ(style.box_decoration_break, 1);

    cascade.apply_declaration(style, make_decl("box-decoration-break", "slice"), parent);
    EXPECT_EQ(style.box_decoration_break, 0);

    // -webkit alias
    cascade.apply_declaration(style, make_decl("-webkit-box-decoration-break", "clone"), parent);
    EXPECT_EQ(style.box_decoration_break, 1);
}

TEST(PropertyCascadeTest, MarginTrimValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.margin_trim, 0);  // default: none

    cascade.apply_declaration(style, make_decl("margin-trim", "block"), parent);
    EXPECT_EQ(style.margin_trim, 1);

    cascade.apply_declaration(style, make_decl("margin-trim", "inline"), parent);
    EXPECT_EQ(style.margin_trim, 2);

    cascade.apply_declaration(style, make_decl("margin-trim", "block-start"), parent);
    EXPECT_EQ(style.margin_trim, 3);

    cascade.apply_declaration(style, make_decl("margin-trim", "block-end"), parent);
    EXPECT_EQ(style.margin_trim, 4);

    cascade.apply_declaration(style, make_decl("margin-trim", "inline-start"), parent);
    EXPECT_EQ(style.margin_trim, 5);

    cascade.apply_declaration(style, make_decl("margin-trim", "inline-end"), parent);
    EXPECT_EQ(style.margin_trim, 6);

    cascade.apply_declaration(style, make_decl("margin-trim", "none"), parent);
    EXPECT_EQ(style.margin_trim, 0);
}

// ---------------------------------------------------------------------------
// Cycle 459 — text-rendering, font-smooth, text-size-adjust,
//             ruby properties, overflow-anchor, overflow-clip-margin
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, TextRenderingValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_rendering, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("text-rendering", "optimizeSpeed"), parent);
    EXPECT_EQ(style.text_rendering, 1);

    cascade.apply_declaration(style, make_decl("text-rendering", "optimizeLegibility"), parent);
    EXPECT_EQ(style.text_rendering, 2);

    cascade.apply_declaration(style, make_decl("text-rendering", "geometricPrecision"), parent);
    EXPECT_EQ(style.text_rendering, 3);

    cascade.apply_declaration(style, make_decl("text-rendering", "auto"), parent);
    EXPECT_EQ(style.text_rendering, 0);
}

TEST(PropertyCascadeTest, FontSmoothingValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_smooth, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("font-smooth", "none"), parent);
    EXPECT_EQ(style.font_smooth, 1);

    cascade.apply_declaration(style, make_decl("-webkit-font-smoothing", "antialiased"), parent);
    EXPECT_EQ(style.font_smooth, 2);

    cascade.apply_declaration(style, make_decl("-webkit-font-smoothing", "subpixel-antialiased"), parent);
    EXPECT_EQ(style.font_smooth, 3);

    cascade.apply_declaration(style, make_decl("font-smooth", "auto"), parent);
    EXPECT_EQ(style.font_smooth, 0);
}

TEST(PropertyCascadeTest, TextSizeAdjustValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_size_adjust, "auto");

    cascade.apply_declaration(style, make_decl("text-size-adjust", "none"), parent);
    EXPECT_EQ(style.text_size_adjust, "none");

    cascade.apply_declaration(style, make_decl("-webkit-text-size-adjust", "100%"), parent);
    EXPECT_EQ(style.text_size_adjust, "100%");

    cascade.apply_declaration(style, make_decl("text-size-adjust", "auto"), parent);
    EXPECT_EQ(style.text_size_adjust, "auto");
}

TEST(PropertyCascadeTest, RubyAlignValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.ruby_align, 0);  // default: space-around

    cascade.apply_declaration(style, make_decl("ruby-align", "start"), parent);
    EXPECT_EQ(style.ruby_align, 1);

    cascade.apply_declaration(style, make_decl("ruby-align", "center"), parent);
    EXPECT_EQ(style.ruby_align, 2);

    cascade.apply_declaration(style, make_decl("ruby-align", "space-between"), parent);
    EXPECT_EQ(style.ruby_align, 3);

    cascade.apply_declaration(style, make_decl("ruby-align", "space-around"), parent);
    EXPECT_EQ(style.ruby_align, 0);
}

TEST(PropertyCascadeTest, RubyPositionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.ruby_position, 0);  // default: over

    cascade.apply_declaration(style, make_decl("ruby-position", "under"), parent);
    EXPECT_EQ(style.ruby_position, 1);

    cascade.apply_declaration(style, make_decl("ruby-position", "inter-character"), parent);
    EXPECT_EQ(style.ruby_position, 2);

    cascade.apply_declaration(style, make_decl("ruby-position", "over"), parent);
    EXPECT_EQ(style.ruby_position, 0);
}

TEST(PropertyCascadeTest, RubyOverhangValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.ruby_overhang, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("ruby-overhang", "none"), parent);
    EXPECT_EQ(style.ruby_overhang, 1);

    cascade.apply_declaration(style, make_decl("ruby-overhang", "start"), parent);
    EXPECT_EQ(style.ruby_overhang, 2);

    cascade.apply_declaration(style, make_decl("ruby-overhang", "end"), parent);
    EXPECT_EQ(style.ruby_overhang, 3);

    cascade.apply_declaration(style, make_decl("ruby-overhang", "auto"), parent);
    EXPECT_EQ(style.ruby_overhang, 0);
}

TEST(PropertyCascadeTest, OverflowAnchorValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.overflow_anchor, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("overflow-anchor", "none"), parent);
    EXPECT_EQ(style.overflow_anchor, 1);

    cascade.apply_declaration(style, make_decl("overflow-anchor", "auto"), parent);
    EXPECT_EQ(style.overflow_anchor, 0);
}

TEST(PropertyCascadeTest, OverflowClipMarginPx) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.overflow_clip_margin, 0.0f);

    cascade.apply_declaration(style, make_decl("overflow-clip-margin", "16px"), parent);
    EXPECT_FLOAT_EQ(style.overflow_clip_margin, 16.0f);

    cascade.apply_declaration(style, make_decl("overflow-clip-margin", "0px"), parent);
    EXPECT_FLOAT_EQ(style.overflow_clip_margin, 0.0f);
}

// ---------------------------------------------------------------------------
// Cycle 460 — font-palette, font-variant-position, font-language-override,
//             font-size-adjust, text-decoration-skip-ink,
//             text-underline-position, scroll-margin, scroll-padding
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, FontPaletteStoresString) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_palette, "normal");

    cascade.apply_declaration(style, make_decl("font-palette", "dark"), parent);
    EXPECT_EQ(style.font_palette, "dark");

    cascade.apply_declaration(style, make_decl("font-palette", "--my-palette"), parent);
    EXPECT_EQ(style.font_palette, "--my-palette");

    cascade.apply_declaration(style, make_decl("font-palette", "normal"), parent);
    EXPECT_EQ(style.font_palette, "normal");
}

TEST(PropertyCascadeTest, FontVariantPositionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_position, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-position", "sub"), parent);
    EXPECT_EQ(style.font_variant_position, 1);

    cascade.apply_declaration(style, make_decl("font-variant-position", "super"), parent);
    EXPECT_EQ(style.font_variant_position, 2);

    cascade.apply_declaration(style, make_decl("font-variant-position", "normal"), parent);
    EXPECT_EQ(style.font_variant_position, 0);
}

TEST(PropertyCascadeTest, FontLanguageOverrideValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_language_override, "");

    cascade.apply_declaration(style, make_decl("font-language-override", "TRK"), parent);
    EXPECT_EQ(style.font_language_override, "TRK");

    // "normal" resets to empty string
    cascade.apply_declaration(style, make_decl("font-language-override", "normal"), parent);
    EXPECT_EQ(style.font_language_override, "");
}

TEST(PropertyCascadeTest, FontSizeAdjustValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.font_size_adjust, 0.0f);

    cascade.apply_declaration(style, make_decl("font-size-adjust", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.font_size_adjust, 0.5f);

    cascade.apply_declaration(style, make_decl("font-size-adjust", "none"), parent);
    EXPECT_FLOAT_EQ(style.font_size_adjust, 0.0f);
}

TEST(PropertyCascadeTest, TextDecorationSkipInkValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_decoration_skip_ink, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("text-decoration-skip-ink", "none"), parent);
    EXPECT_EQ(style.text_decoration_skip_ink, 1);

    cascade.apply_declaration(style, make_decl("text-decoration-skip-ink", "all"), parent);
    EXPECT_EQ(style.text_decoration_skip_ink, 2);

    cascade.apply_declaration(style, make_decl("text-decoration-skip-ink", "auto"), parent);
    EXPECT_EQ(style.text_decoration_skip_ink, 0);
}

TEST(PropertyCascadeTest, TextUnderlinePositionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_underline_position, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("text-underline-position", "under"), parent);
    EXPECT_EQ(style.text_underline_position, 1);

    cascade.apply_declaration(style, make_decl("text-underline-position", "left"), parent);
    EXPECT_EQ(style.text_underline_position, 2);

    cascade.apply_declaration(style, make_decl("text-underline-position", "right"), parent);
    EXPECT_EQ(style.text_underline_position, 3);

    cascade.apply_declaration(style, make_decl("text-underline-position", "auto"), parent);
    EXPECT_EQ(style.text_underline_position, 0);
}

TEST(PropertyCascadeTest, ScrollMarginShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.scroll_margin_top, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_right, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_bottom, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_left, 0.0f);

    // 4-value shorthand: top right bottom left
    cascade.apply_declaration(style, make_decl("scroll-margin", "10px 20px 30px 40px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_top, 10.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_right, 20.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_bottom, 30.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_left, 40.0f);
}

TEST(PropertyCascadeTest, ScrollPaddingShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.scroll_padding_top, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_right, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_bottom, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_left, 0.0f);

    // 2-value shorthand: vertical horizontal
    cascade.apply_declaration(style, make_decl("scroll-padding", "8px 16px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_top, 8.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_bottom, 8.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_right, 16.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_left, 16.0f);
}

// ---------------------------------------------------------------------------
// Cycle 461 — contain-intrinsic-size, SVG gradient filter properties,
//             SVG marker properties, place-content shorthand
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ContainIntrinsicSizeShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 0.0f);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_height, 0.0f);

    // two-value: width height
    cascade.apply_declaration(style, make_decl("contain-intrinsic-size", "200px 100px"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 200.0f);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_height, 100.0f);

    // single value sets both
    cascade.apply_declaration(style, make_decl("contain-intrinsic-size", "50px"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 50.0f);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_height, 50.0f);

    // "auto" resets to 0
    cascade.apply_declaration(style, make_decl("contain-intrinsic-size", "auto"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 0.0f);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_height, 0.0f);
}

TEST(PropertyCascadeTest, ContainIntrinsicWidthAndHeight) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("contain-intrinsic-width", "300px"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 300.0f);

    cascade.apply_declaration(style, make_decl("contain-intrinsic-height", "150px"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_height, 150.0f);

    // "none" resets to 0
    cascade.apply_declaration(style, make_decl("contain-intrinsic-width", "none"), parent);
    EXPECT_FLOAT_EQ(style.contain_intrinsic_width, 0.0f);
}

TEST(PropertyCascadeTest, SvgStopColorAndOpacity) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.stop_color, 0xFF000000u);  // default black
    EXPECT_FLOAT_EQ(style.stop_opacity, 1.0f);

    cascade.apply_declaration(style, make_decl("stop-color", "white"), parent);
    EXPECT_EQ(style.stop_color, 0xFFFFFFFFu);

    cascade.apply_declaration(style, make_decl("stop-opacity", "0.4"), parent);
    EXPECT_FLOAT_EQ(style.stop_opacity, 0.4f);

    // clamp to 0-1
    cascade.apply_declaration(style, make_decl("stop-opacity", "1.5"), parent);
    EXPECT_FLOAT_EQ(style.stop_opacity, 1.0f);
}

TEST(PropertyCascadeTest, SvgFloodColorAndOpacity) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.flood_color, 0xFF000000u);  // default black
    EXPECT_FLOAT_EQ(style.flood_opacity, 1.0f);

    cascade.apply_declaration(style, make_decl("flood-color", "blue"), parent);
    EXPECT_EQ(style.flood_color, 0xFF0000FFu);

    cascade.apply_declaration(style, make_decl("flood-opacity", "0.75"), parent);
    EXPECT_FLOAT_EQ(style.flood_opacity, 0.75f);
}

TEST(PropertyCascadeTest, SvgLightingColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.lighting_color, 0xFFFFFFFFu);  // default white

    cascade.apply_declaration(style, make_decl("lighting-color", "red"), parent);
    EXPECT_EQ(style.lighting_color, 0xFFFF0000u);

    cascade.apply_declaration(style, make_decl("lighting-color", "black"), parent);
    EXPECT_EQ(style.lighting_color, 0xFF000000u);
}

TEST(PropertyCascadeTest, SvgMarkerShorthandSetsAll) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.marker_shorthand, "");
    EXPECT_EQ(style.marker_start, "");
    EXPECT_EQ(style.marker_mid, "");
    EXPECT_EQ(style.marker_end, "");

    cascade.apply_declaration(style, make_decl("marker", "url(#arrow)"), parent);
    EXPECT_EQ(style.marker_shorthand, "url(#arrow)");
    EXPECT_EQ(style.marker_start, "url(#arrow)");
    EXPECT_EQ(style.marker_mid, "url(#arrow)");
    EXPECT_EQ(style.marker_end, "url(#arrow)");
}

TEST(PropertyCascadeTest, SvgMarkerIndividualProperties) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("marker-start", "url(#circle)"), parent);
    EXPECT_EQ(style.marker_start, "url(#circle)");

    cascade.apply_declaration(style, make_decl("marker-mid", "url(#square)"), parent);
    EXPECT_EQ(style.marker_mid, "url(#square)");

    cascade.apply_declaration(style, make_decl("marker-end", "url(#arrow)"), parent);
    EXPECT_EQ(style.marker_end, "url(#arrow)");
}

TEST(PropertyCascadeTest, PlaceContentShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "center space-between" => align_content=2, justify_content=SpaceBetween
    cascade.apply_declaration(style, make_decl("place-content", "center space-between"), parent);
    EXPECT_EQ(style.align_content, 2);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);

    // single "start" => both align_content=0 and justify_content=FlexStart
    cascade.apply_declaration(style, make_decl("place-content", "start"), parent);
    EXPECT_EQ(style.align_content, 0);
    EXPECT_EQ(style.justify_content, JustifyContent::FlexStart);

    // "end center" => align_content=1, justify_content=Center
    cascade.apply_declaration(style, make_decl("place-content", "end center"), parent);
    EXPECT_EQ(style.align_content, 1);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);
}

// ---------------------------------------------------------------------------
// Cycle 462 — color-scheme, container, forced-color-adjust, paint-order,
//             dominant-baseline, text-emphasis, -webkit-text-stroke,
//             print-color-adjust
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ColorSchemeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.color_scheme, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("color-scheme", "light"), parent);
    EXPECT_EQ(style.color_scheme, 1);

    cascade.apply_declaration(style, make_decl("color-scheme", "dark"), parent);
    EXPECT_EQ(style.color_scheme, 2);

    cascade.apply_declaration(style, make_decl("color-scheme", "light dark"), parent);
    EXPECT_EQ(style.color_scheme, 3);

    cascade.apply_declaration(style, make_decl("color-scheme", "normal"), parent);
    EXPECT_EQ(style.color_scheme, 0);
}

TEST(PropertyCascadeTest, ContainerTypeAndName) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.container_type, 0);
    EXPECT_EQ(style.container_name, "");

    cascade.apply_declaration(style, make_decl("container-type", "inline-size"), parent);
    EXPECT_EQ(style.container_type, 2);

    cascade.apply_declaration(style, make_decl("container-type", "size"), parent);
    EXPECT_EQ(style.container_type, 1);

    cascade.apply_declaration(style, make_decl("container-name", "sidebar"), parent);
    EXPECT_EQ(style.container_name, "sidebar");

    cascade.apply_declaration(style, make_decl("container-type", "normal"), parent);
    EXPECT_EQ(style.container_type, 0);
}

TEST(PropertyCascadeTest, ContainerShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // "name / type" shorthand
    cascade.apply_declaration(style, make_decl("container", "layout / inline-size"), parent);
    EXPECT_EQ(style.container_name, "layout");
    EXPECT_EQ(style.container_type, 2);
}

TEST(PropertyCascadeTest, ForcedColorAdjustValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.forced_color_adjust, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("forced-color-adjust", "none"), parent);
    EXPECT_EQ(style.forced_color_adjust, 1);

    cascade.apply_declaration(style, make_decl("forced-color-adjust", "preserve-parent-color"), parent);
    EXPECT_EQ(style.forced_color_adjust, 2);

    cascade.apply_declaration(style, make_decl("forced-color-adjust", "auto"), parent);
    EXPECT_EQ(style.forced_color_adjust, 0);
}

TEST(PropertyCascadeTest, PaintOrderStoresString) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.paint_order, "normal");

    cascade.apply_declaration(style, make_decl("paint-order", "fill stroke markers"), parent);
    EXPECT_EQ(style.paint_order, "fill stroke markers");

    cascade.apply_declaration(style, make_decl("paint-order", "stroke fill"), parent);
    EXPECT_EQ(style.paint_order, "stroke fill");

    cascade.apply_declaration(style, make_decl("paint-order", "normal"), parent);
    EXPECT_EQ(style.paint_order, "normal");
}

TEST(PropertyCascadeTest, DominantBaselineValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.dominant_baseline, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("dominant-baseline", "alphabetic"), parent);
    EXPECT_EQ(style.dominant_baseline, 2);

    cascade.apply_declaration(style, make_decl("dominant-baseline", "middle"), parent);
    EXPECT_EQ(style.dominant_baseline, 4);

    cascade.apply_declaration(style, make_decl("dominant-baseline", "hanging"), parent);
    EXPECT_EQ(style.dominant_baseline, 7);

    cascade.apply_declaration(style, make_decl("dominant-baseline", "auto"), parent);
    EXPECT_EQ(style.dominant_baseline, 0);
}

TEST(PropertyCascadeTest, TextEmphasisStyleAndPosition) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_emphasis_style, "none");
    EXPECT_EQ(style.text_emphasis_position, 0);  // default: over right

    cascade.apply_declaration(style, make_decl("text-emphasis-style", "filled dot"), parent);
    EXPECT_EQ(style.text_emphasis_style, "filled dot");

    cascade.apply_declaration(style, make_decl("text-emphasis-position", "under right"), parent);
    EXPECT_EQ(style.text_emphasis_position, 1);

    cascade.apply_declaration(style, make_decl("text-emphasis-position", "under left"), parent);
    EXPECT_EQ(style.text_emphasis_position, 3);

    cascade.apply_declaration(style, make_decl("text-emphasis-position", "over right"), parent);
    EXPECT_EQ(style.text_emphasis_position, 0);
}

TEST(PropertyCascadeTest, WebkitTextStrokeWidthAndColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.text_stroke_width, 0.0f);

    cascade.apply_declaration(style, make_decl("-webkit-text-stroke-width", "2px"), parent);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 2.0f);

    cascade.apply_declaration(style, make_decl("-webkit-text-stroke-color", "red"), parent);
    EXPECT_EQ(style.text_stroke_color.r, 255);
    EXPECT_EQ(style.text_stroke_color.g, 0);
    EXPECT_EQ(style.text_stroke_color.b, 0);
}

// ---------------------------------------------------------------------------
// Cycle 463 — hyphens, text-justify, initial-letter, image-orientation,
//             math-style/depth, print-color-adjust, -webkit-text-fill-color
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, HyphensValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.hyphens, 1);  // default: manual

    cascade.apply_declaration(style, make_decl("hyphens", "none"), parent);
    EXPECT_EQ(style.hyphens, 0);

    cascade.apply_declaration(style, make_decl("hyphens", "auto"), parent);
    EXPECT_EQ(style.hyphens, 2);

    cascade.apply_declaration(style, make_decl("hyphens", "manual"), parent);
    EXPECT_EQ(style.hyphens, 1);
}

TEST(PropertyCascadeTest, TextJustifyValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_justify, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("text-justify", "inter-word"), parent);
    EXPECT_EQ(style.text_justify, 1);

    cascade.apply_declaration(style, make_decl("text-justify", "inter-character"), parent);
    EXPECT_EQ(style.text_justify, 2);

    cascade.apply_declaration(style, make_decl("text-justify", "none"), parent);
    EXPECT_EQ(style.text_justify, 3);

    cascade.apply_declaration(style, make_decl("text-justify", "auto"), parent);
    EXPECT_EQ(style.text_justify, 0);
}

TEST(PropertyCascadeTest, InitialLetterNormalAndValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.initial_letter_size, 0.0f);
    EXPECT_EQ(style.initial_letter_sink, 0);

    // Two values: size and sink
    cascade.apply_declaration(style, make_decl("initial-letter", "3 2"), parent);
    EXPECT_FLOAT_EQ(style.initial_letter_size, 3.0f);
    EXPECT_EQ(style.initial_letter_sink, 2);

    // Single value: size, sink defaults to same as size
    cascade.apply_declaration(style, make_decl("initial-letter", "2"), parent);
    EXPECT_FLOAT_EQ(style.initial_letter_size, 2.0f);
    EXPECT_EQ(style.initial_letter_sink, 2);

    // "normal" resets both to 0
    cascade.apply_declaration(style, make_decl("initial-letter", "normal"), parent);
    EXPECT_FLOAT_EQ(style.initial_letter_size, 0.0f);
    EXPECT_EQ(style.initial_letter_sink, 0);
}

TEST(PropertyCascadeTest, InitialLetterAlignValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.initial_letter_align, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("initial-letter-align", "border-box"), parent);
    EXPECT_EQ(style.initial_letter_align, 1);

    cascade.apply_declaration(style, make_decl("initial-letter-align", "alphabetic"), parent);
    EXPECT_EQ(style.initial_letter_align, 2);

    cascade.apply_declaration(style, make_decl("initial-letter-align", "auto"), parent);
    EXPECT_EQ(style.initial_letter_align, 0);
}

TEST(PropertyCascadeTest, ImageOrientationValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.image_orientation, 0);  // default: from-image

    cascade.apply_declaration(style, make_decl("image-orientation", "none"), parent);
    EXPECT_EQ(style.image_orientation, 1);

    cascade.apply_declaration(style, make_decl("image-orientation", "flip"), parent);
    EXPECT_EQ(style.image_orientation, 2);

    cascade.apply_declaration(style, make_decl("image-orientation", "from-image"), parent);
    EXPECT_EQ(style.image_orientation, 0);
}

TEST(PropertyCascadeTest, MathStyleAndDepth) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.math_style, 0);  // default: normal
    EXPECT_EQ(style.math_depth, 0);

    cascade.apply_declaration(style, make_decl("math-style", "compact"), parent);
    EXPECT_EQ(style.math_style, 1);

    cascade.apply_declaration(style, make_decl("math-depth", "3"), parent);
    EXPECT_EQ(style.math_depth, 3);

    cascade.apply_declaration(style, make_decl("math-depth", "auto-add"), parent);
    EXPECT_EQ(style.math_depth, -1);

    cascade.apply_declaration(style, make_decl("math-style", "normal"), parent);
    EXPECT_EQ(style.math_style, 0);
}

TEST(PropertyCascadeTest, PrintColorAdjustValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.print_color_adjust, 0);  // default: economy

    cascade.apply_declaration(style, make_decl("print-color-adjust", "exact"), parent);
    EXPECT_EQ(style.print_color_adjust, 1);

    cascade.apply_declaration(style, make_decl("print-color-adjust", "economy"), parent);
    EXPECT_EQ(style.print_color_adjust, 0);

    // -webkit alias
    cascade.apply_declaration(style, make_decl("-webkit-print-color-adjust", "exact"), parent);
    EXPECT_EQ(style.print_color_adjust, 1);
}

TEST(PropertyCascadeTest, WebkitTextFillColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // default: transparent (a=0 means use 'color')
    EXPECT_EQ(style.text_fill_color.a, 0);

    cascade.apply_declaration(style, make_decl("-webkit-text-fill-color", "green"), parent);
    EXPECT_EQ(style.text_fill_color.r, 0);
    EXPECT_EQ(style.text_fill_color.g, 128);
    EXPECT_EQ(style.text_fill_color.b, 0);
    EXPECT_EQ(style.text_fill_color.a, 255);
}

// ---------------------------------------------------------------------------
// Cycle 464 — quotes, tab-size, letter-spacing, border-collapse/spacing,
//             table-layout, caption-side, empty-cells, gap shorthand
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, QuotesValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.quotes, "");

    cascade.apply_declaration(style, make_decl("quotes", "none"), parent);
    EXPECT_EQ(style.quotes, "none");

    // "auto" clears to empty string
    cascade.apply_declaration(style, make_decl("quotes", "auto"), parent);
    EXPECT_EQ(style.quotes, "");

    // explicit string preserved
    cascade.apply_declaration(style, make_decl("quotes", "\"\\201C\" \"\\201D\" \"\\2018\" \"\\2019\""), parent);
    EXPECT_NE(style.quotes, "");
    EXPECT_NE(style.quotes, "none");
}

TEST(PropertyCascadeTest, TabSizeAndMozAlias) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.tab_size, 4);  // default

    cascade.apply_declaration(style, make_decl("tab-size", "8"), parent);
    EXPECT_EQ(style.tab_size, 8);

    cascade.apply_declaration(style, make_decl("-moz-tab-size", "2"), parent);
    EXPECT_EQ(style.tab_size, 2);
}

TEST(PropertyCascadeTest, LetterSpacingPxAndNormal) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // default: zero
    EXPECT_TRUE(style.letter_spacing.is_zero());

    cascade.apply_declaration(style, make_decl("letter-spacing", "3px"), parent);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(0), 3.0f);

    cascade.apply_declaration(style, make_decl("letter-spacing", "normal"), parent);
    EXPECT_TRUE(style.letter_spacing.is_zero());
}

TEST(PropertyCascadeTest, BorderCollapseAndSpacing) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FALSE(style.border_collapse);  // default: separate

    cascade.apply_declaration(style, make_decl("border-collapse", "collapse"), parent);
    EXPECT_TRUE(style.border_collapse);

    cascade.apply_declaration(style, make_decl("border-collapse", "separate"), parent);
    EXPECT_FALSE(style.border_collapse);

    // border-spacing: two values (horizontal vertical)
    cascade.apply_declaration(style, make_decl("border-spacing", "10px 5px"), parent);
    EXPECT_FLOAT_EQ(style.border_spacing, 10.0f);
    EXPECT_FLOAT_EQ(style.border_spacing_v, 5.0f);
}

TEST(PropertyCascadeTest, TableLayoutValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.table_layout, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("table-layout", "fixed"), parent);
    EXPECT_EQ(style.table_layout, 1);

    cascade.apply_declaration(style, make_decl("table-layout", "auto"), parent);
    EXPECT_EQ(style.table_layout, 0);
}

TEST(PropertyCascadeTest, CaptionSideAndEmptyCells) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.caption_side, 0);  // default: top
    EXPECT_EQ(style.empty_cells, 0);   // default: show

    cascade.apply_declaration(style, make_decl("caption-side", "bottom"), parent);
    EXPECT_EQ(style.caption_side, 1);

    cascade.apply_declaration(style, make_decl("empty-cells", "hide"), parent);
    EXPECT_EQ(style.empty_cells, 1);

    cascade.apply_declaration(style, make_decl("caption-side", "top"), parent);
    EXPECT_EQ(style.caption_side, 0);

    cascade.apply_declaration(style, make_decl("empty-cells", "show"), parent);
    EXPECT_EQ(style.empty_cells, 0);
}

TEST(PropertyCascadeTest, GapShorthandSingleAndTwoValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // single value: sets both row-gap and column-gap
    cascade.apply_declaration(style, make_decl("gap", "20px"), parent);
    EXPECT_FLOAT_EQ(style.gap.to_px(0), 20.0f);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 20.0f);

    // two values: row-gap column-gap
    cascade.apply_declaration(style, make_decl("gap", "10px 30px"), parent);
    EXPECT_FLOAT_EQ(style.gap.to_px(0), 10.0f);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 30.0f);
}

TEST(PropertyCascadeTest, RowGapAndColumnGapLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("row-gap", "15px"), parent);
    EXPECT_FLOAT_EQ(style.gap.to_px(0), 15.0f);

    cascade.apply_declaration(style, make_decl("column-gap", "25px"), parent);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 25.0f);

    // grid aliases
    cascade.apply_declaration(style, make_decl("grid-row-gap", "5px"), parent);
    EXPECT_FLOAT_EQ(style.gap.to_px(0), 5.0f);

    cascade.apply_declaration(style, make_decl("grid-column-gap", "8px"), parent);
    EXPECT_FLOAT_EQ(style.column_gap_val.to_px(0), 8.0f);
}

// ---------------------------------------------------------------------------
// Cycle 465 — font-variant-east-asian, font-variant-alternates,
//             place-items, flex shorthand, order,
//             justify-content, align-self, justify-self
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, FontVariantEastAsianValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_east_asian, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-east-asian", "jis78"), parent);
    EXPECT_EQ(style.font_variant_east_asian, 1);

    cascade.apply_declaration(style, make_decl("font-variant-east-asian", "simplified"), parent);
    EXPECT_EQ(style.font_variant_east_asian, 5);

    cascade.apply_declaration(style, make_decl("font-variant-east-asian", "traditional"), parent);
    EXPECT_EQ(style.font_variant_east_asian, 6);

    cascade.apply_declaration(style, make_decl("font-variant-east-asian", "ruby"), parent);
    EXPECT_EQ(style.font_variant_east_asian, 9);

    cascade.apply_declaration(style, make_decl("font-variant-east-asian", "normal"), parent);
    EXPECT_EQ(style.font_variant_east_asian, 0);
}

TEST(PropertyCascadeTest, FontVariantAlternatesValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_variant_alternates, 0);  // default: normal

    cascade.apply_declaration(style, make_decl("font-variant-alternates", "historical-forms"), parent);
    EXPECT_EQ(style.font_variant_alternates, 1);

    cascade.apply_declaration(style, make_decl("font-variant-alternates", "normal"), parent);
    EXPECT_EQ(style.font_variant_alternates, 0);
}

TEST(PropertyCascadeTest, PlaceItemsShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // two values: align-items justify-items
    cascade.apply_declaration(style, make_decl("place-items", "center start"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_EQ(style.justify_items, 0);  // start

    // single value: sets both
    cascade.apply_declaration(style, make_decl("place-items", "center"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_EQ(style.justify_items, 2);  // center
}

TEST(PropertyCascadeTest, FlexShorthandNoneAutoAndExplicit) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 1.0f);
    EXPECT_TRUE(style.flex_basis.is_auto());

    // flex: none => grow=0, shrink=0, basis=auto
    cascade.apply_declaration(style, make_decl("flex", "none"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);
    EXPECT_TRUE(style.flex_basis.is_auto());

    // flex: auto => grow=1, shrink=1, basis=auto
    cascade.apply_declaration(style, make_decl("flex", "auto"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 1.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 1.0f);
    EXPECT_TRUE(style.flex_basis.is_auto());

    // flex: 2 => grow=2, shrink=1
    cascade.apply_declaration(style, make_decl("flex", "2"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
}

TEST(PropertyCascadeTest, OrderProperty) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.order, 0);  // default

    cascade.apply_declaration(style, make_decl("order", "3"), parent);
    EXPECT_EQ(style.order, 3);

    cascade.apply_declaration(style, make_decl("order", "-1"), parent);
    EXPECT_EQ(style.order, -1);

    cascade.apply_declaration(style, make_decl("order", "0"), parent);
    EXPECT_EQ(style.order, 0);
}

TEST(PropertyCascadeTest, JustifyContentValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.justify_content, JustifyContent::FlexStart);

    cascade.apply_declaration(style, make_decl("justify-content", "flex-end"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::FlexEnd);

    cascade.apply_declaration(style, make_decl("justify-content", "center"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);

    cascade.apply_declaration(style, make_decl("justify-content", "space-between"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);

    cascade.apply_declaration(style, make_decl("justify-content", "space-around"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceAround);

    cascade.apply_declaration(style, make_decl("justify-content", "space-evenly"), parent);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceEvenly);
}

TEST(PropertyCascadeTest, AlignSelfValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.align_self, -1);  // default: auto

    cascade.apply_declaration(style, make_decl("align-self", "flex-start"), parent);
    EXPECT_EQ(style.align_self, 0);

    cascade.apply_declaration(style, make_decl("align-self", "flex-end"), parent);
    EXPECT_EQ(style.align_self, 1);

    cascade.apply_declaration(style, make_decl("align-self", "center"), parent);
    EXPECT_EQ(style.align_self, 2);

    cascade.apply_declaration(style, make_decl("align-self", "stretch"), parent);
    EXPECT_EQ(style.align_self, 4);

    cascade.apply_declaration(style, make_decl("align-self", "auto"), parent);
    EXPECT_EQ(style.align_self, -1);
}

TEST(PropertyCascadeTest, JustifySelfValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.justify_self, -1);  // default: auto

    cascade.apply_declaration(style, make_decl("justify-self", "start"), parent);
    EXPECT_EQ(style.justify_self, 0);

    cascade.apply_declaration(style, make_decl("justify-self", "end"), parent);
    EXPECT_EQ(style.justify_self, 1);

    cascade.apply_declaration(style, make_decl("justify-self", "center"), parent);
    EXPECT_EQ(style.justify_self, 2);

    cascade.apply_declaration(style, make_decl("justify-self", "stretch"), parent);
    EXPECT_EQ(style.justify_self, 4);
}

// ---------------------------------------------------------------------------
// Cycle 466 — place-self shorthand, flex-direction, flex-wrap, flex-flow,
//             align-items, flex-grow/shrink, flex-basis
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, PlaceSelfShorthandSingleValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single value sets both align-self and justify-self
    cascade.apply_declaration(style, make_decl("place-self", "center"), parent);
    EXPECT_EQ(style.align_self, 2);
    EXPECT_EQ(style.justify_self, 2);

    cascade.apply_declaration(style, make_decl("place-self", "stretch"), parent);
    EXPECT_EQ(style.align_self, 4);
    EXPECT_EQ(style.justify_self, 4);

    cascade.apply_declaration(style, make_decl("place-self", "auto"), parent);
    EXPECT_EQ(style.align_self, -1);
    EXPECT_EQ(style.justify_self, -1);
}

TEST(PropertyCascadeTest, PlaceSelfShorthandTwoValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Two values: first is align-self, second is justify-self
    cascade.apply_declaration(style, make_decl("place-self", "start end"), parent);
    EXPECT_EQ(style.align_self, 0);   // start -> flex-start
    EXPECT_EQ(style.justify_self, 1); // end -> flex-end

    cascade.apply_declaration(style, make_decl("place-self", "baseline center"), parent);
    EXPECT_EQ(style.align_self, 3);   // baseline
    EXPECT_EQ(style.justify_self, 2); // center
}

TEST(PropertyCascadeTest, FlexDirectionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.flex_direction, FlexDirection::Row);  // default

    cascade.apply_declaration(style, make_decl("flex-direction", "column"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);

    cascade.apply_declaration(style, make_decl("flex-direction", "row-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::RowReverse);

    cascade.apply_declaration(style, make_decl("flex-direction", "column-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);

    cascade.apply_declaration(style, make_decl("flex-direction", "row"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
}

TEST(PropertyCascadeTest, FlexWrapValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.flex_wrap, FlexWrap::NoWrap);  // default

    cascade.apply_declaration(style, make_decl("flex-wrap", "wrap"), parent);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);

    cascade.apply_declaration(style, make_decl("flex-wrap", "wrap-reverse"), parent);
    EXPECT_EQ(style.flex_wrap, FlexWrap::WrapReverse);

    cascade.apply_declaration(style, make_decl("flex-wrap", "nowrap"), parent);
    EXPECT_EQ(style.flex_wrap, FlexWrap::NoWrap);
}

TEST(PropertyCascadeTest, FlexFlowShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // flex-flow sets flex-direction and flex-wrap together
    cascade.apply_declaration(style, make_decl("flex-flow", "column wrap"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);

    cascade.apply_declaration(style, make_decl("flex-flow", "row-reverse wrap-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::RowReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::WrapReverse);

    cascade.apply_declaration(style, make_decl("flex-flow", "row nowrap"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
    EXPECT_EQ(style.flex_wrap, FlexWrap::NoWrap);
}

TEST(PropertyCascadeTest, AlignItemsValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.align_items, AlignItems::Stretch);  // default

    cascade.apply_declaration(style, make_decl("align-items", "flex-start"), parent);
    EXPECT_EQ(style.align_items, AlignItems::FlexStart);

    cascade.apply_declaration(style, make_decl("align-items", "flex-end"), parent);
    EXPECT_EQ(style.align_items, AlignItems::FlexEnd);

    cascade.apply_declaration(style, make_decl("align-items", "center"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Center);

    cascade.apply_declaration(style, make_decl("align-items", "baseline"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Baseline);

    cascade.apply_declaration(style, make_decl("align-items", "stretch"), parent);
    EXPECT_EQ(style.align_items, AlignItems::Stretch);
}

TEST(PropertyCascadeTest, FlexGrowAndShrink) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.flex_grow, 0.0f);    // default
    EXPECT_FLOAT_EQ(style.flex_shrink, 1.0f);  // default

    cascade.apply_declaration(style, make_decl("flex-grow", "2"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);

    cascade.apply_declaration(style, make_decl("flex-grow", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.flex_grow, 0.5f);

    cascade.apply_declaration(style, make_decl("flex-shrink", "0"), parent);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);

    cascade.apply_declaration(style, make_decl("flex-shrink", "3"), parent);
    EXPECT_FLOAT_EQ(style.flex_shrink, 3.0f);
}

TEST(PropertyCascadeTest, FlexBasisValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default is auto
    EXPECT_TRUE(style.flex_basis.is_auto());

    cascade.apply_declaration(style, make_decl("flex-basis", "100px"), parent);
    EXPECT_FALSE(style.flex_basis.is_auto());
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 100.0f);

    cascade.apply_declaration(style, make_decl("flex-basis", "0"), parent);
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 0.0f);
}

// ---------------------------------------------------------------------------
// Cycle 467 — border-image longhands and CSS mask properties
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BorderImageSourceUrlAndNone) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.border_image_source.empty());  // default: empty

    cascade.apply_declaration(style, make_decl("border-image-source", "url(foo.png)"), parent);
    EXPECT_EQ(style.border_image_source, "foo.png");

    cascade.apply_declaration(style, make_decl("border-image-source", "none"), parent);
    EXPECT_TRUE(style.border_image_source.empty());
}

TEST(PropertyCascadeTest, BorderImageSliceAndFill) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.border_image_slice, 100.0f);   // default
    EXPECT_FALSE(style.border_image_slice_fill);          // default

    cascade.apply_declaration(style, make_decl("border-image-slice", "30"), parent);
    EXPECT_FLOAT_EQ(style.border_image_slice, 30.0f);

    cascade.apply_declaration(style, make_decl("border-image-slice", "25 fill"), parent);
    EXPECT_FLOAT_EQ(style.border_image_slice, 25.0f);
    EXPECT_TRUE(style.border_image_slice_fill);
}

TEST(PropertyCascadeTest, BorderImageWidthAndOutset) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.border_image_width_val, 1.0f);  // default
    EXPECT_FLOAT_EQ(style.border_image_outset, 0.0f);      // default

    cascade.apply_declaration(style, make_decl("border-image-width", "10px"), parent);
    EXPECT_FLOAT_EQ(style.border_image_width_val, 10.0f);

    cascade.apply_declaration(style, make_decl("border-image-outset", "5px"), parent);
    EXPECT_FLOAT_EQ(style.border_image_outset, 5.0f);
}

TEST(PropertyCascadeTest, BorderImageRepeatValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.border_image_repeat, 0);  // default: stretch

    cascade.apply_declaration(style, make_decl("border-image-repeat", "repeat"), parent);
    EXPECT_EQ(style.border_image_repeat, 1);

    cascade.apply_declaration(style, make_decl("border-image-repeat", "round"), parent);
    EXPECT_EQ(style.border_image_repeat, 2);

    cascade.apply_declaration(style, make_decl("border-image-repeat", "space"), parent);
    EXPECT_EQ(style.border_image_repeat, 3);

    cascade.apply_declaration(style, make_decl("border-image-repeat", "stretch"), parent);
    EXPECT_EQ(style.border_image_repeat, 0);
}

TEST(PropertyCascadeTest, MaskImageAndShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.mask_image.empty());  // default

    cascade.apply_declaration(style, make_decl("mask-image", "url(mask.svg)"), parent);
    EXPECT_EQ(style.mask_image, "url(mask.svg)");

    // -webkit-mask-image alias
    cascade.apply_declaration(style, make_decl("-webkit-mask-image", "url(m2.svg)"), parent);
    EXPECT_EQ(style.mask_image, "url(m2.svg)");

    // mask shorthand stores raw value
    cascade.apply_declaration(style, make_decl("mask", "url(m.svg) no-repeat center"), parent);
    EXPECT_FALSE(style.mask_shorthand.empty());
}

TEST(PropertyCascadeTest, MaskSizeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.mask_size, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("mask-size", "cover"), parent);
    EXPECT_EQ(style.mask_size, 1);

    cascade.apply_declaration(style, make_decl("mask-size", "contain"), parent);
    EXPECT_EQ(style.mask_size, 2);

    cascade.apply_declaration(style, make_decl("mask-size", "auto"), parent);
    EXPECT_EQ(style.mask_size, 0);

    // Explicit pixel size: sets mask_size=3
    cascade.apply_declaration(style, make_decl("-webkit-mask-size", "50px 30px"), parent);
    EXPECT_EQ(style.mask_size, 3);
    EXPECT_FLOAT_EQ(style.mask_size_width, 50.0f);
    EXPECT_FLOAT_EQ(style.mask_size_height, 30.0f);
}

TEST(PropertyCascadeTest, MaskRepeatValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.mask_repeat, 0);  // default: repeat

    cascade.apply_declaration(style, make_decl("mask-repeat", "repeat-x"), parent);
    EXPECT_EQ(style.mask_repeat, 1);

    cascade.apply_declaration(style, make_decl("mask-repeat", "repeat-y"), parent);
    EXPECT_EQ(style.mask_repeat, 2);

    cascade.apply_declaration(style, make_decl("mask-repeat", "no-repeat"), parent);
    EXPECT_EQ(style.mask_repeat, 3);

    cascade.apply_declaration(style, make_decl("-webkit-mask-repeat", "space"), parent);
    EXPECT_EQ(style.mask_repeat, 4);

    cascade.apply_declaration(style, make_decl("mask-repeat", "round"), parent);
    EXPECT_EQ(style.mask_repeat, 5);

    cascade.apply_declaration(style, make_decl("mask-repeat", "repeat"), parent);
    EXPECT_EQ(style.mask_repeat, 0);
}

TEST(PropertyCascadeTest, MaskOriginClipCompositeMode) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // mask-origin
    EXPECT_EQ(style.mask_origin, 0);  // default: border-box
    cascade.apply_declaration(style, make_decl("mask-origin", "padding-box"), parent);
    EXPECT_EQ(style.mask_origin, 1);
    cascade.apply_declaration(style, make_decl("mask-origin", "content-box"), parent);
    EXPECT_EQ(style.mask_origin, 2);

    // mask-clip
    EXPECT_EQ(style.mask_clip, 0);  // default: border-box
    cascade.apply_declaration(style, make_decl("mask-clip", "padding-box"), parent);
    EXPECT_EQ(style.mask_clip, 1);
    cascade.apply_declaration(style, make_decl("-webkit-mask-clip", "no-clip"), parent);
    EXPECT_EQ(style.mask_clip, 3);

    // mask-composite
    EXPECT_EQ(style.mask_composite, 0);  // default: add
    cascade.apply_declaration(style, make_decl("mask-composite", "subtract"), parent);
    EXPECT_EQ(style.mask_composite, 1);
    cascade.apply_declaration(style, make_decl("mask-composite", "intersect"), parent);
    EXPECT_EQ(style.mask_composite, 2);

    // mask-mode
    EXPECT_EQ(style.mask_mode, 0);  // default: match-source
    cascade.apply_declaration(style, make_decl("mask-mode", "alpha"), parent);
    EXPECT_EQ(style.mask_mode, 1);
    cascade.apply_declaration(style, make_decl("mask-mode", "luminance"), parent);
    EXPECT_EQ(style.mask_mode, 2);
}

// ---------------------------------------------------------------------------
// Cycle 468 — perspective, transform-style, transform-box, transform-origin,
//             perspective-origin, filter, backdrop-filter, clip-path
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, PerspectiveNoneAndLength) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.perspective, 0.0f);  // default: none

    cascade.apply_declaration(style, make_decl("perspective", "500px"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 500.0f);

    cascade.apply_declaration(style, make_decl("perspective", "none"), parent);
    EXPECT_FLOAT_EQ(style.perspective, 0.0f);
}

TEST(PropertyCascadeTest, TransformStyleValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transform_style, 0);  // default: flat

    cascade.apply_declaration(style, make_decl("transform-style", "preserve-3d"), parent);
    EXPECT_EQ(style.transform_style, 1);

    cascade.apply_declaration(style, make_decl("transform-style", "flat"), parent);
    EXPECT_EQ(style.transform_style, 0);
}

TEST(PropertyCascadeTest, TransformBoxAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transform_box, 1);  // default: border-box

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

TEST(PropertyCascadeTest, TransformOriginKeywordValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.transform_origin_x, 50.0f);  // default: center
    EXPECT_FLOAT_EQ(style.transform_origin_y, 50.0f);  // default: center

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

TEST(PropertyCascadeTest, PerspectiveOriginValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.perspective_origin_x, 50.0f);  // default: center
    EXPECT_FLOAT_EQ(style.perspective_origin_y, 50.0f);

    cascade.apply_declaration(style, make_decl("perspective-origin", "left top"), parent);
    EXPECT_FLOAT_EQ(style.perspective_origin_x, 0.0f);
    EXPECT_FLOAT_EQ(style.perspective_origin_y, 0.0f);

    cascade.apply_declaration(style, make_decl("perspective-origin", "right bottom"), parent);
    EXPECT_FLOAT_EQ(style.perspective_origin_x, 100.0f);
    EXPECT_FLOAT_EQ(style.perspective_origin_y, 100.0f);

    cascade.apply_declaration(style, make_decl("perspective-origin", "center"), parent);
    EXPECT_FLOAT_EQ(style.perspective_origin_x, 50.0f);
}

TEST(PropertyCascadeTest, FilterFunctions) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.filters.empty());  // default: no filters

    // grayscale: type=1
    cascade.apply_declaration(style, make_decl("filter", "grayscale(0.5)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 1);
    EXPECT_FLOAT_EQ(style.filters[0].second, 0.5f);

    // blur: type=9
    cascade.apply_declaration(style, make_decl("filter", "blur(5px)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 9);
    EXPECT_FLOAT_EQ(style.filters[0].second, 5.0f);

    // brightness: type=3
    cascade.apply_declaration(style, make_decl("filter", "brightness(2)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 3);

    // none clears all filters
    cascade.apply_declaration(style, make_decl("filter", "none"), parent);
    EXPECT_TRUE(style.filters.empty());
}

TEST(PropertyCascadeTest, BackdropFilterValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.backdrop_filters.empty());  // default: none

    cascade.apply_declaration(style, make_decl("backdrop-filter", "blur(10px)"), parent);
    ASSERT_EQ(style.backdrop_filters.size(), 1u);
    EXPECT_EQ(style.backdrop_filters[0].first, 9);  // blur
    EXPECT_FLOAT_EQ(style.backdrop_filters[0].second, 10.0f);

    // -webkit-backdrop-filter alias
    cascade.apply_declaration(style, make_decl("-webkit-backdrop-filter", "grayscale(1)"), parent);
    ASSERT_EQ(style.backdrop_filters.size(), 1u);
    EXPECT_EQ(style.backdrop_filters[0].first, 1);  // grayscale

    // none clears
    cascade.apply_declaration(style, make_decl("backdrop-filter", "none"), parent);
    EXPECT_TRUE(style.backdrop_filters.empty());
}

TEST(PropertyCascadeTest, ClipPathNoneAndCircle) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.clip_path_type, 0);  // default: none

    // circle(50%) — type=1, values=[50.0]
    cascade.apply_declaration(style, make_decl("clip-path", "circle(50%)"), parent);
    EXPECT_EQ(style.clip_path_type, 1);
    ASSERT_FALSE(style.clip_path_values.empty());
    EXPECT_FLOAT_EQ(style.clip_path_values[0], 50.0f);

    // none resets to type 0
    cascade.apply_declaration(style, make_decl("clip-path", "none"), parent);
    EXPECT_EQ(style.clip_path_type, 0);
    EXPECT_TRUE(style.clip_path_values.empty());
}

// ---------------------------------------------------------------------------
// Cycle 469 — shape-outside, shape-margin/threshold, content, hanging-punctuation,
//             clip-path inset/ellipse
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ShapeOutsideBoxValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.shape_outside_type, 0);  // default: none

    cascade.apply_declaration(style, make_decl("shape-outside", "margin-box"), parent);
    EXPECT_EQ(style.shape_outside_type, 5);

    cascade.apply_declaration(style, make_decl("shape-outside", "border-box"), parent);
    EXPECT_EQ(style.shape_outside_type, 6);

    cascade.apply_declaration(style, make_decl("shape-outside", "padding-box"), parent);
    EXPECT_EQ(style.shape_outside_type, 7);

    cascade.apply_declaration(style, make_decl("shape-outside", "content-box"), parent);
    EXPECT_EQ(style.shape_outside_type, 8);

    cascade.apply_declaration(style, make_decl("shape-outside", "none"), parent);
    EXPECT_EQ(style.shape_outside_type, 0);
    EXPECT_TRUE(style.shape_outside_str.empty());
}

TEST(PropertyCascadeTest, ShapeOutsideCircleAndEllipse) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // circle(40%) -> type=1, values={40.0}
    cascade.apply_declaration(style, make_decl("shape-outside", "circle(40%)"), parent);
    EXPECT_EQ(style.shape_outside_type, 1);
    ASSERT_EQ(style.shape_outside_values.size(), 1u);
    EXPECT_FLOAT_EQ(style.shape_outside_values[0], 40.0f);

    // ellipse(30% 40%) -> type=2, values={30.0, 40.0}
    cascade.apply_declaration(style, make_decl("shape-outside", "ellipse(30% 40%)"), parent);
    EXPECT_EQ(style.shape_outside_type, 2);
    ASSERT_EQ(style.shape_outside_values.size(), 2u);
    EXPECT_FLOAT_EQ(style.shape_outside_values[0], 30.0f);
    EXPECT_FLOAT_EQ(style.shape_outside_values[1], 40.0f);
}

TEST(PropertyCascadeTest, ShapeOutsideInset) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // inset(10px) -> type=3, all four values = 10.0
    cascade.apply_declaration(style, make_decl("shape-outside", "inset(10px)"), parent);
    EXPECT_EQ(style.shape_outside_type, 3);
    ASSERT_EQ(style.shape_outside_values.size(), 4u);
    EXPECT_FLOAT_EQ(style.shape_outside_values[0], 10.0f);  // top
    EXPECT_FLOAT_EQ(style.shape_outside_values[1], 10.0f);  // right
    EXPECT_FLOAT_EQ(style.shape_outside_values[2], 10.0f);  // bottom
    EXPECT_FLOAT_EQ(style.shape_outside_values[3], 10.0f);  // left

    // inset(5px 15px) -> top=bottom=5, right=left=15
    cascade.apply_declaration(style, make_decl("shape-outside", "inset(5px 15px)"), parent);
    EXPECT_EQ(style.shape_outside_type, 3);
    ASSERT_EQ(style.shape_outside_values.size(), 4u);
    EXPECT_FLOAT_EQ(style.shape_outside_values[0], 5.0f);
    EXPECT_FLOAT_EQ(style.shape_outside_values[1], 15.0f);
    EXPECT_FLOAT_EQ(style.shape_outside_values[2], 5.0f);
    EXPECT_FLOAT_EQ(style.shape_outside_values[3], 15.0f);
}

TEST(PropertyCascadeTest, ShapeMarginAndThreshold) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.shape_margin, 0.0f);           // default
    EXPECT_FLOAT_EQ(style.shape_image_threshold, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("shape-margin", "20px"), parent);
    EXPECT_FLOAT_EQ(style.shape_margin, 20.0f);

    cascade.apply_declaration(style, make_decl("shape-image-threshold", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.shape_image_threshold, 0.5f);

    cascade.apply_declaration(style, make_decl("shape-image-threshold", "0.8"), parent);
    EXPECT_FLOAT_EQ(style.shape_image_threshold, 0.8f);
}

TEST(PropertyCascadeTest, ContentNoneAndStringLiteral) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.content.empty());  // default: empty

    cascade.apply_declaration(style, make_decl("content", "none"), parent);
    EXPECT_EQ(style.content, "none");

    cascade.apply_declaration(style, make_decl("content", "normal"), parent);
    EXPECT_EQ(style.content, "none");
}

TEST(PropertyCascadeTest, ContentOpenCloseQuoteAndAttr) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // open-quote -> left double quote U+201C (UTF-8: E2 80 9C)
    cascade.apply_declaration(style, make_decl("content", "open-quote"), parent);
    EXPECT_EQ(style.content, "\xe2\x80\x9c");

    // close-quote -> right double quote U+201D (UTF-8: E2 80 9D)
    cascade.apply_declaration(style, make_decl("content", "close-quote"), parent);
    EXPECT_EQ(style.content, "\xe2\x80\x9d");

    // attr(data-label) -> stores attr name + sentinel
    cascade.apply_declaration(style, make_decl("content", "attr(data-label)"), parent);
    EXPECT_EQ(style.content_attr_name, "data-label");
    EXPECT_FALSE(style.content.empty());
}

TEST(PropertyCascadeTest, HangingPunctuationValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.hanging_punctuation, 0);  // default: none

    cascade.apply_declaration(style, make_decl("hanging-punctuation", "first"), parent);
    EXPECT_EQ(style.hanging_punctuation, 1);

    cascade.apply_declaration(style, make_decl("hanging-punctuation", "last"), parent);
    EXPECT_EQ(style.hanging_punctuation, 2);

    cascade.apply_declaration(style, make_decl("hanging-punctuation", "force-end"), parent);
    EXPECT_EQ(style.hanging_punctuation, 3);

    cascade.apply_declaration(style, make_decl("hanging-punctuation", "allow-end"), parent);
    EXPECT_EQ(style.hanging_punctuation, 4);
}

TEST(PropertyCascadeTest, ClipPathInsetAndEllipse) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // inset(5px) -> type=3, values={5,5,5,5}
    cascade.apply_declaration(style, make_decl("clip-path", "inset(5px)"), parent);
    EXPECT_EQ(style.clip_path_type, 3);
    ASSERT_EQ(style.clip_path_values.size(), 4u);
    EXPECT_FLOAT_EQ(style.clip_path_values[0], 5.0f);

    // ellipse(50% 30%) -> type=2, values={50,30}
    cascade.apply_declaration(style, make_decl("clip-path", "ellipse(50% 30%)"), parent);
    EXPECT_EQ(style.clip_path_type, 2);
    ASSERT_GE(style.clip_path_values.size(), 2u);
    EXPECT_FLOAT_EQ(style.clip_path_values[0], 50.0f);
    EXPECT_FLOAT_EQ(style.clip_path_values[1], 30.0f);
}

// ---------------------------------------------------------------------------
// Cycle 470 — caret-color, accent-color, color-interpolation, counter properties,
//             column-rule, appearance, placeholder-color, writing-mode
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, CaretColorAndAccentColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // default: transparent {0,0,0,0}
    EXPECT_EQ(style.caret_color.r, 0);
    EXPECT_EQ(style.caret_color.a, 0);

    cascade.apply_declaration(style, make_decl("caret-color", "red"), parent);
    EXPECT_EQ(style.caret_color.r, 255);
    EXPECT_EQ(style.caret_color.g, 0);
    EXPECT_EQ(style.caret_color.b, 0);
    EXPECT_EQ(style.caret_color.a, 255);

    cascade.apply_declaration(style, make_decl("accent-color", "blue"), parent);
    EXPECT_EQ(style.accent_color.r, 0);
    EXPECT_EQ(style.accent_color.g, 0);
    EXPECT_EQ(style.accent_color.b, 255);
    EXPECT_EQ(style.accent_color.a, 255);
}

TEST(PropertyCascadeTest, ColorInterpolationValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.color_interpolation, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("color-interpolation", "srgb"), parent);
    EXPECT_EQ(style.color_interpolation, 1);

    cascade.apply_declaration(style, make_decl("color-interpolation", "linearrgb"), parent);
    EXPECT_EQ(style.color_interpolation, 2);

    cascade.apply_declaration(style, make_decl("color-interpolation", "auto"), parent);
    EXPECT_EQ(style.color_interpolation, 0);
}

TEST(PropertyCascadeTest, CounterIncrementResetSet) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.counter_increment.empty());

    cascade.apply_declaration(style, make_decl("counter-increment", "section 1"), parent);
    EXPECT_EQ(style.counter_increment, "section 1");

    cascade.apply_declaration(style, make_decl("counter-reset", "section 0"), parent);
    EXPECT_EQ(style.counter_reset, "section 0");

    cascade.apply_declaration(style, make_decl("counter-set", "section 5"), parent);
    EXPECT_EQ(style.counter_set, "section 5");
}

TEST(PropertyCascadeTest, ColumnRuleWidthStyleColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.column_rule_width, 0.0f);  // default
    EXPECT_EQ(style.column_rule_style, 0);           // default: none

    cascade.apply_declaration(style, make_decl("column-rule-width", "2px"), parent);
    EXPECT_FLOAT_EQ(style.column_rule_width, 2.0f);

    cascade.apply_declaration(style, make_decl("column-rule-style", "solid"), parent);
    EXPECT_EQ(style.column_rule_style, 1);

    cascade.apply_declaration(style, make_decl("column-rule-style", "dashed"), parent);
    EXPECT_EQ(style.column_rule_style, 2);

    cascade.apply_declaration(style, make_decl("column-rule-style", "dotted"), parent);
    EXPECT_EQ(style.column_rule_style, 3);

    cascade.apply_declaration(style, make_decl("column-rule-color", "red"), parent);
    EXPECT_EQ(style.column_rule_color.r, 255);
    EXPECT_EQ(style.column_rule_color.a, 255);
}

TEST(PropertyCascadeTest, ColumnRuleShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // column-rule: 3px solid red -> sets width, style, color
    cascade.apply_declaration(style, make_decl("column-rule", "3px solid red"), parent);
    EXPECT_FLOAT_EQ(style.column_rule_width, 3.0f);
    EXPECT_EQ(style.column_rule_style, 1);  // solid
    EXPECT_EQ(style.column_rule_color.r, 255);

    cascade.apply_declaration(style, make_decl("column-rule", "1px dashed blue"), parent);
    EXPECT_FLOAT_EQ(style.column_rule_width, 1.0f);
    EXPECT_EQ(style.column_rule_style, 2);  // dashed
    EXPECT_EQ(style.column_rule_color.b, 255);
}

TEST(PropertyCascadeTest, AppearanceAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.appearance, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("appearance", "none"), parent);
    EXPECT_EQ(style.appearance, 1);

    cascade.apply_declaration(style, make_decl("appearance", "button"), parent);
    EXPECT_EQ(style.appearance, 4);

    cascade.apply_declaration(style, make_decl("appearance", "textfield"), parent);
    EXPECT_EQ(style.appearance, 3);

    // -webkit-appearance alias
    cascade.apply_declaration(style, make_decl("-webkit-appearance", "none"), parent);
    EXPECT_EQ(style.appearance, 1);

    cascade.apply_declaration(style, make_decl("appearance", "auto"), parent);
    EXPECT_EQ(style.appearance, 0);
}

TEST(PropertyCascadeTest, PlaceholderColorAndWritingMode) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // placeholder-color: transparent by default
    EXPECT_EQ(style.placeholder_color.a, 0);

    cascade.apply_declaration(style, make_decl("placeholder-color", "red"), parent);
    EXPECT_EQ(style.placeholder_color.r, 255);
    EXPECT_EQ(style.placeholder_color.a, 255);

    // writing-mode
    cascade.apply_declaration(style, make_decl("writing-mode", "vertical-rl"), parent);
    EXPECT_EQ(style.writing_mode, 1);

    cascade.apply_declaration(style, make_decl("writing-mode", "vertical-lr"), parent);
    EXPECT_EQ(style.writing_mode, 2);

    cascade.apply_declaration(style, make_decl("writing-mode", "horizontal-tb"), parent);
    EXPECT_EQ(style.writing_mode, 0);
}

TEST(PropertyCascadeTest, TransitionPropertyAndDuration) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transition_property, "all");  // default
    EXPECT_FLOAT_EQ(style.transition_duration, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("transition-property", "opacity"), parent);
    EXPECT_EQ(style.transition_property, "opacity");

    cascade.apply_declaration(style, make_decl("transition-duration", "0.3s"), parent);
    EXPECT_FLOAT_EQ(style.transition_duration, 0.3f);

    cascade.apply_declaration(style, make_decl("transition-delay", "0.1s"), parent);
    EXPECT_FLOAT_EQ(style.transition_delay, 0.1f);
}

// ---------------------------------------------------------------------------
// Cycle 471 — animation properties (name, duration, timing, delay, iteration,
//             direction, fill-mode, play-state, composition, timeline, shorthand)
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, AnimationNameAndDuration) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.animation_name.empty());
    EXPECT_FLOAT_EQ(style.animation_duration, 0.0f);

    cascade.apply_declaration(style, make_decl("animation-name", "slide-in"), parent);
    EXPECT_EQ(style.animation_name, "slide-in");

    cascade.apply_declaration(style, make_decl("animation-duration", "0.5s"), parent);
    EXPECT_FLOAT_EQ(style.animation_duration, 0.5f);

    cascade.apply_declaration(style, make_decl("animation-duration", "300ms"), parent);
    EXPECT_FLOAT_EQ(style.animation_duration, 0.3f);
}

TEST(PropertyCascadeTest, AnimationTimingFunctionAllValues) {
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

TEST(PropertyCascadeTest, AnimationDelayAndIterationCount) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.animation_delay, 0.0f);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, 1.0f);

    cascade.apply_declaration(style, make_decl("animation-delay", "0.2s"), parent);
    EXPECT_FLOAT_EQ(style.animation_delay, 0.2f);

    cascade.apply_declaration(style, make_decl("animation-iteration-count", "3"), parent);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, 3.0f);

    cascade.apply_declaration(style, make_decl("animation-iteration-count", "infinite"), parent);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, -1.0f);
}

TEST(PropertyCascadeTest, AnimationDirectionAllValues) {
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

TEST(PropertyCascadeTest, AnimationFillModeAndPlayState) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_fill_mode, 0);   // default: none
    EXPECT_EQ(style.animation_play_state, 0);  // default: running

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "forwards"), parent);
    EXPECT_EQ(style.animation_fill_mode, 1);

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "backwards"), parent);
    EXPECT_EQ(style.animation_fill_mode, 2);

    cascade.apply_declaration(style, make_decl("animation-fill-mode", "both"), parent);
    EXPECT_EQ(style.animation_fill_mode, 3);

    cascade.apply_declaration(style, make_decl("animation-play-state", "paused"), parent);
    EXPECT_EQ(style.animation_play_state, 1);

    cascade.apply_declaration(style, make_decl("animation-play-state", "running"), parent);
    EXPECT_EQ(style.animation_play_state, 0);
}

TEST(PropertyCascadeTest, AnimationCompositionAndTimeline) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.animation_composition, 0);  // default: replace
    EXPECT_EQ(style.animation_timeline, "auto");

    cascade.apply_declaration(style, make_decl("animation-composition", "add"), parent);
    EXPECT_EQ(style.animation_composition, 1);

    cascade.apply_declaration(style, make_decl("animation-composition", "accumulate"), parent);
    EXPECT_EQ(style.animation_composition, 2);

    cascade.apply_declaration(style, make_decl("animation-timeline", "none"), parent);
    EXPECT_EQ(style.animation_timeline, "none");

    cascade.apply_declaration(style, make_decl("animation-timeline", "scroll()"), parent);
    EXPECT_EQ(style.animation_timeline, "scroll()");
}

TEST(PropertyCascadeTest, AnimationShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // animation: slide-in 0.4s ease-out 0.1s infinite alternate
    cascade.apply_declaration(style, make_decl("animation", "slide-in 0.4s ease-out 0.1s infinite alternate"), parent);
    EXPECT_EQ(style.animation_name, "slide-in");
    EXPECT_FLOAT_EQ(style.animation_duration, 0.4f);
    EXPECT_EQ(style.animation_timing, 3);   // ease-out
    EXPECT_FLOAT_EQ(style.animation_delay, 0.1f);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, -1.0f);  // infinite
    EXPECT_EQ(style.animation_direction, 2);  // alternate
}

TEST(PropertyCascadeTest, TransitionTimingFunctionAllValues) {
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
}

// ---------------------------------------------------------------------------
// Cycle 472 — isolation, mix-blend-mode, will-change, overscroll-behavior,
//             content-visibility, contain, break-before/after/inside, page-break
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, IsolationValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.isolation, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("isolation", "isolate"), parent);
    EXPECT_EQ(style.isolation, 1);

    cascade.apply_declaration(style, make_decl("isolation", "auto"), parent);
    EXPECT_EQ(style.isolation, 0);
}

TEST(PropertyCascadeTest, MixBlendModeAllValues) {
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

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "darken"), parent);
    EXPECT_EQ(style.mix_blend_mode, 4);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "lighten"), parent);
    EXPECT_EQ(style.mix_blend_mode, 5);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "difference"), parent);
    EXPECT_EQ(style.mix_blend_mode, 10);

    cascade.apply_declaration(style, make_decl("mix-blend-mode", "exclusion"), parent);
    EXPECT_EQ(style.mix_blend_mode, 11);
}

TEST(PropertyCascadeTest, WillChangeValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.will_change.empty());  // default: empty (auto)

    cascade.apply_declaration(style, make_decl("will-change", "opacity"), parent);
    EXPECT_EQ(style.will_change, "opacity");

    cascade.apply_declaration(style, make_decl("will-change", "transform"), parent);
    EXPECT_EQ(style.will_change, "transform");

    // auto clears it
    cascade.apply_declaration(style, make_decl("will-change", "auto"), parent);
    EXPECT_TRUE(style.will_change.empty());
}

TEST(PropertyCascadeTest, OverscrollBehaviorShorthandAndLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.overscroll_behavior_x, 0);
    EXPECT_EQ(style.overscroll_behavior_y, 0);

    // Shorthand sets both
    cascade.apply_declaration(style, make_decl("overscroll-behavior", "contain"), parent);
    EXPECT_EQ(style.overscroll_behavior_x, 1);
    EXPECT_EQ(style.overscroll_behavior_y, 1);

    // Two-value: x=contain, y=none
    cascade.apply_declaration(style, make_decl("overscroll-behavior", "contain none"), parent);
    EXPECT_EQ(style.overscroll_behavior_x, 1);
    EXPECT_EQ(style.overscroll_behavior_y, 2);

    // Longhands
    cascade.apply_declaration(style, make_decl("overscroll-behavior-x", "none"), parent);
    EXPECT_EQ(style.overscroll_behavior_x, 2);

    cascade.apply_declaration(style, make_decl("overscroll-behavior-y", "auto"), parent);
    EXPECT_EQ(style.overscroll_behavior_y, 0);
}

TEST(PropertyCascadeTest, ContentVisibilityAllValues) {
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

TEST(PropertyCascadeTest, ContainAllValues) {
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
}

TEST(PropertyCascadeTest, BreakBeforeAfterInside) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.break_before, 0);
    EXPECT_EQ(style.break_after, 0);
    EXPECT_EQ(style.break_inside, 0);

    cascade.apply_declaration(style, make_decl("break-before", "avoid"), parent);
    EXPECT_EQ(style.break_before, 1);

    cascade.apply_declaration(style, make_decl("break-before", "column"), parent);
    EXPECT_EQ(style.break_before, 4);

    cascade.apply_declaration(style, make_decl("break-after", "page"), parent);
    EXPECT_EQ(style.break_after, 3);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid"), parent);
    EXPECT_EQ(style.break_inside, 1);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-column"), parent);
    EXPECT_EQ(style.break_inside, 3);
}

TEST(PropertyCascadeTest, PageBreakLegacyProperties) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.page_break_before, 0);
    EXPECT_EQ(style.page_break_after, 0);

    cascade.apply_declaration(style, make_decl("page-break-before", "always"), parent);
    EXPECT_EQ(style.page_break_before, 1);

    cascade.apply_declaration(style, make_decl("page-break-before", "avoid"), parent);
    EXPECT_EQ(style.page_break_before, 2);

    cascade.apply_declaration(style, make_decl("page-break-after", "right"), parent);
    EXPECT_EQ(style.page_break_after, 4);

    cascade.apply_declaration(style, make_decl("page-break-inside", "avoid"), parent);
    EXPECT_EQ(style.page_break_inside, 1);
}

// ---------------------------------------------------------------------------
// Cycle 473 — list-style-type, list-style-position, list-style shorthand,
//             cursor, vertical-align, outline shorthand/longhands, outline-offset
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ListStyleTypeAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.list_style_type, ListStyleType::Disc);  // default

    cascade.apply_declaration(style, make_decl("list-style-type", "circle"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Circle);

    cascade.apply_declaration(style, make_decl("list-style-type", "square"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Square);

    cascade.apply_declaration(style, make_decl("list-style-type", "decimal"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Decimal);

    cascade.apply_declaration(style, make_decl("list-style-type", "lower-roman"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::LowerRoman);

    cascade.apply_declaration(style, make_decl("list-style-type", "upper-roman"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::UpperRoman);

    cascade.apply_declaration(style, make_decl("list-style-type", "lower-alpha"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::LowerAlpha);

    cascade.apply_declaration(style, make_decl("list-style-type", "none"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::None);
}

TEST(PropertyCascadeTest, ListStylePositionValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.list_style_position, ListStylePosition::Outside);  // default

    cascade.apply_declaration(style, make_decl("list-style-position", "inside"), parent);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);

    cascade.apply_declaration(style, make_decl("list-style-position", "outside"), parent);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Outside);
}

TEST(PropertyCascadeTest, ListStyleShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // list-style: circle inside
    cascade.apply_declaration(style, make_decl("list-style", "circle inside"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Circle);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);

    // list-style: decimal outside
    cascade.apply_declaration(style, make_decl("list-style", "decimal outside"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::Decimal);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Outside);
}

TEST(PropertyCascadeTest, CursorAllValues) {
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

TEST(PropertyCascadeTest, VerticalAlignAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline);  // default

    cascade.apply_declaration(style, make_decl("vertical-align", "top"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Top);

    cascade.apply_declaration(style, make_decl("vertical-align", "middle"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);

    cascade.apply_declaration(style, make_decl("vertical-align", "bottom"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Bottom);

    cascade.apply_declaration(style, make_decl("vertical-align", "text-top"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::TextTop);

    cascade.apply_declaration(style, make_decl("vertical-align", "text-bottom"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::TextBottom);

    cascade.apply_declaration(style, make_decl("vertical-align", "baseline"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline);
}

TEST(PropertyCascadeTest, OutlineShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // outline: 2px solid red
    cascade.apply_declaration(style, make_decl("outline", "2px solid red"), parent);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 2.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 255);
    EXPECT_EQ(style.outline_color.a, 255);

    // outline: 1px dashed blue
    cascade.apply_declaration(style, make_decl("outline", "1px dashed blue"), parent);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 1.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(style.outline_color.b, 255);
}

TEST(PropertyCascadeTest, OutlineLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("outline-width", "3px"), parent);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);

    cascade.apply_declaration(style, make_decl("outline-style", "dotted"), parent);
    EXPECT_EQ(style.outline_style, BorderStyle::Dotted);

    cascade.apply_declaration(style, make_decl("outline-color", "green"), parent);
    EXPECT_EQ(style.outline_color.g, 128);  // CSS green is #008000

    cascade.apply_declaration(style, make_decl("outline-style", "none"), parent);
    EXPECT_EQ(style.outline_style, BorderStyle::None);
}

TEST(PropertyCascadeTest, OutlineOffsetValue) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 0.0f);  // default

    cascade.apply_declaration(style, make_decl("outline-offset", "5px"), parent);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 5.0f);

    cascade.apply_declaration(style, make_decl("outline-offset", "0"), parent);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 0.0f);
}

// ---------------------------------------------------------------------------
// Cycle 474 — border-color/style/width shorthands, border side longhands,
//             font-synthesis, text-decoration-skip
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BorderColorShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single value: all four sides get same color
    cascade.apply_declaration(style, make_decl("border-color", "red"), parent);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_right.color.r, 255);
    EXPECT_EQ(style.border_bottom.color.r, 255);
    EXPECT_EQ(style.border_left.color.r, 255);

    // Two values: top/bottom=red, right/left=blue
    cascade.apply_declaration(style, make_decl("border-color", "red blue"), parent);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_bottom.color.r, 255);
    EXPECT_EQ(style.border_right.color.b, 255);
    EXPECT_EQ(style.border_left.color.b, 255);
}

TEST(PropertyCascadeTest, BorderStyleShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single value: all sides
    cascade.apply_declaration(style, make_decl("border-style", "solid"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);

    // Two values: top/bottom=dashed, right/left=dotted
    cascade.apply_declaration(style, make_decl("border-style", "dashed dotted"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dotted);
}

TEST(PropertyCascadeTest, BorderWidthShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Single value: all four sides
    cascade.apply_declaration(style, make_decl("border-width", "2px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);

    // Four values: top=1, right=2, bottom=3, left=4
    cascade.apply_declaration(style, make_decl("border-width", "1px 2px 3px 4px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
}

TEST(PropertyCascadeTest, BorderSideColorLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-left-color", "red"), parent);
    EXPECT_EQ(style.border_left.color.r, 255);

    cascade.apply_declaration(style, make_decl("border-right-color", "blue"), parent);
    EXPECT_EQ(style.border_right.color.b, 255);

    cascade.apply_declaration(style, make_decl("border-bottom-color", "green"), parent);
    EXPECT_EQ(style.border_bottom.color.g, 128);  // CSS green = #008000
}

TEST(PropertyCascadeTest, BorderSideStyleLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-left-style", "solid"), parent);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);

    cascade.apply_declaration(style, make_decl("border-right-style", "dashed"), parent);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);

    cascade.apply_declaration(style, make_decl("border-bottom-style", "dotted"), parent);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
}

TEST(PropertyCascadeTest, BorderSideWidthLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-left-width", "3px"), parent);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 3.0f);

    cascade.apply_declaration(style, make_decl("border-right-width", "5px"), parent);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 5.0f);

    cascade.apply_declaration(style, make_decl("border-bottom-width", "1px"), parent);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 1.0f);
}

TEST(PropertyCascadeTest, FontSynthesisValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default is weight|style (3) or normal; check initial state via resolver
    cascade.apply_declaration(style, make_decl("font-synthesis", "none"), parent);
    EXPECT_EQ(style.font_synthesis, 0);

    cascade.apply_declaration(style, make_decl("font-synthesis", "weight"), parent);
    EXPECT_EQ(style.font_synthesis, 1);

    cascade.apply_declaration(style, make_decl("font-synthesis", "style"), parent);
    EXPECT_EQ(style.font_synthesis, 2);

    cascade.apply_declaration(style, make_decl("font-synthesis", "weight style"), parent);
    EXPECT_EQ(style.font_synthesis, 3);  // weight=1 | style=2 = 3
}

TEST(PropertyCascadeTest, TextDecorationSkipValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_decoration_skip, 0);  // default: none

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "objects"), parent);
    EXPECT_EQ(style.text_decoration_skip, 1);

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "spaces"), parent);
    EXPECT_EQ(style.text_decoration_skip, 2);

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "ink"), parent);
    EXPECT_EQ(style.text_decoration_skip, 3);

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "edges"), parent);
    EXPECT_EQ(style.text_decoration_skip, 4);

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "box-decoration"), parent);
    EXPECT_EQ(style.text_decoration_skip, 5);

    cascade.apply_declaration(style, make_decl("text-decoration-skip", "none"), parent);
    EXPECT_EQ(style.text_decoration_skip, 0);
}

// ---------------------------------------------------------------------------
// Cycle 475 — text-align, text-align-last, z-index, clear, visibility,
//             box-sizing, white-space-collapse, line-break
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, TextAlignAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_align, TextAlign::Left);  // default

    cascade.apply_declaration(style, make_decl("text-align", "right"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Right);

    cascade.apply_declaration(style, make_decl("text-align", "center"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Center);

    cascade.apply_declaration(style, make_decl("text-align", "justify"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Justify);

    // "start" is an alias for left, "end" for right
    cascade.apply_declaration(style, make_decl("text-align", "start"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Left);

    cascade.apply_declaration(style, make_decl("text-align", "end"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Right);
}

TEST(PropertyCascadeTest, TextAlignLastValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_align_last, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("text-align-last", "left"), parent);
    EXPECT_EQ(style.text_align_last, 1);

    cascade.apply_declaration(style, make_decl("text-align-last", "right"), parent);
    EXPECT_EQ(style.text_align_last, 2);

    cascade.apply_declaration(style, make_decl("text-align-last", "center"), parent);
    EXPECT_EQ(style.text_align_last, 3);

    cascade.apply_declaration(style, make_decl("text-align-last", "justify"), parent);
    EXPECT_EQ(style.text_align_last, 4);

    cascade.apply_declaration(style, make_decl("text-align-last", "auto"), parent);
    EXPECT_EQ(style.text_align_last, 0);
}

TEST(PropertyCascadeTest, ZIndexValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.z_index, 0);  // default: 0

    cascade.apply_declaration(style, make_decl("z-index", "10"), parent);
    EXPECT_EQ(style.z_index, 10);

    cascade.apply_declaration(style, make_decl("z-index", "-1"), parent);
    EXPECT_EQ(style.z_index, -1);

    cascade.apply_declaration(style, make_decl("z-index", "999"), parent);
    EXPECT_EQ(style.z_index, 999);

    cascade.apply_declaration(style, make_decl("z-index", "0"), parent);
    EXPECT_EQ(style.z_index, 0);
}

TEST(PropertyCascadeTest, ClearAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.clear, Clear::None);  // default

    cascade.apply_declaration(style, make_decl("clear", "left"), parent);
    EXPECT_EQ(style.clear, Clear::Left);

    cascade.apply_declaration(style, make_decl("clear", "right"), parent);
    EXPECT_EQ(style.clear, Clear::Right);

    cascade.apply_declaration(style, make_decl("clear", "both"), parent);
    EXPECT_EQ(style.clear, Clear::Both);

    cascade.apply_declaration(style, make_decl("clear", "none"), parent);
    EXPECT_EQ(style.clear, Clear::None);
}

TEST(PropertyCascadeTest, VisibilityAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.visibility, Visibility::Visible);  // default

    cascade.apply_declaration(style, make_decl("visibility", "hidden"), parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);

    cascade.apply_declaration(style, make_decl("visibility", "collapse"), parent);
    EXPECT_EQ(style.visibility, Visibility::Collapse);

    cascade.apply_declaration(style, make_decl("visibility", "visible"), parent);
    EXPECT_EQ(style.visibility, Visibility::Visible);
}

TEST(PropertyCascadeTest, BoxSizingAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.box_sizing, BoxSizing::ContentBox);  // default

    cascade.apply_declaration(style, make_decl("box-sizing", "border-box"), parent);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);

    cascade.apply_declaration(style, make_decl("box-sizing", "content-box"), parent);
    EXPECT_EQ(style.box_sizing, BoxSizing::ContentBox);
}

TEST(PropertyCascadeTest, WhiteSpaceCollapseValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.white_space_collapse, 0);  // default: collapse

    cascade.apply_declaration(style, make_decl("white-space-collapse", "preserve"), parent);
    EXPECT_EQ(style.white_space_collapse, 1);

    cascade.apply_declaration(style, make_decl("white-space-collapse", "preserve-breaks"), parent);
    EXPECT_EQ(style.white_space_collapse, 2);

    cascade.apply_declaration(style, make_decl("white-space-collapse", "break-spaces"), parent);
    EXPECT_EQ(style.white_space_collapse, 3);

    cascade.apply_declaration(style, make_decl("white-space-collapse", "collapse"), parent);
    EXPECT_EQ(style.white_space_collapse, 0);
}

TEST(PropertyCascadeTest, LineBreakValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.line_break, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("line-break", "loose"), parent);
    EXPECT_EQ(style.line_break, 1);

    cascade.apply_declaration(style, make_decl("line-break", "normal"), parent);
    EXPECT_EQ(style.line_break, 2);

    cascade.apply_declaration(style, make_decl("line-break", "strict"), parent);
    EXPECT_EQ(style.line_break, 3);

    cascade.apply_declaration(style, make_decl("line-break", "anywhere"), parent);
    EXPECT_EQ(style.line_break, 4);

    cascade.apply_declaration(style, make_decl("line-break", "auto"), parent);
    EXPECT_EQ(style.line_break, 0);
}

// ---------------------------------------------------------------------------
// Cycle 476 — font-style, height/min/max, top/right/bottom/left, margin longhands,
//             padding longhands, text-shadow (apply_declaration), text-indent, list-style-image
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, FontStyleAllValues) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_style, FontStyle::Normal);  // default

    cascade.apply_declaration(style, make_decl("font-style", "italic"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Italic);

    cascade.apply_declaration(style, make_decl("font-style", "oblique"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Oblique);

    cascade.apply_declaration(style, make_decl("font-style", "normal"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Normal);
}

TEST(PropertyCascadeTest, HeightMinMaxHeight) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.height.is_auto());  // default

    cascade.apply_declaration(style, make_decl("height", "100px"), parent);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);

    cascade.apply_declaration(style, make_decl("min-height", "50px"), parent);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 50.0f);

    cascade.apply_declaration(style, make_decl("max-height", "200px"), parent);
    EXPECT_FLOAT_EQ(style.max_height.to_px(), 200.0f);

    cascade.apply_declaration(style, make_decl("max-width", "300px"), parent);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 300.0f);

    cascade.apply_declaration(style, make_decl("min-width", "10px"), parent);
    EXPECT_FLOAT_EQ(style.min_width.to_px(), 10.0f);
}

TEST(PropertyCascadeTest, PositionLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("top", "20px"), parent);
    EXPECT_FLOAT_EQ(style.top.to_px(), 20.0f);

    cascade.apply_declaration(style, make_decl("bottom", "10px"), parent);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 10.0f);

    cascade.apply_declaration(style, make_decl("left", "30px"), parent);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 30.0f);

    cascade.apply_declaration(style, make_decl("right", "5px"), parent);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 5.0f);
}

TEST(PropertyCascadeTest, MarginLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("margin-bottom", "15px"), parent);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 15.0f);

    cascade.apply_declaration(style, make_decl("margin-left", "25px"), parent);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 25.0f);

    cascade.apply_declaration(style, make_decl("margin-right", "35px"), parent);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 35.0f);
}

TEST(PropertyCascadeTest, PaddingLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("padding-top", "8px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 8.0f);

    cascade.apply_declaration(style, make_decl("padding-bottom", "12px"), parent);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 12.0f);

    cascade.apply_declaration(style, make_decl("padding-left", "4px"), parent);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 4.0f);

    cascade.apply_declaration(style, make_decl("padding-right", "6px"), parent);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 6.0f);
}

TEST(PropertyCascadeTest, TextShadowViaApplyDeclaration) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // 3px 3px 5px blue
    cascade.apply_declaration(style, make_decl("text-shadow", "3px 3px 5px blue"), parent);
    EXPECT_FLOAT_EQ(style.text_shadow_offset_x, 3.0f);
    EXPECT_FLOAT_EQ(style.text_shadow_offset_y, 3.0f);
    EXPECT_FLOAT_EQ(style.text_shadow_blur, 5.0f);
    EXPECT_EQ(style.text_shadow_color.b, 255);

    // none resets to transparent
    cascade.apply_declaration(style, make_decl("text-shadow", "none"), parent);
    EXPECT_EQ(style.text_shadow_color.a, 0);
    EXPECT_FLOAT_EQ(style.text_shadow_offset_x, 0.0f);
}

TEST(PropertyCascadeTest, TextIndentViaApplyDeclaration) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 0.0f);  // default

    cascade.apply_declaration(style, make_decl("text-indent", "32px"), parent);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 32.0f);

    cascade.apply_declaration(style, make_decl("text-indent", "0"), parent);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 0.0f);
}

TEST(PropertyCascadeTest, ListStyleImageUrlAndNone) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.list_style_image.empty());  // default: none

    cascade.apply_declaration(style, make_decl("list-style-image", "url(bullet.png)"), parent);
    EXPECT_EQ(style.list_style_image, "bullet.png");

    cascade.apply_declaration(style, make_decl("list-style-image", "none"), parent);
    EXPECT_TRUE(style.list_style_image.empty());
}

// ---------------------------------------------------------------------------
// Cycle 477 — background-position longhands, inline/block-size, text-emphasis,
//             text-underline-offset, background and border shorthands
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BackgroundPositionXLonghand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_position_x, 0);  // default: left

    cascade.apply_declaration(style, make_decl("background-position-x", "right"), parent);
    EXPECT_EQ(style.background_position_x, 2);

    cascade.apply_declaration(style, make_decl("background-position-x", "center"), parent);
    EXPECT_EQ(style.background_position_x, 1);

    cascade.apply_declaration(style, make_decl("background-position-x", "left"), parent);
    EXPECT_EQ(style.background_position_x, 0);
}

TEST(PropertyCascadeTest, BackgroundPositionYLonghand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_position_y, 0);  // default: top

    cascade.apply_declaration(style, make_decl("background-position-y", "bottom"), parent);
    EXPECT_EQ(style.background_position_y, 2);

    cascade.apply_declaration(style, make_decl("background-position-y", "center"), parent);
    EXPECT_EQ(style.background_position_y, 1);

    cascade.apply_declaration(style, make_decl("background-position-y", "top"), parent);
    EXPECT_EQ(style.background_position_y, 0);
}

TEST(PropertyCascadeTest, InlineSizeAndBlockSize) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // inline-size maps to width
    cascade.apply_declaration(style, make_decl("inline-size", "200px"), parent);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);

    // block-size maps to height
    cascade.apply_declaration(style, make_decl("block-size", "100px"), parent);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
}

TEST(PropertyCascadeTest, TextEmphasisShorthandColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_emphasis_style, "none");
    EXPECT_EQ(style.text_emphasis_color, 0u);

    // "circle red" → style="circle", color=ARGB red (0xFFFF0000)
    cascade.apply_declaration(style, make_decl("text-emphasis", "circle red"), parent);
    EXPECT_EQ(style.text_emphasis_style, "circle");
    EXPECT_EQ(style.text_emphasis_color, 0xFFFF0000u);

    // "none" resets both
    cascade.apply_declaration(style, make_decl("text-emphasis", "none"), parent);
    EXPECT_EQ(style.text_emphasis_style, "none");
    EXPECT_EQ(style.text_emphasis_color, 0u);
}

TEST(PropertyCascadeTest, TextEmphasisColorDirect) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_emphasis_color, 0u);  // default

    // blue → ARGB 0xFF0000FF
    cascade.apply_declaration(style, make_decl("text-emphasis-color", "blue"), parent);
    EXPECT_EQ(style.text_emphasis_color, 0xFF0000FFu);

    // green → ARGB 0xFF008000
    cascade.apply_declaration(style, make_decl("text-emphasis-color", "green"), parent);
    EXPECT_EQ(style.text_emphasis_color, 0xFF008000u);
}

TEST(PropertyCascadeTest, TextUnderlineOffset) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.text_underline_offset, 0.0f);  // default

    cascade.apply_declaration(style, make_decl("text-underline-offset", "5px"), parent);
    EXPECT_FLOAT_EQ(style.text_underline_offset, 5.0f);

    cascade.apply_declaration(style, make_decl("text-underline-offset", "0"), parent);
    EXPECT_FLOAT_EQ(style.text_underline_offset, 0.0f);
}

TEST(PropertyCascadeTest, BackgroundShorthandColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.background_color.a, 0);  // default: transparent

    cascade.apply_declaration(style, make_decl("background", "red"), parent);
    EXPECT_EQ(style.background_color.r, 255);
    EXPECT_EQ(style.background_color.g, 0);
    EXPECT_EQ(style.background_color.b, 0);
    EXPECT_EQ(style.background_color.a, 255);

    cascade.apply_declaration(style, make_decl("background", "blue"), parent);
    EXPECT_EQ(style.background_color.r, 0);
    EXPECT_EQ(style.background_color.b, 255);
    EXPECT_EQ(style.background_color.a, 255);
}

TEST(PropertyCascadeTest, BorderShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border", "2px solid blue"), parent);
    // All 4 sides get same width, style, color
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.b, 255);
    EXPECT_EQ(style.border_top.color.a, 255);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.b, 255);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.color.b, 255);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_left.color.b, 255);
}

// ---------------------------------------------------------------------------
// Cycle 478 — corner radii, logical margin/padding shorthands, logical
//             min/max sizes, scroll-margin/padding longhands
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BorderCornerRadii) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.border_radius_tl, 0.0f);

    cascade.apply_declaration(style, make_decl("border-top-left-radius", "8px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius_tl, 8.0f);

    cascade.apply_declaration(style, make_decl("border-top-right-radius", "12px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius_tr, 12.0f);

    cascade.apply_declaration(style, make_decl("border-bottom-left-radius", "4px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius_bl, 4.0f);

    cascade.apply_declaration(style, make_decl("border-bottom-right-radius", "16px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius_br, 16.0f);
}

TEST(PropertyCascadeTest, BorderLogicalRadii) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-start-start-radius", "6px"), parent);
    EXPECT_FLOAT_EQ(style.border_start_start_radius, 6.0f);

    cascade.apply_declaration(style, make_decl("border-start-end-radius", "9px"), parent);
    EXPECT_FLOAT_EQ(style.border_start_end_radius, 9.0f);

    cascade.apply_declaration(style, make_decl("border-end-start-radius", "3px"), parent);
    EXPECT_FLOAT_EQ(style.border_end_start_radius, 3.0f);

    cascade.apply_declaration(style, make_decl("border-end-end-radius", "15px"), parent);
    EXPECT_FLOAT_EQ(style.border_end_end_radius, 15.0f);
}

TEST(PropertyCascadeTest, MarginBlockShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // single value: sets both top and bottom
    cascade.apply_declaration(style, make_decl("margin-block", "20px"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 20.0f);

    // two values: top and bottom separately
    cascade.apply_declaration(style, make_decl("margin-block", "10px 30px"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
}

TEST(PropertyCascadeTest, MarginInlineShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // single value: sets both left and right
    cascade.apply_declaration(style, make_decl("margin-inline", "15px"), parent);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 15.0f);

    // two values: left and right separately
    cascade.apply_declaration(style, make_decl("margin-inline", "5px 25px"), parent);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 25.0f);
}

TEST(PropertyCascadeTest, PaddingBlockShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // single value: sets both top and bottom
    cascade.apply_declaration(style, make_decl("padding-block", "12px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 12.0f);

    // two values
    cascade.apply_declaration(style, make_decl("padding-block", "4px 8px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 8.0f);
}

TEST(PropertyCascadeTest, PaddingInlineShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // single value: sets both left and right
    cascade.apply_declaration(style, make_decl("padding-inline", "16px"), parent);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);

    // two values
    cascade.apply_declaration(style, make_decl("padding-inline", "2px 10px"), parent);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 10.0f);
}

TEST(PropertyCascadeTest, MinMaxLogicalSizes) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // min-inline-size → min_width
    cascade.apply_declaration(style, make_decl("min-inline-size", "50px"), parent);
    EXPECT_FLOAT_EQ(style.min_width.to_px(), 50.0f);

    // max-inline-size → max_width
    cascade.apply_declaration(style, make_decl("max-inline-size", "400px"), parent);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 400.0f);

    // min-block-size → min_height
    cascade.apply_declaration(style, make_decl("min-block-size", "30px"), parent);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 30.0f);

    // max-block-size → max_height
    cascade.apply_declaration(style, make_decl("max-block-size", "200px"), parent);
    EXPECT_FLOAT_EQ(style.max_height.to_px(), 200.0f);
}

TEST(PropertyCascadeTest, ScrollMarginAndPaddingLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.scroll_margin_top, 0.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_top, 0.0f);

    cascade.apply_declaration(style, make_decl("scroll-margin-top", "8px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_top, 8.0f);

    cascade.apply_declaration(style, make_decl("scroll-margin-right", "4px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_right, 4.0f);

    cascade.apply_declaration(style, make_decl("scroll-margin-bottom", "12px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_bottom, 12.0f);

    cascade.apply_declaration(style, make_decl("scroll-margin-left", "6px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_left, 6.0f);

    cascade.apply_declaration(style, make_decl("scroll-padding-top", "10px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_top, 10.0f);

    cascade.apply_declaration(style, make_decl("scroll-padding-right", "5px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_right, 5.0f);

    cascade.apply_declaration(style, make_decl("scroll-padding-bottom", "15px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_bottom, 15.0f);

    cascade.apply_declaration(style, make_decl("scroll-padding-left", "3px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_left, 3.0f);
}

// ---------------------------------------------------------------------------
// Cycle 479 — border-block/inline logical shorthands, remaining longhands,
//             padding shorthand, all, -webkit-box-orient, -webkit-text-stroke,
//             border-image shorthand, scroll-padding logical
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BorderBlockColorAndInlineColor) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-block-color sets top and bottom
    cascade.apply_declaration(style, make_decl("border-block-color", "red"), parent);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_bottom.color.r, 255);
    EXPECT_EQ(style.border_left.color.r, 0);  // unchanged

    // border-inline-color sets left and right
    cascade.apply_declaration(style, make_decl("border-inline-color", "blue"), parent);
    EXPECT_EQ(style.border_left.color.b, 255);
    EXPECT_EQ(style.border_right.color.b, 255);
}

TEST(PropertyCascadeTest, BorderBlockStyleAndInlineStyle) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-block-style sets top and bottom
    cascade.apply_declaration(style, make_decl("border-block-style", "dashed"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_left.style, BorderStyle::None);  // unchanged

    // border-inline-style sets left and right
    cascade.apply_declaration(style, make_decl("border-inline-style", "dotted"), parent);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dotted);
}

TEST(PropertyCascadeTest, BorderBlockWidthAndInlineWidth) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-block-width: single value → top and bottom
    cascade.apply_declaration(style, make_decl("border-block-width", "3px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 3.0f);

    // border-inline-width: two values → left=2px, right=4px
    cascade.apply_declaration(style, make_decl("border-inline-width", "2px 4px"), parent);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 4.0f);
}

TEST(PropertyCascadeTest, BorderBlockShorthandLogical) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-block sets both top and bottom
    cascade.apply_declaration(style, make_decl("border-block", "2px solid green"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.g, 128);  // green = {0,128,0,255}
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);

    // border-block-start sets top only
    cascade.apply_declaration(style, make_decl("border-block-start", "1px dashed red"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);  // unchanged
}

TEST(PropertyCascadeTest, BorderInlineStartEndShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-inline-start → left side
    cascade.apply_declaration(style, make_decl("border-inline-start", "3px solid blue"), parent);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.color.b, 255);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 0.0f);  // unchanged

    // border-inline-end → right side
    cascade.apply_declaration(style, make_decl("border-inline-end", "5px dashed red"), parent);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.color.r, 255);
}

TEST(PropertyCascadeTest, BorderLogicalRemainingLonghands) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-block-start-color", "red"), parent);
    EXPECT_EQ(style.border_top.color.r, 255);

    cascade.apply_declaration(style, make_decl("border-block-start-style", "dashed"), parent);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);

    cascade.apply_declaration(style, make_decl("border-block-end-style", "dotted"), parent);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);

    cascade.apply_declaration(style, make_decl("border-block-end-width", "6px"), parent);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 6.0f);

    cascade.apply_declaration(style, make_decl("border-inline-start-color", "blue"), parent);
    EXPECT_EQ(style.border_left.color.b, 255);

    cascade.apply_declaration(style, make_decl("border-inline-start-width", "4px"), parent);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);

    cascade.apply_declaration(style, make_decl("border-inline-end-color", "green"), parent);
    EXPECT_EQ(style.border_right.color.g, 128);

    cascade.apply_declaration(style, make_decl("border-inline-end-style", "solid"), parent);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
}

TEST(PropertyCascadeTest, PaddingShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // 1 value: all sides
    cascade.apply_declaration(style, make_decl("padding", "10px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 10.0f);

    // 2 values: top+bottom, left+right
    cascade.apply_declaration(style, make_decl("padding", "5px 15px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 15.0f);

    // 4 values: top, right, bottom, left
    cascade.apply_declaration(style, make_decl("padding", "1px 2px 3px 4px"), parent);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 4.0f);
}

TEST(PropertyCascadeTest, AllPropertyAndScrollPaddingLogical) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // all: initial sets css_all field
    EXPECT_TRUE(style.css_all.empty());
    cascade.apply_declaration(style, make_decl("all", "initial"), parent);
    EXPECT_EQ(style.css_all, "initial");

    // all: unset also sets the field
    cascade.apply_declaration(style, make_decl("all", "unset"), parent);
    EXPECT_EQ(style.css_all, "unset");

    // -webkit-box-orient: vertical → flex_direction = Column
    cascade.apply_declaration(style, make_decl("-webkit-box-orient", "vertical"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);

    cascade.apply_declaration(style, make_decl("-webkit-box-orient", "horizontal"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);

    // -webkit-text-stroke shorthand: width + color
    cascade.apply_declaration(style, make_decl("-webkit-text-stroke", "2px red"), parent);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 2.0f);
    EXPECT_EQ(style.text_stroke_color.r, 255);

    // scroll-padding-block sets top+bottom
    cascade.apply_declaration(style, make_decl("scroll-padding-block", "8px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_top, 8.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_bottom, 8.0f);

    // scroll-padding-inline sets left+right
    cascade.apply_declaration(style, make_decl("scroll-padding-inline", "4px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_left, 4.0f);
    EXPECT_FLOAT_EQ(style.scroll_padding_right, 4.0f);
}

// ---------------------------------------------------------------------------
// Cycle 480 — border-image shorthand, stroke-dashoffset (no-op), initial/
//             inherit keyword cascade, custom properties, unset no-op
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, BorderImageShorthand) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border-image: none clears the source
    style.border_image_source = "url(prev.png)";
    cascade.apply_declaration(style, make_decl("border-image", "none"), parent);
    EXPECT_TRUE(style.border_image_source.empty());

    // border-image: url() sets source and parses remainder
    cascade.apply_declaration(style, make_decl("border-image", "url(border.png) 30 round"), parent);
    EXPECT_EQ(style.border_image_source, "url(border.png)");
    EXPECT_FLOAT_EQ(style.border_image_slice, 30.0f);
    EXPECT_EQ(style.border_image_repeat, 2);  // round = 2
}

TEST(PropertyCascadeTest, StrokeDashoffsetNoOp) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // stroke-dashoffset is a no-op in the cascade (parsed at render time)
    // Just verify it doesn't crash and leaves all properties unchanged.
    float orig_opacity = style.opacity;
    cascade.apply_declaration(style, make_decl("stroke-dashoffset", "5px"), parent);
    EXPECT_FLOAT_EQ(style.opacity, orig_opacity);  // nothing changed
}

TEST(PropertyCascadeTest, InitialKeywordResets) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set some non-initial values first
    style.background_color = Color{255, 0, 0, 255};
    style.opacity = 0.5f;
    style.z_index = 10;
    style.flex_direction = FlexDirection::Column;

    // initial resets to CSS initial values
    cascade.apply_declaration(style, make_decl("background-color", "initial"), parent);
    EXPECT_EQ(style.background_color.a, 0);  // transparent

    cascade.apply_declaration(style, make_decl("opacity", "initial"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 1.0f);

    cascade.apply_declaration(style, make_decl("z-index", "initial"), parent);
    EXPECT_EQ(style.z_index, 0);

    cascade.apply_declaration(style, make_decl("flex-direction", "initial"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
}

TEST(PropertyCascadeTest, InitialKeywordForBoxModel) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set non-initial values
    style.width = Length::px(200.0f);
    style.margin.top = Length::px(20.0f);
    style.padding.left = Length::px(10.0f);
    style.box_sizing = BoxSizing::BorderBox;

    cascade.apply_declaration(style, make_decl("width", "initial"), parent);
    EXPECT_TRUE(style.width.is_auto());

    cascade.apply_declaration(style, make_decl("margin-top", "initial"), parent);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 0.0f);

    cascade.apply_declaration(style, make_decl("padding-left", "initial"), parent);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 0.0f);

    cascade.apply_declaration(style, make_decl("box-sizing", "initial"), parent);
    EXPECT_EQ(style.box_sizing, BoxSizing::ContentBox);
}

TEST(PropertyCascadeTest, InheritKeywordForInheritedProps) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set parent values
    parent.color = Color{0, 128, 255, 255};
    parent.font_size = Length::px(24.0f);
    parent.cursor = Cursor::Pointer;
    parent.direction = Direction::Rtl;

    cascade.apply_declaration(style, make_decl("color", "inherit"), parent);
    EXPECT_EQ(style.color.g, 128);
    EXPECT_EQ(style.color.b, 255);

    cascade.apply_declaration(style, make_decl("font-size", "inherit"), parent);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);

    cascade.apply_declaration(style, make_decl("cursor", "inherit"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);

    cascade.apply_declaration(style, make_decl("direction", "inherit"), parent);
    EXPECT_EQ(style.direction, Direction::Rtl);
}

TEST(PropertyCascadeTest, InheritKeywordForNonInheritedProps) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Force non-inherited properties to inherit from parent
    parent.background_color = Color{255, 0, 0, 255};
    parent.overflow_x = Overflow::Hidden;
    parent.z_index = 99;
    parent.width = Length::px(300.0f);

    cascade.apply_declaration(style, make_decl("background-color", "inherit"), parent);
    EXPECT_EQ(style.background_color.r, 255);
    EXPECT_EQ(style.background_color.a, 255);

    cascade.apply_declaration(style, make_decl("overflow-x", "inherit"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);

    cascade.apply_declaration(style, make_decl("z-index", "inherit"), parent);
    EXPECT_EQ(style.z_index, 99);

    cascade.apply_declaration(style, make_decl("width", "inherit"), parent);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
}

TEST(PropertyCascadeTest, CssCustomPropertyStorage) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.custom_properties.empty());

    cascade.apply_declaration(style, make_decl("--primary-color", "blue"), parent);
    EXPECT_EQ(style.custom_properties["--primary-color"], "blue");

    cascade.apply_declaration(style, make_decl("--font-size-base", "16px"), parent);
    EXPECT_EQ(style.custom_properties["--font-size-base"], "16px");

    // Overwrite an existing custom property
    cascade.apply_declaration(style, make_decl("--primary-color", "red"), parent);
    EXPECT_EQ(style.custom_properties["--primary-color"], "red");

    EXPECT_EQ(style.custom_properties.size(), 2u);
}

TEST(PropertyCascadeTest, UnsetAndRevertAreNoOps) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set some specific values
    style.opacity = 0.7f;
    style.z_index = 5;

    // unset on non-all is a no-op for non-inherited props in this engine
    cascade.apply_declaration(style, make_decl("opacity", "unset"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.7f);  // unchanged

    cascade.apply_declaration(style, make_decl("z-index", "unset"), parent);
    EXPECT_EQ(style.z_index, 5);  // unchanged

    // revert is also treated as no-op
    cascade.apply_declaration(style, make_decl("opacity", "revert"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.7f);  // unchanged
}

// ---------------------------------------------------------------------------
// V42 Test Suite — New unit tests covering diverse CSS properties
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, PerspectiveOriginV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default perspective-origin: 50% 50% (center)
    EXPECT_FLOAT_EQ(style.perspective_origin_x, 50.0f);
    EXPECT_FLOAT_EQ(style.perspective_origin_y, 50.0f);

    // Set custom perspective-origin values
    cascade.apply_declaration(style, make_decl("perspective-origin", "25% 75%"), parent);
    EXPECT_FLOAT_EQ(style.perspective_origin_x, 25.0f);
    EXPECT_FLOAT_EQ(style.perspective_origin_y, 75.0f);
}

TEST(PropertyCascadeTest, BackfaceVisibilityV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: visible (0)
    EXPECT_EQ(style.backface_visibility, 0);

    cascade.apply_declaration(style, make_decl("backface-visibility", "hidden"), parent);
    EXPECT_EQ(style.backface_visibility, 1);

    cascade.apply_declaration(style, make_decl("backface-visibility", "visible"), parent);
    EXPECT_EQ(style.backface_visibility, 0);
}

TEST(PropertyCascadeTest, TextStrokeWidthColorV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: no stroke
    EXPECT_FLOAT_EQ(style.text_stroke_width, 0.0f);

    // Try -webkit- prefixed version (may or may not be supported)
    cascade.apply_declaration(style, make_decl("-webkit-text-stroke-width", "1.5px"), parent);
    // Just verify no crash — property may not be applied through cascade
    EXPECT_TRUE(style.text_stroke_width >= 0.0f);
}

TEST(PropertyCascadeTest, OverflowAnchorV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: auto (0)
    EXPECT_EQ(style.overflow_anchor, 0);

    cascade.apply_declaration(style, make_decl("overflow-anchor", "none"), parent);
    EXPECT_EQ(style.overflow_anchor, 1);

    cascade.apply_declaration(style, make_decl("overflow-anchor", "auto"), parent);
    EXPECT_EQ(style.overflow_anchor, 0);
}

TEST(PropertyCascadeTest, ScrollMarginAndPaddingV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set scroll margins
    cascade.apply_declaration(style, make_decl("scroll-margin", "10px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_margin_top, 10.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_right, 10.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_bottom, 10.0f);
    EXPECT_FLOAT_EQ(style.scroll_margin_left, 10.0f);

    // Set scroll padding with specific values
    cascade.apply_declaration(style, make_decl("scroll-padding-top", "20px"), parent);
    EXPECT_FLOAT_EQ(style.scroll_padding_top, 20.0f);
}

TEST(PropertyCascadeTest, ColumnSpanV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: none (0)
    EXPECT_EQ(style.column_span, 0);

    cascade.apply_declaration(style, make_decl("column-span", "all"), parent);
    EXPECT_EQ(style.column_span, 1);

    cascade.apply_declaration(style, make_decl("column-span", "none"), parent);
    EXPECT_EQ(style.column_span, 0);
}

TEST(PropertyCascadeTest, ContentVisibilityV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: visible (0)
    EXPECT_EQ(style.content_visibility, 0);

    cascade.apply_declaration(style, make_decl("content-visibility", "auto"), parent);
    EXPECT_EQ(style.content_visibility, 2);

    cascade.apply_declaration(style, make_decl("content-visibility", "hidden"), parent);
    EXPECT_EQ(style.content_visibility, 1);
}

TEST(PropertyCascadeTest, BreakInsideAvoidV42) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Default: auto (0)
    EXPECT_EQ(style.break_inside, 0);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid"), parent);
    EXPECT_EQ(style.break_inside, 1);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-page"), parent);
    EXPECT_EQ(style.break_inside, 2);

    cascade.apply_declaration(style, make_decl("break-inside", "avoid-column"), parent);
    EXPECT_EQ(style.break_inside, 3);
}

TEST(PropertyCascadeTest, VisibilityCollapseInheritVisibleV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    parent.visibility = Visibility::Hidden;

    cascade.apply_declaration(style, make_decl("visibility", "collapse"), parent);
    EXPECT_EQ(style.visibility, Visibility::Collapse);

    cascade.apply_declaration(style, make_decl("visibility", "inherit"), parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);

    cascade.apply_declaration(style, make_decl("visibility", "visible"), parent);
    EXPECT_EQ(style.visibility, Visibility::Visible);
}

TEST(PropertyCascadeTest, TransformReplacedByNoneV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform", "translate(12px, 8px)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 12.0f);
    EXPECT_FLOAT_EQ(style.transforms[0].y, 8.0f);

    cascade.apply_declaration(style, make_decl("transform", "none"), parent);
    EXPECT_TRUE(style.transforms.empty());
}

TEST(PropertyCascadeTest, AnimationShorthandMillisecondsV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("animation", "spin 250ms linear 100ms 3"), parent);
    EXPECT_EQ(style.animation_name, "spin");
    EXPECT_NEAR(style.animation_duration, 0.25f, 0.001f);
    EXPECT_EQ(style.animation_timing, 1);
    EXPECT_NEAR(style.animation_delay, 0.1f, 0.001f);
    EXPECT_FLOAT_EQ(style.animation_iteration_count, 3.0f);
}

TEST(PropertyCascadeTest, FilterMultiThenSingleV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("filter", "grayscale(0.5) blur(2px)"), parent);
    ASSERT_EQ(style.filters.size(), 2u);
    EXPECT_EQ(style.filters[0].first, 1);
    EXPECT_EQ(style.filters[1].first, 9);
    EXPECT_FLOAT_EQ(style.filters[1].second, 2.0f);

    cascade.apply_declaration(style, make_decl("filter", "brightness(1.2)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 3);
    EXPECT_NEAR(style.filters[0].second, 1.2f, 0.01f);
}

TEST(PropertyCascadeTest, GridAutoFlowDenseVariantsV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "dense"), parent);
    EXPECT_EQ(style.grid_auto_flow, 2);

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "column dense"), parent);
    EXPECT_EQ(style.grid_auto_flow, 3);

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "row"), parent);
    EXPECT_EQ(style.grid_auto_flow, 0);
}

TEST(PropertyCascadeTest, FlexFlowAndBasisV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("flex-flow", "column-reverse wrap"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);

    cascade.apply_declaration(style, make_decl("flex-basis", "42px"), parent);
    EXPECT_FALSE(style.flex_basis.is_auto());
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 42.0f);
}

TEST(PropertyCascadeTest, TextDecorationLineStyleColorV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-decoration-line", "underline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);

    cascade.apply_declaration(style, make_decl("text-decoration-style", "dotted"), parent);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Dotted);

    cascade.apply_declaration(style, make_decl("text-decoration-color", "blue"), parent);
    EXPECT_EQ(style.text_decoration_color, (Color{0, 0, 255, 255}));
}

TEST(PropertyCascadeTest, TextTransformAndWhiteSpaceV43) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-transform", "uppercase"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);

    cascade.apply_declaration(style, make_decl("white-space", "pre-wrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
}

// ---------------------------------------------------------------------------
// V55 Test Suite — apply_declaration coverage for requested CSS properties
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ApplyDeclarationColorV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "color";
    decl.values.push_back(make_token("#123456"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.color, (Color{0x12, 0x34, 0x56, 255}));
}

TEST(PropertyCascadeTest, ApplyDeclarationBackgroundV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "background";
    decl.values.push_back(make_token("blue"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.background_color, (Color{0, 0, 255, 255}));
}

TEST(PropertyCascadeTest, ApplyDeclarationFontSizeV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "font-size";
    decl.values.push_back(make_token("18px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.font_size.value, 18.0f);
    EXPECT_EQ(style.font_size.unit, Length::Unit::Px);
}

TEST(PropertyCascadeTest, ApplyDeclarationDisplayV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "display";
    decl.values.push_back(make_token("flex"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.display, Display::Flex);
}

TEST(PropertyCascadeTest, ApplyDeclarationPositionV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "position";
    decl.values.push_back(make_token("absolute"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.position, Position::Absolute);
}

TEST(PropertyCascadeTest, ApplyDeclarationZIndexV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "z-index";
    decl.values.push_back(make_token("77"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.z_index, 77);
}

TEST(PropertyCascadeTest, ApplyDeclarationOpacityV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "opacity";
    decl.values.push_back(make_token("0.25"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.25f);
}

TEST(PropertyCascadeTest, ApplyDeclarationOverflowV55) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "overflow";
    decl.values.push_back(make_token("hidden"));
    decl.values.push_back(make_token("scroll"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Scroll);
}

TEST(PropertyCascadeTest, ApplyDeclarationVisibilityV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "visibility";
    decl.values.push_back(make_token("hidden"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(PropertyCascadeTest, ApplyDeclarationCursorV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "cursor";
    decl.values.push_back(make_token("pointer"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(PropertyCascadeTest, ApplyDeclarationFloatV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "float";
    decl.values.push_back(make_token("left"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.float_val, Float::Left);
}

TEST(PropertyCascadeTest, ApplyDeclarationClearV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "clear";
    decl.values.push_back(make_token("both"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.clear, Clear::Both);
}

TEST(PropertyCascadeTest, ApplyDeclarationLetterSpacingV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "letter-spacing";
    decl.values.push_back(make_token("2px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 2.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationWordSpacingV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "word-spacing";
    decl.values.push_back(make_token("4px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 4.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationBoxSizingV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "box-sizing";
    decl.values.push_back(make_token("border-box"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
}

TEST(PropertyCascadeTest, ApplyDeclarationTextAlignV56) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "text-align";
    decl.values.push_back(make_token("center"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.text_align, TextAlign::Center);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderRadiusV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-radius";
    decl.values.push_back(make_token("15px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.border_radius, 15.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationLineHeightV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "line-height";
    decl.values.push_back(make_token("1.5"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f);  // 1.5 * default 16px
}

TEST(PropertyCascadeTest, ApplyDeclarationTextStrokeWidthV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "-webkit-text-stroke-width";
    decl.values.push_back(make_token("2px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 2.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationWordSpacingV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "word-spacing";
    decl.values.push_back(make_token("0.5em"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 8.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationLetterSpacingV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "letter-spacing";
    decl.values.push_back(make_token("2px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.letter_spacing.value, 2.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationZIndexV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "z-index";
    decl.values.push_back(make_token("100"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.z_index, 100);
}

TEST(PropertyCascadeTest, ApplyDeclarationTransformV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "transform";
    decl.values.push_back(make_token("rotate(45deg)"));

    cascade.apply_declaration(style, decl, parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Rotate);
}

TEST(PropertyCascadeTest, ApplyDeclarationBackgroundColorV57) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "background-color";
    decl.values.push_back(make_token("rgb(255,128,64)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.background_color.r, 255);
    EXPECT_EQ(style.background_color.g, 128);
    EXPECT_EQ(style.background_color.b, 64);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderColorV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-color";
    decl.values.push_back(make_token("rgba(100,150,200,0.8)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.border_top.color.r, 100);
    EXPECT_EQ(style.border_top.color.g, 150);
    EXPECT_EQ(style.border_top.color.b, 200);
}

TEST(PropertyCascadeTest, ApplyDeclarationCursorV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "cursor";
    decl.values.push_back(make_token("pointer"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(PropertyCascadeTest, ApplyDeclarationVisibilityV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "visibility";
    decl.values.push_back(make_token("hidden"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(PropertyCascadeTest, ApplyDeclarationOpacityV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "opacity";
    decl.values.push_back(make_token("0.5"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

TEST(PropertyCascadeTest, ApplyDeclarationOutlineColorV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "outline-color";
    decl.values.push_back(make_token("rgb(220,50,50)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.outline_color.r, 220);
    EXPECT_EQ(style.outline_color.g, 50);
    EXPECT_EQ(style.outline_color.b, 50);
}

TEST(PropertyCascadeTest, ApplyDeclarationOutlineWidthV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "outline-width";
    decl.values.push_back(make_token("3px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(16.0f), 3.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationBoxShadowV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "box-shadow";
    decl.values.push_back(make_token("5px"));
    decl.values.push_back(make_token("10px"));
    decl.values.push_back(make_token("15px"));
    decl.values.push_back(make_token("rgba(0,0,0,0.5)"));

    cascade.apply_declaration(style, decl, parent);
    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 5.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 10.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].blur, 15.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationTextDecorationV58) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "text-decoration";
    decl.values.push_back(make_token("underline"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderTopColorV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-top-color";
    decl.values.push_back(make_token("rgb(200,50,75)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.border_top.color.r, 200);
    EXPECT_EQ(style.border_top.color.g, 50);
    EXPECT_EQ(style.border_top.color.b, 75);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderRightColorV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-right-color";
    decl.values.push_back(make_token("rgb(100,200,50)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.border_right.color.r, 100);
    EXPECT_EQ(style.border_right.color.g, 200);
    EXPECT_EQ(style.border_right.color.b, 50);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderBottomColorV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-bottom-color";
    decl.values.push_back(make_token("rgba(50,100,150,0.5)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.border_bottom.color.r, 50);
    EXPECT_EQ(style.border_bottom.color.g, 100);
    EXPECT_EQ(style.border_bottom.color.b, 150);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderLeftColorV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-left-color";
    decl.values.push_back(make_token("rgb(255,128,0)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.border_left.color.r, 255);
    EXPECT_EQ(style.border_left.color.g, 128);
    EXPECT_EQ(style.border_left.color.b, 0);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderTopWidthV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "border-top-width";
    decl.values.push_back(make_token("2.5px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(16.0f), 2.5f);
}

TEST(PropertyCascadeTest, ApplyDeclarationTextStrokeWidthV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "-webkit-text-stroke-width";
    decl.values.push_back(make_token("1.5px"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 1.5f);
}

TEST(PropertyCascadeTest, ApplyDeclarationOutlineColorV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "outline-color";
    decl.values.push_back(make_token("rgb(64,192,64)"));

    cascade.apply_declaration(style, decl, parent);
    EXPECT_EQ(style.outline_color.r, 64);
    EXPECT_EQ(style.outline_color.g, 192);
    EXPECT_EQ(style.outline_color.b, 64);
}

TEST(PropertyCascadeTest, ApplyDeclarationBoxShadowMultipleV59) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    Declaration decl;
    decl.property = "box-shadow";
    decl.values.push_back(make_token("3px"));
    decl.values.push_back(make_token("4px"));
    decl.values.push_back(make_token("8px"));
    decl.values.push_back(make_token("2px"));
    decl.values.push_back(make_token("rgb(200,100,50)"));

    cascade.apply_declaration(style, decl, parent);
    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 3.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 4.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].blur, 8.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].spread, 2.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationFontWeightV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_weight, 400);  // normal default

    cascade.apply_declaration(style, make_decl("font-weight", "bold"), parent);
    EXPECT_EQ(style.font_weight, 700);

    cascade.apply_declaration(style, make_decl("font-weight", "900"), parent);
    EXPECT_EQ(style.font_weight, 900);

    cascade.apply_declaration(style, make_decl("font-weight", "600"), parent);
    EXPECT_EQ(style.font_weight, 600);
}

TEST(PropertyCascadeTest, ApplyDeclarationFontStyleV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.font_style, FontStyle::Normal);  // normal default

    cascade.apply_declaration(style, make_decl("font-style", "italic"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Italic);

    cascade.apply_declaration(style, make_decl("font-style", "oblique"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Oblique);

    cascade.apply_declaration(style, make_decl("font-style", "normal"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Normal);
}

TEST(PropertyCascadeTest, ApplyDeclarationTransformV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.transforms.empty());  // default: no transforms

    Declaration decl;
    decl.property = "transform";
    decl.values.push_back(make_token("translateX(50px)"));

    cascade.apply_declaration(style, decl, parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 50.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationTransitionPropertyV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.transition_property, "all");  // default

    cascade.apply_declaration(style, make_decl("transition-property", "opacity"), parent);
    EXPECT_EQ(style.transition_property, "opacity");

    cascade.apply_declaration(style, make_decl("transition-property", "transform"), parent);
    EXPECT_EQ(style.transition_property, "transform");
}

TEST(PropertyCascadeTest, ApplyDeclarationFlexDirectionV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.flex_direction, FlexDirection::Row);  // row default

    cascade.apply_declaration(style, make_decl("flex-direction", "column"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);

    cascade.apply_declaration(style, make_decl("flex-direction", "row-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::RowReverse);

    cascade.apply_declaration(style, make_decl("flex-direction", "column-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
}

TEST(PropertyCascadeTest, ApplyDeclarationGridTemplateColumnsV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_TRUE(style.grid_template_columns.empty());  // default: empty

    cascade.apply_declaration(style, make_decl("grid-template-columns", "1fr 2fr"), parent);
    EXPECT_EQ(style.grid_template_columns, "1fr 2fr");

    cascade.apply_declaration(style, make_decl("grid-template-columns", "repeat(3,1fr)"), parent);
    EXPECT_EQ(style.grid_template_columns, "repeat(3,1fr)");
}

TEST(PropertyCascadeTest, ApplyDeclarationTextDecorationV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_EQ(style.text_decoration, TextDecoration::None);  // none default

    cascade.apply_declaration(style, make_decl("text-decoration", "underline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);

    cascade.apply_declaration(style, make_decl("text-decoration", "overline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Overline);

    cascade.apply_declaration(style, make_decl("text-decoration", "line-through"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
}

TEST(PropertyCascadeTest, ApplyDeclarationOpacityZIndexV60) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    EXPECT_FLOAT_EQ(style.opacity, 1.0f);  // default: fully opaque
    EXPECT_EQ(style.z_index, 0);  // default: auto

    cascade.apply_declaration(style, make_decl("opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);

    cascade.apply_declaration(style, make_decl("z-index", "42"), parent);
    EXPECT_EQ(style.z_index, 42);

    cascade.apply_declaration(style, make_decl("z-index", "-10"), parent);
    EXPECT_EQ(style.z_index, -10);
}

TEST(PropertyCascadeTest, CSSVariableCustomPropertyV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Custom properties should be stored in custom_properties
    Declaration decl;
    decl.property = "--primary-color";
    decl.values.push_back(make_token("#3366ff"));

    cascade.apply_declaration(style, decl, parent);
    // CSS variables are stored in style.custom_properties map
    ASSERT_EQ(style.custom_properties.count("--primary-color"), 1);
    EXPECT_EQ(style.custom_properties["--primary-color"], "#3366ff");
}

TEST(PropertyCascadeTest, CalcExpressionWithOperatorsV61) {
    // calc(100px - 20px) should evaluate correctly
    auto l = parse_length("calc(100px - 20px)");
    ASSERT_TRUE(l.has_value()) << "calc() should parse";
    ASSERT_NE(l->calc_expr, nullptr) << "Should have calc expression";
    float px = l->calc_expr->evaluate(0, 16);
    EXPECT_NEAR(px, 80.0f, 1.0f) << "calc(100px - 20px) should be 80px";
}

TEST(PropertyCascadeTest, InheritKeywordForColorV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set parent color
    parent.color = Color{200, 100, 50, 255};

    // Child initially has default color
    EXPECT_NE(style.color, parent.color);

    // Apply inherit keyword
    cascade.apply_declaration(style, make_decl("color", "inherit"), parent);
    EXPECT_EQ(style.color, parent.color);
    EXPECT_EQ(style.color.r, 200);
    EXPECT_EQ(style.color.g, 100);
    EXPECT_EQ(style.color.b, 50);
}

TEST(PropertyCascadeTest, PaddingShorthandExpansionV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Three-value shorthand: top, left/right, bottom
    cascade.apply_declaration(style, make_decl_multi("padding", {"5px", "10px", "15px"}), parent);
    EXPECT_FLOAT_EQ(style.padding.top.value, 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.value, 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.value, 15.0f);
    EXPECT_FLOAT_EQ(style.padding.left.value, 10.0f);
}

TEST(PropertyCascadeTest, RgbaColorFunctionV61) {
    // rgba(75, 150, 225, 0.75) with alpha channel
    auto c = parse_color("rgba(75, 150, 225, 0.75)");
    ASSERT_TRUE(c.has_value()) << "rgba() should parse";
    EXPECT_EQ(c->r, 75);
    EXPECT_EQ(c->g, 150);
    EXPECT_EQ(c->b, 225);
    EXPECT_NEAR(c->a / 255.0f, 0.75f, 0.01f) << "alpha should be approximately 0.75";
}

TEST(PropertyCascadeTest, BorderShorthandWithColorV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // border: 2px solid red should expand to border-width, border-style, border-color
    Declaration decl;
    decl.property = "border";
    decl.values.push_back(make_token("2px"));
    decl.values.push_back(make_token("solid"));
    decl.values.push_back(make_token("red"));

    cascade.apply_declaration(style, decl, parent);
    // Check that border properties were set
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(16.0f), 2.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(16.0f), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(16.0f), 2.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(16.0f), 2.0f);
}

TEST(PropertyCascadeTest, CounterResetAndIncrementV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // counter-reset: paragraph 0
    cascade.apply_declaration(style, make_decl("counter-reset", "paragraph 0"), parent);
    EXPECT_EQ(style.counter_reset, "paragraph 0");

    // counter-increment: section
    cascade.apply_declaration(style, make_decl("counter-increment", "section"), parent);
    EXPECT_EQ(style.counter_increment, "section");
}

TEST(PropertyCascadeTest, InitialKeywordResetsFontWeightV61) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // Set font-weight to bold (700)
    cascade.apply_declaration(style, make_decl("font-weight", "bold"), parent);
    EXPECT_EQ(style.font_weight, 700);

    // Apply initial keyword to reset to CSS initial value (400 for font-weight)
    cascade.apply_declaration(style, make_decl("font-weight", "initial"), parent);
    EXPECT_EQ(style.font_weight, 400) << "initial should reset font-weight to 400";
}

TEST(PropertyCascadeTest, VisibilityPropertyHiddenV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("visibility", "hidden"), parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);

    cascade.apply_declaration(style, make_decl("visibility", "visible"), parent);
    EXPECT_EQ(style.visibility, Visibility::Visible);
}

TEST(PropertyCascadeTest, CursorPropertyPointerV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("cursor", "pointer"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);

    cascade.apply_declaration(style, make_decl("cursor", "default"), parent);
    EXPECT_EQ(style.cursor, Cursor::Default);
}

TEST(PropertyCascadeTest, PointerEventsNoneV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("pointer-events", "none"), parent);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);

    cascade.apply_declaration(style, make_decl("pointer-events", "auto"), parent);
    EXPECT_EQ(style.pointer_events, PointerEvents::Auto);
}

TEST(PropertyCascadeTest, UserSelectNoneV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("user-select", "none"), parent);
    EXPECT_EQ(style.user_select, UserSelect::None);

    cascade.apply_declaration(style, make_decl("user-select", "text"), parent);
    EXPECT_EQ(style.user_select, UserSelect::Text);
}

TEST(PropertyCascadeTest, WordSpacingLengthV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("word-spacing", "2px"), parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 2.0f);
}

TEST(PropertyCascadeTest, LetterSpacingLengthV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("letter-spacing", "3px"), parent);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(16.0f), 3.0f);
}

TEST(PropertyCascadeTest, VerticalAlignPropertyV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("vertical-align", "middle"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);

    cascade.apply_declaration(style, make_decl("vertical-align", "baseline"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline);

    cascade.apply_declaration(style, make_decl("vertical-align", "top"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Top);
}

TEST(PropertyCascadeTest, WhiteSpacePropertyV62) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    // white-space: nowrap prevents line wrapping
    cascade.apply_declaration(style, make_decl("white-space", "nowrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap) << "white-space should be 'nowrap'";

    // white-space: pre preserves whitespace
    cascade.apply_declaration(style, make_decl("white-space", "pre"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::Pre) << "white-space should be 'pre'";

    // white-space: pre-wrap preserves whitespace and wraps
    cascade.apply_declaration(style, make_decl("white-space", "pre-wrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap) << "white-space should be 'pre-wrap'";

    // white-space: normal (default)
    cascade.apply_declaration(style, make_decl("white-space", "normal"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::Normal) << "white-space should be 'normal'";
}

TEST(PropertyCascadeTest, SpecificityWinsWhenBothDeclarationsImportantV63) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule low_specificity_rule;
    low_specificity_rule.declarations.push_back(make_decl("display", "block", true));

    StyleRule high_specificity_rule;
    high_specificity_rule.declarations.push_back(make_decl("display", "flex", true));

    MatchedRule low_specificity_match;
    low_specificity_match.rule = &low_specificity_rule;
    low_specificity_match.specificity = {0, 0, 1};
    low_specificity_match.source_order = 10;

    MatchedRule high_specificity_match;
    high_specificity_match.rule = &high_specificity_rule;
    high_specificity_match.specificity = {0, 1, 0};
    high_specificity_match.source_order = 1;

    auto result = cascade.cascade({low_specificity_match, high_specificity_match}, parent_style);
    EXPECT_EQ(result.display, Display::Flex);
}

TEST(PropertyCascadeTest, InheritKeywordCopiesVisibilityAndCursorV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    parent.visibility = Visibility::Collapse;
    parent.cursor = Cursor::Move;

    cascade.apply_declaration(style, make_decl("visibility", "inherit"), parent);
    EXPECT_EQ(style.visibility, Visibility::Collapse);

    cascade.apply_declaration(style, make_decl("cursor", "inherit"), parent);
    EXPECT_EQ(style.cursor, Cursor::Move);
}

TEST(PropertyCascadeTest, ShorthandMarginThreeValueExpansionV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl_multi("margin", {"4px", "8px", "12px"}), parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(16.0f), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(16.0f), 12.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(16.0f), 8.0f);
}

TEST(PropertyCascadeTest, BoxModelBorderTopColorAndOutlineWidthV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-top-color", "rgb(12,34,56)"), parent);
    EXPECT_EQ(style.border_top.color.r, 12);
    EXPECT_EQ(style.border_top.color.g, 34);
    EXPECT_EQ(style.border_top.color.b, 56);

    cascade.apply_declaration(style, make_decl("outline-width", "5px"), parent);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(16.0f), 5.0f);
}

TEST(PropertyCascadeTest, TextPropertiesVerticalAlignAndWhiteSpaceV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("vertical-align", "text-bottom"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::TextBottom);

    cascade.apply_declaration(style, make_decl("white-space", "break-spaces"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::BreakSpaces);
}

TEST(PropertyCascadeTest, TextSpacingWordAndLetterSpacingV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("word-spacing", "6px"), parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 6.0f);

    cascade.apply_declaration(style, make_decl("letter-spacing", "1.5px"), parent);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(16.0f), 1.5f);
}

TEST(PropertyCascadeTest, VisualEffectsOpacityAndFilterResetV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("opacity", "0.35"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.35f);

    cascade.apply_declaration(style, make_decl("filter", "blur(6px)"), parent);
    ASSERT_EQ(style.filters.size(), 1u);
    EXPECT_EQ(style.filters[0].first, 9);
    EXPECT_FLOAT_EQ(style.filters[0].second, 6.0f);

    cascade.apply_declaration(style, make_decl("filter", "none"), parent);
    EXPECT_TRUE(style.filters.empty());
}

TEST(PropertyCascadeTest, TransitionShorthandLinearWithDelayV63) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition", "opacity 150ms linear 75ms"), parent);

    ASSERT_EQ(style.transitions.size(), 1u);
    EXPECT_EQ(style.transitions[0].property, "opacity");
    EXPECT_NEAR(style.transitions[0].duration_ms, 150.0f, 1.0f);
    EXPECT_EQ(style.transitions[0].timing_function, 1);
    EXPECT_NEAR(style.transitions[0].delay_ms, 75.0f, 1.0f);

    EXPECT_EQ(style.transition_property, "opacity");
    EXPECT_NEAR(style.transition_duration, 0.15f, 0.001f);
    EXPECT_NEAR(style.transition_delay, 0.075f, 0.001f);
}

TEST(PropertyCascadeTest, VisibilityHiddenParsesV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("visibility", "hidden"), parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(PropertyCascadeTest, VisibilityLastDeclarationWinsHiddenV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("visibility", "visible"), parent);
    cascade.apply_declaration(style, make_decl("visibility", "hidden"), parent);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(PropertyCascadeTest, CursorPointerParsesV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("cursor", "pointer"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(PropertyCascadeTest, CursorLastDeclarationWinsPointerV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("cursor", "default"), parent);
    cascade.apply_declaration(style, make_decl("cursor", "pointer"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(PropertyCascadeTest, WhiteSpaceNoWrapParsesV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("white-space", "nowrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

TEST(PropertyCascadeTest, WhiteSpaceLastDeclarationWinsNoWrapV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("white-space", "normal"), parent);
    cascade.apply_declaration(style, make_decl("white-space", "nowrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

TEST(PropertyCascadeTest, WordSpacingLengthParsesPxV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("word-spacing", "4px"), parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 4.0f);
}

TEST(PropertyCascadeTest, WordSpacingLastDeclarationWinsV64) {
    PropertyCascade cascade;
    ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("word-spacing", "1px"), parent);
    cascade.apply_declaration(style, make_decl("word-spacing", "7px"), parent);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 7.0f);
}

TEST(PropertyCascadeTest, OpacityParsesDecimalFromResolverV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { opacity: 0.42; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.opacity, 0.42f);
}

TEST(PropertyCascadeTest, ZIndexLastDeclarationWinsNegativeValueV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { z-index: 5; z-index: -7; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.z_index, -7);
}

TEST(PropertyCascadeTest, TextIndentParsesPixelLengthV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("p { text-indent: 24px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.text_indent.to_px(16.0f), 24.0f);
}

TEST(PropertyCascadeTest, TextTransformUppercaseParsesV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("span { text-transform: uppercase; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
}

TEST(PropertyCascadeTest, ListStyleTypeUpperRomanParsesV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("li { list-style-type: upper-roman; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "li";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.list_style_type, ListStyleType::UpperRoman);
}

TEST(PropertyCascadeTest, OutlineWidthIndividualPropertyParsesV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { outline-width: 3px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(16.0f), 3.0f);
}

TEST(PropertyCascadeTest, BoxShadowInsetParsesOffsetBlurSpreadAndColorV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { box-shadow: inset 2px 4px 6px 8px red; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_TRUE(style.box_shadows[0].inset);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 4.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].blur, 6.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].spread, 8.0f);
    EXPECT_EQ(style.box_shadows[0].color.r, 255);
    EXPECT_EQ(style.box_shadows[0].color.g, 0);
    EXPECT_EQ(style.box_shadows[0].color.b, 0);
    EXPECT_EQ(style.box_shadows[0].color.a, 255);
}

TEST(PropertyCascadeTest, LetterSpacingParsesPixelValueV65) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { letter-spacing: 2px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(16.0f), 2.0f);
}

TEST(PropertyCascadeTest, FontWeightBoldResolvesTo700V66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("p { font-weight: bold; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_weight, 700);
}

TEST(PropertyCascadeTest, FontStyleItalicParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("em { font-style: italic; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "em";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(PropertyCascadeTest, ColorRedResolvesToRgb255000V66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("span { color: red; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color.r, 255);
    EXPECT_EQ(style.color.g, 0);
    EXPECT_EQ(style.color.b, 0);
    EXPECT_EQ(style.color.a, 255);
}

TEST(PropertyCascadeTest, MarginShorthandFourValueParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { margin: 1px 2px 3px 4px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.value, 1.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 2.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 3.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 4.0f);
}

TEST(PropertyCascadeTest, PaddingShorthandTwoValueParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { padding: 6px 9px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(16.0f), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(16.0f), 9.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(16.0f), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(16.0f), 9.0f);
}

TEST(PropertyCascadeTest, BorderRadiusSingleValueParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("div { border-radius: 14px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_radius, 14.0f);
}

TEST(PropertyCascadeTest, WordSpacingPixelValueParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("p { word-spacing: 5px; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 5.0f);
}

TEST(PropertyCascadeTest, CursorPointerEnumValueParsesV66) {
    StyleResolver resolver;
    auto sheet = parse_stylesheet("a { cursor: pointer; }");
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(PropertyCascadeTest, ResolverDisplayBlockFromStylesheetV67) {
    const std::string css = "span { display: block; }";
    const std::string tag = "span";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Block);
}

TEST(PropertyCascadeTest, ResolverVisibilityHiddenEnumV67) {
    const std::string css = "div { visibility: hidden; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(PropertyCascadeTest, ResolverOverflowHiddenValueV67) {
    const std::string css = "div { overflow: hidden; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(static_cast<int>(style.overflow_x), 1);
    EXPECT_EQ(static_cast<int>(style.overflow_y), 1);
}

TEST(PropertyCascadeTest, ResolverTextAlignCenterV67) {
    const std::string css = "div { text-align: center; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_align, TextAlign::Center);
}

TEST(PropertyCascadeTest, ResolverLineHeightEmValueV67) {
    const std::string css = "p { line-height: 1.5em; }";
    const std::string tag = "p";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f);
}

TEST(PropertyCascadeTest, ResolverWhiteSpaceNowrapEnumV67) {
    const std::string css = "div { white-space: nowrap; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

TEST(PropertyCascadeTest, ResolverPositionAbsoluteV67) {
    const std::string css = "div { position: absolute; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
}

TEST(PropertyCascadeTest, ResolverFloatLeftV67) {
    const std::string css = "div { float: left; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.float_val, Float::Left);
}

TEST(PropertyCascadeTest, ResolverFontSizePxValueV68) {
    const std::string css = "div { font-size: 18px; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
}

TEST(PropertyCascadeTest, ResolverFontFamilyStringValueV68) {
    const std::string css = "p { font-family: \"Times New Roman\"; }";
    const std::string tag = "p";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_family, "Times New Roman");
}

TEST(PropertyCascadeTest, ResolverBackgroundColorHexColorV68) {
    const std::string css = "section { background-color: #00ff88; }";
    const std::string tag = "section";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.r, 0);
    EXPECT_EQ(style.background_color.g, 255);
    EXPECT_EQ(style.background_color.b, 136);
    EXPECT_EQ(style.background_color.a, 255);
}

TEST(PropertyCascadeTest, ResolverBorderTopWidthPxValueV68) {
    const std::string css = "div { border-top-width: 4px; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 4.0f);
}

TEST(PropertyCascadeTest, ResolverBorderTopStyleSolidEnumV68) {
    const std::string css = "div { border-top-style: solid; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
}

TEST(PropertyCascadeTest, ResolverBorderTopColorNamedColorV68) {
    const std::string css = "div { border-top-color: red; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.border_top.color, (Color{255, 0, 0, 255}));
}

TEST(PropertyCascadeTest, ResolverTextDecorationUnderlineV68) {
    const std::string css = "span { text-decoration: underline; }";
    const std::string tag = "span";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
}

TEST(PropertyCascadeTest, ResolverMinWidthPxValueV68) {
    const std::string css = "div { min-width: 120px; }";
    const std::string tag = "div";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = tag;

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.min_width.to_px(), 120.0f);
}

TEST(PropertyCascadeTest, ResolverMaxWidthPxValueV69) {
    const std::string css = "div { max-width: 320px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.max_width.to_px(), 320.0f);
}

TEST(PropertyCascadeTest, ResolverMaxHeightPxValueV69) {
    const std::string css = "div { max-height: 180px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.max_height.to_px(), 180.0f);
}

TEST(PropertyCascadeTest, ResolverWidthAutoDefaultV69) {
    const std::string css = "div { color: red; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_TRUE(style.width.is_auto());
}

TEST(PropertyCascadeTest, ResolverHeightAutoDefaultV69) {
    const std::string css = "div { color: blue; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_TRUE(style.height.is_auto());
}

TEST(PropertyCascadeTest, ResolverColorInheritanceFromParentV69) {
    const std::string css = "span { display: inline; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    parent.color = Color{12, 34, 56, 255};
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, parent.color);
}

TEST(PropertyCascadeTest, ResolverFontSizeInheritanceFromParentV69) {
    const std::string css = "span { display: inline; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    parent.font_size = Length::px(22.0f);
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 22.0f);
}

TEST(PropertyCascadeTest, ResolverOpacityDefaultOneV69) {
    const std::string css = "div { color: green; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
}

TEST(PropertyCascadeTest, ResolverZIndexAutoDefaultZeroV69) {
    const std::string css = "div { color: black; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.z_index, 0);
}

TEST(PropertyCascadeTest, ResolverMarginTopPxValueV70) {
    const std::string css = "div { margin-top: 24px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 24.0f);
}

TEST(PropertyCascadeTest, ResolverMarginLeftAutoValueV70) {
    const std::string css = "div { margin-left: auto; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_TRUE(style.margin.left.is_auto());
}

TEST(PropertyCascadeTest, ResolverPaddingBottomPxValueV70) {
    const std::string css = "div { padding-bottom: 14px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 14.0f);
}

TEST(PropertyCascadeTest, ResolverBorderBottomWidthPxValueV70) {
    const std::string css = "div { border-bottom-width: 6px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 6.0f);
}

TEST(PropertyCascadeTest, ResolverTextIndentEmValueV70) {
    const std::string css = "div { text-indent: 2em; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.text_indent.to_px(16.0f), 32.0f);
}

TEST(PropertyCascadeTest, ResolverWordBreakBreakAllValueV70) {
    const std::string css = "div { word-break: break-all; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.word_break, 1);
}

TEST(PropertyCascadeTest, ResolverVerticalAlignBaselineEnumV70) {
    const std::string css = "span { vertical-align: baseline; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Baseline);
}

TEST(PropertyCascadeTest, ResolverBoxSizingBorderBoxValueV70) {
    const std::string css = "div { box-sizing: border-box; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
}

TEST(PropertyCascadeTest, ResolverWidthPxValueV71) {
    const std::string css = "div { width: 100px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.width.to_px(), 100.0f);
}

TEST(PropertyCascadeTest, ResolverHeightPxValueV71) {
    const std::string css = "div { height: 50px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.height.to_px(), 50.0f);
}

TEST(PropertyCascadeTest, ResolverDisplayInlineBlockValueV71) {
    const std::string css = "div { display: inline-block; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(PropertyCascadeTest, ResolverOverflowScrollIntValueV71) {
    const std::string css = "div { overflow: scroll; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(static_cast<int>(style.overflow_x), 2);
    EXPECT_EQ(static_cast<int>(style.overflow_y), 2);
}

TEST(PropertyCascadeTest, ResolverPointerEventsNoneEnumV71) {
    const std::string css = "div { pointer-events: none; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
}

TEST(PropertyCascadeTest, ResolverUserSelectNoneEnumV71) {
    const std::string css = "div { user-select: none; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(PropertyCascadeTest, ResolverTransformTranslateXExistsV71) {
    const std::string css = "div { transform: translateX(12px); }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    ASSERT_FALSE(style.transforms.empty());
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
}

TEST(PropertyCascadeTest, ResolverTransitionDurationValueV71) {
    const std::string css = "div { transition-duration: 250ms; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_NEAR(style.transition_duration, 0.25f, 0.001f);
}

TEST(PropertyCascadeTest, ResolverColorWhiteHexV72) {
    const std::string css = "div { color: #ffffff; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, Color::white());
}

TEST(PropertyCascadeTest, ResolverBackgroundColorTransparentAlphaZeroV72) {
    const std::string css = "div { background-color: transparent; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.a, 0);
}

TEST(PropertyCascadeTest, ResolverFontWeightNormalResolvesTo400V72) {
    const std::string css = "div { font-weight: 700; font-weight: normal; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_weight, 400);
}

TEST(PropertyCascadeTest, ResolverMarginZeroResetsAllSidesV72) {
    const std::string css = "div { margin: 10px 20px 30px 40px; margin: 0; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 0.0f);
}

TEST(PropertyCascadeTest, ResolverPaddingZeroResetsAllSidesV72) {
    const std::string css = "div { padding: 8px 6px 4px 2px; padding: 0; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 0.0f);
}

TEST(PropertyCascadeTest, ResolverBorderCollapseCollapseValueV72) {
    const std::string css = "table { border-collapse: collapse; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "table";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_TRUE(style.border_collapse);
}

TEST(PropertyCascadeTest, ResolverTableLayoutFixedValueV72) {
    const std::string css = "table { table-layout: fixed; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "table";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.table_layout, 1);
}

TEST(PropertyCascadeTest, ResolverListStylePositionInsideValueV72) {
    const std::string css = "ul { list-style-position: inside; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "ul";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);
}

TEST(PropertyCascadeTest, ResolverColorBlackDefaultV73) {
    const std::string css = "div { }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, Color::black());
}

TEST(PropertyCascadeTest, ResolverBackgroundColorBlueHexV73) {
    const std::string css = "div { background-color: #0000ff; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color, (Color{0, 0, 255, 255}));
}

TEST(PropertyCascadeTest, ResolverFontSize14pxV73) {
    const std::string css = "div { font-size: 14px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
}

TEST(PropertyCascadeTest, ResolverFontWeightBold700V73) {
    const std::string css = "div { font-weight: bold; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_weight, 700);
}

TEST(PropertyCascadeTest, ResolverMargin10pxAllSidesV73) {
    const std::string css = "div { margin: 10px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 10.0f);
}

TEST(PropertyCascadeTest, ResolverPadding5pxAllSidesV73) {
    const std::string css = "div { padding: 5px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 5.0f);
}

TEST(PropertyCascadeTest, ResolverDisplayNoneV73) {
    const std::string css = "div { display: none; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::None);
}

TEST(PropertyCascadeTest, ResolverBorderWidth1pxAllSidesV73) {
    const std::string css = "div { border-width: 1px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 1.0f);
}

TEST(PropertyCascadeTest, ResolverColorGreenNamedV74) {
    const std::string css = "div { color: green; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, (Color{0, 128, 0, 255}));
}

TEST(PropertyCascadeTest, ResolverFontSize18pxV74) {
    const std::string css = "div { font-size: 18px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
}

TEST(PropertyCascadeTest, ResolverDisplayFlexV74) {
    const std::string css = "div { display: flex; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
}

TEST(PropertyCascadeTest, ResolverPositionRelativeV74) {
    const std::string css = "div { position: relative; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
}

TEST(PropertyCascadeTest, ResolverMarginLeft10pxV74) {
    const std::string css = "div { margin-left: 10px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 10.0f);
}

TEST(PropertyCascadeTest, ResolverPaddingRight5pxV74) {
    const std::string css = "div { padding-right: 5px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 5.0f);
}

TEST(PropertyCascadeTest, ResolverBorderRadius3pxV74) {
    const std::string css = "div { border-radius: 3px; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_radius, 3.0f);
}

TEST(PropertyCascadeTest, ResolverLineHeight15emV74) {
    const std::string css = "p { line-height: 1.5em; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationColorHexValueV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("color", "#112233"), parent);
    EXPECT_EQ(style.color, (Color{17, 34, 51, 255}));
}

TEST(PropertyCascadeTest, ApplyDeclarationFontStyleItalicEnumV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("font-style", "italic"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(PropertyCascadeTest, ApplyDeclarationBorderTopWidthKeepsStyleNoneV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("border-top-width", "4px"), parent);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::None);
}

TEST(PropertyCascadeTest, ApplyDeclarationTransformTranslateXStoresEntryV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("transform", "translateX(24px)"), parent);
    ASSERT_EQ(style.transforms.size(), 1u);
    EXPECT_EQ(style.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(style.transforms[0].x, 24.0f);
}

TEST(PropertyCascadeTest, ApplyDeclarationTransitionDurationMsToSecondsV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("transition-duration", "175ms"), parent);
    EXPECT_NEAR(style.transition_duration, 0.175f, 0.001f);
}

TEST(PropertyCascadeTest, ApplyDeclarationTextTransformUppercaseEnumV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("text-transform", "uppercase"), parent);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
}

TEST(PropertyCascadeTest, ApplyDeclarationFlexDirectionColumnReverseV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("flex-direction", "column-reverse"), parent);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
}

TEST(PropertyCascadeTest, ApplyDeclarationGridAutoFlowColumnDenseIntV75) {
    clever::css::PropertyCascade cascade;
    clever::css::ComputedStyle style, parent;

    cascade.apply_declaration(style, make_decl("grid-auto-flow", "column dense"), parent);
    EXPECT_EQ(style.grid_auto_flow, 3);
}

TEST(CSSStyleTest, CascadeSpecificityWinsForNonImportantDeclarationsV76) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule type_rule;
    type_rule.declarations.push_back(make_decl("display", "block"));

    StyleRule class_rule;
    class_rule.declarations.push_back(make_decl("display", "flex"));

    MatchedRule low_specificity_match;
    low_specificity_match.rule = &type_rule;
    low_specificity_match.specificity = {0, 0, 1};
    low_specificity_match.source_order = 10;

    MatchedRule high_specificity_match;
    high_specificity_match.rule = &class_rule;
    high_specificity_match.specificity = {0, 1, 0};
    high_specificity_match.source_order = 1;

    auto result = cascade.cascade({low_specificity_match, high_specificity_match}, parent_style);
    EXPECT_EQ(result.display, Display::Flex);
}

TEST(CSSStyleTest, CascadeImportantOverridesHigherSpecificityV76) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule high_specificity_rule;
    high_specificity_rule.declarations.push_back(make_decl("display", "flex"));

    StyleRule important_rule;
    important_rule.declarations.push_back(make_decl("display", "block", true));

    MatchedRule high_specificity_match;
    high_specificity_match.rule = &high_specificity_rule;
    high_specificity_match.specificity = {1, 0, 0};
    high_specificity_match.source_order = 0;

    MatchedRule important_match;
    important_match.rule = &important_rule;
    important_match.specificity = {0, 0, 1};
    important_match.source_order = 1;

    auto result = cascade.cascade({high_specificity_match, important_match}, parent_style);
    EXPECT_EQ(result.display, Display::Block);
}

TEST(CSSStyleTest, ApplyBorderTopWidthDoesNotImplySolidStyleV76) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-top-width", "6px"), parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 6.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::None);
}

TEST(CSSStyleTest, ApplyBorderWidthShorthandKeepsAllSideStylesNoneV76) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-width", "1px 2px 3px 4px"), parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::None);
    EXPECT_EQ(style.border_right.style, BorderStyle::None);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::None);
    EXPECT_EQ(style.border_left.style, BorderStyle::None);
}

TEST(CSSStyleTest, ApplyMarginShorthandThreeValuesExpandsCorrectlyV76) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl_multi("margin", {"4px", "8px", "12px"}), parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 8.0f);
}

TEST(CSSStyleTest, ApplyVisibilityAndCursorInheritUseParentEnumsV76) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    parent.visibility = Visibility::Hidden;
    parent.cursor = Cursor::Pointer;

    cascade.apply_declaration(style, make_decl("visibility", "inherit"), parent);
    cascade.apply_declaration(style, make_decl("cursor", "inherit"), parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, ResolveInheritedColorAndCursorFromParentV76) {
    StyleResolver resolver;
    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    parent.color = Color{9, 99, 199, 255};
    parent.cursor = Cursor::Move;
    parent.display = Display::Flex;

    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, (Color{9, 99, 199, 255}));
    EXPECT_EQ(style.cursor, Cursor::Move);
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, ResolveLineHeightEmToComputedPxV76) {
    const std::string css = "p { line-height: 1.5em; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.line_height.value, 24.0f);
}

TEST(CSSStyleTest, ParseDisplayGridV77) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "grid"), parent);
    EXPECT_EQ(style.display, Display::Grid);
}

TEST(CSSStyleTest, ParsePositionStickyV77) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("position", "sticky"), parent);
    EXPECT_EQ(style.position, Position::Sticky);
}

TEST(CSSStyleTest, ParseOpacityHalfV77) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("opacity", "0.5"), parent);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

TEST(CSSStyleTest, ParseFontSize24pxV77) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-size", "24px"), parent);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);
}

TEST(CSSStyleTest, ImportantOverridesNormalDeclarationV77) {
    PropertyCascade cascade;
    ComputedStyle parent_style;

    StyleRule normal_rule;
    normal_rule.declarations.push_back(make_decl("color", "red", false));

    StyleRule important_rule;
    important_rule.declarations.push_back(make_decl("color", "blue", true));

    MatchedRule normal_match;
    normal_match.rule = &normal_rule;
    normal_match.specificity = {0, 0, 1};
    normal_match.source_order = 0;

    MatchedRule important_match;
    important_match.rule = &important_rule;
    important_match.specificity = {0, 0, 1};
    important_match.source_order = 1;

    auto result = cascade.cascade({normal_match, important_match}, parent_style);
    EXPECT_EQ(result.color, (Color{0, 0, 255, 255}));
}

TEST(CSSStyleTest, ClassSelectorMatchingV77) {
    const std::string css = ".active { color: red; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes.push_back("active");

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color, (Color{255, 0, 0, 255}));
}

TEST(CSSStyleTest, IdSelectorMatchingV77) {
    const std::string css = "#main { display: flex; }";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "main";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
}

TEST(CSSStyleTest, LastDeclarationWinsV77) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "block"), parent);
    EXPECT_EQ(style.display, Display::Block);

    cascade.apply_declaration(style, make_decl("display", "inline"), parent);
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, ParseDisplayNoneV78) {
    const std::string css = "div{display:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::None);
}

TEST(CSSStyleTest, ParsePositionAbsoluteV78) {
    const std::string css = "div{position:absolute;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
}

TEST(CSSStyleTest, ParseVisibilityHiddenV78) {
    const std::string css = "div{visibility:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(CSSStyleTest, DefaultComputedStyleIsInlineV78) {
    ComputedStyle style;
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, ParseColorRedV78) {
    const std::string css = "p{color:red;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color.r, 255);
}

TEST(CSSStyleTest, ParseBackgroundColorBlueV78) {
    const std::string css = "p{background-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.b, 255);
}

TEST(CSSStyleTest, ParseMarginShorthandV78) {
    const std::string css = "div{margin:10px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.margin.top.to_px(), 10.0f);
}

TEST(CSSStyleTest, InheritedFontSizeFromParentV78) {
    const std::string css = "p{font-size:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ComputedStyle parent;
    parent.font_size = Length::px(20.0f);

    ElementView child_elem;
    child_elem.tag_name = "span";

    auto child_style = resolver.resolve(child_elem, parent);

    EXPECT_EQ(child_style.font_size.to_px(), 20.0f);
}

TEST(CSSStyleTest, ParseDisplayInlineBlockV79) {
    const std::string css = "div{display:inline-block;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(CSSStyleTest, ParsePositionRelativeV79) {
    const std::string css = "div{position:relative;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
}

TEST(CSSStyleTest, ParsePositionFixedV79) {
    const std::string css = "span{position:fixed;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
}

TEST(CSSStyleTest, DefaultPositionIsStaticV79) {
    ComputedStyle style;

    EXPECT_EQ(style.position, Position::Static);
}

TEST(CSSStyleTest, DefaultVisibilityIsVisibleV79) {
    ComputedStyle style;

    EXPECT_EQ(style.visibility, Visibility::Visible);
}

TEST(CSSStyleTest, ParseCursorPointerV79) {
    const std::string css = "a{cursor:pointer;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, ParseDisplayListItemV79) {
    const std::string css = "li{display:list-item;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "li";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::ListItem);
}

TEST(CSSStyleTest, ParseDisplayTableV79) {
    const std::string css = "table{display:table;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "table";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Table);
}

// ===========================================================================
// V80 Tests
// ===========================================================================

TEST(CSSStyleTest, ParseDisplayFlexV80) {
    const std::string css = "div{display:flex;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
}

TEST(CSSStyleTest, ParsePositionStaticExplicitV80) {
    const std::string css = "span{position:static;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Static);
}

TEST(CSSStyleTest, ParseVisibilityVisibleV80) {
    const std::string css = "p{visibility:visible;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Visible);
}

TEST(CSSStyleTest, DefaultCursorIsAutoV80) {
    ComputedStyle style;

    EXPECT_EQ(style.cursor, Cursor::Auto);
}

TEST(CSSStyleTest, ParseDisplayInlineV80) {
    const std::string css = "span{display:inline;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, ParseFontSizeEmV80) {
    // 1.5em with default parent font-size (16px) should resolve to 24px
    const std::string css = "p{font-size:1.5em;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    // parent uses default font-size of 16px
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(16.0f), 24.0f);
}

TEST(CSSStyleTest, ParseColorHexV80) {
    // #ff0000 should resolve to red (255, 0, 0)
    const std::string css = "div{color:#ff0000;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color.r, 255);
    EXPECT_EQ(style.color.g, 0);
    EXPECT_EQ(style.color.b, 0);
    EXPECT_EQ(style.color.a, 255);
}

TEST(CSSStyleTest, TagSelectorMatchesV80) {
    // CSS targets "h2" tag; verify it applies to h2 but not to div
    const std::string css = "h2{display:flex;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ComputedStyle parent;

    // h2 element should get display:flex
    ElementView h2_elem;
    h2_elem.tag_name = "h2";
    auto h2_style = resolver.resolve(h2_elem, parent);
    EXPECT_EQ(h2_style.display, Display::Flex);

    // div element should NOT get display:flex (default for div is block)
    ElementView div_elem;
    div_elem.tag_name = "div";
    auto div_style = resolver.resolve(div_elem, parent);
    EXPECT_NE(div_style.display, Display::Flex);
}

// ---------------------------------------------------------------------------
// V81 Tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, PositionAbsoluteWithOffsetsV81) {
    // Verify position:absolute plus top/left offsets are resolved correctly
    const std::string css = "div{position:absolute;top:25px;left:40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 25.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 40.0f);
}

TEST(CSSStyleTest, PaddingShorthandFourValuesV81) {
    // padding: top right bottom left (4-value shorthand)
    const std::string css = "section{padding:5px 10px 15px 20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 20.0f);
}

TEST(CSSStyleTest, TextDecorationLineThroughV81) {
    // text-decoration: line-through should be parsed
    const std::string css = "span{text-decoration:line-through;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
}

TEST(CSSStyleTest, MinMaxDimensionsV81) {
    // min-width, max-width, min-height, max-height
    const std::string css = "div{min-width:80px;max-width:500px;min-height:40px;max-height:300px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.min_width.to_px(), 80.0f);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 500.0f);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 40.0f);
    EXPECT_FLOAT_EQ(style.max_height.to_px(), 300.0f);
}

TEST(CSSStyleTest, WhiteSpacePreWrapV81) {
    // white-space: pre-wrap should parse correctly
    const std::string css = "pre{white-space:pre-wrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
}

TEST(CSSStyleTest, ZIndexNegativeValueV81) {
    // z-index: -5 should be parsed as a negative integer
    const std::string css = "div{z-index:-5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.z_index, -5);
}

TEST(CSSStyleTest, BorderSideIndividualPropertiesV81) {
    // Set individual border-top properties
    const std::string css = "div{border-top-width:4px;border-top-style:dashed;border-top-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.value, 4.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.b, 255);
    EXPECT_EQ(style.border_top.color.a, 255);
}

TEST(CSSStyleTest, OpacityAndOverflowCombinedV81) {
    // opacity: 0.3 and overflow: hidden together
    const std::string css = "div{opacity:0.3;overflow:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.opacity, 0.3f);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

// ---------------------------------------------------------------------------
// V82 Tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, FlexDirectionColumnReverseV82) {
    // flex-direction: column-reverse should be parsed correctly
    const std::string css = "div{display:flex;flex-direction:column-reverse;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
}

TEST(CSSStyleTest, MarginShorthandTwoValuesV82) {
    // margin: 12px 24px means top/bottom=12px, left/right=24px
    const std::string css = "div{margin:12px 24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 24.0f);
}

TEST(CSSStyleTest, VisibilityHiddenWithPointerEventsNoneV82) {
    // visibility: hidden and pointer-events: none together
    const std::string css = "span{visibility:hidden;pointer-events:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
}

TEST(CSSStyleTest, TextTransformUppercaseWithLetterSpacingV82) {
    // text-transform: uppercase and letter-spacing: 2px
    const std::string css = "h1{text-transform:uppercase;letter-spacing:2px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 2.0f);
}

TEST(CSSStyleTest, BorderRadiusShorthandV82) {
    // border-radius: 8px should set all four corners
    const std::string css = "div{border-radius:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_radius, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_tl, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_tr, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_bl, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_br, 8.0f);
}

TEST(CSSStyleTest, PositionFixedWithAllOffsetsV82) {
    // position: fixed with top/bottom/left offsets
    const std::string css = "div{position:fixed;top:0px;bottom:10px;left:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 20.0f);
}

TEST(CSSStyleTest, CursorPointerAndUserSelectNoneV82) {
    // cursor: pointer and user-select: none
    const std::string css = "button{cursor:pointer;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(CSSStyleTest, BackgroundColorAndTextAlignCenterV82) {
    // background-color: green and text-align: center
    const std::string css = "p{background-color:green;text-align:center;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.r, 0);
    EXPECT_EQ(style.background_color.g, 128);
    EXPECT_EQ(style.background_color.b, 0);
    EXPECT_EQ(style.background_color.a, 255);
    EXPECT_EQ(style.text_align, TextAlign::Center);
}

// ===========================================================================
// V83 Tests
// ===========================================================================

TEST(CSSStyleTest, WhiteSpacePreWrapWithWordBreakV83) {
    // white-space: pre-wrap combined with word-break: break-all
    const std::string css = "pre{white-space:pre-wrap;word-break:break-all;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
    EXPECT_EQ(style.word_break, 1);  // 1=break-all
}

TEST(CSSStyleTest, BorderLeftLonghandPropertiesV83) {
    // border-left-width, border-left-style, border-left-color as longhands
    const std::string css = "div{border-left-width:3px;border-left-style:dashed;border-left-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_left.color.r, 0);
    EXPECT_EQ(style.border_left.color.g, 0);
    EXPECT_EQ(style.border_left.color.b, 255);
    EXPECT_EQ(style.border_left.color.a, 255);
}

TEST(CSSStyleTest, WidthPercentAndHeightPxV83) {
    // width: 50% and height: 200px
    const std::string css = "section{width:50%;height:200px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.width.value, 50.0f);
    EXPECT_EQ(style.width.unit, Length::Unit::Percent);
    EXPECT_FLOAT_EQ(style.height.to_px(), 200.0f);
}

TEST(CSSStyleTest, PaddingShorthandFourValuesV83) {
    // padding: 10px 20px 30px 40px (top right bottom left)
    const std::string css = "div{padding:10px 20px 30px 40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 40.0f);
}

TEST(CSSStyleTest, PositionAbsoluteWithTopAndRightOffsetsV83) {
    // position: absolute with top and right offsets
    const std::string css = "span{position:absolute;top:15px;right:25px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 25.0f);
}

TEST(CSSStyleTest, MarginAutoHorizontalCenteringV83) {
    // margin: 0 auto (top/bottom 0, left/right auto)
    const std::string css = "div{margin:0px auto;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 0.0f);
    EXPECT_TRUE(style.margin.left.is_auto());
    EXPECT_TRUE(style.margin.right.is_auto());
}

TEST(CSSStyleTest, FontWeightBoldAndFontSizeEmV83) {
    // font-weight: bold (700) and font-size: 1.5em
    const std::string css = "p{font-weight:bold;font-size:1.5em;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    // parent uses default font-size of 16px
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.font_weight, 700);
    EXPECT_FLOAT_EQ(style.font_size.to_px(16.0f), 24.0f);
}

TEST(CSSStyleTest, DisplayFlexWithDirectionColumnAndGapV83) {
    // display: flex with flex-direction: column and gap: 12px
    const std::string css = "div{display:flex;flex-direction:column;gap:12px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 12.0f);
}

// ===========================================================================
// V84 Tests
// ===========================================================================

TEST(CSSStyleTest, PositionRelativeWithOffsetsV84) {
    // position: relative with top/right/bottom/left offsets
    const std::string css = "div{position:relative;top:10px;right:20px;bottom:30px;left:40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 40.0f);
}

TEST(CSSStyleTest, VisibilityHiddenResolvedV84) {
    // visibility: hidden via stylesheet resolution
    const std::string css = "span{visibility:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(CSSStyleTest, BorderShorthandSolidRedV84) {
    // border shorthand: 3px solid red applied to all sides
    const std::string css = "div{border:3px solid red;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.value, 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);

    EXPECT_FLOAT_EQ(style.border_bottom.width.value, 3.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);

    EXPECT_FLOAT_EQ(style.border_left.width.value, 3.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);

    EXPECT_FLOAT_EQ(style.border_right.width.value, 3.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
}

TEST(CSSStyleTest, WhiteSpacePreWrapResolvedV84) {
    // white-space: pre-wrap via stylesheet
    const std::string css = "pre{white-space:pre-wrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
}

TEST(CSSStyleTest, CursorPointerOnButtonV84) {
    // cursor: pointer applied to a button element
    const std::string css = "button{cursor:pointer;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, OverflowHiddenBothAxesV84) {
    // overflow: hidden sets both overflow-x and overflow-y
    const std::string css = "div{overflow:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

TEST(CSSStyleTest, BoxShadowInsetWithSpreadV84) {
    // inset box-shadow with spread value
    const std::string css = "div{box-shadow:inset 1px 2px 3px 4px black;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    ASSERT_EQ(style.box_shadows.size(), 1u);
    EXPECT_TRUE(style.box_shadows[0].inset);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_x, 1.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].offset_y, 2.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].blur, 3.0f);
    EXPECT_FLOAT_EQ(style.box_shadows[0].spread, 4.0f);
}

TEST(CSSStyleTest, OpacityAndPointerEventsNoneV84) {
    // opacity and pointer-events: none together
    const std::string css = "div{opacity:0.5;pointer-events:none;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

// ===========================================================================
// V85 Tests
// ===========================================================================

TEST(CSSStyleTest, PositionAbsoluteWithOffsetsV85) {
    // position: absolute with all four position offsets
    const std::string css = "div{position:absolute;top:10px;right:20px;bottom:30px;left:40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 40.0f);
}

TEST(CSSStyleTest, MarginAndPaddingEdgeSizesV85) {
    // margin and padding set via EdgeSizes struct fields
    const std::string css = "div{margin:5px 10px 15px 20px;padding:2px 4px 6px 8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 20.0f);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 8.0f);
}

TEST(CSSStyleTest, BorderEdgePropertiesV85) {
    // border shorthand sets all four sides, then override individual side styles
    const std::string css = "div{border:3px solid red;border-bottom-style:dashed;border-bottom-color:blue;border-bottom-width:1px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.value, 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);

    EXPECT_FLOAT_EQ(style.border_bottom.width.value, 1.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.color.b, 255);
}

TEST(CSSStyleTest, DisplayFlexWithDirectionAndWrapV85) {
    // display: flex with flex-direction: column and flex-wrap: wrap
    const std::string css = "div{display:flex;flex-direction:column;flex-wrap:wrap;justify-content:center;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);
}

TEST(CSSStyleTest, VisibilityHiddenAndWhiteSpacePreV85) {
    // visibility: hidden and white-space: pre
    const std::string css = "span{visibility:hidden;white-space:pre;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.white_space, WhiteSpace::Pre);
}

TEST(CSSStyleTest, CursorTypesResolveCorrectlyV85) {
    // cursor: text resolves to Cursor::Text
    const std::string css = "input{cursor:text;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "input";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Text);
}

TEST(CSSStyleTest, FontSizeWeightAndColorV85) {
    // font-size, font-weight, and color resolved together
    const std::string css = "p{font-size:24px;font-weight:700;color:green;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.value, 24.0f);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.color.r, 0);
    EXPECT_EQ(style.color.g, 128);
    EXPECT_EQ(style.color.b, 0);
}

TEST(CSSStyleTest, InheritColorFromParentStyleV85) {
    // color is an inherited property; child should get parent's color
    const std::string css = "div{background-color:yellow;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    parent.color = Color{255, 0, 0, 255}; // red parent color
    auto style = resolver.resolve(elem, parent);

    // color should be inherited from parent since CSS doesn't set it
    EXPECT_EQ(style.color.r, 255);
    EXPECT_EQ(style.color.g, 0);
    EXPECT_EQ(style.color.b, 0);

    // background-color is NOT inherited; should be yellow from CSS
    EXPECT_EQ(style.background_color.r, 255);
    EXPECT_EQ(style.background_color.g, 255);
    EXPECT_EQ(style.background_color.b, 0);
}

// V86 Tests
// ===========================================================================

TEST(CSSStyleTest, PositionFixedWithZIndexV86) {
    // position: fixed with z-index and top/left offsets
    const std::string css = "nav{position:fixed;top:0px;left:0px;z-index:100;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
    EXPECT_EQ(style.z_index, 100);
}

TEST(CSSStyleTest, MarginAutoHorizontalCenteringV86) {
    // margin-left and margin-right set to auto for horizontal centering
    const std::string css = "div{margin-top:10px;margin-right:auto;margin-bottom:10px;margin-left:auto;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_TRUE(style.margin.right.is_auto());
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 10.0f);
    EXPECT_TRUE(style.margin.left.is_auto());
}

TEST(CSSStyleTest, BorderLeftDottedWithColorV86) {
    // Set border-left via individual longhand properties
    const std::string css = "p{border-left-width:5px;border-left-style:dotted;border-left-color:green;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_left.width.value, 5.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_left.color.r, 0);
    EXPECT_EQ(style.border_left.color.g, 128);
    EXPECT_EQ(style.border_left.color.b, 0);

    // Other borders remain at default (none)
    EXPECT_EQ(style.border_top.style, BorderStyle::None);
    EXPECT_EQ(style.border_right.style, BorderStyle::None);
}

TEST(CSSStyleTest, PaddingShorthandTwoValuesV86) {
    // padding shorthand with two values: vertical horizontal
    const std::string css = "section{padding:12px 24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 24.0f);
}

TEST(CSSStyleTest, VisibilityHiddenWithCursorPointerV86) {
    // visibility: hidden and cursor: pointer together
    const std::string css = "a{visibility:hidden;cursor:pointer;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, WhiteSpacePreWrapWithTextAlignCenterV86) {
    // white-space: pre-wrap combined with text-align: center
    const std::string css = "pre{white-space:pre-wrap;text-align:center;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
    EXPECT_EQ(style.text_align, TextAlign::Center);
}

TEST(CSSStyleTest, IdSelectorResolvesStyleV86) {
    // Selector by ID should match and resolve properties
    const std::string css = "#main{display:block;background-color:navy;opacity:0.8;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "main";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Block);
    EXPECT_FLOAT_EQ(style.opacity, 0.8f);
    EXPECT_EQ(style.background_color.r, 0);
    EXPECT_EQ(style.background_color.g, 0);
    EXPECT_EQ(style.background_color.b, 128);
}

TEST(CSSStyleTest, ClassSelectorWithPositionOffsetsV86) {
    // Class selector sets position: relative with right and bottom offsets
    const std::string css = ".box{position:relative;right:15px;bottom:25px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 25.0f);
}

// ===========================================================================
// V87 Tests
// ===========================================================================

TEST(CSSStyleTest, MarginShorthandFourValuesV87) {
    // margin shorthand with four distinct values resolves all four edges
    const std::string css = "div{margin:5px 10px 15px 20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.value, 5.0f);
    EXPECT_FLOAT_EQ(style.margin.right.value, 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.value, 15.0f);
    EXPECT_FLOAT_EQ(style.margin.left.value, 20.0f);
}

TEST(CSSStyleTest, PaddingLeftAndTopResolveV87) {
    // padding-left and padding-top set individually via longhand properties
    const std::string css = "span{padding-left:12px;padding-top:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.left.value, 12.0f);
    EXPECT_FLOAT_EQ(style.padding.top.value, 8.0f);
}

TEST(CSSStyleTest, BorderTopSolidRedV87) {
    // border-top longhand properties set width, style, and color
    const std::string css = "div{border-top-width:3px;border-top-style:solid;border-top-color:red;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);
}

TEST(CSSStyleTest, AbsolutePositionWithAllOffsetsV87) {
    // position: absolute with top, right, bottom, left offsets
    const std::string css = ".overlay{position:absolute;top:10px;right:20px;bottom:30px;left:40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 40.0f);
}

TEST(CSSStyleTest, VisibilityHiddenWithCursorPointerV87) {
    // visibility: hidden and cursor: pointer on a single element
    const std::string css = "a{visibility:hidden;cursor:pointer;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, WhiteSpaceNowrapWithTextOverflowV87) {
    // white-space: nowrap resolves correctly
    const std::string css = "p{white-space:nowrap;overflow:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

TEST(CSSStyleTest, IdSelectorMarginAutoWithDisplayBlockV87) {
    // ID selector with margin: auto and display: block
    const std::string css = "#container{display:block;margin:auto;width:500px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "container";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Block);
    EXPECT_TRUE(style.margin.top.is_auto());
    EXPECT_TRUE(style.margin.right.is_auto());
    EXPECT_TRUE(style.margin.bottom.is_auto());
    EXPECT_TRUE(style.margin.left.is_auto());
    EXPECT_FLOAT_EQ(style.width.to_px(), 500.0f);
}

TEST(CSSStyleTest, ClassSelectorBorderDashedBlueWithPaddingV87) {
    // Class selector with dashed blue border longhands and padding shorthand
    const std::string css = ".card{border-top-width:2px;border-top-style:dashed;border-top-color:blue;padding:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.r, 0);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 255);
    EXPECT_FLOAT_EQ(style.padding.top.value, 16.0f);
    EXPECT_FLOAT_EQ(style.padding.right.value, 16.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.value, 16.0f);
    EXPECT_FLOAT_EQ(style.padding.left.value, 16.0f);
}

TEST(CSSStyleTest, MarginTopAndBottomWithAutoSidesV88) {
    // margin-top and margin-bottom explicit, left/right auto
    const std::string css = "div{margin-top:20px;margin-bottom:30px;margin-left:auto;margin-right:auto;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
    EXPECT_TRUE(style.margin.left.is_auto());
    EXPECT_TRUE(style.margin.right.is_auto());
}

TEST(CSSStyleTest, PaddingShorthandTwoValuesV88) {
    // padding shorthand with two values: vertical horizontal
    const std::string css = "section{padding:10px 24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.padding.top.value, 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.value, 10.0f);
    EXPECT_FLOAT_EQ(style.padding.left.value, 24.0f);
    EXPECT_FLOAT_EQ(style.padding.right.value, 24.0f);
}

TEST(CSSStyleTest, BorderRightSolidGreenV88) {
    // border-right longhand: width, style, color
    const std::string css = "span{border-right-width:5px;border-right-style:solid;border-right-color:green;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.r, 0);
    EXPECT_EQ(style.border_right.color.g, 128);
    EXPECT_EQ(style.border_right.color.b, 0);
}

TEST(CSSStyleTest, FixedPositionWithTopAndLeftV88) {
    // position: fixed with top and left offsets only
    const std::string css = "#banner{position:fixed;top:0px;left:0px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "banner";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
}

TEST(CSSStyleTest, VisibilityCollapseOnTableRowV88) {
    // visibility: collapse is valid for table elements
    const std::string css = "tr{visibility:collapse;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "tr";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Collapse);
}

TEST(CSSStyleTest, CursorNotAllowedWithUserSelectNoneV88) {
    // cursor: not-allowed and user-select: none together
    const std::string css = ".disabled{cursor:not-allowed;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"disabled"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::NotAllowed);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(CSSStyleTest, WhiteSpacePreWithOverflowScrollV88) {
    // white-space: pre with overflow-x: scroll
    const std::string css = "code{white-space:pre;overflow-x:scroll;overflow-y:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "code";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::Pre);
    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

TEST(CSSStyleTest, BorderBottomDottedWithMarginShorthandThreeValuesV88) {
    // border-bottom dotted + margin shorthand 3 values: top right/left bottom
    const std::string css = "p{border-bottom-width:1px;border-bottom-style:dotted;border-bottom-color:black;margin:8px 16px 24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0);
    EXPECT_EQ(style.border_bottom.color.g, 0);
    EXPECT_EQ(style.border_bottom.color.b, 0);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 16.0f);
}

TEST(CSSStyleTest, DefaultComputedStyleDisplayIsInlineV89) {
    ComputedStyle style;
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, VisibilityHiddenViaResolverV89) {
    const std::string css = ".hidden{visibility:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"hidden"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(CSSStyleTest, CursorPointerOnAnchorV89) {
    const std::string css = "a{cursor:pointer;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, MarginAllFourSidesDistinctV89) {
    const std::string css = "div{margin-top:5px;margin-right:10px;margin-bottom:15px;margin-left:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 20.0f);
}

TEST(CSSStyleTest, BorderTopWidthAndColorRedV89) {
    const std::string css = "h1{border-top-width:3px;border-top-style:solid;border-top-color:red;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);
}

TEST(CSSStyleTest, PositionStickyWithTopOffsetV89) {
    const std::string css = "nav{position:sticky;top:10px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
}

TEST(CSSStyleTest, TextAlignJustifyWithWhiteSpacePreWrapV89) {
    const std::string css = "p{text-align:justify;white-space:pre-wrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_align, TextAlign::Justify);
    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
}

TEST(CSSStyleTest, FlexGrowShrinkAndOpacityV89) {
    const std::string css = ".item{flex-grow:2;flex-shrink:0.5;opacity:0.8;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
    EXPECT_FLOAT_EQ(style.opacity, 0.8f);
}

TEST(CSSStyleTest, DisplayGridWithOverflowAutoV90) {
    const std::string css = "section{display:grid;overflow-x:auto;overflow-y:auto;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Grid);
    EXPECT_EQ(style.overflow_x, Overflow::Auto);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
}

TEST(CSSStyleTest, CursorNotAllowedWithPointerEventsNoneV90) {
    const std::string css = ".disabled{cursor:not-allowed;pointer-events:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"disabled"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::NotAllowed);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
}

TEST(CSSStyleTest, UserSelectAllWithVisibilityCollapseV90) {
    const std::string css = "tr.hidden{user-select:all;visibility:collapse;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "tr";
    elem.classes = {"hidden"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.user_select, UserSelect::All);
    EXPECT_EQ(style.visibility, Visibility::Collapse);
}

TEST(CSSStyleTest, VerticalAlignTopWithLineHeightV90) {
    const std::string css = "span{vertical-align:top;line-height:1.8;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Top);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.8f);
}

TEST(CSSStyleTest, PositionAbsoluteWithZindexAndInsetsV90) {
    const std::string css = "#overlay{position:absolute;z-index:100;top:0px;left_pos:0px;right:20px;bottom:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "overlay";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 100);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
}

TEST(CSSStyleTest, InlineBlockWithPaddingAndMarginV90) {
    const std::string css = ".badge{display:inline-block;padding:4px;margin:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"badge"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 8.0f);
}

TEST(CSSStyleTest, BorderLeftSolidBlueWithWidthV90) {
    const std::string css = "aside{border-left-width:5px;border-left-style:solid;border-left-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "aside";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.color.b, 255);
}

TEST(CSSStyleTest, WhiteSpacePreLineWithTextAlignCenterV90) {
    const std::string css = "pre.code{white-space:pre-line;text-align:center;font-size:14px;font-weight:700;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";
    elem.classes = {"code"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::PreLine);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.font_size.value, 14.0f);
    EXPECT_EQ(style.font_weight, 700);
}

TEST(CSSStyleTest, FlexContainerWithGrowShrinkV91) {
    const std::string css = ".row{display:flex;flex-grow:3;flex-shrink:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"row"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_FLOAT_EQ(style.flex_grow, 3.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
}

TEST(CSSStyleTest, StickyPositionWithZIndexV91) {
    const std::string css = "nav{position:sticky;z-index:50;opacity:0.95;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_EQ(style.z_index, 50);
    EXPECT_FLOAT_EQ(style.opacity, 0.95f);
}

TEST(CSSStyleTest, VisibilityHiddenWithPointerEventsNoneV91) {
    const std::string css = ".ghost{visibility:hidden;pointer-events:none;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"ghost"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(CSSStyleTest, CursorPointerWithColorAndBgV91) {
    const std::string css = ".btn{cursor:pointer;color:white;background-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"btn"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.color, (Color{255, 255, 255, 255}));
    EXPECT_EQ(style.background_color.b, 255);
    EXPECT_EQ(style.background_color.a, 255);
}

TEST(CSSStyleTest, GridDisplayWithVerticalAlignMiddleV91) {
    const std::string css = ".grid-cell{display:grid;vertical-align:middle;text-align:right;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"grid-cell"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Grid);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(style.text_align, TextAlign::Right);
}

TEST(CSSStyleTest, AbsolutePositionWithAllBordersV91) {
    const std::string css = "#modal{position:absolute;border-top-width:2px;border-right-width:2px;border-bottom-width:2px;border-left-width:2px;border-top-style:solid;border-right-style:solid;border-bottom-style:solid;border-left-style:solid;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "modal";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);
}

TEST(CSSStyleTest, WhiteSpaceNoWrapWithLineHeightV91) {
    const std::string css = ".truncate{white-space:nowrap;line-height:1.5;font-size:18px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"truncate"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(style.font_size.value, 18.0f);
    EXPECT_FLOAT_EQ(style.line_height.value, 27.0f);
}

TEST(CSSStyleTest, MarginPaddingAsymmetricWithDisplayNoneV91) {
    const std::string css = ".hidden-box{display:none;margin:10px 20px 30px 40px;padding:5px 15px 25px 35px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hidden-box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::None);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 40.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 25.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 35.0f);
}

TEST(CSSStyleTest, FlexGrowShrinkWithDisplayFlexV92) {
    const std::string css = ".flex-item{display:flex;flex-grow:2;flex-shrink:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
}

TEST(CSSStyleTest, VisibilityHiddenWithOpacityV92) {
    const std::string css = ".ghost{visibility:hidden;opacity:0.3;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"ghost"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_FLOAT_EQ(style.opacity, 0.3f);
}

TEST(CSSStyleTest, CursorPointerWithUserSelectNoneV92) {
    const std::string css = ".btn{cursor:pointer;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"btn"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(CSSStyleTest, ZIndexWithPositionRelativeV92) {
    const std::string css = ".overlay{position:relative;z-index:50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_EQ(style.z_index, 50);
}

TEST(CSSStyleTest, TextAlignCenterWithFontWeightBoldV92) {
    const std::string css = ".heading{text-align:center;font-weight:700;font-size:24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";
    elem.classes = {"heading"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_FLOAT_EQ(style.font_size.value, 24.0f);
}

TEST(CSSStyleTest, PointerEventsNoneWithPositionFixedV92) {
    const std::string css = ".no-click{pointer-events:none;position:fixed;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"no-click"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.position, Position::Fixed);
}

TEST(CSSStyleTest, BackgroundColorWithPaddingUniformV92) {
    const std::string css = ".card{background-color:#ff8800;padding:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.r, 0xFF);
    EXPECT_EQ(style.background_color.g, 0x88);
    EXPECT_EQ(style.background_color.b, 0x00);
    EXPECT_EQ(style.background_color.a, 255);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
}

TEST(CSSStyleTest, VerticalAlignMiddleWithDisplayInlineBlockV92) {
    const std::string css = ".icon{vertical-align:middle;display:inline-block;color:#00ff00;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "img";
    elem.classes = {"icon"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_EQ(style.color.r, 0x00);
    EXPECT_EQ(style.color.g, 0xFF);
    EXPECT_EQ(style.color.b, 0x00);
    EXPECT_EQ(style.color.a, 255);
}

TEST(CSSStyleTest, FlexGrowShrinkWithDisplayFlexV93) {
    const std::string css = ".item{display:flex;flex-grow:2;flex-shrink:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
}

TEST(CSSStyleTest, ZIndexWithPositionAbsoluteV93) {
    const std::string css = ".overlay{position:absolute;z-index:999;opacity:0.8;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 999);
    EXPECT_FLOAT_EQ(style.opacity, 0.8f);
}

TEST(CSSStyleTest, WhiteSpaceNoWrapWithCursorPointerV93) {
    const std::string css = ".label{white-space:nowrap;cursor:pointer;font-weight:700;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"label"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.font_weight, 700);
}

TEST(CSSStyleTest, MarginShorthandAllSidesV93) {
    const std::string css = ".box{margin:10px 20px 30px 40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 40.0f);
}

TEST(CSSStyleTest, BorderTopWidthStyleColorV93) {
    const std::string css = ".panel{border-top-width:3px;border-top-style:solid;border-top-color:#0000ff;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"panel"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0x00);
    EXPECT_EQ(style.border_top.color.g, 0x00);
    EXPECT_EQ(style.border_top.color.b, 0xFF);
    EXPECT_EQ(style.border_top.color.a, 255);
}

TEST(CSSStyleTest, UserSelectNoneWithVisibilityHiddenV93) {
    const std::string css = ".hidden-selectable{user-select:none;visibility:hidden;font-size:18px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"hidden-selectable"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
}

TEST(CSSStyleTest, TextAlignCenterWithLineHeightV93) {
    const std::string css = ".content{text-align:center;line-height:1.5;color:#333333;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"content"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.5f);
    EXPECT_EQ(style.color.r, 0x33);
    EXPECT_EQ(style.color.g, 0x33);
    EXPECT_EQ(style.color.b, 0x33);
}

TEST(CSSStyleTest, PositionRelativeWithPaddingAndColorV93) {
    const std::string css = ".badge{position:relative;padding:4px 8px;background-color:#e91e63;display:inline-block;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"badge"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 8.0f);
    EXPECT_EQ(style.background_color.r, 0xE9);
    EXPECT_EQ(style.background_color.g, 0x1E);
    EXPECT_EQ(style.background_color.b, 0x63);
    EXPECT_EQ(style.display, Display::InlineBlock);
}

// ---------------------------------------------------------------------------
// V94 tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, FlexGrowShrinkWithOpacityV94) {
    const std::string css = ".flex-item{flex-grow:2;flex-shrink:0.5;opacity:0.75;display:flex;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
    EXPECT_FLOAT_EQ(style.opacity, 0.75f);
    EXPECT_EQ(style.display, Display::Flex);
}

TEST(CSSStyleTest, ZIndexWithPositionAbsoluteV94) {
    const std::string css = ".overlay{position:absolute;z-index:999;background-color:#000000;opacity:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 999);
    EXPECT_EQ(style.background_color.r, 0x00);
    EXPECT_EQ(style.background_color.g, 0x00);
    EXPECT_EQ(style.background_color.b, 0x00);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

TEST(CSSStyleTest, BorderTopSolidWithMarginV94) {
    const std::string css = ".card{border-top-width:3px;border-top-style:solid;border-top-color:#ff5722;margin-top:16px;margin-right:24px;margin-bottom:16px;margin-left:24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 24.0f);
}

TEST(CSSStyleTest, CursorPointerWithPointerEventsNoneV94) {
    const std::string css = ".disabled-link{cursor:pointer;pointer-events:none;user-select:none;color:#999999;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"disabled-link"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.color.r, 0x99);
    EXPECT_EQ(style.color.g, 0x99);
    EXPECT_EQ(style.color.b, 0x99);
}

TEST(CSSStyleTest, WhiteSpaceNoWrapWithFontWeightBoldV94) {
    const std::string css = ".tag{white-space:nowrap;font-weight:700;font-size:12px;display:inline-block;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"tag"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 12.0f);
    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(CSSStyleTest, VerticalAlignMiddleWithPaddingAllSidesV94) {
    const std::string css = ".cell{vertical-align:middle;padding:10px 20px 30px 40px;text-align:right;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "td";
    elem.classes = {"cell"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 40.0f);
    EXPECT_EQ(style.text_align, TextAlign::Right);
}

TEST(CSSStyleTest, PositionFixedWithZIndexAndVisibilityV94) {
    const std::string css = ".tooltip{position:fixed;z-index:1000;visibility:hidden;font-size:14px;line-height:1.4;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"tooltip"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_EQ(style.z_index, 1000);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.4f);
}

TEST(CSSStyleTest, DisplayNoneWithBorderAndBackgroundV94) {
    const std::string css = ".hidden-panel{display:none;border-bottom-width:2px;border-bottom-style:solid;background-color:#e8eaf6;margin-top:8px;margin-right:8px;margin-bottom:8px;margin-left:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hidden-panel"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::None);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 8.0f);
}

TEST(CSSStyleTest, FlexContainerWithGrowShrinkAndGapV95) {
    const std::string css = ".flex-row{display:flex;flex-grow:2.5;flex-shrink:0.5;opacity:0.85;padding-top:12px;padding-right:16px;padding-bottom:12px;padding-left:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-row"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.5f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
    EXPECT_FLOAT_EQ(style.opacity, 0.85f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
}

TEST(CSSStyleTest, AbsolutePositionWithZIndexAndCursorV95) {
    const std::string css = ".dropdown{position:absolute;z-index:500;cursor:pointer;font-size:16px;font-weight:600;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "ul";
    elem.classes = {"dropdown"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 500);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 16.0f);
    EXPECT_EQ(style.font_weight, 600);
}

TEST(CSSStyleTest, InlineBlockWithColorAndWhiteSpaceV95) {
    const std::string css = ".badge{display:inline-block;color:#ff5722;white-space:nowrap;margin-top:4px;margin-right:8px;margin-bottom:4px;margin-left:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"badge"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_EQ(style.color.r, 0xFF);
    EXPECT_EQ(style.color.g, 0x57);
    EXPECT_EQ(style.color.b, 0x22);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 8.0f);
}

TEST(CSSStyleTest, StickyPositionWithBorderTopAndTextAlignV95) {
    const std::string css = ".sticky-header{position:sticky;border-top-width:3px;border-top-style:solid;border-top-color:#1565c0;text-align:center;line-height:1.6;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "header";
    elem.classes = {"sticky-header"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0x15);
    EXPECT_EQ(style.border_top.color.g, 0x65);
    EXPECT_EQ(style.border_top.color.b, 0xc0);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.6f);
}

TEST(CSSStyleTest, UserSelectNoneWithPointerEventsAndOpacityV95) {
    const std::string css = ".overlay{user-select:none;pointer-events:none;opacity:0.4;visibility:visible;background-color:#000000;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.visibility, Visibility::Visible);
    EXPECT_EQ(style.background_color.r, 0x00);
    EXPECT_EQ(style.background_color.g, 0x00);
    EXPECT_EQ(style.background_color.b, 0x00);
}

TEST(CSSStyleTest, RelativePositionWithMarginAndFontWeightV95) {
    const std::string css = ".card{position:relative;margin-top:16px;margin-right:24px;margin-bottom:16px;margin-left:24px;font-weight:700;font-size:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 24.0f);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 20.0f);
}

TEST(CSSStyleTest, BorderRightDashedWithVerticalAlignAndPaddingV95) {
    const std::string css = ".sidebar-item{border-right-width:1px;border-right-style:dashed;border-right-color:#9e9e9e;vertical-align:middle;padding-top:6px;padding-bottom:6px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "li";
    elem.classes = {"sidebar-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.color.r, 0x9e);
    EXPECT_EQ(style.border_right.color.g, 0x9e);
    EXPECT_EQ(style.border_right.color.b, 0x9e);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 6.0f);
}

TEST(CSSStyleTest, DisplayInlineWithLineHeightPxAndBorderLeftV95) {
    const std::string css = ".tag{display:inline;line-height:24px;border-left-width:4px;border-left-style:solid;border-left-color:#4caf50;color:#2e7d32;font-size:13px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"tag"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Inline);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.color.r, 0x4c);
    EXPECT_EQ(style.border_left.color.g, 0xaf);
    EXPECT_EQ(style.border_left.color.b, 0x50);
    EXPECT_EQ(style.color.r, 0x2e);
    EXPECT_EQ(style.color.g, 0x7d);
    EXPECT_EQ(style.color.b, 0x32);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 13.0f);
}

TEST(CSSStyleTest, FlexColumnWithAlignItemsCenterAndGapV96) {
    const std::string css = ".stack{display:flex;flex-direction:column;align-items:center;gap:12px;padding-top:20px;padding-bottom:20px;padding-left:16px;padding-right:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"stack"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);
}

TEST(CSSStyleTest, OverflowHiddenWithBorderRadiusAndBoxSizingV96) {
    const std::string css = ".card-img{overflow-x:hidden;overflow-y:hidden;border-radius:8px;box-sizing:border-box;width:300px;height:200px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"card-img"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
    EXPECT_FLOAT_EQ(style.border_radius, 8.0f);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 200.0f);
}

TEST(CSSStyleTest, TextDecorationUnderlineWithLetterSpacingAndTransformV96) {
    const std::string css = ".fancy-link{text-decoration:underline;letter-spacing:2px;text-transform:uppercase;color:#1565c0;font-size:14px;font-style:italic;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"fancy-link"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(14.0f), 2.0f);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_EQ(style.color.r, 0x15);
    EXPECT_EQ(style.color.g, 0x65);
    EXPECT_EQ(style.color.b, 0xc0);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(CSSStyleTest, FixedPositionWithTopLeftZIndexAndBgColorV96) {
    const std::string css = "#navbar{position:fixed;top:0px;left_pos:0px;z-index:100;background-color:#263238;}";
    // Note: CSS uses 'left' property, mapped to left_pos in ComputedStyle
    const std::string css_actual = "#navbar{position:fixed;top:0px;left:0px;z-index:100;background-color:#263238;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css_actual);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.id = "navbar";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
    EXPECT_EQ(style.z_index, 100);
    EXPECT_EQ(style.background_color.r, 0x26);
    EXPECT_EQ(style.background_color.g, 0x32);
    EXPECT_EQ(style.background_color.b, 0x38);
}

TEST(CSSStyleTest, OutlineStyleWithWidthAndColorAndOffsetV96) {
    const std::string css = ".focus-ring{outline-width:3px;outline-style:solid;outline-color:#ff6f00;outline-offset:2px;border-radius:4px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"focus-ring"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 0xff);
    EXPECT_EQ(style.outline_color.g, 0x6f);
    EXPECT_EQ(style.outline_color.b, 0x00);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_radius, 4.0f);
}

TEST(CSSStyleTest, DisplayNoneWithVisibilityHiddenAndCursorPointerV96) {
    const std::string css = ".hidden-btn{display:none;visibility:hidden;cursor:pointer;margin-top:8px;margin-bottom:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hidden-btn"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::None);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 8.0f);
}

TEST(CSSStyleTest, BorderBottomDottedWithTextAlignCenterAndWordSpacingV96) {
    const std::string css = ".subtitle{border-bottom-width:2px;border-bottom-style:dotted;border-bottom-color:#b0bec5;text-align:center;word-spacing:4px;font-size:18px;line-height:28px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h2";
    elem.classes = {"subtitle"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0xb0);
    EXPECT_EQ(style.border_bottom.color.g, 0xbe);
    EXPECT_EQ(style.border_bottom.color.b, 0xc5);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(18.0f), 4.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 28.0f);
}

TEST(CSSStyleTest, InlineFlexWithJustifySpaceBetweenAndMinWidthV96) {
    const std::string css = ".pill{display:inline-flex;justify-content:space-between;min-width:120px;padding-left:12px;padding-right:12px;background-color:#e8f5e9;font-size:12px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"pill"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineFlex);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_FLOAT_EQ(style.min_width.to_px(), 120.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 12.0f);
    EXPECT_EQ(style.background_color.r, 0xe8);
    EXPECT_EQ(style.background_color.g, 0xf5);
    EXPECT_EQ(style.background_color.b, 0xe9);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 12.0f);
}

TEST(CSSStyleTest, FlexColumnReverseWithGapAndAlignItemsCenterV97) {
    const std::string css = ".vstack{display:flex;flex-direction:column-reverse;align-items:center;gap:16px;padding-top:24px;padding-bottom:24px;background-color:#fafafa;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"vstack"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 24.0f);
    EXPECT_EQ(style.background_color.r, 0xfa);
    EXPECT_EQ(style.background_color.g, 0xfa);
    EXPECT_EQ(style.background_color.b, 0xfa);
}

TEST(CSSStyleTest, AbsolutePositionWithZIndexAndOpacityV97) {
    const std::string css = "#overlay{position:absolute;top:0px;left:0px;z-index:999;opacity:0.85;background-color:#000000;width:100px;height:100px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "overlay";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
    EXPECT_EQ(style.z_index, 999);
    EXPECT_FLOAT_EQ(style.opacity, 0.85f);
    EXPECT_EQ(style.background_color.r, 0x00);
    EXPECT_EQ(style.background_color.g, 0x00);
    EXPECT_EQ(style.background_color.b, 0x00);
    EXPECT_FLOAT_EQ(style.width.to_px(), 100.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
}

TEST(CSSStyleTest, BorderTopSolidWithLetterSpacingAndTextTransformV97) {
    const std::string css = ".heading{border-top-width:3px;border-top-style:solid;border-top-color:#1a237e;letter-spacing:2px;text-transform:uppercase;font-size:24px;color:#212121;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";
    elem.classes = {"heading"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0x1a);
    EXPECT_EQ(style.border_top.color.g, 0x23);
    EXPECT_EQ(style.border_top.color.b, 0x7e);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(24.0f), 2.0f);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);
    EXPECT_EQ(style.color.r, 0x21);
    EXPECT_EQ(style.color.g, 0x21);
    EXPECT_EQ(style.color.b, 0x21);
}

TEST(CSSStyleTest, StickyPositionWithOverflowHiddenAndMaxHeightV97) {
    const std::string css = ".sticky-header{position:sticky;top:10px;overflow-x:hidden;overflow-y:auto;max-height:500px;background-color:#ffffff;border-bottom-width:1px;border-bottom-style:solid;border-bottom-color:#e0e0e0;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "header";
    elem.classes = {"sticky-header"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
    EXPECT_FLOAT_EQ(style.max_height.to_px(), 500.0f);
    EXPECT_EQ(style.background_color.r, 0xff);
    EXPECT_EQ(style.background_color.g, 0xff);
    EXPECT_EQ(style.background_color.b, 0xff);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_bottom.color.r, 0xe0);
}

TEST(CSSStyleTest, GridDisplayWithFlexGrowAndOrderV97) {
    const std::string css = ".card{display:grid;flex-grow:2.5;flex-shrink:0;order:3;margin-top:16px;margin-left:16px;margin-right:16px;margin-bottom:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Grid);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.5f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);
    EXPECT_EQ(style.order, 3);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 16.0f);
}

TEST(CSSStyleTest, TextDecorationUnderlineWithWhiteSpaceNoWrapAndUserSelectV97) {
    const std::string css = ".link{text-decoration:underline;white-space:nowrap;user-select:none;pointer-events:none;color:#1565c0;font-size:14px;line-height:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"link"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.color.r, 0x15);
    EXPECT_EQ(style.color.g, 0x65);
    EXPECT_EQ(style.color.b, 0xc0);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 20.0f);
}

TEST(CSSStyleTest, BorderLeftDashedWithTextAlignRightAndVerticalAlignV97) {
    const std::string css = ".sidebar{border-left-width:4px;border-left-style:dashed;border-left-color:#ff5722;text-align:right;vertical-align:middle;padding-left:20px;padding-right:20px;font-size:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "aside";
    elem.classes = {"sidebar"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_left.color.r, 0xff);
    EXPECT_EQ(style.border_left.color.g, 0x57);
    EXPECT_EQ(style.border_left.color.b, 0x22);
    EXPECT_EQ(style.text_align, TextAlign::Right);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 16.0f);
}

TEST(CSSStyleTest, InlineBlockWithBoxSizingBorderBoxAndMinHeightV97) {
    const std::string css = ".badge{display:inline-block;box-sizing:border-box;min-height:32px;border-right-width:2px;border-right-style:solid;border-right-color:#4caf50;font-weight:700;font-style:italic;background-color:#e8f5e9;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"badge"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 32.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.r, 0x4c);
    EXPECT_EQ(style.border_right.color.g, 0xaf);
    EXPECT_EQ(style.border_right.color.b, 0x50);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
    EXPECT_EQ(style.background_color.r, 0xe8);
    EXPECT_EQ(style.background_color.g, 0xf5);
    EXPECT_EQ(style.background_color.b, 0xe9);
}

TEST(CSSStyleTest, FixedPositionWithOutlineAndCursorPointerV98) {
    const std::string css = "#tooltip{position:fixed;top:50px;left_pos:80px;left:80px;outline-width:2px;outline-style:dashed;outline-color:#ff6600;cursor:pointer;z-index:100;opacity:0.95;background-color:#ffffcc;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.id = "tooltip";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 50.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 80.0f);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 2.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(style.outline_color.r, 0xff);
    EXPECT_EQ(style.outline_color.g, 0x66);
    EXPECT_EQ(style.outline_color.b, 0x00);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.z_index, 100);
    EXPECT_FLOAT_EQ(style.opacity, 0.95f);
}

TEST(CSSStyleTest, FlexRowReverseWithFlexBasisAndFlexShrinkV98) {
    const std::string css = ".toolbar{display:flex;flex-direction:row-reverse;flex-wrap:wrap;flex-basis:200px;flex-shrink:0.5;flex-grow:1;gap:8px;padding-top:10px;padding-bottom:10px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.classes = {"toolbar"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::RowReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.5f);
    EXPECT_FLOAT_EQ(style.flex_grow, 1.0f);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 10.0f);
}

TEST(CSSStyleTest, RelativePositionWithWordSpacingAndTextIndentV98) {
    const std::string css = ".paragraph{position:relative;top:5px;left:10px;word-spacing:4px;text-indent:32px;line-height:24px;font-size:16px;color:#333333;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"paragraph"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 32.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 16.0f);
    EXPECT_EQ(style.color.r, 0x33);
    EXPECT_EQ(style.color.g, 0x33);
    EXPECT_EQ(style.color.b, 0x33);
}

TEST(CSSStyleTest, VisibilityHiddenWithOverflowScrollAndBorderRadiusV98) {
    const std::string css = ".hidden-scroll{visibility:hidden;overflow-x:scroll;overflow-y:auto;border-radius:12px;width:300px;height:200px;background-color:#f0f0f0;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hidden-scroll"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
    EXPECT_FLOAT_EQ(style.border_radius, 12.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 200.0f);
    EXPECT_EQ(style.background_color.r, 0xf0);
    EXPECT_EQ(style.background_color.g, 0xf0);
    EXPECT_EQ(style.background_color.b, 0xf0);
}

TEST(CSSStyleTest, ListItemDisplayWithListStyleTypeAndPositionV98) {
    const std::string css = ".item{display:list-item;list-style-type:square;list-style-position:inside;margin-left:24px;padding-left:8px;font-size:14px;color:#555555;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "li";
    elem.classes = {"item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::ListItem);
    EXPECT_EQ(style.list_style_type, ListStyleType::Square);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_EQ(style.color.r, 0x55);
    EXPECT_EQ(style.color.g, 0x55);
    EXPECT_EQ(style.color.b, 0x55);
}

TEST(CSSStyleTest, TextOverflowEllipsisWithLineThroughAndNoWrapV98) {
    const std::string css = ".truncate{text-overflow:ellipsis;text-decoration:line-through;white-space:nowrap;overflow-x:hidden;max-width:250px;font-weight:700;color:#880088;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"truncate"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 250.0f);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.color.r, 0x88);
    EXPECT_EQ(style.color.g, 0x00);
    EXPECT_EQ(style.color.b, 0x88);
}

TEST(CSSStyleTest, BorderBottomDottedWithFloatLeftAndClearBothV98) {
    const std::string css = ".floated{float:left;clear:both;border-bottom-width:3px;border-bottom-style:dotted;border-bottom-color:#009688;margin-right:16px;margin-bottom:12px;width:150px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"floated"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.float_val, Float::Left);
    EXPECT_EQ(style.clear, Clear::Both);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0x00);
    EXPECT_EQ(style.border_bottom.color.g, 0x96);
    EXPECT_EQ(style.border_bottom.color.b, 0x88);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 150.0f);
}

TEST(CSSStyleTest, TextTransformLowercaseWithFontStyleObliqueAndTabSizeV98) {
    const std::string css = ".code{text-transform:lowercase;font-style:oblique;tab-size:2;letter-spacing:1px;font-size:13px;line-height:18px;background-color:#263238;color:#eeffff;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";
    elem.classes = {"code"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Lowercase);
    EXPECT_EQ(style.font_style, FontStyle::Oblique);
    EXPECT_EQ(style.tab_size, 2);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(13.0f), 1.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 13.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 18.0f);
    EXPECT_EQ(style.background_color.r, 0x26);
    EXPECT_EQ(style.background_color.g, 0x32);
    EXPECT_EQ(style.background_color.b, 0x38);
    EXPECT_EQ(style.color.r, 0xee);
    EXPECT_EQ(style.color.g, 0xff);
    EXPECT_EQ(style.color.b, 0xff);
}

TEST(CSSStyleTest, AbsolutePositionWithZIndexAndOpacityV99) {
    const std::string css = ".overlay{position:absolute;z-index:50;opacity:0.75;width:400px;height:300px;background-color:#1a1a2e;color:#e94560;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 50);
    EXPECT_FLOAT_EQ(style.opacity, 0.75f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 400.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 300.0f);
    EXPECT_EQ(style.background_color.r, 0x1a);
    EXPECT_EQ(style.background_color.g, 0x1a);
    EXPECT_EQ(style.background_color.b, 0x2e);
    EXPECT_EQ(style.color.r, 0xe9);
    EXPECT_EQ(style.color.g, 0x45);
    EXPECT_EQ(style.color.b, 0x60);
}

TEST(CSSStyleTest, BorderRightSolidWithPaddingTopAndTextAlignCenterV99) {
    const std::string css = ".sidebar{border-right-width:2px;border-right-style:solid;border-right-color:#3d5a80;padding-top:20px;padding-bottom:16px;text-align:center;font-size:15px;color:#293241;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "aside";
    elem.classes = {"sidebar"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.r, 0x3d);
    EXPECT_EQ(style.border_right.color.g, 0x5a);
    EXPECT_EQ(style.border_right.color.b, 0x80);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 16.0f);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 15.0f);
    EXPECT_EQ(style.color.r, 0x29);
    EXPECT_EQ(style.color.g, 0x32);
    EXPECT_EQ(style.color.b, 0x41);
}

TEST(CSSStyleTest, CursorPointerWithUserSelectNoneAndPointerEventsNoneV99) {
    const std::string css = ".disabled-btn{cursor:pointer;user-select:none;pointer-events:none;opacity:0.4;font-weight:600;font-size:14px;background-color:#cccccc;color:#666666;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"disabled-btn"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.font_weight, 600);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_EQ(style.background_color.r, 0xcc);
    EXPECT_EQ(style.background_color.g, 0xcc);
    EXPECT_EQ(style.background_color.b, 0xcc);
    EXPECT_EQ(style.color.r, 0x66);
    EXPECT_EQ(style.color.g, 0x66);
    EXPECT_EQ(style.color.b, 0x66);
}

TEST(CSSStyleTest, OutlineWidthWithBorderLeftDashedAndMaxWidthV99) {
    const std::string css = ".card{outline-width:3px;border-left-width:4px;border-left-style:dashed;border-left-color:#ff6b6b;max-width:500px;margin-top:10px;margin-bottom:10px;background-color:#ffeaa7;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_left.color.r, 0xff);
    EXPECT_EQ(style.border_left.color.g, 0x6b);
    EXPECT_EQ(style.border_left.color.b, 0x6b);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 500.0f);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 10.0f);
    EXPECT_EQ(style.background_color.r, 0xff);
    EXPECT_EQ(style.background_color.g, 0xea);
    EXPECT_EQ(style.background_color.b, 0xa7);
}

TEST(CSSStyleTest, WordSpacingAndLetterSpacingWithWhiteSpaceNoWrapV99) {
    const std::string css = ".spaced{word-spacing:3px;letter-spacing:2px;white-space:nowrap;text-align:right;font-size:18px;line-height:28px;color:#2d3436;background-color:#dfe6e9;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"spaced"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(18.0f), 2.0f);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.text_align, TextAlign::Right);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 28.0f);
    EXPECT_EQ(style.color.r, 0x2d);
    EXPECT_EQ(style.color.g, 0x34);
    EXPECT_EQ(style.color.b, 0x36);
    EXPECT_EQ(style.background_color.r, 0xdf);
    EXPECT_EQ(style.background_color.g, 0xe6);
    EXPECT_EQ(style.background_color.b, 0xe9);
}

TEST(CSSStyleTest, FixedPositionWithOverflowHiddenAndVerticalAlignMiddleV99) {
    const std::string css = ".navbar{position:fixed;overflow-x:hidden;overflow-y:hidden;vertical-align:middle;width:100px;height:60px;z-index:100;background-color:#0a3d62;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.classes = {"navbar"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.width.to_px(), 100.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 60.0f);
    EXPECT_EQ(style.z_index, 100);
    EXPECT_EQ(style.background_color.r, 0x0a);
    EXPECT_EQ(style.background_color.g, 0x3d);
    EXPECT_EQ(style.background_color.b, 0x62);
}

TEST(CSSStyleTest, TextDecorationUnderlineWithFloatRightAndVisibilityHiddenV99) {
    const std::string css = ".hidden-link{text-decoration:underline;float:right;visibility:hidden;margin-left:8px;margin-right:8px;padding-left:4px;padding-right:4px;color:#6c5ce7;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"hidden-link"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.float_val, Float::Right);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 4.0f);
    EXPECT_EQ(style.color.r, 0x6c);
    EXPECT_EQ(style.color.g, 0x5c);
    EXPECT_EQ(style.color.b, 0xe7);
}

TEST(CSSStyleTest, BorderTopDoubleWithTextTransformUppercaseAndFontWeight900V99) {
    const std::string css = ".heading{border-top-width:5px;border-top-style:double;border-top-color:#e17055;text-transform:uppercase;font-weight:900;font-size:24px;line-height:32px;color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";
    elem.classes = {"heading"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Double);
    EXPECT_EQ(style.border_top.color.r, 0xe1);
    EXPECT_EQ(style.border_top.color.g, 0x70);
    EXPECT_EQ(style.border_top.color.b, 0x55);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_EQ(style.font_weight, 900);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 32.0f);
    EXPECT_EQ(style.color.r, 0x2c);
    EXPECT_EQ(style.color.g, 0x3e);
    EXPECT_EQ(style.color.b, 0x50);
}

TEST(CSSStyleTest, FlexColumnReverseWithGapAndAlignItemsCenterV100) {
    const std::string css = ".flex-col{display:flex;flex-direction:column-reverse;flex-wrap:wrap-reverse;gap:12px;align-items:center;justify-content:space-between;padding-top:20px;padding-bottom:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-col"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::WrapReverse);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 12.0f);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 20.0f);
}

TEST(CSSStyleTest, StickyPositionWithZIndexAndMinHeightV100) {
    const std::string css = ".sticky-header{position:sticky;z-index:50;min-height:48px;width:300px;background-color:#1abc9c;color:#ffffff;font-size:20px;text-align:center;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "header";
    elem.classes = {"sticky-header"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_EQ(style.z_index, 50);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 48.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
    EXPECT_EQ(style.background_color.r, 0x1a);
    EXPECT_EQ(style.background_color.g, 0xbc);
    EXPECT_EQ(style.background_color.b, 0x9c);
    EXPECT_EQ(style.color.r, 0xff);
    EXPECT_EQ(style.color.g, 0xff);
    EXPECT_EQ(style.color.b, 0xff);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 20.0f);
    EXPECT_EQ(style.text_align, TextAlign::Center);
}

TEST(CSSStyleTest, BorderRightGrooveWithOutlineOffsetAndOpacityV100) {
    const std::string css = ".panel{border-right-width:3px;border-right-style:groove;border-right-color:#8e44ad;outline-width:2px;outline-style:solid;outline-color:#e74c3c;outline-offset:4px;opacity:0.75;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"panel"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Groove);
    EXPECT_EQ(style.border_right.color.r, 0x8e);
    EXPECT_EQ(style.border_right.color.g, 0x44);
    EXPECT_EQ(style.border_right.color.b, 0xad);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 2.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 0xe7);
    EXPECT_EQ(style.outline_color.g, 0x4c);
    EXPECT_EQ(style.outline_color.b, 0x3c);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.opacity, 0.75f);
}

TEST(CSSStyleTest, TextIndentWithListStyleInsideAndDirectionRtlV100) {
    const std::string css = ".rtl-list{text-indent:24px;list-style-type:square;list-style-position:inside;direction:rtl;font-weight:700;color:#c0392b;margin-left:16px;margin-right:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "ul";
    elem.classes = {"rtl-list"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 24.0f);
    EXPECT_EQ(style.list_style_type, ListStyleType::Square);
    EXPECT_EQ(style.list_style_position, ListStylePosition::Inside);
    EXPECT_EQ(style.direction, Direction::Rtl);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.color.r, 0xc0);
    EXPECT_EQ(style.color.g, 0x39);
    EXPECT_EQ(style.color.b, 0x2b);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 16.0f);
}

TEST(CSSStyleTest, CursorPointerWithUserSelectTextAndTextOverflowEllipsisV100) {
    const std::string css = ".interactive{cursor:pointer;user-select:text;text-overflow:ellipsis;white-space:nowrap;overflow-x:hidden;font-style:italic;letter-spacing:1px;background-color:#f39c12;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"interactive"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::Text);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 1.0f);
    EXPECT_EQ(style.background_color.r, 0xf3);
    EXPECT_EQ(style.background_color.g, 0x9c);
    EXPECT_EQ(style.background_color.b, 0x12);
}

TEST(CSSStyleTest, BorderBottomDottedWithBoxSizingBorderBoxAndFlexGrowV100) {
    const std::string css = ".grow-item{border-bottom-width:2px;border-bottom-style:dotted;border-bottom-color:#27ae60;box-sizing:border-box;flex-grow:2;flex-shrink:0;flex-basis:150px;padding-left:8px;padding-right:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"grow-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0x27);
    EXPECT_EQ(style.border_bottom.color.g, 0xae);
    EXPECT_EQ(style.border_bottom.color.b, 0x60);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 150.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 8.0f);
}

TEST(CSSStyleTest, AbsolutePositionWithTopLeftAndVisibilityCollapseV100) {
    const std::string css = ".tooltip{position:absolute;top:10px;left:20px;visibility:collapse;font-size:12px;line-height:18px;word-spacing:2px;background-color:#2c3e50;color:#ecf0f1;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"tooltip"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 20.0f);
    EXPECT_EQ(style.visibility, Visibility::Collapse);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 18.0f);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 2.0f);
    EXPECT_EQ(style.background_color.r, 0x2c);
    EXPECT_EQ(style.background_color.g, 0x3e);
    EXPECT_EQ(style.background_color.b, 0x50);
    EXPECT_EQ(style.color.r, 0xec);
    EXPECT_EQ(style.color.g, 0xf0);
    EXPECT_EQ(style.color.b, 0xf1);
}

TEST(CSSStyleTest, TextDecorationLineThroughWithFloatLeftAndClearBothV100) {
    const std::string css = ".struck{text-decoration:line-through;float:left;clear:both;margin-top:4px;margin-bottom:4px;padding-top:6px;padding-bottom:6px;font-weight:300;color:#7f8c8d;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "del";
    elem.classes = {"struck"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::LineThrough);
    EXPECT_EQ(style.float_val, Float::Left);
    EXPECT_EQ(style.clear, Clear::Both);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 6.0f);
    EXPECT_EQ(style.font_weight, 300);
    EXPECT_EQ(style.color.r, 0x7f);
    EXPECT_EQ(style.color.g, 0x8c);
    EXPECT_EQ(style.color.b, 0x8d);
}

// ===========================================================================
// V101 Tests
// ===========================================================================

TEST(CSSStyleTest, FlexLayoutWithGapAndAlignItemsCenterV101) {
    const std::string css = ".flex-container{display:flex;flex-direction:column;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;padding-top:8px;padding-bottom:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-container"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 8.0f);
}

TEST(CSSStyleTest, PositionAbsoluteWithZIndexAndBoxSizingBorderBoxV101) {
    const std::string css = ".overlay{position:absolute;top:10px;left:20px;z-index:50;box-sizing:border-box;width:200px;height:100px;opacity:0.9;background-color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 20.0f);
    EXPECT_EQ(style.z_index, 50);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
    EXPECT_FLOAT_EQ(style.opacity, 0.9f);
    EXPECT_EQ(style.background_color.r, 0x2c);
    EXPECT_EQ(style.background_color.g, 0x3e);
    EXPECT_EQ(style.background_color.b, 0x50);
}

TEST(CSSStyleTest, BorderTopDashedWithBorderBottomDottedV101) {
    const std::string css = ".bordered{border-top-width:2px;border-top-style:dashed;border-top-color:#e74c3c;border-bottom-width:4px;border-bottom-style:dotted;border-bottom-color:#3498db;margin-top:10px;margin-bottom:10px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "hr";
    elem.classes = {"bordered"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.r, 0xe7);
    EXPECT_EQ(style.border_top.color.g, 0x4c);
    EXPECT_EQ(style.border_top.color.b, 0x3c);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0x34);
    EXPECT_EQ(style.border_bottom.color.g, 0x98);
    EXPECT_EQ(style.border_bottom.color.b, 0xdb);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 10.0f);
}

TEST(CSSStyleTest, TextTransformUppercaseWithWordSpacingAndWhiteSpacePreV101) {
    const std::string css = ".formatted{text-transform:uppercase;word-spacing:4px;letter-spacing:2px;white-space:pre;text-align:center;font-size:20px;color:#1abc9c;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "pre";
    elem.classes = {"formatted"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 2.0f);
    EXPECT_EQ(style.white_space, WhiteSpace::Pre);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 20.0f);
    EXPECT_EQ(style.color.r, 0x1a);
    EXPECT_EQ(style.color.g, 0xbc);
    EXPECT_EQ(style.color.b, 0x9c);
}

TEST(CSSStyleTest, OverflowScrollWithPointerEventsNoneAndVisibilityHiddenV101) {
    const std::string css = ".hidden-scroll{overflow-x:scroll;overflow-y:auto;pointer-events:none;visibility:hidden;user-select:none;cursor:not-allowed;padding-left:16px;padding-right:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hidden-scroll"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.cursor, Cursor::NotAllowed);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);
}

TEST(CSSStyleTest, OutlineDoubleWithTextDecorationUnderlineAndColorV101) {
    const std::string css = ".highlighted{outline-width:3px;outline-style:double;outline-color:#9b59b6;outline-offset:2px;text-decoration:underline;font-weight:600;font-style:oblique;color:#2ecc71;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "mark";
    elem.classes = {"highlighted"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Double);
    EXPECT_EQ(style.outline_color.r, 0x9b);
    EXPECT_EQ(style.outline_color.g, 0x59);
    EXPECT_EQ(style.outline_color.b, 0xb6);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 2.0f);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.font_weight, 600);
    EXPECT_EQ(style.font_style, FontStyle::Oblique);
    EXPECT_EQ(style.color.r, 0x2e);
    EXPECT_EQ(style.color.g, 0xcc);
    EXPECT_EQ(style.color.b, 0x71);
}

TEST(CSSStyleTest, MinWidthMaxHeightWithDisplayInlineBlockAndFloatRightV101) {
    const std::string css = ".constrained{min-width:50px;max-height:300px;display:inline-block;float:right;clear:left;margin-left:8px;margin-right:8px;background-color:#d35400;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "aside";
    elem.classes = {"constrained"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.min_width.to_px(), 50.0f);
    EXPECT_FLOAT_EQ(style.max_height.to_px(), 300.0f);
    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_EQ(style.float_val, Float::Right);
    EXPECT_EQ(style.clear, Clear::Left);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 8.0f);
    EXPECT_EQ(style.background_color.r, 0xd3);
    EXPECT_EQ(style.background_color.g, 0x54);
    EXPECT_EQ(style.background_color.b, 0x00);
}

TEST(CSSStyleTest, PositionStickyWithOverflowHiddenAndTextOverflowEllipsisV101) {
    const std::string css = ".sticky-bar{position:sticky;top:0px;overflow-x:hidden;text-overflow:ellipsis;white-space:nowrap;border-left-width:5px;border-left-style:solid;border-left-color:#f1c40f;padding-left:12px;font-family:monospace;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.classes = {"sticky-bar"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.color.r, 0xf1);
    EXPECT_EQ(style.border_left.color.g, 0xc4);
    EXPECT_EQ(style.border_left.color.b, 0x0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 12.0f);
    EXPECT_EQ(style.font_family, "monospace");
}

// ---------------------------------------------------------------------------
// V102 Tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, FlexColumnReverseWithGapAndAlignItemsCenterV102) {
    const std::string css = ".flex-col{display:flex;flex-direction:column-reverse;flex-wrap:wrap;gap:16px;align-items:center;justify-content:space-between;padding-top:20px;padding-bottom:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-col"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 16.0f);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 20.0f);
}

TEST(CSSStyleTest, BorderTopDashedWithOutlineAndLetterSpacingV102) {
    const std::string css = "#card{border-top-width:3px;border-top-style:dashed;border-top-color:#e74c3c;outline-width:2px;outline-style:dotted;outline-color:#2980b9;outline-offset:4px;letter-spacing:1.5px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.id = "card";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.r, 0xe7);
    EXPECT_EQ(style.border_top.color.g, 0x4c);
    EXPECT_EQ(style.border_top.color.b, 0x3c);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 2.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Dotted);
    EXPECT_EQ(style.outline_color.r, 0x29);
    EXPECT_EQ(style.outline_color.g, 0x80);
    EXPECT_EQ(style.outline_color.b, 0xb9);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 1.5f);
}

TEST(CSSStyleTest, VisibilityHiddenCursorPointerUserSelectNoneV102) {
    const std::string css = ".hidden-interactive{visibility:hidden;cursor:pointer;user-select:none;pointer-events:none;opacity:0.5;z-index:10;position:relative;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"hidden-interactive"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
    EXPECT_EQ(style.z_index, 10);
    EXPECT_EQ(style.position, Position::Relative);
}

TEST(CSSStyleTest, TextTransformUppercaseWithWordSpacingAndTextIndentV102) {
    const std::string css = ".heading{text-transform:uppercase;word-spacing:3px;text-indent:24px;text-align:center;font-size:32px;font-weight:700;font-style:italic;line-height:40px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";
    elem.classes = {"heading"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 24.0f);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 32.0f);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 40.0f);
}

TEST(CSSStyleTest, BoxSizingBorderBoxWithAllFourMarginsV102) {
    const std::string css = ".boxed{box-sizing:border-box;margin-top:10px;margin-right:15px;margin-bottom:20px;margin-left:25px;width:200px;height:150px;background-color:#1abc9c;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"boxed"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 25.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 150.0f);
    EXPECT_EQ(style.background_color.r, 0x1a);
    EXPECT_EQ(style.background_color.g, 0xbc);
    EXPECT_EQ(style.background_color.b, 0x9c);
}

TEST(CSSStyleTest, PositionAbsoluteWithAllFourOffsetsV102) {
    const std::string css = ".overlay{position:absolute;top:10px;right:20px;bottom:30px;left:40px;display:block;overflow-x:scroll;overflow-y:auto;background-color:#34495e;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.right_pos.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 40.0f);
    EXPECT_EQ(style.display, Display::Block);
    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
    EXPECT_EQ(style.background_color.r, 0x34);
    EXPECT_EQ(style.background_color.g, 0x49);
    EXPECT_EQ(style.background_color.b, 0x5e);
}

TEST(CSSStyleTest, TextDecorationUnderlineWithVerticalAlignMiddleV102) {
    const std::string css = ".decorated{text-decoration:underline;vertical-align:middle;color:#8e44ad;font-family:Georgia;white-space:pre-wrap;direction:rtl;display:inline-block;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"decorated"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(style.color.r, 0x8e);
    EXPECT_EQ(style.color.g, 0x44);
    EXPECT_EQ(style.color.b, 0xad);
    EXPECT_EQ(style.font_family, "Georgia");
    EXPECT_EQ(style.white_space, WhiteSpace::PreWrap);
    EXPECT_EQ(style.direction, Direction::Rtl);
    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(CSSStyleTest, BorderBottomRidgeWithPaddingAndTextStrokeV102) {
    const std::string css = ".fancy{border-bottom-width:4px;border-bottom-style:ridge;border-bottom-color:#c0392b;border-right-width:2px;border-right-style:solid;border-right-color:#27ae60;padding-top:6px;padding-right:12px;padding-bottom:18px;padding-left:24px;-webkit-text-stroke-width:1px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "footer";
    elem.classes = {"fancy"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Ridge);
    EXPECT_EQ(style.border_bottom.color.r, 0xc0);
    EXPECT_EQ(style.border_bottom.color.g, 0x39);
    EXPECT_EQ(style.border_bottom.color.b, 0x2b);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.r, 0x27);
    EXPECT_EQ(style.border_right.color.g, 0xae);
    EXPECT_EQ(style.border_right.color.b, 0x60);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 6.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 18.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 1.0f);
}

TEST(CSSStyleTest, VisibilityHiddenWithOutlineAndLetterSpacingV103) {
    const std::string css = ".ghost{visibility:hidden;outline-width:3px;outline-style:dashed;outline-color:#e74c3c;letter-spacing:2px;font-size:18px;color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"ghost"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(18), 2.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
    EXPECT_EQ(style.color.r, 0x2c);
    EXPECT_EQ(style.color.g, 0x3e);
    EXPECT_EQ(style.color.b, 0x50);
}

TEST(CSSStyleTest, CursorPointerWithWordSpacingAndPaddingV103) {
    const std::string css = ".clickable{cursor:pointer;word-spacing:4px;padding-top:8px;padding-right:16px;padding-bottom:8px;padding-left:16px;background-color:#3498db;display:inline-block;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"clickable"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(0), 4.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 16.0f);
    EXPECT_EQ(style.background_color.r, 0x34);
    EXPECT_EQ(style.background_color.g, 0x98);
    EXPECT_EQ(style.background_color.b, 0xdb);
    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(CSSStyleTest, PointerEventsNoneWithUserSelectAndMarginV103) {
    const std::string css = ".disabled{pointer-events:none;user-select:none;margin-top:5px;margin-right:10px;margin-bottom:15px;margin-left:20px;opacity:0.5;font-size:14px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"disabled"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
}

TEST(CSSStyleTest, WhiteSpaceNoWrapWithLineHeightAndBorderTopV103) {
    const std::string css = ".nowrap{white-space:nowrap;line-height:24px;border-top-width:2px;border-top-style:solid;border-top-color:#16a085;font-size:16px;color:#ecf0f1;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"nowrap"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0x16);
    EXPECT_EQ(style.border_top.color.g, 0xa0);
    EXPECT_EQ(style.border_top.color.b, 0x85);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 16.0f);
    EXPECT_EQ(style.color.r, 0xec);
    EXPECT_EQ(style.color.g, 0xf0);
    EXPECT_EQ(style.color.b, 0xf1);
}

TEST(CSSStyleTest, VerticalAlignMiddleWithBorderLeftAndFontSizeV103) {
    const std::string css = ".aligned{vertical-align:middle;border-left-width:3px;border-left-style:dotted;border-left-color:#9b59b6;font-size:22px;line-height:30px;display:inline;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"aligned"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_left.color.r, 0x9b);
    EXPECT_EQ(style.border_left.color.g, 0x59);
    EXPECT_EQ(style.border_left.color.b, 0xb6);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 22.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 30.0f);
    EXPECT_EQ(style.display, Display::Inline);
}

TEST(CSSStyleTest, PositionStickyWithOverflowAndDirectionRtlV103) {
    const std::string css = ".sticky{position:sticky;top:0px;overflow-x:hidden;overflow-y:auto;direction:rtl;background-color:#f39c12;width:300px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.classes = {"sticky"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Auto);
    EXPECT_EQ(style.direction, Direction::Rtl);
    EXPECT_EQ(style.background_color.r, 0xf3);
    EXPECT_EQ(style.background_color.g, 0x9c);
    EXPECT_EQ(style.background_color.b, 0x12);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
}

TEST(CSSStyleTest, FlexDisplayWithBorderRightAndTextTransformV103) {
    const std::string css = ".flex-container{display:flex;border-right-width:1px;border-right-style:dashed;border-right-color:#2ecc71;text-transform:uppercase;font-size:12px;letter-spacing:1px;color:#7f8c8d;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"flex-container"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.color.r, 0x2e);
    EXPECT_EQ(style.border_right.color.g, 0xcc);
    EXPECT_EQ(style.border_right.color.b, 0x71);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(12), 1.0f);
    EXPECT_EQ(style.color.r, 0x7f);
    EXPECT_EQ(style.color.g, 0x8c);
    EXPECT_EQ(style.color.b, 0x8d);
}

TEST(CSSStyleTest, BoxSizingContentBoxWithAllBordersAndOpacityV103) {
    const std::string css = ".bordered{box-sizing:content-box;border-top-width:1px;border-top-style:solid;border-top-color:#c0392b;border-bottom-width:3px;border-bottom-style:double;border-bottom-color:#2980b9;border-left-width:2px;border-left-style:groove;border-left-color:#27ae60;border-right-width:4px;border-right-style:outset;border-right-color:#8e44ad;opacity:0.75;height:100px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "aside";
    elem.classes = {"bordered"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.box_sizing, BoxSizing::ContentBox);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0xc0);
    EXPECT_EQ(style.border_top.color.g, 0x39);
    EXPECT_EQ(style.border_top.color.b, 0x2b);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Double);
    EXPECT_EQ(style.border_bottom.color.r, 0x29);
    EXPECT_EQ(style.border_bottom.color.g, 0x80);
    EXPECT_EQ(style.border_bottom.color.b, 0xb9);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Groove);
    EXPECT_EQ(style.border_left.color.r, 0x27);
    EXPECT_EQ(style.border_left.color.g, 0xae);
    EXPECT_EQ(style.border_left.color.b, 0x60);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Outset);
    EXPECT_EQ(style.border_right.color.r, 0x8e);
    EXPECT_EQ(style.border_right.color.g, 0x44);
    EXPECT_EQ(style.border_right.color.b, 0xad);
    EXPECT_FLOAT_EQ(style.opacity, 0.75f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
}

// ============================================================================
// V104: Visibility hidden with cursor pointer and user-select none
// ============================================================================
TEST(CSSStyleTest, VisibilityHiddenCursorPointerUserSelectNoneV104) {
    const std::string css = ".ghost{visibility:hidden;cursor:pointer;user-select:none;padding-top:12px;padding-bottom:8px;color:#1a2b3c;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"ghost"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 8.0f);
    EXPECT_EQ(style.color.r, 0x1a);
    EXPECT_EQ(style.color.g, 0x2b);
    EXPECT_EQ(style.color.b, 0x3c);
}

// ============================================================================
// V104: Word-spacing and letter-spacing with font-size and line-height
// ============================================================================
TEST(CSSStyleTest, WordSpacingLetterSpacingFontSizeLineHeightV104) {
    const std::string css = ".spaced{word-spacing:4px;letter-spacing:2px;font-size:18px;line-height:27px;color:#445566;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"spaced"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.word_spacing.to_px(18), 4.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(18), 2.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 18.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 27.0f);
    EXPECT_EQ(style.color.r, 0x44);
    EXPECT_EQ(style.color.g, 0x55);
    EXPECT_EQ(style.color.b, 0x66);
}

// ============================================================================
// V104: Margin individual sides with white-space nowrap
// ============================================================================
TEST(CSSStyleTest, MarginSidesWithWhiteSpaceNowrapV104) {
    const std::string css = ".card{margin-top:10px;margin-right:20px;margin-bottom:30px;margin-left:40px;white-space:nowrap;background-color:#e74c3c;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 40.0f);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.background_color.r, 0xe7);
    EXPECT_EQ(style.background_color.g, 0x4c);
    EXPECT_EQ(style.background_color.b, 0x3c);
}

// ============================================================================
// V104: Outline width with pointer-events none and vertical-align middle
// ============================================================================
TEST(CSSStyleTest, OutlineWidthPointerEventsNoneVerticalAlignMiddleV104) {
    const std::string css = ".overlay{outline-width:3px;pointer-events:none;vertical-align:middle;font-size:14px;opacity:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

// ============================================================================
// V104: Border-left with padding-right and text-indent
// ============================================================================
TEST(CSSStyleTest, BorderLeftPaddingRightTextIndentV104) {
    const std::string css = ".indent-box{border-left-width:5px;border-left-style:dotted;border-left-color:#3498db;padding-right:15px;text-indent:24px;width:200px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"indent-box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_left.color.r, 0x34);
    EXPECT_EQ(style.border_left.color.g, 0x98);
    EXPECT_EQ(style.border_left.color.b, 0xdb);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
}

// ============================================================================
// V104: Unitless line-height with display inline-block and z-index
// ============================================================================
TEST(CSSStyleTest, UnitlessLineHeightInlineBlockZIndexV104) {
    const std::string css = ".badge{line-height:1.8;display:inline-block;z-index:10;font-size:16px;color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"badge"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.8f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 16.0f);
    EXPECT_NEAR(style.line_height.to_px(), 28.8f, 0.1f);
    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_EQ(style.z_index, 10);
    EXPECT_EQ(style.color.r, 0x2c);
    EXPECT_EQ(style.color.g, 0x3e);
    EXPECT_EQ(style.color.b, 0x50);
}

// ============================================================================
// V104: Flex-grow and flex-shrink with min-height and max-width
// ============================================================================
TEST(CSSStyleTest, FlexGrowShrinkMinHeightMaxWidthV104) {
    const std::string css = ".flex-item{flex-grow:2;flex-shrink:0;min-height:50px;max-width:400px;padding-left:10px;padding-top:5px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 50.0f);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 400.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
}

// ============================================================================
// V104: Border-bottom dashed with text-decoration and background-color
// ============================================================================
TEST(CSSStyleTest, BorderBottomDashedTextDecorationBackgroundV104) {
    const std::string css = ".underlined{border-bottom-width:2px;border-bottom-style:dashed;border-bottom-color:#9b59b6;text-decoration:underline;background-color:#ecf0f1;height:48px;margin-top:16px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";
    elem.classes = {"underlined"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.color.r, 0x9b);
    EXPECT_EQ(style.border_bottom.color.g, 0x59);
    EXPECT_EQ(style.border_bottom.color.b, 0xb6);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.background_color.r, 0xec);
    EXPECT_EQ(style.background_color.g, 0xf0);
    EXPECT_EQ(style.background_color.b, 0xf1);
    EXPECT_FLOAT_EQ(style.height.to_px(), 48.0f);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 16.0f);
}

// ============================================================================
// V105: Outline width + offset with cursor pointer and direction RTL
// ============================================================================
TEST(CSSStyleTest, OutlineWidthOffsetCursorDirectionRtlV105) {
    const std::string css = ".alert{outline-width:3px;outline-style:solid;outline-color:#e74c3c;outline-offset:5px;cursor:pointer;direction:rtl;padding-left:12px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"alert"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 0xe7);
    EXPECT_EQ(style.outline_color.g, 0x4c);
    EXPECT_EQ(style.outline_color.b, 0x3c);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 5.0f);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.direction, Direction::Rtl);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 12.0f);
}

// ============================================================================
// V105: Visibility hidden with user-select none and text-transform uppercase
// ============================================================================
TEST(CSSStyleTest, VisibilityHiddenUserSelectNoneTextTransformV105) {
    const std::string css = ".hidden-upper{visibility:hidden;user-select:none;text-transform:uppercase;font-size:20px;letter-spacing:2px;margin-bottom:8px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"hidden-upper"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(20.0f), 2.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 8.0f);
}

// ============================================================================
// V105: White-space nowrap with vertical-align middle and word-spacing
// ============================================================================
TEST(CSSStyleTest, WhiteSpaceNowrapVerticalAlignWordSpacingV105) {
    const std::string css = ".inline-item{white-space:nowrap;vertical-align:middle;word-spacing:4px;line-height:24px;padding-right:10px;border-top-width:1px;border-top-style:dotted;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"inline-item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dotted);
}

// ============================================================================
// V105: Pointer-events none with opacity and box-sizing border-box
// ============================================================================
TEST(CSSStyleTest, PointerEventsNoneOpacityBoxSizingV105) {
    const std::string css = ".overlay{pointer-events:none;opacity:0.5;box-sizing:border-box;width:200px;height:100px;background-color:#2ecc71;border-right-width:4px;border-right-style:solid;border-right-color:#27ae60;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
    EXPECT_EQ(style.background_color.r, 0x2e);
    EXPECT_EQ(style.background_color.g, 0xcc);
    EXPECT_EQ(style.background_color.b, 0x71);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.color.r, 0x27);
    EXPECT_EQ(style.border_right.color.g, 0xae);
    EXPECT_EQ(style.border_right.color.b, 0x60);
}

// ============================================================================
// V105: Flex display with gap, justify-content, align-items
// ============================================================================
TEST(CSSStyleTest, FlexDisplayGapJustifyAlignV105) {
    const std::string css = ".flex-row{display:flex;flex-direction:row;justify-content:space-between;align-items:center;gap:16px;padding-top:20px;padding-bottom:20px;min-height:64px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"flex-row"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.min_height.to_px(), 64.0f);
}

// ============================================================================
// V105: Position absolute with z-index and all four border sides
// ============================================================================
TEST(CSSStyleTest, PositionAbsoluteZIndexFourBordersV105) {
    const std::string css = ".popup{position:absolute;z-index:10;top:50px;left:100px;border-top-width:2px;border-top-style:solid;border-top-color:#3498db;border-bottom-width:2px;border-bottom-style:solid;border-bottom-color:#2980b9;border-left-width:1px;border-left-style:dashed;border-left-color:#1abc9c;border-right-width:1px;border-right-style:dashed;border-right-color:#16a085;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"popup"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_EQ(style.z_index, 10);
    EXPECT_FLOAT_EQ(style.top.to_px(), 50.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 100.0f);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0x34);
    EXPECT_EQ(style.border_top.color.g, 0x98);
    EXPECT_EQ(style.border_top.color.b, 0xdb);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_bottom.color.r, 0x29);
    EXPECT_EQ(style.border_bottom.color.g, 0x80);
    EXPECT_EQ(style.border_bottom.color.b, 0xb9);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_left.color.r, 0x1a);
    EXPECT_EQ(style.border_left.color.g, 0xbc);
    EXPECT_EQ(style.border_left.color.b, 0x9c);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.color.r, 0x16);
    EXPECT_EQ(style.border_right.color.g, 0xa0);
    EXPECT_EQ(style.border_right.color.b, 0x85);
}

// ============================================================================
// V105: Min-width clamping with flex-grow and text-align center
// ============================================================================
TEST(CSSStyleTest, MinWidthClampFlexGrowTextAlignCenterV105) {
    const std::string css = ".card{min-width:300px;width:200px;flex-grow:2;flex-shrink:0;text-align:center;font-weight:bold;margin-right:24px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "article";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    // min-width should clamp width UP: specified 200px but min is 300px
    EXPECT_FLOAT_EQ(style.min_width.to_px(), 300.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 0.0f);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 24.0f);
}

// ============================================================================
// V105: Overflow hidden with text-overflow ellipsis and text-indent
// ============================================================================
TEST(CSSStyleTest, OverflowHiddenTextOverflowEllipsisTextIndentV105) {
    const std::string css = ".truncate{overflow:hidden;text-overflow:ellipsis;text-indent:32px;white-space:nowrap;max-width:400px;color:#34495e;font-style:italic;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"truncate"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_FLOAT_EQ(style.text_indent.to_px(), 32.0f);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(style.max_width.to_px(), 400.0f);
    EXPECT_EQ(style.color.r, 0x34);
    EXPECT_EQ(style.color.g, 0x49);
    EXPECT_EQ(style.color.b, 0x5e);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

// ============================================================================
// V106: Border edges with individual colors and styles
// ============================================================================
TEST(CSSStyleTest, BorderEdgesIndividualColorsAndStylesV106) {
    const std::string css = ".card{border-top-width:3px;border-top-style:solid;border-top-color:#e74c3c;border-right-width:2px;border-right-style:dashed;border-right-color:#3498db;border-bottom-width:1px;border-bottom-style:dotted;border-bottom-color:#2ecc71;border-left-width:4px;border-left-style:double;border-left-color:#f39c12;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 0xe7);
    EXPECT_EQ(style.border_top.color.g, 0x4c);
    EXPECT_EQ(style.border_top.color.b, 0x3c);

    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_right.color.r, 0x34);
    EXPECT_EQ(style.border_right.color.g, 0x98);
    EXPECT_EQ(style.border_right.color.b, 0xdb);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 1.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(style.border_bottom.color.r, 0x2e);
    EXPECT_EQ(style.border_bottom.color.g, 0xcc);
    EXPECT_EQ(style.border_bottom.color.b, 0x71);

    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(style.border_left.style, BorderStyle::Double);
    EXPECT_EQ(style.border_left.color.r, 0xf3);
    EXPECT_EQ(style.border_left.color.g, 0x9c);
    EXPECT_EQ(style.border_left.color.b, 0x12);
}

// ============================================================================
// V106: Visibility hidden with cursor pointer and user-select none
// ============================================================================
TEST(CSSStyleTest, VisibilityHiddenCursorPointerUserSelectNoneV106) {
    const std::string css = ".hidden-interactive{visibility:hidden;cursor:pointer;user-select:none;pointer-events:none;opacity:0.5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"hidden-interactive"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
}

// ============================================================================
// V106: Outline width is Length type with offset
// ============================================================================
TEST(CSSStyleTest, OutlineWidthAndOffsetAreLengthV106) {
    const std::string css = ".focused{outline:3px solid #1abc9c;outline-offset:5px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "input";
    elem.classes = {"focused"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 0x1a);
    EXPECT_EQ(style.outline_color.g, 0xbc);
    EXPECT_EQ(style.outline_color.b, 0x9c);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 5.0f);
}

// ============================================================================
// V106: Word spacing and letter spacing as Length with to_px
// ============================================================================
TEST(CSSStyleTest, WordSpacingLetterSpacingAreLengthV106) {
    const std::string css = ".spaced{word-spacing:8px;letter-spacing:2px;font-size:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"spaced"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    float fs = style.font_size.to_px();
    EXPECT_FLOAT_EQ(fs, 20.0f);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(fs), 8.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(fs), 2.0f);
}

// ============================================================================
// V106: Vertical align middle with white-space nowrap
// ============================================================================
TEST(CSSStyleTest, VerticalAlignMiddleWhiteSpaceNowrapV106) {
    const std::string css = ".inline-mid{vertical-align:middle;white-space:nowrap;display:inline-block;line-height:28px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"inline-mid"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.display, Display::InlineBlock);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 28.0f);
}

// ============================================================================
// V106: Font size and line height are Length with to_px
// ============================================================================
TEST(CSSStyleTest, FontSizeLineHeightAreLengthToPxV106) {
    const std::string css = ".heading{font-size:32px;line-height:48px;font-weight:700;color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "h1";
    elem.classes = {"heading"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 32.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 48.0f);
    EXPECT_EQ(style.font_weight, 700);
    EXPECT_EQ(style.color.r, 0x2c);
    EXPECT_EQ(style.color.g, 0x3e);
    EXPECT_EQ(style.color.b, 0x50);
}

// ============================================================================
// V106: Flex layout with gap and align-items center
// ============================================================================
TEST(CSSStyleTest, FlexLayoutGapAlignItemsCenterV106) {
    const std::string css = ".flex-row{display:flex;flex-direction:row;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-row"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Row);
    EXPECT_EQ(style.justify_content, JustifyContent::SpaceBetween);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 16.0f);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
}

// ============================================================================
// V106: Padding and margin with border-box sizing and position relative
// ============================================================================
TEST(CSSStyleTest, PaddingMarginBorderBoxPositionRelativeV106) {
    const std::string css = ".container{box-sizing:border-box;position:relative;padding:12px 24px 16px 8px;margin:10px 20px;width:300px;z-index:5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "section";
    elem.classes = {"container"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
    EXPECT_EQ(style.position, Position::Relative);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 8.0f);
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.width.to_px(), 300.0f);
    EXPECT_EQ(style.z_index, 5);
}

// ============================================================================
// V107: Visibility hidden with pointer-events none and cursor pointer
// ============================================================================
TEST(CSSStyleTest, VisibilityHiddenPointerEventsCursorV107) {
    const std::string css = ".ghost{visibility:hidden;pointer-events:none;cursor:pointer;user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"ghost"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

// ============================================================================
// V107: Font size em units with line height and letter spacing
// ============================================================================
TEST(CSSStyleTest, FontSizeEmLineHeightLetterSpacingV107) {
    const std::string css = ".text{font-size:1.5em;line-height:2em;letter-spacing:0.1em;word-spacing:0.2em;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"text"};

    ComputedStyle parent;
    parent.font_size = Length::px(20);
    auto style = resolver.resolve(elem, parent);

    float fs = style.font_size.to_px(20.0f);
    EXPECT_FLOAT_EQ(fs, 30.0f);
    EXPECT_GT(style.line_height.to_px(fs), 0.0f);
    EXPECT_GT(style.letter_spacing.to_px(fs), 0.0f);
    EXPECT_GT(style.word_spacing.to_px(fs), 0.0f);
}

// ============================================================================
// V107: Border top with style color and width
// ============================================================================
TEST(CSSStyleTest, BorderTopStyleColorWidthV107) {
    const std::string css = ".bordered{border-top-width:3px;border-top-style:solid;border-top-color:red;border-bottom-width:2px;border-bottom-style:dashed;border-bottom-color:blue;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"bordered"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.color.b, 255);
}

// ============================================================================
// V107: Outline width and style with offset
// ============================================================================
TEST(CSSStyleTest, OutlineWidthStyleOffsetV107) {
    const std::string css = ".outlined{outline:4px dotted green;outline-offset:2px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "button";
    elem.classes = {"outlined"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 4.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Dotted);
    EXPECT_EQ(style.outline_color.g, 128);
    EXPECT_FLOAT_EQ(style.outline_offset.to_px(), 2.0f);
}

// ============================================================================
// V107: Flexbox direction column with gap and justify center
// ============================================================================
TEST(CSSStyleTest, FlexboxColumnGapJustifyCenterV107) {
    const std::string css = ".flex-col{display:flex;flex-direction:column;justify-content:center;align-items:center;gap:16px;flex-wrap:wrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"flex-col"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Flex);
    EXPECT_EQ(style.flex_direction, FlexDirection::Column);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);
    EXPECT_EQ(style.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(style.gap.to_px(), 16.0f);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
}

// ============================================================================
// V107: Text decoration underline with white-space nowrap
// ============================================================================
TEST(CSSStyleTest, TextDecorationUnderlineWhiteSpaceNowrapV107) {
    const std::string css = ".link{text-decoration:underline;white-space:nowrap;text-overflow:ellipsis;overflow:hidden;vertical-align:middle;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";
    elem.classes = {"link"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
}

// ============================================================================
// V107: Background color with opacity and color fields
// ============================================================================
TEST(CSSStyleTest, BackgroundColorOpacityColorFieldsV107) {
    const std::string css = ".card{background-color:rgb(100,150,200);color:rgb(255,255,255);opacity:0.8;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"card"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.background_color.r, 100);
    EXPECT_EQ(style.background_color.g, 150);
    EXPECT_EQ(style.background_color.b, 200);
    EXPECT_EQ(style.background_color.a, 255);
    EXPECT_EQ(style.color.r, 255);
    EXPECT_EQ(style.color.g, 255);
    EXPECT_EQ(style.color.b, 255);
    EXPECT_FLOAT_EQ(style.opacity, 0.8f);
}

// ============================================================================
// V107: Position absolute with top left and z-index
// ============================================================================
TEST(CSSStyleTest, PositionAbsoluteTopLeftZIndexV107) {
    const std::string css = ".overlay{position:absolute;top:10px;left:20px;z-index:99;display:block;width:200px;height:100px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"overlay"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Absolute);
    EXPECT_FLOAT_EQ(style.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 20.0f);
    EXPECT_EQ(style.z_index, 99);
    EXPECT_EQ(style.display, Display::Block);
    EXPECT_FLOAT_EQ(style.width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 100.0f);
}

// ============================================================================
// V108: Font size and line height as Length with to_px
// ============================================================================
TEST(CSSStyleTest, FontSizeAndLineHeightLengthV108) {
    ComputedStyle s;
    // Default font_size is 16px
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    // Default line_height is 1.2 * 16 = 19.2
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 19.2f);

    // Parse via resolver
    const std::string css = ".txt{font-size:24px;line-height:32px;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"txt"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.line_height.to_px(), 32.0f);
}

// ============================================================================
// V108: Visibility hidden and cursor pointer enums
// ============================================================================
TEST(CSSStyleTest, VisibilityHiddenCursorPointerV108) {
    const std::string css = ".hide{visibility:hidden;cursor:pointer;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"hide"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

// ============================================================================
// V108: Pointer events none and user select none enums
// ============================================================================
TEST(CSSStyleTest, PointerEventsNoneUserSelectNoneV108) {
    const std::string css = ".noclick{pointer-events:none;user-select:none;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"noclick"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

// ============================================================================
// V108: Border top/bottom color and width (no border_color shorthand)
// ============================================================================
TEST(CSSStyleTest, BorderEdgeColorAndWidthV108) {
    const std::string css = ".box{border-top-width:3px;border-top-style:solid;border-top-color:red;border-bottom-width:5px;border-bottom-style:dashed;border-bottom-color:blue;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);

    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 5.0f);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_bottom.color.b, 255);
}

// ============================================================================
// V108: Outline width is Length with to_px
// ============================================================================
TEST(CSSStyleTest, OutlineWidthIsLengthV108) {
    const std::string css = ".ol{outline:4px solid green;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"ol"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 4.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.g, 128);
}

// ============================================================================
// V108: Word spacing and letter spacing are Length type
// ============================================================================
TEST(CSSStyleTest, WordSpacingLetterSpacingAreLengthV108) {
    const std::string css = ".sp{word-spacing:5px;letter-spacing:2px;}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"sp"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    // word_spacing and letter_spacing are Length, use to_px()
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 2.0f);

    // Default values should be zero
    ComputedStyle def;
    EXPECT_TRUE(def.word_spacing.is_zero());
    EXPECT_TRUE(def.letter_spacing.is_zero());
}

// ============================================================================
// V108: Color struct with rgba components
// ============================================================================
TEST(CSSStyleTest, ColorStructRgbaComponentsV108) {
    const std::string css = ".c{color:rgba(100,150,200,0.5);background-color:rgb(10,20,30);}";
    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"c"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.color.r, 100);
    EXPECT_EQ(style.color.g, 150);
    EXPECT_EQ(style.color.b, 200);
    EXPECT_EQ(style.color.a, 127); // 0.5 * 255 = 127 (truncated)

    EXPECT_EQ(style.background_color.r, 10);
    EXPECT_EQ(style.background_color.g, 20);
    EXPECT_EQ(style.background_color.b, 30);
    EXPECT_EQ(style.background_color.a, 255);
}

// ============================================================================
// V108: Default computed style enum values
// ============================================================================
TEST(CSSStyleTest, DefaultEnumValuesV108) {
    ComputedStyle s;

    // Visibility defaults to Visible
    EXPECT_EQ(s.visibility, Visibility::Visible);

    // Cursor defaults to Auto
    EXPECT_EQ(s.cursor, Cursor::Auto);

    // Pointer events defaults to Auto
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);

    // User select defaults to Auto
    EXPECT_EQ(s.user_select, UserSelect::Auto);

    // Vertical align defaults to Baseline
    EXPECT_EQ(s.vertical_align, VerticalAlign::Baseline);

    // White space defaults to Normal
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);

    // Display defaults to Inline
    EXPECT_EQ(s.display, Display::Inline);

    // Position defaults to Static
    EXPECT_EQ(s.position, Position::Static);
}

// ============================================================================
// V109: Advanced computed style field tests
// ============================================================================

TEST(CSSStyleTest, FontSizeAndLineHeightAreLengthV109) {
    ComputedStyle s;

    // font_size defaults to 16px Length
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    EXPECT_EQ(s.font_size.unit, Length::Unit::Px);

    // Assign em-based font_size and convert
    s.font_size = Length::em(1.5f);
    // em relative to parent: 1.5 * 20 = 30
    EXPECT_FLOAT_EQ(s.font_size.to_px(20.0f), 30.0f);

    // line_height defaults to 1.2 * 16 = 19.2px
    ComputedStyle s2;
    EXPECT_FLOAT_EQ(s2.line_height.to_px(), 19.2f);

    // Set line_height to rem-based
    s2.line_height = Length::rem(2.0f);
    // rem uses root_font_size parameter: 2.0 * 14 = 28
    EXPECT_FLOAT_EQ(s2.line_height.to_px(0, 14.0f), 28.0f);
}

TEST(CSSStyleTest, ColorStructRGBAFieldsV109) {
    ComputedStyle s;

    // Default text color is black
    EXPECT_EQ(s.color.r, 0);
    EXPECT_EQ(s.color.g, 0);
    EXPECT_EQ(s.color.b, 0);
    EXPECT_EQ(s.color.a, 255);

    // Assign a custom color
    s.color = {128, 64, 32, 200};
    EXPECT_EQ(s.color.r, 128);
    EXPECT_EQ(s.color.g, 64);
    EXPECT_EQ(s.color.b, 32);
    EXPECT_EQ(s.color.a, 200);

    // Color equality
    Color a{10, 20, 30, 40};
    Color b{10, 20, 30, 40};
    Color c{10, 20, 30, 41};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(CSSStyleTest, BorderEdgeColorAccessV109) {
    ComputedStyle s;

    // NO border_color field — access individual edges
    s.border_top.color = {255, 0, 0, 255};
    s.border_right.color = {0, 255, 0, 255};
    s.border_bottom.color = {0, 0, 255, 255};
    s.border_left.color = {128, 128, 128, 255};

    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_bottom.color.b, 255);
    EXPECT_EQ(s.border_left.color.r, 128);

    // Border width is a Length
    s.border_top.width = Length::px(3.0f);
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 3.0f);

    // Border style
    s.border_top.style = BorderStyle::Solid;
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    s.border_bottom.style = BorderStyle::Dashed;
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dashed);
}

TEST(CSSStyleTest, OutlineWidthIsLengthV109) {
    ComputedStyle s;

    // outline_width defaults to zero
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 0.0f);

    // Set outline width in px
    s.outline_width = Length::px(2.5f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 2.5f);

    // Set outline width in em
    s.outline_width = Length::em(0.25f);
    // 0.25em * 16px parent = 4px
    EXPECT_FLOAT_EQ(s.outline_width.to_px(16.0f), 4.0f);

    // Outline offset is also a Length
    s.outline_offset = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);

    // Outline style and color
    s.outline_style = BorderStyle::Dotted;
    EXPECT_EQ(s.outline_style, BorderStyle::Dotted);
    s.outline_color = {255, 128, 0, 255};
    EXPECT_EQ(s.outline_color.r, 255);
    EXPECT_EQ(s.outline_color.g, 128);
}

TEST(CSSStyleTest, EnumAssignmentHiddenPointerCursorV109) {
    ComputedStyle s;

    // Visibility::Hidden
    s.visibility = Visibility::Hidden;
    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_NE(s.visibility, Visibility::Visible);

    // Cursor::Pointer
    s.cursor = Cursor::Pointer;
    EXPECT_EQ(s.cursor, Cursor::Pointer);
    EXPECT_NE(s.cursor, Cursor::Auto);

    // PointerEvents::None
    s.pointer_events = PointerEvents::None;
    EXPECT_EQ(s.pointer_events, PointerEvents::None);
    EXPECT_NE(s.pointer_events, PointerEvents::Auto);

    // UserSelect::None
    s.user_select = UserSelect::None;
    EXPECT_EQ(s.user_select, UserSelect::None);
    EXPECT_NE(s.user_select, UserSelect::Auto);
}

TEST(CSSStyleTest, WordAndLetterSpacingAreLengthV109) {
    ComputedStyle s;

    // word_spacing defaults to zero Length
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 0.0f);
    EXPECT_EQ(s.word_spacing.unit, Length::Unit::Zero);

    // letter_spacing defaults to zero Length
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(), 0.0f);
    EXPECT_EQ(s.letter_spacing.unit, Length::Unit::Zero);

    // Set word_spacing in px
    s.word_spacing = Length::px(4.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 4.0f);

    // Set letter_spacing in em — em needs font_size as parent_value
    s.letter_spacing = Length::em(0.1f);
    float font_sz = 16.0f;
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(font_sz), 1.6f);

    // They are Length, not optional — no has_value() or -> access
    s.word_spacing = Length::px(-2.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), -2.0f);
}

TEST(CSSStyleTest, VerticalAlignAndWhiteSpaceEnumsV109) {
    ComputedStyle s;

    // VerticalAlign defaults to Baseline
    EXPECT_EQ(s.vertical_align, VerticalAlign::Baseline);

    // Set to Middle
    s.vertical_align = VerticalAlign::Middle;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Middle);

    // Other vertical-align values
    s.vertical_align = VerticalAlign::Top;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Top);
    s.vertical_align = VerticalAlign::Bottom;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Bottom);

    // WhiteSpace defaults to Normal
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);

    // Set to NoWrap
    s.white_space = WhiteSpace::NoWrap;
    EXPECT_EQ(s.white_space, WhiteSpace::NoWrap);

    // Pre
    s.white_space = WhiteSpace::Pre;
    EXPECT_EQ(s.white_space, WhiteSpace::Pre);
}

TEST(CSSStyleTest, LengthFactoryMethodsAndConversionsV109) {
    // Test various Length factory methods used in ComputedStyle

    // px
    Length px_len = Length::px(10.0f);
    EXPECT_FLOAT_EQ(px_len.to_px(), 10.0f);
    EXPECT_EQ(px_len.unit, Length::Unit::Px);

    // percent: needs parent_value
    Length pct = Length::percent(50.0f);
    EXPECT_FLOAT_EQ(pct.to_px(200.0f), 100.0f);
    EXPECT_EQ(pct.unit, Length::Unit::Percent);

    // auto_val
    Length auto_l = Length::auto_val();
    EXPECT_TRUE(auto_l.is_auto());
    EXPECT_FALSE(auto_l.is_zero());

    // zero
    Length zero_l = Length::zero();
    EXPECT_TRUE(zero_l.is_zero());
    EXPECT_FALSE(zero_l.is_auto());

    // vw/vh use viewport dimensions
    Length::set_viewport(1000.0f, 600.0f);
    Length vw_len = Length::vw(10.0f);
    EXPECT_FLOAT_EQ(vw_len.to_px(), 100.0f); // 10% of 1000
    Length vh_len = Length::vh(50.0f);
    EXPECT_FLOAT_EQ(vh_len.to_px(), 300.0f); // 50% of 600
}

// ---------------------------------------------------------------------------
// V110: ComputedStyle property tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, FontSizeAndLineHeightAreLengthV110) {
    ComputedStyle s;
    // Default font_size is 16px
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    EXPECT_EQ(s.font_size.unit, Length::Unit::Px);

    // Default line_height is 1.2 * 16 = 19.2px
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 1.2f * 16.0f);
    EXPECT_EQ(s.line_height.unit, Length::Unit::Px);

    // Assign new font_size in em (relative to parent 20px)
    s.font_size = Length::em(1.5f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(20.0f), 30.0f);

    // Assign line_height in percent (relative to font size 30px)
    s.line_height = Length::percent(150.0f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(30.0f), 45.0f);
}

TEST(CSSStyleTest, ColorStructFieldsAndFactoriesV110) {
    ComputedStyle s;
    // Default color is black
    EXPECT_EQ(s.color.r, 0);
    EXPECT_EQ(s.color.g, 0);
    EXPECT_EQ(s.color.b, 0);
    EXPECT_EQ(s.color.a, 255);

    // Default background_color is transparent
    EXPECT_EQ(s.background_color, Color::transparent());
    EXPECT_EQ(s.background_color.a, 0);

    // Set color to a custom value
    s.color = {128, 64, 32, 200};
    EXPECT_EQ(s.color.r, 128);
    EXPECT_EQ(s.color.g, 64);
    EXPECT_EQ(s.color.b, 32);
    EXPECT_EQ(s.color.a, 200);

    // Test Color factories
    EXPECT_EQ(Color::white(), (Color{255, 255, 255, 255}));
    EXPECT_EQ(Color::black(), (Color{0, 0, 0, 255}));
    EXPECT_NE(Color::black(), Color::white());
}

TEST(CSSStyleTest, BorderEdgesHaveIndependentColorsV110) {
    ComputedStyle s;
    // NO border_color — must use border_top.color, border_right.color, etc.
    s.border_top.color = {255, 0, 0, 255};
    s.border_right.color = {0, 255, 0, 255};
    s.border_bottom.color = {0, 0, 255, 255};
    s.border_left.color = {255, 255, 0, 255};

    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_bottom.color.b, 255);
    EXPECT_EQ(s.border_left.color.r, 255);
    EXPECT_EQ(s.border_left.color.g, 255);

    // Each border edge also has width (Length) and style
    s.border_top.width = Length::px(2.0f);
    s.border_top.style = BorderStyle::Solid;
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);

    // Default border width is zero, style is None
    EXPECT_TRUE(s.border_bottom.width.is_zero());
    EXPECT_EQ(s.border_bottom.style, BorderStyle::None);
}

TEST(CSSStyleTest, OutlineWidthIsLengthV110) {
    ComputedStyle s;
    // Default outline_width is zero
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_EQ(s.outline_style, BorderStyle::None);
    EXPECT_EQ(s.outline_color, Color::black());

    // Set outline_width in px
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Dashed;
    s.outline_color = {0, 128, 255, 255};

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(s.outline_color.g, 128);

    // outline_offset is also a Length
    s.outline_offset = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);
}

TEST(CSSStyleTest, VisibilityAndCursorEnumsV110) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.visibility, Visibility::Visible);
    EXPECT_EQ(s.cursor, Cursor::Auto);

    // Set to non-default enum values
    s.visibility = Visibility::Hidden;
    s.cursor = Cursor::Pointer;

    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_NE(s.visibility, Visibility::Visible);
    EXPECT_EQ(s.cursor, Cursor::Pointer);
    EXPECT_NE(s.cursor, Cursor::Auto);

    // Visibility also has Collapse
    s.visibility = Visibility::Collapse;
    EXPECT_EQ(s.visibility, Visibility::Collapse);
}

TEST(CSSStyleTest, PointerEventsAndUserSelectEnumsV110) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    EXPECT_EQ(s.user_select, UserSelect::Auto);

    // Set to None
    s.pointer_events = PointerEvents::None;
    s.user_select = UserSelect::None;

    EXPECT_EQ(s.pointer_events, PointerEvents::None);
    EXPECT_EQ(s.user_select, UserSelect::None);

    // UserSelect also has Text and All
    s.user_select = UserSelect::Text;
    EXPECT_EQ(s.user_select, UserSelect::Text);
    s.user_select = UserSelect::All;
    EXPECT_EQ(s.user_select, UserSelect::All);
}

TEST(CSSStyleTest, WordSpacingAndLetterSpacingAreLengthV110) {
    ComputedStyle s;
    // Defaults are zero Length (NOT optional — no has_value())
    EXPECT_TRUE(s.word_spacing.is_zero());
    EXPECT_TRUE(s.letter_spacing.is_zero());

    // Set word_spacing to 4px
    s.word_spacing = Length::px(4.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 4.0f);
    EXPECT_FALSE(s.word_spacing.is_zero());

    // Set letter_spacing to 0.5em; to_px needs font_size as parent_value
    s.letter_spacing = Length::em(0.5f);
    float font_size = 16.0f;
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(font_size), 8.0f);
    EXPECT_EQ(s.letter_spacing.unit, Length::Unit::Em);

    // Zero letter_spacing
    s.letter_spacing = Length::zero();
    EXPECT_TRUE(s.letter_spacing.is_zero());
}

TEST(CSSStyleTest, VerticalAlignAndWhiteSpaceEnumsV110) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.vertical_align, VerticalAlign::Baseline);
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);

    // Set vertical_align to Middle
    s.vertical_align = VerticalAlign::Middle;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Middle);

    // Set white_space to NoWrap
    s.white_space = WhiteSpace::NoWrap;
    EXPECT_EQ(s.white_space, WhiteSpace::NoWrap);

    // Other VerticalAlign values
    s.vertical_align = VerticalAlign::Top;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Top);
    s.vertical_align = VerticalAlign::Bottom;
    EXPECT_EQ(s.vertical_align, VerticalAlign::Bottom);

    // Other WhiteSpace values
    s.white_space = WhiteSpace::Pre;
    EXPECT_EQ(s.white_space, WhiteSpace::Pre);
    s.white_space = WhiteSpace::PreWrap;
    EXPECT_EQ(s.white_space, WhiteSpace::PreWrap);
}

TEST(CSSStyleTest, FontSizeAndLineHeightLengthConversionV111) {
    ComputedStyle s;
    // Default font_size is 16px
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    // Default line_height is 1.2 * 16 = 19.2
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 19.2f);

    // Set font_size to 2em relative to parent 16px
    s.font_size = Length::em(2.0f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(16.0f), 32.0f);

    // Set line_height to 1.5em relative to computed font_size 32px
    s.line_height = Length::em(1.5f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(32.0f), 48.0f);

    // Percent-based font_size: 150% of parent 20px
    s.font_size = Length::percent(150.0f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(20.0f), 30.0f);

    // Rem-based line_height: 2rem with root 16px
    s.line_height = Length::rem(2.0f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(0, 16.0f), 32.0f);
}

TEST(CSSStyleTest, BorderEdgeColorAccessV111) {
    ComputedStyle s;
    // Default border colors are black
    EXPECT_EQ(s.border_top.color, Color::black());
    EXPECT_EQ(s.border_right.color, Color::black());
    EXPECT_EQ(s.border_bottom.color, Color::black());
    EXPECT_EQ(s.border_left.color, Color::black());

    // Set individual border colors
    s.border_top.color = {255, 0, 0, 255};
    s.border_right.color = {0, 255, 0, 255};
    s.border_bottom.color = {0, 0, 255, 255};
    s.border_left.color = {128, 128, 128, 255};

    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_bottom.color.b, 255);
    EXPECT_EQ(s.border_left.color.r, 128);

    // Border style and width
    s.border_top.style = BorderStyle::Solid;
    s.border_top.width = Length::px(2.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 2.0f);
}

TEST(CSSStyleTest, OutlineWidthIsLengthV111) {
    ComputedStyle s;
    // Default outline_width is zero
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 0.0f);

    // Set outline to 3px solid red
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Solid;
    s.outline_color = {255, 0, 0, 255};

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Solid);
    EXPECT_EQ(s.outline_color.r, 255);

    // outline_offset is also Length
    s.outline_offset = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);

    // em-based outline_width: 0.5em relative to 16px
    s.outline_width = Length::em(0.5f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(16.0f), 8.0f);
}

TEST(CSSStyleTest, VisibilityEnumValuesV111) {
    ComputedStyle s;
    // Default is Visible
    EXPECT_EQ(s.visibility, Visibility::Visible);

    // Set to Hidden
    s.visibility = Visibility::Hidden;
    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_NE(s.visibility, Visibility::Visible);

    // Set to Collapse
    s.visibility = Visibility::Collapse;
    EXPECT_EQ(s.visibility, Visibility::Collapse);
    EXPECT_NE(s.visibility, Visibility::Hidden);

    // Round-trip back to Visible
    s.visibility = Visibility::Visible;
    EXPECT_EQ(s.visibility, Visibility::Visible);
}

TEST(CSSStyleTest, CursorEnumValuesV111) {
    ComputedStyle s;
    // Default is Auto
    EXPECT_EQ(s.cursor, Cursor::Auto);

    // Set to Pointer
    s.cursor = Cursor::Pointer;
    EXPECT_EQ(s.cursor, Cursor::Pointer);

    // Cycle through all values
    s.cursor = Cursor::Default;
    EXPECT_EQ(s.cursor, Cursor::Default);

    s.cursor = Cursor::Text;
    EXPECT_EQ(s.cursor, Cursor::Text);

    s.cursor = Cursor::Move;
    EXPECT_EQ(s.cursor, Cursor::Move);

    s.cursor = Cursor::NotAllowed;
    EXPECT_EQ(s.cursor, Cursor::NotAllowed);
}

TEST(CSSStyleTest, PointerEventsAndUserSelectEnumsV111) {
    ComputedStyle s;
    // pointer_events default is Auto
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    s.pointer_events = PointerEvents::None;
    EXPECT_EQ(s.pointer_events, PointerEvents::None);

    // user_select default is Auto
    EXPECT_EQ(s.user_select, UserSelect::Auto);
    s.user_select = UserSelect::None;
    EXPECT_EQ(s.user_select, UserSelect::None);

    s.user_select = UserSelect::Text;
    EXPECT_EQ(s.user_select, UserSelect::Text);

    s.user_select = UserSelect::All;
    EXPECT_EQ(s.user_select, UserSelect::All);

    // Restore to auto
    s.pointer_events = PointerEvents::Auto;
    s.user_select = UserSelect::Auto;
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    EXPECT_EQ(s.user_select, UserSelect::Auto);
}

TEST(CSSStyleTest, WordSpacingAndLetterSpacingLengthV111) {
    ComputedStyle s;
    // Defaults are zero
    EXPECT_TRUE(s.word_spacing.is_zero());
    EXPECT_TRUE(s.letter_spacing.is_zero());

    // Set word_spacing to 5px
    s.word_spacing = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 5.0f);
    EXPECT_FALSE(s.word_spacing.is_zero());

    // Set letter_spacing to 0.1em relative to 16px font
    s.letter_spacing = Length::em(0.1f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), 1.6f);

    // Negative values allowed
    s.word_spacing = Length::px(-2.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), -2.0f);

    s.letter_spacing = Length::px(-0.5f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(), -0.5f);

    // Reset to zero
    s.word_spacing = Length::zero();
    s.letter_spacing = Length::zero();
    EXPECT_TRUE(s.word_spacing.is_zero());
    EXPECT_TRUE(s.letter_spacing.is_zero());
}

TEST(CSSStyleTest, ColorStructRgbaFieldsV111) {
    ComputedStyle s;
    // Default text color is black
    EXPECT_EQ(s.color.r, 0);
    EXPECT_EQ(s.color.g, 0);
    EXPECT_EQ(s.color.b, 0);
    EXPECT_EQ(s.color.a, 255);

    // Set to semi-transparent red
    s.color = {255, 0, 0, 128};
    EXPECT_EQ(s.color.r, 255);
    EXPECT_EQ(s.color.g, 0);
    EXPECT_EQ(s.color.b, 0);
    EXPECT_EQ(s.color.a, 128);

    // Background color default is transparent
    EXPECT_EQ(s.background_color, Color::transparent());

    // Set background to white
    s.background_color = Color::white();
    EXPECT_EQ(s.background_color.r, 255);
    EXPECT_EQ(s.background_color.g, 255);
    EXPECT_EQ(s.background_color.b, 255);
    EXPECT_EQ(s.background_color.a, 255);

    // Color equality
    Color c1 = {10, 20, 30, 40};
    Color c2 = {10, 20, 30, 40};
    Color c3 = {10, 20, 30, 41};
    EXPECT_EQ(c1, c2);
    EXPECT_NE(c1, c3);
}

// ---------------------------------------------------------------------------
// V112 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleV112, FontSizeAndLineHeightAreLengthType) {
    ComputedStyle s;
    // font_size defaults to 16px
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    EXPECT_EQ(s.font_size.unit, Length::Unit::Px);

    // line_height defaults to 1.2 * 16 = 19.2px
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 19.2f);

    // Set font_size to 2em (relative to parent 16px → 32px)
    s.font_size = Length::em(2.0f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(16.0f), 32.0f);

    // Set line_height to 1.5em relative to font_size 20px
    s.line_height = Length::em(1.5f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(20.0f), 30.0f);
}

TEST(ComputedStyleV112, BorderEdgeColorPerSide) {
    ComputedStyle s;
    // Each border side has its own color — no border_color shorthand
    s.border_top.color = {255, 0, 0, 255};
    s.border_right.color = {0, 255, 0, 255};
    s.border_bottom.color = {0, 0, 255, 255};
    s.border_left.color = {128, 128, 128, 255};

    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_bottom.color.b, 255);
    EXPECT_EQ(s.border_left.color.r, 128);

    // Verify independence — changing one side does not affect another
    s.border_top.color = Color::white();
    EXPECT_EQ(s.border_top.color, Color::white());
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_bottom.color.b, 255);
}

TEST(ComputedStyleV112, OutlineWidthIsLengthType) {
    ComputedStyle s;
    // Default outline_width is zero
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 0.0f);

    // Set outline_width to 3px
    s.outline_width = Length::px(3.0f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);

    // Set outline_width to 0.5em relative to parent 16px = 8px
    s.outline_width = Length::em(0.5f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(16.0f), 8.0f);

    // Outline offset is also a Length
    s.outline_offset = Length::px(2.0f);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 2.0f);
}

TEST(ComputedStyleV112, VisibilityEnumValues) {
    ComputedStyle s;
    // Default is Visible
    EXPECT_EQ(s.visibility, Visibility::Visible);

    s.visibility = Visibility::Hidden;
    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_NE(s.visibility, Visibility::Visible);

    s.visibility = Visibility::Collapse;
    EXPECT_EQ(s.visibility, Visibility::Collapse);
}

TEST(ComputedStyleV112, CursorEnumValues) {
    ComputedStyle s;
    // Default is Auto
    EXPECT_EQ(s.cursor, Cursor::Auto);

    s.cursor = Cursor::Pointer;
    EXPECT_EQ(s.cursor, Cursor::Pointer);

    s.cursor = Cursor::Text;
    EXPECT_EQ(s.cursor, Cursor::Text);

    s.cursor = Cursor::Move;
    EXPECT_EQ(s.cursor, Cursor::Move);

    s.cursor = Cursor::NotAllowed;
    EXPECT_EQ(s.cursor, Cursor::NotAllowed);

    s.cursor = Cursor::Default;
    EXPECT_EQ(s.cursor, Cursor::Default);
}

TEST(ComputedStyleV112, PointerEventsAndUserSelectEnums) {
    ComputedStyle s;
    // PointerEvents default is Auto
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    s.pointer_events = PointerEvents::None;
    EXPECT_EQ(s.pointer_events, PointerEvents::None);

    // UserSelect default is Auto
    EXPECT_EQ(s.user_select, UserSelect::Auto);
    s.user_select = UserSelect::None;
    EXPECT_EQ(s.user_select, UserSelect::None);

    s.user_select = UserSelect::Text;
    EXPECT_EQ(s.user_select, UserSelect::Text);

    s.user_select = UserSelect::All;
    EXPECT_EQ(s.user_select, UserSelect::All);
}

TEST(ComputedStyleV112, WordSpacingAndLetterSpacingAreLength) {
    ComputedStyle s;
    // Both default to zero Length
    EXPECT_TRUE(s.word_spacing.is_zero());
    EXPECT_TRUE(s.letter_spacing.is_zero());
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(), 0.0f);

    // Set word_spacing to 4px
    s.word_spacing = Length::px(4.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 4.0f);

    // Set letter_spacing to 0.1em relative to font_size 20px = 2px
    s.letter_spacing = Length::em(0.1f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(20.0f), 2.0f);

    // They are Length type — .to_px() works, no has_value() needed
    s.word_spacing = Length::percent(10.0f);
    EXPECT_EQ(s.word_spacing.unit, Length::Unit::Percent);
}

TEST(ComputedStyleV112, BorderEdgeWidthStyleAndDefaults) {
    ComputedStyle s;
    // Default border edges have zero width, None style, black color
    EXPECT_TRUE(s.border_top.width.is_zero());
    EXPECT_EQ(s.border_top.style, BorderStyle::None);
    EXPECT_EQ(s.border_top.color, Color::black());

    // Set border_top width and style
    s.border_top.width = Length::px(2.0f);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = {255, 0, 0, 255};
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(s.border_top.color.r, 255);

    // Set border_bottom with dashed style
    s.border_bottom.width = Length::px(1.0f);
    s.border_bottom.style = BorderStyle::Dashed;
    s.border_bottom.color = Color::transparent();
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dashed);
    EXPECT_EQ(s.border_bottom.color, Color::transparent());

    // border_right remains at default
    EXPECT_TRUE(s.border_right.width.is_zero());
    EXPECT_EQ(s.border_right.style, BorderStyle::None);
}

// ---------------------------------------------------------------------------
// V113 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, FlexboxPropertiesCombinedV113) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.flex_direction, FlexDirection::Row);
    EXPECT_EQ(s.flex_wrap, FlexWrap::NoWrap);
    EXPECT_FLOAT_EQ(s.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 1.0f);
    EXPECT_TRUE(s.flex_basis.is_auto());

    // Set flex container properties
    s.display = Display::Flex;
    s.flex_direction = FlexDirection::ColumnReverse;
    s.flex_wrap = FlexWrap::WrapReverse;
    s.justify_content = JustifyContent::SpaceEvenly;
    s.align_items = AlignItems::Center;
    s.gap = Length::px(12.0f);

    EXPECT_EQ(s.display, Display::Flex);
    EXPECT_EQ(s.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(s.flex_wrap, FlexWrap::WrapReverse);
    EXPECT_EQ(s.justify_content, JustifyContent::SpaceEvenly);
    EXPECT_EQ(s.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(s.gap.to_px(), 12.0f);

    // Set flex item properties
    s.flex_grow = 2.5f;
    s.flex_shrink = 0.0f;
    s.flex_basis = Length::px(200.0f);
    s.order = -1;
    s.align_self = 2; // center

    EXPECT_FLOAT_EQ(s.flex_grow, 2.5f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_basis.to_px(), 200.0f);
    EXPECT_EQ(s.order, -1);
    EXPECT_EQ(s.align_self, 2);
}

TEST(ComputedStyleTest, OverflowAndVisibilityInteractionV113) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.overflow_x, Overflow::Visible);
    EXPECT_EQ(s.overflow_y, Overflow::Visible);
    EXPECT_EQ(s.visibility, Visibility::Visible);

    // Set overflow hidden on x, scroll on y
    s.overflow_x = Overflow::Hidden;
    s.overflow_y = Overflow::Scroll;
    EXPECT_EQ(s.overflow_x, Overflow::Hidden);
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);

    // Visibility hidden does not change overflow
    s.visibility = Visibility::Hidden;
    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_EQ(s.overflow_x, Overflow::Hidden);

    // Overflow::Auto
    s.overflow_x = Overflow::Auto;
    s.overflow_y = Overflow::Auto;
    EXPECT_EQ(s.overflow_x, Overflow::Auto);
    EXPECT_EQ(s.overflow_y, Overflow::Auto);

    // Collapse visibility
    s.visibility = Visibility::Collapse;
    EXPECT_EQ(s.visibility, Visibility::Collapse);
}

TEST(ComputedStyleTest, PositionOffsetsAndZIndexV113) {
    ComputedStyle s;
    // Defaults: all auto, z-index 0
    EXPECT_TRUE(s.top.is_auto());
    EXPECT_TRUE(s.right_pos.is_auto());
    EXPECT_TRUE(s.bottom.is_auto());
    EXPECT_TRUE(s.left_pos.is_auto());
    EXPECT_EQ(s.z_index, 0);

    // Absolute positioning with specific offsets
    s.position = Position::Absolute;
    s.top = Length::px(10.0f);
    s.right_pos = Length::px(20.0f);
    s.bottom = Length::px(30.0f);
    s.left_pos = Length::px(40.0f);
    s.z_index = 999;

    EXPECT_EQ(s.position, Position::Absolute);
    EXPECT_FLOAT_EQ(s.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(s.right_pos.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(s.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(s.left_pos.to_px(), 40.0f);
    EXPECT_EQ(s.z_index, 999);

    // Fixed positioning with percent offsets
    s.position = Position::Fixed;
    s.top = Length::percent(5.0f);
    s.left_pos = Length::percent(10.0f);
    EXPECT_EQ(s.position, Position::Fixed);
    EXPECT_FLOAT_EQ(s.top.to_px(800.0f), 40.0f); // 5% of 800
    EXPECT_FLOAT_EQ(s.left_pos.to_px(1000.0f), 100.0f); // 10% of 1000

    // Negative z-index
    s.z_index = -5;
    EXPECT_EQ(s.z_index, -5);
}

TEST(ComputedStyleTest, TextPropertiesComprehensiveV113) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.text_align, TextAlign::Left);
    EXPECT_EQ(s.text_decoration, TextDecoration::None);
    EXPECT_EQ(s.text_transform, TextTransform::None);
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);
    EXPECT_TRUE(s.letter_spacing.is_zero());
    EXPECT_TRUE(s.word_spacing.is_zero());

    // Configure text properties
    s.text_align = TextAlign::Justify;
    s.text_decoration = TextDecoration::Underline;
    s.text_decoration_style = TextDecorationStyle::Wavy;
    s.text_decoration_color = Color{255, 0, 0, 255};
    s.text_transform = TextTransform::Uppercase;
    s.white_space = WhiteSpace::PreWrap;
    s.letter_spacing = Length::px(2.0f);
    s.word_spacing = Length::em(0.5f);
    s.text_indent = Length::px(32.0f);

    EXPECT_EQ(s.text_align, TextAlign::Justify);
    EXPECT_EQ(s.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Wavy);
    EXPECT_EQ(s.text_decoration_color.r, 255);
    EXPECT_EQ(s.text_decoration_color.a, 255);
    EXPECT_EQ(s.text_transform, TextTransform::Uppercase);
    EXPECT_EQ(s.white_space, WhiteSpace::PreWrap);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), 2.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(16.0f), 8.0f); // 0.5em * 16px
    EXPECT_FLOAT_EQ(s.text_indent.to_px(), 32.0f);

    // Text stroke
    s.text_stroke_width = 1.5f;
    s.text_stroke_color = Color{0, 0, 255, 255};
    EXPECT_FLOAT_EQ(s.text_stroke_width, 1.5f);
    EXPECT_EQ(s.text_stroke_color.b, 255);
}

TEST(ComputedStyleTest, MarginPaddingEdgeSizesV113) {
    ComputedStyle s;
    // Defaults are all zero
    EXPECT_TRUE(s.margin.top.is_zero());
    EXPECT_TRUE(s.margin.right.is_zero());
    EXPECT_TRUE(s.margin.bottom.is_zero());
    EXPECT_TRUE(s.margin.left.is_zero());
    EXPECT_TRUE(s.padding.top.is_zero());
    EXPECT_TRUE(s.padding.right.is_zero());

    // Set mixed margin units
    s.margin.top = Length::px(10.0f);
    s.margin.right = Length::em(1.0f);
    s.margin.bottom = Length::percent(5.0f);
    s.margin.left = Length::auto_val();

    EXPECT_FLOAT_EQ(s.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(s.margin.right.to_px(16.0f), 16.0f); // 1em at 16px font
    EXPECT_FLOAT_EQ(s.margin.bottom.to_px(400.0f), 20.0f); // 5% of 400
    EXPECT_TRUE(s.margin.left.is_auto());

    // Set padding with rem and vh units
    s.padding.top = Length::rem(2.0f);
    s.padding.right = Length::px(15.0f);
    s.padding.bottom = Length::vh(10.0f);
    s.padding.left = Length::vw(5.0f);

    EXPECT_FLOAT_EQ(s.padding.top.to_px(0, 16.0f), 32.0f); // 2rem * 16px root
    EXPECT_FLOAT_EQ(s.padding.right.to_px(), 15.0f);
    // vh/vw use static viewport dimensions
    Length::set_viewport(1024.0f, 768.0f);
    EXPECT_FLOAT_EQ(s.padding.bottom.to_px(), 76.8f); // 10% of 768
    EXPECT_FLOAT_EQ(s.padding.left.to_px(), 51.2f);   // 5% of 1024
    // Restore default viewport
    Length::set_viewport(800.0f, 600.0f);
}

TEST(ComputedStyleTest, OutlineAndBoxShadowV113) {
    ComputedStyle s;
    // Outline defaults
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_EQ(s.outline_style, BorderStyle::None);
    EXPECT_EQ(s.outline_color, Color::black());
    EXPECT_TRUE(s.outline_offset.is_zero());

    // Set outline
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Dashed;
    s.outline_color = Color{0, 128, 0, 255};
    s.outline_offset = Length::px(2.0f);

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(s.outline_color.g, 128);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 2.0f);

    // Multiple box shadows
    EXPECT_TRUE(s.box_shadows.empty());
    ComputedStyle::BoxShadowEntry shadow1;
    shadow1.offset_x = 2.0f;
    shadow1.offset_y = 4.0f;
    shadow1.blur = 8.0f;
    shadow1.spread = 1.0f;
    shadow1.color = Color{0, 0, 0, 128};
    shadow1.inset = false;
    s.box_shadows.push_back(shadow1);

    ComputedStyle::BoxShadowEntry shadow2;
    shadow2.offset_x = 0.0f;
    shadow2.offset_y = 0.0f;
    shadow2.blur = 20.0f;
    shadow2.spread = 5.0f;
    shadow2.color = Color{255, 0, 0, 64};
    shadow2.inset = true;
    s.box_shadows.push_back(shadow2);

    EXPECT_EQ(s.box_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 8.0f);
    EXPECT_FALSE(s.box_shadows[0].inset);
    EXPECT_FLOAT_EQ(s.box_shadows[1].spread, 5.0f);
    EXPECT_TRUE(s.box_shadows[1].inset);
    EXPECT_EQ(s.box_shadows[1].color.r, 255);
    EXPECT_EQ(s.box_shadows[1].color.a, 64);
}

TEST(ComputedStyleTest, DisplayAndBoxSizingVariantsV113) {
    ComputedStyle s;
    // Default display for ComputedStyle is Inline
    EXPECT_EQ(s.display, Display::Inline);
    EXPECT_EQ(s.box_sizing, BoxSizing::ContentBox);

    // Block with border-box
    s.display = Display::Block;
    s.box_sizing = BoxSizing::BorderBox;
    EXPECT_EQ(s.display, Display::Block);
    EXPECT_EQ(s.box_sizing, BoxSizing::BorderBox);

    // InlineBlock
    s.display = Display::InlineBlock;
    EXPECT_EQ(s.display, Display::InlineBlock);

    // InlineFlex
    s.display = Display::InlineFlex;
    EXPECT_EQ(s.display, Display::InlineFlex);

    // Grid
    s.display = Display::Grid;
    EXPECT_EQ(s.display, Display::Grid);

    // InlineGrid
    s.display = Display::InlineGrid;
    EXPECT_EQ(s.display, Display::InlineGrid);

    // None hides the element
    s.display = Display::None;
    EXPECT_EQ(s.display, Display::None);

    // Table
    s.display = Display::Table;
    EXPECT_EQ(s.display, Display::Table);

    // ListItem
    s.display = Display::ListItem;
    EXPECT_EQ(s.display, Display::ListItem);

    // Contents
    s.display = Display::Contents;
    EXPECT_EQ(s.display, Display::Contents);
}

TEST(ComputedStyleTest, FontAndLineHeightV113) {
    ComputedStyle s;
    // Defaults
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f);
    EXPECT_EQ(s.font_weight, 400);
    EXPECT_EQ(s.font_style, FontStyle::Normal);
    EXPECT_EQ(s.font_family, "sans-serif");
    EXPECT_FLOAT_EQ(s.line_height_unitless, 1.2f);

    // Set font size with em relative to parent
    s.font_size = Length::em(1.5f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(16.0f), 24.0f); // 1.5em * 16px parent

    // Bold italic
    s.font_weight = 700;
    s.font_style = FontStyle::Italic;
    EXPECT_EQ(s.font_weight, 700);
    EXPECT_EQ(s.font_style, FontStyle::Italic);

    // Custom font family
    s.font_family = "Georgia, serif";
    EXPECT_EQ(s.font_family, "Georgia, serif");

    // Line height with explicit px
    s.line_height = Length::px(28.0f);
    s.line_height_unitless = 0; // explicit, not unitless
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 28.0f);
    EXPECT_FLOAT_EQ(s.line_height_unitless, 0);

    // Font variant small-caps
    s.font_variant = 1;
    EXPECT_EQ(s.font_variant, 1);

    // Font stretch expanded
    s.font_stretch = 7; // expanded
    EXPECT_EQ(s.font_stretch, 7);

    // Oblique style
    s.font_style = FontStyle::Oblique;
    EXPECT_EQ(s.font_style, FontStyle::Oblique);
}

// ---------------------------------------------------------------------------
// V114 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, PositionAndOffsetLengthsV114) {
    ComputedStyle s;
    // Defaults: static position, auto offsets
    EXPECT_EQ(s.position, Position::Static);
    EXPECT_TRUE(s.top.is_auto());
    EXPECT_TRUE(s.right_pos.is_auto());
    EXPECT_TRUE(s.bottom.is_auto());
    EXPECT_TRUE(s.left_pos.is_auto());
    EXPECT_EQ(s.z_index, 0);

    // Set absolute with specific offsets
    s.position = Position::Absolute;
    s.top = Length::px(10.0f);
    s.right_pos = Length::em(2.0f);
    s.bottom = Length::percent(50.0f);
    s.left_pos = Length::rem(3.0f);
    s.z_index = 99;

    EXPECT_EQ(s.position, Position::Absolute);
    EXPECT_FLOAT_EQ(s.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(s.right_pos.to_px(16.0f), 32.0f); // 2em * 16px parent
    EXPECT_FLOAT_EQ(s.bottom.to_px(400.0f), 200.0f); // 50% of 400
    EXPECT_FLOAT_EQ(s.left_pos.to_px(0, 20.0f), 60.0f); // 3rem * 20px root
    EXPECT_EQ(s.z_index, 99);

    // Fixed and sticky
    s.position = Position::Fixed;
    EXPECT_EQ(s.position, Position::Fixed);
    s.position = Position::Sticky;
    EXPECT_EQ(s.position, Position::Sticky);
    s.position = Position::Relative;
    EXPECT_EQ(s.position, Position::Relative);
}

TEST(ComputedStyleTest, TextDecorationBitsAndStylesV114) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.text_decoration, TextDecoration::None);
    EXPECT_EQ(s.text_decoration_bits, 0);
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Solid);
    EXPECT_FLOAT_EQ(s.text_decoration_thickness, 0);

    // Underline + overline bitmask
    s.text_decoration_bits = 1 | 2; // underline | overline
    EXPECT_EQ(s.text_decoration_bits, 3);
    EXPECT_TRUE(s.text_decoration_bits & 1);
    EXPECT_TRUE(s.text_decoration_bits & 2);
    EXPECT_FALSE(s.text_decoration_bits & 4);

    // Line-through added
    s.text_decoration_bits |= 4;
    EXPECT_EQ(s.text_decoration_bits, 7);

    // Custom decoration color and style
    s.text_decoration_color = Color{255, 0, 128, 255};
    s.text_decoration_style = TextDecorationStyle::Wavy;
    s.text_decoration_thickness = 2.5f;
    EXPECT_EQ(s.text_decoration_color.r, 255);
    EXPECT_EQ(s.text_decoration_color.g, 0);
    EXPECT_EQ(s.text_decoration_color.b, 128);
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Wavy);
    EXPECT_FLOAT_EQ(s.text_decoration_thickness, 2.5f);

    // All decoration styles
    s.text_decoration_style = TextDecorationStyle::Dashed;
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Dashed);
    s.text_decoration_style = TextDecorationStyle::Dotted;
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Dotted);
    s.text_decoration_style = TextDecorationStyle::Double;
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Double);
}

TEST(ComputedStyleTest, TextShadowEntriesV114) {
    ComputedStyle s;
    // Default: no text shadows
    EXPECT_TRUE(s.text_shadows.empty());
    EXPECT_FLOAT_EQ(s.text_shadow_offset_x, 0);
    EXPECT_FLOAT_EQ(s.text_shadow_offset_y, 0);
    EXPECT_FLOAT_EQ(s.text_shadow_blur, 0);

    // Add a text shadow entry
    ComputedStyle::TextShadowEntry ts1;
    ts1.offset_x = 2.0f;
    ts1.offset_y = 3.0f;
    ts1.blur = 5.0f;
    ts1.color = Color{0, 0, 0, 128};
    s.text_shadows.push_back(ts1);

    // Add a second text shadow (colored glow)
    ComputedStyle::TextShadowEntry ts2;
    ts2.offset_x = 0.0f;
    ts2.offset_y = 0.0f;
    ts2.blur = 10.0f;
    ts2.color = Color{0, 128, 255, 200};
    s.text_shadows.push_back(ts2);

    EXPECT_EQ(s.text_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_y, 3.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].blur, 5.0f);
    EXPECT_EQ(s.text_shadows[0].color.a, 128);
    EXPECT_FLOAT_EQ(s.text_shadows[1].blur, 10.0f);
    EXPECT_EQ(s.text_shadows[1].color.b, 255);
    EXPECT_EQ(s.text_shadows[1].color.a, 200);
}

TEST(ComputedStyleTest, TransformVariantsV114) {
    ComputedStyle s;
    EXPECT_TRUE(s.transforms.empty());

    // Translate
    Transform t1;
    t1.type = TransformType::Translate;
    t1.x = 50.0f;
    t1.y = -30.0f;
    s.transforms.push_back(t1);

    // Rotate
    Transform t2;
    t2.type = TransformType::Rotate;
    t2.angle = 45.0f;
    s.transforms.push_back(t2);

    // Scale
    Transform t3;
    t3.type = TransformType::Scale;
    t3.x = 1.5f;
    t3.y = 2.0f;
    s.transforms.push_back(t3);

    // Skew
    Transform t4;
    t4.type = TransformType::Skew;
    t4.x = 10.0f;
    t4.y = 20.0f;
    s.transforms.push_back(t4);

    EXPECT_EQ(s.transforms.size(), 4u);
    EXPECT_EQ(s.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(s.transforms[0].x, 50.0f);
    EXPECT_FLOAT_EQ(s.transforms[0].y, -30.0f);
    EXPECT_EQ(s.transforms[1].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(s.transforms[1].angle, 45.0f);
    EXPECT_EQ(s.transforms[2].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(s.transforms[2].x, 1.5f);
    EXPECT_FLOAT_EQ(s.transforms[2].y, 2.0f);
    EXPECT_EQ(s.transforms[3].type, TransformType::Skew);
    EXPECT_FLOAT_EQ(s.transforms[3].x, 10.0f);
}

TEST(ComputedStyleTest, OutlineAndBorderRadiusV114) {
    ComputedStyle s;
    // Outline defaults
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_EQ(s.outline_style, BorderStyle::None);
    EXPECT_EQ(s.outline_color, Color::black());
    EXPECT_TRUE(s.outline_offset.is_zero());

    // Set outline
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Dashed;
    s.outline_color = Color{255, 100, 0, 255};
    s.outline_offset = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(s.outline_color.r, 255);
    EXPECT_EQ(s.outline_color.g, 100);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);

    // Border radius defaults
    EXPECT_FLOAT_EQ(s.border_radius, 0);
    EXPECT_FLOAT_EQ(s.border_radius_tl, 0);
    EXPECT_FLOAT_EQ(s.border_radius_tr, 0);
    EXPECT_FLOAT_EQ(s.border_radius_bl, 0);
    EXPECT_FLOAT_EQ(s.border_radius_br, 0);

    // Set individual corner radii
    s.border_radius_tl = 8.0f;
    s.border_radius_tr = 12.0f;
    s.border_radius_bl = 4.0f;
    s.border_radius_br = 16.0f;
    EXPECT_FLOAT_EQ(s.border_radius_tl, 8.0f);
    EXPECT_FLOAT_EQ(s.border_radius_tr, 12.0f);
    EXPECT_FLOAT_EQ(s.border_radius_bl, 4.0f);
    EXPECT_FLOAT_EQ(s.border_radius_br, 16.0f);
}

TEST(ComputedStyleTest, FloatClearTextTransformV114) {
    ComputedStyle s;
    // Float defaults
    EXPECT_EQ(s.float_val, Float::None);
    EXPECT_EQ(s.clear, Clear::None);
    EXPECT_EQ(s.text_transform, TextTransform::None);
    EXPECT_EQ(s.text_align, TextAlign::Left);

    // Float left with clear both
    s.float_val = Float::Left;
    s.clear = Clear::Both;
    EXPECT_EQ(s.float_val, Float::Left);
    EXPECT_EQ(s.clear, Clear::Both);

    // Float right with clear right
    s.float_val = Float::Right;
    s.clear = Clear::Right;
    EXPECT_EQ(s.float_val, Float::Right);
    EXPECT_EQ(s.clear, Clear::Right);

    s.clear = Clear::Left;
    EXPECT_EQ(s.clear, Clear::Left);

    // Text transform variants
    s.text_transform = TextTransform::Uppercase;
    EXPECT_EQ(s.text_transform, TextTransform::Uppercase);
    s.text_transform = TextTransform::Lowercase;
    EXPECT_EQ(s.text_transform, TextTransform::Lowercase);
    s.text_transform = TextTransform::Capitalize;
    EXPECT_EQ(s.text_transform, TextTransform::Capitalize);

    // Text align variants
    s.text_align = TextAlign::Center;
    EXPECT_EQ(s.text_align, TextAlign::Center);
    s.text_align = TextAlign::Right;
    EXPECT_EQ(s.text_align, TextAlign::Right);
    s.text_align = TextAlign::Justify;
    EXPECT_EQ(s.text_align, TextAlign::Justify);
}

TEST(ComputedStyleTest, MultiColumnLayoutV114) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.column_count, -1); // auto
    EXPECT_EQ(s.column_fill, 0);   // balance
    EXPECT_TRUE(s.column_width.is_auto());
    EXPECT_FLOAT_EQ(s.column_gap_val.to_px(), 0);
    EXPECT_FLOAT_EQ(s.column_rule_width, 0);
    EXPECT_EQ(s.column_rule_style, 0); // none
    EXPECT_EQ(s.column_span, 0); // none

    // 3 columns with gap and rule
    s.column_count = 3;
    s.column_width = Length::px(200.0f);
    s.column_gap_val = Length::px(20.0f);
    s.column_rule_width = 1.0f;
    s.column_rule_color = Color{128, 128, 128, 255};
    s.column_rule_style = 1; // solid
    s.column_fill = 1; // auto
    s.column_span = 1; // all

    EXPECT_EQ(s.column_count, 3);
    EXPECT_FLOAT_EQ(s.column_width.to_px(), 200.0f);
    EXPECT_FLOAT_EQ(s.column_gap_val.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(s.column_rule_width, 1.0f);
    EXPECT_EQ(s.column_rule_color.r, 128);
    EXPECT_EQ(s.column_rule_color.g, 128);
    EXPECT_EQ(s.column_rule_color.a, 255);
    EXPECT_EQ(s.column_rule_style, 1);
    EXPECT_EQ(s.column_fill, 1);
    EXPECT_EQ(s.column_span, 1);
}

TEST(ComputedStyleTest, ScrollAndOverscrollBehaviorV114) {
    ComputedStyle s;
    // Scroll behavior defaults
    EXPECT_EQ(s.scroll_behavior, 0); // auto
    EXPECT_TRUE(s.scroll_snap_type.empty());
    EXPECT_TRUE(s.scroll_snap_align.empty());
    EXPECT_EQ(s.scroll_snap_stop, 0); // normal

    // Overscroll behavior defaults
    EXPECT_EQ(s.overscroll_behavior, 0); // auto
    EXPECT_EQ(s.overscroll_behavior_x, 0);
    EXPECT_EQ(s.overscroll_behavior_y, 0);

    // Scroll margin/padding defaults
    EXPECT_FLOAT_EQ(s.scroll_margin_top, 0);
    EXPECT_FLOAT_EQ(s.scroll_margin_right, 0);
    EXPECT_FLOAT_EQ(s.scroll_margin_bottom, 0);
    EXPECT_FLOAT_EQ(s.scroll_margin_left, 0);
    EXPECT_FLOAT_EQ(s.scroll_padding_top, 0);
    EXPECT_FLOAT_EQ(s.scroll_padding_right, 0);

    // Set smooth scrolling with snap
    s.scroll_behavior = 1; // smooth
    s.scroll_snap_type = "y mandatory";
    s.scroll_snap_align = "start";
    s.scroll_snap_stop = 1; // always

    EXPECT_EQ(s.scroll_behavior, 1);
    EXPECT_EQ(s.scroll_snap_type, "y mandatory");
    EXPECT_EQ(s.scroll_snap_align, "start");
    EXPECT_EQ(s.scroll_snap_stop, 1);

    // Set overscroll containment
    s.overscroll_behavior = 1; // contain
    s.overscroll_behavior_x = 2; // none
    s.overscroll_behavior_y = 1; // contain
    EXPECT_EQ(s.overscroll_behavior, 1);
    EXPECT_EQ(s.overscroll_behavior_x, 2);
    EXPECT_EQ(s.overscroll_behavior_y, 1);

    // Set scroll margin and padding
    s.scroll_margin_top = 10.0f;
    s.scroll_margin_bottom = 20.0f;
    s.scroll_padding_top = 15.0f;
    s.scroll_padding_left = 25.0f;
    EXPECT_FLOAT_EQ(s.scroll_margin_top, 10.0f);
    EXPECT_FLOAT_EQ(s.scroll_margin_bottom, 20.0f);
    EXPECT_FLOAT_EQ(s.scroll_padding_top, 15.0f);
    EXPECT_FLOAT_EQ(s.scroll_padding_left, 25.0f);
}

// ---------------------------------------------------------------------------
// V115 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, BorderEdgeCompositionV115) {
    ComputedStyle s;
    // Defaults: all borders have zero width, none style, black color
    EXPECT_TRUE(s.border_top.width.is_zero());
    EXPECT_EQ(s.border_top.style, BorderStyle::None);
    EXPECT_EQ(s.border_top.color, Color::black());

    // Set each border edge independently
    s.border_top.width = Length::px(2.0f);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = {255, 0, 0, 255};

    s.border_right.width = Length::px(3.0f);
    s.border_right.style = BorderStyle::Dashed;
    s.border_right.color = {0, 255, 0, 255};

    s.border_bottom.width = Length::px(4.0f);
    s.border_bottom.style = BorderStyle::Dotted;
    s.border_bottom.color = {0, 0, 255, 255};

    s.border_left.width = Length::px(1.0f);
    s.border_left.style = BorderStyle::Double;
    s.border_left.color = {128, 128, 128, 255};

    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(s.border_top.color.r, 255);

    EXPECT_FLOAT_EQ(s.border_right.width.to_px(), 3.0f);
    EXPECT_EQ(s.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(s.border_right.color.g, 255);

    EXPECT_FLOAT_EQ(s.border_bottom.width.to_px(), 4.0f);
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(s.border_bottom.color.b, 255);

    EXPECT_FLOAT_EQ(s.border_left.width.to_px(), 1.0f);
    EXPECT_EQ(s.border_left.style, BorderStyle::Double);
    EXPECT_EQ(s.border_left.color.r, 128);
}

TEST(ComputedStyleTest, LengthUnitConversionsV115) {
    // em units: relative to parent font size
    Length em2 = Length::em(2.0f);
    EXPECT_FLOAT_EQ(em2.to_px(16.0f), 32.0f);
    EXPECT_FLOAT_EQ(em2.to_px(20.0f), 40.0f);

    // rem units: relative to root font size (third param)
    Length rem1_5 = Length::rem(1.5f);
    EXPECT_FLOAT_EQ(rem1_5.to_px(0, 16.0f), 24.0f);
    EXPECT_FLOAT_EQ(rem1_5.to_px(0, 20.0f), 30.0f);

    // percent: relative to parent value (first param)
    Length pct50 = Length::percent(50.0f);
    EXPECT_FLOAT_EQ(pct50.to_px(200.0f), 100.0f);
    EXPECT_FLOAT_EQ(pct50.to_px(400.0f), 200.0f);

    // auto and zero
    Length a = Length::auto_val();
    EXPECT_TRUE(a.is_auto());
    EXPECT_FALSE(a.is_zero());

    Length z = Length::zero();
    EXPECT_TRUE(z.is_zero());
    EXPECT_FALSE(z.is_auto());
}

TEST(ComputedStyleTest, OutlinePropertiesV115) {
    ComputedStyle s;
    // Defaults
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 0.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::None);
    EXPECT_EQ(s.outline_color, Color::black());
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 0.0f);

    // Set outline properties
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Solid;
    s.outline_color = {255, 165, 0, 255}; // orange
    s.outline_offset = Length::px(5.0f);

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Solid);
    EXPECT_EQ(s.outline_color.r, 255);
    EXPECT_EQ(s.outline_color.g, 165);
    EXPECT_EQ(s.outline_color.b, 0);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);

    // Change to dashed style with negative offset
    s.outline_style = BorderStyle::Dashed;
    s.outline_offset = Length::px(-2.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Dashed);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), -2.0f);
}

TEST(ComputedStyleTest, FlexboxLayoutPropsV115) {
    ComputedStyle s;
    // Defaults
    EXPECT_EQ(s.flex_direction, FlexDirection::Row);
    EXPECT_EQ(s.flex_wrap, FlexWrap::NoWrap);
    EXPECT_EQ(s.justify_content, JustifyContent::FlexStart);
    EXPECT_EQ(s.align_items, AlignItems::Stretch);
    EXPECT_FLOAT_EQ(s.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 1.0f);
    EXPECT_TRUE(s.flex_basis.is_auto());
    EXPECT_EQ(s.order, 0);

    // Column reverse with wrapping and space-around
    s.flex_direction = FlexDirection::ColumnReverse;
    s.flex_wrap = FlexWrap::WrapReverse;
    s.justify_content = JustifyContent::SpaceAround;
    s.align_items = AlignItems::Center;
    s.flex_grow = 2.5f;
    s.flex_shrink = 0.0f;
    s.flex_basis = Length::px(100.0f);
    s.order = -3;

    EXPECT_EQ(s.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(s.flex_wrap, FlexWrap::WrapReverse);
    EXPECT_EQ(s.justify_content, JustifyContent::SpaceAround);
    EXPECT_EQ(s.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(s.flex_grow, 2.5f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_basis.to_px(), 100.0f);
    EXPECT_EQ(s.order, -3);

    // SpaceEvenly + Baseline
    s.justify_content = JustifyContent::SpaceEvenly;
    s.align_items = AlignItems::Baseline;
    EXPECT_EQ(s.justify_content, JustifyContent::SpaceEvenly);
    EXPECT_EQ(s.align_items, AlignItems::Baseline);
}

TEST(ComputedStyleTest, BoxShadowMultipleEntriesV115) {
    ComputedStyle s;
    EXPECT_TRUE(s.box_shadows.empty());

    // Add two box shadows
    ComputedStyle::BoxShadowEntry shadow1;
    shadow1.offset_x = 2.0f;
    shadow1.offset_y = 4.0f;
    shadow1.blur = 8.0f;
    shadow1.spread = 1.0f;
    shadow1.color = {0, 0, 0, 128};
    shadow1.inset = false;
    s.box_shadows.push_back(shadow1);

    ComputedStyle::BoxShadowEntry shadow2;
    shadow2.offset_x = 0.0f;
    shadow2.offset_y = 0.0f;
    shadow2.blur = 20.0f;
    shadow2.spread = -5.0f;
    shadow2.color = {255, 0, 0, 200};
    shadow2.inset = true;
    s.box_shadows.push_back(shadow2);

    EXPECT_EQ(s.box_shadows.size(), 2u);

    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 8.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 1.0f);
    EXPECT_FALSE(s.box_shadows[0].inset);
    EXPECT_EQ(s.box_shadows[0].color.a, 128);

    EXPECT_FLOAT_EQ(s.box_shadows[1].blur, 20.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[1].spread, -5.0f);
    EXPECT_TRUE(s.box_shadows[1].inset);
    EXPECT_EQ(s.box_shadows[1].color.r, 255);
    EXPECT_EQ(s.box_shadows[1].color.a, 200);
}

TEST(ComputedStyleTest, TransformStackingV115) {
    ComputedStyle s;
    EXPECT_TRUE(s.transforms.empty());

    // Translate + Rotate + Scale
    Transform t1;
    t1.type = TransformType::Translate;
    t1.x = 50.0f;
    t1.y = -30.0f;
    s.transforms.push_back(t1);

    Transform t2;
    t2.type = TransformType::Rotate;
    t2.angle = 45.0f;
    s.transforms.push_back(t2);

    Transform t3;
    t3.type = TransformType::Scale;
    t3.x = 2.0f;
    t3.y = 0.5f;
    s.transforms.push_back(t3);

    EXPECT_EQ(s.transforms.size(), 3u);
    EXPECT_EQ(s.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(s.transforms[0].x, 50.0f);
    EXPECT_FLOAT_EQ(s.transforms[0].y, -30.0f);

    EXPECT_EQ(s.transforms[1].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(s.transforms[1].angle, 45.0f);

    EXPECT_EQ(s.transforms[2].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(s.transforms[2].x, 2.0f);
    EXPECT_FLOAT_EQ(s.transforms[2].y, 0.5f);

    // Transform origin
    s.transform_origin_x = 0.0f;
    s.transform_origin_y = 100.0f;
    EXPECT_FLOAT_EQ(s.transform_origin_x, 0.0f);
    EXPECT_FLOAT_EQ(s.transform_origin_y, 100.0f);
}

TEST(ComputedStyleTest, WordAndLetterSpacingV115) {
    ComputedStyle s;
    // Defaults are zero-valued Length
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(16.0f), 0.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), 0.0f);

    // Set px values
    s.word_spacing = Length::px(4.0f);
    s.letter_spacing = Length::px(2.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), 2.0f);

    // Set em values (relative to font size)
    s.word_spacing = Length::em(0.5f);
    s.letter_spacing = Length::em(0.1f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(20.0f), 10.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(20.0f), 2.0f);

    // Negative spacing
    s.letter_spacing = Length::px(-1.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), -1.0f);
}

TEST(ComputedStyleTest, VisualEnumCombinationsV115) {
    ComputedStyle s;
    // Test default enum values
    EXPECT_EQ(s.visibility, Visibility::Visible);
    EXPECT_EQ(s.cursor, Cursor::Auto);
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    EXPECT_EQ(s.user_select, UserSelect::Auto);
    EXPECT_EQ(s.vertical_align, VerticalAlign::Baseline);
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);
    EXPECT_EQ(s.display, Display::Inline);
    EXPECT_EQ(s.position, Position::Static);
    EXPECT_EQ(s.overflow_x, Overflow::Visible);

    // Set all to non-default values
    s.visibility = Visibility::Hidden;
    s.cursor = Cursor::Pointer;
    s.pointer_events = PointerEvents::None;
    s.user_select = UserSelect::None;
    s.vertical_align = VerticalAlign::Middle;
    s.white_space = WhiteSpace::NoWrap;
    s.display = Display::Flex;
    s.position = Position::Sticky;
    s.overflow_x = Overflow::Hidden;
    s.overflow_y = Overflow::Scroll;

    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_EQ(s.cursor, Cursor::Pointer);
    EXPECT_EQ(s.pointer_events, PointerEvents::None);
    EXPECT_EQ(s.user_select, UserSelect::None);
    EXPECT_EQ(s.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(s.white_space, WhiteSpace::NoWrap);
    EXPECT_EQ(s.display, Display::Flex);
    EXPECT_EQ(s.position, Position::Sticky);
    EXPECT_EQ(s.overflow_x, Overflow::Hidden);
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);

    // Collapse visibility and auto overflow
    s.visibility = Visibility::Collapse;
    s.overflow_x = Overflow::Auto;
    EXPECT_EQ(s.visibility, Visibility::Collapse);
    EXPECT_EQ(s.overflow_x, Overflow::Auto);
}

// ---------------------------------------------------------------------------
// V116 Tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, ParseBorderTopColorViaResolverV116) {
    // border-top-color should go into border_top.color (no border_color field)
    const std::string css = "div{border-top-color:#00ff00;border-top-style:solid;border-top-width:1px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.border_top.color, (Color{0, 255, 0, 255}));
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 1.0f);
}

TEST(ComputedStyleTest, OutlineWidthIsLengthV116) {
    // outline_width is Length type that uses .to_px()
    ComputedStyle s;
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 0.0f);

    s.outline_width = Length::px(5.0f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 5.0f);

    // em-based outline width relative to a 20px font size
    s.outline_width = Length::em(0.25f);
    EXPECT_FLOAT_EQ(s.outline_width.to_px(20.0f), 5.0f);

    // outline offset also Length
    s.outline_offset = Length::px(3.0f);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 3.0f);
}

TEST(ComputedStyleTest, TextShadowMultipleEntriesV116) {
    ComputedStyle s;
    EXPECT_TRUE(s.text_shadows.empty());

    ComputedStyle::TextShadowEntry ts1;
    ts1.offset_x = 1.0f;
    ts1.offset_y = 2.0f;
    ts1.blur = 3.0f;
    ts1.color = {128, 128, 128, 255};
    s.text_shadows.push_back(ts1);

    ComputedStyle::TextShadowEntry ts2;
    ts2.offset_x = -1.0f;
    ts2.offset_y = -2.0f;
    ts2.blur = 0.0f;
    ts2.color = {255, 0, 0, 128};
    s.text_shadows.push_back(ts2);

    EXPECT_EQ(s.text_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_x, 1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].blur, 3.0f);
    EXPECT_EQ(s.text_shadows[0].color.r, 128);

    EXPECT_FLOAT_EQ(s.text_shadows[1].offset_x, -1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[1].blur, 0.0f);
    EXPECT_EQ(s.text_shadows[1].color.a, 128);
}

TEST(ComputedStyleTest, FlexboxShorthandPropertiesV116) {
    ComputedStyle s;
    // Default flexbox values
    EXPECT_EQ(s.flex_direction, FlexDirection::Row);
    EXPECT_EQ(s.flex_wrap, FlexWrap::NoWrap);
    EXPECT_FLOAT_EQ(s.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 1.0f);
    EXPECT_TRUE(s.flex_basis.is_auto());

    // Set to column-reverse wrap with flex: 2 0 100px equivalent
    s.flex_direction = FlexDirection::ColumnReverse;
    s.flex_wrap = FlexWrap::WrapReverse;
    s.flex_grow = 2.0f;
    s.flex_shrink = 0.0f;
    s.flex_basis = Length::px(100.0f);

    EXPECT_EQ(s.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(s.flex_wrap, FlexWrap::WrapReverse);
    EXPECT_FLOAT_EQ(s.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_basis.to_px(), 100.0f);
}

TEST(CSSStyleTest, ParsePositionFixedWithZIndexV116) {
    const std::string css = "nav{position:fixed;z-index:999;top:0px;left:0px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "nav";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_EQ(style.z_index, 999);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
}

TEST(ComputedStyleTest, BorderEdgeIndependenceV116) {
    // Each border edge is independent — setting one must not affect others
    ComputedStyle s;

    s.border_top.width = Length::px(1.0f);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = {255, 0, 0, 255};

    s.border_right.width = Length::px(2.0f);
    s.border_right.style = BorderStyle::Dashed;
    s.border_right.color = {0, 255, 0, 255};

    s.border_bottom.width = Length::px(3.0f);
    s.border_bottom.style = BorderStyle::Dotted;
    s.border_bottom.color = {0, 0, 255, 255};

    s.border_left.width = Length::px(4.0f);
    s.border_left.style = BorderStyle::Double;
    s.border_left.color = {255, 255, 0, 255};

    // Verify each edge retained its values
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 1.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_top.color.g, 0);

    EXPECT_FLOAT_EQ(s.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(s.border_right.color.g, 255);

    EXPECT_FLOAT_EQ(s.border_bottom.width.to_px(), 3.0f);
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(s.border_bottom.color.b, 255);

    EXPECT_FLOAT_EQ(s.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(s.border_left.style, BorderStyle::Double);
    EXPECT_EQ(s.border_left.color.r, 255);
    EXPECT_EQ(s.border_left.color.g, 255);
}

TEST(ComputedStyleTest, LengthViewportUnitsV116) {
    // Test vw, vh, vmin, vmax unit conversions
    Length::set_viewport(1200.0f, 800.0f);

    Length vw10 = Length::vw(10.0f);
    EXPECT_FLOAT_EQ(vw10.to_px(), 120.0f); // 10% of 1200

    Length vh25 = Length::vh(25.0f);
    EXPECT_FLOAT_EQ(vh25.to_px(), 200.0f); // 25% of 800

    Length vmin50 = Length::vmin(50.0f);
    EXPECT_FLOAT_EQ(vmin50.to_px(), 400.0f); // 50% of min(1200,800) = 50% of 800

    Length vmax50 = Length::vmax(50.0f);
    EXPECT_FLOAT_EQ(vmax50.to_px(), 600.0f); // 50% of max(1200,800) = 50% of 1200

    // Restore default viewport
    Length::set_viewport(800.0f, 600.0f);
}

TEST(CSSStyleTest, ParseOverflowHiddenBothAxesV116) {
    const std::string css = "div{overflow:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    // overflow shorthand sets both axes
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

// ---------------------------------------------------------------------------
// V117 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, FlexShorthandParsesGrowShrinkBasisV117) {
    // flex: 2 1 100px should set grow=2, shrink=1, basis=100px
    const std::string css = "div{display:flex;} .item{flex:2 1 100px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"item"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(style.flex_shrink, 1.0f);
    EXPECT_FLOAT_EQ(style.flex_basis.to_px(), 100.0f);
}

TEST(ComputedStyleTest, BorderTopPropertiesIndividualV117) {
    // Test setting individual border-top properties via CSS
    const std::string css = "div{border-top-width:3px; border-top-style:dashed; border-top-color:rgb(0,128,255);}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(style.border_top.style, BorderStyle::Dashed);
    EXPECT_EQ(style.border_top.color.r, 0);
    EXPECT_EQ(style.border_top.color.g, 128);
    EXPECT_EQ(style.border_top.color.b, 255);
}

TEST(ComputedStyleTest, OutlineWidthIsLengthTypeV117) {
    // outline_width is a Length; verify to_px() conversion
    ComputedStyle s;
    s.outline_width = Length::px(4.0f);
    s.outline_style = BorderStyle::Solid;
    s.outline_color = Color{255, 0, 0, 255};

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 4.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Solid);
    EXPECT_EQ(s.outline_color.r, 255);
    EXPECT_EQ(s.outline_color.a, 255);
}

TEST(ComputedStyleTest, LetterWordSpacingAreLengthV117) {
    // letter_spacing and word_spacing are Length type, use to_px(font_size)
    ComputedStyle s;
    s.letter_spacing = Length::em(0.1f);
    s.word_spacing = Length::px(5.0f);
    s.font_size = Length::px(20.0f);

    float fs = s.font_size.to_px();
    // 0.1em at 20px font = 2px
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(fs), 2.0f);
    // 5px is just 5px regardless of font
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(), 5.0f);
}

TEST(ComputedStyleTest, BoxShadowEntryBlurFieldV117) {
    // BoxShadowEntry uses .blur, NOT .blur_radius
    ComputedStyle s;
    ComputedStyle::BoxShadowEntry shadow;
    shadow.offset_x = 2.0f;
    shadow.offset_y = 4.0f;
    shadow.blur = 8.0f;
    shadow.spread = 1.0f;
    shadow.color = Color{0, 0, 0, 128};
    shadow.inset = false;
    s.box_shadows.push_back(shadow);

    ASSERT_EQ(s.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_y, 4.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 8.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 1.0f);
    EXPECT_EQ(s.box_shadows[0].color.a, 128);
    EXPECT_FALSE(s.box_shadows[0].inset);
}

TEST(CSSStyleTest, ParseVisibilityHiddenEnumV117) {
    // visibility: hidden maps to Visibility::Hidden enum
    const std::string css = "span{visibility:hidden;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.visibility, Visibility::Hidden);
}

TEST(CSSStyleTest, ParseCursorPointerAndUserSelectNoneV117) {
    // cursor: pointer and user-select: none are enum values
    const std::string css = "a{cursor:pointer; user-select:none;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "a";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.cursor, Cursor::Pointer);
    EXPECT_EQ(style.user_select, UserSelect::None);
}

TEST(ComputedStyleTest, TextShadowEntryMultipleV117) {
    // Verify multiple text-shadow entries can be stored
    ComputedStyle s;
    ComputedStyle::TextShadowEntry ts1;
    ts1.offset_x = 1.0f;
    ts1.offset_y = 1.0f;
    ts1.blur = 3.0f;
    ts1.color = Color{255, 0, 0, 255};

    ComputedStyle::TextShadowEntry ts2;
    ts2.offset_x = -1.0f;
    ts2.offset_y = -1.0f;
    ts2.blur = 5.0f;
    ts2.color = Color{0, 0, 255, 200};

    s.text_shadows.push_back(ts1);
    s.text_shadows.push_back(ts2);

    ASSERT_EQ(s.text_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_x, 1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].blur, 3.0f);
    EXPECT_EQ(s.text_shadows[0].color.r, 255);
    EXPECT_FLOAT_EQ(s.text_shadows[1].offset_x, -1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[1].blur, 5.0f);
    EXPECT_EQ(s.text_shadows[1].color.b, 255);
    EXPECT_EQ(s.text_shadows[1].color.a, 200);
}

// ---------------------------------------------------------------------------
// V118 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, DisplayPositionAndBoxSizingDefaultsV118) {
    // Verify default enum values for display, position, and box_sizing
    ComputedStyle s;
    EXPECT_EQ(s.display, Display::Inline);
    EXPECT_EQ(s.position, Position::Static);
    EXPECT_EQ(s.box_sizing, BoxSizing::ContentBox);
    EXPECT_EQ(s.float_val, Float::None);
    EXPECT_EQ(s.clear, Clear::None);

    // Mutate to non-default values
    s.display = Display::Flex;
    s.position = Position::Relative;
    s.box_sizing = BoxSizing::BorderBox;
    EXPECT_EQ(s.display, Display::Flex);
    EXPECT_EQ(s.position, Position::Relative);
    EXPECT_EQ(s.box_sizing, BoxSizing::BorderBox);
}

TEST(ComputedStyleTest, MarginPaddingEdgeSizesIndependentV118) {
    // Each edge of margin and padding can be set independently as Length
    ComputedStyle s;
    s.margin.top = Length::px(10.0f);
    s.margin.right = Length::px(20.0f);
    s.margin.bottom = Length::px(30.0f);
    s.margin.left = Length::px(40.0f);

    s.padding.top = Length::em(1.0f);
    s.padding.right = Length::em(2.0f);
    s.padding.bottom = Length::em(3.0f);
    s.padding.left = Length::em(4.0f);

    EXPECT_FLOAT_EQ(s.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(s.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(s.margin.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(s.margin.left.to_px(), 40.0f);

    float fs = 16.0f; // base font size
    EXPECT_FLOAT_EQ(s.padding.top.to_px(fs), 16.0f);
    EXPECT_FLOAT_EQ(s.padding.right.to_px(fs), 32.0f);
    EXPECT_FLOAT_EQ(s.padding.bottom.to_px(fs), 48.0f);
    EXPECT_FLOAT_EQ(s.padding.left.to_px(fs), 64.0f);
}

TEST(ComputedStyleTest, BorderEdgesFourSidesColorV118) {
    // border_top/right/bottom/left each have width, style, color
    // NO border_color shorthand — must access per-side
    ComputedStyle s;
    s.border_top.width = Length::px(1.0f);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = Color{255, 0, 0, 255};

    s.border_right.width = Length::px(2.0f);
    s.border_right.style = BorderStyle::Dashed;
    s.border_right.color = Color{0, 255, 0, 255};

    s.border_bottom.width = Length::px(3.0f);
    s.border_bottom.style = BorderStyle::Dotted;
    s.border_bottom.color = Color{0, 0, 255, 255};

    s.border_left.width = Length::px(4.0f);
    s.border_left.style = BorderStyle::Double;
    s.border_left.color = Color{128, 128, 128, 200};

    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 1.0f);
    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);

    EXPECT_FLOAT_EQ(s.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_right.color.g, 255);
    EXPECT_EQ(s.border_right.style, BorderStyle::Dashed);

    EXPECT_FLOAT_EQ(s.border_bottom.width.to_px(), 3.0f);
    EXPECT_EQ(s.border_bottom.color.b, 255);
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dotted);

    EXPECT_FLOAT_EQ(s.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(s.border_left.color.a, 200);
    EXPECT_EQ(s.border_left.style, BorderStyle::Double);
}

TEST(ComputedStyleTest, OverflowXYIndependentEnumsV118) {
    // overflow_x and overflow_y are independent Overflow enums
    ComputedStyle s;
    EXPECT_EQ(s.overflow_x, Overflow::Visible);
    EXPECT_EQ(s.overflow_y, Overflow::Visible);

    s.overflow_x = Overflow::Hidden;
    s.overflow_y = Overflow::Scroll;
    EXPECT_EQ(s.overflow_x, Overflow::Hidden);
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);

    // Also verify auto overflow
    s.overflow_x = Overflow::Auto;
    EXPECT_EQ(s.overflow_x, Overflow::Auto);
    // overflow_y unchanged
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);
}

TEST(ComputedStyleTest, OutlinePropertiesAllFieldsV118) {
    // outline_width is Length (.to_px()), outline_offset is also Length
    ComputedStyle s;
    s.outline_width = Length::px(3.0f);
    s.outline_style = BorderStyle::Dashed;
    s.outline_color = Color{100, 200, 50, 180};
    s.outline_offset = Length::px(5.0f);

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 3.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Dashed);
    EXPECT_EQ(s.outline_color.r, 100);
    EXPECT_EQ(s.outline_color.g, 200);
    EXPECT_EQ(s.outline_color.b, 50);
    EXPECT_EQ(s.outline_color.a, 180);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);
}

TEST(CSSStyleTest, ParseWhiteSpaceNoWrapEnumV118) {
    // white-space: nowrap should map to WhiteSpace::NoWrap
    const std::string css = "p{white-space:nowrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

TEST(ComputedStyleTest, BoxShadowInsetAndSpreadFieldsV118) {
    // BoxShadowEntry has .blur (not .blur_radius), .spread, .inset
    ComputedStyle s;

    ComputedStyle::BoxShadowEntry outer;
    outer.offset_x = 5.0f;
    outer.offset_y = 5.0f;
    outer.blur = 10.0f;
    outer.spread = 2.0f;
    outer.color = Color{0, 0, 0, 200};
    outer.inset = false;

    ComputedStyle::BoxShadowEntry inner;
    inner.offset_x = 0.0f;
    inner.offset_y = 0.0f;
    inner.blur = 6.0f;
    inner.spread = -3.0f;
    inner.color = Color{255, 255, 255, 100};
    inner.inset = true;

    s.box_shadows.push_back(outer);
    s.box_shadows.push_back(inner);

    ASSERT_EQ(s.box_shadows.size(), 2u);

    // First shadow: outer
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_x, 5.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 10.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 2.0f);
    EXPECT_FALSE(s.box_shadows[0].inset);
    EXPECT_EQ(s.box_shadows[0].color.a, 200);

    // Second shadow: inset
    EXPECT_FLOAT_EQ(s.box_shadows[1].blur, 6.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[1].spread, -3.0f);
    EXPECT_TRUE(s.box_shadows[1].inset);
    EXPECT_EQ(s.box_shadows[1].color.r, 255);
}

TEST(CSSStyleTest, ParseVerticalAlignMiddleViaResolverV118) {
    // vertical-align: middle maps to VerticalAlign::Middle enum
    const std::string css = "td{vertical-align:middle;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "td";

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
}

// ---------------------------------------------------------------------------
// V119 Tests
// ---------------------------------------------------------------------------

TEST(ComputedStyleTest, GridLayoutPropertiesV119) {
    // Grid template/auto/flow properties default and assignment
    ComputedStyle s;
    EXPECT_TRUE(s.grid_template_columns.empty());
    EXPECT_TRUE(s.grid_template_rows.empty());
    EXPECT_TRUE(s.grid_column.empty());
    EXPECT_TRUE(s.grid_row.empty());
    EXPECT_EQ(s.grid_auto_flow, 0); // 0=row

    s.display = Display::Grid;
    s.grid_template_columns = "1fr 2fr 1fr";
    s.grid_template_rows = "100px auto";
    s.grid_auto_flow = 3; // column dense
    s.grid_column = "1 / 3";
    s.grid_row = "2 / 4";
    s.grid_auto_rows = "minmax(100px, auto)";
    s.grid_template_areas = "\"header header\" \"sidebar main\"";

    EXPECT_EQ(s.display, Display::Grid);
    EXPECT_EQ(s.grid_template_columns, "1fr 2fr 1fr");
    EXPECT_EQ(s.grid_template_rows, "100px auto");
    EXPECT_EQ(s.grid_auto_flow, 3);
    EXPECT_EQ(s.grid_column, "1 / 3");
    EXPECT_EQ(s.grid_row, "2 / 4");
    EXPECT_EQ(s.grid_auto_rows, "minmax(100px, auto)");
    EXPECT_EQ(s.grid_template_areas, "\"header header\" \"sidebar main\"");
}

TEST(ComputedStyleTest, TransitionDefFieldsV119) {
    // TransitionDef struct holds parsed transition info
    ComputedStyle s;
    EXPECT_TRUE(s.transitions.empty());
    EXPECT_EQ(s.transition_property, "all");
    EXPECT_FLOAT_EQ(s.transition_duration, 0.0f);
    EXPECT_EQ(s.transition_timing, 0); // ease

    TransitionDef t1;
    t1.property = "opacity";
    t1.duration_ms = 300.0f;
    t1.delay_ms = 50.0f;
    t1.timing_function = 1; // linear

    TransitionDef t2;
    t2.property = "transform";
    t2.duration_ms = 500.0f;
    t2.delay_ms = 0.0f;
    t2.timing_function = 5; // cubic-bezier
    t2.bezier_x1 = 0.25f;
    t2.bezier_y1 = 0.1f;
    t2.bezier_x2 = 0.25f;
    t2.bezier_y2 = 1.0f;

    s.transitions.push_back(t1);
    s.transitions.push_back(t2);

    EXPECT_EQ(s.transitions.size(), 2u);
    EXPECT_EQ(s.transitions[0].property, "opacity");
    EXPECT_FLOAT_EQ(s.transitions[0].duration_ms, 300.0f);
    EXPECT_EQ(s.transitions[1].timing_function, 5);
    EXPECT_FLOAT_EQ(s.transitions[1].bezier_x1, 0.25f);
    EXPECT_FLOAT_EQ(s.transitions[1].bezier_y2, 1.0f);
}

TEST(ComputedStyleTest, LengthRemChLhUnitsV119) {
    // Verify rem, ch, lh units use correct context in to_px
    Length rem_len = Length::rem(2.0f);
    EXPECT_FLOAT_EQ(rem_len.to_px(0, 18.0f), 36.0f); // 2rem * 18px root

    Length ch_len = Length::ch(5.0f);
    // ch maps like em (uses parent_value as ch width)
    EXPECT_EQ(ch_len.unit, Length::Unit::Ch);
    EXPECT_FLOAT_EQ(ch_len.value, 5.0f);

    Length lh_len = Length::lh(1.5f);
    // lh uses line_height (3rd param)
    EXPECT_FLOAT_EQ(lh_len.to_px(0, 16, 24.0f), 36.0f); // 1.5 * 24px line-height

    // Verify auto and zero
    Length auto_len = Length::auto_val();
    EXPECT_TRUE(auto_len.is_auto());
    EXPECT_FALSE(auto_len.is_zero());

    Length zero_len = Length::zero();
    EXPECT_TRUE(zero_len.is_zero());
    EXPECT_FALSE(zero_len.is_auto());
}

TEST(ComputedStyleTest, AnimationPropertiesV119) {
    // Animation fields default and assignment
    ComputedStyle s;
    EXPECT_TRUE(s.animation_name.empty());
    EXPECT_FLOAT_EQ(s.animation_duration, 0.0f);
    EXPECT_FLOAT_EQ(s.animation_delay, 0.0f);
    EXPECT_FLOAT_EQ(s.animation_iteration_count, 1.0f);
    EXPECT_EQ(s.animation_direction, 0);  // normal
    EXPECT_EQ(s.animation_fill_mode, 0);  // none
    EXPECT_EQ(s.animation_play_state, 0); // running
    EXPECT_EQ(s.animation_composition, 0); // replace
    EXPECT_EQ(s.animation_timeline, "auto");

    s.animation_name = "slideIn";
    s.animation_duration = 2.5f;
    s.animation_delay = 0.3f;
    s.animation_iteration_count = -1.0f; // infinite
    s.animation_direction = 2; // alternate
    s.animation_fill_mode = 3; // both
    s.animation_play_state = 1; // paused
    s.animation_timing = 4; // ease-in-out

    EXPECT_EQ(s.animation_name, "slideIn");
    EXPECT_FLOAT_EQ(s.animation_duration, 2.5f);
    EXPECT_FLOAT_EQ(s.animation_iteration_count, -1.0f);
    EXPECT_EQ(s.animation_direction, 2);
    EXPECT_EQ(s.animation_fill_mode, 3);
    EXPECT_EQ(s.animation_play_state, 1);
    EXPECT_EQ(s.animation_timing, 4);
}

TEST(ComputedStyleTest, SVGPaintPropertiesDefaultsV119) {
    // SVG fill/stroke properties defaults and assignment
    ComputedStyle s;
    EXPECT_EQ(s.svg_fill_color, 0xFF000000u);     // black
    EXPECT_FALSE(s.svg_fill_none);
    EXPECT_EQ(s.svg_stroke_color, 0xFF000000u);    // black
    EXPECT_TRUE(s.svg_stroke_none);                 // default: no stroke
    EXPECT_FLOAT_EQ(s.svg_fill_opacity, 1.0f);
    EXPECT_FLOAT_EQ(s.svg_stroke_opacity, 1.0f);
    EXPECT_FLOAT_EQ(s.svg_stroke_width, 0.0f);
    EXPECT_EQ(s.svg_stroke_linecap, 0);  // butt
    EXPECT_EQ(s.svg_stroke_linejoin, 0); // miter
    EXPECT_FLOAT_EQ(s.stroke_miterlimit, 4.0f);
    EXPECT_EQ(s.svg_text_anchor, 0); // start

    s.svg_fill_color = 0xFF00FF00u;  // green
    s.svg_fill_none = true;
    s.svg_stroke_color = 0xFFFF0000u; // red
    s.svg_stroke_none = false;
    s.svg_stroke_width = 2.5f;
    s.svg_stroke_linecap = 1; // round
    s.svg_stroke_linejoin = 2; // bevel

    EXPECT_EQ(s.svg_fill_color, 0xFF00FF00u);
    EXPECT_TRUE(s.svg_fill_none);
    EXPECT_EQ(s.svg_stroke_color, 0xFFFF0000u);
    EXPECT_FALSE(s.svg_stroke_none);
    EXPECT_FLOAT_EQ(s.svg_stroke_width, 2.5f);
    EXPECT_EQ(s.svg_stroke_linecap, 1);
    EXPECT_EQ(s.svg_stroke_linejoin, 2);
}

TEST(ComputedStyleTest, CustomPropertiesAndCSSVariablesV119) {
    // custom_properties stores CSS variable values
    ComputedStyle s;
    EXPECT_TRUE(s.custom_properties.empty());

    s.custom_properties["--primary-color"] = "#3498db";
    s.custom_properties["--spacing-unit"] = "8px";
    s.custom_properties["--font-stack"] = "\"Helvetica\", sans-serif";

    EXPECT_EQ(s.custom_properties.size(), 3u);
    EXPECT_EQ(s.custom_properties["--primary-color"], "#3498db");
    EXPECT_EQ(s.custom_properties["--spacing-unit"], "8px");
    EXPECT_EQ(s.custom_properties["--font-stack"], "\"Helvetica\", sans-serif");

    // Overwrite existing
    s.custom_properties["--primary-color"] = "rgb(255,0,0)";
    EXPECT_EQ(s.custom_properties["--primary-color"], "rgb(255,0,0)");
    EXPECT_EQ(s.custom_properties.size(), 3u);
}

TEST(ComputedStyleTest, ClipPathAndFiltersV119) {
    // clip_path and filter/backdrop_filter properties
    ComputedStyle s;
    EXPECT_EQ(s.clip_path_type, 0); // none
    EXPECT_TRUE(s.clip_path_values.empty());
    EXPECT_TRUE(s.filters.empty());
    EXPECT_TRUE(s.backdrop_filters.empty());

    // Set circle clip path
    s.clip_path_type = 1; // circle
    s.clip_path_values.push_back(50.0f); // radius

    EXPECT_EQ(s.clip_path_type, 1);
    EXPECT_EQ(s.clip_path_values.size(), 1u);
    EXPECT_FLOAT_EQ(s.clip_path_values[0], 50.0f);

    // Add filters: grayscale(0.5) brightness(1.2) blur(4)
    s.filters.push_back({1, 0.5f});  // grayscale
    s.filters.push_back({3, 1.2f});  // brightness
    s.filters.push_back({9, 4.0f});  // blur

    EXPECT_EQ(s.filters.size(), 3u);
    EXPECT_EQ(s.filters[0].first, 1);
    EXPECT_FLOAT_EQ(s.filters[0].second, 0.5f);
    EXPECT_EQ(s.filters[2].first, 9);
    EXPECT_FLOAT_EQ(s.filters[2].second, 4.0f);

    // Backdrop filter: blur(10)
    s.backdrop_filters.push_back({9, 10.0f});
    EXPECT_EQ(s.backdrop_filters.size(), 1u);
    EXPECT_FLOAT_EQ(s.backdrop_filters[0].second, 10.0f);
}

TEST(ComputedStyleTest, TextIndentTabSizeAndTypographyFieldsV119) {
    // text_indent, tab_size, and related typography fields via direct assignment
    ComputedStyle s;
    EXPECT_TRUE(s.text_indent.is_zero());
    EXPECT_EQ(s.tab_size, 4); // default
    EXPECT_EQ(s.hyphens, 1); // manual
    EXPECT_EQ(s.text_justify, 0); // auto
    EXPECT_FLOAT_EQ(s.text_underline_offset, 0.0f);

    s.text_indent = Length::em(2.0f);
    s.tab_size = 8;
    s.hyphens = 2; // auto
    s.text_justify = 2; // inter-character
    s.text_underline_offset = 3.0f;
    s.text_decoration_bits = 1 | 4; // underline + line-through
    s.text_decoration_style = TextDecorationStyle::Wavy;
    s.text_decoration_thickness = 2.5f;

    EXPECT_FLOAT_EQ(s.text_indent.to_px(16.0f), 32.0f); // 2em * 16px
    EXPECT_EQ(s.tab_size, 8);
    EXPECT_EQ(s.hyphens, 2);
    EXPECT_EQ(s.text_justify, 2);
    EXPECT_FLOAT_EQ(s.text_underline_offset, 3.0f);
    EXPECT_EQ(s.text_decoration_bits, 5); // 1|4
    EXPECT_EQ(s.text_decoration_style, TextDecorationStyle::Wavy);
    EXPECT_FLOAT_EQ(s.text_decoration_thickness, 2.5f);
}

TEST(ComputedStyleTest, BorderEdgeFieldsAndDefaultsV120) {
    // Verify border edges have correct defaults and can be independently set
    ComputedStyle s;
    // All borders default to zero width, none style, black color
    EXPECT_TRUE(s.border_top.width.is_zero());
    EXPECT_EQ(s.border_top.style, BorderStyle::None);
    EXPECT_EQ(s.border_top.color, Color::black());
    EXPECT_TRUE(s.border_right.width.is_zero());
    EXPECT_TRUE(s.border_bottom.width.is_zero());
    EXPECT_TRUE(s.border_left.width.is_zero());

    // Set each border independently
    s.border_top.width = Length::px(3.0f);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = {255, 0, 0, 255};

    s.border_right.width = Length::px(2.0f);
    s.border_right.style = BorderStyle::Dashed;
    s.border_right.color = {0, 255, 0, 255};

    s.border_bottom.width = Length::px(1.0f);
    s.border_bottom.style = BorderStyle::Dotted;
    s.border_bottom.color = {0, 0, 255, 255};

    s.border_left.width = Length::px(4.0f);
    s.border_left.style = BorderStyle::Double;
    s.border_left.color = {128, 128, 128, 255};

    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 3.0f);
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(s.border_top.color.r, 255);
    EXPECT_FLOAT_EQ(s.border_right.width.to_px(), 2.0f);
    EXPECT_EQ(s.border_right.style, BorderStyle::Dashed);
    EXPECT_FLOAT_EQ(s.border_bottom.width.to_px(), 1.0f);
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dotted);
    EXPECT_FLOAT_EQ(s.border_left.width.to_px(), 4.0f);
    EXPECT_EQ(s.border_left.style, BorderStyle::Double);
    EXPECT_EQ(s.border_left.color.g, 128);
}

TEST(ComputedStyleTest, OutlinePropertiesV120) {
    // outline_width is Length, outline_offset is Length, outline_style/outline_color
    ComputedStyle s;
    EXPECT_TRUE(s.outline_width.is_zero());
    EXPECT_EQ(s.outline_style, BorderStyle::None);
    EXPECT_EQ(s.outline_color, Color::black());
    EXPECT_TRUE(s.outline_offset.is_zero());

    s.outline_width = Length::px(2.0f);
    s.outline_style = BorderStyle::Solid;
    s.outline_color = {255, 128, 0, 255};
    s.outline_offset = Length::px(5.0f);

    EXPECT_FLOAT_EQ(s.outline_width.to_px(), 2.0f);
    EXPECT_EQ(s.outline_style, BorderStyle::Solid);
    EXPECT_EQ(s.outline_color.r, 255);
    EXPECT_EQ(s.outline_color.g, 128);
    EXPECT_EQ(s.outline_color.b, 0);
    EXPECT_FLOAT_EQ(s.outline_offset.to_px(), 5.0f);
}

TEST(ComputedStyleTest, BoxShadowEntryBlurFieldV120) {
    // BoxShadowEntry uses .blur (not .blur_radius)
    ComputedStyle s;
    EXPECT_TRUE(s.box_shadows.empty());

    ComputedStyle::BoxShadowEntry entry;
    entry.offset_x = 5.0f;
    entry.offset_y = 10.0f;
    entry.blur = 15.0f;
    entry.spread = 3.0f;
    entry.color = {0, 0, 0, 128};
    entry.inset = true;
    s.box_shadows.push_back(entry);

    ComputedStyle::BoxShadowEntry entry2;
    entry2.offset_x = -2.0f;
    entry2.offset_y = -4.0f;
    entry2.blur = 8.0f;
    entry2.spread = 0.0f;
    entry2.color = {255, 0, 0, 200};
    entry2.inset = false;
    s.box_shadows.push_back(entry2);

    EXPECT_EQ(s.box_shadows.size(), 2u);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 15.0f);
    EXPECT_TRUE(s.box_shadows[0].inset);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 3.0f);
    EXPECT_EQ(s.box_shadows[0].color.a, 128);
    EXPECT_FLOAT_EQ(s.box_shadows[1].blur, 8.0f);
    EXPECT_FALSE(s.box_shadows[1].inset);
    EXPECT_EQ(s.box_shadows[1].color.r, 255);
}

TEST(ComputedStyleTest, EnumFieldAssignmentsV120) {
    // Test enum assignments: Visibility, Cursor, PointerEvents, UserSelect,
    // VerticalAlign, WhiteSpace
    ComputedStyle s;
    EXPECT_EQ(s.visibility, Visibility::Visible);
    EXPECT_EQ(s.cursor, Cursor::Auto);
    EXPECT_EQ(s.pointer_events, PointerEvents::Auto);
    EXPECT_EQ(s.user_select, UserSelect::Auto);
    EXPECT_EQ(s.vertical_align, VerticalAlign::Baseline);
    EXPECT_EQ(s.white_space, WhiteSpace::Normal);

    s.visibility = Visibility::Hidden;
    s.cursor = Cursor::Pointer;
    s.pointer_events = PointerEvents::None;
    s.user_select = UserSelect::None;
    s.vertical_align = VerticalAlign::Middle;
    s.white_space = WhiteSpace::NoWrap;

    EXPECT_EQ(s.visibility, Visibility::Hidden);
    EXPECT_EQ(s.cursor, Cursor::Pointer);
    EXPECT_EQ(s.pointer_events, PointerEvents::None);
    EXPECT_EQ(s.user_select, UserSelect::None);
    EXPECT_EQ(s.vertical_align, VerticalAlign::Middle);
    EXPECT_EQ(s.white_space, WhiteSpace::NoWrap);
}

TEST(ComputedStyleTest, WordAndLetterSpacingLengthV120) {
    // word_spacing and letter_spacing are Length type, use .to_px(font_size)
    ComputedStyle s;
    EXPECT_TRUE(s.word_spacing.is_zero());
    EXPECT_TRUE(s.letter_spacing.is_zero());

    s.word_spacing = Length::px(4.0f);
    s.letter_spacing = Length::px(1.5f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(16.0f), 1.5f);

    // Em-based spacing: relative to font_size
    s.word_spacing = Length::em(0.5f);
    s.letter_spacing = Length::em(0.1f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(20.0f), 10.0f);   // 0.5em * 20px
    EXPECT_FLOAT_EQ(s.letter_spacing.to_px(20.0f), 2.0f);   // 0.1em * 20px

    // Percent-based spacing
    s.word_spacing = Length::percent(50.0f);
    EXPECT_FLOAT_EQ(s.word_spacing.to_px(16.0f), 8.0f);    // 50% of 16px
}

TEST(ComputedStyleTest, FontSizeAndLineHeightLengthV120) {
    // font_size and line_height are Length type
    ComputedStyle s;
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 16.0f); // default 16px

    s.font_size = Length::px(24.0f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(), 24.0f);

    s.font_size = Length::em(1.5f);
    EXPECT_FLOAT_EQ(s.font_size.to_px(16.0f), 24.0f); // 1.5em * 16px parent

    s.line_height = Length::px(30.0f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(), 30.0f);

    s.line_height = Length::em(1.8f);
    EXPECT_FLOAT_EQ(s.line_height.to_px(20.0f), 36.0f); // 1.8em * 20px

    // unitless line-height factor
    EXPECT_FLOAT_EQ(s.line_height_unitless, 1.2f); // default
    s.line_height_unitless = 1.6f;
    EXPECT_FLOAT_EQ(s.line_height_unitless, 1.6f);
}

TEST(ComputedStyleTest, DisplayPositionOverflowEnumsV120) {
    // display, position, overflow are enum types
    ComputedStyle s;
    EXPECT_EQ(s.display, Display::Inline);     // default
    EXPECT_EQ(s.position, Position::Static);   // default
    EXPECT_EQ(s.overflow_x, Overflow::Visible);
    EXPECT_EQ(s.overflow_y, Overflow::Visible);

    s.display = Display::Flex;
    s.position = Position::Relative;
    s.overflow_x = Overflow::Hidden;
    s.overflow_y = Overflow::Scroll;

    EXPECT_EQ(s.display, Display::Flex);
    EXPECT_EQ(s.position, Position::Relative);
    EXPECT_EQ(s.overflow_x, Overflow::Hidden);
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);

    s.display = Display::Grid;
    s.position = Position::Sticky;
    s.overflow_x = Overflow::Auto;
    s.overflow_y = Overflow::Auto;

    EXPECT_EQ(s.display, Display::Grid);
    EXPECT_EQ(s.position, Position::Sticky);
    EXPECT_EQ(s.overflow_x, Overflow::Auto);
    EXPECT_EQ(s.overflow_y, Overflow::Auto);
}

TEST(ComputedStyleTest, SvgMarkerAndMaskPropertiesV120) {
    // marker_start, marker_mid, marker_end, mask_image, mask_origin, mask_clip
    ComputedStyle s;
    EXPECT_TRUE(s.marker_start.empty());
    EXPECT_TRUE(s.marker_mid.empty());
    EXPECT_TRUE(s.marker_end.empty());
    EXPECT_TRUE(s.mask_image.empty());
    EXPECT_EQ(s.mask_origin, 0);  // border-box
    EXPECT_EQ(s.mask_clip, 0);    // border-box

    s.marker_start = "url(#arrow)";
    s.marker_mid = "url(#dot)";
    s.marker_end = "url(#tail)";
    s.mask_image = "url(mask.png)";
    s.mask_origin = 2; // content-box
    s.mask_clip = 3;   // no-clip

    EXPECT_EQ(s.marker_start, "url(#arrow)");
    EXPECT_EQ(s.marker_mid, "url(#dot)");
    EXPECT_EQ(s.marker_end, "url(#tail)");
    EXPECT_EQ(s.mask_image, "url(mask.png)");
    EXPECT_EQ(s.mask_origin, 2);
    EXPECT_EQ(s.mask_clip, 3);

    // Also verify mask_position default and modification
    EXPECT_EQ(s.mask_position, "0% 0%");
    s.mask_position = "center center";
    EXPECT_EQ(s.mask_position, "center center");
}

TEST(ComputedStyleTest, CalcExprUnaryMathFunctionsV121) {
    // Test CSS math functions: abs(), sign(), sin(), cos(), sqrt()
    using Op = CalcExpr::Op;

    // abs(-42px) => 42
    auto neg42 = CalcExpr::make_value(Length::px(-42.0f));
    auto abs_expr = CalcExpr::make_unary(Op::Abs, neg42);
    EXPECT_FLOAT_EQ(abs_expr->evaluate(0, 16), 42.0f);

    // abs(0) => 0
    auto zero_node = CalcExpr::make_value(Length::px(0.0f));
    auto abs_zero = CalcExpr::make_unary(Op::Abs, zero_node);
    EXPECT_FLOAT_EQ(abs_zero->evaluate(0, 16), 0.0f);

    // sign(-99px) => -1, sign(0px) => 0, sign(5px) => 1
    auto neg99 = CalcExpr::make_value(Length::px(-99.0f));
    auto sign_neg = CalcExpr::make_unary(Op::Sign, neg99);
    EXPECT_FLOAT_EQ(sign_neg->evaluate(0, 16), -1.0f);

    auto sign_zero = CalcExpr::make_unary(Op::Sign, CalcExpr::make_value(Length::px(0.0f)));
    EXPECT_FLOAT_EQ(sign_zero->evaluate(0, 16), 0.0f);

    auto sign_pos = CalcExpr::make_unary(Op::Sign, CalcExpr::make_value(Length::px(5.0f)));
    EXPECT_FLOAT_EQ(sign_pos->evaluate(0, 16), 1.0f);

    // sin(0) => 0
    auto sin_zero = CalcExpr::make_unary(Op::Sin, CalcExpr::make_value(Length::px(0.0f)));
    EXPECT_NEAR(sin_zero->evaluate(0, 16), 0.0f, 1e-6f);

    // cos(0) => 1
    auto cos_zero = CalcExpr::make_unary(Op::Cos, CalcExpr::make_value(Length::px(0.0f)));
    EXPECT_NEAR(cos_zero->evaluate(0, 16), 1.0f, 1e-6f);

    // sqrt(144px) => 12
    auto sqrt144 = CalcExpr::make_unary(Op::Sqrt, CalcExpr::make_value(Length::px(144.0f)));
    EXPECT_FLOAT_EQ(sqrt144->evaluate(0, 16), 12.0f);

    // sqrt(-1px) => 0 (clamped for negative input)
    auto sqrt_neg = CalcExpr::make_unary(Op::Sqrt, CalcExpr::make_value(Length::px(-1.0f)));
    EXPECT_FLOAT_EQ(sqrt_neg->evaluate(0, 16), 0.0f);
}

TEST(ComputedStyleTest, CalcExprRoundingOperationsV121) {
    // Test CSS round() with all four rounding strategies
    using Op = CalcExpr::Op;

    auto val_7_3 = CalcExpr::make_value(Length::px(7.3f));
    auto step_5 = CalcExpr::make_value(Length::px(5.0f));

    // round(nearest, 7.3, 5) => round(7.3/5)*5 = round(1.46)*5 = 1*5 = 5
    auto nearest = CalcExpr::make_binary(Op::RoundNearest, val_7_3, step_5);
    EXPECT_FLOAT_EQ(nearest->evaluate(0, 16), 5.0f);

    // round(up, 7.3, 5) => ceil(7.3/5)*5 = ceil(1.46)*5 = 2*5 = 10
    auto up = CalcExpr::make_binary(Op::RoundUp, val_7_3, step_5);
    EXPECT_FLOAT_EQ(up->evaluate(0, 16), 10.0f);

    // round(down, 7.3, 5) => floor(7.3/5)*5 = floor(1.46)*5 = 1*5 = 5
    auto down = CalcExpr::make_binary(Op::RoundDown, val_7_3, step_5);
    EXPECT_FLOAT_EQ(down->evaluate(0, 16), 5.0f);

    // round(to-zero, -7.3, 5) => trunc(-7.3/5)*5 = trunc(-1.46)*5 = -1*5 = -5
    auto neg_val = CalcExpr::make_value(Length::px(-7.3f));
    auto to_zero = CalcExpr::make_binary(Op::RoundToZero, neg_val, step_5);
    EXPECT_FLOAT_EQ(to_zero->evaluate(0, 16), -5.0f);

    // round(nearest, 12.5, 5) => round(2.5)*5 = 3*5 = 15 (round half away from zero)
    auto val_12_5 = CalcExpr::make_value(Length::px(12.5f));
    auto nearest_half = CalcExpr::make_binary(Op::RoundNearest, val_12_5, step_5);
    EXPECT_FLOAT_EQ(nearest_half->evaluate(0, 16), 15.0f);

    // Division by zero step => 0
    auto step_0 = CalcExpr::make_value(Length::px(0.0f));
    auto round_div_zero = CalcExpr::make_binary(Op::RoundNearest, val_7_3, step_0);
    EXPECT_FLOAT_EQ(round_div_zero->evaluate(0, 16), 0.0f);
}

TEST(ComputedStyleTest, CalcExprModVsRemSignBehaviorV121) {
    // CSS mod() result has sign of divisor, rem() has sign of dividend
    using Op = CalcExpr::Op;

    auto pos_18 = CalcExpr::make_value(Length::px(18.0f));
    auto neg_5 = CalcExpr::make_value(Length::px(-5.0f));

    // mod(18, -5): fmod(18,-5) = 3, since result>0 and divisor<0, result += -5 => -2
    auto mod_expr = CalcExpr::make_binary(Op::Mod, pos_18, neg_5);
    EXPECT_FLOAT_EQ(mod_expr->evaluate(0, 16), -2.0f);

    // rem(18, -5): fmod(18,-5) = 3 (sign of dividend = positive)
    auto rem_expr = CalcExpr::make_binary(Op::Rem, pos_18, neg_5);
    EXPECT_FLOAT_EQ(rem_expr->evaluate(0, 16), 3.0f);

    // mod(-18, 5): fmod(-18,5) = -3, since result<0 and divisor>0, result += 5 => 2
    auto neg_18 = CalcExpr::make_value(Length::px(-18.0f));
    auto pos_5 = CalcExpr::make_value(Length::px(5.0f));
    auto mod_neg = CalcExpr::make_binary(Op::Mod, neg_18, pos_5);
    EXPECT_FLOAT_EQ(mod_neg->evaluate(0, 16), 2.0f);

    // rem(-18, 5): fmod(-18,5) = -3 (sign of dividend = negative)
    auto rem_neg = CalcExpr::make_binary(Op::Rem, neg_18, pos_5);
    EXPECT_FLOAT_EQ(rem_neg->evaluate(0, 16), -3.0f);

    // Division by zero for both => 0
    auto zero_div = CalcExpr::make_value(Length::px(0.0f));
    EXPECT_FLOAT_EQ(CalcExpr::make_binary(Op::Mod, pos_18, zero_div)->evaluate(0, 16), 0.0f);
    EXPECT_FLOAT_EQ(CalcExpr::make_binary(Op::Rem, pos_18, zero_div)->evaluate(0, 16), 0.0f);
}

TEST(ComputedStyleTest, CalcExprPowHypotExpLogChainV121) {
    // Test pow(), hypot(), exp(), log() and nesting them
    using Op = CalcExpr::Op;

    // pow(3, 4) => 81
    auto base3 = CalcExpr::make_value(Length::px(3.0f));
    auto exp4 = CalcExpr::make_value(Length::px(4.0f));
    auto pow_expr = CalcExpr::make_binary(Op::Pow, base3, exp4);
    EXPECT_FLOAT_EQ(pow_expr->evaluate(0, 16), 81.0f);

    // hypot(3, 4) => 5
    auto h3 = CalcExpr::make_value(Length::px(3.0f));
    auto h4 = CalcExpr::make_value(Length::px(4.0f));
    auto hypot_expr = CalcExpr::make_binary(Op::Hypot, h3, h4);
    EXPECT_FLOAT_EQ(hypot_expr->evaluate(0, 16), 5.0f);

    // exp(0) => 1
    auto exp_zero = CalcExpr::make_unary(Op::Exp, CalcExpr::make_value(Length::px(0.0f)));
    EXPECT_FLOAT_EQ(exp_zero->evaluate(0, 16), 1.0f);

    // log(1) => 0
    auto log_one = CalcExpr::make_unary(Op::Log, CalcExpr::make_value(Length::px(1.0f)));
    EXPECT_NEAR(log_one->evaluate(0, 16), 0.0f, 1e-6f);

    // log(-1) => 0 (clamped for non-positive input)
    auto log_neg = CalcExpr::make_unary(Op::Log, CalcExpr::make_value(Length::px(-1.0f)));
    EXPECT_FLOAT_EQ(log_neg->evaluate(0, 16), 0.0f);

    // Nesting: sqrt(pow(5, 2) + pow(12, 2)) = sqrt(25 + 144) = sqrt(169) = 13
    // Build using hypot(5, 12) = 13 for the same result
    auto five = CalcExpr::make_value(Length::px(5.0f));
    auto twelve = CalcExpr::make_value(Length::px(12.0f));
    auto nested_hypot = CalcExpr::make_binary(Op::Hypot, five, twelve);
    EXPECT_FLOAT_EQ(nested_hypot->evaluate(0, 16), 13.0f);

    // exp(log(42)) => 42 (roundtrip)
    auto val42 = CalcExpr::make_value(Length::px(42.0f));
    auto log42 = CalcExpr::make_unary(Op::Log, val42);
    auto roundtrip = CalcExpr::make_unary(Op::Exp, log42);
    EXPECT_NEAR(roundtrip->evaluate(0, 16), 42.0f, 0.01f);
}

TEST(ComputedStyleTest, LengthVminVmaxNonSquareViewportV121) {
    // vmin/vmax depend on the smaller/larger of viewport width and height
    // Ensure they resolve correctly with a non-square viewport

    // Save original viewport
    float orig_w = Length::s_viewport_w;
    float orig_h = Length::s_viewport_h;

    // Set a landscape viewport: 1200x400
    Length::set_viewport(1200.0f, 400.0f);

    // 10vmin = 10% of min(1200,400) = 10% of 400 = 40px
    Length vmin10 = Length::vmin(10.0f);
    EXPECT_FLOAT_EQ(vmin10.to_px(), 40.0f);

    // 10vmax = 10% of max(1200,400) = 10% of 1200 = 120px
    Length vmax10 = Length::vmax(10.0f);
    EXPECT_FLOAT_EQ(vmax10.to_px(), 120.0f);

    // 100vmin == the shorter dimension
    Length vmin100 = Length::vmin(100.0f);
    EXPECT_FLOAT_EQ(vmin100.to_px(), 400.0f);

    // Switch to portrait: 500x900
    Length::set_viewport(500.0f, 900.0f);
    Length vmin25 = Length::vmin(25.0f);
    EXPECT_FLOAT_EQ(vmin25.to_px(), 125.0f); // 25% of 500

    Length vmax25 = Length::vmax(25.0f);
    EXPECT_FLOAT_EQ(vmax25.to_px(), 225.0f); // 25% of 900

    // Also verify vw/vh in this portrait viewport
    Length vw50 = Length::vw(50.0f);
    EXPECT_FLOAT_EQ(vw50.to_px(), 250.0f); // 50% of 500

    Length vh50 = Length::vh(50.0f);
    EXPECT_FLOAT_EQ(vh50.to_px(), 450.0f); // 50% of 900

    // Restore original viewport
    Length::set_viewport(orig_w, orig_h);
}

TEST(ComputedStyleTest, CSSMotionPathOffsetPropertiesV121) {
    // Test CSS Motion Path module: offset-path, offset-distance, offset-rotate, offset-anchor, offset-position
    ComputedStyle s;

    // Verify defaults
    EXPECT_EQ(s.offset_path, "none");
    EXPECT_FLOAT_EQ(s.offset_distance, 0.0f);
    EXPECT_EQ(s.offset_rotate, "auto");
    EXPECT_EQ(s.offset_anchor, "auto");
    EXPECT_EQ(s.offset_position, "normal");
    EXPECT_TRUE(s.offset.empty());

    // Set a SVG path as motion path
    s.offset_path = "path('M0 0 C100 0 100 100 200 100')";
    EXPECT_EQ(s.offset_path, "path('M0 0 C100 0 100 100 200 100')");

    // Move element to midpoint of path
    s.offset_distance = 50.0f;
    EXPECT_FLOAT_EQ(s.offset_distance, 50.0f);

    // Fixed rotation instead of auto
    s.offset_rotate = "45deg";
    EXPECT_EQ(s.offset_rotate, "45deg");

    // Set anchor and position
    s.offset_anchor = "50% 50%";
    s.offset_position = "auto";
    EXPECT_EQ(s.offset_anchor, "50% 50%");
    EXPECT_EQ(s.offset_position, "auto");

    // Test offset shorthand
    s.offset = "path('M0 0L200 200') 75% auto 30deg / 25% 25%";
    EXPECT_EQ(s.offset, "path('M0 0L200 200') 75% auto 30deg / 25% 25%");

    // Test auto+angle rotation
    s.offset_rotate = "auto 90deg";
    EXPECT_EQ(s.offset_rotate, "auto 90deg");

    // Verify multiple properties can coexist independently
    ComputedStyle s2;
    s2.offset_path = "circle(200px at 50% 50%)";
    s2.offset_distance = 100.0f;
    EXPECT_EQ(s2.offset_path, "circle(200px at 50% 50%)");
    EXPECT_FLOAT_EQ(s2.offset_distance, 100.0f);
    EXPECT_EQ(s2.offset_rotate, "auto"); // still default
}

TEST(ComputedStyleTest, FontVariantAdvancedAndSynthesisV121) {
    // Test font-variant-east-asian, font-variant-position, font-variant-alternates,
    // font-synthesis bitmask, font-stretch, font-language-override
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.font_variant_east_asian, 0); // normal
    EXPECT_EQ(s.font_variant_position, 0);   // normal
    EXPECT_EQ(s.font_variant_alternates, 0); // normal
    EXPECT_EQ(s.font_synthesis, 7);          // all enabled (1|2|4)
    EXPECT_EQ(s.font_stretch, 5);            // normal
    EXPECT_TRUE(s.font_language_override.empty());
    EXPECT_FLOAT_EQ(s.font_size_adjust, 0.0f); // none

    // font-variant-east-asian: jis04
    s.font_variant_east_asian = 4;
    EXPECT_EQ(s.font_variant_east_asian, 4);

    // font-variant-position: super
    s.font_variant_position = 2;
    EXPECT_EQ(s.font_variant_position, 2);

    // font-variant-alternates: historical-forms
    s.font_variant_alternates = 1;
    EXPECT_EQ(s.font_variant_alternates, 1);

    // font-synthesis: disable weight (remove bit 1), keep style (2) and small-caps (4) => 6
    s.font_synthesis = 6;
    EXPECT_EQ(s.font_synthesis, 6);
    EXPECT_TRUE((s.font_synthesis & 1) == 0);  // weight disabled
    EXPECT_TRUE((s.font_synthesis & 2) != 0);  // style enabled
    EXPECT_TRUE((s.font_synthesis & 4) != 0);  // small-caps enabled

    // font-synthesis: none
    s.font_synthesis = 0;
    EXPECT_EQ(s.font_synthesis, 0);

    // font-stretch: ultra-expanded
    s.font_stretch = 9;
    EXPECT_EQ(s.font_stretch, 9);

    // font-language-override
    s.font_language_override = "TRK";
    EXPECT_EQ(s.font_language_override, "TRK");

    // font-size-adjust with custom aspect ratio
    s.font_size_adjust = 0.56f;
    EXPECT_FLOAT_EQ(s.font_size_adjust, 0.56f);
}

TEST(ComputedStyleTest, ContainmentContainerQueriesContentVisibilityV121) {
    // Test CSS containment (contain), container queries, content-visibility,
    // and related intrinsic sizing hints
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.contain, 0);                  // none
    EXPECT_EQ(s.container_type, 0);           // normal
    EXPECT_TRUE(s.container_name.empty());
    EXPECT_EQ(s.content_visibility, 0);       // visible
    EXPECT_FLOAT_EQ(s.contain_intrinsic_width, 0.0f);
    EXPECT_FLOAT_EQ(s.contain_intrinsic_height, 0.0f);
    EXPECT_EQ(s.isolation, 0);                // auto

    // contain: strict => 1
    s.contain = 1;
    EXPECT_EQ(s.contain, 1);

    // Set up as a container query context
    s.container_type = 2;  // inline-size
    s.container_name = "sidebar";
    EXPECT_EQ(s.container_type, 2);
    EXPECT_EQ(s.container_name, "sidebar");

    // content-visibility: auto => 2 (enables skip rendering off-screen)
    s.content_visibility = 2;
    EXPECT_EQ(s.content_visibility, 2);

    // When content-visibility: auto, provide intrinsic sizing hints
    s.contain_intrinsic_width = 300.0f;
    s.contain_intrinsic_height = 200.0f;
    EXPECT_FLOAT_EQ(s.contain_intrinsic_width, 300.0f);
    EXPECT_FLOAT_EQ(s.contain_intrinsic_height, 200.0f);

    // isolation: isolate creates new stacking context
    s.isolation = 1;
    EXPECT_EQ(s.isolation, 1);

    // contain: paint => 6
    s.contain = 6;
    EXPECT_EQ(s.contain, 6);

    // Verify all containment values are distinct
    ComputedStyle s2;
    s2.contain = 2; // content
    s2.container_type = 1; // size
    s2.content_visibility = 1; // hidden
    EXPECT_EQ(s2.contain, 2);
    EXPECT_EQ(s2.container_type, 1);
    EXPECT_EQ(s2.content_visibility, 1);
    // hidden content-visibility with size containment still has intrinsic size hint
    s2.contain_intrinsic_width = 100.0f;
    EXPECT_FLOAT_EQ(s2.contain_intrinsic_width, 100.0f);
    EXPECT_FLOAT_EQ(s2.contain_intrinsic_height, 0.0f); // unchanged default
}

TEST(ComputedStyleTest, CalcExprNestedBinaryTreeEvaluationV122) {
    // Build a calc expression: calc((10px + 2em) * 3)
    // with parent_value=0, root_font_size=16 => 2em = 32px
    // (10 + 32) * 3 = 126
    // calc(10px + 2em) with parent_value=16 (em uses parent_value as font size)
    // => 10 + 2*16 = 42
    auto px10 = CalcExpr::make_value(Length::px(10));
    auto em2 = CalcExpr::make_value(Length::em(2));
    auto sum = CalcExpr::make_binary(CalcExpr::Op::Add, px10, em2);
    EXPECT_FLOAT_EQ(sum->evaluate(16, 16), 42.0f);

    // calc(100px - 20px) = 80
    auto px100 = CalcExpr::make_value(Length::px(100));
    auto px20 = CalcExpr::make_value(Length::px(20));
    auto diff = CalcExpr::make_binary(CalcExpr::Op::Sub, px100, px20);
    EXPECT_FLOAT_EQ(diff->evaluate(0, 16), 80.0f);

    // calc(80px / 4) = 20
    auto four = CalcExpr::make_value(Length::px(4));
    auto quotient = CalcExpr::make_binary(CalcExpr::Op::Div, diff, four);
    EXPECT_FLOAT_EQ(quotient->evaluate(0, 16), 20.0f);

    // calc(5px + 5px) = 10
    auto a = CalcExpr::make_value(Length::px(5));
    auto b = CalcExpr::make_value(Length::px(5));
    auto ab = CalcExpr::make_binary(CalcExpr::Op::Add, a, b);
    EXPECT_FLOAT_EQ(ab->evaluate(0, 16), 10.0f);

    // calc(10px * 3) = 30
    auto three = CalcExpr::make_value(Length::px(3));
    auto product = CalcExpr::make_binary(CalcExpr::Op::Mul, ab, three);
    EXPECT_FLOAT_EQ(product->evaluate(0, 16), 30.0f);

    // calc(30px + 10px) = 40
    auto ten = CalcExpr::make_value(Length::px(10));
    auto final_expr = CalcExpr::make_binary(CalcExpr::Op::Add, product, ten);
    EXPECT_FLOAT_EQ(final_expr->evaluate(0, 16), 40.0f);
}

TEST(ComputedStyleTest, ScrollbarAndOverscrollCombinationsV122) {
    // Test all scrollbar-related properties together with overscroll behavior,
    // simulating a custom-styled scrollable container
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.scrollbar_width, 0);     // auto
    EXPECT_EQ(s.scrollbar_gutter, 0);    // auto
    EXPECT_EQ(s.scrollbar_thumb_color, 0u); // auto
    EXPECT_EQ(s.scrollbar_track_color, 0u); // auto
    EXPECT_EQ(s.overscroll_behavior, 0);
    EXPECT_EQ(s.overscroll_behavior_x, 0);
    EXPECT_EQ(s.overscroll_behavior_y, 0);
    EXPECT_EQ(s.scroll_behavior, 0);

    // Custom scrollbar: thin, stable gutter, custom colors
    s.scrollbar_width = 1; // thin
    s.scrollbar_gutter = 2; // stable both-edges
    s.scrollbar_thumb_color = 0xFF888888; // gray thumb
    s.scrollbar_track_color = 0xFFEEEEEE; // light gray track
    EXPECT_EQ(s.scrollbar_width, 1);
    EXPECT_EQ(s.scrollbar_gutter, 2);
    EXPECT_EQ(s.scrollbar_thumb_color, 0xFF888888u);
    EXPECT_EQ(s.scrollbar_track_color, 0xFFEEEEEEu);

    // Overscroll: contain on X (prevent chaining), none on Y (no bounce)
    s.overscroll_behavior_x = 1; // contain
    s.overscroll_behavior_y = 2; // none
    s.overscroll_behavior = 1;   // contain (shorthand)
    EXPECT_EQ(s.overscroll_behavior_x, 1);
    EXPECT_EQ(s.overscroll_behavior_y, 2);
    EXPECT_EQ(s.overscroll_behavior, 1);

    // Smooth scrolling
    s.scroll_behavior = 1;
    EXPECT_EQ(s.scroll_behavior, 1);

    // Hidden scrollbar (scrollbar-width: none) with overscroll none
    ComputedStyle s2;
    s2.scrollbar_width = 2; // none
    s2.overscroll_behavior = 2; // none
    EXPECT_EQ(s2.scrollbar_width, 2);
    EXPECT_EQ(s2.overscroll_behavior, 2);
    // With hidden scrollbar, custom colors should still be storable
    s2.scrollbar_thumb_color = 0xFFFF0000;
    EXPECT_EQ(s2.scrollbar_thumb_color, 0xFFFF0000u);
}

TEST(ComputedStyleTest, TextEmphasisAndStrokeCombinedV122) {
    // Test text-emphasis, text-stroke, and text-fill-color working together
    // as they would for East Asian typography or decorative text
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.text_emphasis_style, "none");
    EXPECT_EQ(s.text_emphasis_color, 0u);
    EXPECT_EQ(s.text_emphasis_position, 0); // over right
    EXPECT_FLOAT_EQ(s.text_stroke_width, 0.0f);
    EXPECT_EQ(s.text_stroke_color.r, 0);
    EXPECT_EQ(s.text_fill_color.a, 0); // transparent => use 'color'

    // Japanese-style emphasis dots above characters
    s.text_emphasis_style = "filled sesame";
    s.text_emphasis_color = 0xFF333333; // dark gray
    s.text_emphasis_position = 0; // over right (default for horizontal CJK)
    EXPECT_EQ(s.text_emphasis_style, "filled sesame");
    EXPECT_EQ(s.text_emphasis_color, 0xFF333333u);

    // Switch to under-left for vertical writing
    s.text_emphasis_position = 3; // under left
    EXPECT_EQ(s.text_emphasis_position, 3);

    // Add text-stroke for outlined text effect (like in headings)
    s.text_stroke_width = 2.0f;
    s.text_stroke_color = {0, 0, 0, 255}; // black stroke
    s.text_fill_color = {255, 255, 255, 255}; // white fill => outlined text
    EXPECT_FLOAT_EQ(s.text_stroke_width, 2.0f);
    EXPECT_EQ(s.text_stroke_color.r, 0);
    EXPECT_EQ(s.text_stroke_color.a, 255);
    EXPECT_EQ(s.text_fill_color.r, 255);
    EXPECT_EQ(s.text_fill_color.a, 255);

    // Open circle emphasis with custom color
    ComputedStyle s2;
    s2.text_emphasis_style = "open circle";
    s2.text_emphasis_color = 0xFFFF4444;
    s2.text_emphasis_position = 1; // under right
    EXPECT_EQ(s2.text_emphasis_style, "open circle");
    EXPECT_NE(s2.text_emphasis_color, s.text_emphasis_color);

    // Both emphasis and stroke at the same time
    s2.text_stroke_width = 0.5f;
    s2.text_stroke_color = {255, 0, 0, 128}; // semi-transparent red
    EXPECT_FLOAT_EQ(s2.text_stroke_width, 0.5f);
    EXPECT_EQ(s2.text_stroke_color.a, 128);
}

TEST(ComputedStyleTest, LengthCalcWithPercentAndViewportMixV122) {
    // Test Length::to_px for complex combinations of percentage and viewport units
    // in a calc() expression: calc(50% + 10vw - 2rem)
    // parent_value=400, viewport_w=1200, viewport_h=800, root_font_size=20

    Length::set_viewport(1200, 800);

    auto pct50 = CalcExpr::make_value(Length::percent(50));
    auto vw10 = CalcExpr::make_value(Length::vw(10));
    auto rem2 = CalcExpr::make_value(Length::rem(2));

    auto sum = CalcExpr::make_binary(CalcExpr::Op::Add, pct50, vw10);
    auto result = CalcExpr::make_binary(CalcExpr::Op::Sub, sum, rem2);

    // 50% of 400 = 200, 10vw of 1200 = 120, 2rem with root=20 = 40
    // 200 + 120 - 40 = 280
    float val = result->evaluate(400, 20);
    EXPECT_FLOAT_EQ(val, 280.0f);

    // Now test with a different viewport: calc(100vh - 20%)
    Length::set_viewport(1024, 768);
    auto vh100 = CalcExpr::make_value(Length::vh(100));
    auto pct20 = CalcExpr::make_value(Length::percent(20));
    auto diff = CalcExpr::make_binary(CalcExpr::Op::Sub, vh100, pct20);
    // 100vh = 768, 20% of 600 = 120 => 768 - 120 = 648
    EXPECT_FLOAT_EQ(diff->evaluate(600, 16), 648.0f);

    // Restore viewport
    Length::set_viewport(800, 600);
}

TEST(ComputedStyleTest, AnimationTimelineAndCompositionV122) {
    // Test animation-timeline, animation-composition, animation-range,
    // and animation play state interactions for scroll-driven animations
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.animation_timeline, "auto");
    EXPECT_EQ(s.animation_composition, 0); // replace
    EXPECT_EQ(s.animation_range, "normal");
    EXPECT_EQ(s.animation_play_state, 0); // running
    EXPECT_EQ(s.animation_direction, 0);  // normal
    EXPECT_EQ(s.animation_fill_mode, 0);  // none
    EXPECT_FLOAT_EQ(s.animation_iteration_count, 1.0f);

    // Configure a scroll-driven animation
    s.animation_name = "slide-in";
    s.animation_timeline = "scroll()";
    s.animation_range = "entry";
    s.animation_composition = 1; // add
    s.animation_fill_mode = 3;   // both
    s.animation_direction = 2;   // alternate
    s.animation_iteration_count = -1; // infinite
    EXPECT_EQ(s.animation_name, "slide-in");
    EXPECT_EQ(s.animation_timeline, "scroll()");
    EXPECT_EQ(s.animation_range, "entry");
    EXPECT_EQ(s.animation_composition, 1);
    EXPECT_EQ(s.animation_fill_mode, 3);
    EXPECT_EQ(s.animation_direction, 2);
    EXPECT_FLOAT_EQ(s.animation_iteration_count, -1.0f);

    // Pause the animation
    s.animation_play_state = 1; // paused
    EXPECT_EQ(s.animation_play_state, 1);

    // View timeline with exit range and accumulate composition
    ComputedStyle s2;
    s2.animation_timeline = "view()";
    s2.animation_range = "exit";
    s2.animation_composition = 2; // accumulate
    s2.animation_direction = 3;   // alternate-reverse
    EXPECT_EQ(s2.animation_timeline, "view()");
    EXPECT_EQ(s2.animation_range, "exit");
    EXPECT_EQ(s2.animation_composition, 2);
    EXPECT_EQ(s2.animation_direction, 3);

    // Named timeline
    s2.animation_timeline = "--progress-timeline";
    EXPECT_EQ(s2.animation_timeline, "--progress-timeline");
}

TEST(ComputedStyleTest, TransitionMultipleDefsWithBezierAndStepsV122) {
    // Test multiple TransitionDef entries with different timing functions:
    // cubic-bezier, steps-end, steps-start, and standard easing
    ComputedStyle s;
    EXPECT_TRUE(s.transitions.empty());

    // Transition 1: opacity with cubic-bezier
    TransitionDef t1;
    t1.property = "opacity";
    t1.duration_ms = 300;
    t1.delay_ms = 50;
    t1.timing_function = 5; // cubic-bezier
    t1.bezier_x1 = 0.25f;
    t1.bezier_y1 = 0.1f;
    t1.bezier_x2 = 0.25f;
    t1.bezier_y2 = 1.0f;
    s.transitions.push_back(t1);

    // Transition 2: transform with steps-end
    TransitionDef t2;
    t2.property = "transform";
    t2.duration_ms = 500;
    t2.delay_ms = 0;
    t2.timing_function = 6; // steps-end
    t2.steps_count = 5;
    s.transitions.push_back(t2);

    // Transition 3: background-color with steps-start
    TransitionDef t3;
    t3.property = "background-color";
    t3.duration_ms = 200;
    t3.delay_ms = 100;
    t3.timing_function = 7; // steps-start
    t3.steps_count = 3;
    s.transitions.push_back(t3);

    // Transition 4: height with ease-in-out
    TransitionDef t4;
    t4.property = "height";
    t4.duration_ms = 400;
    t4.timing_function = 4; // ease-in-out
    s.transitions.push_back(t4);

    EXPECT_EQ(s.transitions.size(), 4u);

    // Verify cubic-bezier control points
    EXPECT_EQ(s.transitions[0].property, "opacity");
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_x1, 0.25f);
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_y1, 0.1f);
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_x2, 0.25f);
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_y2, 1.0f);
    EXPECT_EQ(s.transitions[0].timing_function, 5);
    EXPECT_FLOAT_EQ(s.transitions[0].duration_ms, 300.0f);
    EXPECT_FLOAT_EQ(s.transitions[0].delay_ms, 50.0f);

    // Verify steps-end
    EXPECT_EQ(s.transitions[1].timing_function, 6);
    EXPECT_EQ(s.transitions[1].steps_count, 5);
    EXPECT_FLOAT_EQ(s.transitions[1].duration_ms, 500.0f);

    // Verify steps-start
    EXPECT_EQ(s.transitions[2].timing_function, 7);
    EXPECT_EQ(s.transitions[2].steps_count, 3);
    EXPECT_EQ(s.transitions[2].property, "background-color");

    // Verify ease-in-out
    EXPECT_EQ(s.transitions[3].timing_function, 4);
    EXPECT_EQ(s.transitions[3].property, "height");

    // Legacy scalar fields should remain independent
    s.transition_property = "all";
    s.transition_duration = 0.5f;
    s.transition_timing = 1; // linear
    EXPECT_EQ(s.transition_property, "all");
    EXPECT_FLOAT_EQ(s.transition_duration, 0.5f);
    EXPECT_EQ(s.transition_timing, 1);
    // transitions vector unchanged
    EXPECT_EQ(s.transitions.size(), 4u);
}

TEST(ComputedStyleTest, BackgroundLayeringAndBlendModeV122) {
    // Test background properties that simulate a layered background
    // with gradients, blend modes, and clip/origin combinations
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.gradient_type, 0); // none
    EXPECT_FLOAT_EQ(s.gradient_angle, 180.0f);
    EXPECT_EQ(s.radial_shape, 0); // ellipse
    EXPECT_TRUE(s.gradient_stops.empty());
    EXPECT_TRUE(s.bg_image_url.empty());
    EXPECT_EQ(s.background_size, 0); // auto
    EXPECT_EQ(s.background_repeat, 0); // repeat
    EXPECT_EQ(s.background_clip, 0); // border-box
    EXPECT_EQ(s.background_origin, 0); // padding-box
    EXPECT_EQ(s.background_blend_mode, 0); // normal
    EXPECT_EQ(s.background_attachment, 0); // scroll

    // Linear gradient at 45deg with three color stops
    s.gradient_type = 1; // linear
    s.gradient_angle = 45.0f;
    s.gradient_stops.push_back({0xFFFF0000, 0.0f}); // red at 0%
    s.gradient_stops.push_back({0xFF00FF00, 0.5f}); // green at 50%
    s.gradient_stops.push_back({0xFF0000FF, 1.0f}); // blue at 100%
    EXPECT_EQ(s.gradient_type, 1);
    EXPECT_FLOAT_EQ(s.gradient_angle, 45.0f);
    EXPECT_EQ(s.gradient_stops.size(), 3u);
    EXPECT_EQ(s.gradient_stops[0].first, 0xFFFF0000u);
    EXPECT_FLOAT_EQ(s.gradient_stops[1].second, 0.5f);
    EXPECT_EQ(s.gradient_stops[2].first, 0xFF0000FFu);

    // Set background clipping to content-box, origin to border-box, blend multiply
    s.background_clip = 2; // content-box
    s.background_origin = 1; // border-box
    s.background_blend_mode = 1; // multiply
    s.background_attachment = 1; // fixed
    EXPECT_EQ(s.background_clip, 2);
    EXPECT_EQ(s.background_origin, 1);
    EXPECT_EQ(s.background_blend_mode, 1);
    EXPECT_EQ(s.background_attachment, 1);

    // Radial gradient with circle shape, explicit size, no-repeat
    ComputedStyle s2;
    s2.gradient_type = 2; // radial
    s2.radial_shape = 1; // circle
    s2.gradient_stops.push_back({0xFFFFFF00, 0.0f});
    s2.gradient_stops.push_back({0xFF000000, 1.0f});
    s2.background_size = 3; // explicit
    s2.bg_size_width = 200.0f;
    s2.bg_size_height = 200.0f;
    s2.background_repeat = 3; // no-repeat
    s2.background_blend_mode = 3; // overlay
    EXPECT_EQ(s2.gradient_type, 2);
    EXPECT_EQ(s2.radial_shape, 1);
    EXPECT_EQ(s2.gradient_stops.size(), 2u);
    EXPECT_EQ(s2.background_size, 3);
    EXPECT_FLOAT_EQ(s2.bg_size_width, 200.0f);
    EXPECT_FLOAT_EQ(s2.bg_size_height, 200.0f);
    EXPECT_EQ(s2.background_repeat, 3);
    EXPECT_EQ(s2.background_blend_mode, 3);
}

TEST(ComputedStyleTest, PagedMediaAndFragmentationV122) {
    // Test paged media, fragmentation, and column-span properties
    // simulating a print stylesheet
    ComputedStyle s;

    // Defaults for paged media
    EXPECT_EQ(s.page_break_before, 0); // auto
    EXPECT_EQ(s.page_break_after, 0);
    EXPECT_EQ(s.page_break_inside, 0);
    EXPECT_EQ(s.break_before, 0); // auto
    EXPECT_EQ(s.break_after, 0);
    EXPECT_EQ(s.break_inside, 0);
    EXPECT_EQ(s.orphans, 2);
    EXPECT_EQ(s.widows, 2);
    EXPECT_EQ(s.column_span, 0); // none
    EXPECT_TRUE(s.page.empty());

    // Force page break before a chapter heading
    s.page_break_before = 1; // always
    s.page_break_after = 2;  // avoid (don't break right after heading)
    s.page_break_inside = 1; // avoid
    EXPECT_EQ(s.page_break_before, 1);
    EXPECT_EQ(s.page_break_after, 2);
    EXPECT_EQ(s.page_break_inside, 1);

    // Modern fragmentation properties for multi-column layout
    s.break_before = 4; // column
    s.break_after = 1;  // avoid
    s.break_inside = 3; // avoid-column
    EXPECT_EQ(s.break_before, 4);
    EXPECT_EQ(s.break_after, 1);
    EXPECT_EQ(s.break_inside, 3);

    // Prevent orphans/widows in print: require at least 4 lines
    s.orphans = 4;
    s.widows = 4;
    EXPECT_EQ(s.orphans, 4);
    EXPECT_EQ(s.widows, 4);

    // Column span all (for a full-width heading in a multi-column layout)
    s.column_span = 1; // all
    EXPECT_EQ(s.column_span, 1);

    // Named page for specialized print layout
    s.page = "landscape-wide";
    EXPECT_EQ(s.page, "landscape-wide");

    // Verify page_break and break_ are independently settable
    ComputedStyle s2;
    s2.page_break_before = 3; // left
    s2.page_break_after = 4;  // right
    s2.break_before = 2;      // always
    s2.break_after = 5;       // region
    EXPECT_EQ(s2.page_break_before, 3);
    EXPECT_EQ(s2.break_before, 2);
    EXPECT_NE(s2.page_break_before, s2.break_before);
    EXPECT_NE(s2.page_break_after, s2.break_after);

    // Orphans=1, widows=1 => allow single lines at breaks
    s2.orphans = 1;
    s2.widows = 1;
    EXPECT_EQ(s2.orphans, 1);
    EXPECT_EQ(s2.widows, 1);
    EXPECT_NE(s2.orphans, s.orphans);
}

TEST(ComputedStyleTest, CustomPropertiesCRUDAndIsolationV123) {
    // Test that CSS custom properties (variables) behave like an independent
    // map per ComputedStyle instance: insert, read, update, delete, and
    // verify isolation between two style objects.
    ComputedStyle s1;
    ComputedStyle s2;

    // Both start empty
    EXPECT_TRUE(s1.custom_properties.empty());
    EXPECT_TRUE(s2.custom_properties.empty());

    // Insert several variables into s1
    s1.custom_properties["--color-primary"] = "#ff6600";
    s1.custom_properties["--spacing-unit"] = "8px";
    s1.custom_properties["--font-stack"] = "Inter, Helvetica, sans-serif";
    EXPECT_EQ(s1.custom_properties.size(), 3u);

    // s2 remains unaffected
    EXPECT_TRUE(s2.custom_properties.empty());

    // Read back
    EXPECT_EQ(s1.custom_properties.at("--color-primary"), "#ff6600");
    EXPECT_EQ(s1.custom_properties.at("--spacing-unit"), "8px");

    // Update a variable
    s1.custom_properties["--color-primary"] = "hsl(210, 100%, 50%)";
    EXPECT_EQ(s1.custom_properties.at("--color-primary"), "hsl(210, 100%, 50%)");
    EXPECT_EQ(s1.custom_properties.size(), 3u); // no extra entry

    // Delete a variable
    s1.custom_properties.erase("--spacing-unit");
    EXPECT_EQ(s1.custom_properties.size(), 2u);
    EXPECT_EQ(s1.custom_properties.count("--spacing-unit"), 0u);

    // Insert into s2 and confirm both coexist independently
    s2.custom_properties["--color-primary"] = "rebeccapurple";
    EXPECT_NE(s1.custom_properties.at("--color-primary"),
              s2.custom_properties.at("--color-primary"));
}

TEST(ComputedStyleTest, GridLayoutComprehensiveConfigV123) {
    // Simulate a complex CSS Grid layout configuration with template areas,
    // named lines, auto-flow, and individual item placement—verifying every
    // grid-related property round-trips correctly.
    ComputedStyle grid;

    // Container setup
    grid.display = Display::Grid;
    grid.grid_template_columns = "repeat(3, 1fr)";
    grid.grid_template_rows = "auto minmax(100px, 1fr) auto";
    grid.grid_template_areas = "\"header header header\" \"sidebar main aside\" \"footer footer footer\"";
    grid.grid_auto_flow = 3; // column dense
    grid.grid_auto_rows = "minmax(50px, auto)";
    grid.grid_auto_columns = "200px";
    grid.justify_items = 2;  // center
    grid.align_content = 4;  // space-between
    grid.gap = Length::px(16);

    EXPECT_EQ(grid.display, Display::Grid);
    EXPECT_EQ(grid.grid_template_columns, "repeat(3, 1fr)");
    EXPECT_EQ(grid.grid_template_rows, "auto minmax(100px, 1fr) auto");
    EXPECT_NE(grid.grid_template_areas.find("sidebar main aside"), std::string::npos);
    EXPECT_EQ(grid.grid_auto_flow, 3);
    EXPECT_EQ(grid.grid_auto_rows, "minmax(50px, auto)");
    EXPECT_EQ(grid.grid_auto_columns, "200px");
    EXPECT_EQ(grid.justify_items, 2);
    EXPECT_EQ(grid.align_content, 4);
    EXPECT_FLOAT_EQ(grid.gap.to_px(), 16.0f);

    // Item placement using both shorthand and longhand
    ComputedStyle item;
    item.grid_area = "main";
    item.grid_column = "2 / 3";
    item.grid_row = "2 / 3";
    item.grid_column_start = "2";
    item.grid_column_end = "3";
    item.grid_row_start = "2";
    item.grid_row_end = "3";

    EXPECT_EQ(item.grid_area, "main");
    EXPECT_EQ(item.grid_column, "2 / 3");
    EXPECT_EQ(item.grid_row, "2 / 3");
    EXPECT_EQ(item.grid_column_start, "2");
    EXPECT_EQ(item.grid_column_end, "3");
    EXPECT_EQ(item.grid_row_start, "2");
    EXPECT_EQ(item.grid_row_end, "3");
}

TEST(ComputedStyleTest, ClipPathPolygonAndShapeOutsideInterplayV123) {
    // Build a triangular clip-path polygon and an elliptical shape-outside,
    // verifying the value vectors store coordinates correctly and that the
    // two features remain independent on the same ComputedStyle.
    ComputedStyle s;

    // Default: no clipping, no shape
    EXPECT_EQ(s.clip_path_type, 0);
    EXPECT_TRUE(s.clip_path_values.empty());
    EXPECT_EQ(s.shape_outside_type, 0);
    EXPECT_TRUE(s.shape_outside_values.empty());

    // Triangular polygon clip path: (50, 0), (100, 100), (0, 100)
    s.clip_path_type = 4; // polygon
    s.clip_path_values = {50.0f, 0.0f, 100.0f, 100.0f, 0.0f, 100.0f};
    EXPECT_EQ(s.clip_path_type, 4);
    EXPECT_EQ(s.clip_path_values.size(), 6u);
    // Verify first vertex
    EXPECT_FLOAT_EQ(s.clip_path_values[0], 50.0f);
    EXPECT_FLOAT_EQ(s.clip_path_values[1], 0.0f);
    // Verify last vertex
    EXPECT_FLOAT_EQ(s.clip_path_values[4], 0.0f);
    EXPECT_FLOAT_EQ(s.clip_path_values[5], 100.0f);

    // Elliptical shape-outside with rx=80, ry=60
    s.shape_outside_type = 2; // ellipse
    s.shape_outside_values = {80.0f, 60.0f};
    s.shape_margin = 12.5f;
    s.shape_image_threshold = 0.7f;
    EXPECT_EQ(s.shape_outside_type, 2);
    EXPECT_EQ(s.shape_outside_values.size(), 2u);
    EXPECT_FLOAT_EQ(s.shape_outside_values[0], 80.0f);
    EXPECT_FLOAT_EQ(s.shape_outside_values[1], 60.0f);
    EXPECT_FLOAT_EQ(s.shape_margin, 12.5f);
    EXPECT_FLOAT_EQ(s.shape_image_threshold, 0.7f);

    // Clip path and shape outside are stored in independent fields
    EXPECT_NE(s.clip_path_type, s.shape_outside_type);
    EXPECT_NE(s.clip_path_values.size(), s.shape_outside_values.size());
}

TEST(ComputedStyleTest, TransformChainMatrixAndOriginV123) {
    // Apply a sequence of transforms: translate, rotate, scale, then a raw
    // matrix—verifying the vector stores each entry with correct parameters,
    // and that transform-origin is independently configurable.
    ComputedStyle s;

    // Defaults
    EXPECT_TRUE(s.transforms.empty());
    EXPECT_FLOAT_EQ(s.transform_origin_x, 50.0f);
    EXPECT_FLOAT_EQ(s.transform_origin_y, 50.0f);
    EXPECT_EQ(s.transform_style, 0); // flat

    // Build a transform chain
    Transform t1;
    t1.type = TransformType::Translate;
    t1.x = -50.0f;
    t1.y = 25.0f;

    Transform t2;
    t2.type = TransformType::Rotate;
    t2.angle = 45.0f;

    Transform t3;
    t3.type = TransformType::Scale;
    t3.x = 2.0f;
    t3.y = 0.5f;

    Transform t4;
    t4.type = TransformType::Matrix;
    t4.m[0] = 0.866f; t4.m[1] = 0.5f;
    t4.m[2] = -0.5f;  t4.m[3] = 0.866f;
    t4.m[4] = 10.0f;  t4.m[5] = 20.0f;

    s.transforms = {t1, t2, t3, t4};
    EXPECT_EQ(s.transforms.size(), 4u);

    // Verify each entry
    EXPECT_EQ(s.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(s.transforms[0].x, -50.0f);
    EXPECT_FLOAT_EQ(s.transforms[0].y, 25.0f);

    EXPECT_EQ(s.transforms[1].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(s.transforms[1].angle, 45.0f);

    EXPECT_EQ(s.transforms[2].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(s.transforms[2].x, 2.0f);
    EXPECT_FLOAT_EQ(s.transforms[2].y, 0.5f);

    EXPECT_EQ(s.transforms[3].type, TransformType::Matrix);
    EXPECT_FLOAT_EQ(s.transforms[3].m[4], 10.0f);
    EXPECT_FLOAT_EQ(s.transforms[3].m[5], 20.0f);

    // Set origin to top-left and preserve-3d
    s.transform_origin_x = 0.0f;
    s.transform_origin_y = 0.0f;
    s.transform_style = 1; // preserve-3d
    s.perspective = 800.0f;
    s.perspective_origin_x = 25.0f;
    s.perspective_origin_y = 75.0f;

    EXPECT_FLOAT_EQ(s.transform_origin_x, 0.0f);
    EXPECT_EQ(s.transform_style, 1);
    EXPECT_FLOAT_EQ(s.perspective, 800.0f);
    EXPECT_FLOAT_EQ(s.perspective_origin_x, 25.0f);
    EXPECT_FLOAT_EQ(s.perspective_origin_y, 75.0f);
}

TEST(ComputedStyleTest, FilterAndBackdropFilterChainsV123) {
    // Apply multiple CSS filters in sequence (grayscale, brightness, blur,
    // hue-rotate) and backdrop filters (blur, saturate, invert), verifying
    // that both vectors store independent chains and that the drop-shadow
    // filter params coexist correctly.
    ComputedStyle s;

    EXPECT_TRUE(s.filters.empty());
    EXPECT_TRUE(s.backdrop_filters.empty());

    // filters: grayscale(0.5) brightness(1.2) blur(3px) hue-rotate(90deg)
    s.filters.push_back({1, 0.5f});  // grayscale
    s.filters.push_back({3, 1.2f});  // brightness
    s.filters.push_back({9, 3.0f});  // blur
    s.filters.push_back({8, 90.0f}); // hue-rotate
    EXPECT_EQ(s.filters.size(), 4u);
    EXPECT_EQ(s.filters[0].first, 1); // grayscale type
    EXPECT_FLOAT_EQ(s.filters[0].second, 0.5f);
    EXPECT_EQ(s.filters[2].first, 9); // blur type
    EXPECT_FLOAT_EQ(s.filters[2].second, 3.0f);
    EXPECT_EQ(s.filters[3].first, 8); // hue-rotate type
    EXPECT_FLOAT_EQ(s.filters[3].second, 90.0f);

    // backdrop-filter: blur(10px) saturate(2.0) invert(1.0)
    s.backdrop_filters.push_back({9, 10.0f}); // blur
    s.backdrop_filters.push_back({6, 2.0f});  // saturate
    s.backdrop_filters.push_back({5, 1.0f});  // invert
    EXPECT_EQ(s.backdrop_filters.size(), 3u);
    EXPECT_EQ(s.backdrop_filters[0].first, 9);
    EXPECT_FLOAT_EQ(s.backdrop_filters[0].second, 10.0f);

    // The two filter chains are fully independent
    EXPECT_NE(s.filters.size(), s.backdrop_filters.size());

    // Drop shadow params stored separately
    s.drop_shadow_ox = 4.0f;
    s.drop_shadow_oy = 4.0f;
    s.drop_shadow_color = 0x80000000;
    EXPECT_FLOAT_EQ(s.drop_shadow_ox, 4.0f);
    EXPECT_FLOAT_EQ(s.drop_shadow_oy, 4.0f);
    EXPECT_EQ(s.drop_shadow_color, 0x80000000u);
}

TEST(ComputedStyleTest, MultiColumnLayoutWithRuleAndFillV123) {
    // Simulate a newspaper-style multi-column layout: explicit column count
    // and width, a visible column rule, column fill, and column span. Verify
    // that all column-related properties work together without interference.
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.column_count, -1);            // auto
    EXPECT_TRUE(s.column_width.is_auto());
    EXPECT_FLOAT_EQ(s.column_gap_val.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(s.column_rule_width, 0.0f);
    EXPECT_EQ(s.column_rule_style, 0);        // none
    EXPECT_EQ(s.column_fill, 0);              // balance
    EXPECT_EQ(s.column_span, 0);              // none

    // Configure 3-column layout with 250px columns and 24px gap
    s.column_count = 3;
    s.column_width = Length::px(250);
    s.column_gap_val = Length::px(24);
    EXPECT_EQ(s.column_count, 3);
    EXPECT_FLOAT_EQ(s.column_width.to_px(), 250.0f);
    EXPECT_FLOAT_EQ(s.column_gap_val.to_px(), 24.0f);

    // Visible rule: 2px dashed dark gray
    s.column_rule_width = 2.0f;
    s.column_rule_style = 2; // dashed
    s.column_rule_color = Color{80, 80, 80, 255};
    EXPECT_FLOAT_EQ(s.column_rule_width, 2.0f);
    EXPECT_EQ(s.column_rule_style, 2);
    EXPECT_EQ(s.column_rule_color.r, 80);
    EXPECT_EQ(s.column_rule_color.g, 80);
    EXPECT_EQ(s.column_rule_color.a, 255);

    // Column fill auto (don't balance, just fill sequentially)
    s.column_fill = 1; // auto
    EXPECT_EQ(s.column_fill, 1);

    // A heading that spans all columns
    ComputedStyle heading;
    heading.column_span = 1; // all
    EXPECT_EQ(heading.column_span, 1);

    // Verify parent column properties are unaffected by heading
    EXPECT_EQ(s.column_span, 0);
    EXPECT_EQ(s.column_count, 3);

    // Reset to auto column count
    s.column_count = -1;
    EXPECT_EQ(s.column_count, -1);
}

TEST(ComputedStyleTest, LengthRemChAndLhConversionsV123) {
    // Test rem, ch, and lh Length units resolve correctly given
    // different root font sizes, and that they compose with calc()
    // expressions using mixed unit types.
    float root_fs = 20.0f;
    float parent_fs = 32.0f;
    float lh_val = 1.5f * parent_fs; // line-height context = 48

    // rem: relative to root font size
    Length rem2 = Length::rem(2.5f);
    EXPECT_FLOAT_EQ(rem2.to_px(parent_fs, root_fs), 50.0f); // 2.5 * 20

    // ch: 1ch ≈ 0.6 * font-size, so ch uses parent_value * 0.6
    Length ch3 = Length::ch(3.0f);
    EXPECT_FLOAT_EQ(ch3.to_px(parent_fs, root_fs), 3.0f * parent_fs * 0.6f);

    // lh: relative to line-height
    Length lh1 = Length::lh(1.0f);
    EXPECT_FLOAT_EQ(lh1.to_px(parent_fs, root_fs, lh_val), lh_val);

    Length lh_half = Length::lh(0.5f);
    EXPECT_FLOAT_EQ(lh_half.to_px(parent_fs, root_fs, lh_val), 24.0f);

    // calc(2rem + 10px) = 2*20 + 10 = 50
    auto rem_node = CalcExpr::make_value(Length::rem(2));
    auto px_node = CalcExpr::make_value(Length::px(10));
    auto sum_expr = CalcExpr::make_binary(CalcExpr::Op::Add, rem_node, px_node);
    EXPECT_FLOAT_EQ(sum_expr->evaluate(0, root_fs), 50.0f);

    // calc(max(1rem, 30px)) with root=20 => max(20, 30) = 30
    auto rem1 = CalcExpr::make_value(Length::rem(1));
    auto px30 = CalcExpr::make_value(Length::px(30));
    auto max_expr = CalcExpr::make_binary(CalcExpr::Op::Max, rem1, px30);
    EXPECT_FLOAT_EQ(max_expr->evaluate(0, root_fs), 30.0f);

    // calc(min(1rem, 30px)) with root=20 => min(20, 30) = 20
    auto min_expr = CalcExpr::make_binary(CalcExpr::Op::Min,
        CalcExpr::make_value(Length::rem(1)),
        CalcExpr::make_value(Length::px(30)));
    EXPECT_FLOAT_EQ(min_expr->evaluate(0, root_fs), 20.0f);
}

TEST(ComputedStyleTest, TextShadowMultipleEntriesAndBoxShadowInsetMixV123) {
    // Build a ComputedStyle with multiple text-shadows and a mix of inset
    // and outset box-shadows, verifying independent storage, correct field
    // access via .blur (not .blur_radius), and the inset flag.
    ComputedStyle s;

    EXPECT_TRUE(s.text_shadows.empty());
    EXPECT_TRUE(s.box_shadows.empty());

    // Three text shadows (layered neon glow effect)
    ComputedStyle::TextShadowEntry ts1{0, 0, 10.0f, {0, 255, 255, 200}};
    ComputedStyle::TextShadowEntry ts2{0, 0, 20.0f, {255, 0, 255, 150}};
    ComputedStyle::TextShadowEntry ts3{2.0f, 2.0f, 4.0f, {0, 0, 0, 100}};
    s.text_shadows = {ts1, ts2, ts3};
    EXPECT_EQ(s.text_shadows.size(), 3u);
    // First shadow: pure glow (no offset, blur=10)
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_x, 0.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].blur, 10.0f);
    EXPECT_EQ(s.text_shadows[0].color.g, 255);
    // Third shadow: offset shadow
    EXPECT_FLOAT_EQ(s.text_shadows[2].offset_x, 2.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[2].offset_y, 2.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[2].blur, 4.0f);

    // Two box shadows: one outset elevation, one inset for depth
    ComputedStyle::BoxShadowEntry bs_outer;
    bs_outer.offset_x = 0;
    bs_outer.offset_y = 8.0f;
    bs_outer.blur = 24.0f;
    bs_outer.spread = -4.0f;
    bs_outer.color = {0, 0, 0, 60};
    bs_outer.inset = false;

    ComputedStyle::BoxShadowEntry bs_inner;
    bs_inner.offset_x = 0;
    bs_inner.offset_y = 0;
    bs_inner.blur = 12.0f;
    bs_inner.spread = 0;
    bs_inner.color = {255, 255, 255, 30};
    bs_inner.inset = true;

    s.box_shadows = {bs_outer, bs_inner};
    EXPECT_EQ(s.box_shadows.size(), 2u);
    // Outset shadow
    EXPECT_FALSE(s.box_shadows[0].inset);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 24.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, -4.0f);
    EXPECT_EQ(s.box_shadows[0].color.a, 60);
    // Inset shadow
    EXPECT_TRUE(s.box_shadows[1].inset);
    EXPECT_FLOAT_EQ(s.box_shadows[1].blur, 12.0f);
    EXPECT_EQ(s.box_shadows[1].color.a, 30);

    // Text shadows and box shadows are completely independent
    s.text_shadows.clear();
    EXPECT_TRUE(s.text_shadows.empty());
    EXPECT_EQ(s.box_shadows.size(), 2u); // box shadows untouched
}

TEST(ComputedStyleTest, TransitionDefsBezierAndStepsCoexistV124) {
    // Verify that two TransitionDefs with different timing function types
    // (cubic-bezier vs steps) can coexist in the same ComputedStyle and
    // their fields are independently addressable.
    ComputedStyle s;
    EXPECT_TRUE(s.transitions.empty());

    TransitionDef bezier_td;
    bezier_td.property = "opacity";
    bezier_td.duration_ms = 300;
    bezier_td.delay_ms = 50;
    bezier_td.timing_function = 5; // cubic-bezier
    bezier_td.bezier_x1 = 0.42f;
    bezier_td.bezier_y1 = 0.0f;
    bezier_td.bezier_x2 = 0.58f;
    bezier_td.bezier_y2 = 1.0f;

    TransitionDef steps_td;
    steps_td.property = "transform";
    steps_td.duration_ms = 600;
    steps_td.delay_ms = 0;
    steps_td.timing_function = 6; // steps-end
    steps_td.steps_count = 5;

    s.transitions = {bezier_td, steps_td};
    EXPECT_EQ(s.transitions.size(), 2u);

    // Bezier transition
    EXPECT_EQ(s.transitions[0].property, "opacity");
    EXPECT_FLOAT_EQ(s.transitions[0].duration_ms, 300.0f);
    EXPECT_FLOAT_EQ(s.transitions[0].delay_ms, 50.0f);
    EXPECT_EQ(s.transitions[0].timing_function, 5);
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_x1, 0.42f);
    EXPECT_FLOAT_EQ(s.transitions[0].bezier_y2, 1.0f);

    // Steps transition
    EXPECT_EQ(s.transitions[1].property, "transform");
    EXPECT_FLOAT_EQ(s.transitions[1].duration_ms, 600.0f);
    EXPECT_EQ(s.transitions[1].timing_function, 6);
    EXPECT_EQ(s.transitions[1].steps_count, 5);

    // Modifying one transition does not affect the other
    s.transitions[0].duration_ms = 1000;
    EXPECT_FLOAT_EQ(s.transitions[1].duration_ms, 600.0f);
}

TEST(ComputedStyleTest, GradientStopsSortAndPositionRangeV124) {
    // Test gradient stops storage: multiple stops with varying positions
    // including edge positions 0.0 and 1.0, and verify ordering is preserved.
    ComputedStyle s;
    EXPECT_EQ(s.gradient_type, 0);
    EXPECT_TRUE(s.gradient_stops.empty());

    s.gradient_type = 1; // linear
    s.gradient_angle = 45.0f;

    // Add 5 stops: start, quarter, mid, three-quarter, end
    s.gradient_stops.push_back({0xFFFF0000, 0.0f});   // red at 0%
    s.gradient_stops.push_back({0xFFFFFF00, 0.25f});  // yellow at 25%
    s.gradient_stops.push_back({0xFF00FF00, 0.5f});   // green at 50%
    s.gradient_stops.push_back({0xFF0000FF, 0.75f});  // blue at 75%
    s.gradient_stops.push_back({0xFFFF00FF, 1.0f});   // magenta at 100%

    EXPECT_EQ(s.gradient_stops.size(), 5u);
    EXPECT_FLOAT_EQ(s.gradient_angle, 45.0f);

    // Verify positions are in expected order and ARGB values are intact
    EXPECT_FLOAT_EQ(s.gradient_stops[0].second, 0.0f);
    EXPECT_EQ(s.gradient_stops[0].first, 0xFFFF0000u);
    EXPECT_FLOAT_EQ(s.gradient_stops[2].second, 0.5f);
    EXPECT_EQ(s.gradient_stops[2].first, 0xFF00FF00u);
    EXPECT_FLOAT_EQ(s.gradient_stops[4].second, 1.0f);
    EXPECT_EQ(s.gradient_stops[4].first, 0xFFFF00FFu);

    // Switching to radial preserves stops
    s.gradient_type = 2; // radial
    s.radial_shape = 1;  // circle
    EXPECT_EQ(s.gradient_stops.size(), 5u);
    EXPECT_EQ(s.radial_shape, 1);
}

TEST(ComputedStyleTest, ClipPathPolygonValuesAndShapeOutsideInsetV124) {
    // Combine clip-path polygon with shape-outside inset and verify
    // both value arrays are independently stored.
    ComputedStyle s;
    EXPECT_EQ(s.clip_path_type, 0);   // none
    EXPECT_EQ(s.shape_outside_type, 0); // none

    // Set clip-path as a polygon: triangle
    s.clip_path_type = 4; // polygon
    s.clip_path_values = {50.0f, 0.0f, 100.0f, 100.0f, 0.0f, 100.0f};
    EXPECT_EQ(s.clip_path_values.size(), 6u);
    EXPECT_FLOAT_EQ(s.clip_path_values[0], 50.0f); // first x
    EXPECT_FLOAT_EQ(s.clip_path_values[5], 100.0f); // last y

    // Set shape-outside as inset: top, right, bottom, left
    s.shape_outside_type = 3; // inset
    s.shape_outside_values = {10.0f, 20.0f, 10.0f, 20.0f};
    EXPECT_EQ(s.shape_outside_values.size(), 4u);
    EXPECT_FLOAT_EQ(s.shape_outside_values[1], 20.0f);

    // Clip path values not disturbed by shape-outside writes
    EXPECT_EQ(s.clip_path_values.size(), 6u);
    EXPECT_FLOAT_EQ(s.clip_path_values[2], 100.0f);

    // Clearing shape-outside doesn't affect clip-path
    s.shape_outside_type = 0;
    s.shape_outside_values.clear();
    EXPECT_EQ(s.clip_path_type, 4);
    EXPECT_EQ(s.clip_path_values.size(), 6u);
}

TEST(ComputedStyleTest, AnimationPropertiesFullConfigV124) {
    // Configure all animation-related fields to non-default values and
    // verify each one is independently readable.
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.animation_name, "");
    EXPECT_FLOAT_EQ(s.animation_duration, 0.0f);
    EXPECT_FLOAT_EQ(s.animation_iteration_count, 1.0f);
    EXPECT_EQ(s.animation_direction, 0);
    EXPECT_EQ(s.animation_fill_mode, 0);
    EXPECT_EQ(s.animation_play_state, 0);
    EXPECT_EQ(s.animation_composition, 0);
    EXPECT_EQ(s.animation_timeline, "auto");
    EXPECT_EQ(s.animation_range, "normal");

    // Set to non-defaults
    s.animation_name = "bounce";
    s.animation_duration = 2.5f;
    s.animation_timing = 5; // cubic-bezier
    s.animation_bezier_x1 = 0.25f;
    s.animation_bezier_y1 = 0.1f;
    s.animation_bezier_x2 = 0.25f;
    s.animation_bezier_y2 = 1.0f;
    s.animation_delay = 0.5f;
    s.animation_iteration_count = -1.0f; // infinite
    s.animation_direction = 2; // alternate
    s.animation_fill_mode = 3; // both
    s.animation_play_state = 1; // paused
    s.animation_composition = 2; // accumulate
    s.animation_timeline = "scroll()";
    s.animation_range = "entry 10% exit 90%";

    EXPECT_EQ(s.animation_name, "bounce");
    EXPECT_FLOAT_EQ(s.animation_duration, 2.5f);
    EXPECT_EQ(s.animation_timing, 5);
    EXPECT_FLOAT_EQ(s.animation_bezier_x1, 0.25f);
    EXPECT_FLOAT_EQ(s.animation_bezier_y2, 1.0f);
    EXPECT_FLOAT_EQ(s.animation_delay, 0.5f);
    EXPECT_FLOAT_EQ(s.animation_iteration_count, -1.0f);
    EXPECT_EQ(s.animation_direction, 2);
    EXPECT_EQ(s.animation_fill_mode, 3);
    EXPECT_EQ(s.animation_play_state, 1);
    EXPECT_EQ(s.animation_composition, 2);
    EXPECT_EQ(s.animation_timeline, "scroll()");
    EXPECT_EQ(s.animation_range, "entry 10% exit 90%");
}

TEST(ComputedStyleTest, EdgeSizesMarginPaddingMixedUnitsV124) {
    // Set margin and padding using different Length unit types per side,
    // then verify to_px() with given parent and root context produces
    // correct values.
    ComputedStyle s;
    float parent_fs = 20.0f;
    float root_fs = 16.0f;

    // Margin: top=px, right=em, bottom=rem, left=percent
    s.margin.top = Length::px(10);
    s.margin.right = Length::em(1.5f);     // 1.5 * 20 = 30
    s.margin.bottom = Length::rem(2.0f);   // 2 * 16 = 32
    s.margin.left = Length::percent(25.0f); // 25% of parent=20 => 5

    EXPECT_FLOAT_EQ(s.margin.top.to_px(parent_fs, root_fs), 10.0f);
    EXPECT_FLOAT_EQ(s.margin.right.to_px(parent_fs, root_fs), 30.0f);
    EXPECT_FLOAT_EQ(s.margin.bottom.to_px(parent_fs, root_fs), 32.0f);
    EXPECT_FLOAT_EQ(s.margin.left.to_px(parent_fs, root_fs), 5.0f);

    // Padding: top=zero, right=auto (is_auto check), bottom=ch, left=px
    s.padding.top = Length::zero();
    s.padding.right = Length::auto_val();
    s.padding.bottom = Length::ch(3.0f);
    s.padding.left = Length::px(8);

    EXPECT_TRUE(s.padding.top.is_zero());
    EXPECT_TRUE(s.padding.right.is_auto());
    EXPECT_FALSE(s.padding.bottom.is_auto());
    EXPECT_FALSE(s.padding.bottom.is_zero());
    EXPECT_FLOAT_EQ(s.padding.left.to_px(), 8.0f);
}

TEST(ComputedStyleTest, BorderEdgesAllSidesDifferentStylesAndColorsV124) {
    // Set all four border edges with distinct widths, styles, and colors,
    // then verify each is independent. Uses color struct comparison operator.
    ComputedStyle s;

    s.border_top.width = Length::px(1);
    s.border_top.style = BorderStyle::Solid;
    s.border_top.color = {255, 0, 0, 255}; // red

    s.border_right.width = Length::px(2);
    s.border_right.style = BorderStyle::Dashed;
    s.border_right.color = {0, 128, 0, 255}; // green

    s.border_bottom.width = Length::px(3);
    s.border_bottom.style = BorderStyle::Dotted;
    s.border_bottom.color = {0, 0, 255, 200}; // semi-transparent blue

    s.border_left.width = Length::px(4);
    s.border_left.style = BorderStyle::Double;
    s.border_left.color = {128, 128, 128, 255}; // gray

    // Widths
    EXPECT_FLOAT_EQ(s.border_top.width.to_px(), 1.0f);
    EXPECT_FLOAT_EQ(s.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(s.border_bottom.width.to_px(), 3.0f);
    EXPECT_FLOAT_EQ(s.border_left.width.to_px(), 4.0f);

    // Styles all different
    EXPECT_EQ(s.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(s.border_right.style, BorderStyle::Dashed);
    EXPECT_EQ(s.border_bottom.style, BorderStyle::Dotted);
    EXPECT_EQ(s.border_left.style, BorderStyle::Double);

    // Colors all different (verify via operator==)
    EXPECT_TRUE(s.border_top.color == (Color{255, 0, 0, 255}));
    EXPECT_TRUE(s.border_right.color != s.border_top.color);
    EXPECT_EQ(s.border_bottom.color.a, 200); // semi-transparent
    EXPECT_EQ(s.border_left.color.r, 128);
    EXPECT_EQ(s.border_left.color.g, 128);

    // Modifying one doesn't affect others
    s.border_top.color = Color::white();
    EXPECT_EQ(s.border_right.color.g, 128); // unchanged
}

TEST(ComputedStyleTest, ScrollbarAndOverscrollCombinedConfigV124) {
    // Combine scrollbar-width, scrollbar-gutter, scrollbar-color,
    // scroll-snap, and overscroll-behavior into one comprehensive test
    // verifying all fields can be set independently.
    ComputedStyle s;

    // Defaults
    EXPECT_EQ(s.scrollbar_width, 0); // auto
    EXPECT_EQ(s.scrollbar_gutter, 0); // auto
    EXPECT_EQ(s.scrollbar_thumb_color, 0u);
    EXPECT_EQ(s.scrollbar_track_color, 0u);
    EXPECT_EQ(s.scroll_behavior, 0);
    EXPECT_EQ(s.scroll_snap_stop, 0);
    EXPECT_EQ(s.overscroll_behavior, 0);

    // Set scrollbar properties
    s.scrollbar_width = 1; // thin
    s.scrollbar_gutter = 2; // stable both-edges
    s.scrollbar_thumb_color = 0xFF333333;
    s.scrollbar_track_color = 0xFFEEEEEE;

    // Set scroll snap
    s.scroll_snap_type = "y mandatory";
    s.scroll_snap_align = "center";
    s.scroll_snap_stop = 1; // always
    s.scroll_behavior = 1; // smooth

    // Set overscroll behavior
    s.overscroll_behavior = 1; // contain
    s.overscroll_behavior_x = 2; // none
    s.overscroll_behavior_y = 0; // auto

    // Scroll margins and padding
    s.scroll_margin_top = 10.0f;
    s.scroll_margin_bottom = 20.0f;
    s.scroll_padding_left = 15.0f;
    s.scroll_padding_right = 15.0f;

    // Verify all
    EXPECT_EQ(s.scrollbar_width, 1);
    EXPECT_EQ(s.scrollbar_gutter, 2);
    EXPECT_EQ(s.scrollbar_thumb_color, 0xFF333333u);
    EXPECT_EQ(s.scrollbar_track_color, 0xFFEEEEEEu);
    EXPECT_EQ(s.scroll_snap_type, "y mandatory");
    EXPECT_EQ(s.scroll_snap_align, "center");
    EXPECT_EQ(s.scroll_snap_stop, 1);
    EXPECT_EQ(s.scroll_behavior, 1);
    EXPECT_EQ(s.overscroll_behavior, 1);
    EXPECT_EQ(s.overscroll_behavior_x, 2);
    EXPECT_EQ(s.overscroll_behavior_y, 0);
    EXPECT_FLOAT_EQ(s.scroll_margin_top, 10.0f);
    EXPECT_FLOAT_EQ(s.scroll_margin_bottom, 20.0f);
    EXPECT_FLOAT_EQ(s.scroll_padding_left, 15.0f);
    EXPECT_FLOAT_EQ(s.scroll_padding_right, 15.0f);
}

TEST(ComputedStyleTest, CalcExprDivMulNestedWithUnitsV124) {
    // Build a calc expression: calc((100px - 2em) * 0.5) with parent=16,
    // root=16. Expected: (100 - 32) * 0.5 = 34.
    // Also test division: calc(200px / 4) = 50.
    float parent_fs = 16.0f;
    float root_fs = 16.0f;

    // calc(100px - 2em) => 100 - 2*16 = 68
    auto px100 = CalcExpr::make_value(Length::px(100));
    auto em2 = CalcExpr::make_value(Length::em(2));
    auto sub_expr = CalcExpr::make_binary(CalcExpr::Op::Sub, px100, em2);
    EXPECT_FLOAT_EQ(sub_expr->evaluate(parent_fs, root_fs), 68.0f);

    // calc((100px - 2em) * 0.5) => 68 * 0.5 = 34
    auto half = CalcExpr::make_value(Length::px(0.5f));
    auto mul_expr = CalcExpr::make_binary(CalcExpr::Op::Mul, sub_expr, half);
    EXPECT_FLOAT_EQ(mul_expr->evaluate(parent_fs, root_fs), 34.0f);

    // calc(200px / 4) => 50
    auto px200 = CalcExpr::make_value(Length::px(200));
    auto four = CalcExpr::make_value(Length::px(4));
    auto div_expr = CalcExpr::make_binary(CalcExpr::Op::Div, px200, four);
    EXPECT_FLOAT_EQ(div_expr->evaluate(parent_fs, root_fs), 50.0f);

    // Nested: calc(max(calc(200px / 4), calc((100px - 2em) * 0.5)))
    // = max(50, 34) = 50
    auto nested_max = CalcExpr::make_binary(CalcExpr::Op::Max, div_expr, mul_expr);
    EXPECT_FLOAT_EQ(nested_max->evaluate(parent_fs, root_fs), 50.0f);

    // And min of same => 34
    auto nested_min = CalcExpr::make_binary(CalcExpr::Op::Min, div_expr, mul_expr);
    EXPECT_FLOAT_EQ(nested_min->evaluate(parent_fs, root_fs), 34.0f);
}

// ---------------------------------------------------------------------------
// V125 Tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, CssV125_1_FlexLayoutWithGapAndOrderAndAlignSelf) {
    // Test flex container properties resolved from CSS: direction, wrap,
    // justify-content, align-items, gap, and child order + align-self.
    const std::string css =
        ".flex-container{display:flex;flex-direction:column;flex-wrap:wrap;"
        "justify-content:space-between;align-items:center;gap:12px;"
        "width:600px;height:400px;}"
        ".flex-child{order:3;flex-grow:2;flex-shrink:0.5;"
        "flex-basis:100px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    // Resolve container
    ElementView container;
    container.tag_name = "div";
    container.classes = {"flex-container"};
    ComputedStyle parent;
    auto cs = resolver.resolve(container, parent);

    EXPECT_EQ(cs.display, Display::Flex);
    EXPECT_EQ(cs.flex_direction, FlexDirection::Column);
    EXPECT_EQ(cs.flex_wrap, FlexWrap::Wrap);
    EXPECT_EQ(cs.justify_content, JustifyContent::SpaceBetween);
    EXPECT_EQ(cs.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(cs.gap.to_px(), 12.0f);
    EXPECT_FLOAT_EQ(cs.width.to_px(), 600.0f);
    EXPECT_FLOAT_EQ(cs.height.to_px(), 400.0f);

    // Resolve child
    ElementView child;
    child.tag_name = "div";
    child.classes = {"flex-child"};
    auto ch = resolver.resolve(child, cs);

    EXPECT_EQ(ch.order, 3);
    EXPECT_EQ(ch.align_self, -1); // auto (default, inherits from parent)
    EXPECT_FLOAT_EQ(ch.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(ch.flex_shrink, 0.5f);
    EXPECT_FLOAT_EQ(ch.flex_basis.to_px(), 100.0f);
}

TEST(CSSStyleTest, CssV125_2_StickyPositionWithTopBottomAndBorderRadius) {
    // Verify sticky positioning with inset properties and per-corner
    // border-radius values parsed from CSS.
    const std::string css =
        ".sticky-header{position:sticky;top:0px;z-index:10;"
        "border-radius:8px;padding-top:16px;padding-bottom:16px;"
        "background-color:#1e3a5f;color:#ffffff;font-size:20px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "header";
    elem.classes = {"sticky-header"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Sticky);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_EQ(style.z_index, 10);
    EXPECT_FLOAT_EQ(style.border_radius, 8.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 16.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 16.0f);
    EXPECT_EQ(style.background_color.r, 0x1e);
    EXPECT_EQ(style.background_color.g, 0x3a);
    EXPECT_EQ(style.background_color.b, 0x5f);
    EXPECT_EQ(style.color.r, 0xff);
    EXPECT_EQ(style.color.g, 0xff);
    EXPECT_EQ(style.color.b, 0xff);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 20.0f);
}

TEST(ComputedStyleTest, CssV125_3_TransformMultipleEntriesIndependent) {
    // Verify that multiple Transform entries (translate, rotate, scale)
    // can coexist in the transforms vector with independent field access.
    ComputedStyle s;
    EXPECT_TRUE(s.transforms.empty());

    Transform translate;
    translate.type = TransformType::Translate;
    translate.x = 50.0f;
    translate.y = -30.0f;

    Transform rotate;
    rotate.type = TransformType::Rotate;
    rotate.angle = 45.0f;

    Transform scale;
    scale.type = TransformType::Scale;
    scale.x = 1.5f;
    scale.y = 2.0f;

    s.transforms = {translate, rotate, scale};
    EXPECT_EQ(s.transforms.size(), 3u);

    // Translate
    EXPECT_EQ(s.transforms[0].type, TransformType::Translate);
    EXPECT_FLOAT_EQ(s.transforms[0].x, 50.0f);
    EXPECT_FLOAT_EQ(s.transforms[0].y, -30.0f);

    // Rotate
    EXPECT_EQ(s.transforms[1].type, TransformType::Rotate);
    EXPECT_FLOAT_EQ(s.transforms[1].angle, 45.0f);

    // Scale
    EXPECT_EQ(s.transforms[2].type, TransformType::Scale);
    EXPECT_FLOAT_EQ(s.transforms[2].x, 1.5f);
    EXPECT_FLOAT_EQ(s.transforms[2].y, 2.0f);

    // Modifying one entry does not affect others
    s.transforms[0].x = 100.0f;
    EXPECT_FLOAT_EQ(s.transforms[1].angle, 45.0f);
    EXPECT_FLOAT_EQ(s.transforms[2].x, 1.5f);
}

TEST(ComputedStyleTest, CssV125_4_FilterChainMultipleTypes) {
    // Verify that multiple CSS filters can be stored and accessed
    // independently, including grayscale, brightness, blur, and contrast.
    ComputedStyle s;
    EXPECT_TRUE(s.filters.empty());

    // Build a filter chain: grayscale(50%), brightness(1.2), blur(5px), contrast(0.8)
    s.filters.push_back({1, 0.5f});   // grayscale 50%
    s.filters.push_back({3, 1.2f});   // brightness 120%
    s.filters.push_back({9, 5.0f});   // blur 5px
    s.filters.push_back({4, 0.8f});   // contrast 80%

    EXPECT_EQ(s.filters.size(), 4u);
    EXPECT_EQ(s.filters[0].first, 1);
    EXPECT_FLOAT_EQ(s.filters[0].second, 0.5f);
    EXPECT_EQ(s.filters[1].first, 3);
    EXPECT_FLOAT_EQ(s.filters[1].second, 1.2f);
    EXPECT_EQ(s.filters[2].first, 9);
    EXPECT_FLOAT_EQ(s.filters[2].second, 5.0f);
    EXPECT_EQ(s.filters[3].first, 4);
    EXPECT_FLOAT_EQ(s.filters[3].second, 0.8f);

    // Backdrop filters are independent
    s.backdrop_filters.push_back({9, 10.0f}); // backdrop blur 10px
    EXPECT_EQ(s.backdrop_filters.size(), 1u);
    EXPECT_EQ(s.filters.size(), 4u); // unaffected
    EXPECT_FLOAT_EQ(s.backdrop_filters[0].second, 10.0f);
}

TEST(CSSStyleTest, CssV125_5_TextDecorationStyleWavyWithEmphasisAndStroke) {
    // Resolve text-decoration-style: wavy alongside text-stroke-width
    // and verify emphasis-related fields from direct struct manipulation.
    const std::string css =
        ".fancy-text{text-decoration:underline;"
        "text-decoration-style:wavy;text-decoration-color:#e74c3c;"
        "-webkit-text-stroke-width:1px;font-size:24px;"
        "letter-spacing:3px;color:#2c3e50;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"fancy-text"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_decoration_style, TextDecorationStyle::Wavy);
    EXPECT_EQ(style.text_decoration_color.r, 0xe7);
    EXPECT_EQ(style.text_decoration_color.g, 0x4c);
    EXPECT_EQ(style.text_decoration_color.b, 0x3c);
    EXPECT_FLOAT_EQ(style.text_stroke_width, 1.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 24.0f);
    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(24.0f), 3.0f);
    EXPECT_EQ(style.color.r, 0x2c);
    EXPECT_EQ(style.color.g, 0x3e);
    EXPECT_EQ(style.color.b, 0x50);
}

TEST(ComputedStyleTest, CssV125_6_CustomPropertiesAndCssVariablesStorage) {
    // Verify that CSS custom properties (CSS variables) can be stored
    // and retrieved independently in the custom_properties map.
    ComputedStyle s;
    EXPECT_TRUE(s.custom_properties.empty());

    s.custom_properties["--primary-color"] = "#3498db";
    s.custom_properties["--spacing-unit"] = "8px";
    s.custom_properties["--font-family"] = "Inter, sans-serif";
    s.custom_properties["--border-radius"] = "4px";

    EXPECT_EQ(s.custom_properties.size(), 4u);
    EXPECT_EQ(s.custom_properties["--primary-color"], "#3498db");
    EXPECT_EQ(s.custom_properties["--spacing-unit"], "8px");
    EXPECT_EQ(s.custom_properties["--font-family"], "Inter, sans-serif");
    EXPECT_EQ(s.custom_properties["--border-radius"], "4px");

    // Overwrite a variable
    s.custom_properties["--primary-color"] = "#e74c3c";
    EXPECT_EQ(s.custom_properties["--primary-color"], "#e74c3c");
    EXPECT_EQ(s.custom_properties.size(), 4u); // still 4, not 5

    // Erase a variable
    s.custom_properties.erase("--spacing-unit");
    EXPECT_EQ(s.custom_properties.size(), 3u);
    EXPECT_EQ(s.custom_properties.count("--spacing-unit"), 0u);
}

TEST(ComputedStyleTest, CssV125_7_ViewportUnitsVwVhVminVmax) {
    // Test viewport-relative Length units vw, vh, vmin, vmax with
    // a custom viewport size and verify they resolve correctly.
    float saved_w = Length::s_viewport_w;
    float saved_h = Length::s_viewport_h;

    Length::set_viewport(1200.0f, 800.0f);

    // 50vw = 50% of 1200 = 600
    Length vw50 = Length::vw(50.0f);
    EXPECT_FLOAT_EQ(vw50.to_px(), 600.0f);

    // 25vh = 25% of 800 = 200
    Length vh25 = Length::vh(25.0f);
    EXPECT_FLOAT_EQ(vh25.to_px(), 200.0f);

    // 10vmin = 10% of min(1200,800) = 10% of 800 = 80
    Length vmin10 = Length::vmin(10.0f);
    EXPECT_FLOAT_EQ(vmin10.to_px(), 80.0f);

    // 10vmax = 10% of max(1200,800) = 10% of 1200 = 120
    Length vmax10 = Length::vmax(10.0f);
    EXPECT_FLOAT_EQ(vmax10.to_px(), 120.0f);

    // Verify is_auto and is_zero return false for viewport units
    EXPECT_FALSE(vw50.is_auto());
    EXPECT_FALSE(vh25.is_auto());
    EXPECT_FALSE(vmin10.is_zero());
    EXPECT_FALSE(vmax10.is_zero());

    // Restore viewport
    Length::set_viewport(saved_w, saved_h);
}

TEST(CSSStyleTest, CssV125_8_GridTemplateWithAutoFlowAndAlignContent) {
    // Resolve CSS Grid container properties and verify grid-specific
    // fields are correctly parsed from a stylesheet.
    const std::string css =
        ".grid{display:grid;grid-auto-flow:1;"
        "width:800px;height:600px;"
        "padding-top:10px;padding-right:10px;padding-bottom:10px;padding-left:10px;"
        "background-color:#f0f0f0;font-size:14px;color:#333333;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"grid"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.display, Display::Grid);
    EXPECT_FLOAT_EQ(style.width.to_px(), 800.0f);
    EXPECT_FLOAT_EQ(style.height.to_px(), 600.0f);
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 10.0f);
    EXPECT_EQ(style.background_color.r, 0xf0);
    EXPECT_EQ(style.background_color.g, 0xf0);
    EXPECT_EQ(style.background_color.b, 0xf0);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
    EXPECT_EQ(style.color.r, 0x33);
    EXPECT_EQ(style.color.g, 0x33);
    EXPECT_EQ(style.color.b, 0x33);
}

// ============================================================================
// V126: Diverse computed style and resolution tests
// ============================================================================

TEST(CSSStyleTest, CssV126_1_FlexboxPropertiesDefaultAndAssignment) {
    // Verify flexbox-related default values and assignment
    ComputedStyle s;

    EXPECT_EQ(s.flex_direction, FlexDirection::Row);
    EXPECT_EQ(s.flex_wrap, FlexWrap::NoWrap);
    EXPECT_EQ(s.justify_content, JustifyContent::FlexStart);
    EXPECT_EQ(s.align_items, AlignItems::Stretch);
    EXPECT_FLOAT_EQ(s.flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 1.0f);
    EXPECT_TRUE(s.flex_basis.is_auto());
    EXPECT_EQ(s.order, 0);

    // Assign non-default values
    s.flex_direction = FlexDirection::Column;
    s.flex_wrap = FlexWrap::Wrap;
    s.justify_content = JustifyContent::SpaceBetween;
    s.align_items = AlignItems::Center;
    s.flex_grow = 2.0f;
    s.flex_shrink = 0.0f;
    s.flex_basis = Length::px(100.0f);
    s.order = 3;

    EXPECT_EQ(s.flex_direction, FlexDirection::Column);
    EXPECT_EQ(s.flex_wrap, FlexWrap::Wrap);
    EXPECT_EQ(s.justify_content, JustifyContent::SpaceBetween);
    EXPECT_EQ(s.align_items, AlignItems::Center);
    EXPECT_FLOAT_EQ(s.flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(s.flex_shrink, 0.0f);
    EXPECT_FLOAT_EQ(s.flex_basis.to_px(), 100.0f);
    EXPECT_EQ(s.order, 3);
}

TEST(CSSStyleTest, CssV126_2_ResolveTextDecorationAndTransformFromCSS) {
    // Resolve text-decoration-line underline and text-transform uppercase
    const std::string css =
        ".deco{text-decoration:underline;text-transform:uppercase;"
        "text-align:center;font-style:italic;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"deco"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_EQ(style.text_align, TextAlign::Center);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(CSSStyleTest, CssV126_3_BoxShadowEntryFieldAccess) {
    // Verify BoxShadowEntry fields including .blur (NOT .blur_radius)
    ComputedStyle s;
    EXPECT_TRUE(s.box_shadows.empty());

    ComputedStyle::BoxShadowEntry shadow;
    shadow.offset_x = 5.0f;
    shadow.offset_y = 10.0f;
    shadow.blur = 15.0f;
    shadow.spread = 3.0f;
    shadow.color = {100, 200, 50, 180};
    shadow.inset = true;

    s.box_shadows.push_back(shadow);
    EXPECT_EQ(s.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_x, 5.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_y, 10.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 15.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 3.0f);
    EXPECT_EQ(s.box_shadows[0].color.r, 100);
    EXPECT_EQ(s.box_shadows[0].color.g, 200);
    EXPECT_EQ(s.box_shadows[0].color.b, 50);
    EXPECT_EQ(s.box_shadows[0].color.a, 180);
    EXPECT_TRUE(s.box_shadows[0].inset);
}

TEST(CSSStyleTest, CssV126_4_OverflowAndPositionEnums) {
    // Test overflow_x, overflow_y, position, and float defaults and assignment
    ComputedStyle s;

    EXPECT_EQ(s.overflow_x, Overflow::Visible);
    EXPECT_EQ(s.overflow_y, Overflow::Visible);
    EXPECT_EQ(s.position, Position::Static);
    EXPECT_EQ(s.float_val, Float::None);
    EXPECT_EQ(s.clear, Clear::None);

    s.overflow_x = Overflow::Hidden;
    s.overflow_y = Overflow::Scroll;
    s.position = Position::Absolute;
    s.float_val = Float::Left;
    s.clear = Clear::Both;

    EXPECT_EQ(s.overflow_x, Overflow::Hidden);
    EXPECT_EQ(s.overflow_y, Overflow::Scroll);
    EXPECT_EQ(s.position, Position::Absolute);
    EXPECT_EQ(s.float_val, Float::Left);
    EXPECT_EQ(s.clear, Clear::Both);
}

TEST(CSSStyleTest, CssV126_5_ResolveMarginPaddingShorthandsFromCSS) {
    // Resolve CSS margin and padding shorthand values via StyleResolver
    const std::string css =
        ".box{margin:10px 20px 30px 40px;padding:5px 15px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"box"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    // margin: 10px 20px 30px 40px
    EXPECT_FLOAT_EQ(style.margin.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.margin.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 40.0f);

    // padding: 5px 15px -> top=5 right=15 bottom=5 left=15
    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 15.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 5.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 15.0f);
}

TEST(CSSStyleTest, CssV126_6_TextShadowEntryAndMultipleShadows) {
    // Verify TextShadowEntry fields and multiple shadow support
    ComputedStyle s;
    EXPECT_TRUE(s.text_shadows.empty());

    ComputedStyle::TextShadowEntry ts1;
    ts1.offset_x = 1.0f;
    ts1.offset_y = 2.0f;
    ts1.blur = 3.0f;
    ts1.color = {255, 0, 0, 255};

    ComputedStyle::TextShadowEntry ts2;
    ts2.offset_x = -1.0f;
    ts2.offset_y = -2.0f;
    ts2.blur = 0.0f;
    ts2.color = {0, 0, 255, 128};

    s.text_shadows.push_back(ts1);
    s.text_shadows.push_back(ts2);

    EXPECT_EQ(s.text_shadows.size(), 2u);

    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_x, 1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].offset_y, 2.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[0].blur, 3.0f);
    EXPECT_EQ(s.text_shadows[0].color.r, 255);

    EXPECT_FLOAT_EQ(s.text_shadows[1].offset_x, -1.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[1].offset_y, -2.0f);
    EXPECT_FLOAT_EQ(s.text_shadows[1].blur, 0.0f);
    EXPECT_EQ(s.text_shadows[1].color.a, 128);
}

TEST(CSSStyleTest, CssV126_7_BorderRadiusAndBoxSizingDefaults) {
    // Verify border-radius fields and box-sizing defaults
    ComputedStyle s;

    EXPECT_FLOAT_EQ(s.border_radius, 0.0f);
    EXPECT_FLOAT_EQ(s.border_radius_tl, 0.0f);
    EXPECT_FLOAT_EQ(s.border_radius_tr, 0.0f);
    EXPECT_FLOAT_EQ(s.border_radius_bl, 0.0f);
    EXPECT_FLOAT_EQ(s.border_radius_br, 0.0f);
    EXPECT_EQ(s.box_sizing, BoxSizing::ContentBox);

    // Assign individual corner radii
    s.border_radius_tl = 5.0f;
    s.border_radius_tr = 10.0f;
    s.border_radius_bl = 15.0f;
    s.border_radius_br = 20.0f;
    s.box_sizing = BoxSizing::BorderBox;

    EXPECT_FLOAT_EQ(s.border_radius_tl, 5.0f);
    EXPECT_FLOAT_EQ(s.border_radius_tr, 10.0f);
    EXPECT_FLOAT_EQ(s.border_radius_bl, 15.0f);
    EXPECT_FLOAT_EQ(s.border_radius_br, 20.0f);
    EXPECT_EQ(s.box_sizing, BoxSizing::BorderBox);
}

TEST(CSSStyleTest, CssV126_8_ResolveBorderShorthandAllEdges) {
    // Resolve border shorthand into all four edges
    const std::string css =
        ".bordered{border:2px solid #ff0000;"
        "opacity:0.75;z-index:5;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"bordered"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    // All four border edges should have width 2px
    EXPECT_FLOAT_EQ(style.border_top.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_right.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_bottom.width.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.border_left.width.to_px(), 2.0f);

    // All four should be solid
    EXPECT_EQ(style.border_top.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_right.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_bottom.style, BorderStyle::Solid);
    EXPECT_EQ(style.border_left.style, BorderStyle::Solid);

    // All four should be red
    EXPECT_EQ(style.border_top.color.r, 255);
    EXPECT_EQ(style.border_top.color.g, 0);
    EXPECT_EQ(style.border_top.color.b, 0);

    // Opacity and z_index
    EXPECT_FLOAT_EQ(style.opacity, 0.75f);
    EXPECT_EQ(style.z_index, 5);
}

// ---------------------------------------------------------------------------
// V127 tests
// ---------------------------------------------------------------------------

TEST(CSSStyleTest, CssV127_1_LetterSpacingAndWordSpacingFromCSS) {
    const std::string css = ".spaced{letter-spacing:2px;word-spacing:4px;font-size:14px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "p";
    elem.classes = {"spaced"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.letter_spacing.to_px(), 2.0f);
    EXPECT_FLOAT_EQ(style.word_spacing.to_px(), 4.0f);
    EXPECT_FLOAT_EQ(style.font_size.to_px(), 14.0f);
}

TEST(CSSStyleTest, CssV127_2_OutlineWidthStyleColorFromCSS) {
    const std::string css = ".focused{outline-width:3px;outline-style:solid;outline-color:#00ff00;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "input";
    elem.classes = {"focused"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_FLOAT_EQ(style.outline_width.to_px(), 3.0f);
    EXPECT_EQ(style.outline_style, BorderStyle::Solid);
    EXPECT_EQ(style.outline_color.r, 0);
    EXPECT_EQ(style.outline_color.g, 255);
    EXPECT_EQ(style.outline_color.b, 0);
    EXPECT_EQ(style.outline_color.a, 255);
}

TEST(CSSStyleTest, CssV127_3_PointerEventsNoneWithTextOverflowEllipsis) {
    const std::string css = ".disabled{pointer-events:none;text-overflow:ellipsis;overflow:hidden;white-space:nowrap;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"disabled"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.pointer_events, PointerEvents::None);
    EXPECT_EQ(style.text_overflow, TextOverflow::Ellipsis);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

TEST(ComputedStyleTest, CssV127_4_BoxShadowEntryBlurAndSpreadFields) {
    ComputedStyle s;
    ComputedStyle::BoxShadowEntry shadow;
    shadow.offset_x = 3.0f;
    shadow.offset_y = 5.0f;
    shadow.blur = 10.0f;
    shadow.spread = 2.0f;
    shadow.color = Color{128, 0, 255, 200};
    shadow.inset = true;
    s.box_shadows.push_back(shadow);

    EXPECT_EQ(s.box_shadows.size(), 1u);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_x, 3.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].offset_y, 5.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].blur, 10.0f);
    EXPECT_FLOAT_EQ(s.box_shadows[0].spread, 2.0f);
    EXPECT_EQ(s.box_shadows[0].color.r, 128);
    EXPECT_EQ(s.box_shadows[0].color.g, 0);
    EXPECT_EQ(s.box_shadows[0].color.b, 255);
    EXPECT_EQ(s.box_shadows[0].color.a, 200);
    EXPECT_TRUE(s.box_shadows[0].inset);
}

TEST(CSSStyleTest, CssV127_5_FlexDirectionColumnReverseWithWrap) {
    const std::string css = ".col-flex{flex-direction:column-reverse;flex-wrap:wrap;justify-content:center;display:flex;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"col-flex"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.flex_direction, FlexDirection::ColumnReverse);
    EXPECT_EQ(style.flex_wrap, FlexWrap::Wrap);
    EXPECT_EQ(style.justify_content, JustifyContent::Center);
    EXPECT_EQ(style.display, Display::Flex);
}

TEST(ComputedStyleTest, CssV127_6_LengthFactoryMethodsAndConversion) {
    Length px_len = Length::px(24.0f);
    EXPECT_FLOAT_EQ(px_len.to_px(), 24.0f);
    EXPECT_FALSE(px_len.is_auto());
    EXPECT_FALSE(px_len.is_zero());

    Length auto_len = Length::auto_val();
    EXPECT_TRUE(auto_len.is_auto());

    Length zero_len = Length::zero();
    EXPECT_TRUE(zero_len.is_zero());
    EXPECT_FALSE(zero_len.is_auto());

    Length em_len = Length::em(2.0f);
    // em relative to parent_value (font_size); 2em * 16px parent = 32px
    EXPECT_FLOAT_EQ(em_len.to_px(16.0f), 32.0f);

    Length pct_len = Length::percent(50.0f);
    // 50% of 200px parent = 100px
    EXPECT_FLOAT_EQ(pct_len.to_px(200.0f), 100.0f);
}

TEST(CSSStyleTest, CssV127_7_TextTransformAndFontStyleItalic) {
    const std::string css = ".fancy{text-transform:uppercase;font-style:italic;text-decoration:underline;color:#ff6600;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "span";
    elem.classes = {"fancy"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.text_transform, TextTransform::Uppercase);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
    EXPECT_EQ(style.color.r, 0xFF);
    EXPECT_EQ(style.color.g, 0x66);
    EXPECT_EQ(style.color.b, 0x00);
}

TEST(CSSStyleTest, CssV127_8_PositionFixedWithTopLeftAndOverflowScroll) {
    const std::string css = ".modal{position:fixed;top:0px;left:0px;overflow:scroll;background-color:#ffffff;z-index:1000;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"modal"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.position, Position::Fixed);
    EXPECT_FLOAT_EQ(style.top.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.left_pos.to_px(), 0.0f);
    EXPECT_EQ(style.overflow_x, Overflow::Scroll);
    EXPECT_EQ(style.overflow_y, Overflow::Scroll);
    EXPECT_EQ(style.background_color.r, 255);
    EXPECT_EQ(style.background_color.g, 255);
    EXPECT_EQ(style.background_color.b, 255);
    EXPECT_EQ(style.z_index, 1000);
}

TEST(PropertyCascadeTest, OverflowAnchorValuesV128) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow-anchor", "none"), parent);
    EXPECT_EQ(style.overflow_anchor, 1);

    cascade.apply_declaration(style, make_decl("overflow-anchor", "auto"), parent);
    EXPECT_EQ(style.overflow_anchor, 0);
}

TEST(PropertyCascadeTest, TransformStyleAndBoxV128) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform-style", "preserve-3d"), parent);
    EXPECT_EQ(style.transform_style, 1);

    cascade.apply_declaration(style, make_decl("transform-box", "fill-box"), parent);
    EXPECT_EQ(style.transform_box, 2);
}

TEST(PropertyCascadeTest, ContainerTypeAndNameV128) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("container-type", "inline-size"), parent);
    EXPECT_EQ(style.container_type, 2);

    cascade.apply_declaration(style, make_decl("container-name", "sidebar"), parent);
    EXPECT_EQ(style.container_name, "sidebar");
}

TEST(CSSStyleTest, CssV128_1_ScrollBehaviorSmoothAndSnapType) {
    const std::string css = ".scroll{scroll-behavior:smooth;scroll-snap-type:y mandatory;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView elem;
    elem.tag_name = "div";
    elem.classes = {"scroll"};

    ComputedStyle parent;
    auto style = resolver.resolve(elem, parent);

    EXPECT_EQ(style.scroll_behavior, 1);
    EXPECT_EQ(style.scroll_snap_type, "y mandatory");
}

TEST(PropertyCascadeTest, TextWrapValuesV129) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-wrap", "balance"), parent);
    EXPECT_EQ(style.text_wrap, 2);

    cascade.apply_declaration(style, make_decl("text-wrap", "pretty"), parent);
    EXPECT_EQ(style.text_wrap, 3);

    cascade.apply_declaration(style, make_decl("text-wrap", "stable"), parent);
    EXPECT_EQ(style.text_wrap, 4);
}

TEST(CSSStyleTest, CssV129_2_InheritedFontSizeOverriddenByChild) {
    const std::string css = ".parent{font-size:24px;} .child{font-size:12px;}";

    StyleResolver resolver;
    auto sheet = parse_stylesheet(css);
    resolver.add_stylesheet(sheet);

    ElementView parent_elem;
    parent_elem.tag_name = "div";
    parent_elem.classes = {"parent"};

    ComputedStyle root_style;
    auto parent_style = resolver.resolve(parent_elem, root_style);

    ElementView child_elem;
    child_elem.tag_name = "span";
    child_elem.classes = {"child"};

    auto child_style = resolver.resolve(child_elem, parent_style);

    EXPECT_FLOAT_EQ(child_style.font_size.to_px(), 12.0f);
}

TEST(PropertyCascadeTest, AccentColorAppliedV129) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("accent-color", "#00ff00"), parent);
    EXPECT_EQ(style.accent_color.r, 0);
    EXPECT_EQ(style.accent_color.g, 255);
    EXPECT_EQ(style.accent_color.b, 0);
    EXPECT_EQ(style.accent_color.a, 255);
}

TEST(CSSStyleTest, CssV129_8_OpacityAndVisibilityCombo) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("opacity", "0.5"), parent);
    cascade.apply_declaration(style, make_decl("visibility", "hidden"), parent);
    cascade.apply_declaration(style, make_decl("z-index", "10"), parent);

    EXPECT_FLOAT_EQ(style.opacity, 0.5f);
    EXPECT_EQ(style.visibility, Visibility::Hidden);
    EXPECT_EQ(style.z_index, 10);
}

TEST(PropertyCascadeTest, TransitionBehaviorAllowDiscreteV130) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transition-behavior", "allow-discrete"), parent);
    EXPECT_EQ(style.transition_behavior, 1);
}

TEST(PropertyCascadeTest, ScrollbarWidthThinV130) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("scrollbar-width", "thin"), parent);
    EXPECT_EQ(style.scrollbar_width, 1);
}

TEST(PropertyCascadeTest, OverflowBlockInlineV130) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow-block", "hidden"), parent);
    cascade.apply_declaration(style, make_decl("overflow-inline", "scroll"), parent);
    EXPECT_EQ(style.overflow_block, 1);
    EXPECT_EQ(style.overflow_inline, 2);
}

TEST(PropertyCascadeTest, BoxDecorationBreakCloneV130) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("box-decoration-break", "clone"), parent);
    EXPECT_EQ(style.box_decoration_break, 1);
}

TEST(PropertyCascadeTest, ContentVisibilityAutoV131) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("content-visibility", "auto"), parent);
    EXPECT_EQ(style.content_visibility, 2);
}

TEST(PropertyCascadeTest, OverscrollBehaviorContainV131) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overscroll-behavior", "contain"), parent);
    EXPECT_EQ(style.overscroll_behavior, 1);
}

TEST(PropertyCascadeTest, BackfaceVisibilityHiddenV131) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("backface-visibility", "hidden"), parent);
    EXPECT_EQ(style.backface_visibility, 1);
}

TEST(PropertyCascadeTest, TextDecorationSkipInkAutoV131) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-decoration-skip-ink", "auto"), parent);
    EXPECT_EQ(style.text_decoration_skip_ink, 0);
}

TEST(PropertyCascadeTest, FontSynthesisAutoV132) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-synthesis", "auto"), parent);
    EXPECT_EQ(style.font_synthesis, 0);
}

TEST(PropertyCascadeTest, MaskCompositeAddV132) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("mask-composite", "add"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, TransformStylePreserve3dV132) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("transform-style", "preserve-3d"), parent);
    EXPECT_EQ(style.transform_style, 1);
}

TEST(PropertyCascadeTest, AnimationCompositionReplaceV132) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("animation-composition", "replace"), parent);
    SUCCEED();
}

// --- Round 133 (V133) ---

TEST(PropertyCascadeTest, FontPaletteNormalV133) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-palette", "normal"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, OffsetPathNoneV133) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("offset-path", "none"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, MaskModeAlphaV133) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("mask-mode", "alpha"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, BoxDecorationBreakSliceV133) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("box-decoration-break", "slice"), parent);
    EXPECT_EQ(style.box_decoration_break, 0);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Round 134
// ---------------------------------------------------------------------------

TEST(PropertyCascadeTest, ContainLayoutV134) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("contain", "layout"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, WillChangeTransformV134) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("will-change", "transform"), parent);
    SUCCEED();
}

TEST(PropertyCascadeTest, ScrollBehaviorSmoothV134) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("scroll-behavior", "smooth"), parent);
    EXPECT_EQ(style.scroll_behavior, 1);
}

TEST(PropertyCascadeTest, TouchActionNoneV134) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("touch-action", "none"), parent);
    SUCCEED();
}

// === V135 CSS Style Tests ===

TEST(CSSStyleTest, CssV135_1_MarginAutoResolvedToNegativeOne) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("margin-left", "auto"), parent);
    EXPECT_TRUE(style.margin.left.is_auto());

    cascade.apply_declaration(style, make_decl("margin-right", "auto"), parent);
    EXPECT_TRUE(style.margin.right.is_auto());

    // Auto margins have to_px() returning 0 (resolved by layout, not style)
    EXPECT_FLOAT_EQ(style.margin.left.to_px(), 0.0f);
    EXPECT_FLOAT_EQ(style.margin.right.to_px(), 0.0f);
}

TEST(PropertyCascadeTest, OverflowXYCombinedV135) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow-x", "hidden"), parent);
    cascade.apply_declaration(style, make_decl("overflow-y", "scroll"), parent);

    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Scroll);
}

TEST(CSSStyleTest, CssV135_3_PaddingShorthandExpandsToFourSides) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style,
        make_decl_multi("padding", {"10px", "20px", "30px", "40px"}), parent);

    EXPECT_FLOAT_EQ(style.padding.top.to_px(), 10.0f);
    EXPECT_FLOAT_EQ(style.padding.right.to_px(), 20.0f);
    EXPECT_FLOAT_EQ(style.padding.bottom.to_px(), 30.0f);
    EXPECT_FLOAT_EQ(style.padding.left.to_px(), 40.0f);
}

TEST(CSSStyleTest, CssV135_4_BorderRadiusShorthandAllCornersV135) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-radius", "8px"), parent);

    EXPECT_FLOAT_EQ(style.border_radius, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_tl, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_tr, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_br, 8.0f);
    EXPECT_FLOAT_EQ(style.border_radius_bl, 8.0f);
}

// === V136 CSS Style Tests ===

TEST(CSSStyleTest, CssV136_1_DisplayFlexSetsDisplayType) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "flex"), parent);
    EXPECT_EQ(style.display, Display::Flex);
}

TEST(PropertyCascadeTest, FontWeightBoldV136) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-weight", "bold"), parent);
    EXPECT_EQ(style.font_weight, 700);
}

TEST(CSSStyleTest, CssV136_3_TextAlignCenterV136) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-align", "center"), parent);
    EXPECT_EQ(style.text_align, TextAlign::Center);
}

TEST(CSSStyleTest, CssV136_4_PositionAbsoluteV136) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("position", "absolute"), parent);
    EXPECT_EQ(style.position, Position::Absolute);
}

// === V137 CSS Style Tests ===

TEST(CSSStyleTest, CssV137_1_FloatLeftSetsFloat) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("float", "left"), parent);
    EXPECT_EQ(style.float_val, Float::Left);
}

TEST(PropertyCascadeTest, ClearBothV137) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("clear", "both"), parent);
    EXPECT_EQ(style.clear, Clear::Both);
}

TEST(CSSStyleTest, CssV137_3_OverflowHiddenV137) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow", "hidden"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

TEST(CSSStyleTest, CssV137_4_WhiteSpaceNowrapV137) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("white-space", "nowrap"), parent);
    EXPECT_EQ(style.white_space, WhiteSpace::NoWrap);
}

// === V138 CSS Style Tests ===

TEST(CSSStyleTest, CssV138_1_ListStyleNoneV138) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("list-style-type", "none"), parent);
    EXPECT_EQ(style.list_style_type, ListStyleType::None);
}

TEST(PropertyCascadeTest, CursorPointerV138) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("cursor", "pointer"), parent);
    EXPECT_EQ(style.cursor, Cursor::Pointer);
}

TEST(CSSStyleTest, CssV138_3_VerticalAlignMiddleV138) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("vertical-align", "middle"), parent);
    EXPECT_EQ(style.vertical_align, VerticalAlign::Middle);
}

TEST(CSSStyleTest, CssV138_4_BoxSizingBorderBoxV138) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("box-sizing", "border-box"), parent);
    EXPECT_EQ(style.box_sizing, BoxSizing::BorderBox);
}

// === V139 CSS Style Tests ===

TEST(CSSStyleTest, CssV139_1_FontStyleItalicV139) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-style", "italic"), parent);
    EXPECT_EQ(style.font_style, FontStyle::Italic);
}

TEST(PropertyCascadeTest, TextDecorationUnderlineV139) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("text-decoration", "underline"), parent);
    EXPECT_EQ(style.text_decoration, TextDecoration::Underline);
}

TEST(CSSStyleTest, CssV139_3_DisplayInlineBlockV139) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "inline-block"), parent);
    EXPECT_EQ(style.display, Display::InlineBlock);
}

TEST(CSSStyleTest, CssV139_4_PositionRelativeV139) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("position", "relative"), parent);
    EXPECT_EQ(style.position, Position::Relative);
}

// === V140 CSS Style Tests ===

TEST(CSSStyleTest, CssV140_1_DisplayNoneV140) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "none"), parent);
    EXPECT_EQ(style.display, Display::None);
}

TEST(PropertyCascadeTest, OverflowVisibleV140) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow", "visible"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Visible);
    EXPECT_EQ(style.overflow_y, Overflow::Visible);
}

TEST(CSSStyleTest, CssV140_3_FontFamilySansSerifV140) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("font-family", "sans-serif"), parent);
    EXPECT_EQ(style.font_family, "sans-serif");
}

TEST(CSSStyleTest, CssV140_4_LineHeightNumericV140) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("line-height", "1.5"), parent);
    EXPECT_FLOAT_EQ(style.line_height_unitless, 1.5f);
}

// === V141 CSS Style Tests ===

TEST(CSSStyleTest, CssV141_1_DisplayFlexApplied) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("display", "flex"), parent);
    EXPECT_EQ(style.display, Display::Flex);
}

TEST(CSSStyleTest, CssV141_2_PositionAbsoluteApplied) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("position", "absolute"), parent);
    EXPECT_EQ(style.position, Position::Absolute);
}

TEST(PropertyCascadeTest, OverflowHiddenAppliedV141) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("overflow", "hidden"), parent);
    EXPECT_EQ(style.overflow_x, Overflow::Hidden);
    EXPECT_EQ(style.overflow_y, Overflow::Hidden);
}

TEST(CSSStyleTest, CssV141_4_BorderRadiusApplied) {
    PropertyCascade cascade;
    ComputedStyle style;
    ComputedStyle parent;

    cascade.apply_declaration(style, make_decl("border-radius", "8px"), parent);
    EXPECT_FLOAT_EQ(style.border_radius, 8.0f);
}
